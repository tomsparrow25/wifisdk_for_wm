/*! \file mdev_dac.h
 *  \brief Digital Analog Converter Driver
 *
 * DAC driver provides mdev interface to use DAC in mc200.
 * Follow the steps below to use DAC driver from
 * applications
 *
 * 1. Include Files
 *
 * Include mdev.h and mdev_dac.h in your source file.
 *
 * 2. DAC driver initialization
 *
 * Call \ref dac_drv_init from your application initialization function.
 * This will initialize and register dac driver with the mdev interface.
 *
 * 3. Open DAC device to use
 *
 * \ref dac_drv_open should be called once before using DAC hardware.
 * Here you will specify DAC channel (A or B). The other parameters
 * that are set by default are -<br>
 * DAC_WAVE_NORMAL - No predefined waveform<br>
 * DAC_RANGE_LARGE - Output Range Large<br>
 * DAC_OUTPUT_PAD - Output is enable to pad<br>
 * DAC_NON_TIMING_CORRELATED - Non timing corelated mode<br>
 *
 * If these defaults need to be changed they should be changed in
 * \ref dac_drv_open
 *
 * \code
 * {
 *	mdev_t *dac_dev;
 *	dac_dev = dac_drv_open(MDEV_DAC, DAC_CH_A);
 * }
 * \endcode
 *
 * 4. Set output of DAC
 *
 * Call \ref dac_drv_output to get the output of the mentioned
 * dac. Max resolution is 10 bit.
 *
 * 5. Closing a device.
 *
 * A call to \ref dac_drv_close informs the DAC driver to release the
 * resources that it is holding.
 *
 *
 *  Copyright 2008-2013, Marvell International Ltd.
 *  All Rights Reserved.
 */

#ifndef _MDEV_DAC_H_
#define _MDEV_DAC_H_

#include <mdev.h>
#include <mc200_dac.h>

#define DAC_LOG(...)  wmprintf("[dac] " __VA_ARGS__)

#define MDEV_DAC "MDEV_DAC"

/** Register DAC Device
 *
 * This function registers dac driver with mdev interface.
 *
 * \return WM_SUCCESS on success, error code otherwise.
 */
int dac_drv_init();

/** Open DAC Device
 *
 * This function opens the device for use and returns a handle. This handle
 * should be used for other calls to the DAC driver.
 *
 * \param[in] name Name of the driver(MDEV_DAC) to be opened.
 * \param[in] channel Channel to be used.
 * \return NULL if error, mdev_t handle otherwise.
 */
mdev_t *dac_drv_open(const char *name, DAC_ChannelID_Type channel);

/** Set DAC Output
 *
 * This function sets the output of dac device.
 *
 * \param[in] mdev_p mdev_t handle to the driver.
 * \param[in] val Max 10 bit value to be set.
 */
void dac_drv_output(mdev_t *mdev_p, int val);

/** Close DAC Device
 *
 * This function closes the device after use and frees up resources.
 *
 * \param[in] dev mdev handle to the driver to be closed.
 * \return WM_SUCCESS on success, -WM_FAIL on error.
 */
int dac_drv_close(mdev_t *dev);

#endif /* !_MDEV_DAC_H_ */
