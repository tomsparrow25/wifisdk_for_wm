/*
 *  Copyright 2008-2013, Marvell International Ltd.
 *  All Rights Reserved.
 */

#include <wmtypes.h>
#include <wmstdio.h>
#include <arch/sys.h>
#include <string.h>
#include <wmstdio.h>
#include <wm_os.h>
#include <wm_net.h>
#include <wlan.h>
#include <httpc.h>


#include <websockets.h>
#include "wslay_frame.h"

#define dbg(...)

#define WEBSOCKET_KEY               "rvvUr2a24dCLT+fe4V0SA"
#define MIN(x, y) 		    (((x) < (y)) ? (x) : (y))

static ssize_t send_callback(const uint8_t *buf, size_t len,
				int flags, void *user_data)
{
	int ret;
	http_session_t *handle = (http_session_t *) user_data;
	ret = http_lowlevel_write(*handle, buf, len);
	return ret;
}

static ssize_t recv_callback(uint8_t *buf, size_t len,
				int flags, void *user_data)
{
	int ret;
	http_session_t *handle = (http_session_t *) user_data;
	ret = http_lowlevel_read(*handle, buf, len);
	return ret;
}

struct wslay_frame_callbacks callbacks = { send_callback,
					recv_callback,
					NULL };


#define HTTP_HDR_SWITCHING_PROTO  101
int ws_upgrade_socket(http_session_t *handle, http_req_t *req,
		      const char *protocol, ws_context_t *ws_ctx)
{
	int status;
	http_resp_t *resp;
	wslay_frame_context_ptr *ctx = (wslay_frame_context_ptr *) ws_ctx;

	http_add_header(*handle, req, "Connection", "Upgrade");
	http_add_header(*handle, req, "Upgrade", "websocket");
	http_add_header(*handle, req, "Sec-WebSocket-Protocol", protocol);
	http_add_header(*handle, req, "Sec-WebSocket-Version", "13");
	http_add_header(*handle, req, "Sec-WebSocket-Key", WEBSOCKET_KEY);

	status = http_send_request(*handle, req);
	if (status != WM_SUCCESS) {
		http_close_session(handle);
		return -WM_FAIL;
	} else

	status = http_get_response_hdr(*handle, &resp);
	/* Validate the response members? */
	if (status != WM_SUCCESS ||
	    resp->status_code != HTTP_HDR_SWITCHING_PROTO)
		return -WM_FAIL;

	if (wslay_frame_context_init(ctx, &callbacks, handle)) {
		dbg("Failed to create context\r\n");
		return -WM_FAIL;
	}

	return WM_SUCCESS;
}

int ws_frame_send(ws_context_t *ws_ctx, ws_frame_t *f)
{
	int nsend;
	struct wslay_frame_iocb iocb;
	wslay_frame_context_ptr *ctx = (wslay_frame_context_ptr *) ws_ctx;

	memset(&iocb, 0, sizeof(iocb));
	iocb.fin = f->fin;
	iocb.mask = 0;

	if (f->opcode == WS_TEXT_FRAME) {
		iocb.opcode = WSLAY_TEXT_FRAME;
	} else if (f->opcode == WS_PONG_FRAME) {
		iocb.opcode = WSLAY_PONG;
	} else if (f->opcode == WS_BIN_FRAME) {
		iocb.opcode = WSLAY_BINARY_FRAME;
	} else if (f->opcode == WS_PING_FRAME) {
		iocb.opcode = WSLAY_PING;
	} else if (f->opcode == WS_CONT_FRAME) {
		iocb.opcode = WSLAY_CONTINUATION_FRAME;
	} else
		return -WM_FAIL;
	iocb.data_length = iocb.payload_length = f->data_len;
	iocb.data = f->data;

	nsend = wslay_frame_send(*ctx, &iocb);

	return nsend;
}

int ws_frame_recv(ws_context_t *ws_ctx, ws_frame_t *f)
{
	int nrecv;
	struct wslay_frame_iocb iocb;
	wslay_frame_context_ptr *ctx = (wslay_frame_context_ptr *) ws_ctx;

	memset(&iocb, 0, sizeof(iocb));
	memset(f, 0, sizeof(*f));

	nrecv = wslay_frame_recv(*ctx, &iocb);

	f->fin = iocb.fin;
	f->opcode = iocb.opcode;
	f->data = iocb.data;
	f->data_len = iocb.data_length;

	if (nrecv == WSLAY_ERR_WANT_READ) {
		nrecv = -WM_E_AGAIN;
	} else if (nrecv == WSLAY_ERR_SOCK_ERR) {
		nrecv = -WM_E_HTTPC_SOCKET_ERROR;
	} else if (nrecv == WSLAY_ERR_SOCK_SHUTDOWN) {
		nrecv = -WM_E_HTTPC_SOCKET_SHUTDOWN;
	}

	return nrecv;
}

void ws_close_ctx(ws_context_t *ws_ctx)
{
	wslay_frame_context_ptr *ctx = (wslay_frame_context_ptr *) ws_ctx;
	wslay_frame_context_free(*ctx);
}
