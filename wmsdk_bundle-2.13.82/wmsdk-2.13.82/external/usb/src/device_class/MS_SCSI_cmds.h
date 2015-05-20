// /////////////////////////////////////////////////////////////////////////////
// $Header:
// 
//  ============================================================================
//  Copyright (c) 2011   Marvell Semiconductor, Inc. All Rights Reserved
//                       
//                          Marvell Confidential
//  ============================================================================
//
//

#include <stdint.h>

#ifndef MS_SCSI_CMDS_H
#define MS_SCSI_CMDS_H

//=============================================================================
// Includes
//=============================================================================

// The number of bytes that are present in the command block wrapper (CBW) and
// the command status wrapper (CSW).
#define NUM_BYTES_IN_CBW  31
#define NUM_BYTES_IN_CSW  13

// The signatures that need to be seen when receiving a command block wrapper (CBW) or
// sending a command status wrapper (CSW).
#define USB_DCBW_SIGNATURE (0x43425355)
#define USB_DCSW_SIGNATURE (0x53425355)

// The bit in a command block wrapper to say whether we have an IN or OUT.
#define USB_CBW_DIRECTION_BIT 0x80

// Supported media types.
#define DISK_DRIVE_TYPE 0x00
#define CD_ROM_TYPE     0x05

// The two choices for rmb in the MASS_STORAGE_DEVICE_INFO_STRUCT.
#define MEDIA_NOT_REMOVABLE 0x00
#define MEDIA_IS_REMOVABLE  0x80

// The status value returned in the CSW.
#define CSW_STATUS_PASSED      0x0
#define CSW_STATUS_FAILED      0x1
#define CSW_STATUS_PHASE_ERROR 0x2

// Defines for the information returned in response to the request sense command.
// Sense keys.
#define NO_SENSE        0x0
#define UNIT_NOT_READY  0x2
#define MEDIUM_ERROR    0x3
#define HARDWARE_ERROR  0x4
#define ILLEGAL_REQUEST 0x5
#define UNIT_ATTENTION  0x6
#define DATA_PROTECT    0x7
#define ABORTED_COMMAND 0xB

// The additional sense and additional sense qualifier codes.
#define LOG_UNIT_NOT_RDY_OP_IN_PRGS_ASC  0x04
#define LOG_UNIT_NOT_RDY_OP_IN_PRGS_ASCQ 0x07 

#define UNRECOVERED_RD_ERR_ASC  0x11
#define UNRECOVERED_RD_ERR_ASCQ 0x0

#define UNRECOVERED_WRT_ERR_ASC  0xC
#define UNRECOVERED_WRT_ERR_ASCQ 0x0

#define LBA_OUT_OF_RANGE_ASC  0x21
#define LBA_OUT_OF_RANGE_ASCQ 0x0

#define INVALID_FIELD_IN_CDB_ASC  0x24
#define INVALID_FIELD_IN_CDB_ASCQ 0x0

#define LOG_UNIT_NOT_SUPPORTED_ASC  0x25
#define LOG_UNIT_NOT_SUPPORTED_ASCQ 0x25

#define NOT_READY_X_MEDIA_CHANGED_ASC  0x28
#define NOT_READY_X_MEDIA_CHANGED_ASCQ 0

#define POWER_ON_RESET_ASC  0x29
#define POWER_ON_RESET_ASCQ 0

#define MEDIUM_NOT_PRESENT_ASC  0x3A
#define MEDIUM_NOT_PRESENT_ASCQ 0

// The media removal prevent position and mask.
#define MR_PREVENT_POS  4
#define MR_PREVENT_MASK 0x1

#define WSD_READ_SPEED (24 * 1024)
#define WSD_WRITE_SPEED (8 * 1024)

// Helper #defines for getting the read/write speeds into the arrays. As before
// the indices at the end refer to the byte placement in the structures. Also
// in this case, the start and ending performances are the same, so the starting
// performance values will also be used in the ending performance.
#define WSD_READ_SPEED_8 ((WSD_READ_SPEED & 0xFF000000) >> 24)
#define WSD_READ_SPEED_9 ((WSD_READ_SPEED & 0x00FF0000) >> 16)
#define WSD_READ_SPEED_10 ((WSD_READ_SPEED & 0x0000FF00) >> 8)
#define WSD_READ_SPEED_11 ((WSD_READ_SPEED & 0x000000FF) >> 0)
#define WSD_WRITE_SPEED_12 ((WSD_WRITE_SPEED & 0xFF000000) >> 24)
#define WSD_WRITE_SPEED_13 ((WSD_WRITE_SPEED & 0x00FF0000) >> 16)
#define WSD_WRITE_SPEED_14 ((WSD_WRITE_SPEED & 0x0000FF00) >> 8)
#define WSD_WRITE_SPEED_15 ((WSD_WRITE_SPEED & 0x000000FF) >> 0)

// The CDFS sector size.
#define CDFS_SECTOR_SIZE 2048

// A FAT sector size.
#define FAT_SECT_SZ 512

// The number of profile descriptors. This is used in the PROFILE_LIST_FEATURE_STRUCT
// below. That structure in turn in part of the data that is returned in a Get
// Configuration command.
#define NUM_PROFILE_DESC 1

// Used in the handle_get_performance_command(). In our case the starting and
// ending performance is the same.
#define NOM_CD_RD_SPEED ((24 * 1024 * 1024)/ 1000)
#define NOM_CD_WRT_SPEED ((8 * 1024 * 1024)/ 1000)

// Helper #defines for getting the read/write speeds into the arrays. As before
// the indices at the end refer to the byte placement in the structures. Also
// in this case, the start and ending performances are the same, so the starting
// performance values will also be used in the ending performance.
#define NOM_CD_RD_SPEED_4 ((NOM_CD_RD_SPEED & 0xFF000000) >> 24)
#define NOM_CD_RD_SPEED_5 ((NOM_CD_RD_SPEED & 0x00FF0000) >> 16)
#define NOM_CD_RD_SPEED_6 ((NOM_CD_RD_SPEED & 0x0000FF00) >> 8)
#define NOM_CD_RD_SPEED_7 ((NOM_CD_RD_SPEED & 0x000000FF) >> 0)
#define NOM_CD_WRT_SPEED_4 ((NOM_CD_WRT_SPEED & 0xFF000000) >> 24)
#define NOM_CD_WRT_SPEED_5 ((NOM_CD_WRT_SPEED & 0x00FF0000) >> 16)
#define NOM_CD_WRT_SPEED_6 ((NOM_CD_WRT_SPEED & 0x0000FF00) >> 8)
#define NOM_CD_WRT_SPEED_7 ((NOM_CD_WRT_SPEED & 0x000000FF) >> 0)

//=============================================================================
// Enums
//=============================================================================
typedef enum {IN_CBW_STATE, IN_DATA_STATE} MS_STATE;

//=============================================================================
// Macros
//=============================================================================
// Used to determine whether the CBW sent in is an IN or OUT.
#define IN_CMD_SENT(x)  ((x)->bmCBWFlags & USB_CBW_DIRECTION_BIT)
#define OUT_CMD_SENT(x) (!((x)->bmCBWFlags & USB_CBW_DIRECTION_BIT))

#define PREVENT_MEDIA_REMOVAL(x) (x->CBWCB[MR_PREVENT_POS] & MR_PREVENT_MASK)
#define ALLOW_MEDIA_REMOVAL(x)   (!(x->CBWCB[MR_PREVENT_POS] & MR_PREVENT_MASK))

//=============================================================================
// Structures
//=============================================================================

// USB Command Block Wrapper
typedef struct cbw_struct 
{
    uint32_t dCBWSignature;
    uint32_t dCBWTag;
    uint32_t dCBWDataLength;
    unsigned char  bmCBWFlags;
    unsigned char  bCBWLun;
    unsigned char  bCBWCBLength;
    unsigned char  CBWCB[16];
} CBW_STRUCT;

// USB Command Status Wrapper
typedef struct csw_struct 
{
   uint32_t dCSWSignature;
   uint32_t dCSWTag;
   uint32_t dCSWDataResidue;
   unsigned char  bCSWStatus;
} CSW_STRUCT;


typedef struct scsi_request_sense_s
{
    // The response code and bit 7 is the valid bit. This determines
    // whether the information field is defined or not.
    unsigned char response_code;
    // Obselete.
    unsigned char obselete;
    // Sense key.
    unsigned char sense_key;
    // The information field.
    unsigned char sense_info[4];
    // Additional sense length.
    unsigned char add_sense_len;
    // Command specific information.
    unsigned char cmd_specific_info[4];
    // Additional sense code.
    unsigned char add_sense_code;
    // Additional sense code qualifier.
    unsigned char add_sense_code_qual;
    // field replaceable unit code.
    unsigned char field_replace_unit_code;
    // A concatenation of the SKSV bit and the MSB of the sense 
    // key specific information.
    unsigned char sksv_sense_key_specific;
    // The last two bytes of the sense key specific information.
    unsigned char sense_key_specific[2];

} scsi_request_sense_t;

typedef struct scsi_inquiry_s 
{
    // Peripheral device type/qualifier.
    unsigned char peripheral_device_type;
    // Removable medium bit position (7), all other bits reserved.
    unsigned char rmb;
    // Version, we usually claim ANSI 2.
    unsigned char ANSI_ECMA_ISO_version;
    // Response data format set so that data returned in supported format.
    unsigned char response_data_format;
    // The remaining bytes in the inquiry command.
    unsigned char additional_length;
    // Next three bytes are reserved, set to zero.
    unsigned char reserved[3];
    // 8 bytes of vendor information in ASCII.
    unsigned char vendor_information[8];
    // 16 bytes of product information in ASCII.
    unsigned char product_ID[16];
    // Product Revision level in ASCII.
    unsigned char product_revision_level[4];

} scsi_inquiry_t;

// SCSI Mode Sense(6) data.
typedef struct scsi_mode_sense_six_s
{
    // The number of data bytes to follow.
    unsigned char mode_data_length;
    // The medium type, this is always set to zero.
    unsigned char medium_type;
    // Bit 7, write protect is not set. Other bits reserved or don't care.
    unsigned char device_specific_param;
    // The block descriptors to follow.
    unsigned char block_descrip_len;

} scsi_mode_sense_six_t;

// SCSI Mode Sense(10) data.
typedef struct scsi_mode_sense_ten_s
{
    // The number of data bytes to follow.
    unsigned char mode_data_length[2];
    // The medium type, this is always set to zero.
    unsigned char medium_type;
    // Bit 7, write protect. Other bits reserved or don't care.
    unsigned char device_specific_param;
    // Long logical block address.
    unsigned char long_LBA;
    // Not used, reserved.
    unsigned char reserved;
    // The block descriptor length if present.
    unsigned char block_descrip_len[2];

} scsi_mode_sense_ten_t;

// The event header portion of scsi_event_status_notif_t.
typedef struct event_header_s
{
    // The number of bytes of data that follow. This does not include the bytes
    // specify the data length.
    unsigned char event_desc_len[2];
    // This byte represents the NEA (No Event Available) bit and the notification
    // class field.
    unsigned char nea_notif_class;
    // The supported event classes.
    unsigned char supported_event_class;

} event_header_t;

// The Operational Change Events portion of scsi_event_status_notif_t.
typedef struct op_change_events_s
{
    // Operational change.
    unsigned char event_code;
    // This byte represents the persistent bit and operational status field.
    unsigned char per_prevent_op_status;
    // The actual operational change.
    unsigned char op_change[2];

} op_change_events_t;

// The data that is returned for the Get Event Status Notification command.
typedef struct scsi_event_status_notif_s
{
    event_header_t     event_header;
    op_change_events_t op_change_events;

} scsi_event_status_notif_t;

// The definition for the rds_base_info of read_disk_struct_dvd_cpyrt_info_t
typedef struct read_disk_structure_base_s
{
    // The disk structure data length not counting these two bytes.
    unsigned char disk_struct_data_len[2];
    // Reserved.
    unsigned char reserved[2];

} read_disk_structure_base_t;

// The data that is returned for a read disk structure command.
typedef struct scsi_read_disk_protect_s
{
    read_disk_structure_base_t rds_base_info;
    // The copyright protection scheme.
    unsigned char              cpyrt_prot_sys_type;
    // Region management. Setting this to zero allows all regions to view.
    unsigned char              region_manage_info;
    // Reserved.
    unsigned char              reserved[2];

} scsi_read_disk_protect_t;

// SCSI READ FORMAT CAPACITIES data.
typedef struct read_format_capacities_s
{
    // Reserved.
    unsigned char reserved[3];
    // The number of data bytes to come.
    unsigned char capacity_list_length;
    // The number of pages present on device. Set dynamically.
    unsigned char num_of_blocks[4];
    // What other media is present.
    unsigned char descriptor_type;
    // The block size. Set dynamically.
    unsigned char block_length[3];

} read_format_capacities_t;

// The track descriptor. Used in the read_toc_response_t below.
typedef struct toc_track_desc_s
{
    // Reserved, set to zero.
    unsigned char reserved1;
    // A byte representing the ADR and CONTROL fields.
    unsigned char adr_control;
    // The track number this descriptor represents.
    unsigned char track_num;
    // Reserved, set to zero.
    unsigned char reserved2;
    // The track starting address.
    unsigned char track_start_adrs[4];

} toc_track_desc_t;

// The data that is returned for a Read TOC command.
typedef struct read_toc_response_s
{
    // The amount of data to come in this structure.
    unsigned char    TOC_data_len[2];
    // The first track number.
    unsigned char    first_track_num;
    // The last track number.
    unsigned char    last_track_num;
    toc_track_desc_t TOC_track_desc_one;
    toc_track_desc_t TOC_track_desc_two;

} read_toc_response_t;

typedef struct read_track_info_s
{
    // Length of the command. Set dynamically.
    unsigned char data_len[2];
    // LSB of track number
    unsigned char track_num_LSB;
    // LSB of session number
    unsigned char session_num_LSB;
    // Reserved, set to zero.
    unsigned char reserved1;
    // A byte representing the damage bit, copy bit, track mode, and
    // the layer jump recording status.
    unsigned char damage_copy_track_mode;
    // A byte representing the reserved track bit, blank bit, packet/inc bit, and
    // the data mode bit.
    unsigned char RT_blank_pktinc_FP_data_mode;
    // A byte representing the last recorded address bit and the new writable address
    // bit.
    unsigned char LRA_V_NWA_V;
    // The track start address of the first block with user information.
    unsigned char trk_start_adrs[4];
    // The next writable address connected to the bit above.
    unsigned char next_writable_adrs[4];
    // The number of free blocks. Set dynamically.
    unsigned char free_blocks[4];
    // The fixed packet size. Set dynamically.
    unsigned char fixed_pkt_size[4];
    // The track size.
    unsigned char track_size[4];
    // The last recorded address.
    unsigned char last_recorded_adrs[4];
    // MSB of the track number.
    unsigned char track_num_MSB;
    // MSB of the session number.
    unsigned char session_num_MSB;
    // Reserved, set to zero.
    unsigned char reserved2[2];

}read_track_info_t;

// The data that is returned for a read buffer capacity command.
typedef struct buffer_capacity_s
{
    // The number of bytes to return. Depends upon what the user requests.
    unsigned char data_length[2];
    // Reserved, set to zero.
    unsigned char reserved;
    // Whether the information is in blocks or not.
    unsigned char reserved_or_block;
    // If we return the capacity information in blocks, this field is zeroed,
    // else it is the buffer length again.
    unsigned char buf_len_or_reserved[4];
    // The buffer length in blocks or in bytes.
    unsigned char buf_len_in_bytes_or_blocks[4];

}  buffer_capacity_t;

// Report key for key format = 001000b, key class = 0.
typedef struct report_key_rpc_state_s
{
    // The length of the data to come.
    unsigned char data_length[2];
    // Reserved, set to zero.
    unsigned char reserved1[2];
    // A byte representing type code, vendor resets available, and the number of
    // user controlled changes available. 
    unsigned char tc_vra_ucca;
    // The region mask.
    unsigned char region_mask;
    // The region playback controls.
    unsigned char rpc_scheme;
    // Reserved, set to zero.
    unsigned char reserved2;

} report_key_rpc_state_t;


typedef struct exception_perf_s
{
    // The difference in performance between the sectors. Set to zero as there is none.
    unsigned char LBA[4];
    // The time difference between sectors. Set to zero as there is none.
    unsigned char time[2];

}exception_perf_t;

typedef struct nominal_perf_s
{
    // The starting sector logical block address. Will be zero.
    unsigned char start_LBA[4];
    // The performance value for the starting sector. Set dynamically.
    unsigned char start_perf[4];
    // The last logical block address. Set dynamically.
    unsigned char end_LBA[4];
    // The performance value for the ending sector. Set dynamically.
    unsigned char end_perf[4];

} nominal_perf_t;

typedef struct performance_header_s
{
    // The data length. This is set dynamically and depends upon what is requested.
    unsigned char perf_data_len[4];
    // A byte representing the write bit and expect bit.
    unsigned char write_except;
    // Reserved, set to zero.
    unsigned char reserved[3];

} performance_header_t;

typedef struct get_performance_data_s
{
    performance_header_t perf_header;
    nominal_perf_t       nom_perf_data;
    exception_perf_t     excep_perf_data;

} get_performance_data_t;

typedef struct perf_write_speed_s
{
    // A byte representing the write rotation control bit, restore drive defaults bit,
    // the exact bit, and the mixture of read/write bit.
    unsigned char WRC_RDD_exact_MRW;
    // Reserved, set to zero.
    unsigned char reserved[3];
    // The last logical block address. Set dynamically.
    unsigned char end_LBA[4];
    // The read speed in KB/sec.
    unsigned char read_speed[4];
    // The write speed in KB/sec.
    unsigned char write_speed[4];

} perf_write_speed_t;

typedef struct get_perf_write_speed_s
{
    performance_header_t perf_header;
    perf_write_speed_t   perf_wrt_speed;

} get_perf_write_speed_t;

// USB mass storage READ CAPACITY data.
typedef struct report_media_cap_s 
{
   unsigned char last_log_blk_adrs[4];
   unsigned char blk_len_in_bytes[4];

} report_media_cap_t;


//=============================================================================
// Function declarations
//=============================================================================

/**
 *
 * \brief Make the call to set the request sense data for a particular LUN.
 * 
 * \retval N/A 
 * 
 */
void set_request_sense_data(uint8_t LUN,uint8_t sense_key, uint8_t add_sense_code,
                            uint8_t add_sense_code_qual);

/**
 *
 * \brief Get the mass storage state, whether handling a CBW or 
 *        data
 *  
 * \retval the mass storage state.
 *  
 */
MS_STATE get_MS_state();

/**
 *
 * \brief Notify mass storage that set address was called.
 *  
 * \retval N/A 
 *  
 */
void notify_MS_SetAddr(void);

/**
 *
 * \brief Notify mass storage that set configuration was called.
 *  
 * \retval N/A 
 *  
 */
void notify_MS_SetCfg(void);

/**
 *
 * \brief Return the number of registered LUNs. This amounts to 
 *        all LUNs that were registered.
 *  
 * \retval the number of registered LUNs.
 *  
 */
uint32_t get_num_registered_LUNs();

/**
 *
 * \brief Return the number of advertised LUNs. This will be 
 *        those LUNs that wish to have their file system
 *        published to the outside world.
 *  
 * \retval the number of advertised LUNs. 
 *  
 */
uint32_t get_num_advertised_LUNs();

#endif // MS_SCSI_CMDS_H
