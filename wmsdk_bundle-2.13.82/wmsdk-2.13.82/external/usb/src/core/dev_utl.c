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
***  This file contains USB device API specific utility functions.
***
**************************************************************************
**END*********************************************************/
#include <incl/devapi.h>
#include <incl/usbprv_dev.h>
#include <incl/asic_usb.h>

/*FUNCTION*-------------------------------------------------------------
*
*  Function Name  : _usb_device_unstall_endpoint
*  Returned Value : USB_OK or error code
*  Comments       :
*     Unstalls the endpoint in specified direction
*
*END*-----------------------------------------------------------------*/
void _usb_device_unstall_endpoint(
					 /* [IN] the USB_USB_dev_initialize state structure */
					 _usb_device_handle handle,
					 /* [IN] the Endpoint number */
					 uint8_t ep_num,
					 /* [IN] direction */
					 uint8_t direction)
{				/* Body */

#ifdef _DEVICE_DEBUG_
	DEBUG_LOG_TRACE("_usb_device_unstall_endpoint");
#endif

	USB_device_lock();

	_usb_dci_vusb20_unstall_endpoint(handle, ep_num, direction);

	USB_device_unlock();

#ifdef _DEVICE_DEBUG_
	DEBUG_LOG_TRACE("_usb_device_unstall_endpoint, SUCCESSFULL");
#endif

}				/* EndBody */

/*FUNCTION*----------------------------------------------------------------
*
* Function Name  : _usb_device_get_status
* Returned Value : USB_OK or error code
* Comments       :
*     Provides API to access the USB internal state.
*
*END*--------------------------------------------------------------------*/
uint8_t _usb_device_get_status(
				    /* [IN] Handle to the USB device */
				    _usb_device_handle handle,
				    /* [IN] What to get the status of */
				    uint8_t component,
				    /* [OUT] The requested status */
				    uint16_t * status)
{				/* Body */
	USB_DEV_STATE_STRUCT_PTR usb_dev_ptr;

	usb_dev_ptr = (USB_DEV_STATE_STRUCT_PTR) handle;

#ifdef _DEVICE_DEBUG_
	DEBUG_LOG_TRACE("_usb_device_get_status, SUCCESSFULL");
#endif

	USB_device_lock();

	switch (component) {

	case USB_STATUS_DEVICE_STATE:
		*status = usb_dev_ptr->USB_STATE;
		break;

	case USB_STATUS_DEVICE:
		*status = usb_dev_ptr->USB_DEVICE_STATE;
		break;
#ifndef SMALL_CODE_SIZE
	case USB_STATUS_INTERFACE:
		break;
#endif
	case USB_STATUS_ADDRESS:
		*status = usb_dev_ptr->DEVICE_ADDRESS;
		break;

	case USB_STATUS_CURRENT_CONFIG:
		*status = usb_dev_ptr->USB_CURR_CONFIG;
		break;

	case USB_STATUS_SOF_COUNT:
		*status = usb_dev_ptr->USB_SOF_COUNT;
		break;

	default:
		if (component & USB_STATUS_ENDPOINT) {

			*status = _usb_dci_vusb20_get_endpoint_status(handle,
								      component
								      &
								      USB_STATUS_ENDPOINT_NUMBER_MASK);
		} else {
			USB_device_unlock();
#ifdef _DEVICE_DEBUG_
			DEBUG_LOG_TRACE("_usb_device_get_status, bad status");
#endif
			return USBERR_BAD_STATUS;
		}		/* Endif */
		break;

	}			/* Endswitch */

	USB_device_unlock();

#ifdef _DEVICE_DEBUG_
	DEBUG_LOG_TRACE("_usb_device_get_status, SUCCESSFUL");
#endif

	return USB_OK;
}				/* EndBody */

/*FUNCTION*----------------------------------------------------------------
*
* Function Name  : _usb_device_set_status
* Returned Value : USB_OK or error code
* Comments       :
*     Provides API to set internal state
*
*END*--------------------------------------------------------------------*/
uint8_t _usb_device_set_status(
				    /* [IN] Handle to the usb device */
				    _usb_device_handle handle,
				    /* [IN] What to set the status of */
				    uint8_t component,
				    /* [IN] What to set the status to */
				    uint16_t setting)
{				/* Body */
	USB_DEV_STATE_STRUCT_PTR usb_dev_ptr;

#ifdef _DEVICE_DEBUG_
	DEBUG_LOG_TRACE("_usb_device_set_status");
#endif

	usb_dev_ptr = (USB_DEV_STATE_STRUCT_PTR) handle;
	USB_device_lock();

	switch (component) {

	case USB_STATUS_DEVICE_STATE:
		usb_dev_ptr->USB_STATE = setting;
		break;

	case USB_STATUS_DEVICE:
		usb_dev_ptr->USB_DEVICE_STATE = setting;
		break;

	case USB_STATUS_INTERFACE:
		break;

	case USB_STATUS_CURRENT_CONFIG:
		usb_dev_ptr->USB_CURR_CONFIG = setting;
		break;

	case USB_STATUS_SOF_COUNT:
		usb_dev_ptr->USB_SOF_COUNT = setting;
		break;

	case USB_FORCE_FULL_SPEED:
		_usb_dci_vusb20_set_speed_full((void *)usb_dev_ptr, setting);
		break;

	case USB_CLEAR_FULL_SPEED:
		_usb_dci_vusb20_clear_speed_full((void *)usb_dev_ptr, setting);
		break;
	case USB_PHY_LOW_POWER_SUSPEND:
		_usb_dci_vusb20_suspend_phy((void *)usb_dev_ptr, setting);
		break;

	case USB_STATUS_ADDRESS:
		usb_dev_ptr->DEVICE_ADDRESS = setting;
		_usb_dci_vusb20_set_address((void *)usb_dev_ptr, setting);
		break;

	case USB_STATUS_TEST_MODE:
		_usb_dci_vusb20_set_test_mode(handle, setting);
		break;

	default:
		if (component & USB_STATUS_ENDPOINT) {
			_usb_dci_vusb20_set_endpoint_status(handle,
							    component &
							    USB_STATUS_ENDPOINT_NUMBER_MASK,
							    setting);
		} else {

			USB_device_unlock();
#ifdef _DEVICE_DEBUG_
			DEBUG_LOG_TRACE("_usb_device_set_status, bad status");
#endif
			return USBERR_BAD_STATUS;
		}		/* Endif */
		break;

	}			/* Endswitch */

	USB_device_unlock();

#ifdef _DEVICE_DEBUG_
	DEBUG_LOG_TRACE("_usb_device_set_status, SUCCESSFUL");
#endif

	return USB_OK;
}				/* EndBody */

/*FUNCTION*-------------------------------------------------------------
*
*  Function Name  : _usb_device_stall_endpoint
*  Returned Value : USB_OK or error code
*  Comments       :
*     Stalls the endpoint.
*
*END*-----------------------------------------------------------------*/
void _usb_device_stall_endpoint(
				       /* [IN] the USB_USB_dev_initialize state structure */
				       _usb_device_handle handle,
				       /* [IN] the Endpoint number */
				       uint8_t ep_num,
				       /* [IN] direction */
				       uint8_t direction)
{				/* Body */
#ifdef _DEVICE_DEBUG_
	DEBUG_LOG_TRACE("_usb_device_stall_endpoint");
#endif

	_usb_dci_vusb20_stall_endpoint(handle, ep_num, direction);

#ifdef _DEVICE_DEBUG_
	DEBUG_LOG_TRACE("_usb_device_stall_endpoint, SUCCESSFUL");
#endif
}				/* EndBody */

/*FUNCTION*-------------------------------------------------------------
*
*  Function Name  : _usb_device_process_resume
*  Returned Value : USB_OK or error code
*  Comments       :
*        Process Resume event
*
*END*-----------------------------------------------------------------*/
void _usb_device_assert_resume(
				      /* [IN] the USB_USB_dev_initialize state structure */
				      _usb_device_handle handle)
{				/* Body */
#ifdef _DEVICE_DEBUG_
	DEBUG_LOG_TRACE("_usb_device_assert_resume");
#endif

	_usb_dci_vusb20_assert_resume(handle);

#ifdef _DEVICE_DEBUG_
	DEBUG_LOG_TRACE("_usb_device_assert_resume, SUCCESSFUL");
#endif
}				/* EndBody */

/* EOF */
