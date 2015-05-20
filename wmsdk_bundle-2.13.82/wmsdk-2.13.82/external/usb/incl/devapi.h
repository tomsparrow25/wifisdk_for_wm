/***********************************************************
* Portions (c) Copyright 2008 Marvell International Ltd.
*
*               Marvell Confidential
* ==========================================================
*/

#ifndef __devapi_h__
#define __devapi_h__
/*******************************************************************************
** File          : $HeadURL$
** Author        : $Author: prolig $
** Project       : HSCTRL
** Instances     :
** Creation date :
********************************************************************************
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
** Modification history:
** $Date: 2007/04/13 $
** $Revision: #2 $
*******************************************************************************
*** Description:
***  This file contains the declarations specific to the USB Device API
***
**************************************************************************
**END*********************************************************/
#include <stdint.h>
#include <stdbool.h>
#include "typesdev.h"
#include "usbdev.h"
#include "usbprvdev.h"
#include "usb_debugdev.h"

#define  USB_SEND                         (1)
#define  USB_RECV                         (0)

/* Endpoint types */
#define  USB_CONTROL_ENDPOINT             (0)
#define  USB_ISOCHRONOUS_ENDPOINT         (1)
#define  USB_BULK_ENDPOINT                (2)
#define  USB_INTERRUPT_ENDPOINT           (3)

/* Informational Request/Set Types */
#define  USB_STATUS_DEVICE_STATE          (0x01)
#define  USB_STATUS_INTERFACE             (0x02)
#define  USB_STATUS_ADDRESS               (0x03)
#define  USB_STATUS_CURRENT_CONFIG        (0x04)
#define  USB_STATUS_SOF_COUNT             (0x05)
#define  USB_STATUS_DEVICE                (0x06)
#define  USB_STATUS_TEST_MODE             (0x07)
#define  USB_FORCE_FULL_SPEED             (0x08)
#define  USB_PHY_LOW_POWER_SUSPEND        (0x09)
#define  USB_CLEAR_FULL_SPEED             (0x0A)
#define  USB_STATUS_ENDPOINT              (0x10)
#define  USB_STATUS_ENDPOINT_NUMBER_MASK  (0x8F)

#define  USB_TEST_MODE_TEST_PACKET        (0x0400)

/* Available service types */
/* Services 0 through 15 are reserved for endpoints */
#define  USB_SERVICE_EP0                  (0x00)
#define  USB_SERVICE_EP1                  (0x01)
#define  USB_SERVICE_EP2                  (0x02)
#define  USB_SERVICE_EP3                  (0x03)
#define  USB_SERVICE_BUS_RESET            (0x10)
#define  USB_SERVICE_SUSPEND              (0x11)
#define  USB_SERVICE_SOF                  (0x12)
#define  USB_SERVICE_RESUME               (0x13)
#define  USB_SERVICE_SLEEP                (0x14)
#define  USB_SERVICE_SPEED_DETECTION      (0x15)
#define  USB_SERVICE_ERROR                (0x16)
#define  USB_SERVICE_STALL                (0x17)

#undef TRUE
#define TRUE 1
#undef FALSE
#define FALSE 0

/* haitao 03/19/12 */
#define CPU_LITTLE_ENDIAN

typedef void *_usb_device_handle;

#ifdef __cplusplus
extern "C" {
#endif

	extern uint8_t _usb_device_init(uint8_t, _usb_device_handle *, uint8_t);
	extern uint8_t _usb_device_recv_data(_usb_device_handle, uint8_t, uint8_t *,
					   uint32_t);
	extern uint8_t _usb_device_send_data(_usb_device_handle, uint8_t, uint8_t *,
					   uint32_t);
	extern uint8_t _usb_device_get_transfer_status(_usb_device_handle, uint8_t,
						     uint8_t);
	extern void _usb_device_get_transfer_details(_usb_device_handle, uint8_t,
						     uint8_t, uint32_t * *);
	extern uint32_t _usb_device_cancel_transfer(_usb_device_handle, uint8_t,
						  uint8_t);
	extern uint8_t _usb_device_get_status(_usb_device_handle, uint8_t,
					    uint16_t *);
	extern uint8_t _usb_device_set_status(_usb_device_handle, uint8_t, uint16_t);
	extern uint8_t _usb_device_register_service(_usb_device_handle, uint8_t,
						  void (*service) (void *, bool,
								   uint8_t,
								   uint8_t *,
								   uint32_t,
								   uint8_t,
								   uint8_t));

	extern uint8_t _usb_device_unregister_service(_usb_device_handle, uint8_t);
	extern uint8_t _usb_device_call_service(_usb_device_handle, uint8_t, bool,
					      bool, uint8_t *, uint32_t, uint8_t);
	extern void _usb_device_shutdown(_usb_device_handle);
	extern void _usb_device_set_address(_usb_device_handle, uint8_t);
	extern void _usb_device_read_setup_data(_usb_device_handle, uint8_t,
						uint8_t *);
	extern void _usb_device_assert_resume(_usb_device_handle);
	extern uint8_t _usb_device_init_endpoint(_usb_device_handle, uint8_t,
					       uint16_t, uint8_t, uint8_t, uint8_t);
	extern void _usb_device_stall_endpoint(_usb_device_handle, uint8_t,
					       uint8_t);
	extern void _usb_device_unstall_endpoint(_usb_device_handle, uint8_t,
						 uint8_t);
	extern uint8_t _usb_device_deinit_endpoint(_usb_device_handle, uint8_t,
						 uint8_t);

	void usb_change_hw_zlp(_usb_device_handle handle, uint32_t EPNum,
			       uint8_t zlpflag);
#ifdef __cplusplus
}
#endif
#endif
/* EOF */
