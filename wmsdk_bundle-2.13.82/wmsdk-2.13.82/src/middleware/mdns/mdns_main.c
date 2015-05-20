#include <rtc.h>
#include <wmstdio.h>
#include <stdlib.h>
#include <wmtypes.h>
#include <wm_os.h>
#include <wm_net.h>
#include <mdns_port.h>
#include <mdns.h>
#include <wifi.h>

#ifdef CONFIG_MDNS_DEBUG
#define LOG wmprintf
#else
#define LOG(...)
#endif

os_thread_t mdns_querier_thread;
os_thread_t mdns_responder_thread;
os_thread_stack_define(mdns_querier_thread_stack, 1024);
os_thread_stack_define(mdns_responder_thread_stack, 1024);

void *mdns_thread_create(mdns_thread_entry entry, int id)
{
	void *ret = NULL;
	if (id == MDNS_THREAD_RESPONDER) {
		if (os_thread_create(&mdns_responder_thread, "mdns_resp_thread",
				     (void (*)(os_thread_arg_t))entry, NULL,
				     &mdns_responder_thread_stack,
				     OS_PRIO_3) == WM_SUCCESS) {
			ret = &mdns_responder_thread;
		}
	} else if (id == MDNS_THREAD_QUERIER) {
		if (os_thread_create
		    (&mdns_querier_thread, "mdns_querier_thread",
		     (void (*)(os_thread_arg_t))entry, NULL,
		     &mdns_querier_thread_stack,
		     OS_PRIO_3) == WM_SUCCESS) {
			ret = &mdns_querier_thread;
		}
	}
	return ret;
}

void mdns_thread_delete(void *t)
{
	os_thread_delete((os_thread_t *) t);
}

void mdns_thread_yield(void *t)
{
	os_thread_sleep(0);
}

uint32_t mdns_time_ms(void)
{
	return os_get_timestamp() / 1000;
}

int mdns_rand_range(int n)
{
	int r = rand();
	return r / (RAND_MAX / n + 1);
}

int mdns_socket_mcast(uint32_t mcast_addr, uint16_t port)
{
	int sock;
	int yes = 1;
	unsigned char ttl = 255;
	uint8_t mcast_mac[MLAN_MAC_ADDR_LENGTH];
	struct sockaddr_in in_addr;
	struct ip_mreq mc;
	memset(&in_addr, 0, sizeof(in_addr));
	in_addr.sin_family = AF_INET;
	in_addr.sin_port = port;
	in_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	wifi_get_ipv4_multicast_mac(ntohl(mcast_addr), mcast_mac);
	wifi_add_mcast_filter(mcast_mac);
	sock = net_socket(AF_INET, SOCK_DGRAM, 0);
	if (sock < 0) {
		LOG("error: could not open multicast socket\n");
		return -WM_E_MDNS_FSOC;
	}
#ifdef SO_REUSEPORT
	if (setsockopt(sock, SOL_SOCKET, SO_REUSEPORT, (char *)&yes,
		       sizeof(yes)) < 0) {
		LOG("error: failed to set SO_REUSEPORT option\n");
		net_close(sock);
		return -WM_E_MDNS_FREUSE;
	}
#endif
	setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (char *)&yes, sizeof(yes));

	if (net_bind(sock, (struct sockaddr *)&in_addr, sizeof(in_addr))) {
		net_close(sock);
		return -WM_E_MDNS_FBIND;
	}
/* join multicast group */
	mc.imr_multiaddr.s_addr = mcast_addr;
	mc.imr_interface.s_addr = htonl(INADDR_ANY);
	if (setsockopt(sock, IPPROTO_IP, IP_ADD_MEMBERSHIP, (char *)&mc,
		       sizeof(mc)) < 0) {
		LOG("error: failed to join multicast group\r\n");
		net_close(sock);
		return -WM_E_MDNS_FMCAST_JOIN;
	}
	/* set other IP-level options */
	if (setsockopt(sock, IPPROTO_IP, IP_MULTICAST_TTL, (char *)&ttl,
		       sizeof(ttl)) < 0) {
		LOG("error: failed to set multicast TTL\r\n");
		net_close(sock);
		return -WM_E_MDNS_FMCAST_SET;
	}
	if (net_set_mcast_interface() < 0) {
		LOG("Setting mcast interface failed: %d\r\n",
		net_get_sock_error(sock));
	}
	return sock;
}

int mdns_socket_loopback(uint16_t port, int listen)
{
	int s, one = 1, addr_len, ret;
	struct sockaddr_in addr;

	s = net_socket(PF_INET, SOCK_DGRAM, 0);
	if (s < 0) {
		LOG("error: Failed to create loopback socket.\r\n");
		return -WM_E_MDNS_FSOC;
	}

	if (listen) {
		/* bind loopback socket */
		memset((char *)&addr, 0, sizeof(addr));
		addr.sin_family = PF_INET;
		addr.sin_port = port;
		addr.sin_addr.s_addr = inet_addr("127.0.0.1");
		addr_len = sizeof(struct sockaddr_in);
		setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (char *)&one,
			   sizeof(one));
		ret = net_bind(s, (struct sockaddr *)&addr, addr_len);
		if (ret < 0) {
			LOG("Failed to bind control socket\r\n");
			return -WM_E_MDNS_FBIND;
		}
	}
	return s;
}

int mdns_socket_close(int s)
{
	return net_close(s);
}

