/*
 *  Copyright (C) 2008-2013, Marvell International Ltd.
 *  All Rights Reserved.
 */
#include <wmstdio.h>
#include <app_framework.h>
#include <reset_prov_helper.h>
#include <appln_cb.h>
#include <appln_dbg.h>
#include <mdev_gpio.h>
#include "push_button.h"

static int pm_handle;

/*
 * On some boards (e.g. mc200_dev-v2) the push-button
 * GPIO stays at a voltage higher than zero, which
 * causes a spurious GPIO interrupt while waking up
 * from PM2. Futher investigation pending, this callback
 * is registered as a work-around for this issue.
 */
void reset_prov_pb_pm_callback(power_save_event_t event, void *data)
{
        GPIO_IntClr(appln_cfg.reset_prov_pb_gpio);
}

static void reset_prov_pb_callback(void)
{
	ll_printf("Reset to prov pushbutton press\r\n");
	app_reset_configured_network();
}

void hp_configure_reset_prov_pushbutton()
{
	push_button_set_cb(appln_cfg.reset_prov_pb_gpio,
			   reset_prov_pb_callback);
	pm_handle = pm_register_cb(ACTION_EXIT_PM2,
				reset_prov_pb_pm_callback, NULL);
}

void hp_unconfigure_reset_prov_pushbutton()
{
	push_button_set_cb(appln_cfg.reset_prov_pb_gpio,
			   NULL);
	pm_deregister_cb(pm_handle);
}

