#include <stdlib.h>
#include <p2p.h>
#include <app_framework.h>
#include <wm_net.h>

#include "app_ctrl.h"
#include "app_dbg.h"

app_p2p_role_data_t p2p_role_data;
static int p2p_callback(enum p2p_event event, void *data, uint16_t len)
{
	switch (event) {
	case P2P_STARTED:
		app_l("P2P Started");
		break;
	case P2P_AUTO_GO_STARTED:
		app_l("P2P Auto GO Started");
		p2p_role_data.role = ROLE_AUTO_GO;
		p2p_role_data.data = data;
		app_ctrl_notify_event(AF_EVT_P2P_STARTED, &p2p_role_data);
		break;
	case P2P_DEVICE_STARTED:
		p2p_role_data.role = ROLE_UNDECIDED;
		p2p_role_data.data = data;
		app_ctrl_notify_event(AF_EVT_P2P_STARTED, &p2p_role_data);
		app_l("P2P Device Started");
		break;
	case P2P_CLIENT_STARTED:
		app_l("P2P Client Started");
		p2p_role_data.role = ROLE_CLIENT;
		p2p_role_data.data = data;
		app_ctrl_notify_event(AF_EVT_P2P_ROLE_NEGOTIATED,
				&p2p_role_data);
		break;
	case P2P_GO_STARTED:
		app_l("P2P GO Started");
		p2p_role_data.role = ROLE_GO;
		p2p_role_data.data = data;
		app_ctrl_notify_event(AF_EVT_P2P_ROLE_NEGOTIATED,
				&p2p_role_data);
		break;
	case P2P_SESSION_STARTED:
		app_l("P2P Session Started");
		app_ctrl_notify_event(AF_EVT_P2P_SESSION_STARTED, data);
		break;
	case P2P_SESSION_PIN_CHKSUM_FAILED:
		app_e("P2P PIN Checksum Failed");
		app_ctrl_notify_event(AF_EVT_INTERNAL_P2P_SESSION_FAILED, data);
		break;
	case P2P_SESSION_ABORTED:
		app_w("P2P Session Aborted");
		app_ctrl_notify_event(AF_EVT_INTERNAL_P2P_SESSION_FAILED, data);
		break;
	case P2P_SESSION_TIMEOUT:
		app_e("P2P Session Failed");
		app_ctrl_notify_event(AF_EVT_INTERNAL_P2P_SESSION_FAILED, data);
		break;
	case P2P_SESSION_SUCCESSFUL:
		app_l("P2P Session Successful");
		app_ctrl_notify_event(AF_EVT_INTERNAL_P2P_SESSION_SUCCESSFUL,
				data);
		break;
	case P2P_AP_SESSION_SUCCESSFUL:
		app_l("P2P AP Session Successful");
		app_ctrl_notify_event(AF_EVT_INTERNAL_P2P_AP_SESSION_SUCCESSFUL,
				data);
		break;
	case P2P_SESSION_FAILED:
		app_e("P2P Session Failed");
		app_ctrl_notify_event(AF_EVT_INTERNAL_P2P_SESSION_FAILED, data);
		break;
	case P2P_FAILED:
		app_e("P2P Failed");
		app_ctrl_notify_event(AF_EVT_P2P_FAILED, data);
		break;
	case P2P_FINISHED:
		app_l("P2P Finished");
		app_ctrl_notify_event(AF_EVT_P2P_FINISHED, data);
		break;
	}
	return WM_SUCCESS;
}

static struct p2p_config p2p_conf = {
	.role = 4,
	.auto_go = ROLE_UNDECIDED,
	.pin_generator = 1,
	.version = 0x20,
	.version2 = 0x20,
	.device_name = "Marvell-WM",
	.manufacture = "Marvell",
	.model_name = "MC200-878x",
	.model_number = "0001",
	.serial_number = "0001",
	.config_methods = 0x188,
	.primary_dev_category = 01,
	.primary_dev_subcategory = 01,
	.rf_bands = 1,
	.os_version = 0xFFFFFFFF,
	.p2p_msg_max_retry = 5,
	.p2p_msg_timeout = 5000,
	.pin_len = 8,
	.p2p_callback = p2p_callback,
};

struct p2p_scan_result *app_p2p_scan(int *no)
{
	return p2p_scan(no);
}

static unsigned int ipin, pin_flag;
static struct p2p_scan_result p2p_remote, *scan_ptr;
int app_p2p_connect()
{
	int ret;
	if (pin_flag)
		ret = p2p_connect(CMD_P2P_PIN, ipin, scan_ptr);
	else
		ret = p2p_connect(CMD_P2P_PBC, 0, scan_ptr);
	return ret;
}
int app_p2p_session_start(char *pin, struct p2p_scan_result *remote)
{
	if (pin) {
		/* This is a pin session */
		ipin = atoi(pin);
		pin_flag = 1;
	} else {
		/* This is a PBC session */
		pin_flag = 0;
	}
	if (remote) {
		memcpy(&p2p_remote, remote, sizeof(struct p2p_scan_result));
		scan_ptr = &p2p_remote;
	} else
		scan_ptr = NULL;
	app_ctrl_notify_event(AF_EVT_INTERNAL_P2P_SESSION_REQUESTED, NULL);
	return WM_SUCCESS;
}

int app_p2p_disconnect()
{
	return p2p_disconnect();
}
int app_start_p2p_network()
{
	int ret;
	ret = p2p_start(&p2p_conf);
	if (ret != WM_SUCCESS)
		app_e("failed to start P2P %d", ret);
	return ret;
}

int app_p2p_start(app_p2p_role_t role)
{
	if (role == ROLE_AUTO_GO) {
		p2p_conf.auto_go = ROLE_AUTO_GO;
	} else {
		p2p_conf.auto_go = ROLE_UNDECIDED;
	}
	app_ctrl_notify_event(AF_EVT_INTERNAL_P2P_REQUESTED, NULL);
	return WM_SUCCESS;
}

int app_p2p_stop()
{
	int ret;

	ret = p2p_stop();

	if (ret != WM_SUCCESS)
		app_e("failed to stop P2P");
	return ret;
}

#define DEF_NET_NAME	    "default"
int app_connect_to_p2p_go(struct wlan_network *net)
{
	app_l("connecting to P2P GO");
	memcpy(net->name, DEF_NET_NAME, sizeof(net->name));
	int ret = wlan_add_network(net);
	if (ret != WLAN_ERROR_NONE) {
		app_e("network_mgr: failed to add network %d", ret);
		return -WM_E_AF_NW_ADD;
	}
	wlan_connect(net->name);
	return WM_SUCCESS;
}

int app_p2p_ip_get(char *ipaddr)
{
	int ret;
	struct wlan_ip_config addr;

	ret = wlan_get_wfd_address(&addr);
	if (ret != WLAN_ERROR_NONE) {
		app_w("app_network_config: failed to get IP address");
		strcpy(ipaddr, "0.0.0.0");
		return -WM_FAIL;
	}
	net_inet_ntoa(addr.ip, ipaddr);
	return WM_SUCCESS;
}
