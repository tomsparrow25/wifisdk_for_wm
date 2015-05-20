/*
 *  Copyright (C) 2008-2013, Marvell International Ltd.
 *  All Rights Reserved.
 */

#ifndef _PUSH_BUTTON_H_
#define _PUSH_BUTTON_H_

#include <wmstdio.h>
#include <wm_os.h>
#include <board.h>

static inline void push_button_set_cb(int pin, intCallback_Type *gpio_cb)
{
	if (pin < 0)
		return;

	gpio_drv_set_cb(NULL, pin, GPIO_INT_FALLING_EDGE,
			gpio_cb);
}

static inline void push_button_reset_cb(int pin)
{
	if (pin < 0)
		return;

	gpio_drv_set_cb(NULL, pin, GPIO_INT_FALLING_EDGE,
			NULL);
}
#endif /* ! _PUSH_BUTTON_H_ */
