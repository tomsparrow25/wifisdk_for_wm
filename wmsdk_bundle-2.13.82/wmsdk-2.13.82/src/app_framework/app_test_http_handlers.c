/*
 *  Copyright 2008-2013, Marvell International Ltd.
 *  All Rights Reserved.
 */

/* app_test_http_handlers.c: This file implements the APIs that are required by
 * the automated test scripts for testing purposes. This file is not built for
 * the production release.
 */


#include <httpd.h>
#include <wm_utils.h>

#include "app_ctrl.h"
#include "app_dbg.h"

#define J_NAME_EPOCH	  	"epoch"
#define J_NAME_PROV_PIN		"prov_pin"

extern struct provision_pin_config pr_pin;

struct httpd_wsgi_call g_app_test_handlers[];
int g_app_test_handlers_no;


/* Retrieve the current epoch of the device */
static int app_test_get_epoch(httpd_request_t *req)
{
	char content[20];
	struct json_str jstr;

	json_str_init(&jstr, content, sizeof(content), 0);
	json_start_object(&jstr);
	json_set_val_int(&jstr, J_NAME_EPOCH, sys_get_epoch());
	json_close_object(&jstr);
	return httpd_send_response(req, HTTP_RES_200, content, strlen(content),
				HTTP_CONTENT_JSON_STR);
}


void app_test_http_register_handlers()
{
	int rc;
	rc = httpd_register_wsgi_handlers(g_app_test_handlers,
				g_app_test_handlers_no);
	if (rc) {
		app_e("test_http: failed to register test web handler");
	}

}

struct httpd_wsgi_call g_app_test_handlers[] = {
	{"/test/epoch", HTTPD_DEFAULT_HDR_FLAGS, 0,
		app_test_get_epoch, NULL, NULL, NULL},
};

int g_app_test_handlers_no = sizeof(g_app_test_handlers) /
		sizeof(struct httpd_wsgi_call);
