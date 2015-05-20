/*
 *  Copyright 2008-2013, Marvell International Ltd.
 *  All Rights Reserved.
 */

/* app_psm.c: This file takes care of maintaining relevant information about the
 * device into psm. This includes, the device name and epoch.
 */

#include <string.h>
#include <stdlib.h>

#include <wmstdio.h>
#include <wm_utils.h>
#include <psm.h>
#include <partition.h>
#include <app_framework.h>

#include "app_psm.h"
#include "app_dbg.h"

/* Every device has a name associated with it. This name is stored within the
 * psm. This function retrieves this name.
 */
int app_get_device_name(char *dev_name, int len)
{
	int ret;
	psm_handle_t handle;

	ret = psm_open(&handle, SYS_MOD_NAME);
	if (ret != 0) {
		app_e("app_get_device_name: Failed to open psm"
			" module. Error: %d", ret);
		app_psm_handle_partition_error(ret, SYS_MOD_NAME);
		return ret;
	}
	psm_get(&handle, VAR_SYS_DEVICE_NAME, dev_name, len);
	psm_close(&handle);
	return WM_SUCCESS;
}

/* Every device has a name associated with it. This name is stored within the
 * psm. This function sets a new name within the device.
 */
int app_set_device_name(char *dev_name)
{
	int ret;
	psm_handle_t handle;

	ret = psm_open(&handle, SYS_MOD_NAME);
	if (ret != 0) {
		app_e("app_set_device_name: Failed to open psm"
			" module. Error: %d", ret);
		app_psm_handle_partition_error(ret, SYS_MOD_NAME);
		return ret;
	}

	if (dev_name)
		psm_set(&handle, VAR_SYS_DEVICE_NAME, dev_name);

	psm_close(&handle);
	return WM_SUCCESS;
}

/* This function is specifically required to initialize the device name if it
 * isn't already set.
 */
int app_init_device_name(char *dev_name)
{
	char tmp[64];

	app_get_device_name(tmp, sizeof(tmp));
	if (strlen(tmp) == 0) {
		/* Device name does not exists, lets initialize in psm */
		app_set_device_name(dev_name);
	}
	return WM_SUCCESS;
}

/* Every device has a alias associated with it. This alias is stored within the
 * psm. This function retrieves this alias.
 */
int app_get_device_alias(char *alias, int len)
{
	int ret;
	psm_handle_t handle;

	ret = psm_open(&handle, SYS_MOD_NAME);
	if (ret != 0) {
		app_e("app_get_device_alias: Failed to open psm"
			" module. Error: %d", ret);
		app_psm_handle_partition_error(ret, SYS_MOD_NAME);
		return ret;
	}
	psm_get(&handle, VAR_SYS_DEVICE_ALIAS, alias, len);
	psm_close(&handle);
	return WM_SUCCESS;
}

/* Every device has a alias associated with it. This alias is stored within the
 * psm. This function sets a new alias within the device.
 */
int app_set_device_alias(char *alias)
{
	int ret;
	psm_handle_t handle;

	ret = psm_open(&handle, SYS_MOD_NAME);
	if (ret != 0) {
		app_e("app_set_device_alias: Failed to open psm"
			" module. Error: %d", ret);
		app_psm_handle_partition_error(ret, SYS_MOD_NAME);
		return ret;
	}

	if (alias)
		psm_set(&handle, VAR_SYS_DEVICE_ALIAS, alias);

	psm_close(&handle);
	return WM_SUCCESS;
}

/* This function is specifically required to initialize the alias if it
 * isn't already set.
 */
int app_init_device_alias(char *alias)
{
	char tmp[64];

	app_get_device_alias(tmp, sizeof(tmp));
	if (strlen(tmp) == 0) {
		/* Device name does not exists, lets initialize in psm */
		app_set_device_alias(alias);
	}
	return WM_SUCCESS;
}

/* Count number of bootup and store into psm */
int app_psm_get_epoch(int inc_flag)
{
	psm_handle_t handle;
	int ret, modified = 0;
	unsigned int val;
	char temp[100];

	ret = psm_open(&handle, NETWORK_MOD_NAME);
	if (ret != 0) {
		app_e("app_network_get_epoch: Failed to open psm"
			" module. Error: %d", ret);
		app_psm_handle_partition_error(ret, NETWORK_MOD_NAME);
		return ret;
	}

	/* Read current epoch value */
	psm_get(&handle, VAR_NET_EPOCH, temp, sizeof(temp));
	if (strlen(temp) == 0) {
		/* First time. Set epoch to zero */
		val = 1;
		modified = 1;
	} else {
		val = atoi(temp);
		/* Increment epoch value */
		if (inc_flag) {
			val++;
			modified = 1;
		}
	}

	/* Set the new value, if it changed */
	if (modified) {
		snprintf(temp, sizeof(temp), "%d", val);
		psm_set(&handle, VAR_NET_EPOCH, temp);
	}

	psm_close(&handle);
	return val;
}

int app_psm_init(void)
{
	int ret;
	struct partition_entry *p;
	flash_desc_t fl;

	ret = part_init();
	if (ret != WM_SUCCESS) {
		app_e("app_psm: could not read partition table");
		return ret;
	}
	p = part_get_layout_by_id(FC_COMP_PSM, NULL);
	if (!p) {
		app_e("app_psm: no psm partition found");
		return WM_FAIL;
	}

	part_to_flash_desc(p, &fl);
	ret = psm_init(&fl);
	if (ret) {
		app_e("app_psm: failed to init PSM,"
			"status code %d", ret);
		return WM_FAIL;
	}

	ret = psm_register_module(NETWORK_MOD_NAME,
					COMMON_PARTITION, PSM_CREAT);
	if (ret != WM_SUCCESS && ret != -WM_E_EXIST) {
		app_e("app_psm: failed to register network"
			" module with psm");
		return WM_FAIL;
	}

	ret = psm_register_module(SYS_MOD_NAME, COMMON_PARTITION, PSM_CREAT);
	if (ret != WM_SUCCESS && ret != -WM_E_EXIST) {
		app_e("app_network_config: failed to register sys module"
			" with psm");
		return WM_FAIL;
	}

	ret = psm_register_module(SCAN_CONFIG_MOD_NAME,
				  COMMON_PARTITION, PSM_CREAT);
	if (ret != WM_SUCCESS && ret != -WM_E_EXIST) {
		app_e("app_network_config: failed to register scan_config"
			" module with psm");
		return WM_FAIL;
	}

	/* Increment epoch(boot count) and set into psm */
	sys_set_epoch(app_psm_get_epoch(1));

	return WM_SUCCESS;
}

void app_psm_handle_partition_error(int errno, const char *module_name)
{
	short partition_id;
	partition_id = psm_get_partition_id(module_name);
	if (partition_id >= 0) {
		switch (errno) {
		/* Right now we are handling only bad crc errors
		 * Any new errors that need to be handled can be added over
		 * here. */
		case -WM_E_PSM_METADATA_CRC:
			psm_e("Incorrect crc of psm metadata");
			wmprintf("Erasing entire psm contents\r\n");
			psm_erase_and_init();
			break;
		case -WM_E_CRC:
			psm_e("Incorrect crc of psm partition %d",
				partition_id);
			wmprintf("Erasing psm partiton %d", partition_id);
			psm_erase_partition(partition_id);
			break;
		}
	}
}
