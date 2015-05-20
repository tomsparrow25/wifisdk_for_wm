/*
 *  Copyright (C) 2008-2013, Marvell International Ltd.
 *  All Rights Reserved.
 */
#include <httpc.h>
#include <wmcloud.h>
#if APPCONFIG_HTTPS_CLOUD
#include "ca_cert_pem.h"
#include <wm-tls.h>
#endif

extern cloud_t c;
static os_semaphore_t sem;

static uint8_t is_cloud_started;
int wmcloud_get_ui_link(httpd_request_t *req);

static int cloud_read_post_resp(http_session_t hS, char *buf, unsigned *buf_len)
{
	int status, size_read;
	int offset;
	http_resp_t *resp;

	status = http_get_response_hdr(hS, &resp);
	if (status != WM_SUCCESS) {
		return status;
	}

	if (resp->status_code != HTTP_OK) {
		cl_dbg("Unexpected HTTP response (%d) to POST",
			    resp->status_code);
		return -WM_FAIL;
	}

	for (offset = 0; 1; offset += size_read, *buf_len -= offset) {
		if (!*buf_len) {
			/* We have exhausted our buffer but still server
			 * wants to send us data */
			cl_dbg("More data than expected"
				    " in resp to POST");
			return -WM_FAIL;
		}

		size_read = http_read_content(hS, &buf[offset], *buf_len);
		if (size_read < 0) {
			cl_dbg("Failure while reading"
				    " http content: %d.", size_read);
			return -WM_FAIL;
		}
		if (size_read == 0)
			break;
	}

	*buf_len = offset;
	return WM_SUCCESS;
}

static int send_cloud_post(const cloud_t *c, http_session_t hS)
{
	static http_req_t req = {
		.type = HTTP_POST,
		.resource = NULL,
		.version = HTTP_VER_1_1,
		.content = NULL,
		.content_len = 0,
	};

	req.resource = c->url;
	req.content = c->send_packet;
	req.content_len = strlen(c->send_packet);

	/* Prepare the HTTP request */
	int status = http_prepare_req(hS, &req, STANDARD_HDR_FLAGS);
	if (status != WM_SUCCESS) {
		cl_dbg("Error while preparing POST request");
		return status;
	}

	/* Add the Content-Type Header*/
	status = http_add_header(hS, &req, "Content-Type",
				 CLOUD_PACKET_CONTENT_TYPE);
	if (status != WM_SUCCESS) {
		cl_dbg("Error while adding header");
		return status;
	}

	/* Send the HTTP request */
	status = http_send_request(hS, &req);
	if (status != WM_SUCCESS) {
		cl_dbg("Error while sending POST to %s", c->url);
		return status;
	}
	return WM_SUCCESS;
}

static int connect_to_cloud(const cloud_t *c, http_session_t *hS)
{
	/*
	 * Not setting the TLS_ENABLE flag. If this c->url has an
	 * 'https' or the port number is 443 the client will switch
	 * automatically to TLS.
	 */

	int timeout = DEFAULT_CLOUD_SOCKET_TIMEOUT * 1000;
	int status = http_open_session(hS, c->url, 0,
#if APPCONFIG_HTTPS_CLOUD
				       &c->tls_cfg,
#else
				       NULL,
#endif
				       0);
	if (status == WM_SUCCESS) {
		/* Set timeout on cloud socket	*/
		http_setsockopt(*hS, SOL_SOCKET, SO_RCVTIMEO,
			&timeout, sizeof(int));
	}
	return status;
}

int cloud_get_ui_link(httpd_request_t *req)
{
	return wmcloud_get_ui_link(req);
}

static void cloud_loop()
{
	int ret;
	bool repeat_POST = false;
	unsigned recv_len;

	/* Connection is not established yet, so initialize the state machine
	 * with connection error */
	cloud_sm(EVT_CONN_ERROR);

	ret = create_transmit_packet(&c);
	/* create_transmit_packet() returns the length of tx packet */
	if (ret == 0) {
		cloud_sm(EVT_INT_ERROR);
		return;
	}

	/* Connect to the cloud */
	ret = connect_to_cloud(&c, &c.hS);
	if (ret != WM_SUCCESS) {
		cloud_sm(EVT_CONN_ERROR);
		return;
	}

begin:
	c.sequence++;
	repeat_POST = false;
	cloud_sm(EVT_OP_SUCCESS);

#ifdef CLOUD_DUMP_DATA
	cl_dbg("SEND: Cloud packet:");
	dump_cloud_packet(c.send_packet, strlen(c.send_packet));
#endif /* CLOUD_DUMP_DATA */

	/* Send the cloud post */
	ret = send_cloud_post(&c, c.hS);
	if (ret != WM_SUCCESS) {
		cloud_sm(EVT_TX_ERROR);
		if (c.hS)
			http_close_session(&c.hS);
		return;
	} else
		g_wm_stats.wm_cl_post_succ++;

	recv_len = CLOUD_PACKET_MAXSIZE;
	do {
		/* Read the response from the cloud server */
		ret = cloud_read_post_resp(c.hS, c.recv_packet, &recv_len);
		if (ret == WM_SUCCESS)
			break;

		/* Continue if timeout. For any other status values, close the
		 * session and then retry the connection */
		if (ret == -WM_E_HTTPC_SOCKET_ERROR && errno == EAGAIN)
			continue;

		http_close_session(&c.hS);
		return;
	} while (!c.stop_request);

	if (c.stop_request) {
		if (c.hS)
			http_close_session(&c.hS);
		return;
	}

#ifdef CLOUD_DUMP_DATA
	cl_dbg("RECV: Cloud packet:");
	dump_cloud_packet(c.recv_packet, recv_len);
#endif /* CLOUD_DUMP_DATA */

	/* Process the response from the cloud server */
	cloud_process_server_response(&c, recv_len, &repeat_POST);
	if (repeat_POST)
		goto begin;

	http_close_session(&c.hS);
}

static void cloud_sleep(cloud_t *c)
{
	os_semaphore_get(&sem, os_msec_to_ticks(100));
}

int cloud_wakeup_for_send()
{
	return WM_SUCCESS;
}
void cloud_thread_main(os_thread_arg_t arg)
{
	while (!c.stop_request) {
		cloud_loop();
		cloud_sleep(&c);
	}
	c.stop_request = false;
	os_thread_self_complete(NULL);
}

#define STACK_SIZE (1024 * 4)
int cloud_start(const char *dev_class, void (*handle_req)(struct json_str
		*jstr, struct json_object *obj, bool *repeat_POST),
		void (*periodic_post)(struct json_str *jstr))
{
	int ret;
	if (is_cloud_started)
		return WM_SUCCESS;

	ret = os_semaphore_create(&sem, "cloud_sem");
	if (ret != WM_SUCCESS) {
		cl_dbg("Cloud semaphore creation error %d", ret);
		return -WM_FAIL;
	}
	os_semaphore_get(&sem, OS_WAIT_FOREVER);

	ret = cloud_actual_start(dev_class, handle_req, periodic_post,
			STACK_SIZE);
	if (ret == WM_SUCCESS)
		is_cloud_started = 1;
	else
		os_semaphore_delete(&sem);

	return ret;

}

int cloud_stop(void)
{
	int ret;

	if (!is_cloud_started)
		return WM_SUCCESS;

	os_semaphore_put(&sem);

	ret = cloud_actual_stop();

	os_semaphore_delete(&sem);
	is_cloud_started = 0;

	return ret;
}
