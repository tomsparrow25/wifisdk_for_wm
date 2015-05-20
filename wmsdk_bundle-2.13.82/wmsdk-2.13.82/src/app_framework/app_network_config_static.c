/*
 *  Copyright 2008-2013 Marvell International Ltd.
 *  All Rights Reserved.
 */

/* app_network_config_static.c: This file is used if a hard-coded network should
 * be configured within the application. This file is only available as a
 * reference and not used.
 */

#include <string.h>

#include <wmstdio.h>
#include <wlan.h>
#include <cli_utils.h>
#include <wm_net.h>

#include "app_network_config.h"
#include "app_dbg.h"
#include "app_ctrl.h"


#ifdef APP_NO_PSM_CONFIG
/* Choose the static configuration */
#define APP_TEST_CONFIG_1

int app_network_config_init(void)
{
	return 0;
}

int app_network_is_provisioned(void)
{
	/* No need to provision */
	return 1;
}

int app_network_get_nw(struct wlan_network *network)
{
#ifdef APP_TEST_CONFIG_1
	/* Marvell test AP - WPA2/AES, DHCP */
#define DEF_NET_NAME1 "apx-gw"
#define DEF_SSID     "Marvell-WMG-CES"
#define DEF_BSSID    "00:00:00:00:00:00"
	memcpy(network->name, DEF_NET_NAME1, strlen(DEF_NET_NAME1) + 1);
	memcpy(network->ssid, DEF_SSID, strlen(DEF_SSID) + 1);
	get_mac(DEF_BSSID, network->bssid, ':');
	network->channel = 0;
	network->address.dhcp = 1;
	network->security.type = WLAN_SECURITY_WPA2;
	strcpy(network->security.psk, "marvellces");
	network->security.psk_len = strlen("marvellces");
#elif defined(APP_TEST_CONFIG_2)
	/* Marvell test access point - no security, static IP */
#define DEF_NET_NAME1 "test"
#define DEF_SSID     "kanchenjunga"
#define DEF_BSSID    "00:00:00:00:00:00"
#define DEF_IP  "192.168.1.100"
#define DEF_GW  "192.168.1.1"
#define DEF_NETMASK  "255.255.255.0"
	memcpy(network->name, DEF_NET_NAME1, strlen(DEF_NET_NAME1) + 1);
	memcpy(network->ssid, DEF_SSID, strlen(DEF_SSID) + 1);
	get_mac(DEF_BSSID, network->bssid, ':');
	network->channel = 0;
	network->address.dhcp = 0;
	network->address.ip = inet_aton(DEF_IP);
	network->address.gw = inet_aton(DEF_GW);
	network->address.netmask = inet_aton(DEF_NETMASK);
#elif defined(APP_TEST_CONFIG_3)
	/* Customize this */
#define DEF_NET_NAME1 "home"
#define DEF_SSID     "ssid"
#define DEF_BSSID    "00:00:00:00:00:00"
	memcpy(network->name, DEF_NET_NAME1, strlen(DEF_NET_NAME1) + 1);
	memcpy(network->ssid, DEF_SSID, strlen(DEF_SSID) + 1);
	get_mac(DEF_BSSID, network->bssid, ':');
	network->channel = 0;
	network->address.dhcp = 1;
	network->security.type = WLAN_SECURITY_WPA;
	strcpy(network->security.psk, "password");
	network->security.psk_len = strlen("password");
#else
#error Please define a network configuration
#endif

	return 0;
}

int app_network_set_nw(struct wlan_network *network)
{
	return 0;
}

int app_network_set_nw_state(int state)
{
	return 0;
}

int app_network_get_nw_state(void)
{
	return 0;
}

#endif /* APP_NO_PSM_CONFIG */
