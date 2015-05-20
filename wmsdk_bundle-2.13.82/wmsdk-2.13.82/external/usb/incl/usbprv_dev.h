/***********************************************************
* Portions (c) Copyright 2008 Marvell International Ltd.
*
*               Marvell Confidential
* ==========================================================
*/

#ifndef __usbprv_dev_h__
#define __usbprv_dev_h__
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
** $Revision: #4 $
*******************************************************************************
*** Description:
***  This file contains the private defines, externs and
***  data structure definitions required by the VUSB_HS Device
***  driver.
***
**************************************************************************
**END*********************************************************/

#include <stdint.h>
#include "vusbhsdev.h"

#define  MAX_EP_TR_DESCRS                    (32)
#define  MAX_XDS_FOR_TR_CALLS                (32)
#define  MAX_USB_DEVICES                     (1)
#define  USB_MAX_ENDPOINTS                   (16)

/*Assumption here is that all control endpoints are sequential 0,1,..
if they are not you need to modify the tr_complete routine to handle that */
#define  USB_MAX_CONTROL_ENDPOINTS           (1)

#define  USB_MAX_CTRL_PAYLOAD                (64)

/* Macro for aligning the EP queue head to 32 byte boundary */
#define USB_MEM32_ALIGN(n)                   ((n) + (-(n) & 31))

/* Macro for aligning the EP queue head to 1024 byte boundary */
#define USB_MEM1024_ALIGN(n)                 ((n) + (-(n) & 1023))

/* Macro for aligning the EP queue head to 2048 byte boundary */
#define USB_MEM2048_ALIGN(n)                 ((n) + (-(n) & 2047))

#define USB_XD_QADD(head, tail, XD)      \
	if ((head) == NULL) {         \
		(head) = (XD);            \
	} else {                      \
		(tail)->SCRATCH_PTR->PRIVATE = (XD);   \
	} /* Endif */                 \
	(tail) = (XD);               \
	(XD)->SCRATCH_PTR->PRIVATE = NULL

#define USB_XD_QGET(head, tail, XD)      \
	(XD) = (head);               \
	if (head) {                   \
		(head) = (XD_STRUCT_PTR)((head)->SCRATCH_PTR->PRIVATE);  \
		if ((head) == NULL) {      \
			(tail) = NULL;          \
		} /* Endif */              \
	}				/* Endif */

#define EHCI_DTD_QADD(head, tail, dTD)      \
	if ((head) == NULL) {         \
		(head) = (dTD);            \
	} else {                      \
	   (tail)->SCRATCH_PTR->PRIVATE = (void *) (dTD);   \
	} /* Endif */                 \
	(tail) = (dTD);               \
	(dTD)->SCRATCH_PTR->PRIVATE = NULL

#define EHCI_DTD_QGET(head, tail, dTD)      \
	(dTD) = (head);               \
	if (head) {                   \
		(head) = (head)->SCRATCH_PTR->PRIVATE;  \
		if ((head) == NULL) {      \
			(tail) = NULL;          \
		} /* Endif */              \
	}				/* Endif */

/***************************************
**
** Data structures
**
*/

/* Callback function storage structure */
typedef struct service_struct {
	uint8_t TYPE;
	void (*SERVICE) (void *, bool, uint8_t, uint8_t *, uint32_t, uint8_t,
			 uint8_t);
	struct service_struct *NEXT;
} SERVICE_STRUCT, *SERVICE_STRUCT_PTR;

typedef struct xd_struct {
	uint8_t EP_NUM;		/* Endpoint number */
	uint8_t BDIRECTION;	/* Direction : Send/Receive */
	uint8_t EP_TYPE;		/* Type of the endpoint: Ctrl, Isoch, Bulk,
				 ** Int
				 */
	uint8_t BSTATUS;		/* Current transfer status */
	uint8_t *WSTARTADDRESS;	/* Address of first byte */
	uint32_t WTOTALLENGTH;	/* Number of bytes to send/recv */
	uint32_t WSOFAR;		/* Number of bytes recv'd so far */
	uint16_t WMAXPACKETSIZE;	/* Max Packet size */
	bool DONT_ZERO_TERMINATE;
	uint8_t MAX_PKTS_PER_UFRAME;
	SCRATCH_STRUCT_PTR SCRATCH_PTR;
} XD_STRUCT, *XD_STRUCT_PTR;

/* The USB Device State Structure */
typedef struct {
	bool BUS_RESETTING;	/* Device is
				 ** being reset
				 */
	bool TRANSFER_PENDING;	/* Transfer pending ? */

	VUSB20_REG_STRUCT_PTR DEV_PTR;	/* Device Controller
					 ** Register base
					 ** address
					 */

	void *CALLBACK_STRUCT_PTR;

	SERVICE_STRUCT_PTR SERVICE_HEAD_PTR;	/* Head struct
						 ** address of
						 ** registered services
						 */
	XD_STRUCT_PTR TEMP_XD_PTR;	/* Temp xd for ep init */
	XD_STRUCT_PTR XD_BASE;
	XD_STRUCT_PTR XD_HEAD;	/* Head Transaction
				 ** descriptors
				 */
	XD_STRUCT_PTR XD_TAIL;	/* Tail Transaction
				 ** descriptors
				 */
	XD_STRUCT_PTR PENDING_XD_PTR;	/* pending transfer */
	uint32_t XD_ENTRIES;
	VUSB20_EP_QUEUE_HEAD_STRUCT_PTR EP_QUEUE_HEAD_PTR;	/* Endpoint Queue
								 ** head
								 */
	uint8_t *DRIVER_MEMORY;
	VUSB20_EP_QUEUE_HEAD_STRUCT_PTR EP_QUEUE_HEAD_BASE;
	VUSB20_EP_TR_STRUCT_PTR DTD_BASE_PTR;	/* Device transfer
						 ** descriptor pool
						 ** address
						 */
	VUSB20_EP_TR_STRUCT_PTR DTD_ALIGNED_BASE_PTR;	/* Aligned transfer
							 ** descriptor pool
							 ** address
							 */
	VUSB20_EP_TR_STRUCT_PTR DTD_HEAD;
	VUSB20_EP_TR_STRUCT_PTR DTD_TAIL;
	VUSB20_EP_TR_STRUCT_PTR EP_DTD_HEADS[USB_MAX_ENDPOINTS * 2];
	VUSB20_EP_TR_STRUCT_PTR EP_DTD_TAILS[USB_MAX_ENDPOINTS * 2];
	SCRATCH_STRUCT_PTR XD_SCRATCH_STRUCT_BASE;

	SCRATCH_STRUCT_PTR SCRATCH_STRUCT_BASE;

	/* These fields are kept only for USB_shutdown() */
	//void(_CODE_PTR_                  OLDISR_PTR)(void *);
	void (*OLDISR_PTR) (void *);
	void *OLDISR_DATA;
	uint16_t USB_STATE;
	uint16_t USB_DEVICE_STATE;
	uint16_t USB_SOF_COUNT;
	uint16_t DTD_ENTRIES;
	uint16_t ERRORS;
	uint16_t USB_DEV_STATE_B4_SUSPEND;
	uint8_t DEV_NUM;		/* USB device number
				 ** on the board
				 */
	uint8_t DEV_VEC;		/* Interrupt vector
				 ** number for USB
				 */
	uint8_t SPEED;		/* Low Speed,
				 ** High Speed,
				 ** Full Speed
				 */
	uint8_t MAX_ENDPOINTS;	/* Max endpoints
				 ** supported by this
				 ** device
				 */

	uint8_t USB_CURR_CONFIG;
	uint8_t DEVICE_ADDRESS;
} USB_DEV_STATE_STRUCT, *USB_DEV_STATE_STRUCT_PTR;

/***************************************
**
** Prototypes
**
*/
#ifdef __cplusplus
extern "C" {
#endif
	extern void _usb_dci_vusb20_isr(uint32_t int_num);
	extern uint8_t _usb_dci_vusb20_init(uint8_t, _usb_device_handle);
	extern void _usb_device_free_XD(void *);
	extern void _usb_dci_vusb20_free_dTD(void *);
	extern uint8_t _usb_dci_vusb20_add_dTD(_usb_device_handle, XD_STRUCT_PTR);
	extern uint8_t _usb_dci_vusb20_send_data(_usb_device_handle,
					XD_STRUCT_PTR);
	extern uint8_t _usb_dci_vusb20_recv_data(_usb_device_handle,
					XD_STRUCT_PTR);
	extern uint32_t _usb_dci_vusb20_cancel_transfer(_usb_device_handle, uint8_t,
						uint8_t);
	extern uint8_t _usb_dci_vusb20_get_transfer_status(_usb_device_handle,
							 uint8_t, uint8_t);
	extern XD_STRUCT_PTR
	_usb_dci_vusb20_get_transfer_details(_usb_device_handle, uint8_t,
						 uint8_t);
	extern uint8_t _bsp_get_usb_vector(uint8_t, uint8_t);
	extern void *_bsp_get_usb_base(uint8_t, uint8_t);
	extern void *_bsp_get_usb_capability_register_base(uint8_t, uint8_t);
	extern void _usb_dci_vusb20_process_tr_complete(_usb_device_handle);
	extern void _usb_dci_vusb20_process_reset(_usb_device_handle);
	extern void _usb_dci_vusb20_process_tr_complete(_usb_device_handle);
	extern void _usb_dci_vusb20_process_suspend(_usb_device_handle);
	extern void _usb_dci_vusb20_process_SOF(_usb_device_handle);
	extern void _usb_dci_vusb20_process_port_change(_usb_device_handle);
	extern void _usb_dci_vusb20_process_error(_usb_device_handle);
	extern void _usb_dci_vusb20_shutdown(_usb_device_handle);
	extern void _usb_dci_vusb20_set_speed_full(_usb_device_handle, uint8_t);
	extern void _usb_dci_vusb20_clear_speed_full(_usb_device_handle, uint8_t);
	extern void _usb_dci_vusb20_suspend_phy(_usb_device_handle, uint8_t);
	extern void _usb_dci_vusb20_hnp_shutdown(void);
	extern void _usb_dci_vusb20_set_address(_usb_device_handle, uint8_t);
	extern void _usb_dci_vusb20_get_setup_data(_usb_device_handle,
						uint8_t, uint8_t *);
	extern void _usb_dci_vusb20_assert_resume(_usb_device_handle);
	extern uint8_t _usb_dci_vusb20_init_endpoint(_usb_device_handle,
						XD_STRUCT_PTR);
	extern void _usb_dci_vusb20_stall_endpoint(_usb_device_handle, uint8_t,
						uint8_t);
	extern void _usb_dci_vusb20_unstall_endpoint(_usb_device_handle, uint8_t,
						uint8_t);
	extern uint8_t _usb_dci_vusb20_deinit_endpoint(_usb_device_handle, uint8_t,
						uint8_t);
	extern void _usb_dci_vusb20_set_endpoint_status(_usb_device_handle,
							uint8_t, uint8_t);
	extern void _usb_dci_vusb20_set_test_mode(_usb_device_handle, uint16_t);
	extern uint8_t _usb_dci_vusb20_get_endpoint_status(_usb_device_handle,
							uint8_t);
	extern void _usb_dci_vusb20_reset_data_toggle(_usb_device_handle, uint8_t,
						uint8_t);
	extern void _usb_dci_vusb20_chip_initialize(_usb_device_handle);
	void usb_dci_vusb20_change_zlp(_usb_device_handle handle, uint32_t EPNum,
				bool zlpflag);

#ifdef __cplusplus
}
#endif
#endif
