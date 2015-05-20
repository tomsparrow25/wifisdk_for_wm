/*
 *	Copyright 2008-2013,2011 Marvell International Ltd.
 *	All Rights Reserved.
 */

/* httpd_wsgi.c: This file contains the functions for the WSGI handling of the
 * httpd. Additionally, two WSGI handlers are also registered. These handlers
 * perform the basic tasks of parsing the HTTPD headers and the form data in
 * case of non-WSGI requests.
 */

#include <string.h>

#include <wm_net.h>
#include <httpd.h>
#include <http_parse.h>
#include <http-strings.h>
#include <wmtime.h>
#include <wmstats.h>

#include "httpd_priv.h"

#define MAX_WSGI_HANDLERS 32

static struct httpd_wsgi_call *calls[MAX_WSGI_HANDLERS];

/** This is the maximum size of a POST response */
#define MAX_HTTP_POST_RESPONSE 256
char http_response[MAX_HTTP_POST_RESPONSE];

/* Register a WSGI call in the list of handlers */
int httpd_register_wsgi_handler(struct httpd_wsgi_call *wsgi_call)
{
	int i, store_index = -1;

	if (!wsgi_call->uri)
		return WM_SUCCESS;

	for (i = 0; i < MAX_WSGI_HANDLERS; i++) {
		/*Find the first empty location in the calls array */
		if (!calls[i] && store_index == -1) {
			httpd_d("Found empty location %d", i);
			store_index = i;
			continue;
		}
		if (strcmp(calls[i]->uri, wsgi_call->uri) == 0) {
			httpd_d("Found wsgi %s at slot %d",
			      wsgi_call->uri, i);
			return WM_SUCCESS;
		}
	}
	if (store_index == -1) {
		httpd_e("Array full.. Cannot register wsgi %s", wsgi_call->uri);
		return -WM_FAIL;
	}

	httpd_d("Register wsgi %s at %d", wsgi_call->uri,
	      store_index);

	calls[store_index] = wsgi_call;
	return WM_SUCCESS;
}


int httpd_register_wsgi_handlers(struct httpd_wsgi_call *wsgi_call_list, int
					handlers_no)
{
	int i, ret, err = 0;

	for (i = 0; i < handlers_no; i++) {
		ret = httpd_register_wsgi_handler(&wsgi_call_list[i]);
		if (ret != WM_SUCCESS) {
			httpd_e("Error in registering %s handler\r\n",
				wsgi_call_list[i].uri);
			err++;
		}
	}
	if (err)
		return -WM_FAIL;
	else
		return WM_SUCCESS;
}

/* Unregister a WSGI call */
int httpd_unregister_wsgi_handler(struct httpd_wsgi_call *wsgi_call)
{
	int i;

	for (i = 0; i < MAX_WSGI_HANDLERS; i++) {
		if (calls[i] && (calls[i] == wsgi_call)) {
			calls[i] = NULL;
			return 0;
		}
	}

	return 0;
}

int httpd_unregister_wsgi_handlers(struct httpd_wsgi_call *wsgi_call_list,
				int handlers_no)
{
	int i, ret, err = 0;

	for (i = 0; i < handlers_no; i++) {
		ret = httpd_unregister_wsgi_handler(&wsgi_call_list[i]);
		if (ret != WM_SUCCESS) {
			httpd_e("Error in unregistering handler\r\n");
			err++;
		}
	}
	if (err)
		return -WM_FAIL;
	else
		return WM_SUCCESS;
}

typedef enum {
	ANY_OTHER_CHAR = 0,
	/* First \r found */
	FIRST_CR_FOUND,
	/* First \n found */
	FIRST_LF_FOUND,
	/* Second \r found */
	SECOND_CR_FOUND,
	/* Second \n found */
	SECOND_LF_FOUND,
} httpd_purge_state_t;


int httpd_purge_headers(int sock)
{
	unsigned char ch;
	int ret;
	httpd_purge_state_t purge_state = ANY_OTHER_CHAR;
	while ((ret = httpd_recv(sock, &ch, 1, 0)) != 0) {
		switch (ch) {
		case '\r':
			if (purge_state == ANY_OTHER_CHAR)
				purge_state = FIRST_CR_FOUND;
			if (purge_state == FIRST_LF_FOUND)
				purge_state = SECOND_CR_FOUND;
			break;
		case '\n':
			if (purge_state == FIRST_CR_FOUND)
				purge_state = FIRST_LF_FOUND;
			if (purge_state == SECOND_CR_FOUND)
				purge_state = SECOND_LF_FOUND;
			break;
		default:
			purge_state = ANY_OTHER_CHAR;

		}
		if (purge_state == SECOND_LF_FOUND)
			return WM_SUCCESS;
	}
	return -WM_FAIL;

}

int httpd_send_header(int sock, const char *name, const char *value)
{

	int ret;

	ret = httpd_send(sock, name, strlen(name));
	if (ret)
		return ret;
	ret = httpd_send(sock, ": ", 2);
	if (ret)
		return ret;
	ret = httpd_send(sock, value, strlen(value));
	if (ret)
		return ret;
	ret = httpd_send_crlf(sock);
	if (ret)
		return ret;

	return WM_SUCCESS;
}

int httpd_send_default_headers(int sock, int hdr_fields)
{
	int ret = WM_SUCCESS;

	if (hdr_fields & HTTPD_HDR_ADD_SERVER) {
		ret = httpd_send(sock, http_header_server,
			strlen(http_header_server));
		if (ret != WM_SUCCESS)
			return -WM_FAIL;
	}

	if (hdr_fields & HTTPD_HDR_ADD_CONN_CLOSE) {
		ret = httpd_send(sock, http_header_conn_close,
			strlen(http_header_conn_close));
		if (ret != WM_SUCCESS)
			return -WM_FAIL;
	}

	if (hdr_fields & HTTPD_HDR_ADD_CONN_KEEP_ALIVE) {
		ret = httpd_send(sock, http_header_conn_keep_alive,
			strlen(http_header_conn_keep_alive));
		if (ret != WM_SUCCESS)
			return -WM_FAIL;
	}

	/* Currently as we are sending chunked data, this is mandatory */
	if (hdr_fields & HTTPD_HDR_ADD_TYPE_CHUNKED) {
		ret = httpd_send(sock, http_header_type_chunked,
			strlen(http_header_type_chunked));
		if (ret != WM_SUCCESS)
			return -WM_FAIL;
	}

	if (hdr_fields & HTTPD_HDR_ADD_CACHE_CTRL) {
		ret = httpd_send(sock, http_header_cache_ctrl,
			strlen(http_header_cache_ctrl));
		if (ret != WM_SUCCESS)
			return -WM_FAIL;
	}

	if (hdr_fields & HTTPD_HDR_ADD_CACHE_CTRL_NO_CHK) {
		ret = httpd_send(sock, http_header_cache_ctrl_no_chk,
			strlen(http_header_cache_ctrl_no_chk));
		if (ret != WM_SUCCESS)
			return -WM_FAIL;
	}

	if (hdr_fields & HTTPD_HDR_ADD_PRAGMA_NO_CACHE) {
		ret = httpd_send(sock, http_header_pragma_no_cache,
			strlen(http_header_pragma_no_cache));
		if (ret != WM_SUCCESS)
			return -WM_FAIL;
	}

	return WM_SUCCESS;
}

static inline bool chunked_encoding(httpd_request_t *req)
{
	return req->wsgi->hdr_fields & HTTPD_HDR_ADD_TYPE_CHUNKED;
}


int httpd_send_response_301(httpd_request_t *req, char *location, const char
		*content_type, char *content, int content_len)
{
	int ret;

	/* Parse the header tags. This is valid only for GET or HEAD request */
	if (req->type == HTTPD_REQ_TYPE_GET ||
		req->type == HTTPD_REQ_TYPE_HEAD) {
		ret = httpd_purge_headers(req->sock);

		if (ret != WM_SUCCESS) {
			httpd_e("Unable to purge headers: 0x%x",
				__builtin_return_address(0));
			return ret;
		}
	}

	ret = httpd_send(req->sock, HTTP_RES_301, strlen(HTTP_RES_301));
	if (ret != WM_SUCCESS) {
		httpd_e("Error in sending the first line");
		return ret;
	}

	/* Send default headers */
	httpd_d("HTTP Req for URI %s", req->wsgi->uri);
	if (req->wsgi->hdr_fields) {
		ret = httpd_send_default_headers(req->sock,
				req->wsgi->hdr_fields);
		if (ret != WM_SUCCESS) {
			httpd_e("Error in sending default headers");
			return ret;
		}
	}
	ret = httpd_send_header(req->sock, "Location", location);
	if (ret != WM_SUCCESS) {
		httpd_e("Error in sending Location");
		return ret;
	}

	ret = httpd_send_header(req->sock, "Content-Type", content_type);
	if (ret != WM_SUCCESS) {
		httpd_e("Error in sending Content-Type");
		return ret;
	}

	/* Send Content-Length if non-chunked */
	if (!chunked_encoding(req)) {
		/* 6 should be more than enough */
		char con_len[6];
		snprintf(con_len, sizeof(con_len), "%d", content_len);

		ret = httpd_send_header(req->sock, "Content-Length", con_len);
		if (ret != WM_SUCCESS) {
			httpd_e("Error in sending Content-Length");
			return ret;
		}
	}

	httpd_send_crlf(req->sock);

	/* Content can be NULL */
	if (!content)
		return WM_SUCCESS;
	/* HTTP Head response does not require any content. It should
	 * contain identical headers as per the corresponding GET request */
	if (req->type != HTTPD_REQ_TYPE_HEAD) {
		if (chunked_encoding(req)) {
			ret = httpd_send_chunk(req->sock, content, content_len);
			if (ret != WM_SUCCESS) {
				httpd_e("Error in sending response content");
				return ret;
			}

			ret = httpd_send_chunk(req->sock, NULL, 0);
			if (ret != WM_SUCCESS)
				httpd_e("Error in sending last chunk");
		} else {
			/* Send our data */
			ret = httpd_send(req->sock, content, content_len);
			if (ret != WM_SUCCESS)
				httpd_e("Error sending response");
		}
	}
	return ret;
}

int httpd_send_response(httpd_request_t *req, const char *first_line,
			char *content, int length, const char *content_type)
{
	int ret;

	/* Parse the header tags. This is valid only for GET or HEAD request */
	if (req->type == HTTPD_REQ_TYPE_GET ||
		req->type == HTTPD_REQ_TYPE_HEAD) {
		ret = httpd_purge_headers(req->sock);

		if (ret != WM_SUCCESS) {
			httpd_e("Unable to purge headers: 0x%x",
				__builtin_return_address(0));
			return ret;
		}
	}

	ret = httpd_send(req->sock, first_line, strlen(first_line));
	if (ret != WM_SUCCESS) {
		httpd_e("Error in sending the first line");
		return ret;
	}

	/* Send default headers */
	httpd_d("HTTP Req for URI %s", req->wsgi->uri);
	if (req->wsgi->hdr_fields) {
		ret = httpd_send_default_headers(req->sock,
				req->wsgi->hdr_fields);
		if (ret != WM_SUCCESS) {
			httpd_e("Error in sending default headers");
			return ret;
		}
	}
	ret = httpd_send_header(req->sock, "Content-Type", content_type);
	if (ret != WM_SUCCESS) {
		httpd_e("Error in sending Content-Type");
		return ret;
	}

	/* Send Content-Length if non-chunked */
	if (!chunked_encoding(req)) {
		/* 6 should be more than enough */
		char con_len[6];
		snprintf(con_len, sizeof(con_len), "%d", length);

		ret = httpd_send_header(req->sock, "Content-Length", con_len);
		if (ret != WM_SUCCESS) {
			httpd_e("Error in sending Content-Length");
			return ret;
		}
	}

	httpd_send_crlf(req->sock);

	/* HTTP Head response does not require any content. It should
	 * contain identical headers as per the corresponding GET request */
	if (req->type != HTTPD_REQ_TYPE_HEAD) {
		if (chunked_encoding(req)) {
			ret = httpd_send_chunk(req->sock, content, length);
			if (ret != WM_SUCCESS) {
				httpd_e("Error in sending response content");
				return ret;
			}

			ret = httpd_send_chunk(req->sock, NULL, 0);
			if (ret != WM_SUCCESS)
				httpd_e("Error in sending last chunk");
		} else {
			/* Send our data */
			ret = httpd_send(req->sock, content, length);
			if (ret != WM_SUCCESS)
				httpd_e("Error sending response");
		}
	}
	return ret;
}
int httpd_get_data(httpd_request_t *req, char *content, int length)
{
	int ret;
	char *buf;

	/* Is this condition required? */
	if (req->body_nbytes >= HTTPD_MAX_MESSAGE - 2)
		return -WM_FAIL;


	buf = os_mem_alloc(HTTPD_MAX_MESSAGE);
	if (!buf) {
		httpd_e("Failed to allocate memory for buffer");
		return -WM_FAIL;
	}

	if (!req->hdr_parsed) {
		ret = httpd_parse_hdr_tags(req, req->sock, buf,
			HTTPD_MAX_MESSAGE);

		if (ret != WM_SUCCESS) {
			httpd_e("Unable to parse header tags: 0x%x",
				__builtin_return_address(0));
			goto out;
		} else {
			httpd_d("Headers parsed successfully\r\n");
			req->hdr_parsed = 1;
		}
	}

	/* handle here */
	ret = httpd_recv(req->sock, content,
			length, 0);
	if (ret == -1) {
		httpd_e("Failed to read POST data");
		goto out;
	}
	/* scratch will now have the JSON data */
	content[ret] = '\0';
	req->remaining_bytes -= ret;
	httpd_d("Read %d bytes and remaining %d bytes",
		ret, req->remaining_bytes);
out:
	os_mem_free(buf);
	return req->remaining_bytes;
}

int httpd_get_data_json(httpd_request_t *req, char *content, int length,
				struct json_object *obj)
{
	int ret;
	ret = httpd_get_data(req, content, length);
	if (ret < 0)
		return -WM_FAIL;
	json_object_init(obj, content);
	return ret;
}

static int get_matching_chars(const char *s1, const char *s2)
{
	int match = 0;

	if (s1 != NULL && s2 != NULL) {
		while (*s1++ == *s2++)
			match++;
	}
	return match;

}

/* Function to skip the initial ipaddress/hostname path in a URL */
char *httpd_skip_absolute_http_path(char *request)
{
#define ABS_HTTP_PATH "http://"
	if (!strcmp(request, ABS_HTTP_PATH)) {
		/* This is a full path request instead of just the absolute
		 * path. Skip the initial, ipaddress/hostname part */
		request += strlen(ABS_HTTP_PATH);
		/* fixme: this while loop outside the if block?
		 * if IP address without http:// is entered? */
		while (*request && *request != '/')
			request++;
	}
	httpd_d("The request is %s", request);
	return request;
}

/* Match the request URI against the one that is registered with us. */
int httpd_validate_uri(char *req, const char *api, int flags)
{
	unsigned char *c1 = (unsigned char *)req;
	unsigned char *c2 = (unsigned char *)api;

/* The 'api' contains our reference copy, the 'req' is sent by the user. It may
 * have additional characters in the front. Let us make sure if 'req' contains
 * exactly the same characters as the api first, then lets make exceptions if
 * any.
 */
	while (*c2) {
		if (*c1 != *c2)
			return -1;
		c1++;
		c2++;
	}

	if (flags & APP_HTTP_FLAGS_NO_EXACT_MATCH) {
		/* The contents match as much as the API length, so this is good
		 * enough for us.
		 */
		return 0;
	}

	/* We come here only if we need an exact match. */

	/* '?' terminates the pathname, so this is an exact match. There may be
	 * some additional parameters that follow the '?', but that isn't a part
	 * of the path name.
	 */
	if (*c1 == '?')
		return 0;

	/* Skip over any trailing forward slashes */
	while (*c1 && (*c1 == '/'))
			c1++;

	if (*c1)
		return -1;
	else
		return 0;
}


/* Check if there are any matching WSGI calls, and if so, execute them. */
int httpd_wsgi(httpd_request_t *req_p)
{
	struct httpd_wsgi_call *f;
	int err = -WM_E_HTTPD_NO_HANDLER;
	int index, ret = 0;
	int match_index = -1, cur_char_match = 0;
	bool found;

	char *ptr, *request = httpd_skip_absolute_http_path(req_p->filename);

	httpd_d("httpd_wsgi: looking for %s", request);
	g_wm_stats.wm_hd_wsgi_call++;
	g_wm_stats.wm_hd_time = wmtime_time_get_posix();
	/* IMP: fixme: Need to check for ? at the end or any forward slashes /
	 * httpd_validate URI? */
	for (index = 0; index < MAX_WSGI_HANDLERS; index++) {
		f = calls[index];
		if (f == NULL)
			continue;
		if (f->http_flags & APP_HTTP_FLAGS_NO_EXACT_MATCH) {
			ret = get_matching_chars(request,
						f->uri);
			if (ret > cur_char_match) {
				cur_char_match = ret;
				match_index = index;
			}
		} else {
			if (!strncmp(request, f->uri, strlen(f->uri))) {
				found = 0;
				ptr = request;
				ptr += strlen(f->uri);
				/* '?' terminates a filename */
				if (*ptr == '?')
					found = 1;
				else {
					/* Check for any number of
					 * forward slashes */
					while (*ptr && (*ptr == '/')) {
						ptr++;
					}
					if (!*ptr)
						found = 1;
				}
				if (found) {
					httpd_d("Anchored pattern match: %s",
						f->uri);
					match_index = index;
					/* Break if there is an exact match */
					break;
				}
			}
		}
	}
	if (match_index == -1)
		return err;

	/* Match found. So map the wsgi to this request */
	req_p->wsgi = calls[match_index];
	switch (req_p->type) {
	case HTTPD_REQ_TYPE_HEAD:
	case HTTPD_REQ_TYPE_GET:
		if (calls[match_index]->get_handler)
			err = calls[match_index]->get_handler(req_p);
		else
			return err;
		break;
	case HTTPD_REQ_TYPE_POST:
		if (calls[match_index]->set_handler)
			err = calls[match_index]->set_handler(req_p);
		else
			return err;
		break;
	case HTTPD_REQ_TYPE_PUT:
		if (calls[match_index]->put_handler)
			err = calls[match_index]->put_handler(req_p);
		else
			return err;
		break;
	case HTTPD_REQ_TYPE_DELETE:
		if (calls[match_index]->delete_handler)
			err = calls[match_index]->delete_handler(req_p);
		else
			return err;
		break;
	default:
		return err;
	}

	if (err == WM_SUCCESS)
		return HTTPD_DONE;
	else
		return err;

}

/* Initialise the WSGI handler data structures */
int httpd_wsgi_init(void)
{
	memset(calls, 0, sizeof(calls));

	return WM_SUCCESS;
}
