/*
 *  Copyright 2008-2013, Marvell International Ltd.
 *  All Rights Reserved.
 */

/* app_fs.c: This files contains code related to the FS modules that are used by
 * the application framework.
 */
#include <wmstdio.h>
#include <fs.h>
#include <ftfs.h>
#include <rfget.h>
#include <partition.h>
#include <app_framework.h>

#include "app_dbg.h"
#include "app_fs.h"

static struct fs *fs = 0;

/* Flag to indicate failure of file system */
static uint8_t fs_failed;
static struct ftfs_super rfget_ftfs;
static struct partition_entry *app_ftfs_passive;


/* Initialise the filesystem that will be used by the webserver */
int app_fs_init(int fs_ver, const char *name)
{
	fs = rfget_ftfs_init(&rfget_ftfs, fs_ver, name, &app_ftfs_passive);
	/* Unable to get any file system. Return failure */
	if (!fs) {
		app_e("fs: failed to init FTFS");
		/* Make a note of whether filesystem initialization has
		 * failed.
		 */
		fs_failed = 1;
		return -WM_FAIL;
	}

	return WM_SUCCESS;
}

/* Used to get the filesystem handle for the filesystem in use. */
struct fs *app_fs_get()
{
	return fs;
}

struct partition_entry *app_fs_get_passive()
{
	return app_ftfs_passive;
}
/* We make a note of whether the filesystem initialization failed or not. We
 * allow the application framework to continue even if there is no FS or the FS
 * has been corrupted.
 *
 * This function can then be used to determine if the file system initialization
 * has failed, and if so report it somewhere. Typically, the cloud thread
 * reports this incident to the cloud, so that a new filesystem can be
 * upgraded.
 *
 */
uint8_t app_fs_is_failed(void)
{
	return fs_failed;
}
