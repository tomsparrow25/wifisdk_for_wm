/* Copyright 2008-2013, Marvell International Ltd.
 *  All Rights Reserved.
 */

/*! \file mdev_pinmux.h
 * \brief GPIO Pin Multiplexer Driver
 *
 *  Various functionalities are multiplexed on the same GPIO pin
 *  as the number of Pins are limited.
 *  The functionality to be assigned to any particular pin can be
 *  configured using the PINMUX driver.
 *
 * @section mdev_pinmux_usage Usage
 *
 *  A typical PINMUX device usage scenario is as follows:
 *
 *  -# Initialize the PINMUX driver using pinmux_drv_init().
 *  -# Open the PINMUX device handle  using pinmux_drv_open() call.\n
 *  -# Set the alternate function using pinmux_drv_setfunc()
 *  -# Close the PINMUX device using pinmux_drv_close() after its use.\n
 *     pinmux_drv_close() informs the PINMUX driver to release the
 *     resources that it is holding. It is always good practice to
 *     free the resources after use.
 *
 *  Code snippet:\n
 *  Following code demonstrates how to use PINMUX driver APIs
 *  to set a pin's alternate function.
 *  @code
 *
 *  {
 *    pinmux_drv_init();
 *         .
 *         .
 *         .
 *    mdev_t* pinmux_dev = pinmux_drv_open("MDEV_PINMUX");
 *    if (pinmux_dev == NULL)
 *        err_handling();
 *
 *    int err = pinmux_drv_setfunc(pinmux_dev, pin, PINMUX_FUNCTION_0);
 *    if (err != WM_SUCESS)
 *        err_handling();
 *    pinmux_drv_close(pinmux_dev);
 *    return;
 *  }
 *
 * @endcode
 *
 */


#ifndef _MDEV_PINMUX_H_
#define _MDEV_PINMUX_H_

#include <mdev.h>
#include <mc200_pinmux.h>
#include <wmlog.h>

#define PINMUX_LOG(...)  wmlog("pinmux", ##__VA_ARGS__)


/** Initialize the PINMUX driver
 *
 *  This function initializes PINMUX driver and registers
 *  it with mdev interface.
 *  @return WM_SUCCESS on success
 *  @return Error code otherwise
 */
int pinmux_drv_init(void);

/** Open PINMUX device driver
 *
 * This function opens the handle to PINMUX device and enables
 * application to use the device. This handle should be used
 * in other calls related to PINMUX device.
 *
 *  @param [in] name Name of mdev pinmux driver.
 *              It should be "MDEV_PINMUX" string.
 *  @return handle to driver on success
 *  @return NULL otherwise
 */
mdev_t *pinmux_drv_open(const char *name);

/** Close the PINMUX device
 *
 * This function closes the handle to PINMUX device.
 *  @param [in] dev Handle to the PINMUX device returned by pinmux_drv_open().
 *  @return WM_SUCCESS on success
 */
int pinmux_drv_close(mdev_t *dev);

/** Set alternate function of a GPIO pin
 *
 *  Sets the alternate function for the specified pin.
 *
 *  @param [in]  dev Handle to the PINMUX device returned by pinmux_drv_open().
 *  @param [in] pin Pin number(GPIO_NO_Type) of the pin to be modified
 *  @param [in] func Alternate function number(GPIO_PinMuxFunc_Type)
 *  @return WM_SUCCESS on success
 *  @return -WM_FAIL  on error
 */
int pinmux_drv_setfunc(mdev_t *dev, int pin, int func);
#endif /* _MDEV_PINMUX_H_ */
