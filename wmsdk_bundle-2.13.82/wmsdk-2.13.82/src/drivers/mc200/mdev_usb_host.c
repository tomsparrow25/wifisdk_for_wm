/*
 *  Copyright 2008-2013, Marvell International Ltd.
 *  All Rights Reserved.
 */

#include <wmstdio.h>
#include <mdev_usb_host.h>
#include <wm_os.h>
#include <cli.h>

static mdev_t mdev_usb_host;
static const char *MDEV_NAME_usb_host = "MDEV_USB_HOST";
static os_mutex_t usb_host_mutex;

typedef struct class_drv_funcs {
	int (*drv_read) (uint8_t *buf, uint32_t count);
	int (*drv_write) (uint8_t *buf, uint32_t count);
	int (*drv_start) ();
	int (*drv_stop) ();
} class_drv_funcs_t;

static class_drv_funcs_t cl_funcs;

extern uvc_drv_stats_t stats;

void usb_uvc_get_stats(int argc, char **argv)
{
	wmprintf("usb uvc statistics =========:\r\n");
	wmprintf("total rx bytes:\t\t %llu\r\n", stats.rx_bytes);
	wmprintf("no. of frames:\t\t %u\r\n", stats.sof_cnt);
#define MB (1 << 10 << 10)
	wmprintf("throughput (~):\t\t %u.%u Mbps\r\n",
				stats.bps / MB, stats.bps % MB);
	wmprintf("overflow count:\t\t %u\r\n", stats.overflow_err_cnt);
	wmprintf("rx error count:\t\t %u\r\n", stats.rx_err_cnt);
}

void usb_uvc_dump_descriptors(int argc, char **argv)
{
	usb_uvc_print_descriptors();
}

struct cli_command uvc_command[] = {
	{"uvc_stats", NULL, usb_uvc_get_stats},
	{"uvc_dump_desc", NULL, usb_uvc_dump_descriptors},
};

int usb_host_drv_start(mdev_t *dev)
{
	int ret;

	ret = os_mutex_get(&usb_host_mutex, OS_WAIT_FOREVER);
	if (ret == -WM_FAIL) {
		USB_HOST_LOG("failed to get mutex\n\r");
		return ret;
	}

	RTURegisterEHCI((void *)USB_BASE, USB_IRQn);

	if (((class_drv_funcs_t *)(dev->private_data))->drv_start)
		ret = ((class_drv_funcs_t *)(dev->private_data))->
							drv_start();
	os_mutex_put(&usb_host_mutex);
	return ret;
}

int usb_host_drv_stop(mdev_t *dev)
{
	int ret;

	ret = os_mutex_get(&usb_host_mutex, OS_WAIT_FOREVER);
	if (ret == -WM_FAIL) {
		USB_HOST_LOG("failed to get mutex\n\r");
		return ret;
	}
	if (((class_drv_funcs_t *)(dev->private_data))->drv_stop)
		ret = ((class_drv_funcs_t *)(dev->private_data))->
							drv_stop();
	RTUShutDown();

	os_mutex_put(&usb_host_mutex);
	return ret;
}

int usb_host_drv_write(mdev_t *dev, const void *buf, uint32_t nbytes)
{
	int ret, bytes_written = -WM_FAIL;

	ret = os_mutex_get(&usb_host_mutex, OS_WAIT_FOREVER);
	if (ret == -WM_FAIL) {
		USB_HOST_LOG("failed to get mutex\n\r");
		return ret;
	}

	if (((class_drv_funcs_t *)(dev->private_data))->drv_write)
		bytes_written = ((class_drv_funcs_t *)(dev->private_data))->
			drv_write((void *)buf, nbytes);
	os_mutex_put(&usb_host_mutex);
	return bytes_written;
}

int usb_host_drv_read(mdev_t *dev, void *buf, uint32_t nbytes)
{
	int ret, bytes_read = -WM_FAIL;

	ret = os_mutex_get(&usb_host_mutex, OS_WAIT_FOREVER);
	if (ret == -WM_FAIL) {
		USB_HOST_LOG("failed to get mutex\n\r");
		return ret;
	}

	if (((class_drv_funcs_t *)(dev->private_data))->drv_read)
		bytes_read = ((class_drv_funcs_t *)(dev->private_data))->
						drv_read(buf, nbytes);
	os_mutex_put(&usb_host_mutex);
	return bytes_read;
}

int usb_host_drv_close(mdev_t *dev)
{
	return WM_SUCCESS;
}

mdev_t *usb_host_drv_open(const char *name)
{
	mdev_t *mdev_p = mdev_get_handle(name);

	if (mdev_p == NULL) {
		USB_HOST_LOG("driver open called without registering device"
							" (%s)\n\r", name);
		return NULL;
	}
	return mdev_p;
}

int usb_host_drv_init(usb_class_host class_t, void *data)
{
	int ret, i;

	if (mdev_get_handle(MDEV_NAME_usb_host) != NULL)
		return WM_SUCCESS;

	mdev_usb_host.name = MDEV_NAME_usb_host;

	ret = os_mutex_create(&usb_host_mutex, "usb-host", OS_MUTEX_INHERIT);
	if (ret == -WM_FAIL) {
		USB_HOST_LOG("Err: Failed to create mutex\r\n");
		return -WM_FAIL;
	}

	USB_HwInit();
	os_thread_sleep(1000);

	if (class_t == USB_CLASS_VIDEO) {
		/* Every class driver is supposed to register
		 * read/write/start/stop calls with cl_funcs here.
		 */
		cl_funcs.drv_read = usb_uvc_drv_read;
		cl_funcs.drv_write = NULL;
		cl_funcs.drv_start = usb_uvc_drv_start;
		cl_funcs.drv_stop = usb_uvc_drv_stop;
		mdev_usb_host.private_data = (uint32_t) &cl_funcs;

		ret = usb_uvc_drv_init(data);
		if (ret < 0) {
			USB_HOST_LOG
				("Err: Failed to initialize uvc driver\r\n");
			os_mutex_delete(&usb_host_mutex);
			return -WM_FAIL;
		}
		RTURegisterCallback(USBVideo);

	} else if (class_t == USB_CLASS_COMM) {
		/* Every class driver is supposed to register
		 * read/write/start/stop calls with cl_funcs here.
		 */
		cl_funcs.drv_read = usb_cdc_drv_read;
		cl_funcs.drv_write = usb_cdc_drv_write;
		cl_funcs.drv_start = usb_cdc_drv_start;
		cl_funcs.drv_stop = usb_cdc_drv_stop;
		mdev_usb_host.private_data = (uint32_t) &cl_funcs;

		ret = usb_cdc_drv_init(data);
		if (ret < 0) {
			USB_HOST_LOG
				("Err: Failed to initialize cdc driver\r\n");
			os_mutex_delete(&usb_host_mutex);
			return -WM_FAIL;
		}
		RTURegisterCallback(USBComm);
	} else if (class_t == USB_CLASS_AUDIO) {
		/* Every class driver is supposed to register
		 * read/write/start/stop calls with cl_funcs here.
		 */
		cl_funcs.drv_read = usb_uac_drv_read;
		cl_funcs.drv_write = NULL;
		cl_funcs.drv_start = NULL;
		cl_funcs.drv_stop = NULL;
		mdev_usb_host.private_data = (uint32_t) &cl_funcs;

		ret = usb_uac_drv_init(data);
		if (ret < 0) {
			USB_HOST_LOG
				("Err: Failed to initialize uac driver\r\n");
			os_mutex_delete(&usb_host_mutex);
			return -WM_FAIL;
		}
		RTURegisterCallback(USBAudio);

	}  else {
		USB_HOST_LOG("Err: Driver do not support %d class\r\n",
								class_t);
		os_mutex_delete(&usb_host_mutex);
		return -WM_FAIL;
	}

	if (class_t == USB_CLASS_VIDEO) {
		for (i = 0; i < sizeof(uvc_command) /
				sizeof(struct cli_command); i++)
			cli_register_command(&uvc_command[i]);
	}

	mdev_register(&mdev_usb_host);

	return WM_SUCCESS;
}
