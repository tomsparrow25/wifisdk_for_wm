/*
 * Copyright 2008-2013, Marvell International Ltd.
 * All Rights Reserved.
 */

/** telnetd.c: Telnet Daemon for WMSDK
 */

#include <wmstdio.h>
#include <wm_os.h>
#include <wm_net.h>
#include <telnetd.h>

telnetd_t telnetd;

#define BUF_SIZE 200
struct telnetd_data_read {
	char buff[BUF_SIZE];
	short current_offset;
	short total_data;
} data_read;
stdio_funcs_t *old_stdio_funcs;

static void telnetd_drop_connection(void)
{
	/* The other side disconnects */
	net_close(telnetd.conn);
	data_read.current_offset = -1;
	telnetd.conn = -1;
	c_stdio_funcs = old_stdio_funcs;
	old_stdio_funcs = NULL;
}

int telnetd_printf(char *str)
{
	int len;

	len = strlen(str);
	if (len > 0)
		return send(telnetd.conn, str, len, 0);

	return 0;
}

int telnetd_getchar(uint8_t *inbyte_p)
{
	int error;

	if (data_read.current_offset == -1) {
		/* Exhausted data in receive buffer, read from socket */
		data_read.total_data = recv(telnetd.conn, data_read.buff,
					    sizeof(data_read.buff), 0);
		/* Process data from socket */
		if (data_read.total_data < 0) {
			error = net_get_sock_error(telnetd.conn);
			if (error == -WM_E_AGAIN)
				/* Currently no data available */
				return 0;
			else if (error == -WM_E_BADF) {
				__console_wmprintf("telnetd: Client"
						" closed connection \r\n");
				telnetd_drop_connection();
				return 0;
			} else {
				__console_wmprintf("telnetd: Some"
						" error on receive %d\r\n", error);
				telnetd_stop();
				*inbyte_p = '0';
				return error;
			}
		} else if (data_read.total_data == 0) {
			__console_wmprintf("telnetd: Client closed"
						" connection \r\n");
			telnetd_drop_connection();
			return 0;
		}

		data_read.current_offset = 0;
	}
	/* Data is now present in the buffer, read character from there */
	*inbyte_p = data_read.buff[data_read.current_offset];
	data_read.current_offset++;

	/* Zap \r\n to \r only, since serial consoles (which is the other input
	 * mechanism) just passes \r.
	 */
	if (*inbyte_p == '\r'
	    && data_read.buff[data_read.current_offset] == '\n')
		data_read.current_offset++;

	if (data_read.current_offset == data_read.total_data) {
		/* Buffer is exhausted, set to -1 */
		data_read.current_offset = -1;
	}

	return 1;
}

int telnetd_flush()
{
	/* Nothing here for now */
	return WM_SUCCESS;
}

stdio_funcs_t telnetd_stdio_funcs = {
	telnetd_printf,
	telnetd_flush,
	telnetd_getchar,
};

static void telnetd_thread(os_thread_arg_t data)
{
	int error, new_socket;
	unsigned long addr_from_len;
	struct sockaddr_in addr_from;

	error = net_listen(telnetd.socket, 1);

	if (error < 0) {
		error = net_get_sock_error(telnetd.socket);
		net_close(telnetd.socket);
		telnetd.socket = 0;
		os_thread_self_complete(NULL);
	}

	addr_from_len = sizeof(addr_from);

	/* Accept loop */
	while (telnetd.run) {
		if (telnetd.conn == -1) {
			new_socket = net_accept(telnetd.socket,
						(struct sockaddr *)&addr_from,
						&addr_from_len);
			if (new_socket < 0) {
				error = net_get_sock_error(new_socket);
				net_close(telnetd.socket);
				os_thread_self_complete(NULL);
			}

			/* Make socket non-blocking as it will be used from
			 * interrupt context. */
			error = net_socket_blocking(new_socket,
						    NET_BLOCKING_OFF);
			if (error != NET_SUCCESS) {
				os_thread_self_complete(NULL);
			}

			/* Remember the current connection */
			telnetd.conn = new_socket;
			old_stdio_funcs = c_stdio_funcs;
			c_stdio_funcs = &telnetd_stdio_funcs;
			wmprintf("%s", "\r\n# ");
		}
		/* 5 seconds */
		os_thread_sleep(os_msec_to_ticks(5000));
	}
	os_thread_self_complete(NULL);
}

static int telnetd_init(int port)
{
	struct sockaddr_in addr_listen;
	int addr_len;
	int options;
	int error;

	telnetd.conn = -1;
	data_read.current_offset = -1;

	/* Create a listening socket */
	telnetd.socket = net_socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (telnetd.socket < 0)
		return -WM_E_TELNETD_SOCKET;

	options = 1;
	setsockopt(telnetd.socket,
		   SOL_SOCKET, SO_REUSEADDR, (char *)&options, sizeof(options));

#ifndef CONFIG_LWIP_STACK
	int opt;
	/* Limit Send/Receive queue of socket to 2KB */
	opt = 2048;
	setsockopt(telnetd.socket,
		   SOL_SOCKET, SO_SNDBUF, (char *)&opt, sizeof(opt));
	setsockopt(telnetd.socket,
		   SOL_SOCKET, SO_RCVBUF, (char *)&opt, sizeof(opt));
#endif /* CONFIG_LWIP_STACK */

	addr_listen.sin_family = AF_INET;
	addr_listen.sin_port = htons(port);
	addr_listen.sin_addr.s_addr = htonl(INADDR_ANY);
	addr_len = sizeof(struct sockaddr_in);

	error = net_bind(telnetd.socket, (struct sockaddr *)&addr_listen,
			addr_len);

	if (error < 0)
		return -WM_E_TELNETD_BIND_SOCKET;

	return WM_SUCCESS;
}

static os_thread_t telnetd_server_thread;

/*  Buffer to be used as stack */
os_thread_stack_define(telnetd_server_stack, 1024);

int telnetd_start(int port)
{
	int ret;
	telnetd_init(port);

	if (telnetd.run)
		return -WM_E_TELNETD_SERVER_RUNNING;
	telnetd.run = 1;

	ret = os_thread_create(&telnetd_server_thread,
				"telnetd",
				telnetd_thread,
				0, &telnetd_server_stack, OS_PRIO_3);
	if (ret)
		return -WM_E_TLENETD_THREAD_CREATE;
	return WM_SUCCESS;
}

void telnetd_stop(void)
{

	if (telnetd.run == 0)
		return;

	if (old_stdio_funcs) {
		/* Retrieve original stdio functions */
		c_stdio_funcs = old_stdio_funcs;
		old_stdio_funcs = NULL;
	}

	os_thread_delete(&telnetd_server_thread);
	/* Close all connections */
	net_close(telnetd.conn);
	net_close(telnetd.socket);
	telnetd.conn = telnetd.socket = -1;
	/* Stop the telnetd thread. */
	telnetd.run = 0;

}
