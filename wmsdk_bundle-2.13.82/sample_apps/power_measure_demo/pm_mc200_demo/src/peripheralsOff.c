/*
 *  Copyright (C) 2012, Marvell International Ltd.
 *  All Rights Reserved.
 */

#include "mc200.h"
#include "mc200_driver.h"
#include "peripheralsOff.h"

/* Following functions for on or off periphral clocks */
void PeriClk_Off(void)
{
	/* All peripherals clock off */
	PMU->PERI_CLK_EN.WORDVAL = 0xFFFFFFFF;
	/* CAU clock off */
	PMU->CAU_CTRL.WORDVAL = 0x00000000;
}
/* Following functions for on or off periphrals */
void ULP_Off(void)
{
	/* ULPCOMP disable */
	PMU->PMIP_CMP_CTRL.BF.COMP_EN = 0;
}

void BRN_Off(void)
{
	/* Brownout detect AV18/VFL/V12/VBAT disable */
	PMU->PMIP_BRNDET_VBAT.BF.BRNDET_VBAT_EN = 0;
	PMU->PMIP_BRNDET_V12.BF.BRNDET_V12_EN = 0;
	PMU->PMIP_BRNDET_VFL.BF.BRNDET_VFL_EN = 0;
	PMU->PMIP_BRNDET_AV18.BF.BRNDET_AV18_EN = 0;
}

void ACOMP_Off(void)
{
	/* ACOMP0 and ACOMP1 disable */
	ACOMP_Disable(ACOMP_ACOMP0);
	ACOMP_Disable(ACOMP_ACOMP1);
}

void ADC_Off(void)
{
	/* ADC0 and ADC1 disable */
	ADC_Disable(ADC0_ID);
	ADC_Disable(ADC1_ID);
}

void DAC_Off(void)
{
	/* DAC Chaneel A and Chaneel B disable */
	DAC_ChannelDisable(DAC_CH_A);
	DAC_ChannelDisable(DAC_CH_B);
}

void DMA_Off(void)
{
	/* DMA disable */
	DMA_Disable();
}

void GPT_Off(void)
{
	/* GPT0, GPT1, GPT2 and GPT3 disable */
	GPT_Stop(GPT0_ID);
	GPT_Stop(GPT1_ID);
	GPT_Stop(GPT2_ID);
	GPT_Stop(GPT3_ID);
}

void I2C_Off(void)
{
	/* I2C0, I2C1 and I2C2 disable */
	I2C_Disable(I2C0_PORT);
	I2C_Disable(I2C1_PORT);
	I2C_Disable(I2C2_PORT);
}

void QSPI0_Off(void)
{
	/* Stop QSPI0 */
	QSPI0->CONF.BF.XFER_STOP = 1;
	/* De-assert QSPI0 SS */
	QSPI0->CNTL.BF.SS_EN = DISABLE;
}

void QSPI1_Off(void)
{
	/* Stop QSPI1 */
	QSPI1->CONF.BF.XFER_STOP = 1;
	/* De-assert QSPI1 SS */
	QSPI1->CNTL.BF.SS_EN = DISABLE;
}

void RTC_Off(void)
{
	CLK_RTCClkSrc(CLK_RC32K);
	/* Stop RTC */
	RTC_Stop();
	/* Disable RTC Clk */
	CLK_ModuleClkDisable(CLK_RTC);

	/* Disable RTC Clock Source */
	CLK_RC32KDisable();


}

void SDIO_Off(void)
{
	/* Power off sdio bus power */
	SDIOC_PowerOff();
}

void SSP_Off(void)
{
	/* SSP0, SSP1 and SSP2 disable */
	SSP_Disable(SSP0_ID);
	SSP_Disable(SSP1_ID);
	SSP_Disable(SSP2_ID);
}

void USB_Off(void)
{
	/* no releated operation */
}

void AES_Off(void)
{
	/* AES disable */
	AES_Disable();
}

void CRC_Off(void)
{
	/* CRC disable */
	CRC_Disable();
}

void FLASHHC_Off(void)
{
	/* FLASHC disable */
	FLASHC->FCCR.BF.FLASHC_PAD_EN = 0;
}

void GPIO_Off(void)
{
	/* no releated operation */
}

void UART_Off(void)
{
	/* no releated operation */
}

void WDT_Off(void)
{
	/* This function no use for if WDT start then can't disable */
	WDT_Disable();
}

void AllPeri_Off(void)
{
	ULP_Off();
	BRN_Off();
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
	GPIO_Off();
	UART_Off();
	WDT_Off();
}
