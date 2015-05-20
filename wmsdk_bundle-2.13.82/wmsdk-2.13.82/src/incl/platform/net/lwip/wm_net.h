/*! \file wm_net.h
 *\brief Network Abstraction Layer
 *
 * This provides the calls related to the network layer. The SDK uses lwIP as
 * the network stack.
 *
 * Here we document the network utility functions provided by the SDK. The
 * detailed lwIP API documentation can be found at:
 * http://lwip.wikia.com/wiki/Application_API_layers
 *
 */
/*
 * Copyright 2008-2013, Marvell International Ltd.
 * All Rights Reserved.
 */

#ifndef _WM_NET_H_
#define _WM_NET_H_

#include <string.h>

#include <lwip/sockets.h>
#include <lwip/netdb.h>
#include <lwip/stats.h>

#include <wm_os.h>
#include <wmtypes.h>

/*
 * fixme: This dependancy of wm_net on wlc manager header should be
 * removed. This is the lowest level file used to access lwip
 * functionality and should not contain higher layer dependancies.
 */
#include <wlan.h>

typedef ip_addr_t	ip_address_str;
typedef ip_addr_t 	in_addr;

#define mDnsInit	mdns_init
#define NET_SUCCESS WM_SUCCESS
#define NET_ERROR (-WM_FAIL)
#define NET_ENOBUFS ENOBUFS

#define NET_BLOCKING_OFF 1
#define NET_BLOCKING_ON	0

/* Error Codes
 * lwIP provides all standard errnos defined in arch.h, hence no need to
 * redefine them here.
 * */

/* To be consistent with naming convention */
#define net_socket(domain, type, protocol) socket(domain, type, protocol)
#define net_select(nfd, read, write, except, timeout) \
			select(nfd, read, write, except, timeout)
#define net_bind(sock, addr, len) bind(sock, addr, len)
#define net_listen(sock, backlog) listen(sock, backlog)
#define net_close(c) close((c))
#define net_accept(sock, addr, len) accept(sock, addr, len)
#define net_shutdown(c, b) shutdown(c, b)
#define net_connect(sock, addr, len) connect(sock, addr, len)
#define net_read(sock, data, len) read(sock, data, len)
#define net_write(sock, data, len) write(sock, data, len)

int net_dhcp_hostname_set(char *);
extern struct stats_ lwip_stats;

static inline int net_get_tx()
{
	return lwip_stats.link.xmit;
}

static inline int net_get_rx()
{
	return lwip_stats.link.recv;
}

static inline int net_get_error()
{
	int temp;
	temp = lwip_stats.link.drop + lwip_stats.link.chkerr +
		lwip_stats.link.lenerr + lwip_stats.link.memerr+
		lwip_stats.link.rterr + lwip_stats.link.proterr+
		lwip_stats.link.opterr + lwip_stats.link.err;
	return temp;
}

static inline int net_set_mcast_interface()
{
	return WM_SUCCESS;
}

static inline int net_socket_blocking(int sock, int state)
{
	return lwip_ioctl(sock, FIONBIO, &state);
}

static inline int net_get_sock_error(int sock)
{
	switch (errno) {
	case EWOULDBLOCK:
		return -WM_E_AGAIN;
	case EBADF:
		return -WM_E_BADF;
	case ENOBUFS:
		return -WM_E_NOMEM;
	default:
		return -WM_FAIL;
	}
}

static inline uint32_t net_inet_aton(const char *cp)
{
	struct in_addr addr;
	inet_aton(cp, &addr);
	return addr.s_addr;
}

static inline int net_gethostbyname(const char *cp, struct hostent **hentry)
{
	struct hostent *he;
	if ((he = gethostbyname(cp)) == NULL) {
		wmprintf("%s failed for %s\n\r", __FUNCTION__, cp);
		return -WM_FAIL;
	}
	*hentry = he;
	return WM_SUCCESS;
}

static inline void net_inet_ntoa(unsigned long addr, char *cp)
{
	struct in_addr saddr;
	saddr.s_addr = addr;
	/* No length, sigh! */
	strcpy(cp, inet_ntoa(saddr));
}

void *net_ip_to_iface(uint32_t ipaddr);

/** Get interface handle from socket descriptor
 *
 * Given a socket descriptor this API returns which interface it is bound with.
 *
 * \param[in] sock socket descriptor
 *
 * \return[out] interface handle
 */
void *net_sock_to_interface(int sock);

void net_wlan_init(void);

/** Get station interface handle
 *
 * Some APIs require the interface handle to be passed to them. The handle can
 * be retrieved using this API.
 *
 * \return station interface handle
 */
void *net_get_sta_handle(void);
#define net_get_mlan_handle() net_get_sta_handle()

/** Get micro-AP interface handle
 *
 * Some APIs require the interface handle to be passed to them. The handle can
 * be retrieved using this API.
 *
 * \return micro-AP interface handle
 */
void *net_get_uap_handle(void);

/** Get WiFi Direct (P2P) interface handle
 *
 * Some APIs require the interface handle to be passed to them. The handle can
 * be retrieved using this API.
 *
 * \return WiFi Direct (P2P) interface handle
 */
void *net_get_wfd_handle(void);

/** Take interface up
 *
 * Change interface state to up. Use net_get_sta_handle(),
 * net_get_uap_handle() or net_get_wfd_handle() to get interface handle.
 *
 * \param[in] intrfc_handle interface handle
 *
 * \return void
 */
void net_interface_up(void *intrfc_handle);

/** Take interface down
 *
 * Change interface state to down. Use net_get_sta_handle(),
 * net_get_uap_handle() or net_get_wfd_handle() to get interface handle.
 *
 * \param[in] intrfc_handle interface handle
 *
 * \return void
 */
void net_interface_down(void *intrfc_handle);

/** Stop DHCP client on given interface
 *
 * Stop the DHCP client on given interface state. Use net_get_sta_handle(),
 * net_get_uap_handle() or net_get_wfd_handle() to get interface handle.
 *
 * \param[in] intrfc_handle interface handle
 *
 * \return void
 */
void net_interface_dhcp_stop(void *intrfc_handle);

int net_configure_address(struct wlan_ip_config *addr, void *intrfc_handle);

void net_configure_dns(struct wlan_ip_config *address, enum wlan_bss_role role);

/** Get interface IP Address in \ref wlan_ip_config
 *
 * This function will get the IP address of a given interface. Use
 * net_get_sta_handle(), net_get_uap_handle() or net_get_wfd_handle() to get
 * interface handle.
 *
 * \param[out] addr \ref wlan_ip_config
 * \param[in] intrfc_handle interface handle
 *
 * \return WM_SUCCESS on success or error code.
 */
int net_get_if_addr(struct wlan_ip_config *addr, void *intrfc_handle);

/** Get interface IP Address
 *
 * This function will get the IP Address of a given interface. Use
 * net_get_sta_handle(), net_get_uap_handle() or net_get_wfd_handle() to get
 * interface handle.
 *
 * \param[out] ip ip address pointer
 * \param[in] intrfc_handle interface handle
 *
 * \return WM_SUCCESS on success or error code.
 */
int net_get_if_ip_addr(uint32_t *ip, void *intrfc_handle);

/** Get interface IP Subnet-Mask
 *
 * This function will get the Subnet-Mask of a given interface. Use
 * net_get_sta_handle(), net_get_uap_handle() or net_get_wfd_handle() to get
 * interface handle.
 *
 * \param[in] mask Subnet Mask pointer
 * \param[in] intrfc_handle interface
 *
 * \return WM_SUCCESS on success or error code.
 */
int net_get_if_ip_mask(uint32_t *mask, void *intrfc_handle);

/** Initialize the network stack
 *
 *
 *  This function initializes the network stack. This function is
 *  called by wlan_start().
 *
 *  Applications may optionally call this function directly:
 *  # if they wish to use the networking stack (loopback interface)
 *  without the wlan functionality.
 *  # if they wish to initialize the networking stack even before wlan
 *  comes up.
 *
 * \note This function may safely be called multiple times.
 */
void net_ipv4stack_init(void);

void net_sockinfo_dump();

void net_stat(char *name);
#ifdef CONFIG_P2P
int netif_get_bss_type();
#endif

static inline void net_diag_stats(char *str, int len)
{
	/* Report stats */
	strncpy(str, "", len);
}

#endif /* _WM_NET_H_ */
