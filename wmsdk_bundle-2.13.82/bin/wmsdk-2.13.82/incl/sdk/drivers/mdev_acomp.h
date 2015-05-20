/*! \file mdev_acomp.h
 *  \brief Analog Comparator Driver
 *
 * ACOMP driver provides mdev interface to use 2 analog comparators
 * in mc200. Follow the steps below to use ACOMP driver from
 * applications
 *
 * 1. Include Files
 *
 * Include mdev.h and mdev_acomp.h in your source file.
 *
 * 2. ACOMP driver initialization
 *
 * Call \ref acomp_drv_init from your application initialization function.
 * This will initialize and register acomp driver with the mdev interface.
 *
 * 3. Open ACOMP device to use
 *
 * \ref acomp_drv_open should be called once before using ACOMP hardware.
 * Here you will specify ACOMP number (from 0 to 1). The other parameters
 * that are set by default are -<br>
 * ACOMP_HYSTER_0MV - Hysteresis 0mV for postive input<br>
 * ACOMP_HYSTER_0MV - Hysteresis 0mV for negative input<br>
 * LOGIC_LO         - Output LOW during inactive state<br>
 * ACOMP_PWMODE_1   - BIAS Current (Response time)<br>
 * ACOMP_WARMTIME_16CLK - Warm-up time 16 clock cycles<br>
 * ACOMP_PIN_OUT_SYN - Synchronous pin output<br>
 *
 * If these defaults need to be changed they should be changed in
 * \ref acomp_drv_open
 *
 * \code
 * {
 *	mdev_t *acomp_dev;
 *	acomp_dev = acomp_drv_open(MDEV_ACOMP<0-1>);
 * }
 * \endcode
 *
 * 4. Get result/output of ACOMP
 *
 * Call \ref acomp_drv_result to get the output of the mentioned
 * comparator.
 *
 * 5. Closing a device.
 *
 * A call to \ref acomp_drv_close informs the ACOMP driver to release the
 * resources that it is holding.
 *
 *
 *  Copyright 2008-2013, Marvell International Ltd.
 *  All Rights Reserved.
 */

#ifndef _MDEV_ACOMP_H_
#define _MDEV_ACOMP_H_

#include <mdev.h>
#include <mc200_acomp.h>

#define ACOMP_LOG(...)  wmprintf("[acomp] " __VA_ARGS__)

/** Register ACOMP Device
 *
 * This function registers acomp driver with mdev interface.
 *
 * \param[in] id ACOMP device to be used.
 * \return WM_SUCCESS on success, error code otherwise.
 */
int acomp_drv_init(ACOMP_ID_Type id);

/** Open ACOMP Device
 *
 * This function opens the device for use and returns a handle. This handle
 * should be used for other calls to the ACOMP driver.
 *
 * \param[in] acomp_id ACOMP device id (ACOMP_ID_Type) to be opened.
 * \param[in] pos Positive channel to be used.
 * \param[in] neg Negative channel to be used.
 * \return NULL if error, mdev_t handle otherwise.
 */
mdev_t *acomp_drv_open(ACOMP_ID_Type acomp_id, ACOMP_PosChannel_Type pos,
					 ACOMP_NegChannel_Type neg);

/** Get ACOMP Output
 *
 * This function gets the output of acomp device.
 *
 * \param[in] mdev_p mdev_t handle to the driver.
 * \return The output value 0 or 1
 */
int acomp_drv_result(mdev_t *mdev_p);

/** Close ACOMP Device
 *
 * This function closes the device after use and frees up resources.
 *
 * \param[in] dev mdev handle to the driver to be closed.
 * \return WM_SUCCESS on success, -WM_FAIL on error.
 */
int acomp_drv_close(mdev_t *dev);

#endif /* !_MDEV_ACOMP_H_ */
