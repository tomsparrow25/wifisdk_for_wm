
/*
 *  Copyright (C) 2012, Marvell International Ltd.
 *  All Rights Reserved.
 */

/* perf_demo_drv.c
 * This file contains the driver for the sensor used in this sample perf_demo
 * application
 */

#include <stdio.h>
#include <wmstdio.h>
#include <string.h>
#include <wmtime.h>
#include <wm_os.h>
#include <peripherals/lcd.h>
#include <board.h>

#include "perf_demo_app.h"
#include "perf_demo_drv.h"

#define BLANK_LINE "                "

/** Driver for the LCD
 *
 * This function takes 2 strings and writes them onto the LCD.
 *
 * Once you have read this, please go to perf_demo_cloud_get_status().
 */
void perf_demo_write_to_lcd(const char *line1, const char *line2)
{
	mdev_t *lcd_dev;

	lcd_dev = lcd_drv_open("MDEV_LCD");

	lcd_drv_write(lcd_dev, BLANK_LINE, BLANK_LINE);
	lcd_drv_write(lcd_dev, line1, line2);

	lcd_drv_close(lcd_dev);
}

int perf_demo_get_time(int *hour, int *minute)
{
	struct tm date_time;
	wmtime_time_get(&date_time);
	*hour = (int)date_time.tm_hour;
	*minute = (int)date_time.tm_min;
	return 0;
}

int perf_demo_set_time(int hour, int minute)
{
	struct tm date_time;

	if (hour < 0 || hour > 23)
		return -1;

	if (minute < 0 || minute > 59)
		return -1;

	wmtime_time_get(&date_time);
	date_time.tm_hour = (char)hour;
	date_time.tm_min = (char)minute;
	wmtime_time_set(&date_time);

	return 0;
}

void user_app_hardfault_handler(void)
{
	board_led_on(board_led_1());
	board_led_on(board_led_2());
	board_led_on(board_led_3());
	board_led_on(board_led_4());
}
