/*
 *  Copyright (C) 2012, Marvell International Ltd.
 *  All Rights Reserved.
 */

/* This project demonstrates power manager functionality and different PM modes
 * of 88MC200 SoC */

#include <wmstdio.h>
#include <wm_os.h>
#include <arch/pm_mc200.h>
#include <pwrmgr.h>
#include <wmtime.h>
#include <cli.h>
#include <stdlib.h>
#include <mc200_pmu.h>
#include <board.h>
#include "peripheralsOn.h"
#include "peripheralsOff.h"
#include<arch/pm_mc200.h>
#include "mc200_xflash.h"
#include "mc200_pinmux.h"
#include <flash.h>


typedef enum {
	MC200_PM_CASE1 = 1,
	MC200_PM_CASE2 = 2,
	MC200_PM_CASE3 = 3,
	MC200_PM_CASE4 = 4,
	MC200_PM_CASE5 = 5,
	MC200_PM_CASE6 = 6,
	MC200_PM_CASE7 = 7,
	MC200_PM_CASE8 = 8,
	MC200_PM_CASE9 = 9,
	MC200_PM_CASE10 = 10,
	MC200_PM_CASE11 = 11,
	MC200_PM_CASE12 = 12,
	MC200_PM_CASE13 = 13,
	MC200_PM_CASE14 = 14,
	MC200_PM_CASE15 = 15,

} MC200_PM_CASE;

void pm_pm0_in_idle();

void pm_mc200_case(MC200_PM_CASE mc200_pm_case);

 /* case for pm0 */
void pm0_sys200m_xtal32m_allperi_on(void);
void pm0_sys200m_xtal32m_allperi_off(void);
void pm0_sys200m_rc32m_allperi_on(void);
void pm0_sys200m_rc32m_allperi_off(void);

void pm0_sys32m_xtal32m_allperi_on(void);
void pm0_sys32m_xtal32m_allperi_off(void);
void pm0_sys32m_rc32m_allperi_on(void);
void pm0_sys32m_rc32m_allperi_off(void);

 /* cases for pm1 */
void pm1_sys200m_xtal32m_allperi_on(void);
void pm1_sys200m_xtal32m_allperi_off(void);
void pm1_sys200m_rc32m_allperi_on(void);
void pm1_sys200m_rc32m_allperi_off(void);

 /* cases for pm2 */
void pm2_sys1m_rc32m_allperi_off(void);

 /* case for pm3 */
void pm3_sys1m_rc32m_allperi_off(void);

 /* case for pm4 */
void pm4_sys1m_rc32m_allperi_off(void);

void RC32M_On(void);
void SFLL200M_On(CLK_Src_Type srcClock);
void AUPLL_On(CLK_Src_Type srcClock);

void mc200_gpio_power_on(void);

void delay(uint32_t cycle)
{
	cycle = (cycle << 7);
	while (cycle--)
		;
}

void xflash_off()
{
	/* Power on QSPI1 related VDDIO domain */
	PMU_PowerOnVDDIO(PMU_VDDIO_D2);
	/* QSPI1 module clock enable */
	CLK_ModuleClkEnable(CLK_QSPI1);
	/* GPIO module clock enable */
	CLK_ModuleClkEnable(CLK_GPIO);
	/* Configure GPIO as QSPI1 function */
	GPIO_PinMuxFun(GPIO_72, GPIO72_QSPI1_SSn);
	GPIO_PinMuxFun(GPIO_73, GPIO73_QSPI1_CLK);
	GPIO_PinMuxFun(GPIO_76, GPIO76_QSPI1_D0);
	GPIO_PinMuxFun(GPIO_77, GPIO77_QSPI1_D1);
	GPIO_PinMuxFun(GPIO_78, GPIO78_QSPI1_D2);
	GPIO_PinMuxFun(GPIO_79, GPIO79_QSPI1_D3);
	XFLASH_PowerDown(ENABLE);
}

void pm_mc200_case(MC200_PM_CASE pm_mc200_case)
{
	wmprintf(" Please reset the device once readings are noted\r\n");
	switch (pm_mc200_case) {
		/* PM0 (200MHz) + External crystal XTAL32MHz
		 * IPs ON */
	case MC200_PM_CASE1:
		wmprintf("\r\n Power Mode : PM0\r\n");
		wmprintf(" System Clock : 200M(S = XTAL32M)\r\n");
		wmprintf(" Clock State: RC32M = Off; XTAL32M = On\r\n ");
		wmprintf("SFLL = 200M; AUPLL = On \r\n");
		wmprintf(" All Peripherals On\r\n");
		wmstdio_flush();
		pm_pm0_in_idle();
		pm0_sys200m_xtal32m_allperi_on();
		break;

		/* PM0 (200MHz) + External crystal XTAL32MHz
		 * IPs OFF */
	case MC200_PM_CASE2:
		wmprintf("\r\n Power Mode : PM0\r\n");
		wmprintf(" System Clock : 200M(S = XTAL32M)\r\n");
		wmprintf(" Clock State: RC32M = Off; XTAL32M = On\r\n ");
		wmprintf("SFLL = 200M; AUPLL = Off \r\n");
		wmprintf(" All Peripherals Off\r\n");
		wmstdio_flush();
		pm_pm0_in_idle();
		pm0_sys200m_xtal32m_allperi_off();
		break;

		/* PM0 (200MHz) + RC 32KHz
		 * IPs ON */
	case MC200_PM_CASE3:
		wmprintf("\r\n Power Mode : PM0\r\n");
		wmprintf(" System Clock : 200M(S = RC32M)\r\n");
		wmprintf(" Clock State: RC32M = On; XTAL32M = Off\r\n ");
		wmprintf("SFLL = 200M; AUPLL = On \r\n");
		wmprintf(" All Peripherals On\r\n");
		wmstdio_flush();
		pm_pm0_in_idle();
		pm0_sys200m_rc32m_allperi_on();
		break;

		/* PM0(200MHz) + RC 32MHz
		 * IPs OFF */
	case MC200_PM_CASE4:
		wmprintf("\r\n Power Mode : PM0\r\n");
		wmprintf(" System Clock : 200M(S = RC32M)\r\n");
		wmprintf(" Clock State: RC32M = On; XTAL32M = Off\r\n ");
		wmprintf("SFLL = 200M; AUPLL = On \r\n");
		wmprintf(" All Peripherals Off\r\n");
		wmstdio_flush();
		pm_pm0_in_idle();
		pm0_sys200m_rc32m_allperi_off();
		break;

		/* PM0 (32MHz) + External crystal 32MHz
		 * IPs ON */
	case MC200_PM_CASE5:
		wmprintf("\r\n Power Mode : PM0\r\n");
		wmprintf(" System Clock : 32M(S = XTAL32M)\r\n");
		wmprintf(" Clock State: RC32M = On; XTAL32M = Off\r\n ");
		wmprintf("SFLL = Off; AUPLL = On \r\n");
		wmprintf(" All Peripherals On\r\n");
		wmstdio_flush();
		pm_pm0_in_idle();
		pm0_sys32m_xtal32m_allperi_on(); /* check for extern32M */
		break;

		/* PM0 (32MHz) + External crystal 32MHz
		 * IPs OFF */
	case MC200_PM_CASE6:
		wmprintf("\r\n Power Mode : PM0\r\n");
		wmprintf(" System Clock : 32M(S = XTAL32M)\r\n");
		wmprintf(" Clock State: RC32M = On; XTAL32M = Off\r\n ");
		wmprintf("SFLL = Off; AUPLL = Off \r\n");
		wmprintf(" All Peripherals Off\r\n");
		wmstdio_flush();
		pm_pm0_in_idle();
		pm0_sys32m_xtal32m_allperi_off(); /* check for extern32M */
		break;


		/* PM0 (32MHz) + RC 32KHz */
		/* IPs ON */
	case MC200_PM_CASE7:
		wmprintf("\r\n Power Mode : PM0\r\n");
		wmprintf(" System Clock : 32M(S = RC32M)\r\n");
		wmprintf(" Clock State: RC32M = On; XTAL32M = Off\r\n ");
		wmprintf("SFLL = Off; AUPLL = On \r\n");
		wmprintf(" All Peripherals On\r\n");
		wmstdio_flush();
		pm_pm0_in_idle();
		pm0_sys32m_rc32m_allperi_on();
		break;

		/* PM0 (32MHz) + RC 32MHz
		 * IPs OFF */
	case MC200_PM_CASE8:
		wmprintf("\r\n Power Mode : PM0\r\n");
		wmprintf(" System Clock : 32M(S = RC32M)\r\n");
		wmprintf(" Clock State: RC32M = On; XTAL32M = Off\r\n ");
		wmprintf("SFLL = Off; AUPLL = Off \r\n");
		wmprintf(" All Peripherals Off\r\n");
		wmstdio_flush();
		pm_pm0_in_idle();
		pm0_sys32m_rc32m_allperi_off();
		break;

		/* PM1 (200MHz) + External crystal XTAL32MHz
		 * IPs ON */
	case MC200_PM_CASE9:
		wmprintf("\r\n Power Mode : PM1\r\n");
		wmprintf(" System Clock : 200M(S = XTAL32M)\r\n");
		wmprintf(" Clock State: RC32M = Off; XTAL32M = On\r\n ");
		wmprintf("SFLL = On; AUPLL = On \r\n");
		wmprintf(" All Peripherals On\r\n");
		wmstdio_flush();
		pm1_sys200m_xtal32m_allperi_on();
		break;

		/* PM1 (200MHz) + External crystal XTAL32MHz
		 * IPs OFF */
	case MC200_PM_CASE10:
		wmprintf("\r\n Power Mode : PM1\r\n");
		wmprintf(" System Clock : 200M(S = XTAL32M)\r\n");
		wmprintf(" Clock State: RC32M = Off; XTAL32M = On\r\n ");
		wmprintf("SFLL = ON; AUPLL = Off \r\n");
		wmprintf(" All Peripherals Off\r\n");
		wmstdio_flush();
		pm1_sys200m_xtal32m_allperi_off();
		break;

		/* PM1 (200MHz) + RC32MHz
		 * IPs ON */
	case MC200_PM_CASE11:
		wmprintf("\r\n Power Mode : PM1\r\n");
		wmprintf(" System Clock : 200M(S = RC32M)\r\n");
		wmprintf(" Clock State: RC32M = On; XTAL32M = Off\r\n ");
		wmprintf("SFLL = ON; AUPLL = ON \r\n");
		wmprintf(" All Peripherals On\r\n");
		wmstdio_flush();
		pm1_sys200m_rc32m_allperi_on();
		break;

		/* PM1 (200MHz) + RC32MHz
		 * IPs OFF */
	case MC200_PM_CASE12:
		wmprintf("\r\n Power Mode : PM1\r\n");
		wmprintf(" System Clock : 200M(S = RC32M)\r\n");
		wmprintf(" Clock State: RC32M = On; XTAL32M = Off\r\n ");
		wmprintf("SFLL = On; AUPLL = Off \r\n");
		wmprintf(" All Peripherals Off\r\n");
		wmstdio_flush();
		pm1_sys200m_rc32m_allperi_off();
		break;

		/* PM2 (1MHz) + RC32M
		 * IPS OFF */
	case MC200_PM_CASE13:
		wmprintf("\r\n Power Mode : PM2\r\n");
		wmprintf(" System Clock : 32M(S = RC32M)\r\n");
		wmprintf(" Clock State: RC32M = On; XTAL32M = Off\r\n ");
		wmprintf("SFLL = Off; AUPLL = Off \r\n");
		wmprintf(" All Peripherals Off\r\n");
		wmstdio_flush();
		pm2_sys1m_rc32m_allperi_off();
		break;

		/* PM3 (1MHz) + RC32M
		 * IPS OFF */
	case MC200_PM_CASE14:
		wmprintf("\r\n Power Mode : PM3\r\n");
		wmprintf(" System Clock : 32M(S=RC32M)\r\n");
		wmprintf(" Clock State: RC32M = On; XTAL32M = Off\r\n ");
		wmprintf("SFLL = Off; AUPLL = Off \r\n");
		wmprintf(" All Peripherals Off\r\n");
		wmstdio_flush();
		pm3_sys1m_rc32m_allperi_off();
		break;

		/* PM4 (1MHz) + RC32M
		 * IPS OFF */
	case MC200_PM_CASE15:
		wmprintf("\r\n Power Mode : PM4\r\n");
		wmprintf(" System Clock : 32M(S = RC32M)\r\n");
		wmprintf(" Clock State: RC32M = On; XTAL32M = Off\r\n ");
		wmprintf("SFLL = Off; AUPLL = Off \r\n");
		wmprintf(" All Peripherals Off\r\n");
		wmstdio_flush();
		pm4_sys1m_rc32m_allperi_off();
		break;

	default:
		break;
	}
}

void pm0_sys200m_xtal32m_allperi_on(void)
{
	/* MC200_PM_CASE1 */

	/* clk init start */
	/* Turn on XTAL32M */
	CLK_MainXtalEnable(CLK_OSC_INTERN);
	while (CLK_GetClkStatus(CLK_MAINXTAL) == RESET)
		;

	/* Turn on SFLL to 200M */
	SFLL200M_On(CLK_MAINXTAL);

	while (CLK_GetClkStatus(CLK_SFLL) == RESET)
		;
	/* System clock source to SFLL 200M */
	CLK_SystemClkSrc(CLK_SFLL);

	/* Turn off RC32M when it is not used as clock source */
	CLK_RC32MDisable();
	/* clk init end */

	/* Turn on AUPLL */
	AUPLL_On(CLK_MAINXTAL);

	/* configure VDDIO_D0 and VDDIO_SDIO to 1.8v */
	mc200_gpio_power_on();

	/* PMU power domian */
	PMU_PowerOnVDDIO(PMU_VDDIO_D0);
	PMU_PowerOnVDDIO(PMU_VDDIO_AON);
	PMU_PowerOnVDDIO(PMU_VDDIO_D1);
	PMU_PowerOnVDDIO(PMU_VDDIO_SDIO);
	PMU_PowerOnVDDIO(PMU_VDDIO_D2);
	PMU_PowerOnVDDIO(PMU_VDDIO_FL);

	/* This function will enable all the module clock and cau clock */
	/* You can also call CLK_ModuleClkEnable(moduleNme) */
	PeriClk_On();

	/* IPs Enable */
	ULP_On();
	BRN_On();
	ACOMP_On();
	ADC_On();
	DAC_On();
	DMA_On();
	GPT_On();
	I2C_On();
	QSPI0_On();
	QSPI1_On();
	/* RTC is On by default */
	SDIO_On();
	SSP_On();
	USB_On();
	AES_On();
	CRC_On();
	FLASHHC_On();
	UART_On();
	WDT_On();
	GPIO_On();
}

void pm0_sys200m_xtal32m_allperi_off()
{
	/* MC200_PM_CASE2 */

	/* Turn on XTAL32M */
	CLK_MainXtalEnable(CLK_OSC_INTERN);
	while (CLK_GetClkStatus(CLK_MAINXTAL) == RESET)
		;

	/* sys clk : CLK_MAINXTAL */
	CLK_SystemClkSrc(CLK_MAINXTAL);

	/* Turn on SFLL to 200M */
	SFLL200M_On(CLK_MAINXTAL);

	/* Turn off RC32M when it is not used as clock source */
	CLK_RC32MDisable();

	/* Turn off AUPLL */
	CLK_AupllDisable();

	/* configure VDDIO_D0 and VDDIO_SDIO to 1.8v */
	mc200_gpio_power_on();

	/* This function will disable all the module clock and cau clock */
	/* You can also call CLK_ModuleClkDisable(moduleNme) */
	PeriClk_Off();


	/* IPs Enable */
	ULP_On();
	BRN_On();

	ACOMP_Off();
	ADC_Off();
	DAC_Off();
	DMA_Off();
	GPT_Off();
	I2C_Off();
	QSPI0_Off();
	QSPI1_Off();
	RTC_Off();
	SDIO_Off();
	SSP_Off();
	USB_Off();
	AES_Off();
	CRC_Off();
	FLASHHC_Off();
	UART_Off();
	WDT_Off();
	GPIO_Off();

	/* PMU power domian */
	PMU_PowerOffVDDIO(PMU_VDDIO_D0);
	PMU_PowerOnVDDIO(PMU_VDDIO_AON);
	PMU_PowerOffVDDIO(PMU_VDDIO_D1);
	PMU_PowerOffVDDIO(PMU_VDDIO_SDIO);
	PMU_PowerOffVDDIO(PMU_VDDIO_D2);
	PMU_PowerOffVDDIO(PMU_VDDIO_FL);

}

void pm0_sys200m_rc32m_allperi_on(void)
{
	/* MC200_PM_CASE3 */

	/* Turn On RC32M */
	RC32M_On();
	/* Turn on SFLL to 200M */
	SFLL200M_On(CLK_RC32M);

	 while (CLK_GetClkStatus(CLK_SFLL) == RESET)
		;
	/* System clock source to SFLL 200M */
	CLK_SystemClkSrc(CLK_SFLL);

	/* Turn off XTAL32M when it is not used as clock source */
	CLK_MainXtalDisable();

	/* Turn on AUPLL */
	AUPLL_On(CLK_RC32M);

	/* configure VDDIO_D0 and VDDIO_SDIO to 1.8v */
	mc200_gpio_power_on();

	/* PMU power domian */
	PMU_PowerOnVDDIO(PMU_VDDIO_D0);
	PMU_PowerOnVDDIO(PMU_VDDIO_AON);
	PMU_PowerOnVDDIO(PMU_VDDIO_D1);
	PMU_PowerOnVDDIO(PMU_VDDIO_SDIO);
	PMU_PowerOnVDDIO(PMU_VDDIO_D2);
	PMU_PowerOnVDDIO(PMU_VDDIO_FL);

	/* This function will enable all the module clock and cau clock */
	/* You can also call CLK_ModuleClkEnable(moduleNme) */
	PeriClk_On();

	/* IPs Enable */
	ULP_On();
	BRN_On();
	ACOMP_On();
	ADC_On();
	DAC_On();
	DMA_On();
	GPT_On();
	I2C_On();
	QSPI0_On();
	QSPI1_On();
	/* RTC is on by default */
	SDIO_On();
	SSP_On();
	USB_On();
	AES_On();
	CRC_On();
	FLASHHC_On();
	UART_On();
	WDT_On();
	GPIO_On();
}

void pm0_sys200m_rc32m_allperi_off()
{
	/* MC200_PM_CASE4 */

	/* Turn On RC32M */
	RC32M_On();

	/* System clock source to SFLL 200M */
	CLK_SystemClkSrc(CLK_SFLL);

	while (CLK_GetClkStatus(CLK_SFLL) == RESET)
		;

	/* System clock source to SFLL 200M */
	CLK_SystemClkSrc(CLK_SFLL);

	/* Turn off XTAL32M */
	CLK_MainXtalDisable();

	/* Turn off AUPLL */
	CLK_AupllDisable();

	/* configure VDDIO_D0 and VDDIO_SDIO to 1.8v */
	mc200_gpio_power_on();

	/* This function will disable all the module clock and cau clock */
	/* You can also call CLK_ModuleClkDisable(moduleNme) */
	PeriClk_Off();

	/* IPs Enable */
	ULP_On();
	BRN_On();

	ACOMP_Off();
	ADC_Off();
	DAC_Off();
	DMA_Off();
	GPT_Off();
	I2C_Off();
	QSPI0_Off();
	QSPI1_Off();
	RTC_Off();
	SDIO_Off();
	SSP_Off();
	USB_Off();
	AES_Off();
	CRC_Off();
	FLASHHC_Off();
	UART_Off();
	WDT_Off();
	GPIO_Off();

	/* PMU power domian */
	PMU_PowerOffVDDIO(PMU_VDDIO_D0);
	PMU_PowerOnVDDIO(PMU_VDDIO_AON);
	PMU_PowerOffVDDIO(PMU_VDDIO_D1);
	PMU_PowerOffVDDIO(PMU_VDDIO_SDIO);
	PMU_PowerOffVDDIO(PMU_VDDIO_D2);
	PMU_PowerOffVDDIO(PMU_VDDIO_FL);

}

void pm0_sys32m_xtal32m_allperi_on(void)
{
	/* MC200_PM_CASE55 */

	/* Turn on XTAL32M */
	CLK_MainXtalEnable(CLK_OSC_INTERN);

	while (CLK_GetClkStatus(CLK_MAINXTAL) == RESET)
		;

	/* System clock source to CLK_MAINXTAL */
	CLK_SystemClkSrc(CLK_MAINXTAL);

	/* Turn off RC32M */
	CLK_RC32MDisable();

	/* Turn on AUPLL */
	AUPLL_On(CLK_MAINXTAL);

	/* configure VDDIO_D0 and VDDIO_SDIO to 1.8v */
	mc200_gpio_power_on();

	/* PMU power domian */
	PMU_PowerOnVDDIO(PMU_VDDIO_D0);
	PMU_PowerOnVDDIO(PMU_VDDIO_AON);
	PMU_PowerOnVDDIO(PMU_VDDIO_D1);
	PMU_PowerOnVDDIO(PMU_VDDIO_SDIO);
	PMU_PowerOnVDDIO(PMU_VDDIO_D2);
	PMU_PowerOnVDDIO(PMU_VDDIO_FL);

	/* This function will enable all the module clock and cau clock */
	/* You can also call CLK_ModuleClkEnable(moduleNme) */
	PeriClk_On();

	/* IPs Enable */
	ULP_On();
	BRN_On();
	ACOMP_On();
	ADC_On();
	DAC_On();
	DMA_On();
	GPT_On();
	I2C_On();
	QSPI0_On();
	QSPI1_On();
	/* RTC is On by default */
	SDIO_On();
	SSP_On();
	USB_On();
	AES_On();
	CRC_On();
	FLASHHC_On();
	UART_On();
	WDT_On();
	GPIO_On();

}

void pm0_sys32m_xtal32m_allperi_off(void)
{
	/* MC200_PM_CASE5 = 6 */

	/* Turn on XTAL32M */
	CLK_MainXtalEnable(CLK_OSC_INTERN);

	while (CLK_GetClkStatus(CLK_MAINXTAL) == RESET)
		;

	/* System clock source to CLK_MAINXTAL */
	CLK_SystemClkSrc(CLK_MAINXTAL);

	/* Turn off RC32M */
	CLK_RC32MDisable();

	CLK_SfllDisable();

	/* Turn off AUPLL */
	CLK_AupllDisable();

	/* This function will disable all the module clock and cau clock */
	/* You can also call CLK_ModuleClkDisable(moduleNme) */
	PeriClk_Off();

	/* IPs Enable */
	ULP_On();
	BRN_On();

	ACOMP_Off();
	ADC_Off();
	DAC_Off();
	DMA_Off();
	GPT_Off();
	I2C_Off();
	QSPI0_Off();
	QSPI1_Off();
	RTC_Off();
	SDIO_Off();
	SSP_Off();
	USB_Off();
	AES_Off();
	CRC_Off();
	FLASHHC_Off();
	UART_Off();
	WDT_Off();
	GPIO_Off();

	/* PMU power domian */
	PMU_PowerOffVDDIO(PMU_VDDIO_D0);
	PMU_PowerOnVDDIO(PMU_VDDIO_AON);
	PMU_PowerOffVDDIO(PMU_VDDIO_D1);
	PMU_PowerOffVDDIO(PMU_VDDIO_SDIO);
	PMU_PowerOffVDDIO(PMU_VDDIO_D2);
	PMU_PowerOffVDDIO(PMU_VDDIO_FL);

}

void pm0_sys32m_rc32m_allperi_on(void)
{
	/* MC200_PM_CASE7 */

	/* Turn On RC32M */
	RC32M_On();

	while (CLK_GetClkStatus(CLK_RC32M) == RESET)
		;

	/* System clock source to RC32M */
	CLK_SystemClkSrc(CLK_RC32M);

	/* Turn off XTAL32M */
	CLK_MainXtalDisable();

	/* Turn off SFLL */
	CLK_SfllDisable();

	/* Turn on AUPLL */
	AUPLL_On(CLK_RC32M);

	/* configure VDDIO_D0 and VDDIO_SDIO to 1.8v */
	mc200_gpio_power_on();

	/* PMU power domian */
	PMU_PowerOnVDDIO(PMU_VDDIO_D0);
	PMU_PowerOnVDDIO(PMU_VDDIO_AON);
	PMU_PowerOnVDDIO(PMU_VDDIO_D1);
	PMU_PowerOnVDDIO(PMU_VDDIO_SDIO);
	PMU_PowerOnVDDIO(PMU_VDDIO_D2);
	PMU_PowerOnVDDIO(PMU_VDDIO_FL);

	/* This function will enable all the module clock and cau clock */
	/* You can also call CLK_ModuleClkEnable(moduleNme) */
	PeriClk_On();
	/* Enable RC32K */
	CLK_RC32KEnable();

	/* Disable XTAL32K */
	CLK_Xtal32KDisable();

	/* IPs Enable */
	ULP_On();
	BRN_On();
	ACOMP_On();
	ADC_On();
	DAC_On();
	DMA_On();
	GPT_On();
	I2C_On();
	QSPI0_On();
	QSPI1_On();
	/* RTC is On by default */
	SDIO_On();
	SSP_On();
	USB_On();
	AES_On();
	CRC_On();
	FLASHHC_On();
	UART_On();
	WDT_On();
	GPIO_On();
}

void pm0_sys32m_rc32m_allperi_off(void)
{
	/* MC200_PM_CASE8 */

	/* Turn On RC32M */
	RC32M_On();

	/* System clock source to RC32M */
	CLK_SystemClkSrc(CLK_RC32M);

	while (CLK_GetClkStatus(CLK_RC32M) == RESET)
		;

	/* Turn off XTAL32M */
	CLK_MainXtalDisable();

	/* Turn off SFLL */
	CLK_SfllDisable();
		/* Turn off AUPLL */
	CLK_AupllDisable();

	/* configure VDDIO_D0 and VDDIO_SDIO to 1.8v */
	mc200_gpio_power_on();

	/* This function will disable all the module clock and cau clock */
	/* You can also call CLK_ModuleClkDisable(moduleNme) */
	PeriClk_Off();

	/* IPs Enable */
	ULP_On();
	BRN_On();

	ACOMP_Off();
	ADC_Off();
	DAC_Off();
	DMA_Off();
	GPT_Off();
	I2C_Off();
	QSPI0_Off();
	QSPI1_Off();
	RTC_Off();
	SDIO_Off();
	SSP_Off();
	USB_Off();
	AES_Off();
	CRC_Off();
	FLASHHC_Off();
	UART_Off();
	WDT_Off();
	GPIO_Off();

	/* PMU power domian */
	PMU_PowerOffVDDIO(PMU_VDDIO_D0);
	PMU_PowerOnVDDIO(PMU_VDDIO_AON);
	PMU_PowerOffVDDIO(PMU_VDDIO_D1);
	PMU_PowerOffVDDIO(PMU_VDDIO_SDIO);
	PMU_PowerOffVDDIO(PMU_VDDIO_D2);
	PMU_PowerOffVDDIO(PMU_VDDIO_FL);

}

void pm1_sys200m_xtal32m_allperi_on()
{
	/* MC200_PM_CASE9 */

	/* Turn on XTAL32M */
	CLK_MainXtalEnable(CLK_OSC_INTERN);

	/* check if MAINXTAL is RDY */
	while (CLK_GetClkStatus(CLK_MAINXTAL) == RESET)
		;

	/* Turn on SFLL to 200M */
	SFLL200M_On(CLK_MAINXTAL);

	/* check if MAINXTAL is RDY */
	while (CLK_GetClkStatus(CLK_SFLL) == RESET)
		;

	/* System clock source to SFLL 200M */
	CLK_SystemClkSrc(CLK_SFLL);

	/* Turn off RC32M when not used as a clock source */
	CLK_RC32MDisable();

	/* Turn on AUPLL */
	AUPLL_On(CLK_MAINXTAL);

	/* configure VDDIO_D0 and VDDIO_SDIO to 1.8v */
	mc200_gpio_power_on();

	/*x32k is disable in main */

	/* IPs Enable */
	ULP_On();
	BRN_On();
	ACOMP_On();
	ADC_On();
	DAC_On();
	DMA_On();
	GPT_On();
	I2C_On();
	QSPI0_On();
	QSPI1_On();
	/* RTC is on by default */
	SDIO_On();
	SSP_On();
	USB_On();
	AES_On();
	CRC_On();
	FLASHHC_On();
	UART_On();
	WDT_On();
	GPIO_On();

	/* PMU power domian */
	PMU_PowerOffVDDIO(PMU_VDDIO_D0);
	PMU_PowerOnVDDIO(PMU_VDDIO_AON);
	PMU_PowerOffVDDIO(PMU_VDDIO_D1);
	PMU_PowerOffVDDIO(PMU_VDDIO_SDIO);
	PMU_PowerOffVDDIO(PMU_VDDIO_D2);
	PMU_PowerOffVDDIO(PMU_VDDIO_FL);

	SysTick->CTRL = 0;
	SysTick->LOAD = 0;

	/* Idle mode sleep */
	PMU_SetSleepMode(PMU_PM1);
	os_disable_all_interrupts();
	__WFI();

}

void pm1_sys200m_xtal32m_allperi_off(void)
{
	/* MC200_PM_CASE10 */

	/* Turn on XTAL32M */
	CLK_MainXtalEnable(CLK_OSC_INTERN);

	/* check if MAINXTAL is RDY */
	while (CLK_GetClkStatus(CLK_MAINXTAL) == RESET)
		;

	/* Turn on SFLL to 200M */
	SFLL200M_On(CLK_MAINXTAL);

	/* check if MAINXTAL is RDY */
	while (CLK_GetClkStatus(CLK_SFLL) == RESET)
		;

	/* System clock source to SFLL 200M */
	CLK_SystemClkSrc(CLK_SFLL);

	/* Turn off RC32M when it is not used as a clock source */
	CLK_RC32MDisable();

	/* Turn off AUPLL */
	CLK_AupllDisable();

	/* configure VDDIO_D0 and VDDIO_SDIO to 1.8v */
	mc200_gpio_power_on();

	/* This function will disable all the module clock and cau clock */
	/* You can also call CLK_ModuleClkDisable(moduleNme) */
	PeriClk_Off();

	/* IPs Enable */
	ULP_On();
	BRN_On();
	ACOMP_Off();
	ADC_Off();
	DAC_Off();
	DMA_Off();
	GPT_Off();
	I2C_Off();
	QSPI0_Off();
	QSPI1_Off();
	RTC_Off();
	SDIO_Off();
	SSP_Off();
	USB_Off();
	AES_Off();
	CRC_Off();
	FLASHHC_Off();
	UART_Off();
	WDT_Off();
	GPIO_Off();

	/* PMU power domian */
	PMU_PowerOffVDDIO(PMU_VDDIO_D0);
	PMU_PowerOnVDDIO(PMU_VDDIO_AON);
	PMU_PowerOffVDDIO(PMU_VDDIO_D1);
	PMU_PowerOffVDDIO(PMU_VDDIO_SDIO);
	PMU_PowerOffVDDIO(PMU_VDDIO_D2);
	PMU_PowerOffVDDIO(PMU_VDDIO_FL);

	/* Disable SysTick */
	SysTick->CTRL = 0;
	SysTick->LOAD = 0;

	/* Idle mode sleep */
	PMU_SetSleepMode(PMU_PM1);
	os_disable_all_interrupts();
	__WFI();

}

void pm1_sys200m_rc32m_allperi_on(void)
{
	/* MC200_PM_CASE11 */

	/* Turn On RC32M */
	RC32M_On();

	while (CLK_GetClkStatus(CLK_RC32M) == RESET)
		;

	/* Turn on SFLL to 200M */
	SFLL200M_On(CLK_RC32M);

	/* check if MAINXTAL is RDY */
	while (CLK_GetClkStatus(CLK_SFLL) == RESET)
		;

	/* System clock source to SFLL 200M */
	CLK_SystemClkSrc(CLK_SFLL);


	/* Turn off XTAL32M when it is not used as a clock source */
	CLK_MainXtalDisable();

	/* Turn on AUPLL */
	AUPLL_On(CLK_MAINXTAL);

	/* configure VDDIO_D0 and VDDIO_SDIO to 1.8v */
	mc200_gpio_power_on();


	/* IPs Enable */
	ULP_On();
	BRN_On();
	ACOMP_On();
	ADC_On();
	DAC_On();
	DMA_On();
	GPT_On();
	I2C_On();
	QSPI0_On();
	QSPI1_On();
	/* RTC is on by default */
	SDIO_On();
	SSP_On();
	USB_On();
	AES_On();
	CRC_On();
	FLASHHC_On();
	UART_On();
	WDT_On();
	GPIO_On();

	/* Disable SysTick */
	SysTick->CTRL = 0;
	SysTick->LOAD = 0;

	/* Idle mode sleep */
	PMU_SetSleepMode(PMU_PM1);
	os_disable_all_interrupts();
	__WFI();
}

void pm1_sys200m_rc32m_allperi_off(void)
{
	/* MC200_PM_CASE12 */

	/* Turn On RC32M */
	RC32M_On();

	while (CLK_GetClkStatus(CLK_RC32M) == RESET)
		;

	/* Turn on SFLL to 200M */
	SFLL200M_On(CLK_RC32M);

	/* check if MAINXTAL is RDY */
	while (CLK_GetClkStatus(CLK_SFLL) == RESET)
		;

	/* System clock source to SFLL 200M */
	CLK_SystemClkSrc(CLK_SFLL);

	/* Turn off XTAL32M when it is not used as a clock source */
	CLK_MainXtalDisable();

	/* Turn off AUPLL */
	CLK_AupllDisable();

	/* configure VDDIO_D0 and VDDIO_SDIO to 1.8v */
	mc200_gpio_power_on();

	/* This function will disable all the module clock and cau clock */
	/* You can also call CLK_ModuleClkDisable(moduleNme) */
	PeriClk_Off();

	/* IPs Enable */
	ULP_On();
	BRN_On();
	ACOMP_Off();
	ADC_Off();
	DAC_Off();
	DMA_Off();
	GPT_Off();
	I2C_Off();
	QSPI0_Off();
	QSPI1_Off();
	RTC_Off();
	SDIO_Off();
	SSP_Off();
	USB_Off();
	AES_Off();
	CRC_Off();
	FLASHHC_Off();
	UART_Off();
	WDT_Off();
	GPIO_Off();

	/* PMU power domian */
	PMU_PowerOffVDDIO(PMU_VDDIO_D0);
	PMU_PowerOnVDDIO(PMU_VDDIO_AON);
	PMU_PowerOffVDDIO(PMU_VDDIO_D1);
	PMU_PowerOffVDDIO(PMU_VDDIO_SDIO);
	PMU_PowerOffVDDIO(PMU_VDDIO_D2);
	PMU_PowerOffVDDIO(PMU_VDDIO_FL);

	/* Disable SysTick */
	SysTick->CTRL = 0;
	SysTick->LOAD = 0;

	/* Idle mode sleep */
	PMU_SetSleepMode(PMU_PM1);
	os_disable_all_interrupts();
	__WFI();

}

void pm2_sys1m_rc32m_allperi_off(void)
{
	/* MC200_PM_CASE13 */

	/* Turn on RC32M */
	RC32M_On();

	/* check if RC32M clock is ready */
	while (CLK_GetClkStatus(CLK_RC32M) == RESET)
		;

	/* Take PMU clock down to 1Mhz */
	RC32M->CLK.BF.SEL_32M = 0;
	CLK_ModuleClkDivider(CLK_PMU, 0xf);

	/* Check if PLL clock is ready */
	while (CLK_GetClkStatus(CLK_SFLL) == RESET)
		;

	/* System clock source to RC32M */
	CLK_SystemClkSrc(CLK_RC32M);

	/* Turn off XTAL32M */
	CLK_MainXtalDisable();

	/* Turn off SFLL */
	CLK_SfllDisable();

	/* Turn off AUPLL */
	CLK_AupllDisable();

	/* configure VDDIO_D0 and VDDIO_SDIO to 1.8v */
	mc200_gpio_power_on();

	/* This function will disable all the module clock and cau clock */
	/* You can also call CLK_ModuleClkDisable(moduleNme) */
	PeriClk_Off();

	/* Enable XTAL32K */
	GPIO_PinMuxFun(GPIO_18, GPIO18_OSC32K_IN);
	GPIO_PinMuxFun(GPIO_19, GPIO19_OSC32K_OUT);

	/* IPs Enable */
	ULP_On();
	BRN_On();
	ACOMP_Off();
	ADC_Off();
	DAC_Off();
	DMA_Off();
	GPT_Off();
	I2C_Off();
	QSPI0_Off();
	QSPI1_Off();
	RTC_Off();
	SDIO_Off();
	SSP_Off();
	USB_Off();
	AES_Off();
	CRC_Off();
	FLASHHC_Off();
	UART_Off();
	WDT_Off();
	GPIO_Off();

	/* PMU power domian */
	PMU_PowerOffVDDIO(PMU_VDDIO_D0);
	PMU_PowerOffVDDIO(PMU_VDDIO_D1);
	PMU_PowerOffVDDIO(PMU_VDDIO_SDIO);
	PMU_PowerOffVDDIO(PMU_VDDIO_D2);

	PMU_PowerDeepOffVDDIO(PMU_VDDIO_FL);
	PMU_PowerDeepOffVDDIO(PMU_VDDIO_AON);

	SFLASH_HOLDnConfig(MODE_SHUTDOWN);
	SFLASH_DIOConfig(MODE_SHUTDOWN);
	SFLASH_WPConfig(MODE_SHUTDOWN);
	SFLASH_DOConfig(MODE_SHUTDOWN);

	GPIO_PinMuxFun(GPIO_18, PINMUX_FUNCTION_0);
	GPIO_PinMuxFun(GPIO_19, PINMUX_FUNCTION_0);
	GPIO_PinMuxFun(GPIO_20, PINMUX_FUNCTION_1);
	GPIO_PinMuxFun(GPIO_21, PINMUX_FUNCTION_1);
	GPIO_PinMuxFun(GPIO_22, PINMUX_FUNCTION_1);
	GPIO_PinMuxFun(GPIO_23, PINMUX_FUNCTION_1);
	GPIO_PinMuxFun(GPIO_24, PINMUX_FUNCTION_1);

	/* Disable SysTick */
	SysTick->CTRL = 0;
	SysTick->LOAD = 0;

	/* Standby mode */
	PMU_SetSleepMode(PMU_PM2);
	__WFI();
}

void pm3_sys1m_rc32m_allperi_off(void)
{
	/* MC200_PM_CASE14 */

	/* Turn on RC32M */
	RC32M_On();

	/* check if RC32M clock is ready */
	while (CLK_GetClkStatus(CLK_RC32M) == RESET)
		;

	/* Take PMU clock down to 1Mhz */
	RC32M->CLK.BF.SEL_32M = 0;
	CLK_ModuleClkDivider(CLK_PMU, 0xf);

	/* check if PLL clock is ready */
	while (CLK_GetClkStatus(CLK_SFLL) == RESET)
		;

	/* System clock source to RC32M */
	CLK_SystemClkSrc(CLK_RC32M);

	/* Turn off XTAL32M when it is not used as clock source */
	CLK_MainXtalDisable();

	/* Turn off SFLL */
	CLK_SfllDisable();

	/* Turn off AUPLL */
	CLK_AupllDisable();

	/* configure VDDIO_D0 and VDDIO_SDIO to 1.8v */
	mc200_gpio_power_on();

	/* This function will disable all the module clock and cau clock */
	/* You can also call CLK_ModuleClkDisable(moduleNme) */
	PeriClk_Off();

	/* IPs Enable */
	ULP_On();
	BRN_On();
	ACOMP_Off();
	ADC_Off();
	DAC_Off();
	DMA_Off();
	GPT_Off();
	I2C_Off();
	QSPI0_Off();
	QSPI1_Off();
	RTC_Off();
	SDIO_Off();
	SSP_Off();
	USB_Off();
	AES_Off();
	CRC_Off();
	FLASHHC_Off();
	UART_Off();
	WDT_Off();
	GPIO_Off();

	/* PMU power domian */
	PMU_PowerOffVDDIO(PMU_VDDIO_D0);
	PMU_PowerOnVDDIO(PMU_VDDIO_AON);
	PMU_PowerOffVDDIO(PMU_VDDIO_D1);
	PMU_PowerOffVDDIO(PMU_VDDIO_SDIO);
	PMU_PowerOffVDDIO(PMU_VDDIO_D2);
	PMU_PowerOffVDDIO(PMU_VDDIO_FL);
	PMU_PowerDeepOffVDDIO(PMU_VDDIO_AON);

	SysTick->CTRL = 0;
	SysTick->LOAD = 0;

	/* Sleep mode */
	PMU_SetSleepMode(PMU_PM3);
	__WFI();

}

void pm4_sys1m_rc32m_allperi_off(void)
{
	/* MC200_PM_CASE15 */

	/* Turn on RC32M */
	RC32M_On();

	/* check if RC32M clock is ready */
	while (CLK_GetClkStatus(CLK_RC32M) == RESET)
		;

	/* Take PMU clock down to 1Mhz */
	RC32M->CLK.BF.SEL_32M = 0;

	CLK_ModuleClkDivider(CLK_PMU, 0xf);

	/* Check if PLL clock is ready */
	while (CLK_GetClkStatus(CLK_SFLL) == RESET)
		;

	/* System clock source to RC32M */
	CLK_SystemClkSrc(CLK_RC32M);

	/* Turn off XTAL32M  when it is not used as a clock source*/
	CLK_MainXtalDisable();

	/* Turn off SFLL */
	CLK_SfllDisable();

	/* Turn off AUPLL */
	CLK_AupllDisable();

	/* configure VDDIO_D0 and VDDIO_SDIO to 1.8v */
	mc200_gpio_power_on();

	/* This function will disable all the module clock and cau clock */
	/* You can also call CLK_ModuleClkDisable(moduleNme) */
	PeriClk_Off();

	/* IPs Enable */
	ULP_On();
	BRN_On();
	ACOMP_Off();
	ADC_Off();
	DAC_Off();
	DMA_Off();
	GPT_Off();
	I2C_Off();
	QSPI0_Off();
	QSPI1_Off();
	RTC_Off();
	SDIO_Off();
	SSP_Off();
	USB_Off();
	AES_Off();
	CRC_Off();
	FLASHHC_Off();
	UART_Off();
	WDT_Off();
	GPIO_Off();

	/* PMU power domian */
	PMU_PowerOffVDDIO(PMU_VDDIO_D0);
	PMU_PowerOnVDDIO(PMU_VDDIO_AON);
	PMU_PowerOffVDDIO(PMU_VDDIO_D1);
	PMU_PowerOffVDDIO(PMU_VDDIO_SDIO);
	PMU_PowerOffVDDIO(PMU_VDDIO_D2);
	PMU_PowerOffVDDIO(PMU_VDDIO_FL);
	PMU_PowerDeepOffVDDIO(PMU_VDDIO_AON);

	SysTick->CTRL = 0;
	SysTick->LOAD = 0;

	/* Sleep mode */
	PMU_SetSleepMode(PMU_PM4);
	__WFI();
}

void RC32M_On(void)
{
	/* XTAL32M On */
	CLK_MainXtalEnable(CLK_OSC_INTERN);
	while (CLK_GetClkStatus(CLK_MAINXTAL) == RESET)
		;
	/* AUPLL On */
	CLK_AupllConfig_Type aupllConfigSet;
	aupllConfigSet.refDiv = 16;
	aupllConfigSet.fbDiv = 270;
	aupllConfigSet.icp = 4;
	aupllConfigSet.updateRate = 1;
	aupllConfigSet.usbPostDiv = 0;

	CLK_AupllEnable(&aupllConfigSet);
	delay(0x1000);

	CLK_AupllClkOutEnable(CLK_AUPLL_CAU);
	delay(0x1000);

	CLK_CAUClkSrc(CLK_MAINXTAL);

	/* RC32M On */
	CLK_RC32MEnable();
	CLK_RC32MCalibration(CLK_AUTO_CAL, 0);
	while (CLK_GetClkStatus(CLK_RC32M) == RESET)
		;

	CLK_AupllDisable();

	CLK_CAUClkSrc(CLK_RC32M);
}

void SFLL200M_On(CLK_Src_Type srcClock)
{
	CLK_SfllConfig_Type sfllConfigSet;
	/* Configure SFLL to 200MHz */
	sfllConfigSet.refClockSrc = srcClock;
	sfllConfigSet.refDiv = 0x50;
	sfllConfigSet.fbDiv = 0x1F3;
	sfllConfigSet.kvco = 7;
	sfllConfigSet.postDiv = 0;

	/* Enable SFLL */
	CLK_SfllEnable(&sfllConfigSet);

	/* Wait for SFLL ready */
	while (CLK_GetClkStatus(CLK_SFLL) == RESET)
		;

	CLK_ModuleClkDivider(CLK_APB0, 2);
	CLK_ModuleClkDivider(CLK_APB1, 2);
	CLK_ModuleClkDivider(CLK_PMU, 4);
	CLK_ModuleClkDivider(CLK_GPT0, 4);
	CLK_ModuleClkDivider(CLK_GPT1, 4);
	CLK_ModuleClkDivider(CLK_GPT2, 4);
	CLK_ModuleClkDivider(CLK_GPT3, 4);
	CLK_ModuleClkDivider(CLK_SSP0, 4);
	CLK_ModuleClkDivider(CLK_SSP1, 4);
	CLK_ModuleClkDivider(CLK_SSP2, 4);
	CLK_ModuleClkDivider(CLK_I2C0, 3);
	CLK_ModuleClkDivider(CLK_I2C1, 3);
	CLK_ModuleClkDivider(CLK_I2C2, 3);
	CLK_ModuleClkDivider(CLK_QSPI0, 4);
	CLK_ModuleClkDivider(CLK_QSPI1, 4);
	CLK_ModuleClkDivider(CLK_FLASHC, 2);
}

void AUPLL_On(CLK_Src_Type srcClock)
{
	/* AUPLL On */
	CLK_AupllConfig_Type aupllConfigSet;
	aupllConfigSet.refDiv = 16;
	aupllConfigSet.fbDiv = 270;
	aupllConfigSet.icp = 4;
	aupllConfigSet.updateRate = 1;
	aupllConfigSet.usbPostDiv = 0;

	CLK_AupllEnable(&aupllConfigSet);
	delay(0x1000);

	CLK_AupllClkOutEnable(CLK_AUPLL_CAU);
	delay(0x1000);
	CLK_CAUClkSrc(srcClock);
}

void mc200_gpio_power_on(void)
{
	PMU_ConfigVDDIOLevel(PMU_VDDIO_D0, PMU_VDDIO_LEVEL_1P8V);
	PMU_ConfigVDDIOLevel(PMU_VDDIO_SDIO, PMU_VDDIO_LEVEL_1P8V);
}

static void cmd_power_mode(int argc, char *argv[])
{
	int opt = 0;
	if (argc == 2) {
		opt = atoi(argv[1]);
	} else {
		wmprintf(" Please enter valid option 1-10 \r\n");
		return;
	}
	pm_mc200_case(opt);
}

static struct cli_command commands[] = {
	{"power-mode-index", "<index > ", cmd_power_mode},
};

static int power_mode_cli_init()
{
	int i;
	for (i = 0; i < sizeof(commands) / sizeof(struct cli_command); i++)
		if (cli_register_command(&commands[i]))
			return 1;
	return 0;
}

int test_init()
{
	int ret = 0;
	wmstdio_init(UART0_ID, 0);
	cli_init();
	pm_init();
	RTC_On();
	wmtime_init();
	pm_mc200_cli_init();
	/* Flash driver for internal flash
	 * registers a power sve callback
	 * which configures Serial flash pins to disable them
	 * on PM2 entry.
	 */
	flash_drv_init();
	/* Turn off external flash */
	xflash_off();
	power_mode_cli_init();
	return ret;
}

/**
 * All application specific initialization is performed here
 */
int main(void)
{
	test_init();
	wmprintf(" Current measurement Application Started\r\n");
	wmprintf("Use CLI power-mode-index <index> to take readings\r\n");
	return 0;
}
