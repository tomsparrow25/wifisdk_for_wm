/*
 * Copyright 2008-2013, Marvell International Ltd.
 * All Rights Reserved.
 */

#ifndef __boot_flags_h
#define __boot_flags_h

#include <wmtypes.h>
#include <mc200_pmu.h>

/* BooROM version for A0 chip */
#define BOOTROM_A0_VERSION 3

/*
 * Location where the boot flags are stored by the bootloader.
 * Currently this is stored in NVRAM area after BootROM occupied range.
 */
#define BOOT_FLAGS		((uint32_t *)0x480C0024)

/* final location for those boot flags */
extern uint32_t g_boot_flags;
extern uint32_t g_prev_version_fw;
extern uint32_t g_rst_cause;

/* boot flag definitions */
#define BOOT_PARTITION_MASK		0x03	/* partition number used to boot */
#define BOOT_PARTITION_TABLE_MASK	0x100	/* boot partition table */
#define PARTITION_TABLE1_STATE_MASK	0x200   /* partition table1 state */
#define PARTITION_TABLE2_STATE_MASK	0x400   /* partition table2 state */
#define BOOT_BACKUP_FIRMWARE_FORCED	0x10	/* from manual override */
#define BOOT_MAIN_FIRMWARE_BAD_CRC	0x20	/* main firmware had errors */

/*
 * Override condition delay is returned as milisecs in the top 16 bits.
 * If the bootloader detects this override condition for more than
 * 8 seconds then the bootloader selects the backup firmware and sets
 * the BOOT_BACKUP_FIRMWARE_FORCED bit, in which case the reported delay
 * will be 0.
 */
#define BOOT_OVR_COND_DELAY	(g_boot_flags >> 16)

/*
 * Bootrom stores some information in initial few bytes of retention ram.
 * Following is the format used for storing this info.
 */
struct bootrom_info {
	uint8_t powermode;
	uint8_t pinstrap;
	uint8_t bootsource;
	uint8_t bootflag;
	uint32_t reserved;
	uint32_t pm3_entryaddr;
	uint32_t main_enteryaddr;
	uint32_t crystal_freq;
	uint32_t system_clk;
	uint32_t version;
	uint32_t reserved1;
};

extern unsigned long _nvram_start;

static inline int boot_get_partition_no()
{
	return g_boot_flags & BOOT_PARTITION_MASK;
}

static inline int boot_old_fw_version()
{
	return g_prev_version_fw;
}

static inline int boot_factory_reset()
{
	/*
	 * Check and execute Reset to Factory Action
	 */
	if ((BOOT_OVR_COND_DELAY / 1000) >= 5)
		return 1;
	else
		return 0;
}

static inline uint32_t boot_store_reset_cause()
{
	uint32_t rst_cause =  PMU_GetLastResetCause();
	PMU_ClrLastResetCause(PMU_RESETSRC_ALL);
	return rst_cause;
}

static inline uint32_t boot_reset_cause()
{
	return g_rst_cause;
}

void boot_init();


#endif
