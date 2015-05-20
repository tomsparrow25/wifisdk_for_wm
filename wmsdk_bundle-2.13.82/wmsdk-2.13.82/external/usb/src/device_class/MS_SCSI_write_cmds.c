// /////////////////////////////////////////////////////////////////////////////
// 
//  ============================================================================
//  Copyright (c) 2011   Marvell Semiconductor, Inc. All Rights Reserved
//                       
//                          Marvell Confidential
//  ============================================================================
//
//
// Description: this module is tasked with handling OUT commands and also those
//              commands that write to the media.



////////////////////////////////////////////////////////////////////////////////
// Includes
////////////////////////////////////////////////////////////////////////////////
#include <stdint.h>
#include <string.h>
#include "error_types.h"
#include <wmassert.h>
#include "MS_SCSI_cmds.h"
#include "MS_SCSI_top.h"
#include "media_sd.h"
#include "debug.h"

#include <incl/agLinkedList.h>
#include <incl/devapi.h>
#include <incl/usbprv_dev.h>
#include <incl/asic_usb.h>
#include <incl/usb_list.h>
#include <incl/usb_device_api.h>
#include <incl/usbdrv.h>
#include <incl/io.h>
#include <incl/usbprvdev.h>

#include <wm_os.h>
#include <wmstdio.h>

//=============================================================================
// Defines
//=============================================================================

//=============================================================================
// Private data
//=============================================================================

//=============================================================================
// Local function declarations
//=============================================================================
static error_type_t handle_test_unit_ready(uint8_t LUN);
static error_type_t handle_scsi_sector_write(uint8_t LUN, CBW_STRUCT *cbw,
                                             uint32_t sector_adrs, uint32_t num_sectors,
                                             scsi_data_t *SCSI_data);



error_type_t handle_SCSI_write_cmd(CBW_STRUCT *cbw_ptr, scsi_data_t *SCSI_data)
{
    uint32_t     tot_sectors;
    uint32_t     start_sec_adrs;
    error_type_t status      = FAIL;
    uint8_t      cur_command = cbw_ptr->CBWCB[0];
    uint8_t      LUN         = cbw_ptr->bCBWLun;



    switch (cur_command)
    {
    case TEST_UNIT_RDY_CMD:
        status = handle_test_unit_ready(LUN);

        // No data is expected for this command.
        SCSI_data->num_exp_wrt_bytes = 0;
        break;

    // Just handle with a good status, not much else to do here.
    case PREV_ALLOW_MEDIA_REM_CMD:

        // No data is expected for this command.
        SCSI_data->num_exp_wrt_bytes = 0;
        status                       = OK;
        break;

    // We do not do anything with the data that comes in, just say that the status
    // was OK.
    case MODE_SELECT_6_CMD:

        // This will be used later to determine what the residue will be.
        SCSI_data->num_exp_wrt_bytes = cbw_ptr->CBWCB[4];
        status                       = OK;
        break;

    // We do not do anything with the data that comes in, just say that the status
    // was OK.
    case SEND_KEY_CMD:
        // This will be used later to determine what the residue will be.
        SCSI_data->num_exp_wrt_bytes =  cbw_ptr->CBWCB[8] << 8;
        SCSI_data->num_exp_wrt_bytes |= cbw_ptr->CBWCB[9];

        status = OK;
        break;

    case WRITE_DATA_10_CMD:
        {
            bool adrs_out_of_range;

            tot_sectors =  cbw_ptr->CBWCB[7] << 8;
            tot_sectors |= cbw_ptr->CBWCB[8];

            // This will be used later to determine what the residue will be.
            SCSI_data->num_exp_wrt_bytes = 
            tot_sectors * media_manager_scsi_get_sector_size(LUN);

            start_sec_adrs  = cbw_ptr->CBWCB[2] << 24;
            start_sec_adrs |= cbw_ptr->CBWCB[3] << 16;       
            start_sec_adrs |= cbw_ptr->CBWCB[4] << 8;
            start_sec_adrs |= cbw_ptr->CBWCB[5];

            adrs_out_of_range = check_acs_out_of_range(LUN, 
                                                       start_sec_adrs,
                                                       tot_sectors);
			
            // If the address is not out of range.
            if (!adrs_out_of_range)
            {

                status = handle_scsi_sector_write(LUN,
                                                  cbw_ptr,
                                                  start_sec_adrs,
                                                  tot_sectors,
                                                  SCSI_data);
            }
        }
        break;

    case VS_WRITE_CMD:
        ASSERT(0);
        break;

    default:
        dbg_printf("%s - unknown command: %#x\n", __func__, cur_command);
        ASSERT(0);
        break;
    }

    return(status);
}

//=============================================================================
// Local functions
//=============================================================================
error_type_t handle_test_unit_ready(uint8_t LUN)
{
    error_type_t status = OK;

    // If this is a CD-ROM check to see if it is time to respond with a good status.
    // This will mimic a CD-ROM coming up.
    if (get_device_type_of_LUN(LUN) == CD_ROM_TYPE)
    {
        if (!emulationReady(LUN))
        {
            status = FAIL;
        }
    }

    return(status);
}




/*bob 09/12/2012*/
/****************************************************************************//**
 * @brief  Simply cutting large scale downstream data into small pieces. The 
           size of piece is indicated by MSC_MAX_RW_BUFFER_SIZE. 
           
 * @param[in]  LUN:logical unit number
               cbw_ptr: point to a cbw struct
               sector_adrs: start block address
               num_sector: number of blocks to read             
               SCSI_data: SCSI_data structure 

 * @return error type
 *******************************************************************************/


error_type_t handle_scsi_sector_write(uint8_t     LUN, 
                                      CBW_STRUCT  *cbw_ptr,
                                      uint32_t    sector_adrs,
                                      uint32_t    num_sectors,
                                      scsi_data_t *SCSI_data)
{
    error_type_t status = OK;

	uint32_t sector_size = media_manager_scsi_get_sector_size(LUN);
    
    if (SCSI_data->num_wrt_bytes > 0)
    {     

        sector_adrs = sector_adrs + SCSI_data->num_wrt_sectors; 
        status = media_manager_scsi_sector_write(LUN, 
                                                 sector_adrs, 
                                                 SCSI_data->num_wrt_bytes / sector_size, 
                                                 SCSI_data->wrt_data_ptr);
    }
    if (status == OK)
    {
        SCSI_data->num_wrt_sectors = SCSI_data->num_wrt_sectors + SCSI_data->num_wrt_bytes / sector_size;
    }
	return(status);
}

