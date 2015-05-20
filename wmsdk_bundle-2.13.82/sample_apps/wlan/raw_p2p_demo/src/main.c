/*
 *  Copyright (C) 2008-2013, Marvell International Ltd.
 *  All Rights Reserved.
 */

/*
 * Simple Wi-Fi Direct (P2P) Application
 *
 * Summary:
 *
 * This application shows how to use WMSDK Wi-Fi Direct feature.
 *
 * Description:
 *
 * WLAN Initialization:
 *
 * When WLAN subsystem is started, it starts up the P2P
 * sub-system and initializes the device as P2P device. We receive
 * the event when the WLAN subsystem has been started and initialized.
 *
 * On boot up device comes up as a Wi-Fi Direct device and can be
 * discoverable on Wi-Fi Direct supported Smartphones.
 *
 * When user initiates connection request from Smartphone to device
 * or vice versa P2P registration protocols get starts and
 * on success device gets connected with Smartphone.
 *
 */

#include <wm_os.h>
#include <arch/sys.h>
#include <wmtime.h>
#include <partition.h>
#include <appln_cb.h>
#include <appln_dbg.h>
#include <cli.h>
#include <cli_utils.h>
#include <wmstdio.h>
#include <wmsysinfo.h>
#include <wm_net.h>
#include <httpd.h>
#include <led_indicator.h>
#include <board.h>
#include <dhcp-server.h>
#include <wmtime.h>
#include <psm.h>
#include <ftfs.h>
#include <mdev_gpio.h>
#include <wlan.h>
#include <p2p.h>

#include "p2p_helper.h"

/* Enable this macro to have p2p scan results print on console. */
/* #define PRINT_SCAN_RESULTS */

/* Thread handle */
static os_thread_t app_thread;
/* Buffer to be used as stack */
static os_thread_stack_define(app_stack, 512);

#define LISTEN_INTERVAL 200

#define WL_ID_P2P_DEEP_SLEEP "p2p_deep_sleep"

#define P2P_TIMEOUT	(300 * 1000) /* 5 Minutes */
os_timer_t p2p_timeout_timer;

int p2p_device_started;
int p2p_session_on;
int num_scan_results;
struct p2p_scan_result *p2p_scan_results;
static int do_sleep;
static int p2p_timeout;

static void p2p_demo_simulate_p2p_disconnect(int argc, char **argv)
{
	p2p_disconnect();
}

void p2p_timeout_timer_handler(os_timer_arg_t arg)
{
	dbg("P2P Timeout occurred");
	wakelock_get(WL_ID_P2P_DEEP_SLEEP);
	do_sleep = 1;
}

static struct cli_command p2p_cmds[] = {
	{"p2p-disconnect", NULL, p2p_demo_simulate_p2p_disconnect},
};

static void p2p_demo_main(os_thread_arg_t data)
{
#ifdef PRINT_SCAN_RESULTS
	int i, j, len;
#endif

	dbg("P2P Demo : Main Thread Started");
	while (1) {
		if (p2p_device_started && !p2p_session_on) {
			p2p_scan_results = p2p_scan(&num_scan_results);
#ifdef PRINT_SCAN_RESULTS
			if (num_scan_results) {
				wmprintf("");
				dbg("P2P Scan Result : %d "
						"entries in List",
						num_scan_results);
				dbg
					("---------------------------------"
					 "---------------------------------"
					 "-------------------------");
				dbg
					("  # |    Intf Address   |       "
					"Device Name        |  Op mode |   "
					"Dev Address     |  SSID |");
				dbg
					("---------------------------------"
					 "---------------------------------"
					 "-------------------------");

				for (j = 0; j < num_scan_results;
						j++) {
					/* Index Number */
					wmprintf("\t %02d | ", j);

					/* Device ID */
					print_mac((const char *)
						p2p_scan_results[j].device_id);
					wmprintf("|");
					/** Device Name printing in smart way */
					len =
						((MAX_DEVICE_NAME_LEN - 6) -
						 strlen((const char *)
					p2p_scan_results[j].device_name)) / 2;
					for (i = 0; i < len; i++)
						wmprintf(" ");
					for (i = 0; i < strlen((const char *)
					p2p_scan_results[j].device_name); i++)
						wmprintf("%c",
					p2p_scan_results[j].device_name[i]);
					for (i = 0; i <
						((MAX_DEVICE_NAME_LEN - 6) -
						strlen((const char *)
							p2p_scan_results[j].
						device_name) - len); i++)
						wmprintf(" ");
					wmprintf("|");

					/* Group Capability */
					if (!(p2p_scan_results[j].
						group_capability &
							P2P_GROUP_OWNER_MASK))
						wmprintf("  Device  |");
					else
						wmprintf("    GO    |");

					if (memcmp(p2p_scan_results[j].
							device_address,
						"\x00\x00\x00\x00\x00\x00", 6)
						) {
						wmprintf(" ");
						print_mac((const char *)
							p2p_scan_results[j].
								device_address);
						wmprintf("|");
					}
					if (p2p_scan_results[j].group_capability
						 & P2P_GROUP_OWNER_MASK)
						wmprintf("  %s",
							p2p_scan_results
							[j].go_ssid);
					wmprintf("");
				}
				wmprintf("\r\n");
			} else
				dbg("P2P device not "
						"found");
#endif
		}
		if (do_sleep) {
			hp_unconfigure_p2p_pushbutton();
			app_p2p_stop();
			do_sleep = 0;
		}
		/* Sleep 5 seconds */
		os_thread_sleep(os_msec_to_ticks(5000));
	}
}

void ps_state_to_desc(char *ps_state_desc, int ps_state)
{
	switch (ps_state) {
	case WLAN_IEEE:
		strcpy(ps_state_desc, "IEEE PS");
		break;
	case WLAN_DEEP_SLEEP:
		strcpy(ps_state_desc, "Deep sleep");
		break;
	case WLAN_PDN:
		strcpy(ps_state_desc, "Power down");
		break;
	case WLAN_ACTIVE:
		strcpy(ps_state_desc, "WLAN Active");
		break;
	default:
		strcpy(ps_state_desc, "Unknown");
		break;
	}
}

/* This is a callback handler registered with wlan subsystem. This callback
 * gets invoked on occurrence of any wlan event
 */
static int wlan_event_callback(enum wlan_event_reason event, void *data)
{
	int ret;
	void *interface;
	char ps_state_desc[32];

	/* After successful wlan subsytem initialization, start
	 * Wi-Fi Direct feature.
	 */
	if (event == WLAN_REASON_INITIALIZED) {

		hp_configure_p2p_pushbutton();
		app_p2p_start();
	}

	if (event == WLAN_REASON_UAP_SUCCESS) {

		interface = net_get_wfd_handle();

		/* Start dhcp server on p2p interface */
		ret = dhcp_server_start(interface);
		if (ret) {
			dbg("Error in starting dhcp server");
			return -1;
		}

	}

	if (event == WLAN_REASON_SUCCESS) {
		wlan_configure_listen_interval(LISTEN_INTERVAL);
		pm_ieeeps_hs_cfg(true, WAKE_ON_ARP_BROADCAST | WAKE_ON_UNICAST |
			WAKE_ON_MULTICAST | WAKE_ON_MAC_EVENT);
	}

	if (event == WLAN_REASON_PS_ENTER) {
		int ps_state = (int) data;
		ps_state_to_desc(ps_state_desc, ps_state);
		dbg("Power save enter : %s", ps_state_desc);
		if (ps_state == WLAN_DEEP_SLEEP) {
			dbg("Enter into PM4: %d milliseconds"
					"", p2p_timeout);
			pm_mc200_state(PM4, p2p_timeout);
		}
	}
	if (event == WLAN_REASON_PS_EXIT) {
		int ps_state = (int) data;
		ps_state_to_desc(ps_state_desc, ps_state);
		dbg("Power save exit : %s", ps_state_desc);
	}

	return 0;
}

static void modules_init()
{
	int ret;

	ret = wmstdio_init(UART0_ID, 0);
	if (ret != WM_SUCCESS) {
		while (1)
			;
		/* do nothing -- stall */
	}

	ret = cli_init();
	if (ret != WM_SUCCESS) {
		dbg("Error: cli_init failed");
		while (1)
			;
		/* do nothing -- stall */
	}
	/* Initialize time subsystem.
	 *
	 * Initializes time to 1/1/1970 epoch 0.
	 */
	ret = wmtime_init();
	if (ret != WM_SUCCESS) {
		dbg("Error: wmtime_init failed");
		while (1)
			;
		/* do nothing -- stall */
	}

	/*
	 * Register Time CLI Commands
	 */
	ret = wmtime_cli_init();
	if (ret != WM_SUCCESS) {
		dbg("Error: wmtime_cli_init failed");
		while (1)
			;
		/* do nothing -- stall */
	}

	/*
	 * Initialize Power Management Subsystem
	 */
	ret = pm_init();
	if (ret != WM_SUCCESS) {
		dbg("Error: pm_init failed");
		while (1)
			;
		/* do nothing -- stall */
	}

	ret = pm_mc200_cli_init();
	if (ret != WM_SUCCESS) {
		dbg("Error: pm_mc200_cli_init failed");
		while (1)
			;
		/* do nothing -- stall */
	}

	/*
	 * Register sysinfo module.
	 */
	ret = sysinfo_init();
	if (ret != WM_SUCCESS) {
		dbg("Error: sysinfo_init failed");
		while (1)
			;
		/* do nothing -- stall */
	}

	ret = gpio_drv_init();
	if (ret != WM_SUCCESS) {
		dbg("Error: gpio_drv_init failed");
		while (1)
			;
		/* do nothing -- stall */
	}

	ret = wlan_cli_init();
	if (ret != WM_SUCCESS) {
		dbg("Error: wlan_cli_init failed");
		while (1)
			;
		/* do nothing -- stall */
	}
	return;
}

int main()
{
	int i;

	modules_init();

	dbg("Build Time: " __DATE__ " " __TIME__ "");

	/* Start the wm_core and wlan subsystem */
	if (wm_core_and_wlan_init() != WM_SUCCESS) {
		dbg("Failed to init wm_core and wlan ");
		while (1)
			;
		/* do nothing -- stall */
	}

	dbg("WiFi Direct (P2P) demo application v1 Started");

	if (wlan_start(wlan_event_callback) != WM_SUCCESS) {
		dbg("Failed to init wm_core and wlan ");
		while (1)
			;
		/* do nothing -- stall */
	}

	for (i = 0; i < sizeof(p2p_cmds) / sizeof(struct cli_command); i++)
		if (cli_register_command(&p2p_cmds[i]))
			wmprintf("Command register error");

	if (os_timer_create(&p2p_timeout_timer,
				"P2P Timeout Timer",
				os_msec_to_ticks(P2P_TIMEOUT),
				&p2p_timeout_timer_handler,
				NULL,
				OS_TIMER_ONE_SHOT,
				OS_TIMER_NO_ACTIVATE) < 0) {
		dbg("error in creating timer");
	}

	/* Create the main application thread */
	return os_thread_create(&app_thread,	/* thread handle */
				"p2p_demo_thread",/* thread name */
				p2p_demo_main,	/* entry function */
				0,		/* argument */
				&app_stack,	/* stack */
				OS_PRIO_2);	/* priority - medium low */

	return 0;
}
