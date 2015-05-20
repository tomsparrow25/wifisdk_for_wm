/*
 *  Copyright (C) 2008-2012, Marvell International Ltd.
 *  All Rights Reserved.
 */

#define ADC_IP_GPIO 7
#include <wmstdio.h>
#include <wm_os.h>
#include <mdev_gpio.h>
#include <mdev_adc.h>
#include <mdev_pinmux.h>
#include <mc200_dma.h>


/*
 * Simple Application which uses ADC driver.
 *
 * Summary:
 * This application uses ADC (analog to digital converter) to sample
 * an analog signal applied at GPIO_7. This application by default samples
 * 500 samples and prints the output of ADC on console.
 *
 * Description:
 *
 * This application is written using APIs exposed by MDEV
 * layer of ADC driver.
 *
 * The application configures GPIO_7 as Analog input to ADC0 periferral.
 * ADC configurations used:
 * sampling frequency : 3.9KHz
 * Resolution: 16 bits
 * ADC gain: 2
 * Reference source for ADC: Internal VREF (1.2V)
 * Note: Maximum analog input voltage allowed with Internal VREF is 1.2V.
 * User may change the default ADC configuration as per the requirements.
 * When ADC gain is selected to 2; input voltage range reduces to half (0.6V)
 */

/*------------------Macro Definitions ------------------*/
#define SAMPLES 500
#define ADC_GAIN 2
#define BIT_RESOLUTION_FACTOR 32767		/* For 16 bit resolution (2^15-1) */
#define VMAX_IN_mV	1200	/* Max input voltage in milliVolts */

/*------------------Global Variable Definitions ---------*/

uint16_t buffer[SAMPLES];
mdev_t *adc_dev;

/* This is an entry point for the application.
   All application specific initialization is performed here. */
int main(void)
{
	int samples = SAMPLES;
	wmstdio_init(UART0_ID, 0);

	wmprintf("\r\n-----------APP start----------------------\r\n");

	/* adc initialization */
	if (adc_drv_init(ADC0_ID) != WM_SUCCESS) {
		wmprintf("Error: cannot init adc\n\r");
		return -1;
	}

	ADC_CFG_Type config;

	/* get default ADC gain value */
	adc_get_config(&config);
	wmprintf("Default adc gain value = %d\r\n", config.adcGainSel);

	/* Modify ADC gain to 2 */
	adc_modify_default_config(adcGainSel, 2);
	adc_get_config(&config);
	wmprintf("Modified ADC gain value = %d\r\n", config.adcGainSel);

	adc_dev = adc_drv_open(ADC0_ID, ADC_CH0);

	adc_drv_get_samples(adc_dev, buffer, samples);

	int j;
	for(j = 0; j < samples; j++) {

		buffer[j] = (VMAX_IN_mV * buffer[j]) / BIT_RESOLUTION_FACTOR;
		wmprintf("result %d ---- %d mV\r\n", j, buffer[j]);
	}

	wmprintf("------THE_END--------");

	adc_drv_close(adc_dev);

	return 0;

}
