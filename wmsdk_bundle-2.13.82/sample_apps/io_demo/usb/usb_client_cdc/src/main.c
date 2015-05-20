/*
 *  Copyright (C) 2008-2013 Marvell International Ltd.
 *  All Rights Reserved.
 *
 *  This application demonstrates the use of the USB driver for exercising
 *  the basic functionality of Communication Device Class (CDC-ACM) of USB.
 *
 *  Summary:
 *  The application exposes a USB CDC class device. This implies that the
 *  device will be detected as a USB Serial device. An echo server is
 *  implemented.
 *
 *  Description:
 *  For using this application, connect the USB-OTG port of your development
 *  board to your host's USB post using a USB cable. If the correct drivers are
 *  available, your development board will be detected as a virtual COM port on
 *  your host. (Note that a driver for Windows is provided in the
 *  usb/usb_client_cdc/src/ directory, named as marvell-mc200-vcom.inf)
 *
 *  The virtual COM port can be opened using a serial communication utility like
 *  Putty, Minicom or HyperTerminal. The serial communication utility should be
 *  configured with a baud rate of 9600, 8 data bits and no parity.
 *
 *  The application implements an echo server. This implies that whatever
 *  you type in the utility will be echoed back (that is seen on the screen).
 *
 *  An application creates a (very minimal) usb-rw thread for read and
 *  write-back (echo server) in continuous manner using usb client driver
 *  provided usb_drv_{read/write} APIs.
 */

#include <wmstdio.h>
#include <wm_os.h>
#include <mdev_usb.h>
#include <appln_dbg.h>
#include <appln_cb.h>
#include <cli.h>

/* Thread handle */
static os_thread_t usb_rw_thread;

/* Buffer to be used as stack */
static os_thread_stack_define(usb_rw_stack, 8096);

/* Global handle for USB driver */
static mdev_t *usb_dev;

/* Thread main function responsible for basic USB CDC echo server
 * implementation
 */
static void usb_rw_function(os_thread_arg_t data)
{
	uint32_t byteRead, bytetoRead = 64;
	uint8_t buf[bytetoRead + 1];

	wmprintf("[usb-cdc-app] usb_rw thread started\r\n");

	while (1) {
		byteRead = usb_drv_read(usb_dev, buf, bytetoRead, 60, 2);

		if (byteRead != 0 && byteRead != WM_E_USB_TIMEOUT_ERROR &&
		    byteRead != WM_E_USB_CONNECTION_ERROR)
			usb_drv_write(usb_dev, buf, byteRead, 60, 2);
	}
}

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

/*
 * The application entry point is main(). At this point, minimal
 * initialization of the hardware is done, and also the RTOS is
 * initialized.
 */
int main()
{
	modules_init();

	dbg("Build Time: " __DATE__ " " __TIME__ "");
	wmprintf("[usb-cdc-app] USB Client CDC demo application "
				"started\r\n");

	/* Initialize and open USB driver */
	usb_drv_init(USB_CDC_CLASS);
	usb_dev = usb_drv_open("MDEV_USB");

	/* Create the main application thread */
	os_thread_create(&usb_rw_thread,	/* thread handle */
			 "usb_rw",		/* thread name */
			 usb_rw_function,	/* entry function */
			 0,			/* argument */
			 &usb_rw_stack,		/* stack */
			 OS_PRIO_3);		/* priority - medium low */

	return 0;
}
