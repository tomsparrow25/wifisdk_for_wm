/*
 *  Copyright 2008-2013,2011 Marvell International Ltd.
 *  All Rights Reserved.
 */

#include <arch/sys.h>
#include "app_ctrl.h"

app_event_handler_t g_ev_hdlr;

int app_framework_start(app_event_handler_t event_handler)
{
	int ret = wm_wlan_init();
	if (ret != WM_SUCCESS)
		return ret;

	g_ev_hdlr = event_handler;
	return app_ctrl_init();
}
