/*
 *  Copyright (C) 2008-2013, Marvell International Ltd.
 *  All Rights Reserved.
 */

#include <wmstdio.h>
#include <wm_os.h>
#include <board.h>
#include <led_indicator.h>

/*-----------------------Global declarations----------------------*/
struct led_private_data {
	uint8_t led_no;
	uint8_t curr_state;
	os_timer_t timer;
	int on_duty_cycle;
	int off_duty_cycle;
} led_data[LED_COUNT];

static int decide_led_array_index(int led_no)
{
	int i;
	for (i = 0; i < LED_COUNT; i++) {
		if (led_data[i].led_no == led_no) {
			return i;
		} else if (led_data[i].led_no == 0) {
			led_data[i].led_no = led_no;
			return i;
		}
	}
	return -WM_FAIL;
}

void led_on(int led_no)
{
	int idx = decide_led_array_index(led_no);
	int ret;

	if (idx == -WM_FAIL)
		return;

	if (os_timer_is_active(&led_data[idx].timer) == WM_SUCCESS) {
		ret = os_timer_delete(&led_data[idx].timer);
		if (ret != WM_SUCCESS) {
			wmprintf("Unable to delete LED timer\n\r");
			return;
		}
	}
	led_data[idx].curr_state = LED_ON;
	board_led_on(led_no);
}

void led_off(int led_no)
{
	int idx = decide_led_array_index(led_no);
	int err;

	if (idx == -WM_FAIL)
		return;
	if (os_timer_is_active(&led_data[idx].timer) == WM_SUCCESS) {
		err = os_timer_delete(&led_data[idx].timer);
		if (err != WM_SUCCESS) {
			wmprintf("Unable to delete LED timer\n\r");
			return;
		}
	}
	led_data[idx].curr_state = LED_OFF;
	board_led_off(led_no);
}

static void led_cb(os_timer_arg_t handle)
{
	int tid = (int) os_timer_get_context(&handle);
	if (tid >= LED_COUNT) {
		return;
	}
	if (led_data[tid].curr_state == LED_ON) {
		board_led_off(led_data[tid].led_no);
		led_data[tid].curr_state = LED_OFF;
		os_timer_change(&led_data[tid].timer,
				led_data[tid].off_duty_cycle, -1);
		os_timer_activate(&led_data[tid].timer);
	} else {
		board_led_on(led_data[tid].led_no);
		led_data[tid].curr_state = LED_ON;
		os_timer_change(&led_data[tid].timer,
				led_data[tid].on_duty_cycle, -1);
		os_timer_activate(&led_data[tid].timer);
	}
}

void led_blink(int led_no, int on_duty_cycle, int off_duty_cycle)
{
	int err, idx;

	idx = decide_led_array_index(led_no);

	if (idx == -WM_FAIL)
		return;

	if (os_timer_is_active(&led_data[idx].timer) == WM_SUCCESS) {
		err = os_timer_delete(&led_data[idx].timer);
		if (err != WM_SUCCESS) {
			return;
		}
	}
	led_data[idx].on_duty_cycle = on_duty_cycle;
	led_data[idx].off_duty_cycle = off_duty_cycle;

	board_led_on(led_no);
	led_data[idx].curr_state = LED_ON;

	err = os_timer_create(&led_data[idx].timer,
			      "led-timer",
			      os_msec_to_ticks(led_data[idx].on_duty_cycle),
			      led_cb,
			      (void *)idx,
			      OS_TIMER_ONE_SHOT,
			      OS_TIMER_AUTO_ACTIVATE);
	if (err != WM_SUCCESS) {
		return;
	}
}
