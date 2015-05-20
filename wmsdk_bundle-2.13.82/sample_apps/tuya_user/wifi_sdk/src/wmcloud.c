/*
 *  Copyright (C) 2008-2013, Marvell International Ltd.
 *  All Rights Reserved.
 */

/* WMCLOUD Protocol
   (corresponding server implementation can be found in wmcloud.js)

   * Communication Methods.
   wmcloud protocol is based on top of HTTP. The device can work in one of the
   following manner.
   1. Device with periodic post: The device HTTP POSTs the data periodically or
   whenever the application desires. If the cloud server wishes to send any
   requests to the device, they shall be piggy backed on the next periodic
   post done by the device. Such requests may make device to communicate back
   to the cloud server with consecutive POSTs.
   2. Device with long polling: The device HTTP POSTs the data but wmcloud
   server keeps the response pending so that it can send the control commands
   through the response whenever desired. After device receives this response
   it acts on requests if required and POSTs next request immediately.

   Data Format:
   All the communication happens using JSON as data interchange format.
   Most of the requests as well as responses follow following syntax:
   {
	"header": {
	   "uuid": "uuid",
	   "name": "device name",
	   "epoch": epoch,
	   "sequence": sequence,
	   "type": "device type"
	},
	"data": {
	   "cloud": {
		"url":"?",
	   },
	   "sys": {
		"firmware": {
		   "fw_version":"?",
		   "fs_version":"?",
		   "fw_url":"<url>",
		   "fs_url":"<url>"
		},
		"diag" {
		   "diag_live":"?",
		   "diag_history":"?"
		},
		"reboot":1,
		"rssi":"?",
		"time":"?"
	   },
	   "application": {
		"foo":"bar"
	   }
	}
   }
   Notes:
   1. "?" can be incoming value for parameters specifying that cloud wishes to
   query that parameter.
   2. header fields shown are sent from the device to the cloud. Cloud only
   sends "ack":1 in the header of the response.
   3. Only header is mandatory data sub-objects are optional.
   4. application can use data.<application> sub-object as desired to send
   application data to cloud and to receive commands from cloud.
   5. Cloud frontend application too can use the same format (without header)
   to send any data to the device. (e.g. it can send
   { "data": {"sys": {"rssi":"?"} } } to cloud server on /devices/<uuid> to
   query current RSSI of the device with specified UUID.

*/

#include <json.h>
#include <app_framework.h>
#if APPCONFIG_HTTPS_CLOUD
#include "ca_cert_pem.h"
#include <wm-tls.h>
#endif
#include <wm_utils.h>
#include <ctype.h>
#include <httpc.h>
#include <rfget.h>
#include <psm.h>
#include <wmcloud.h>

cloud_t c;
static cloud_sub_state_t sub_state;

void cloud_thread_main(os_thread_arg_t arg);
void cloud_process_cmds(cloud_t *c, struct json_object *obj,
		   struct json_str *jstr, bool *repeat_POST);


/* #define CLOUD_DUMP_DATA */
/* FIXME: The JSON object can be dumped as a tree */
#ifdef CLOUD_DUMP_DATA
void dump_cloud_packet(const char *buffer, const unsigned len)
{
	int i, buff_index = 0;
	/* We will print max 80 chars on one line */
	for (i = 0; buff_index < len; i++) {
		if (i == 80) {
			wmprintf(" ");
			i = 0;
		}
		if (isprint(buffer[buff_index]))
			wmprintf("%c", buffer[buff_index++]);
		else
			wmprintf("-0x%x-", buffer[buff_index++]);
	}
	wmprintf("\r\n");
}
#endif /* CLOUD_DUMP_DATA */

/* Create the header.
 *
 * Fields:
 * UUID: to uniquely identify the device
 * name: the name of the device for easy identification
 * epoch: boot-up count
 * sequence: incremented on every send. This along with epoch is used to avoid
 *    replay attacks.
 */
unsigned cloud_create_hdr(struct json_str *jstr, const cloud_t *c)
{
	json_push_object(jstr, J_NAME_HEADER);

	json_set_val_str(jstr, J_NAME_UUID, c->uuid);
	json_set_val_str(jstr, J_NAME_NAME, c->name);
	json_set_val_int(jstr, J_NAME_EPOCH, sys_get_epoch());
	json_set_val_int(jstr, J_NAME_SEQUENCE, c->sequence);
	if (c->dev_class)
		json_set_val_str(jstr, J_NAME_TYPE, c->dev_class);

	json_pop_object(jstr);

	return WM_SUCCESS;
}

void cloud_process_server_response(cloud_t *c, unsigned len,
				bool *repeat_POST)
{
	struct json_str jstr;
	json_str_init(&jstr, c->send_packet, CLOUD_PACKET_MAXSIZE - 1, 0);
	/*
	 * NULL terminate the packet as this is expected to be a JSON
	 * response. If this was a binary packet this step would be
	 * unnecessary.
	 */
	c->recv_packet[len] = '\0';

	struct json_object obj;

	json_object_init(&obj, c->recv_packet);

	json_start_object(&jstr);
	/*
	 * Prepare the send packet. The packet may not actually be sent if
	 * the cloud_process_cmds (called below) decides so.
	 */
	cloud_create_hdr(&jstr, c);

	json_push_object(&jstr, J_NAME_DATA);
	cloud_process_cmds(c, &obj, &jstr, repeat_POST);
	json_pop_object(&jstr);

	json_close_object(&jstr);

}

int create_transmit_packet(cloud_t *c)
{
	struct json_str jstr;

	json_str_init(&jstr, c->send_packet, CLOUD_PACKET_MAXSIZE - 1, 0);
	json_start_object(&jstr);
	cloud_create_hdr(&jstr, c);
	json_push_object(&jstr, J_NAME_DATA);

	if (c->app_cloud_periodic_post) {
		c->app_cloud_periodic_post(&jstr);
	}

	json_pop_object(&jstr);
	json_close_object(&jstr);

	return strlen(c->send_packet);
}

/* Note:
 * Cloud shutdown may take time as cloud thread may be busy or waiting for
 * response from cloud server. App ctrl makes sure that no further events
 * are sent during this period by blocking on cloud shutdown call.
 * Hence there is no race condition between cloud shutdown and start events
 * (e.g. in case of abrupt link loss for short duration).
 */
static int cloud_halt(cloud_t *c)
{
	int status;
	/* Values of time are in millisecs */
	int total_wait_time = (DEFAULT_CLOUD_SOCKET_TIMEOUT + 1) * 1000;
	int check_interval = 200;
	int num_iterations = total_wait_time / check_interval;

	/* Set stop_request and wait for cloud thread to acknowledge it */
	c->stop_request = true;
	cl_dbg("Sent cloud shutdown request. Current State: %d",
		c->state);

	/* worst case wait  = cloud post + eventlog request time */
	while (c->stop_request == true  && --num_iterations) {
		os_thread_sleep(os_msec_to_ticks(check_interval));
	}

	c->stop_request = false;

	/* Delete the thread */
	status = os_thread_delete(&c->thread_hnd);
	if (status) {
		cl_dbg("Failed to delete cloud thread: %d", status);
		return -WM_FAIL;
	}
	cl_dbg("Cloud thread deleted");
	/*
	 * Allow the idle thread to clear cloud thread's stack
	 * since actual thread resources are deallocated in idle
	 * thread
	 */
	os_thread_sleep(os_msec_to_ticks(50));

	return WM_SUCCESS;
}

static int is_cloud_enabled(bool *enabled)
{
	*enabled = false;
	char str_enabled[2];
	int rv = psm_get_single(CLOUD_MOD_NAME, J_NAME_ENABLED,
				str_enabled, sizeof(str_enabled));
	if (rv != WM_SUCCESS) {
		*enabled = false;
		return rv;
	}

	if (!strncmp(str_enabled, "1", sizeof(str_enabled)))
		*enabled = true;
	else
		*enabled = false;

	return WM_SUCCESS;
}

static int cloud_activate(cloud_t *c)
{
	int status;
	os_thread_stack_define(cloud_stack, c->stack_size);

	cloud_params_load(c);

	status = os_thread_create(&c->thread_hnd, "cloud_thread",
				  cloud_thread_main, 0, &cloud_stack,
				  OS_PRIO_3);
	if (status != WM_SUCCESS) {
		cl_dbg("Failed to start cloud_thread: %d", status);
		return status;
	}

	cl_dbg("Thread started");
	return WM_SUCCESS;
}

static int cloud_init(cloud_t *c)
{
	struct partition_entry *p;
	flash_desc_t fl;
	bool cloud_enabled;

	cl_dbg("Initializing cloud ...");

	p = part_get_layout_by_id(FC_COMP_PSM, NULL);
	if (p == NULL) {
		cl_dbg("Can't find PSM partition");
		return -WM_FAIL;
	}

	part_to_flash_desc(p, &fl);

	if (psm_init(&fl) != WM_SUCCESS) {
		cl_dbg("Error initializing PSM");
		return -WM_FAIL;
	}

	int status = psm_register_module(CLOUD_MOD_NAME,
					 COMMON_PARTITION, PSM_CREAT);
	if (status != WM_SUCCESS && status != -WM_E_EXIST) {
		cl_dbg("Failed to register cloud module with psm: %d", status);
		return -WM_FAIL;
	}

	status = is_cloud_enabled(&cloud_enabled);
	if (status != WM_SUCCESS) {
		cl_dbg("Unable to read cloud enable status");
		return status;
	}

	if (!cloud_enabled) {
		cl_dbg("Warn: Cloud is disabled. Will not start");
		return -WM_FAIL;
	}

#if APPCONFIG_HTTPS_CLOUD
	/* XXX: Setting time to Aug 29 2013. This needs to be fixed. */
	wmtime_time_set_posix(1377778888);
	/* Initialize the TLS configuration structure */
	c->tls_cfg.flags = TLS_CHECK_SERVER_CERT;
	c->tls_cfg.tls.client.client_cert = NULL;
	c->tls_cfg.tls.client.client_cert_size = 0;
	c->tls_cfg.tls.client.server_cert = ca_cert;
	c->tls_cfg.tls.client.server_cert_size = sizeof(ca_cert) - 1;
#endif
	return WM_SUCCESS;
}

int wmcloud_get_ui_link(httpd_request_t *req)
{
	char content[50];
	char url[30];
	int i = 0, cnt = 0, index_1 = 0;

	if (c.state != CLOUD_ACTIVE || !c.url)
		return httpd_send_response(req, HTTP_RES_200, CLOUD_INACTIVE,
			strlen(CLOUD_INACTIVE), HTTP_CONTENT_PLAIN_TEXT_STR);

	/* Extract the IP address from the url
	 * If valid, URL could be of the type:
	 * i. http://192.168.100.100/cloud or
	 * ii. http://192.168.100.100/ or
	 * iii. 192.168.100.100 or
	 * iv. http://192.168.100.100 or
	 * v. 192.168.100.100/cloud or
	 * vi. 192.168.100.100/
	 */
	while (c.url[i] != '\0') {
		if (c.url[i] == '/') {
			if (cnt == 0) {
				/* Store the location of first '/' */
				index_1  = i;
			}
			cnt++;
		}
		if (cnt == 3) {
			/* URL is of type i or ii. */
			url[i++] = '\0';
			break;
		}
		url[i] = c.url[i];
		i++;
	}

	if (cnt == 0 || cnt == 2) {
		/* URL is of type iii. or iv. */
		strncpy(url, c.url, sizeof(url));
	} else if (cnt == 1) {
		/* URL is of type v. or vi. */
		url[index_1++] = '\0';
	}

	snprintf(content, sizeof(content), "Moved to %s", url);
	return httpd_send_response_301(req, content + 9,
		HTTP_CONTENT_PLAIN_TEXT_STR, content,
		strlen(content));

}

int cloud_sm(cloud_event_t event)
{
	int ret = -WM_FAIL;
	cloud_state_t next_state;
	os_mutex_get(&c.mutex, OS_WAIT_FOREVER);
begin:
	next_state = c.state;
	cl_dbg("Event %d on state: %d", event, c.state);
	switch (c.state) {

	case CLOUD_UNINIT:
		if (event == EVT_STRT) {
			ret = cloud_init(&c);
			if (ret != WM_SUCCESS)
				goto end;
			next_state = CLOUD_ACTIVE;
		}
		break;

	case CLOUD_ACTIVE:
		if (event == EVT_ENTER || event == EVT_STRT) {
			ret = cloud_activate(&c);
			if (ret != WM_SUCCESS)
				goto end;
		}

		if (event == EVT_INT_ERROR)
			sub_state = INT_ERROR;

		if (event == EVT_CONN_ERROR)
			sub_state = CONN_ERROR;

		if (event == EVT_TX_ERROR) {
			sub_state = TX_ERROR;
			g_wm_stats.wm_cl_post_fail++;
		}

		if (event == EVT_OP_SUCCESS)
			sub_state = RUNNING;

		if (event == EVT_STOP) {
			if (sub_state)
				next_state = CLOUD_HALT;
		}
		break;

	case CLOUD_HALT:
		if (event == EVT_ENTER) {
			ret = cloud_halt(&c);
			sub_state = 0;
			goto end;
		}

		if (event == EVT_STRT)
			next_state = CLOUD_ACTIVE;
		break;

	default:
		cl_dbg("Ignoring event");
	}

	if (c.state != next_state) {
		cl_dbg("State transition: %d --> %d",
			c.state, next_state);
		c.state = next_state;
		event = EVT_ENTER;
		goto begin;
	}

end:
	os_mutex_put(&c.mutex);
	return ret;
}

int cloud_actual_start(const char *dev_class, void (*handle_req)(struct json_str
		*jstr, struct json_object *obj, bool *repeat_POST),
		void (*periodic_post)(struct json_str *jstr),
		unsigned int stack_size)
{
	int ret;
	/* Cloud specific initialization */
	memset(&c, 0x00, sizeof(cloud_t));
	app_sys_get_uuid(c.uuid, UUID_MAX_LEN);
	/* If HTTPS is enabled, then large stack is required: around 12k
	 * So override the input value with 12k size */
#if APPCONFIG_HTTPS_CLOUD
	c.stack_size = 1024 * 12;
#else
	c.stack_size = stack_size;
#endif

	if (dev_class)
		strncpy(c.dev_class, dev_class, CLASS_NAME_MAX);

	/* Register the cloud handlers */
	if (handle_req)
		c.app_cloud_handle_req = handle_req;
	if (periodic_post)
		c.app_cloud_periodic_post = periodic_post;

	ret = os_mutex_create(&c.mutex, "cloud_mutex", OS_MUTEX_INHERIT);
	if (ret != WM_SUCCESS) {
		cl_dbg("Cloud mutex creation error %d", ret);
		return -WM_FAIL;
	}

	/* Send start event to the state machine */
	ret = cloud_sm(EVT_STRT);
	if (ret != WM_SUCCESS)
		os_mutex_delete(&c.mutex);

	return ret;
}

int cloud_actual_stop(void)
{
	/* Send stop event to the state machine */
	int ret = cloud_sm(EVT_STOP);
	os_mutex_delete(&c.mutex);

	return ret;
}

