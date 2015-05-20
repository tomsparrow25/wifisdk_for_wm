
/* *********************************************************
* (c) Copyright 2008-2014 Marvell International Ltd.
*
*               Marvell Confidential
* ==========================================================
 */
/**
 * \brief This is the usb interface file init function
*/

#include <string.h>
#include <wmstdio.h>
#include <wmerrno.h>
#include <incl/usbsysinit.h>
#include <incl/usb_device_api.h>
#include <incl/usb_hw.h>
#include <incl/usb_cdc.h>
#include <incl/usb_hid.h>
#include <incl/usbdrv.h>

char UsbMfgStr[] = "MARVELL";
char *UsbProdStr[] = {"WRONG PRODUCT","MARVELL MC200 VCOM ","MARVELL MC200 HID ","WRONG PRODUCT","MARVELL MC200 MSC "};
char *UsbSerNum[] = {"WRONG SerNum ","DEMO00000020","DEMO00000030 ","WRONG SerNum ","DEMO00000040"};


void MS_manager_thread();

/**
 * \brief Initialize the usb interface.
 * \param Interfaces A bit mask that specifies the interfaces to initilize.
 */

static uint32_t usb_device_init(uint32_t Interfaces)
{
	usb_sys_config_t usbConfig;

	usbConfig.USBVendorID = 0x1286;
	usbConfig.USBProductID = 0x8080;
	usbConfig.USBMfgStr = UsbMfgStr;

	usbConfig.USBProdStr = UsbProdStr[Interfaces];
	usbConfig.USBFormatterSerNum = UsbSerNum[Interfaces];
	usbConfig.USBForceFullSpeed = 0;
	usbConfig.USBLang = 0x0409;

	/* we have the configuration, init the hw and configure everything. */
	if (usb_dev_init() == WM_SUCCESS) {
		if (usb_dev_set_config(&usbConfig) == WM_SUCCESS) {
			/* init the interfaces.
			 * The order of this init is the same order that the interfaces will be
			 * reported to the host.  To change the order, change the order of the
			 * inits. */

			if (Interfaces & USB_CDC) {
				if (usb_cdcdevice_init() != WM_SUCCESS) {
					usb_e
					    ("usb_cdcdevice_init Failed");
					return -WM_FAIL;
				}
			}

			if (Interfaces & USB_HID) {
				if (usb_hiddevice_init() != WM_SUCCESS) {
					usb_e
					    ("usb_cdcdevice_init Failed");
					return -WM_FAIL;
				}
			}

            		if (Interfaces & USB_MSC) {
                		if (usb_mscdevice_init() != OK) {
                    			usb_e("usb_device_init usb_mscdevice_init failure\n\r");
                		}

				MS_manager_thread();
            		}

			/* Now finish the init. */
			if (usb_dev_start() != WM_SUCCESS) {
				usb_e
				    ("usb_dev_start Failed");
				return -WM_FAIL;
			}
		} else {
			usb_e
			    ("usb_dev_set_config Failed");
			return -WM_FAIL;
		}
	} else {
		usb_e("usb_dev_init Failed");
		return -WM_FAIL;
	}

	return WM_SUCCESS;
}

int32_t usb_device_system_init(uint32_t Interfaces)
{
    USB_HwInit();

    return usb_device_init(Interfaces);
}

