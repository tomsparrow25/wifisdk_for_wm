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

/* this functions for off periphral clocks */
void PeriClk_Off(void);  /* RTC is on */

/* this functions for off periphral */
void AllPeri_Off(void);

void ULP_Off(void);
void BRN_Off(void);
void ACOMP_Off(void);
void ADC_Off(void);
void DAC_Off(void);
void DMA_Off(void);
void GPT_Off(void);
void I2C_Off(void);
void QSPI0_Off(void);
void QSPI1_Off(void);
void RTC_Off(void);
void SDIO_Off(void);
void SSP_Off(void);
void USB_Off(void);
void AES_Off(void);
void CRC_Off(void);
void FLASHHC_Off(void);
void GPIO_Off(void);
void UART_Off(void);
void WDT_Off(void);
