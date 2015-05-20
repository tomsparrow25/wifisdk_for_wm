/*
 *  Copyright 2008-2013, Marvell International Ltd.
 *  All Rights Reserved.
 */

/*
 * mdev_gpio.c: mdev driver for GPIO
 */
#include <wmstdio.h>
#include <mdev_gpio.h>
#include <wm_os.h>
#include <mc200_clock.h>

static mdev_t MDEV_gpio;
static const char *MDEV_NAME_gpio = "MDEV_GPIO";
static os_mutex_t gpio_mutex;

int gpio_drv_write(mdev_t *dev, int pin, int val)
{
	int ret = WM_SUCCESS;

	ret = os_mutex_get(&gpio_mutex, OS_WAIT_FOREVER);
	if (ret == -WM_FAIL) {
		GPIO_LOG("failed to get mutex\n\r");
		return ret;
	}
	GPIO_WritePinOutput(pin, val);
	os_mutex_put(&gpio_mutex);
	return ret;
}

int gpio_drv_read(mdev_t *dev, int pin, int *val)
{
	int ret = WM_SUCCESS;

	ret = os_mutex_get(&gpio_mutex, OS_WAIT_FOREVER);
	if (ret == -WM_FAIL) {
		GPIO_LOG("failed to get mutex\n\r");
		return ret;
	}
	*val = GPIO_ReadPinLevel(pin);
	os_mutex_put(&gpio_mutex);
	return ret;
}

int gpio_drv_setdir(mdev_t *dev, int pin, int dir)
{
	int ret = WM_SUCCESS;

	ret = os_mutex_get(&gpio_mutex, OS_WAIT_FOREVER);
	if (ret == -WM_FAIL) {
		GPIO_LOG("failed to get mutex\n\r");
		return ret;
	}
	GPIO_SetPinDir(pin, dir);
	os_mutex_put(&gpio_mutex);
	return ret;
}

int gpio_drv_set_cb(mdev_t *dev, int pin, GPIO_Int_Type type,
					intCallback_Type *gpio_cb)
{
	if (pin < 0)
		return -WM_FAIL;

	if (type != GPIO_INT_DISABLE && type != GPIO_INT_BOTH_EDGES
	    && type != GPIO_INT_FALLING_EDGE && type != GPIO_INT_RISING_EDGE)
		return -WM_FAIL;

	int ret = os_mutex_get(&gpio_mutex, OS_WAIT_FOREVER);
	if (ret == -WM_FAIL) {
		GPIO_LOG("failed to get mutex\n\r");
		return ret;
	}
	/* Check if any interrupt is pending for this pin
	 * if so clear it before installing callback
	 */
	if (GPIO_GetIntStatus(pin))
		GPIO_IntClr(pin);

	install_int_callback(INT_GPIO, pin, gpio_cb);
	GPIO_IntConfig(pin, type);
	GPIO_IntMask(pin, (gpio_cb == NULL) ? MASK : UNMASK);
	NVIC_EnableIRQ(GPIO_IRQn);
	os_mutex_put(&gpio_mutex);
	return ret;
}

int gpio_drv_close(mdev_t *dev)
{
	return WM_SUCCESS;
}

mdev_t *gpio_drv_open(const char *name)
{
	mdev_t *mdev_p = mdev_get_handle(name);

	if (mdev_p == NULL) {
		GPIO_LOG("driver open called without registering device"
			 " (%s)\n\r", name);
	}
	return mdev_p;
}

int gpio_drv_init(void)
{
	int ret = WM_SUCCESS;

	if (mdev_get_handle(MDEV_NAME_gpio) != NULL)
		return ret;

	MDEV_gpio.name = MDEV_NAME_gpio;

	ret = os_mutex_create(&gpio_mutex, "gpio", OS_MUTEX_INHERIT);
	if (ret == -WM_FAIL) {
		GPIO_LOG("Failed to create mutex");
		return ret;
	}

	/* Enable GPIO Clock */
	CLK_ModuleClkEnable(CLK_GPIO);

	mdev_register(&MDEV_gpio);

	return ret;
}
