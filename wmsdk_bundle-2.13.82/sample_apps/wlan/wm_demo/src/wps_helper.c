/*
 *  Copyright (C) 2008-2013, Marvell International Ltd.
 *  All Rights Reserved.
 */
#include <wmstdio.h>
#include <app_framework.h>
#include <wps_helper.h>
#include <appln_cb.h>
#include <appln_dbg.h>
#include <mdev_gpio.h>
#include "push_button.h"

#ifdef CONFIG_WPS2

static int pm_handle;

/*
 * On some boards (e.g. mc200_dev-v2) the push-button
 * GPIO stays at a voltage higher than zero, which
 * causes a spurious GPIO interrupt while waking up
 * from PM2. Futher investigation pending, this callback
 * is registered as a work-around for this issue.
 */
void wps_pb_pm_callback(power_save_event_t event, void *data)
{
        GPIO_IntClr(appln_cfg.wps_pb_gpio);
}

static void wps_pb_callback(void)
{
	ll_printf("WPS pushbutton press\r\n");
	app_prov_wps_session_start(NULL);
}

void hp_configure_wps_pushbutton()
{
	push_button_set_cb(appln_cfg.wps_pb_gpio,
				wps_pb_callback);
	pm_handle = pm_register_cb(ACTION_EXIT_PM2,
				wps_pb_pm_callback, NULL);
}

void hp_unconfigure_wps_pushbutton()
{
	push_button_set_cb(appln_cfg.wps_pb_gpio,
			   NULL);
	pm_deregister_cb(pm_handle);
}

#else
void hp_configure_wps_pushbutton()
{
}

void hp_unconfigure_wps_pushbutton()
{

}
#endif  /* CONFIG_WPS2 */
