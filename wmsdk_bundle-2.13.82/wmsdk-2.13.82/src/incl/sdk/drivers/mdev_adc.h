/*! \file mdev_adc.h
 *  \brief Analog to Digital Converter Driver
 *
 * ADC driver provides mdev interface to use 2 ADCs
 * in mc200. Follow the steps below to use ADC driver from
 * applications
 *
 * 1. Include Files
 *
 * Include mdev.h and mdev_adc.h in your source file.
 *
 * 2. ADC driver initialization
 *
 * Call \ref adc_drv_init from your application initialization function.
 * This will initialize and register adc driver with the mdev interface.
 *
 * 3. Open ADC device to use
 *
 * \ref adc_drv_open should be called once before using ADC hardware.
 * Here you will specify ADC number (from 0 to 1). The other parameters
 * that are set by default are -<br>
 * ADC_RESOLUTION_16BIT - Resolution 16 bit<br>
 * ADC_VREF_INTERNAL - Internal 1.2V reference<br>
 * ADC_GAIN_1 - PGA gain set to 1<br>
 * ADC_CLOCK_DIVIDER_32 - ADC clock divided by 32<br>
 * ADC_BIAS_FULL - Full biasing mode<br>
 *
 * If these defaults need to be changed they should be changed in
 * \ref adc_drv_open
 *
 * \code
 * {
 *	mdev_t *adc_dev;
 *	adc_dev = adc_drv_open(ADC<0-1>_ID);
 * }
 * \endcode
 *
 * 4. Get result/output of ADC
 *
 * Call \ref adc_drv_result to get the output of the mentioned
 * adc.
 *
 * 5. Closing a device.
 *
 * A call to \ref adc_drv_close informs the ADC driver to release the
 * resources that it is holding.
 *
 *
 *  Copyright 2008-2013, Marvell International Ltd.
 *  All Rights Reserved.
 */

#ifndef _MDEV_ADC_H_
#define _MDEV_ADC_H_

#include <mdev.h>
#include <mc200_adc.h>

#define ADC_LOG(...)  wmprintf("[adc] " __VA_ARGS__)

typedef enum {
	adcResolution,
	adcVrefSource,
	adcGainSel,
	adcClockDivider,
	adcBiasMode
} adc_cfg_param;

/** Register ADC Device
 *
 * This function registers adc driver with mdev interface.
 *
 * \param[in] adc_id The ADC to be initialized.
 * \return WM_SUCCESS on succes, error code otherwise.
 */
int adc_drv_init(ADC_ID_Type adc_id);

/** Get adc driver configuration
  *
  * This function returns a structure with ADC parameters as its
  * members. If the default parameters are changed by the user, this
  * function returns the structure with current parameters.
  *\param[in] config Pointer to ADC config stucture
  */
void adc_get_config(ADC_CFG_Type *config);

/** Modify default ADC configuration
  *
  * This function allows the user to over-ride the default parameter/s
  * Call to this function is not mandatory and should be made if the user wants
  * to change any of the defaults for ADC.
  * It can change a single parameter in each call.
  * User may choose to modify one or more default parameters.
  * All the un-modified parameter are set to the default value.
  * Default values are as below:
  * Config-Type   |  Default value
  *	adcResolution = ADC_RESOLUTION_16BIT,
  *	adcVrefSource = ADC_VREF_INTERNAL,
  * adcGainSel = ADC_GAIN_1,
  * adcClockDivider =	ADC_CLOCK_DIVIDER_32,
  * adcBiasMode =	ADC_BIAS_FULL
  *
  *\param[in] config_type ADC Config-Type
  *\param[in] value Value for the chosen Config-Type
  */
void adc_modify_default_config(adc_cfg_param config_type, int value);

/** Open ADC Device
 *
 * This function opens the device for use and returns a handle. This handle
 * should be used for other calls to the ADC driver.
 *
 * \param[in] adc_id adc port of the driver to be opened.
 * \param[in] channel Channel to be used.
 * \return NULL if error, mdev_t handle otherwise.
 */
mdev_t *adc_drv_open(ADC_ID_Type adc_id, ADC_Channel_Type channel);

/** Get ADC Output
 *
 * This function gets the output of adc device.
 *
 * \param[in] mdev_p  mdev_t handle to the driver.
 * \return The output value (max 16 bit)
 */
int adc_drv_result(mdev_t *mdev_p);

/** Calibrate ADC
  *
  * This function removes linear gain and offset errors and starts ADC
  * sampling.
  *
  * \param[in] dev mdev handle to the driver
  * \param[in] sysOffsetCalVal offset value for zero correction
  * \return WM_SUCCESS on success , -WM_FAIL on error in calibration
  */
int adc_drv_calib(mdev_t *dev, int16_t sysOffsetCalVal);

/** Set timeout for ADC driver
  * This function allows user to set timeout for ADC conversion.
  * This time is time required for ADC conversion for a single
  * chunk (i.e. maximum 1023 datapoints) and dma transfer time.
  * If user does not set the timeout, default value is used. By default
  * this function waits till the DMA_TRANS_COMPLETE interrupt occurs.
  * Note: To set timeout this function needs to be called before adc_drv_init()
  * call.
  *
  * @param[in] id ADC ID.
  * @param[in] tout The timeout value.
  **/
void adc_drv_timeout(ADC_ID_Type id, uint32_t tout);

/** Get ADC samples for user supplied analog input
  *
  * This function provides n samples for user supplied analog input
  * (n specified by the user).It internally uses DMA channel 0 to transfer data
  * to the user buffer.The DMA transfer takes place in chunks of data-points.
  * Maximum chunk size is 1023 datapoints. Each ADC sample by default uses
  * 16 bit resolution.
  * \param[in] mdev_p mdev handle to the driver
  * \param[in,out] buf pointer to user allocated buffer to store sample points
  * \param[in] num number of samples required
  *
  * @return -WM_FAIL if operation failed.
  * @return WM_SUCCESS if operation was a success.
  */

int adc_drv_get_samples(mdev_t *mdev_p, uint16_t *buf, int num);

/** Close ADC Device
 *
 * This function closes the device after use and frees up resources.
 *
 * \param[in] dev mdev handle to the driver to be closed.
 * \return WM_SUCCESS on success, -WM_FAIL on error.
 */
int adc_drv_close(mdev_t *dev);

#endif /* !_MDEV_ADC_H_ */
