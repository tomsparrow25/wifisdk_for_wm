/*
 *  Copyright 2008-2013,2011 Marvell International Ltd.
 *  All Rights Reserved.
 */
/* httpd_handle.c: This is the main processing of an HTTP
 * request. There could be three types of requests, a WSGI, an SSI or
 * a file. This file contains routines that deal with this.
 */

/*
 * Copyright (c) 2004, Adam Dunkels.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * This file is part of the uIP TCP/IP stack.
 *
 * Author: Adam Dunkels <adam@sics.se>
 *
 * \Adapted: C Michael Sundius, cozybit Inc.
 *     took the http handling code from this server and integrated it
 *     into cozybit's simple http server.  This module now only handles an http
 *     request contained in the input buffer and returns a reply buffer.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <wmstdio.h>
#include <wm_net.h>
#include <httpd.h>
#include <http_parse.h>
#include <http-strings.h>
#include <wmstats.h>

#include "httpd_priv.h"

#ifdef CONFIG_ENABLE_HTTPS
#include <wm-tls.h>

extern tls_handle_t httpd_tls_handle;
#endif /* ENABLE_HTTPS */

#define STATE_WAITING 0
#define STATE_OUTPUT  1

static httpd_request_t httpd_req;

/* Send the chunk header, which is just an ascii size followed by a cr lf.
 */
#define CHUNK_SIZE_DIGITS 0x10
int httpd_send_chunk_begin(int conn, int size)
{

	int err;
	char buf[CHUNK_SIZE_DIGITS];
	int i;
	int digit;
	int begin = 1;

	for (i = CHUNK_SIZE_DIGITS - 1; i >= 0; i--) {
		digit = size & 0xf;
		if (!begin && !size) {
			i++;
			break;
		}
		buf[i] = (digit > 9) ? digit - 0xA + 'a' : digit + '0';
		size = size >> 4;
		begin = 0;
	}
	err = httpd_send(conn, &buf[i], CHUNK_SIZE_DIGITS - i);
	if (err != WM_SUCCESS) {
		return err;
	}

	err = httpd_send_crlf(conn);

	return err;
}

/* Send the last chunk which is simply an ascii "0\r\n\r\n"
 */
int httpd_send_last_chunk(int conn)
{
	int err;

	err = httpd_send(conn, http_last_chunk,
			    sizeof(http_last_chunk) - 1);

	if (err != WM_SUCCESS) {
		httpd_e("Send last chunk failed");
	}

	return err;
}

/* Send one chunk of data of a given size
 */
int httpd_send_chunk(int conn, const char *buf, int len)
{
	int err;

	if (len) {
		/* Send chunk begin header */
		err = httpd_send_chunk_begin(conn, len);
		if (err != WM_SUCCESS)
			return err;
		/* Send our data */
		err = httpd_send(conn, buf, len);
		if (err != WM_SUCCESS)
			return err;
		/* Send chunk end indicator */
		err = httpd_send_crlf(conn);
		if (err != WM_SUCCESS)
			return err;
	} else {
		/* Length is 0, last chunk */
		err = httpd_send(conn, http_last_chunk,
				   sizeof(http_last_chunk) - 1);
		if (err != WM_SUCCESS)
			return err;
	}

	return err;
}

/*Helper function to send a buffer over a connection.
 */
int httpd_send(int conn, const char *buf, int len)
{
	int num;
	do {
#ifdef CONFIG_ENABLE_HTTPS
		if (httpd_is_https_active())
			num = tls_send(httpd_tls_handle, buf, len);
		else
#endif /* ENABLE_HTTPS */
			num = send(conn, buf, len, 0);
		if (num < 0) {
			httpd_e("send() failed: %d",
			      net_get_sock_error(conn));
			return -WM_FAIL;
		}
		len -= num;
		buf += len;
	} while (len > 0);


	return WM_SUCCESS;
}

int httpd_recv(int fd, void *buf, size_t n, int flags)
{
#ifdef CONFIG_ENABLE_HTTPS
	if (httpd_is_https_active())
		return tls_recv(httpd_tls_handle, buf, n);
	else
#endif /* ENABLE_HTTPS */
		return recv(fd, buf, n, flags);
}

int httpd_send_hdr_from_code(int sock, int stat_code,
			     enum http_content_type content_type)
{
	const char *str;
	unsigned int str_len;
	/*
	 * Set the HTTP Response Status 200 OK, 404 NOT FOUND etc.
	 * Also set the Transfer-Encoding to chunked.
	 */
	switch (stat_code) {
	case -WM_E_HTTPD_HANDLER_404:
		str = http_header_404;
		str_len = strlen(http_header_404);
		break;
	case -WM_E_HTTPD_HANDLER_400:
		str = http_header_400;
		str_len = strlen(http_header_400);
		break;
	case -WM_E_HTTPD_HANDLER_500:
		str = http_header_500;
		str_len = strlen(http_header_500);
		break;
	default:
		/* The handler doesn't want an HTTP error code, but would return
		 * 200 OK with an error in the response */
		str = http_header_200;
		str_len = strlen(http_header_200);
		break;
	}

	if (httpd_send(sock, str, str_len) != WM_SUCCESS) {
		return -WM_FAIL;
	}

	/* Send the Content-Type */
	switch (content_type) {
	case HTTP_CONTENT_JSON:
		str = http_content_type_json_nocache;
		str_len = sizeof(http_content_type_json_nocache) - 1;
		break;
	case HTTP_CONTENT_XML:
		str = http_content_type_xml_nocache;
		str_len = sizeof(http_content_type_xml_nocache) - 1;
		break;
	case HTTP_CONTENT_HTML:
		str = http_content_type_html;
		str_len = sizeof(http_content_type_html) - 1;
		break;
	case HTTP_CONTENT_JPEG:
		str = http_content_type_jpg;
		str_len = sizeof(http_content_type_jpg) - 1;
		break;
	default:
		str = http_content_type_plain;
		str_len = sizeof(http_content_type_plain) - 1;
		break;
	}
	if (httpd_send(sock, str, str_len) != WM_SUCCESS) {
		return -WM_FAIL;
	}
	return WM_SUCCESS;
}



/* set the system wide error. */
static char httpd_error[HTTPD_MAX_ERROR_STRING + 1];
void httpd_set_error(const char *fmt, ...)
{
	va_list argp;

	va_start(argp, fmt);
	vsnprintf(httpd_error, HTTPD_MAX_ERROR_STRING + 1, fmt, argp);
	va_end(argp);
	httpd_d("%s", httpd_error);
}

/* Helper function to send an error.
 */
int httpd_send_error(int conn, int http_error)
{
	int err = 0;

	switch (http_error) {
	case HTTP_404:
		err = httpd_send(conn, http_header_404,
				 strlen(http_header_404));
		break;
	case HTTP_500:
		err = httpd_send(conn, http_header_500,
				 strlen(http_header_500));
		break;

	case HTTP_505:
		err = httpd_send(conn, http_header_505,
				 strlen(http_header_505));
		break;
	}

	if (err != WM_SUCCESS) {
		return err;
	}

	err = httpd_send(conn, http_header_type_chunked,
			strlen(http_header_type_chunked));
	if (err != WM_SUCCESS) {
		return err;
	}

	err = httpd_send(conn, http_content_type_plain,
			 sizeof(http_content_type_plain) - 1);
	if (err != WM_SUCCESS) {
		return err;
	}

	err = httpd_send_crlf(conn);
	if (err != WM_SUCCESS) {
		return err;
	}

	err = httpd_send_chunk(conn, httpd_error, strlen(httpd_error));
	if (err != WM_SUCCESS) {
		return err;
	}
	return httpd_send_last_chunk(conn);
}


/*
 * @pre Only first line of HTTP header (which has the resource name) has
 * been read from the socket. We need to read the remaining header and the
 * data after that.
 */
void httpd_purge_socket_data(httpd_request_t *req, char *msg_in,
			     int msg_in_len, int conn)
{
	int status = httpd_parse_hdr_tags(req, conn, msg_in, msg_in_len);
	if (status != WM_SUCCESS) {
		/* We were unsuccessful in purging the socket.*/
		httpd_e("Unable to purge socket: %d", status);
		return;
	}

	unsigned data_remaining = req->body_nbytes;

	while (data_remaining) {
		unsigned to_read = msg_in_len >= data_remaining ?
			data_remaining : msg_in_len;
		int actually_read = httpd_recv(conn, msg_in, to_read, 0);
		if (actually_read < 0) {
			httpd_e("Unable to read content."
				"Was purging socket data");
			return;
		}
		data_remaining -= actually_read;
	}
}

/* Handle an incoming message (request) from the client. This is the
 * main processing function of the HTTPD.
 */
int httpd_handle_message(int conn)
{
	int err;
	int req_line_len;
	char msg_in[128];

	/* clear out the httpd_req structure */
	memset(&httpd_req, 0x00, sizeof(httpd_req));

	httpd_req.sock = conn;

	/* Read the first line of the HTTP header */
	req_line_len = htsys_getln_soc(conn, msg_in, sizeof(msg_in));
	if (req_line_len == 0)
		return HTTPD_DONE;

	if (req_line_len < 0) {
		httpd_e("Could not read from socket");
		return -WM_FAIL;
	}

	/* Parse the first line of the header */
	err = httpd_parse_hdr_main(msg_in, &httpd_req);
	if (err == -WM_E_HTTPD_NOTSUPP)
		/* Send 505 HTTP Version not supported */
		return httpd_send_error(conn, HTTP_505);
	else if (err != WM_SUCCESS) {
		/* Send 500 Internal Server Error */
		return httpd_send_error(conn, HTTP_500);
	}

	/* set a generic error that can be overridden by the wsgi handling. */
	httpd_d("Presetting error to: ");
	httpd_set_error("wsgi handler failed");

	/* Web Services Gateway Interface branch point:
	 * At this point we have the request type (httpd_req.type) and the path
	 * (httpd_req.filename) and all the headers waiting to be read from
	 * the socket.
	 *
	 * The call bellow will iterate through all the url patterns and
	 * invoke the handlers that match the request type and pattern.  If
	 * request type and url patern match, invoke the handler.
	 */
	err = httpd_wsgi(&httpd_req);

	if (err == HTTPD_DONE) {
		httpd_d("Done processing request.");
		return WM_SUCCESS;
	} else if (err == -WM_E_HTTPD_NO_HANDLER) {
		httpd_w("No handler for the given URL %s was found",
			httpd_req.filename);
		/*
		 * We have not yet read the complete data from the current
		 * request, from the socket. We are in an error state and
		 * we wish to cancel this HTTP transaction. We sent
		 * appropriate message to the client and read (flush) out
		 * all the pending data in the socket. We let the client
		 * close the socket for us, if necessary.
		 */
		httpd_purge_socket_data(&httpd_req, msg_in,
				sizeof(msg_in), conn);
		httpd_set_error("File %s not_found", httpd_req.filename);
		httpd_send_error(conn, HTTP_404);
		return WM_SUCCESS;
	} else {
		httpd_e("WSGI handler failed.");
		/* Send 500 Internal Server Error */
		return httpd_send_error(conn, HTTP_500);
	}

}
