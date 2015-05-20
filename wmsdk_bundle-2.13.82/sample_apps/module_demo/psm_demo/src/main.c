/*
 *  Copyright (C) 2008-2013, Marvell International Ltd.
 *  All Rights Reserved.
 */

/* Application that demonstrates Persistent Storage Manager (PSM) usage
 *
 * Summary:
 *
 * This application maintains a count of how many times the application was
 * booted up. This count is maintained in PSM.
 *
 * Description:
 *
 * On bootup,
 *  - the application identifies the partition that stores the PSM data
 *  - uses this information to initialize the PSM
 *  - registers a module 'bootinfo' with the PSM, if not already present
 *  - reads and prints the variable 'bootcount' from this partition
 *  - increments the value of 'bootcount' by 1 and writes the new value to PSM
 *
 * Since the value is written to PSM, the value is maintained across system
 * reboot events.
 */
#include <stdlib.h>

#include <wmstdio.h>
#include <wm_os.h>
#include <psm.h>
#include <cli.h>
#include <partition.h>

#define STR_BUF_SZ 10

/* This is an entry point for the application.
   All application specific initialization is performed here. */
int main(void)
{
	int bootcount = 0;
	int ret;
	psm_handle_t handle;
	char strval[STR_BUF_SZ] = { '\0' };
	struct partition_entry *p;
	flash_desc_t fl;

	/* Initializes console on uart0 */
	ret = wmstdio_init(UART0_ID, 0);
	if (ret == -WM_FAIL) {
		wmprintf("Failed to initialize console on uart0\r\n");
		return -1;
	}

	/* Initialize the command line interface module */
	ret = cli_init();
	if (ret == -WM_FAIL) {
		wmprintf("Failed to initialize cli\r\n");
		return -1;
	}

	/* Initialize the partition module */
	ret = part_init();
	if (ret != 0) {
		wmprintf("Failed to initialize partition\r\n");
		return -1;
	}
	p = part_get_layout_by_id(FC_COMP_PSM, NULL);
	if (!p) {
		wmprintf("Error: no psm partition found");
		return WM_FAIL;
	}
	part_to_flash_desc(p, &fl);

	/* Initilize psm module */
	ret = psm_init(&fl);
	if (ret != 0) {
		wmprintf("Failed to initialize psm module\r\n");
		return -1;
	}

	/* Initilize cli for psm module */
	ret = psm_cli_init();
	if (ret != 0) {
		wmprintf("Failed to register psm-cli commands\r\n");
		return -1;
	}

	wmprintf("PSM Demo application Started\r\n");

	/* Create a bootinfo module in a PSM partition with key =
	 * part_sysinfo */
	ret = psm_register_module("bootinfo", "part_sysinfo", PSM_CREAT);
	if (ret && (ret != -WM_E_EXIST)) {
		wmprintf("Error creating PSM module\r\n");
		return 1;
	}
	/* Open handle for specific PSM module name */
	ret = psm_open(&handle, "bootinfo");
	if (ret) {
		wmprintf("Error opening PSM module\r\n");
		return 1;
	}

	/* This functions retrieves the value of a variable from psm */
	ret = psm_get(&handle, "bootcount", strval, 10);
	if (ret) {
		wmprintf("No bootcount value found.\r\n");
		wmprintf("Initializing bootcount to zero.\r\n");
		bootcount = 0;
	} else {
		bootcount = atoi(strval);
		wmprintf("Bootcount = %d found. Incrementing it to %d\r\n", \
				bootcount, bootcount+1);
		bootcount++;
	}

	snprintf(strval, STR_BUF_SZ, "%d", bootcount);

	/* This function sets the value of a variable in psm  */
	ret = psm_set(&handle, "bootcount", strval);
	if (ret) {
		wmprintf("Error writing value in PSM\r\n");
		return 1;
	}

	psm_close(&handle);

	return 0;
}
