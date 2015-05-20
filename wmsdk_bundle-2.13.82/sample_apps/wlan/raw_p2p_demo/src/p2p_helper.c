/*
 *  Copyright (C) 2008-2013, Marvell International Ltd.
 *  All Rights Reserved.
 */
#include <wmstdio.h>
#include <appln_cb.h>
#include <appln_dbg.h>
#include <mdev_gpio.h>
#include <push_button.h>
#include <led_indicator.h>
#include <stdlib.h>
#include <wm_os.h>
#include <wlan.h>
#include <wm_net.h>
#include <p2p.h>
#include "p2p_helper.h"
#include "power_mgr_helper.h"

extern os_timer_t p2p_timeout_timer;

extern int p2p_device_started;
extern int p2p_session_on;
extern int num_scan_results;
extern struct p2p_scan_result *p2p_scan_results;

char Device_Name[33];
char ip[16];

static struct wlan_network p2p_network;

void connet_to_p2p_network()
{
	int retry_cnt = 5, ret;
	int connect_retry = 20;
	enum wlan_connection_state state;
	struct wlan_ip_config addr;

	if (wlan_add_network(&p2p_network)) {
		dbg("failed to add p2p network");
		goto fail;
	}

	do {
		dbg("Connecting to %s ..... ", p2p_network.name);

		/* Initiate connection to network */
		ret = wlan_connect(p2p_network.name);

		if (ret != 0) {
			dbg("Failed to connect to P2P GO");
			goto retry;
		}

		connect_retry = 200;

		do {

			/* wait for interface up */
			os_thread_sleep(os_msec_to_ticks(50));

			if (wlan_get_connection_state(&state)) {
				dbg("Error: unable to get "
						"connection state");
				continue;
			}

			if (state == WLAN_CONNECTED || connect_retry == 0)
				break;

			connect_retry--;

		} while (state != WLAN_DISCONNECTED);


		if (state != WLAN_CONNECTED) {
			dbg("Error: Not connected");
			goto retry;
		}

		dbg("Connected to following Group Owner :");
		dbg("SSID = [%s], BSSID = "
			 "[%02x:%02x:%02x:%02x:%02x:%02x]",
			 p2p_network.name,
			 p2p_network.bssid[0],
			 p2p_network.bssid[1],
			 p2p_network.bssid[2],
			 p2p_network.bssid[3],
			 p2p_network.bssid[4],
			 p2p_network.bssid[5]);

		ret = wlan_get_wfd_address(&addr);
		if (ret != WLAN_ERROR_NONE) {
			dbg("failed to get IP address");
			strcpy(ip, "0.0.0.0");
			goto retry;
		}

		net_inet_ntoa(addr.ip, ip);

		break;

retry:
		retry_cnt--;
		dbg("Connect to GO FAIL ! Retrying .....");
	} while (retry_cnt != 0);

	if (retry_cnt == 0) {
fail:
		os_thread_sleep(os_msec_to_ticks(500));

		ret = wlan_disconnect();
		if (ret != 0)
			dbg("Failed to disconnect");

		ret = wlan_remove_network(p2p_network.name);
		if (ret != 0)
			dbg("Failed to remove network");

	}

}

static int p2p_callback(enum p2p_event event, void *data, uint16_t len)
{
	if (event == P2P_STARTED) {
		dbg("P2P Started");
	} else if (event == P2P_SESSION_SUCCESSFUL) {
		if (data != NULL && len != 0) {
			memcpy(&p2p_network, data, sizeof(p2p_network));
		}

		connet_to_p2p_network();
		led_off(board_led_4());
		led_on(board_led_1());
	} else if (event == P2P_AUTO_GO_STARTED) {
		if (data != NULL && len != 0) {
			strncpy(Device_Name, (char *)data, len);
		}
		dbg("P2P Auto GO Started: %s", Device_Name);
		led_on(board_led_1());
	} else if (event == P2P_DEVICE_STARTED) {
		if (data != NULL && len != 0) {
			strncpy(Device_Name, (char *)data, len);
		}
		dbg("P2P Device Started: %s", Device_Name);
		led_on(board_led_1());
		p2p_device_started = 1;
		hp_pm_init();
		os_timer_reset(&p2p_timeout_timer);
		os_timer_activate(&p2p_timeout_timer);
	} else if (event == P2P_AP_SESSION_SUCCESSFUL) {
		led_off(board_led_4());
		led_on(board_led_1());
	} else if (event == P2P_GO_STARTED) {
		if (data != NULL && len != 0) {
			strncpy(Device_Name, (char *)data, len);
		}
		dbg("P2P GO Started: %s", Device_Name);
	} else if (event == P2P_CLIENT_STARTED) {
		dbg("P2P Client Started");
	} else if (event == P2P_SESSION_STARTED) {
		p2p_session_on = 1;
		dbg("P2P Session started");
		led_off(board_led_4());
		led_on(board_led_1());
		os_timer_reset(&p2p_timeout_timer);
		os_timer_deactivate(&p2p_timeout_timer);
	} else if (event == P2P_SESSION_FAILED) {
		p2p_session_on = 0;
		dbg("P2P Session failed");
		led_off(board_led_4());
		led_on(board_led_1());
		os_timer_reset(&p2p_timeout_timer);
		os_timer_activate(&p2p_timeout_timer);
	} else if (event == P2P_SESSION_TIMEOUT) {
		p2p_session_on = 0;
		dbg("P2P Session timeout");
		led_off(board_led_4());
		led_on(board_led_1());
		os_timer_reset(&p2p_timeout_timer);
		os_timer_activate(&p2p_timeout_timer);
	} else if (event == P2P_FINISHED) {
		p2p_device_started = 0;
		p2p_session_on = 0;
		dbg("P2P Stopped");
		led_off(board_led_4());
		led_on(board_led_1());
		dbg("Start Deep Sleep mode");
		wlan_deepsleepps_on();
	}

	return WM_SUCCESS;
}

static struct p2p_config p2p_conf = {
	.role = 4,
	.auto_go = P2P_AUTOGO,
	.pin_generator = 1,
	.version = 0x20,
	.version2 = 0x20,
	.device_name = "Marvell-Embedded-Client",
	.manufacture = "Marvell",
	.model_name = "RD-88W-PLUG-8782-A0",
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

static int app_p2p_session_start(char *pin)
{
	int ret;

	if (pin) {
		/* This is a pin session */
		unsigned int ipin;
		ipin = atoi(pin);
		ret = p2p_connect(CMD_P2P_PIN, ipin, &p2p_scan_results[0]);
	} else {
		/* This is a PBC session */
		ret = p2p_connect(CMD_P2P_PBC, 0, &p2p_scan_results[0]);
	}

	if (ret != WM_SUCCESS) {
		dbg("p2p connect failed");
		return -WM_FAIL;
	}

	return WM_SUCCESS;
}

void app_p2p_start()
{
	int ret;

	ret = p2p_start(&p2p_conf);

	if (ret != WM_SUCCESS)
		dbg("failed to start P2P");
}

void app_p2p_stop()
{
	int ret;

	ret = p2p_stop();

	if (ret != WM_SUCCESS)
		dbg("failed to stop P2P");
}

static void p2p_pb_callback(void)
{
	dbg("P2P pushbutton press");
	app_p2p_session_start(NULL);
}

void hp_configure_p2p_pushbutton()
{
	push_button_set_cb(board_button_1(),
			   p2p_pb_callback);
}

void hp_unconfigure_p2p_pushbutton()
{
	push_button_set_cb(board_button_1(),
			   NULL);
}
