/*
 * ==========================================================
 * (c) Copyright 2008-2014 Marvell International Ltd.
 *
 *               Marvell Confidential
 * ==========================================================
 */
/**
 * \brief This is the usb cdc interface file init function
*/

#include <wm_os.h>
#include <wmstdio.h>

#include <incl/devapi.h>
#include <incl/usb_device_api.h>
#include <incl/usbdrv.h>
#include <incl/usbsysinit.h>
#include <incl/usb_cdc.h>

/*
    These strings are for identifying the various interfaces.
*/
static char CdcInter[] = "MARVELL-MC200-VCOM";

uint8_t EP0Buf[8];

uint8_t CdcClassDesc[] = {
/*Header Functional Descriptor*/
	0x05,			/* bLength: Endpoint Descriptor size */
	CDC_CS_INTERFACE,	/* bDescriptorType: CS_INTERFACE */
	CDC_HEADER,		/* bDescriptorSubtype: Header Func Desc */
	WBVAL(CDC_V1_10),	/* 1.10 *//* bcdCDC */
	/*Call Management Functional Descriptor */
	0x05,			/* bFunctionLength */
	CDC_CS_INTERFACE,	/* bDescriptorType: CS_INTERFACE */
	CDC_CALL_MANAGEMENT,	/* bDescriptorSubtype: Call Management Func Desc */
	0x01,			/* bmCapabilities: device handles call management */
	0x01,			/* bDataInterface: CDC data IF ID */
/*Abstract Control Management Functional Descriptor*/
	0x04,			/* bFunctionLength */
	CDC_CS_INTERFACE,	/* bDescriptorType: CS_INTERFACE */
	CDC_ABSTRACT_CONTROL_MANAGEMENT,	/* bDescriptorSubtype: Abstract Control Management desc */
	0x02,			/* bmCapabilities: SET_LINE_CODING, GET_LINE_CODING, SET_CONTROL_LINE_STATE supported */
/*Union Functional Descriptor*/
	0x05,			/* bFunctionLength */
	CDC_CS_INTERFACE,	/* bDescriptorType: CS_INTERFACE */
	CDC_UNION,		/* bDescriptorSubtype: Union func desc */
	0,			/* bMasterInterface: Communication class interface is master */
	1,			/* bSlaveInterface0: Data class interface is slave 0 */
};

uint8_t CDCInterfAssocDesc[] = {
/*Header Functional Descriptor*/
	0x08,			/* bLength: Interface association Descriptor size */
	0x0B,			/* bDescriptorType:  */
	0,			/* bFirstInterface */
	2,			/* bInterfaceCount */
	0x02,			/* bFunctionClass */
	0x02,			/* bFunctionSubClass */
	0x00,			/* bFunctionProtocol */
	0x00,			/* biFunction */
};

unsigned char NotificationBuf[10];

CDC_LINE_CODING CDC_LineCoding = { 9600, 0, 0, 8 };

uint16_t CDC_SerialState = 0x0000;

/******************************************************************************
 * @brief      Send encapsulated command
 *
 * @param[in]  none
 *
 * @return TRUE or FALSE
 *
 *******************************************************************************/
uint32_t CDC_SendEncapsulatedCommand(void)
{
	/* added the handle */

	return (TRUE);
}

/******************************************************************************
 * @brief      Get encapsulated command
 *
 * @param[in]  none
 *
 * @return TRUE or FALSE
 *
 *******************************************************************************/
uint32_t CDC_GetEncapsulatedResponse(void)
{

	/* added the handle */

	return (TRUE);
}

/*******************************************************************************
 * @brief      Set communication feature
 *
 * @param[in]  feature selection
 *
 * @return TRUE or FALSE
 *
 *******************************************************************************/
uint32_t CDC_SetCommFeature(uint16_t FeatureSelector)
{

	/* added the handle */

	return (TRUE);
}

/******************************************************************************
 * @brief      Get communication feature
 *
 * @param[in]  feature selection
 *
 * @return TRUE or FALSE
 *
 *******************************************************************************/
uint32_t CDC_GetCommFeature(uint16_t FeatureSelector)
{

	/* added the handle */

	return (TRUE);
}

/*******************************************************************************
 * @brief      Clear communication feature
 *
 * @param[in]  feature selection
 *
 * @return TRUE or FALSE
 *
 *******************************************************************************/
uint32_t CDC_ClearCommFeature(uint16_t FeatureSelector)
{

	/* added the handle */

	return (TRUE);
}

/*******************************************************************************
 * @brief      Set line coding
 *
 * @param[in]  none
 *
 * @return TRUE or FALSE
 *
 *******************************************************************************/
uint32_t CDC_SetLineCoding(void)
{

	CDC_LineCoding.dwDTERate = (EP0Buf[0] << 0)
	    | (EP0Buf[1] << 8)
	    | (EP0Buf[2] << 16)
	    | (EP0Buf[3] << 24);
	CDC_LineCoding.bCharFormat = EP0Buf[4];
	CDC_LineCoding.bParityType = EP0Buf[5];
	CDC_LineCoding.bDataBits = EP0Buf[6];

	return (TRUE);
}

/*******************************************************************************
 * @brief      Get line coding
 *
 * @param[in]  none
 *
 * @return TRUE or FALSE
 *
 *******************************************************************************/
uint32_t CDC_GetLineCoding(void)
{

	EP0Buf[0] = (CDC_LineCoding.dwDTERate >> 0) & 0xFF;
	EP0Buf[1] = (CDC_LineCoding.dwDTERate >> 8) & 0xFF;
	EP0Buf[2] = (CDC_LineCoding.dwDTERate >> 16) & 0xFF;
	EP0Buf[3] = (CDC_LineCoding.dwDTERate >> 24) & 0xFF;
	EP0Buf[4] = CDC_LineCoding.bCharFormat;
	EP0Buf[5] = CDC_LineCoding.bParityType;
	EP0Buf[6] = CDC_LineCoding.bDataBits;

	return (TRUE);
}

/*******************************************************************************
 * @brief      Set control line state
 *
 * @param[in]  control singnal bit map
 *
 * @return TRUE or FALSE
 *
 *******************************************************************************/
uint32_t CDC_SetControlLineState(uint16_t ControlSignalBitmap)
{

	/* added the handle */

	return (TRUE);
}

/*******************************************************************************
 * @brief      Send break
 *
 * @param[in]  Duration of break
 *
 * @return TRUE or FALSE
 *
 *******************************************************************************/
uint32_t CDC_SendBreak(uint16_t DurationOfBreak)
{

	/* added the handle */

	return (TRUE);
}

/*FUNCTION*----------------------------------------------------------------
*
* Function Name  : cdcInterface0Callback
* Returned Value :
* Comments       :
*         Handles cdc class and vendor specific requests
*
*END*--------------------------------------------------------------------*/
static bool cdcInterface0Callback(
					 /* USB handle */
					 void *usb_handle,
					 /* The setup packet void * */
					 void *setup_pkt,
					 /* Callback state */
					 uint8_t state)
{				/* Body */
	bool found = false;
	SETUP_STRUCT_PTR_t setup_ptr = (SETUP_STRUCT_PTR_t) setup_pkt;
	_usb_device_handle handle = usb_handle;

	switch (state) {
	case USB_CLASS_REQUEST_CALLBACK:
		switch (setup_ptr->REQUEST) {
		case CDC_SEND_ENCAPSULATED_COMMAND:
			CDC_SendEncapsulatedCommand();	/* data to be received, see USB_EVT_OUT */
			break;
		case CDC_GET_ENCAPSULATED_RESPONSE:
			CDC_GetEncapsulatedResponse();
			break;
		case CDC_SET_COMM_FEATURE:
			CDC_SetCommFeature(setup_ptr->VALUE);	/* data to be received, see USB_EVT_OUT */
			break;
		case CDC_GET_COMM_FEATURE:
			CDC_GetCommFeature(setup_ptr->VALUE);
			break;
		case CDC_CLEAR_COMM_FEATURE:
			CDC_ClearCommFeature(setup_ptr->VALUE);
			break;
		case CDC_SET_LINE_CODING:
			_usb_device_recv_data(handle, 0, EP0Buf, 7);

			_usb_device_send_data(handle, 0, 0, 0);

			CDC_SetLineCoding();

			os_thread_sleep(os_msec_to_ticks(50));

			found = true;

			break;
		case CDC_GET_LINE_CODING:
			CDC_GetLineCoding();

			_usb_device_send_data(handle, 0, EP0Buf, 7);

			_usb_device_recv_data(handle, 0, 0, 0);

			found = true;

			break;
		case CDC_SET_CONTROL_LINE_STATE:
			CDC_SetControlLineState(setup_ptr->VALUE);

			_usb_device_send_data(handle, 0, 0, 0);

			os_thread_sleep(os_msec_to_ticks(50));

			found = true;

			break;
		case CDC_SEND_BREAK:
			CDC_SendBreak(setup_ptr->VALUE);
			break;

		default:
			break;

		}		/* EndSwitch */
		break;

	case USB_VENDOR_REQUEST_CALLBACK:
		break;

	default:
		// Ignore
		break;
	}

	return (found);
}				/* Endbody */

/**
 * \brief Initialize the cdc interface.
 */

int usb_cdcdevice_init(void)
{
	usb_intf_config_t usbInterface0, usbInterface1;
	int status;

	/* init the interface0 */
	usbInterface0.NumOuts = 0;
	usbInterface0.NumIns = 0;
	usbInterface0.NumInts = 1;
	usbInterface0.EPType = e_BULK;
	usbInterface0.ClassCode = 2;
	usbInterface0.SubClass = 2;
	usbInterface0.Protocol = 0;
	usbInterface0.Function = e_CDC;
	usbInterface0.ClassDesc = CdcClassDesc;
	usbInterface0.ClassDescSize = sizeof(CdcClassDesc);
	usbInterface0.InterfAssocDes = CDCInterfAssocDesc;
	usbInterface0.InterfAssocName = CdcInter;
	usbInterface0.InterfaceName = NULL;
	usbInterface0.ZeroTermination = USB_DEVICE_ZERO_TERMINATE;
	usbInterface0.InterfaceCallback = cdcInterface0Callback;
	usbInterface0.ZLP = USB_DEVICE_DONT_ZLP;

	/* init the interface1 */
	usbInterface1.NumOuts = 1;
	usbInterface1.NumIns = 1;
	usbInterface1.NumInts = 0;
	usbInterface1.EPType = e_BULK;
	usbInterface1.ClassCode = 0x0A;
	usbInterface1.SubClass = 0;
	usbInterface1.Protocol = 0;
	usbInterface1.Function = e_CDC;
	usbInterface1.ClassDesc = NULL;
	usbInterface1.ClassDescSize = 0;
	usbInterface1.InterfAssocDes = NULL;
	usbInterface1.InterfAssocName = NULL;
	usbInterface1.InterfaceName = NULL;
	usbInterface1.ZeroTermination = USB_DEVICE_ZERO_TERMINATE;
	usbInterface1.InterfaceCallback = NULL;
	usbInterface1.ZLP = USB_DEVICE_DONT_ZLP;

	if ((status =
	     usb_dev_intf_add(USB_CDC_INTERFACE, &usbInterface0)) != WM_SUCCESS)
		usb_e
		    ("USB usbDevOpen failed for usb_device_init0000");

	if ((status =
	     usb_dev_intf_add(USB_CDC_INTERFACE, &usbInterface1)) != WM_SUCCESS)
		usb_e
		    ("USB usbDevOpen failed for usb_device_init1111");

	return status;
}
