/*
 *  Copyright (C) 2008-2013, Marvell International Ltd.
 *  All Rights Reserved.
 */

/**
 *
 * Summary:
 * Simple application which uses SSP driver as slave.
 *
 * Description:
 * This application uses APIs exposed by mdev layer of SSP driver.
 * In this application, 88MC200 is configured in slave mode
 * (using SPI protocol).
 * It receives data sent by SSP master on SSP bus and prints it on the console.
 *
 * Specification of SSP pins are:
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

/* Thread handle */
static os_thread_t app_thread;

/* Buffer to be used as stack */
static os_thread_stack_define(app_stack, 4096);

/* Buffer to read data from SSP interface*/
uint8_t read_data[BUF_LEN];

/* SSP mdev handle */
mdev_t *ssp0;

/* Read data from SSP master after every 100msec */
static void ssp_slave_read(os_thread_arg_t data)
{
	wmprintf("ssp_slave_demo Application Started\r\n");

	int len, i, cnt = 0;

	while (1) {
		cnt++;
		/* Read data from SSP bus */
		len = ssp_drv_read(ssp0, read_data, BUF_LEN);
		wmprintf("Read iteration %d:\r\n", cnt);

		for (i = 0; i < len ; i++)
			wmprintf("%d ", read_data[i]);
		if (len)
			wmprintf("\n\r");

		os_thread_sleep(os_msec_to_ticks(100));
	}
}

/**
 * All application specific initializations are performed here.
 */
int main(void)
{
	/* Initialize wmstdio console */
	wmstdio_init(UART0_ID, 0);
	wmprintf("SPI_Demo application Started\r\n");

	/* Initialize SSP Driver */
	ssp_drv_init(SSP0_ID);

	/* Set SPI clock frequency (Optional) */
	ssp_drv_set_clk(SSP0_ID, 13000000);

	/*
	 * Set SPI software ringbuffer size (Optional).
	 * This buffer is allocated in driver.
	 */
	ssp_drv_rxbuf_size(SSP0_ID, 1024);

	/* Configure SSP as SPI device. SSP0 is configured as slave
	 */
	ssp0 = ssp_drv_open(SSP0_ID, SSP_FRAME_SPI, SSP_SLAVE,
			DMA_DISABLE, -1, 0);


	/* Create the application thread */

	return os_thread_create(&app_thread,	/* thread handle */
			"ssp_demo",	/* thread name */
			ssp_slave_read,	/* entry function */
			0,	/* argument */
			&app_stack,	/* stack */
			OS_PRIO_2);	/* priority - medium low */
}
