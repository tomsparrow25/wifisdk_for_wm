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

#define WL_ID_P2P_DEEP_SLEEP "p2p_deep_sleep"

#define P2P_TIMEOUT (300 * 1000) /* 5 Minutes */

/* When CPU is expected to be idle for more than this time (millisecond) in
 * future, low power state is entered.
 */
#define THRESHOLD_TIME 10

static os_timer_t p2p_timeout_timer;
static unsigned int pm_cb_handle;
static bool p2p_timeout_occurred;

static void p2p_timeout_timer_cb(os_timer_arg_t arg)
{
	dbg("P2P Timeout occurred");
	wakelock_get(WL_ID_P2P_DEEP_SLEEP);
	p2p_timeout_occurred = true;
}

void hp_pm_reset_p2p_timeout_timer()
{
	p2p_timeout_occurred = false;
	os_timer_reset(&p2p_timeout_timer);
	os_timer_activate(&p2p_timeout_timer);
}

void hp_pm_stop_p2p_timeout_timer()
{
	p2p_timeout_occurred = false;
	os_timer_reset(&p2p_timeout_timer);
	os_timer_deactivate(&p2p_timeout_timer);
}

bool hp_pm_is_p2p_timeout_occurred()
{
	return p2p_timeout_occurred;
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

	int err = os_timer_create(&p2p_timeout_timer,
				"P2P Timeout Timer",
				os_msec_to_ticks(P2P_TIMEOUT),
				&p2p_timeout_timer_cb,
				NULL,
				OS_TIMER_ONE_SHOT,
				OS_TIMER_NO_ACTIVATE);

	if (err != WM_SUCCESS) {
		dbg("Unable to start P2P timeout timer");
	}
	pm_mc200_cfg(true, PM2, THRESHOLD_TIME);

	pm_cb_handle = pm_register_cb(ACTION_EXIT_PM2 | ACTION_EXIT_PM3,
			hp_pm_cb, NULL);
	return WM_SUCCESS;
}

void hp_pm_wifi_ps_enable()
{
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
	p2p_timeout_occurred = false;
	os_timer_deactivate(&p2p_timeout_timer);
}
