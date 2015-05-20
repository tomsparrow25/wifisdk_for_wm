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
***  This file contains the VUSB_HS Device Controller interface
***  function to cancel a transfer.
***
**************************************************************************
**END*********************************************************/
#include <incl/devapi.h>
#include <incl/usbprv_dev.h>
#include <incl/asic_usb.h>

/*FUNCTION*-------------------------------------------------------------
*
*  Function Name  : _usb_dci_vusb20_cancel_transfer
*  Returned Value : The amount of data xferred.
*  Comments       :
*        Cancels a transfer
*
*END*-----------------------------------------------------------------*/
uint32_t _usb_dci_vusb20_cancel_transfer(
					      /* [IN] the USB_dev_initialize state structure */
					      _usb_device_handle handle,
					      /* [IN] the Endpoint number */
					      uint8_t ep_num,
					      /* [IN] direction */
					      uint8_t direction)
{				/* Body */
	USB_DEV_STATE_STRUCT_PTR usb_dev_ptr;
	VUSB20_REG_STRUCT_PTR dev_ptr;
	VUSB20_EP_TR_STRUCT_PTR dTD_ptr, check_dTD_ptr;
	VUSB20_EP_QUEUE_HEAD_STRUCT_PTR ep_queue_head_ptr;
	XD_STRUCT_PTR xd_ptr;
	uint32_t temp, bit_pos;
	uint32_t actual_transfer_length = 0;

#ifdef _DEVICE_DEBUG_
	DEBUG_LOG_TRACE("_usb_dci_vusb20_cancel_transfer");
#endif

	usb_dev_ptr = (USB_DEV_STATE_STRUCT_PTR) handle;
	dev_ptr = (VUSB20_REG_STRUCT_PTR) usb_dev_ptr->DEV_PTR;

	bit_pos = (1 << (16 * direction + ep_num));
	temp = (2 * ep_num + direction);

	ep_queue_head_ptr = (VUSB20_EP_QUEUE_HEAD_STRUCT_PTR)
	    (dev_ptr->REGISTERS.OPERATIONAL_DEVICE_REGISTERS.EP_LIST_ADDR) +
	    temp;

	/* Unlink the dTD */
	dTD_ptr = usb_dev_ptr->EP_DTD_HEADS[temp];

	if (dTD_ptr) {

		if (dTD_ptr->NEXT_TR_ELEM_PTR & VUSBHS_TD_ADDR_MASK) {
			check_dTD_ptr =
			    (VUSB20_EP_TR_STRUCT_PTR) (((uint32_t)
							dTD_ptr->NEXT_TR_ELEM_PTR)
						       & VUSBHS_TD_ADDR_MASK);
		} else {
			check_dTD_ptr =
			    (VUSB20_EP_TR_STRUCT_PTR) ((uint32_t)
						       dTD_ptr->NEXT_TR_ELEM_PTR
						       & VUSBHS_TD_ADDR_MASK);
		}

		if (dTD_ptr->SIZE_IOC_STS & VUSBHS_TD_STATUS_ACTIVE) {
			/* Flushing will halt the pipe */
			/* Write 1 to the Flush register */
			dev_ptr->REGISTERS.
			    OPERATIONAL_DEVICE_REGISTERS.ENDPTFLUSH = bit_pos;

			/* Wait until flushing completed */
			while (dev_ptr->REGISTERS.
			       OPERATIONAL_DEVICE_REGISTERS.ENDPTFLUSH &
			       bit_pos) {
				/* ENDPTFLUSH bit should be cleared to indicate this
				 ** operation is complete
				 */
			}	/* EndWhile */

			while (dev_ptr->REGISTERS.
			       OPERATIONAL_DEVICE_REGISTERS.ENDPTSTATUS &
			       bit_pos) {
				/* Write 1 to the Flush register */
				dev_ptr->REGISTERS.
				    OPERATIONAL_DEVICE_REGISTERS.ENDPTFLUSH =
				    bit_pos;

				/* Wait until flushing completed */
				while (dev_ptr->
				       REGISTERS.OPERATIONAL_DEVICE_REGISTERS.ENDPTFLUSH
				       & bit_pos) {
					/* ENDPTFLUSH bit should be cleared to indicate this
					 ** operation is complete
					 */
				}	/* EndWhile */
			}	/* EndWhile */
		}

		/* Endif */
		/* Get the length */
		xd_ptr = (XD_STRUCT_PTR) dTD_ptr->SCRATCH_PTR->XD_FOR_THIS_DTD;
		if (xd_ptr) {
			actual_transfer_length = xd_ptr->WTOTALLENGTH - ((ep_queue_head_ptr->SIZE_IOC_INT_STS) >> 16);	// remaining length
		}

		/* Retire the current dTD */
		dTD_ptr->SIZE_IOC_STS = 0;
		dTD_ptr->NEXT_TR_ELEM_PTR = VUSBHS_TD_NEXT_TERMINATE;

		/* The transfer descriptor for this dTD */
		xd_ptr = (XD_STRUCT_PTR) dTD_ptr->SCRATCH_PTR->XD_FOR_THIS_DTD;
		dTD_ptr->SCRATCH_PTR->PRIVATE = (void *)usb_dev_ptr;
		/* Free the dTD */
		_usb_dci_vusb20_free_dTD((void *)dTD_ptr);

		/* Update the dTD head and tail for specific endpoint/direction */
		if (!check_dTD_ptr) {
			usb_dev_ptr->EP_DTD_HEADS[temp] = NULL;
			usb_dev_ptr->EP_DTD_TAILS[temp] = NULL;
			if (xd_ptr) {
				xd_ptr->SCRATCH_PTR->PRIVATE =
				    (void *)usb_dev_ptr;
				/* Free the transfer descriptor */
				_usb_device_free_XD((void *)xd_ptr);
			}
			/* Endif */
			/* No other transfers on the queue */
			ep_queue_head_ptr->NEXT_DTD_PTR =
			    VUSB_EP_QUEUE_HEAD_NEXT_TERMINATE;
			ep_queue_head_ptr->SIZE_IOC_INT_STS = 0;
		} else {
			usb_dev_ptr->EP_DTD_HEADS[temp] = check_dTD_ptr;

			if (xd_ptr) {
				if ((uint32_t) check_dTD_ptr->SCRATCH_PTR->
				    XD_FOR_THIS_DTD != (uint32_t) xd_ptr) {
					xd_ptr->SCRATCH_PTR->PRIVATE =
					    (void *)usb_dev_ptr;
					/* Free the transfer descriptor */
					_usb_device_free_XD((void *)xd_ptr);
				}	/* Endif */
			}
			/* Endif */
			if (check_dTD_ptr->SIZE_IOC_STS &
			    VUSBHS_TD_STATUS_ACTIVE) {

				/* Start CR 1015 */
				/* Prime the Endpoint */
				/* haitao 03/14/12 */
				dev_ptr->REGISTERS.
				    OPERATIONAL_DEVICE_REGISTERS.ENDPTPRIME =
				    bit_pos;

				if (!
				    (dev_ptr->
				     REGISTERS.OPERATIONAL_DEVICE_REGISTERS.ENDPTSTATUS
				     & bit_pos)) {
					while (dev_ptr->
					       REGISTERS.OPERATIONAL_DEVICE_REGISTERS.ENDPTPRIME
					       & bit_pos) {
						/* Wait for the ENDPTPRIME to go to zero */
					}	/* EndWhile */

					if (dev_ptr->
					    REGISTERS.OPERATIONAL_DEVICE_REGISTERS.ENDPTSTATUS
					    & bit_pos) {
						/* The endpoint was not not primed so no other transfers on
						 ** the queue
						 */
						goto done;
					}	/* Endif */
				} else {
					goto done;
				}	/* Endif */

				/* No other transfers on the queue */
				ep_queue_head_ptr->NEXT_DTD_PTR =
				    ((uint32_t) check_dTD_ptr);
				ep_queue_head_ptr->SIZE_IOC_INT_STS = 0;

				/* Prime the Endpoint */
				/* haitao 03/14/12 */
				dev_ptr->REGISTERS.
				    OPERATIONAL_DEVICE_REGISTERS.ENDPTPRIME =
				    bit_pos;
			}	/* Endif */
		}		/* Endif */
	}
	/* Endif */
done:

	return actual_transfer_length;

	/* End CR 1015 */

#ifdef _DEVICE_DEBUG_
	DEBUG_LOG_TRACE("_usb_dci_vusb20_cancel_transfer, SUCCESSFUL");
#endif

}				/* EndBody */

/* EOF */
