// /////////////////////////////////////////////////////////////////////////////
// $Header:
// 
//  ============================================================================
//  Copyright (c) 2008   Marvell Semiconductor, Inc. All Rights Reserved
//                       
//                          Marvell Confidential
//  ============================================================================
//
//
// Description: this is a mass storage class driver header file.

#ifndef MASS_STORAGE_BASE_H
#define MASS_STORAGE_BASE_H

////////////////////////////////////////////////////////////////////////////////
// Includes
////////////////////////////////////////////////////////////////////////////////
#include "agConnectMgr.h"

////////////////////////////////////////////////////////////////////////////////
// Defines
////////////////////////////////////////////////////////////////////////////////

// Supported media types.
#define DISK_DRIVE_TYPE 0x00
#define CD_ROM_TYPE     0x05

// The two choices for rmb in the MASS_STORAGE_DEVICE_INFO_STRUCT.
#define MEDIA_NOT_REMOVABLE 0x00
#define MEDIA_IS_REMOVABLE  0x80

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

// The status value returned in the CSW.
#define CSW_STATUS_PASSED      0x0
#define CSW_STATUS_FAILED      0x1
#define CSW_STATUS_PHASE_ERROR 0x2

// Defines for the information returned in response to the request sense command.
// Sense keys.
#define NO_SENSE        0x0
#define UNIT_NOT_READY  0x2
#define ILLEGAL_REQUEST 0x5
#define UNIT_ATTENTION  0x6
#define ABORTED_COMMAND 0xB

// The additional sense and additional sense qualifier codes.
#define LOG_UNIT_NOT_RDY_OP_IN_PRGS_ASC  0x04
#define LOG_UNIT_NOT_RDY_OP_IN_PRGS_ASCQ 0x07 

#define UNRECOVERED_RD_ERR_ASC  0x11
#define UNRECOVERED_RD_ERR_ASCQ 0x0

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

// The CDFS sector size.
#define CDFS_SECTOR_SIZE 2048

// A FAT sector size.
#define FAT_SECT_SZ 512

// The write speed descriptor read/write speeds. Used in the handle_get_performance_command().
// In KB/sec.
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

////////////////////////////////////////////////////////////////////////////////
// Enums
////////////////////////////////////////////////////////////////////////////////
typedef enum {IN_CBW_STATE, IN_DATA_STATE} MS_STATE;

////////////////////////////////////////////////////////////////////////////////
// Macros
////////////////////////////////////////////////////////////////////////////////
// Used to determine whether the CBW sent in is an IN or OUT.
#define IN_CMD_SENT(x)  ((x)->bmCBWFlags & USB_CBW_DIRECTION_BIT)
#define OUT_CMD_SENT(x) (!((x)->bmCBWFlags & USB_CBW_DIRECTION_BIT))

#define PREVENT_MEDIA_REMOVAL(x) (x->CBWCB[MR_PREVENT_POS] & MR_PREVENT_MASK)
#define ALLOW_MEDIA_REMOVAL(x)   (!(x->CBWCB[MR_PREVENT_POS] & MR_PREVENT_MASK))

////////////////////////////////////////////////////////////////////////////////
// Structures
////////////////////////////////////////////////////////////////////////////////

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

// USB Mass storage Device information. The fields for this structure
// come from the SCSI primary commmands 3 document. In particular the
// section dealing with the inquiry command.
typedef struct mass_storage_device_info 
{
    unsigned char peripheral_device_type;
    unsigned char rmb;
    unsigned char ANSI_ECMA_ISO_version;
    unsigned char response_data_format;
    unsigned char additional_length;
    unsigned char reserved[3];
    unsigned char vendor_information[8];
    unsigned char product_ID[16];
    unsigned char product_revision_level[4];

} MASS_STORAGE_DEVICE_INFO_STRUCT;

// USB mass storage READ CAPACITY data.
typedef struct mass_storage_read_capacity 
{
   unsigned char LAST_LOGICAL_BLOCK_ADDRESS[4];
   unsigned char BLOCK_LENGTH_IN_BYTES[4];

} MASS_STORAGE_READ_CAPACITY_STRUCT;

// Structure to hold the information for a write.
typedef struct write_transfer_info
{
    uint32_t     write_address;
    uint32_t     CBW_write_length;
    unsigned char      *write_buffer_ptr;
    uint32_t     total_bytes_written;
    CSW_STRUCT CSW_to_ret;

} WRITE_TRANSFER_INFO;

// SCSI READ FORMAT CAPACITIES data.
typedef struct read_format_capacities_struct
{
    unsigned char reserved[3];
    unsigned char capacity_list_length;
    unsigned char num_of_blocks[4];
    unsigned char descriptor_type;
    unsigned char block_length[3];

} READ_FORMAT_CAPACITIES_STRUCT;

typedef struct lun_wrt_spd_table
{
    unsigned char reserved;
    unsigned char rotation_ctrl;
    unsigned char wrt_speed_supported[2];

} LUN_WRT_SPD_TABLE;

// SCSI Mode Sense(6) data.
typedef struct mode_parameter_header_struct_six
{
    unsigned char mode_data_length;
    unsigned char medium_type;
    unsigned char device_specific_param;
    unsigned char block_descrip_len;

} MODE_PARAMETER_HEADER_STRUCT_SIX;

// SCSI Mode Sense(10) data.
typedef struct mode_parameter_header_struct_ten
{
    unsigned char mode_data_length[2];
    unsigned char medium_type;
    unsigned char device_specific_param;
    unsigned char long_LBA;
    unsigned char reserved;
    unsigned char block_descrip_len[2];

} MODE_PARAMETER_HEADER_STRUCT_TEN;

// SCSI Request Sense data.
typedef struct request_sense_struct
{
    unsigned char response_code;
    unsigned char obselete;
    unsigned char sense_key;
    unsigned char sense_info[4];
    unsigned char add_sense_len;
    unsigned char cmd_specific_info[4];
    unsigned char add_sense_code;
    unsigned char add_sense_code_qual;
    unsigned char field_replace_unit_code;
    unsigned char sksv_sense_key_specific;
    unsigned char sense_key_specific[2];

} REQUEST_SENSE_STRUCT;

// These structures below are those that are needed for returning data on a Get Event
// Status Notification command (0x4A). This command allows the host to query the drive
// as to event changes.
//
// The event header.
typedef struct event_header_struct
{
    unsigned char event_desc_len[2];
    unsigned char nea_notif_class;
    unsigned char supported_event_class;

} EVENT_HEADER_STRUCT;

// The Operational Change Events structure.
typedef struct op_change_events_struct
{
    unsigned char event_code;
    unsigned char per_prevent_op_status;
    unsigned char op_change[2];

} OP_CHANGE_EVENTS_STRUCT;

// The data that is returned for the Get Event Status Notification command.
typedef struct get_event_status_notif_struct
{
    EVENT_HEADER_STRUCT     event_header;
    OP_CHANGE_EVENTS_STRUCT op_change_events;

} GET_EVENT_STATUS_NOTIF_STRUCT;

// The track descriptor. Used in the READ_TOC_RESPONSE_STRUCT below.
typedef struct toc_track_desc_struct
{
    unsigned char reserved1;
    unsigned char adr_control;
    unsigned char track_num;
    unsigned char reserved2;
    unsigned char track_start_adrs[4];

} TOC_TRACK_DESC_STRUCT;

// The data that is returned for a Read TOC command.
typedef struct read_toc_response_struct
{
    unsigned char                 TOC_data_len[2];
    unsigned char                 first_track_num;
    unsigned char                 last_track_num;
    TOC_TRACK_DESC_STRUCT TOC_track_desc_one;
    TOC_TRACK_DESC_STRUCT TOC_track_desc_two;

} READ_TOC_RESPONSE_STRUCT;

// The data that is returned for a read buffer capacity command.
typedef struct buffer_capacity_struct
{
    unsigned char data_length[2];
    unsigned char reserved;
    unsigned char reserved_or_block;
    unsigned char buf_len_or_reserved[4];
    unsigned char buf_len_in_bytes_or_blocks[4];

}  BUFFER_CAPACITY_STRUCT;

// The data that is returned for a read disk structure command.
typedef struct read_disk_structure_base_struct
{
    unsigned char disk_struct_data_len[2];
    unsigned char reserved[2];

} READ_DISK_STRUCTURE_BASE_STRUCT;

typedef struct read_disk_struct_dvd_cpyrt_info_struct
{
    READ_DISK_STRUCTURE_BASE_STRUCT rds_base_info;
    unsigned char                           cpyrt_prot_sys_type;
    unsigned char                           region_manage_info;
    unsigned char                           reserved[2];

} READ_DISK_STRUCT_DVD_CPYRT_INFO_STRUCT;

typedef struct exception_perf_struct
{
    unsigned char LBA[4];
    unsigned char time[2];

}EXCEPTION_PERF_STRUCT;

typedef struct nominal_perf_struct
{
    unsigned char start_LBA[4];
    unsigned char start_perf[4];
    unsigned char end_LBA[4];
    unsigned char end_perf[4];

} NOMINAL_PERF_STRUCT;

typedef struct performance_header_struct
{
    unsigned char perf_data_len[4];
    unsigned char write_except;
    unsigned char reserved[3];

} PERFORMANCE_HEADER_STRUCT;

typedef struct perf_write_speed_struct
{
    unsigned char WRC_RDD_exact_MRW;
    unsigned char reserved[3];
    unsigned char end_LBA[4];
    unsigned char read_speed[4];
    unsigned char write_speed[4];

} PERF_WRITE_SPEED_STRUCT;

typedef struct get_performance_data_struct
{
    PERFORMANCE_HEADER_STRUCT perf_header;
    NOMINAL_PERF_STRUCT       nom_perf_data;
    EXCEPTION_PERF_STRUCT     excep_perf_data;

} GET_PERFORMANCE_DATA_STRUCT;

typedef struct get_perf_write_speed_struct
{
    PERFORMANCE_HEADER_STRUCT perf_header;
    PERF_WRITE_SPEED_STRUCT   perf_wrt_speed;

} GET_PERF_WRITE_SPEED_STRUCT;

typedef struct report_key_rpc_state_struct
{
    unsigned char data_length[2];
    unsigned char reserved1[2];
    unsigned char tc_vra_ucca;
    unsigned char region_mask;
    unsigned char rpc_scheme;
    unsigned char reserved2;

} REPORT_KEY_RPC_STATE_STRUCT;

typedef struct read_track_info_struct
{
    unsigned char data_len[2];
    unsigned char track_num_LSB;
    unsigned char session_num_LSB;
    unsigned char reserved1;
    unsigned char damage_copy_track_mode;
    unsigned char RT_blank_pktinc_FP_data_mode;
    unsigned char LRA_V_NWA_V;
    unsigned char trk_start_adrs[4];
    unsigned char next_writable_adrs[4];
    unsigned char free_blocks[4];
    unsigned char fixed_pkt_size[4];
    unsigned char track_size[4];
    unsigned char last_recorded_adrs[4];
    unsigned char track_num_MSB;
    unsigned char session_num_MSB;
    unsigned char reserved2[2];

}READ_TRACK_INFO_STRUCT;

////////////////////////////////////////////////////////////////////////////////
// Functions
////////////////////////////////////////////////////////////////////////////////

bool is_MS_init_complete();
void send_CSW_or_data(IOReg *pipe_ptr, void *data_ptr, uint32_t num_bytes_to_send);
void init_storage_volume(const char *volume_name, uint32_t *page_size_ptr,
                         uint32_t *num_sectors_ptr);
bool check_if_volume_formatted(const char *volume_name);
bool check_for_valid_CBW(char *data_buffer_ptr, uint32_t num_bytes, bool CBW_seen);
void wrt_data_to_storage(char *data_buffer_ptr, uint32_t num_bytes, IOReg *io_reg_ptr,
                         WRITE_TRANSFER_INFO *write_info_ptr, const char *volume_name,
                         REQUEST_SENSE_STRUCT *request_sense_ptr);
void handle_sending_CSW(CBW_STRUCT *cbw_ptr, IOReg *io_reg_ptr, uint32_t ret_status);
int do_FAT_volume_read(void *ret_data_ptr, uint32_t read_address, uint32_t read_length,
                       const char *volume_name, REQUEST_SENSE_STRUCT *req_sense_ptr,

                       uint8_t periph_device_type);
void handle_read_command(CBW_STRUCT *cbw_ptr, IOReg *io_reg_ptr,
                         uint32_t flash_size_in_bytes, uint32_t page_size, 
                         const char *volume_name, REQUEST_SENSE_STRUCT *req_sense_ptr,
                         unsigned char periph_device_type);
void handle_read_TOC_response_command(CBW_STRUCT *cbw_ptr, IOReg *io_reg_ptr,
                                      uint32_t start_LBA_value, uint32_t last_LBA_value,
                                      READ_TOC_RESPONSE_STRUCT *read_TOC_resp_ptr);
void handle_read_track_info_command(CBW_STRUCT *cbw_ptr, IOReg *io_reg_ptr,
                                    uint32_t num_pages, uint32_t page_size,
                                    READ_TRACK_INFO_STRUCT *read_track_ptr);

void handle_read_buffer_capacity_command(CBW_STRUCT *cbw_ptr, IOReg *io_reg_ptr,
                                         BUFFER_CAPACITY_STRUCT *buf_cap_ptr,
                                         uint32_t page_size);
void handle_report_key_command(CBW_STRUCT *cbw_ptr, IOReg *io_reg_ptr,
                               REPORT_KEY_RPC_STATE_STRUCT *report_key_rpc_state_ptr,
                               REQUEST_SENSE_STRUCT *req_sense_ptr);
void handle_get_performance_command(CBW_STRUCT *cbw_ptr, IOReg *io_reg_ptr,
                                    GET_PERFORMANCE_DATA_STRUCT *perf_data_ptr,
                                    GET_PERF_WRITE_SPEED_STRUCT *perf_wrt_speed_ptr,
                                    uint32_t last_LBA, REQUEST_SENSE_STRUCT *req_sense_ptr);
void handle_read_disk_structure_command(CBW_STRUCT *cbw_ptr, IOReg *io_reg_ptr,
                                        READ_DISK_STRUCT_DVD_CPYRT_INFO_STRUCT *rds_DVD_cpyrt_ptr);
void handle_read_CD_MSF_command(CBW_STRUCT *cbw_ptr, IOReg *io_reg_ptr, uint32_t page_size, 
                                uint32_t flash_size_in_bytes, const char *volume_name, 
                                REQUEST_SENSE_STRUCT *req_sense_ptr, unsigned char periph_device_type);
void handle_read_CD_command(CBW_STRUCT *cbw_ptr, IOReg *io_reg_ptr, uint32_t flash_size_in_bytes,
                            uint32_t page_size, const char *volume_name,
                            REQUEST_SENSE_STRUCT *req_sense_ptr, unsigned char periph_device_type);
void handle_bus_reset_command(CBW_STRUCT *cbw_ptr, IOReg *io_reg_ptr, REQUEST_SENSE_STRUCT *req_sense_ptr);
void handle_bus_reset_all_interface_command(CBW_STRUCT *cbw_ptr, IOReg *io_reg_ptr, 
                                            REQUEST_SENSE_STRUCT *req_sense_ptr);
void handle_test_unit_ready(CBW_STRUCT *cbw_ptr, IOReg *pipe_ptr, REQUEST_SENSE_STRUCT *req_sense_ptr);
void handle_request_sense_command(CBW_STRUCT *cbw_ptr, IOReg *pipe_ptr,
                                  REQUEST_SENSE_STRUCT *request_sense_ptr);
void handle_inquiry_command(CBW_STRUCT *cbw_ptr, IOReg *pipe_ptr,
                            MASS_STORAGE_DEVICE_INFO_STRUCT *device_info_ptr);
void handle_mode_sense_six_command(CBW_STRUCT *cbw_ptr, IOReg *pipe_ptr,
                                   MODE_PARAMETER_HEADER_STRUCT_SIX *mode_6_param_hdr_ptr);
void handle_start_stop_unit_command(CBW_STRUCT *cbw_ptr, IOReg *io_reg_ptr, 
                                    REQUEST_SENSE_STRUCT *req_sense_ptr);
void handle_media_removal_command(CBW_STRUCT *cbw_ptr, IOReg *io_reg_ptr, REQUEST_SENSE_STRUCT *req_sense_ptr);
void handle_read_format_capacities(CBW_STRUCT *cbw_ptr, IOReg *pipe_ptr,
                                   READ_FORMAT_CAPACITIES_STRUCT *rd_format_cap_ptr);
void handle_report_capacity(CBW_STRUCT *cbw_ptr, IOReg *pipe_ptr,
                            MASS_STORAGE_READ_CAPACITY_STRUCT *rd_cap_ptr, REQUEST_SENSE_STRUCT *req_sense_ptr);
void handle_verify_command(CBW_STRUCT *cbw_ptr, IOReg *io_reg_ptr, REQUEST_SENSE_STRUCT *req_sense_ptr);
void handle_synchronize_cache_command(CBW_STRUCT *cbw_ptr, IOReg *io_reg_ptr, 
                                      REQUEST_SENSE_STRUCT *req_sense_ptr);
void handle_get_configuration_command(CBW_STRUCT *cbw_ptr, IOReg *io_reg_ptr, UINT8 *data_ptr,
                                      uint32_t num_bytes_to_wrt, bool send_stall);
void handle_get_event_status_notif_command(CBW_STRUCT *cbw_ptr, IOReg *pipe_ptr,
                                           GET_EVENT_STATUS_NOTIF_STRUCT *es_notif_ptr);
void handle_mode_sense_ten_command(CBW_STRUCT *cbw_ptr, IOReg *pipe_ptr,
                                   MODE_PARAMETER_HEADER_STRUCT_TEN *mode_10_param_hdr_ptr);
void handle_close_track_command(CBW_STRUCT *cbw_ptr, IOReg *io_reg_ptr, 
                                REQUEST_SENSE_STRUCT *req_sense_ptr);
void handle_set_CD_speed(CBW_STRUCT *cbw_ptr, IOReg *io_reg_ptr, REQUEST_SENSE_STRUCT *req_sense_ptr);
void send_vendor_specific_data(CBW_STRUCT *cbw_ptr, IOReg *io_reg_ptr, UINT8 *VS_data_ptr,
                               uint32_t num_VS_data_bytes, uint32_t *num_bytes_written);
void handle_unsupported_command(CBW_STRUCT *cbw_ptr, IOReg *pipe_ptr, 
                                REQUEST_SENSE_STRUCT *req_sense_ptr);
void flush_all_sectors_to_NAND(const char *NAND_part_name_ptr);
void send_CSW_for_zero_byte_OUT(uint32_t trans_length, CBW_STRUCT *cbw_ptr, IOReg *io_reg_ptr);
void send_IN_when_OUT_expected(CBW_STRUCT *cbw_ptr, IOReg *io_reg_ptr);
void handle_write_out_of_range(CBW_STRUCT *cbw_ptr, IOReg *io_reg_ptr, uint32_t data_len_swapped);
void handle_no_data_ret_cmd(CBW_STRUCT *cbw_ptr, IOReg *io_reg_ptr,
                            REQUEST_SENSE_STRUCT *req_sense_ptr);
MS_STATE get_MS_state();

#endif // MASS_STORAGE_BASE_H
