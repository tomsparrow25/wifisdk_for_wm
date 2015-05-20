/* 
 *  Copyright 2008-2013, Marvell International Ltd.
 *  All Rights Reserved.
 */

#ifndef _APP_FS_H_
#define _APP_FS_H_

#include <ftfs.h>

/* Init function for app file system module */
int app_fs_init(int fs_ver, const char *part_name);

/* Accessor for application file system */
struct fs *app_fs_get();

/* Check if FS initialization has failed */
uint8_t app_fs_is_failed(void);

/* Get the current passive FTFS partition */
struct partition_entry *app_fs_get_passive();

#endif
