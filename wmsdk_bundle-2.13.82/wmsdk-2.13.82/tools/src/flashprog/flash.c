/*
 * Copyright 2008-2012, Marvell International Ltd.
 * All Rights Reserved.
 */

#include <stdio.h>
#include <string.h>
#include <flash.h>
#include <ftfs.h>
#include <psm.h>
#include <partition.h>
#include <arch/flash_layout.h>
#include <mc200_flash.h>
#include <mc200_xflash.h>
#include "flashprog.h"
#include "firmware.h"
#include "crc32.h"
#include "mc200_spi_flash.h"

int fl_dev_eraseall(int fl_dev)
{
	if (fp_fl[fl_dev])
		return 1;

	switch (fl_dev) {
	case FL_INT:
		return FLASH_EraseAll();
#ifdef CONFIG_XFLASH_DRIVER
	case FL_EXT:
		return XFLASH_EraseAll();
#endif
#ifdef CONFIG_SPI_FLASH_DRIVER
	case FL_SPI_EXT:
		return spi_flash_eraseall();
#endif
	default:
		printf("Unsupported flash device type %d\n", fl_dev);
		return 1;
	}
}

int fl_dev_erase(int fl_dev, u32 start, u32 end)
{
	if (fp_fl[fl_dev])
		return 1;

	switch (fl_dev) {
	case FL_INT:
		return FLASH_Erase(start, end);
#ifdef CONFIG_XFLASH_DRIVER
	case FL_EXT:
		return XFLASH_Erase(start, end);
#endif
#ifdef CONFIG_SPI_FLASH_DRIVER
	case FL_SPI_EXT:
		return spi_flash_erase(start, end);
#endif
	default:
		printf("Unsupported flash device type %d\n", fl_dev);
		return 1;
	}
}

int fl_dev_write(int fl_dev, u32 addr, u8 *buf, u32 len)
{
	if (fp_fl[fl_dev]) {
		fseek(fp_fl[fl_dev], addr, SEEK_SET);
		return fwrite(buf, 1, len, fp_fl[fl_dev]);
	}

	switch (fl_dev) {
	case FL_INT:
		return FLASH_Write(FLASH_PROGRAM_QUAD, addr, buf, len);
#ifdef CONFIG_XFLASH_DRIVER
	case FL_EXT:
		return XFLASH_Write(XFLASH_PROGRAM_QUAD, addr, buf, len);
#endif
#ifdef CONFIG_SPI_FLASH_DRIVER
	case FL_SPI_EXT:
		return spi_flash_write(buf, len, addr);
#endif
	default:
		printf("Unsupported flash device type %d\n", fl_dev);
		return 1;
	}
}

int fl_dev_read(int fl_dev, u32 addr, u8 *buf, u32 len)
{
	if (fp_fl[fl_dev]) {
		fseek(fp_fl[fl_dev], addr, SEEK_SET);
		return fread(buf, 1, len, fp_fl[fl_dev]);
	}

	switch (fl_dev) {
	case FL_INT:
		return FLASH_Read(FLASH_FAST_READ_QUAD_IO, addr, buf, len);
#ifdef CONFIG_XFLASH_DRIVER
	case FL_EXT:
		return XFLASH_Read(XFLASH_FAST_READ_QUAD_IO, addr, buf, len);
#endif
#ifdef CONFIG_SPI_FLASH_DRIVER
	case FL_SPI_EXT:
		return spi_flash_read(buf, len, addr);
#endif
	default:
		printf("Unsupported flash device type %d\n", fl_dev);
		return 1;
	}
}

/*
 * Description	: write data buffer to flash and append CRC of written data
 * Input	: int fl_dev	=> Flash Device to be written
 *		: u32 addr	=> Offset at which data is to be written
 *		: u32 len	=> Length of data to be written
 *		: void *buf	=> Pointer to the data buffer
 * Output	: 0  => ok
 *		: -1 => Fail
 */
static int flash_write_with_crc(int fl_dev, u32 addr, u32 len, void *buf)
{
	int error;
	u32 crc;

	error = fl_dev_write(fl_dev, addr, (u8 *) buf, len);
	if (error == 0)
		return error;
	crc = crc32(buf, len, 0);
	return fl_dev_write(fl_dev, addr + len, (u8 *) &crc, sizeof(crc));
}

int flash_get_comp(const char *comp)
{
	if (!strcmp(comp, "FC_COMP_FW"))
		return FC_COMP_FW;
	else if (!strcmp(comp, "FC_COMP_WLAN_FW"))
		return FC_COMP_WLAN_FW;
	else if (!strcmp(comp, "FC_COMP_FTFS"))
		return FC_COMP_FTFS;
	else if (!strcmp(comp, "FC_COMP_BOOT2"))
		return FC_COMP_BOOT2;
	else if (!strcmp(comp, "FC_COMP_PSM"))
		return FC_COMP_PSM;
	else if (!strcmp(comp, "FC_COMP_USER_APP"))
		return FC_COMP_USER_APP;
	else {
		printf("Error: Invalid flash component %s\n", comp);
		return -1;
	}
}

char *flash_get_fc_name(enum flash_comp comp)
{
	struct partition_entry *pe;
	pe = part_get_layout_by_id(comp, 0);
	if (pe == NULL)
		printf("Error: Invalid flash component %d\n", comp);
	return pe->name;
}

int part_update(u32 addr, struct partition_table *pt,
					struct partition_entry *pe)
{
	int ret;
	uint32_t entries = pt->partition_entries_no;

	if (entries > MAX_FL_COMP) {
		printf("Exceeds max partition entries %d, Truncating...\n",
				MAX_FL_COMP);
		pt->partition_entries_no = MAX_FL_COMP;
		entries = MAX_FL_COMP;
	}

	ret = fl_dev_erase(FL_PART_DEV, addr, (addr + FL_PART_SIZE - 1));
	if (ret == 0)
		goto end;
	ret = flash_write_with_crc(FL_PART_DEV, addr, sizeof(*pt) - 4, pt);
	if (ret == 0)
		goto end;
	addr += sizeof(*pt);
	ret = flash_write_with_crc(FL_PART_DEV, addr, (sizeof(*pe) * entries),
			pe);
	if (ret == 0)
		goto end;
	return 1;
end:
	return ret;
}

int fl_read_part_table(int device, uint32_t part_addr,
				struct partition_table *pt)
{
	uint32_t crc;

	fl_dev_read(device, part_addr, (u8 *)pt,
			sizeof(struct partition_table));
	crc = crc32(pt, sizeof(struct partition_table), 0);
	if (crc != 0)
		return -1;
	return 0;
}

int fl_read_part_entries(int device, uint32_t part_addr)
{
	uint32_t read_size, crc, crc_p;

	part_addr += sizeof(struct partition_table);
	read_size = g_flash_table.partition_entries_no *
		sizeof(struct partition_entry);
	fl_dev_read(device, part_addr, (u8 *)g_flash_parts, read_size);
	part_addr += read_size;

	crc = crc32(g_flash_parts, read_size, 0);
	fl_dev_read(device, part_addr, (u8 *)&crc_p, 4);
	if (crc != crc_p)
		return -1;
	return 0;
}

int validate_flash_layout(struct partition_entry *fl, int comp_no)
{
	struct partition_entry *f1, *f2;
	uint32_t int_size, ext_size;
	int cnt;
	short index;

	for (cnt = 0; cnt < comp_no; cnt++) {
		int_size = ext_size = 0;

		fl[cnt].device == FL_INT ?
			(int_size = fl[cnt].start + fl[cnt].size) :
			(ext_size = fl[cnt].start + fl[cnt].size);

		if (int_size > FLASH_CHIP_SIZE  ||
				ext_size > XFLASH_CHIP_SIZE) {
			printf("Err: Component %s out of flash range\n",
						fl[cnt].name);
			printf("%s : %d\r\n", __FILE__, __LINE__);
			return -1;
		}
	}

	f1 = part_get_layout_by_id(FC_COMP_BOOT2, 0);
	if (f1 != NULL) {
		if (f1->start != 0x0 || f1->device != FL_INT) {
			printf("Err: Boot2 component can only be at 0x0 "
					"in internal flash\n");
			printf("%s : %d\r\n", __FILE__, __LINE__);
			return -1;
		}
	}

	index = 0;
	f1 = part_get_layout_by_id(FC_COMP_FW, &index);
	f2 = part_get_layout_by_id(FC_COMP_FW, &index);
	if (f1 != NULL && f2 != NULL) {
		if (f1->device != f2->device) {
			printf("Err: Upgradable %s components should have"
					" same flash type\n",
					f1->name);
			printf("%s : %d\r\n", __FILE__, __LINE__);
			return -1;
		}
		if (f1->size != f2->size) {
			printf("Err: Upgradable %s components should have"
					" same size\n",
					f1->name);
			printf("%s : %d\r\n", __FILE__, __LINE__);
			return -1;
		}
		if (f1->device != FL_INT || f2->device != FL_INT) {
			printf("Err: Upgradable %s components can only be"
					" present in internal flash\n",
					f1->name);
			printf("%s : %d\r\n", __FILE__, __LINE__);
			return -1;
		}
	}

	index = 0;
	f1 = part_get_layout_by_id(FC_COMP_FTFS, &index);
	f2 = part_get_layout_by_id(FC_COMP_FTFS, &index);
	if (f1 != NULL && f2 != NULL) {
		if (f1->size != f2->size) {
			printf("Err: Upgradable %s components should have"
					" same size\n", f1->name);
			printf("%s : %d\r\n", __FILE__, __LINE__);
			return -1;
		}
	}
	return 0;
}

void check_fl_layout_and_warn()
{
	struct partition_entry *f1, *f2, *f;
	uint32_t sig;
	short index;
	u8 *buf;
	u8 ftfs_sig[8];

	f1 = part_get_layout_by_id(FC_COMP_BOOT2, 0);
	if (f1 != NULL) {
		u32 boot2_len, boot2_crc, crc;
		fl_dev_read(f1->device, f1->start + 16,
					(u8 *)&boot2_crc, sizeof(uint32_t));
		if (boot2_crc != 0xffffffff) {
			fl_dev_read(f1->device,
					f1->start + 16 + sizeof(uint32_t),
					(u8 *)&boot2_len, sizeof(uint32_t));
			fl_dev_read(f1->device,
					f1->start + FL_BOOTROM_H_SIZE,
					(u8 *)gbuffer_1, boot2_len);
			crc = crc32(gbuffer_1, boot2_len, 0);
		} else {
			crc = 0;
		}

		if (crc != boot2_crc && boot2_crc == 0xffffffff)
			printf("# Warning: Boot2 block is empty\n");
		else if (crc != boot2_crc)
			printf("# Warning: Boot2 block is corrupted,"
				"actual: %x, expected: %x\n",
				crc, boot2_crc);
		else
			printf("# Valid Boot2 image found\n");
	}

	f1 = part_get_layout_by_id(FC_COMP_PSM, 0);
	if (f1 != NULL) {
		fl_dev_read(f1->device, f1->start, (u8 *)gbuffer_1,
							PARTITION_SIZE);
		psm_metadata_t *metadata = (psm_metadata_t *) gbuffer_1;
		u32 crc = crc32(metadata->vars, METADATA_ENV_SIZE, 0);
		if (crc != metadata->crc && metadata->crc == 0xffffffff)
			printf("# Warning: PSM block is empty\n");
		else if (crc != metadata->crc)
			printf("# Warning: PSM block is corrupted,"
				"actual: %x, expected: %x\n",
				crc, metadata->crc);
		else
			printf("# Valid PSM block found\n");
	}

	buf = (u8 *)&sig;
	index = 0;
	f1 = part_get_layout_by_id(FC_COMP_FW, &index);
	f2 = part_get_layout_by_id(FC_COMP_FW, &index);
	if (f1 != NULL && f2 != NULL)
		/* Select high generation level image */
		f = (f1->gen_level >= f2->gen_level ? f1 : f2);
	else
		f = f1;

	if (f != NULL) {
		fl_dev_read(f->device, f->start, buf, 4);
		if (sig != FW_MAGIC_STR) {
			printf("# Warning: Valid %s image not found at"
				" address 0x%x %s\n",
				f->name,
				(unsigned int) f->start,
				f->device == FL_EXT ? "(ext flash)" :
				"");
		 } else {
			 printf("# Valid %s image (active) found at address"
				" 0x%x %s\n",
				f->name,
				(unsigned int) f->start,
				f->device == FL_EXT ? "(ext flash)" :
				"");
		 }
	}

	index = 0;
	f1 = part_get_layout_by_id(FC_COMP_WLAN_FW, &index);
	f2 = part_get_layout_by_id(FC_COMP_WLAN_FW, &index);
	if (f1 != NULL && f2 != NULL)
		/* Select high generation level image */
		f = (f1->gen_level >= f2->gen_level ? f1 : f2);
	else
		f = f1;

	if (f1 != NULL) {
		fl_dev_read(f1->device, f1->start, buf, 4);
		if (sig != WLAN_FW_MAGIC) {
			printf("# Warning: Valid %s image not found at"
				" address 0x%x %s\n",
				f1->name,
				(unsigned int) f1->start,
				f1->device == FL_EXT ? "(ext flash)" :
				"");
		} else {
			 printf("# Valid %s image (active) found at address"
				" 0x%x %s\n",
				f1->name,
				(unsigned int) f1->start,
				f1->device == FL_EXT ? "(ext flash)" :
				"");
		 }
	}

	index = 0;
	f1 = part_get_layout_by_id(FC_COMP_FTFS, &index);
	f2 = part_get_layout_by_id(FC_COMP_FTFS, &index);
	if (f1 != NULL && f2 != NULL)
		/* Select high generation level image */
		f = (f1->gen_level >= f2->gen_level ? f1 : f2);
	else
		f = f1;

	if (f1 != NULL) {
		fl_dev_read(f1->device, f1->start, ftfs_sig, sizeof(ftfs_sig));
		if (!ft_is_valid_magic(ftfs_sig)) {
			printf("# Warning: Valid %s image not found at"
				" address 0x%x %s\n",
				f1->name,
				(unsigned int) f1->start,
				f1->device == FL_EXT ? "(ext flash)" :
				"");
		} else {
			 printf("# Valid %s image (active) found at address"
				" 0x%x %s\n",
				f1->name,
				(unsigned int) f1->start,
				f1->device == FL_EXT ? "(ext flash)" :
				"");
		 }
	}
}
