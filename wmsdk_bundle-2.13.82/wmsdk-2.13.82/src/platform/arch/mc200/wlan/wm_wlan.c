/*
 *  Copyright 2008-2013, Marvell International Ltd.
 *  All Rights Reserved.
 */

#include <wlan.h>
#include <wmstdio.h>
#include <cli.h>
#include <healthmon.h>
#include <wmtime.h>
#include <arch/boot_flags.h>
#include <partition.h>

#define REVID_MASK 0x1FF

#define boot_l(...)                              \
	wmlog("boot", ##__VA_ARGS__)
#define init_e(...)                              \
	wmlog_e("init", ##__VA_ARGS__)


static void boot_report_flags(void)
{
	struct bootrom_info *info = (struct bootrom_info *)&_nvram_start;

	boot_l("SDK-%s gcc-%s (%s %s)", SDK_VERSION,
			__VERSION__, __DATE__, __TIME__);
	boot_l("Boot Flags: 0x%x", g_boot_flags);

	boot_l(" - Partition Table: %d ",
			!(!(g_boot_flags & BOOT_PARTITION_TABLE_MASK)));

	boot_l(" - Firmware Partition: %d ",
		       g_boot_flags & BOOT_PARTITION_MASK);
	if (g_boot_flags & BOOT_BACKUP_FIRMWARE_FORCED) {
		boot_l(" - Backup firmware manually forced");
	} else if (g_boot_flags & BOOT_MAIN_FIRMWARE_BAD_CRC) {
		boot_l(" - Backup firmware due to CRC error in main"
				" firmware");
	} else {
		boot_l(" - Main firmware");
		boot_l(" - Boot Override Time: %d.%03d sec",
			       BOOT_OVR_COND_DELAY / 1000,
			       BOOT_OVR_COND_DELAY % 1000);
	}

	/* Bits of register SYS_CTRL->REV_ID can be described
	 * as follows:
	 *             [31:24]         Company_ID
	 *             [23:20]         Foundry
	 *             [19:16]         Process
	 *             [15:14]         Project_ID
	 *             [8:6]           Project_Rev_first_part
	 *             [5:3]           Project_Rev_second_part
	 *             [2:0]           Project_Rev_third_part
	 */
	boot_l("Boot Info:");
#ifdef CONFIG_ENABLE_WARNING_LOGS
	volatile uint32_t revid = SYS_CTRL->REV_ID.WORDVAL;
	boot_l(" - Chip revision id: 0x%x", (revid & REVID_MASK));
#endif /* CONFIG_ENABLE_WARNING_LOGS*/
	boot_l(" - Bootrom version: %u", info->version);
	boot_l(" - Boot option: %s",
			info->pinstrap ? "flash" : "uart");
	if ((info->bootflag) & 0x2)
		boot_l(" - Lower 32K flash protected");
}

int wm_wlan_init()
{
	static char init_done;
	struct partition_entry *p;
	short history = 0;
	struct partition_entry *f1, *f2;

	if (init_done)
		return WM_SUCCESS;

	int ret = part_init();
	if (ret != WM_SUCCESS) {
		init_e("wm_wlan_init: could not read partition table");
		return ret;
	}
	f1 = part_get_layout_by_id(FC_COMP_WLAN_FW, &history);
	f2 = part_get_layout_by_id(FC_COMP_WLAN_FW, &history);

	if (f1 && f2)
		p = part_get_active_partition(f1, f2);
	else if (!f1 && f2)
		p = f2;
	else if (!f2 && f1)
		p = f1;
	else
		return -WLAN_ERROR_FW_NOT_DETECTED;

	flash_desc_t fl;
	part_to_flash_desc(p, &fl);

	/* Initialize wlan */
	ret = wlan_init(&fl);
	if (ret != WM_SUCCESS)
		return ret;

	init_done = 1;
	return WM_SUCCESS;
}

int wm_core_init(void)
{
	int ret = 0;
	static char init_done;

	if (init_done)
		return WM_SUCCESS;

	wmstdio_init(UART0_ID, 0);

	ret = cli_init();
	if (ret != WM_SUCCESS) {
		init_e("Cli init failed.");
		goto out;
	}

	ret = wmtime_init();
	if (ret != WM_SUCCESS) {
		init_e("Wmtime init failed.");
		goto out;
	}

	ret = pm_init();
	if (ret != WM_SUCCESS) {
		init_e("Power manager init failed.");
		goto out;
	}

	ret = healthmon_init();
	if (ret != WM_SUCCESS) {
		init_e("Healthmon init failed.");
		goto out;
	}

	boot_report_flags();

	wmlog("boot", "Reset Cause Register: 0x%x",
	      boot_reset_cause());
	if (boot_reset_cause() & (1<<5))
		wmlog("boot", " - Watchdog reset bit is set");

	/* Read partition table layout from flash */
	part_init();
out:
	if (ret == WM_SUCCESS)
		init_done = 1;
	return ret;
}

int wm_core_and_wlan_init()
{
	int ret = wm_core_init();
	if (ret != WM_SUCCESS) {
		init_e("Error: Core init failed");
		return ret;
	}

	ret = wm_wlan_init();
	if (ret != WM_SUCCESS)
		init_e("Error: Unable to initialize wlan");
	return ret;
}

