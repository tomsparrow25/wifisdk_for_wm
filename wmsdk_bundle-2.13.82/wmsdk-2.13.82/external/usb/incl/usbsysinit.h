
/*
 * ============================================================================
 * Copyright (c) 2008-2014   Marvell International, Ltd. All Rights Reserved
 *
 *                         Marvell Confidential
 * ============================================================================
*/

#ifndef usbsysinit_h
#define usbsysinit_h

#include <stdint.h>
#include <usb_port.h>

#ifdef __cplusplus
extern "C" {
#endif

#define USB_CDC          0x01
#define USB_HID          0x02
#define USB_MSC          0x04

int32_t usb_device_system_init(uint32_t Interfaces);
error_type_t usb_mscdevice_init(void);

#ifdef __cplusplus
}
#endif
#endif
