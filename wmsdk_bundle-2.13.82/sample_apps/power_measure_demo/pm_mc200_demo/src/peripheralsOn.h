#include "mc200.h"
#include "mc200_driver.h"
#include "mc200_clock.h"
#include "mc200_gpio.h"
#include "mc200_pinmux.h"
#include "mc200_pmu.h"
#include "mc200_acomp.h"
#include "mc200_adc.h"
#include "mc200_dac.h"
#include "mc200_dac.h"
#include "mc200_qspi0.h"
#include "mc200_qspi1.h"
#include "mc200_rtc.h"
#include "mc200_gpio.h"
#include "mc200_uart.h"
#include "mc200_i2c.h"
#include "mc200_gpt.h"
#include "mc200_wdt.h"
#include "mc200_sdio.h"
#include "mc200_ssp.h"
#include "mc200_dma.h"
#include "mc200_aes.h"
#include "mc200_crc.h"


/* this functions for on periphral clocks */
void PeriClk_On(void);   /* RTC is on */

/* this functions for on periphral */
void ULP_On(void);
void BRN_On(void);
void ACOMP_On(void);
void ADC_On(void);
void DAC_On(void);
void DMA_On(void);
void GPT_On(void);
void I2C_On(void);
void QSPI0_On(void);
void QSPI1_On(void);
void RTC_On(void);
void SDIO_On(void);
void SSP_On(void);
void USB_On(void);
void AES_On(void);
void CRC_On(void);
void FLASHHC_On(void);
void UART_On(void);
void WDT_On(void);
void GPIO_On(void);
