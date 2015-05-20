/*
 * Copyright 2008-2013, Marvell International Ltd.
 * All Rights Reserved.
 */

#include <wmstdio.h>
#include <arch/boot_flags.h>
#include <wmlog.h>

/* permanent boot flags storage */
uint32_t g_boot_flags;
uint32_t g_prev_version_fw;
uint32_t g_rst_cause;

void boot_init(void)
{
	/* Read boot flags stored by boot2 */
	g_boot_flags = *BOOT_FLAGS;

	if (g_boot_flags & BOOT_BACKUP_FIRMWARE_FORCED) {
		g_prev_version_fw = 1;
	} else if (g_boot_flags & BOOT_MAIN_FIRMWARE_BAD_CRC) {
		g_prev_version_fw = 1;
	}
	g_rst_cause = boot_store_reset_cause();
}

