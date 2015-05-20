/*! \file mdev_sdio.h
 * \brief SDIO Driver
 *
 *  The SDIO driver is used to configure and do
 *  data transfer over the SDIO interface
 *
 * \section mdev_sdio_usage Usage
 *
 *  Steps to use sdio with mdev:
 *
 *  1. Register gpio using \ref sdio_drv_init()
 *
 *  2. Open the device using sdio_drv_open() call.
 *     This will return a handler to gpio.
 *
 *  3. To do various SDIO related operations:
 *
 *	- sdio_drv_write() : Write using CMD53
 *	- sdio_drv_read() : Read using CMD53
 *	- sdio_drv_creg_write() : Write using CMD52
 *	- sdio_drv_creg_read() : Read using CMD52
 *
 *     Locking mechanism is implemented to provide atomic access.
 *
 *  4. Close the device using sdio_drv_close() call.
 */
/*
 *  Copyright 2008-2013, Marvell International Ltd.
 *  All Rights Reserved.
 */

#ifndef _MDEV_SDIO_H_
#define _MDEV_SDIO_H_

#include <mdev.h>
#include <mc200_sdio.h>
#include <wmlog.h>

#define sdio_e(...)                            \
	wmlog_e("sdio", ##__VA_ARGS__)

#define sdio_l(...)				\
	wmlog("sdio", ##__VA_ARGS__)

#define SDIO_GO_IDLE_STATE                 ((uint8_t)0)
#define SDIO_SET_REL_ADDR                  ((uint8_t)3)
#define SDIO_SDIO_SEN_OP_COND              ((uint8_t)5)
#define SDIO_SEL_DESEL_CARD                ((uint8_t)7)
#define SDIO_RW_DIRECT                     ((uint8_t)52)
#define SDIO_RW_DIRECT_EXTENDED            ((uint8_t)53)

/* Depends on DMA_BUFSZ */
#define DMA_BOUNDARY_SIZE		(512 * 1024)

/** Read Card Register
 *
 * This is used to read card register using CMD52.
 * This is a blocking call.
 *
 *  \param dev mdev handle returned by sdio_drv_open
 *  \param addr Card Register Address
 *  \param fn Number of the function with the Card
 *  \param resp Response of CMD52
 *  \return true on success, false otherwise
 */
int sdio_drv_creg_read(mdev_t *dev, int addr, int fn, uint32_t *resp);

/** Write to Card Register
 *
 * This is used to write to card register using CMD52.
 * This is a blocking call.
 *
 *  \param dev mdev handle returned by sdio_drv_open
 *  \param addr Card Register Address
 *  \param fn Number of the function with the Card
 *  \param data value to be written on register
 *  \param resp Response of CMD52
 *  \return true on success, false otherwise
 */
int sdio_drv_creg_write(mdev_t *dev, int addr, int fn, uint8_t data,
							uint32_t *resp);
/** Read Data from SDIO
 *
 * This is used to read data from SDIO card using CMD53.
 * This is a blocking call.
 *
 *  \param dev mdev handle returned by sdio_drv_open
 *  \param addr Card Register Address
 *  \param fn Number of the function with the Card
 *  \param bcnt Number of blocks to be read
 *  \param bsize Size of each block
 *  \param buf Buffer to read the data into
 *  \param resp Response of CMD53
 *  \return true on success, false otherwise
 */
int sdio_drv_read(mdev_t *dev, uint32_t addr, uint32_t fn, uint32_t bcnt,
				uint32_t bsize, uint8_t *buf, uint32_t *resp);

/** Write Data to SDIO
 *
 * This is used to write data to SDIO card using CMD53.
 * This is a blocking call.
 *
 *  \param dev mdev handle returned by sdio_drv_open
 *  \param addr Card Register Address
 *  \param fn Number of the function with the Card
 *  \param bcnt Number of blocks to be written
 *  \param bsize Size of each block
 *  \param buf Buffer to write the data into
 *  \param resp Response of CMD53
 *  \return true on success, false otherwise
 */
int sdio_drv_write(mdev_t *dev, uint32_t addr, uint32_t fn, uint32_t bcnt,
				uint32_t bsize, uint8_t *buf, uint32_t *resp);

/** Open SDIO Driver
 *
 * This returns a mdev handle.
 *
 *  \param name Name of driver to be opened
 *  \return handle to the opened driver on success, NULL on failure
 */
mdev_t *sdio_drv_open(const char *name);

/** Initialize the SDIO Driver
 *
 * This should be called once before using the driver.
 * The callback for Card Interrupt can be registered using this call.
 *
 *  \param cd_int Callback for Card Detect Interrupt
 *  \return WM_SUCCESS or -WM_FAIL
 */
int sdio_drv_init(void (*cd_int)(int));
/*
 * Re-initializes SDIO driver
 *
 * This function is called on exit from MC200 low power modes
 * PM3 and PM4. Since SDIO controller register configuration is not retained
 * in on exit from PM3 and PM4 controller needs to be reconfigured.
 *  \return WM_SUCESS or -WM_FAIL
 *  This function assumes that sdio_drv_init has been called earlier
 */

int sdio_drv_reinit(void (*cd_int)(int));

/** De-initialize the SDIO Driver
 *
 * SDIO will not be useable after this call.
 * All resources are released and driver is
 * deregistered.
 *
 */
void sdio_drv_deinit(void);

/** Register additional callback
 *
 * Additional callback for Card Interrupt (other than the one
 * set during sdio_drv_init() ) can be registered using this call.
 *
 *  \param cd_int Callback for Card Detect Interrupt
 */
void sdio_drv_set_cb1(void (*cd_int)(int));

#endif /* !_MDEV_SDIO_H_ */
