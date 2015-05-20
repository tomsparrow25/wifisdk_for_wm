/*
 *  Copyright (C) 2012, Marvell International Ltd.
 *  All Rights Reserved.
 */

#include "mc200.h"
#include "mc200_driver.h"
#include "peripheralsOn.h"

void USB_SystemInit(void);
void USB_HostInit(uint32_t intTldVal, uint32_t count);

/* Following functions for on periphral clocks */
void PeriClk_On(void)
{
	/* All peripherals clock on */
	PMU->PERI_CLK_EN.WORDVAL = 0x0;

	/* CAU clock on */
	PMU->CAU_CTRL.WORDVAL = 0x0000000F;
}

/* Following functions for on periphrals */
void ULP_On()
{
	/* ULPCOMP enable */
	PMU->PMIP_CMP_CTRL.BF.COMP_EN = 1;
}

void BRN_On()
{
	/* Brownout detect AV18/VFL/V12/VBAT enable */
	PMU->PMIP_BRNDET_VBAT.BF.BRNDET_VBAT_EN = 1;
	PMU->PMIP_BRNDET_V12.BF.BRNDET_V12_EN = 1;
	PMU->PMIP_BRNDET_VFL.BF.BRNDET_VFL_EN = 1;
	PMU->PMIP_BRNDET_AV18.BF.BRNDET_AV18_EN = 1;
}

void ACOMP_On(void)
{
	ACOMP_CFG_Type acompConfigSet = { ACOMP_HYSTER_0MV,
		ACOMP_HYSTER_0MV,
		LOGIC_LO,
		ACOMP_PWMODE_1,
		ACOMP_WARMTIME_16CLK,
		ACOMP_PIN_OUT_DISABLE,
	};

	ACOMP_Init(ACOMP_ACOMP0, &acompConfigSet);
	ACOMP_ChannelConfig(ACOMP_ACOMP0,
			    ACOMP_POS_CH_DACA, ACOMP_NEG_CH_DACA);
	ACOMP_Enable(ACOMP_ACOMP0);

	ACOMP_Init(ACOMP_ACOMP1, &acompConfigSet);
	ACOMP_ChannelConfig(ACOMP_ACOMP1,
			    ACOMP_POS_CH_DACA, ACOMP_NEG_CH_DACA);
	ACOMP_Enable(ACOMP_ACOMP1);
}

void ADC_On(void)
{
	int i = 0;
	for (i = 0; i <= 1; i++) {
		ADC_Reset((ADC_ID_Type) i);
		ADC_ModeSelect((ADC_ID_Type) i, ADC_MODE_ADC);
		ADC_ChannelConfig((ADC_ID_Type) i, ADC_CH0);
		ADC_Enable((ADC_ID_Type) i);
		ADC_ConversionStop((ADC_ID_Type) i);
	}
}

void DAC_On(void)
{
	DAC_ChannelConfig_Type dacCfg = { DAC_WAVE_NORMAL,
		DAC_OUTPUT_INTERNAL,
		DAC_NON_TIMING_CORRELATED,
	};

	DAC_ChannelConfig(DAC_CH_A, &dacCfg);
	DAC_ChannelEnable(DAC_CH_A);

	DAC_ChannelConfig(DAC_CH_B, &dacCfg);
	DAC_ChannelEnable(DAC_CH_B);
}

void DMA_On(void)
{
	uint8_t srcBit8Buf[0x10];
	uint8_t destBit8Buf[0x10];
	int i = 0;
	/*!< Source address of DMA transfer */
	DMA_CFG_Type DmaCfg = { (uint32_t) srcBit8Buf,
				/*!< Destination address of DMA transfer */
				(uint32_t) destBit8Buf,
				DMA_MEM_TO_MEM,
				/*!< 1 item for source burst transaction
				  length.
				  Each item width is as same as source
				  tansfer width. */
				DMA_ITEM_1,
				/*!< 1 item for destination burst transaction
				  length.
				  Each item width is as same as destination
				  tansfer width. */
				DMA_ITEM_1,
				/*!< Source address increment */
				DMA_ADDR_INC,
				/*!< Destination address increment */
				DMA_ADDR_INC,
				/*!< 8 bits source transfer width */
				DMA_TRANSF_WIDTH_8,
				/*!< 8 bits destination transfer width */
				DMA_TRANSF_WIDTH_8,
				/*!< Source hardware handshaking select */
				DMA_HW_HANDSHAKING,
				/*!< Destination hardware handshaking select */
				DMA_HW_HANDSHAKING,
				/*!< Channel is set the highest priority */
				DMA_CH_PRIORITY_7,
				/*!< Default */
				DMA_HW_HS_INTER_0,
				/*!< Default */
				DMA_HW_HS_INTER_0,
				/*!< Space/data available for single AHB
				  transfer of the specified transfer width */
				DMA_FIFO_MODE_0,
	};
	CLK_ModuleClkEnable(CLK_DMAC);
	for (i = 0; i <= 7; i++) {
		DMA_ChannelInit((DMA_Channel_Type) i, &DmaCfg);
		DMA_ChannelCmd((DMA_Channel_Type) i, DISABLE);
	}
	DMA_Enable();
}

void GPT_On(void)
{
	int i = 0;
	GPT_Config_Type gptDefConfig = {
		GPT_CLOCK_0,	/* Clock 0 */
		/* counter value register update mode: auto-update normal */
		GPT_CNT_VAL_UPDATE_NORMAL,
		/* divider value: 0 */
		0,
		/* prescaler value: 0 */
		0,
		/* counter overflow value: 0xffffffff */
		0xffffffff
	};

	for (i = 0; i <= 3; i++) {
		GPT_Init((GPT_ID_Type) i, &gptDefConfig);
		GPT_ChannelFuncSelect((GPT_ID_Type) i, GPT_CH_1,
				      GPT_CH_FUNC_NO);
		GPT_Start((GPT_ID_Type) i);
	}
}

void I2C_On(void)
{
	int i = 0;
	I2C_CFG_Type i2cCfg = { I2C_MASTER,	/* master */
		I2C_SPEED_HIGH,	/* high speed mode */
		I2C_ADDR_BITS_7,	/* master address bit mode */
		I2C_ADDR_BITS_7,	/* slave address bit mode */
		0x55,		/* own slave address */
		ENABLE,		/* restart function */
	};

	for (i = 0; i < 3; i++) {
		i2cCfg.ownSlaveAddr++;
		I2C_Init((I2C_ID_Type) i, (I2C_CFG_Type *) &i2cCfg);
		I2C_SetSlaveAddr((I2C_ID_Type) i, i2cCfg.ownSlaveAddr);
		I2C_Enable((I2C_ID_Type) i);
	}
}

void QSPI0_On(void)
{
	QSPI_CFG_Type qspiCfg = { QSPI_DATA_PIN_SINGLE,
		QSPI_ADDR_PIN_SINGLE,
		QSPI_POL0_PHA0,
		QSPI_CAPT_EDGE_FIRST,
		QSPI_BYTE_LEN_1BYTE,
		/* Serial interface clock prescaler is SPI clock / 20 */
		QSPI_CLK_DIVIDE_20,
	};

	QSPI0_Reset();
	QSPI0_Init(&qspiCfg);
	QSPI0_StartTransfer(QSPI_R_EN);
}

void QSPI1_On(void)
{
	QSPI_CFG_Type qspiCfg = { QSPI_DATA_PIN_SINGLE,
		QSPI_ADDR_PIN_SINGLE,
		QSPI_POL0_PHA0,
		QSPI_CAPT_EDGE_FIRST,
		QSPI_BYTE_LEN_1BYTE,
		/* Serial interface clock
		   prescaler is SPI clock / 20 */
		QSPI_CLK_DIVIDE_20,
	};
	QSPI1_Reset();
	QSPI1_Init(&qspiCfg);
	QSPI1_StartTransfer(QSPI_R_EN);
}

void RTC_On(void)
{
	/* ClkSrc for RTC : CLK_RC32K, hence Disable Xtal32K */
	CLK_Xtal32KDisable();
	CLK_RC32KEnable();
	/* check if RC32M clock is ready */
	while (CLK_GetClkStatus(CLK_RC32M) == RESET)
		;
	CLK_RTCClkSrc(CLK_RC32K);
	CLK_ModuleClkEnable(CLK_RTC);
	RTC_Start();
}

void SDIO_On(void)
{
	SDIOC_CFG_Type sdioCfg = { ENABLE,
		SDIOC_BUS_WIDTH_4,
		SDIOC_SPEED_NORMAL,
		SDIOC_VOLTAGE_3P3,
		ENABLE,
		DISABLE
	};

	SDIOC_Init(&sdioCfg);
	SDIOC_PowerOn();
}

void SSP_On(void)
{
	int i = 0;
	SSP_CFG_Type sspCfg = { SSP_NORMAL,
		SSP_FRAME_SSP,
		SSP_SLAVE,
		SSP_TR_MODE,
		SSP_DATASIZE_8,
		SSP_SAMEFRM_PSP,
		SSP_TRAILBYTE_CORE,
		SSP_SLAVECLK_TRANSFER,
		SSP_TXD3STATE_12SLSB,
		ENABLE,
		0x3
	};

	for (i = 0; i < 3; i++) {
		CLK_SSPClkSrc((CLK_SspID_Type) i, CLK_SYSTEM);
		CLK_ModuleClkDivider(CLK_SSP0, 4);
		SSP_Init((SSP_ID_Type) i, &sspCfg);
		SSP_Enable((SSP_ID_Type) i);
	}
}

void USB_On(void)
{
	USB_SystemInit();
	USB_HostInit(0, 3);
}

void AES_On(void)
{
	AES_Enable();
}

void CRC_On(void)
{
	CRC_Enable();
}

void FLASHHC_On(void)
{
	/* Enable FLASHC */
	FLASHC->FCCR.BF.FLASHC_PAD_EN = 1;
}

void UART_On(void)
{
	int i = 0;
	UART_CFG_Type UartCfg = { 9600,
		UART_DATABITS_8,
		UART_PARITY_NONE,
		UART_STOPBITS_1,
		DISABLE
	};

	for (i = 0; i <= 3; i++) {
		CLK_SetUARTClkSrc((CLK_UartID_Type) i, CLK_UART_SLOW);
		UART_Init((UART_ID_Type) i, &UartCfg);
	}
}

void WDT_On(void)
{
	WDT_Config_Type wdtCfg;
	wdtCfg.timeoutVal = 0x0F;
	wdtCfg.mode = WDT_MODE_RESET;
	wdtCfg.resetPulseLen = WDT_RESET_PULSE_LEN_2;
	WDT_Init(&wdtCfg);
	WDT_Disable();
}

void GPIO_On(void)
{
	/* no releated operation */
}
/*
 * @brief      Initialize the USB device
 * @param[in]  none
 * @return none
 */
void USB_SystemInit(void)
{
	/* set usb as 60M clock */
	CLK_AupllConfig_Type usbClkCfg = { 0x10,
		0x10E,
		0x04,
		0x01,
		0x00
	};
	/* Set pinmux as usb function */
	GPIO_PinMuxFun(GPIO_57, GPIO57_USB_DP);
	GPIO_PinMuxFun(GPIO_58, GPIO58_USB_DM);

	/* Power on the USB phy */
	SYS_CTRL->USB_PHY_CTRL.BF.REG_PU_USB = 1;

	/* Set the USB clock */
	CLK_AupllEnable(&usbClkCfg);
	/* Enable the USB clock */
	CLK_ModuleClkEnable(CLK_USBC);
	CLK_AupllClkOutEnable(CLK_AUPLL_USB);
	GPIO_PinMuxFun(GPIO_45, GPIO45_USB2_DRVVBUS);
}
/*
 * @brief      Initialize the USB host
  * @param[in]  intTldVal: interrupt threshold control
 * @param[in]  count: Asynchronous Schedule Park Mode Count
 * @return none
 */

void USB_HostInit(uint32_t intTldVal, uint32_t count)
{
	/* set the host mode */
	USBC->USBMODE.BF.CM = 0x03;
	/* set ITC */
	USBC->USBCMD.BF.ITC = intTldVal;
	/* set ASP */
	USBC->USBCMD.BF.ASP0 = count & 0x01;
	USBC->USBCMD.BF.ASP0 = count & 0x02;

	/* set AHBBRST */
	USBC->SBUSCFG.BF.AHBBRST = 0x01;

	/* set burst size */
	USBC->BURSTSIZE.WORDVAL = 0x1818;

	/* set TXFILLTUNING */
	USBC->TXFILLTUNING.WORDVAL = 0x10;

	/* Enable port change detect interrupt */
	USBC->USBINTR.BF.PCE = 0x01;

	/* Enable usb ID interrupt */
	USBC->OTGSC.BF.IDIE = 0x01;

	/* Enable USB interrupt */
	NVIC_EnableIRQ(USB_IRQn);

	/* set host controller run */
	USBC->USBCMD.WORDVAL |= 0x01;
}
