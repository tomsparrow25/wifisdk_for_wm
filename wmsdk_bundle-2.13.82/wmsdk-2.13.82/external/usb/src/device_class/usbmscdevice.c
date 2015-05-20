
/* *********************************************************
* (c) Copyright 2008-2009 Marvell International Ltd. 
*
*               Marvell Confidential
* ==========================================================
 */
/**
 * \brief This is the usb mass storage interface file init function
*/

#include <wm_os.h>
#include <wmstdio.h>
#include <wmassert.h>

#include <incl/usb_port.h>
#include <incl/debug.h>
#include "ATypes.h"
#include <incl/devapi.h>
#include <incl/usb_device_api.h>
#include <incl/usbdrv.h>
#include <incl/usbsysinit.h>
#include <incl/usbmscdevice.h>



static int _max_LUNs(void)
{
  /* Without mass storage reflector none of the LUNs will work. Not sure why
     USBMS is being built in this case but, to be nice, we'll just pretend
     that there is 1 device (there are really none but -1 is not valid answer)*/
  return 0;
}

#define GET_MAX_LUN_REQUEST     0xFE
#define BULK_ONLY_RESET_REQUEST 0xFF

// The probability count trigger that we are not on a Windows machine.
#define TRIGGER_FOR_NON_WINDOWS 8

// We need to see at least this many descriptor requests to determine that we are
// likely on a Windows machine.
#define NUM_DESC_SEEN_FOR_NO_USB_RESET 8

// How much we will decrease the total descriptors count if we see the manufacturer
// string requested. Neither 2K, XP, or Vista as for it.
#define TOTAL_DESC_DECREASE_FOR_MANUFAC_STR 5

// The device handle.
static io_device_impl_t *MS_handle;

// The logical unit number (LUN).
static uint8_t num_LUNs = 0;

/*FUNCTION*----------------------------------------------------------------
* 
* Function Name  : get_max_lun_response
* Returned Value : 
* Comments       :
*     Chapter 9 Class specific request
*     See section 9.4.11 (page 195) of the USB 1.1 Specification.
* 
*END*--------------------------------------------------------------------*/

static void get_max_lun_response
    (
        /* USB handle */
        _usb_device_handle handle
    )
{   
#if defined(HAVE_USB_MASS_STORAGE_DEVICE)

    uint32_t total_descriptor_requests = 0;

    // If the print server reset timer is running, go ahead and send
    // a message to stop it. If we are handling this request, we are
    // not on a print server.
    if (!is_print_server_timer_stopped())
    {
        Send_Stop_PSR_Timer();
    }
    
    // Send the max LUN.
    _usb_device_send_data(handle, 0, &num_LUNs, 1);
    
    // Receive the handshake.
    _usb_device_recv_data(handle, 0, 0, 0);

    // Determine what type of host we are attached to.
    if (!has_host_detection_finished())
    {
        uint32_t non_Windows_prob_cnt;
        // Check to see how many descriptor requests we have received. This will help us
        // determine whether we are running on a Windows machine or not. By getting here
        // we know that we are most likely not on a print server at least.
        total_descriptor_requests = get_total_descriptor_requests();

        // 2K, XP, and Vista do not ask for this string. If it is seen decrease the total
        // descriptors seen.
        if (check_req_for_manufac_str_set())
        {
            if (total_descriptor_requests >= TOTAL_DESC_DECREASE_FOR_MANUFAC_STR)
            {
                total_descriptor_requests -= TOTAL_DESC_DECREASE_FOR_MANUFAC_STR;
            }
        }

        // This is where we check whether we are connnected to a Windows machine or not.
        // If it is something along the line of a Mac or Linux, go ahead and simulate a
        // USB plug pull. Neither of these operating systems work with Smart Install.
        non_Windows_prob_cnt = get_prob_cnt_non_Windows_host();
        if ((non_Windows_prob_cnt >= TRIGGER_FOR_NON_WINDOWS) ||
            (total_descriptor_requests < NUM_DESC_SEEN_FOR_NO_USB_RESET))
        {
            DPRINTF(DBG_SOFT|DBG_OUTPUT,("%s - Not connected to a Windows machine, resetting USB bus.\n", 
                                         __FUNCTION__));
            DPRINTF(DBG_SOFT|DBG_OUTPUT,("%s - will come up with a non-mass storage configuration.\n", 
                                         __FUNCTION__));

            // Signal that we are not attached to a Windows machine.
            set_not_on_Windows_machine(true);

            // This is for Apple. It will ask for the serial number before it asks for the configuration 
            // descriptor where the serial number is set depending upon the flags. It then gets all confused
            // about which serial number to use even though it gets the right one after the configuration
            // descriptor. It seems to ignore that one though. 
            set_the_serial_number(get_formatter_serial_number());

            // Do the reset.
            ext_usb_reset_command();
        }

        set_that_host_detection_is_complete();
    }
    return;
#else

    UINT8 max_luns[4];

    max_luns[0] = _max_LUNs();

    // Send max lun 
    _usb_device_send_data( handle, 0, max_luns, 1 ); 
    
    _usb_device_recv_data(handle, 0, 0, 0);

    return;

#endif

} 


/*FUNCTION*----------------------------------------------------------------
* 
* Function Name  : massStorageInterfaceCallback
* Returned Value : 
* Comments       : 
*         Handles mass storage class and vendor specific requests
* 
*END*--------------------------------------------------------------------*/
static bool massStorageInterfaceCallback
   (
      /* USB handle */
      void * usb_handle,
    
      /* The setup packet void * */
      void * setup_pkt,

      /* Callback state */
      uint8_t state
   )
{ /* Body */

#if defined(HAVE_USB_MASS_STORAGE_DEVICE)

    bool               found       = false;
    SETUP_STRUCT_PTR_t setup_ptr   = (SETUP_STRUCT_PTR_t) setup_pkt;
    _usb_device_handle handle      = usb_handle;
    uint8_t            intf_num    = 0;
    bool               intf_found  = false;
    bool               doing_setup = false;

    intf_found = get_interface_number(USB_MASS_STORAGE_INTERFACE, &intf_num);
    ASSERT(intf_found);

    //if (state == USB_CLASS_REQUEST_CALLBACK && 
    //    (setup_ptr->REQUEST == GET_MAX_LUN_REQUEST || setup_ptr->REQUEST == BULK_ONLY_RESET_REQUEST))
    //{
    //    DPRINTF((DBG_ERROR|DBG_OUTPUT),("%x, %x, %x %x\n", 
    //                                    intf_num, setup_ptr->INDEX, setup_ptr->VALUE, setup_ptr->LENGTH));
    //}
    
    switch (state) 
    {
    case USB_CLASS_REQUEST_CALLBACK:
        switch (setup_ptr->REQUEST) 
        {
           case GET_MAX_LUN_REQUEST:
               // Check that the interface number matches and that the value and length are correct.
               if ((setup_ptr->INDEX == intf_num) && 
                   (setup_ptr->VALUE == 0)        &&
                   (setup_ptr->LENGTH == 1))
               {
                   get_max_lun_response(handle);
                   found = true;
               }
               else
               {
                   _usb_device_stall_endpoint(handle, 0, 0);
               }
               
              break;

            case BULK_ONLY_RESET_REQUEST:
                // Check that the interface number matches and that the value and length are correct.
                if ((setup_ptr->INDEX == intf_num) && 
                    (setup_ptr->VALUE == 0)        &&
                    (setup_ptr->LENGTH == 0))
                {
                    // Note: the (void *) cast on the handle is used to avoid some #include difficulties
                    // when compiling. Also set the doing_setup to true since we only get here if it is
                    // true.
                    doing_setup = true;
                    handle_bulk_only_MS_reset((void *)handle, doing_setup);
                    found = true;
                }
                else
                {
                    _usb_device_stall_endpoint(handle, 0, 0);
                }
        
                break;

           default:
              break;
        } /* EndSwitch */
        break;

    default:
        // Ignore
        break;
    }

    return(found);
#else

    bool found = false;
    SETUP_STRUCT_PTR_t setup_ptr = setup_pkt;
    _usb_device_handle handle = usb_handle;

    switch (state) 
    {
    case USB_CLASS_REQUEST_CALLBACK:
        switch (setup_ptr->REQUEST) 
        {
           case GET_MAX_LUN_REQUEST:
              get_max_lun_response(handle);
              found = true;
              break;

           default:
              break;
        } /* EndSwitch */
        break;

    default:
        // Ignore
        break;
    }

    return(found);

#endif

} /* Endbody */



/**
 * \brief Initialize the usb mass storage interface.
 
 */

error_type_t usb_mscdevice_init(void)
{
    usb_intf_config_t usbInterface;
    error_type_t      status;

    // Initialize the mass storage interface.
    usbInterface.NumOuts = 1;
    usbInterface.NumIns = 1;    
    usbInterface.NumInts = 0;    
    usbInterface.EPType = e_BULK;// bob 08/27/12  IntNum->EPType
    usbInterface.ClassCode = 0x08;
    usbInterface.SubClass = 0x06;
    usbInterface.Protocol = 0x50;
    usbInterface.Function = e_MassStorage;
    usbInterface.ClassDesc = NULL;
    usbInterface.ClassDescSize = 0;
    usbInterface.InterfAssocDes = NULL;
    usbInterface.InterfAssocName = NULL;
    usbInterface.InterfaceName = "MassStorage";
    usbInterface.ZeroTermination = USB_DEVICE_DONT_ZERO_TERMINATE;
    usbInterface.InterfaceCallback = massStorageInterfaceCallback;
    usbInterface.ZLP = USB_DEVICE_DONT_ZLP;
#if defined(HAVE_USB_MASS_STORAGE_DEVICE)
    usbInterface.BOT               = 1;
#endif

    if ((status = usb_dev_intf_add(USB_MSC_INTERFACE, &usbInterface)) == OK)
    {
        
        if ((status = usb_dev_intf_open(&MS_handle, USB_MSC_INTERFACE)) != OK)
        {
            DPRINTF("\nUSB usb_dev_intf_open failed for usb_mscdevice_init\n");
        }
    }
    else
    {
        DPRINTF("\nUSB usb_dev_intf_add failed for usb_mscdevice_init\n");
    }

    return status;
}


void set_number_of_LUNs(uint32_t new_num_LUNs)
{
    num_LUNs = new_num_LUNs;
}

uint32_t get_number_of_LUNs()
{
    return(num_LUNs);
}

int32_t usb_MS_device_read(uint8_t *buffer, uint32_t len, uint32_t timeout)
{
    ASSERT(MS_handle);
    return(usb_dev_intf_read(MS_handle, buffer, len, timeout));
}

int32_t usb_MS_device_write(uint8_t *buffer, uint32_t len, uint32_t time_out)
{
    ASSERT(MS_handle);
    return(usb_dev_intf_write(MS_handle, buffer, len, time_out));
}

int32_t usb_MS_device_ioctl(ioctl_cmd_t cmd, void *data_for_ioctl)
{
    ASSERT(MS_handle);
    return(usb_dev_intf_ioctl(MS_handle, cmd, data_for_ioctl));
}
