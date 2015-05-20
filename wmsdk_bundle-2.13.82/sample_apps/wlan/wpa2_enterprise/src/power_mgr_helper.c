/*
 *  Copyright (C) 2008-2012, Marvell International Ltd.
 *  All Rights Reserved.
 */

/* System includes */
#include <wmstdio.h>
#include <string.h>
#include <wmtime.h>
#include <wm_os.h>
#include <wlan.h>
#include <pwrmgr.h>
#include <appln_dbg.h>
#include <app_framework.h>
#include "power_mgr_helper.h"


#define UAP_DOWN_TIMEOUT (30 * 1000)

/* When CPU is expected to be idle for more than this time (millisecond) in
 * future, low power state is entered.
 */
#define THRESHOLD_TIME 10

static os_timer_t uap_down_timer;
static unsigned int pm_cb_handle;



/** Provisioning done timer call back function
 * Once the provisioning is done, we wait for provisioning client to send
 * AF_EVT_PROV_CLIENT_DONE which stops uap and dhcp server. But if any case
 * client doesn't send AF_EVT_PROV_CLIENT_DONE event, then we wait for
 * 60seconds(timer) to shutdown UAP.
 */
static void uap_down_timer_cb(os_timer_arg_t arg)
{
	if (is_uap_started()) {
		app_uap_stop();
	}
}
/*
 * Once the user has pressed the wake key this variable remembers this
 * event. The other modules of the application refer to this before
 * deciding to enable low modes.
 */
static bool wake_status;

static void hp_pm_cb(power_save_event_t event, void *data)
{
	/* If Wake up source is external GPIO wake up key then we disable
	 * power manager framework and IEEE power save.
	 */
	if (pm_wakeup_source() & EGPIO0) {
		wake_status = true;
		pm_mc200_cfg(false, PM1, 0);
		pm_ieeeps_hs_cfg(false, 0);
	}
}

bool hp_pm_get_wake_status(void)
{
	return wake_status;
}

void hp_pm_reset_wake_status()
{
	wake_status = false;
}

void hp_pm_set_wake_status()
{
	wake_status = true;
}


int hp_pm_init()
{

	int err = os_timer_create(&uap_down_timer,
				  "uap-down-timer",
				  os_msec_to_ticks(UAP_DOWN_TIMEOUT),
				  &uap_down_timer_cb,
				  NULL,
				  OS_TIMER_ONE_SHOT,
				  OS_TIMER_NO_ACTIVATE);
	if (err != WM_SUCCESS) {
		dbg("Unable to start uap down timer");
	}
	pm_mc200_cfg(true, PM2, THRESHOLD_TIME);

	pm_cb_handle = pm_register_cb(ACTION_EXIT_PM2 | ACTION_EXIT_PM3,
			hp_pm_cb, NULL);
	return WM_SUCCESS;
}

void hp_pm_wifi_ps_enable()
{

	/*
	 * If micro AP interface is up
	 * queue a timer which will take
	 * micro AP interface down.
	 */
	if (is_uap_started()) {
		os_timer_activate(&uap_down_timer);
		return;
	}
	 /* if power management is enabled enable IEEE Power save for
	  * WIFI card.
	  * IEEEPS and host sleep configuration needs to be send on
	  * every connection since it is lost on station gets
	  * disconnected.
	  */
	if (!hp_pm_get_wake_status() && is_sta_connected()) {
		dbg("Configuring IEEEPS");
		pm_ieeeps_hs_cfg(true, WAKE_ON_ARP_BROADCAST
				 | WAKE_ON_UNICAST
				 | WAKE_ON_MULTICAST
				 | WAKE_ON_MAC_EVENT);
	}
}

void hp_pm_wifi_ps_disable()
{
	pm_ieeeps_hs_cfg(false, 0);
	os_timer_deactivate(&uap_down_timer);
}
