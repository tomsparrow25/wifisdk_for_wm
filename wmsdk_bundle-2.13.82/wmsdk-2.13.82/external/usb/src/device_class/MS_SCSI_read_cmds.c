// /////////////////////////////////////////////////////////////////////////////
// 
//  ============================================================================
//  Copyright (c) 2011   Marvell Semiconductor, Inc. All Rights Reserved
//                       
//                          Marvell Confidential
//  ============================================================================
//
//
// Description: this module is tasked with accessing the media and preparing the
//              information to return with the particular SCSI command that is 
//              called.


//=============================================================================
// Includes
//=============================================================================
#include <stdint.h>
#include <string.h>
#include <stdio.h>

#include <incl/usb_port.h>
#include <incl/devapi.h>
#include <incl/usb_device_api.h>
#include <incl/usbdrv.h>
#include <incl/usbprvdev.h>
#include <wm_os.h>
#include <wmstdio.h>


#include "io.h"
#include "ATypes.h"
#include "error_types.h"
#include <wmassert.h>
#include "MS_SCSI_cmds.h"
#include "MS_SCSI_top.h"
#include "debug.h"
#include "usb_device_api.h"
#include "media_sd.h"

#if defined(HAVE_NAND)
#include "fs_init.h"
#endif

//=============================================================================
// Defines
//=============================================================================
// Used in the read CD command.
#define SYNC_PATTERN_LEN 12
#define LBA_AS_MSF_LEN    3

// The minimum and maximum number of reads to determine when we can initiate
// a SW USB bus reset.
#if defined(HAVE_NAND)
#define SI_MIN  4
#define SI_MAX  50
#endif

//=============================================================================
// Enums
//=============================================================================
#if defined(HAVE_NAND)
typedef enum {SI_STATE_EARLY    = 0x0, 
              SI_STATE_CRITICAL = 0x1,
              SI_STATE_LATE     = 0x2,
              SI_STATE_PEND     = 0x3,} SI_RESET_STATE;
#endif

//=============================================================================
// Private data
//=============================================================================

// The current LUN index.
static uint32_t LUN_index = 0;

// Arrays of pointers to structures that contain data that we sent back on
// various commands that this module handles.
static read_format_capacities_t *rd_format_cap_array[MAX_LUN_NUM + 1]    = {NULL};
static read_toc_response_t      *read_toc_resp_array[MAX_LUN_NUM + 1]    = {NULL};
static read_track_info_t        *read_track_info_array[MAX_LUN_NUM + 1]  = {NULL};
static buffer_capacity_t        *buffer_capacity_array[MAX_LUN_NUM + 1]  = {NULL};
static get_performance_data_t   *get_perf_data_array[MAX_LUN_NUM + 1]    = {NULL};
static get_perf_write_speed_t   *get_perf_wrt_spd_array[MAX_LUN_NUM + 1] = {NULL};
static report_media_cap_t       *report_media_cap_array[MAX_LUN_NUM + 1] = {NULL};

static uint8_t read_CD_preload_data[SYNC_PATTERN_LEN + LBA_AS_MSF_LEN + 1] =
{
    0x0,  0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x0,
    0x0,  0x0,  0x0,  0x0
};

static uint8_t read_CD_MSF_preload_data[SYNC_PATTERN_LEN + LBA_AS_MSF_LEN + 1] =
{
    0x0,  0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x0,
    0x0,  0x0,  0x0,  0x2
};


static uint32_t read_CD_preload_data_len     = sizeof(read_CD_preload_data);
static uint32_t read_CD_MSF_preload_data_len = sizeof(read_CD_MSF_preload_data);

// Used to determine when we can drop off/on the bus to simulate a USB reset.
#if defined(HAVE_NAND)
static SI_RESET_STATE SI_Reset_State      = SI_STATE_EARLY;
static uint32_t       SI_Reads            = 0;
static uint32_t       SI_Reset_State_Time = 0; 
#endif

//=============================================================================
// Local function declarations
//=============================================================================
static void init_read_format_cap_info();
static void init_read_toc_response_info();
static void init_read_track_info();
static void init_buffer_capacity_info();
static void init_get_performance_data_info();
static void init_get_perf_write_speed_info();
static void set_read_track_cmd_data(uint8_t LUN, CBW_STRUCT *cbw_ptr, 
                                    uint32_t sector_size, uint32_t tot_sectors);
static void set_buffer_capacity_data(uint8_t LUN, CBW_STRUCT *cbw_ptr, 
                                     uint32_t sector_size);
static void set_perf_data_desc_in_perf_cmd(uint8_t LUN, CBW_STRUCT *cbw_ptr, 
                                           uint32_t last_LBA, uint32_t *ret_data_len);
static void set_write_speed_desc_in_perf_cmd(uint8_t LUN, CBW_STRUCT *cbw_ptr, 
                                             uint32_t last_LBA, uint32_t *ret_data_len);
static void set_read_TOC_cmd_data(uint8_t LUN, CBW_STRUCT *cbw_ptr);
static error_type_t handle_scsi_sector_read(uint8_t LUN, CBW_STRUCT *cbw_ptr,
                                            uint32_t sector_adrs, uint32_t num_sectors,
                                            uint8_t *preload_data,
                                            uint32_t preload_data_len,
                                            scsi_data_t *SCSI_data);
static void init_report_media_cap_info();

#if defined(HAVE_NAND)
static void handle_reset_if_needed(CBW_STRUCT *cbw_ptr);
static void handle_SI_Reset(uint32_t given_CSW_tag);
#endif



void init_SCSI_read_cmds_for_new_LUN()
{
    init_read_format_cap_info();
    init_read_toc_response_info();
    init_read_track_info();
    init_buffer_capacity_info();
    init_get_performance_data_info();
    init_get_perf_write_speed_info();
    init_report_media_cap_info();

    // Only increment the LUN index here.
    ++LUN_index;
}

error_type_t handle_SCSI_read_cmd(CBW_STRUCT *cbw_ptr, scsi_data_t *SCSI_data)
{
    
    uint32_t   sector_size;
    uint32_t   tot_sectors;
    uint32_t   last_LBA_value;
    uint32_t   sec_start_adrs;
    bool       adrs_out_of_range;
    uint8_t    cur_command      = cbw_ptr->CBWCB[0];
    uint8_t    LUN              = cbw_ptr->bCBWLun;
    uint32_t   num_bytes_in_cmd = 0;    // Keep as zero, value returned if failure.
    uint8_t    *cmd_data        = NULL; // Keep NULL, value returned if failure.
    error_type_t status         = OK;

    switch (cur_command)
    {
    case READ_FORMAT_CAP_CMD:
        {           
/*bob 10/11/2012*/
           num_bytes_in_cmd = sizeof(read_format_capacities_t);
            cmd_data         = (uint8_t *)rd_format_cap_array[LUN];


        }
        break;

    case READ_TOC_CMD:
        {
            set_read_TOC_cmd_data(LUN, cbw_ptr);

            num_bytes_in_cmd = sizeof(read_toc_response_t);
            cmd_data         = (uint8_t *)read_toc_resp_array[LUN];
        }
        break;

    case READ_TRACK_INFO_CMD:
        {
            sector_size = media_manager_scsi_get_sector_size(LUN);
            tot_sectors = media_manager_scsi_get_total_num_sectors(LUN);

            set_read_track_cmd_data(LUN, cbw_ptr, sector_size, tot_sectors);

            num_bytes_in_cmd = sizeof(read_track_info_t);
            cmd_data         = (uint8_t *)read_track_info_array[LUN];
        }
        break;

    case READ_BUFFER_CAP_CMD:
        {
            sector_size = media_manager_scsi_get_sector_size(LUN);

            set_buffer_capacity_data(LUN, cbw_ptr, sector_size);

            num_bytes_in_cmd = sizeof(buffer_capacity_t);
            cmd_data         = (uint8_t *)buffer_capacity_array[LUN];
        }
        break;

    case REPORT_CUR_MEDIA_CAP_CMD:
        {
            // If this is a CD-ROM check to see if we are ready to be accessed. This
            // emulates what a real CD-ROM does.
            if (get_device_type_of_LUN(LUN) == CD_ROM_TYPE)
            {
                if (!emulationReady(LUN))
                {
                    status = FAIL;
                    break;
                }
            }

            last_LBA_value = get_last_log_blk_adrs(LUN);
            sector_size    = media_manager_scsi_get_sector_size(LUN);

            report_media_cap_array[LUN]->last_log_blk_adrs[0] = 
                                                      (last_LBA_value & 0xFF000000) >> 24;
            report_media_cap_array[LUN]->last_log_blk_adrs[1] = 
                                                      (last_LBA_value & 0x00FF0000) >> 16;
            report_media_cap_array[LUN]->last_log_blk_adrs[2] = 
                                                      (last_LBA_value & 0x0000FF00) >> 8;
            report_media_cap_array[LUN]->last_log_blk_adrs[3] =  
                                                       last_LBA_value & 0x000000FF;

            report_media_cap_array[LUN]->blk_len_in_bytes[0] = 
                                                         (sector_size & 0xFF000000) >> 24;
            report_media_cap_array[LUN]->blk_len_in_bytes[1] = 
                                                         (sector_size & 0x00FF0000) >> 16;
            report_media_cap_array[LUN]->blk_len_in_bytes[2] = 
                                                         (sector_size & 0x0000FF00) >> 8;
            report_media_cap_array[LUN]->blk_len_in_bytes[3] =  
                                                          sector_size & 0x000000FF;

            num_bytes_in_cmd = sizeof(report_media_cap_t);
            cmd_data         = (uint8_t *)report_media_cap_array[LUN];
        }
        break;

    case GET_PERFORMANCE_CMD:
        {
            last_LBA_value = get_last_log_blk_adrs(LUN);

            // Check the type of get performance command we are to return. We only
            // handle the get performance data and the write speed descriptor.
            if (cbw_ptr->CBWCB[10] == 0)
            {
                set_perf_data_desc_in_perf_cmd(LUN, 
                                               cbw_ptr, 
                                               last_LBA_value, 
                                               &num_bytes_in_cmd);

                num_bytes_in_cmd = sizeof(get_performance_data_t);
                cmd_data         = (uint8_t *)get_perf_data_array[LUN];
            }
            else if (cbw_ptr->CBWCB[10] == 3)
            {
                set_write_speed_desc_in_perf_cmd(LUN, 
                                                 cbw_ptr, 
                                                 last_LBA_value, 
                                                 &num_bytes_in_cmd);

                num_bytes_in_cmd = sizeof(get_perf_write_speed_t);
                cmd_data         = (uint8_t *)get_perf_wrt_spd_array[LUN];
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

    case READ_CD_MSF_CMD:
        {
            uint8_t minutes;
            uint8_t seconds;
            uint8_t frames;
            uint32_t sec_end_adrs;

            minutes = cbw_ptr->CBWCB[3];
            seconds = cbw_ptr->CBWCB[4];
            frames  = cbw_ptr->CBWCB[5];

            sec_start_adrs = (minutes * 60 + seconds) * 75 + frames - 150;

            minutes = cbw_ptr->CBWCB[6];
            seconds = cbw_ptr->CBWCB[7];
            frames  = cbw_ptr->CBWCB[8];
            
            sec_end_adrs = (minutes * 60 + seconds) * 75 + frames - 150;

            tot_sectors = sec_end_adrs - sec_start_adrs;

            // This will be used later to determine what the residue will be.
            SCSI_data->num_exp_rd_bytes = 
            tot_sectors * media_manager_scsi_get_sector_size(LUN);

            adrs_out_of_range = check_acs_out_of_range(LUN, 
                                                       sec_start_adrs, 
                                                       tot_sectors);

            if (!adrs_out_of_range)
            {
                uint32_t data_offset = SYNC_PATTERN_LEN;
                
                read_CD_MSF_preload_data[data_offset++] = minutes;
                read_CD_MSF_preload_data[data_offset++] = seconds;
                read_CD_MSF_preload_data[data_offset++] = frames;

                status = handle_scsi_sector_read(LUN,
                                                 cbw_ptr,
                                                 sec_start_adrs,
                                                 tot_sectors,
                                                 &read_CD_MSF_preload_data[0],
                                                 read_CD_MSF_preload_data_len,
                                                 SCSI_data);
            }
            else
            {
                status = FAIL;
            }

            // We do not need to set the cmd_data or num_bytes_in_cmd as the data
            // has already been placed in the SCSI_data structure.
        }
        break;

    case READ_CD_CMD:
        {
            tot_sectors  = cbw_ptr->CBWCB[6] << 16;
            tot_sectors |= cbw_ptr->CBWCB[7] << 8;
            tot_sectors |= cbw_ptr->CBWCB[8];

            // This will be used later to determine what the residue will be.
            SCSI_data->num_exp_rd_bytes = 
            tot_sectors * media_manager_scsi_get_sector_size(LUN);

            sec_start_adrs =  cbw_ptr->CBWCB[2] << 24;
            sec_start_adrs |= cbw_ptr->CBWCB[3] << 16;       
            sec_start_adrs |= cbw_ptr->CBWCB[4] << 8;
            sec_start_adrs |= cbw_ptr->CBWCB[5];
            
            adrs_out_of_range = check_acs_out_of_range(LUN, 
                                                       sec_start_adrs, 
                                                       tot_sectors);
            if (!adrs_out_of_range)
            {
                uint8_t  LBA_as_MSF[LBA_AS_MSF_LEN];
                uint32_t sec_size = media_manager_scsi_get_sector_size(LUN);

                convert_LBA_to_MSF_format(sec_start_adrs * sec_size,
                                          LBA_as_MSF);

                memcpy(&read_CD_preload_data[SYNC_PATTERN_LEN], 
                       &LBA_as_MSF[0], 
                       LBA_AS_MSF_LEN);

                status = handle_scsi_sector_read(LUN,
                                                 cbw_ptr,
                                                 sec_start_adrs,
                                                 tot_sectors,
                                                 &read_CD_preload_data[0],
                                                 read_CD_preload_data_len,
                                                 SCSI_data);
            }
            else
            {
                status = FAIL;
            }

            // We do not need to set the cmd_data or num_bytes_in_cmd as the data
            // has already been placed in the SCSI_data structure.
        }
        break;

    case READ_DATA_10_CMD:
        {
#if defined(HAVE_NAND)
            handle_reset_if_needed(cbw_ptr);
#endif
              
            tot_sectors =  cbw_ptr->CBWCB[7] << 8;
            tot_sectors |= cbw_ptr->CBWCB[8];
            
            // This will be used later to determine what the residue will be.
            SCSI_data->num_exp_rd_bytes = 
            tot_sectors * media_manager_scsi_get_sector_size(LUN);
                        
            sec_start_adrs =  cbw_ptr->CBWCB[2] << 24;
            sec_start_adrs |= cbw_ptr->CBWCB[3] << 16;       
            sec_start_adrs |= cbw_ptr->CBWCB[4] << 8;
            sec_start_adrs |= cbw_ptr->CBWCB[5];

            adrs_out_of_range = check_acs_out_of_range(LUN, 
                                                       sec_start_adrs, 
                                                       tot_sectors);
            if (!adrs_out_of_range)
            {
                status = handle_scsi_sector_read(LUN,
                                                 cbw_ptr,
                                                 sec_start_adrs,
                                                 tot_sectors,
                                                 NULL,
                                                 0,
                                                 SCSI_data);
            }
            else
            {
                status = FAIL;
            }

            // We do not need to set the cmd_data or num_bytes_in_cmd as the data
            // has already been placed in the SCSI_data structure.
        }
        break;

#if defined(HAVE_NAND)
    case VS_READ_CMD:
        ASSERT(0);
        break;
#endif

    // Vendor specific. Drop us off/on bus, come back with different interfaces.
#if defined(HAVE_NAND)

    case VS_REQ_SHOW_PRINT_INTFS_CMD:
    case VS_REQ_SHOW_ALL_INTFS_CMD:
    case VS_REQ_SHOW_DEF_INTF_CMD:
        {
            if(SI_Reset_State == SI_STATE_CRITICAL)
            {
                SI_Reset_State_Time = tx_time_get();
                dbg_printf("%s - (0x%x) PENDING Bus Reset command %d @ %ld\n",
                           __func__, cur_command, SI_Reads, SI_Reset_State_Time);
                SI_Reset_State = SI_STATE_PEND;
            } 
            else 
            {
                dbg_printf("%s - (0x%x) Bus Reset command. %d\n",__func__, 
                           cur_command, SI_Reads);
                set_request_for_USB_reset(cbw_ptr->dCBWTag);
            }

            if (cur_command == VS_REQ_SHOW_ALL_INTFS_CMD)
            {
                // Make all the read-only devices writable.
                make_RO_devices_writable();

                // This will set the flag that tells the USB driver to return all
                // interfaces on the next USB reset.
                set_ret_all_intfs_flag();
                
                // Get a lock on the file system for the volumes. This will keep
                // the POSIX layer from fighting with sector access layer and
                // causing errors during writes.
                acquire_FS_access_lock();
            }
            else if (cur_command == VS_REQ_SHOW_DEF_INTF_CMD)
            {
                // Set any read-only devices back.
                ret_RO_devices_to_default();

                // Tell the USB driver to return the default interface(s) on the
                // next USB reset.
                reset_intf_flags_to_default();

                // Release the lock on the file system.
                release_FS_access_lock();
            }
            break;
        }

        // There is no data to return for these commands.

        break;

#endif // if defined(HAVE_NAND)

    default:
        dbg_printf("%s - unknown command: %#x\n", __func__, cur_command);
        ASSERT(0);
        break;
    }


    // Copy the structure data to something we can return.
    if (num_bytes_in_cmd > 0 && status == OK)
    {
        set_returned_data(cbw_ptr, SCSI_data, (char *)cmd_data, num_bytes_in_cmd);

        // For particular commands that access the media but do not have lengths
        // specified in the SCSI command, set the expected bytes to the bytes read.
        // This will effectively make the residue zero.
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

#if defined(HAVE_NAND)
void do_USB_reset_if_needed(CBW_STRUCT *cbw_ptr)
{
    if (SI_Reset_State_Time && ((tx_time_get()-SI_Reset_State_Time) > (2*SYS_TICK_FREQ)))
    {
        handle_SI_Reset(cbw_ptr->dCBWTag);
    }
}
#endif

//=============================================================================
// Local functions
//=============================================================================

void init_read_format_cap_info()
{
    rd_format_cap_array[LUN_index] = USB_memcalloc(sizeof(read_format_capacities_t), 1);
    ASSERT(rd_format_cap_array[LUN_index]);
/*bob 10/11/2012*/
  uint8_t BulkBuf[12];
  uint32_t i;
  uint32_t   sector_size;
  uint32_t   tot_sectors;

   sector_size = media_manager_scsi_get_sector_size(LUN_index);
   tot_sectors = media_manager_scsi_get_total_num_sectors(LUN_index);
            
  BulkBuf[ 0] = 0x00;
  BulkBuf[ 1] = 0x00;
  BulkBuf[ 2] = 0x00;
  BulkBuf[ 3] = 0x08;          /* Capacity List Length */

  /* Block Count */
  BulkBuf[ 4] = (tot_sectors >> 24) & 0xFF;
  BulkBuf[ 5] = (tot_sectors >> 16) & 0xFF;
  BulkBuf[ 6] = (tot_sectors >>  8) & 0xFF;
  BulkBuf[ 7] = (tot_sectors >>  0) & 0xFF;

  /* Block Length */
  BulkBuf[ 8] = 0x02;          /* Descriptor Code: Formatted Media */
  BulkBuf[ 9] = (sector_size >> 16) & 0xFF;
  BulkBuf[10] = (sector_size >>  8) & 0xFF;
  BulkBuf[11] = (sector_size >>  0) & 0xFF;

  for(i = 0; i < 12; i++)
  {
    *(((uint8_t *)rd_format_cap_array[LUN_index])+ i) = BulkBuf[i];
  }

    // The rest of the values will be set as the READ_FORMAT_CAP_CMD is handled.
}

void init_read_toc_response_info()
{
    read_toc_resp_array[LUN_index] = USB_memcalloc(sizeof(read_toc_response_t), 1);
    ASSERT(read_toc_resp_array[LUN_index]);

    read_toc_resp_array[LUN_index]->first_track_num = 0x1;
    read_toc_resp_array[LUN_index]->last_track_num  = 0x1;

    read_toc_resp_array[LUN_index]->TOC_track_desc_one.adr_control = 0x14;
    read_toc_resp_array[LUN_index]->TOC_track_desc_two.adr_control = 0x14;

    read_toc_resp_array[LUN_index]->TOC_track_desc_one.track_num = 0x1;
    read_toc_resp_array[LUN_index]->TOC_track_desc_two.track_num = 0xAA;

    // The rest of the values will be set when the READ_TOC_CMD is handled.
}

void init_read_track_info()
{
    read_track_info_array[LUN_index] = USB_memcalloc(sizeof(read_track_info_t), 1);
    ASSERT(read_track_info_array[LUN_index]);

    read_track_info_array[LUN_index]->track_num_LSB                = 0x1;
    read_track_info_array[LUN_index]->session_num_LSB              = 0x1;
    read_track_info_array[LUN_index]->damage_copy_track_mode       = 0x4;
    read_track_info_array[LUN_index]->RT_blank_pktinc_FP_data_mode = 0x31;
    read_track_info_array[LUN_index]->LRA_V_NWA_V                  = 0x3;
    read_track_info_array[LUN_index]->last_recorded_adrs[3]        = 0x1;

    // The rest of the values will be set when the READ_TRACK_INFO_CMD is handled or
    // else they are set to zero.
}

void init_buffer_capacity_info()
{
    buffer_capacity_array[LUN_index] = USB_memcalloc(sizeof(buffer_capacity_t), 1);
    ASSERT(buffer_capacity_array[LUN_index]);

    // All the values will be set when the READ_BUFFER_CAP_CMD is handled.
}

void init_get_performance_data_info()
{
    get_perf_data_array[LUN_index] = USB_memcalloc(sizeof(get_performance_data_t), 1);
    ASSERT(get_perf_data_array[LUN_index]);
}

void init_get_perf_write_speed_info()
{
    get_perf_wrt_spd_array[LUN_index] = USB_memcalloc(sizeof(get_perf_write_speed_t), 1);
    ASSERT(get_perf_wrt_spd_array[LUN_index]);

    get_perf_wrt_spd_array[LUN_index]->perf_wrt_speed.read_speed[0] = WSD_READ_SPEED_8;
    get_perf_wrt_spd_array[LUN_index]->perf_wrt_speed.read_speed[1] = WSD_READ_SPEED_9;
    get_perf_wrt_spd_array[LUN_index]->perf_wrt_speed.read_speed[2] = WSD_READ_SPEED_10;
    get_perf_wrt_spd_array[LUN_index]->perf_wrt_speed.read_speed[3] = WSD_READ_SPEED_11;

    get_perf_wrt_spd_array[LUN_index]->perf_wrt_speed.write_speed[0] = WSD_WRITE_SPEED_12;
    get_perf_wrt_spd_array[LUN_index]->perf_wrt_speed.write_speed[1] = WSD_WRITE_SPEED_13;
    get_perf_wrt_spd_array[LUN_index]->perf_wrt_speed.write_speed[2] = WSD_WRITE_SPEED_14;
    get_perf_wrt_spd_array[LUN_index]->perf_wrt_speed.write_speed[3] = WSD_WRITE_SPEED_15;
}

void init_report_media_cap_info()
{
    report_media_cap_array[LUN_index] = USB_memcalloc(sizeof(report_media_cap_t), 1);
    ASSERT(report_media_cap_array[LUN_index]);

    // All the values will be set when the REPORT_CUR_MEDIA_CAP_CMD is handled.
}

void set_read_track_cmd_data(uint8_t    LUN, 
                             CBW_STRUCT *cbw_ptr, 
                             uint32_t   sector_size, 
                             uint32_t   tot_sectors)
{
    uint32_t alloc_len;
    uint32_t data_len;
    uint32_t bytes_to_ret = sizeof(read_track_info_t);

    // Get how many bytes to return.
    alloc_len  = cbw_ptr->CBWCB[7] << 8;
    alloc_len |= cbw_ptr->CBWCB[8];

    if (alloc_len < bytes_to_ret)
    {
        bytes_to_ret = alloc_len;
    }

    //read_track_info_array
    // The data length is the bytes to return minus the number of bytes needed
    // to represent the data length.
    data_len = bytes_to_ret - 2;

    read_track_info_array[LUN]->data_len[0] = (data_len << 8) & 0xFF00;
    read_track_info_array[LUN]->data_len[1] = data_len & 0x00FF;

    read_track_info_array[LUN]->free_blocks[0] = (tot_sectors << 24) & 0xFF000000;
    read_track_info_array[LUN]->free_blocks[1] = (tot_sectors << 16) & 0x00FF0000;
    read_track_info_array[LUN]->free_blocks[2] = (tot_sectors << 8)  & 0x0000FF00;
    read_track_info_array[LUN]->free_blocks[3] = (tot_sectors << 0)  & 0x000000FF;


    read_track_info_array[LUN]->fixed_pkt_size[0] = (sector_size << 24) & 0xFF000000;
    read_track_info_array[LUN]->fixed_pkt_size[1] = (sector_size << 16) & 0x00FF0000;
    read_track_info_array[LUN]->fixed_pkt_size[2] = (sector_size << 8)  & 0x0000FF00;
    read_track_info_array[LUN]->fixed_pkt_size[3] = (sector_size << 0)  & 0x000000FF;
}

void set_buffer_capacity_data(uint8_t LUN, CBW_STRUCT *cbw_ptr, uint32_t sector_size)
{
    uint32_t buf_size            = (64 * 1024); // Common value.
    uint32_t num_bytes_in_struct = sizeof(buffer_capacity_t);
    uint32_t page_size           = media_manager_scsi_get_sector_size(LUN);
    uint32_t num_bytes_requested;
    uint32_t num_bytes_to_ret;
    bool     ret_block_info;

    // Check to see if we are to return the data as bytes or blocks.
    ret_block_info = cbw_ptr->CBWCB[1] & 0x1;

    // See how many bytes we have bee requested to return. This is a way of 
    // telling us to return less bytes than the sizeof the struct.
    num_bytes_requested =  cbw_ptr->CBWCB[8];
    num_bytes_requested |= (cbw_ptr->CBWCB[7] << 8);

    // Set the number of bytes to return. If they asked for us to return
    // less bytes, honor the request. Else return the number of bytes in
    // the struct.
    if (num_bytes_requested < num_bytes_in_struct)
    {
        num_bytes_to_ret = num_bytes_requested;
    }
    else
    {
        num_bytes_to_ret = num_bytes_in_struct;
    }

    // If we are to return block information, divide the buffer size down.
    if (ret_block_info)
    {
        if (page_size > 0)
        {
            buf_size /= page_size;
        }
    }

    //buffer_capacity_array
    // Set the buffer length.
    buffer_capacity_array[LUN]->buf_len_in_bytes_or_blocks[0] = 
                                                            (buf_size & 0xff000000) << 24;
    buffer_capacity_array[LUN]->buf_len_in_bytes_or_blocks[1] = 
                                                            (buf_size & 0x00ff0000) << 16;
    buffer_capacity_array[LUN]->buf_len_in_bytes_or_blocks[2] = 
                                                            (buf_size & 0x0000ff00) << 8;
    buffer_capacity_array[LUN]->buf_len_in_bytes_or_blocks[3] = 
                                                            (buf_size & 0x000000ff);

    // If we are to return block infomation, these fields are to be zeroed. If not
    // then in our case they are simply the buffer length again.
    if (ret_block_info)
    {
        buffer_capacity_array[LUN]->buf_len_or_reserved[0] = 0;
        buffer_capacity_array[LUN]->buf_len_or_reserved[1] = 0;
        buffer_capacity_array[LUN]->buf_len_or_reserved[2] = 0;
        buffer_capacity_array[LUN]->buf_len_or_reserved[3] = 0;

        buffer_capacity_array[LUN]->reserved_or_block = 0x1;
    }
    else
    {
        buffer_capacity_array[LUN]->buf_len_or_reserved[0] = 
                                buffer_capacity_array[LUN]->buf_len_in_bytes_or_blocks[0];
        buffer_capacity_array[LUN]->buf_len_or_reserved[1] = 
                                buffer_capacity_array[LUN]->buf_len_in_bytes_or_blocks[1];
        buffer_capacity_array[LUN]->buf_len_or_reserved[2] = 
                                buffer_capacity_array[LUN]->buf_len_in_bytes_or_blocks[2];
        buffer_capacity_array[LUN]->buf_len_or_reserved[3] = 
                                buffer_capacity_array[LUN]->buf_len_in_bytes_or_blocks[3];

        buffer_capacity_array[LUN]->reserved_or_block = 0x0;
    }

    // This field is reserved so set it to zero.
    buffer_capacity_array[LUN]->reserved = 0;

    // The data length is the bytes to return. This length does not however include
    // the length of the data field itself.
    buffer_capacity_array[LUN]->data_length[0] = ((num_bytes_to_ret - 2) & 0xff00) << 8;
    buffer_capacity_array[LUN]->data_length[1] = (num_bytes_to_ret - 2)  & 0x00ff;

}

void set_read_TOC_cmd_data(uint8_t LUN, CBW_STRUCT *cbw_ptr)
{
    uint32_t start_LBA_value  = 0;
    uint32_t last_LBA_value   = get_last_log_blk_adrs(LUN);
    bool     use_MSF_format   = cbw_ptr->CBWCB[1] & 0x2;
    uint32_t num_bytes_to_ret = sizeof(read_toc_response_t) - 2;
    uint8_t  LBA_as_MSF[3];

    if (!use_MSF_format)
    {
        num_bytes_to_ret -= sizeof(toc_track_desc_t); 
    }

    //read_toc_resp_array

    if (use_MSF_format)
    {
        convert_LBA_to_MSF_format(start_LBA_value,
                                  &LBA_as_MSF[0]);

        read_toc_resp_array[LUN]->TOC_track_desc_one.track_start_adrs[0] = 0;
        read_toc_resp_array[LUN]->TOC_track_desc_one.track_start_adrs[1] = LBA_as_MSF[0];
        read_toc_resp_array[LUN]->TOC_track_desc_one.track_start_adrs[2] = LBA_as_MSF[1];
        read_toc_resp_array[LUN]->TOC_track_desc_one.track_start_adrs[3] = LBA_as_MSF[2];

        convert_LBA_to_MSF_format(last_LBA_value,
                                  &LBA_as_MSF[0]);

        read_toc_resp_array[LUN]->TOC_track_desc_two.track_start_adrs[0] = 0;
        read_toc_resp_array[LUN]->TOC_track_desc_two.track_start_adrs[1] = LBA_as_MSF[0];
        read_toc_resp_array[LUN]->TOC_track_desc_two.track_start_adrs[2] = LBA_as_MSF[1];
        read_toc_resp_array[LUN]->TOC_track_desc_two.track_start_adrs[3] = LBA_as_MSF[2];
    }
    else
    {
        read_toc_resp_array[LUN]->TOC_track_desc_one.track_start_adrs[0] = 
                                                     (start_LBA_value & 0xFF000000) >> 24;
        read_toc_resp_array[LUN]->TOC_track_desc_one.track_start_adrs[1] = 
                                                     (start_LBA_value & 0x00FF0000) >> 16;
        read_toc_resp_array[LUN]->TOC_track_desc_one.track_start_adrs[2] = 
                                                     (start_LBA_value & 0x0000FF00) >> 8;
        read_toc_resp_array[LUN]->TOC_track_desc_one.track_start_adrs[3] = 
                                                     (start_LBA_value & 0x000000FF);
    }

    read_toc_resp_array[LUN]->TOC_data_len[0] = (num_bytes_to_ret & 0x0000FF00) >> 8;
    read_toc_resp_array[LUN]->TOC_data_len[1] = (num_bytes_to_ret & 0x000000FF);


}

void set_perf_data_desc_in_perf_cmd(uint8_t    LUN, 
                                    CBW_STRUCT *cbw_ptr, 
                                    uint32_t   last_LBA,
                                    uint32_t   *ret_data_len)
{
    uint32_t num_descriptors   = (cbw_ptr->CBWCB[8] << 16) | cbw_ptr->CBWCB[9]; 
    uint8_t  except_data_type  = cbw_ptr->CBWCB[1] & 0x3;
    uint8_t  except_bit        = cbw_ptr->CBWCB[1] & 0x2;
    uint8_t  write_bit         = cbw_ptr->CBWCB[1] & 0x4;

    // Set these two bits in the returned header.
    get_perf_data_array[LUN]->perf_header.write_except = ((write_bit << 1) | except_bit);

    // Check the number of descriptors. If it is zero, then only return the header.
    // Else we will return the information requested.
    if (num_descriptors > 0)
    {
        *ret_data_len = sizeof(performance_header_t);
        uint32_t header_data_len;

        // See if we are to return the nominal and exception performance data or just 
        // the nominal performance data.
        if (except_data_type == 0 || except_data_type == 1)
        {
            get_perf_data_array[LUN]->nom_perf_data.end_LBA[0] = 
                                                            (last_LBA & 0xFF000000) >> 24;
            get_perf_data_array[LUN]->nom_perf_data.end_LBA[1] = 
                                                            (last_LBA & 0x00FF0000) >> 16;
            get_perf_data_array[LUN]->nom_perf_data.end_LBA[2] = 
                                                            (last_LBA & 0x0000FF00) >> 8;
            get_perf_data_array[LUN]->nom_perf_data.end_LBA[3] =  
                                                             last_LBA & 0x000000FF;

            // Check to see if we are to return the write or read performance data.
            if (write_bit)
            {
                get_perf_data_array[LUN]->nom_perf_data.start_perf[0] = NOM_CD_WRT_SPEED_4;
                get_perf_data_array[LUN]->nom_perf_data.start_perf[1] = NOM_CD_WRT_SPEED_5;
                get_perf_data_array[LUN]->nom_perf_data.start_perf[2] = NOM_CD_WRT_SPEED_6;
                get_perf_data_array[LUN]->nom_perf_data.start_perf[3] = NOM_CD_WRT_SPEED_7;
            }
            else
            {
                get_perf_data_array[LUN]->nom_perf_data.start_perf[0] = NOM_CD_RD_SPEED_4;
                get_perf_data_array[LUN]->nom_perf_data.start_perf[1] = NOM_CD_RD_SPEED_5;
                get_perf_data_array[LUN]->nom_perf_data.start_perf[2] = NOM_CD_RD_SPEED_6;
                get_perf_data_array[LUN]->nom_perf_data.start_perf[3] = NOM_CD_RD_SPEED_7;
            }

            // Our start and ending data is the same.
            get_perf_data_array[LUN]->nom_perf_data.end_perf[0] = 
                                    get_perf_data_array[LUN]->nom_perf_data.start_perf[0];
            get_perf_data_array[LUN]->nom_perf_data.end_perf[1] = 
                                    get_perf_data_array[LUN]->nom_perf_data.start_perf[1];
            get_perf_data_array[LUN]->nom_perf_data.end_perf[2] = 
                                    get_perf_data_array[LUN]->nom_perf_data.start_perf[2];
            get_perf_data_array[LUN]->nom_perf_data.end_perf[3] = 
                                    get_perf_data_array[LUN]->nom_perf_data.start_perf[3];

            *ret_data_len += sizeof(nominal_perf_t);

            // There is nothing to set in the exception data structure. Just increment 
            // the total data returned if we are to return this data.
            if (except_data_type == 1)
            {
                *ret_data_len += sizeof(exception_perf_t);
            }
        }
        // Just returning the exception data.
        else
        {
            *ret_data_len += sizeof(exception_perf_t);
        }

        // The header data length does not include the bytes used to represent that value.
        header_data_len = *ret_data_len - 4;

        get_perf_data_array[LUN]->perf_header.perf_data_len[0] = 
                                                     (header_data_len & 0xFF000000) >> 24;
        get_perf_data_array[LUN]->perf_header.perf_data_len[1] = 
                                                     (header_data_len & 0x00FF0000) >> 16;
        get_perf_data_array[LUN]->perf_header.perf_data_len[2] = 
                                                     (header_data_len & 0x0000FF00) >> 8; 
        get_perf_data_array[LUN]->perf_header.perf_data_len[3] =  
                                                      header_data_len & 0x000000FF;       
    }
    // Just return the header information.
    else
    {
        get_perf_data_array[LUN]->perf_header.perf_data_len[0] = 0;
        get_perf_data_array[LUN]->perf_header.perf_data_len[1] = 0;
        get_perf_data_array[LUN]->perf_header.perf_data_len[2] = 0;
        get_perf_data_array[LUN]->perf_header.perf_data_len[3] = 0x4;
    }
}

void set_write_speed_desc_in_perf_cmd(uint8_t    LUN, 
                                      CBW_STRUCT *cbw_ptr, 
                                      uint32_t   last_LBA,
                                      uint32_t   *ret_data_len)
{
    uint32_t num_descriptors = (cbw_ptr->CBWCB[8] << 16) | cbw_ptr->CBWCB[9]; 
    uint8_t  except_bit      = cbw_ptr->CBWCB[1] & 0x2;
    uint8_t  write_bit       = cbw_ptr->CBWCB[1] & 0x4;

    // Set these two bits in the returned header.
    get_perf_wrt_spd_array[LUN]->perf_header.write_except = ((write_bit << 1) | 
                                                    except_bit);
    // Check the number of descriptors. If it is zero, then only return the header.
    // Else we will return the information requested.
    if (num_descriptors > 0)
    {
        *ret_data_len = sizeof(get_perf_write_speed_t);
        uint32_t header_data_len;

        // This is the only value that we need to set dynamically.
        get_perf_wrt_spd_array[LUN]->perf_wrt_speed.end_LBA[0] = 
                                                            (last_LBA & 0xFF000000) >> 24;
        get_perf_wrt_spd_array[LUN]->perf_wrt_speed.end_LBA[1] = 
                                                            (last_LBA & 0x00FF0000) >> 16;
        get_perf_wrt_spd_array[LUN]->perf_wrt_speed.end_LBA[2] = 
                                                            (last_LBA & 0x0000FF00) >> 8;
        get_perf_wrt_spd_array[LUN]->perf_wrt_speed.end_LBA[3] =  
                                                             last_LBA & 0x000000FF;

        // The header data length does not include the bytes used to represent that value.
        header_data_len = *ret_data_len - 4;

        get_perf_wrt_spd_array[LUN]->perf_header.perf_data_len[0] = 
                                                     (header_data_len & 0xFF000000) >> 24;
        get_perf_wrt_spd_array[LUN]->perf_header.perf_data_len[1] = 
                                                     (header_data_len & 0x00FF0000) >> 16;
        get_perf_wrt_spd_array[LUN]->perf_header.perf_data_len[2] = 
                                                     (header_data_len & 0x0000FF00) >> 8;
        get_perf_wrt_spd_array[LUN]->perf_header.perf_data_len[3] =  
                                                      header_data_len & 0x000000FF;
    }
    // Only returning the header.
    else
    {
        get_perf_wrt_spd_array[LUN]->perf_header.perf_data_len[0] = 0;
        get_perf_wrt_spd_array[LUN]->perf_header.perf_data_len[1] = 0;
        get_perf_wrt_spd_array[LUN]->perf_header.perf_data_len[2] = 0;
        get_perf_wrt_spd_array[LUN]->perf_header.perf_data_len[3] = 
                                                         sizeof(performance_header_t) - 4;

    }
}

/*bob 09/12/2012*/
/****************************************************************************//**
 * @brief  Simply cutting large scale upstream data into small pieces. The 
           size of piece is indicated by MSC_MAX_RW_BUFFER_SIZE.
           
 * @param[in]  LUN:logical unit number
               cbw_ptr: point to a cbw struct
               sector_adrs: start block address
               num_sector: number of blocks to read
               preload_data: point to preload_data
               preload_data_len: length of preload_data
               SCSI_data: SCSI_data structure 

 * @return error type
 *******************************************************************************/

error_type_t handle_scsi_sector_read(uint8_t     LUN, 
                                     CBW_STRUCT  *cbw_ptr,
                                     uint32_t    sector_adrs,
                                     uint32_t    num_sectors,
                                     uint8_t     *preload_data,
                                     uint32_t    preload_data_len,
                                     scsi_data_t *SCSI_data)
{
    error_type_t status        = FAIL;
    uint32_t num_bytes_rd_untransf = cbw_ptr->dCBWDataLength - SCSI_data->num_bytes_read;
    uint32_t data_len_swapped  = le32_to_cpu(num_bytes_rd_untransf);
    uint32_t buffer_size;
    SCSI_data->num_bytes_read = 0;
        
    if (data_len_swapped && num_sectors)
    {
        uint32_t tot_mem_to_alloc;
        uint32_t mem_index    = 0;
        uint32_t sector_size  = media_manager_scsi_get_sector_size(LUN);
        uint32_t len_in_bytes = sector_size * num_sectors;
        
        if (data_len_swapped < len_in_bytes)
        {
            len_in_bytes = data_len_swapped;
        }
        // Determine the number of bytes to return.
        buffer_size = get_ret_buffer_size(len_in_bytes,
                                          cbw_ptr,
                                          &(SCSI_data->send_stall));
        
        tot_mem_to_alloc = buffer_size + preload_data_len;

		

		if (tot_mem_to_alloc > MSC_MAX_RW_BUFFER_SIZE)
		{
          
		   tot_mem_to_alloc = MSC_MAX_RW_BUFFER_SIZE ;
		   
		}
					
        SCSI_data->read_data_ptr = USB_memalloc_aligned(tot_mem_to_alloc, 32);
		
        ASSERT(SCSI_data->read_data_ptr);
        
        if (preload_data)
        {
            memcpy(&SCSI_data->read_data_ptr, preload_data, preload_data_len);

            mem_index += preload_data_len;
        }

        num_sectors = tot_mem_to_alloc/sector_size;

        status = media_manager_scsi_sector_read(LUN, 
                                                sector_adrs + SCSI_data->num_rd_sectors, 
                                                num_sectors, 
                                                &SCSI_data->read_data_ptr[mem_index]);
		SCSI_data->num_bytes_read = tot_mem_to_alloc;
        SCSI_data->num_rd_sectors = SCSI_data->num_rd_sectors + SCSI_data->num_bytes_read / sector_size;
    }

    return(status);
}

// In MS_SCSI_top.h
#if defined(HAVE_NAND)
void init_bus_reset_state()
{
    SI_Reset_State      = SI_STATE_EARLY;
    SI_Reads            = 0;
    SI_Reset_State_Time = 0;
}
#endif

#if defined(HAVE_NAND)
void handle_reset_if_needed(CBW_STRUCT *cbw_ptr)
{
    ++SI_Reads;
    switch (SI_Reset_State)
    {
    case SI_STATE_EARLY:
        if (SI_Reads > SI_MIN) 
        {
            SI_Reset_State = SI_STATE_CRITICAL;
            dbg_printf("SI_Reset_State = ENTER CRITICAL:%d\n",SI_Reads);
        }
        break;
    case SI_STATE_CRITICAL:
        if (SI_Reads > SI_MAX)
        {
            SI_Reset_State = SI_STATE_LATE;
            dbg_printf("SI_Reset_State = EXITING:%d\n",SI_Reads);
        } 
        break;
    case SI_STATE_PEND:
        if (SI_Reads > SI_MAX)
        {
            handle_SI_Reset(cbw_ptr->dCBWTag);
        } 
        break;
    case SI_STATE_LATE:
        break;
    default:
        break;
    }
}
#endif

////////////////////////////////////////////////////////////////////////////////
//
// FUNCTION NAME: handle_SI_Reset
//
// 
// \brief 
// 
// \param 
// 
// \retval none
// 
////////////////////////////////////////////////////////////////////////////////
#if defined(HAVE_NAND)
void handle_SI_Reset(uint32_t given_CSW_tag)
{
    // if we have a PENDED a bus reset, we will reset on conclusion of this CMD.
    SI_Reset_State = SI_STATE_EARLY;
    SI_Reads = 0;
    SI_Reset_State_Time = 0;
    dbg_printf("%s - PENDED Bus Reset command\n", __func__);

    set_request_for_USB_reset(given_CSW_tag);

} // end handle_SI_Reset
#endif
