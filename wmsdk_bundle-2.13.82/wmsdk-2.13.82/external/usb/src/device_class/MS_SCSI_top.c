// /////////////////////////////////////////////////////////////////////////////
// 
//  ============================================================================
//  Copyright (c) 2011   Marvell Semiconductor, Inc. All Rights Reserved
//                       
//                          Marvell Confidential
//  ============================================================================
//
//
// Description: this is the top-level module for handling SCSI transactions on 
//              mass storage.


//=============================================================================
// Includes
//=============================================================================
#include <stdint.h>
#include <string.h>

#include <incl/usb_port.h>
#include <incl/devapi.h>
#include <incl/usb_device_api.h>
#include <incl/usbdrv.h>
#include <incl/usbprvdev.h>
#include <wm_os.h>
#include <wmstdio.h>

#include "error_types.h"
#include <wmassert.h>
#include "MS_SCSI_cmds.h"
#include "MS_SCSI_top.h"
#include "media_sd.h"
#include "io.h"
#include "debug.h"

#define SYS_TICK_FREQ 32000000

//=============================================================================
// Defines
//=============================================================================
#define AN_OUT_CMD 1
#define AN_IN_CMD  0


//=============================================================================
// Enums
//=============================================================================

// The state we are in before generating a USB reset.
typedef enum {SI_EMULATE_STATE_RESET = 0x0, 
              SI_EMULATE_STATE_NOTPRESENT,
              SI_EMULATE_STATE_NOTREADY,
              SI_EMULATE_STATE_READY,} SI_EMULATE_STATE;

//=============================================================================
// Structures
//=============================================================================

typedef struct scsi_cmd_handle_s
{
    uint8_t command;
    uint8_t is_cmd_an_OUT;
    error_type_t (*scsi_cmd_func_handler)(CBW_STRUCT *cbw_ptr, scsi_data_t *SCSI_data);

} scsi_cmd_handle_t;

//=============================================================================
// Private data
//=============================================================================

// All of the commands that we handle.
static scsi_cmd_handle_t scsi_cmd_array[] =
{
    {.command               = TEST_UNIT_RDY_CMD, 
     .is_cmd_an_OUT         = AN_OUT_CMD, 
     .scsi_cmd_func_handler = handle_SCSI_write_cmd
    },
    {.command               = MODE_SELECT_6_CMD, 
     .is_cmd_an_OUT         = AN_OUT_CMD, 
     .scsi_cmd_func_handler = handle_SCSI_write_cmd
    },
    {.command               = WRITE_DATA_10_CMD, 
     .is_cmd_an_OUT         = AN_OUT_CMD, 
     .scsi_cmd_func_handler = handle_SCSI_write_cmd
    },
    {.command               = SEND_KEY_CMD, 
     .is_cmd_an_OUT         = AN_OUT_CMD, 
     .scsi_cmd_func_handler = handle_SCSI_write_cmd
    },
    {.command               = VS_WRITE_CMD, 
     .is_cmd_an_OUT         = AN_OUT_CMD, 
     .scsi_cmd_func_handler = handle_SCSI_write_cmd
    },
    {.command               = READ_FORMAT_CAP_CMD, 
     .is_cmd_an_OUT         = AN_IN_CMD, 
     .scsi_cmd_func_handler = handle_SCSI_read_cmd
    },
    {.command               = REPORT_CUR_MEDIA_CAP_CMD, 
     .is_cmd_an_OUT         = AN_IN_CMD, 
     .scsi_cmd_func_handler = handle_SCSI_read_cmd
    },
    {.command               = READ_DATA_10_CMD, 
     .is_cmd_an_OUT         = AN_IN_CMD, 
     .scsi_cmd_func_handler = handle_SCSI_read_cmd
    },
    {.command               = READ_TOC_CMD, 
     .is_cmd_an_OUT         = AN_IN_CMD, 
     .scsi_cmd_func_handler = handle_SCSI_read_cmd
    },
    {.command               = READ_TRACK_INFO_CMD, 
     .is_cmd_an_OUT         = AN_IN_CMD, 
     .scsi_cmd_func_handler = handle_SCSI_read_cmd
    },
    {.command               = READ_BUFFER_CAP_CMD, 
     .is_cmd_an_OUT         = AN_IN_CMD, 
     .scsi_cmd_func_handler = handle_SCSI_read_cmd
    },
    {.command               = REPORT_KEY_CMD, 
     .is_cmd_an_OUT         = AN_IN_CMD, 
     .scsi_cmd_func_handler = handle_SCSI_read_cmd
    },
    {.command               = GET_PERFORMANCE_CMD, 
     .is_cmd_an_OUT         = AN_IN_CMD, 
     .scsi_cmd_func_handler = handle_SCSI_read_cmd
    },
    {.command               = READ_CD_MSF_CMD, 
     .is_cmd_an_OUT         = AN_IN_CMD, 
     .scsi_cmd_func_handler = handle_SCSI_read_cmd
    },
    {.command               = READ_CD_CMD, 
     .is_cmd_an_OUT         = AN_IN_CMD, 
     .scsi_cmd_func_handler = handle_SCSI_read_cmd
    },
    {.command               = VS_READ_CMD, 
     .is_cmd_an_OUT         = AN_IN_CMD, 
     .scsi_cmd_func_handler = handle_SCSI_read_cmd
    },
    {.command               = REQUEST_SENSE_CMD, 
     .is_cmd_an_OUT         = AN_IN_CMD, 
     .scsi_cmd_func_handler = handle_SCSI_other_cmd
    },
    {.command               = INQUIRY_CMD, 
     .is_cmd_an_OUT         = AN_IN_CMD, 
     .scsi_cmd_func_handler = handle_SCSI_other_cmd
    },
    {.command               = MODE_SENSE_6_CMD, 
     .is_cmd_an_OUT         = AN_IN_CMD, 
     .scsi_cmd_func_handler = handle_SCSI_other_cmd
    },
    {.command               = START_STOP_UNIT_CMD, 
     .is_cmd_an_OUT         = AN_IN_CMD, 
     .scsi_cmd_func_handler = handle_SCSI_other_cmd
    },
    {.command               = PREV_ALLOW_MEDIA_REM_CMD, 
     .is_cmd_an_OUT         = AN_OUT_CMD, 
     .scsi_cmd_func_handler = handle_SCSI_write_cmd
    },
    {.command               = VERIFY_CMD, 
     .is_cmd_an_OUT         = AN_IN_CMD, 
     .scsi_cmd_func_handler = handle_SCSI_other_cmd
    },
    {.command               = SYNC_CACHE_CMD, 
     .is_cmd_an_OUT         = AN_IN_CMD, 
     .scsi_cmd_func_handler = handle_SCSI_other_cmd
    },
//  {.command               = GET_CONFIG_CMD,
//   .is_cmd_an_OUT         = AN_IN_CMD,
//   .scsi_cmd_func_handler = handle_SCSI_other_cmd
//  },
    {.command               = GET_EVENT_STATUS_NOTIF_CMD, 
     .is_cmd_an_OUT         = AN_IN_CMD, 
     .scsi_cmd_func_handler = handle_SCSI_other_cmd
    },
    {.command               = MODE_SENSE_10_CMD, 
     .is_cmd_an_OUT         = AN_IN_CMD, 
     .scsi_cmd_func_handler = handle_SCSI_other_cmd
    },
    {.command               = CLOSE_TRACK_SESSION_CMD, 
     .is_cmd_an_OUT         = AN_IN_CMD, 
     .scsi_cmd_func_handler = handle_SCSI_other_cmd
    },
    {.command               = READ_DISK_STRUCT_CMD, 
     .is_cmd_an_OUT         = AN_IN_CMD, 
     .scsi_cmd_func_handler = handle_SCSI_other_cmd
    },
    {.command               = SET_CD_SPEED_CMD, 
     .is_cmd_an_OUT         = AN_IN_CMD, 
     .scsi_cmd_func_handler = handle_SCSI_other_cmd
    },
    // It seems odd to have these handled by the read module when they are OUTs. 
    // Knowing when reads stop is part of the reset.
    {.command               = VS_REQ_SHOW_PRINT_INTFS_CMD, 
     .is_cmd_an_OUT         = AN_OUT_CMD, 
     .scsi_cmd_func_handler = handle_SCSI_read_cmd
    }, 
    {.command               = VS_REQ_SHOW_ALL_INTFS_CMD, 
     .is_cmd_an_OUT         = AN_OUT_CMD, 
     .scsi_cmd_func_handler = handle_SCSI_read_cmd
    },
    {.command               = VS_REQ_SHOW_DEF_INTF_CMD, 
     .is_cmd_an_OUT         = AN_OUT_CMD, 
     .scsi_cmd_func_handler = handle_SCSI_read_cmd
    }
};

// The number of SCSI commands that we handle.
static uint32_t num_scsi_cmds = sizeof(scsi_cmd_array)/sizeof(scsi_cmd_handle_t);

// Used to check what state we are in before letting a SW generated USB reset occur.
static SI_EMULATE_STATE SI_Emulate_State             = SI_EMULATE_STATE_RESET;
static uint32_t         gMSTicksSetAddr              = 0;
static uint32_t         gMSTicksSetCfg               = 0;
static bool             SI_Emulate_Trigger_ModeSense = false;


//=============================================================================
// Local functions
//=============================================================================
static error_type_t handle_SCSI_unsupported_cmd(CBW_STRUCT *cbw_ptr, 
                                                scsi_data_t *SCSI_data);


void init_SCSI_cmds_for_new_LUN(uint8_t drive_type,
                                uint8_t rem_media,
                                char    *vendor_info,
                                char    *prod_info,
                                char    *rev_level)
{
    init_SCSI_other_cmds_for_new_LUN(drive_type,
                                     rem_media,
                                     vendor_info,
                                     prod_info,
                                     rev_level);

    init_SCSI_read_cmds_for_new_LUN();
}


error_type_t handle_SCSI_cmd(CBW_STRUCT *cbw_ptr, scsi_data_t *SCSI_data)
{
    uint32_t          i;
    error_type_t      status          = FAIL;
//    uint8_t           LUN             = cbw_ptr->bCBWLun;
    uint8_t           current_command = cbw_ptr->CBWCB[0];
    scsi_cmd_handle_t *cur_cmd_handle = NULL;

    for (i = 0; i < num_scsi_cmds; ++i)
    {
        if (scsi_cmd_array[i].command == current_command)
        {
            cur_cmd_handle = &scsi_cmd_array[i];
            break;
        }
    }
    
    if (cur_cmd_handle == NULL)
    {
        status = handle_SCSI_unsupported_cmd(cbw_ptr, SCSI_data);
    }
    else
    {

/* Haitao */
/*bob 10/11/2012*/
        {
#if defined(HAVE_NAND)
            // Check to see if we need to do a USB reset.
            do_USB_reset_if_needed(cbw_ptr);
#endif

            // Perform the command.
            status = cur_cmd_handle->scsi_cmd_func_handler(cbw_ptr, SCSI_data);
        }
    }

    return(status);
}

bool is_cmd_an_OUT_cmd(uint8_t cur_command, CBW_STRUCT *cbw_ptr)
{
    uint32_t i;
    bool     cmd_is_an_OUT = false;
    bool     found_cmd     = false;

    // Here, we want to look at the list of commands we support to determine whether
    // the command is an IN/OUT. In another part of the code we will look at the CBW
    // and see if the IN/OUT matches. The MSC likes to test things like this.
    for (i = 0; i < num_scsi_cmds; ++i)
    {
        if (scsi_cmd_array[i].command == cur_command)
        {
            found_cmd = true;

            if (scsi_cmd_array[i].is_cmd_an_OUT)
            {
                cmd_is_an_OUT = true;
            }

            break;
        }
    }

    // We do not handle this command so we need to know if it is an IN/OUT to know
    // whether to expect write data that needs to be dumped.
    if (!found_cmd)
    {
        dbg_printf("%s - the cmd: %#x was not found in list.\n", __func__, 
                   cur_command);
        
        if(OUT_CMD_SENT(cbw_ptr))
        {
            cmd_is_an_OUT = true;
        }
    }

    return(cmd_is_an_OUT);
}

void notify_MS_SetAddr(void)
{
    SI_Emulate_State             = SI_EMULATE_STATE_RESET;
    SI_Emulate_Trigger_ModeSense = false;
    gMSTicksSetAddr              = os_ticks_get();
}

void notify_MS_SetCfg(void)
{
    gMSTicksSetCfg = os_ticks_get();
    
    // This is an indicator that PnP took awhile, new drivers loading
    if ((os_ticks_get() - gMSTicksSetAddr) > (SYS_TICK_FREQ * 2))
    {
        dbg_printf("notify_MS_SetCfg = new drivers\n");
    }
}

bool emulationReady(uint8_t LUN)
{
    // Set the sense data for when the host asks what the problem was.
    switch (SI_Emulate_State)
    {
    case SI_EMULATE_STATE_RESET:
        if ((SI_Emulate_Trigger_ModeSense) || 
            ((os_ticks_get() - gMSTicksSetCfg) > (SYS_TICK_FREQ)))
        {
            set_request_sense_data(LUN,
                                   UNIT_ATTENTION,
                                   POWER_ON_RESET_ASC,
                                   POWER_ON_RESET_ASCQ);

            SI_Emulate_State = SI_EMULATE_STATE_NOTPRESENT;
            //dbg_printf("%s - emulation in reset going to not present state.\n", 
            //            __func__);
        }
        else
        {
            set_request_sense_data(LUN,
                                   UNIT_ATTENTION,
                                   NOT_READY_X_MEDIA_CHANGED_ASC,
                                   NOT_READY_X_MEDIA_CHANGED_ASCQ);

            SI_Emulate_State = SI_EMULATE_STATE_RESET;
            //dbg_printf("%s - emulation in in reset going to reset state.\n", 
            //           __func__);
        }
        break;

    case SI_EMULATE_STATE_NOTPRESENT:

        set_request_sense_data(LUN,
                               UNIT_NOT_READY,
                               MEDIUM_NOT_PRESENT_ASC,
                               MEDIUM_NOT_PRESENT_ASCQ);

        SI_Emulate_State = SI_EMULATE_STATE_NOTREADY;
        //dbg_printf("%s - emulation in in not present going to not ready state.\n", 
        //           __func__);
        break;

    case SI_EMULATE_STATE_NOTREADY:

        set_request_sense_data(LUN,
                               UNIT_ATTENTION,
                               NOT_READY_X_MEDIA_CHANGED_ASC,
                               NOT_READY_X_MEDIA_CHANGED_ASCQ);

        SI_Emulate_State = SI_EMULATE_STATE_READY;
        //dbg_printf("%s - emulation in not ready going to ready state.\n", __func__);
        break;

    case SI_EMULATE_STATE_READY:
        //dbg_printf("%s - emulation in ready state and staying there.\n", __func__);
        return true;
        break;
    }

    return false;

} // end emulationReady

uint32_t get_ret_buffer_size(uint32_t   num_bytes_needed,
                             CBW_STRUCT *cbw_ptr,
                             uint32_t   *send_stall_ptr)
{
    uint32_t num_bytes_to_alloc;
    uint32_t data_len_swapped = le32_to_cpu(cbw_ptr->dCBWDataLength);
    *send_stall_ptr = false;

    // This is the main good case, the number of bytes to return matches what should
    // be returned for the specified command.
    if (data_len_swapped == num_bytes_needed)
    {
        num_bytes_to_alloc = num_bytes_needed;
    }
    else
    {
        // The host wants us to return less data than what should be returned for
        // the specified command. Return the data length requested.
        if (data_len_swapped < num_bytes_needed)
        {
            num_bytes_to_alloc = data_len_swapped;
        }
        // The host wants us to return more data than should be returned for the specified
        // command. What we will do in this case is return what we have and then then
        // stall on the EP. This lets the host know what is going on.
        else
        {
            //printf("data_len_swapped = 0x%x, num_bytes_needed = 0x%x, 0x%x\n",data_len_swapped, num_bytes_needed,cbw_ptr->dCBWDataLength );
            num_bytes_to_alloc = num_bytes_needed;

            /* Haitao */
            //*send_stall_ptr = true;
        }
    }
    
    return (num_bytes_to_alloc);

} // end get_buffer_for_ret_data

void set_returned_data(CBW_STRUCT  *cbw_ptr, 
                       scsi_data_t *SCSI_data,
                       char        *src_data, 
                       uint32_t    num_bytes_needed)
{
	SCSI_data->num_bytes_read = get_ret_buffer_size(num_bytes_needed,
                                                    cbw_ptr,
                                                    &(SCSI_data->send_stall));

    SCSI_data->read_data_ptr = USB_memalloc_aligned(SCSI_data->num_bytes_read, 32);
    ASSERT(SCSI_data->read_data_ptr);

    memcpy(SCSI_data->read_data_ptr, src_data, SCSI_data->num_bytes_read);
}

uint32_t get_last_log_blk_adrs(uint8_t LUN)
{
    return(media_manager_scsi_get_total_num_sectors(LUN) - 1);
}

void convert_LBA_to_MSF_format(uint32_t LBA_value,
                               uint8_t  *LBA_as_MSF_ptr)
{
    uint8_t minutes;
    uint8_t seconds;
    uint8_t frames;

    minutes = (uint8_t)((LBA_value + 150) / (60 * 75));

    seconds = (uint8_t)((LBA_value + 150 - (minutes * 60 * 75)) / 75);
    
    frames  = (uint8_t)(LBA_value + 150 - (minutes * 60 * 75) - (seconds * 75));

    LBA_as_MSF_ptr[0] = minutes;
    LBA_as_MSF_ptr[1] = seconds;
    LBA_as_MSF_ptr[2] = frames;

}

bool check_acs_out_of_range(uint8_t LUN, uint32_t start_adrs, uint32_t len)
{
    bool     acs_out_of_range = false;
    uint64_t ending_adrs;
    uint64_t part_size_in_bytes;
    uint32_t sector_size;

    sector_size = media_manager_scsi_get_sector_size(LUN);
    
    ending_adrs = (start_adrs + len) * sector_size;

    part_size_in_bytes = media_manager_scsi_get_total_num_sectors(LUN) * sector_size;

    if (ending_adrs > part_size_in_bytes)
    {
        acs_out_of_range = true;

        set_request_sense_data(LUN,
                               ILLEGAL_REQUEST,
                               LBA_OUT_OF_RANGE_ASC,
                               LBA_OUT_OF_RANGE_ASCQ);
    }

    return(acs_out_of_range);
}

//=============================================================================
// Local function definitions
//=============================================================================

//
// Handle the unsupported command gracefully.
//
error_type_t handle_SCSI_unsupported_cmd(CBW_STRUCT *cbw_ptr, scsi_data_t *SCSI_data)
{
    dbg_printf("ERROR %s - unsupported command: %#x.\n", __func__, 
               cbw_ptr->CBWCB[0]);
    return(FAIL);
}

