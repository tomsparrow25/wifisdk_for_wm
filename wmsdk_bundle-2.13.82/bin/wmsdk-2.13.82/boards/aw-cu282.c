/*
 *  *  Copyright (C) 2008-2012, Marvell International Ltd.
 *   *  All Rights Reserved.
 *    */
/*
 *    This is a board specific configuration file for
 *    AW-CU282 module (based on schematic
 */

#include <wmtypes.h>
#include <wmerrno.h>
#include <wm_os.h>
#include <board.h>
#include <mc200_gpio.h>
#include <mc200_pinmux.h>
#include <mc200_pmu.h>
#include <mc200_uart.h>
#include <mc200_i2c.h>

#ifdef CONFIG_WiFi_8801
#error You have selected Wi-Fi 8801 based chipset. \
	Please use "make mc200_defconfig" to select 878x-based Chipset.
#endif

int board_main_xtal()
{
	return 32000000;
}

int board_main_osc()
{
	return -WM_FAIL;
}

int board_cpu_freq()
{
	return 200000000;
}

int board_32k_xtal()
{
	return false;
}

int board_32k_osc()
{
	return false;
}

int board_card_detect()
{
	return true;
}

void board_sdio_pdn()
{
	GPIO_PinMuxFun(GPIO_17, GPIO17_GPIO17);
	GPIO_SetPinDir(GPIO_17, GPIO_OUTPUT);
	GPIO_WritePinOutput(GPIO_17, GPIO_IO_LOW);
}

void board_sdio_pwr()
{
	GPIO_WritePinOutput(GPIO_17, GPIO_IO_HIGH);
}

void board_sdio_reset()
{
	GPIO_PinMuxFun(GPIO_9, GPIO9_GPIO9);
	GPIO_SetPinDir(GPIO_9, GPIO_OUTPUT);
	GPIO_WritePinOutput(GPIO_9, GPIO_IO_LOW);
	_os_delay(200);
	GPIO_WritePinOutput(GPIO_9, GPIO_IO_HIGH);
	_os_delay(200);
}

int board_sdio_pdn_support()
{
	return true;
}

int board_button_3()
{
	return -WM_FAIL;
}

int board_button_pressed(int pin)
{
	return false;
}

void board_gpio_power_on()
{	/*This switches on power  to GPIO blocks on MC200*/
	GPIO_PinMuxFun(GPIO_18, PINMUX_FUNCTION_6);
	PMU_ConfigVDDIOLevel(PMU_VDDIO_D0, PMU_VDDIO_LEVEL_1P8V);
	PMU_ConfigVDDIOLevel(PMU_VDDIO_SDIO, PMU_VDDIO_LEVEL_1P8V);
	PMU_ConfigWakeupPin(PMU_GPIO26_INT, PMU_WAKEUP_LEVEL_LOW);
}

void board_uart_pin_config(int id)
{
	switch (id) {
	case UART0_ID:
		GPIO_PinMuxFun(GPIO_74, GPIO74_UART0_TXD);
		GPIO_PinMuxFun(GPIO_75, GPIO75_UART0_RXD);
		break;
	case UART1_ID:
	case UART2_ID:
	case UART3_ID:
		/* Not implemented yet */
		break;
	}
}

void board_i2c_pin_config(int id)
{
	switch (id) {
	case I2C0_PORT:
		/* Not implemented */
		break;
	case I2C1_PORT:
		/* Not implemented */
		break;
	case I2C2_PORT:
		/* Not implemented */
		break;
	}
}

void board_usb_pin_config()
{
}

void board_ssp_pin_config(int id, bool cs)
{
}

void board_sdio_pin_config()
{
	GPIO_PinMuxFun(GPIO_51, GPIO51_SDIO_CLK);
	GPIO_PinMuxFun(GPIO_52, GPIO52_SDIO_3);
	GPIO_PinMuxFun(GPIO_53, GPIO53_SDIO_2);
	GPIO_PinMuxFun(GPIO_54, GPIO54_SDIO_1);
	GPIO_PinMuxFun(GPIO_55, GPIO55_SDIO_0);
	GPIO_PinMuxFun(GPIO_56, GPIO56_SDIO_CMD);
}

/*
 *	Application Specific APIs
 *	Define these only if your application needs/uses them.
 */
int board_led_1()
{
	return -WM_FAIL;
}

int board_led_2()
{
	return -WM_FAIL;
}

int board_led_3()
{
	return -WM_FAIL;
}

int board_led_4()
{
	return -WM_FAIL;
}

void board_led_on(int pin)
{
	if (pin < 0)
		return;

	GPIO_PinMuxFun(pin, PINMUX_FUNCTION_0);
	GPIO_SetPinDir(pin, GPIO_OUTPUT);
	GPIO_WritePinOutput(pin, GPIO_IO_LOW);
}

void board_led_off(int pin)
{
	if (pin < 0)
		return;

	GPIO_PinMuxFun(pin, PINMUX_FUNCTION_0);
	GPIO_SetPinDir(pin, GPIO_OUTPUT);
	GPIO_WritePinOutput(pin, GPIO_IO_HIGH);
}

int board_button_1()
{
	return -WM_FAIL;
}

int board_button_2()
{
	return -WM_FAIL;
}

void board_lcd_backlight_on()
{
}
void board_lcd_backlight_off()
{
}

void board_lcd_reset()
{
}

int board_wifi_host_wakeup()
{
	return 4;
}

int board_wakeup0_functional()
{
	return true;
}

int  board_wakeup1_functional()
{
	return true;
}

int board_wakeup0_wifi()
{
	return false;
}

int board_wakeup1_wifi()
{
	return true;
}
