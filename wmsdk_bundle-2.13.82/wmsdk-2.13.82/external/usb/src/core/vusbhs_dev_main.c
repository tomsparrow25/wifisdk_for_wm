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
***  This file contains the main VUSB_HS Device Controller interface
***  functions.
***
**************************************************************************
**END*********************************************************/
#include <incl/devapi.h>
#include <incl/usbprv_dev.h>
#include <incl/asic_usb.h>

/* in the  OTG mode this need to be a global */
USB_DEV_STATE_STRUCT_PTR global_usb_device_state_struct_ptr;

volatile uint8_t gInUSBDeviceIsr;

#ifdef __MISSING_RESUME_INTERRUPT_WORKAROUND__
uint32_t port_change_counter = 0;
uint32_t port_change_log[100];
uint32_t sof_after_suspend = 0;
#endif /* __MISSING_RESUME_INTERRUPT_WORKAROUND__ */

/*FUNCTION*-------------------------------------------------------------
*
*  Function Name  : USB_dcache_flush_and_invalidate
*  Returned Value :
*  Comments       :
*        Helper routine to flush and invalidate an area of the data cache.
*        Note that the user should simply provide the specific memory area
*        of interest ... this routine will handle aligning it to the
*        actual cache lines.
*
*END*-----------------------------------------------------------------*/
static void USB_dcache_flush_and_invalidate(void *startAddr,
					    uint32_t sizeInBytes)
{
	/* haitao 03/14 */
	;
}

/*FUNCTION*-------------------------------------------------------------
*
*  Function Name  : _usb_dci_vusb20_init
*  Returned Value : USB_OK or error code
*  Comments       :
*        Initializes the USB device controller.
*
*END*-----------------------------------------------------------------*/
uint8_t _usb_dci_vusb20_init(
				  /* [IN] the USB device controller to initialize */
				  uint8_t devnum,
				  /* [OUT] the USB_dev_initialize state structure */
				  _usb_device_handle handle)
{				/* Body */
	USB_DEV_STATE_STRUCT_PTR usb_dev_ptr;
	VUSB20_REG_STRUCT_PTR cap_dev_ptr;
	uint32_t temp;
	uint32_t total_memory = 0;
	uint8_t *driver_memory;
	//RD uint32_t index = 0;

#ifdef _DEVICE_DEBUG_
	DEBUG_LOG_TRACE("_usb_dci_vusb20_init");
#endif

	usb_dev_ptr = (USB_DEV_STATE_STRUCT_PTR) handle;

	cap_dev_ptr = (VUSB20_REG_STRUCT_PTR)
	    _bsp_get_usb_capability_register_base(devnum, TYPE_DEVICE);

	/* Get the base address of the VUSB_HS registers */
	usb_dev_ptr->DEV_PTR =
	    (VUSB20_REG_STRUCT_PTR) ((uint32_t) cap_dev_ptr +
				     (cap_dev_ptr->
				      REGISTERS.CAPABILITY_REGISTERS.CAPLENGTH_HCIVER
				      & EHCI_CAP_LEN_MASK));

#ifndef SMALL_CODE_SIZE

	/* Get the maximum number of endpoints supported by this USB controller */
	usb_dev_ptr->MAX_ENDPOINTS =
	    (cap_dev_ptr->REGISTERS.CAPABILITY_REGISTERS.DCC_PARAMS &
	     VUSB20_MAX_ENDPTS_SUPPORTED);

	temp = (usb_dev_ptr->MAX_ENDPOINTS * 2);

       /****************************************************************
         Consolidated memory allocation
       ****************************************************************/
	total_memory = ((temp * sizeof(VUSB20_EP_QUEUE_HEAD_STRUCT))) +
	    ((MAX_EP_TR_DESCRS * sizeof(VUSB20_EP_TR_STRUCT)) + 32) +
	    (sizeof(SCRATCH_STRUCT) * MAX_EP_TR_DESCRS);

	driver_memory = (uint8_t *) USB_memalloc_aligned(total_memory, 2048);	// hw forces this alignment
	if (driver_memory == NULL) {
#ifdef _DEVICE_DEBUG_
		DEBUG_LOG_TRACE("_usb_dci_vusb20_init, malloc failed");
#endif
		return USBERR_ALLOC;
	}
#else

       /****************************************************************
         Consolidated memory allocation
       ****************************************************************/
#ifndef NO_DYNAMIC_MEMORY_ALLOCATION
	total_memory =
	    ((usb_dev_ptr->MAX_ENDPOINTS * 2 *
	      sizeof(VUSB20_EP_QUEUE_HEAD_STRUCT))) +
	    ((MAX_EP_TR_DESCRS * sizeof(VUSB20_EP_TR_STRUCT)) + 32) +
	    (sizeof(SCRATCH_STRUCT) * MAX_EP_TR_DESCRS);
	driver_memory = (uint8_t *) USB_memalloc_aligned(total_memory, 2048);
	if (driver_memory == NULL) {
#ifdef _DEVICE_DEBUG_
		DEBUG_LOG_TRACE("_usb_dci_vusb20_init, malloc failed");
#endif
		return USBERR_ALLOC;
	}
#else
	total_memory =
	    ((usb_dev_ptr->MAX_ENDPOINTS * 2 *
	      sizeof(VUSB20_EP_QUEUE_HEAD_STRUCT)) + 2048) +
	    ((MAX_EP_TR_DESCRS * sizeof(VUSB20_EP_TR_STRUCT)) + 32) +
	    (sizeof(SCRATCH_STRUCT) * MAX_EP_TR_DESCRS);
	driver_memory = (uint8_t *) USB_DRIVER_MEMORY_START;
#endif

#endif

    /****************************************************************
      Zero out the memory allocated
    ****************************************************************/
	USB_memzero((void *)driver_memory, total_memory);

#ifdef ENABLE_DCACHE
   /****************************************************************
      Flush the zeroed memory if cache is enabled
   ****************************************************************/
	USB_dcache_flush_and_invalidate((void *)driver_memory, total_memory);

#endif

#ifndef NO_DYNAMIC_MEMORY_ALLOCATION
   /****************************************************************
     Keep a void * to driver memory allocation
   ****************************************************************/
	usb_dev_ptr->DRIVER_MEMORY = driver_memory;

   /****************************************************************
      Assign QH base
   ****************************************************************/
	usb_dev_ptr->EP_QUEUE_HEAD_BASE = (VUSB20_EP_QUEUE_HEAD_STRUCT_PTR)
	    driver_memory;
	driver_memory += ((temp * sizeof(VUSB20_EP_QUEUE_HEAD_STRUCT)));

	usb_dev_ptr->EP_QUEUE_HEAD_PTR = (VUSB20_EP_QUEUE_HEAD_STRUCT_PTR)
	    (uint32_t) usb_dev_ptr->EP_QUEUE_HEAD_BASE;

   /****************************************************************
      Assign DTD base
   ****************************************************************/
	usb_dev_ptr->DTD_BASE_PTR = (VUSB20_EP_TR_STRUCT_PTR)
	    driver_memory;
	driver_memory +=
	    ((MAX_EP_TR_DESCRS * sizeof(VUSB20_EP_TR_STRUCT)) + 32);

	/* Align the dTD base to 32 byte boundary */
	usb_dev_ptr->DTD_ALIGNED_BASE_PTR = (VUSB20_EP_TR_STRUCT_PTR)
	    USB_MEM32_ALIGN((uint32_t) usb_dev_ptr->DTD_BASE_PTR);

   /****************************************************************
      Assign SCRATCH Structure base
   ****************************************************************/
	/* Allocate memory for internal scratch structure */
	usb_dev_ptr->SCRATCH_STRUCT_BASE = (SCRATCH_STRUCT_PTR)
	    driver_memory;
#else
	usb_dev_ptr->EP_QUEUE_HEAD_BASE = (VUSB20_EP_QUEUE_HEAD_STRUCT_PTR)
	    USB_DRIVER_QH_BASE;
	usb_dev_ptr->DTD_BASE_PTR = (VUSB20_EP_TR_STRUCT_PTR)
	    USB_DRIVER_DTD_BASE;
	usb_dev_ptr->SCRATCH_STRUCT_BASE = (SCRATCH_STRUCT_PTR)
	    USB_DRIVER_SCRATCH_STRUCT_BASE;
	usb_dev_ptr->EP_QUEUE_HEAD_PTR =
	    (uint32_t) usb_dev_ptr->EP_QUEUE_HEAD_BASE;

	usb_dev_ptr->DTD_ALIGNED_BASE_PTR = (uint32_t) usb_dev_ptr->DTD_BASE_PTR;

#endif
/*********************************************************************
If there is no need to install ISR, don't install it. Some USB products
would just like to hardcode the function pointers for ISR calls.
*********************************************************************/
#ifndef NO_ISR_INSTALLATION
#ifndef __USB_OTG__
	/* Install the interrupt service routine */
	//     USB_install_isr(usb_dev_ptr->DEV_VEC, _usb_dci_vusb20_isr, NULL);
#endif /* __USB_OTG__ */
#endif

	usb_dev_ptr->USB_STATE = USB_STATE_UNKNOWN;

	global_usb_device_state_struct_ptr = usb_dev_ptr;

	/* Initialize the VUSB_HS controller */
	_usb_dci_vusb20_chip_initialize((void *)usb_dev_ptr);

#ifdef _DEVICE_DEBUG_
	DEBUG_LOG_TRACE("_usb_dci_vusb20_init, SUCCESSFUL");
#endif

	return USB_OK;
}				/* EndBody */

/*FUNCTION*-------------------------------------------------------------
*
*  Function Name  : _usb_dci_vusb20_chip_initialize
*  Returned Value : USB_OK or error code
*  Comments       :
*        Initializes the USB device controller.
*
*END*-----------------------------------------------------------------*/
void _usb_dci_vusb20_chip_initialize(
					    /* [IN] the USB_dev_initialize state structure */
					    _usb_device_handle handle)
{				/* Body */
	USB_DEV_STATE_STRUCT_PTR usb_dev_ptr;
	VUSB20_REG_STRUCT_PTR dev_ptr;
	VUSB20_REG_STRUCT_PTR cap_dev_ptr;
	VUSB20_EP_QUEUE_HEAD_STRUCT_PTR ep_queue_head_ptr;
	VUSB20_EP_TR_STRUCT_PTR dTD_ptr;
	uint32_t i, port_control;
	SCRATCH_STRUCT_PTR temp_scratch_ptr;

#ifdef _DEVICE_DEBUG_
	DEBUG_LOG_TRACE("_usb_dci_vusb20_chip_initialize");
#endif

	usb_dev_ptr = (USB_DEV_STATE_STRUCT_PTR) handle;

	dev_ptr = (VUSB20_REG_STRUCT_PTR) usb_dev_ptr->DEV_PTR;
	cap_dev_ptr = (VUSB20_REG_STRUCT_PTR)
	    _bsp_get_usb_capability_register_base(usb_dev_ptr->DEV_NUM,
						  TYPE_DEVICE);

	/* Stop the controller */
	dev_ptr->REGISTERS.OPERATIONAL_DEVICE_REGISTERS.USB_CMD &=
	    ~EHCI_CMD_RUN_STOP;

	// Delay before reseting the controller
	os_thread_sleep(os_msec_to_ticks(20));

	/* Reset the controller to get default values */
	dev_ptr->REGISTERS.OPERATIONAL_DEVICE_REGISTERS.USB_CMD =
	    EHCI_CMD_CTRL_RESET;

	while (dev_ptr->REGISTERS.OPERATIONAL_DEVICE_REGISTERS.USB_CMD &
	       EHCI_CMD_CTRL_RESET) {
		/* Wait for the controller reset to complete */
	}			/* EndWhile */
	/* When coming out of bootcode might need to remove the dropping of the bus since it might
	 * cause some problems  dirk fixme */
#ifdef TRIP_WIRE
	/* Program the controller to be the USB device controller */
#ifdef CPU_LITTLE_ENDIAN
	dev_ptr->REGISTERS.OPERATIONAL_DEVICE_REGISTERS.USB_MODE =
	    (VUSBHS_MODE_CTRL_MODE_DEV | VUSBHS_MODE_SETUP_LOCK_DISABLE);
#else
	dev_ptr->REGISTERS.OPERATIONAL_DEVICE_REGISTERS.USB_MODE =
	    (VUSBHS_MODE_CTRL_MODE_DEV | VUSBHS_MODE_SETUP_LOCK_DISABLE |
	     VUSBHS_MODE_BIG_ENDIAN);
#endif
#else
	/* Program the controller to be the USB device controller */
#ifdef CPU_LITTLE_ENDIAN
	dev_ptr->REGISTERS.OPERATIONAL_DEVICE_REGISTERS.USB_MODE =
	    (VUSBHS_MODE_CTRL_MODE_DEV);
#else
	dev_ptr->REGISTERS.OPERATIONAL_DEVICE_REGISTERS.USB_MODE =
	    (VUSBHS_MODE_CTRL_MODE_DEV | VUSBHS_MODE_BIG_ENDIAN);
#endif
#endif

	/* Initialize the internal dTD head and tail to NULL */
	usb_dev_ptr->DTD_HEAD = NULL;
	usb_dev_ptr->DTD_TAIL = NULL;

	/* Make sure the 16 MSBs of this register are 0s */
	dev_ptr->REGISTERS.OPERATIONAL_DEVICE_REGISTERS.ENDPT_SETUP_STAT = 0;

	ep_queue_head_ptr = usb_dev_ptr->EP_QUEUE_HEAD_PTR;

	/* Initialize all device queue heads */
	for (i = 0; i < (usb_dev_ptr->MAX_ENDPOINTS * 2); i++) {
		/* Interrupt on Setup packet */
		(ep_queue_head_ptr + i)->MAX_PKT_LENGTH =
		    (((uint32_t) USB_MAX_CTRL_PAYLOAD <<
		      VUSB_EP_QUEUE_HEAD_MAX_PKT_LEN_POS) |
		     VUSB_EP_QUEUE_HEAD_IOS);
		(ep_queue_head_ptr + i)->NEXT_DTD_PTR =
		    VUSB_EP_QUEUE_HEAD_NEXT_TERMINATE;
	}			/* Endfor */

	/* Configure the Endpoint List Address */
	dev_ptr->REGISTERS.OPERATIONAL_DEVICE_REGISTERS.EP_LIST_ADDR =
	    ((uint32_t) ep_queue_head_ptr);

	if (cap_dev_ptr->REGISTERS.CAPABILITY_REGISTERS.HCS_PARAMS &
	    VUSB20_HCS_PARAMS_PORT_POWER_CONTROL_FLAG) {
		port_control =
		    dev_ptr->REGISTERS.OPERATIONAL_DEVICE_REGISTERS.PORTSCX[0];
		port_control &=
		    (~EHCI_PORTSCX_W1C_BITS | ~EHCI_PORTSCX_PORT_POWER);
		dev_ptr->REGISTERS.OPERATIONAL_DEVICE_REGISTERS.PORTSCX[0] =
		    port_control;
	}
	/* Endif */
	dTD_ptr = usb_dev_ptr->DTD_ALIGNED_BASE_PTR;

	temp_scratch_ptr = usb_dev_ptr->SCRATCH_STRUCT_BASE;

	/* Enqueue all the dTDs */
	for (i = 0; i < MAX_EP_TR_DESCRS; i++) {
		dTD_ptr->SCRATCH_PTR = temp_scratch_ptr;
		dTD_ptr->SCRATCH_PTR->FREE = _usb_dci_vusb20_free_dTD;
		/* Set the dTD to be invalid */
		dTD_ptr->NEXT_TR_ELEM_PTR = VUSBHS_TD_NEXT_TERMINATE;
		/* Set the Reserved fields to 0 */
		dTD_ptr->SIZE_IOC_STS &= ~VUSBHS_TD_RESERVED_FIELDS;
		dTD_ptr->SCRATCH_PTR->PRIVATE = (void *)usb_dev_ptr;
		_usb_dci_vusb20_free_dTD((void *)dTD_ptr);
		dTD_ptr++;
		temp_scratch_ptr++;
	}			/* Endfor */

	/* Initialize the endpoint 0 properties */
	dev_ptr->REGISTERS.OPERATIONAL_DEVICE_REGISTERS.ENDPTCTRLX[0] =
	    (EHCI_EPCTRL_TX_DATA_TOGGLE_RST | EHCI_EPCTRL_RX_DATA_TOGGLE_RST);
	dev_ptr->REGISTERS.OPERATIONAL_DEVICE_REGISTERS.ENDPTCTRLX[0] &=
	    ~(EHCI_EPCTRL_TX_EP_STALL | EHCI_EPCTRL_RX_EP_STALL);

	/* Enable interrupts don't enable if one is disabled by a compiler switch */
	dev_ptr->REGISTERS.OPERATIONAL_DEVICE_REGISTERS.USB_INTR =
	    (EHCI_INTR_INT_EN |
#ifndef ERROR_INTERRUPT_DISABLE
	     EHCI_INTR_ERR_INT_EN |
#endif
#ifndef PORT_CHANGE_INTERRUPT_DISABLE
	     EHCI_INTR_PORT_CHANGE_DETECT_EN |
#endif
#ifndef SOF_INTERRUPT_DISABLE
	     EHCI_INTR_SOF_UFRAME_EN |
#endif
#ifndef SUSPEND_INTERRUPT_DISABLE
	     EHCI_INTR_DEVICE_SUSPEND |
#endif
	     EHCI_INTR_RESET_EN);

#ifndef SMALL_CODE_SIZE
	usb_dev_ptr->USB_STATE = USB_STATE_UNKNOWN;
#endif

	// Set Burst Size
	dev_ptr->REGISTERS.OPERATIONAL_DEVICE_REGISTERS.BURST_SIZE = 0x00000808;

	/* Set the Run bit in the command register */
	dev_ptr->REGISTERS.OPERATIONAL_DEVICE_REGISTERS.USB_CMD =
	    EHCI_CMD_RUN_STOP;

#ifdef _DEVICE_DEBUG_
	DEBUG_LOG_TRACE("_usb_dci_vusb20_chip_initialize, SUCCESSFUL");
#endif

}				/* EndBody */

/*FUNCTION*-------------------------------------------------------------
*
*  Function Name  : _usb_dci_vusb20_free_dTD
*  Returned Value : void
*  Comments       :
*        Enqueues a dTD onto the free DTD ring.
*
*END*-----------------------------------------------------------------*/

void _usb_dci_vusb20_free_dTD(
				     /* [IN] the dTD to enqueue */
				     void *dTD_ptr)
{				/* Body */
	USB_DEV_STATE_STRUCT_PTR usb_dev_ptr = (USB_DEV_STATE_STRUCT_PTR)
	    (((VUSB20_EP_TR_STRUCT_PTR) dTD_ptr)->SCRATCH_PTR->PRIVATE);
	//bool usbdev_enabled;

#ifdef _DEVICE_DEBUG_
	DEBUG_LOG_TRACE("_usb_dci_vusb20_free_dTD");
#endif

	/*
	 ** This function can be called from any context, and it needs mutual
	 ** exclusion with itself.
	 */
	/* haitao 03/14/12 */
	USB_device_lock();
	/*
	 ** Add the dTD to the free dTD queue (linked via PRIVATE) and
	 ** increment the tail to the next descriptor
	 */
	EHCI_DTD_QADD(usb_dev_ptr->DTD_HEAD, usb_dev_ptr->DTD_TAIL,
		      (VUSB20_EP_TR_STRUCT_PTR) dTD_ptr);
	usb_dev_ptr->DTD_ENTRIES++;

	/* haitao 03/14/12 */
	USB_device_unlock();

#ifdef _DEVICE_DEBUG_
	DEBUG_LOG_TRACE("_usb_dci_vusb20_free_dTD, SUCCESSFUL");
#endif

}				/* Endbody */

/*FUNCTION*-------------------------------------------------------------
*
*  Function Name  : _usb_dci_vusb20_add_dTD
*  Returned Value : USB_OK or error code
*  Comments       :
*        Adds a device transfer desriptor(s) to the queue.
*
*END*-----------------------------------------------------------------*/
uint8_t _usb_dci_vusb20_add_dTD(
				     /* [IN] the USB_dev_initialize state structure */
				     _usb_device_handle handle,
				     /* [IN] The transfer descriptor address */
				     XD_STRUCT_PTR xd_ptr)
{				/* Body */
	USB_DEV_STATE_STRUCT_PTR usb_dev_ptr;
	VUSB20_REG_STRUCT_PTR dev_ptr;
	VUSB20_EP_TR_STRUCT_PTR dTD_ptr, temp_dTD_ptr, first_dTD_ptr = NULL;
	VUSB20_EP_QUEUE_HEAD_STRUCT_PTR ep_queue_head_ptr;
	uint32_t curr_pkt_len, remaining_len;
	uint32_t curr_offset, temp, bit_pos, temp_ep_stat;
#ifdef TRIP_WIRE
	bool read_safe;
#endif
	uint32_t deadman_count;

   /*********************************************************************
   For a optimal implementation, we need to detect the fact that
   we are adding DTD to an empty list. If list is empty, we can
   actually skip several programming steps esp. those for ensuring
   that there is no race condition.The following bool will be useful
   in skipping some code here.
   *********************************************************************/
	bool list_empty = FALSE;

	TOMBSTONE_USB_DEVICE_DEBUG(0x40230000);

#ifdef _DEVICE_DEBUG_
	DEBUG_LOG_TRACE("_usb_dci_vusb20_add_dTD");
#endif

	usb_dev_ptr = (USB_DEV_STATE_STRUCT_PTR) handle;
	dev_ptr = (VUSB20_REG_STRUCT_PTR) usb_dev_ptr->DEV_PTR;

	remaining_len = xd_ptr->WTOTALLENGTH;

	curr_offset = 0;
	temp = (2 * xd_ptr->EP_NUM + xd_ptr->BDIRECTION);
	bit_pos = (1 << (16 * xd_ptr->BDIRECTION + xd_ptr->EP_NUM));

	ep_queue_head_ptr = (VUSB20_EP_QUEUE_HEAD_STRUCT_PTR)
	    (dev_ptr->REGISTERS.OPERATIONAL_DEVICE_REGISTERS.EP_LIST_ADDR) +
	    temp;

	TOMBSTONE_USB_DEVICE_DEBUG(remaining_len);
	TOMBSTONE_USB_DEVICE_DEBUG(temp);
	TOMBSTONE_USB_DEVICE_DEBUG(bit_pos);
	TOMBSTONE_USB_DEVICE_DEBUG((uint32_t) ep_queue_head_ptr);

   /*********************************************************************
   This loops iterates through the length of the transfer and divides
   the data in to DTDs each handling the a max of 0x4000 bytes of data.
   The first DTD in the list is stored in a void * called first_dTD_ptr.
   This void * is later linked in to QH for processing by the hardware.
   *********************************************************************/

	deadman_count = 0xFFFFFFFF;

	do {
		/* Check if we need to split the transfer into multiple dTDs */
		if (remaining_len > VUSB_EP_MAX_LENGTH_TRANSFER) {
			curr_pkt_len = VUSB_EP_MAX_LENGTH_TRANSFER;
		} else {
			curr_pkt_len = remaining_len;
		}		/* Endif */

		remaining_len -= curr_pkt_len;

		/* Get a dTD from the queue */
		TOMBSTONE_USB_DEVICE_DEBUG(0x40230001);

		EHCI_DTD_QGET(usb_dev_ptr->DTD_HEAD, usb_dev_ptr->DTD_TAIL,
			      dTD_ptr);

		if (!dTD_ptr) {
			TOMBSTONE_USB_DEVICE_DEBUG(0x40230002);
#ifdef _DEVICE_DEBUG_
			DEBUG_LOG_TRACE("_usb_dci_vusb20_add_dTD, SUCCESSFUL");
#endif
			return USBERR_TR_FAILED;
		}
		/* Endif */
		usb_dev_ptr->DTD_ENTRIES--;

		if (curr_offset == 0) {
			first_dTD_ptr = dTD_ptr;
		}
		/* Endif */
#if 0
		//#ifdef ENABLE_DCACHE
      /**************************************************************
      USB Memzero does not bypass the cache and hence we must use
      DTD void * to update the memory and bypass the cache. If
      your DTD are allocated from an uncached regigio, you can
      eliminitate this approach and switch back to USB_memzero().
      **************************************************************/

		dTD_ptr->NEXT_TR_ELEM_PTR = 0;
		dTD_ptr->SIZE_IOC_STS = 0;
		dTD_ptr->BUFF_PTR0 = 0;
		dTD_ptr->BUFF_PTR1 = 0;
		dTD_ptr->BUFF_PTR2 = 0;
		dTD_ptr->BUFF_PTR3 = 0;
		dTD_ptr->BUFF_PTR4 = 0;

#else
		/* Zero the dTD. Leave the last 4 bytes as that is the scratch void * */
		TOMBSTONE_USB_DEVICE_DEBUG(0x40230003);
		USB_memzero((void *)dTD_ptr, (sizeof(VUSB20_EP_TR_STRUCT) - 4));
#endif

		/* Initialize the dTD */
		dTD_ptr->SCRATCH_PTR->PRIVATE = handle;

		/* Set the Terminate bit */
		dTD_ptr->NEXT_TR_ELEM_PTR = VUSB_EP_QUEUE_HEAD_NEXT_TERMINATE;

      /*************************************************************
      FIX ME: For hig-speed and high-bandwidth ISO IN endpoints,
      we must initialize the multiplied field so that Host can issues
      multiple IN transactions on the endpoint. See the DTD data
      structure for MultiIO field.

      S Garg 11/06/2003
      *************************************************************/

		/* Fill in the transfer size */
		if (!remaining_len) {
			TOMBSTONE_USB_DEVICE_DEBUG(0x40230004);
			dTD_ptr->SIZE_IOC_STS = ((curr_pkt_len <<
						  VUSBHS_TD_LENGTH_BIT_POS) |
						 (VUSBHS_TD_IOC) |
						 (VUSBHS_TD_STATUS_ACTIVE));
		} else {
			TOMBSTONE_USB_DEVICE_DEBUG(0x40230005);
			dTD_ptr->SIZE_IOC_STS =
			    ((curr_pkt_len << VUSBHS_TD_LENGTH_BIT_POS)
			     | VUSBHS_TD_STATUS_ACTIVE);
		}		/* Endif */

		/* Set the reserved field to 0 */
		dTD_ptr->SIZE_IOC_STS &= ~VUSBHS_TD_RESERVED_FIELDS;

		/* 4K apart buffer page pointers */
#if 1
		dTD_ptr->BUFF_PTR0 =
		    (uint32_t) (xd_ptr->WSTARTADDRESS + curr_offset);
#else
		dTD_ptr->BUFF_PTR0 = (uint32_t) (devDescrip);
#endif
		dTD_ptr->BUFF_PTR1 = (dTD_ptr->BUFF_PTR0 + 4096);
		dTD_ptr->BUFF_PTR2 = (dTD_ptr->BUFF_PTR1 + 4096);
		dTD_ptr->BUFF_PTR3 = (dTD_ptr->BUFF_PTR2 + 4096);
		dTD_ptr->BUFF_PTR4 = (dTD_ptr->BUFF_PTR3 + 4096);

		curr_offset += curr_pkt_len;

		/* Maintain the first and last device transfer descriptor per
		 ** endpoint and direction
		 */
		if (!usb_dev_ptr->EP_DTD_HEADS[temp]) {
			TOMBSTONE_USB_DEVICE_DEBUG(0x40230006);
			usb_dev_ptr->EP_DTD_HEADS[temp] = dTD_ptr;
			USB_dcache_flush_and_invalidate((void *)dTD_ptr,
							sizeof
							(VUSB20_EP_TR_STRUCT));

	 /***********************************************
         If list does not have a head, it means that list
         is empty. An empty condition is detected.
         ***********************************************/
			list_empty = TRUE;
		}

		/* Endif */
		/* Check if the transfer is to be queued at the end or beginning */
		temp_dTD_ptr = usb_dev_ptr->EP_DTD_TAILS[temp];

		/* Remember which XD to use for this dTD */
		dTD_ptr->SCRATCH_PTR->XD_FOR_THIS_DTD = (void *)xd_ptr;

		/* New tail */
		usb_dev_ptr->EP_DTD_TAILS[temp] = dTD_ptr;
		USB_dcache_flush_and_invalidate((void *)usb_dev_ptr,
						sizeof(USB_DEV_STATE_STRUCT));
		USB_dcache_flush_and_invalidate((void *)xd_ptr,
						sizeof(XD_STRUCT));
		USB_dcache_flush_and_invalidate((void *)dTD_ptr,
						sizeof(VUSB20_EP_TR_STRUCT));

		if (temp_dTD_ptr) {
			TOMBSTONE_USB_DEVICE_DEBUG(0x40230007);
			/* Should not do |=. The Terminate bit should be zero */
			temp_dTD_ptr->NEXT_TR_ELEM_PTR = ((uint32_t) dTD_ptr);

		}
		/* Endif */
		deadman_count--;

		if (deadman_count == 0x0) {
			TOMBSTONE_USB_DEVICE_DEBUG(0x40230008);
			break;
		}

	} while (remaining_len);	/* EndWhile */

   /**************************************************************
   In the loop above DTD has already been added to the list
   However endpoint has not been primed yet. If list is not empty we need safter ways to add DTD to the
   existing list. Else we just skip to adding DTD to QH safely.
   **************************************************************/

	if (!list_empty) {
		TOMBSTONE_USB_DEVICE_DEBUG(0x40230009);
#ifdef TRIP_WIRE
		TOMBSTONE_USB_DEVICE_DEBUG(0x4023000A);
	       /*********************************************************
               Hardware v3.2+ require the use of semaphore to ensure that
               QH is safely updated.
               *********************************************************/

	       /*********************************************************
               Check the prime bit. If set goto done
               *********************************************************/
		if (dev_ptr->REGISTERS.
		    OPERATIONAL_DEVICE_REGISTERS.ENDPTPRIME & bit_pos) {
			TOMBSTONE_USB_DEVICE_DEBUG(0x4023000B);
			goto done;
		}

		read_safe = FALSE;
		while (!read_safe) {

		  /*********************************************************
                  start with setting the semaphores
                  *********************************************************/
			dev_ptr->REGISTERS.
			    OPERATIONAL_DEVICE_REGISTERS.USB_CMD |=
			    EHCI_CMD_ATDTW_TRIPWIRE_SET;

		  /*********************************************************
                  Read the endpoint status
                  *********************************************************/

			temp_ep_stat =
			    dev_ptr->REGISTERS.
			    OPERATIONAL_DEVICE_REGISTERS.ENDPTSTATUS & bit_pos;

		  /*********************************************************
                  Reread the ATDTW semaphore bit to check if it is cleared.
                  When hardware see a hazard, it will clear the bit or
                  else we remain set to 1 and we can proceed with priming
                  of endpoint if not already primed.
                  *********************************************************/
			if (dev_ptr->REGISTERS.
			    OPERATIONAL_DEVICE_REGISTERS.USB_CMD &
			    EHCI_CMD_ATDTW_TRIPWIRE_SET) ;
			{
				TOMBSTONE_USB_DEVICE_DEBUG(0x4023000C);
				read_safe = TRUE;
			}

		}		/*end while loop */

	       /*********************************************************
               Clear the semaphore
               *********************************************************/
		dev_ptr->REGISTERS.OPERATIONAL_DEVICE_REGISTERS.USB_CMD &=
		    EHCI_CMD_ATDTW_TRIPWIRE_CLEAR;

	       /*********************************************************
               If endpoint is not active, we activate it now.
               *********************************************************/
		if (!temp_ep_stat) {
			TOMBSTONE_USB_DEVICE_DEBUG(0x4023000D);
			/* No other transfers on the queue */
			ep_queue_head_ptr->NEXT_DTD_PTR =
			    ((uint32_t) first_dTD_ptr);
			ep_queue_head_ptr->SIZE_IOC_INT_STS = 0;
			USB_dcache_flush_and_invalidate((void *)
							ep_queue_head_ptr,
							sizeof
							(VUSB20_EP_QUEUE_HEAD_STRUCT));

			/* Prime the Endpoint */
			//DEBUG_PRINTF(LOG_DEBUG|DBG_FUNCTIONS,"USB: prime #1 ep %x%d\n",bit_pos);
			dev_ptr->REGISTERS.
			    OPERATIONAL_DEVICE_REGISTERS.ENDPTPRIME = bit_pos;
			TOMBSTONE_USB_DEVICE_DEBUG(bit_pos);
		}
#else /*workaround old method */

		TOMBSTONE_USB_DEVICE_DEBUG(0x4023000E);
		/* Start CR 1015 */
		/* Prime the Endpoint */
		//DEBUG_PRINTF(LOG_DEBUG|DBG_FUNCTIONS,"USB: prime #2 ep %x%d\n",bit_pos);
		dev_ptr->REGISTERS.OPERATIONAL_DEVICE_REGISTERS.ENDPTPRIME =
		    bit_pos;

		if (!
		    (dev_ptr->REGISTERS.
		     OPERATIONAL_DEVICE_REGISTERS.ENDPTSTATUS & bit_pos)) {
			deadman_count = 0xFFFFFFFF;
			TOMBSTONE_USB_DEVICE_DEBUG(0x4023000F);
			/* old workaround will be compiled */
			while (dev_ptr->REGISTERS.
			       OPERATIONAL_DEVICE_REGISTERS.ENDPTPRIME &
			       bit_pos) {
				/* Wait for the ENDPTPRIME to go to zero */
				deadman_count--;
				if (deadman_count == 0x0) {
					TOMBSTONE_USB_DEVICE_DEBUG(0x40230010);
					break;
				}
			}	/* EndWhile */

			if (dev_ptr->REGISTERS.
			    OPERATIONAL_DEVICE_REGISTERS.ENDPTSTATUS & bit_pos)
			{
				/* The endpoint was not not primed so no other transfers on
				 ** the queue
				 */
				TOMBSTONE_USB_DEVICE_DEBUG(0x4023011);
				goto done;
			}
			/* Endif */
		} else {
			TOMBSTONE_USB_DEVICE_DEBUG(0x40230012);
			goto done;
		}		/* Endif */

		/* No other transfers on the queue */
		ep_queue_head_ptr->NEXT_DTD_PTR = ((uint32_t) first_dTD_ptr);
		ep_queue_head_ptr->SIZE_IOC_INT_STS = 0;

		USB_dcache_flush_and_invalidate((void *)ep_queue_head_ptr,
						sizeof
						(VUSB20_EP_QUEUE_HEAD_STRUCT));
		//DEBUG_PRINTF(LOG_DEBUG|DBG_FUNCTIONS,"USB: prime #3 ep %x%d\n",bit_pos);

		/* Prime the Endpoint */
		dev_ptr->REGISTERS.OPERATIONAL_DEVICE_REGISTERS.ENDPTPRIME =
		    bit_pos;

#endif

	} else {
		TOMBSTONE_USB_DEVICE_DEBUG(0x40230013);

		/* No other transfers on the queue */
		ep_queue_head_ptr->NEXT_DTD_PTR = ((uint32_t) first_dTD_ptr);
		ep_queue_head_ptr->SIZE_IOC_INT_STS = 0;
		USB_dcache_flush_and_invalidate((void *)ep_queue_head_ptr,
						sizeof
						(VUSB20_EP_QUEUE_HEAD_STRUCT));
		//DEBUG_PRINTF(LOG_DEBUG|DBG_FUNCTIONS,"USB: prime #4 ep %x%d\n",bit_pos);

		/* Prime the Endpoint */
		dev_ptr->REGISTERS.OPERATIONAL_DEVICE_REGISTERS.ENDPTPRIME =
		    bit_pos;

		TOMBSTONE_USB_DEVICE_DEBUG(0x40230014);
	}

#ifdef _DEVICE_DEBUG_
	DEBUG_LOG_TRACE("_usb_dci_vusb20_add_dTD, SUCCESSFUL");
#endif

done:
#ifdef _DEVICE_DEBUG_
	DEBUG_LOG_TRACE("_usb_dci_vusb20_add_dTD, SUCCESSFUL");
#endif

	TOMBSTONE_USB_DEVICE_DEBUG(0x40230099);
	return USB_OK;
	/* End CR 1015 */
}				/* EndBody */

/*FUNCTION*-------------------------------------------------------------
*
*  Function Name  : _usb_dci_vusb20_send_data
*  Returned Value : USB_OK or error code
*  Comments       :
*        Sends data by adding and executing the dTD. Non-blocking.
*
*END*-----------------------------------------------------------------*/
uint8_t _usb_dci_vusb20_send_data(
				       /* [IN] the USB_dev_initialize state structure */
				       _usb_device_handle handle,
				       /* [IN] The transfer descriptor address */
				       XD_STRUCT_PTR xd_ptr)
{				/* Body */
	uint8_t val;

#ifdef _DEVICE_DEBUG_
	DEBUG_LOG_TRACE("_usb_dci_vusb20_send_data, SUCCESSFUL");
#endif
	TOMBSTONE_USB_DEVICE_DEBUG(0x40600000);
	/* Add and execute the device transfer descriptor */
	val = _usb_dci_vusb20_add_dTD(handle, xd_ptr);
	TOMBSTONE_USB_DEVICE_DEBUG(0x40600099);
	return (val);
}				/* EndBody */

/*FUNCTION*-------------------------------------------------------------
*
*  Function Name  : _usb_dci_vusb20_recv_data
*  Returned Value : USB_OK or error code
*  Comments       :
*        Receives data by adding and executing the dTD. Non-blocking.
*
*END*-----------------------------------------------------------------*/
uint8_t _usb_dci_vusb20_recv_data(
				       /* [IN] the USB_dev_initialize state structure */
				       _usb_device_handle handle,
				       /* [IN] The transfer descriptor address */
				       XD_STRUCT_PTR xd_ptr)
{				/* Body */

	TOMBSTONE_USB_DEVICE_DEBUG(0x40220000);
#ifdef _DEVICE_DEBUG_
	DEBUG_LOG_TRACE("_usb_dci_vusb20_recv_data, SUCCESSFUL");
#endif
	/* Add and execute the device transfer descriptor */
	return (_usb_dci_vusb20_add_dTD(handle, xd_ptr));
}				/* EndBody */

/*FUNCTION*-------------------------------------------------------------
*
*  Function Name  : _usb_dci_vusb20_process_tr_complete
*  Returned Value : None
*  Comments       :
*        Services transaction complete interrupt
*
*END*-----------------------------------------------------------------*/
void _usb_dci_vusb20_process_tr_complete(
						/* [IN] the USB_dev_initialize state structure */
						_usb_device_handle handle)
{				/* Body */
	USB_DEV_STATE_STRUCT_PTR usb_dev_ptr;
	VUSB20_REG_STRUCT_PTR dev_ptr;
	VUSB20_EP_TR_STRUCT_PTR dTD_ptr;
	VUSB20_EP_TR_STRUCT_PTR temp_dTD_ptr;
	VUSB20_EP_QUEUE_HEAD_STRUCT_PTR ep_queue_head_ptr;
	uint32_t temp, i, ep_num, direction, bit_pos;
	uint32_t remaining_length = 0;
	uint32_t actual_transfer_length = 0;
	uint32_t errors = 0;
	XD_STRUCT_PTR xd_ptr;
	XD_STRUCT_PTR temp_xd_ptr = NULL;
	uint8_t *buff_start_address = NULL;
	bool endpoint_detected = FALSE;

	usb_dev_ptr = (USB_DEV_STATE_STRUCT_PTR) handle;
	dev_ptr = (VUSB20_REG_STRUCT_PTR) usb_dev_ptr->DEV_PTR;

#ifdef _DEVICE_DEBUG_
	DEBUG_LOG_TRACE("_usb_dci_vusb20_process_tr_complete");
#endif

	/* We use separate loops for ENDPTSETUPSTAT and ENDPTCOMPLETE because the
	 ** setup packets are to be read ASAP
	 */

	/* Process all Setup packet received interrupts */
	bit_pos =
	    dev_ptr->REGISTERS.OPERATIONAL_DEVICE_REGISTERS.ENDPT_SETUP_STAT;

	if (bit_pos) {
		for (i = 0; i < USB_MAX_CONTROL_ENDPOINTS; i++) {
			if (bit_pos & (1 << i)) {
				_usb_device_call_service(handle, i, TRUE, FALSE,
							 0, 8, 0);
			}	/* Endif */
		}		/* Endfor */
	}

	/* Endif */
	/* Don't clear the endpoint setup status register here. It is cleared as a
	 ** setup packet is read out of the buffer
	 */
	/* Process non-setup transaction complete interrupts */
	bit_pos = dev_ptr->REGISTERS.OPERATIONAL_DEVICE_REGISTERS.ENDPTCOMPLETE;

	/* Clear the bits in the register */
	dev_ptr->REGISTERS.OPERATIONAL_DEVICE_REGISTERS.ENDPTCOMPLETE = bit_pos;

	if (bit_pos) {
		/* Get the endpoint number and the direction of transfer */

		for (i = 0; i < USB_MAX_ENDPOINTS * 2; i++) {

			endpoint_detected = FALSE;
			if ((i < USB_MAX_ENDPOINTS) && (bit_pos & (1 << i))) {
				ep_num = i;
				direction = 0;
				endpoint_detected = TRUE;
			} else if ((i >= USB_MAX_ENDPOINTS) &&
				   (bit_pos &
				    (1 << (i + 16 - USB_MAX_ENDPOINTS)))) {
				ep_num = (i - USB_MAX_ENDPOINTS);
				direction = 1;
				endpoint_detected = TRUE;
			}

			if (endpoint_detected) {

				temp = (2 * ep_num + direction);

				/* Get the first dTD */
				dTD_ptr = usb_dev_ptr->EP_DTD_HEADS[temp];

				ep_queue_head_ptr =
				    (VUSB20_EP_QUEUE_HEAD_STRUCT_PTR)
				    (dev_ptr->
				     REGISTERS.OPERATIONAL_DEVICE_REGISTERS.EP_LIST_ADDR)
				    + temp;

				/* Process all the dTDs for respective transfers */
				while (dTD_ptr) {

					if (dTD_ptr->SIZE_IOC_STS &
					    VUSBHS_TD_STATUS_ACTIVE) {
						/* No more dTDs to process. Next one is owned by VUSB */
						break;
					}

					/* Endif */
					/* Get the correct internal transfer descriptor */
					xd_ptr =
					    (XD_STRUCT_PTR)
					    dTD_ptr->SCRATCH_PTR->
					    XD_FOR_THIS_DTD;
					if (xd_ptr) {
						buff_start_address =
						    xd_ptr->WSTARTADDRESS;
						actual_transfer_length =
						    xd_ptr->WTOTALLENGTH;
						temp_xd_ptr = xd_ptr;
					}

					/* Endif */
					/* Get the address of the next dTD */
					if (dTD_ptr->NEXT_TR_ELEM_PTR &
					    VUSBHS_TD_ADDR_MASK) {
						temp_dTD_ptr =
						    (VUSB20_EP_TR_STRUCT_PTR)
						    ((dTD_ptr->NEXT_TR_ELEM_PTR)
						     & VUSBHS_TD_ADDR_MASK);
					} else {
						temp_dTD_ptr =
						    (VUSB20_EP_TR_STRUCT_PTR)
						    (dTD_ptr->NEXT_TR_ELEM_PTR &
						     VUSBHS_TD_ADDR_MASK);
					}

					/* Read the errors */
					errors =
					    (dTD_ptr->SIZE_IOC_STS &
					     VUSBHS_TD_ERROR_MASK);

					if (!errors) {
						/* No errors */
						/* Get the length of transfer from the current dTD */
						remaining_length +=
						    ((dTD_ptr->SIZE_IOC_STS &
						      VUSB_EP_TR_PACKET_SIZE) >>
						     16);
						actual_transfer_length -=
						    remaining_length;
					} else {
						if (errors &
						    VUSBHS_TD_STATUS_HALTED) {
							/* Clear the errors and Halt condition */
							ep_queue_head_ptr->SIZE_IOC_INT_STS
							    &= ~errors;
						}	/* Endif */
					}	/* Endif */

					/* Retire the processed dTD */
					_usb_dci_vusb20_cancel_transfer(handle,
									ep_num,
									direction);
					if (temp_dTD_ptr) {
						if ((uint32_t)
						    temp_dTD_ptr->SCRATCH_PTR->
						    XD_FOR_THIS_DTD !=
						    (uint32_t) temp_xd_ptr) {
							/* Transfer complete. Call the register service function for the
							 ** endpoint
							 */
							_usb_device_call_service
							    (handle, ep_num,
							     FALSE,
							     (bool) direction,
							     buff_start_address,
							     actual_transfer_length,
							     errors);
							remaining_length = 0;
						}	/* Endif */
					} else {
						/* Transfer complete. Call the register service function for the
						 ** endpoint
						 */
						_usb_device_call_service(handle,
									 ep_num,
									 FALSE,
									 (bool)
									 direction,
									 buff_start_address,
									 actual_transfer_length,
									 errors);
					}	/* Endif */
					dTD_ptr = temp_dTD_ptr;
					errors = 0;
				}	/* Endwhile */
			}	/* Endif */
		}		/* Endfor */
	}
	/* Endif */
#ifdef _DEVICE_DEBUG_
	DEBUG_LOG_TRACE("_usb_dci_vusb20_process_tr_complete, SUCCESSFUL");
#endif

}				/* EndBody */

/*FUNCTION*-------------------------------------------------------------
*
*  Function Name  : _usb_dci_vusb20_isr
*  Returned Value : None
*  Comments       :
*        Services all the VUSB_HS interrupt sources
*
*END*-----------------------------------------------------------------*/
void _usb_dci_vusb20_isr(uint32_t int_num)
{				/* Body */
	USB_DEV_STATE_STRUCT_PTR usb_dev_ptr;
	VUSB20_REG_STRUCT_PTR dev_ptr;
	uint32_t status;

	usb_dev_ptr =
	    (USB_DEV_STATE_STRUCT_PTR) global_usb_device_state_struct_ptr;
#ifndef __USB_OTG__
	gInUSBDeviceIsr = 1;
#endif

	TOMBSTONE_USB_DEVICE_DEBUG(0x40500000);
#ifdef _DEVICE_DEBUG_
	DEBUG_LOG_TRACE("_usb_dci_vusb20_isr");
#endif

	dev_ptr = (VUSB20_REG_STRUCT_PTR) usb_dev_ptr->DEV_PTR;

	for (;;) {
		status =
		    dev_ptr->REGISTERS.OPERATIONAL_DEVICE_REGISTERS.USB_STS;
		os_thread_sleep(os_msec_to_ticks(1));
		if (!
		    (status & dev_ptr->REGISTERS.
		     OPERATIONAL_DEVICE_REGISTERS.USB_INTR)) {
			TOMBSTONE_USB_DEVICE_DEBUG(0x40500001);
			break;
		}

		/* Endif */
		/* Clear all the interrupts occured */
		dev_ptr->REGISTERS.OPERATIONAL_DEVICE_REGISTERS.USB_STS =
		    status;

		if (status & EHCI_STS_RESET) {
			TOMBSTONE_USB_DEVICE_DEBUG(0x40500002);
			_usb_dci_vusb20_process_reset((void *)usb_dev_ptr);
		}
		/* Endif */
#ifndef PORT_CHANGE_INTERRUPT_DISABLE
		if (status & EHCI_STS_PORT_CHANGE) {
			TOMBSTONE_USB_DEVICE_DEBUG(0x40500003);
			_usb_dci_vusb20_process_port_change((void *)
							    usb_dev_ptr);
		}		/* Endif */
#endif

#ifndef ERROR_INTERRUPT_DISABLE
		if (status & EHCI_STS_ERR) {
			TOMBSTONE_USB_DEVICE_DEBUG(0x40500004);
			_usb_dci_vusb20_process_error((void *)usb_dev_ptr);
		}		/* Endif */
#endif

#ifndef SOF_INTERRUPT_DISABLE
		if (status & EHCI_STS_SOF) {
			TOMBSTONE_USB_DEVICE_DEBUG(0x40500005);
			_usb_dci_vusb20_process_SOF((void *)usb_dev_ptr);
		}		/* Endif */
#endif

		if (status & EHCI_STS_INT) {
			TOMBSTONE_USB_DEVICE_DEBUG(0x40500006);
			_usb_dci_vusb20_process_tr_complete((void *)
							    usb_dev_ptr);
		}
		/* Endif */
#ifndef SUSPEND_INTERRUPT_DISABLE
		if (status & EHCI_STS_SUSPEND) {
			TOMBSTONE_USB_DEVICE_DEBUG(0x40500007);
			_usb_dci_vusb20_process_suspend((void *)usb_dev_ptr);
			return;
		}		/* Endif */
#endif

	}			/* Endfor */
#ifndef __USB_OTG__
	gInUSBDeviceIsr = 0;
#endif

#ifdef _DEVICE_DEBUG_
	DEBUG_LOG_TRACE("_usb_dci_vusb20_isr, SUCCESSFUL");
#endif
	TOMBSTONE_USB_DEVICE_DEBUG(0x40500099);
}				/* EndBody */

/*FUNCTION*-------------------------------------------------------------
*
*  Function Name  : _usb_dci_vusb20_process_reset
*  Returned Value : None
*  Comments       :
*        Services reset interrupt
*
*END*-----------------------------------------------------------------*/
void _usb_dci_vusb20_process_reset(
					  /* [IN] the USB_dev_initialize state structure */
					  _usb_device_handle handle)
{				/* Body */
	USB_DEV_STATE_STRUCT_PTR usb_dev_ptr;
	VUSB20_REG_STRUCT_PTR dev_ptr;
	uint32_t temp;
	uint8_t i;

#ifdef _DEVICE_DEBUG_
	DEBUG_LOG_TRACE("_usb_dci_vusb20_process_reset");
#endif

	usb_dev_ptr = (USB_DEV_STATE_STRUCT_PTR) handle;
	dev_ptr = (VUSB20_REG_STRUCT_PTR) usb_dev_ptr->DEV_PTR;

	/* Inform the application so that it can cancel all previously queued transfers */
	_usb_device_call_service(usb_dev_ptr, USB_SERVICE_BUS_RESET, FALSE,
				 FALSE, 0, 0, 0);

	/* The address bits are past bit 25-31. Set the address */
	dev_ptr->REGISTERS.OPERATIONAL_DEVICE_REGISTERS.DEVICE_ADDR &=
	    ~0xFE000000;

	/* Clear all the setup token semaphores */
	temp = dev_ptr->REGISTERS.OPERATIONAL_DEVICE_REGISTERS.ENDPT_SETUP_STAT;
	dev_ptr->REGISTERS.OPERATIONAL_DEVICE_REGISTERS.ENDPT_SETUP_STAT = temp;

	/* Clear all the endpoint complete status bits */
	temp = dev_ptr->REGISTERS.OPERATIONAL_DEVICE_REGISTERS.ENDPTCOMPLETE;
	dev_ptr->REGISTERS.OPERATIONAL_DEVICE_REGISTERS.ENDPTCOMPLETE = temp;

	while (dev_ptr->REGISTERS.
	       OPERATIONAL_DEVICE_REGISTERS.ENDPTPRIME & 0xFFFFFFFF) {
		/* Wait until all ENDPTPRIME bits cleared */
	}			/* Endif */

	/* Write 1s to the Flush register */
	dev_ptr->REGISTERS.OPERATIONAL_DEVICE_REGISTERS.ENDPTFLUSH = 0xFFFFFFFF;

	/* Unstall all endpoints */
	for (i = 0; i < usb_dev_ptr->MAX_ENDPOINTS; i++) {
		_usb_dci_vusb20_unstall_endpoint(handle, i, 0);
		_usb_dci_vusb20_unstall_endpoint(handle, i, 1);
	}			/* Endfor */

	if (dev_ptr->REGISTERS.OPERATIONAL_DEVICE_REGISTERS.PORTSCX[0] &
	    EHCI_PORTSCX_PORT_RESET) {
		usb_dev_ptr->BUS_RESETTING = TRUE;
		usb_dev_ptr->USB_STATE = USB_STATE_POWERED;
	} else {
		/* re-initialize */
		_usb_dci_vusb20_chip_initialize((void *)usb_dev_ptr);
#ifdef _DEVICE_DEBUG_
		DEBUG_LOG_TRACE
		    ("_usb_dci_vusb20_process_reset, SUCCESSFUL reinit hw");
#endif
		return;
	}			/* Endif */

	_usb_device_call_service(usb_dev_ptr, USB_SERVICE_BUS_RESET, FALSE,
				 FALSE, 0, 0, 0);

#ifdef _DEVICE_DEBUG_
	DEBUG_LOG_TRACE("_usb_dci_vusb20_process_reset, SUCCESSFUL");
#endif

}				/* EndBody */

#ifndef SUSPEND_INTERRUPT_DISABLE
/*FUNCTION*-------------------------------------------------------------
*
*  Function Name  : _usb_dci_vusb20_process_suspend
*  Returned Value : None
*  Comments       :
*        Services suspend interrupt
*
*END*-----------------------------------------------------------------*/
void _usb_dci_vusb20_process_suspend(
					    /* [IN] the USB_dev_initialize state structure */
					    _usb_device_handle handle)
{				/* Body */
	USB_DEV_STATE_STRUCT_PTR usb_dev_ptr;

#ifdef _DEVICE_DEBUG_
	DEBUG_LOG_TRACE("_usb_dci_vusb20_process_suspend");
#endif

	usb_dev_ptr = (USB_DEV_STATE_STRUCT_PTR) handle;

	usb_dev_ptr->USB_DEV_STATE_B4_SUSPEND = usb_dev_ptr->USB_STATE;

	usb_dev_ptr->USB_STATE = USB_STATE_SUSPEND;

	/* Inform the upper layers */
	_usb_device_call_service(usb_dev_ptr, USB_SERVICE_SLEEP, FALSE, FALSE,
				 0, 0, 0);

#ifdef __MISSING_RESUME_INTERRUPT_WORKAROUND__
	sof_after_suspend = 0;
#endif /* __MISSING_RESUME_INTERRUPT_WORKAROUND__ */

#ifdef _DEVICE_DEBUG_
	DEBUG_LOG_TRACE("_usb_dci_vusb20_process_suspend, SUCCESSFUL");
#endif

}				/* EndBody */
#endif

#ifndef SOF_INTERRUPT_DISABLE
/*FUNCTION*-------------------------------------------------------------
*
*  Function Name  : _usb_dci_vusb20_process_SOF
*  Returned Value : None
*  Comments       :
*        Services SOF interrupt
*
*END*-----------------------------------------------------------------*/
void _usb_dci_vusb20_process_SOF(
					/* [IN] the USB_dev_initialize state structure */
					_usb_device_handle handle)
{				/* Body */
	USB_DEV_STATE_STRUCT_PTR usb_dev_ptr;
	VUSB20_REG_STRUCT_PTR dev_ptr;

#ifdef _DEVICE_DEBUG_
	DEBUG_LOG_TRACE("_usb_dci_vusb20_process_SOF");
#endif

	usb_dev_ptr = (USB_DEV_STATE_STRUCT_PTR) handle;
	dev_ptr = (VUSB20_REG_STRUCT_PTR) usb_dev_ptr->DEV_PTR;

	/* Inform the upper layer */
	_usb_device_call_service(usb_dev_ptr, USB_SERVICE_SOF, 0, 0, 0,
				 dev_ptr->
				 REGISTERS.OPERATIONAL_DEVICE_REGISTERS.
				 USB_FRINDEX, 0);

#ifdef __MISSING_RESUME_INTERRUPT_WORKAROUND__
#ifdef __USB_OTG__
	/* 10 SOFs after the suspend means that we should provide
	   a resume callback */
	if ((usb_dev_ptr->USB_STATE == USB_STATE_SUSPEND) &&
	    (sof_after_suspend > TB_A_SUSPEND)) {
		usb_dev_ptr->USB_STATE = usb_dev_ptr->USB_DEV_STATE_B4_SUSPEND;
		/* Inform the upper layers */
		_usb_device_call_service(usb_dev_ptr, USB_SERVICE_RESUME, 0, 0,
					 0, 0, 0);
	}
#endif
#endif /* __MISSING_RESUME_INTERRUPT_WORKAROUND__ */

#ifdef _DEVICE_DEBUG_
	DEBUG_LOG_TRACE("_usb_dci_vusb20_process_SOF, SUCCESSFUL");
#endif

}				/* EndBody */
#endif

#ifndef PORT_CHANGE_INTERRUPT_DISABLE
/*FUNCTION*-------------------------------------------------------------
*
*  Function Name  : _usb_dci_vusb20_process_port_change
*  Returned Value : None
*  Comments       :
*        Services port change detect interrupt
*
*END*-----------------------------------------------------------------*/
void _usb_dci_vusb20_process_port_change(
						/* [IN] the USB_dev_initialize state structure */
						_usb_device_handle handle)
{				/* Body */
	USB_DEV_STATE_STRUCT_PTR usb_dev_ptr;
	VUSB20_REG_STRUCT_PTR dev_ptr;

#ifdef _DEVICE_DEBUG_
	DEBUG_LOG_TRACE("_usb_dci_vusb20_process_port_change");
#endif

	usb_dev_ptr = (USB_DEV_STATE_STRUCT_PTR) handle;
	dev_ptr = (VUSB20_REG_STRUCT_PTR) usb_dev_ptr->DEV_PTR;

	if (usb_dev_ptr->BUS_RESETTING) {
		/* Bus reset operation complete */
		usb_dev_ptr->BUS_RESETTING = FALSE;
	}
	/* Endif */
#ifdef __MISSING_RESUME_INTERRUPT_WORKAROUND__
	port_change_log[port_change_counter] =
	    dev_ptr->REGISTERS.OPERATIONAL_DEVICE_REGISTERS.PORTSCX[0];
	port_change_counter++;
#endif /* __MISSING_RESUME_INTERRUPT_WORKAROUND__ */

	if (!(dev_ptr->REGISTERS.OPERATIONAL_DEVICE_REGISTERS.PORTSCX[0] &
	      EHCI_PORTSCX_PORT_RESET)) {
		/* Get the speed */
		if (dev_ptr->REGISTERS.OPERATIONAL_DEVICE_REGISTERS.PORTSCX[0] &
		    EHCI_PORTSCX_PORT_HIGH_SPEED) {
			usb_dev_ptr->SPEED = USB_SPEED_HIGH;
		} else {
			usb_dev_ptr->SPEED = USB_SPEED_FULL;
		}		/* Endif */

		/* Inform the upper layers of the speed of operation */
		_usb_device_call_service(usb_dev_ptr,
					 USB_SERVICE_SPEED_DETECTION, FALSE,
					 FALSE, 0, usb_dev_ptr->SPEED, 0);

	}
	/* Endif */
#ifndef SMALL_CODE_SIZE
	if (dev_ptr->REGISTERS.OPERATIONAL_DEVICE_REGISTERS.PORTSCX[0] &
	    EHCI_PORTSCX_PORT_SUSPEND) {

		usb_dev_ptr->USB_DEV_STATE_B4_SUSPEND = usb_dev_ptr->USB_STATE;
		usb_dev_ptr->USB_STATE = USB_STATE_SUSPEND;

		/* Inform the upper layers */
		_usb_device_call_service(usb_dev_ptr, USB_SERVICE_SUSPEND,
					 FALSE, FALSE, 0, 0, 0);
	}			/* Endif */
#endif

	if (!(dev_ptr->REGISTERS.OPERATIONAL_DEVICE_REGISTERS.PORTSCX[0] &
	      EHCI_PORTSCX_PORT_SUSPEND) &&
	    (usb_dev_ptr->USB_STATE == USB_STATE_SUSPEND)) {
		usb_dev_ptr->USB_STATE = usb_dev_ptr->USB_DEV_STATE_B4_SUSPEND;
		/* Inform the upper layers */
		_usb_device_call_service(usb_dev_ptr, USB_SERVICE_RESUME, FALSE,
					 FALSE, 0, 0, 0);

#ifdef _DEVICE_DEBUG_
		DEBUG_LOG_TRACE
		    ("_usb_dci_vusb20_process_port_change, SUCCESSFUL, resumed");
#endif
		return;
	}
	/* Endif */
	usb_dev_ptr->USB_STATE = USB_STATE_DEFAULT;

#ifdef _DEVICE_DEBUG_
	DEBUG_LOG_TRACE("_usb_dci_vusb20_process_port_change, SUCCESSFUL");
#endif

}				/* EndBody */
#endif

#ifndef ERROR_INTERRUPT_DISABLE
/*FUNCTION*-------------------------------------------------------------
*
*  Function Name  : _usb_dci_vusb20_process_error
*  Returned Value : None
*  Comments       :
*        Services error interrupt
*
*END*-----------------------------------------------------------------*/
void _usb_dci_vusb20_process_error(
					  /* [IN] the USB_dev_initialize state structure */
					  _usb_device_handle handle)
{				/* Body */
	USB_DEV_STATE_STRUCT_PTR usb_dev_ptr;
#ifdef _DEVICE_DEBUG_
	DEBUG_LOG_TRACE("_usb_dci_vusb20_process_error");
#endif

	usb_dev_ptr = (USB_DEV_STATE_STRUCT_PTR) handle;

	/* Increment the error count */
	usb_dev_ptr->ERRORS++;

#ifdef _DEVICE_DEBUG_
	DEBUG_LOG_TRACE("_usb_dci_vusb20_process_error, SUCCESSFUL");
#endif

}				/* EndBody */
#endif
/*FUNCTION*-------------------------------------------------------------
*
*  Function Name  : _usb_dci_vusb20_set_speed_full
*  Returned Value : None
*  Comments       :
*        Force the controller port in full speed mode.
*
*END*-----------------------------------------------------------------*/
void _usb_dci_vusb20_set_speed_full(
					   /* [IN] the USB_dev_initialize state structure */
					   _usb_device_handle handle,
					   /* The port number on the device */
					   uint8_t port_number)
{				/* Body */
	USB_DEV_STATE_STRUCT_PTR usb_dev_ptr;
	VUSB20_REG_STRUCT_PTR dev_ptr;
	uint32_t port_control;

#ifdef _DEVICE_DEBUG_
	DEBUG_LOG_TRACE("_usb_dci_vusb20_set_speed_full");
#endif

	usb_dev_ptr = (USB_DEV_STATE_STRUCT_PTR) handle;
	dev_ptr = (VUSB20_REG_STRUCT_PTR) usb_dev_ptr->DEV_PTR;

	port_control =
	    dev_ptr->REGISTERS.
	    OPERATIONAL_DEVICE_REGISTERS.PORTSCX[port_number];
	port_control |= EHCI_PORTSCX_FORCE_FULL_SPEED_CONNECT;
	dev_ptr->REGISTERS.OPERATIONAL_DEVICE_REGISTERS.PORTSCX[port_number] =
	    port_control;

#ifdef _DEVICE_DEBUG_
	DEBUG_LOG_TRACE("_usb_dci_vusb20_set_speed_full, SUCCESSFUL");
#endif

}				/* EndBody */

/*FUNCTION*----------------------------------------------------------------
*
* Function Name  : _usb_dci_vusb20_clear_speed_full
* Returned Value : None
* Comments       :
*     Clear the Force Full Speed bit in the hardware.
*
*END*--------------------------------------------------------------------*/
void _usb_dci_vusb20_clear_speed_full(
					     /* [IN] the USB_dev_initialize state structure */
					     _usb_device_handle handle,
					     /* The port number on the device */
					     uint8_t port_number)
{				/* Body */
	USB_DEV_STATE_STRUCT_PTR usb_dev_ptr;
	VUSB20_REG_STRUCT_PTR dev_ptr;
	uint32_t port_control;

#ifdef _DEVICE_DEBUG_
	DEBUG_LOG_TRACE("_usb_dci_vusb20_clear_speed_full");
#endif

	usb_dev_ptr = (USB_DEV_STATE_STRUCT_PTR) handle;
	dev_ptr = (VUSB20_REG_STRUCT_PTR) usb_dev_ptr->DEV_PTR;

	port_control =
	    dev_ptr->REGISTERS.
	    OPERATIONAL_DEVICE_REGISTERS.PORTSCX[port_number];
	port_control &= ~EHCI_PORTSCX_FORCE_FULL_SPEED_CONNECT;
	dev_ptr->REGISTERS.OPERATIONAL_DEVICE_REGISTERS.PORTSCX[port_number] =
	    port_control;

#ifdef _DEVICE_DEBUG_
	DEBUG_LOG_TRACE("_usb_dci_vusb20_clear_speed_full, SUCCESSFUL");
#endif

}				/* EndBody */

/*FUNCTION*-------------------------------------------------------------
*
*  Function Name  : _usb_dci_vusb20_suspend_phy
*  Returned Value : None
*  Comments       :
*        Suspends the PHY in low power mode
*
*END*-----------------------------------------------------------------*/
void _usb_dci_vusb20_suspend_phy(
					/* [IN] the USB_dev_initialize state structure */
					_usb_device_handle handle,
					/* The port number on the device */
					uint8_t port_number)
{				/* Body */
	USB_DEV_STATE_STRUCT_PTR usb_dev_ptr;
	VUSB20_REG_STRUCT_PTR dev_ptr;
	uint32_t port_control;

#ifdef _DEVICE_DEBUG_
	DEBUG_LOG_TRACE("_usb_dci_vusb20_suspend_phy");
#endif

	usb_dev_ptr = (USB_DEV_STATE_STRUCT_PTR) handle;
	dev_ptr = (VUSB20_REG_STRUCT_PTR) usb_dev_ptr->DEV_PTR;

	port_control =
	    dev_ptr->REGISTERS.
	    OPERATIONAL_DEVICE_REGISTERS.PORTSCX[port_number];
	port_control |= EHCI_PORTSCX_PHY_CLOCK_DISABLE;
	dev_ptr->REGISTERS.OPERATIONAL_DEVICE_REGISTERS.PORTSCX[port_number] =
	    port_control;

#ifdef _DEVICE_DEBUG_
	DEBUG_LOG_TRACE("_usb_dci_vusb20_suspend_phy, SUCCESSFUL");
#endif

}				/* EndBody */

/*FUNCTION*-------------------------------------------------------------
*
*  Function Name  : _usb_dci_vusb20_set_address
*  Returned Value : None
*  Comments       :
*        Sets the newly assigned device address
*
*END*-----------------------------------------------------------------*/
void _usb_dci_vusb20_set_address(
					/* [IN] the USB_dev_initialize state structure */
					_usb_device_handle handle,
					/* Address of the device assigned by the host */
					uint8_t address)
{				/* Body */
	USB_DEV_STATE_STRUCT_PTR usb_dev_ptr;
	VUSB20_REG_STRUCT_PTR dev_ptr;

#ifdef _DEVICE_DEBUG_
	DEBUG_LOG_TRACE("_usb_dci_vusb20_set_address");
#endif

	usb_dev_ptr = (USB_DEV_STATE_STRUCT_PTR) handle;
	dev_ptr = (VUSB20_REG_STRUCT_PTR) usb_dev_ptr->DEV_PTR;

#ifdef SET_ADDRESS_HARDWARE_ASSISTANCE
   /***********************************************************
   Hardware Rev 4.0 onwards have special assistance built in
   for handling the set_address command. As per the USB specs
   a device should be able to receive the response on a new
   address, within 2 msecs after status phase of set_address is
   completed. Since 2 mili second may be a very small time window
   (on high interrupt latency systems) before software could
   come to the code below and write the device register,
   this routine will be called in advance when status phase of
   set_address is still not finished. The following line in the
   code will set the bit 24 to '1' and hardware will take
   the address and queue it in an internal buffer. From which
   it will use it to decode the next USB token. Please look
   at hardware rev details for the implementation of this
   assistance.

   Also note that writing bit 24 to 0x01 will not break
   any old hardware revs because it was an unused bit.
   ***********************************************************/
	/* The address bits are past bit 25-31. Set the address
	   also set the bit 24 to 0x01 to start hardware assitance */
	dev_ptr->REGISTERS.OPERATIONAL_DEVICE_REGISTERS.DEVICE_ADDR =
	    ((uint32_t) address << VUSBHS_ADDRESS_BIT_SHIFT) |
	    (0x01 << (VUSBHS_ADDRESS_BIT_SHIFT - 1));
#else
	dev_ptr->REGISTERS.OPERATIONAL_DEVICE_REGISTERS.DEVICE_ADDR =
	    (uint32_t) address << VUSBHS_ADDRESS_BIT_SHIFT;
#endif

	usb_dev_ptr->USB_STATE = USB_STATE_ADDRESS;

#ifdef _DEVICE_DEBUG_
	DEBUG_LOG_TRACE("_usb_dci_vusb20_set_address, SUCCESSFUL");
#endif

}				/* EndBody */

/*FUNCTION*-------------------------------------------------------------
*
*  Function Name  : _usb_dci_vusb20_get_setup_data
*  Returned Value : None
*  Comments       :
*        Reads the Setup data from the 8-byte setup buffer
*
*END*-----------------------------------------------------------------*/
void _usb_dci_vusb20_get_setup_data(
					   /* [IN] the USB_dev_initialize state structure */
					   _usb_device_handle handle,
					   /* [IN] the Endpoint number */
					   uint8_t ep_num,
					   /* [OUT] address of the buffer to read the setup data into */
					   uint8_t * buffer_ptr)
{				/* Body */
	USB_DEV_STATE_STRUCT_PTR usb_dev_ptr;
	VUSB20_REG_STRUCT_PTR dev_ptr;
	VUSB20_EP_QUEUE_HEAD_STRUCT_PTR ep_queue_head_ptr;
#ifdef TRIP_WIRE
	bool read_safe;
#endif

#ifdef _DEVICE_DEBUG_
	DEBUG_LOG_TRACE("_usb_dci_vusb20_get_setup_data");
#endif

	usb_dev_ptr = (USB_DEV_STATE_STRUCT_PTR) handle;
	dev_ptr = (VUSB20_REG_STRUCT_PTR) usb_dev_ptr->DEV_PTR;

	/* Get the endpoint queue head */
	ep_queue_head_ptr = (VUSB20_EP_QUEUE_HEAD_STRUCT_PTR)
	    (dev_ptr->REGISTERS.OPERATIONAL_DEVICE_REGISTERS.EP_LIST_ADDR)
	    + 2 * ep_num + USB_RECV;

   /********************************************************************
   CR 1219. Hardware versions 2.3+ have a implementation of tripwire
   semaphore mechanism that requires that we read the contents of
   QH safely by using the semaphore. Read the USBHS document to under
   stand how the code uses the semaphore mechanism. The following are
   the steps in brief

   1. USBCMD Write 1 to Setup Tripwire in register.
   2. Duplicate contents of dQH.StatusBuffer into local software byte
      array.
   3  Read Setup TripWire in register. (if set - continue; if
      cleared goto 1.)
   4. Write '0' to clear Setup Tripwire in register.
   5. Process setup packet using local software byte array copy and
      execute status/handshake phases.


   ********************************************************************/
#ifdef TRIP_WIRE		/*if semaphore mechanism is used the following code
				   is compiled in */
	read_safe = FALSE;
	while (!read_safe) {
      /*********************************************************
      start with setting the semaphores
      *********************************************************/

		dev_ptr->REGISTERS.OPERATIONAL_DEVICE_REGISTERS.USB_CMD |=
		    EHCI_CMD_SETUP_TRIPWIRE_SET;

      /*********************************************************
      Duplicate the contents of SETUP buffer to our buffer
      *********************************************************/
		/* Copy the setup packet to private buffer */
		USB_memcopy((uint8_t *) ep_queue_head_ptr->SETUP_BUFFER,
			    buffer_ptr, 8);
		USB_dcache_flush_and_invalidate((void *)
						ep_queue_head_ptr->SETUP_BUFFER,
						8);

      /*********************************************************
      If setup tripwire semaphore is cleared by hardware it means
      that we have a danger and we need to restart.
      else we can exit out of loop safely.
      *********************************************************/
		if (dev_ptr->REGISTERS.OPERATIONAL_DEVICE_REGISTERS.USB_CMD &
		    EHCI_CMD_SETUP_TRIPWIRE_SET) {
			read_safe = TRUE;	/* we can proceed exiting out of loop */
		}
	}

   /*********************************************************
   Clear the semaphore bit now
   *********************************************************/
	dev_ptr->REGISTERS.OPERATIONAL_DEVICE_REGISTERS.USB_CMD &=
	    EHCI_CMD_SETUP_TRIPWIRE_CLEAR;

#else
	/*when semaphore is not used */
	/* Copy the setup packet to private buffer */
	USB_memcopy((uint8_t *) ep_queue_head_ptr->SETUP_BUFFER, buffer_ptr, 8);
	USB_dcache_flush_and_invalidate((void *)ep_queue_head_ptr->SETUP_BUFFER,
					8);

#endif

	/* Clear the bit in the ENDPTSETUPSTAT */
	dev_ptr->REGISTERS.OPERATIONAL_DEVICE_REGISTERS.ENDPT_SETUP_STAT =
	    (1 << ep_num);

#ifdef _DEVICE_DEBUG_
	DEBUG_LOG_TRACE("_usb_dci_vusb20_get_setup_data, SUCCESSFUL");
#endif

}				/* EndBody */

/**
 * \brief change the zero length packet (zlp) setting in the hw to the zlpflag value
 * \param[in] handle The opened usb handle
 * \param[in] EP_NUM The endpoint number to change.
 * \param[in] zlpflag the Value to change it to.
 */
void usb_dci_vusb20_change_zlp(_usb_device_handle handle, uint32_t EP_NUM,
			       bool zlpflag)
{
	USB_DEV_STATE_STRUCT_PTR usb_dev_ptr;
	VUSB20_REG_STRUCT_PTR dev_ptr;
	VUSB20_EP_QUEUE_HEAD_STRUCT *ep_queue_head_ptr;

	// get the address we need.
	usb_dev_ptr = (USB_DEV_STATE_STRUCT_PTR) handle;
	dev_ptr = (VUSB20_REG_STRUCT_PTR) usb_dev_ptr->DEV_PTR;
	/* Get the endpoint queue head address in the hw */
	ep_queue_head_ptr = (VUSB20_EP_QUEUE_HEAD_STRUCT_PTR)
	    (dev_ptr->REGISTERS.OPERATIONAL_DEVICE_REGISTERS.EP_LIST_ADDR) +
	    2 * EP_NUM + USB_SEND;

	//
	// Set the zlp setting accordingly.
	//
	if (zlpflag) {
		// set the bit
		ep_queue_head_ptr->MAX_PKT_LENGTH |=
		    VUSB_EP_QUEUE_HEAD_ZERO_LEN_TER_SEL;
	} else {
		// clear the bit.
		ep_queue_head_ptr->MAX_PKT_LENGTH &=
		    ~VUSB_EP_QUEUE_HEAD_ZERO_LEN_TER_SEL;
	}

	USB_dcache_flush_and_invalidate((void *)ep_queue_head_ptr,
					sizeof(VUSB20_EP_QUEUE_HEAD_STRUCT));
}

/*FUNCTION*-------------------------------------------------------------
*
*  Function Name  : _usb_dci_vusb20_init_endpoint
*  Returned Value : None
*  Comments       :
*        Initializes the specified endpoint and the endpoint queue head
*
*END*-----------------------------------------------------------------*/
uint8_t _usb_dci_vusb20_init_endpoint(
					   /* [IN] the USB_dev_initialize state structure */
					   _usb_device_handle handle,
					   /* [IN] the transaction descriptor address */
					   XD_STRUCT_PTR xd_ptr)
{				/* Body */
	USB_DEV_STATE_STRUCT_PTR usb_dev_ptr;
	VUSB20_REG_STRUCT_PTR dev_ptr;
	VUSB20_EP_QUEUE_HEAD_STRUCT *ep_queue_head_ptr;
	uint32_t bit_pos;

#ifdef _DEVICE_DEBUG_
	DEBUG_LOG_TRACE("_usb_dci_vusb20_init_endpoint");
#endif

	usb_dev_ptr = (USB_DEV_STATE_STRUCT_PTR) handle;
	dev_ptr = (VUSB20_REG_STRUCT_PTR) usb_dev_ptr->DEV_PTR;

	/* Get the endpoint queue head address */
	ep_queue_head_ptr = (VUSB20_EP_QUEUE_HEAD_STRUCT_PTR)
	    (dev_ptr->REGISTERS.OPERATIONAL_DEVICE_REGISTERS.EP_LIST_ADDR) +
	    2 * xd_ptr->EP_NUM + xd_ptr->BDIRECTION;

	bit_pos = (1 << (16 * xd_ptr->BDIRECTION + xd_ptr->EP_NUM));

	/* Check if the Endpoint is Primed */
	if ((!
	     (dev_ptr->REGISTERS.
	      OPERATIONAL_DEVICE_REGISTERS.ENDPTPRIME & bit_pos)) &&
	    (!(dev_ptr->REGISTERS.
	       OPERATIONAL_DEVICE_REGISTERS.ENDPTSTATUS & bit_pos))) {
		/* Set the max packet length, interrupt on Setup and Mult fields */
		if (xd_ptr->EP_TYPE == USB_ISOCHRONOUS_ENDPOINT) {
			/* Mult bit should be set for isochronous endpoints */
			ep_queue_head_ptr->MAX_PKT_LENGTH =
			    ((xd_ptr->WMAXPACKETSIZE << 16) |
			     ((xd_ptr->MAX_PKTS_PER_UFRAME ?
			       xd_ptr->MAX_PKTS_PER_UFRAME : 1)
			      << VUSB_EP_QUEUE_HEAD_MULT_POS));
		} else {
			if (xd_ptr->EP_TYPE != USB_CONTROL_ENDPOINT) {
				ep_queue_head_ptr->MAX_PKT_LENGTH =
				    ((xd_ptr->
				      WMAXPACKETSIZE << 16) |
				     (xd_ptr->DONT_ZERO_TERMINATE ?
				      VUSB_EP_QUEUE_HEAD_ZERO_LEN_TER_SEL : 0));
			} else {
				ep_queue_head_ptr->MAX_PKT_LENGTH =
				    ((xd_ptr->WMAXPACKETSIZE << 16) |
				     VUSB_EP_QUEUE_HEAD_IOS);
			}	/* Endif */
		}		/* Endif */

		/* Enable the endpoint for Rx and Tx and set the endpoint type */
		dev_ptr->REGISTERS.
		    OPERATIONAL_DEVICE_REGISTERS.ENDPTCTRLX[xd_ptr->EP_NUM] |=
		    ((xd_ptr->BDIRECTION
		      ? (EHCI_EPCTRL_TX_ENABLE | EHCI_EPCTRL_TX_DATA_TOGGLE_RST)
		      : (EHCI_EPCTRL_RX_ENABLE |
			 EHCI_EPCTRL_RX_DATA_TOGGLE_RST)) | (xd_ptr->EP_TYPE <<
							     (xd_ptr->BDIRECTION
							      ?
							      EHCI_EPCTRL_TX_EP_TYPE_SHIFT
							      :
							      EHCI_EPCTRL_RX_EP_TYPE_SHIFT)));
	} else {
#ifdef _DEVICE_DEBUG_
		DEBUG_LOG_TRACE("_usb_dci_vusb20_init_endpoint, error ep init");
#endif
		return USBERR_EP_INIT_FAILED;
	}			/* Endif */

#ifdef _DEVICE_DEBUG_
	DEBUG_LOG_TRACE("_usb_dci_vusb20_init_endpoint, SUCCESSFUL");
#endif

	return USB_OK;

}				/* EndBody */

/*FUNCTION*-------------------------------------------------------------
*
*  Function Name  : _usb_dci_vusb20_get_transfer_status
*  Returned Value : USB_OK or error code
*  Comments       :
*        Gets the status of a transfer
*
*END*-----------------------------------------------------------------*/
uint8_t _usb_dci_vusb20_get_transfer_status(
						 /* [IN] the USB_dev_initialize state structure */
						 _usb_device_handle handle,
						 /* [IN] the Endpoint number */
						 uint8_t ep_num,
						 /* [IN] direction */
						 uint8_t direction)
{				/* Body */
	USB_DEV_STATE_STRUCT_PTR usb_dev_ptr;
	VUSB20_EP_TR_STRUCT_PTR dTD_ptr;
	XD_STRUCT_PTR xd_ptr;
	uint8_t status;

	usb_dev_ptr = (USB_DEV_STATE_STRUCT_PTR) handle;

#ifdef _DEVICE_DEBUG_
	DEBUG_LOG_TRACE("_usb_dci_vusb20_get_transfer_status");
#endif

	/* Unlink the dTD */
	dTD_ptr = usb_dev_ptr->EP_DTD_HEADS[2 * ep_num + direction];

	if (dTD_ptr) {
		/* Get the transfer descriptor for the dTD */
		xd_ptr = (XD_STRUCT_PTR) dTD_ptr->SCRATCH_PTR->XD_FOR_THIS_DTD;
		status = xd_ptr->BSTATUS;
	} else {
		status = USB_STATUS_IDLE;
	}			/* Endif */

#ifdef _DEVICE_DEBUG_
	DEBUG_LOG_TRACE("_usb_dci_vusb20_get_transfer_status, SUCCESSFUL");
#endif

	return (status);

}				/* EndBody */

/*FUNCTION*-------------------------------------------------------------
*
*  Function Name  : _usb_dci_vusb20_get_transfer_details
*  Returned Value : void * to structure that has details for transfer
*        Gets the status of a transfer
*
*END*-----------------------------------------------------------------*/
XD_STRUCT_PTR _usb_dci_vusb20_get_transfer_details(
							  /* [IN] the USB_dev_initialize state structure */
							  _usb_device_handle
							  handle,
							  /* [IN] the Endpoint number */
							  uint8_t ep_num,
							  /* [IN] direction */
							  uint8_t direction)
{				/* Body */
	USB_DEV_STATE_STRUCT_PTR usb_dev_ptr;
	VUSB20_REG_STRUCT_PTR dev_ptr;
	VUSB20_EP_TR_STRUCT_PTR dTD_ptr, temp_dTD_ptr;
	XD_STRUCT_PTR xd_ptr;
	uint32_t temp, remaining_bytes;
	VUSB20_EP_QUEUE_HEAD_STRUCT_PTR ep_queue_head_ptr;

	usb_dev_ptr = (USB_DEV_STATE_STRUCT_PTR) handle;
	dev_ptr = (VUSB20_REG_STRUCT_PTR) usb_dev_ptr->DEV_PTR;
	temp = (2 * ep_num + direction);

	/* get a void * to QH for this endpoint */
	ep_queue_head_ptr = (VUSB20_EP_QUEUE_HEAD_STRUCT_PTR)
	    (dev_ptr->REGISTERS.OPERATIONAL_DEVICE_REGISTERS.EP_LIST_ADDR) +
	    temp;

#ifdef _DEVICE_DEBUG_
	DEBUG_LOG_TRACE("_usb_dci_vusb20_get_transfer_status");
#endif

	/* Unlink the dTD */
	dTD_ptr = usb_dev_ptr->EP_DTD_HEADS[2 * ep_num + direction];

	if (dTD_ptr) {

		/* Get the transfer descriptor for the dTD */
		xd_ptr = (XD_STRUCT_PTR) dTD_ptr->SCRATCH_PTR->XD_FOR_THIS_DTD;
		if (!xd_ptr)
			return NULL;

		/* Initialize the transfer length field */
		xd_ptr->WSOFAR = 0;
		remaining_bytes = 0;

		/*if length of this transfer is greater than 20K
		   we have multiple DTDs to count */
		if (xd_ptr->WTOTALLENGTH > VUSB_EP_MAX_LENGTH_TRANSFER) {
			/* it is a valid DTD. We should parse all DTDs for this XD
			   and find the total bytes used so far */
			temp_dTD_ptr = dTD_ptr;

			/*loop through the list of DTDS until an active DTD is found
			   or list has finished */
			while (!
			       (dTD_ptr->NEXT_TR_ELEM_PTR &
				VUSBHS_TD_NEXT_TERMINATE)) {

	    /**********************************************************
            If this DTD has been overlayed, we take the actual length
            from QH.
            **********************************************************/

				if ((uint32_t)
				    (ep_queue_head_ptr->CURR_DTD_PTR &
				     VUSBHS_TD_ADDR_MASK) ==
				    ((uint32_t) temp_dTD_ptr)) {
					remaining_bytes +=
					    ((ep_queue_head_ptr->SIZE_IOC_INT_STS & VUSB_EP_TR_PACKET_SIZE) >> 16);
				} else {
					/* take the length from DTD itself */
					remaining_bytes +=
					    ((temp_dTD_ptr->SIZE_IOC_STS &
					      VUSB_EP_TR_PACKET_SIZE) >> 16);
				}

				dTD_ptr = temp_dTD_ptr;

				/* Get the address of the next dTD */
				if (temp_dTD_ptr->NEXT_TR_ELEM_PTR &
				    VUSBHS_TD_ADDR_MASK) {
					temp_dTD_ptr = (VUSB20_EP_TR_STRUCT_PTR)
					    ((temp_dTD_ptr->NEXT_TR_ELEM_PTR) &
					     VUSBHS_TD_ADDR_MASK);
				} else {
					temp_dTD_ptr = (VUSB20_EP_TR_STRUCT_PTR)
					    (temp_dTD_ptr->NEXT_TR_ELEM_PTR &
					     VUSBHS_TD_ADDR_MASK);
				}

			}
			xd_ptr->WSOFAR = xd_ptr->WTOTALLENGTH - remaining_bytes;

		} else {
			/*look at actual length from QH */
			xd_ptr->WSOFAR = xd_ptr->WTOTALLENGTH -
			    ((ep_queue_head_ptr->SIZE_IOC_INT_STS &
			      VUSB_EP_TR_PACKET_SIZE) >> 16);

		}

	} else {
		xd_ptr = NULL;
	}			/* Endif */

#ifdef _DEVICE_DEBUG_
	DEBUG_LOG_TRACE("_usb_dci_vusb20_get_transfer_status, SUCCESSFUL");
#endif

	return (xd_ptr);

}				/* EndBody */

/* EOF */
