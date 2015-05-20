/*
 *  Copyright (C) 2008-2013, Marvell International Ltd.
 *  All Rights Reserved.
 */

/*
 * This is a board specific configuration file for the High-Flying based
 * modules. The name/version is printed on the board.
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
#include <mc200_ssp.h>

#ifdef CONFIG_WiFi_8801
#error You have selected Wi-Fi 8801 based chipset. \
	Please use "make mc200_defconfig" to select 878x-based Chipset.
#endif

int board_main_xtal()
{
	return 12000000;
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
	return true;
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
	GPIO_PinMuxFun(GPIO_68, GPIO68_GPIO68);
	GPIO_SetPinDir(GPIO_68, GPIO_OUTPUT);
	GPIO_WritePinOutput(GPIO_68, GPIO_IO_LOW);
}

void board_sdio_pwr()
{
	GPIO_PinMuxFun(GPIO_68, GPIO68_GPIO68);
	GPIO_SetPinDir(GPIO_68, GPIO_OUTPUT);
	GPIO_WritePinOutput(GPIO_68, GPIO_IO_HIGH);
}

void board_sdio_reset()
{
	GPIO_PinMuxFun(GPIO_4, GPIO4_GPIO4);
	GPIO_SetPinDir(GPIO_4, GPIO_OUTPUT);
	GPIO_WritePinOutput(GPIO_4, GPIO_IO_LOW);
	_os_delay(200);
	GPIO_WritePinOutput(GPIO_4, GPIO_IO_HIGH);
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
{	if (pin < 0)
		return false;

	GPIO_PinMuxFun(pin, PINMUX_FUNCTION_0);
	GPIO_SetPinDir(pin, GPIO_INPUT);
	if (GPIO_ReadPinLevel(pin) == GPIO_IO_LOW)
		return true;

	return false;
}

void board_gpio_power_on()
{
	PMU_ConfigVDDIOLevel(PMU_VDDIO_D0, PMU_VDDIO_LEVEL_1P8V);
	PMU_ConfigVDDIOLevel(PMU_VDDIO_SDIO, PMU_VDDIO_LEVEL_1P8V);
	PMU_ConfigWakeupPin(PMU_GPIO26_INT, PMU_WAKEUP_LEVEL_LOW);
	/* Enable the Boot override button */
	PMU_PowerOnVDDIO(PMU_VDDIO_D0);
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

void board_sdio_pin_config()
{
	GPIO_PinMuxFun(GPIO_51, GPIO51_SDIO_CLK);
	GPIO_PinMuxFun(GPIO_52, GPIO52_SDIO_3);
	GPIO_PinMuxFun(GPIO_53, GPIO53_SDIO_2);
	GPIO_PinMuxFun(GPIO_54, GPIO54_SDIO_1);
	GPIO_PinMuxFun(GPIO_55, GPIO55_SDIO_0);
	GPIO_PinMuxFun(GPIO_56, GPIO56_SDIO_CMD);
}

void board_i2c_pin_config(int id)
{
	switch (id) {
	case I2C0_PORT:
		GPIO_PinMuxFun(GPIO_44, GPIO44_I2C0_SDA);
		GPIO_PinMuxFun(GPIO_45, GPIO45_I2C0_SCL);
		break;
	case I2C1_PORT:
		GPIO_PinMuxFun(GPIO_4, GPIO4_I2C1_SDA);
		GPIO_PinMuxFun(GPIO_5, GPIO5_I2C1_SCL);
		break;
	case I2C2_PORT:
		GPIO_PinMuxFun(GPIO_10, GPIO10_I2C2_SDA);
		GPIO_PinMuxFun(GPIO_11, GPIO11_I2C2_SCL);
		break;
	}
}

void board_usb_pin_config()
{
}

void board_ssp_pin_config(int id, bool cs)
{
	switch (id) {
	case SSP0_ID:
		GPIO_PinMuxFun(GPIO_32, GPIO32_SSP0_CLK);
		if (cs)
			GPIO_PinMuxFun(GPIO_33, GPIO33_SSP0_FRM);
		GPIO_PinMuxFun(GPIO_34, GPIO34_SSP0_RXD);
		GPIO_PinMuxFun(GPIO_35, GPIO35_SSP0_TXD);
		break;
	case SSP1_ID:
		GPIO_PinMuxFun(GPIO_63, GPIO63_SSP1_CLK);
		if (cs)
			GPIO_PinMuxFun(GPIO_64, GPIO64_SSP1_FRM);
		GPIO_PinMuxFun(GPIO_65, GPIO65_SSP1_RXD);
		GPIO_PinMuxFun(GPIO_66, GPIO66_SSP1_TXD);
		break;
	case SSP2_ID:
		GPIO_PinMuxFun(GPIO_40, GPIO40_SSP2_CLK);
		if (cs)
			GPIO_PinMuxFun(GPIO_41, GPIO41_SSP2_FRM);
		GPIO_PinMuxFun(GPIO_42, GPIO42_SSP2_RXD);
		GPIO_PinMuxFun(GPIO_43, GPIO43_SSP2_TXD);
		break;
	}
}

int board_led_1()
{
	return GPIO_28;
}

int board_led_2()
{
	return GPIO_30;
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
	GPIO_PinMuxFun(GPIO_2, PINMUX_FUNCTION_0);
	GPIO_SetPinDir(GPIO_2, GPIO_INPUT);

	return GPIO_2;
}

int board_button_wps()
{
	GPIO_PinMuxFun(GPIO_17, PINMUX_FUNCTION_0);
	GPIO_SetPinDir(GPIO_17, GPIO_INPUT);

	return GPIO_17;
}

int board_button_sleep_rq()
{
	GPIO_PinMuxFun(GPIO_25, PINMUX_FUNCTION_0);
	GPIO_SetPinDir(GPIO_25, GPIO_INPUT);

	return GPIO_25;
}

int board_button_nreload()
{
	GPIO_PinMuxFun(GPIO_5, PINMUX_FUNCTION_0);
	GPIO_SetPinDir(GPIO_5, GPIO_INPUT);

	return GPIO_5;
}

int board_button_2()
{
	GPIO_PinMuxFun(GPIO_17, PINMUX_FUNCTION_0);
	GPIO_SetPinDir(GPIO_17, GPIO_INPUT);

	return GPIO_17;
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
	return 26;
}

int board_wakeup0_functional()
{
	return true;
}

int board_wakeup1_functional()
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
