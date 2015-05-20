/*
 *  Copyright (C) 2008-2013, Marvell International Ltd.
 *  All Rights Reserved.
 */

#include <httpc.h>
#include <wmcloud.h>
#include <websockets.h>
#include <lwip/api.h>
#if APPCONFIG_HTTPS_CLOUD
#include "ca_cert_pem.h"
#include <wm-tls.h>
#endif

#define WEBSOCKET_SLEEP_MSEC                1000

os_queue_t ws_event_queue;
static os_queue_pool_define(ws_event_pool,
			4 * sizeof(ws_event_t));
extern cloud_t c;
ws_context_t ws;

static uint8_t is_cloud_started;
int wmcloud_get_ui_link(httpd_request_t *req);

static int connect_to_cloud(cloud_t *c, http_session_t *hS)
{
	int status;
	http_req_t req;

	status = http_open_session(hS, c->url, 0,
#if APPCONFIG_HTTPS_CLOUD
				       &c->tls_cfg,
#else
				       NULL,
#endif
				       0);
	if (status != WM_SUCCESS) {
		return -WM_FAIL;
	}
	req.type = HTTP_GET;
	req.resource = "/";
	req.version = HTTP_VER_1_1;
	status = http_prepare_req(*hS, &req, STANDARD_HDR_FLAGS);
	if (status != WM_SUCCESS) {
		http_close_session(hS);
		return -WM_FAIL;
	}

	status = ws_upgrade_socket(hS, &req, "chat", &ws);
	if (status != WM_SUCCESS) {
		http_close_session(hS);
		return -WM_FAIL;
	}

	return WM_SUCCESS;
}

void app_recv_cb(int s, void *data)
{
	int ret;
	ws_event_t ev = WS_RECV_DATA;
	ret = os_queue_send(&ws_event_queue, &ev, OS_NO_WAIT);
	if (ret != WM_SUCCESS)
		cl_dbg("Error: Failed to send event on the queue\r\n");
}


static int send_cloud_post(const cloud_t *c, http_session_t hS)
{
	int ret;
	ws_frame_t f;

	f.fin = 1;
	f.opcode = WS_TEXT_FRAME;
	f.data = (uint8_t *) &c->send_packet;
	f.data_len = strlen(c->send_packet);
#ifdef CLOUD_DUMP_DATA
	cl_dbg("SEND: Cloud packet:");
	dump_cloud_packet(f.data, f.data_len);
#endif /* CLOUD_DUMP_DATA */
	ret = ws_frame_send(&ws, &f);
	if (ret <= 0) {
		cl_dbg("Error while sending POST to %s", c->url);
		return -WM_FAIL;
	} else
		return WM_SUCCESS;
}


static int cloud_read_post_resp(http_session_t hS, char *buf, unsigned *buf_len)
{
#define WS_KEEPALIVE_SENT 0x10000
	int ret;
	ws_frame_t f;
	ret = ws_frame_recv(&ws, &f);
	if (f.opcode == WS_PING_FRAME) {
		f.opcode = WS_PONG_FRAME;
		ws_frame_send(&ws, &f);
		return WS_KEEPALIVE_SENT;
	}

	if (ret > 0) {
		*buf_len = ret;
		strncpy(buf, (char *) f.data, *buf_len);
		return WM_SUCCESS;
	} else {
		*buf_len = 0;
		cl_dbg("Error in reading data\r\n");
		return -WM_FAIL;
	}
}

int cloud_wakeup_for_send()
{
	if (c.state != CLOUD_ACTIVE)
		return -WM_FAIL;

	ws_event_t ev = WS_SEND_DATA;
	return os_queue_send(&ws_event_queue, &ev, OS_NO_WAIT);
}


static void cloud_sleep()
{
	os_thread_sleep(os_msec_to_ticks(100));
}

int cloud_get_ui_link(httpd_request_t *req)
{
	return wmcloud_get_ui_link(req);
}

static void cloud_loop()
{
	bool send_resp;
	int ret, status;
	ws_event_t ev;
	unsigned recv_len;

	/* Connection is not established yet, so initialize the state machine
	 * with connection error */
	cloud_sm(EVT_CONN_ERROR);

	if (connect_to_cloud(&c, &c.hS) != WM_SUCCESS) {
		cloud_sm(EVT_CONN_ERROR);
		return;
	}

	/* Websocket is now created */
	lwip_register_recv_cb(http_get_sockfd_from_handle(c.hS), app_recv_cb,
				NULL);
	cloud_sm(EVT_OP_SUCCESS);

	cloud_wakeup_for_send();

begin:
	ret = os_queue_recv(&ws_event_queue, &ev, OS_WAIT_FOREVER);

	if (ret == WM_SUCCESS && ev == WS_STOP_TRANSACTION) {
		if (c.hS) {
			lwip_register_recv_cb(
				http_get_sockfd_from_handle(c.hS),
				NULL, NULL);
			ws_close_ctx(&ws);
			http_close_session(&c.hS);
		}
		return;
	}

	switch (ev) {
	case WS_SEND_DATA:
		if (!send_resp) {
			status = create_transmit_packet(&c);
			/* create_transmit_packet() returns the
			 * length of tx packet */
			if (status == 0) {
				cloud_sm(EVT_INT_ERROR);
				return;
			}
		} else
			send_resp = false;
		ret = send_cloud_post(&c, c.hS);
		if (ret != WM_SUCCESS) {
			cloud_sm(EVT_TX_ERROR);
			if (c.hS) {
				lwip_register_recv_cb(
					http_get_sockfd_from_handle(c.hS),
					NULL, NULL);
				ws_close_ctx(&ws);
				http_close_session(&c.hS);
			}
			return;
		} else
			g_wm_stats.wm_cl_post_succ++;
		c.sequence++;
		break;
	case WS_RECV_DATA:
		send_resp = false;
		recv_len = CLOUD_PACKET_MAXSIZE;
		ret = cloud_read_post_resp(c.hS, c.recv_packet, &recv_len);
		if (ret == WS_KEEPALIVE_SENT)
			goto begin;

		if (ret == -WM_FAIL || recv_len == 0) {
			cloud_sm(EVT_TX_ERROR);
			if (c.hS) {
				lwip_register_recv_cb(
					http_get_sockfd_from_handle(c.hS),
					NULL, NULL);
				ws_close_ctx(&ws);
				http_close_session(&c.hS);
			}
			return;
		}

		cloud_process_server_response(&c, recv_len, &send_resp);
		if (send_resp)
			cloud_wakeup_for_send();
		break;
	default:
		break;
	}
	goto begin;
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


#define STACK_SIZE (1024 * 8)
int cloud_start(const char *dev_class, void (*handle_req)(struct json_str
		*jstr, struct json_object *obj, bool *repeat_POST),
		void (*periodic_post)(struct json_str *jstr))
{
	int ret;

	if (is_cloud_started)
		return WM_SUCCESS;

	ret = os_queue_create(&ws_event_queue, "ws_event_queue",
		sizeof(ws_event_t), &ws_event_pool);
	if (ret != WM_SUCCESS) {
		cl_dbg("Cloud queue creation error %d", ret);
		return ret;
	}

	ret = cloud_actual_start(dev_class, handle_req, periodic_post,
			STACK_SIZE);
	if (ret == WM_SUCCESS)
		is_cloud_started = 1;
	else
		os_queue_delete(&ws_event_queue);

	return ret;

}

int cloud_stop(void)
{
	int ret = 0;

	if (!is_cloud_started)
		return WM_SUCCESS;

	ws_event_t ev = WS_STOP_TRANSACTION;
	ret = os_queue_send(&ws_event_queue, &ev, OS_NO_WAIT);
	if (ret != WM_SUCCESS) {
		/*Deleting the thread even if os_queue_send is unsuccessful */
		cl_dbg("Error: Failed to send stop event on the queue\r\n");
	}

	ret = cloud_actual_stop();
	os_queue_delete(&ws_event_queue);
	is_cloud_started = 0;

	return ret;

}
