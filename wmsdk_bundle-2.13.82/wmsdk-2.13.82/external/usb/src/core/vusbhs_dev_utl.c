/***********************************************************
* Portions (c) Copyright 2008 Marvell International Ltd.
*
*               Marvell Confidential
* ==========================================================
*/

/*******************************************************************************
** File          : $HeadURL$
** Author        : $Author: rasbury $
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
** $Date: 2007/08/01 $
** $Revision: #3 $
*******************************************************************************
*** Description:
***  This file contains the VUSB_HS Device Controller interface functions.
***
**************************************************************************
**END*********************************************************/
#include <incl/devapi.h>
#include <incl/usbprv_dev.h>
#include <incl/asic_usb.h>

#define  USB_TEST_MODE_TEST_PACKET_LENGTH   (53)

/* Test packet for Test Mode : TEST_PACKET. USB 2.0 Specification section 7.1.20 */
uint8_t test_packet[USB_TEST_MODE_TEST_PACKET_LENGTH] = {
	/* Synch */
	/* DATA 0 PID */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA,
	0xAA, 0xEE, 0xEE, 0xEE, 0xEE, 0xEE, 0xEE, 0xEE,
	0xEE, 0xFE, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x7F, 0xBF, 0xDF,
	0xEF, 0xF7, 0xFB, 0xFD, 0xFC, 0x7E, 0xBF, 0xDF,
	0xEF, 0xF7, 0xFB, 0xFD, 0x7E
};

/*FUNCTION*-------------------------------------------------------------
*
*  Function Name  : _usb_dci_vusb20_assert_resume
*  Returned Value : None
*  Comments       :
*        Resume signalling for remote wakeup
*
*END*-----------------------------------------------------------------*/
void _usb_dci_vusb20_assert_resume(
					  /* [IN] the USB_dev_initialize state structure */
					  _usb_device_handle handle)
{				/* Body */
	USB_DEV_STATE_STRUCT_PTR usb_dev_ptr;
	VUSB20_REG_STRUCT_PTR dev_ptr;
	uint32_t temp;

#ifdef _DEVICE_DEBUG_
	DEBUG_LOG_TRACE("_usb_dci_vusb20_assert_resume");
#endif

	usb_dev_ptr = (USB_DEV_STATE_STRUCT_PTR) handle;
	dev_ptr = (VUSB20_REG_STRUCT_PTR) usb_dev_ptr->DEV_PTR;

	/* Assert the Resume signal */
	temp = dev_ptr->REGISTERS.OPERATIONAL_DEVICE_REGISTERS.PORTSCX[0];
	temp &= ~EHCI_PORTSCX_W1C_BITS;
	temp |= EHCI_PORTSCX_PORT_FORCE_RESUME;
	dev_ptr->REGISTERS.OPERATIONAL_DEVICE_REGISTERS.PORTSCX[0] = temp;

	/* Port change interrupt will be asserted at the end of resume
	 ** operation
	 */

#ifdef _DEVICE_DEBUG_
	DEBUG_LOG_TRACE("_usb_dci_vusb20_assert_resume, SUCCESSFUL");
#endif

}				/* EndBody */

/*FUNCTION*-------------------------------------------------------------
*
*  Function Name  : _usb_dci_vusb20_stall_endpoint
*  Returned Value : None
*  Comments       :
*        Stalls the specified endpoint
*
*END*-----------------------------------------------------------------*/
void _usb_dci_vusb20_stall_endpoint(
					   /* [IN] the USB_dev_initialize state structure */
					   _usb_device_handle handle,
					   /* [IN] the Endpoint number */
					   uint8_t ep_num,
					   /* [IN] direction */
					   uint8_t direction)
{				/* Body */
	USB_DEV_STATE_STRUCT_PTR usb_dev_ptr;
	VUSB20_REG_STRUCT_PTR dev_ptr;
	VUSB20_EP_QUEUE_HEAD_STRUCT *ep_queue_head_ptr;

#ifdef _DEVICE_DEBUG_
	DEBUG_LOG_TRACE("_usb_dci_vusb20_stall_endpoint");
#endif

	usb_dev_ptr = (USB_DEV_STATE_STRUCT_PTR) handle;
	dev_ptr = (VUSB20_REG_STRUCT_PTR) usb_dev_ptr->DEV_PTR;

	/* Get the endpoint queue head address */
	ep_queue_head_ptr = (VUSB20_EP_QUEUE_HEAD_STRUCT_PTR)
	    (dev_ptr->REGISTERS.OPERATIONAL_DEVICE_REGISTERS.EP_LIST_ADDR) +
	    2 * ep_num + direction;

	/* Stall the endpoint for Rx or Tx and set the endpoint type */
	if (ep_queue_head_ptr->MAX_PKT_LENGTH & VUSB_EP_QUEUE_HEAD_IOS) {
		/* This is a control endpoint so STALL both directions */
		dev_ptr->REGISTERS.
		    OPERATIONAL_DEVICE_REGISTERS.ENDPTCTRLX[ep_num] |=
		    (EHCI_EPCTRL_TX_EP_STALL | EHCI_EPCTRL_RX_EP_STALL);
	} else {
		dev_ptr->REGISTERS.
		    OPERATIONAL_DEVICE_REGISTERS.ENDPTCTRLX[ep_num] |=
		    (direction ? EHCI_EPCTRL_TX_EP_STALL :
		     EHCI_EPCTRL_RX_EP_STALL);
	}			/* Endif */

#ifdef _DEVICE_DEBUG_
	DEBUG_LOG_TRACE("_usb_dci_vusb20_stall_endpoint, SUCCESSFUL");
#endif

}				/* EndBody */

/*FUNCTION*-------------------------------------------------------------
*
*  Function Name  : _usb_dci_vusb20_unstall_endpoint
*  Returned Value : None
*  Comments       :
*        Unstall the specified endpoint in the specified direction
*
*END*-----------------------------------------------------------------*/
void _usb_dci_vusb20_unstall_endpoint(
					     /* [IN] the USB_dev_initialize state structure */
					     _usb_device_handle handle,
					     /* [IN] the Endpoint number */
					     uint8_t ep_num,
					     /* [IN] direction */
					     uint8_t direction)
{				/* Body */
	USB_DEV_STATE_STRUCT_PTR usb_dev_ptr;
	VUSB20_REG_STRUCT_PTR dev_ptr;
#ifdef _DEVICE_DEBUG_
	DEBUG_LOG_TRACE("_usb_dci_vusb20_unstall_endpoint");
#endif

	usb_dev_ptr = (USB_DEV_STATE_STRUCT_PTR) handle;
	dev_ptr = (VUSB20_REG_STRUCT_PTR) usb_dev_ptr->DEV_PTR;

	/* Enable the endpoint for Rx or Tx and set the endpoint type */
	dev_ptr->REGISTERS.OPERATIONAL_DEVICE_REGISTERS.ENDPTCTRLX[ep_num] &=
	    (direction ? ~EHCI_EPCTRL_TX_EP_STALL : ~EHCI_EPCTRL_RX_EP_STALL);

#ifdef _DEVICE_DEBUG_
	DEBUG_LOG_TRACE("_usb_dci_vusb20_unstall_endpoint, SUCCESSFUL");
#endif

}				/* EndBody */

/*FUNCTION*----------------------------------------------------------------
*
* Function Name  : _usb_dci_vusb20_get_endpoint_status
* Returned Value : None
* Comments       :
*     Gets the endpoint status
*
*END*--------------------------------------------------------------------*/
uint8_t _usb_dci_vusb20_get_endpoint_status(
						 /* [IN] Handle to the USB device */
						 _usb_device_handle handle,
						 /* [IN] Endpoint to get */
						 uint8_t ep)
{				/* Body */
	USB_DEV_STATE_STRUCT_PTR usb_dev_ptr;
	VUSB20_REG_STRUCT_PTR dev_ptr;
	uint8_t ep_num = ep & 0x0f;
	uint8_t ep_dir = ep & 0x80;

#ifdef _DEVICE_DEBUG_
	DEBUG_LOG_TRACE("_usb_dci_vusb20_get_endpoint_status");
#endif

	usb_dev_ptr = (USB_DEV_STATE_STRUCT_PTR) handle;
	dev_ptr = (VUSB20_REG_STRUCT_PTR) usb_dev_ptr->DEV_PTR;

#ifdef _DEVICE_DEBUG_
	DEBUG_LOG_TRACE("_usb_dci_vusb20_get_endpoint_status, SUCCESSFUL");
#endif

	return ((dev_ptr->REGISTERS.
		 OPERATIONAL_DEVICE_REGISTERS.ENDPTCTRLX[ep_num] & (ep_dir ?
								    EHCI_EPCTRL_TX_EP_STALL
								    :
								    EHCI_EPCTRL_RX_EP_STALL))
		? 1 : 0);

}				/* EndBody */

/*FUNCTION*----------------------------------------------------------------
*
* Function Name  : _usb_dci_vusb20_set_test_mode
* Returned Value : None
* Comments       :
*     sets/resets the test mode
*
*END*--------------------------------------------------------------------*/
void _usb_dci_vusb20_set_test_mode(
					  /* [IN] Handle to the USB device */
					  _usb_device_handle handle,
					  /* [IN] Test mode */
					  uint16_t test_mode)
{				/* Body */
	USB_DEV_STATE_STRUCT_PTR usb_dev_ptr;
	VUSB20_REG_STRUCT_PTR dev_ptr;
	uint32_t temp;

#ifdef _DEVICE_DEBUG_
	DEBUG_LOG_TRACE("_usb_dci_vusb20_set_test_mode");
#endif

	usb_dev_ptr = (USB_DEV_STATE_STRUCT_PTR) handle;
	dev_ptr = (VUSB20_REG_STRUCT_PTR) usb_dev_ptr->DEV_PTR;

	temp = dev_ptr->REGISTERS.OPERATIONAL_DEVICE_REGISTERS.ENDPTCTRLX[0];

	dev_ptr->REGISTERS.OPERATIONAL_DEVICE_REGISTERS.ENDPTCTRLX[0] =
	    (temp | EHCI_EPCTRL_TX_DATA_TOGGLE_RST);

	if (test_mode == USB_TEST_MODE_TEST_PACKET) {
		_usb_device_send_data(handle, 0, test_packet,
				      USB_TEST_MODE_TEST_PACKET_LENGTH);
	}
	/* Endif */
	temp = dev_ptr->REGISTERS.OPERATIONAL_DEVICE_REGISTERS.PORTSCX[0];
	temp &= ~EHCI_PORTSCX_W1C_BITS;

	dev_ptr->REGISTERS.OPERATIONAL_DEVICE_REGISTERS.PORTSCX[0] = (temp |
								      ((uint32_t)
								       test_mode
								       << 8));

#ifdef _DEVICE_DEBUG_
	DEBUG_LOG_TRACE("_usb_dci_vusb20_set_test_mode, SUCCESSFUL");
#endif

}				/* EndBody */

/*FUNCTION*----------------------------------------------------------------
*
* Function Name  : _usb_dci_vusb20_set_endpoint_status
* Returned Value : None
* Comments       :
*     Sets the endpoint registers e.g. to enable TX, RX, control
*
*END*--------------------------------------------------------------------*/
void _usb_dci_vusb20_set_endpoint_status(
						/* [IN] Handle to the USB device */
						_usb_device_handle handle,
						/* [IN] Endpoint to set */
						uint8_t ep,
						/* [IN] Endpoint characteristics */
						uint8_t stall)
{				/* Body */
	USB_DEV_STATE_STRUCT_PTR usb_dev_ptr;
	VUSB20_REG_STRUCT_PTR dev_ptr;
	uint8_t ep_num = ep & 0x0f;
	uint8_t ep_dir = ep & 0x80;

#ifdef _DEVICE_DEBUG_
	DEBUG_LOG_TRACE("_usb_dci_vusb20_set_endpoint_status");
#endif
	usb_dev_ptr = (USB_DEV_STATE_STRUCT_PTR) handle;
	dev_ptr = (VUSB20_REG_STRUCT_PTR) usb_dev_ptr->DEV_PTR;

	if (stall) {
		// Set Stall
		dev_ptr->REGISTERS.
		    OPERATIONAL_DEVICE_REGISTERS.ENDPTCTRLX[ep_num] |=
		    (ep_dir ? EHCI_EPCTRL_TX_EP_STALL :
		     EHCI_EPCTRL_RX_EP_STALL);
	} else {
		// Clear Stall
		dev_ptr->REGISTERS.
		    OPERATIONAL_DEVICE_REGISTERS.ENDPTCTRLX[ep_num] &=
		    (ep_dir ? ~EHCI_EPCTRL_TX_EP_STALL :
		     ~EHCI_EPCTRL_RX_EP_STALL);
		// Reset Data Toggle
		dev_ptr->REGISTERS.
		    OPERATIONAL_DEVICE_REGISTERS.ENDPTCTRLX[ep_num] |=
		    (ep_dir ? EHCI_EPCTRL_TX_DATA_TOGGLE_RST :
		     EHCI_EPCTRL_RX_DATA_TOGGLE_RST);
	}			/* Endif */
#ifdef _DEVICE_DEBUG_
	DEBUG_LOG_TRACE("_usb_dci_vusb20_set_endpoint_status, SUCCESSFUL");
#endif

}				/* EndBody */

/*FUNCTION*-------------------------------------------------------------
*
*  Function Name  : _usb_dci_vusb20_reset_data_toggle
*  Returned Value : None
*  Comments       :
*        Reset the data toggle for the specified endpoint in the
*  specified direction
*
*END*-----------------------------------------------------------------*/
void _usb_dci_vusb20_reset_data_toggle(
					      /* [IN] the USB_dev_initialize state structure */
					      _usb_device_handle handle,
					      /* [IN] the Endpoint number */
					      uint8_t ep_num,
					      /* [IN] direction */
					      uint8_t direction)
{				/* Body */
	USB_DEV_STATE_STRUCT_PTR usb_dev_ptr;
	VUSB20_REG_STRUCT_PTR dev_ptr;
	VUSB20_EP_QUEUE_HEAD_STRUCT *ep_queue_head_ptr;

#ifdef _DEVICE_DEBUG_
	DEBUG_LOG_TRACE("_usb_dci_vusb20_reset_data_toggle");
#endif

	usb_dev_ptr = (USB_DEV_STATE_STRUCT_PTR) handle;
	dev_ptr = (VUSB20_REG_STRUCT_PTR) usb_dev_ptr->DEV_PTR;

	/* Get the endpoint queue head address */
	ep_queue_head_ptr = (VUSB20_EP_QUEUE_HEAD_STRUCT_PTR)
	    (dev_ptr->REGISTERS.OPERATIONAL_DEVICE_REGISTERS.EP_LIST_ADDR) +
	    2 * ep_num + direction;

	/* Reset the data toggle for this endpoint. */
	if (ep_queue_head_ptr->MAX_PKT_LENGTH & VUSB_EP_QUEUE_HEAD_IOS) {
		/* This is a control endpoint so reset both directions */
		dev_ptr->REGISTERS.
		    OPERATIONAL_DEVICE_REGISTERS.ENDPTCTRLX[ep_num] |=
		    (EHCI_EPCTRL_TX_DATA_TOGGLE_RST |
		     EHCI_EPCTRL_RX_DATA_TOGGLE_RST);
	} else {
		dev_ptr->REGISTERS.
		    OPERATIONAL_DEVICE_REGISTERS.ENDPTCTRLX[ep_num] |=
		    (direction ? EHCI_EPCTRL_TX_DATA_TOGGLE_RST :
		     EHCI_EPCTRL_RX_DATA_TOGGLE_RST);
	}			/* Endif */

#ifdef _DEVICE_DEBUG_
	DEBUG_LOG_TRACE("_usb_dci_vusb20_reset_data_toggle, SUCCESSFUL");
#endif

}				/* EndBody */
