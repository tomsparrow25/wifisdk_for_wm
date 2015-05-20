/*
 *  Copyright (C) 2011-2013, Marvell International Ltd.
 *  All Rights Reserved.
 */

/**
 *  @brief This is a second stage boot loader for the mc200
 *  It provides below features-
 *  -- Reading partition table information and execute the
 *     latest programmed firmware
 *  -- Boot from backup firmware
 *  -- BootOverride Delay calculation
 *  -- Passing boot flags to firmware for appropriate actions
 */

#include <stdint.h>
#include <stdio.h>

#include "boot2.h"

#include <crc32.h>
#include <flash.h>
#include <partition.h>
#include <mc200_flash.h>
#include <arch/flash_layout.h>
#include <arch/boot_flags.h>
#include <mc200_gpt.h>
#include <mc200_clock.h>
#include <board.h>
#include <firmware_structure.h>

#define DEFAULT_FIRMWARE_OFFSET (0x10000)
#define DEFAULT_FIRMWARE_SIZE (0x10000*6)

/*
 * Boot override delay (in milisecs)
 */
#ifndef OVERRIDE_BACKUP_FIRMWARE_DELAY
#define OVERRIDE_BACKUP_FIRMWARE_DELAY	(8 * 1000) /* 8 seconds */
#endif

/* Make the "MRVL" string into an endian invariant 32-bit constant */
#if defined(__ARMEL__)
#define FW_MAGIC_STR		(('M' << 0)|('R' << 8)|('V' << 16)|('L' << 24))
#elif defined(__ARMEB__)
#define FW_MAGIC_STR		(('M' << 24)|('R' << 16)|('V' << 8)|('L' << 0))
#else
#error "unknown endian mode"
#endif

#define FAIL			1	/* non-zero return = failure */

/**
 * read_flash : Reads flash memory
 * @buf : pointer to butter in which flash content to be read
 * @addr: Flash read address
 * @size : no of bytes to be read
 * @return value : calculated crc
 */
static uint32_t read_flash(void *buf, uint32_t addr, uint32_t size,
			   uint32_t crc)
{
	unsigned int i;
	unsigned char *ptr, *adr;
	ptr = buf;
	adr = (void *)addr;
	FLASH_Read(FLASH_FAST_READ_QUAD_IO, addr, (unsigned char *)buf, size);
	crc = crc32(buf, size, crc);

	return crc;
}

static int load_partition_table(struct partition_table *ph,
				  uint32_t addr)
{
	int ret = 0;
	uint32_t crc = read_flash(ph, addr, sizeof(*ph), 0);

	if (crc != 0 || ph->magic != PARTITION_TABLE_MAGIC) {
		if (crc != 0)
			writel((readel(nvram_addr) | BAD_PART_TABLE_CRC),
								nvram_addr);
		else
			writel((readel(nvram_addr) | BAD_PART_TABLE_MAGIC),
								nvram_addr);
		/* Invalidate number of partition entries */
		ph->partition_entries_no = 0;
		/* Invalidate generation level */
		ph->gen_level = 0;

		return -1;
	}
	return 0;
}

static int select_active_partition_table(uint32_t *addr)
{
	int ret, status;
	struct partition_table ph[2];

	status = 0;
	ret = load_partition_table(&ph[0], FL_PART1_START);
	if (ret != 0)
		/* Set partition table 1 status to indicate corruption */
		status |= PARTITION_TABLE1_STATE_MASK;

	ret = load_partition_table(&ph[1], FL_PART2_START);
	if (ret != 0)
		/* Set partition table 2 status to indicate corruption */
		status |= PARTITION_TABLE2_STATE_MASK;

	if (ph[1].gen_level > ph[0].gen_level) {
		*addr = FL_PART2_START;
		status |= BOOT_PARTITION_TABLE_MASK;
		return status;
	} else {
		*addr = FL_PART1_START;
		return status;
	}
}

static void load_partition_entry(struct partition_table *pt,
		struct partition_entry *ph,
		uint32_t addr, short *start_index)
{
	int i = 0;
	if (start_index)
		i = *start_index;

	for (; i < pt->partition_entries_no; i++) {
		read_flash(ph, addr, sizeof(*ph), 0);
		addr += sizeof(*ph);
		if (ph->type == FC_COMP_FW)
			break;
	}
	*start_index = i + 1;
	if (ph->type != FC_COMP_FW)
		/* Invalidate partition generation level */
		ph->gen_level = 0;
}

static void read_from_seghdr(struct firmware_segment *segment, void *addr,
								uint32_t size)
{
	while (size) {
		uint32_t length = segment->length;
		if (!length)
			return;
		if (length > size)
			length = size;
		read_flash(addr, segment->offset, length, 0);
		segment->offset += length;
		segment->length -= length;
		addr += length;
		size -= length;
	}

	return;
}

static int load_firmware(uint32_t firm_addr, uint32_t firm_size)
{
	struct firmware_segment segment;
	struct img_hdr ih;
	struct seg_hdr sh;
	uint32_t value, seg_cnt;
	uint32_t *data_ptr;
	int (*fn_ptr) (uint32_t);

	segment.offset = firm_addr;
	segment.length = firm_size;

	/* load and validate image header */
	read_from_seghdr(&segment, &ih, sizeof(ih));
	if (ih.magic_str != FW_MAGIC_STR ||
				ih.magic_sig != FW_MAGIC_SIG) {
		writel((readel(nvram_addr) | BAD_FIRMWARE_SIG), nvram_addr);
		return FAIL;
	}

	while (ih.seg_cnt--) {
		read_from_seghdr(&segment, &sh, sizeof(sh));
		sh.offset += firm_addr;

		switch (sh.type) {
		case FW_BLK_LOADABLE_SEGMENT:
			if ((sh.laddr & 0x1f000000) == 0x1f000000)
				break;
			if (read_flash((void *)sh.laddr, sh.offset, sh.len,
								sh.crc))
				return FAIL;
			break;

		case FW_BLK_POKE_DATA:
			data_ptr = (void *)sh.laddr;
			value = sh.len;
			*data_ptr = value;
			break;

		case FW_BLK_FN_CALL:
			writel((readel(nvram_addr) | BOOT_SUCCESS),
							nvram_addr);
			fn_ptr = (void *)sh.laddr;
			value = sh.len;
			if (fn_ptr(value) != 0)
				return FAIL;
			break;

		case FW_BLK_ANCILLARY:
			/* skip over it */
			break;

		default:
			return FAIL;
		}
	}

	if ((ih.entry & 0x1f000000) == 0x1f000000) {
		/* Enable Flash Controller */
		PMU->PERI1_CLK_DIV.BF.FLASH_CLK_DIV = 4;
		PMU->PERI_CLK_EN.BF.FLASH_QSPI_CLK_EN = 0;
		FLASHC->FCCR.BF.FLASHC_PAD_EN = 1;
	}

	/* Hand over execution */
	writel((readel(nvram_addr) | BOOT_SUCCESS), nvram_addr);
	fn_ptr = (void *)ih.entry;
	fn_ptr((uint32_t)NULL);
}

/*
 * Note: override condition delay is returned as milisecs in the top 16 bits,
 * or BOOT_BACKUP_FIRMWARE_FORCED if the backup firmware delay threshold
 * has been reached.
 */
static uint32_t test_boot_override(void)
{
	uint32_t t0, t1, result;
	GPT_Config_Type gpt;

	if (!board_button_pressed(board_button_1()))
		return 0;
	/*
	 * Enable timer 0 clock for boot-override calculation, intentionally
	 * avoided using CLK_ModuleClkEnable() because that adds un-necessary
	 * text bloat.
	 */
	PMU->PERI_CLK_EN.BF.GPT0_CLK_EN = 0;

	/* Start hardware timer 0 for timing measurement */
	gpt.clockSrc = GPT_CLOCK_0;
	gpt.cntUpdate = GPT_CNT_VAL_UPDATE_FAST;
	gpt.clockPrescaler = 0x0;
	/* Clock at 32MHz, use divider 2^5 to make it 1MHz */
	gpt.clockDivider = 5;
	gpt.uppVal = 0xffffffff;

	/* Initialize and load timer */
	GPT_Init(GPT0_ID, &gpt);
	/* Clear old status */
	GPT_CounterReset(GPT0_ID);
	/* Start timer operation */
	GPT_Start(GPT0_ID);

	/* Override signal is asserted, so let's see for how long. */
	t0 = (GPT_GetCounterVal(GPT0_ID) / 1000) << 16;
	do {
		t1 = (GPT_GetCounterVal(GPT0_ID) / 1000) << 16;
		result = t1 - t0;
		if (result >= (OVERRIDE_BACKUP_FIRMWARE_DELAY << 16)) {
			/* no point waiting any longer */
			result = BOOT_BACKUP_FIRMWARE_FORCED;
			goto done;
		}
	} while (board_button_pressed(board_button_1()));

done:
	/* Stop timer and disable clock */
	GPT_Stop(GPT0_ID);
	PMU->PERI_CLK_EN.BF.GPT0_CLK_EN = 1;

	return result;
}

int boot2_main()
{
	int ret;
	short index;
	bool p_num;
	uint32_t boot_flags;
	struct partition_entry p_entry[2];
	struct partition_table p_table;
	uint32_t part_addr;

	boot_flags = test_boot_override();
	dbg_printf("bootoverride value %08x\r\n", boot_flags);
	dbg_printf("bootoverride time %d.%d secs\r\n", boot_flags / 1000,
		   boot_flags % 1000);

	/* Select active partition table */
	ret = select_active_partition_table(&part_addr);
	boot_flags |= ret;

	/* Read out partition table */
	load_partition_table(&p_table, part_addr);
	part_addr += sizeof(struct partition_table);

	/* Read out firmware partition entries */
	if (p_table.partition_entries_no > 0) {
		index = 0;
		load_partition_entry(&p_table, &p_entry[0], part_addr, &index);
		part_addr += sizeof(struct partition_entry) * index;
		load_partition_entry(&p_table, &p_entry[1], part_addr, &index);

		dbg_printf("gen levno: %d,%d\r\n",
			   p_entry[0].gen_level, p_entry[1].gen_level);

		/* select partition with highest generation number */
		p_num = (p_entry[1].gen_level > p_entry[0].gen_level);
	} else {
		/* No partition entries found, load default */
		p_num = 0;
		p_entry[0].start = DEFAULT_FIRMWARE_OFFSET;
		p_entry[0].size = DEFAULT_FIRMWARE_SIZE;
		p_entry[0].gen_level = 1;
	}

	if (!(boot_flags & BOOT_BACKUP_FIRMWARE_FORCED)) {
		/* store boot flags in memory for firmware consumption */
		*BOOT_FLAGS = boot_flags | p_num;

		writel((readel(nvram_addr) | PRIMARY_FIRMWARE), nvram_addr);

		if (p_entry[p_num].gen_level >= 1)
			load_firmware(p_entry[p_num].start,
					p_entry[p_num].size);

		/* we get here only if selected partition/firmware is bad */
		boot_flags |= BOOT_MAIN_FIRMWARE_BAD_CRC;
		writel((readel(nvram_addr) | BAD_FIRMWARE_IMG), nvram_addr);
	}

	/* Backup firmware: partition selection has to be flipped */
	p_num ^= 1;

	/* store boot flags in memory for firmware consumption */
	*BOOT_FLAGS = boot_flags | p_num;

	if (p_entry[p_num].gen_level >= 1)
		load_firmware(p_entry[p_num].start, p_entry[p_num].size);

	writel((readel(nvram_addr) | BOOT_FAILED), nvram_addr);
	dbg_printf("fw-boot failed boot_flags(%08x)\r\n", boot_flags);
	/* Nothing succeeded. We're toast... */
	for (;;)
		;
}
