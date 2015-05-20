/*
 *  Copyright 2008-2013 Marvell International Ltd.
 *  All Rights Reserved.
 */

/* httpd.c: This file contains the routines for the main HTTPD server thread and
 * its initialization.
 */

#include <string.h>

#include <wmstdio.h>
#include <wm_os.h>
#include <wm_net.h>
#include <wmassert.h>
#include <httpd.h>
#include <http-strings.h>
#ifdef CONFIG_ENABLE_TLS
#include <wm-tls.h>
#endif /* CONFIG_ENABLE_TLS */

#include "httpd_priv.h"

typedef enum {
	HTTPD_INACTIVE = 0,
	HTTPD_INIT_DONE,
	HTTPD_THREAD_RUNNING,
	HTTPD_THREAD_SUSPENDED,
} httpd_state_t;

httpd_state_t httpd_state;

static os_thread_t httpd_main_thread;

#ifdef CONFIG_ENABLE_HTTPS
static os_thread_stack_define(httpd_stack, 4096 * 2 + 4096);
#else /* ! CONFIG_ENABLE_HTTPS */
static os_thread_stack_define(httpd_stack, 4096 * 2 + 2048);
#endif /* CONFIG_ENABLE_HTTPS */

/* Why HTTPD_MAX_MESSAGE + 2?
 * Handlers are allowed to use HTTPD_MAX_MESSAGE bytes of this buffer.
 * Internally, the POST var processing needs a null termination byte and an
 * '&' termination byte.
 */
static bool httpd_stop_req;

#define HTTPD_CLIENT_SOCK_TIMEOUT 10
#define HTTPD_TIMEOUT_EVENT 0

/** Maximum number of backlogged http connections
 *
 *  httpd has a single listening socket from which it accepts connections.
 *  HTTPD_MAX_BACKLOG_CONN is the maximum number of connections that can be
 *  pending.  For example, suppose a webpage contains 10 images.  If a client
 *  attempts to load all 10 of those images at once, only the first
 *  HTTPD_MAX_BACKLOG_CONN attempts can succeed.  Some clients will retry when
 *  the attempts fail; others will limit the maximum number of open connections
 *  that it has.  But some may attempt to load all 10 simultaneously.  If your
 *  web pages have many images, or css files, or java script files, you may
 *  need to increase this number.
 *
 *  \note Your underlying TCP/IP stack may have other limitations
 *  besides the backlog.  For example, the treck stack limits the
 *  number of system-wide TCP sockets to TM_OPTION_TCP_SOCKETS_MAX.
 *  You will have to adjust this value if you need more than
 *  TM_OPTION_TCP_SOCKETS_MAX simultaneous TCP sockets.
 *
 */
#define HTTPD_MAX_BACKLOG_CONN 5

#ifdef CONFIG_ENABLE_HTTP
static int http_sockfd;
#endif /* CONFIG_ENABLE_HTTP */

#ifdef CONFIG_ENABLE_HTTPS
static int https_sockfd;
tls_handle_t httpd_tls_handle;
httpd_tls_certs_t httpd_tls_certs;
#endif /* CONFIG_ENABLE_HTTPS */

int client_sockfd;
static bool https_active;

bool httpd_is_https_active()
{
	return https_active;
}

static int httpd_close_sockets()
{
	int ret, status = WM_SUCCESS;

#ifdef CONFIG_ENABLE_HTTP
	if (http_sockfd != -1) {
		ret = net_close(http_sockfd);
		if (ret != 0) {
			httpd_w("failed to close http socket: %d",
				net_get_sock_error(http_sockfd));
			status = -WM_FAIL;
		}
		http_sockfd = -1;
	}
#endif /* CONFIG_ENABLE_HTTP */

#ifdef CONFIG_ENABLE_HTTPS
	if (https_sockfd != -1) {
		ret = net_close(https_sockfd);
		if (ret != 0) {
			httpd_w("failed to close https socket: %d",
				net_get_sock_error(https_sockfd));
			status = -WM_FAIL;
		}
		https_sockfd = -1;
	}
#endif /* CONFIG_ENABLE_HTTPS */

	if (client_sockfd != -1) {
#ifdef CONFIG_ENABLE_HTTPS
		if (https_active && httpd_tls_handle) {
			tls_close(&httpd_tls_handle);
		}
#endif /* CONFIG_ENABLE_HTTPS */
		ret = net_close(client_sockfd);
		if (ret != 0) {
			httpd_w("Failed to close client socket: %d",
				net_get_sock_error(client_sockfd));
			status = -WM_FAIL;
		}
		client_sockfd = -1;
	}

	return status;
}

static void httpd_suspend_thread(bool warn)
{
	if (warn) {
		httpd_w("Suspending thread");
	} else {
		httpd_d("Suspending thread");
	}
	httpd_close_sockets();
	httpd_state = HTTPD_THREAD_SUSPENDED;
	os_thread_self_complete(NULL);
}

static int httpd_setup_new_socket(int port)
{
	int one = 1;
	int status, addr_len, sockfd;
	struct sockaddr_in addr_listen;

	/* create listening TCP socket */
	sockfd = net_socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (sockfd < 0) {
		status = net_get_sock_error(sockfd);
		httpd_e("Socket creation failed: Port: %d Status: %d",
			port, status);
		return status;
	}

	setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR,
		   (char *)&one, sizeof(one));
	addr_listen.sin_family = AF_INET;
	addr_listen.sin_port = htons(port);
	addr_listen.sin_addr.s_addr = htonl(INADDR_ANY);
	addr_len = sizeof(struct sockaddr_in);

#ifndef CONFIG_LWIP_STACK
	/* Limit receive queue of listening socket which in turn limits backlog
	 * connection queue size */
	int opt, optlen;
	opt = 2048;
	optlen = sizeof(opt);
	if (setsockopt(sockfd, SOL_SOCKET, SO_RCVBUF,
		       (char *)&opt, optlen) == -1) {
		httpd_w("Unsupported option SO_RCVBUF on Port: %d: Status: %d",
			net_get_sock_error(sockfd), port);
	}
#endif /* CONFIG_LWIP_STACK */

	/* bind insocket */
	status = net_bind(sockfd, (struct sockaddr *)&addr_listen,
			  addr_len);
	if (status < 0) {
		status = net_get_sock_error(sockfd);
		httpd_e("Failed to bind socket on port: %d Status: %d",
			status, port);
		return status;
	}

	status = net_listen(sockfd, HTTPD_MAX_BACKLOG_CONN);
	if (status < 0) {
		status = net_get_sock_error(sockfd);
		httpd_e("Failed to listen on port %d: %d.", port, status);
		return status;
	}

	httpd_d("Listening on port %d.", port);
	return sockfd;
}

static int httpd_setup_main_sockets()
{
#ifdef CONFIG_ENABLE_HTTP
	http_sockfd = httpd_setup_new_socket(HTTP_PORT);
	if (http_sockfd < 0) {
		/* Socket creation failed */
		return http_sockfd;
	}
#endif /* CONFIG_ENABLE_HTTP */

#ifdef CONFIG_ENABLE_HTTPS
	https_sockfd = httpd_setup_new_socket(HTTPS_PORT);
	if (https_sockfd < 0) {
		/* Socket creation failed */
		return https_sockfd;
	}
#endif /* CONFIG_ENABLE_HTTPS */

	return WM_SUCCESS;
}

static int httpd_select(int max_sock, const fd_set *readfds,
			 fd_set *active_readfds, int timeout_secs)
{
	int activefds_cnt;
	struct timeval timeout;

	fd_set local_readfds;

	if (timeout_secs >= 0)
		timeout.tv_sec = timeout_secs;
	timeout.tv_usec = 0;

	memcpy(&local_readfds, readfds, sizeof(fd_set));
	httpd_d("WAITING for activity");

	activefds_cnt = net_select(max_sock + 1, &local_readfds,
				   NULL, NULL,
				   timeout_secs >= 0 ? &timeout : NULL);
	if (activefds_cnt < 0) {
		httpd_e("Select failed: %d", timeout_secs);
		httpd_suspend_thread(true);
	}

	if (httpd_stop_req) {
		httpd_d("HTTPD stop request received");
		httpd_stop_req = FALSE;
		httpd_suspend_thread(false);
	}

	if (activefds_cnt) {
		/* Update users copy of fd_set only if he wants */
		if (active_readfds)
			memcpy(active_readfds, &local_readfds,
			       sizeof(fd_set));
		return activefds_cnt;
	}

	httpd_d("TIMEOUT");

	return HTTPD_TIMEOUT_EVENT;
}

static int  httpd_accept_client_socket(const fd_set *active_readfds)
{
	int main_sockfd = -1;
	struct sockaddr_in addr_from;
	unsigned long addr_from_len;

#ifdef CONFIG_ENABLE_HTTPS
	int status;
#endif /* CONFIG_ENABLE_HTTPS */

#ifdef CONFIG_ENABLE_HTTP
	if (FD_ISSET(http_sockfd, active_readfds)) {
		main_sockfd = http_sockfd;
		https_active  = FALSE;
	}
#endif /* CONFIG_ENABLE_HTTP */

#ifdef CONFIG_ENABLE_HTTPS
	if (FD_ISSET(https_sockfd, active_readfds) && (main_sockfd == -1)) {
		main_sockfd = https_sockfd;
		https_active = TRUE;
	}
#endif /* CONFIG_ENABLE_HTTPS */

	ASSERT(main_sockfd != -1);

	addr_from_len = sizeof(addr_from);

	client_sockfd = net_accept(main_sockfd, (struct sockaddr *)&addr_from,
				   &addr_from_len);
	if (client_sockfd < 0) {
		httpd_e("net_accept client socket failed %d (errno: %d).",
			client_sockfd, errno);
		return -WM_FAIL;
	}

	/*
	 * Enable TCP Keep-alive for accepted client connection
	 *  -- By enabling this feature TCP sends probe packet if there is
	 *  inactivity over connection for specfied interval
	 *  -- If there is no response to probe packet for specified retries
	 *  then connection is closed with RST packet to peer end
	 *  -- Ref: http://tldp.org/HOWTO/html_single/TCP-Keepalive-HOWTO/
	 *
	 * We are doing this as we have single threaded web server with
	 * synchronous (blocking) API usage like send, recv and they might get
	 * blocked due to un-availability of peer end, causing web server to
	 * be in-responsive forever.
	 */
	int optval = true;
	if (setsockopt(client_sockfd, SOL_SOCKET, SO_KEEPALIVE,
					&optval, sizeof(optval)) == -1) {
		httpd_w("Unsupported option SO_KEEPALIVE: %d",
				net_get_sock_error(client_sockfd));
	}

	/* TCP Keep-alive idle/inactivity timeout is 10 seconds */
	optval = 10;
	if (setsockopt(client_sockfd, IPPROTO_TCP, TCP_KEEPIDLE,
					&optval, sizeof(optval)) == -1) {
		httpd_w("Unsupported option TCP_KEEPIDLE: %d",
				net_get_sock_error(client_sockfd));
	}

	/* TCP Keep-alive retry count is 5 */
	optval = 5;
	if (setsockopt(client_sockfd, IPPROTO_TCP, TCP_KEEPCNT,
					&optval, sizeof(optval)) == -1) {
		httpd_w("Unsupported option TCP_KEEPCNT: %d",
				net_get_sock_error(client_sockfd));
	}

	/* TCP Keep-alive retry interval (in case no response for probe
	 * packet) is 1 second.
	 */
	optval = 1;
	if (setsockopt(client_sockfd, IPPROTO_TCP, TCP_KEEPINTVL,
					&optval, sizeof(optval)) == -1) {
		httpd_w("Unsupported option TCP_KEEPINTVL: %d",
				net_get_sock_error(client_sockfd));
	}

#ifndef CONFIG_LWIP_STACK
	int opt, optlen;
	opt = 2048;
	optlen = sizeof(opt);
	if (setsockopt(client_sockfd, SOL_SOCKET, SO_SNDBUF,
		       (char *)&opt, optlen) == -1) {
		httpd_w("Unsupported option SO_SNDBUF: %d",
			net_get_sock_error(client_sockfd));
	}
#endif /* CONFIG_LWIP_STACK */

	httpd_d("connecting %d to %x:%d.", client_sockfd,
	      addr_from.sin_addr.s_addr, addr_from.sin_port);

#ifdef CONFIG_ENABLE_HTTPS
	if (https_active) {
		const tls_init_config_t tls_server_cfg = {
			.flags                                      = TLS_SERVER_MODE,
			/* From the included header file */
			.tls.server.server_cert = httpd_tls_certs.server_cert,
			.tls.server.server_cert_size
			= httpd_tls_certs.server_cert_size,
			.tls.server.server_key = httpd_tls_certs.server_key,
			.tls.server.server_key_size
			= httpd_tls_certs.server_key_size,
			.tls.server.client_cert = httpd_tls_certs.client_cert,
			.tls.server.client_cert_size
			= httpd_tls_certs.client_cert_size,
		};
		
		if (tls_session_init(&httpd_tls_handle, 0, &tls_server_cfg) !=
		    WM_SUCCESS) {
			httpd_e("Failed to init TLS HTTP server. Aborting");
			net_close(client_sockfd);
			client_sockfd = -1;
			httpd_suspend_thread(true);
		}
		
		status = tls_server_set_clientfd(httpd_tls_handle,
						 client_sockfd);
		if (status != WM_SUCCESS) {
			net_close(client_sockfd);
			client_sockfd = -1;
			httpd_e("TLS Client socket set failed");
			return status;
		}
	}
#endif /* CONFIG_ENABLE_HTTPS */
	return WM_SUCCESS;
}

static void httpd_handle_client_connection(const fd_set *active_readfds)
{
	int activefds_cnt, status;
	fd_set readfds;

	if (httpd_stop_req) {
		httpd_d("HTTPD stop request received");
		httpd_stop_req = FALSE;
		httpd_suspend_thread(false);
	}

	status = httpd_accept_client_socket(active_readfds);
	if (status != WM_SUCCESS)
		return;

	httpd_d("Client socket accepted: %d", client_sockfd);
	FD_ZERO(&readfds);
	FD_SET(client_sockfd, &readfds);

	while (1) {
		if (httpd_stop_req) {
			httpd_d("HTTPD stop request received");
			httpd_stop_req = FALSE;
			httpd_suspend_thread(false);
		}

		httpd_d("Waiting on client socket");
		activefds_cnt = httpd_select(client_sockfd, &readfds, NULL,
					   HTTPD_CLIENT_SOCK_TIMEOUT);

		if (httpd_stop_req) {
			httpd_d("HTTPD stop request received");
			httpd_stop_req = FALSE;
			httpd_suspend_thread(false);
		}

		if (activefds_cnt == HTTPD_TIMEOUT_EVENT) {
			/* Timeout has occured */
			httpd_w("Client socket timeout occurred. "
				"Force closing socket");
#ifdef CONFIG_ENABLE_HTTPS
	if (https_active && httpd_tls_handle) {
		tls_close(&httpd_tls_handle);
	}
#endif /* CONFIG_ENABLE_HTTPS */
			status = net_close(client_sockfd);
			if (status != WM_SUCCESS) {
				status = net_get_sock_error(client_sockfd);
				httpd_e("Failed to close socket %d", status);
				httpd_suspend_thread(true);
			}

			client_sockfd = -1;
			break;
		}

		httpd_d("Handling %d", client_sockfd);
		/* Note:
		 * Connection will be handled with call to
		 * httpd_handle_message twice, first for
		 * handling request (WM_SUCCESS) and second
		 * time as there is no more data to receive
		 * (client closed connection) and hence
		 * will return with status HTTPD_DONE
		 * closing socket.
		 */
		/* FIXME: remove this memset if all is working well */
		/* memset(&httpd_message_in[0], 0, sizeof(httpd_message_in)); */
		status = httpd_handle_message(client_sockfd);
		if (status == WM_SUCCESS) {
			/* The handlers are expected more data on the
			   socket */
			continue;
		}

		/* Either there was some error or everything went well */
		httpd_d("Close socket %d.  %s: %d", client_sockfd,
		      status == HTTPD_DONE ? "Handler done" : "Handler failed",
		      status);

#ifdef CONFIG_ENABLE_HTTPS
	if (https_active) {
		tls_close(&httpd_tls_handle);
	}
#endif /* CONFIG_ENABLE_HTTPS */

		status = net_close(client_sockfd);
		if (status != WM_SUCCESS) {
			status = net_get_sock_error(client_sockfd);
			httpd_e("Failed to close socket %d", status);
			httpd_suspend_thread(true);
		}
		client_sockfd = -1;

		break;
	}
}

static void httpd_main(os_thread_arg_t data)
{
	int status, max_sockfd = -1;
	fd_set readfds, active_readfds;

	status = httpd_setup_main_sockets();
	if (status != WM_SUCCESS)
		httpd_suspend_thread(true);

	FD_ZERO(&readfds);

#ifdef CONFIG_ENABLE_HTTP
	FD_SET(http_sockfd, &readfds);
	max_sockfd = http_sockfd;
#endif /* CONFIG_ENABLE_HTTP */

#ifdef CONFIG_ENABLE_HTTPS
	FD_SET(https_sockfd, &readfds);
	if(https_sockfd > max_sockfd)
		max_sockfd = https_sockfd;
#endif /* CONFIG_ENABLE_HTTPS */

	while (1) {
		httpd_d("Waiting on main socket");
		httpd_select(max_sockfd, &readfds, &active_readfds, -1);
		httpd_handle_client_connection(&active_readfds);
	}

	/*
	 * Thread will never come here. The functions called from the above
	 * infinite loop will cleanly shutdown this thread when situation
	 * demands so.
	 */
}

static inline int tcp_local_connect(int *sockfd)
{
	uint16_t port;
	int retry_cnt = 3;

	httpd_d("Doing local connect for shutting down server\n\r");

	*sockfd = -1;
	while (retry_cnt--) {
		*sockfd = net_socket(AF_INET, SOCK_STREAM, 0);
		if (*sockfd >= 0)
			break;
		/* Wait some time to allow some sockets to get released */
		os_thread_sleep(os_msec_to_ticks(1000));
	}

	if (*sockfd < 0) {
		httpd_e("Unable to create socket to stop server");
		return -WM_FAIL;
	}


#ifdef CONFIG_ENABLE_HTTPS
	port = HTTPS_PORT;
#else
	port = HTTP_PORT;
#endif /* CONFIG_ENABLE_HTTPS */

	char *host = "127.0.0.1";
	struct sockaddr_in addr;
	memset(&addr, 0, sizeof(struct sockaddr_in));

	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	addr.sin_addr.s_addr = inet_addr(host);
	memset(&(addr.sin_zero), '\0', 8);

	httpd_d("local connecting ...");
	if (connect(*sockfd, (const struct sockaddr *)&addr,
		    sizeof(addr)) != 0) {
		httpd_e("Server close error. tcp connect failed %s:%d",
			host, port);
		net_close(*sockfd);
		*sockfd = 0;
		return -WM_FAIL;
	}

	/*
	 * We do not wish to do anything with this connection. Its sole
	 * purpose was to wake the main httpd thread out of sleep.
	 */

	return WM_SUCCESS;
}


static int httpd_signal_and_wait_for_halt()
{
	const int total_wait_time_ms = 1000 * 20;	/* 20 seconds */
	const int check_interval_ms = 100;	/* 100 ms */

	int num_iterations = total_wait_time_ms / check_interval_ms;

	httpd_d("Sent stop request");
	httpd_stop_req = TRUE;

	/* Do a dummy local connect to wakeup the httpd thread */
	int sockfd;
	int rv = tcp_local_connect(&sockfd);
	if (rv != WM_SUCCESS)
		return rv;

	while (httpd_state != HTTPD_THREAD_SUSPENDED && num_iterations--) {
		os_thread_sleep(os_msec_to_ticks(check_interval_ms));
	}

	net_close(sockfd);
	if (httpd_state == HTTPD_THREAD_SUSPENDED)
		return WM_SUCCESS;

	httpd_e("Timed out waiting for httpd to stop. "
		"Force closed temporary socket");

	httpd_stop_req = FALSE;
	return -WM_FAIL;
}

static int httpd_thread_cleanup(void)
{
	int status = WM_SUCCESS;

	switch (httpd_state) {
	case HTTPD_INIT_DONE:
		/*
		 * We have no threads, no sockets to close.
		 */
		break;
	case HTTPD_THREAD_RUNNING:
		status = httpd_signal_and_wait_for_halt();
		if (status != WM_SUCCESS)
			httpd_e("Unable to stop thread. Force killing it.");
		/* No break here on purpose */
	case HTTPD_THREAD_SUSPENDED:
		status = os_thread_delete(&httpd_main_thread);
		if (status != WM_SUCCESS)
			httpd_w("Failed to delete thread.");
		status = httpd_close_sockets();
		httpd_state = HTTPD_INIT_DONE;
		break;
	default:
		ASSERT(0);
		return -WM_FAIL;
	}

	return status;
}

int httpd_is_running(void)
{
	return (httpd_state == HTTPD_THREAD_RUNNING);
}

/* This pairs with httpd_stop() */
int httpd_start(void)
{
	int status;

	if (httpd_state != HTTPD_INIT_DONE) {
		httpd_w("Already started");
		return WM_SUCCESS;
	}

#ifdef CONFIG_ENABLE_HTTPS
	if (httpd_tls_certs.server_cert == NULL ||
	    httpd_tls_certs.server_key == NULL) {
		httpd_e("Please set the certificates by calling "
			"httpd_use_tls_certificates()");
		return -WM_FAIL;
	}
#endif /* CONFIG_ENABLE_HTTPS */

	status = os_thread_create(&httpd_main_thread, "httpd", httpd_main, 0,
			       &httpd_stack, OS_PRIO_3);
	if (status != WM_SUCCESS) {
		httpd_e("Failed to create httpd thread: %d", status);
		return -WM_FAIL;
	}

	httpd_state = HTTPD_THREAD_RUNNING;
	return WM_SUCCESS;
}

/* This pairs with httpd_start() */
int httpd_stop(void)
{
	ASSERT(httpd_state >= HTTPD_THREAD_RUNNING);
	return httpd_thread_cleanup();
}

/* This pairs with httpd_init() */
int httpd_shutdown(void)
{
	int ret;
	ASSERT(httpd_state >= HTTPD_INIT_DONE);

	httpd_d("Shutting down.");

	ret = httpd_thread_cleanup();
	if (ret != WM_SUCCESS)
		httpd_e("Thread cleanup failed");

	httpd_state = HTTPD_INACTIVE;

	return ret;
}

/* This pairs with httpd_shutdown() */
int httpd_init()
{
	int status;

	if (httpd_state != HTTPD_INACTIVE)
		return WM_SUCCESS;

	httpd_d("Initializing");

	client_sockfd = -1;
#ifdef CONFIG_ENABLE_HTTP
	http_sockfd  = -1;
#endif /* CONFIG_ENABLE_HTTP */
#ifdef CONFIG_ENABLE_HTTPS
	https_sockfd  = -1;
#endif /* CONFIG_ENABLE_HTTPS */

	status = httpd_wsgi_init();
	if (status != WM_SUCCESS) {
		httpd_e("Failed to initialize WSGI!");
		return status;
	}

	status = httpd_ssi_init();
	if (status != WM_SUCCESS) {
		httpd_e("Failed to initialize SSI!");
		return status;
	}

	httpd_state = HTTPD_INIT_DONE;

	return WM_SUCCESS;
}

int httpd_use_tls_certificates(const httpd_tls_certs_t *tls_certs)
{
#ifdef CONFIG_ENABLE_HTTPS
	ASSERT(tls_certs->server_cert != NULL);
	ASSERT(tls_certs->server_key != NULL);
	/* Client can be NULL */
	httpd_d("Setting TLS certificates");
	memcpy(&httpd_tls_certs, tls_certs, sizeof(httpd_tls_certs_t));
	return WM_SUCCESS;
#else /* ! CONFIG_ENABLE_HTTPS */
	httpd_w("HTTPS is not enabled in server. "
		"Superfluous call to %s", __func__);
	return -WM_FAIL;
#endif /* CONFIG_ENABLE_HTTPS */
}
