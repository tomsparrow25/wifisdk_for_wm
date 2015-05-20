/*}
 *  Copyright 2008-2013 Marvell International Ltd.
 *  All Rights Reserved.
 */
/* app_ctrl.c: This maintains the state machine for the application
 * framework. Details of the state machine can be found in the doxygen
 * documentation with the app_framework.h header file.
 */

#include <string.h>
#include <wmstdio.h>
#include <psm.h>
#include <wm_os.h>
#include <wm_net.h>
#include <arch/boot_flags.h>
#include <app_framework.h>

#include "app_ctrl.h"
#include "app_network_config.h"
#include "app_network_mgr.h"
#include "app_dbg.h"

#ifdef CONFIG_P2P
#include "app_p2p.h"
#include <dhcp-server.h>
#endif
/* app ctrl event queue message */
typedef struct {
	uint32_t event;
	void *data;
} app_ctrl_msg_t;

bool sta_stop_cmd;

typedef enum {
	AF_STATE_INIT_SYSTEM = 0,
	/* Dummy state to indicate the start of provisioning states */
	AF_STATE_UNCONFIGURED,
	AF_STATE_PROV_START_MARKER,
	AF_STATE_PROV_INIT,
	AF_STATE_PROV_STARTED,
	AF_STATE_PROV_WPS_DONE,
	AF_STATE_PROV_DONE,
	AF_STATE_PROV_RESET_TO_PROV,
	/* Dummy state to indicate the end of provisioning states */
	AF_STATE_PROV_END_MARKER,
	/* Dummy state to indicate the start of normal states */
	AF_STATE_NORMAL_START_MARKER,
	AF_STATE_CONFIGURED,
	AF_STATE_NORMAL_INIT,
	AF_STATE_NORMAL_CONNECTING,
	AF_STATE_NORMAL_CONNECTED,
	AF_STATE_NORMAL_DISCONNECTED,
	AF_STATE_NORMAL_NW_CHANGED,
	/* Dummy state to indicate the end of normal states */
	AF_STATE_NORMAL_END_MARKER,
	AF_STATE_UAP_START_MARKER,
	AF_STATE_UAP_INIT,
	AF_STATE_UAP_STARTING,
	AF_STATE_UAP_UP,
	AF_STATE_UAP_END_MARKER,
#ifdef CONFIG_P2P
	AF_STATE_P2P_START_MARKER,
	AF_STATE_P2P_INIT,
	AF_STATE_P2P_DEVICE_STARTED,
	AF_STATE_P2P_GO_UP,
	AF_STATE_P2P_SESSION_STARTED,
	AF_STATE_P2P_SESSION_COMPLETE,
	AF_STATE_P2P_CONNECTED,
	AF_STATE_P2P_END_MARKER,
#endif
} app_ctrl_state_t;

static struct app_ctrl_curr_state {
	app_ctrl_state_t st_curr, st_next, uap_curr, uap_next,
		prov_curr, prov_next
#ifdef CONFIG_P2P
		, p2p_curr, p2p_next
#endif
	;
} s;
static char event_handled;
static int (*app_additional_sm)(app_ctrl_event_t event, void *data);
struct connection_state conn_state;

#define RECONN_ATTEMPTS_INFINITE    0

#define IGNORE_EVENT 1

/* message queue */
#define APP_CTRL_MAX_EVENTS 20
static os_queue_pool_define(app_ctrl_event_queue_data,
			    APP_CTRL_MAX_EVENTS * sizeof(app_ctrl_msg_t));
static os_queue_t app_ctrl_event_queue;
/* app ctrl event processing thread stack */
static os_thread_stack_define(app_ctrl_event_stack, 2048);
/* app ctrl event processing thread */
static os_thread_t app_ctrl_event_thread;

int (*dhcp_start_cb)(void *iface_handle);
void (*dhcp_stop_cb)();

#ifdef CONFIG_APP_FRM_PROV_WPS
os_semaphore_t app_wps_sem;
#endif

#ifdef CONFIG_DEBUG_BUILD
static const char *app_ctrl_state_name(app_ctrl_state_t state)
{
	const char *ptr;

	switch (state) {
	case AF_STATE_INIT_SYSTEM:
		ptr = "INIT_SYSTEM";
		break;
	case AF_STATE_PROV_START_MARKER:
		ptr = "PROV_START_MARKER";
		break;
	case AF_STATE_PROV_INIT:
		ptr = "PROV_INIT";
		break;
	case AF_STATE_PROV_STARTED:
		ptr = "PROV_STARTED";
		break;
	case AF_STATE_PROV_END_MARKER:
		ptr = "PROV_END_MARKER";
		break;
	case AF_STATE_NORMAL_START_MARKER:
		ptr = "NORMAL_START_MARKER";
		break;
	case AF_STATE_NORMAL_INIT:
		ptr = "NORMAL_INIT";
		break;
	case AF_STATE_NORMAL_CONNECTING:
		ptr = "NORMAL_CONNECTING";
		break;
	case AF_STATE_NORMAL_CONNECTED:
		ptr = "NORMAL_CONNECTED";
		break;
	case AF_STATE_NORMAL_DISCONNECTED:
		ptr = "NORMAL_DISCONNECTED";
		break;
	case AF_STATE_NORMAL_NW_CHANGED:
		ptr = "NORMAL_NW_CHANGED";
		break;
	case AF_STATE_NORMAL_END_MARKER:
		ptr = "NORMAL_END_MARKER";
		break;
	case AF_STATE_UNCONFIGURED:
		ptr = "UNCONFIGURED";
		break;
	case AF_STATE_CONFIGURED:
		ptr = "CONFIGURED";
		break;
	case AF_STATE_UAP_INIT:
		ptr = "UAP_INIT";
		break;
	case AF_STATE_UAP_STARTING:
		ptr = "UAP_STARTING";
		break;
	case AF_STATE_UAP_UP:
		ptr = "UAP_UP";
		break;
	case AF_STATE_PROV_RESET_TO_PROV:
		ptr = "RESET_TO_PROV";
		break;
	case AF_STATE_PROV_WPS_DONE:
		ptr = "PROV_WPS_DONE";
		break;
	case AF_STATE_PROV_DONE:
		ptr = "PROV_DONE";
		break;
#ifdef CONFIG_P2P
	case AF_STATE_P2P_INIT:
		ptr = "P2P_INIT";
		break;
	case AF_STATE_P2P_DEVICE_STARTED:
		ptr = "P2P_DEVICE_STARTED";
		break;
	case AF_STATE_P2P_GO_UP:
		ptr = "P2P_GO_UP";
		break;
	case AF_STATE_P2P_SESSION_STARTED:
		ptr = "P2P_SESSION_STARTED";
		break;
	case AF_STATE_P2P_SESSION_COMPLETE:
		ptr = "P2P_SESSION_COMPLETE";
		break;
	case AF_STATE_P2P_CONNECTED:
		ptr = "P2P_CONNECTED";
		break;
#endif /* CONFIG_P2P */
	default:
		ptr = "default";
		break;
	}

	return ptr;
}

#endif /* CONFIG_DEBUG_BUILD */

const char *app_ctrl_event_name(app_ctrl_event_t event)
{
	const char *ptr;

	switch (event) {
#ifdef CONFIG_APP_FRAME_INTERNAL_DEBUG
	case AF_EVT_INTERNAL_STATE_ENTER:
		ptr = "INTERNAL_STATE_ENTER";
		break;
	case AF_EVT_INTERNAL_WLAN_INIT_FAIL:
		ptr = "INTERNAL_WLAN_INIT_FAIL";
		break;
	case AF_EVT_INTERNAL_PROV_REQUESTED:
		ptr = "INTERNAL_PROV_REQUESTED";
		break;
	case AF_EVT_INTERNAL_PROV_NW_SET:
		ptr = "INTERNAL_PROV_NW_SET";
		break;
	case AF_EVT_INTERNAL_UAP_REQUESTED:
		ptr = "INTERNAL_UAP_REQUESTED";
		break;
	case AF_EVT_INTERNAL_UAP_STOP:
		ptr = "INTERNAL_UAP_STOP";
		break;
	case AF_EVT_INTERNAL_STA_REQUESTED:
		ptr = "INTERNAL_STA_REQUESTED";
		break;
#endif /* CONFIG_APP_FRAME_INTERNAL_DEBUG */
	case AF_EVT_INIT_DONE:
		ptr = "INIT_DONE";
		break;
	case AF_EVT_WLAN_INIT_DONE:
		ptr = "WLAN_INIT_DONE";
		break;
	case AF_EVT_CRITICAL:
		ptr = "CRITICAL";
		break;
	case AF_EVT_NORMAL_INIT:
		ptr = "NORMAL_INIT";
		break;
	case AF_EVT_NORMAL_CONNECTING:
		ptr = "NORMAL_CONNECTING";
		break;
	case AF_EVT_NORMAL_CONNECTED:
		ptr = "NORMAL_CONNECTED";
		break;
	case AF_EVT_NORMAL_CONNECT_FAILED:
		ptr = "NORMAL_CONNECT_FAILED";
		break;
	case AF_EVT_NORMAL_LINK_LOST:
		ptr = "NORMAL_LINK_LOST";
		break;
	case AF_EVT_NORMAL_CHAN_SWITCH:
		ptr = "NORMAL_CHAN_SWITCH";
		break;
	case AF_EVT_NORMAL_WPS_DISCONNECT:
		ptr = "NORMAL_WPS_DISCONNECT";
		break;
	case AF_EVT_NORMAL_USER_DISCONNECT:
		ptr = "NORMAL_USER_DISCONNECT";
		break;
	case AF_EVT_NORMAL_ADVANCED_SETTINGS:
		ptr = "NORMAL_ADVANCED_SETTINGS";
		break;
	case AF_EVT_NORMAL_DHCP_RENEW:
		ptr = "NORMAL_DHCP_RENEW";
		break;
	case AF_EVT_PROV_DONE:
		ptr = "PROV_DONE";
		break;
	case AF_EVT_PROV_WPS_SSID_SELECT_REQ:
		ptr = "PROV_WPS_SSID_SELECT_REQ";
		break;
	case AF_EVT_PROV_WPS_START:
		ptr = "PROV_WPS_START";
		break;
	case AF_EVT_PROV_WPS_REQ_TIMEOUT:
		ptr = "PROV_WPS_REQ_TIMEOUT";
		break;
	case AF_EVT_PROV_WPS_SUCCESSFUL:
		ptr = "PROV_WPS_SUCCESSFUL";
		break;
	case AF_EVT_PROV_WPS_UNSUCCESSFUL:
		ptr = "PROV_WPS_UNSUCCESSFUL";
		break;
	case AF_EVT_NORMAL_RESET_PROV:
		ptr = "NORMAL_RESET_PROV";
		break;
	case AF_EVT_UAP_STARTED:
		ptr = "UAP_STARTED";
		break;
	case AF_EVT_UAP_STOPPED:
		ptr = "UAP_STOPPED";
		break;
	case AF_EVT_PS_ENTER:
		ptr = "PS_ENTER";
		break;
	case AF_EVT_PS_EXIT:
		ptr = "PS_EXIT";
		break;
	case AF_EVT_PROV_CLIENT_DONE:
		ptr = "PROV_CLIENT_DONE";
		break;
	default:
		ptr = "default";
		break;
	}
	return ptr;
}

static void app_print_state_change(char *sm, app_ctrl_state_t curr,
				   app_ctrl_state_t next)
{
	if (curr != next) {
		app_l("app_ctrl [%s]: State Change: %s => %s",
		      sm, app_ctrl_state_name(curr),
		      app_ctrl_state_name(next));
	}
}

static int app_ctrl_attempt_reconnect(void)
{
	int ret;

	do {
		/* Initiate connection to network */
		ret = app_reconnect_wlan();
		if (ret != WM_SUCCESS) {
			app_e("failed to connect to network");
		}
	} while (ret != WM_SUCCESS);

	return WM_SUCCESS;
}

int app_ctrl_event_to_prj(app_ctrl_event_t event, void *data)
{
	if (g_ev_hdlr) {
		return (*g_ev_hdlr)((int)event, data);
	}
	return -WM_FAIL;
}

static int app_ctrl_crit_to_prj(int error, app_ctrl_state_t state)
{
	app_crit_data_t crit;
	crit.cd_error = error;
	return app_ctrl_event_to_prj(AF_EVT_CRITICAL, &crit);
}

static void app_ctrl_ignored_event(int event, int state)
{
	app_w("app_ctrl: Ignored event %d in state %d", event, state);
}

static int app_ctrl_state_init_system(struct app_init_state *state)
{

	int ret;

	/* Initialize the app psm module */
	ret = app_psm_init();
	if (ret) {
		app_e("app_main: failed to init app config");
		return -WM_E_AF_PSM_INIT;
	}

	app_l("app_ctrl: reset_to_factory=%d, prev_fw_version=%d",
		boot_factory_reset(), boot_old_fw_version());

	if (boot_factory_reset()) {
		state->factory_reset = 1;
	}

	if (boot_old_fw_version()) {
		state->backup_fw = 1;
	}
	state->rst_cause = boot_reset_cause();

	return WM_SUCCESS;
}

static void app_ctrl_prov_network_set(struct wlan_network *net)
{
	int ret = app_network_set_nw(net);
	if (ret != WM_SUCCESS) {
		/* If there is some problem in writing the network, it usually
		 * indicates that there is some corruption in psm. In this case
		 * we will trigger a reset-to-factory ourselves.
		 *
		 * This was seen in atleast 1 field deployment, and thus causing
		 * the device to never come back up in normal mode. Instead of
		 * leaving the fix as a policy to the application, we are just
		 * dealing with it in here.
		 */
		if (app_ctrl_crit_to_prj(ret, s.st_curr) !=
		    -WM_E_SKIP_DEF_ACTION) {
			psm_erase_and_init();
			app_reboot(REASON_NW_ADD_FAIL);
		}
	}
}

#ifdef CONFIG_P2P
app_conn_failure_reason_t reason;
static void app_ctrl_p2p_sm(app_ctrl_event_t event, void *data)
{
	int p2p_event_handled = 0;
	app_p2p_role_data_t *p2p_role_data;
	switch (s.p2p_curr) {
	case AF_STATE_P2P_INIT:
		if (event == AF_EVT_INTERNAL_P2P_REQUESTED) {
			app_start_p2p_network();
		} else if (event == AF_EVT_P2P_STARTED) {
			p2p_role_data = data;
			if (p2p_role_data->role == ROLE_AUTO_GO)
				s.p2p_next = AF_STATE_P2P_GO_UP;
			else
				s.p2p_next = AF_STATE_P2P_DEVICE_STARTED;
			p2p_event_handled = 1;
		}
		break;
	case AF_STATE_P2P_GO_UP:
		if (event == AF_EVT_UAP_STARTED) {
			app_l("app_p2p: uAP Started");
			if (dhcp_server_start(net_get_wfd_handle())) {
				app_e("app_p2p: Error in starting dhcp server");
			} else {
				app_d("app_p2p: DHCP server started on"
						" P2P Interface");
			}
			p2p_event_handled = 1;
		} else if (event == AF_EVT_INTERNAL_P2P_SESSION_REQUESTED) {
			app_d("app_p2p: P2P session requested");
			app_p2p_connect();
		} else if (event == AF_EVT_P2P_SESSION_STARTED) {
			app_d("app_p2p: P2P session with client started");
		} else if (event == AF_EVT_INTERNAL_P2P_AP_SESSION_SUCCESSFUL) {
			app_d("app_p2p: P2P session with client successful");
		} else if (event == AF_EVT_P2P_FINISHED) {
			s.p2p_next = AF_STATE_P2P_INIT;
			p2p_event_handled = 1;
		}
		break;
	case AF_STATE_P2P_DEVICE_STARTED:
		if (event == AF_EVT_INTERNAL_P2P_SESSION_REQUESTED) {
			app_d("app_p2p: P2P session requested");
			app_p2p_connect();
		} else if (event == AF_EVT_P2P_SESSION_STARTED) {
			app_d("app_p2p: P2P session started");
			s.p2p_next = AF_STATE_P2P_SESSION_STARTED;
			p2p_event_handled = 1;
		} else if (event == AF_EVT_P2P_FINISHED) {
			s.p2p_next = AF_STATE_P2P_INIT;
			p2p_event_handled = 1;
		}
		break;
	case AF_STATE_P2P_SESSION_STARTED:
		if (event == AF_EVT_P2P_ROLE_NEGOTIATED) {
			p2p_role_data = data;
			if (p2p_role_data->role == ROLE_GO) {
				app_d("app_p2p: GO role negotiated");
				s.p2p_next = AF_STATE_P2P_GO_UP;
			} else
				app_d("app_p2p: Client role negotiated");
			p2p_event_handled = 1;
		} else if (event == AF_EVT_INTERNAL_P2P_SESSION_SUCCESSFUL) {
			s.p2p_next = AF_STATE_P2P_SESSION_COMPLETE;
		} else if (event == AF_EVT_INTERNAL_P2P_SESSION_FAILED) {
			reason = P2P_CONNECT_FAILED;
			app_ctrl_event_to_prj(AF_EVT_NORMAL_CONNECT_FAILED,
					&reason);
			s.p2p_next = AF_STATE_P2P_DEVICE_STARTED;
		} else if (event == AF_EVT_P2P_FINISHED) {
			s.p2p_next = AF_STATE_P2P_INIT;
			p2p_event_handled = 1;
		}
		break;
	case AF_STATE_P2P_SESSION_COMPLETE:
		if (event == AF_EVT_INTERNAL_STATE_ENTER) {
			app_connect_to_p2p_go((struct wlan_network *)data);
		} else if (event == AF_EVT_NORMAL_CONNECTED) {
			app_d("app_p2p: Connected to GO");
			s.p2p_next = AF_STATE_P2P_CONNECTED;
			p2p_event_handled = 1;
		} else if ((event == AF_EVT_NORMAL_CONNECT_FAILED) ||
				(event == AF_EVT_NORMAL_LINK_LOST)) {
			s.p2p_next = AF_STATE_P2P_DEVICE_STARTED;
			p2p_event_handled = 1;
		}
		break;
	case AF_STATE_P2P_CONNECTED:
		if ((event == AF_EVT_NORMAL_CONNECT_FAILED) ||
				(event == AF_EVT_NORMAL_USER_DISCONNECT) ||
				(event == AF_EVT_NORMAL_LINK_LOST)) {
			s.p2p_next = AF_STATE_P2P_DEVICE_STARTED;
			p2p_event_handled = 1;
		} else if (event == AF_EVT_P2P_FINISHED) {
			s.p2p_next = AF_STATE_P2P_INIT;
			p2p_event_handled = 1;
		}
		break;
	default:
		break;
	}
	if (p2p_event_handled) {
		event_handled = 1;
		app_ctrl_event_to_prj(event, data);
	} else
		app_w("app_p2p: Ignored event %d in state %d",
				event, s.p2p_curr);
	app_print_state_change("p2p", s.p2p_curr, s.p2p_next);
	if (s.p2p_curr != s.p2p_next) {
		s.p2p_curr = s.p2p_next;
		app_ctrl_p2p_sm(AF_EVT_INTERNAL_STATE_ENTER, data);
	}
}
#endif /* CONFIG_P2P */
static void app_ctrl_uap_sm(app_ctrl_event_t event, void *data)
{
	int ret;

	switch (s.uap_curr) {
	case AF_STATE_UAP_INIT:
		if (event == AF_EVT_INTERNAL_UAP_REQUESTED) {
			ret = app_network_mgr_start_uap();
			if (ret != WM_SUCCESS)
				app_ctrl_crit_to_prj(ret, s.uap_curr);
			s.uap_next = AF_STATE_UAP_STARTING;
			event_handled = 1;
		}
		break;
	case AF_STATE_UAP_STARTING:
		if (event == AF_EVT_UAP_STARTED) {
			app_ctrl_event_to_prj(AF_EVT_UAP_STARTED, NULL);
			if (dhcp_start_cb) {
				if (dhcp_start_cb(net_get_uap_handle()))
					app_e("Error in starting dhcp"
					      "server\r\n");
			}
			s.uap_next = AF_STATE_UAP_UP;
			event_handled = 1;
		}
		break;
	case AF_STATE_UAP_UP:
		if (event == AF_EVT_INTERNAL_UAP_STOP) {
			if (dhcp_stop_cb) {
				dhcp_stop_cb();
				dhcp_stop_cb = NULL;
			}
			app_network_mgr_stop_uap();
			event_handled = 1;
		}
		if (event == AF_EVT_UAP_STOPPED) {
			app_network_mgr_remove_uap_network();
			app_ctrl_event_to_prj(AF_EVT_UAP_STOPPED, NULL);
			s.uap_next = AF_STATE_UAP_INIT;
			event_handled = 1;
		}
		break;
	default:
		break;
	}
	app_print_state_change("uap", s.uap_curr, s.uap_next);
	s.uap_curr = s.uap_next;
}

static int app_ctrl_prov_sm(app_ctrl_event_t event, void *data)
{

	app_d("app_ctrl_prov_sm: state = %s event = %s",
		app_ctrl_state_name(s.prov_curr), app_ctrl_event_name(event));

	switch (s.prov_curr) {
	case AF_STATE_PROV_INIT:
		if (event == AF_EVT_INTERNAL_PROV_REQUESTED) {
			s.prov_next = AF_STATE_PROV_STARTED;
			event_handled = 1;
		}
		break;
	case AF_STATE_PROV_STARTED:
#ifdef CONFIG_APP_FRM_PROV_WPS
		switch (event) {
		case AF_EVT_PROV_WPS_SSID_SELECT_REQ:
			app_ctrl_event_to_prj(event, NULL);
			event_handled = 1;
			/* Releasing semaphore so that app ctrl
			 * can start wps session */
			os_semaphore_put(&app_wps_sem);
			break;
		case AF_EVT_PROV_WPS_SUCCESSFUL:
			s.prov_next = AF_STATE_PROV_WPS_DONE;
			app_ctrl_event_to_prj(event, NULL);
			event_handled = 1;
			break;
		case AF_EVT_PROV_WPS_UNSUCCESSFUL:
		case AF_EVT_PROV_WPS_REQ_TIMEOUT:
		case AF_EVT_PROV_WPS_START:
			/* Forward the event */
			app_ctrl_event_to_prj(event, NULL);
			event_handled = 1;
			break;
		default:
			break;
		}
#endif
		/* AF_EVT_INTERNAL_PROV_NW_SET event can be received
		 * here from uAP provisioning.
		 */
		if (event == AF_EVT_INTERNAL_PROV_NW_SET) {
			app_ctrl_event_to_prj(AF_EVT_PROV_DONE,
					      data);
			s.prov_next = AF_STATE_PROV_DONE;
			event_handled = 1;
		}
		break;

#ifdef CONFIG_APP_FRM_PROV_WPS
	case AF_STATE_PROV_WPS_DONE:
		/* AF_EVT_INTERNAL_PROV_NW_SET event can be received
		 * here from WPS provisioning.
		 */
		if (event == AF_EVT_INTERNAL_PROV_NW_SET) {
			app_ctrl_event_to_prj(AF_EVT_PROV_DONE,
					      data);
			s.prov_next = AF_STATE_PROV_DONE;
			event_handled = 1;
		}
		break;
#endif
	case AF_STATE_PROV_DONE:
		/* Events are not be handled in this state because
		 * it is final state after uAP/WPS provisioning
		 * successful attempt.
		 */
		break;
	default:
		break;
	}
	app_print_state_change("prov", s.prov_curr, s.prov_next);
	s.prov_curr = s.prov_next;
	return WM_SUCCESS;
}

static void app_set_state_unconfigured()
{
	app_network_set_nw_state(APP_NETWORK_NOT_PROVISIONED);
	memset(&conn_state, 0, sizeof(conn_state));
	s.st_next = AF_STATE_UNCONFIGURED;
	s.prov_curr = s.prov_next = AF_STATE_PROV_INIT;
	app_ctrl_event_to_prj(AF_EVT_NORMAL_RESET_PROV, NULL);
}

static void app_ctrl_sm(app_ctrl_event_t event, void *data)
{
	int ret;
	static bool unload_network;

	app_d("app_ctrl_sm: state = %s event = %s",
		app_ctrl_state_name(s.st_curr), app_ctrl_event_name(event));

	/* Hand over prov events to the provisioning state machine */
	if (app_ctrl_prov_sm(event, data) == IGNORE_EVENT) {
		app_d("Prov: Ignore event %d", event);
		return;
	}

	if (app_additional_sm) {
		if ((*app_additional_sm)(event, data) == IGNORE_EVENT) {
			app_d("Additional SM: Ignore event %d", event);
			return;
		}
	}
	switch (s.st_curr) {
	case AF_STATE_INIT_SYSTEM:
		if (event == AF_EVT_INTERNAL_STATE_ENTER) {
			struct app_init_state init_state;

			memset(&init_state, 0, sizeof(init_state));
			/* Initialize all the modules in the system */
			ret = app_ctrl_state_init_system(&init_state);
			if (ret != WM_SUCCESS) {
				app_ctrl_crit_to_prj(ret, s.st_curr);
			}
			/* Call application callback to indicate module init */
			app_ctrl_event_to_prj(AF_EVT_INIT_DONE, &init_state);

			app_start_wlan();
			/* No state transitions yet, we stay in this state until
			 * we get a response from the wlcmgr.
			 */
			event_handled = 1;
		} else if (event == AF_EVT_WLAN_INIT_DONE) {
			int configured;
			/* The wlcmgr is up and running go to the next state */
			configured = app_network_get_nw_state();

			if (configured == APP_NETWORK_PROVISIONED) {
				g_wm_stats.wm_prov_type =
					APP_NETWORK_PROVISIONED;
				app_d("app_ctrl: starting in "
					"configured mode");
				s.st_next = AF_STATE_CONFIGURED;
			} else {
				/* Even if there is an error in reading the
				 * state, we go to provisioning mode.
				 */
				g_wm_stats.wm_prov_type =
				    APP_NETWORK_NOT_PROVISIONED;
				app_d("app_ctrl: starting in "
					"unconfigured mode");
				s.st_next = AF_STATE_UNCONFIGURED;
			}
			app_ctrl_event_to_prj(AF_EVT_WLAN_INIT_DONE,
					      (void *)configured);
			event_handled = 1;
		} else if (event == AF_EVT_INTERNAL_WLAN_INIT_FAIL) {
			/* Problem starting the wlcmgr */
			app_ctrl_crit_to_prj(-WM_E_AF_WLCMGR_FAIL, s.st_curr);
			event_handled = 1;
		}
		break;
	case AF_STATE_NORMAL_INIT:
		/* Note: This state can be folded into the NORMAL_CONNECTING
		 * state below */
		if (event == AF_EVT_INTERNAL_STATE_ENTER) {
			/* Start normal network */
			app_ctrl_event_to_prj(AF_EVT_NORMAL_INIT, NULL);

			s.st_next = AF_STATE_NORMAL_CONNECTING;
			event_handled = 1;
		}
		break;
	case AF_STATE_NORMAL_CONNECTING:
		switch (event) {
		case AF_EVT_INTERNAL_STATE_ENTER:
			conn_state.state = CONN_STATE_CONNECTING;
			/* Load the configured network and connect to it */
			app_ctrl_event_to_prj(AF_EVT_NORMAL_CONNECTING, NULL);
			/* If a valid network is already loaded, this will just
			 * return success */
			ret = app_load_configured_network();
			if (ret != WM_SUCCESS) {
				/* Notify the application if the network data
				 * couldn't be read. */
				app_ctrl_crit_to_prj(ret, s.st_curr);
			} else {
				ret = app_connect_wlan();
				if (ret != WM_SUCCESS) {
					if (sta_stop_cmd == true) {
						sta_stop_cmd = false;
						app_ctrl_notify_event(
						AF_EVT_NORMAL_USER_DISCONNECT,
						NULL);
					} else {
						app_ctrl_attempt_reconnect();
					}
				}
			}
			/* No state transitions yet, we stay in this state until
			 * we get a response from the wlcmgr. */
			event_handled = 1;
			break;
		case AF_EVT_NORMAL_CONNECTING:
			app_ctrl_event_to_prj(AF_EVT_NORMAL_CONNECTING, NULL);
			event_handled = 1;
			break;
		case AF_EVT_NORMAL_CONNECTED:
			s.st_next = AF_STATE_NORMAL_CONNECTED;
			event_handled = 1;
			break;
		case AF_EVT_NORMAL_CONNECT_FAILED:
#ifdef CONFIG_AUTO_TEST_BUILD
			wmprintf("%d attempt to reconnect\r\n",
				 app_reconnect_attempts);
			if (app_reconnect_attempts > 5) {
				wmprintf("Erasing psm ....\r\n");
				psm_erase();
				wmprintf("Psm erased due to connection failure,"
					 " device will reboot\r\n");
				pm_reboot_soc();
				break;
			}
#endif

			if (*(app_conn_failure_reason_t *)data
			    == AUTH_FAILED) {
				conn_state.auth_failed++;
			} else if (*(app_conn_failure_reason_t *)data
				   == DHCP_FAILED) {
				conn_state.dhcp_failed++;
			} else {
				conn_state.conn_failed++;
			}

			if (unload_network) {
				app_unload_configured_network();
				app_load_configured_network();
				unload_network = 0;
			}

			ret =  app_ctrl_event_to_prj(event, data);
			if (sta_stop_cmd == true) {
				sta_stop_cmd = false;
				app_ctrl_notify_event(
						AF_EVT_NORMAL_USER_DISCONNECT,
						NULL);
				s.st_next = AF_STATE_NORMAL_DISCONNECTED;

			} else {
				if (!ret) {
					app_ctrl_attempt_reconnect();
				} else {
					s.st_next =
						AF_STATE_NORMAL_DISCONNECTED;
				}
			}
			event_handled = 1;
			/* We continue to be in the connecting state */
			break;
		/* While the state machine is in
		 * AF_STATE_NORMAL_CONNECTING state, WiFi
		 * connection get established first followed
		 * by IP address assignment.
		 * State machine goes to next state -
		 * AF_STATE_NORMAL_CONNECTED state which
		 * means 'connected at IP layer (i.e. configured
		 * successfully with IP address)'.
		 *
		 * Link loss can happen once the WiFi connection
		 * is Up, and before the IP address assignment
		 * Hence handle link loss event while in CONNECTING state.
		 */
		case AF_EVT_NORMAL_LINK_LOST:
			app_handle_link_loss();
			event_handled = 1;
			break;
		case AF_EVT_NORMAL_USER_DISCONNECT:
			ret = app_unload_configured_network();
			if (ret == -WM_E_AF_NW_DEL) {
				/* wlcmgr isn't allowing us to remove the
				 * network for some reason. Wait for its
				 * activity to be over and then retry
				 */
				int i;
				for (i = 0; i < 3; i++) {
					os_thread_sleep(4000);
					ret = app_unload_configured_network();
					if (ret == WM_SUCCESS)
						break;
				}
			}
			if (ret != WM_SUCCESS) {
				app_e("Couldn't unload older network\r\n");
			}
			memset(&conn_state, 0, sizeof(conn_state));
			s.st_next = AF_STATE_NORMAL_DISCONNECTED;
			app_ctrl_event_to_prj(AF_EVT_NORMAL_USER_DISCONNECT,
					      NULL);
			event_handled = 1;
			break;
		case AF_EVT_NORMAL_ADVANCED_SETTINGS:
		{
			struct wlan_network *net = data;
			app_ctrl_prov_network_set(net);
			/* Cannot unload the network here since the wlcmgr will
			 * not remove it if the connection is in-progress.
			 */
			unload_network = 1;
			/* Reset the statistics */
			memset(&conn_state, 0, sizeof(conn_state));
			app_ctrl_event_to_prj(
				AF_EVT_NORMAL_ADVANCED_SETTINGS,
				data);
			break;
		}
		case AF_EVT_NORMAL_RESET_PROV:
			s.st_next = AF_STATE_PROV_RESET_TO_PROV;
			event_handled = 1;
			break;
		default:
			break;
		}
		break;
	case AF_STATE_NORMAL_CONNECTED:
		switch (event) {
		case AF_EVT_INTERNAL_STATE_ENTER:
			conn_state.conn_success++;
			conn_state.state = CONN_STATE_CONNECTED;
			app_ctrl_event_to_prj(AF_EVT_NORMAL_CONNECTED, NULL);
			/* No state transitions yet, we stay in this state until
			 * there is any other event. */
			event_handled = 1;
			break;
		case AF_EVT_NORMAL_CONNECT_FAILED:
			/* Sometimes, the AP sends a de-auth event in connected
			 * state. And, AF_EVT_NORMAL_CONNECT_FAILED event is
			 * received. So, for this purpose the case is handled
			 * here.
			 */
			app_ctrl_event_to_prj(AF_EVT_NORMAL_CONNECT_FAILED,
					      NULL);
			/* Stay in the same state */
			event_handled = 1;
			s.st_next = AF_STATE_NORMAL_CONNECTING;
			break;
		case AF_EVT_NORMAL_LINK_LOST:
			app_handle_link_loss();
			event_handled = 1;
			break;
		case AF_EVT_NORMAL_CHAN_SWITCH:
			app_handle_chan_switch();
			event_handled = 1;
			break;
		case AF_EVT_NORMAL_DHCP_RENEW:
			app_ctrl_event_to_prj(AF_EVT_NORMAL_DHCP_RENEW, NULL);
			event_handled = 1;
			break;
		case AF_EVT_NORMAL_USER_DISCONNECT:
			app_ctrl_event_to_prj(
				AF_EVT_NORMAL_USER_DISCONNECT,
				NULL);
			s.st_next = AF_STATE_NORMAL_DISCONNECTED;
			event_handled = 1;
			break;
		case AF_EVT_NORMAL_ADVANCED_SETTINGS:
		{
			struct wlan_network *net = data;
			app_ctrl_prov_network_set(net);
			/* Reset the statistics */
			memset(&conn_state, 0, sizeof(conn_state));
			app_ctrl_event_to_prj(
				AF_EVT_NORMAL_ADVANCED_SETTINGS,
				data);
			s.st_next = AF_STATE_NORMAL_NW_CHANGED;
			event_handled = 1;
		}
			break;
		case AF_EVT_NORMAL_RESET_PROV:
			event_handled = 1;
			s.st_next = AF_STATE_PROV_RESET_TO_PROV;
			break;
		case AF_EVT_PROV_CLIENT_DONE:
			app_ctrl_event_to_prj(AF_EVT_PROV_CLIENT_DONE, NULL);
			event_handled = 1;
			break;
		default:
			break;
		}
		break;
	case AF_STATE_PROV_RESET_TO_PROV:
		switch (event) {
		case AF_EVT_INTERNAL_STATE_ENTER:
			ret = app_network_set_nw_state
				(APP_NETWORK_NOT_PROVISIONED);
			if (ret != WM_SUCCESS) {
				ret = app_ctrl_crit_to_prj(ret, s.st_curr);
				if (ret != -WM_E_SKIP_DEF_ACTION) {
					psm_erase_and_init();
					app_reboot(REASON_NW_ADD_FAIL);
				}
			}
			app_ctrl_event_to_prj(AF_EVT_NORMAL_PRE_RESET_PROV,
					      NULL);
			wlan_disconnect();
			/* Reset the statistics */
			memset(&conn_state, 0, sizeof(conn_state));
			event_handled = 1;
			break;
		case AF_EVT_NORMAL_CONNECT_FAILED:
		case AF_EVT_NORMAL_USER_DISCONNECT:
			ret = app_unload_configured_network();
			if (ret == -WM_E_AF_NW_DEL) {
				/* wlcmgr isn't allowing us to remove the
				 * network for some reason. Wait for its
				 * activity to be over and then retry
				 */
				int i;
				for (i = 0; i < 3; i++) {
					os_thread_sleep(4000);
					ret = app_unload_configured_network();
					if (ret == WM_SUCCESS)
						break;
				}
			}
			if (ret != WM_SUCCESS) {
				app_e("Couldn't unload older network\r\n");
			}
			s.st_next = AF_STATE_UNCONFIGURED;
			s.prov_curr = s.prov_next = AF_STATE_PROV_INIT;
			app_ctrl_event_to_prj(AF_EVT_NORMAL_RESET_PROV, NULL);
			event_handled = 1;
			break;
		default:
			break;
		}
		break;
	case AF_STATE_NORMAL_DISCONNECTED:
		switch (event) {
		case AF_EVT_INTERNAL_STATE_ENTER:
			/* We enter this state only if the user (from the CLI)
			 * or the application (using wlan_disconnect())
			 * disconnects from the network that we have connected
			 * to.
			 *
			 * If the user now connects to a different network, we
			 * will go back to our primary network on link-loss
			 *
			 */
			/* Nothing to do */
			app_d("\r\nReceived wlan-disconnect from user ");
			sta_stop_cmd = false;
			event_handled = 1;
			break;
		case AF_EVT_NORMAL_CONNECTED:
			s.st_next = AF_STATE_NORMAL_CONNECTED;
			event_handled = 1;
			break;
		case AF_EVT_NORMAL_CONNECT_FAILED:
			app_w("Connection attempt failed");
			app_ctrl_event_to_prj(AF_EVT_NORMAL_CONNECT_FAILED,
					      data);
			/* Stay in the same state */
			event_handled = 1;
			break;
		case AF_EVT_NORMAL_ADVANCED_SETTINGS:
		{
			struct wlan_network *net = data;
			app_ctrl_prov_network_set(net);
			app_unload_configured_network();
			/* Reset the statistics */
			memset(&conn_state, 0, sizeof(conn_state));
			app_ctrl_event_to_prj(
				AF_EVT_NORMAL_ADVANCED_SETTINGS,
				data);
			s.st_next = AF_STATE_NORMAL_INIT;
			event_handled = 1;
			break;
		}
		case AF_EVT_NORMAL_RESET_PROV:
			app_set_state_unconfigured();
			app_unload_configured_network();
			break;
		case AF_EVT_INTERNAL_STA_REQUESTED:
			/* Start station */
			s.st_next = AF_STATE_NORMAL_INIT;
			event_handled = 1;
			break;
		default:
			break;
		}
		break;
	case AF_STATE_NORMAL_NW_CHANGED:
		switch (event) {
		case AF_EVT_INTERNAL_STATE_ENTER:
			/* Disconnect from the already connected network */
			wlan_disconnect();
			event_handled = 1;
			break;
		case AF_EVT_NORMAL_USER_DISCONNECT:
			/* We unload the network so that in the connecting
			 * state, the new network is read from the flash and
			 * used. */
			app_unload_configured_network();
			s.st_next = AF_STATE_NORMAL_CONNECTING;
			event_handled = 1;
		default:
			break;
		}
		break;
	case AF_STATE_UNCONFIGURED:
		switch (event) {
		case AF_EVT_INTERNAL_STATE_ENTER:
			/* Nothing to do */
			event_handled = 1;
			break;
		case AF_EVT_INTERNAL_PROV_NW_SET:
			/* Although the NORMAL_CONNECTING state reads and loads
			 * the network from psm, we want pre-load that network
			 * here. The reasoning is that, the data that we
			 * received here contains a valid 'channel' field, while
			 * the data in the psm sets the 'channel' field to 0.
			 *
			 * Why do we need channel field here? This makes the
			 * post-provisioning experience smoother, since then the
			 * connection scan only scans a single channel, and
			 * hence the uap connectivity is not hampered much.
			 *
			 * Why do we not need channel in psm? For roaming the in
			 * an WDS setup, there could be multiple APs with the
			 * same SSID on different channels. We want to enable
			 * that functionality too.
			 */
			app_load_this_network((struct wlan_network *)data);
			((struct wlan_network *)data)->channel = 0;
			/* Write network */
			app_ctrl_prov_network_set((struct wlan_network *)data);
			/* Jump to normal mode */
			s.st_next = AF_STATE_NORMAL_INIT;
			event_handled = 1;
			break;
		case AF_EVT_INTERNAL_STA_REQUESTED:
			/* If a valid network has already been loaded, it means
			 * that the application wants to override the
			 * unconfigured state and hence the state will change
			 * to the normal init state.
			 */
			if (app_check_valid_network_loaded()) {
				/* Start station */
				app_l("app_ctrl: "
					"Connecting to the loaded network");
				s.st_next = AF_STATE_NORMAL_INIT;
			}
			event_handled = 1;
			break;
		default:
			break;
		}
		break;
	case AF_STATE_CONFIGURED:
		switch (event) {
		case AF_EVT_INTERNAL_STATE_ENTER:
			/* Nothing to do */
			event_handled = 1;
			break;
		case AF_EVT_INTERNAL_STA_REQUESTED:
			/* Start station */
			s.st_next = AF_STATE_NORMAL_INIT;
			event_handled = 1;
			break;
		case AF_EVT_NORMAL_RESET_PROV:
			app_set_state_unconfigured();
			break;
		default:
			break;
		}
		break;
	default:
		app_w("Invalid State %d", s.st_curr);
		break;
	}
	/* Hand over uap events to the uap state machine */
	app_ctrl_uap_sm(event, data);

#ifdef CONFIG_P2P
	/* Hand over events to the p2p state machine */
	app_ctrl_p2p_sm(event, data);
#endif /* CONFIG_P2P */
	if (!event_handled) {
		app_ctrl_ignored_event(event, s.st_curr);
/* Pass the unhandled events to applications as these may be the
 * application specific events.
 */
		if (event > APP_CTRL_EVT_MAX)
			app_ctrl_event_to_prj(event, data);
	}
	event_handled = 0;
}

static void app_ctrl_event_handler(os_thread_arg_t arg)
{
	int ret;
	app_ctrl_msg_t msg;

	while (1) {
		/* Receive message on queue */
		ret = os_queue_recv(&app_ctrl_event_queue, &msg,
				    OS_WAIT_FOREVER);
		if (ret != WM_SUCCESS) {
			app_e("app_ctrl: failed to get message "
				"from queue [%d]", ret);
			continue;
		}
		if (msg.event == AF_EVT_PS_ENTER ||
		    msg.event == AF_EVT_PS_EXIT) {
			app_ctrl_event_to_prj(msg.event, msg.data);
			/* Since these events are not
			 * dependent on state continue
			 * to receive next message
			 */
			continue;
		}
		/* Dispatch to state machine */
		app_ctrl_sm((app_ctrl_event_t) msg.event, msg.data);

		while (s.st_curr != s.st_next) {
			/* If the state machine should proceed to a new state,
			 * update the state and send a STATE_ENTER event. This
			 * allows the state machine to perform any actions on
			 * entry into a state.
			 */
			app_print_state_change("sta", s.st_curr, s.st_next);
			s.st_curr = s.st_next;
			app_ctrl_sm(AF_EVT_INTERNAL_STATE_ENTER, NULL);
		}
	}
}

static int __app_ctrl_notify_event(app_ctrl_event_t event, void *data, int flag)
{
	int ret;
	app_ctrl_msg_t msg;
	msg.event = event;
	msg.data = data;

	ret = os_queue_send(&app_ctrl_event_queue, &msg, flag);
	if (ret != WM_SUCCESS) {
		app_e("app_taskctrl: failed to send event(%d) on queue",
			event);
	}
	return ret;
}

int app_ctrl_notify_event(app_ctrl_event_t event, void *data)
{
	return __app_ctrl_notify_event(event, data, OS_NO_WAIT);
}

int app_ctrl_notify_event_wait(app_ctrl_event_t event, void *data)
{
	return __app_ctrl_notify_event(event, data, OS_WAIT_FOREVER);
}


int app_ctrl_notify_application_event(app_ctrl_event_t event, void *data)
{
	return app_ctrl_notify_event(event, data);
}
int app_reset_configured_network()
{
	return app_ctrl_notify_event(AF_EVT_NORMAL_RESET_PROV, NULL);
}

int app_ctrl_init()
{
	int ret;

	s.st_curr = s.st_next = AF_STATE_INIT_SYSTEM;
	s.uap_curr = s.uap_next = AF_STATE_UAP_INIT;
	s.prov_curr = s.prov_next = AF_STATE_PROV_INIT;
#ifdef CONFIG_P2P
	s.p2p_curr = s.p2p_next = AF_STATE_P2P_INIT;
#endif

	sta_stop_cmd = false;

#ifdef CONFIG_APP_FRM_PROV_WPS
	ret = os_semaphore_create(&app_wps_sem, "wps_semaphore");
	if (ret) {
		app_e("app_ctrl: failed to create wps semaphore: %d", ret);
		return ret;
	}
	/* Get semaphore for first time so that next get will be blocking*/
	os_semaphore_get(&app_wps_sem, OS_WAIT_FOREVER);
#endif
	/* Initialize the event queue */
	ret = os_queue_create(&app_ctrl_event_queue, "app_ctrl_event_queue",
			      sizeof(app_ctrl_msg_t),
			      &app_ctrl_event_queue_data);
	if (ret) {
		app_e("app_crl: failed to create message queue: %d", ret);
		return -1;
	}

	/* Initialize the app_ctrl event processing task */
	ret = os_thread_create(&app_ctrl_event_thread,
			       "app_ctrl_event_task",
			       app_ctrl_event_handler,
			       0, &app_ctrl_event_stack, OS_PRIO_3);
	if (ret) {
		app_e("app_ctrl: failed to create thread: %d", ret);
		return -1;
	}

	/* Send init event to state machine */
	app_ctrl_notify_event(AF_EVT_INTERNAL_STATE_ENTER, NULL);

	return 0;
}

int app_sta_stop()
{
	if (wlan_disconnect() == WLAN_ERROR_NONE) {
		sta_stop_cmd = true;
		return WM_SUCCESS;
	}
	return -WM_FAIL;
}

/*
 * For now, we return true, if we are provisioned, false, otherwise. 
 */
int app_ctrl_getmode()
{
	return (s.st_curr > AF_STATE_NORMAL_START_MARKER &&
		s.st_curr < AF_STATE_NORMAL_END_MARKER);
}

/* Hook to change internal app ctrl state machine's state */
void app_ctrl_set_state(app_ctrl_state_t state)
{
	s.st_next = state;
}

/* Application can override this with their link loss event handler call to
 * perform any app specific task.
 */
void app_handle_link_loss()
{
	app_ctrl_event_to_prj(AF_EVT_NORMAL_LINK_LOST, NULL);
	app_ctrl_set_state(AF_STATE_NORMAL_CONNECTING);
	return;
}

void app_handle_chan_switch()
{
	app_ctrl_event_to_prj(AF_EVT_NORMAL_CHAN_SWITCH, NULL);
	app_ctrl_set_state(AF_STATE_NORMAL_CONNECTING);
	return;
}

int app_add_sm(int (*sm_func_ptr)(app_ctrl_event_t event, void *data))
{
	if (app_additional_sm)
		return -WM_FAIL;
	app_additional_sm = sm_func_ptr;
	return WM_SUCCESS;
}

int app_delete_sm(int (*sm_func_ptr)(app_ctrl_event_t event, void *data))
{
	if (app_additional_sm != sm_func_ptr)
		return -WM_FAIL;
	app_additional_sm = NULL;
	return WM_SUCCESS;
}
