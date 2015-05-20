// /////////////////////////////////////////////////////////////////////////////
// 
//  ============================================================================
//  Copyright (c) 2011   Marvell Semiconductor, Inc. All Rights Reserved
//                       
//                          Marvell Confidential
//  ============================================================================
//
//
// Description: this module is tasked with handling, storing and returning
//              information about about a SCSI drive such as request sense commands,
//              inquiry commands and so on. All the information is stored here
//              and this modules does not access the media for its information. 


//=============================================================================
// Includes
//=============================================================================
#include <stdint.h>
#include <string.h>
#include "error_types.h"
#include "MS_SCSI_cmds.h"
#include "MS_SCSI_top.h"
#include "debug.h"
#include <wmassert.h>

#include <wm_os.h>
#include <wmstdio.h>
#include <incl/agLinkedList.h>
#include <incl/devapi.h>
#include <incl/usbprv_dev.h>
#include <incl/asic_usb.h>
#include <incl/usb_list.h>
#include <incl/usb_device_api.h>
#include <incl/usbdrv.h>
#include <incl/io.h>
#include <incl/usbprvdev.h>


//=============================================================================
// Private data
//=============================================================================
static uint32_t LUN_index = 0;

// Arrays of pointers to structures that contain data that we sent back on
// various commands that this module handles.
static scsi_request_sense_t      *request_sense_array[MAX_LUN_NUM + 1]        = {NULL};
static scsi_inquiry_t            *inquiry_array[MAX_LUN_NUM + 1]              = {NULL};
static scsi_mode_sense_six_t     *mode_sense_six_array[MAX_LUN_NUM + 1]       = {NULL};
static scsi_mode_sense_ten_t     *mode_sense_ten_array[MAX_LUN_NUM + 1]       = {NULL};
static scsi_event_status_notif_t *event_status_notif_array[MAX_LUN_NUM + 1]   = {NULL};
static scsi_read_disk_protect_t  *read_disk_protect_array[MAX_LUN_NUM + 1]    = {NULL};
static report_key_rpc_state_t    *report_key_rpc_state_array[MAX_LUN_NUM + 1] = {NULL};

// Used to return the request sense information.
static scsi_request_sense_t ret_req_sense_info = {0};

// Used to hold the read only device indices.
static uint32_t *RO_device_indices = NULL;
static uint32_t num_RO_dev_changed = 0;

//=============================================================================
// Local function declarations
//=============================================================================
static void init_request_sense_info();
static void init_inquiry_info(uint8_t drive_type, uint8_t rem_media, char *vendor_info, 
                              char *prod_info, char *rev_level);
static void init_mode_sense_six_info();
static void init_mode_sense_ten_info();
static void init_event_status_notif_info();
static void init_read_disk_protect_info();
static void init_report_key_rpc_state_info();
static void clear_request_sense_data(uint8_t LUN);



void init_SCSI_other_cmds_for_new_LUN(uint8_t drive_type,
                                      uint8_t rem_media,
                                      char    *vendor_info, 
                                      char    *prod_info, 
                                      char    *rev_level)
{
    init_request_sense_info();
    init_inquiry_info(drive_type, rem_media, vendor_info, prod_info, rev_level);
    init_mode_sense_six_info();
    init_mode_sense_ten_info();
    init_event_status_notif_info();
    init_read_disk_protect_info();
    init_report_key_rpc_state_info();

    // Only increment this here.
    ++LUN_index;
}

error_type_t handle_SCSI_other_cmd(CBW_STRUCT *cbw_ptr, scsi_data_t *SCSI_data)
{
    uint8_t LUN = cbw_ptr->bCBWLun;

    // If this is zero, nothing has been initialized.
    ASSERT(LUN_index);

    SCSI_data->read_data_ptr  = NULL;
    SCSI_data->num_bytes_read = 0;
    SCSI_data->send_stall     = false;

    if (LUN > (LUN_index - 1))
    {
        SCSI_data->send_stall     = true;
        SCSI_data->command_status = CSW_STATUS_FAILED;

        dbg_printf("ERROR %s - requested LUN: %u > max LUN: %u\n", __func__,
                   cbw_ptr->bCBWLun, LUN_index - 1);

        return(FAIL);
    }

    uint8_t      cur_command      = cbw_ptr->CBWCB[0];
    uint32_t     num_bytes_in_cmd = 0;
    uint8_t      *cmd_data        = NULL;
    error_type_t status           = OK;

    switch (cur_command)
    {
    case REQUEST_SENSE_CMD:           

        // Copy the data to another structure that we will return as we will be clearing
        // the original request sense data.
        memcpy((void *)&ret_req_sense_info, 
               request_sense_array[LUN], 
               sizeof(scsi_request_sense_t));

        num_bytes_in_cmd = sizeof(scsi_request_sense_t);
        cmd_data         = (uint8_t *)&ret_req_sense_info;
        
        clear_request_sense_data(LUN);
        break;         
        
    case INQUIRY_CMD:
        num_bytes_in_cmd = sizeof(scsi_inquiry_t);
        cmd_data         = (uint8_t *)inquiry_array[LUN];
        break;

    case MODE_SENSE_6_CMD:
        num_bytes_in_cmd = sizeof(scsi_mode_sense_six_t);
        cmd_data         = (uint8_t *)mode_sense_six_array[LUN];
        break;

    case MODE_SENSE_10_CMD:
        num_bytes_in_cmd = sizeof(scsi_mode_sense_ten_t);
        cmd_data         = (uint8_t *)mode_sense_ten_array[LUN];
        break;

    case GET_EVENT_STATUS_NOTIF_CMD:
        num_bytes_in_cmd = sizeof(scsi_event_status_notif_t);
        cmd_data         = (uint8_t *)event_status_notif_array[LUN];
        break;

    case READ_DISK_STRUCT_CMD:
        num_bytes_in_cmd = sizeof(scsi_read_disk_protect_t);
        cmd_data         = (uint8_t *)read_disk_protect_array[LUN];
        break;

    // We do not really do anything with these command except respond with a good
    // status.
    case SET_CD_SPEED_CMD:
    case START_STOP_UNIT_CMD:
    case VERIFY_CMD:
    case SYNC_CACHE_CMD:
    case CLOSE_TRACK_SESSION_CMD:
        num_bytes_in_cmd = 0;
        cmd_data         = NULL;
        break;

    case REPORT_KEY_CMD:
        {
            // Check to see that they are asking for the RPC state. This is the only one
            // that we handle.
            if ((cbw_ptr->CBWCB[10] & 0x3f) == 0x8)
            {
                num_bytes_in_cmd = sizeof(report_key_rpc_state_t);
                cmd_data         = (uint8_t *)report_key_rpc_state_array[LUN];
            }
            else
            {
                status = FAIL;
                set_request_sense_data(LUN, 
                                       ILLEGAL_REQUEST, 
                                       INVALID_FIELD_IN_CDB_ASC,
                                       INVALID_FIELD_IN_CDB_ASCQ);
            }
        }
        break;

    default:
        dbg_printf("%s - unknown command: %#x\n", __func__, cur_command);
        ASSERT(0);
        break;
    }

    if (num_bytes_in_cmd > 0 && status == OK)
    {
        set_returned_data(cbw_ptr, SCSI_data, (char *)cmd_data, num_bytes_in_cmd);

        // When the media is accessed, this is used to determine in the CSW what the
        // residue will be and as such is set to the amount of data expected in the
        // SCSI command. Here however we will simply set it to the amount of data
        // read and effectively make the residue zero.
        SCSI_data->num_exp_rd_bytes = SCSI_data->num_bytes_read;
    }

    if (status == OK)
    {
        SCSI_data->command_status = CSW_STATUS_PASSED;
    }
    else
    {
        SCSI_data->command_status = CSW_STATUS_FAILED;
    }

    return(status);
}

void make_RO_devices_writable()
{
    uint32_t i;

    // If the device memory has not already been allocated, go ahead and do so.
    if (!RO_device_indices)
    {
        RO_device_indices = USB_memcalloc(LUN_index * sizeof(uint32_t), 1);
        ASSERT(RO_device_indices);
    }


    for (i = 0; i < LUN_index; ++i)
    {
        if (inquiry_array[i]->peripheral_device_type == CD_ROM_TYPE)
        {
            inquiry_array[i]->peripheral_device_type = DISK_DRIVE_TYPE;

            /* Haitao comment unuseful for SD card */
            //media_manager_scsi_set_dev_as_RW(i);

            RO_device_indices[num_RO_dev_changed] = i;
            ++num_RO_dev_changed;
        }
    }

    //dbg_printf("%s - num_RO_dev_changed: %d\n", __func__, num_RO_dev_changed);
}

void ret_RO_devices_to_default()
{
    uint32_t i;
    uint32_t changed_indice;

    ASSERT(num_RO_dev_changed <= LUN_index);

    for (i = 0; i < num_RO_dev_changed; ++i)
    {
        changed_indice = RO_device_indices[i];
        inquiry_array[changed_indice]->peripheral_device_type = CD_ROM_TYPE;

        /* Haitao comment unuseful for SD card */
        //media_manager_scsi_set_dev_as_RO(changed_indice);
    }

    if (RO_device_indices)
    {
        USB_memfree_and_null(RO_device_indices);
        num_RO_dev_changed = 0;
    }
}

uint8_t get_device_type_of_LUN(uint8_t LUN)
{
    if (LUN > (LUN_index - 1))
    {
        dbg_printf("ERROR %s - requested LUN: %u too large, max: %u.\n", __func__,
                   LUN, LUN_index - 1);

        ASSERT(0);
        // Return something nonsensical for the device type if the assert is not
        // functional.
        return(0xFF);
    }

    return(inquiry_array[LUN]->peripheral_device_type);
}

//=============================================================================
// Local function declarations
//=============================================================================

void set_request_sense_data(uint8_t LUN,
                            uint8_t sense_key, 
                            uint8_t add_sense_code,
                            uint8_t add_sense_code_qual)
{
    // Make sure that the LUN is in range.
    if (LUN > (LUN_index - 1))
    {
        ASSERT(0);
        return;
    }

    request_sense_array[LUN]->sense_key           = sense_key;
    request_sense_array[LUN]->add_sense_code      = add_sense_code;
    request_sense_array[LUN]->add_sense_code_qual = add_sense_code_qual;
}

void init_request_sense_info()
{
    request_sense_array[LUN_index] = USB_memcalloc(sizeof(scsi_request_sense_t), 1);
    ASSERT(request_sense_array[LUN_index]);

    // Set the default non-zero values.
    request_sense_array[LUN_index]->response_code = 0x70;
    request_sense_array[LUN_index]->sense_key     = NO_SENSE;
    request_sense_array[LUN_index]->add_sense_len = 0xA;
}

void init_inquiry_info(uint8_t drive_type,
                       uint8_t rem_media,
                       char    *vendor_info, 
                       char    *prod_info, 
                       char    *rev_level)
{
    // Reset the variables that control the bus reset.
#if defined(HAVE_NAND)
    init_bus_reset_state();
#endif

    inquiry_array[LUN_index] = USB_memcalloc(sizeof(scsi_inquiry_t), 1);
    ASSERT(inquiry_array[LUN_index]);
/*bob 10/11/2012*/
  uint8_t BulkBuf[36];
  uint32_t i;

  BulkBuf[ 0] = 0x00;          /* Direct Access Device */
  BulkBuf[ 1] = 0x80;          /* RMB = 1: Removable Medium */
  BulkBuf[ 2] = 0x00;          /* Version: No conformance claim to standard */
  BulkBuf[ 3] = 0x01;

  BulkBuf[ 4] = 36-4;          /* Additional Length */
  BulkBuf[ 5] = 0x80;          /* SCCS = 1: Storage Controller Component */
  BulkBuf[ 6] = 0x00;
  BulkBuf[ 7] = 0x00;

  BulkBuf[ 8] = 'M';           /* Vendor Identification */
  BulkBuf[ 9] = 'A';
  BulkBuf[10] = 'R';
  BulkBuf[11] = 'V';
  BulkBuf[12] = 'E';
  BulkBuf[13] = 'L';
  BulkBuf[14] = 'L';
  BulkBuf[15] = ' ';

  BulkBuf[16] = 'M';           /* Product Identification */
  BulkBuf[17] = 'C';
  BulkBuf[18] = '2';
  BulkBuf[19] = '0';
  BulkBuf[20] = '0';
  BulkBuf[21] = '-';
  BulkBuf[22] = 'D';
  BulkBuf[23] = 'i';
  BulkBuf[24] = 's';
  BulkBuf[25] = 'k';
  BulkBuf[26] = ' ';
  BulkBuf[27] = ' ';
  BulkBuf[28] = ' ';
  BulkBuf[29] = ' ';
  BulkBuf[30] = ' ';
  BulkBuf[31] = ' ';

  BulkBuf[32] = '1';           /* Product Revision Level */
  BulkBuf[33] = '.';
  BulkBuf[34] = '0';
  BulkBuf[35] = ' ';

  for(i = 0; i < 36; i++)
  {
    *(((uint8_t *)inquiry_array[LUN_index])+ i) = BulkBuf[i];
  }

}

void init_mode_sense_six_info()
{
    mode_sense_six_array[LUN_index] = USB_memcalloc(sizeof(scsi_mode_sense_six_t),
                                                           1);
    ASSERT(mode_sense_six_array[LUN_index]);

    mode_sense_six_array[LUN_index]->mode_data_length = 0x3;
}

void init_mode_sense_ten_info()
{
    mode_sense_ten_array[LUN_index] = USB_memcalloc(sizeof(scsi_mode_sense_ten_t), 
                                                          1);
    ASSERT(mode_sense_ten_array[LUN_index]);

    mode_sense_ten_array[LUN_index]->mode_data_length[1] = 0x6;
}

void init_event_status_notif_info()
{
    event_status_notif_array[LUN_index] = USB_memcalloc(sizeof(scsi_event_status_notif_t), 
                                                     1);
    ASSERT(event_status_notif_array[LUN_index]);
}

void init_read_disk_protect_info()
{
    read_disk_protect_array[LUN_index] = USB_memcalloc(sizeof(scsi_read_disk_protect_t), 1);
    ASSERT(read_disk_protect_array[LUN_index]);

    read_disk_protect_array[LUN_index]->rds_base_info.disk_struct_data_len[1] = 6;
}

void init_report_key_rpc_state_info()
{
    report_key_rpc_state_array[LUN_index] = USB_memcalloc(sizeof(report_key_rpc_state_t), 1);
    ASSERT(report_key_rpc_state_array[LUN_index]);

    report_key_rpc_state_array[LUN_index]->data_length[1] = 0x6;
    report_key_rpc_state_array[LUN_index]->tc_vra_ucca    = 0x25;
    report_key_rpc_state_array[LUN_index]->region_mask    = 0xFF;
}

void clear_request_sense_data(uint8_t LUN)
{
    memset(&request_sense_array[LUN], 0, sizeof(scsi_request_sense_t));
    
    request_sense_array[LUN]->response_code = 0x70;
    request_sense_array[LUN]->add_sense_len = 0xA;
}

