/* 
 *  Copyright (C) 2008-2012, Marvell International Ltd.
 *  All Rights Reserved.
 */

#ifndef __DHCP_PRIV_H__
#define __DHCP_PRIV_H__

#include <wmlog.h>

#define dhcp_e(...)				\
	wmlog_e("dhcp", ##__VA_ARGS__)
#define dhcp_w(...)				\
	wmlog_w("dhcp", ##__VA_ARGS__)

#ifdef CONFIG_DHCP_SERVER_DEBUG
#define dhcp_d(...)				\
	wmlog("dhcp", ##__VA_ARGS__)
#else
#define dhcp_d(...)
#endif /* ! CONFIG_DHCP_DEBUG */

int dhcp_server_init(void *intrfc_handle);
void dhcp_clean_sockets();
void dhcp_server(os_thread_arg_t data);
int dhcp_send_halt(void);

#endif
