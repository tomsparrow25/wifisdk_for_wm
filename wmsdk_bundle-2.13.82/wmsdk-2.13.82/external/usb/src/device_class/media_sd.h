#include <stdio.h>
#include "error_types.h"

uint32_t media_manager_scsi_get_sector_size(uint8_t lun);

uint32_t media_manager_scsi_get_total_num_sectors(uint8_t lun);

error_type_t media_manager_scsi_sector_read(uint8_t lun, uint32_t sector_addr, uint32_t num_sectors, uint8_t *data);

error_type_t media_manager_scsi_sector_write(uint8_t lun, uint32_t sector_addr, uint32_t num_sectors, uint8_t *data);
void memory_init();

/*bob 10/11/2012*/
#define SRAM_MEDIA
//#define SD_CARD_MEDIA 
