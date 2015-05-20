/*
 * ==========================================================
 * (c) Copyright 2008-2014 Marvell International Ltd. 
 *
 *               Marvell Confidential
 * ==========================================================
 */
/**
 * \brief This is the usb hid interface file init function
*/

#include <wm_os.h>
#include <wmstdio.h>

#include <incl/devapi.h>
#include <incl/usb_device_api.h>
#include <incl/usbdrv.h>
#include <incl/usbsysinit.h>
#include <incl/usb_hid.h>
#include <incl/debug.h>




uint8_t ReportDescriptor[] ={
    0x06, 0x00,0xFF,               // USAGE_PAGE(User define)
    0x09, 0x01,                    // USAGE(User define)
    0xa1, 0x01,                    // COLLECTION (Application)
    0x09, 0x02,                    // USAGE(User define)
    0x15, 0x00,                    //   LOGICAL_MINIMUM (0)
    0x25, 0xFF,                    //   LOGICAL_MAXIMUM (255)
    0x75, 0x08,                    //   REPORT_SIZE (8)
    0x95, 0x01,                    //   REPORT_COUNT (1)
    0x81, 0x02,                    //   INPUT (Data,Array,Abs)
    0x09, 0x03,                    // USAGE(User define)
    0x15, 0x00,                    //   LOGICAL_MINIMUM (0)
    0x25, 0xFF,                    //   LOGICAL_MAXIMUM (255)
    0x95, 0x08,                    //   REPORT_COUNT (8)
    0x75, 0x01,                    //   REPORT_SIZE (1)
    0x91, 0x00,                    //   OUTPUT (Data,Array,Abs)
    0xc0                           // END_COLLECTION
  }; /* ReportDescriptor */


uint8_t ReportDescriptorSize = sizeof(ReportDescriptor);
//extern TX_THREAD               thread_usb_hid;

/*
    These strings are for identifying the various interfaces.
*/
static char HidInter[]="MARVELL-MC200-HID";
uint8_t  LedEP0Buf[8];




/****************************************************************************//**
 * @brief      delay cycles
 *
 * @param[in]  cycle: number of cycles to delay
 *
 * @return none
 *
 *******************************************************************************/
void delay(uint32_t cycle)
{
  cycle = (cycle << 7);

  while(cycle--);

}

/****************************************************************************//**
 * @brief      Get Report command 
 *
 * @param[in]  _usb_device_handle
 *
 * @return TRUE or FALSE
 *
 *******************************************************************************/
uint32_t HID_GetReport(_usb_device_handle handle) 
{
  /* added the handle */
  _usb_device_send_data(handle, 0, ReportDescriptor, sizeof(ReportDescriptor));
  
  return (TRUE);
}

/****************************************************************************//**
 * @brief      Get Idle command 
 *
 * @param[in]  none
 *
 * @return TRUE or FALSE
 *
 *******************************************************************************/
uint32_t HID_GetIdle(void ) 
{

  /* added the handle */
  return (TRUE);
}

/****************************************************************************//**
 * @brief      Get Protocol
 *
 * @param[in]  usb_device_handle
 *
 * @return TRUE or FALSE
 *
 *******************************************************************************/
uint32_t HID_GetProtocol(_usb_device_handle handle) 
{
  /* added the handle */

  return (TRUE);
}

/****************************************************************************//**
 * @brief      Set Report
 *
 * @param[in]  none
 *
 * @return TRUE or FALSE
 *
 *******************************************************************************/
uint32_t HID_SetReport (_usb_device_handle handle) 
{

  /* added the handle */

  return (TRUE);
}

/****************************************************************************//**
 * @brief      Set Idle
 *
 * @param[in]  none
 *
 * @return TRUE or FALSE
 *
 *******************************************************************************/
uint32_t HID_SetIdle () 
{

  /* added the handle */
  
  return (TRUE);
}

/****************************************************************************//**
 * @brief      Set Protocol
 *
 * @param[in]  none
 *
 * @return TRUE or FALSE
 *
 *******************************************************************************/
uint32_t HID_SetProtocol (void) 
{
  /* added the handle */
  return (TRUE);
}



/*FUNCTION*----------------------------------------------------------------
* 
* Function Name  : hidInterface0Callback
* Returned Value : 
* Comments       : 
*         Handles hid class and vendor specific requests
* 
*END*--------------------------------------------------------------------*/
static bool hidInterface0Callback
(
      /* USB handle */
      void * usb_handle,

      /* The setup packet void * */
      void * setup_pkt,

      /* Callback state */
      uint8_t state
   )
{ /* Body */
   bool found = false;
   SETUP_STRUCT_PTR_t setup_ptr = (SETUP_STRUCT_PTR_t) setup_pkt;
   _usb_device_handle handle = usb_handle;
   
   switch (state) 
   {
   case USB_CLASS_REQUEST_CALLBACK:
       switch (setup_ptr->REQUEST) 
       {
          case HID_GET_REPORT_REQUEST:
               HID_GetReport(handle);     
               found = true;
            break;
            
          case HID_GET_IDLE_REQUEST:
               HID_GetIdle(); 
            break;
            
          case HID_GET_PROTOCOL_REQUEST:
               HID_GetProtocol(handle);                       /* data to be received, see USB_EVT_OUT */
            break;
            
          case HID_SET_REPORT_REQUEST:                                               

			   HID_SetReport(handle); 
			   
               found = true;                 
           break;
            
            
          case HID_SET_IDLE_REQUEST:      
             HID_SetIdle();               
            break;
            
          case HID_SET_PROTOCOL:  
             HID_SetProtocol();            
            break;
            
          default:
             break;

       } /* EndSwitch */
       break;

   case USB_VENDOR_REQUEST_CALLBACK:
       break;

   default:
       // Ignore
       break;
   }

   return(found);
} /* Endbody */

/**
 * \brief Initialize the print interface.
 */

error_type_t usb_hiddevice_init(void)
{
    usb_intf_config_t usbInterface0;
    error_type_t status;
    HID_DESCRIPTOR  USB_HID_DESCRIPTOR;

    /* init the HID descriptor */
    USB_HID_DESCRIPTOR.bLength = (uint8_t) sizeof(HID_DESCRIPTOR);                /* size of this descriptor in bytes */
    USB_HID_DESCRIPTOR.bDescriptorType = HID_DESCRIPTOR_TYPE;                     /* HID descriptor type */
    USB_HID_DESCRIPTOR.bcdHID = WBVAL(HID_V1_10);                                 /* USB HID specification release version */
    USB_HID_DESCRIPTOR.bCountrycode = 0x00;                                       /* Hardware target country */
    USB_HID_DESCRIPTOR.bNumDescriptorType = 0x01;                                 /* Number of HID class descriptors to follow */
    USB_HID_DESCRIPTOR.bReportDescriptorType = HID_REPORT_DESCRIPTOR_TYPE;       /* Report descriptor type */
    USB_HID_DESCRIPTOR.wDescriptorLength = WBVAL(sizeof(ReportDescriptor));       /* Total length of Report decriptor*/
    
  
     
    /* init the interface0 */
    usbInterface0.NumOuts = 1;
    usbInterface0.NumIns = 1;
    usbInterface0.EPType = e_INTERRUPT;
    usbInterface0.ClassCode = HID_INTERFACE_CLASS;
    usbInterface0.SubClass = HID_DEVICE_SUBCLASS_NONEBOOT ;
    usbInterface0.Protocol = HID_PROTOCOL_NONE;
    usbInterface0.Function = e_HID;
    usbInterface0.ClassDesc = (uint8_t *) &USB_HID_DESCRIPTOR;
    usbInterface0.ClassDescSize = sizeof(USB_HID_DESCRIPTOR) ;
    usbInterface0.InterfAssocDes = NULL ;
    usbInterface0.InterfAssocName = NULL;
    usbInterface0.InterfaceName = HidInter;
    usbInterface0.ZeroTermination = USB_DEVICE_ZERO_TERMINATE;
    usbInterface0.InterfaceCallback = hidInterface0Callback;
    usbInterface0.ZLP = USB_DEVICE_DONT_ZLP;

    
    if ((status = usb_dev_intf_add(USB_HID_INTERFACE, &usbInterface0)) != OK)
    {    
        DPRINTF("\nUSB usbDevOpen failed for usb_prtdevice_init0000\n");
    }
   
    return status;
}

 
