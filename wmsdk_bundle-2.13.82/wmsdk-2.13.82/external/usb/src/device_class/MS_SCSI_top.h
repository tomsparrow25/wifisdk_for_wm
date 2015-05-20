/* 
 *
 * ============================================================================
 * Copyright (c) 2008-2011   Marvell Semiconductor, Inc. All Rights Reserved
 *
 *                         Marvell Confidential
 * ============================================================================
 *
 */

#ifndef MS_SCSI_TOP_H
#define MS_SCSI_TOP_H

//=============================================================================
// Includes
//=============================================================================
#include <stdint.h>
#include <stdbool.h>

//=============================================================================
// Defines
//=============================================================================

// SCSI Commands
#define TEST_UNIT_RDY_CMD           0x00 
#define REQUEST_SENSE_CMD           0x03 
#define INQUIRY_CMD                 0x12 
#define MODE_SELECT_6_CMD           0x15 
#define MODE_SENSE_6_CMD            0x1A 
#define START_STOP_UNIT_CMD         0x1B 
#define PREV_ALLOW_MEDIA_REM_CMD    0x1E 
#define READ_FORMAT_CAP_CMD         0x23 
#define REPORT_CUR_MEDIA_CAP_CMD    0x25 
#define READ_DATA_10_CMD            0x28 
#define WRITE_DATA_10_CMD           0x2A 
#define VERIFY_CMD                  0x2F 
#define SYNC_CACHE_CMD              0x35 
#define READ_TOC_CMD                0x43 
#define GET_CONFIG_CMD              0x46 
#define GET_EVENT_STATUS_NOTIF_CMD  0x4A 
#define READ_TRACK_INFO_CMD         0x52 
#define MODE_SENSE_10_CMD           0x5A 
#define CLOSE_TRACK_SESSION_CMD     0x5B 
#define READ_BUFFER_CAP_CMD         0x5C 
#define SEND_KEY_CMD                0xA3 
#define REPORT_KEY_CMD              0xA4 
#define GET_PERFORMANCE_CMD         0xAC 
#define READ_DISK_STRUCT_CMD        0xAD 
#define READ_CD_MSF_CMD             0xB9 
#define SET_CD_SPEED_CMD            0xBB 
#define READ_CD_CMD                 0xBE 
#define VS_REQ_SHOW_PRINT_INTFS_CMD 0xD0 
#define VS_REQ_SHOW_ALL_INTFS_CMD   0xD1 
#define VS_REQ_SHOW_DEF_INTF_CMD    0xD2
#define VS_WRITE_CMD                0xE0 
#define VS_READ_CMD                 0xE1 

// The maximum number of logical unit numbers allowed. This is defined in section
// 3.2 of the USB Mass Storage Class Bulk-Only Transport specification.
#define MAX_LUN_NUM 15
/*bob 09/12/2012*/
/*3KB bytes for SD card*/
#define MSC_MAX_RW_BUFFER_SIZE           6 * 512

//=============================================================================
// Structures
//=============================================================================

/*bob 09/12/2012*/
typedef struct scsi_data_s
{
    uint8_t  *read_data_ptr;
    uint32_t num_bytes_read;
    uint32_t num_exp_rd_bytes;
    uint32_t num_rd_sectors;//number of sectors have read
    uint32_t num_wrt_bytes;
    uint32_t num_exp_wrt_bytes;
	uint32_t num_wrt_sectors;//number of sectors have written 
    uint8_t  *wrt_data_ptr;
    uint32_t send_stall;
    uint32_t command_status;
    
} scsi_data_t;

//=============================================================================
// Function declarations
//=============================================================================

/**
 * \brief When a new SCSI device is registerd, initialize the data structures 
 *        for the device.
 *  
 * \retval N/A 
 */
void init_SCSI_cmds_for_new_LUN(uint8_t drive_type, uint8_t rem_media, char *vendor_info,
                                char *prod_info, char *rev_level);
/**
 * \brief These are the read commands that access the media that 
 *        need partial intialization.
 *  
 *  \retval N/A 
 */
void init_SCSI_read_cmds_for_new_LUN();

/**
 * \brief These are the rest of the SCSI commands that need 
 *        initialization.
 *  
 * \retval N/A  
 */
void init_SCSI_other_cmds_for_new_LUN(uint8_t drive_type, uint8_t rem_media, 
                                      char *vendor_info, char *prod_info, 
                                      char *rev_level);

/**
 * \brief Top level function to decide how to handle a SCSI 
 *        command.
 *  
 * \retval OK or FAIL. 
 */
error_type_t handle_SCSI_cmd(CBW_STRUCT *cbw_ptr, scsi_data_t *SCSI_data);

/**
 * \brief Handle either an OUT command and/or one that will 
 *        write to the media.
 *  
 * \retval OK or FAIL. 
 */
error_type_t handle_SCSI_write_cmd(CBW_STRUCT *cbw_ptr, scsi_data_t *SCSI_data);

/**
 * \brief Handle a command that needs to access the media either 
 *        for data or media specific information.
 *  
 * \retval OK or FAIL. 
 */
error_type_t handle_SCSI_read_cmd(CBW_STRUCT *cbw_ptr, scsi_data_t *SCSI_data);

/**
 * \brief Handle the commands that do not access the media. 
 * 
 * \retval OK or FAIL. 
 */
error_type_t handle_SCSI_other_cmd(CBW_STRUCT *cbw_ptr, scsi_data_t *SCSI_data);

/**
 * \brief Determine the amount of data to return depending upon 
 *        how much is available and how much is requested. Copy
 *        it to the buffer that will be returned.
 * 
 * \retval N/A  
 */
void set_returned_data(CBW_STRUCT *cbw_ptr, scsi_data_t *SCSI_data, char *src_data, 
                       uint32_t num_bytes_needed);

/**
 * \brief Return the last logical block address from a specific 
 *        device.
 *  
 * \retval the last logical block address. 
 */
uint32_t get_last_log_blk_adrs(uint8_t LUN);

/**
 * \brief Check whether an access is out of range. 
 *  
 *  
 * \retval true if in range, false if not. 
 */
bool check_acs_out_of_range(uint8_t LUN, uint32_t start_adrs, uint32_t len);

/**
 * \brief Convert a logical block address (LBA) format to minute 
 *        seconds frames (MSF) format.
 * 
 * \retval N/A  
 */
void convert_LBA_to_MSF_format(uint32_t LBA_value, uint8_t  *LBA_as_MSF_ptr);

/**
 * \brief Determine the number of bytes we need to allocate for 
 *        the data we are to return for a specified command.
 * 
 * \retval buffer size needed. 
 */

uint32_t get_ret_buffer_size(uint32_t num_bytes_needed, CBW_STRUCT *cbw_ptr,
                            uint32_t *send_stall_ptr);


/**
 * \brief Determine if a command is supposed to be an OUT 
 *        command or not.
 * 
 * \retval true if command is an OUT, false if not.
 */
bool is_cmd_an_OUT_cmd(uint8_t cur_command, CBW_STRUCT *cbw_ptr);

/**
 * \brief We are emulating a Thumb Drive with a CDROM partition. 
 *        For compatibility it fakes that it is not ready
 *        immediately.
 *  
 * \retval N/A  
 */
bool emulationReady(uint8_t LUN);

#if defined(HAVE_NAND)
/**
 * \brief Reset the variables that control how we go about doing a 
 *        of the USB bus.
 *  
 * \retval N/A  
 */
void init_bus_reset_state();
#endif

/**
 * \brief Make all read-only devices writable. 
 *  
 * \retval N/A  
 */
void make_RO_devices_writable();

/**
 * \brief Return all read-only devices to their default state. 
 *  
 * \retval N/A  
 */
void ret_RO_devices_to_default();

/**
 *
 * \brief Return the device type given a LUN. 
 *  
 * \retval the type of LUN, disk, CD-ROM, etc.
 *  
 */
uint8_t get_device_type_of_LUN(uint8_t LUN);

/**
 *
 * \brief determine if a USB reset has been scheduled.
 *  
 * \retval N/A
 *  
 */
void do_USB_reset_if_needed(CBW_STRUCT *cbw_ptr);

#endif // MS_SCSI_TOP_H
