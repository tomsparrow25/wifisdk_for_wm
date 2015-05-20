/*
 * Copyright (C) 2008-2012, Marvell International Ltd.
 * All Rights Reserved.
 */

#include <inttypes.h>
#include <wmstdio.h>
#include <wifi.h>
#include <wm_os.h>
#include <wm_net.h>
#include <wlan.h>
#include <lwip/netifapi.h>
#include <lwip/tcpip.h>
#include <lwip/dns.h>
#include <lwip/dhcp.h>
#include <wmlog.h>

#define net_e(...)                             \
	wmlog_e("net", ##__VA_ARGS__)

#define net_d(...)                             \
	wmlog("net", ##__VA_ARGS__)

struct interface {
	struct netif netif;
	struct ip_addr ipaddr;
	struct ip_addr nmask;
	struct ip_addr gw;
};

static struct interface g_mlan;
static struct interface g_uap;
#ifdef CONFIG_P2P
static struct interface g_wfd;
#endif

err_t lwip_netif_init(struct netif *netif);
err_t lwip_netif_uap_init(struct netif *netif);
#ifdef CONFIG_P2P
err_t lwip_netif_wfd_init(struct netif *netif);
#endif

int net_dhcp_hostname_set(char *hostname)
{
	netif_set_hostname(&g_mlan.netif, hostname);
	return WM_SUCCESS;
}

void net_ipv4stack_init()
{
	static bool tcpip_init_done;
	if (tcpip_init_done)
		return;
	net_d("Initializing TCP/IP stack");
	tcpip_init(NULL, NULL);
	tcpip_init_done = true;
}

void net_wlan_init(void)
{
	static int wlan_init_done;
	int ret;
	if (!wlan_init_done) {
		net_ipv4stack_init();
		g_mlan.ipaddr.addr = INADDR_ANY;
		ret = netifapi_netif_add(&g_mlan.netif, &g_mlan.ipaddr,
					 &g_mlan.ipaddr, &g_mlan.ipaddr, NULL,
					 lwip_netif_init, tcpip_input);
		if (ret) {
			/*FIXME: Handle the error case cleanly */
			net_e("MLAN interface add failed");
		}
		ret = netifapi_netif_add(&g_uap.netif, &g_uap.ipaddr,
					 &g_uap.ipaddr, &g_uap.ipaddr, NULL,
					 lwip_netif_uap_init, tcpip_input);
		if (ret) {
			/*FIXME: Handle the error case cleanly */
			net_e("UAP interface add failed");
		}
#ifdef CONFIG_P2P
		g_wfd.ipaddr.addr = INADDR_ANY;
		ret = netifapi_netif_add(&g_wfd.netif, &g_wfd.ipaddr,
					 &g_wfd.ipaddr, &g_wfd.ipaddr, NULL,
					 lwip_netif_wfd_init, tcpip_input);
		if (ret) {
			/*FIXME: Handle the error case cleanly */
			net_e("P2P interface add failed\r\n");
		}
#endif
		wlan_init_done = 1;
	}

	wlan_wlcmgr_send_msg(WIFI_EVENT_NET_INTERFACE_CONFIG,
				WIFI_EVENT_REASON_SUCCESS, NULL);
	return;
}

static void wm_netif_status_callback(struct netif *n)
{
	if (n->flags & NETIF_FLAG_UP){
		if(n->flags & NETIF_FLAG_DHCP){ 
			if (n->ip_addr.addr != INADDR_ANY) {
				wlan_wlcmgr_send_msg(WIFI_EVENT_NET_DHCP_CONFIG, 
						     WIFI_EVENT_REASON_SUCCESS, NULL);
			} else {
				wlan_wlcmgr_send_msg(WIFI_EVENT_NET_DHCP_CONFIG, 
						     WIFI_EVENT_REASON_FAILURE, NULL);
			}
		} else {
			wlan_wlcmgr_send_msg(WIFI_EVENT_NET_DHCP_CONFIG,
					     WIFI_EVENT_REASON_FAILURE, NULL);
		}
	} else if (!(n->flags & NETIF_FLAG_DHCP)) {
		wlan_wlcmgr_send_msg(WIFI_EVENT_NET_DHCP_CONFIG,
				     WIFI_EVENT_REASON_FAILURE, NULL);
	}
}

static int check_iface_mask(void *handle, uint32_t ipaddr)
{
	uint32_t interface_ip, interface_mask;
	net_get_if_ip_addr(&interface_ip, handle);
	net_get_if_ip_mask(&interface_mask, handle);
	if (interface_ip > 0)
		if ((interface_ip & interface_mask) ==
		    (ipaddr & interface_mask))
			return WM_SUCCESS;
	return -WM_FAIL;
}

void *net_ip_to_interface(uint32_t ipaddr)
{
	int ret;
	void *handle;
	/* Check mlan handle */
	handle = net_get_mlan_handle();
	ret = check_iface_mask(handle, ipaddr);
	if (ret == WM_SUCCESS)
		return handle;

	/* Check uap handle */
	handle = net_get_uap_handle();
	ret = check_iface_mask(handle, ipaddr);
	if (ret == WM_SUCCESS)
		return handle;

	/* If more interfaces are added then above check needs to done for
	 * those newly added interfaces
	 */
	return NULL;
}

void *net_sock_to_interface(int sock)
{
	struct sockaddr_in peer;
	unsigned long peerlen = sizeof(peer);
	void *req_iface = NULL;

	getpeername(sock, (struct sockaddr *)&peer, &peerlen);
	req_iface = net_ip_to_interface(peer.sin_addr.s_addr);
	return req_iface;
}

void *net_get_sta_handle(void)
{
	return &g_mlan;
}

void *net_get_uap_handle(void)
{
	return &g_uap;
}

#ifdef CONFIG_P2P
void *net_get_wfd_handle(void)
{
	return &g_wfd;
}
#endif

void net_interface_up(void *intrfc_handle)
{
	struct interface *if_handle = (struct interface *)intrfc_handle;
	netifapi_netif_set_up(&if_handle->netif);
}

void net_interface_down(void *intrfc_handle)
{
	struct interface *if_handle = (struct interface *)intrfc_handle;
	netifapi_netif_set_down(&if_handle->netif);
}

void net_interface_dhcp_stop(void *intrfc_handle)
{
	struct interface *if_handle = (struct interface *)intrfc_handle;
	netifapi_dhcp_stop(&if_handle->netif);
	netif_set_status_callback(&if_handle->netif,
				NULL);
}

int net_configure_address(struct wlan_ip_config *addr, void *intrfc_handle)
{
	if (!addr)
		return -WM_E_INVAL;
	if (!intrfc_handle)
		return -WM_E_INVAL;

	struct interface *if_handle = (struct interface *)intrfc_handle;

	net_d("configuring interface %s (with %s)",
		(if_handle == &g_mlan) ? "mlan" :
#ifdef CONFIG_P2P
		(if_handle == &g_uap) ? "uap" : "wfd",
#else
		"uap",
#endif
		(addr->addr_type == ADDR_TYPE_DHCP)
		? "DHCP client" : "Static IP");
	netifapi_netif_set_down(&if_handle->netif);
	/* De-register previously registered DHCP Callback for correct
	 * address configuration.
	 */
	netif_set_status_callback(&if_handle->netif,
				NULL);
	if (if_handle == &g_mlan)
		netifapi_netif_set_default(&if_handle->netif);
	switch(addr->addr_type) {
	case ADDR_TYPE_STATIC:
		if_handle->ipaddr.addr = addr->ip;
		if_handle->nmask.addr = addr->netmask;
		if_handle->gw.addr = addr->gw;
		netifapi_netif_set_addr(&if_handle->netif, &if_handle->ipaddr,
					&if_handle->nmask, &if_handle->gw);
		netifapi_netif_set_up(&if_handle->netif);
		break;

	case ADDR_TYPE_DHCP:
		/* Reset the address since we might be transitioning from static to DHCP */
		memset(&if_handle->ipaddr, 0, sizeof(struct ip_addr));
		memset(&if_handle->nmask, 0, sizeof(struct ip_addr));
		memset(&if_handle->gw, 0, sizeof(struct ip_addr));
		netifapi_netif_set_addr(&if_handle->netif, &if_handle->ipaddr,
				&if_handle->nmask, &if_handle->gw);

		netif_set_status_callback(&if_handle->netif,
					wm_netif_status_callback);
		netifapi_dhcp_start(&if_handle->netif);
		break;
	case ADDR_TYPE_LLA:
		/* For dhcp, instead of netifapi_netif_set_up, a
		   netifapi_dhcp_start() call will be used */
		net_e("Not supported as of now...");
		break;
	default:
		break;
	} 

	/* Finally this should send the following event. */
	if ((if_handle == &g_mlan)
#ifdef CONFIG_P2P
	    ||
	    ((if_handle == &g_wfd) && (netif_get_bss_type() == BSS_TYPE_STA))
#endif
	    ) {
		wlan_wlcmgr_send_msg(WIFI_EVENT_NET_STA_ADDR_CONFIG,
					WIFI_EVENT_REASON_SUCCESS, NULL);

		/* XXX For DHCP, the above event will only indicate that the
		 * DHCP address obtaining process has started. Once the DHCP
		 * address has been obtained, another event,
		 * WD_EVENT_NET_DHCP_CONFIG, should be sent to the wlcmgr.
		 */
	} else if ((if_handle == &g_uap)
#ifdef CONFIG_P2P
	    ||
	    ((if_handle == &g_wfd) && (netif_get_bss_type() == BSS_TYPE_UAP))
#endif
	    ) {
		wlan_wlcmgr_send_msg(WIFI_EVENT_UAP_NET_ADDR_CONFIG,
					WIFI_EVENT_REASON_SUCCESS, NULL);
	}

	return WM_SUCCESS;
}

int net_get_if_addr(struct wlan_ip_config *addr, void *intrfc_handle)
{
	struct ip_addr tmp;
	struct interface *if_handle = (struct interface *)intrfc_handle;

	addr->ip = if_handle->netif.ip_addr.addr;
	addr->netmask = if_handle->netif.netmask.addr;
	addr->gw = if_handle->netif.gw.addr;

	tmp = dns_getserver(0);
	addr->dns1 = tmp.addr;
	tmp = dns_getserver(1);
	addr->dns2 = tmp.addr; 

	return WM_SUCCESS;
}

int net_get_if_ip_addr(uint32_t *ip, void *intrfc_handle)
{
	struct interface *if_handle = (struct interface *)intrfc_handle;

	*ip = if_handle->netif.ip_addr.addr;
	return WM_SUCCESS;
}

int net_get_if_ip_mask(uint32_t *nm, void *intrfc_handle)
{
	struct interface *if_handle = (struct interface *)intrfc_handle;

	*nm = if_handle->netif.netmask.addr;
	return WM_SUCCESS;
}

void net_configure_dns(struct wlan_ip_config *address, enum wlan_bss_role role)
{
	struct ip_addr tmp;

	if (address->addr_type == ADDR_TYPE_STATIC) {
		if (role != WLAN_BSS_ROLE_UAP) {
			if (address->dns1 == 0)
				address->dns1 = address->gw;
			if (address->dns2 == 0)
				address->dns2 = address->dns1;
		}
		tmp.addr = address->dns1;
		dns_setserver(0, &tmp);
		tmp.addr = address->dns2;
		dns_setserver(1, &tmp);
	}

	/* DNS MAX Retries should be configured in lwip/dns.c to 3/4 */
	/* DNS Cache size of about 4 is sufficient */
}

void net_sockinfo_dump()
{
	stats_sock_display();
}

void net_stat(char *name)
{
	stats_display(name);
}
