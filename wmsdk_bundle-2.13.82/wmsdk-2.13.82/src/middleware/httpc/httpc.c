/*! \file httpclient.c
 *  \brief HTTP client API implementation.
 *
 * This is the HTTP client.
 *
 *  Copyright 2008-2013, Marvell International Ltd.
 *  All Rights Reserved.
 */

#include <string.h>
#include <ctype.h>

#include <wmtypes.h>
#include <wmassert.h>
#include <wmstdio.h>
#include <wmtime.h>
#include <wm_os.h>
#include <wm_net.h>
#include <wm_utils.h>
#include <wmstats.h>

#ifdef CONFIG_ENABLE_TLS
#include <wm-tls.h>
#endif /* CONFIG_ENABLE_TLS */

#include <httpc.h>

#include <wmlog.h>
#define httpc_e(...)				\
	wmlog_e("httpc", ##__VA_ARGS__)
#define httpc_w(...)				\
	wmlog_w("httpc", ##__VA_ARGS__)

#ifdef CONFIG_HTTPC_DEBUG
#define httpc_d(...)				\
	wmlog("httpc", ##__VA_ARGS__)
#else
#define httpc_d(...)
#endif /* ! CONFIG_HTTPC_DEBUG */

/* #define CONFIG_HTTPC_FUNC_DEBUG */

#ifdef CONFIG_HTTPC_FUNC_DEBUG
#define httpc_entry(...)			\
	wmlog_entry(__VA_ARGS__)
#else
#define httpc_entry(...)
#endif /* ! CONFIG_HTTPC_DEBUG */

/* #define DUMP_RECV_DATA */

#define PARSE_EXTRA_BYTES 10
#define DEFAULT_RETRY_COUNT 3

#define DEFAULT_HTTP_PORT 80
#define DEFAULT_HTTPS_PORT 443

/** Max amount of time in mS, the socket functions user (httpc module) will
    retry in case there are timeouts */
#define DEF_INT_MAX_TO_MS 1000

/*
 * We are not sure of the exact header size. Instead of reading one octet
 * at a time we will have prefetch buffer. This buffer will be used to read
 * the either header and then this header is parsed.
 * This prefetch buffer is also used as a buffer to store the request
 * header, before sending an HTTP request.
 */
#define MAX_REQ_RESP_HDR_SIZE 2048	/* 1024 */
/*
 * Reuse prefetch buffer to extract the proper hostname from the user given
 * scheme://hostname:port combination.
 */
#define MAX_HOSTNAME_LEN MAX_REQ_RESP_HDR_SIZE
/*
 * Chunk header is the data structure before each chunk. Chunked data is a
 * type of transfer encoding.
 */
#define MAX_CHUNK_HEADER_SIZE 32
#define PROTOCOL_STR "HTTP"
#define PROTOCOL_VER_1_0 "1.0"
#define PROTOCOL_VER_1_1 "1.1"

#define DEFAULT_USER_AGENT "WMSDK"

#define NEXT_STR(str) (char *)(str + strlen(str) + 1)

typedef enum {
	SESSION_ALLOC,
	SESSION_INIT_DONE,
	SESSION_PREPARE,
	SESSION_REQ_DONE,
	SESSION_RESP_DONE,
	SESSION_DATA_IN_PROGRESS,
	SESSION_COMPLETE,
	SESSION_ERROR,		/* FIXME: Should we flush the socket stream
				   during close, in this state? */
	SESSION_RAW,
} session_state_t;

typedef enum {
	STATE_STANDARD,
	STATE_START_CHUNK,
	STATE_BETWEEN_CHUNK,
	STATE_END_CHUNK,
	STATE_ERROR_CHUNK,
} chunk_state_t;

/*
 * Prefetch buffer info.
 */
typedef struct {
	char *buf;
	int size_read;		/* The total data in the buffer including
				   the header */
	int offset;		/* The offset (excluding) upto which data
				   is processed */
	/* This caches the offset of name-value pairs in the http header */
	int hdr_fields_start_offset;
} prefetch_t;

typedef struct {
	chunk_state_t state;
	int size;
} chunk_t;

#define HOSTNM_MAX	255
typedef struct {
	int outstanding_requests; /* Used for interleaved mode */
	session_state_t state;
	int sockfd;		/* Socket descriptor. Not exposed to user */
	char hostname[HOSTNM_MAX + 1];	/* Hostname of connecting server */
	prefetch_t pbuf;	/* Prefetch buffer */
	http_resp_t resp;	/* The response structure. */
	chunk_t chunk;		/* For chunked transfers  */
	int content_returned;	/* content bytes returned till now */
#ifdef CONFIG_ENABLE_TLS
	tls_handle_t tls_handle;	/* tls handle for tls connections */
#endif				/* CONFIG_ENABLE_TLS */
} session_t;


/**
 * @internal
 *
 * @note If this function returns WM_E_HTTPC_SOCKET_ERROR then errno should
 * be read to get more information.
 */
static int _http_raw_recv(session_t *s, char *buf, int maxlen,
			  int wait_for_to)
{
	ASSERT(maxlen != 0);

	int rv;
	uint64_t ceiling = os_total_ticks_get() + DEF_INT_MAX_TO_MS;

	while (1) {
#ifdef CONFIG_ENABLE_TLS
		if (s->tls_handle) {
			rv = tls_recv(s->tls_handle, buf, maxlen);
		} else {
#endif
			rv = recv(s->sockfd, buf, maxlen, 0);
#ifdef CONFIG_ENABLE_TLS
		}
#endif

		if (rv > 0)
			break;
		if (rv == 0)
			return -WM_E_HTTPC_SOCKET_SHUTDOWN;
		/* rv is < 0 */
		if (errno != EAGAIN || /* Not a timeout issue */
			!wait_for_to || /* Caller does not us to wait */
			os_total_ticks_get() > ceiling)
			return -WM_E_HTTPC_SOCKET_ERROR;
	}

	return rv;
}

static int _http_raw_send(session_t *s, const char *buf, int len)
{
#ifdef CONFIG_ENABLE_TLS
	if (s->tls_handle)
		return tls_send(s->tls_handle, buf, len);
	else
#endif
		return send(s->sockfd, buf, len, 0);
}

int httpc_write_chunked(http_session_t handle, const char *data, int len)
{
	session_t *s = (session_t *) handle;
	if (!handle || (len && !data)) {
		httpc_e("Invalid params");
		return -WM_E_INVAL;
	}
	if (!(s->state == SESSION_REQ_DONE &&
	      s->chunk.state == STATE_START_CHUNK))  {
		httpc_e("Cannot write data. session state: %d,"
			" session chunk state: %d", s->state, s->chunk.state);
		return -WM_FAIL;
	}

	int post_len = sprintf(s->pbuf.buf, "%x\r\n", len);
	int sent = _http_raw_send(s, s->pbuf.buf, post_len);
	if (sent != post_len) {
		s->chunk.state = STATE_ERROR_CHUNK;
		return sent;
	}
	if (len) {
		sent = _http_raw_send(s, data, len);
		if (sent != len) {
			s->chunk.state = STATE_ERROR_CHUNK;
			return sent;
		}

		sent = _http_raw_send(s, "\r\n", 2);
		if (sent != 2) {
			s->chunk.state = STATE_ERROR_CHUNK;
			return sent;
		}
	} else {
		sent = _http_raw_send(s, "\r\n", 2);
		if (sent != 2) {
			s->chunk.state = STATE_ERROR_CHUNK;
			return sent;
		}

		s->chunk.state = STATE_END_CHUNK;
	}
	return WM_SUCCESS;
}

/**
 * @internal
 *
 * Read the stream until header delimiter is found in the read
 * blocks. Store these read octets into the prefetch buffer.
 */
static int prefetch_header(session_t *s)
{
	prefetch_t *pbuf = &s->pbuf;
	/* One less so that delimit strings */
	int max_read_size = MAX_REQ_RESP_HDR_SIZE - 1;
	int total_read_size = 0;
	int size_read;
	httpc_d("Start header read");
	memset(pbuf->buf, 0x00, MAX_REQ_RESP_HDR_SIZE);
	while (max_read_size) {
		int read_start_offset = total_read_size;
		size_read = _http_raw_recv(s, &pbuf->buf[read_start_offset],
					   max_read_size, true);
		httpc_d("Header read: %d bytes errno  = %d",
			size_read < 0 ? get_code(size_read) : size_read,
			errno);
		if (size_read < 0) {
			if (total_read_size != 0) {
				/* Unrecoverable condition */
				s->state = SESSION_ERROR;
			}

			return size_read;
		}

		total_read_size += size_read;
		pbuf->buf[total_read_size] = 0;
		/*
		 * Check if just read data has the header end
		 *  delimiter. The delimiter may have started in tail bytes
		 *  of last read buffer.
		 */
		int check_offset =
		    read_start_offset > 4 ? read_start_offset - 4 : 0;
		if (strstr(&pbuf->buf[check_offset], "\r\n\r\n")) {
			/*
			 * Even if we have found the delimiter in the read
			 * buffer, the buffer may have some content octets
			 * after the header. This will be read later by the
			 * prefetch-buffer-aware read function.
			 */
			pbuf->size_read = total_read_size;
			pbuf->offset = 0;
			return WM_SUCCESS;
		}

		max_read_size -= size_read;
	}

	httpc_e("Header not found.");
	httpc_e("The stream may be in inconsistent state or");
	httpc_e("in a rare case, the header may be bigger that %d bytes",
		MAX_CHUNK_HEADER_SIZE - 1);
	return -WM_FAIL;
}

/**
 * @internal
 * Prefetch buffer aware network read.
 *
 * All network read requests except header reads go through this
 * function. For the reads, the data not yet read from the prefetch buffer
 * will be returned. If prefetch buffer is empty then the TCP stream will
 * be read directly.
 *
 * If timeout (wait_for_to) parameter is 'true' then this function will
 * re-try read for DEF_INT_MAX_TO_MS time. Note to caller: Set this
 * parameter to true if you are doing a task transparent to the httpc caller.
 */
static int recv_buf(session_t *s, char *buf, int max_read_size,
		    bool wait_for_to)
{
	/* The header is already read */
	int r;
	prefetch_t *pbuf = &s->pbuf;

	/* Check if pre-fetched data is already read */
	if (pbuf->offset == pbuf->size_read) {
		r = _http_raw_recv(s, buf, max_read_size, wait_for_to);
		httpc_d("Using actual read: %d %d", r, max_read_size);
		return r;
	}

	/* There is some prefetched data left. */
	int prefetch_remaining = (pbuf->size_read - pbuf->offset);
	int to_copy = prefetch_remaining <= max_read_size ?
	    prefetch_remaining : max_read_size;

	httpc_d("Using prefetch: %d %d %d %d",
		pbuf->offset, prefetch_remaining, to_copy, max_read_size);
	memcpy(buf, &pbuf->buf[pbuf->offset], to_copy);
	pbuf->offset += to_copy;


	return to_copy;
}

/**
 * Create a socket and connect to the peer. The 'hostname' parameter can
 * either be a hostname or a ip address.
 */
static int tcp_socket(int *sockfd, struct sockaddr_in *addr,
		      char *hostname, uint16_t port, int retry_cnt)
{
	char *host = hostname;

	if (isalpha(hostname[0])) {
		/* hostname is in human readable form. get the
		   corresponding IP address */
		struct hostent *entry = NULL;
		net_gethostbyname(hostname, &entry);

		if (entry) {
			struct sockaddr_in tmp;
			memset(&tmp, 0, sizeof(struct sockaddr_in));
			memcpy(&tmp.sin_addr.s_addr, entry->h_addr_list[0],
			       entry->h_length);
			host = inet_ntoa(tmp.sin_addr);
		} else {
			httpc_e("No entry for host %s found", hostname);
			g_wm_stats.wm_ht_dns_fail++;
			return -WM_FAIL;
		}
	}

	if (!retry_cnt)
		retry_cnt = DEFAULT_RETRY_COUNT;
	while (retry_cnt--) {
		*sockfd = net_socket(AF_INET, SOCK_STREAM, 0);
		if (*sockfd >= 0)
			break;
		/* Wait some time to allow some sockets to get released */
		os_thread_sleep(os_msec_to_ticks(1000));
	}

	if (*sockfd < 0) {
		httpc_e("Unable to create socket");
		g_wm_stats.wm_ht_sock_fail++;
		return -WM_FAIL;
	}
#ifndef CONFIG_LWIP_STACK
	int opt;
	/* Limit Send/Receive buffer size of socket to 2KB */
	opt = 2048;
	if (setsockopt(*sockfd, SOL_SOCKET, SO_SNDBUF,
		       (char *)&opt, sizeof(opt)) == -1) {
		httpc_w("Unsupported option SO_SNDBUF: %d",
			net_get_sock_error(*sockfd));
	}

	opt = 2048;
	if (setsockopt(*sockfd, SOL_SOCKET, SO_RCVBUF,
		       (char *)&opt, sizeof(opt)) == -1) {
		httpc_w("Unsupported option SO_RCVBUF: %d",
			net_get_sock_error(*sockfd));
	}
#endif /* CONFIG_LWIP_STACK */

	memset(addr, 0, sizeof(struct sockaddr_in));

	addr->sin_family = AF_INET;
	addr->sin_port = htons(port);
	addr->sin_addr.s_addr = inet_addr(host);
	memset(&(addr->sin_zero), '\0', 8);

#ifdef HTTPC_TCP_NODELAY
	{
		int on = 1;
		socklen_t len = sizeof(on);
		int res =
		    setsockopt(*sockfd, IPPROTO_TCP, TCP_NODELAY, &on, len);
		if (res < 0) {
			httpc_e("setsockopt TCP_NODELAY failed");
			net_close(*sockfd);
			*sockfd = 0;
			return -WM_FAIL;
		}
	}
#endif /* HTTPC_TCP_NODELAY */
	return WM_SUCCESS;
}

/**
 * Create a socket and connect to the given host
 *
 * @param[in,out] sock Pointer to a socket variable to be filled by callee
 * @param[in] hostname Hostname or an IP address
 * @param[in] port Port number
 *
 * @return WM_SUCCESS if the socket was created successfully and -WM_FAIL
 * if failed
 */
static inline int tcp_connect(int *sockfd, char *hostname,
			      uint16_t port, int retry_cnt)
{
	struct sockaddr_in addr;
	int r = tcp_socket(sockfd, &addr, hostname, port, retry_cnt);
	if (r != WM_SUCCESS) {
		httpc_e("Socket creation for %s:%d failed", hostname, port);
		return r;
	}

	httpc_d("Connecting .. %s:%d", hostname, port);
	if (connect(*sockfd, (const struct sockaddr *)&addr,
		    sizeof(addr)) != 0) {
		httpc_e("tcp connect failed %s:%d errno=%d", hostname, port,
			errno);
		if (errno == ECONNRESET)
			g_wm_stats.wm_ht_conn_reset++;
		else if (errno == EHOSTUNREACH)
			g_wm_stats.wm_ht_conn_no_route++;
		else if (errno == ETIMEDOUT)
			g_wm_stats.wm_ht_conn_timeout++;
		else
			g_wm_stats.wm_ht_conn_other++;
		net_close(*sockfd);
		*sockfd = 0;
		return -WM_E_HTTPC_TCP_CONNECT_FAIL;
	}

	httpc_d("Connected .. sockfd: %d", *sockfd);
	return WM_SUCCESS;
}

static inline session_t *new_session_object()
{
	session_t *s = os_mem_alloc(sizeof(session_t));
	if (!s) {
		httpc_e("Could not allocate session object");
		return NULL;
	}

	memset(s, 0x00, sizeof(session_t));
	s->pbuf.buf = os_mem_alloc(MAX_REQ_RESP_HDR_SIZE);
	if (!s->pbuf.buf) {
		httpc_e("Could not allocate prefetch buffer");
		os_mem_free(s);
		return NULL;
	}

	s->state = SESSION_ALLOC;
	return s;
}

static inline void delete_session_object(session_t *s)
{
	if (s->pbuf.buf)
		os_mem_free(s->pbuf.buf);
	os_mem_free(s);
}

static int _http_parse_URL(char *URL, parsed_url_t *parsed_url)
{
	char *pos = URL;
	const char *r;
	char *token;
	bool found_portno = false;

	httpc_d("Parsing: %s", URL);

	parsed_url->portno = DEFAULT_HTTP_PORT;

	/* Check for the scheme */
	token = strstr(pos, "://");
	if (token) {
		*token = 0;
		httpc_d("Scheme: %s", pos);
		parsed_url->scheme = pos;
		pos = token + 3;
	} else {
		httpc_d("No scheme in given URL");
		parsed_url->scheme = NULL;
	}

	parsed_url->hostname = pos;

	/* Check for the port number */
	token = strchr(pos, ':');
	if (token) {
		*token = 0;
		httpc_d("Parse: Hostname: %s", pos);
		/* Skip the ':' */
		token++;
		/*
		 * Set r to start of port string so that we can search for
		 * start of resource later.
		 */
		r = token;
		if (sscanf(token, "%u", &parsed_url->portno) != 1) {
			httpc_e("Port number parsing failed: %s", token);
			return -WM_FAIL;
		}

		found_portno = true;
		httpc_d("Parse: Port No: %d", parsed_url->portno);
	} else {
		httpc_d("No port number given");
		/*
		 * No port number given. So have to search from start of
		 * hostname for resource start
		 */
		r = parsed_url->hostname;
	}

	/* Check for the resource */
	token = strchr(r, '/');
	if (token) {
		/*
		 * Resource is found. We have to move the resource string
		 * (including the NULL termination) to he right by one byte
		 * to NULL terminate the hostname string. However, this can
		 * be avoided if the hostname was postfixed by the port no.
		 */
		if (!found_portno) {
			memmove(token + 1, token, strlen(token) + 1);
			/* NULL terminate the hostname */
			*token = 0;
			token++;
		} else
			*(token - 1) = 0;

		/* point token to the resource */
		httpc_d("Resource: %s", token);
		parsed_url->resource = token;
	} else {
		httpc_d("No resource specified in given URL");
		parsed_url->resource = NULL;
	}

	return WM_SUCCESS;
}

int http_parse_URL(const char *URL, char *tmp_buf, int tmp_buf_len,
		   parsed_url_t *parsed_url)
{
	if (!URL || !tmp_buf || !parsed_url) {
		httpc_e("Cannot parse URL");
		httpc_e("NULL pointer(s) received as argument.");
		return -WM_E_INVAL;
	}

	int min_size_req = strlen(URL) + PARSE_EXTRA_BYTES;
	if (min_size_req > tmp_buf_len) {
		httpc_e("Cannot parse URL");
		httpc_e("Temporary buffer size is less than required");
		return -WM_E_INVAL;
	}

	strcpy(tmp_buf, URL);
	return _http_parse_URL(tmp_buf, parsed_url);
}

int http_open_session(http_session_t *handle,
		      const char *hostname, int flags,
		      const tls_init_config_t *cfg, int retry_cnt)
{
	httpc_entry("hostname: %s", hostname);
	int r;
	session_t *s;

	if (!handle || !hostname) {
		httpc_e("Cannot create session.");
		httpc_e("NULL pointer(s) received as argument.");
		return -WM_E_INVAL;
	}

	s = new_session_object();
	if (!s)
		return -WM_FAIL;

	if (strlen(hostname) >= MAX_HOSTNAME_LEN) {
		httpc_e("Host name corrupt");
		delete_session_object(s);
		return -WM_FAIL;
	}

	parsed_url_t url;
	r = http_parse_URL(hostname, s->pbuf.buf, MAX_REQ_RESP_HDR_SIZE, &url);
	if (r != WM_SUCCESS) {
		delete_session_object(s);
		return r;
	}

	if (url.scheme && strcmp(url.scheme, "https") == 0) {
		httpc_d("Force enable TLS flag");
		flags |= TLS_ENABLE;
		if (url.portno == DEFAULT_HTTP_PORT)
			url.portno = DEFAULT_HTTPS_PORT;
	}

#ifndef CONFIG_ENABLE_TLS
	if (flags & TLS_ENABLE) {
		httpc_e("TLS is not enabled in configuration.");
		httpc_e("Cannot set up a TLS connection.");
		delete_session_object(s);
		return -WM_E_HTTPC_TLS_NOT_ENABLED;
	}
#endif /* CONFIG_ENABLE_TLS */

	httpc_d("Connect: %s Port: %d", url.hostname, url.portno);

	r = tcp_connect(&s->sockfd, (char *)url.hostname, url.portno,
			retry_cnt);
	if (r != WM_SUCCESS) {
		delete_session_object(s);
		*handle = 0;
		return r;
	}

#ifdef CONFIG_ENABLE_TLS
	if (flags & TLS_ENABLE) {
		tls_lib_init();
		httpc_d("TLS init session");
		r = tls_session_init(&s->tls_handle, s->sockfd, cfg);
		if (r != WM_SUCCESS) {
			httpc_e("TLS session init failed");
			net_close(s->sockfd);
			delete_session_object(s);
			return r;
		}
	}
#endif /* CONFIG_ENABLE_TLS */

	/* Save parsed hostname in session structure. This is required to be
	   filled into request header later */
	strncpy(s->hostname, url.hostname, HOSTNM_MAX);
	*handle = (http_session_t) s;
	s->state = SESSION_INIT_DONE;

	return r;
}

static const char *get_version_string(http_ver_t version)
{
	switch (version) {
	case HTTP_VER_1_0:
		return (const char *)PROTOCOL_VER_1_0;
	case HTTP_VER_1_1:
		return (const char *)PROTOCOL_VER_1_1;
	default:
		httpc_e("Unknown version given");
		return NULL;
	}
}

/**
 * @return On success returns string corresponding to the type. On error,
 * returns NULL .
 */
static const char *get_method_string(http_method_t type)
{
	switch (type) {
	case HTTP_GET:		/* retrieve information */
		return (const char *)"GET";
	case HTTP_POST:	/* request to accept new sub-ordinate of resource */
		return (const char *)"POST";
	case HTTP_HEAD:	/* get meta-info */
		return (const char *)"HEAD";
	case HTTP_OPTIONS:	/* request to server for communication
				   options */
	case HTTP_PUT:		/* modify or create new resource referred
				   to by URI */
	case HTTP_DELETE:	/* delete the resource */
	case HTTP_TRACE:	/* echo */
	case HTTP_CONNECT:	/* do we need this  ? */
		httpc_e("Method not yet supported.");
		return NULL;
	default:
		httpc_e("Unknown method given.");
	}

	return NULL;
}

static inline int _http_add_header(session_t *s, const char *name,
				   const char *value)
{
	if (value &&
	    (strlen(s->pbuf.buf) + strlen(name) + strlen(value) + 4) <
	    MAX_REQ_RESP_HDR_SIZE) {
		strcat(s->pbuf.buf, name);
		strcat(s->pbuf.buf, ": ");
		strcat(s->pbuf.buf, value);
		strcat(s->pbuf.buf, "\r\n");
		return WM_SUCCESS;
	}
	return -WM_FAIL;
}

int http_add_header(http_session_t handle,
		    const http_req_t *req __attribute__((unused)),
		    const char *name, const char *value)
{
	httpc_d("Enter: %s", __func__);
	session_t *s = (session_t *) handle;

	if (!handle || !name || !value || s->state != SESSION_PREPARE) {
		httpc_e("Cannot add header");
		return -WM_E_INVAL;
	}

	return _http_add_header(s, name, value);
}

static const char *sanitize_resource_name(session_t *s, const char *resource)
{
	parsed_url_t url;
	int r;
	const char *default_resource = "/";

	if (!resource)
		return default_resource;

	/* Check if the resource string starts with a '/' */
	if (resource[0] != default_resource[0]) {
		httpc_d("Have to extract\n\r");
		/* The resource string is either a valid URL or just garbage */
		r = http_parse_URL(resource, s->pbuf.buf, MAX_REQ_RESP_HDR_SIZE,
				   &url);
		if (r != WM_SUCCESS) {
			httpc_e("Resource extraction error: %d", r);
			return NULL;
		}
		if (!url.resource)
			return default_resource;
		else
			return url.resource;
	} else
		return resource;
}

static inline int _http_send_request(session_t *s, const http_req_t *req)
{
	/* Complete the header */
	strcat(s->pbuf.buf, "\r\n");

	/* Send the header on the network */
	int to_send = strlen(s->pbuf.buf);
	int sent = _http_raw_send(s, s->pbuf.buf, to_send);
	if (sent != to_send) {
		httpc_e("Failed to send header");
		return -WM_E_IO;
	}

	/* Send the data if this was POST request */
	if (req->type == HTTP_POST && req->content_len) {
		sent = _http_raw_send(s, req->content, req->content_len);
		if (sent != req->content_len) {
			httpc_e("Failed to send data.");
			return -WM_E_IO;
		}
	}
	s->outstanding_requests++;
	return WM_SUCCESS;
}

static int _http_prepare_req(session_t *s, const http_req_t *req,
			     http_hdr_field_sel_t field_flags)
{
	const char *method, *version, *resource;
	method = get_method_string(req->type);
	if (!method) {
		httpc_e("HTTPC: Method invalid");
		s->state = SESSION_ERROR;
		return -WM_E_INVAL;
	}

	version = get_version_string(req->version);
	if (!version) {
		httpc_e("HTTPC: version invalid");
		s->state = SESSION_ERROR;
		return -WM_E_INVAL;
	}

	resource = sanitize_resource_name(s, req->resource);
	if (!resource) {
		httpc_e("HTTPC: resource invalid");
		s->state = SESSION_ERROR;
		return -WM_E_INVAL;
	}

	/* Start generating the header */
	sprintf(s->pbuf.buf, "%s %s %s/%s\r\n", method, resource,
		PROTOCOL_STR, version);

	/* Fill in saved hostname */
	_http_add_header(s, "Host", s->hostname);

	if (field_flags & HDR_ADD_DEFAULT_USER_AGENT)
		_http_add_header(s, "User-Agent", DEFAULT_USER_AGENT);

	if (field_flags & HDR_ADD_CONN_KEEP_ALIVE)
		_http_add_header(s, "Connection", "keep-alive");

	if (field_flags & HDR_ADD_CONN_CLOSE) {
		ASSERT(!(field_flags & HDR_ADD_CONN_KEEP_ALIVE));
		_http_add_header(s, "Connection", "close");
	}
	if (field_flags & HDR_ADD_CONTENT_TYPE_JSON) {
		_http_add_header(s, "Content-Type", "application/json");
	}else if(field_flags & HDR_ADD_CONTENT_TYPE_FORM_URLENCODE) {
        _http_add_header(s, "Content-Type", "application/x-www-form-urlencoded; charset=UTF-8");
    }
    
	if (field_flags & HDR_ADD_TYPE_CHUNKED) {
		_http_add_header(s, "Transfer-Encoding", "chunked");
		s->chunk.state = STATE_START_CHUNK;
	} else if (req->type == HTTP_POST) {
		/* If chunked encoding is used for sending data,
		 * Content-Length is not added in header
		 */

		int cur_pos = strlen(s->pbuf.buf);
		sprintf(&s->pbuf.buf[cur_pos], "Content-Length: %d\r\n",
			req->content_len);
	}

	s->state = SESSION_PREPARE;
	return WM_SUCCESS;
}

int http_prepare_req(http_session_t handle, const http_req_t *req,
		     http_hdr_field_sel_t field_flags)
{
	httpc_entry();
	session_t *s = (session_t *) handle;

	if (!handle || !req) {
		httpc_e("Cannot prepare request, invalid parameters."
			" State: %d", s->state);
		return -WM_E_INVAL;
	}
	if (s->state != SESSION_INIT_DONE &&
	    s->state != SESSION_REQ_DONE)  {
		httpc_e("Cannot prepare request, invalid session state."
			" State: %d", s->state);
		return -WM_E_INVAL;
	}
	/*
	 * We are here means either this API was called:
	 * - directly after request was done (Interleaved Requests)
	 *   OR
	 * - after last API called was read content
	 */
	memset(&s->chunk, 0x00, sizeof(chunk_t));
	return _http_prepare_req(s, req, field_flags);
}

int http_send_request(http_session_t handle, const http_req_t *req)
{
	httpc_entry();
	session_t *s = (session_t *) handle;

	httpc_d("Enter: http_send_request");

	if (!req || !handle || s->state != SESSION_PREPARE) {
		httpc_e("Cannot send request");
		return -WM_E_INVAL;
	}

	s->pbuf.size_read = 0;
	s->pbuf.offset = 0;
	s->pbuf.hdr_fields_start_offset = -1;
	memset(&s->resp, 0x00, sizeof(http_resp_t));
	s->content_returned = 0;

	if (req->type == HTTP_POST && !req->content && req->content_len) {
		httpc_e("Cannot send request. Buffer empty.");
		return -WM_E_INVAL;
	}

	int r = _http_send_request(s, req);
	if (r == WM_SUCCESS) {
		s->state = SESSION_REQ_DONE;
		return r;
	}

	s->state = SESSION_ERROR;
	return r;
}

static void parse_keep_alive_header(const char *value, http_resp_t *resp)
{
	int ret;
	if (!strstr(value, "timeout")) {
		httpc_w("No timeout specified in response header.");
		httpc_w("NAK'ing keep alive by force");
		resp->keep_alive_ack = false;
		return;
	}

	/* The header has a timeout value spec */
	ret = sscanf(value, "timeout=%d", &resp->keep_alive_timeout);
	if (ret <= 0) {
		httpc_w("timeout value not found !");
		resp->keep_alive_ack = false;
		return;
	}

	httpc_d("Server timeout value: %d", resp->keep_alive_timeout);
	value = strstr(value, "max");
	if (!value) {
		httpc_w("max value not found !");
		resp->keep_alive_ack = false;
		return;
	}

	ret = sscanf(value, "max=%d", &resp->keep_alive_max);
	if (ret <= 0) {
		httpc_w("max value invalid !");
		resp->keep_alive_ack = false;
		return;
	}

	httpc_d("Alive Max value:%d", resp->keep_alive_max);
	resp->keep_alive_ack = true;
}

/**
 * The parameter off is updated with the new offset pointing at the data,
 * if any.
 * 'len' is the amount of valid data present in the buffer
 *
 * @return On success, i.e. if the header end was found WM_SUCCESS will be
 * returned. On error -WM_FAIL will be returned.
 */
static int load_header_fields(char *header, int len, int *off,
			      http_resp_t *resp)
{
	int offset = *off;
	bool header_done = false;
	char *substr;
	char *pair, *name, *value;

	while (offset < len) {
		if (header[offset] == '\r' && header[offset + 1] == '\n') {
			/* Point to the start of data in case of standard
			   read or start of first chunk header in case of
			   chunked read */
			offset += 2;
			header_done = true;
			break;
		}
		/* FIXME: can this happen ? */
		ASSERT(header[offset] != '\r' && header[offset + 1] != '\n');

		/* Extract the name-value pair */
		pair = &header[offset];
		substr = strstr(pair, "\r\n");
		if (!substr) {
			httpc_e("Header parsing error in pair %s", pair);
			return -WM_FAIL;
		}
		*substr = 0;

		int pair_len = strlen(pair);
		if (((pair + pair_len) - header) > len) {
			httpc_e
			    ("Hdr parsing over bound:len: %d pair_len: %d",
			     len, pair_len);
			httpc_e("#### %s", pair);
			return -WM_FAIL;
		}

		/* Go past the header: value and the header boundary '\r\n' */
		offset += pair_len + 2;

		/* Separate name-value */
		name = pair;
		substr = strstr(name, ": ");
		if (!substr) {
			httpc_e("Header parsing error in name %s", pair);
			return -WM_FAIL;
		}
		*substr = 0;

		/* Skip the name string, the delimited :
		   and the space after that */
		value = name + strlen(name) + 2;

		if (!strcasecmp(name, "Server"))
			resp->server = value;
		if (!strcasecmp(name, "Last-Modified"))
			resp->modify_time = http_date_to_time(value);
		if (!strcasecmp(name, "Content-Type"))
			resp->content_type = value;
		if (!strcasecmp(name, "Connection")) {
			if (!strcasecmp(value, "Keep-Alive")) {
				httpc_d("Server allows keep alive");
				resp->keep_alive_ack = true;
			} else {
				if (resp->keep_alive_ack == true) {
					httpc_d("Keep-Alive present but "
						"connection: Close ?");
				}
				httpc_d("Server does not allow keep alive");
				resp->keep_alive_ack = false;
			}
		}
		if (!strcasecmp(name, "Keep-Alive"))
			parse_keep_alive_header(value, resp);
		if (!strcasecmp(name, "Content-Length"))
			resp->content_length = strtol(value, NULL, 10);
		if (!strcasecmp(name, "Content-Encoding"))
			resp->content_encoding = value;
		if (!strcasecmp(name, "Transfer-Encoding"))
			if (!strcasecmp(value, "chunked"))
				resp->chunked = true;
	}

	if (header_done) {
		*off = offset;
		return WM_SUCCESS;
	} else
		return -WM_FAIL;
}

/**
 * @param[out] data_offset Points to start of data after last byte of header (if any)
 */
static inline int _parse_http_header(char *header, int len,
				     http_resp_t *resp,
				     int *hdr_fields_start_offset,
				     int *data_offset)
{
	int offset = 0;
	char *http_ver = NULL, *status_str = NULL, *substr;
	char *str = header;
	char *delim;

	httpc_d("Parsing HTTP header");
	/* Get the status line */
	delim = strchr(str, '/');
	if (delim) {
		*delim = 0;
		resp->protocol = str;
	}
	if (!resp->protocol || strcasecmp(resp->protocol, PROTOCOL_STR)) {
		httpc_e("Protocol mismatch in header.");
		return -WM_FAIL;
	}

	str = NEXT_STR(resp->protocol);
	delim = strchr(str, ' ');
	if (delim) {
		*delim = 0;
		http_ver = str;
	}
	if (!http_ver) {
		httpc_e("Protocol version not found.");
		return -WM_FAIL;
	} else {
		if (!strcasecmp(http_ver, PROTOCOL_VER_1_0))
			resp->version = HTTP_VER_1_0;
		else if (!strcasecmp(http_ver, PROTOCOL_VER_1_1)) {
			resp->version = HTTP_VER_1_1;
			/*
			 * Since it is HTTP 1.1 we will default to
			 * persistent connection. If there is explicit
			 * "Connection: Close" in the header, this
			 * variable to be reset later to false
			 * automatically.
			 */
			httpc_d("Defaulting to keep alive");
			resp->keep_alive_ack = true;
		} else {
			httpc_e("Protocol version mismatch.");
			return -WM_FAIL;
		}
	}

	str = NEXT_STR(http_ver);
	delim = strchr(str, ' ');
	if (delim) {
		*delim = 0;
		status_str = str;
	}
	if (!status_str) {
		httpc_e("Status code not found.");
		return -WM_FAIL;
	}

	resp->status_code = strtol(status_str, NULL, 10);
	str = NEXT_STR(status_str);
	substr = strstr(str, "\r\n");
	if (substr) {
		*substr = 0;
		resp->reason_phrase = str;
	} else {
		httpc_e("Status code string not found.");
		return -WM_FAIL;
	}

	offset = (NEXT_STR(resp->reason_phrase) + 1 - header);
	*hdr_fields_start_offset = offset;
	load_header_fields(header, len, &offset, resp);
	/*      if (len > offset) { */
	/* buffer has more data to be  processed later */
	*data_offset = offset;
	/*} */

	return WM_SUCCESS;
}

/*
 * Reset variables and close the transaction.
 *
 * @pre Response header is read
 */
static inline void http_close_transaction(session_t *s)
{
	s->outstanding_requests--;
	/* Reset returned content length */
	s->content_returned = 0;
	if (s->resp.keep_alive_ack)
		s->state = SESSION_INIT_DONE;
	else
		s->state = SESSION_COMPLETE;
}

int http_get_response_hdr(http_session_t handle, http_resp_t **resp)
{
	httpc_entry();
	session_t *s = (session_t *) handle;
	if (!handle) {
		httpc_e("Unable to get resp header, invalid params."
			" State: %d", s->state);
		return -WM_E_INVAL;
	}
	if (s->state != SESSION_REQ_DONE &&
	    !(s->state == SESSION_INIT_DONE &&
	      s->outstanding_requests > 0)) {
		httpc_e("Unable to get resp header, invalid session state."
		" State: %d", s->state);
		return -WM_E_INVAL;
	}

	/*
	 * We are here means either this API was called:
	 * - directly after request was done
	 *   OR
	 * - after last API called was read content (Interleaved mode)
	 */

	/*
	 * We read even past the header data. The data after the header
	 * will be returned to the caller in next call to http_read_content.
	 */
	int r = prefetch_header(s);
	if (r != WM_SUCCESS)
		return r;

	r = _parse_http_header(s->pbuf.buf, s->pbuf.size_read, &s->resp,
			       &s->pbuf.hdr_fields_start_offset,
			       &s->pbuf.offset);
	if (r == WM_SUCCESS) {
		httpc_d("Header has used up %d bytes from the %d bytes header",
			s->pbuf.offset, s->pbuf.size_read);
		s->state = SESSION_RESP_DONE;

		if ((s->resp.content_length == 0) &&
		    !s->resp.chunked) {
			httpc_d("No content present. Closing this x'action");
			/* Non-chunked transaction with content length
			   zero. For chunked transfers 's->content_length'
			   variable is always zero.
			 */
			http_close_transaction(s);
		}

		if (resp)
			*resp = &s->resp;
		/* Set the chunk state to start if the transfer encoding is
		 * given as chunked  */
		if (s->resp.chunked) {
			s->chunk.state = STATE_START_CHUNK;
		}
	} else {
		s->state = SESSION_ERROR;
	}

	return r;
}

/* A function very dependant on the design of httpc. It checks for
   '\0\n'. Note that the pair length returned, does not include the NULL
   terminating character. Also note that this function totally assumes that
   the name and value both are NULL terminated already. Basically should
   work because the header as been already parsed once.*/
static inline int get_header_pair(char *buf, int maxlen, int *pair_len,
				  char **name, char **value)
{
	if (buf[0] == '\0')
		return -WM_FAIL;

	bool name_terminator_found = false;
	char *name_term_address;

	/* We are right now at the start of a name-value pair */
	int i;
	for (i = 1; i < maxlen; i++) {
		if (buf[i] == '\0') {
			if (!name_terminator_found) {
				name_terminator_found = true;
				name_term_address = &buf[i];
				continue;
			}

			if ((i + 1) < maxlen && buf[i + 1] == '\n') {
				if ((&buf[i] - name_term_address) < 3) {
					/* Sanity check */
					return -WM_FAIL;
				}
				*name = buf;
				*value = name_term_address + 2;
				*pair_len = i;
				return WM_SUCCESS;
			}
		}
	}

	return -WM_FAIL;
}

static int _http_get_response_hdr_value(session_t *s,
					const char *header_name,
					char **value,
					http_header_pair_t *arr, int *count)
{
	/* We should already know the offset from where the name-value
	   pairs in the http header start */
	ASSERT(s->pbuf.hdr_fields_start_offset != -1);

	/* Create a local copy as this function may be called more than
	   once */
	int offset = s->pbuf.hdr_fields_start_offset;
	/* This is the total size of the header in the buffer */
	const int len = s->pbuf.size_read;
	char *header = s->pbuf.buf;
	int arr_index = 0;
	while (offset < len) {
		if (header[offset] == '\r' && header[offset + 1] == '\n') {
			/* Point to the start of data in case of standard
			   read or start of first chunk header in case of
			   chunked read */
			offset += 2;
			break;
		}
		/* FIXME: can this happen ? */
		ASSERT(header[offset] != '\r' && header[offset + 1] != '\n');

		int pair_len;
		char *name, *val;
		int rv = get_header_pair(&header[offset], len - offset,
					 &pair_len, &name, &val);
		if (rv != WM_SUCCESS) {
			httpc_e("Error in parsing header: %d", rv);
			return rv;
		}
		if (header_name == NULL) {
			/* User has asked us for the entire list */
			arr[arr_index].name = name;
			arr[arr_index].value = val;
			arr_index++;
			if (arr_index == *count)
				break;
			/* Go past the header: value and the header boundary
			   '\0\n' */
			offset += pair_len + 2;
			continue;
		}

		if (strncmp(name, header_name, len - offset)) {
			/* This is not the header caller needs */

			/* Go past the header: value and the header boundary
			   '\0\n' */
			offset += pair_len + 2;
			continue;
		}

		*value = val;
		return WM_SUCCESS;
	}

	/* Comes here only if complete list is required */
	/* Sanity check for single name value requested */
	if (header_name != NULL)
		return -WM_FAIL;

	*count = arr_index;
	return WM_SUCCESS;
}

int http_get_response_hdr_value(http_session_t handle,
				const char *header_name, char **value)
{
	httpc_entry();
	session_t *s = (session_t *) handle;
	if (!handle) {
		httpc_e("Unable to get resp header.");
		return -WM_E_INVAL;
	}

	switch (s->state) {
	case SESSION_RESP_DONE:
	case SESSION_REQ_DONE:
	case SESSION_INIT_DONE:
		if (s->outstanding_requests > 0)
			break;
	default:
		httpc_e("Invalid state: %d. Cannot read hdr value", s->state);
		return -WM_E_INVAL;
	}

	if (s->state == SESSION_REQ_DONE ||  s->state == SESSION_INIT_DONE) {
		/*
		 * The caller has not called http_get_response_hdr. We need
		 * to do it.
		 */
		int status = http_get_response_hdr(handle, NULL);
		if (status != WM_SUCCESS)
			return status;
	}

	/* Failure here is not a session error */
	return _http_get_response_hdr_value(s, header_name, value,
					    NULL, NULL);
}


int http_get_response_hdr_all(http_session_t handle, http_header_pair_t *arr,
			      int *count)
{
	httpc_entry();
	session_t *s = (session_t *) handle;
	if (!handle || !arr || !count) {
		httpc_e("Unable to get resp header.");
		return -WM_E_INVAL;
	}

	switch (s->state) {
	case SESSION_RESP_DONE:
	case SESSION_REQ_DONE:
	case SESSION_INIT_DONE:
		if (s->outstanding_requests > 0)
			break;
	default:
		httpc_e("Invalid state: %d. Cannot read hdr value", s->state);
		return -WM_E_INVAL;
	}

	if (s->state == SESSION_REQ_DONE || s->state == SESSION_INIT_DONE) {
		/*
		 * The caller has not called http_get_response_hdr. We need
		 * to do it.
		 */
		int status = http_get_response_hdr(handle, NULL);
		if (status != WM_SUCCESS)
			return status;
	}

	/* Failure here is not a session error */
	if (*count == 0)
		return WM_SUCCESS;
	return _http_get_response_hdr_value(s, NULL, NULL, arr, count);
}

/**
 * @return On success, returns the amount of data read. On error, return -WM_FAIL
 */
static int http_read_standard(session_t *s, void *buf, uint32_t max_len)
{
	if (s->resp.content_length == s->content_returned) {
		httpc_d("Standard read complete");
		http_close_transaction(s);
		return 0;
	}

	int content_len = s->resp.content_length;
	int size_remaining = content_len - s->content_returned;
	int size_to_read = size_remaining <= max_len ? size_remaining : max_len;
	int size_read = recv_buf(s, buf, size_to_read, false);
	if (size_read < 0) {
		if (
		    (size_read == -WM_E_HTTPC_SOCKET_ERROR &&
		     errno != EAGAIN) ||
		    (size_read == -WM_E_HTTPC_SOCKET_SHUTDOWN)) {
			httpc_e("Error in reading content: %d", size_read);
			s->state = SESSION_ERROR;
		}

		return size_read;
	}
	s->content_returned += size_read;
	s->state = SESSION_DATA_IN_PROGRESS;
	return size_read;
}

/**
 * @pre The buffer should point to the start of the chunk header.
 *
 * @return On success, i.e. if the data size was found correctly. If data
 * size was not found then -WM_FAIL is returned.
 */
static int get_content_len(session_t *s)
{
	char chunk_header[MAX_CHUNK_HEADER_SIZE];
	bool first_delim_found = false;
	int i;
	char c;
	for (i = 0; i < MAX_CHUNK_HEADER_SIZE; i++) {
		int size_read = recv_buf(s, &c, 1, true);
		if (size_read < 0) {
			httpc_e("Error while reading chunk len: %d",
				size_read);
			return size_read;
		}

		chunk_header[i] = c;
		if (first_delim_found) {
			if (chunk_header[i] != '\n') {
				httpc_e("Unable to read len. No newline");
				return -WM_FAIL;
			}
			break;
		}
		if (chunk_header[i] == '\r')
			first_delim_found = true;
	}

	if (i == MAX_CHUNK_HEADER_SIZE) {
		httpc_e("Unable to read length. Header corrupted.");
		return -WM_FAIL;
	}

	/* replace \r with string delim */
	chunk_header[i - 1] = 0;
	httpc_d("Chunk size str (value in hex): %s", chunk_header);
	return strtol(chunk_header, NULL, 16);
}

/*
 * Reads and discards the '\r\n' end part of a chunk block.
 */
static int zap_chunk_boundary(session_t *s)
{
	char chunk_boundary[2];
	int size_read;
	size_read = recv_buf(s, &chunk_boundary[0], 1, true);
	if (size_read != 1) {
		httpc_e("Error while reading chunk len: %d",
			size_read);
		s->chunk.state = STATE_ERROR_CHUNK;
		return size_read;
	}
	size_read = recv_buf(s, &chunk_boundary[1], 1, true);
	if (size_read != 1) {
		httpc_e("Error while reading chunk len: %d",
			size_read);
		s->chunk.state = STATE_ERROR_CHUNK;
		return size_read;
	}

	if (strncmp(chunk_boundary, "\r\n", 2)) {
		s->chunk.state = STATE_ERROR_CHUNK;
		return -WM_FAIL;
	}
	return WM_SUCCESS;
}

/**
 * @return On success, returns the amount of data read. On error, return -WM_FAIL
 */
static int http_read_chunked(session_t *s, void *buf, uint32_t max_len)
{
	if (s->chunk.state == STATE_BETWEEN_CHUNK) {
		/* partial length from a chunk was returned earlier */
		int size_remaining = s->chunk.size - s->content_returned;
		int size_to_read =
		    size_remaining <= max_len ? size_remaining : max_len;
		httpc_d
		    ("Readng %d bytes of chunked data (b/w)... remn'g %d",
		     size_to_read, size_remaining);
		int size_read = recv_buf(s, buf, size_to_read, false);
		if (size_read < 0) {
			if (
			    (size_read == -WM_E_HTTPC_SOCKET_ERROR &&
			     errno != EAGAIN) ||
			    (size_read == -WM_E_HTTPC_SOCKET_SHUTDOWN)) {
				httpc_e("Failed to read partial chunk: %d",
					size_read);
				s->chunk.state = STATE_ERROR_CHUNK;
			}
			return size_read;
		}

		s->content_returned += size_read;
		httpc_d("Read %d bytes of partial chunked data.", size_read);
		httpc_d("Cumulative: %d  Total: %d",
			s->content_returned, s->chunk.size);
		if (s->content_returned == s->chunk.size) {
			/* This chunk is done with */
			if (zap_chunk_boundary(s) == WM_SUCCESS) {
				s->content_returned = 0;
				s->chunk.size = 0;
				s->chunk.state = STATE_START_CHUNK;
			} else {
				httpc_e("Zap chunk boundary failed");
				return -WM_FAIL;
			}
		}
		return size_read;
	} else if (s->chunk.state == STATE_START_CHUNK) {
		/* We are start of chunk header */
		s->chunk.size = get_content_len(s);
		if (s->chunk.size < 0) {
			s->chunk.state = STATE_ERROR_CHUNK;
			s->state = SESSION_ERROR;
			/* Return error code */
			return s->chunk.size;
		}
		httpc_d("Chunk size: %d", s->chunk.size);
		if (s->chunk.size == 0) {
			/* HTTP transaction complete */
			if (zap_chunk_boundary(s) == WM_SUCCESS) {
				s->outstanding_requests--;
				/* Reset returned content length */
				s->content_returned = 0;
				if (s->resp.keep_alive_ack)
					s->state = SESSION_INIT_DONE;
				else
					s->state = SESSION_COMPLETE;
				return 0;
			} else {
				httpc_e("At end of transaction, ");
				httpc_e("could not zap chunk boundary");
				return -WM_FAIL;
			}
		}
		int to_read =
		    s->chunk.size <= max_len ? s->chunk.size : max_len;
		int size_read = recv_buf(s, buf, to_read, false);
		if (size_read < 0) {
			if (
			    (size_read == -WM_E_HTTPC_SOCKET_ERROR &&
			     errno != EAGAIN) ||
			    (size_read == -WM_E_HTTPC_SOCKET_SHUTDOWN)) {
				httpc_e("Failed to read chunk data: %d",
					size_read);
				s->chunk.state = STATE_ERROR_CHUNK;
			}

			return size_read;
		}
		s->content_returned = size_read;
		if (s->content_returned == s->chunk.size) {
			/* This chunk was done with in one go */
			if (zap_chunk_boundary(s) == WM_SUCCESS) {
				s->content_returned = 0;
				s->chunk.size = 0;
				s->chunk.state = STATE_START_CHUNK;
			} else {
				httpc_e("Could not zap chunk boundary");
				return -WM_FAIL;
			}
		} else
			s->chunk.state = STATE_BETWEEN_CHUNK;

		httpc_d("Read %d bytes of start chunked data.", size_read);
		httpc_d("Cumulative: %d  Total: %d",
			s->content_returned, s->chunk.size);
		return size_read;
	} else {
		httpc_e("Unknown state %d", s->chunk.state);
	}

	return -WM_FAIL;
}

int http_read_content(http_session_t handle, void *buf, uint32_t max_len)
{
	httpc_entry();
	if (!handle || !buf || !max_len) {
		httpc_e("Cannot read content.");
		return -WM_E_INVAL;
	}
	int size_read;
	int status;
	session_t *s = (session_t *) handle;
	http_resp_t *resp;

	switch (s->state) {
	case SESSION_INIT_DONE:
		if (s->outstanding_requests == 0) {
			httpc_e("Cannot read content, no outstanding requests"
				". Session state %d", s->state);
			return -WM_FAIL;
		}
	case SESSION_REQ_DONE:
		/*
		 * The caller has not called http_get_response_hdr. We need
		 * to do it.
		 */
		status = http_get_response_hdr(handle, &resp);
		if (status != WM_SUCCESS)
			return status;
		if (resp->status_code != 200) {
			httpc_e("Not reading any content.");
			httpc_e("HTTP status code: %d", resp->status_code);
			return -WM_FAIL;
		}
		break;
	case SESSION_RESP_DONE:
	case SESSION_DATA_IN_PROGRESS:
		if (STATE_ERROR_CHUNK == s->chunk.state)
			return -WM_FAIL;
		break;
	case SESSION_COMPLETE:
	case SESSION_ALLOC:
	case SESSION_PREPARE:
	case SESSION_ERROR:
	case SESSION_RAW:
	default:
		httpc_e("Cannot read content. Unknown state %d", s->state);
		return -WM_FAIL;
	}

	if (s->resp.chunked)
		size_read = http_read_chunked(s, buf, max_len);
	else
		size_read = http_read_standard(s, buf, max_len);

#ifdef DUMP_RECV_DATA
	char *buff = buf;
	int i;
	wmprintf("\n\rHTTPC: ********** HTTPC data *************\n\r");
	if (size_read > 0) {
		for (i = 0; i < size_read; i++) {
			if (buff[i] == '\r')
				continue;
			if (buff[i] == '\n') {
				wmprintf("\n\r");
				continue;
			}
			wmprintf("%c", buff[i]);
		}
		wmprintf("\n\r");
	} else {
		wmprintf("Size: %d Not reading\n\r", size_read);
	}
	wmprintf("HTTPC: ***********************************\n\r");
#endif /* DUMP_RECV_DATA */

	return size_read;
}

void http_close_session(http_session_t *handle)
{
	session_t *s = (session_t *) *handle;

#ifdef CONFIG_ENABLE_TLS
	if (s->tls_handle)
		tls_close(&s->tls_handle);
#endif /* CONFIG_ENABLE_TLS */

	net_close(s->sockfd);
	delete_session_object(s);
	*handle = 0;
}


int http_lowlevel_read(http_session_t handle, void *buf, unsigned maxlen)
{
	if (!handle || !buf || !maxlen) {
		httpc_e("Cannot read lowlevel");
		return -WM_E_INVAL;
	}

	session_t *s = (session_t *)handle;
	if (s->state < SESSION_INIT_DONE || s->state == SESSION_ERROR) {
		httpc_e("Unable to do lowlevel read");
		return -WM_E_INVAL;
	}

	s->state = SESSION_RAW;
	return recv_buf(s, buf, maxlen, false);
}

int http_lowlevel_write(http_session_t handle, const void *buf, unsigned len)
{
	if (!handle || !buf || !len) {
		httpc_e("Cannot write lowlevel");
		return -WM_E_INVAL;
	}

	session_t *s = (session_t *)handle;
	if (s->state < SESSION_INIT_DONE || s->state == SESSION_ERROR) {
		httpc_e("Unable to do lowlevel write");
		return -WM_E_INVAL;
	}

	s->state = SESSION_RAW;
	/* After this point user is not allowed any call apart from
	   http_close_session */
	return _http_raw_send(s, buf, len);
}

int http_setsockopt(http_session_t handle, int level, int optname,
		    const void *optval, socklen_t optlen)
{
	if (!handle)
		return -WM_E_INVAL;
	session_t *s = (session_t *)handle;
	if (s->state < SESSION_INIT_DONE ||
	    s->state == SESSION_ERROR) {
		httpc_e("Unable to do setsockopt. Invalid state: %d\r\n",
			s->state);
		return -WM_FAIL;
	}
	return setsockopt(s->sockfd, level, optname,
			  optval, optlen);
}

int http_getsockopt(http_session_t handle, int level, int optname,
		    void *optval, socklen_t *optlen)
{
	if (!handle)
		return -WM_E_INVAL;

	session_t *s = (session_t *)handle;
	if (s->state < SESSION_INIT_DONE ||
	    s->state == SESSION_ERROR) {
		httpc_e("Unable to do getsockopt. Invalid state: %d\r\n",
			s->state);
		return -WM_FAIL;
	}

	return getsockopt(s->sockfd, level, optname,
			  optval, optlen);
}

int http_get_sockfd_from_handle(http_session_t handle)
{
	if (!handle)
		return -WM_E_INVAL;
	session_t *s = (session_t *)handle;
	return s->sockfd;
}
