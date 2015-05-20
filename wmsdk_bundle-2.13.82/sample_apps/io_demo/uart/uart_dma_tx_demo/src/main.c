/*
 *  Copyright (C) 2008-2013, Marvell International Ltd.
 *  All Rights Reserved.
 */

/*
 *
 * Summary:
 * Simple Application which uses UART driver with DMA.
 *
 * Description:
 * This project shows how to enable DMA on UART MDEV layer. UART1 is configured
 * at baud rate of 115200 and enabled as 8bit with DMA support. Application
 * sends 512 bytes of data in one DMA write on UART1 interface using DMA.
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
	int len, i;
	mdev_t *dev;

	wmstdio_init(UART0_ID, 0);
	/* Initilize UART1 with 8bit. This will register UART1 MDEV handler */
	uart_drv_init(UART1_ID, UART_8BIT);

	/* Enable DMA on UART1 */
	uart_drv_xfer_mode(UART1_ID, UART_DMA_ENABLE);

	/* Open UART1 with 115200 baud rate. This will return mdev UART1
	 * handle */
	dev = uart_drv_open(UART1_ID, 115200);
	wmprintf("UART1 DMA TX Demo\n\r");

	i = 0;
	while (1) {
		memset(buf, i, BUFSIZE);
		len = uart_drv_write(dev, (uint8_t *)buf, BUFSIZE);
		wmprintf("Written %d bytes\n\r", len);
		os_thread_sleep(os_msec_to_ticks(1000));
		i = (i + 1) % 0x100;
	}

	return 0;
}
