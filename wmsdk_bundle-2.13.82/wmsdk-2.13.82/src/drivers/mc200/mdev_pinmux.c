
/*
 *  Copyright 2008-2013, Marvell International Ltd.
 *  All Rights Reserved.
 */

/*
 * mdev_pinmux.c: generic Pinmux helper functions
 */

#include <wmtypes.h>
#include <wmstdio.h>
#include <mdev_pinmux.h>
#include <wm_os.h>

static mdev_t MDEV_pinmux;
static const char *MDEV_NAME_pinmux = "MDEV_PINMUX";
static os_mutex_t pinmux_mutex;

int pinmux_drv_setfunc(mdev_t *dev, int pin, int func)
{
	int ret = WM_SUCCESS;

	ret = os_mutex_get(&pinmux_mutex, OS_WAIT_FOREVER);
	if (ret == -WM_FAIL) {
		PINMUX_LOG("failed to get mutex\n\r");
		return ret;
	}
	GPIO_PinMuxFun(pin, func);
	os_mutex_put(&pinmux_mutex);

	return ret;
}

int pinmux_drv_close(mdev_t *dev)
{
	return 0;
}

mdev_t *pinmux_drv_open(const char *name)
{
	mdev_t *mdev_p = mdev_get_handle(name);

	if (mdev_p == NULL)
		PINMUX_LOG("driver open called without registering device");
	return mdev_p;
}

int pinmux_drv_init(void)
{
	int ret = WM_SUCCESS;

	if (mdev_get_handle(MDEV_NAME_pinmux) != NULL)
		return ret;

	MDEV_pinmux.name = MDEV_NAME_pinmux;
	ret = os_mutex_create(&pinmux_mutex, "pinmux", OS_MUTEX_INHERIT);
	if (ret == -WM_FAIL) {
		PINMUX_LOG("Failed to create mutex");
		return ret;
	}

	mdev_register(&MDEV_pinmux);

	return ret;
}
