/*
 *  Copyright (C) 2008-2012, Marvell International Ltd.
 *  All Rights Reserved.
 */

#ifndef _PM_MC200_WIFI_DEMO_APP_H_
#define _PM_MC200_WIFI_DEMO_APP_H_

/** Starts the application */
int pm_mc200_wifi_demo_app_start();
/** Stops the application */
int pm_mc200_wifi_demo_app_stop();
/** The event handler */
int pm_mc200_wifi_demo_app_event_handler(int event, void *data);

extern char pm_mc200_wifi_demo_hostname[];
extern char pm_mc200_wifi_demo_ssid[];

#ifdef PM_MC200_WIFI_DEMOAPP_DEBUG_PRINT
#define PM_MC200_WIFI_DEMOAPP_DBG(...)  \
	wmprintf("[app-pm-demo debug] " __VA_ARGS__)
#else
#define PM_MC200_WIFI_DEMOAPP_DBG(...)	(void)0
#endif

#define PM_MC200_WIFI_DEMOAPP_LOG(...)  \
	wmprintf("[app-pm-demo] " __VA_ARGS__)

#endif
