/*
 *  Copyright 2008-2013, Marvell International Ltd.
 *  All Rights Reserved.
 */

#ifndef _APP_PSM_H
#define _APP_PSM_H

#define SYS_MOD_NAME		"sys"
#define SCAN_CONFIG_MOD_NAME	"scan_config"
#define NETWORK_MOD_NAME        "network"

#define VAR_SYS_DEVICE_NAME	"name"
#define VAR_SYS_DEVICE_ALIAS	"alias"
#define VAR_NET_EPOCH           "epoch"


/** Init function for applications psm module */
int app_psm_init(void);

/** Modifier/Accessor to get/set epoch(bootup count). The epoch is incremented
 * before returning, if inc_flag is set.
 */
int app_psm_get_epoch(int inc_flag);


/** Initialize Device alias in psm */
int app_init_device_alias(char *alias);

/** Get current device alias */
int app_get_device_alias(char *alias, int len);

/** Set device alias */
int app_set_device_alias(char *alias);

/** Handle psm partition error in case of corruption */
void app_psm_handle_partition_error(int errno, const char *module_name);

#endif /* ! _APP_PSM_H */
