/*
 *  Copyright (C) 2008-2013, Marvell International Ltd.
 *  All Rights Reserved.
 */

/**
 *
 * Summary:
 * Simple Application which uses SSP driver for full duplex communication.
 *
 * Description:
 * This application uses APIs exposed by mdev layer of SSP driver to demonstrate
 * full duplex communication (using SPI protocol).
 * By default, SSP is configured in master mode. It can be configured as slave
 * by defining SSP_FULL_DUPLEX_SLAVE macro in the application.
 * The application sends 16 bytes data to other SPI device and reads 16 bytes
 * data after every 1000 msec and prints it on the console.
 *
 * Specification of SSP0 pins are:
 * Pin No(lk20-v3)  | Pin No(mc200_dev-v2) | Use |
 * -----------------------------------------------
 *   I32            |   IO32               | CLK |
 *   I33            |   IO33               | FRM |
 *   I34            |   IO34               | RX  |
 *   I35            |   IO35               | TX  |
 * -----------------------------------------------
 */

#include <wmstdio.h>
#include <wm_os.h>
#include <mdev_ssp.h>

/* Buffer length */
#define BUF_LEN 16

/* Buffer to write data on SSP interface*/
uint8_t write_data[BUF_LEN];
/* Buffer to read data from SSP interface*/
uint8_t read_data[BUF_LEN];

/* SSP mdev handle */
mdev_t *ssp0;

/**
 * All application specific initializations are performed here.
 */
int main(void)
{
	int len, i, j;

	/* Initialize wmstdio console */
	wmstdio_init(UART0_ID, 0);
	wmprintf("SSP full duplex demo application started\r\n");

	/* Initialize SSP Driver */
	ssp_drv_init(SSP0_ID);
	/* Set SPI clock frequency (Optional) */
	ssp_drv_set_clk(SSP0_ID, 13000000);
	/*
	 * Set SPI software ringbuffer size (Optional).
	 * This buffer is allocated in driver.
	 */
	ssp_drv_rxbuf_size(SSP0_ID, 1024);

#ifndef SSP_FULL_DUPLEX_SLAVE
	/*
	 * Configure SSP as SPI device in master mode if SSP_FULL_DUPLEX_SLAVE
	 * macro is not defined.
	 */
	ssp0 = ssp_drv_open(SSP0_ID, SSP_FRAME_SPI, SSP_MASTER, DMA_DISABLE,
			-1, 0);
	wmprintf("SSP Master Started\r\n");
#else
	/*
	 * Configure SSP as SPI device in slave mode if SSP_FULL_DUPLEX_SLAVE
	 * macro is defined.
	 */
	ssp0 = ssp_drv_open(SSP0_ID, SSP_FRAME_SPI, SSP_SLAVE, DMA_DISABLE,
			-1, 0);
	wmprintf("SSP Slave Started\r\n");
#endif

	for (i = 0; i < BUF_LEN; i++)
		write_data[i] = i;
	i = 0;
	while (1) {
		i++;
		memset(read_data, 0, BUF_LEN);
		/*
		 * Write 16 bytes data on SPI interface and read 16 bytes
		 * data from SPI interface.
		 */
		len = ssp_drv_write(ssp0, write_data, read_data, BUF_LEN, 1);
		wmprintf("Iteration %d: %d bytes written\n\r", i, len);
		for (j = 0; j < BUF_LEN; j++)
			wmprintf(" %x ", read_data[j]);
		wmprintf("\n\r");
		os_thread_sleep(os_msec_to_ticks(1000));
	}
}
