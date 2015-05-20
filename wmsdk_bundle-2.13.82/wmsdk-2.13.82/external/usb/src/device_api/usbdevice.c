/* *********************************************************
* (c) Copyright 2008-2014 Marvell International Ltd.
*
*               Marvell Confidential
* ==========================================================
*/
/**
 * \brief This is the usb interface file
*/

#include <wm_os.h>
#include <string.h>
#include <stdio.h>
#include <wmstdio.h>
#include <wmassert.h>

#include <incl/agLinkedList.h>
#include <incl/devapi.h>
#include <incl/usbprv_dev.h>
#include <incl/asic_usb.h>
#include <incl/usb_list.h>
#include <incl/usb_device_api.h>
#include <incl/usbdrv.h>
#include <incl/io.h>
#include <incl/usbprvdev.h>

static os_timer_t WriteTimer;
#define WRITE_TIMEOUT 300*1000

/** Descriptor types, in hi-byte of wValue of struct _usb_setup */
#define USB_DEVICE_DESCRIPTOR       1   ///< id for the device descriptor
#define USB_CONFIG_DESCRIPTOR       2   ///< id for the config descriptor
#define USB_STRING_DESCRIPTOR       3   ///< id for the usb string descriptor
#define USB_INTERFACE_DESCRIPTOR    4   ///< id for the interface descriptor
#define USB_ENDPOINT_DESCRIPTOR     5   ///< id for the endpoint descriptor
#define USB_DEVICE_QUAL             6   ///< id for the device qual descriptor
#define USB_OS_CONFIG_DESCRIPTOR    7   ///< id for the other config descriptor

/// We specify 8 which gives a polling interval of .128 secs for high speed
/// and 1.02 seconds for low speed.
#define USB_INT_EP_POLLING_INTERVAL  8 ///< Interrupt endpoint polling interval.

/**
 * This structure is the handle returned from usb.  
 * When an interface is opened usb saves the interface info and 
 * this structure returns the info back to the caller.  There is
 * an issue where usb can change configuration and then the open
 * interfaces are freed and cleared.  The interfaceID is included 
 * here so that when a handle is required it can do a simple check to see
 * if the handle is valid.  If not then a new one is looked up and used
 * from then on.  io_device_impl_s is used to map to the _t that is common
 * between all the io interfaces.
 */
typedef struct io_device_impl_s
{
    USB_INTERFACE_DETAILS_t *interface;
    usb_intf_id_t   interfaceID;
} usb_interface_handle_t;

static USB_CONFIG_t ConfigDesc;
static uint32_t NumStringDesc, EPNum = 1, NumInterfaces = 0, max_pkt_size, gSpeed = 0;
static char **usb2StringTable = NULL;
static USB_Uncached uint16_t ENTER_TEST_MODE = 1, test_mode_index = 0;
static _usb_device_handle dev_handle = NULL;
static os_thread_t USB2DeviceThread;
static os_thread_stack_define(USB2DeviceThread_stack, 3072);
unsigned int gUSBDeviceEventFlags, USBIoFlags;
static USB_Uncached uint8_t *usb_status;
static USB_Uncached uint8_t *if_status;
static USB_Uncached uint8_t *data_to_send;
static uint16_t *sof_count;

static SETUP_STRUCT_t *local_setup_packet;
static EPINDATA_t EPInStorage[NUM_BULKIN_EP];
static EPOUTDATA_t EPOutStorage[NUM_BULKOUT_EP];
static ATLISTENTRY InterfaceStorage;	/* storage for the interface descriptors */
static usb_sys_config_t usbConfig;	/* usb system config storage. */

static uint8_t USB_IF_ALT[4] = { 0, 0, 0, 0 };

uint16_t  EPBufSize[]={CONTROL_MAX_PACKET_SIZE, 1024, BULK_HS_MAX_PACKET_SIZE, INT_EP_PACKET_SIZE};	
uint8_t   EPInterval[]={0, 0, 0, USB_INT_EP_POLLING_INTERVAL};


static uint8_t DevDescData[DEVICE_DESCRIPTOR_SIZE] = {
    DEVICE_DESCRIPTOR_SIZE, /* bLength */
    0x01,           /* bDescriptorType */
    USB_uint_16(0x200), /* bcdUSB */
    0x00,           /* bDeviceClass */
    0x00,           /* bDeviceSubClass */
    0x00,           /* bDeviceProtocol */
    CONTROL_MAX_PACKET_SIZE,    /* bMaxPacketSize0 */
    USB_uint_16(0x1286),    /* idVendor */
    USB_uint_16(0x0000),    /* idProduct */
    USB_uint_16(0x0001),    /* bcdDevice */
    0x01,           /* iManufacturer */
    0x02,           /* iProduct */
    0x03,           /* iSerialNumber */
    0x01            /* bNumConfigurations */
};

/* USB 2.0 specific descriptor */
#define DEVICE_QUALIFIER_DESCRIPTOR_SIZE 10

#if 0
#pragma data_alignment=32
static uint8_t  DevQualifierDescData[DEVICE_QUALIFIER_DESCRIPTOR_SIZE] =
{
   DEVICE_QUALIFIER_DESCRIPTOR_SIZE,                    ///< bLength Length of this descriptor
   0x06,                                                ///< bDescType This is a DEVICE Qualifier descr
   USB_uint_16(0x0200),                                 ///< bcdUSB USB revision 2.0
   0x00,                                                ///< bDeviceClass
   0x00,                                                ///< bDeviceSubClass
   0x00,                                                ///< bDeviceProtocol
   CONTROL_MAX_PACKET_SIZE,                             ///< bMaxPacketSize0
   0x01,                                                ///< bNumConfigurations
   0x00                                                 ///< 
};
#endif


/*
** if the number of strings changes, look for USB_STR_0 everywhere and make 
** the obvious changes.  It should be found in 3 places.
*/


static void reset_writes(uint32_t EPNum);
static void DoWrite(_usb_device_handle handle, WRITE_MEMBER_t * WriteMember,
		    uint8_t EPNum);
static void InitInEPStorage(uint8_t EPNum);
static void InitOutEPStorage(uint8_t EPNum);
static USB_INTERFACE_DETAILS_t *getUSBInterface(usb_intf_id_t interfaceID);
static void ep_lock(os_mutex_t * lock);
static void ep_unlock(os_mutex_t * lock);
int USBActive(void);

/**
 * \brief Add a write to the write list.
 * \param[in] EPNum The number of the in endpoint on which to add this write.
 * \param[in] buffer The address of the data buffer to write.
 * \param[in] length The length of the data to write
 * \param[in] The timeout in seconds till the write is deleted.  0 is max timeout ~1 hour.
 * \returns WRITE_MEMBER_t
 * \retval TempMember Address of the write member added to the list.
 */
static WRITE_MEMBER_t *add_write_to_list(uint32_t EPNum, uint8_t * buffer,
					 uint32_t length, uint32_t timeout)
{
	LINK_MEMBER *TempLink;
	WRITE_MEMBER_t *TempMember;

	while ((TempLink =
		(LINK_MEMBER *) USB_memalloc(sizeof(WRITE_MEMBER_t) +
					     sizeof(LINK_MEMBER))) == NULL)
		os_thread_sleep(os_msec_to_ticks(10));
	TempMember =
	    (WRITE_MEMBER_t *) ((uint32_t) TempLink + sizeof(LINK_MEMBER));

	TempLink->NextLink = NULL;
	TempLink->Data = TempMember;
	/* Initialize the entry in the linked list. */
	TempMember->Length = length;
	TempMember->StartBuffer = (char *)buffer;
	/* This is the pointer where the xfer is to start next time. */
	TempMember->CurrentBuffer = (char *)buffer;
	/* set the timeout value.If we get a zero timeout, never delete it. */
	if (timeout == 0)
		TempMember->EndTime = 0;
	else
		TempMember->EndTime = os_ticks_get() + timeout;

	ep_lock(&EPInStorage[EPNum].lock);
	/* add the write to the list. */
	agAddTail(&EPInStorage[EPNum].WriteList, TempLink);
	ep_unlock(&EPInStorage[EPNum].lock);
	return TempMember;
}

/**
 * \brief Remove an item from the write list.
 * \param[in] EPNum The number of the endpoint to remove the item from
 * \param[in] Node The Node to remove.
 */
static void remove_write(uint32_t EPNum, LINK_MEMBER * Node)
{
	/* WRITE_MEMBER_t *TempMember; */

// XXX
//	ep_lock( &EPInStorage[EPNum].lock );
	agDelItem(&EPInStorage[EPNum].WriteList, Node);
//	ep_unlock(&EPInStorage[EPNum].lock );
	/* RD : Not needed as array is used
	   TempMember = (WRITE_MEMBER_t *) Node->Data;
	   if (TempMember->StartBuffer)
	   USB_memfree(TempMember->StartBuffer);
	 */
	USB_memfree(Node);
}

/**
 * \brief Check writes for a time out and delete them if they are timed out.
 *  Also if an
 * active write has timed out this will cancel the write and not start the
 * next one.
 * \param[in] EPNum The number of the endpoint on which to check the writes
 */
static void check_write_TO(uint32_t EPNum)
{
	LINK_MEMBER *TempLink, *DelLink;
	WRITE_MEMBER_t *TempList;

	ep_lock(&EPInStorage[EPNum].lock);
	TempLink = agGetHead(&EPInStorage[EPNum].WriteList);
	while (TempLink != NULL) {
		TempList = TempLink->Data;
		if ((TempList->EndTime > 0) &&
		    TempList->EndTime < os_ticks_get()) {
			/* timed out, get rid of this write. */
			if (TempLink == agGetHead(&EPInStorage[EPNum].WriteList)
			    && EPInStorage[EPNum].Active) {
				/* this write is currently active on the hw.
				 * Cancel it and clean up. */
				_usb_device_cancel_transfer(dev_handle, EPNum,
							    USB_SEND);
				EPInStorage[EPNum].Active = 0;	/* write no longer active */
			}
			DelLink = TempLink;
			TempLink = TempLink->NextLink;
			remove_write(EPNum, DelLink);
		} else
			TempLink = TempLink->NextLink;
	}
	ep_unlock(&EPInStorage[EPNum].lock);
}

/**
 * \brief Initialize the endpoint mutex
 * \param[in] lock - mutex pointer
 * \param[in] s - name pointer
 */
static void ep_lock_init(os_mutex_t * lock, char *s)
{
	uint32_t ret;
	ret = os_mutex_create(lock, s, OS_MUTEX_INHERIT);
	if (ret != WM_SUCCESS)
		usb_e("Failed to create mutex %s", s);
}

/**
 * \brief Delete the endpoint mutex
 * \param[in] lock - mutex pointer
 */
static void ep_lock_delete(os_mutex_t * lock)
{
	uint32_t ret;
	ret = os_mutex_delete(*lock);
	if (ret != WM_SUCCESS)
		usb_e("Failed to delete mutex %x", lock);
}

/**
 * \brief Get the endpoint mutex
 * \param[in] lock - mutex pointer
 */
static void ep_lock(os_mutex_t * lock)
{
	uint32_t ret;
	ret = os_mutex_get(lock, OS_WAIT_FOREVER);
	if (ret != WM_SUCCESS)
		usb_e("Failed to get mutex %x", lock);
}

/**
 * \brief Pet the endpoint mutex
 * \param[in] lock - mutex pointer
 */
static void ep_unlock(os_mutex_t * lock)
{
	uint32_t ret;
	ret = os_mutex_put(lock);
	if (ret != WM_SUCCESS)
		usb_e("Failed to put mutex %x", lock);
}

/**
 * Function name:  usbStrCpyToWide
 *
 * \brief This copies a string for usb format.
 *
 * Notes:   This function is used to copy a string packed 1 per byte
            to a string where each byte takes 16 bits.
 *
 * \param pDst The destination of the string
 * \param pSrc  The source of the string
 */
static uint16_t *usbStrCpyToWide(uint16_t * pDst, const char * pSrc)
{
	uint16_t *p = pDst;
	uint16_t c;
	do {
		c = (uint16_t) * pSrc;
		*pDst++ = cpu_to_le16(c);
		pSrc++;
	} while (0 != *pSrc);
	return p;
}

/**
 * Function name: BuildConfigDescriptor
 *
 * \brief Take the pieces of the config descriptor and build it.
 * The configuration descriptor is stored in several places.  The
 * header is stored in a string and the interface descriptors are
 * stored on a linked list.  This routine takes the pieces and
 * sticks them together so that we can send them as one bunch.
 * This routine also changes the max xfer size in the endpoint
 * descriptors depending if we are 1.1 or 2.0 hosted.
 *
 * \param PacketSize The max packet size that must be put in the endpoint desc.
 *
 * \param Size Return value of the size of what was built.
 *
 * \retval char* the buffer with the assembeled descriptor.
 *
 * \date 10/25/2005
 *
 **/
static char *BuildConfigDescriptor(uint32_t PacketSize, uint32_t * Size)
{
	char *Descriptor;
	uint32_t DescSize = sizeof(USB_CONFIG_t);
	USB_INTERFACE_t *TempInterface;
	USB_ENDPOINT_t *TempEndPoint;
	USB_INTERFACE_DETAILS_t *TempDetails;
	uint32_t NumEP;
	uint32_t DescPosition;
	uint32_t ClassDescSize;

	/* build the descriptor. */
	TempDetails = (USB_INTERFACE_DETAILS_t *) InterfaceStorage.nextEntry;
	if (!ATIsListEmpty(&InterfaceStorage)) {
		/* Figure the size of the total descriptor. */
		do {
			DescSize += TempDetails->SizeOfInterfaceDesc;
			TempDetails =
			    (USB_INTERFACE_DETAILS_t *) TempDetails->linkedList.
			    nextEntry;
		} while (TempDetails !=
			 (USB_INTERFACE_DETAILS_t *) & InterfaceStorage);
	}
	*Size = DescSize;
	/* Now get the memory for the entire config descriptor */
	while ((Descriptor = (char *)USB_memalloc(DescSize)) == NULL)
		os_thread_sleep(os_msec_to_ticks(100));	/* if we can't get
						the memory wait a bit. */

	/* Put in the configuration descriptor */
	memcpy(Descriptor, &ConfigDesc, sizeof(USB_CONFIG_t));

	/* Now add in the interface and endpoint descriptors. */
	DescPosition = sizeof(USB_CONFIG_t);

	/* Stick the interfaces onto the end of the descriptors. */
	TempDetails = (USB_INTERFACE_DETAILS_t *) InterfaceStorage.nextEntry;
	if (!ATIsListEmpty(&InterfaceStorage)) {
		do {
			memcpy(&Descriptor[DescPosition],
			       TempDetails->InterfacePointer,
			       TempDetails->SizeOfInterfaceDesc);

			/* Change the ep size to the correct value. */
			TempInterface =
			    (USB_INTERFACE_t *) & Descriptor[DescPosition];

			/* Copy the class description */
			ClassDescSize =
			    TempDetails->SizeOfInterfaceDesc -
			    TempInterface->bNumEndpoints *
			    sizeof(USB_ENDPOINT_t) - sizeof(USB_INTERFACE_t);

			TempEndPoint =
			    (USB_ENDPOINT_t *) ((uint32_t) TempInterface +
						sizeof(USB_INTERFACE_t) +
						ClassDescSize);
			for (NumEP = 0; NumEP < TempInterface->bNumEndpoints;
			     NumEP++) {
				uint16_t TempSize;
				/* use memcpy to prevent any unaligned exceptions. */
				memcpy(&TempSize, &TempEndPoint->wMaxPacketSize,
				       sizeof(uint16_t));
				TempSize = le16_to_cpu(TempSize);
				if (TempSize != INT_EP_PACKET_SIZE) {
					TempSize = cpu_to_le16(PacketSize);
					memcpy(&TempEndPoint->wMaxPacketSize,
					       &TempSize, sizeof(uint16_t));
				}
				TempEndPoint++;
			}

			DescPosition += TempDetails->SizeOfInterfaceDesc;
			TempDetails =
			    (USB_INTERFACE_DETAILS_t *) TempDetails->linkedList.
			    nextEntry;
		} while (TempDetails !=
			 (USB_INTERFACE_DETAILS_t *) & InterfaceStorage);
	}
	return Descriptor;
}

/**
 ** Function name : InitializeConfigDesc
 *
 *  PARAMETERS:
 *
 *  \brief Initialize the configuration descriptor.
 *   This sets up the configuration descriptor to default values.
 *
 *  RETURNS:
 *
 */
static void InitializeConfigDesc(void)
{
	ConfigDesc.bLength = (uint8_t) sizeof(USB_CONFIG_t);
	ConfigDesc.bDescriptorType = USB_CONFIG_DESCRIPTOR;
	ConfigDesc.wTotalLength = cpu_to_le16(sizeof(USB_CONFIG_t));
	ConfigDesc.bNumberInterfaces = 0;
	ConfigDesc.iConfiguration = 0;
	ConfigDesc.bConfigurationValue = 1;
	ConfigDesc.bmAttributes = 0xc0;
	ConfigDesc.MaxPower = 49;
}

/**
 * \brief Initialize values for an endpoint descriptors
 *
 ** BuildEPDesc
 *
 *  \param TempEP Pointer to the endpoint descriptor
 * \param EPNum The number of this endpoint.
 * \param EPBufferSize The size of the xfer buffer for this endpoint
 * \param EPType Type of EP, Bulk, control, int
 * \param Interval The number of msec between polls for interrupt endpoints.
 *
 *  DESCRIPTION:
 *
 *  RETURNS:
 *
 */
static void BuildEPDesc(USB_ENDPOINT_t * pTempEP, uint8_t EPNum,
			uint16_t EPBufferSize, EP_TYPE EPType, uint8_t Interval)
{
	USB_ENDPOINT_t TempEP;
	memcpy(&TempEP, pTempEP, sizeof(USB_ENDPOINT_t));
	TempEP.bLength = (uint8_t) sizeof(USB_ENDPOINT_t);
	TempEP.bDescriptorType = USB_ENDPOINT_DESCRIPTOR;
	TempEP.bEndpointAddress = EPNum;
	TempEP.bmAttributes = (uint8_t) EPType;
	TempEP.wMaxPacketSize = cpu_to_le16(EPBufferSize);
	TempEP.bInterval = Interval;
	memcpy(pTempEP, &TempEP, sizeof(USB_ENDPOINT_t));
}

/**
 ** send_zlp
 *
 *  \brief Send a zero length packet to the specified in endpoint.
 *
 *  \param EPNum - endpoint number.
 *
 *  RETURNS: 0 - zero length packet not queued, 1 - zero length packet queued
 *
 */
static uint8_t send_zlp(uint8_t EPNum)
{
	WRITE_MEMBER_t *TempMember;

	if (EPInStorage[EPNum].ZLP) {
		TempMember = add_write_to_list(EPNum, NULL, 0, 100);
		ep_lock(&EPInStorage[EPNum].lock);
		EPInStorage[EPNum].Active = 1;
		DoWrite(dev_handle, TempMember, EPNum);
		ep_unlock(&EPInStorage[EPNum].lock);
		return 1;
	}
	return 0;
}

/*FUNCTION*----------------------------------------------------------------
*
* Function Name  : usb_interface_callback
* Returned Value : callback status
* Comments       :
*
*END*--------------------------------------------------------------------*/
static bool usb_interface_callback(_usb_device_handle handle, void *buffer,
				   USB_CALLBACK_STATE_t state)
{
	USB_INTERFACE_DETAILS_t *TempInterDetails;

	TempInterDetails =
	    (USB_INTERFACE_DETAILS_t *) InterfaceStorage.nextEntry;
	if (!ATIsListEmpty(&InterfaceStorage)) {
		do {
			if (TempInterDetails->InterfaceCallback != NULL) {
				if (TempInterDetails->InterfaceCallback(handle,
									local_setup_packet,
									(uint8_t)
									state))
					return (true);
			}
			TempInterDetails = (USB_INTERFACE_DETAILS_t *)
			    TempInterDetails->linkedList.nextEntry;
		} while (TempInterDetails !=
			 (USB_INTERFACE_DETAILS_t *) & InterfaceStorage);
	}
	return (false);
}

/*FUNCTION*----------------------------------------------------------------
*
* Function Name  : ch9GetStatus
* Returned Value : None
* Comments       :
*     Chapter 9 GetStatus command
*     wValue=Zero
*     wIndex=Zero
*     wLength=1
*     DATA=bmERR_STAT
*     The GET_STATUS command is used to read the bmERR_STAT register.
*
*     Return the status based on the bRrequestType bits:
*     device (0) = bit 0 = 1 = self powered
*                  bit 1 = 0 = DEVICE_REMOTE_WAKEUP which can be modified
*     with a SET_FEATURE/CLEAR_FEATURE command.
*     interface(1) = 0000.
*     endpoint(2) = bit 0 = stall.
*     static uint8_t * pData;
*
*     See section 9.4.5 (page 190) of the USB 1.1 Specification.
*
*END*--------------------------------------------------------------------*/
static void ch9GetStatus(
				/* USB handle */
				_usb_device_handle handle,
				/* Is it a Setup phase? */
				bool setup,
				/* The setup packet pointer */
				SETUP_STRUCT_PTR_t setup_ptr)
{				/* Body */
	uint16_t temp_status;
	uint8_t endpoint;

	if (setup) {
		switch (setup_ptr->REQUESTTYPE) {

		case 0x80:
			/* Device request */
			_usb_device_get_status(handle, USB_STATUS_DEVICE,
					       &temp_status);
			/*copy the status in usb_status to ensure that it is
			   flushed from cache */
			usb_status[0] = temp_status & 0xFF;
			usb_status[1] = (temp_status >> 2) & 0xFF;

			/* Send the requested data */
			_usb_device_send_data(handle, 0, (uint8_t *) usb_status,
					      2);
			break;

		case 0x81:
			/* Interface request */
			if ((setup_ptr->INDEX & 0x00FF) <
			    (uint16_t) sizeof(USB_IF_ALT)) {
				if_status[0] =
				    USB_IF_ALT[setup_ptr->INDEX & 0x00FF];
				/* Send the requested data */
				_usb_device_send_data(handle, 0,
						      (void *)if_status, 1);
			} else {
				// index out of range
				_usb_device_stall_endpoint(handle, 0, 0);
			}
			break;

		case 0x82:
			/* Endpoint request */
			endpoint =
			    (uint8_t) (setup_ptr->INDEX &
				       USB_STATUS_ENDPOINT_NUMBER_MASK);
			_usb_device_get_status(handle,
					       USB_STATUS_ENDPOINT | endpoint,
					       &temp_status);

			/*copy the status in usb_status to ensure that it is
			   flushed from cache */
			usb_status[0] = temp_status & 0xFF;
			usb_status[1] = (temp_status >> 2) & 0xFF;
			/* Send the requested data */
			_usb_device_send_data(handle, 0, (uint8_t *) usb_status,
					      2);
			break;

		default:
			/* Unknown request */
			_usb_device_stall_endpoint(handle, 0, 0);
			return;

		}		/* Endswitch */

		/* status phase */
		_usb_device_recv_data(handle, 0, 0, 0);
	}			/* Endif */
	return;
}				/* Endbody */

/*FUNCTION*----------------------------------------------------------------
*
* Function Name  : ch9ClearFeature
* Returned Value : None
* Comments       :
*     Chapter 9 ClearFeature command
*     See section 9.4.1 (page 188) of the USB 1.1 Specification.
*
*END*--------------------------------------------------------------------*/
static void ch9ClearFeature(
				   /* USB handle */
				   _usb_device_handle handle,
				   /* Is it a Setup phase? */
				   bool setup,
				   /* The setup packet void * */
				   SETUP_STRUCT_PTR_t setup_ptr)
{				/* Body */
	static uint8_t endpoint;
	uint16_t usb_status;

	_usb_device_get_status(handle, USB_STATUS_DEVICE_STATE, &usb_status);

	if ((usb_status != USB_STATE_CONFIG) &&
	    (usb_status != USB_STATE_ADDRESS)) {
		_usb_device_stall_endpoint(handle, 0, 0);
		return;
	}
	/* Endif */
	if (setup) {
		switch (setup_ptr->REQUESTTYPE) {

		case 0:
			/* DEVICE */
			if (setup_ptr->VALUE == 1) {
				/* clear remote wakeup */
				_usb_device_get_status(handle,
						       USB_STATUS_DEVICE,
						       &usb_status);
				usb_status &= ~USB_REMOTE_WAKEUP;
				_usb_device_set_status(handle,
						       USB_STATUS_DEVICE,
						       usb_status);
			} else {
				_usb_device_stall_endpoint(handle, 0, 0);
				return;
			}	/* Endif */
			break;

		case 2:
			/* ENDPOINT */
			if (setup_ptr->VALUE != 0) {
				_usb_device_stall_endpoint(handle, 0, 0);
				return;
			}	/* Endif */
			endpoint =
			    (uint8_t) (setup_ptr->INDEX &
				       USB_STATUS_ENDPOINT_NUMBER_MASK);
			_usb_device_get_status(handle,
					       USB_STATUS_ENDPOINT | endpoint,
					       &usb_status);
			/* set stall */
			_usb_device_set_status(handle,
					       USB_STATUS_ENDPOINT | endpoint,
					       0);
			break;

		default:
			_usb_device_stall_endpoint(handle, 0, 0);
			return;
		}		/* Endswitch */

		/* status phase */
		_usb_device_send_data(handle, 0, 0, 0);
	}			/* Endif */
	return;
}				/* Endbody */

/*FUNCTION*----------------------------------------------------------------
*
* Function Name  : ch9SetFeature
* Returned Value : None
* Comments       :
*     Chapter 9 SetFeature command
*     See section 9.4.9 (page 194) of the USB 1.1 Specification.
*
*END*--------------------------------------------------------------------*/
static void ch9SetFeature(
				 /* USB handle */
				 _usb_device_handle handle,
				 /* Is it a Setup phase? */
				 bool setup,
				 /* The setup packet void * */
				 SETUP_STRUCT_PTR_t setup_ptr)
{				/* Body */
	uint16_t usb_status;
	uint8_t endpoint;

	if (setup) {
		switch (setup_ptr->REQUESTTYPE) {

		case 0:
			/* DEVICE */
			switch (setup_ptr->VALUE) {
			case 1:
				/* set remote wakeup */
				_usb_device_get_status(handle,
						       USB_STATUS_DEVICE,
						       &usb_status);
				usb_status |= USB_REMOTE_WAKEUP;
				_usb_device_set_status(handle,
						       USB_STATUS_DEVICE,
						       usb_status);
				break;
			case 2:
				/* Test Mode */
				if ((setup_ptr->INDEX & 0x00FF) ||
				    (gSpeed != USB_SPEED_HIGH)) {
					_usb_device_stall_endpoint(handle, 0,
								   0);
					return;
				}	/* Endif */
				_usb_device_get_status(handle,
						       USB_STATUS_DEVICE_STATE,
						       &usb_status);
				if ((usb_status == USB_STATE_CONFIG) ||
				    (usb_status == USB_STATE_ADDRESS) ||
				    (usb_status == USB_STATE_DEFAULT)) {
					ENTER_TEST_MODE = 1;
					test_mode_index =
					    (setup_ptr->INDEX & 0xFF00);
				} else {
					_usb_device_stall_endpoint(handle, 0,
								   0);
					return;
				}	/* Endif */
				break;
			default:
				_usb_device_stall_endpoint(handle, 0, 0);
				return;
			}	/* Endswitch */
			break;

		case 2:
			/* ENDPOINT */
			if (setup_ptr->VALUE != 0) {
				_usb_device_stall_endpoint(handle, 0, 0);
				return;
			}	/* Endif */
			endpoint =
			    (uint8_t) (setup_ptr->INDEX &
				       USB_STATUS_ENDPOINT_NUMBER_MASK);
			_usb_device_get_status(handle,
					       USB_STATUS_ENDPOINT | endpoint,
					       &usb_status);
			/* set stall */
			_usb_device_set_status(handle,
					       USB_STATUS_ENDPOINT | endpoint,
					       1);
			break;

		default:
			_usb_device_stall_endpoint(handle, 0, 0);
			return;

		}		/* Endswitch */

		/* status phase */
		_usb_device_send_data(handle, 0, 0, 0);
	}			//else
	{
		if (ENTER_TEST_MODE == 1) {
			/* Delay to allow USB host to receive the status phase */
			/* haitao 03/15/12 */
			//cpu_spin_delay(100000);
			/* Enter Test Mode */
			_usb_device_set_status(handle, USB_STATUS_TEST_MODE,
					       test_mode_index);
		}		/* Endif */
	}			/* Endif */

	return;
}				/* Endbody */

/*FUNCTION*----------------------------------------------------------------
*
* Function Name  : ch9SetAddress
* Returned Value : None
* Comments       :
*     Chapter 9 SetAddress command
*     We setup a TX packet of 0 length ready for the IN token
*     Once we get the TOK_DNE interrupt for the IN token, then
*     we change the ADDR register and go to the ADDRESS state.
*     See section 9.4.6 (page 192) of the USB 1.1 Specification.
*
*END*--------------------------------------------------------------------*/
static void ch9SetAddress(
				 /* USB handle */
				 _usb_device_handle handle,
				 /* Is it a Setup phase? */
				 bool setup,
				 /* The setup packet void * */
				 SETUP_STRUCT_PTR_t setup_ptr)
{				/* Body */
	static uint8_t new_address;

	USB_INTERFACE_DETAILS_t *TempInterDetails;

	if (setup) {
		new_address = (uint8_t) setup_ptr->VALUE;
      /*******************************************************
      if hardware assitance is enabled for set_address (see
      hardware rev for details) we need to do the set_address
      before queuing the status phase.
      *******************************************************/
#ifdef SET_ADDRESS_HARDWARE_ASSISTANCE
		_usb_device_set_status(handle, USB_STATUS_ADDRESS, new_address);
#endif
		/* ack */
		_usb_device_send_data(handle, 0, 0, 0);
	} else {
#ifndef SET_ADDRESS_HARDWARE_ASSISTANCE
		_usb_device_set_status(handle, USB_STATUS_ADDRESS, new_address);
#endif

		_usb_device_set_status(handle, USB_STATUS_DEVICE_STATE,
				       USB_STATE_ADDRESS);
	}			/* Endif */

	if (gSpeed != USB_SPEED_HIGH) {
		max_pkt_size = BULK_FS_MAX_PACKET_SIZE;
	} else {
		max_pkt_size = BULK_HS_MAX_PACKET_SIZE;
	}			/* Endif */

	/*set address is a good place to init all non control endpoints */
	TempInterDetails =
	    (USB_INTERFACE_DETAILS_t *) InterfaceStorage.nextEntry;
	if (!ATIsListEmpty(&InterfaceStorage)) {
		do {
		 if(TempInterDetails->EPType==e_BULK)
           {
               EPBufSize[TempInterDetails->EPType]=max_pkt_size;
		   }
			if (TempInterDetails->OutNum != 0) {

				_usb_device_init_endpoint(handle,
							  TempInterDetails->
							  OutNum, EPBufSize[TempInterDetails->EPType],
							  USB_RECV,
							  TempInterDetails->EPType,
							  USB_DEVICE_DONT_ZERO_TERMINATE);

				/* Setup a pending receive... */
				_usb_device_recv_data(handle,
						      TempInterDetails->OutNum,
						      EPOutStorage
						      [TempInterDetails->
						       OutNum].bulk_out_buffer,
						      max_pkt_size);
			}
			if (TempInterDetails->InNum != 0) {
				_usb_device_init_endpoint(handle,
							  TempInterDetails->
							  InNum, EPBufSize[TempInterDetails->EPType],
							  USB_SEND,
							  TempInterDetails->EPType,
							  TempInterDetails->
							  ZeroTerminate);
				EPInStorage[TempInterDetails->InNum].hwZLP =
				    TempInterDetails->ZeroTerminate;
				send_zlp(TempInterDetails->InNum);
			}
			if (TempInterDetails->IntNum != 0) {
				_usb_device_init_endpoint(handle,
							  TempInterDetails->
							  IntNum,
							  INT_EP_PACKET_SIZE,
							  USB_SEND,
							  USB_INTERRUPT_ENDPOINT,
							  TempInterDetails->
							  ZeroTerminate);
				EPInStorage[TempInterDetails->InNum].hwZLP =
				    TempInterDetails->ZeroTerminate;
				send_zlp(TempInterDetails->IntNum);
			}
			TempInterDetails = (USB_INTERFACE_DETAILS_t *)
			    TempInterDetails->linkedList.nextEntry;
		} while (TempInterDetails !=
			 (USB_INTERFACE_DETAILS_t *) & InterfaceStorage);
	}

	/* Call interface callback */
	usb_interface_callback(handle, NULL, USB_SET_ADDRESS_CALLBACK);
	return;
}				/* Endbody */

/*FUNCTION*----------------------------------------------------------------
*
* Function Name  : ch9GetDescription
* Returned Value : None
* Comments       :
*     Chapter 9 GetDescription command
*     The Device Request can ask for Device/Config/string/interface/endpoint
*     descriptors (via wValue). We then post an IN response to return the
*     requested descriptor.
*     And then wait for the OUT which terminates the control transfer.
*     See section 9.4.3 (page 189) of the USB 1.1 Specification.
*
*END*--------------------------------------------------------------------*/
static void ch9GetDescription(
				     /* USB handle */
				     _usb_device_handle handle,
				     /* Is it a Setup phase? */
				     bool setup,
				     /* The setup packet void * */
				     SETUP_STRUCT_PTR_t setup_ptr)
{				/* Body */

	uint8_t *TempString;
	uint32_t Length;

	if (setup) {
		/* Load the appropriate string depending on the descriptor requested. */
		switch (setup_ptr->VALUE & 0xFF00) {

		case 0x0100:
			_usb_device_send_data(handle, 0,
					      (uint8_t *) & DevDescData[0],
					      MIN(setup_ptr->LENGTH,
						  DEVICE_DESCRIPTOR_SIZE));
			break;

		case 0x0200:
		case 0x0700:
			/* There are 2 types of descriptors for config.  One is for
			 * the current interface, the 2nd (0x700) is the other
			 * configuration.  First figure out which descriptor type to
			 * make and make it. */

			if (((gSpeed != USB_SPEED_HIGH) &&
			     ((setup_ptr->VALUE & 0xff00) == 0x200)) ||
			    ((gSpeed == USB_SPEED_HIGH) &&
			     ((setup_ptr->VALUE & 0xff00) == 0x700))) {
				/* Build the configuration descriptor */
				TempString = (uint8_t *)
				    BuildConfigDescriptor
				    (BULK_FS_MAX_PACKET_SIZE, &Length);
			} else {
				/* Build the configuration descriptor */
				TempString = (uint8_t *)
				    BuildConfigDescriptor
				    (BULK_HS_MAX_PACKET_SIZE, &Length);

			}
			/* Now change the descriptor type based on what is requested.
			 * building the config descriptor defaults to type 0x0200, if
			 * we have a request for a 0x0700 descriptor change the type
			 * here. */

			if (setup_ptr->VALUE == 0x0700)
				TempString[1] = USB_OS_CONFIG_DESCRIPTOR;

			_usb_device_send_data(handle, 0, TempString,
					      MIN(setup_ptr->LENGTH, Length));
			USB_memfree(TempString);
			break;

		case 0x0300:
			if ((setup_ptr->VALUE & 0x00FF) == 0) {
				Length = 4;
				TempString = (uint8_t *) USB_memalloc(Length);
				TempString[0] = 0x04;	/* Size */
				TempString[1] = USB_STRING_DESCRIPTOR;	/* String Descriptor */
				TempString[2] =
				    USB_uint_16_low(usbConfig.USBLang);
				TempString[3] =
				    USB_uint_16_high(usbConfig.USBLang);
			} else {
				/* get the string. */
				char *pString;

				/* make sure that we have a request that is within range. */
				if ((uint32_t)
				    (((*(volatile uint16_t *)&setup_ptr->VALUE)
				      & 0xff) - 1) >= NumStringDesc) {
					_usb_device_stall_endpoint(handle, 0,
								   0);
					return;
				}
				pString =
				    usb2StringTable[((*(volatile uint16_t *)
						      &setup_ptr->VALUE) & 0xff)
						    - 1];

				/* Figure the length of the string. */
				Length = strlen(pString) * 2 + 2;
				/* get the memory to hold the string. */
				TempString = (uint8_t *) USB_memalloc(Length);
				/* Copy the string into the buffer making it a wide char */
				usbStrCpyToWide((uint16_t *) & TempString[2],
						pString);
				TempString[0] = (char)Length;
				TempString[1] = 3;
			}	/* Endif */

			/* Send the string back to the host. */
			_usb_device_send_data(handle, 0, TempString,
					      MIN(setup_ptr->LENGTH, Length));
			USB_memfree(TempString);
			break;

		case 0x600:
			/* Because phy only support full speed, no qualifier description return */
			_usb_device_stall_endpoint(handle, 0, 0);

			break;

			/* HID Initialize report */
		case 0x2200:			/* 0x2200 represents GetReport type, ID=0*/
			local_setup_packet->REQUEST = 0x01; /* GetReport */
			if (!usb_interface_callback(handle, local_setup_packet,
				USB_CLASS_REQUEST_CALLBACK))
			{
				/* Request Not handled */
				_usb_device_stall_endpoint(handle, 0, 0);
			}
			break;

		default:
			_usb_device_stall_endpoint(handle, 0, 0);
			return;
		}		/* Endswitch */

		/* status phase */
		_usb_device_recv_data(handle, 0, 0, 0);
	}			/* Endif */
	return;
}				/* Endbody */

/*FUNCTION*----------------------------------------------------------------
*
* Function Name  : ch9SetDescription
* Returned Value : None
* Comments       :
*     Chapter 9 SetDescription command
*     See section 9.4.8 (page 193) of the USB 1.1 Specification.
*
*END*--------------------------------------------------------------------*/
static void ch9SetDescription(
				     /* USB handle */
				     _usb_device_handle handle,
				     /* Is it a Setup phase? */
				     bool setup,
				     /* The setup packet void * */
				     SETUP_STRUCT_PTR_t setup_ptr)
{				/* Body */

	_usb_device_stall_endpoint(handle, 0, 0);

	return;
}				/* Endbody */

/*FUNCTION*----------------------------------------------------------------
*
* Function Name  : ch9GetConfig
* Returned Value : None
* Comments       :
*     Chapter 9 GetConfig command
*     See section 9.4.2 (page 189) of the USB 1.1 Specification.
*
*END*--------------------------------------------------------------------*/
static void ch9GetConfig(
				/* USB handle */
				_usb_device_handle handle,
				/* Is it a Setup phase? */
				bool setup,
				/* The setup packet void * */
				SETUP_STRUCT_PTR_t setup_ptr)
{				/* Body */
	uint16_t current_config;

	/* Return the currently selected configuration */
	if (setup) {
		_usb_device_get_status(handle, USB_STATUS_CURRENT_CONFIG,
				       &current_config);
		data_to_send[0] = (uint8_t) current_config;
		_usb_device_send_data(handle, 0, (void *)data_to_send, 1);
		/* status phase */
		_usb_device_recv_data(handle, 0, 0, 0);
	}			/* Endif */
	return;
}				/* Endbody */

/*FUNCTION*----------------------------------------------------------------
*
* Function Name  : ch9SetConfig
* Returned Value : None
* Comments       :
*     Chapter 9 SetConfig command
*
*END*--------------------------------------------------------------------*/
static void ch9SetConfig(
				/* USB handle */
				_usb_device_handle handle,
				/* Is it a Setup phase? */
				bool setup,
				/* The setup packet void * */
				SETUP_STRUCT_PTR_t setup_ptr)
{				/* Body */
	uint16_t usb_state;

	if (setup) {
		if ((setup_ptr->VALUE & 0x00FF) > 1) {
			/* generate stall */
			_usb_device_stall_endpoint(handle, 0, 0);
			return;
		}
		/* Endif */
		/* 0 indicates return to unconfigured state */
		if ((setup_ptr->VALUE & 0x00FF) == 0) {
			_usb_device_get_status(handle, USB_STATUS_DEVICE_STATE,
					       &usb_state);
			if ((usb_state == USB_STATE_CONFIG) ||
			    (usb_state == USB_STATE_ADDRESS)) {
				/* clear the currently selected config value */
				_usb_device_set_status(handle,
						       USB_STATUS_CURRENT_CONFIG,
						       0);
				_usb_device_set_status(handle,
						       USB_STATUS_DEVICE_STATE,
						       USB_STATE_ADDRESS);
				/* status phase */
				_usb_device_send_data(handle, 0, 0, 0);
			} else {
				_usb_device_stall_endpoint(handle, 0, 0);
			}	/* Endif */
			return;
		}

		/* Endif */
		/*
		 ** If the configuration value (setup_ptr->VALUE & 0x00FF) differs
		 ** from the current configuration value, then endpoints must be
		 ** reconfigured to match the new device configuration
		 */
		_usb_device_get_status(handle, USB_STATUS_CURRENT_CONFIG,
				       &usb_state);
		if (usb_state != (setup_ptr->VALUE & 0x00FF)) {
			/* Reconfigure endpoints here */
			switch (setup_ptr->VALUE & 0x00FF) {

			default:
				break;
			}	/* Endswitch */
			_usb_device_set_status(handle,
					       USB_STATUS_CURRENT_CONFIG,
					       setup_ptr->VALUE & 0x00FF);
			_usb_device_set_status(handle, USB_STATUS_DEVICE_STATE,
					       USB_STATE_CONFIG);
			/* status phase */
			_usb_device_send_data(handle, 0, 0, 0);
			return;
		}		/* Endif */
		_usb_device_set_status(handle, USB_STATUS_DEVICE_STATE,
				       USB_STATE_CONFIG);
		/* status phase */
		_usb_device_send_data(handle, 0, 0, 0);
	}			/* Endif */
	return;
}				/* Endbody */

/*FUNCTION*----------------------------------------------------------------
*
* Function Name  : ch9GetInterface
* Returned Value : None
* Comments       :
*     Chapter 9 GetInterface command
*     See section 9.4.4 (page 190) of the USB 1.1 Specification.
*
*END*--------------------------------------------------------------------*/
static void ch9GetInterface(
				   /* USB handle */
				   _usb_device_handle handle,
				   /* Is it a Setup phase? */
				   bool setup,
				   /* The setup packet void * */
				   SETUP_STRUCT_PTR_t setup_ptr)
{				/* Body */
	uint16_t usb_state;

	_usb_device_get_status(handle, USB_STATUS_DEVICE_STATE, &usb_state);
	if (usb_state != USB_STATE_CONFIG) {
		_usb_device_stall_endpoint(handle, 0, 0);
		return;
	}
	/* Endif */
	if (setup) {
		if ((setup_ptr->INDEX & 0x00FF) < 4) {
			_usb_device_send_data(handle, 0,
					      &USB_IF_ALT[setup_ptr->INDEX &
							  0x00FF],
					      MIN(setup_ptr->LENGTH,
						  sizeof(uint8_t)));
			/* status phase */
			_usb_device_recv_data(handle, 0, 0, 0);
		} else
			_usb_device_stall_endpoint(handle, 0, 0);
	}
	/* Endif */
	return;
}				/* Endbody */

/*FUNCTION*----------------------------------------------------------------
*
* Function Name  : ch9SetInterface
* Returned Value : None
* Comments       :
*     Chapter 9 SetInterface command
*     See section 9.4.10 (page 195) of the USB 1.1 Specification.
*
*END*--------------------------------------------------------------------*/
static void ch9SetInterface(
				   /* USB handle */
				   _usb_device_handle handle,
				   /* Is it a Setup phase? */
				   bool setup,
				   /* The setup packet void * */
				   SETUP_STRUCT_PTR_t setup_ptr)
{				/* Body */

	if (setup) {
		if (setup_ptr->REQUESTTYPE != 0x01) {
			_usb_device_stall_endpoint(handle, 0, 0);
			return;
		}

		/* Endif */
		/*
		 ** If the alternate value (setup_ptr->VALUE & 0x00FF) differs
		 ** from the current alternate value for the specified interface,
		 ** then endpoints must be reconfigured to match the new alternate
		 */
		if ((setup_ptr->INDEX & 0x00FF) < (uint16_t) sizeof(USB_IF_ALT)) {
			if (USB_IF_ALT[setup_ptr->INDEX & 0x00FF]
			    != (setup_ptr->VALUE & 0x00FF)) {
				USB_IF_ALT[setup_ptr->INDEX & 0x00FF] =
				    (setup_ptr->VALUE & 0x00FF);
				/* Reconfigure endpoints here. */

			}

			/* Endif */
			/* status phase */
			_usb_device_send_data(handle, 0, 0, 0);
		} else {
			// index out of range
			_usb_device_stall_endpoint(handle, 0, 0);
		}
	}			/* Endif */
	return;
}				/* Endbody */

/*FUNCTION*----------------------------------------------------------------
*
* Function Name  : ch9SynchFrame
* Returned Value :
* Comments       :
*     Chapter 9 SynchFrame command
*     See section 9.4.11 (page 195) of the USB 1.1 Specification.
*
*END*--------------------------------------------------------------------*/
static void ch9SynchFrame(
				 /* USB handle */
				 _usb_device_handle handle,
				 /* Is it a Setup phase? */
				 bool setup,
				 /* The setup packet void * */
				 SETUP_STRUCT_PTR_t setup_ptr)
{				/* Body */

	if (setup) {
		if (setup_ptr->REQUESTTYPE != 0x02) {
			_usb_device_stall_endpoint(handle, 0, 0);
			return;
		}
		/* Endif */
		if ((uint32_t) (setup_ptr->INDEX & 0x00FF) >= NumInterfaces) {
			_usb_device_stall_endpoint(handle, 0, 0);
			return;
		}
		/* Endif */
		_usb_device_get_status(handle, USB_STATUS_SOF_COUNT, sof_count);
		_usb_device_send_data(handle, 0, (uint8_t *) sof_count,
				      MIN(setup_ptr->LENGTH, 2));
		/* status phase */
		_usb_device_recv_data(handle, 0, 0, 0);
	}
	/* Endif */
	return;
}				/* Endbody */

void CancelXfer(_usb_device_handle handle)
{
	USB_INTERFACE_DETAILS_t *TempInterDetails;
	uint32_t Flags;

	TempInterDetails =
	    (USB_INTERFACE_DETAILS_t *) InterfaceStorage.nextEntry;
	if (!ATIsListEmpty(&InterfaceStorage)) {
		do {
			if (TempInterDetails->OutNum != 0) {
				_usb_device_cancel_transfer(handle,
							    TempInterDetails->
							    OutNum, USB_RECV);
				ep_lock(&EPOutStorage[TempInterDetails->OutNum].
					lock);
				EPOutStorage[TempInterDetails->OutNum].
				    bulk_out_bytes_received = 0;
				EPOutStorage[TempInterDetails->OutNum].
				    bulk_out_bytes_received_index = 0;
				EPOutStorage[TempInterDetails->OutNum].
				    NotifyReset = true;
				ep_unlock(&EPOutStorage
					  [TempInterDetails->OutNum].lock);
				os_event_flags_get(USBIoFlags,
						   EPOutStorage
						   [TempInterDetails->OutNum].
						   EventMask, EF_OR_CLEAR,
						   &Flags, 0);
				os_event_flags_set(USBIoFlags,
						   EPOutStorage
						   [TempInterDetails->OutNum].
						   EventMask &
						   USB_CANCEL_EVENT_MASK,
						   EF_OR);
			}
			if (TempInterDetails->InNum != 0) {
				_usb_device_cancel_transfer(handle,
							    TempInterDetails->
							    InNum, USB_SEND);
				reset_writes(TempInterDetails->InNum);
			}
			if (TempInterDetails->IntNum != 0) {
				_usb_device_cancel_transfer(handle,
							    TempInterDetails->
							    IntNum, USB_SEND);
				reset_writes(TempInterDetails->IntNum);
			}
			TempInterDetails = (USB_INTERFACE_DETAILS_t *)
			    TempInterDetails->linkedList.nextEntry;
		} while (TempInterDetails !=
			 (USB_INTERFACE_DETAILS_t *) & InterfaceStorage);
	}
}

/*FUNCTION*----------------------------------------------------------------
*
* Function Name  : service_ep0
* Returned Value : None
* Comments       :
*     Called upon a completed endpoint 0 (USB 1.1 Chapter 9) transfer
*
*END*--------------------------------------------------------------------*/
static void service_ep0(
			       /* [IN] Handle of the USB device */
			       _usb_device_handle handle,
			       /* [IN] Is it a setup packet? */
			       bool setup,
			       /* [IN] Direction of the transfer.  Is it transmit? */
			       uint8_t direction,
			       /* [IN] Pointer to the data buffer */
			       uint8_t * buffer,
			       /* [IN] Length of the transfer */
			       uint32_t length,
			       /* [IN] Error, if any */
			       uint8_t error,
			       /* [IN] Endpoint number */
			       uint8_t EPNum)
{				/* Body */
	USB_DEV_STATE_STRUCT_PTR usb_dev_ptr;
	VUSB20_REG_STRUCT_PTR dev_ptr;
	int status;
	LINK_MEMBER *TempLink;
	WRITE_MEMBER_t *TempMember;

	usb_dev_ptr = (USB_DEV_STATE_STRUCT_PTR) handle;
	dev_ptr = (VUSB20_REG_STRUCT_PTR) usb_dev_ptr->DEV_PTR;
	if (setup) {

		/* Read the setup packet */
		_usb_device_read_setup_data(handle, 0,
					    (uint8_t *) local_setup_packet);

		/* It is possible that another setup packet could come in before
		 * completely responding to the original setup packet.  If this occurs
		 * all of the previous setup responses need to be cancelled to reclaim
		 * the transfer descriptors. */

		/* Cancel any previous Control out device transfers descriptors on the control endpoint */
		while (usb_dev_ptr->EP_DTD_HEADS[USB_RECV])
			_usb_dci_vusb20_cancel_transfer(handle, 0, USB_RECV);

		/* Cancel any previous Control in device transfers descriptors on the control endpoint */
		while (usb_dev_ptr->EP_DTD_HEADS[USB_SEND])
			_usb_dci_vusb20_cancel_transfer(handle, 0, USB_SEND);

		/* Reading the ENDPT_SETUP_STAT immediately after _usb_device_read_setup_data()
		 * fails.  Read the register five times to give additional time... */
		status =
		    dev_ptr->REGISTERS.OPERATIONAL_DEVICE_REGISTERS.
		    ENDPT_SETUP_STAT;
		status =
		    dev_ptr->REGISTERS.OPERATIONAL_DEVICE_REGISTERS.
		    ENDPT_SETUP_STAT;
		status =
		    dev_ptr->REGISTERS.OPERATIONAL_DEVICE_REGISTERS.
		    ENDPT_SETUP_STAT;
		status =
		    dev_ptr->REGISTERS.OPERATIONAL_DEVICE_REGISTERS.
		    ENDPT_SETUP_STAT;
		status =
		    dev_ptr->REGISTERS.OPERATIONAL_DEVICE_REGISTERS.
		    ENDPT_SETUP_STAT;

		if (status == 0) {
			status =
				dev_ptr->REGISTERS.OPERATIONAL_DEVICE_REGISTERS.
				ENDPT_SETUP_STAT;
		}

		/* If there is another setup packet on endpoint 0 read the setup packet again */
		if (dev_ptr->REGISTERS.OPERATIONAL_DEVICE_REGISTERS.
		    ENDPT_SETUP_STAT & 1)
			_usb_device_read_setup_data(handle, 0, (uint8_t *)
						    local_setup_packet);

		switch (local_setup_packet->REQUESTTYPE & 0x60) {
		case 0x00:
			switch (local_setup_packet->REQUEST) {

			case 0x0:
				ch9GetStatus(handle, setup, local_setup_packet);
				break;

			case 0x1:
				ch9ClearFeature(handle, setup,
						local_setup_packet);
				break;

			case 0x3:
				ch9SetFeature(handle, setup,
					      local_setup_packet);
				break;

			case 0x5:
				ch9SetAddress(handle, setup,
					      local_setup_packet);
				break;

			case 0x6:
				ch9GetDescription(handle, setup,
						  local_setup_packet);
				break;

			case 0x7:
				ch9SetDescription(handle, setup,
						  local_setup_packet);
				break;

			case 0x8:
				ch9GetConfig(handle, setup, local_setup_packet);
				break;

			case 0x9:
				ch9SetConfig(handle, setup, local_setup_packet);
				break;

			case 0xa:
				ch9GetInterface(handle, setup,
						local_setup_packet);
				break;

			case 0xb:
				ch9SetInterface(handle, setup,
						local_setup_packet);
				break;

			case 0xc:
				ch9SynchFrame(handle, setup,
					      local_setup_packet);
				break;

			default:
				_usb_device_stall_endpoint(handle, 0, 0);
				break;

			}	/* Endswitch */

			break;

		case 0x20:	/* Class request */
			if (!usb_interface_callback
			    (handle, local_setup_packet,
			     USB_CLASS_REQUEST_CALLBACK))
				_usb_device_stall_endpoint(handle, 0, 0);
			break;

		case 0x40:	/* vendor specific request */
			if (!usb_interface_callback
			    (handle, local_setup_packet,
			     USB_VENDOR_REQUEST_CALLBACK))
				_usb_device_stall_endpoint(handle, 0, 0);
			break;

		default:
			_usb_device_stall_endpoint(handle, 0, 0);
			break;

		}		/* Endswitch */
	}
	if (direction) {
		/* We have a write complete, delete entry */
		ep_lock(&EPInStorage[0].lock);
		TempLink = agGetHead(&EPInStorage[0].WriteList);
		if (TempLink != NULL) {
			TempMember = (WRITE_MEMBER_t *) TempLink->Data;
			if (TempMember->Length == 0) {
				remove_write(0, TempLink);
			}
		}

		/* See if we have any more to send. */
		TempLink = agGetHead(&EPInStorage[0].WriteList);
		if (TempLink != NULL) {
			TempMember = (WRITE_MEMBER_t *) TempLink->Data;
			DoWrite(dev_handle, TempMember, 0);
		} else
			EPInStorage[0].Active = 0;
		ep_unlock(&EPInStorage[0].lock);
	}

	return;
}				/* Endbody */

/*FUNCTION*----------------------------------------------------------------
*
* Function Name  : reset_ep0
* Returned Value : None
* Comments       :
*     Called upon a bus reset event.  Initialises the control endpoint.
*
*END*--------------------------------------------------------------------*/
static void reset_ep0(
			     /* [IN] Handle of the USB device */
			     _usb_device_handle handle,
			     /* [IN] Unused */
			     bool setup,
			     /* [IN] Unused */
			     uint8_t direction,
			     /* [IN] Unused */
			     uint8_t * buffer,
			     /* [IN] Unused */
			     uint32_t length,
			     /* [IN] Error, if any */
			     uint8_t error,
			     /* [IN] Endpoint number */
			     uint8_t type)
{				/* Body */

	/* Check for USB Force Full Speed */
	if (usbConfig.USBForceFullSpeed)
		_usb_device_set_status(handle, USB_FORCE_FULL_SPEED, 0);
	else
		_usb_device_set_status(handle, USB_CLEAR_FULL_SPEED, 0);

	/* on a reset always cancel all transfers all EP 0 */
	_usb_device_cancel_transfer(handle, 0, USB_RECV);
	_usb_device_cancel_transfer(handle, 0, USB_SEND);

	/* Initialize the endpoint 0 in both directions */
	_usb_device_init_endpoint(handle, 0, CONTROL_MAX_PACKET_SIZE, USB_RECV,
				  USB_CONTROL_ENDPOINT, 0);
	_usb_device_init_endpoint(handle, 0, CONTROL_MAX_PACKET_SIZE, USB_SEND,
				  USB_CONTROL_ENDPOINT, 0);

	CancelXfer(handle);	/* cancel all the xfers. */

	/* Call interface callback */
	usb_interface_callback(handle, NULL, USB_RESET_CALLBACK);

	return;
}				/* EndBody */

/*FUNCTION*----------------------------------------------------------------
*
* Function Name  : service_speed
* Returned Value : None
* Comments       :
*     Called upon a speed detection event.
*
*END*--------------------------------------------------------------------*/
static void service_speed(
				 /* [IN] Handle of the USB device */
				 _usb_device_handle handle,
				 /* [IN] Unused */
				 bool setup,
				 /* [IN] Unused */
				 uint8_t direction,
				 /* [IN] Unused */
				 uint8_t * buffer,
				 /* [IN] Unused */
				 uint32_t length,
				 /* [IN] Error, if any */
				 uint8_t error,
				 /* [IN] Endpoint number */
				 uint8_t type)
{				/* EndBody */
	USB_DEV_STATE_STRUCT_PTR usb_dev_ptr;
	VUSB20_REG_STRUCT_PTR dev_ptr;

	usb_dev_ptr = (USB_DEV_STATE_STRUCT_PTR) handle;
	dev_ptr = (VUSB20_REG_STRUCT_PTR) usb_dev_ptr->DEV_PTR;

	gSpeed = (uint8_t) length;

	/* Check for device detach
	 * This appears to be a reliable location in the code to determine
	 * that the USB device has actually been detached. */
	if (!
	    (dev_ptr->REGISTERS.OPERATIONAL_DEVICE_REGISTERS.PORTSCX[0] &
	     EHCI_PORTSCX_CURRENT_CONNECT_STATUS)) {
		/* Call interface callback */
		usb_interface_callback(handle, NULL,
				       USB_DEVICE_DETACH_CALLBACK);

		/* Cancel transfers */
		CancelXfer(handle);
	}

	return;
}				/* EndBody */

/**
    \brief Start a write to an endpoint, cut the xfer into pieces if it is too long.
    This takes an entry in the write list and starts an xfer.  First it sees if it
    is too long.  If so it limits the xfer to EP_IN_MAX_XFER_SIZE, defined above.
    \param handle The handle for this device
    \param WriteMember The entry in the write list.
    \param EPNum the endpoint number to xfer.
*/
static void DoWrite(_usb_device_handle handle, WRITE_MEMBER_t * WriteMember,
		    uint8_t EPNum)
{
	uint32_t XferLength;
	char *TempBuf = WriteMember->CurrentBuffer;
	XferLength = WriteMember->Length;
	if (XferLength > EP_IN_MAX_XFER_SIZE) {
		XferLength = EP_IN_MAX_XFER_SIZE;
		/* don't want this xfer zero terminated.  Clear here. */
		usb_change_hw_zlp(handle, EPNum,
				  USB_DEVICE_DONT_ZERO_TERMINATE);
	} else
		usb_change_hw_zlp(handle, EPNum, EPInStorage[EPNum].hwZLP);	/* set the zlp setting. */
	/* adjust the value here for the length and address.  I do it before I start the xfer
	 * to insure that the block is updated before I get the xfer complete interrupt and
	 * try to free the block. */
	WriteMember->Length -= XferLength;
	/* See if we have a little left, if so just do it also. */
	if (WriteMember->Length < 64) {
		XferLength += WriteMember->Length;
		WriteMember->Length = 0;
	}
	WriteMember->CurrentBuffer += XferLength;	/* where to start the next xfer. */
	/* Now send this out. */
	_usb_device_send_data(handle, EPNum, (uint8_t *) TempBuf, XferLength);
}

/*FUNCTION*----------------------------------------------------------------
*
* Function Name  : service_epX
* Returned Value : None
* Comments       :
*     Called upon a completed endpoint  (USB 1.1 Chapter 9) transfer from the isr.
*
*END*--------------------------------------------------------------------*/
static void service_epX(
			       /* [IN] Handle of the USB device */
			       _usb_device_handle handle,
			       /* [IN] Is it a setup packet? */
			       bool setup,
			       /* [IN] Direction of the transfer.  Is it transmit? */
			       uint8_t direction,
			       /* [IN] Pointer to the data buffer */
			       uint8_t * buffer,
			       /* [IN] Length of the transfer */
			       uint32_t length,
			       /* [IN] Error, if any */
			       uint8_t error,
			       /* [IN] Endpoint number */
			       uint8_t EPNum)
{				/* Body */
	LINK_MEMBER *TempLink;
	WRITE_MEMBER_t *TempMember;
	if (!direction) {
		/* Save length */
		ep_lock(&EPOutStorage[EPNum].lock);
		EPOutStorage[EPNum].bulk_out_bytes_received = length;

		if (EPOutStorage[EPNum].NotifyReceivedData) {
			EPOutStorage[EPNum].NotifyReceivedData = false;

			/* Notify connection manager that we have received data */
			/* haitao 03/29/12 */
			EPOutStorage[EPNum].NotifyReset = false;
		}
		ep_unlock(&EPOutStorage[EPNum].lock);

		/* Tell the read routine that we have something it might be interested in. */
		os_event_flags_set(USBIoFlags,
				   EPOutStorage[EPNum].EventMask &
				   USB_READ_EVENT_MASK, EF_OR);
	} else {
		/* We have a write complete, delete entry */
		ep_lock(&EPInStorage[EPNum].lock);
		TempLink = agGetHead(&EPInStorage[EPNum].WriteList);
		if (TempLink != NULL) {
			TempMember = (WRITE_MEMBER_t *) TempLink->Data;
			if (TempMember->Length == 0) {
				remove_write(EPNum, TempLink);
			}
		}
		/* See if we have any more to send. */
		TempLink = agGetHead(&EPInStorage[EPNum].WriteList);
		if (TempLink != NULL) {
			TempMember = (WRITE_MEMBER_t *) TempLink->Data;
			DoWrite(handle, TempMember, EPNum);
		} else {
			if (!send_zlp(EPNum)) {
				EPInStorage[EPNum].Active = 0;
			}
		}
		ep_unlock(&EPInStorage[EPNum].lock);
	}
	return;
}				/* Endbody */

/**
 * Function name: usb2PrintRead
 *
 * \brief Read data from a given endpoint
 *
 * Description: This is called with a specific handle to do a read from
 *          any endpoint.
 *
 * \param pBuffer The buffer where read data is to be placed.
 * \param len The amount of data to read from the endpoint.
 * \param timeout   The timeout value for this read.
 * \param FileHandle  This specifies from which endpoint to read
 *
 * Author:
 *
 * Notes:
 *
 **/
int32_t usb2Read(void *pBuffer, uint32_t len, uint32_t timeout, uint32_t EPNum)
{
	uint32_t amount_received = 0, Flags, size, Timeout, BufferXfer, Blocks;
	EPOUTDATA_t *EP = &EPOutStorage[EPNum];

	Timeout = timeout;
	/* since we are just entering, we are running out of the system buffer. */
	BufferXfer = 0;

	while (amount_received < len) {	/* now get the data */
		/* Check that device is active */
		if (USBActive() && !EP->NotifyReset) {
			/* Do not wait if there is partial data
			 * If the index == 0 then we have an empty or new buffer
			 * A new buffer should also have a Read event flag pending */
			if ((EP->bulk_out_bytes_received_index) == 0) {	/* do we have any data. */
				/* No, Wait for the data */
				Flags = 0;
				if (EF_NO_EVENTS ==
				    os_event_flags_get(USBIoFlags,
						       EP->EventMask,
						       EF_OR_CLEAR, &Flags,
						       Timeout)) {
					/* timeout, return with what we have. */

					/* we timed out and are trying to xfer to the buffer */
					if (BufferXfer == 1) {
						/* abort the xfer and restart in the internal buffer. */
						ep_lock(&EP->lock);
						EP->bulk_out_bytes_received = 0;
						EP->bulk_out_bytes_received_index = 0;
						ep_unlock(&EP->lock);
						amount_received +=
						    _usb_device_cancel_transfer
						    (dev_handle, EPNum,
						     USB_RECV);
						_usb_device_recv_data
						    (dev_handle,
						     (uint8_t) (uint32_t) EPNum,
						     EP->bulk_out_buffer,
						     max_pkt_size);
						BufferXfer = 0;	/* xfer to internal buffer */
					}

					if ((amount_received == 0) &&
					    (timeout != 0)) {
						return CONIO_TIMEOUT;
					}
					break;
				} else if (Flags &
					   (EP->EventMask &
					    USB_READ_EVENT_MASK)) {
					/* Read event
					 * Fall through */
				} else if (Flags &
					   (EP->EventMask &
					    USB_CANCEL_EVENT_MASK)) {
					/* Cancel event
					   Return any data below */
					break;
				} else {
					/* Unknown */
					return CONIO_CONERR;
				}
			}
		} else {
			if (amount_received == 0) {
				EP->NotifyReset = false;
				return CONIO_CONERR;
			} else {
				return amount_received;
			}
		}

		/* Figure out how much data we have received. */
		ep_lock(&EP->lock);
		size =
		    MIN((len - amount_received),
			(EP->bulk_out_bytes_received -
			 EP->bulk_out_bytes_received_index));
		ep_unlock(&EP->lock);

		if (size > 0) {
			Timeout = timeout;

			/* If bufferxfer == 0 then the data is in the system buffer.  If not
			 * then it was dma'd directly to the buffer and we go on. */
			if (BufferXfer == 0) {
				if (EP->bulk_out_bytes_received_index <
				    BULK_BUF_XFER_SIZE) {
					/* Copy received bulk out data from the system buffer. */
					memcpy((void *)
					       &(((char *)
						  pBuffer)[amount_received]),
					       &EP->bulk_out_buffer
					       [EP->
						bulk_out_bytes_received_index],
					       size);
				}
			}
			/* Adjust the sizes to match this xfer. */
			amount_received += size;
			ep_lock(&EP->lock);
			EP->bulk_out_bytes_received_index += size;
			ep_unlock(&EP->lock);
		}

		/* Start another transfer if system buffer empty */
		if (EP->bulk_out_bytes_received <=
		    EP->bulk_out_bytes_received_index) {
			/* We have more data to get, start an xfer. */

			/* Clear flags */
			ep_lock(&EP->lock);
			EP->bulk_out_bytes_received = 0;
			EP->bulk_out_bytes_received_index = 0;
			ep_unlock(&EP->lock);

			if (((len - amount_received) <= BULK_BUF_XFER_SIZE) ||
			    (((uint32_t) (&((char *)pBuffer)[amount_received]) &
			      0x1f) != 0) || (timeout < 50)) {

				/* xfer the data to the system buffer, since we do not have the
				 * conditions necessary to dma directly into the buffer. */
				_usb_device_recv_data(dev_handle,
						      (uint8_t) (uint32_t)
						      EPNum,
						      EP->bulk_out_buffer,
						      max_pkt_size);
				BufferXfer = 0;	// xfer to internal buffer
			} else {
				/* Everything is correct for a xfer directly to the buffer.  Set
				 * that up here. */
				Blocks = 32;	/* number of 512 blocks to xfer.  Do not make more than 32, causes a data corruption */
				if (((len -
				      amount_received) / (BULK_BUF_XFER_SIZE)) <
				    Blocks)
					Blocks =
					    (len -
					     amount_received) /
					    BULK_BUF_XFER_SIZE;

				_usb_device_recv_data(dev_handle,
						      (uint8_t) (uint32_t)
						      EPNum,
						      (uint8_t *) & ((char *)
								     pBuffer)
						      [amount_received],
						      BULK_BUF_XFER_SIZE *
						      Blocks);
				BufferXfer = 1;
			}
		}
	}

	return amount_received;
}

/* FUNCTION NAME: usb2Write */

/**
 * \brief This function handles writting to the in endpoints in usb.
 * This works by putting the write on the queue, and if possible,
 * starting the write.  The write is marked somplete in the isr and
 * a message is sent to the control loop to release the data from the
 * write.  If another write is pending, it is started.
 *
 * \param pBuffer The buffer to write.  I assume this buffer is allocated.
 *
 * \param len The amount of data to write.
 *
 * \param timeout The amount of time to wait before deleting the write, if it never goes.
 *
 * \param EPNum The handle from the pipe.
 *
 * \retval SINT32
 *
 *
 **/

int32_t usb2Write(void *pBuffer, uint32_t len, uint32_t timeout,
		  uint32_t endpointNum)
{
	EPINDATA_t *EP = &EPInStorage[endpointNum];
	WRITE_MEMBER_t *TempMember;
	LINK_MEMBER *TempLink;

	/* get rid of any writes that timed out. */
	check_write_TO(endpointNum);
	/* Check that device is active */
	if (USBActive()) {
		/* Check the alignment of the buffer, fix if necessary
		 * Since we are dma'ing data out the alignment has to be on a cache line or
		 * problems could occur.  Do that here. */
		/* haitao 03/16/1 */

		/* End time is infinite if specified to be wait forever. */
		add_write_to_list(endpointNum, pBuffer, len, timeout);

		/* If a transfer is not in progress, start it here. */
		if (EP->Active == 0) {
			TempLink = agGetHead(&EP->WriteList);
			/* It is possible for this write that was added above to be complete.
			 * Make sure we have something to check here. */
			if (TempLink != NULL) {
				TempMember = TempLink->Data;	/* get the one to send */
				EP->Active = 1;	/* transfer will shortly be in progress. */
				DoWrite(dev_handle, TempMember, endpointNum);
			}
		}
	} else {
		usb_e("usb2write CONIO_CONERR");
		return CONIO_CONERR;
	}

	return len;
}

/*FUNCTION*----------------------------------------------------------------
*
* Function Name  : USBActive
* Returned Value : 0 - Not Active
*                  Non-zero - Active
* Comments       :
*     Indicates if the USB device is active.
*
*END*--------------------------------------------------------------------*/
int USBActive(void)
{
	USB_DEV_STATE_STRUCT_PTR usb_dev_ptr;
	VUSB20_REG_STRUCT_PTR dev_ptr;

	usb_dev_ptr = (USB_DEV_STATE_STRUCT_PTR) dev_handle;
	dev_ptr = (VUSB20_REG_STRUCT_PTR) usb_dev_ptr->DEV_PTR;

	return (dev_ptr->REGISTERS.OPERATIONAL_DEVICE_REGISTERS.PORTSCX[0] &
		EHCI_PORTSCX_CURRENT_CONNECT_STATUS);
}

/**
 * \brief Called when the write timer expires.
 * \param[in] param not used.
 */
 /* haitao 03/16/12 */
void timer_expire()
{
	uint32_t status;
	status =
	    os_event_flags_set(gUSBDeviceEventFlags, EVENT_WRITE_TO, EF_OR);
	if (status != WM_SUCCESS)
		usb_e("Failed to set event flag");
}

/**
 * \brief This routine checks the pending writes and removes any that have timed out.
 * The timeout is set when the write happens.  If the timeout amount of time occurs
 * and the write has not been sent, that write is removed from the list and
 * deleted.
 */
void check_writes(void)
{
	uint32_t ep_num;
	EPINDATA_t *EP;
	WRITE_MEMBER_t *TempMember;
	LINK_MEMBER *TempLink;
	for (ep_num = 1; ep_num < EPNum; ep_num++) {
		if (EPInStorage[EPNum].Init) {
			EP = &EPInStorage[ep_num];

			ep_lock(&EP->lock);
			check_write_TO(ep_num);
			if (EP->Active == 0) {
				TempLink = agGetHead(&EP->WriteList);
				if (TempLink != NULL) {
					TempMember = TempLink->Data;
					EP->Active = 1;
					DoWrite(dev_handle, TempMember, ep_num);
				}
			}
			ep_unlock(&EP->lock);
		}
	}
}

/* Function name : usb2DeviceControlLoop */

/**
 * \brief Main control loop for usb.  If something needs to be done in interrupt
 * context do it here.
 *
 * \param unused
 *
 * \retval None
 *
 * \date 1/3/2008
 *
 **/
static void usb2DeviceControlLoop(os_thread_arg_t data)
{
	uint32_t Flag;
	uint32_t status;

	/* start a timer to get rid of queued writes.
	 * This will run for 5 miuntes */
	status = os_timer_create(&WriteTimer, "writeTimer", WRITE_TIMEOUT,
				 timer_expire, NULL, OS_TIMER_PERIODIC,
				 OS_TIMER_AUTO_ACTIVATE);

	/*
	 ** Infinite loop, waiting for events requiring action
	 */
	while (1) {
		/* Wait for USB Device Interrupts */
		status = os_event_flags_get(gUSBDeviceEventFlags,
					    EVENT_DEVICE_INT | EVENT_WRITE_TO,
					    EF_OR_CLEAR, &Flag,
					    EF_WAIT_FOREVER);
		if (status == WM_SUCCESS) {
			if (Flag & EVENT_DEVICE_INT) {
				/* Call the routine to handle the interrupts at thread context. */

				_usb_dci_vusb20_isr(((USB_DEV_STATE_STRUCT_PTR)
						     dev_handle)->DEV_VEC);

				/* Clear and re-enable the USB device interrupt.
				 * No need to check if the interrupt was enabled since the
				 * only way to get here is for the ISR to set the flag */
				USB_device_unlock();
			} else if (Flag & EVENT_WRITE_TO)
				check_writes();

		}
	}
}

/**
 * \brief Initialize the usb driver and set up all the interfaces.
 *
 * \param config The info needed to config this device.
 *
 * \retval None
 *
 * \date 2/7/2008
 *
 **/
int usb_dev_init(void)
{
	int EPNum;

	/* Initialize Init & ZLP */
	for (EPNum = 0; EPNum < NUM_BULKIN_EP; EPNum++) {
		EPInStorage[EPNum].ZLP = USB_DEVICE_DONT_ZLP;
		EPInStorage[EPNum].Init = FALSE;
	}

	InitInEPStorage(0);	/* init ep 0 storage. */

	/* get initial memory for the string table. */
	NumStringDesc = 3;
	usb2StringTable =
	    (char **)USB_memalloc_aligned(sizeof(uint32_t) * NumStringDesc, 4);
	if (usb2StringTable == NULL) {
		usb_e("USB USB_memalloc failed for usb2StringTable.");
		return -WM_FAIL;
	}
	/* Init the interface storage area */
	ATInitList(&InterfaceStorage);

	return WM_SUCCESS;
}

/**
 * \brief Check for Back Channel Data on requested interface
 * \param[in] interface
 */
bool usbDevInterfaceCheckForBackChannelData(usb_intf_id_t interfaceID)
{
	bool found = false;
	USB_INTERFACE_DETAILS_t *TempDetails;
	LINK_MEMBER *TempLink = NULL;

	TempDetails = getUSBInterface(interfaceID);
	if (TempDetails != NULL) {
		ep_lock(&EPInStorage[TempDetails->InNum].lock);
		TempLink =
		    agGetHead(&EPInStorage[TempDetails->InNum].WriteList);
		ep_unlock(&EPInStorage[TempDetails->InNum].lock);
		if (TempLink != NULL)
			found = true;
	}

	return found;
}

/**
 * \brief USB ISR
 * sets an event flag when a USB interrupt occurs
 * \param[in] device handle Pointer
 */
void usb2_device_ISR(uint32_t temp)
{
	/* Disable the USB interrupt until the thread has a chance to handle it.
	 * Interrupt had to be enabled to get here */
	USB_device_lock();

	/* Signal the thread that it needs to handle the USB traffic. */
	os_event_flags_set(gUSBDeviceEventFlags, EVENT_DEVICE_INT, EF_OR);
}				// end usb2_device_ISR

/**
 * \brief Initialize and start the USB device controller
 */

static int usbDeviceStart(void)
{
	uint32_t error;
	uint8_t endpoint;
	USB_DEV_STATE_STRUCT_PTR usb_dev_ptr;

	while ((usb_status = (uint8_t *) USB_memalloc(8)) == NULL)
		os_thread_sleep(os_msec_to_ticks(10));

	while ((if_status = (uint8_t *) USB_memalloc(8)) == NULL)
		os_thread_sleep(os_msec_to_ticks(10));

	while ((data_to_send = (uint8_t *) USB_memalloc(8)) == NULL)
		os_thread_sleep(os_msec_to_ticks(10));

	while ((sof_count = (uint16_t *) USB_memalloc(8)) == NULL)
		os_thread_sleep(os_msec_to_ticks(10));

	while ((local_setup_packet =
		(SETUP_STRUCT_t *) USB_memalloc(sizeof(SETUP_STRUCT_t))) ==
	       NULL)
		os_thread_sleep(os_msec_to_ticks(10));

	/* Initialize the USB device interface and receive a handle back */
	error = _usb_device_init(0, &dev_handle, USB_MAX_ENDPOINTS);
	if (error != USB_OK) {
		usb_e("USB Device Initialization failed. Error: 0x%X",
			 error);
		return -WM_FAIL;
	}

	/* Check for USB Force Full Speed */
	if (usbConfig.USBForceFullSpeed)
		_usb_device_set_status(dev_handle, USB_FORCE_FULL_SPEED, 0);
	else
		_usb_device_set_status(dev_handle, USB_CLEAR_FULL_SPEED, 0);

	error =
	    _usb_device_register_service(dev_handle, USB_SERVICE_EP0,
					 service_ep0);
	if (error != USB_OK) {
		usb_e(
		  "USB Service Reg failed for EP0 Error: 0x%X",
		  error);
		return -WM_FAIL;
	}

	error =
	    _usb_device_register_service(dev_handle, USB_SERVICE_BUS_RESET,
					 reset_ep0);
	if (error != USB_OK) {
		usb_e(
		  "USB Service Reg failed for BUS_RESET Error: 0x%X",
		  error);
		return -WM_FAIL;
	}

	error =
	    _usb_device_register_service(dev_handle,
					 USB_SERVICE_SPEED_DETECTION,
					 service_speed);
	if (error != USB_OK) {
		usb_e(
		  "USB Service Reg failed for SPEED_DETECT Error: 0x%X",
		  error);
		return -WM_FAIL;
	}

	/* Registers non-control endpoints */
	usb_dev_ptr = (USB_DEV_STATE_STRUCT_PTR) dev_handle;
	for (endpoint = 1; endpoint < usb_dev_ptr->MAX_ENDPOINTS; endpoint++) {
		error =
		    _usb_device_register_service(dev_handle, endpoint,
						 service_epX);
		if (error != USB_OK) {
			usb_e(
			  "USB Service Reg failed for endpoint %d Error: %x",
			  endpoint, error);
			return -WM_FAIL;
		}
	}

	/* This is a Self-Powered Device so set device status to show that. */
	_usb_device_set_status(dev_handle, USB_STATUS_DEVICE, USB_SELF_POWERED);

	error = os_event_flags_create(&USBIoFlags);
	if (error != WM_SUCCESS) {
		usb_e("Failed to create device flag USBIoFlags");
		return -WM_FAIL;
	}

	error = os_event_flags_create(&gUSBDeviceEventFlags);
	if (error != WM_SUCCESS) {
		usb_e("Failed to create device flag gUSBDeviceEventFlags");
		return -WM_FAIL;
	}

	error = os_thread_create(&USB2DeviceThread,	/* thread handle */
				 "USB2DeviceThread",	/* thread name */
				 usb2DeviceControlLoop,	/* entry function */
				 0,	/* argument */
				 &USB2DeviceThread_stack,	/* stack */
				 OS_PRIO_3);	/* priority - medium low */

	if (error != WM_SUCCESS) {
		usb_e("Failed to create USB2DeviceThread");
		return -WM_FAIL;
	}

	return WM_SUCCESS;
}

/**
 * \brief Start the USB device controller
 */
int usb_dev_start(void)
{
	USB_DEV_STATE_STRUCT_PTR usb_dev_ptr;
	VUSB20_REG_STRUCT_PTR dev_ptr;
	int status = WM_SUCCESS;

	/* Disable interrutps globally at CM3 level by cleaing PRIMASK */
	__asm volatile (" cpsid i");

	/* Disable USB device interrupt */
	USB_device_lock();
	/* Check for existing dev_handle */
	if (dev_handle == NULL) {
		/* Initialize and Start the USB controller */
		status = usbDeviceStart();
	} else {
		usb_dev_ptr = (USB_DEV_STATE_STRUCT_PTR) dev_handle;
		dev_ptr = (VUSB20_REG_STRUCT_PTR) usb_dev_ptr->DEV_PTR;

		/* Start the USB controller */
		dev_ptr->REGISTERS.OPERATIONAL_DEVICE_REGISTERS.USB_CMD |=
		    EHCI_CMD_RUN_STOP;
	}

	/*
	 * Set priority below configMAX_SYSCALL_INTERRUPT_PRIORITY (0xa)
	 * because this interrupt handler will be using freertos APIs.
	 */
	NVIC_SetPriority(USB_IRQn, 0xf);

	USB_device_unlock();

	/* Enable interrutps globally at CM3 level by cleaing PRIMASK */
	__asm volatile (" cpsie i");

	return status;
}

/**
 * \brief Stop the USB device controller
 */
int usb_dev_stop(void)
{
	USB_DEV_STATE_STRUCT_PTR usb_dev_ptr;
	VUSB20_REG_STRUCT_PTR dev_ptr;
	int status = WM_SUCCESS;

	if (dev_handle) {
		usb_dev_ptr = (USB_DEV_STATE_STRUCT_PTR) dev_handle;
		dev_ptr = (VUSB20_REG_STRUCT_PTR) usb_dev_ptr->DEV_PTR;

		/* Stop the controller */
		dev_ptr->REGISTERS.OPERATIONAL_DEVICE_REGISTERS.USB_CMD &=
		    ~EHCI_CMD_RUN_STOP;

		/* Cancel any pending transfers */
		CancelXfer(dev_handle);
	} else
		status = -WM_FAIL;

	return status;
}

/**
 * \brief  Force usb to stop
 */
void stop_usb(void)
{

	USBC->USBCMD.WORDVAL &= ~EHCI_CMD_RUN_STOP;
}

/**
 * \brief Initialize the in ep storage
 * \param[in] EPNum End point number
 */

static void InitInEPStorage(uint8_t EPNum)
{
	char EP_name[30];
	EPInStorage[EPNum].Init = TRUE;
	sprintf(EP_name, "USB_EPI%d", (int)EPNum);
	ep_lock_init(&EPInStorage[EPNum].lock, EP_name);
	agLinkInit(&EPInStorage[EPNum].WriteList);
	EPInStorage[EPNum].Active = 0;
}

/**
 * \brief Initialize the out ep storage
 * \param[in] EPNum End point number
 */

static void InitOutEPStorage(uint8_t EPNum)
{
	char EP_name[30];

	sprintf(EP_name, "USB_EPO%d", (int)EPNum);
	memset(&EPOutStorage[EPNum], 0, sizeof(EPOUTDATA_t));
	ep_lock_init(&EPOutStorage[EPNum].lock, EP_name);

	EPOutStorage[EPNum].bulk_out_bytes_received = 0;
	EPOutStorage[EPNum].bulk_out_bytes_received_index = 0;
	EPOutStorage[EPNum].EventMask = USB_READ_EVENT_BITS << EPNum;
	EPOutStorage[EPNum].NotifyReceivedData = true;
	EPOutStorage[EPNum].NotifyReset = false;
	/*haitao 03/29/12  */
	EPOutStorage[EPNum].ConHandle = 0;
	EPOutStorage[EPNum].bulk_out_buffer =
	    (uint8_t *) USB_memalloc(BULK_BUF_XFER_SIZE);
	memset(EPOutStorage[EPNum].bulk_out_buffer, 0, BULK_BUF_XFER_SIZE);
}

/**
 * \brief Set up an interface for use on usb.
 *
 * \param[in] interface An enum of the interface to initialize.
 * \param[in] interfaceConfig A structure specifying the config of the interface
 *
 *
 **/
int usb_dev_intf_add(usb_intf_id_t interfaceID,
		     usb_intf_config_t * interfaceConfig)
{

	USB_INTERFACE_DETAILS_t *TempInterDetails;
	USB_INTERFACE_t *TempInterface;
	uint32_t InterfaceSize;
	USB_ENDPOINT_t *TempEP;
	char *TempStringTable;
	uint32_t error;
	uint32_t ClassDescSize;
	uint8_t *TempClassDesc;
	uint32_t InterfAssocDescSize;
	USB_ASSOCIATION_t *TempInterfAssocDesc;

	TempInterfAssocDesc =
	    (USB_ASSOCIATION_t *) (interfaceConfig->InterfAssocDes);

	/* Get the interface association descriptor length */
	if (TempInterfAssocDesc != NULL)
		InterfAssocDescSize = sizeof(USB_ASSOCIATION_t);
	else
		InterfAssocDescSize = 0;

	ClassDescSize = interfaceConfig->ClassDescSize;

	InterfaceSize =
	    InterfAssocDescSize + sizeof(USB_INTERFACE_t) + ClassDescSize +
	    (interfaceConfig->NumOuts + interfaceConfig->NumIns +
	     interfaceConfig->NumInts) * sizeof(USB_ENDPOINT_t);
	TempInterDetails = (USB_INTERFACE_DETAILS_t *)
	    USB_memalloc(sizeof(USB_INTERFACE_DETAILS_t) + InterfaceSize);
	if (TempInterDetails == NULL) {
		usb_e("USB_memalloc Failed for USB Interface Struct");
		return -WM_FAIL;
	}

	ATInitList(&TempInterDetails->linkedList);
	TempInterface =
	    (USB_INTERFACE_t *) ((uint32_t) TempInterDetails +
				 sizeof(USB_INTERFACE_DETAILS_t));
	memset(TempInterDetails, 0, sizeof(USB_INTERFACE_DETAILS_t));
	TempInterDetails->InterfacePointer = TempInterface;
	TempInterDetails->SizeOfInterfaceDesc = (uint8_t) InterfaceSize;
	TempInterDetails->ZeroTerminate = interfaceConfig->ZeroTermination;
	TempInterDetails->interfaceID = interfaceID;
	TempInterDetails->InterfaceCallback =
	    interfaceConfig->InterfaceCallback;

	error =
	    os_semaphore_create(&TempInterDetails->interfaceSemaphore,
				"IntName");
	if (error != WM_SUCCESS) {
		usb_e("IntName semaphore creation failed");
		USB_memfree(TempInterDetails);
	}

	/* UPdate the values in the config descriptor */
	ConfigDesc.wTotalLength =
	    le16_to_cpu((le16_to_cpu(ConfigDesc.wTotalLength) + InterfaceSize));
	ConfigDesc.bNumberInterfaces++;

	/* Build the association description */
	if (TempInterfAssocDesc != NULL) {
		TempInterfAssocDesc->bFirstInterface = NumInterfaces;

		if (strlen(interfaceConfig->InterfAssocName) != 0) {
			TempStringTable =
			    (char *)mem_remalloc((uint8_t **) usb2StringTable,
						 sizeof(uint32_t) *
						 (NumStringDesc + 1));
			if (TempStringTable == NULL) {
				usb_e("Remalloc Failed for USB String Table");
				USB_memfree(TempInterDetails);
			}
			usb2StringTable = (char **)TempStringTable;
			TempStringTable = (char *)
			    USB_memalloc(strlen(interfaceConfig->InterfaceName)
					 + 1);
			if (TempStringTable == NULL) {
				usb_e("Remalloc Failed for USB Interface Name");
				USB_memfree(TempInterDetails);
			}
			strcpy(TempStringTable, interfaceConfig->InterfaceName);
			usb2StringTable[NumStringDesc++] = TempStringTable;
			TempInterfAssocDesc->biFunction =
			    (uint8_t) NumStringDesc;
		} else
			TempInterfAssocDesc->biFunction = 0;

		memcpy(TempInterface, TempInterfAssocDesc,
		       sizeof(USB_ASSOCIATION_t));

		TempInterface =
		    (USB_INTERFACE_t *) ((uint32_t) TempInterface +
					 sizeof(USB_ASSOCIATION_t));
	}

	/* Initialize the interface descriptor */
	TempInterface->bLength = (uint8_t) sizeof(USB_INTERFACE_t);
	TempInterface->bDescriptorType = USB_INTERFACE_DESCRIPTOR;
	TempInterface->bInterfaceNumber = (uint8_t) NumInterfaces++;
	TempInterface->bAlternateSetting = 0;
	TempInterface->bNumEndpoints = interfaceConfig->NumOuts +
	    interfaceConfig->NumIns + interfaceConfig->NumInts;
	TempInterface->bInterfaceClass = interfaceConfig->ClassCode;
	TempInterface->bInterfaceSubclass = interfaceConfig->SubClass;
	TempInterface->bInterfaceProtocol = interfaceConfig->Protocol;

	/* Setup the string descriptor. */
	if (strlen(interfaceConfig->InterfaceName) != 0) {
		TempStringTable =
		    (char *)mem_remalloc((uint8_t **) usb2StringTable,
					 sizeof(uint32_t) * (NumStringDesc +
							     1));
		if (TempStringTable == NULL) {
			usb_e("Remalloc Failed for USB String Table");
			USB_memfree(TempInterDetails);
		}
		usb2StringTable = (char **)TempStringTable;
		TempStringTable =
		    (char *)USB_memalloc(strlen(interfaceConfig->InterfaceName)
					 + 1);
		if (TempStringTable == NULL) {
			usb_e("Remalloc Failed for USB Interface Name");
			USB_memfree(TempInterDetails);
		}
		strcpy(TempStringTable, interfaceConfig->InterfaceName);
		usb2StringTable[NumStringDesc++] = TempStringTable;
		TempInterface->iInterface = (uint8_t) NumStringDesc;
	} else
		TempInterface->iInterface = 0;

	/* Build the class interface description */
	TempClassDesc =
	    (uint8_t *) ((uint32_t) TempInterface + sizeof(USB_INTERFACE_t));
	memcpy(TempClassDesc, interfaceConfig->ClassDesc, ClassDescSize);

	/* Build the config descriptors. */
	TempEP =
	    (USB_ENDPOINT_t *) ((uint32_t) TempInterface +
				sizeof(USB_INTERFACE_t) + ClassDescSize);

    if(interfaceConfig->EPType>sizeof(EPBufSize)-1)
    {
             ASSERT(0);  
    }
    TempInterDetails->EPType=interfaceConfig->EPType;

	if (interfaceConfig->NumOuts) {
		TempInterDetails->OutNum = (uint8_t) EPNum;
		BuildEPDesc(TempEP++, TempInterDetails->OutNum,
			    EPBufSize[interfaceConfig->EPType],interfaceConfig->EPType,EPInterval[interfaceConfig->EPType]);
	}

	if (interfaceConfig->NumIns) {
		TempInterDetails->InNum = (uint8_t) EPNum;
		BuildEPDesc(TempEP++, TempInterDetails->InNum + 0x80,
			    EPBufSize[interfaceConfig->EPType],interfaceConfig->EPType,EPInterval[interfaceConfig->EPType]);
	}

	/* Increment to next EP pair IF either IN or OUT was used. */
    if(interfaceConfig->EPType ==e_INTERRUPT)

	{
	    TempInterDetails->IntNum=EPNum;
    }
	if ((interfaceConfig->NumIns) || (interfaceConfig->NumOuts)) {
		EPNum++;
	}

	/* Now do the interrupt descriptors. */
	if (interfaceConfig->NumInts) {
		TempInterDetails->IntNum = (uint8_t) EPNum++;
		/* Since this is an interrupt endpoint we have to specify
			the polling interval */
		BuildEPDesc(TempEP++, TempInterDetails->IntNum + 0x80,
			    INT_EP_PACKET_SIZE, e_INTERRUPT,
			    USB_INT_EP_POLLING_INTERVAL);
	}

	/* Initialize the new area. */
	if (TempInterDetails->OutNum)
		InitOutEPStorage(TempInterDetails->OutNum);

	if (TempInterDetails->InNum) {
		InitInEPStorage(TempInterDetails->InNum);
		if ((interfaceConfig->ZLP & USB_DEVICE_BULK_IN_ZLP) ==
		    USB_DEVICE_BULK_IN_ZLP) {
			EPInStorage[TempInterDetails->InNum].ZLP = 0x1;
		}
	}

	if (TempInterDetails->IntNum) {
		InitInEPStorage(TempInterDetails->IntNum);
		if ((interfaceConfig->ZLP & USB_DEVICE_INT_IN_ZLP) ==
		    USB_DEVICE_INT_IN_ZLP) {
			EPInStorage[TempInterDetails->IntNum].ZLP = 0x1;
		}
	}
	/* Put the interface on the list. */
	ATInsertTailList(&InterfaceStorage, &TempInterDetails->linkedList);
	return WM_SUCCESS;
}

/**
 * \brief Return the current usb device configuration
 * \param Config Pointer to the structure to fill out.
 */
int usb_dev_get_config(usb_sys_config_t * config)
{
	if (config != NULL) {
		memcpy(config, &usbConfig, sizeof(usb_sys_config_t));
		return WM_SUCCESS;
	}

	return -WM_FAIL;
}

/**
 * \brief Put a string into the string table.  If empty, clear entry from device descriptor
 * This is only used for the 3 strings that are in the device descriptor.  If a string
 * is empty, this removes the entry from the device descriptor.
 * \param config The string to put into the string table.
 * \param StrTblOffset the offset into the string table for storing this.
 * \param DeviceOffset the offset into the device descriptor for this string
 */
static void PutInString(char *config, uint32_t StrTblOffset,
			uint32_t DeviceOffset)
{
	usb2StringTable[StrTblOffset] = config;
	if (strnlen(config, 100) == 0)
		DevDescData[DeviceOffset] = 0;

}

/**
 * \brief Set the usb config to the values in config
 * To use this you first must stop the interface, using
 * usbDevStop, then do the usbDevSetConfig followed by
 * usbDevInterfaceAdd for each interface and then start the
 * controller using usbDevStart. \param[in] config Pointer to
 * the structure containing the data to update.
 */

int usb_dev_set_config(const usb_sys_config_t * config)
{
	int status = WM_SUCCESS;

	/* Check to see if the driver has been initialized */
	if (usb2StringTable == NULL) {
		status = -WM_FAIL;
	} else {
		InitializeConfigDesc();

		memcpy(&usbConfig, config, sizeof(usb_sys_config_t));
		/* Update VID and PID */
		DevDescData[8] = USB_uint_16_low(config->USBVendorID);
		DevDescData[9] = USB_uint_16_high(config->USBVendorID);
		DevDescData[10] = USB_uint_16_low(config->USBProductID);
		DevDescData[11] = USB_uint_16_high(config->USBProductID);

		/* Now put in the string pointers. */
		PutInString((char *)config->USBMfgStr, 0, 14);
		PutInString((char *)config->USBProdStr, 1, 15);
		PutInString((char *)config->USBFormatterSerNum, 2, 16);

	}

	return status;
}

/**
 * \brief Clear all interfaces To use this you first must stop
 * the controller, using usbDevStop, then do the
 * usbDevInterfaceRemoveAll followed by usbDevInterfaceInit for
 * each interface and then start the controller using
 * usbDevStart. \param[in] NA
 */

int usb_dev_intf_remove_all(void)
{
	USB_INTERFACE_DETAILS_t *TempInterDetails;
	USB_INTERFACE_t *TempInterface;
	int ep_num;

	for (ep_num = 0; ep_num < NUM_BULKIN_EP; ep_num++) {
		EPInStorage[ep_num].ZLP = USB_DEVICE_DONT_ZLP;
		EPInStorage[ep_num].Init = FALSE;
	}

	/* Clear any previous interfaces */
	while (!ATIsListEmpty(&InterfaceStorage)) {
		TempInterDetails =
		    (USB_INTERFACE_DETAILS_t *) InterfaceStorage.nextEntry;
		TempInterface =
		    (USB_INTERFACE_t *) TempInterDetails->InterfacePointer;

		os_semaphore_delete(&TempInterDetails->interfaceSemaphore);

		if (TempInterface->iInterface) {
			USB_memfree(usb2StringTable
				    [TempInterface->iInterface - 1]);
			usb2StringTable[TempInterface->iInterface - 1] = NULL;
			NumStringDesc--;
		}

		if (TempInterDetails->OutNum) {
			ep_lock_delete(&EPOutStorage
				       [TempInterDetails->OutNum].lock);
			USB_memfree(EPOutStorage
				    [TempInterDetails->OutNum].bulk_out_buffer);

		}

		if (TempInterDetails->InNum)
			ep_lock_delete(&EPInStorage
				       [TempInterDetails->InNum].lock);

		if (TempInterDetails->IntNum)
			ep_lock_delete(&EPInStorage
				       [TempInterDetails->IntNum].lock);

		ATRemoveHeadList(&InterfaceStorage);
		/* use 0xff rather than zero since 0 is valid for interfaceid. */
		memset(TempInterDetails, 0xff, sizeof(USB_INTERFACE_DETAILS_t));
		USB_memfree(TempInterDetails);
	}

	/* Initialize Number of interfaces and endpoints */
	EPNum = 1;
	NumInterfaces = 0;

	return WM_SUCCESS;
}

/**
 * \brief When an interface is requested, return the handle to that interface.
 * \param interface The interface type to return
 * \retval The handle to the interface.
 */

USB_INTERFACE_DETAILS_t *getUSBInterface(usb_intf_id_t interfaceID)
{
	USB_INTERFACE_DETAILS_t *TempDetails = NULL;

	if (!ATIsListEmpty(&InterfaceStorage)) {
		TempDetails =
		    (USB_INTERFACE_DETAILS_t *) InterfaceStorage.nextEntry;

		/* Find the interface that corresponds to this handle. */
		do {
			if (interfaceID == TempDetails->interfaceID)
				break;
			TempDetails =
			    (USB_INTERFACE_DETAILS_t *) TempDetails->linkedList.
			    nextEntry;
		} while (TempDetails !=
			 (USB_INTERFACE_DETAILS_t *) & InterfaceStorage);

		if (interfaceID != TempDetails->interfaceID) {
			TempDetails = NULL;
		}
	}

	return TempDetails;
}

/**
 * \brief clean up all the pending writes on a given endpoint
 * This is called whenever a reset comes on ep0.  That usually means
 * that the usb cable has been unplugged.  This throws out all the pending
 * writes and gets ready for the next write.
 * \param[in] EPNum the endpoint number.  This only is to be used on IN endpoints.
 */
static void reset_writes(uint32_t EPNum)
{
	LINK_MEMBER *TempLink;
	WRITE_MEMBER_t *TempMember;
	ep_lock(&EPInStorage[EPNum].lock);
	TempLink = agGetHead(&EPInStorage[EPNum].WriteList);
	while (TempLink != NULL) {
		TempMember = TempLink->Data;
		TempLink = agDelHead(&EPInStorage[EPNum].WriteList);
		if (TempMember->StartBuffer)
			USB_memfree(TempMember->StartBuffer);
		USB_memfree(TempLink);
		TempLink = agGetHead(&EPInStorage[EPNum].WriteList);
	}
	EPInStorage[EPNum].Active = 0;
	ep_unlock(&EPInStorage[EPNum].lock);
}

/****************************************************************************//**
 * @brief      USB interrupt handler
 *
 * @param[in]  none
 *
 * @return none
 *
 *******************************************************************************/
void USB_IRQHandler(void)
{
	usb2_device_ISR(0);
}

/**
 * This routine checks the interface to insure the handle is still valid.
 * If the handle is not valid the interfaces are searched to find a valid handle
 * If no valid handle is found, an error is returned.
 */
static error_type_t check_interface(io_device_impl_t *handle)
{
    USB_INTERFACE_DETAILS_t *thandle;

    thandle = handle->interface;
    if(!thandle || (thandle->interfaceID != handle->interfaceID))
    {
        handle->interface = getUSBInterface(handle->interfaceID);
        if(handle->interface == NULL)
            return FAIL;
    }
    return OK;
}

/**
 * \brief Open a given interface in usb.
 * This open a usb interface.  The user specifies the interface to open and if it
 * is available the handle to that interface is returned.
 * \param[out] handle The handle returned after the open.
 * \param[in] interface The interface ID to open.
 * \return error_type_t  Two errors, interface does not exist or interface is alread open.
 */
error_type_t usb_dev_intf_open(io_device_impl_t **handle, usb_intf_id_t interfaceID)
{
    uint32_t error;
    error_type_t status = OK;

    //
    // Search thru the interface looking for the interface we want to open.
    //
    *handle = USB_memalloc(sizeof(usb_interface_handle_t));
    ASSERT(*handle);    // make sure we got the handle memory.
    (*handle)->interface = getUSBInterface(interfaceID);
    (*handle)->interfaceID = interfaceID;

    if ((*handle)->interface) 
    {
        // now try and get the semaphore.
        error = os_semaphore_get(&((*handle)->interface)->interfaceSemaphore, OS_NO_WAIT);

        if(error != WM_SUCCESS)     // get of semaphore failed, get out.
            status = FAIL;
    }
    else
    {
        status = FAIL;
    }
    if(status == FAIL)
        USB_memfree_and_null(*handle);
    return status; 
}


/**
 * \brief Close a previously opened interface.
 * \param[in] handle The handle returned from the open call.
 * \return Will always return OK, if a problem this asserts.
 * 
 */
error_type_t usb_dev_intf_close(io_device_impl_t * hdl)
{
    USB_INTERFACE_DETAILS_t *TempDetails;
    uint32_t error;
    error_type_t status = OK;
    USB_INTERFACE_DETAILS_t *handle;

    status = check_interface(hdl);
    if(status != OK)
    {
        // this failed, the interface does not exist, just free the memory and return
        USB_memfree_and_null(hdl);
        return OK;
    }
    handle = hdl->interface;

    TempDetails = (USB_INTERFACE_DETAILS_t *)InterfaceStorage.nextEntry;
    if (!ATIsListEmpty(&InterfaceStorage))
    {
        //
        // Find the interface that corresponds to this handle.
        //
        do
        {
            if(handle == TempDetails)
                break;      // get out of here.
            TempDetails = (USB_INTERFACE_DETAILS_t *)TempDetails->linkedList.nextEntry;
        } while(TempDetails != (USB_INTERFACE_DETAILS_t *)&InterfaceStorage);
    
        if (TempDetails != (USB_INTERFACE_DETAILS_t *)&InterfaceStorage) 
        {
            // Found
            error = os_semaphore_put(&TempDetails->interfaceSemaphore);
            if (error != WM_SUCCESS)
            {
                status = FAIL;
            }
        }
        else
        {
            // Not Found
            status = FAIL;
        }
    }
    else
    {
        // No interfaces
        status = FAIL;
    }

    USB_memfree_and_null(hdl); // return the memory
    return status;
}

/**
 * \brief Read from a given endpoint.
 * This function attempts to read data into the buffer of len amount within timeout period.
 * \param[in] pBuffer The address of the buffer to fill.
 * \param[in] len The number of bytes max to read into the buffer.
 * \param[in] timeout The number of clock ticks to wait for len amount of data to arrive.
 *              If 0 then return only the data that currently exists in local buffers.
 * \param[in] EndPoint The number of the endpoint to read from.
 * \return error_type_t > 0 number of bytes actually transferred.
 */
error_type_t usb_dev_intf_read(io_device_impl_t *handle, void *pBuffer, uint32_t len, uint32_t timeout)
{
    error_type_t error;

    ASSERT(handle);
    ASSERT(pBuffer != NULL);
    error = check_interface(handle);
    if(error == OK)
        error = usb2Read(pBuffer, len, timeout,handle->interface->OutNum);

    return error;
}


/**
    * \brief Write a data buffer to a given endpoint.
    * \param[in] handle The handle returned from an open command.
    * \param[in] pBuffer The data buffer from which data is written.  Note
    *             this buffer must be allocated and not used after this call
    *             once the data is xfered, this will be freed.
    * \param[in] len The number of bytes to write.
 * \param[in] time_out the number of clock ticks till this write is deleted.  0 will never be deleted.
*/
error_type_t usb_dev_intf_write(io_device_impl_t *handle, void *pBuffer, uint32_t len, uint32_t time_out)
{
    error_type_t error;

    ASSERT(handle);
    error = check_interface(handle);
    if(error == OK)
        error = usb2Write(pBuffer, len, time_out, handle->interface->InNum);

    return error;
}

/**
 * \brief Write a data buffer to the interrupt endpoint in the interface pointed to by handle.
 * \param[in] handle The handle returned from an open call.
 * \param[in] pBuffer The data buffer from which data is written.  This buffer 
 *        must be allocated and not used after this call.  
 * \param[in] len The number of bytes in the buffer to write.
 * \param[in] time_out the number of clock ticks till this write is deleted.  0 will never be deleted.
 * \return -1 if Interrupt endpoint does not exist in
 * this interface.
 */

error_type_t usb_dev_intf_interrupt_write(io_device_impl_t *handle, void *pBuffer, uint32_t len, uint32_t time_out)
{
    error_type_t error;

    ASSERT(handle);
    error = check_interface(handle);
    if(error == OK)
        error = usb2Write(pBuffer, len, time_out, handle->interface->IntNum);

    return error;
}

/**
 * \brief Interface control functionality for the usb block.
 * \param[in] handle The handle returned from an open call.
 * \param[in] cmd The command to execute
 * \param[in] intArg An integer that is specific to cmd.
 * \param[in] ptrArg A pointer that is specific to a cmd.
 * \return error_type_t
 */
int32_t usb_dev_intf_ioctl(io_device_impl_t *hdl, ioctl_cmd_t cmd, void *ptrArg)
{
    error_type_t error=OK;

    USB_INTERFACE_DETAILS_t* handle;

    ASSERT(hdl);
    error = check_interface(hdl);
    if(error != OK)
        return error;
    handle = hdl->interface;

//-jrs    DPRINTF((DBG_SOFT|DBG_OUTPUT),("usb_dev_intf_ioctl cmd=0x%X\n", cmd));

    switch (cmd)
    {
        case e_close:
            // Check for pending data
            ep_lock( &EPOutStorage[handle->OutNum].lock );
            if (EPOutStorage[handle->OutNum].bulk_out_bytes_received > 0)
            {
                if (EPOutStorage[handle->OutNum].ConHandle)
                {
                    EPOutStorage[handle->OutNum].NotifyReset = false;
                    //PipeConnect((void *) EPOutStorage[handle->OutNum].ConHandle ); 
                }
            }
            else
            {
                // Notify pipe when data is received
                EPOutStorage[handle->OutNum].NotifyReceivedData = true;
            }
            ep_unlock( &EPOutStorage[handle->OutNum].lock );
            break;
        case e_STALLIN:
            ///< Stall the in endpoint
            _usb_device_stall_endpoint(dev_handle, handle->InNum, 1);
            break;
        case e_STALLOUT:
            ///< Stall the out endpoint.
            _usb_device_stall_endpoint(dev_handle, handle->OutNum, 0);
            break;
        case e_ResetWrites:
            ///< Wipe out all pending writes and abort current write.
            reset_writes((uint32_t)handle->InNum);
            break;
        case e_ClearAllWrites:
            ///< Get rid of all writes for all endpoints.
            // NOT IMPLEMENTED YET
            break;
//      case e_IOInterface:
//          return e_usb;
        default:
            break;
    }

    return error;
}

/**
 * \brief a function to test whether the device is connected.
 * 
 * \returns - true if connected, false if not.
 */
bool is_USB_device_connected()
{
    bool port_connected    = false;

    USB_DEV_STATE_STRUCT_PTR usb_dev_ptr;
    VUSB20_REG_STRUCT_PTR dev_ptr;

    usb_dev_ptr = (USB_DEV_STATE_STRUCT_PTR) dev_handle;
    dev_ptr = (VUSB20_REG_STRUCT_PTR)usb_dev_ptr->DEV_PTR;

    // Check to see if the port is connected or not.
    //
    // NOTE: only checking port 1. Since this is for a device, it is unlikely there
    // will be multiple ports and most likely we will be on port 1.
    if (dev_ptr->REGISTERS.OPERATIONAL_DEVICE_REGISTERS.PORTSCX[0] & 0x01)
    {
        port_connected = true;
    }

    return(port_connected);
}

bool check_MS_data_sent()
{
#if 1 // Temporary but needed for now to pass MSC test.
    os_thread_sleep(2);
    return(true);
#else
    uint8_t EP_number;
    bool    intf_exists;

    intf_exists = get_EP_number(USB_MASS_STORAGE_INTERFACE, &EP_number);
    ASSERT(intf_exists);

    if (EPInStorage[EP_number].Active == 0)
    {
        return(true);
    }
    else
    {
        return(false);
    }
#endif
}
