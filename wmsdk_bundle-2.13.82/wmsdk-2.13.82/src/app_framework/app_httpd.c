/*
 *  Copyright 2008-2013,2012 Marvell International Ltd.
 *  All Rights Reserved.
 */

/* app_httpd.c: This file contains functions that are related to the starting
 * and stopping of the httpd service.
 */

#include <wmstdio.h>
#include <httpd.h>
#include <fs.h>
#include <app_framework.h>
#include <diagnostics.h>

#include "app_http.h"
#include "app_ctrl.h"
#include "app_fs.h"
#include "app_dbg.h"
#include "app_test_http_handlers.h"

static bool is_httpd_init;
static bool is_handlers_registered;
/* Start the HTTP Server */
static int __app_httpd_start()
{
	int ret;

	app_d("app_httpd: initializing web-services");

	/* Initialize HTTPD */
	if (is_httpd_init == false) {
		ret = httpd_init();
		if (ret == -WM_FAIL) {
			app_e("app_httpd: failed to initialize httpd");
			return ret;
		}
		is_httpd_init = true;
	}
	/* Start httpd thread */
	ret = httpd_start();
	if (ret == -WM_FAIL) {
		app_e("app_httpd: failed to start httpd thread");
		httpd_shutdown();
	}

	return ret;
}

int app_httpd_start()
{
	int ret;

	ret = __app_httpd_start();
	if (ret != WM_SUCCESS)
		return ret;
	if (is_handlers_registered == false) {
		app_sys_register_sys_handler();
		app_sys_normal_set_network_cb();
		app_test_http_register_handlers();
		is_handlers_registered = true;
	}

	return WM_SUCCESS;
}

int app_httpd_with_fs_start(int fs_ver, const char *part_name, struct fs **fs)
{
	int ret;

	if (app_fs_get() == NULL) {
		/* If FS is not already initialized, init it. */
		app_fs_init(fs_ver, part_name);
	}

	if (fs != NULL) {
		*fs = app_fs_get();
	}

	ret = __app_httpd_start();
	if (ret != WM_SUCCESS)
		return ret;

	if (is_handlers_registered == false) {

		/* Also register the file handler, if init is successful */
		if (app_fs_get())
			app_sys_register_file_handler();

		/* Register the sys handlers */
		app_sys_register_sys_handler();
		app_sys_normal_set_network_cb();
		app_test_http_register_handlers();
		is_handlers_registered = true;
	}

	return WM_SUCCESS;
}

int app_httpd_stop()
{
	/* HTTPD and services */
	app_d("app_httpd: stopping down httpd");
	if (httpd_stop() != WM_SUCCESS)
		app_e("app_httpd: Failed to halt httpd");

	return WM_SUCCESS;
}

