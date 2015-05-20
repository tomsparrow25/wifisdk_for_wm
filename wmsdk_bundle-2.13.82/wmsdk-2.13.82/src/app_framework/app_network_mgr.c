/*
 *  Copyright 2008-2013, Marvell International Ltd.
 *  All Rights Reserved.
 */

/* app_network_mgr.c : This file contains the functions that interact with the
 * wlcmgr like start, connect, etc. This contains the wlcmgr callback function
 * as well (for the normal mode), which transfers control over to the app_ctrl.
 */

#include <wmstdio.h>
#include <wlan.h>
#include <cli_utils.h>
#include <app_framework.h>
#include <wm_net.h>
#include <diagnostics.h>
#include <dhcp-server.h>

#include "app_ctrl.h"
#include "app_network_mgr.h"
#include "app_network_config.h"
#include "app_dbg.h"
#include "app_http.h"

int app_reconnect_attempts = 0;
app_conn_failure_reason_t fr;

static struct wlan_network app_network, *uap_network;
static int app_network_valid;
static char wlan_started;

extern int (*dhcp_start_cb)(void *iface_handle);
extern void (*dhcp_stop_cb)();
extern bool sta_stop_cmd;


int app_check_valid_network_loaded()
{
	return app_network_valid;
}
/* Connect to the default network */
int app_connect_wlan()
{
	int ret;
	/* Initiate connection to network */
	ret = wlan_connect(app_network.name);
	if (ret != WLAN_ERROR_NONE) {
		app_w("network_mgr: failed to connect to network");
		return -WM_E_AF_NW_CONN;
	}
	return WM_SUCCESS;
}

int app_reconnect_wlan()
{
	app_reconnect_attempts++;
	app_d("network_mgr: reconnecting to network %d",
					app_reconnect_attempts);
	app_ctrl_notify_event(AF_EVT_NORMAL_CONNECTING, NULL);
	return app_connect_wlan();
}

/* Read the default network from the psm */
int app_load_configured_network()
{
	if (app_network_valid) {
		app_w("network_mgr: valid network already loaded");
		return WM_SUCCESS;
	}

	/* Read network data to connect to */
	memset(&app_network, 0, sizeof(app_network));
	if (app_network_get_nw(&app_network) != 0) {
		app_w("network_mgr: failed to read network data");
		return -WM_E_AF_NW_RD;
	}

	if (wlan_add_network(&app_network) != WLAN_ERROR_NONE) {
		app_e("network_mgr: failed to add network");
		return -WM_E_AF_NW_ADD;
	}

	app_network_valid = 1;
	return WM_SUCCESS;
}

int app_load_this_network(struct wlan_network *network)
{
	if (app_network_valid) {
		app_w("network_mgr: valid network already loaded");
		return WM_SUCCESS;
	}

	if (network == NULL)
		return -WM_FAIL;

	app_network = *network;
	memcpy(app_network.name, DEF_NET_NAME, sizeof(app_network.name));
	int ret = wlan_add_network(&app_network);
	if (ret != WLAN_ERROR_NONE) {
		app_e("network_mgr: failed to add network %d", ret);
		return -WM_E_AF_NW_ADD;
	}

	app_network_valid = 1;
	return WM_SUCCESS;
}

int app_unload_configured_network()
{
	if (!app_network_valid) {
		app_e("network_mgr: no valid network loaded");
		return WM_SUCCESS;
	}

	if (wlan_remove_network(app_network.name) != WLAN_ERROR_NONE) {
		app_e("network_mgr: failed to remove network");
		return -WM_E_AF_NW_DEL;
	}

	app_network_valid = 0;
	return WM_SUCCESS;
}

/* Callback Function passed to WLAN Connection Manager. The callback function
 * gets called when there are WLAN Events that need to be handled by the
 * application.
 */
int wlan_event_cb(enum wlan_event_reason reason, void *data)
{
	app_d("network_mgr: WLAN: received event %d", reason);

	switch (reason) {
	case WLAN_REASON_INITIALIZED:
		app_d("network_mgr: WLAN initialized");
		app_ctrl_notify_event(AF_EVT_WLAN_INIT_DONE, NULL);
		break;
	case WLAN_REASON_INITIALIZATION_FAILED:
		app_e("network_mgr: WLAN: initialization failed");
		app_ctrl_notify_event(AF_EVT_INTERNAL_WLAN_INIT_FAIL, NULL);
		break;
	case WLAN_REASON_SUCCESS:
		app_d("network_mgr: WLAN: connected to network");
		/* Reset number of connection attempts */
		app_reconnect_attempts = 0;
		/* Notify Application Controller */
		app_ctrl_notify_event_wait(AF_EVT_NORMAL_CONNECTED,
					   NULL);
		break;
	case WLAN_REASON_CONNECT_FAILED:
		app_w("network_mgr: WLAN: connect failed");
		app_ctrl_notify_event(AF_EVT_NORMAL_CONNECT_FAILED, NULL);
		break;
	case WLAN_REASON_NETWORK_NOT_FOUND:
		fr = NETWORK_NOT_FOUND;
		app_w("network_mgr: WLAN: network not found");
		app_ctrl_notify_event(AF_EVT_NORMAL_CONNECT_FAILED, &fr);
		break;
	case WLAN_REASON_NETWORK_AUTH_FAILED:
		fr = AUTH_FAILED;
		app_w("network_mgr: WLAN: network authentication failed");
		app_ctrl_notify_event(AF_EVT_NORMAL_CONNECT_FAILED, &fr);
		break;
	case WLAN_REASON_ADDRESS_SUCCESS:
		app_d("network mgr: DHCP new lease\n\r");
		app_ctrl_notify_event(AF_EVT_NORMAL_DHCP_RENEW, NULL);
		break;
	case WLAN_REASON_ADDRESS_FAILED:
		fr = DHCP_FAILED;
		app_w("network_mgr: failed to obtain an IP address");
		app_ctrl_notify_event(AF_EVT_NORMAL_CONNECT_FAILED, &fr);
		break;
	case WLAN_REASON_WPS_DISCONNECT:
		app_w("network_mgr: wps disconnected");
		app_ctrl_notify_event(AF_EVT_NORMAL_WPS_DISCONNECT, NULL);
		break;
	case WLAN_REASON_USER_DISCONNECT:
		app_w("network_mgr: disconnected");
		sta_stop_cmd = false;
		app_ctrl_notify_event(AF_EVT_NORMAL_USER_DISCONNECT, NULL);
		break;
	case WLAN_REASON_LINK_LOST:
		app_w("network_mgr: WLAN: link lost");
		app_ctrl_notify_event(AF_EVT_NORMAL_LINK_LOST, NULL);
		break;
	case WLAN_REASON_CHAN_SWITCH:
		app_w("network_mgr: WLAN: channel switch");
		app_ctrl_notify_event(AF_EVT_NORMAL_CHAN_SWITCH, NULL);
		break;
	case WLAN_REASON_UAP_SUCCESS:
		app_d("network_mgr: WLAN: UAP Started");
		app_ctrl_notify_event(AF_EVT_UAP_STARTED, NULL);
		break;
	case WLAN_REASON_UAP_STOPPED:
		app_d("network_mgr: WLAN: UAP Stopped");
		app_ctrl_notify_event(AF_EVT_UAP_STOPPED, NULL);
		break;
	case WLAN_REASON_PS_ENTER:
		app_d("network_mgr: WLAN: PS_ENTER");
		app_ctrl_notify_event(AF_EVT_PS_ENTER, data);
		break;
	case  WLAN_REASON_PS_EXIT:
		app_d("network_mgr: WLAN: PS EXIT");
		app_ctrl_notify_event(AF_EVT_PS_EXIT, data);
		break;
	default:
		app_d("network_mgr: WLAN: Unknown Event: %d", reason);
	}
	return 0;
}

/* Start the wlcmgr, register the wlan_event_cb callback defined above. */
int app_start_wlan()
{
	int ret;

	if (wlan_started) {
		/* In case of certain scenarios where uAP and normal mode
		 * both are under the control of application framework, there
		 * is no need to stop and start wlan again. But wlan_start is
		 * required while starting the normal mode. So just send
		 * WLAN_REASON_INITIALIZED event to create an illusion to
		 * follow the normal path thereafter.
		 */
		wlan_event_cb(WLAN_REASON_INITIALIZED, NULL);
		return WLAN_ERROR_NONE;
	}

	ret = wlan_start(wlan_event_cb);
	if (ret) {
		app_e("netwokr_mgr: failed to start WLAN CM, "
			"status code %d\r\n", ret);
		return -WM_FAIL;
	}
	wlan_started = 1;
	return ret;
}

/* Stop the wlcmgr */
int app_stop_wlan()
{
	wlan_stop();
	wlan_started = 0;
	return 0;
}

int app_network_mgr_start_uap()
{
	int ret;

	wlan_remove_network(uap_network->name);

	ret = wlan_add_network(uap_network);
	if (ret != WLAN_ERROR_NONE) {
		return -WM_FAIL;
	}
	ret = wlan_start_network(uap_network->name);
	if (ret != WLAN_ERROR_NONE) {
		return -WM_FAIL;
	}
	return WM_SUCCESS;
}

int app_network_mgr_remove_uap_network()
{
	wlan_remove_network(uap_network->name);
	os_mem_free(uap_network);
	return WM_SUCCESS;
}
int app_network_mgr_stop_uap()
{
	if (wlan_stop_network(uap_network->name) != WLAN_ERROR_NONE) {
		return -WM_FAIL;
	}
	return WM_SUCCESS;
}

int app_uap_start_by_network(struct wlan_network *net)
{
	uap_network = os_mem_alloc(sizeof(struct wlan_network));
	memcpy(uap_network, net, sizeof(struct wlan_network));
	app_ctrl_notify_event(AF_EVT_INTERNAL_UAP_REQUESTED, NULL);
	return WM_SUCCESS;
}

#define MIN_WPA2_PASS_LEN  8
int app_uap_start(char *ssid, char *wpa2_pass)
{
	struct wlan_network u_net;

	bzero(&u_net, sizeof(u_net));
	/* Set SSID as passed by the user */
	strncpy(u_net.ssid, ssid, sizeof(u_net.ssid));
	/* If a passphrase is passed, set the security to WPA2, else set it to
	 * None.
	 */
	if (wpa2_pass) {
		if (strlen(wpa2_pass) < MIN_WPA2_PASS_LEN)
			return -EINVAL;
		u_net.security.type = WLAN_SECURITY_WPA2;
		/* Set the passphrase */
		strncpy(u_net.security.psk, wpa2_pass,
			sizeof(u_net.security.psk));
		u_net.security.psk_len = strlen(u_net.security.psk);
	} else {
		u_net.security.type = WLAN_SECURITY_NONE;
	}

	/* Set profile name */
	strcpy(u_net.name, "uap-network");
	/* Set channel selection to auto (0) */
	u_net.channel = 0;
	/* Set network type to uAP */
	u_net.type = WLAN_BSS_TYPE_UAP;
	/* Set network role to uAP */
	u_net.role = WLAN_BSS_ROLE_UAP;
	/* Set IP address to 192.168.10.1 */
	u_net.address.ip = htonl(0xc0a80a01UL);
	/* Set default gateway to 192.168.10.1 */
	u_net.address.gw = htonl(0xc0a80a01UL);
	/* Set netmask to 255.255.255.0 */
	u_net.address.netmask = htonl(0xffffff00UL);
	/* Specify address type as static assignment */
	u_net.address.addr_type = ADDR_TYPE_STATIC;
	dhcp_start_cb = NULL;
	dhcp_stop_cb = NULL;
	return app_uap_start_by_network(&u_net);
}

int app_uap_stop()
{
	app_ctrl_notify_event(AF_EVT_INTERNAL_UAP_STOP, NULL);
	dhcp_start_cb = NULL;
	return WM_SUCCESS;
}

int app_uap_start_with_dhcp(char *ssid, char *wpa2_pass)
{
	if (app_uap_start(ssid, wpa2_pass) == WM_SUCCESS) {
		dhcp_start_cb = dhcp_server_start;
		dhcp_stop_cb = dhcp_server_stop;
		return WM_SUCCESS;
	}
	return -WM_FAIL;
}

int app_configure_network(struct wlan_network *net)
{
	if (app_network_get_nw_state() == APP_NETWORK_PROVISIONED) {
		app_sys_network_set_cb(net);
	} else {
		app_ctrl_notify_event(AF_EVT_INTERNAL_PROV_NW_SET,
				      net);
	}
	return WM_SUCCESS;
}

int app_sta_start()
{
	app_ctrl_notify_event(AF_EVT_INTERNAL_STA_REQUESTED, NULL);
	return WM_SUCCESS;
}

int app_sta_start_by_network(struct wlan_network *net)
{
	int ret;
	if (net == NULL)
		return -WM_FAIL;
	if (app_network_valid)
		app_unload_configured_network();
	ret = app_load_this_network(net);
	if (ret != WM_SUCCESS)
		return ret;
	app_ctrl_notify_event(AF_EVT_INTERNAL_STA_REQUESTED, NULL);
	return WM_SUCCESS;
}

