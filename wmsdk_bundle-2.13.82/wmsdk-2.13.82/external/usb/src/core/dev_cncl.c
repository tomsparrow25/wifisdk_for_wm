/***********************************************************
* Portions (c) Copyright 2008 Marvell International Ltd.
*
*               Marvell Confidential
* ==========================================================
*/

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
** $Revision: #3 $
*******************************************************************************
*** Description:
***  This file contains USB device API specific function to cancel
*** transfer.
***
**************************************************************************
**END*********************************************************/
#include <incl/devapi.h>
#include <incl/usbprv_dev.h>
#include <incl/asic_usb.h>

/*FUNCTION*-------------------------------------------------------------
*
*  Function Name  : _usb_device_cancel_transfer
*  Returned Value : USB_OK or error code
*  Comments       :
*       the amount of data transferred.
*
*END*-----------------------------------------------------------------*/
uint32_t _usb_device_cancel_transfer(
					  /* [IN] the USB_USB_dev_initialize state structure */
					  _usb_device_handle handle,
					  /* [IN] the Endpoint number */
					  uint8_t ep_num,
					  /* [IN] direction */
					  uint8_t direction)
{				/* Body */
	uint32_t amount_xfered;

#ifdef _DEVICE_DEBUG_
	DEBUG_LOG_TRACE("_usb_device_cancel_transfer");
#endif

	/* Cancel transfer on the specified endpoint for the specified
	 ** direction
	 */
	amount_xfered =
	    _usb_dci_vusb20_cancel_transfer(handle, ep_num, direction);

	USB_device_unlock();

#ifdef _DEVICE_DEBUG_
	DEBUG_LOG_TRACE("_usb_device_cancel_transfer, SUCCESSFUL");
#endif
	return amount_xfered;

}				/* EndBody */

/* EOF */
