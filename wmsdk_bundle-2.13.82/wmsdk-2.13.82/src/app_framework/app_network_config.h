/*
 *  Copyright 2008-2013,2011 Marvell International Ltd.
 *  All Rights Reserved.
 */

#ifndef _APP_NETWORK_CONFIG_H
#define _APP_NETWORK_CONFIG_H

#include "app_psm.h"

#define APP_NETWORK_PSM_NOT_PROVISIONED     "0"
#define APP_NETWORK_PSM_PROVISIONED         "1"

#ifndef APP_NO_PSM_CONFIG

/* psm details   */
/* Max WPA2 passphrase can be upto 64 ASCII chars */
#define MAX_PSM_VAL		65
#define PART_NAME         	"refpart"
#define DEF_NET_NAME       	"default"
#define VAR_NET_SSID      	"ssid"
#define VAR_NET_BSSID     	"bssid"
#define VAR_NET_CHANNEL   	"channel"
#define VAR_NET_SECURITY  	"security"
#define VAR_NET_PASSPHRASE 	"passphrase"
#define VAR_NET_POLLINT   	"pollinterval"
#define VAR_NET_SENSORNAME 	"sensorname"
#define VAR_NET_CONFIGURED 	"configured"
#define VAR_NET_LAN        	"lan"
#define VAR_NET_IP_ADDRESS 	"ip_address"
#define VAR_NET_SUBNET_MASK 	"subnet_mask"
#define VAR_NET_GATEWAY    	"gateway"
#define VAR_NET_DNS1		"dns1"
#define VAR_NET_DNS2		"dns2"
#define VAR_SCAN_DURATION	"scan_duration"
#define VAR_SPLIT_SCAN_DELAY	"split_scan_delay"
#define VAR_SCAN_INTERVAL	"scan_interval"
#define VAR_SCAN_CHANNEL		"channel"

#endif /* APP_NO_PSM_CONFIG */

/** Check if a valid network has been already loaded */
int app_check_valid_network_loaded();

/** Modifier to set network configuration */
int app_network_set_nw(struct wlan_network *network);

/** Accessor to get network configuration */
WEAK int app_network_get_nw(struct wlan_network *network);

/** Check network configuration state */
WEAK int app_network_get_nw_state();

/** Set network configuration state */
int app_network_set_nw_state(int state);

#endif
