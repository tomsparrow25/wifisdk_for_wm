/*
 *  Copyright (C) 2008-2012, Marvell International Ltd.
 *  All Rights Reserved.
 */
#ifndef PM_MC200_WIFI_DEMODRV_H_
#define PM_MC200_WIFI_DEMODRV_H_

#include "mc200/pm_mc200_wifi_demo_mc200_drv.h"

void pm_mc200_wifi_demo_write_to_lcd(const char *line1, const char *line2);
int pm_mc200_wifi_demo_get_time(int *hour, int *minute);
int pm_mc200_wifi_demo_set_time(int hour, int minute);

#endif /* PM_MC200_WIFI_DEMODRV_H_ */
