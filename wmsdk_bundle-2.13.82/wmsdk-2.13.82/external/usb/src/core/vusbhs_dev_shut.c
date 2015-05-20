/***********************************************************
* Portions (c) Copyright 2008 Marvell International Ltd.
*
*               Marvell Confidential
* ==========================================================
*/

/*******************************************************************************
** File          : $HeadURL$
** Author        : $Author: ballen $
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
** $Date: 2007/08/07 $
** $Revision: #3 $
*******************************************************************************
*** Description:
***  This file contains the VUSB_HS Device Controller interface function
***  to shutdown the device.
***
**************************************************************************
**END*********************************************************/
#include <incl/devapi.h>
#include <incl/usbprv_dev.h>
#include <incl/asic_usb.h>

#ifdef __USB_OTG__
#include "otgapi.h"
#include "usbprv_otg.h"
extern USB_OTG_STATE_STRUCT_PTR usb_otg_state_struct_ptr;
#endif

/*FUNCTION*-------------------------------------------------------------
*
*  Function Name  : _usb_dci_vusb20_shutdown
*  Returned Value : None
*  Comments       :
*        Shuts down the VUSB_HS Device
*
*END*-----------------------------------------------------------------*/
void _usb_dci_vusb20_shutdown(
				     /* [IN] the USB_dev_initialize state structure */
				     _usb_device_handle handle)
{				/* Body */
	USB_DEV_STATE_STRUCT_PTR usb_dev_ptr;
	VUSB20_REG_STRUCT_PTR dev_ptr;

#ifdef _DEVICE_DEBUG_
	DEBUG_LOG_TRACE("_usb_dci_vusb20_shutdown");
#endif

	usb_dev_ptr = (USB_DEV_STATE_STRUCT_PTR) handle;

	dev_ptr = (VUSB20_REG_STRUCT_PTR) usb_dev_ptr->DEV_PTR;

	/* Disable interrupts */
	dev_ptr->REGISTERS.OPERATIONAL_DEVICE_REGISTERS.USB_INTR &=
	    ~(EHCI_INTR_INT_EN | EHCI_INTR_ERR_INT_EN |
	      EHCI_INTR_PORT_CHANGE_DETECT_EN | EHCI_INTR_RESET_EN);

	USB_memfree(usb_dev_ptr->DRIVER_MEMORY);

#ifdef __USB_OTG__
#ifdef HNP_HARDWARE_ASSISTANCE
      /********************************************************
      If we are shutting device control down under HNP, we should
      skip shutting down and resetting because it will be
      done by Host routines that will initialize the host..
      ********************************************************/
	if (usb_otg_state_struct_ptr->STATE_STRUCT_PTR->B_HNP_ENABLE == TRUE) {
		return;
	}
#endif
#endif
	/* Reset the Run the bit in the command register to stop VUSB */
	dev_ptr->REGISTERS.OPERATIONAL_DEVICE_REGISTERS.USB_CMD &=
	    ~EHCI_CMD_RUN_STOP;

	/* Reset the controller to get default values */
	dev_ptr->REGISTERS.OPERATIONAL_DEVICE_REGISTERS.USB_CMD =
	    EHCI_CMD_CTRL_RESET;

#ifdef _DEVICE_DEBUG_
	DEBUG_LOG_TRACE("_usb_dci_vusb20_shutdown, SUCCESSFUL");
#endif

}				/* EndBody */
