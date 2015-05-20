/*  Copyright 2008-2013, Marvell International Ltd.
 *  All Rights Reserved.
 */
/*! \file mdev_usb_host.h
 *  \brief Universal Serial Bus (USB) Host driver
 *
 *  The USB host driver is used to initiate and arbitrate communication over
 *  USB-OTG port of 88MC200. It is meant to use USB hardware to communicate with
 *  external devices connected on the USB-OTG bus.
 *
 *  @section mdev_usb_host_usage Usage
 *
 *  A typical USB host usage scenario is as follows:
 *
 *  -# Initialize USB host driver using usb_host_drv_init().
 *  -# Open the USB host handle using usb_host_drv_open() call.
 *  -# Configure and start USB device using usb_host_drv_start() call
 *  -# Use usb_host_drv_read() or usb_host_drv_write() calls to read USB data
 *   or to write USB data respectively.
 *  -# Stop USB device using usb_host_drv_stop() call
 *  -# Close USB host using usb_host_drv_close() call after its use.
 *
 *  Code snippet:\n
 *  Following code demonstrate how to use USB CDC host driver APIs for
 *  read/write operation. It sets line coding parameters as 9600,8,N,1 and
 *  reads 4 bytes of data and writes it back.
 *
 *  @code
 *  {
 *   int ret;
 *   usb_cdc_drv_data drv;
 *   mdev_t *usb_dev;
 *   char buf[4];
 *
 *   drv.Parity = 0;
 *   drv.Baud = 9600;
 *   drv.StopBits = 1;
 *   drv.WordLength = 8;
 *
 *   ret = usb_host_drv_init(USB_CLASS_COMM, &drv);
 *   if (ret != WM_SUCCESS)
 *	return;
 *
 *   usb_dev = usb_host_drv_open("MDEV_USB_HOST");
 *   if (usb_dev == NULL)
 *	return;
 *
 *   ret = usb_host_drv_start(usb_dev);
 *   if (ret != WM_SUCCESS)
 *	return;
 *
 *   while((usb_host_drv_read(usb_dev, buf, sizeof(buf))) <= 0);
 *   ret = usb_host_drv_write(usb_dev, buf, sizeof(buf));
 *   if (ret < 0)
 *	return;
 *   usb_host_drv_stop(usb_dev);
 *   usb_host_drv_close(usb_dev);
 *  }
 *  @endcode
 *
 */

#ifndef _MDEV_USB_HOST_H_
#define _MDEV_USB_HOST_H_

#include <mdev.h>
#include <wmerrno.h>
#include <usbvideo.h>
#include <usbaudio.h>
#include <usbcdc.h>
#include <usbclass.h>

#define USB_HOST_LOG(...)  wmprintf("[usb-host] " __VA_ARGS__)

/** enum indicates usb  host class type */
typedef enum {
	/** USB Video Specification (UVC) Class */
	USB_CLASS_VIDEO,
	/** USB Communication Specification (CDC) Class */
	USB_CLASS_COMM,
	/** USB Audio Specification (UAC) Class */
	USB_CLASS_AUDIO
} usb_class_host;

/** Enable/Disable debug logs for USB UVC Class Driver */
#define USB_UVC_DEBUG 1
/** Enable/Disable debug logs for USB CDC Class Driver */
#define USB_CDC_DEBUG 1

/** Initialize USB Host Driver
 *
 * This function registers the USB Host driver with the mdev interface and
 * initializes the USB device with the provided operational class.
 *
 * \param [in] class_t usb class for which device should be used
 * e.g \ref USB_CLASS_VIDEO or \ref USB_CLASS_COMM
 * \param [in] data USB Host class specific data pointer
 * \return WM_SUCCESS on success
 * \return -WM_FAIL on failure
 * \note Video class driver currently supports  STREAM_MJPEG and
 * STREAM_UNCOMPRESSED video formats only
 */
int usb_host_drv_init(usb_class_host class_t, void *data);

/** Open USB device
 *
 * This returns a handle that should be used for other USB Host driver calls.
 *
 * \param [in] name Name of mdev usb driver. It should be "MDEV_USB_HOST"
 * string.
 * \return handle to driver on success
 * \return NULL otherwise
 */
mdev_t *usb_host_drv_open(const char *name);

/** Start USB Host EHCI Controller
 *
 * This function enables (power on) USB Host EHCI Controller.
 *
 * In addition specific USB Class driver can do its own implementation,
 * e.g. for video class this function will configure USB video device with
 * configuration provided during init phase and starts video streaming.
 *
 * \param [in] dev mdev handle returned from call to usb_drv_open()
 *
 * \return WM_SUCCESS on success
 * \return error code on failure
 */
int usb_host_drv_start(mdev_t *dev);

/** Stop USB Host EHCI Controller
 *
 * This function disables (power off) USB Host EHCI Controller.
 *
 * In addition specific USB Class driver can do its own implementation,
 * e.g. for video class this function will stop video streaming from end
 * device, putting interface into zero bandwidth mode.
 *
 * \param [in] dev mdev handle returned from call to usb_drv_open()
 *
 * \return WM_SUCCESS on success
 * \return error code on failure
 */
int usb_host_drv_stop(mdev_t *dev);

/** Read from USB device
 *
 * This function reads the specified number of bytes into the provided buffer
 * from the USB Host device. This call is NON-BLOCKING one.
 *
 * \param [in] dev mdev_t handle to the driver
 * \param [out] buf Buffer for storing the values read
 * \param [in] nbytes Number of bytes read out, 0 if data is not yet available,
 * -1 on buffer overflow error
 * \return number of bytes read
 * \note For USB Video Class, video data will start from frame boundary at
 * first read or after a buffer overflow exception has occurred.
 */
int usb_host_drv_read(mdev_t *dev, void *buf, uint32_t nbytes);

/** Write to USB device
 *
 * This function writes the specified number of bytes to the USB Host device
 * \param[in] dev mdev_t handle to the driver
 * \param[in] buf Pointer to buffer to be written
 * \param[in] nbytes Number for bytes to be written
 * \return number of bytes written on success
 * \return -1 on failure
 */
int usb_host_drv_write(mdev_t *dev, const void *buf, uint32_t nbytes);

/** Close USB Device
 *
 * This function closes the device after use.
 *
 * \param [in] dev mdev handle to the driver to be closed
 * \return WM_SUCCESS on success
 */
int usb_host_drv_close(mdev_t *dev);

#endif
