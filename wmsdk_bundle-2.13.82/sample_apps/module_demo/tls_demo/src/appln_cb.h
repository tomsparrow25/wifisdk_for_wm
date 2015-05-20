/*
 *  Copyright (C) 2008-2013, Marvell International Ltd.
 *  All Rights Reserved.
 */

#ifndef __APPLN_CB_H__
#define __APPLN_CB_H__

#define MAX_SRVNAME_LEN 32

/* This structure holds all the application specific
 * configuration data.
 */
typedef struct __appln_config {
	/* These two variables are used for starting uAP network. */
	char *ssid;
	char *passphrase;

	/* If mdns is enabled, following variables hold hostname
	 *  and service name respectively.
	 */
	char servname[MAX_SRVNAME_LEN];
	char *hostname;

	/*
	 * GPIO number for WPS push button.
	 * When not configured, set it to -1.
	 */
	int wps_pb_gpio;

	/*
	 * GPIO number for reset to provisioning push button.
	 * When not configured, set it to -1.
	 */
	int reset_prov_pb_gpio;
} appln_config_t;

extern appln_config_t appln_cfg;


/* Mandatory - Critical error handler */
extern void appln_critical_error_handler(void *);

#endif  /* ! __APPLN_CB_H__ */

