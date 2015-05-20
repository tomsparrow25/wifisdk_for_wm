/*
 *  Copyright (C) 2008-2013 Marvell International Ltd.
 *  All Rights Reserved.
 *
 *  Simple serial port emulation using USB Host Stack.
 *
 *  Summary:
 *  The application exposes a USB CDC class host. Application communicates
 *  with CDC-ACM class devices. Host continuously sends out string with
 *  baud rate of 9600, 8 data bits, no parity and one stop bit. Application
 *  has been tested with a device set up as an echo server.
 *
 *  Description:
 *  For using this application, connect USB-OTG port of one development board
 *  to USB-OTG port of another development board. Make sure host end of cable
 *  is connected to USB-OTG port of host and device end to the device.
 *
 *  Application sends out "USB OTG Demo" to the client using usb_host_drv_write
 *  API and reads contents of IN endpoint using usb_host_drv_read API.
 *
 *  Application expects echo server at the device side, which echoes string
 *  sent to it by host and host prints the string on terminal.
 *
 */

#include <wmstdio.h>
#include <wm_os.h>
#include <wm_net.h>
#include <mdev_usb_host.h>
#include <arch/sys.h>
#include <wmsysinfo.h>
#include <app_framework.h>
#include <wmtime.h>
#include <appln_dbg.h>
#include <appln_cb.h>
#include <cli.h>

#define USBRS232_PARITY_NONE  0
#define USBRS232_PARITY_ODD   1
#define USBRS232_PARITY_EVEN  2
#define USBRS232_PARITY_MARK  3
#define USBRS232_PARITY_SPACE 4
#define USBRS232_STOPBITS_1   1
#define USBRS232_STOPBITS_2   2
#define USBRS232_STOPBITS_1p5 3 // 1.5 stop bits

#define MAX_DATA_LENTH	20
#define BAUD_RATE	9600
#define WORDLENGTH	8

static uint8_t str[MAX_DATA_LENTH] __attribute__ ((section(".iobufs")));
static uint8_t buf[MAX_DATA_LENTH] __attribute__ ((section(".iobufs")));

static void modules_init()
{
	int ret;

	ret = wmstdio_init(UART0_ID, 0);
	if (ret != WM_SUCCESS) {
		while (1)
			;
		/* do nothing -- stall */
	}

	ret = cli_init();
	if (ret != WM_SUCCESS) {
		dbg("Error: cli_init failed");
		while (1)
			;
		/* do nothing -- stall */
	}

	return;
}
int main()
{
	int ret;
	usb_cdc_drv_data drv;
	mdev_t *p;

	drv.Parity = USBRS232_PARITY_ODD;
	drv.Baud = BAUD_RATE;
	drv.StopBits = USBRS232_STOPBITS_1;
	drv.WordLength = WORDLENGTH;

	modules_init();

	dbg("Build Time: " __DATE__ " " __TIME__ "");
	wmprintf("[usb-cdc-app] USB CDC Class demo application "
				"started\r\n");

	ret = usb_host_drv_init(USB_CLASS_COMM, &drv);
	if (ret != WM_SUCCESS)
		wmprintf("Failed to init usb host driver\r\n");

	p = usb_host_drv_open("MDEV_USB_HOST");
	if (p == NULL) {
		wmprintf("Failed to get mdev handle\r\n");
		return -WM_FAIL;
	}

	ret = usb_host_drv_start(p);
	if (ret != WM_SUCCESS)
		wmprintf("Failed to start usb host driver\r\n");


	snprintf((char *)str, MAX_DATA_LENTH, "USB CDC Demo");

	while (1) {
		ret = usb_host_drv_write(p, str, sizeof(str));
		if (ret < 0)
			wmprintf("Error writing usb\n\r");
		else {
			ret = usb_host_drv_read(p, buf, sizeof(buf));
			if (ret < 0)
				wmprintf("Error reading from usb\r\n");
			wmprintf("Received string: %s\r\n", buf);
		}
		os_thread_sleep(os_ticks_to_msec(1000));
	}

	return 0;
}

