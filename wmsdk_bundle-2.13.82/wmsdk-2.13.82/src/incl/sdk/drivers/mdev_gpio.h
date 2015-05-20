/*  Copyright 2008-2013, Marvell International Ltd.
 *  All Rights Reserved.
 */
/*! \file mdev_gpio.h
 * \brief General Purpose Input Output (GPIO) Driver
 *
 * The GPIO driver is used to handle I/O requests to read/write GPIO pins.
 * GPIO pins are  used for communication with external entities.
 * They are used for sending control signals like chip select
 * and clock.
 *
 * @section mdev_gpio_usage Usage
 *
 *  A typical GPIO device usage scenario is as follows:
 *
 *  -# Initialize the GPIO driver using gpio_drv_init().
 *  -# Open the GPIO device handle  using gpio_drv_open() call.\n
 *  -# Use gpio_drv_setdir() to set direction of GPIO pin (input or output).
 *  -# Use gpio_drv_read() or gpio_drv_write()  to read GPIO state or\n
 *     to write GPIO pin state respectively.
 *  -# GPIO pins can be used to generate interrupts and interrupt\n
 *     callback is registered or unregistered using gpio_drv_set_cb().
 *  -# Close the GPIO device using gpio_drv_close() after its use.\n
 *     gpio_drv_close() informs the GPIO driver to release the
 *     resources that it is holding. It is always good practice to
 *     free the resources after use.
 *
 *  Code snippet:\n
 *  Following code demonstrates how to use GPIO driver APIs
 *  to set a pin as output and set it high.
 *  @code
 *
 *  {
 *     gpio_drv_init();

 *         .
 *         .
 *         .
 *    mdev_t* gpio_dev = gpio_drv_open("MDEV_GPIO");
 *    if (gpio_dev == NULL)
 *       err_handling();
 *    int err = gpio_drv_setdir(gpio_dev, pin, GPIO_IO_OUTPUT);
 *    if (err != WM_SUCESS)
 *       err_handling();

 *    err = gpio_drv_write(gpio_dev, pin, GPIO_IO_HIGH);
 *    if (err != WM_SUCESS)
 *       err_handling();
 *    gpio_drv_close(gpio_dev);
 *    return;
 *  }
 *
 * @endcode
 *  Following code demonstrates how to use GPIO driver APIs
 *  to read a pin state.
 *  @code
 *
 *  {
 *    int err = gpio_drv_init();
 *    if (err != WM_SUCESS)
 *       err_handling();
 *         .
 *         .
 *         .
 *    mdev_t* gpio_dev = gpio_drv_open("MDEV_GPIO");
 *    if (gpio_dev == NULL)
 *       err_handling();
 *    int state;
 *    int err = gpio_drv_read(gpio_dev, pin, &state);
 *    if (err != WM_SUCESS)
 *       err_handling();
 *    gpio_drv_close(gpio_dev);
 *    return;
 *  }
 * @endcode
 *  Following code demonstrates how to use GPIO driver APIs
 *  to generate and handle an interrupt.
 * @code
 *  static void gpio_cb()
 *  {
 *      process_interrupt_of_gpio();
 *  }

 *  {
 *    int err = gpio_drv_init();
 *    if (err != WM_SUCESS)
 *       err_handling();

 *         .
 *         .
 *         .
 *    mdev_t* gpio_dev = gpio_drv_open("MDEV_GPIO");
 *    if (gpio_dev == NULL)
 *       err_handling();
 *
 *    int err = gpio_drv_setdir(gpio_dev, pin, GPIO_IO_INPUT);
 *    if (err != WM_SUCESS)
 *       err_handling();
 *
 *    err = gpio_drv_set_cb(gpio_dev, pin, GPIO_INT_FALLING_EDGE,
 *                        gpio_cb);
 *    if (err != WM_SUCESS)
 *       err_handling();
 *
 *    gpio_drv_close(gpio_dev);
 *    return;
 *  }
 * @endcode
 * @note
 *     -# Locking mechanism is implemented to provide atomic access.
 *     -# The APIs assume that caller passed correct GPIO pin number.
 *     -# GPIO_Int_Type enumeration has folling possible values
 *        GPIO_INT_RISING_EDGE , GPIO_INT_FALLING_EDGE, GPIO_INT_DISABLE,
 *        GPIO_INT_DISABLE.
 *     -# A GPIO pin state can have only two possible values
 *        GPIO_IO_LOW and GPIO_IO_HIGH.
 *     -# A GPIO pin direction can be either GPIO_INPUT or GPIO_OUTPUT.
 *
 */

#ifndef _MDEV_GPIO_H_
#define _MDEV_GPIO_H_

#include <mdev.h>
#include <mc200_gpio.h>
#include <wmlog.h>

#define GPIO_LOG(...)  wmlog("gpio", ##__VA_ARGS__)

#define WL_CIU_MISC10		((void *)0x800020ac)


/** Initialize the GPIO driver
 *
 *  This function initializes GPIO driver and registers it with mdev interface.
 *  @return WM_SUCCESS on success
 *  @return Error code otherwise
 */
int gpio_drv_init(void);

/** Open GPIO device driver
 *
 * This function opens the handle to GPIO device and enables application to use
 * the device. This handle should be used in other calls related to GPIO device.
 *
 *  @param name Name of mdev gpio driver.
 *              It should be "MDEV_GPIO" string.
 *  @return handle to driver on success
 *  @return NULL otherwise
 */
mdev_t *gpio_drv_open(const char *name);

/** Close the GPIO device
 *
 * This function closes the handle to GPIO device.
 *  @param [in] dev Handle to the GPIO device returned by gpio_drv_open().
 *  @return WM_SUCCESS on Success
 */
int gpio_drv_close(mdev_t *dev);

/** Set direction of GPIO pin
 *
 *  This function allows caller to set direction of a GPIO pin.
 *  @param [in] dev Handle to the GPIO device returned by gpio_drv_open().
 *  @param [in] pin Pin Number(GPIO_XX) of GPIO pin (example: GPIO_12)
 *  @param [in] dir Either GPIO_INPUT or GPIO_OUTPUT
 *  @return WM_SUCCESS on Success
 *  @return Error code otherwise
 */

int gpio_drv_setdir(mdev_t *dev, int pin, int dir);

/** Set GPIO pin state
 *
 *  This function allows caller to set GPIO pin's state to high or low.
 *  @param [in] dev Handle to the GPIO device returned by gpio_drv_open().
 *  @param [in] pin Pin Number(GPIO_XX) of GPIO pin
 *  @param [in] val Value to be set.
 *         GPIO_IO_LOW(0) or  GPIO_IO_HIGH(1)
 *  @return WM_SUCCESS on success
 *  @return Error code otherwise
 */
int gpio_drv_write(mdev_t *dev, int pin, int val);

/** Read GPIO pin status
 *
 *  This function allows caller to get GPIO pin's state.
 *
 *  @param [in] dev Handle to the GPIO device returned by gpio_drv_open().
 *  @param [in]  pin Pin Number(GPIO_XX) of GPIO pin (example: GPIO_12)
 *  @param [out] *val Value read
 *  @return WM_SUCCESS on success
 *  @return Error code otherwise
 */
int gpio_drv_read(mdev_t *dev, int pin, int *val);

/** Register CallBack for GPIO Pin Interrupt
 *
 *  @param [in] dev Handle to the GPIO device returned by gpio_drv_open().
 *  @param [in] pin Pin Number(GPIO_XX) of GPIO pin
 *  @param [in] type Possible values from GPIO_Int_Type
 *  @param [in] gpio_cb Function to be called when said interrupt occurs.
 *              If NULL the callback will be de-registered.
 *  @note intCallback_Type is defined as typedef void (intCallback_Type)(void);
 *        GPIO pin's direction should be set as input to generate an interrupt.
 *
 *  @return WM_SUCCESS on success
 *  @return -WM_FAIL  if invalid parameters
 */
int gpio_drv_set_cb(mdev_t *dev, int pin, GPIO_Int_Type type,
					intCallback_Type *gpio_cb);
#endif /* _MDEV_GPIO_H_ */
