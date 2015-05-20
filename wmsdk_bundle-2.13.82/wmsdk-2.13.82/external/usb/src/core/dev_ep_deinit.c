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
***  This file contains USB device API specific function to
*** deinitialize the endpoint.
***
**************************************************************************
**END*********************************************************/
#include <incl/devapi.h>
#include <incl/usbprv_dev.h>
#include <incl/asic_usb.h>

/*FUNCTION*-------------------------------------------------------------
*
*  Function Name  : _usb_device_deinit_endpoint
*  Returned Value : USB_OK or error code
*  Comments       :
*  Disables the endpoint and the data structures associated with the
*  endpoint
*
*END*-----------------------------------------------------------------*/
uint8_t _usb_device_deinit_endpoint(
					 /* [IN] the USB_USB_dev_initialize state structure */
					 _usb_device_handle handle,
					 /* [IN] the Endpoint number */
					 uint8_t ep_num,
					 /* [IN] Direction */
					 uint8_t direction)
{				/* Body */
	uint8_t error = 0;

#ifdef _DEVICE_DEBUG_
	DEBUG_LOG_TRACE("_usb_device_deinit_endpoint");
#endif

	USB_device_lock();

	error = _usb_dci_vusb20_deinit_endpoint(handle, ep_num, direction);

	USB_device_unlock();

#ifdef _DEVICE_DEBUG_
	DEBUG_LOG_TRACE("_usb_device_deinit_endpoint,SUCCESSFUL");
#endif

	return error;
}				/* EndBody */

/* EOF */
