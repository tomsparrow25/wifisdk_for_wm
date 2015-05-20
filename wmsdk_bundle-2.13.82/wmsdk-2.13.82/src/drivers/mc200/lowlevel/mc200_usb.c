/*
 *
 * ============================================================================
 * Copyright (c) 2008-2014   Marvell International Ltd. All Rights Reserved.
 *
 *                         Marvell Confidential
 * ============================================================================
 */

#include <mc200.h>
#include <mc200_driver.h>
#include <mc200_pinmux.h>
#include <mc200_clock.h>
#include <mc200_gpio.h>
#include <mc200_pmu.h>
#include <board.h>

/****************************************************************************
 * @brief      Initialize the USB device hardware
 *
 * @param[in]  none
 *
 * @return none
 *
 ***************************************************************************/
void USB_HwInit(void)
{
	/* set usb as 60M clock */
	CLK_AupllConfig_Type usbClkCfg = { 0x10, 0x10E, 0x04, 0x01, 0x00 };

	/* Power on the USB phy */
	SYS_CTRL->USB_PHY_CTRL.BF.REG_PU_USB = 1;

	/* Set the USB clock */
	CLK_AupllEnable(&usbClkCfg);

	/* Enable the USB clock */
	CLK_ModuleClkEnable(CLK_USBC);
	CLK_AupllClkOutEnable(CLK_AUPLL_USB);

	/* Set pinmux as usb function */
	board_usb_pin_config();
}
