/*
 *  Copyright (C) 2008-2012, Marvell International Ltd.
 *  All Rights Reserved.
 */

/*
 *
 * Summary:
 * Simple Application which uses UART peripheral in DMA mode.
 *
 * Description:
 * This project shows how to enable DMA on UART MDEV layer. UART1 is configured
 * at baud rate of 115200 and enabled as 8bit with DMA support. Application
 * receives 512 bytes of data on UART1 interface using DMA.
 */

#include <string.h>
#include <wm_os.h>
#include <wmtypes.h>
#include <wmstdio.h>
#include <mdev_uart.h>

/* Size of UART buffer */
#define BUFSIZE (512)

/* Buffer for read and write*/
char buf[BUFSIZE];

/* This is an entry point for the application.
   All application specific initializations are performed here. */
int main(void)
{
	int error;
	uint32_t i = 0;
	uint8_t cnt = 0;
	mdev_t *dev;
	uint32_t total = 0;

	wmstdio_init(UART0_ID, 0);
	/* Initilize UART1 with 8bit. This will register UART1 MDEV handler */
	uart_drv_init(UART1_ID, UART_8BIT);

	/* Enable DMA on UART1 */
	uart_drv_xfer_mode(UART1_ID, UART_DMA_ENABLE);

	/* Set DMA block size */
	uart_drv_dma_rd_blk_size(UART1_ID, 512);

	/* Set internal rx ringbuffer size to 3K */
	uart_drv_rxbuf_size(UART1_ID, 1024 * 3);

	/* Open UART1 with 115200 baud rate. This will return MDEV UART1
	 * handle */
	dev = uart_drv_open(UART1_ID, 115200);
	wmprintf("UART1 DMA RX Demo\n\r");

	while (1) {
		/* Read one character from UART1 Port. */
		memset(buf, 0, BUFSIZE);
		total += uart_drv_read(dev, (uint8_t *)&buf, BUFSIZE - total);
		if (total >= BUFSIZE) {
			wmprintf("\n\r");
			error = 0;
			for (i = 0; i < BUFSIZE; i++)
				if (buf[i] != cnt)
					error = 1;
			if (error == 0)
				wmprintf("Received Frame #0x%02x", buf[0]);
			else
				wmprintf("Frame Missed, Expected: %d, Got:"
					"buf[0] = 0x%x, buf[BUF_LEN - 1] = 0x%x\n\r",
					cnt, buf[0], buf[BUFSIZE - 1]);
			cnt++;
			total = 0;
		}
		os_thread_relinquish();
	}

	return 0;
}
