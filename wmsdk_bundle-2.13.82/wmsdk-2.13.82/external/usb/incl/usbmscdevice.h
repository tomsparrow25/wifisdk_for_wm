/* 
 *
 * ============================================================================
 * Copyright (c) 2008-2011   Marvell Semiconductor, Inc. All Rights Reserved
 *
 *                         Marvell Confidential
 * ============================================================================
 *
 */

#ifndef USB_MSC_DEVICE_H
#define USB_MSC_DEVICE_H
#include "error_types.h"
#include "usb_device_api.h"

error_type_t usb_mscdevice_init(void);

void set_number_of_LUNs(uint32_t new_num_LUNs);
uint32_t get_number_of_LUNs();
int32_t usb_MS_device_read(uint8_t *buffer, uint32_t len, uint32_t timeout);
int32_t usb_MS_device_write(uint8_t *buffer, uint32_t len, uint32_t time_out);
int32_t usb_MS_device_ioctl(ioctl_cmd_t cmd, void *data_for_ioctl);

#endif
