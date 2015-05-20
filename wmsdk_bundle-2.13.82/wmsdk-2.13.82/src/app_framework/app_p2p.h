/*
 *  Copyright 2008-2013, Marvell International Ltd.
 *  All Rights Reserved.
 */

#ifndef _APP_P2P_H_
#define _APP_P2P_H_

/** Start the p2p module */
int app_start_p2p_network();

/** Initiate a connection to the P2P node */
int app_p2p_connect();

/** Connect to the P2P group owner (GO) whose details are passed ad the
 * argument.
 */
int app_connect_to_p2p_go(struct wlan_network *net);
#endif /* _APP_P2P_H_ */
