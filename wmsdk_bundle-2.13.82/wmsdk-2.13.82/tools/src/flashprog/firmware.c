/*
 * Copyright 2008-2012, Marvell International Ltd.
 * All Rights Reserved.
 */

#include <stdio.h>
#include <stdlib.h>
#include <partition.h>
#include <mc200_flash.h>
#include <mc200_crc.h>
#include "flashprog.h"
#include "firmware.h"
#include "crc32.h"

static uint32_t spi_read_crc(int fl_dev, void *buf, uint32_t addr,
				uint32_t size, uint32_t crc)
{
	fl_dev_read(fl_dev, addr, (u8 *) buf, size);
	return crc32(buf, size, crc);
}

static void read_from_seghdr(int fl_dev, struct firmware_segment *segment,
						void *addr, uint32_t size)
{
	while (size) {
		uint32_t length = segment->length;
		if (!length)
			return;
		if (length > size)
			length = size;
		fl_dev_read(fl_dev, (uint32_t)segment->offset, (u8 *)addr,
								length);
		segment->offset += length;
		segment->length -= length;
		addr += length;
		size -= length;
	}

	return ;
}

static uint32_t check_crc(int fl_dev, uint32_t offset, uint32_t len,
							uint32_t crc)
{
	uint32_t dummy_buf[32];
	uint32_t result = 0, blk = 128;

#ifndef CONFIG_SOFTWARE_CRC32
	CRC_Enable();
	CRC_SetMode(CRC_32_IEEE);
	CRC_SetStreamLen(len);
#endif

	while (len) {
		if (blk > len)
			blk = len;
		fl_dev_read(fl_dev, offset, (u8 *) dummy_buf, blk);
#ifndef CONFIG_SOFTWARE_CRC32
		CRC_Stream_Feed((uint8_t *)dummy_buf, blk);
#else
		result = soft_crc32(dummy_buf, blk, result);
#endif
		len -= blk;
		offset += blk;
	}
#ifndef CONFIG_SOFTWARE_CRC32
	result = CRC_GetResult();
	CRC_Disable();
#endif

	return (result == crc) ? 0 : 1;
}

static char *test_load_firmware(int fl_dev, struct partition_entry *f)
{
	struct firmware_segment segment;
	struct img_hdr ih;
	struct seg_hdr sh;

	segment.offset = f->start;
	segment.length = f->size;

	/* load and validate magic section */
	read_from_seghdr(fl_dev, &segment, &ih, sizeof(ih));
	if ((ih.magic_str != FW_MAGIC_STR) ||
			(ih.magic_sig != FW_MAGIC_SIG))
		return "wrong firmware magic section";

	while (ih.seg_cnt--) {
		read_from_seghdr(fl_dev, &segment, &sh, sizeof(sh));
		sh.offset += f->start;

		switch (sh.type) {
		case FW_BLK_LOADABLE_SEGMENT:
			if (check_crc(fl_dev, sh.offset, sh.len, sh.crc))
				return "bad firmware loadable segment CRC";
			break;

		case FW_BLK_POKE_DATA:
		case FW_BLK_FN_CALL:
		case FW_BLK_ANCILLARY:
			/* do nothing */
			break;

		default:
			return "unknown firmware segment";
		}
	}

	return NULL;
}

int verify_firmware(int fl_dev, uint32_t partition_offset,
		struct partition_entry *pe, uint32_t firmware_size)
{
	struct partition_table ph;
	uint32_t crc;
	char *error = NULL;

	crc = spi_read_crc(fl_dev, &ph, partition_offset, sizeof(ph), 0);
	if (crc != 0) {
		error = "bad firmware partition table header CRC";
	} else if (ph.magic != PARTITION_TABLE_MAGIC) {
		error = "bad firmware partition table header magic";
	} else {
		error = test_load_firmware(fl_dev, pe);
	}
	if (error) {
		fprintf(stderr, "Error: %s\n", error);
		return -1;
	}
	return 0;
}
