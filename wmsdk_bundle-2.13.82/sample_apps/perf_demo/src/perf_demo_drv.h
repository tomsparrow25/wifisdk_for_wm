/*
 *  Copyright (C) 2008-2012, Marvell International Ltd.
 *  All Rights Reserved.
 */
#ifndef PERF_DEMO_DRV_H_
#define PERF_DEMO_DRV_H_

#include "perf_demo_mc200_drv.h"

void perf_demo_write_to_lcd(const char *line1, const char *line2);
int perf_demo_get_time(int *hour, int *minute);
int perf_demo_set_time(int hour, int minute);
void user_app_hardfault_handler(void);

#endif /* PERF_DEMO_DRV_H_ */
