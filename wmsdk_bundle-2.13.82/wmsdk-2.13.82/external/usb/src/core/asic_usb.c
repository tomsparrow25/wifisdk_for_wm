/*
 *
 * ============================================================================
 * Copyright (c) 2008-2014   Marvell International Ltd. All Rights Reserved.
 *
 *                         Marvell Confidential
 * ============================================================================
 */

/*******************************************************************************
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
********************************************************************************/

/**
 *
 * \brief This file contains the USB hw configuration parameters and related
 * functions for a specific platform.
 *
 */
#include <stdio.h>
#include <stdint.h>
#include <incl/asic_usb.h>

/*FUNCTION*-------------------------------------------------------------
*
*  Function Name  : _bsp_get_usb_vector
*  Returned Value : interrupt vector number
*  Comments       :
*        Get the vector number for the specified device number
*END*-----------------------------------------------------------------*/

uint8_t _bsp_get_usb_vector(uint8_t device_number, uint8_t device_type)
{				/* Body */

#ifdef HAVE_USB_DEVICE
	if (device_type == TYPE_DEVICE) {
		if (device_number == 0) {
			return BSP_VUSB20_DEVICE_VECTOR0;
		}		/* Endif */
	}
#endif

#ifdef __USB_OTG__
	if (device_type == TYPE_OTG) {
		if (device_number == 0) {
			return BSP_VUSB20_OTG_VECTOR0;
		}		/* Endif */
	}
#endif

	return (0x0);
}				/* EndBody */

/*FUNCTION*-------------------------------------------------------------
*
*  Function Name  : _bsp_get_usb_base
*  Returned Value : Address of the VUSB register base
*  Comments       :
*        Get the USB register base address
*END*-----------------------------------------------------------------*/

void *_bsp_get_usb_base(uint8_t device_number, uint8_t device_type)
{				/* Body */

#ifdef HAVE_USB_DEVICE
	if (device_type == TYPE_DEVICE) {
		if (device_number == 0) {
			return ((void *)BSP_VUSB20_DEVICE_BASE_ADDRESS0);
		}		/* Endif */
	}
#endif

#ifdef __USB_OTG__
	if (device_type == TYPE_OTG) {
		if (device_number == 0) {
			return ((void *)BSP_VUSB20_OTG_BASE_ADDRESS0);
		}		/* Endif */
	}
#endif

	return (NULL);
}				/* EndBody */

/*FUNCTION*-------------------------------------------------------------
*
*  Function Name  : _bsp_get_usb_capability_register_base
*  Returned Value : Address of the VUSB1.1 capability register base
*  Comments       :
*        Get the USB capability register base address
*END*-----------------------------------------------------------------*/

void *_bsp_get_usb_capability_register_base(
						   /* [IN] the device number */
						   uint8_t device_number,
						   uint8_t device_type)
{				/* Body */

#ifdef HAVE_USB_DEVICE
	if (device_type == TYPE_DEVICE) {
		if (device_number == 0) {
			return ((void *)(BSP_VUSB20_DEVICE_BASE_ADDRESS0 +
					 BSP_VUSB20_DEVICE_CAPABILITY_OFFSET));
		}		/* Endif */
	}
#endif

#ifdef __USB_OTG__
	if (device_type == TYPE_OTG) {
		if (device_number == 0) {
			return ((void *)BSP_VUSB20_OTG_BASE_ADDRESS0);
		}		/* Endif */
	}
#endif

	return (NULL);
}				/* EndBody */

/* EOF */
