/*  Copyright 2008-2013, Marvell International Ltd.
 *  All Rights Reserved.
 */

/*! \file mdev_ssp.h
 * \brief Synchronous Serial Protocol (SSP) Driver
 *
 * The SSP port is a synchronous serial controller that can be connected to
 * a variety of external devices which use serial protocol for data transfer.
 * 88MC200 has 3 SSP interfaces viz, SSP0 , SSP1, SSP2. The SSP ports can
 * be configured to operate in Master Mode (wherein attached device is slave)
 * or Slave Mode (wherein external device is master).
 * SSP driver exposes various SSP device features through mdev interface.
 *
 * @section mdev_ssp_usage Usage
 *
 * -# Initialize the SSP driver using ssp_drv_init().
 * -# Open the SSP device handle using ssp_drv_open().
 *    ssp_drv_open() takes as input  frame format (SPI or I2S)
 *    and master/slave mode. Optionally it can also accept custom
 *    chip select gpio pin for slave device.
 * -# Use ssp_drv_read() or ssp_drv_write() to read SSP data or
 * -# ssp_drv_set_clk() can be used to set frequency of SSP port.
 * -# ssp_drv_rxbuf_size() can be called to set Receive buffer size.
 *    to write data to SSP respectively.
 * -# Close the SSP device using ssp_drv_close() after its use.
 *    ssp_drv_close() call informs the SSP driver to release the resources that
 *    it is holding. It is always good practice to free the resources after use.
 *
 *  Code snippet:\n
 *  Following code demonstrates how to configure SSP0 in master mode
 *  and transmit data.
 *
 *  @code
 *  {
 *	int ret;
 *	uint8_t buf[512];
 *      uint8_t read_buf[512];
 *	mdev_t ssp_dev;
 *      ssp_drv_init(SSP0_ID);
 *      ssp_drv_set_clk(SSP0_ID, 13000000);
 *      ssp_drv_rxbuf_size(SSP0_ID, 1024);
 *      ssp_dev = ssp_drv_open(SSP0_ID, SSP_FRAME_SPI,
 *                             SSP_MASTER, DMA_DISABLE,
 *                             -1, 0);
 *      unit32_t len = ssp_drv_write(ssp_dev, write_data,
 *                                    NULL,
 *                                    512,
 *                                    0);
 *                       .
 *                       .
 *                       .
 *      len = ssp_drv_read(ssp_dev, read_buf, 512);
 *
 *	ssp_drv_deinit(ssp_dev);
 * }
 * @endcode
 *
 *  @note SSP_ID_Type enumeration has values SSP0_ID, SSP1_ID, SSP2_ID
 *
 *  @section default_param Default Parameter Configuration
 *
 *     Default configuration parameter set for SPI are:<br>
 *     mode : SSP_NORMAL<br>
 *     frameformat : SSP_FRAME_SPI<br>
 *     master or slave : configured by user<br>
 *     tr/rx mode  : SSP_TR_MODE<br>
 *     frame data size : SSP_DATASIZE_8<br>
 *     serial frame polarity type : SSP_SAMEFRM_PSP<br>
 *     slave clock running type : SSP_SLAVECLK_TRANSFER<br>
 *     txd turns to three state type : SSP_TXD3STATE_ELSB (i.e. on clock edge
 *     ends LSB)<br>
 *     Enable or Disable SSP turns txd three state mode: ENABLE<br>
 *     Timeout : 0x3<br>
 *
 *     Default configuration parameters set for I2S are:<br>
 *     mode : SSP_NORMAL<br>
 *     frameformat : SSP_FRAME_PSP<br>
 *     master or slave : configured by user<br>
 *     tr/rx mode  : SSP_TR_MODE<br>
 *     frame data size : SSP_DATASIZE_8<br>
 *     serial frame polarity type : SSP_SAMEFRM_PSP<br>
 *     slave clock running type : SSP_SLAVECLK_TRANSFER<br>
 *     txd turns to three state type : SSP_TXD3STATE_12LSB (i.e. 1/2 clock
 *     cycle after start LSB)<br>
 *     Enable or Disable SSP turns txd three state mode: ENABLE<br>
 *     Timeout : 0x3<br>
 */

#ifndef _MDEV_SSP_H_
#define _MDEV_SSP_H_

#include <mdev.h>
#include <mc200_ssp.h>

#define SSP_LOG(...)  ll_printf("[ssp] " __VA_ARGS__)

#define ACTIVE_HIGH 1
#define ACTIVE_LOW 0

#define SPI_DUMMY_BYTE 0x00

/** enum to enable or disable DMA in SSP data transfer */
typedef enum {
	/** Disable ssp DMA controller */
	DMA_DISABLE = 0,
	/** Enable ssp DMA controller */
	DMA_ENABLE,
} SSP_DMA;


/** Initialize SSP Driver
 *
 * This function initializes SSP driver. And registers the device with the mdev
 * interface.
 *
 * @param[in] id  ssp port to be initialized.
 * @return WM_SUCCESS on success
 * @return error code otherwise
 */

int ssp_drv_init(SSP_ID_Type id);

/** Open handle to SSP Device
 *
 * This function opens the handle to SSP device and enables application to use
 * the device. This handle should be used in other calls related to SSP device.
 *
 *  @param[in] ssp_id SSP ID of the driver to be opened
 *  @param[in] format configure mdev driver for SPI/I2S
 *  @param[in] mode it can be either master or slave
 *  @param[in] dma it can be either enabled or disabled
 *  @param[in] cs chip select gpio number (optional) or -1, for master mode
 *  only
 *  @param[in] level chip select gpio level, active low or active high, for
 *  master mode only
 *  @return handle to driver on success
 *  @return NULL otherwise
 *
 *  @note
 *       -# If DMA mode is enabled, it will used DMA channel 0 for SSP tx and
 *          DMA channel 1 for SSP rx operation.
 */
mdev_t *ssp_drv_open(SSP_ID_Type ssp_id, SSP_FrameFormat_Type format,
			SSP_MS_Type mode, SSP_DMA dma, int cs, bool level);

/** Close handle to SSP Device
 *
 * This function closes the handle to SSP device and resets the SSP.
 *
 * @param[in] dev Handle to the SSP device returned by ssp_drv_open().
 * @return WM_SUCCESS on success
 * @return -WM_FAIL on error.
 */
int ssp_drv_close(mdev_t *dev);

/** Write data to SSP port
 *
 * This function is used to write data to the SSP in order to transmit them
 * serially over serial out line.
 * The write call is BLOCKING. It will not return till all
 * the requested number of data elements (8 bit) are transmitted.
 *
 *  @param[in] dev Handle to the SSP device returned by ssp_drv_open().
 *  @param[in] data  data to be written
 *  @param[in] din buffer address for data to be read (if flag = 1) or null
 *  @param[in] num Number of 8 bit elements to be written
 *  @param[in] flag flag = 1, read data from rxfifo
 *  @return Number of 8 bit elements actually written
 *
 *  @note When SSP pushes out a byte from txfifo, it receives a byte in rxfifo.
 *        So, depending on slave device behavior one may want to read out these
 *        bytes from rxfifo and store them or discard them.
 *        If flag = 1 then rxfifo will be read out and data will be stored in
 *        din. If din is NULL data will be read out and discarded. If flag = 0
 *        data will not be read.
 *        e.g. Some flash chip interfaced over ssp might need to discard read
 *        data when writing some command to flash chip.
 */
int ssp_drv_write(mdev_t *dev, const uint8_t *data, uint8_t *din,
		uint32_t num, bool flag);

/** Read from SSP port
 *

 * This function is used to read data from the SSP
 * The Read call is NONBLOCKING for slave and BLOCKING for master.
 *
 *  @param[in] dev Handle to the SSP device returned by ssp_drv_open().
 *  @param[out] data Pointer to an allocated buffer of size equal to or more
 *             than the value of the third parameter num.
 *  @param[in] num The maximum number of bytes to be read from the SSP.
 *  @return Number of 8 bit elements actually read
 */
int ssp_drv_read(mdev_t *dev, uint8_t *data, uint32_t num);

/** Assert chip select gpio
 *
 *  This will assert chip select gpio (only applicable to master mode) as per
 *  level configured during driver init step. Use of this api is entirely
 *  optional.
 *  @param[in] dev Handle to the SSP device returned by ssp_drv_open().
 */
void ssp_drv_cs_activate(mdev_t *dev);

/** De-assert chip select gpio
 *
 *  This will de-assert chip select gpio (only applicable to master mode) as
 *  per level configured during driver init step. Use of this api is entirely
 *  optional.
 *  @param[in] dev Handle to the SSP device returned by ssp_drv_open().
 */
void ssp_drv_cs_deactivate(mdev_t *dev);

/** Set clock frequency
 *
 *  This is optional call to set ssp clock frequency. By default
 *  clock frequency is set to 10MHz. This call should be made after
 * ssp_drv_init and before ssp_drv_open to over-write default clock
 * configuration.
 *
 * @param[in] ssp_id SSP ID of the driver
 * @param[in] freq frequency to be set
 * @return Actual frequency set by driver
 * @return -WM_FAIL on error
 */
uint32_t ssp_drv_set_clk(SSP_ID_Type ssp_id, uint32_t freq);

/** Set RX Ringbuffer size
 *
 * This is optional call to change rx ringbuffer size. By default
 * ringbuffer size is set to 2K. This call should be made after
 * ssp_drv_init and before ssp_drv_open to over-write default configuration.
 *
 * @param[in] ssp_id SSP ID of the driver
 * @param[in] size size of ring buffer in bytes
 * @return WM_SUCCESS on success
 * @return -WM_FAIL on error
 */
int ssp_drv_rxbuf_size(SSP_ID_Type ssp_id, uint32_t size);
#endif /* _MDEV_SSP_H_ */
