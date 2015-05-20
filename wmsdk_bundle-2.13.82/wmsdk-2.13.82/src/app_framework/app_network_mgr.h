/*
 *  Copyright 2008-2013, Marvell International Ltd.
 *  All Rights Reserved.
 */

#ifndef _APP_NETWORK_MGR_H
#define _APP_NETWORK_MGR_H


/** Starts and connects to WLAN */
int app_start_wlan();

/** Stops WLAN */
int app_stop_wlan();

/** Connect to WLAN */
int app_connect_wlan();
int app_reconnect_wlan();

/* Start/Stop the uap network */
int app_network_mgr_start_uap();
int app_network_mgr_stop_uap();

/* Remove the uap network from the list of known wlan networks */
int app_network_mgr_remove_uap_network();

/** Load the configured network */
int app_load_configured_network();
int app_load_this_network(struct wlan_network *net);
/** Unload the configured network */
int app_unload_configured_network();

#endif
