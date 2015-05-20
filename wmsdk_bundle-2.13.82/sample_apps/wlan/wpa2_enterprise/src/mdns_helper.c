/*
 *  Copyright (C) 2008-2013, Marvell International Ltd.
 *  All Rights Reserved.
 */
#include <wmstdio.h>
#include <app_framework.h>
#include <mdns_helper.h>
#include <appln_cb.h>
#include <appln_dbg.h>
#include <wm_utils.h>

struct mdns_service my_service = {
	.servname = appln_cfg.servname,
	.servtype = "http",
	.domain = "local",
	.proto = MDNS_PROTO_TCP,
	.port = 80,
};

#define MAX_MDNS_TXT_SIZE 128
char mytxt[MAX_MDNS_TXT_SIZE];

void hp_mdns_announce(void *iface, int state)
{
	if (state == UP) {
		/* system epoch changes after every reboot. Adding epoch in mdns
		 * txt record will update txt record after every reboot and
		 * hence mdns querier will give MDNS_UPDATE event.
		 */
		snprintf(mytxt, MAX_MDNS_TXT_SIZE, "txtvers=2:epoch=%d",
			 sys_get_epoch());
		SET_TXT_REC(my_service, mytxt);
		app_mdns_add_service(&my_service, iface);
	} else {
		app_mdns_iface_state_change(iface, state);
	}
}

void hp_mdns_deannounce(void *iface)
{
	app_mdns_remove_service(&my_service, iface);
}

void hp_mdns_down_up(void *iface)
{
	/* mdns interface state is changed to DOWN to
	 * to remove previous mdns service from
	 * mdns querier cache. Later state is changed
	 * to UP to add services in querier cache.
	 */
	app_mdns_iface_state_change(iface, DOWN);
	/* 100ms sleep is added to send mdns down packet before
	 * announcing service again.
	 */
	os_thread_sleep(os_msec_to_ticks(100));
	hp_mdns_announce(iface, UP);

}
