#include <openssl/ssl.h>

#include <string.h>
#include <stdlib.h>

#include <wmstdio.h>
#include <wmerrno.h>
#include <cli.h>
#include <httpc.h>

#include "tls-demo.h"

static int request_http_get(const char *url_str, http_session_t * handle,
			    http_resp_t **http_resp)
{
	int status;

	dbg("Connecting to %s", url_str);

	/* Initialize the TLS configuration structure */
	tls_init_config_t cfg = {
		.flags = 0,
		.tls.client.client_cert = NULL,
		.tls.client.client_cert_size = 0,
		.tls.client.ca_cert = NULL,
		.tls.client.ca_cert_size = 0,
	};

	/*
	 * Open an HTTP Session with the specified URL.
	 */
	status = http_open_session(handle, url_str, 0, &cfg, 0);
	if (status != WM_SUCCESS) {
		dbg("Open session failed URL: %s", url_str);
		goto http_session_open_fail;
	}

	/* Setup the parameters for the HTTP Request. This only prepares the
	 * request to be sent out.  */
	http_req_t req;
	req.type = HTTP_GET;
	req.resource = url_str;
	req.version = HTTP_VER_1_1;
	req.content = NULL;

	status = http_prepare_req(*handle, &req, STANDARD_HDR_FLAGS);
	if (status != WM_SUCCESS) {
		dbg("Request prepare failed: URL: %s", url_str);
		goto http_session_error;
	}

	/* Send the HTTP GET request, that was prepared earlier, to the web
	 * server */
	status = http_send_request(*handle, &req);
	if (status != WM_SUCCESS) {
		dbg("Request send failed: URL: %s", url_str);
		goto http_session_error;
	}

	/* Read the response and make sure that an HTTP_OK response is
	 * received */
	http_resp_t *resp;
	status = http_get_response_hdr(*handle, &resp);
	if (status != WM_SUCCESS) {
		dbg("Unable to get response header: %d", status);
		goto http_session_error;
	}

	if (resp->status_code != HTTP_OK) {
		dbg("Unexpected status code received from server: %d",
			  resp->status_code);
		status = -WM_FAIL;
	}

	if (http_resp)
		*http_resp = resp;

	return WM_SUCCESS;
http_session_error:
	http_close_session(handle);
http_session_open_fail:
	return status;
}

#define MAX_DOWNLOAD_DATA 32
void tls_httpc_client(char *url_str)
{
	int status, i, dsize;
	http_session_t handle;
	char buf[MAX_DOWNLOAD_DATA];

	status = request_http_get(url_str, &handle, NULL);
	if (status != 0) {
		dbg("Unable to print resource");
		return;
	}

	TLSD("Resource dump begins");
	while (1) {
		/* Keep reading data over the HTTPS session and print it out on
		 * the console. */
		dsize = http_read_content(handle, buf, MAX_DOWNLOAD_DATA);
		if (dsize < 0) {
			dbg("Unable to read data on http session: %d",
				   dsize);
			break;
		}
		if (dsize == 0) {
			TLSD("********* All data read **********");
			break;
		}

		for (i = 0; i < dsize; i++) {
			if (buf[i] == '\r')
				continue;
			if (buf[i] == '\n') {
				wmprintf("\n\r");
				continue;
			}
			wmprintf("%c", buf[i]);
		}
	}
	/* Close the HTTP session */
	http_close_session(&handle);
}

