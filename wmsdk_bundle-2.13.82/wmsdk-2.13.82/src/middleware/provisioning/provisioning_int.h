/*
 * Copyright 2008-2013, Marvell International Ltd.
 * All Rights Reserved.
 *
 */

#ifndef __PROVISIONING_INT_H__
#define __PROVISIONING_INT_H__

enum prov_internal_event {
	EVENT_WLAN,
	EVENT_WPS,
/* Internal commands */
/* Set network from WLAN network based provisioning */
#define SET_NW_WLANNW		1
/* Set network from WPS based provisioning */
#define SET_NW_WPS		2
/* Perform WPS session attempt */
#define WPS_SESSION_ATTEMPT	3
/* Re-initialize WLAN network */
#define PROV_WLANNW_REINIT	4
/* Start Prov State Machine */
#define PROV_INIT_SM            5
	EVENT_CMD,
};

#define MESSAGE(event, data) ((((event) & 0xff) << 8) | ((data) & 0xff))
#define MESSAGE_EVENT(message) (((message) >> 8) & 0xff)
#define MESSAGE_DATA(data) ((data) & 0xff)

extern struct site_survey survey;

struct prov_gdata {
	unsigned int state;
	struct provisioning_config *pc;
	unsigned char scan_running;
	int scan_interval;
/* Provisioning WLAN network started */
#define PROV_WLANNW_STARTED		(unsigned int)(1<<0)
/* WLAN network based provisioning successful */
#define PROV_WLANNW_SUCCESSFUL		(unsigned int)(1<<1)
	int prov_nw_state;
	struct wlan_network current_net;
	os_semaphore_t sem_current_net;

#ifdef CONFIG_WPS2
/* WPS provisioning started */
#define PROV_WPS_STARTED	(unsigned int)(1<<0)
/* WPS pushbutton attempt enabled */
#define PROV_WPS_PBC_ENABLED	(unsigned int)(1<<1)
/* WPS PIN attempt enabled */
#define PROV_WPS_PIN_ENABLED	(unsigned int)(1<<2)
/* WPS based provisioning successful */
#define PROV_WPS_SUCCESSFUL	(unsigned int)(1<<3)
	int wps_state;
	unsigned int wps_pin;
	unsigned long time_pin_expiry;
	unsigned long time_pbc_expiry;
	struct wlan_scan_result wps_session_nw;
#endif
};

#define MAX_SITES                       20
struct site_survey {
	struct wlan_scan_result sites[MAX_SITES];
	unsigned int num_sites;
	os_mutex_t lock;
};

/* Provisioning web handlers internal APIs */
int prov_get_state(void);
int prov_set_network(int prov_method, struct wlan_network *net);

#include <wmlog.h>

#define prov_e(...)				\
	wmlog_e("prov", ##__VA_ARGS__)
#define prov_w(...)				\
	wmlog_w("prov", ##__VA_ARGS__)

#ifdef CONFIG_PROVISIONING_DEBUG
#define prov_d(...)				\
	wmlog("prov", ##__VA_ARGS__)
#else
#define prov_d(...)
#endif /* ! CONFIG_PROVISIONING_DEBUG */


#endif				/* __PROVISIONING_INT_H__ */
