/*! \file lcd.h
 * \brief LCD Driver
 *
 *  The LCD driver is used to write data to the LCD.
 *
 * \section mdev_lcd_usage Usage
 *
 *  lcd_drv_init() will register the LCD driver with mdev.
 *
 *  Steps to use lcd with mdev:
 *
 *  1. Register lcd using lcd_drv_init().
 *
 *  2. Open driver using  lcd_drv_open() call. This will return
 *     a handler to lcd driver.
 *
 *  3. Write data using lcd_drv_write().
 *
 *
 *  4. Close driver using lcd_drv_close() call.
 *
 */
/*  Copyright 2008-2013, Marvell International Ltd.
 *  All Rights Reserved.
 */

/* NHD-C0216CiZ-FSW-FBW-3V3 LCD Driver */

#ifndef _LCD_H_
#define _LCD_H_

#include "mdev_i2c.h"
#include "wmlog.h"

#define lcd_e(...)				\
	wmlog_e("lcd", ##__VA_ARGS__)
#define lcd_w(...)				\
	wmlog_w("lcd", ##__VA_ARGS__)
#define lcd_d(...)				\
	wmlog("lcd", ##__VA_ARGS__)

/** Register the LCD driver
 *
 *  \param id I2C Port to which lcd is connected
 * \return WM_SUCCESS on success, error code otherwise.
 */
int lcd_drv_init(I2C_ID_Type id);

/** Open LCD device
 *
 *  \param name Name of driver to be opened. It should be "MDEV_LCD" string.
 *  \return mdev handle on success, NULL otherwise
 */
extern mdev_t *lcd_drv_open(const char *name);

/** Close LCD device
 *
 *  \param dev Handle of the driver to be closed
 *  \return 0 on success
 */
int lcd_drv_close(mdev_t *dev);

/** Write to the lcd
 *
 *  \param dev Handle of the driver
 *  \param line1 Data to be written on first line of lcd
 *  \param line2 Data to be written on second line of lcd
 *  \return 0 on success
 */
int lcd_drv_write(mdev_t *dev, const char *line1, const char *line2);
#endif /* _LCD_H_ */
