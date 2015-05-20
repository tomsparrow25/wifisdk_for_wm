#include "media_sd.h"
#include "wm_utils.h"




#define BPB_BytsPerSec    11    /* Sector size [byte] (2) */
#define BPB_TotSec16      19    /* FAT size [sector] (2) */
#define BPB_TotSec32      32    /* FAT size [sector] (4) */

#define LD_WORD(ptr)      (uint16_t)(((uint16_t)*((uint8_t*)(ptr)+1)<<8)|(uint16_t)*(uint8_t*)(ptr))
#define	LD_DWORD(ptr)     (uint32_t)(((uint32_t)*((uint8_t*)(ptr)+3)<<24)|((uint32_t)*((uint8_t*)(ptr)+2)<<16)|((uint32_t)*((uint8_t*)(ptr)+1)<<8)|(uint32_t)*(uint8_t*)(ptr))

#if defined(SD_CARD_MEDIA)
uint32_t media_manager_scsi_get_sector_size(uint8_t lun)
{
  SDIO_ERR_Type errStatus;

  uint8_t data[512];
  uint32_t sector_size;
  
  errStatus = SD_Read(0, 1, 512, data);
  
  if(errStatus == SDIO_OK)
  {
    sector_size = (uint32_t)LD_WORD(data + BPB_BytsPerSec);
      
    return sector_size;
  }

  return 0;
}

uint32_t media_manager_scsi_get_total_num_sectors(uint8_t lun)
{
  SDIO_ERR_Type errStatus;

  uint8_t data[512];
  uint32_t num_sectors;
  
  errStatus = SD_Read(0, 1, 512, data);
  
  if(errStatus == SDIO_OK)
  {
    num_sectors = (uint32_t)LD_WORD(data + BPB_TotSec16);
    
    if(!num_sectors)
      num_sectors = (uint32_t)LD_DWORD(data + BPB_TotSec32);
      
    return num_sectors;
  }

  return 0;
}

error_type_t media_manager_scsi_sector_read(uint8_t lun, uint32_t sector_addr, uint32_t num_sectors, uint8_t *data)
{
  SDIO_ERR_Type errStatus;
  uint32_t addr;
  
  addr = sector_addr * 512;

  errStatus = SD_Read(addr, num_sectors, 512, data);

  if(errStatus != SDIO_OK)
    return FAIL;

  return OK;
}

error_type_t media_manager_scsi_sector_write(uint8_t lun, uint32_t sector_addr, uint32_t num_sectors, uint8_t *data)
{
  SDIO_ERR_Type errStatus;
  uint32_t addr;
  
  addr = sector_addr * 512;

  errStatus = SD_Write(addr, num_sectors, 512, data);
  
  if(errStatus != SDIO_OK)    
    return FAIL;

  return OK;
}
#else
/* stubs in case definition is required for compilation */
WEAK uint32_t media_manager_scsi_get_sector_size(uint8_t lun)
{
  return 0;
}

WEAK uint32_t media_manager_scsi_get_total_num_sectors(uint8_t lun)
{
  return 0;
}

WEAK error_type_t media_manager_scsi_sector_read(uint8_t lun,
		uint32_t sector_addr, uint32_t num_sectors, uint8_t *data)
{
  return OK;
}

WEAK error_type_t media_manager_scsi_sector_write(uint8_t lun,
		uint32_t sector_addr, uint32_t num_sectors, uint8_t *data)
{
  return OK;
}
#endif
