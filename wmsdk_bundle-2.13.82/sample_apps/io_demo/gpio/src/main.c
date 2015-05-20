/*
 *  Copyright (C) 2008-2013, Marvell International Ltd.
 *  All Rights Reserved.
 */

#include <wmstdio.h>
#include <wm_os.h>
#include <mdev_gpio.h>
#include <mdev_pinmux.h>
#include <mc200/mc200_gpio.h>
#include <board.h>

/*
 * Simple Application which uses GPIO driver.
 *
 * Summary:
 * This application turns ON a LED when the pushbutton on HW
 * board is pressed and turns OFF the same LED when pushbutton
 * pressed next time.
 *
 * Description:
 *
 * This application is written using APIs exposed by MDEV
 * layer of GPIO driver and Pinmux Driver.
 *
 * The application configures button_2 of HW board so that
 * when user presses the button_2 on board, the led_2 will glow.
 * When user again presses button_2, the led_2 will be turned off.
 */

/*------------------Macro Definitions ------------------*/

#define GPIO_LED_FN  PINMUX_FUNCTION_0
#define GPIO_PUSHBUTTON_FN PINMUX_FUNCTION_0

/*------------------Global Variable Definitions ---------*/

/* This holds  LED gpio pin number */
static unsigned int gpio_led;
/* This holds Pushbutton gpio pin number */
static unsigned int gpio_pushbutton;
/* This indicates the state of LED on or off */
static unsigned int gpio_led_state;

/* This function turns on the LED*/
static void gpio_led_on(void)
{
	mdev_t *gpio_dev = gpio_drv_open("MDEV_GPIO");
	/* Turn on LED by writing  0 in GPIO register */
	gpio_drv_write(gpio_dev, gpio_led, 0);
	gpio_drv_close(gpio_dev);
	gpio_led_state = 1;
}

/* This function turns off the LED*/
static void gpio_led_off(void)
{
	mdev_t *gpio_dev = gpio_drv_open("MDEV_GPIO");
	/* Turn off LED by writing  1 in GPIO register */
	gpio_drv_write(gpio_dev, gpio_led, 1);
	gpio_drv_close(gpio_dev);
	gpio_led_state = 0;
}

/* This function is called when push button is pressed*/
static void pushbutton_cb()
{
	if (gpio_led_state)
		gpio_led_off();
	else
		gpio_led_on();
}

/* Configure GPIO pins to be used as LED and push button */
static void configure_gpios()
{
	mdev_t *pinmux_dev, *gpio_dev;

	/* Initialize  pinmux driver */
	pinmux_drv_init();

	/* Open pinmux driver */
	pinmux_dev = pinmux_drv_open("MDEV_PINMUX");

	/* Initialize GPIO driver */
	gpio_drv_init();

	/* Open GPIO driver */
	gpio_dev = gpio_drv_open("MDEV_GPIO");

	/* Configure GPIO pin function for GPIO connected to LED */
	pinmux_drv_setfunc(pinmux_dev, gpio_led, GPIO_LED_FN);

	/* Confiugre GPIO pin direction as Output */
	gpio_drv_setdir(gpio_dev, gpio_led, GPIO_OUTPUT);
	gpio_drv_write(gpio_dev, gpio_led, 1);

	/* Configure GPIO pin function for GPIO connected to push button */
	pinmux_drv_setfunc(pinmux_dev, gpio_pushbutton, GPIO_PUSHBUTTON_FN);

	/* Confiugre GPIO pin direction as input */
	gpio_drv_setdir(gpio_dev, gpio_pushbutton, GPIO_INPUT);

	/* Register a callback for push button interrupt */
	gpio_drv_set_cb(gpio_dev, gpio_pushbutton, GPIO_INT_FALLING_EDGE,
			pushbutton_cb);

	/* Close drivers */
	pinmux_drv_close(pinmux_dev);
	gpio_drv_close(gpio_dev);
}

/* This is an entry point for the application.
   All application specific initialization is performed here. */
int main(void)
{
	int ret = 0;

	/* Initializes console on UART0 */
	ret = wmstdio_init(UART0_ID, 0);
	if (ret == -WM_FAIL) {
		wmprintf("Failed to initialize console on uart0\r\n");
		return -1;
	}

	wmprintf(" GPIO demo application started\r\n");
	wmprintf(" This application demonstrates the"
		 " use of various GPIO driver APIs\r\n");
	wmprintf(" Press button 2 to toggle state of LED\r\n");

	gpio_led = board_led_2();
	gpio_pushbutton = board_button_2();

	wmprintf(" LED Pin : %d\r\n", gpio_led);
	wmprintf(" Pushbutton Pin : %d\r\n", gpio_pushbutton);

	configure_gpios();
	while (1)
		;
	return 0;
}
