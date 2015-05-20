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
** $Revision: #5 $
*******************************************************************************
*** Description:
***  This file contains the main USB device API functions that will be
***  used by most applications.
***
**************************************************************************
**END*********************************************************/
#include <incl/devapi.h>
#include <incl/usbprv_dev.h>
#include <incl/asic_usb.h>

extern void usb2_device_ISR(uint32_t temp);
/*FUNCTION*-------------------------------------------------------------
*
*  Function Name  : _usb_device_free_XD
*  Returned Value : void
*  Comments       :
*        Enqueues a XD onto the free XD ring.
*
*END*-----------------------------------------------------------------*/

void _usb_device_free_XD(
				/* [IN] the dTD to enqueue */
				void *xd_ptr)
{				/* Body */
	USB_DEV_STATE_STRUCT_PTR usb_dev_ptr = (USB_DEV_STATE_STRUCT_PTR)
	    (((XD_STRUCT_PTR) xd_ptr)->SCRATCH_PTR->PRIVATE);

	/*
	 ** This function can be called from any context, and it needs mutual
	 ** exclusion with itself.
	 */

#ifdef _DEVICE_DEBUG_
	DEBUG_LOG_TRACE("_usb_device_free_XD");
#endif

	USB_device_lock();

	/*
	 ** Add the XD to the free XD queue (linked via PRIVATE) and
	 ** increment the tail to the next descriptor
	 */
	USB_XD_QADD(usb_dev_ptr->XD_HEAD, usb_dev_ptr->XD_TAIL,
		    (XD_STRUCT_PTR) xd_ptr);
	usb_dev_ptr->XD_ENTRIES++;

	USB_device_unlock();
#ifdef _DEVICE_DEBUG_
	DEBUG_LOG_TRACE("_usb_device_free_XD, SUCCESSFUL");
#endif

}				/* Endbody */

/*FUNCTION*-------------------------------------------------------------
*
*  Function Name  : _usb_device_init
*  Returned Value : USB_OK or error code
*  Comments       :
*        Initializes the USB device specific data structures and calls
*  the low-level device controller chip initialization routine.
*
*END*-----------------------------------------------------------------*/
uint8_t _usb_device_init(
			      /* [IN] the USB device controller to initialize */
			      uint8_t devnum,
			      /* [OUT] the USB_USB_dev_initialize state structure */
			      _usb_device_handle * handle,
			      /* [IN] number of endpoints to initialize */
			      uint8_t endpoints)
{				/* Body */
	USB_DEV_STATE_STRUCT_PTR usb_dev_ptr;
	XD_STRUCT_PTR xd_ptr;
	uint8_t i, error;
	SCRATCH_STRUCT_PTR temp_scratch_ptr;
#ifdef SMALL_CODE_SIZE
	uint32_t total_memory;
	uint8_t *stack_mem_ptr;
#endif

#ifdef _DEVICE_DEBUG_
	DEBUG_LOG_TRACE("_usb_device_init");
#endif

#ifndef REDUCED_ERROR_CHECKING
	if (devnum > MAX_USB_DEVICES) {
#ifdef _DEVICE_DEBUG_
		DEBUG_LOG_TRACE
		    ("_usb_device_init, error invalid device number");
#endif
		return USBERR_INVALID_DEVICE_NUM;
	}			/* Endif */
#endif

   /**************************************************************
   All memory allocations should be consolidated in one.
   **************************************************************/

#ifndef SMALL_CODE_SIZE
	/* Allocate memory for the state structure */
	usb_dev_ptr = (USB_DEV_STATE_STRUCT_PTR)
	    USB_memalloc(sizeof(USB_DEV_STATE_STRUCT));

	if (usb_dev_ptr == NULL) {
#ifdef _DEVICE_DEBUG_
		DEBUG_LOG_TRACE("_usb_device_init, error NULL device handle");
#endif
		return USBERR_ALLOC_STATE;
	}

	/* Endif */
	/* Zero out the internal USB state structure */
	USB_memzero(usb_dev_ptr, sizeof(USB_DEV_STATE_STRUCT));
#else
#ifndef NO_DYNAMIC_MEMORY_ALLOCATION
	total_memory = sizeof(USB_DEV_STATE_STRUCT) +
	    sizeof(XD_STRUCT) * MAX_XDS_FOR_TR_CALLS +
	    sizeof(SCRATCH_STRUCT) * MAX_XDS_FOR_TR_CALLS + sizeof(XD_STRUCT);

	stack_mem_ptr = USB_memalloc(total_memory);
	if (stack_mem_ptr == NULL) {
#ifdef _DEVICE_DEBUG_
		DEBUG_LOG_TRACE("_usb_device_init, error NULL device handle");
#endif
		return USBERR_ALLOC_STATE;
	}			/* Endif */
#else
	stack_mem_ptr = (uint8_t *) USB_STACK_MEMORY_START;
#endif

	USB_memzero((void *)stack_mem_ptr, total_memory);

	usb_dev_ptr = (USB_DEV_STATE_STRUCT_PTR) stack_mem_ptr;

#endif
   /**************************************************************
   **************************************************************/
	/* Multiple devices will have different base addresses and
	 ** interrupt vectors (For future)
	 */
	usb_dev_ptr->DEV_PTR = _bsp_get_usb_base(devnum, TYPE_DEVICE);
	usb_dev_ptr->DEV_VEC = _bsp_get_usb_vector(devnum, TYPE_DEVICE);
	usb_dev_ptr->USB_STATE = USB_STATE_UNKNOWN;

	usb_dev_ptr->MAX_ENDPOINTS = endpoints;

#ifndef SMALL_CODE_SIZE
	/* Allocate MAX_XDS_FOR_TR_CALLS */
	xd_ptr = (XD_STRUCT_PTR)
	    USB_memalloc(sizeof(XD_STRUCT) * MAX_XDS_FOR_TR_CALLS);
	if (xd_ptr == NULL) {
#ifdef _DEVICE_DEBUG_
		DEBUG_LOG_TRACE("_usb_device_init, malloc error");
#endif
		return USBERR_ALLOC_TR;
	}

	USB_memzero(xd_ptr, sizeof(XD_STRUCT) * MAX_XDS_FOR_TR_CALLS);

	/* Allocate memory for internal scratch structure */
	usb_dev_ptr->XD_SCRATCH_STRUCT_BASE = (SCRATCH_STRUCT_PTR)
	    USB_memalloc(sizeof(SCRATCH_STRUCT) * MAX_XDS_FOR_TR_CALLS);

	if (usb_dev_ptr->XD_SCRATCH_STRUCT_BASE == NULL) {
#ifdef _DEVICE_DEBUG_
		DEBUG_LOG_TRACE("_usb_device_init, malloc error");
#endif
		return USBERR_ALLOC;
	}

	/* Endif */
#else
	/* Allocate MAX_XDS_FOR_TR_CALLS */
	xd_ptr = (XD_STRUCT_PTR) (stack_mem_ptr + sizeof(USB_DEV_STATE_STRUCT));
	usb_dev_ptr->XD_SCRATCH_STRUCT_BASE = (SCRATCH_STRUCT_PTR)
	    (stack_mem_ptr + sizeof(USB_DEV_STATE_STRUCT) +
	     (sizeof(XD_STRUCT) * MAX_XDS_FOR_TR_CALLS)
	    );

#endif

	usb_dev_ptr->XD_BASE = xd_ptr;
	temp_scratch_ptr = usb_dev_ptr->XD_SCRATCH_STRUCT_BASE;

#ifndef SMALL_CODE_SIZE
	usb_dev_ptr->XD_HEAD = NULL;
	usb_dev_ptr->XD_TAIL = NULL;
	usb_dev_ptr->XD_ENTRIES = 0;
#endif

	/* Enqueue all the XDs */
	for (i = 0; i < MAX_XDS_FOR_TR_CALLS; i++) {
		xd_ptr->SCRATCH_PTR = temp_scratch_ptr;
		xd_ptr->SCRATCH_PTR->FREE = _usb_device_free_XD;
		xd_ptr->SCRATCH_PTR->PRIVATE = (void *)usb_dev_ptr;
		_usb_device_free_XD((void *)xd_ptr);
		xd_ptr++;
		temp_scratch_ptr++;
	}			/* Endfor */

#ifndef SMALL_CODE_SIZE
	usb_dev_ptr->TEMP_XD_PTR =
	    (XD_STRUCT_PTR) USB_memalloc(sizeof(XD_STRUCT));
	USB_memzero(usb_dev_ptr->TEMP_XD_PTR, sizeof(XD_STRUCT));

#else
	usb_dev_ptr->TEMP_XD_PTR =
	    (XD_STRUCT_PTR) (stack_mem_ptr + sizeof(USB_DEV_STATE_STRUCT) +
			     (sizeof(XD_STRUCT) * MAX_XDS_FOR_TR_CALLS) +
			     sizeof(SCRATCH_STRUCT) * MAX_XDS_FOR_TR_CALLS);
#endif
	/* Initialize the USB controller chip */

	error = _usb_dci_vusb20_init(devnum, usb_dev_ptr);

	if (error) {
#ifdef _DEVICE_DEBUG_
		DEBUG_LOG_TRACE("_usb_device_init, init failed");
#endif
		return USBERR_INIT_FAILED;
	}
	/* Endif */
	*handle = usb_dev_ptr;
#ifdef _DEVICE_DEBUG_
	DEBUG_LOG_TRACE("_usb_device_init, SUCCESSFUL");
#endif
	return USB_OK;
}				/* EndBody */

#ifndef SMALL_CODE_SIZE

/*FUNCTION*----------------------------------------------------------------
*
* Function Name  : _usb_device_register_service
* Returned Value : USB_OK or error code
* Comments       :
*     Registers a callback routine for a specified event or endpoint.
*
*END*--------------------------------------------------------------------*/
uint8_t _usb_device_register_service(
					  /* [IN] Handle to the USB device */
					  _usb_device_handle handle,
					  /* [IN] type of event or endpoint number to service */
					  uint8_t type,
					  /* [IN] Pointer to the service's callback function */
					  void (*service) (void *, bool,
							   uint8_t, uint8_t *,
							   uint32_t, uint8_t,
							   uint8_t)
    )
{				/* Body */
	USB_DEV_STATE_STRUCT_PTR usb_dev_ptr;
	SERVICE_STRUCT_PTR service_ptr;
	SERVICE_STRUCT_PTR *search_ptr;

#ifdef _DEVICE_DEBUG_
	DEBUG_LOG_TRACE("_usb_device_register_service");
#endif

	usb_dev_ptr = (USB_DEV_STATE_STRUCT_PTR) handle;
	/* Needs mutual exclusion */
	USB_device_lock();

	/* Search for an existing entry for type */
	for (search_ptr = &usb_dev_ptr->SERVICE_HEAD_PTR;
	     *search_ptr; search_ptr = &(*search_ptr)->NEXT) {
		if ((*search_ptr)->TYPE == type) {
			/* Found an existing entry */

			USB_device_unlock();
#ifdef _DEVICE_DEBUG_
			DEBUG_LOG_TRACE
			    ("_usb_device_register_service, service closed");
#endif
			return USBERR_OPEN_SERVICE;
		}		/* Endif */
	}			/* Endfor */

	/* No existing entry found - create a new one */
	service_ptr = (SERVICE_STRUCT_PTR) USB_memalloc(sizeof(SERVICE_STRUCT));

	if (!service_ptr) {
		USB_device_unlock();
#ifdef _DEVICE_DEBUG_
		DEBUG_LOG_TRACE
		    ("_usb_device_register_service, error allocating service");
#endif
		return USBERR_ALLOC;
	}
	/* Endif */
	service_ptr->TYPE = type;
	service_ptr->SERVICE = service;
	service_ptr->NEXT = NULL;
	*search_ptr = service_ptr;

	USB_device_unlock();

#ifdef _DEVICE_DEBUG_
	DEBUG_LOG_TRACE("_usb_device_register_service, SUCCESSFUL");
#endif
	return USB_OK;
}				/* EndBody */

/*FUNCTION*----------------------------------------------------------------
*
* Function Name  : _usb_device_unregister_service
* Returned Value : USB_OK or error code
* Comments       :
*     Unregisters a callback routine for a specified event or endpoint.
*
*END*--------------------------------------------------------------------*/
uint8_t _usb_device_unregister_service(
					    /* [IN] Handle to the USB device */
					    _usb_device_handle handle,
					    /* [IN] type of event or endpoint number to service */
					    uint8_t type)
{				/* Body */
	USB_DEV_STATE_STRUCT_PTR usb_dev_ptr;
	SERVICE_STRUCT_PTR service_ptr;
	SERVICE_STRUCT_PTR *search_ptr;

#ifdef _DEVICE_DEBUG_
	DEBUG_LOG_TRACE("_usb_device_unregister_service");
#endif

	usb_dev_ptr = (USB_DEV_STATE_STRUCT_PTR) handle;
	/* Needs mutual exclusion */
	USB_device_lock();

	/* Search for an existing entry for type */
	for (search_ptr = &usb_dev_ptr->SERVICE_HEAD_PTR;
	     *search_ptr; search_ptr = &(*search_ptr)->NEXT) {
		if ((*search_ptr)->TYPE == type) {
			/* Found an existing entry - delete it */
			break;
		}		/* Endif */
	}			/* Endfor */

	/* No existing entry found */
	if (!*search_ptr) {

		USB_device_unlock();
#ifdef _DEVICE_DEBUG_
		DEBUG_LOG_TRACE
		    ("_usb_device_unregister_service, no service found");
#endif
		return USBERR_CLOSED_SERVICE;
	}
	/* Endif */
	service_ptr = *search_ptr;
	*search_ptr = service_ptr->NEXT;

	USB_memfree(service_ptr);

	USB_device_unlock();

#ifdef _DEVICE_DEBUG_
	DEBUG_LOG_TRACE("_usb_device_unregister_service, SUCCESSFUL");
#endif
	return USB_OK;

}				/* EndBody */

#endif

/*FUNCTION*----------------------------------------------------------------
*
* Function Name  : _usb_device_call_service
* Returned Value : USB_OK or error code
* Comments       :
*     Calls the appropriate service for the specified type, if one is
*     registered. Used internally only.
*
*END*--------------------------------------------------------------------*/
uint8_t _usb_device_call_service(
				      /* [IN] Handle to the USB device */
				      _usb_device_handle handle,
				      /* [OUT] Type of service or endpoint */
				      uint8_t type,
				      /* [OUT] Is it a Setup transfer? */
				      bool setup,
				      /* [OUT] Direction of transmission; is it a Transmit? */
				      bool direction,
				      /* [OUT] Pointer to the data */
				      uint8_t * buffer_ptr,
				      /* [OUT] Number of bytes in transmission */
				      uint32_t length,
				      /* [OUT] Any errors */
				      uint8_t errors) {	/* Body */

#ifndef SMALL_CODE_SIZE

	USB_DEV_STATE_STRUCT_PTR usb_dev_ptr;
	SERVICE_STRUCT *service_ptr;

#ifdef _DEVICE_DEBUG_
	DEBUG_LOG_TRACE("_usb_device_call_service");
#endif
	usb_dev_ptr = (USB_DEV_STATE_STRUCT_PTR) handle;
	/* Needs mutual exclusion */
	USB_device_lock();

	/* Search for an existing entry for type */
	for (service_ptr = usb_dev_ptr->SERVICE_HEAD_PTR;
	     service_ptr; service_ptr = service_ptr->NEXT) {
		if (service_ptr->TYPE == type) {
			service_ptr->SERVICE(handle, setup, direction,
					     buffer_ptr, length, errors, type);

			USB_device_unlock();
#ifdef _DEVICE_DEBUG_
			DEBUG_LOG_TRACE
			    ("_usb_device_call_service, SUCCESSFUL service called");
#endif
			return USB_OK;
		}
		/* Endif */
	}			/* Endfor */

	USB_device_unlock();

#ifdef _DEVICE_DEBUG_
	DEBUG_LOG_TRACE("_usb_device_call_service, SUCCESSFUL service closed");
#endif
	return USBERR_CLOSED_SERVICE;

/**********************************************************************
When code size needs to be reduced we just hardcode the routine names
inside this routine. This allows the applications to, not call
register or unregister routines, thus saving code space while
providing a performance improvement.
**********************************************************************/

#else

	switch (type) {
	case USB_SERVICE_EP0:
		service_ep0(handle, setup, direction, buffer_ptr, length,
			    errors, type);
		break;
	case USB_SERVICE_EP1:
		service_ep1(handle, setup, direction, buffer_ptr, length,
			    errors, type);
		break;
	case USB_SERVICE_EP2:
		break;
	case USB_SERVICE_EP3:
		break;
	case USB_SERVICE_BUS_RESET:
		/* call your application routine here */
		reset_ep0(handle, setup, direction, buffer_ptr, length, errors,
			  type);
		break;
	case USB_SERVICE_SUSPEND:
		break;
	case USB_SERVICE_SOF:
		break;
	case USB_SERVICE_RESUME:
		break;
	case USB_SERVICE_SLEEP:
		/*bus_suspend(handle, setup, direction, buffer_ptr, length, errors); */
		break;
	case USB_SERVICE_SPEED_DETECTION:
		service_speed(handle, setup, direction, buffer_ptr, length,
			      errors, type);
		/*detect_speed(handle, setup, direction, buffer_ptr, length, errors); */
		break;
	case USB_SERVICE_ERROR:
		break;
	case USB_SERVICE_STALL:
		break;
	}

	return USB_OK;
#endif
}				/* EndBody */

/**
 * \brief Change the zlp setting in the hw.
 * \param[in] handle The opened usb handle
 * \param[in] EPNum The number of the endpoint to change
 * \param[in] zlpflag The value to change to flag to.
 */

void usb_change_hw_zlp(_usb_device_handle handle, uint32_t EPNum, uint8_t zlpflag)
{

	USB_DEV_STATE_STRUCT_PTR usb_dev_ptr;
	//
	// Change the xd structures with the new value.
	usb_dev_ptr = (USB_DEV_STATE_STRUCT_PTR) handle;
	usb_dev_ptr->TEMP_XD_PTR->DONT_ZERO_TERMINATE = (bool) zlpflag;
	usb_dev_ptr->XD_BASE->DONT_ZERO_TERMINATE = (bool) zlpflag;
	usb_dev_ptr->XD_HEAD->DONT_ZERO_TERMINATE = (bool) zlpflag;
	usb_dev_ptr->XD_TAIL->DONT_ZERO_TERMINATE = (bool) zlpflag;
	// change the hw to match what we want.
	usb_dci_vusb20_change_zlp(handle, EPNum, zlpflag);
}

/*FUNCTION*-------------------------------------------------------------
*
*  Function Name  : _usb_device_init_endpoint
*  Returned Value : USB_OK or error code
*  Comments       :
*     Initializes the endpoint and the data structures associated with the
*  endpoint
*
*END*-----------------------------------------------------------------*/
uint8_t _usb_device_init_endpoint(
				       /* [IN] the USB_USB_dev_initialize state structure */
				       _usb_device_handle handle,
				       /* [IN] the Endpoint number */
				       uint8_t ep_num,
				       /* [IN] MAX Packet size for this endpoint */
				       uint16_t max_pkt_size,
				       /* [IN] Direction */
				       uint8_t direction,
				       /* [IN] Type of Endpoint */
				       uint8_t type,
				       /* [IN] After all data is transfered, should we terminate the transfer
				        ** with a zero length packet if the last packet size == MAX_PACKET_SIZE?
				        */
				       uint8_t flag)
{				/* Body */
	uint8_t error = 0;
	USB_DEV_STATE_STRUCT_PTR usb_dev_ptr;

#ifdef _DEVICE_DEBUG_
	DEBUG_LOG_TRACE("_usb_device_init_endpoint");
#endif

	usb_dev_ptr = (USB_DEV_STATE_STRUCT_PTR) handle;

	/* Initialize the transfer descriptor */
	usb_dev_ptr->TEMP_XD_PTR->EP_NUM = ep_num;
	usb_dev_ptr->TEMP_XD_PTR->BDIRECTION = direction;
	usb_dev_ptr->TEMP_XD_PTR->WMAXPACKETSIZE = max_pkt_size;
	usb_dev_ptr->TEMP_XD_PTR->EP_TYPE = type;
	usb_dev_ptr->TEMP_XD_PTR->DONT_ZERO_TERMINATE = (bool) flag;
	usb_dev_ptr->TEMP_XD_PTR->MAX_PKTS_PER_UFRAME =
	    ((flag & USB_MAX_PKTS_PER_UFRAME) >> 1);

	error = _usb_dci_vusb20_init_endpoint(handle, usb_dev_ptr->TEMP_XD_PTR);

#ifdef _DEVICE_DEBUG_
	DEBUG_LOG_TRACE("_usb_device_init_endpoint, SUCCESSFUL");
#endif

	return error;
}				/* EndBody */

/*FUNCTION*-------------------------------------------------------------
*
*  Function Name  : _usb_device_get_transfer_status
*  Returned Value : Status of the transfer
*  Comments       :
*        returns the status of the transaction on the specified endpoint.
*
*END*-----------------------------------------------------------------*/
uint8_t _usb_device_get_transfer_status(
					     /* [IN] the USB_USB_dev_initialize state structure */
					     _usb_device_handle handle,
					     /* [IN] the Endpoint number */
					     uint8_t ep_num,
					     /* [IN] direction */
					     uint8_t direction)
{				/* Body */
	uint8_t status;

#ifdef _DEVICE_DEBUG_
	DEBUG_LOG_TRACE("_usb_device_get_transfer_status");
#endif

	USB_device_lock();

	status = _usb_dci_vusb20_get_transfer_status(handle, ep_num, direction);

	USB_device_unlock();

#ifdef _DEVICE_DEBUG_
	DEBUG_LOG_TRACE("_usb_device_get_transfer_status, SUCCESSFUL");
#endif

	/* Return the status of the last queued transfer */
	return (status);

}				/* EndBody */

/*FUNCTION*-------------------------------------------------------------
*
*  Function Name  : _usb_device_get_transfer_details
*  Returned Value : Status of the transfer
*  Comments       :
*        returns the status of the transaction on the specified endpoint.
*
*END*-----------------------------------------------------------------*/
void _usb_device_get_transfer_details(
					     /* [IN] the USB_USB_dev_initialize state structure */
					     _usb_device_handle handle,
					     /* [IN] the Endpoint number */
					     uint8_t ep_num,
					     /* [IN] direction */
					     uint8_t direction,
					     /* [OUT] transfer detail data structure maintained by driver */
					     uint32_t * *xd_ptr_ptr)
{				/* Body */
	XD_STRUCT_PTR temp;

#ifdef _DEVICE_DEBUG_
	DEBUG_LOG_TRACE("_usb_device_get_transfer_status");
#endif

	USB_device_lock();

	temp = _usb_dci_vusb20_get_transfer_details(handle, ep_num, direction);
	(*xd_ptr_ptr) = (uint32_t *) temp;

	USB_device_unlock();

#ifdef _DEVICE_DEBUG_
	DEBUG_LOG_TRACE("_usb_device_get_transfer_status, SUCCESSFUL");
#endif

	return;

}				/* EndBody */

/*FUNCTION*-------------------------------------------------------------
*
*  Function Name  : _usb_device_read_setup_data
*  Returned Value : USB_OK or error code
*  Comments       :
*        Reads the setup data from the hardware
*
*END*-----------------------------------------------------------------*/
void _usb_device_read_setup_data(
					/* [IN] the USB_USB_dev_initialize state structure */
					_usb_device_handle handle,
					/* [IN] the Endpoint number */
					uint8_t ep_num,
					/* [IN] buffer for receiving Setup packet */
					uint8_t * buff_ptr)
{				/* Body */
#ifdef _DEVICE_DEBUG_
	DEBUG_LOG_TRACE("_usb_device_read_setup_data");
#endif

	_usb_dci_vusb20_get_setup_data(handle, ep_num, buff_ptr);

#ifdef _DEVICE_DEBUG_
	DEBUG_LOG_TRACE("_usb_device_read_setup_data, SUCCESSFUL");
#endif

}				/* EndBody */

/*FUNCTION*-------------------------------------------------------------
*
*  Function Name  : _usb_device_set_address
*  Returned Value : USB_OK or error code
*  Comments       :
*        Sets the device address as assigned by the host during enumeration
*
*END*-----------------------------------------------------------------*/
void _usb_device_set_address(
				    /* [IN] the USB_USB_dev_initialize state structure */
				    _usb_device_handle handle,
				    /* [IN] the USB address to be set in the hardware */
				    uint8_t address)
{				/* Body */
#ifdef _DEVICE_DEBUG_
	DEBUG_LOG_TRACE("_usb_device_set_address");
#endif

	_usb_dci_vusb20_set_address(handle, address);

#ifdef _DEVICE_DEBUG_
	DEBUG_LOG_TRACE("_usb_device_set_address, SUCCESSFUL");
#endif

}				/* EndBody */
