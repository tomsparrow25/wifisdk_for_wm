/*
 *
 * ============================================================================
 * Portions Copyright (c) 2008-2014 Marvell International Ltd. All Rights Reserved.
 *
 *                         Marvell Confidential
 * ============================================================================
 */
/**
 *
 * \file asic_usb.h
 * \brief This file contains product-specific configuration data
 * to be used to configure the USB Device driver for a specific product
 *
 */

#ifndef __asic_usb_h__
#define __asic_usb_h__
/*
 ********************************************************************************
 ** ChipIdea Microelectronica - IPCS
 ** TECMAIA, Rua Eng. Frederico Ulrich, n 2650
 ** 4470-920 MOREIRA MAIA
 ** Portugal
 ** Tel: +351 229471010
 ** Fax: +351 229471011
 ** e_mail: chipidea.com
 ********************************************************************************
 ** ISO 9001:2000 - Certified Company
 ** (C) 2005 Copyright Chipidea(R)
 ** Chipidea(R) - Microelectronica, S.A. reserves the right to make changes to
 ** the information contained herein without notice. No liability shall be
 ** incurred as a result of its use or application.
 ********************************************************************************
 */

#include <stdint.h>
#include <mc200/mc200_interrupt.h>
#include <mc200/mc200.h>

#ifdef __cplusplus
extern "C" {
#endif

#define USB_Uncached			     		 volatile

/* Macro for aligning the EP queue head to 32 byte boundary */
#define USB_MEM32_ALIGN(n)                  ((n) + (-(n) & 31))
#define USB_CACHE_ALIGN(n)     		        USB_MEM32_ALIGN(n)

typedef uint32_t VUSB_REGISTER;

#define  BSP_VUSB20_DEVICE_BASE_ADDRESS0     (0x44001000)	/* base address */
#define  BSP_VUSB20_DEVICE_VECTOR0           (USB_IRQn)	/* interrupt number */
#define  BSP_VUSB20_DEVICE_CAPABILITY_OFFSET (0x100)
#define  NUM_OUT_EPS 6
#define  NUM_IN_EPS 6

#define TOMBSTONE_DEBUG(x)

#define TOMBSTONE_USB_DEVICE_DEBUG(x)

#define _disable_device_interrupts()    NVIC_DisableIRQ(USB_IRQn)
#define _enable_device_interrupts()     NVIC_EnableIRQ(USB_IRQn)

#define USB_uint_16_low(x)          ((x) & 0xFF)
#define USB_uint_16_high(x)         (((x) >> 8) & 0xFF)

#define USB_uint_16(x)              USB_uint_16_low(x), USB_uint_16_high(x)

/* Ignore SOF ints for now */
#define SOF_INTERRUPT_DISABLE
#define TRIP_WIRE
#define SET_ADDRESS_HARDWARE_ASSISTANCE
#define PATCH_3

#define TYPE_DEVICE	0
#define TYPE_OTG	2

#define HAVE_USB_DEVICE

#ifdef __cplusplus
}
#endif
#endif
