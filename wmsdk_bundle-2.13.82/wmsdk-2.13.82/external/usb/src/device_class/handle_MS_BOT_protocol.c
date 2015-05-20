// /////////////////////////////////////////////////////////////////////////////
// 
//  ============================================================================
//  Copyright (c) 2011   Marvell Semiconductor, Inc. All Rights Reserved
//                       
//                          Marvell Confidential
//  ============================================================================
//
//
// Description: the module tasked with handling the mass storage class bulk only
//              transport protocol. 

//=============================================================================
// Includes
//=============================================================================
#include <stdint.h>
#include <string.h>
#include <incl/usb_port.h>
#include "io.h"
#include "error_types.h"
#include "debug.h"
#include "ATypes.h"
#include "MS_SCSI_cmds.h"
#include "MS_SCSI_top.h"
#include "usb_device_api.h"
#include "debug.h"
#include <wmassert.h>

#include <wm_os.h>
#include <wmstdio.h>

#include <incl/usbdrv.h>
#include <incl/usbsysinit.h>
#include <incl/usbmscdevice.h>


#ifdef HAVE_NAND
#include "fs_init.h"
#include "fixed_MS_dev_acs_routines.h"
#endif

int USBActive(void);
void MS_manager_thread();

/* Thread handle */
os_thread_t usb_ms_thread;

/* Buffer to be used as stack */
static os_thread_stack_define(usb_ms_stack, 8096);

//=============================================================================
// Defines
//=============================================================================

#define HAVE_SD


// The number message and stack size for the main parsing task.
#define MS_NUM_MSGS   5
#define MS_STACK_SIZE (6 * 1024)

// The number of ticks to wait for read data to appear.
#define MS_RD_DATA_TIMEOUT 20

//=============================================================================
// Structures
//=============================================================================
typedef struct part_name_to_prod_name_map_s
{
    char *part_name;
    char *prod_name;

} part_name_to_prod_name_map_t;


//=============================================================================
// Private data
//=============================================================================
#if defined(HAVE_NAND)
static part_name_to_prod_name_map_t part_name_to_prod_name_map[] = 
{
#if defined(HAVE_EI_PARTITION)
    {EI_PARTITION_NAME, "Smart Install"},
#endif
#if defined(HAVE_FAX_PARTITION)
    {FAX_PARTITION_NAME, "FAX Debug"},
#endif
#if defined(HAVE_KINOMA_PARTITION)
    {KINOMA_PARTITION_NAME, "Kinoma Debug"}
#endif
};
#endif


// Storage for the current CBW.
static CBW_STRUCT cur_CBW;

// What state we are currently in, whether waiting for a CBW or handling data.
static MS_STATE cur_MS_state = IN_CBW_STATE;

// A place to save off the number of LUNs that were registered.
static uint32_t num_registered_LUNs = 0;

// A place to save off the number of LUNs that are to be advertised.
static uint32_t num_advertised_LUNs = 0;


//=============================================================================
// Local functions
//=============================================================================
#if defined(HAVE_NAND)
static char *get_prod_name_from_part_name(char *part_name);
#endif

#if (defined(HAVE_USB_MASS_STORAGE_DEVICE) && defined(HAVE_NAND)) || \
     defined(HAVE_USB_MASS_STORAGE) || defined(HAVE_SD) || defined(HAVE_MEMS)
static char *get_vendor_info();
#endif

static void MS_manager();
static void wait_USB_active_again();
static bool check_for_valid_CBW(CBW_STRUCT *cbw_ptr, uint32_t num_bytes);
static void register_MS_SCSI_devices();


static error_type_t get_write_data(CBW_STRUCT *cbw_ptr, uint32_t *num_bytes_ret,
                                   uint8_t **ret_write_data_ptr);
static CSW_STRUCT *get_CSW(CBW_STRUCT *cbw_ptr);
static void set_CSW_status_for_IN(CSW_STRUCT *csw_ptr, CBW_STRUCT *cbw_ptr,
                                  scsi_data_t *SCSI_data);
static void set_CSW_status_for_OUT(CSW_STRUCT *csw_ptr, CBW_STRUCT *cbw_ptr,
                                   scsi_data_t *SCSI_data);
static void send_MS_data(uint8_t *data_ptr, uint32_t num_bytes_to_send);
static void wait_data_to_be_sent();
static void init_SCSI_data(scsi_data_t *SCSI_data);

uint32_t get_num_registered_LUNs()
{
    return(num_registered_LUNs);
}

uint32_t get_num_advertised_LUNs()
{
    return(num_advertised_LUNs);
}

MS_STATE get_MS_state()
{
    return (cur_MS_state);

}

//=============================================================================
// Local function definitions
//=============================================================================

//
// Do the registration of MS SCSI devices.
//
void register_MS_SCSI_devices()
{
// This is needed due to the proliferation of projects that are interconnected.
#if (defined(HAVE_USB_MASS_STORAGE_DEVICE) && defined(HAVE_NAND)) || \
     defined(HAVE_USB_MASS_STORAGE) || defined(HAVE_SD) || defined(HAVE_MEMS)
    char     *prod_ID       = NULL;
    char     *vendor_info   = get_vendor_info();
    char     *prod_revision = "1.0";
    uint8_t  drive_type     = DISK_DRIVE_TYPE;
    uint8_t  rem_media      = MEDIA_IS_REMOVABLE;
#endif

    // This will keep track of the logical unit numbers that are registered and those
    // that are advertised. All LUNs will be registered but only particular ones will
    // be advertised as a mass storage device when the USB MS is queried.    
    num_registered_LUNs = 0;
    num_advertised_LUNs = 0;

// These MS SCSI devices should be initialized first.
#ifdef HAVE_USB_MASS_STORAGE_DEVICE

#if defined(HAVE_NAND)
    uint32_t i;
    uint32_t num_partitions = get_num_NAND_partitions();

    for (i = 0; i < num_partitions; ++i)
    {
        char *part_name = (char *)get_NAND_partition_name(i);
        ASSERT(part_name);

        if (part_name)
        {
            media_manager_scsi_reg_device(part_name);
    
            prod_ID = get_prod_name_from_part_name(part_name);
    
#if defined(HAVE_EI_PARTITION)
            // The EI partition needs to be a CD-ROM drive type.
            if (!strcmp(EI_PARTITION_NAME, part_name))
            {
                drive_type = CD_ROM_TYPE;
                ++num_advertised_LUNs;

                // Tell the media manager that this device is read only.
                media_manager_scsi_init_dev_as_RO(part_name);
            }
            else
            {
                drive_type = DISK_DRIVE_TYPE;
            }
#else
            drive_type = DISK_DRIVE_TYPE;
#endif

            // Set that any writes will need a flush in order to make sure that
            // the data is pushed out of cache and into the permanent storage.
            media_manager_init_wrts_need_flush(part_name);

            init_SCSI_cmds_for_new_LUN(drive_type,
                                       rem_media,
                                       vendor_info,
                                       prod_ID,
                                       prod_revision);

            ++num_registered_LUNs;
        }
        else
        {
            dbg_printf("ERROR %s - no partition name for index: %u\n", __func__,
                       i);
        }
    }
#endif // defined(HAVE_NAND)

#endif // HAVE_USB_MASS_STORAGE_DEVICE

#ifdef HAVE_USB_MASS_STORAGE
    media_manager_scsi_reg_device( MEDIA_MANAGER_USB_DESC );

    prod_ID = MEDIA_MANAGER_USB_DESC;
    
    init_SCSI_cmds_for_new_LUN(drive_type,
                               rem_media,
                               vendor_info,
                               prod_ID,
                               prod_revision);

    ++num_registered_LUNs;
    ++num_advertised_LUNs;
#endif

#ifdef HAVE_SD
    //media_manager_scsi_reg_device( MEDIA_MANAGER_SD_DESC );

    //prod_ID = MEDIA_MANAGER_SD_DESC;
    prod_ID = "MC200 SD CARD ";

    init_SCSI_cmds_for_new_LUN(drive_type,
                               rem_media,
                               vendor_info,
                               prod_ID,
                               prod_revision);

    ++num_registered_LUNs;
    ++num_advertised_LUNs;
#endif

#ifdef HAVE_MEMS
    media_manager_scsi_reg_device( MEDIA_MANAGER_MEMS_DESC );

    prod_ID = MEDIA_MANAGER_MEMS_DESC;

    init_SCSI_cmds_for_new_LUN(drive_type,
                               rem_media,
                               vendor_info,
                               prod_ID,
                               prod_revision);

    ++num_registered_LUNs;
    ++num_advertised_LUNs;
#endif

    // The values for num_registered_LUNs and num_advertised_LUNs needs to be
    // adjusted back by one. This is how the USB MS LUN query works.
    if (num_registered_LUNs > 0)
    {
        num_registered_LUNs -= 1;
    }

    if (num_advertised_LUNs > 0)
    {
        num_advertised_LUNs -= 1;
    }

    // Tell the USB MS how many advertised LUNs we have.
    set_number_of_LUNs(num_advertised_LUNs);
}

//
// Find the product name for the inquiry command given the partition name. 
//
#if defined(HAVE_NAND)
char *get_prod_name_from_part_name(char *part_name)
{
    uint32_t i;
    uint32_t num_indices = 
                   sizeof(part_name_to_prod_name_map)/sizeof(part_name_to_prod_name_map_t);
    char     *prod_name = "Unknown Debug";

    for (i = 0; i < num_indices; ++i)
    {
        if (!strcmp(part_name, part_name_to_prod_name_map[i].part_name))
        {
            prod_name = part_name_to_prod_name_map[i].prod_name;
            break;
        }
    }

    return(prod_name);
}
#endif

//
// The mass storage thread.
//
void MS_manager()
{
    
#if defined(HAVE_USB_MASS_STORAGE_DEVICE) && defined(HAVE_NAND)
    // Make sure that the NAND is initialized before proceeding.
    wait_NAND_initialized();
#endif

    // Register all the SCSI devices that we have.
    register_MS_SCSI_devices();

    while (true)
    {
        error_type_t status;
        int32_t      bytes_read  = 0;
        uint32_t     timeout     = MS_RD_DATA_TIMEOUT;
        scsi_data_t  SCSI_data;
        CSW_STRUCT   *csw_ptr;

        // The current MS state that we are in.
        cur_MS_state = IN_CBW_STATE;

        bytes_read = usb_MS_device_read((uint8_t *)(&cur_CBW), 
                                        NUM_BYTES_IN_CBW, 
                                        timeout);

        if (bytes_read > 0)
        {
            bool valid_CBW;

            // Make sure that the CBW is valid.
            valid_CBW = check_for_valid_CBW(&cur_CBW, bytes_read);

            //dbg_printf("%s - valid CBW: %d\n", __func__, valid_CBW);

            if (valid_CBW)
            {
                bool cmd_is_an_OUT;
                uint32_t cur_command = cur_CBW.CBWCB[0];

                //if (cur_command != TEST_UNIT_RDY_CMD)
                //{
                //    dbg_printf("%#x.\n", cur_command);
                //}

                // Change the state to the data state.
                cur_MS_state = IN_DATA_STATE;

                // This just initializes the structure.
                init_SCSI_data(&SCSI_data);

                // Check whether this command is an OUT or not. This routine will
                // check a list as to whether the command is specified as an OUT or
                // not. It does not look at the CBW.
                cmd_is_an_OUT = is_cmd_an_OUT_cmd(cur_command, &cur_CBW);

                // In this instance check the CBW as to whether the OUT bit is
                // set or not. Even if it is supposed to be an IN command, data
                // could be coming which we have to retrieve and dump. The MSC
                // test likes to do things like that.

                /*bob 09/12/2012*/
                // Handle large-scale data from PC. Cut it into small pieces as heap resource 
                // is limited.

				if (OUT_CMD_SENT(&cur_CBW))                    
				  { 	
                       uint32_t total_wrt_bytes = 0;
					   do 
					   {
                             
                            //dbg_printf("%s - gettting write data for command: %#x\n", 
							//			 __func__, cur_command);							
                             status = get_write_data(&cur_CBW,
                                                     &(SCSI_data.num_wrt_bytes),
                                                     &(SCSI_data.wrt_data_ptr));
				             
								// Send the command off to be processed.                                            
                             status = handle_SCSI_cmd(&cur_CBW, &SCSI_data);    
						     if(SCSI_data.wrt_data_ptr != NULL)
                             {
                                USB_memfree_and_null(SCSI_data.wrt_data_ptr);
                             }
                             total_wrt_bytes = total_wrt_bytes + SCSI_data.num_wrt_bytes;
                             SCSI_data.num_wrt_bytes = total_wrt_bytes;                              
						}while(total_wrt_bytes!= SCSI_data.num_exp_wrt_bytes && status == OK);
				  }
                /*bob 09/12/2012*/
                // Handle large-scale data from device to PC. Cut it into small pieces as heap resource 
                // is limited.
				 else               
				 {
				      uint32_t total_rd_bytes = 0;  
                      do
                      {
                                                       
                            status = handle_SCSI_cmd(&cur_CBW, &SCSI_data);                            
                            if (status == OK)
                            {                   
                                // Send the data on its way.
                                send_MS_data(SCSI_data.read_data_ptr,
                                             SCSI_data.num_bytes_read);

                            // need time to finish the send process
                                wait_data_to_be_sent();
                            // free read pointer
                                SCSI_data.read_data_ptr = NULL;
                                total_rd_bytes = total_rd_bytes + SCSI_data.num_bytes_read;
                                SCSI_data.num_bytes_read = total_rd_bytes; 
                            }
                            
                            if (SCSI_data.send_stall)
                            {
                                // Wait for the data to be sent before we send the
                                // stall. Otherwise the stall will make it in ahead
                                // of the data.
                                wait_data_to_be_sent();        
                                usb_MS_device_ioctl(e_STALLIN, NULL);
                            }  
                            
					  }while(total_rd_bytes != SCSI_data.num_exp_rd_bytes && status == OK);
				 }
       
                // Get the command status word that we will return.
                csw_ptr = get_CSW(&cur_CBW);

                if (status == OK)
                {
                    // We branch here depending upon whether the command is supposed to
                    // be an OUT or not. Many times the MSC test will send IN commands
                    // with the OUT bit set in the CBW and we are expected to handle
                    // this according to the BOT spec.
                    if (cmd_is_an_OUT)
                    {
                        //dbg_printf("%s - An OUT was sent.\n", __func__);

                        set_CSW_status_for_OUT(csw_ptr, &cur_CBW, &SCSI_data);

                        if (csw_ptr->bCSWStatus != CSW_STATUS_PASSED)
                        {
                            usb_MS_device_ioctl(e_STALLIN, NULL);
                        }
                    }
                    else
                    {
                        set_CSW_status_for_IN(csw_ptr,
                                              &cur_CBW,
                                              &SCSI_data);
                        if (csw_ptr->bCSWStatus != CSW_STATUS_PASSED)
                        {
                            dbg_printf("%s - sending stall.\n", __func__);
                            usb_MS_device_ioctl(e_STALLIN, NULL);

                            // We are not going to send back the data so go ahead
                            // and free it.
                            if (SCSI_data.read_data_ptr)
                            {
                                USB_memfree_and_null(SCSI_data.read_data_ptr);
                            }

                            // In this case also check if the write data pointer
                            // is not NULL. This might be a case where the USB MSC
                            // test sent data when it was asking for read data.
                            if (SCSI_data.wrt_data_ptr)
                            {
                                USB_memfree_and_null(SCSI_data.wrt_data_ptr);
                            }
                        }
                     }
                }
                else
                {
                    //dbg_printf("%s - Handle SCSI command failed.\n", __func__);

                    // The command failed for some reason. Send a stall and set the 
                    // CSW appropriately.
                    usb_MS_device_ioctl(e_STALLIN, NULL);


                    csw_ptr->bCSWStatus      = CSW_STATUS_FAILED;

					csw_ptr->dCSWDataResidue = cur_CBW.dCBWDataLength;
				

                    // Check to see if the write or read pointer has memory
                    // associated with it. Free it if so, we are not going to do
                    // anything with it.
                    if (SCSI_data.read_data_ptr)
                    {
                        USB_memfree_and_null(SCSI_data.read_data_ptr);
                    }
                    if (SCSI_data.wrt_data_ptr)
                    {
                        USB_memfree_and_null(SCSI_data.wrt_data_ptr);
                    }
                }
                   
         
                // Send the CSW on its way.
                //dbg_printf("%s - sending the CSW\n", __func__);
                send_MS_data((uint8_t *)csw_ptr, NUM_BYTES_IN_CSW);

                // Someone else will free the data so just clear our pointer.
                csw_ptr = NULL;

            }
            else
            {
                // We did not get a valid CBW so set that we need the mass storage
                // reset before we will clear the stalls. This is another MSC test.
                //dbg_printf("ERROR %s - Not a valid CBW.\n", __func__);
                usb_MS_device_ioctl(e_STALLIN, NULL);
                usb_MS_device_ioctl(e_STALLOUT, NULL);

#ifdef HAVE_USB_MASS_STORAGE_DEVICE
                set_need_bulk_only_MS_reset();
#endif
            }  
        }
        else
        {
            // Make sure the USB is active if the read is zero.
            wait_USB_active_again();       
        }

    } // end while(true)
}


void MS_manager_thread()
{
        os_thread_create(&usb_ms_thread,        /* thread handle */
                         "usb_ms",      /* thread name */
                         MS_manager,    /* entry function */
                         0,     /* argument */
                         &usb_ms_stack, /* stack */
                         OS_PRIO_3);    /* priority - medium low */
}

//
// Go out and get write data.
//
/*bob 09/12/2012*/
/****************************************************************************//**
 * @brief  Simply cutting large scale downstream data into small pieces. The 
           size of piece is indicated by MSC_MAX_RW_BUFFER_SIZE.
           
 * @param[in]  cbw_ptr: point to a cbw struct
               num_byres_ret: point to the number of bytes have read from PC
               ret_write_data_ptr: point to the data

 * @return error type
 *******************************************************************************/
error_type_t get_write_data(CBW_STRUCT *cbw_ptr,
                            uint32_t   *num_bytes_ret,
                            uint8_t    **ret_write_data_ptr)			                    
{
    uint32_t     num_bytes_wrt_untransf = cbw_ptr->dCBWDataLength-*num_bytes_ret;
    uint32_t     num_bytes_to_read = le32_to_cpu(num_bytes_wrt_untransf);
    uint8_t      *data_buffer      = NULL;
    int32_t      bytes_read        = 0;
    uint32_t     timeout           = MS_RD_DATA_TIMEOUT;
    error_type_t status            = OK;

    *num_bytes_ret = 0;
    *ret_write_data_ptr = NULL;
    
	
	
    print_log("*****num_bytes_to_read = %d\n", num_bytes_to_read);
    
    // Get the write data if something is there.
    if (num_bytes_to_read > 0)
    {    
	    if(num_bytes_to_read > MSC_MAX_RW_BUFFER_SIZE)
		{
		    num_bytes_to_read = MSC_MAX_RW_BUFFER_SIZE;
			data_buffer = (uint8_t *)USB_memalloc_aligned(num_bytes_to_read, 32);		
		}
        else
		{  
		    data_buffer = (uint8_t *)USB_memalloc_aligned(num_bytes_to_read, 32);
        }
        if(NULL == data_buffer)
        {
            print_log("#####num_bytes_to_read = %d\n", num_bytes_to_read);
            while(1);
        }
        
        ASSERT(data_buffer);
	
        bytes_read = usb_MS_device_read(data_buffer, 
                                        num_bytes_to_read, 
                                        timeout);
        if (bytes_read > 0)
        {
            *num_bytes_ret      = bytes_read;
            *ret_write_data_ptr = data_buffer;
            status              = OK;
        }
        else
        {
            status = FAIL;
            dbg_printf("ERROR %s - failed to read the %d bytes of USB data.\n",
                       __func__, num_bytes_to_read);
        }
    }

    return(status);
}


//
// Mostly like the device has been disconnected from the USB, wait until it is
// reconnected.
//
void wait_USB_active_again()
{
    while (true)
    {
        if (USBActive())
        {
            break;
        }
        else
        {
            os_thread_sleep(1);
        }
    }
}

//
// Check to see if the data that was just received is a valid command
// block wrapper.
//
bool check_for_valid_CBW(CBW_STRUCT *cbw_ptr, uint32_t num_bytes)
{
    bool valid_CBW = false;

    // Check that we have a valid signature for command block wrapper and that 
    // the number of bytes received is correct.
    if ((le32_to_cpu(cbw_ptr->dCBWSignature) == USB_DCBW_SIGNATURE) && 
        (num_bytes == NUM_BYTES_IN_CBW))
    {
        valid_CBW = true;
    }

    return (valid_CBW);

} // end check_for_valid_CBW

//
// Get the storage for the CSW and also partially populate it.
//
CSW_STRUCT *get_CSW(CBW_STRUCT *cbw_ptr)
{
    CSW_STRUCT *csw_ptr;

    csw_ptr = (CSW_STRUCT *)USB_memalloc_aligned(NUM_BYTES_IN_CSW, 32);
    ASSERT(csw_ptr);

    csw_ptr->dCSWSignature = le32_to_cpu(USB_DCSW_SIGNATURE);
    csw_ptr->dCSWTag       = cbw_ptr->dCBWTag;
  
    return(csw_ptr);
}

//
// Set the CSW for the OUT status depending on the CBW and how much data has 
// been received.
//
void set_CSW_status_for_OUT(CSW_STRUCT  *csw_ptr,
                            CBW_STRUCT  *cbw_ptr,
                            scsi_data_t *SCSI_data)
{
    uint32_t residue;
    uint32_t data_len_swapped = le32_to_cpu(cbw_ptr->dCBWDataLength);

    // Make sure that the direction bit is set to OUT.
    if (OUT_CMD_SENT(cbw_ptr))
    {
        //dbg_printf("%s - %d %d %d\n", __func__, SCSI_data->num_exp_wrt_bytes,
        //           data_len_swapped, SCSI_data->num_wrt_bytes);

        // We received less than or equal to what was expected between the field of
        // the SCSI command and the specified CBW data length. These are both good.
        if (SCSI_data->num_exp_wrt_bytes <= data_len_swapped)
        {           
            if (SCSI_data->num_exp_wrt_bytes < data_len_swapped)
            {
                residue = data_len_swapped - SCSI_data->num_exp_wrt_bytes;
            }
            else
            { 
                residue = data_len_swapped - SCSI_data->num_wrt_bytes;
            }  
            csw_ptr->dCSWDataResidue = le32_to_cpu(residue);
            csw_ptr->bCSWStatus      = CSW_STATUS_PASSED;
        }
        // We received write data when the command did not expect it. This is
        // a fail.
        else if ((data_len_swapped > 0) && (SCSI_data->num_exp_wrt_bytes == 0))
        {
            csw_ptr->dCSWDataResidue = le32_to_cpu(data_len_swapped);
            csw_ptr->bCSWStatus      = CSW_STATUS_FAILED;
        }
        // The SCSI field data length was greater than the CBW data length. This
        // is a phase error.
        else if (SCSI_data->num_exp_wrt_bytes > data_len_swapped)
        {
            residue = SCSI_data->num_exp_wrt_bytes - data_len_swapped;

            csw_ptr->dCSWDataResidue = le32_to_cpu(residue);
            csw_ptr->bCSWStatus      = CSW_STATUS_PHASE_ERROR;
        }
        // Don't know how we got here.
        else
        {
            ASSERT(0);
            csw_ptr->dCSWDataResidue = le32_to_cpu(data_len_swapped);
            csw_ptr->bCSWStatus      = CSW_STATUS_FAILED;
        }
    }
    // The OUT bit was not set correctly for this command. Set a phase error.
    else
    {
        csw_ptr->dCSWDataResidue = le32_to_cpu(data_len_swapped);
        csw_ptr->bCSWStatus      = CSW_STATUS_PHASE_ERROR;
    }
}

//
// Set the CSW for the OUT status depending on the CBW and how much data has 
// been returned.
//
void set_CSW_status_for_IN(CSW_STRUCT  *csw_ptr,
                           CBW_STRUCT  *cbw_ptr,
                           scsi_data_t *SCSI_data)
{
    uint32_t residue;
    uint32_t data_len_swapped = le32_to_cpu(cbw_ptr->dCBWDataLength);

    // Make sure that the direction bit is set to IN.
    if (IN_CMD_SENT(cbw_ptr))
    {
        //dbg_printf("%s - %d %d %d\n", __func__, SCSI_data->num_exp_rd_bytes,
        //           data_len_swapped, SCSI_data->num_bytes_read);

        // If we received what we expected or a bit less, this is a good status.
        // Set the residue accordingly if any.
        if (SCSI_data->num_exp_rd_bytes <= data_len_swapped)
        {           

            if (SCSI_data->num_exp_rd_bytes < data_len_swapped)
            {
                residue = data_len_swapped - SCSI_data->num_exp_rd_bytes;
            }
            else
            {
                residue = data_len_swapped - SCSI_data->num_bytes_read;
            }
            csw_ptr->dCSWDataResidue = le32_to_cpu(residue);
            csw_ptr->bCSWStatus      = CSW_STATUS_PASSED;
        }
        // This is the case where data was requested but not expected.
        // This is a failed status.
        else if ((data_len_swapped > 0) && (SCSI_data->num_exp_rd_bytes == 0))
        {
            csw_ptr->dCSWDataResidue = le32_to_cpu(data_len_swapped);
            csw_ptr->bCSWStatus      = CSW_STATUS_FAILED;
        }
        // The data requested in the SCSI command does not match the data length
        // specified in the CBW. This is a phase error.
        else if (SCSI_data->num_exp_rd_bytes > data_len_swapped)
        {
            residue = SCSI_data->num_exp_rd_bytes - data_len_swapped;

            csw_ptr->dCSWDataResidue = le32_to_cpu(residue);
            csw_ptr->bCSWStatus      = CSW_STATUS_PHASE_ERROR;
        }
        // Don't know how we got here.
        else
        {
            ASSERT(0);
            csw_ptr->dCSWDataResidue = le32_to_cpu(data_len_swapped);
            csw_ptr->bCSWStatus      = CSW_STATUS_FAILED;
        }
    }
    // The IN bit was not set correctly for this command. Set a phase error.
    else
    {
        csw_ptr->dCSWDataResidue = le32_to_cpu(data_len_swapped);
        csw_ptr->bCSWStatus      = CSW_STATUS_PHASE_ERROR;
    }
}


// 
// Handle the details of writing data back to the USB driver. This data
// will be send back to the host on the next IN request.
// 
void send_MS_data(uint8_t  *data_ptr,
                  uint32_t num_bytes_to_send)
{
    uint32_t num_bytes_wrt;
    uint32_t timeout = 0;

    // Write the data to the USB ISR write queue. This will be sent when the next
    // IN is sent by the host.
    num_bytes_wrt = usb_MS_device_write(data_ptr, num_bytes_to_send, timeout);

    if (num_bytes_wrt != num_bytes_to_send)
    {
        DPRINTF("ERROR %s: only %d of %d bytes written.\n", 
                                     __FUNCTION__, num_bytes_wrt, num_bytes_to_send);

        // Check to see if we are connected or not. If we are, go ahead and reset as
        // there is not much else we can do at the moment.
        if (is_USB_device_connected())
        {
            dbg_printf("%s - Reset to recover.\n", __FUNCTION__);
            os_thread_sleep(100);
        }
        // We are not connected. Just go ahead and return, no reset is necessary.
        else
        {
            DPRINTF("%s - port not attached, no reset necessary\n",__FUNCTION__);
        }
    }
    
} // end send_MS_data

// 
// The idea with this function is that it is called before a stall is sent in
// certain cases. Therefore we want to look for whether the data was sent or
// not and if not to wait for a tick. After the wait, check again. We do not
// want to loop a bunch of times because if there is a problem we are going to
// send a stall anyway.
// 
void wait_data_to_be_sent()
{
    int i;
    int ticks_to_wait = 2;

    for (i = 0; i < ticks_to_wait; ++i)
    {
        if(check_MS_data_sent())
        {
            break;
        }
        else
        {
            os_thread_sleep(1);
        }
    }
}

//
// Initialize the SCSI data structure.
//
void init_SCSI_data(scsi_data_t *SCSI_data)
{
    memset(SCSI_data, 0, sizeof(scsi_data_t));
}

#if (defined(HAVE_USB_MASS_STORAGE_DEVICE) && defined(HAVE_NAND)) || \
     defined(HAVE_USB_MASS_STORAGE) || defined(HAVE_SD) || defined(HAVE_MEMS)

char *get_vendor_info()
{
    // A maximum of 8 characters allowed in the vendor information name.
#ifdef DEFAULT_VENDOR_INFO
    return(DEFAULT_VENDOR_INFO);
#else
    return("NONE");
#endif
}

#endif
