/*
 *  Copyright 2008-2013, Marvell International Ltd.
 *  All Rights Reserved.
 */

/* app_mdns_services.c: mDNS-SD can be used for device discovery by the
 * application framework. This file defines the functions to initialize, start
 * and stop the mDNS-SD service.
 */

#include <wmstdio.h>
#include <wm_net.h>
#include <mdns.h>
#include <app_framework.h>

#include "app_ctrl.h"
#include "app_dbg.h"

static const char *md_domain = "local";
static bool mdns_started;

/* Refresh ip address and hostname */
void app_mdns_refresh_hostname(char *hostname)
{
	mdns_set_hostname(hostname);
}

/* The network is up, start the mDNS-SD service */
void app_mdns_start(char *hostname)
{
	if (mdns_started)
		return;
	/* Start the mdns thread */
	mdns_start(md_domain, hostname);
	mdns_started = 1;
}

/* The network is down, stop the mdns service */
void app_mdns_stop(void)
{
	mdns_started = 0;
	/* Stop the mdns thread */
	mdns_stop();

	/* Nothing to unregister */
}

void app_mdns_add_service_iface(struct mdns_service_config config)
{
	mdns_add_service_iface(config);
}

/* Remove interface and services on them from mdns */
void app_mdns_remove_service_iface(void *iface)
{
	mdns_remove_service_iface(iface);
}

#define MAX_MDNS_LST 5
#define MAX_INTERFACE 2
#define STA_INTERFACE 0
#define UAP_INTERFACE 1

struct mdns_service *app_srv_list_t[MAX_INTERFACE][MAX_MDNS_LST + 1] = {
	{NULL}, {NULL}
};

static int app_mdns_add_srv(struct mdns_service *srv, struct mdns_service
			    *app_srv_list[], void *iface)
{
	struct mdns_service_config config;
	int i, free_index = 0;

	config.iface_handle = iface;
	if (!srv)
		return -WM_FAIL;

	for (i = 0; i < MAX_MDNS_LST; i++)
		if (app_srv_list[i] == NULL) {
			free_index = i;
			break;
		}
	if (free_index == (MAX_MDNS_LST - 1))
		return WM_SUCCESS;

	if (free_index) {
		mdns_remove_service_iface(config.iface_handle);
		os_thread_sleep(os_msec_to_ticks(1000));
	}

	app_srv_list[free_index] = srv;
	app_srv_list[free_index + 1] = NULL;
	config.services = app_srv_list;

	int ret = mdns_add_service_iface(config);
	if (ret == WM_SUCCESS) {
		app_mdns_iface_state_change(config.iface_handle, UP);
		os_thread_sleep(os_msec_to_ticks(1000));
	}
	return ret;
}

static int app_mdns_remove_srv(struct mdns_service *srv, struct mdns_service
			       *app_srv_list[], void *iface)
{
	int i, free_index = 0;
	struct mdns_service_config config;

	config.iface_handle = iface;

	if (!srv)
		return -WM_FAIL;
	for (i = 0; i < MAX_MDNS_LST; i++)
		if (app_srv_list[i] == NULL) {
			free_index = i;
			break;
		}

	for (i = 0; i < MAX_MDNS_LST; i++) {
		if (app_srv_list[i] == srv) {
			mdns_remove_service_iface(config.iface_handle);
			os_thread_sleep(os_msec_to_ticks(1000));
			free_index--;
			app_srv_list[i] = NULL;
			while (i < MAX_MDNS_LST - 1) {
				app_srv_list[i] = app_srv_list[i + 1];
				i++;
			}
			app_srv_list[MAX_MDNS_LST - 1] = NULL;
			if (free_index) {
				config.services = app_srv_list;
				mdns_add_service_iface(config);
				app_mdns_iface_state_change(config.iface_handle,
							    UP);
				os_thread_sleep(os_msec_to_ticks(1000));
			}
			return WM_SUCCESS;
		}
	}
	return -WM_FAIL;
}

int app_mdns_add_service(struct mdns_service *srv, void *iface)
{
	if (iface == net_get_sta_handle()) {
		return app_mdns_add_srv(srv, app_srv_list_t[STA_INTERFACE],
					iface);
	} else if (iface == net_get_uap_handle()) {
		return app_mdns_add_srv(srv, app_srv_list_t[UAP_INTERFACE],
					iface);
	}
	return -WM_FAIL;
}

int app_mdns_remove_service(struct mdns_service *srv, void *iface)
{
	if (iface == net_get_sta_handle()) {
		return app_mdns_remove_srv(srv, app_srv_list_t[STA_INTERFACE],
					   iface);
	} else if (iface == net_get_uap_handle()) {
		return app_mdns_remove_srv(srv, app_srv_list_t[UAP_INTERFACE],
					   iface);
	}
	return -WM_FAIL;
}

void app_mdns_iface_state_change(void *iface, int state)
{
	mdns_iface_state_change(iface, state);
}
