/*
 *  Copyright (C) 2008-2013, Marvell International Ltd.
 *  All Rights Reserved.
 */

/*
 *
 * Summary:
 * Simple Application to echo the data on UART.
 *
 * Description:
 * This project shows how to use UART1 with MDEV layer. UART1 is configured at
 * baud rate of 115200 and enabled as 8bit. Application echoes back the line
 * written on UART1 port.
 */

#include <string.h>
#include <wm_os.h>
#include <wmtypes.h>
#include <wmstdio.h>
#include <mdev_uart.h>
#include <wmstdio.h>

/* Size of UART buffer */
#define BUFSIZE 256

/* Buffer for read and write*/
char buf[BUFSIZE];

/* This is an entry point for the application.
   All application specific initializations are performed here. */
int main(void)
{
	int count = 0, len;
	char temp;
	mdev_t *dev;

	/* Initialize wmstdio console on UART0 */
	wmstdio_init(UART0_ID, 0);

	wmprintf("You are connected to UART0 port. uart_demo app started on"
		 " UART1 port. Please check UART1 port.\n\r");

	/* Initialize UART1 with 8bit. This will register UART1 MDEV handler */
	uart_drv_init(UART1_ID, UART_8BIT);

	/* Open UART1 with 15200 baud rate. This will return mdev UART1
	 * handle */
	dev = uart_drv_open(UART1_ID, 115200);
	snprintf(buf, BUFSIZE, "UART1 Demo\n\r");

	/* Print "UART1 Demo" message on UART1 Port" */
	uart_drv_write(dev, (uint8_t *)buf, 12);

	memset(buf, 0, BUFSIZE - 1);

	while (1) {
		/* Read one character from UART1 Port. */
		len = uart_drv_read(dev, (uint8_t *)&temp, 1);
		if (len > 0 && (temp == 13 || count == (BUFSIZE - 3))) {
			temp = '\n';
			uart_drv_write(dev, (uint8_t *)&temp, 1);
			temp = '\r';
			uart_drv_write(dev, (uint8_t *)&temp, 1);
			buf[count++] = '\n';
			buf[count++] = '\r';

			/* Write (echo back) on UART1 Port*/
			uart_drv_write(dev, (uint8_t *)buf, count);
			count = 0;
			memset(buf, 0, BUFSIZE - 1);
			continue;
		} else if (len) {
			buf[count] = temp;
			count++;
			uart_drv_write(dev, (uint8_t *)&temp, 1);
		}

		os_thread_sleep(os_msec_to_ticks(100));
	}

	return 0;
}
