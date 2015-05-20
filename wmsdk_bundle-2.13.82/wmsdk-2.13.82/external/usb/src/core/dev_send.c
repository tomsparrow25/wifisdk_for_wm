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
** $Revision: #4 $
*******************************************************************************
*** Description:
***  This file contains USB device API specific function to send data.
***
**************************************************************************
**END*********************************************************/
#include <incl/devapi.h>
#include <incl/usbprv_dev.h>
#include <incl/asic_usb.h>

/*FUNCTION*-------------------------------------------------------------
*
*  Function Name  : _usb_device_send_data
*  Returned Value : USB_OK or error code
*  Comments       :
*        Sends data on a specified endpoint.
*
*END*-----------------------------------------------------------------*/
uint8_t _usb_device_send_data(
				   /* [IN] the USB_USB_dev_initialize state structure */
				   _usb_device_handle handle,
				   /* [IN] the Endpoint number */
				   uint8_t ep_num,
				   /* [IN] buffer to send */
				   uint8_t * buff_ptr,
				   /* [IN] length of the transfer */
				   uint32_t size)
{				/* Body */
	uint8_t error;
	XD_STRUCT_PTR xd_ptr;
	USB_DEV_STATE_STRUCT_PTR usb_dev_ptr;

	TOMBSTONE_USB_DEVICE_DEBUG(0x40200000);

#ifdef _DEVICE_DEBUG_
	DEBUG_LOG_TRACE("_usb_device_send_data");
#endif

	usb_dev_ptr = (USB_DEV_STATE_STRUCT_PTR) handle;

#ifdef ENABLE_DCACHE
   /********************************************************
   If system has a data cache, it is assumed that buffer
   passed to this routine will be aligned on a cache line
   boundry. The following code will flush the
   buffer before passing it to hardware driver.
   ********************************************************/
	USB_dcache_flush_mlines((void *)buff_ptr, size);
#endif

	USB_device_lock();

	if (!usb_dev_ptr->XD_ENTRIES) {
		TOMBSTONE_USB_DEVICE_DEBUG(0x40200001);

		USB_device_unlock();
#ifdef _DEVICE_DEBUG_
		DEBUG_LOG_TRACE("_usb_device_send_data, transfer in progress");
#endif
		return USB_STATUS_TRANSFER_IN_PROGRESS;
	}

	/* Endif */
	/* Get a transfer descriptor */
	USB_XD_QGET(usb_dev_ptr->XD_HEAD, usb_dev_ptr->XD_TAIL, xd_ptr);

	usb_dev_ptr->XD_ENTRIES--;

	/* Initialize the new transfer descriptor */
	xd_ptr->EP_NUM = ep_num;
	xd_ptr->BDIRECTION = USB_SEND;
	xd_ptr->WTOTALLENGTH = size;
	xd_ptr->WSOFAR = 0;
	xd_ptr->WSTARTADDRESS = buff_ptr;
	xd_ptr->DONT_ZERO_TERMINATE = usb_dev_ptr->XD_BASE->DONT_ZERO_TERMINATE;

	xd_ptr->BSTATUS = USB_STATUS_TRANSFER_ACCEPTED;

	TOMBSTONE_USB_DEVICE_DEBUG(0x40200002);
	error = _usb_dci_vusb20_send_data(handle, xd_ptr);
	TOMBSTONE_USB_DEVICE_DEBUG(0x40200003);

	USB_device_unlock();

	if (error) {
		TOMBSTONE_USB_DEVICE_DEBUG(0x40200004);
#ifdef _DEVICE_DEBUG_
		DEBUG_LOG_TRACE("_usb_device_send_data, transfer failed");
#endif
		return USBERR_TX_FAILED;
	}
	/* Endif */
#ifdef _DEVICE_DEBUG_
	DEBUG_LOG_TRACE("_usb_device_send_data, SUCCESSFUL");
#endif

	TOMBSTONE_USB_DEVICE_DEBUG(0x40200099);
	return error;
}				/* EndBody */
