/*
 *  Copyright (C) 2008-2012, Marvell International Ltd.
 *  All Rights Reserved.
 */

#ifndef _PERF_DEMO_APP_H_
#define _PERF_DEMO_APP_H_

/** Starts the application */
int perf_demo_app_start();
/** Stops the application */
int perf_demo_app_stop();
/** The event handler */
int perf_demo_app_event_handler(int event, void *data);

extern char perf_demo_hostname[];
extern char perf_demo_ssid[];

#ifdef PERF_DEMO_APP_DEBUG_PRINT
#define PERF_DEMO_APP_DBG(...)  wmprintf("[app-perf-demo debug] " __VA_ARGS__)
#else
#define PERF_DEMO_APP_DBG(...)	(void)0
#endif

#define PERF_DEMO_APP_LOG(...)  wmprintf("[app-perf-demo] " __VA_ARGS__)

#endif
