/*
 *  Copyright (C) 2008-2013, Marvell International Ltd.
 *  All Rights Reserved.
 */

/* System includes */
#include <wmstdio.h>
#include <string.h>
#include <wmtime.h>
#include <wm_os.h>
#include <wlan.h>
#include <pwrmgr.h>

#include "power_mgr_helper.h"

static unsigned int pm_cb_handle;

static void pm_cb(power_save_event_t event, void *data)
{
	/* If Wake up source is external GPIO wake up key then we disable
	 * power manager framework and IEEE power save.
	 */
	if (pm_wakeup_source() & EGPIO0) {
		pm_mc200_cfg(false, PM1, 0);
		pm_ieeeps_hs_cfg(false, 0);
	}
}

void pm_register_callback_handle()
{
	pm_cb_handle = pm_register_cb(ACTION_EXIT_PM2 | ACTION_EXIT_PM3,
			pm_cb, NULL);
}

void pm_deregister_callback_handle()
{
	pm_deregister_cb(pm_cb_handle);
}

int hp_pm_init()
{
	pm_register_callback_handle();
	pm_mc200_cfg(true, PM2, 10);
	return WM_SUCCESS;
}
