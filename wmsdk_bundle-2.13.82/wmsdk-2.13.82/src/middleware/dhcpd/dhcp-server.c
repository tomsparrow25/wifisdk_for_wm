/*
 * Copyright (C) 2008-2012, Marvell International Ltd.
 * All Rights Reserved.
 */

/** dhcp-server.c: The DHCP Server
 */
#include <string.h>

#include <wm_os.h>
#include <wm_net.h>
#include <dhcp-server.h>
#include <wlan.h>
#include <wmstats.h>

#include "dns.h"
#include "dhcp-bootp.h"
#include "dhcp-priv.h"

#ifdef CONFIG_DHCP_SERVER_TESTS
static int capture_response(int sock, struct sockaddr *addr, char *msg,
			    int len);
#define SEND_RESPONSE(w,x,y,z)	capture_response(w,x,y,z)
#else
#define SEND_RESPONSE(w,x,y,z)	send_response(w,x,y,z)
#endif

#define DEFAULT_DHCP_ADDRESS_TIMEOUT	(60U*60U*1U)
#define SERVER_BUFFER_SIZE		1024
#define MAC_IP_CACHE_SIZE		8

uint32_t dhcp_address_timeout = DEFAULT_DHCP_ADDRESS_TIMEOUT;
os_mutex_t dhcp_mutex;
static int ctrl = -1;
#define CTRL_PORT 12679
static char ctrl_msg[16];

struct client_mac_cache {
	uint8_t client_mac[6];    /* mac address of the connected device */
	uint32_t client_ip;       /* ip address of the connected device */
};

static struct {
	int sock;
	int dnsock;
	int count_clients;       /* to keep count of cached devices */
	char msg[SERVER_BUFFER_SIZE];
	char spoofed_name[MAX_QNAME_SIZE + 1];
	struct sockaddr_in saddr;	/* dhcp server address */
	struct sockaddr_in dnaddr;	/* dns server address */
	struct sockaddr_in baddr;	/* broadcast address */
	struct client_mac_cache ip_mac_mapping[MAC_IP_CACHE_SIZE];
	uint32_t netmask;		/* network order */
	uint32_t my_ip;		/* network order */
	uint32_t client_ip;	/* last address that was requested, network
				 * order */
	uint32_t current_ip;     /* keep track of assigned IP addresses */
} dhcps;

static int get_broadcast_addr(struct sockaddr_in *addr);
static int get_ip_addr_from_interface(uint32_t *ip, void *interface_handle);
static int get_netmask_from_interface(uint32_t *nm, void *interface_handle);
static int send_gratuitous_arp(uint32_t ip);
static bool ac_add(uint8_t *chaddr, uint32_t client_ip);
static uint32_t ac_lookup_mac(uint8_t *chaddr);
static uint8_t *ac_lookup_ip(uint32_t client_ip);
static bool ac_not_full();

static bool ac_add(uint8_t *chaddr, uint32_t client_ip)
{
	/* adds ip-mac mapping in cache */
	if (ac_not_full()) {
		dhcps.ip_mac_mapping[dhcps.count_clients].client_mac[0]
			= chaddr[0];
		dhcps.ip_mac_mapping[dhcps.count_clients].client_mac[1]
			= chaddr[1];
		dhcps.ip_mac_mapping[dhcps.count_clients].client_mac[2]
			= chaddr[2];
		dhcps.ip_mac_mapping[dhcps.count_clients].client_mac[3]
			= chaddr[3];
		dhcps.ip_mac_mapping[dhcps.count_clients].client_mac[4]
			= chaddr[4];
		dhcps.ip_mac_mapping[dhcps.count_clients].client_mac[5]
			= chaddr[5];
		dhcps.ip_mac_mapping[dhcps.count_clients].client_ip = client_ip;
		dhcps.count_clients++;
		return WM_SUCCESS;
	}
	return -WM_FAIL;
}

static uint32_t ac_lookup_mac(uint8_t *chaddr)
{
	/* returns ip address, if mac address is present in cache */
	int i;
	for (i = 0; i < dhcps.count_clients && i < MAC_IP_CACHE_SIZE; i++) {
		if ((dhcps.ip_mac_mapping[i].client_mac[0] == chaddr[0]) &&
		    (dhcps.ip_mac_mapping[i].client_mac[1] == chaddr[1]) &&
		    (dhcps.ip_mac_mapping[i].client_mac[2] == chaddr[2]) &&
		    (dhcps.ip_mac_mapping[i].client_mac[3] == chaddr[3]) &&
		    (dhcps.ip_mac_mapping[i].client_mac[4] == chaddr[4]) &&
		    (dhcps.ip_mac_mapping[i].client_mac[5] == chaddr[5])) {
			return dhcps.ip_mac_mapping[i].client_ip;
		}
	}
	return 0x00000000;
}

static uint8_t *ac_lookup_ip(uint32_t client_ip)
{
	/* returns mac address, if ip address is present in cache */
	int i;
	for (i = 0; i < dhcps.count_clients && i < MAC_IP_CACHE_SIZE; i++) {
		if ((dhcps.ip_mac_mapping[i].client_ip)	== client_ip) {
			return dhcps.ip_mac_mapping[i].client_mac;
		}
	}
	return NULL;
}

static bool ac_not_full()
{
	/* returns true if cache is not full */
	return (dhcps.count_clients < MAC_IP_CACHE_SIZE);
}

static bool ac_valid_ip(uint32_t requested_ip)
{
	/* skip over our own address, the network address or the
	 * broadcast address
	 */
	if (requested_ip == ntohl(dhcps.my_ip) ||
	       (requested_ip == ntohl(dhcps.my_ip &
					  dhcps.netmask)) ||
	       (requested_ip == ntohl((dhcps.my_ip |
					   (0xffffffff & ~dhcps.netmask))))) {
		return false;
	}
	if (ac_lookup_ip(htonl(requested_ip)) != NULL)
		return false;
	return true;
}

static void write_u32(char *dest, uint32_t be_value)
{
	*dest++ = be_value & 0xFF;
	*dest++ = (be_value >> 8) & 0xFF;
	*dest++ = (be_value >> 16) & 0xFF;
	*dest = be_value >> 24;
}

/* Configure the DHCP dynamic IP lease time*/
int dhcp_server_lease_timeout(uint32_t val)
{
	if ((val == 0) || (val > (60U*60U*24U*49700U))) {
		return -EINVAL;
	} else {
		dhcp_address_timeout = val;
		return WM_SUCCESS;
	}
}

/* calculate the address to give out to the next DHCP DISCOVER request
 *
 * DHCP clients will be assigned addresses in sequence in the subnet's address space.
 */
static unsigned int next_yiaddr()
{
#ifdef CONFIG_DHCP_SERVER_DEBUG
	struct in_addr ip;
#endif
	uint32_t new_ip;
	struct bootp_header *hdr = (struct bootp_header *)dhcps.msg;

	/* if device requesting for ip address is already registered,
	 * if yes, assign previous ip address to it
	 */
	new_ip = ac_lookup_mac(hdr->chaddr);
	if (new_ip == (0x00000000)) {
		/* next IP address in the subnet */
		dhcps.current_ip = ntohl(dhcps.my_ip & dhcps.netmask) |
			((dhcps.current_ip + 1) & ntohl(~dhcps.netmask));
		while (!ac_valid_ip(dhcps.current_ip)) {
			dhcps.current_ip = ntohl(dhcps.my_ip & dhcps.netmask) |
				((dhcps.current_ip + 1) &
				 ntohl(~dhcps.netmask));
		}

		new_ip = htonl(dhcps.current_ip);

		if (ac_add(hdr->chaddr, new_ip) !=
		    WM_SUCCESS)
			dhcp_w("No space to store new mapping..");
	}

#ifdef CONFIG_DHCP_SERVER_DEBUG
	ip.s_addr = new_ip;
	dhcp_d("New client IP will be %s", inet_ntoa(ip));
	ip.s_addr = dhcps.my_ip & dhcps.netmask;
#endif

	return new_ip;
}

static unsigned int make_response(char *msg, enum dhcp_message_type type)
{
	struct bootp_header *hdr;
	struct bootp_option *opt;
	char *offset = msg;

	hdr = (struct bootp_header *)offset;
	hdr->op = BOOTP_OP_RESPONSE;
	hdr->htype = 1;
	hdr->hlen = 6;
	hdr->hops = 0;
	hdr->ciaddr = 0;
	hdr->yiaddr = (type == DHCP_MESSAGE_ACK) ? dhcps.client_ip : 0;
	hdr->yiaddr = (type == DHCP_MESSAGE_OFFER) ?
	    next_yiaddr() : hdr->yiaddr;
	hdr->siaddr = 0;
	hdr->riaddr = 0;
	offset += sizeof(struct bootp_header);

	opt = (struct bootp_option *)offset;
	opt->type = BOOTP_OPTION_DHCP_MESSAGE;
	*(uint8_t *) opt->value = type;
	opt->length = 1;
	offset += sizeof(struct bootp_option) + opt->length;

	if (type == DHCP_MESSAGE_NAK)
		return (unsigned int)(offset - msg);

	opt = (struct bootp_option *)offset;
	opt->type = BOOTP_OPTION_SUBNET_MASK;
	write_u32(opt->value, dhcps.netmask);
	opt->length = 4;
	offset += sizeof(struct bootp_option) + opt->length;

	opt = (struct bootp_option *)offset;
	opt->type = BOOTP_OPTION_ADDRESS_TIME;
	write_u32(opt->value, htonl(dhcp_address_timeout));
	opt->length = 4;
	offset += sizeof(struct bootp_option) + opt->length;

	opt = (struct bootp_option *)offset;
	opt->type = BOOTP_OPTION_DHCP_SERVER_ID;
	write_u32(opt->value, dhcps.my_ip);
	opt->length = 4;
	offset += sizeof(struct bootp_option) + opt->length;

	opt = (struct bootp_option *)offset;
	opt->type = BOOTP_OPTION_ROUTER;
	write_u32(opt->value, dhcps.my_ip);
	opt->length = 4;
	offset += sizeof(struct bootp_option) + opt->length;

	opt = (struct bootp_option *)offset;
	opt->type = BOOTP_OPTION_NAMESERVER;
	write_u32(opt->value, 0);
#ifdef CONFIG_DHCP_DNS
	write_u32(opt->value, dhcps.my_ip);
#endif
	opt->length = 4;
	offset += sizeof(struct bootp_option) + opt->length;

	opt = (struct bootp_option *)offset;
	opt->type = BOOTP_END_OPTION;
	offset++;

	return (unsigned int)(offset - msg);
}

void dhcp_stat(int argc, char **argv)
{
	int i = 0;
	wmprintf("DHCP Server Lease Duration : %d seconds\r\n",
		 (int)DEFAULT_DHCP_ADDRESS_TIMEOUT);
	if (dhcps.count_clients == 0) {
		wmprintf("No IP-MAC mapping stored\r\n");
	} else {
		wmprintf("Client IP\tClient MAC\r\n");
		for (i = 0; i < dhcps.count_clients && i < MAC_IP_CACHE_SIZE;
		     i++) {
			wmprintf("%s\t%02X:%02X:%02X:%02X:%02X:%02X\r\n",
				 inet_ntoa(dhcps.ip_mac_mapping[i].client_ip),
				 dhcps.ip_mac_mapping[i].client_mac[0],
				 dhcps.ip_mac_mapping[i].client_mac[1],
				 dhcps.ip_mac_mapping[i].client_mac[2],
				 dhcps.ip_mac_mapping[i].client_mac[3],
				 dhcps.ip_mac_mapping[i].client_mac[4],
				 dhcps.ip_mac_mapping[i].client_mac[5]);
		}
	}
}

static int send_response(int sock, struct sockaddr *addr, char *msg, int len)
{
	int nb;
	unsigned int sent = 0;
	while (sent < len) {
		nb = sendto(sock, msg + sent, len - sent, 0, addr,
			    sizeof(struct sockaddr_in));
		if (nb < 0) {
			dhcp_e("failed to send response");
			return -WM_E_DHCPD_RESP_SEND;
		}
		sent += nb;
	}

	dhcp_d("sent response, %d bytes %s", sent,
	    inet_ntoa(((struct sockaddr_in *)addr)->sin_addr));
	g_wm_stats.wm_dhcp_req_cnt++;
	return WM_SUCCESS;
}

/* examine all the questions and look for spoofed_name.  return a pointer past
 * the questions, or NULL if run out of buffer space.
 *
 * *found is set to non-zero if spoofed_name is found.
 * */
static char *parse_questions(unsigned int num_questions, uint8_t * pos,
			     int *found)
{
	uint8_t *base = pos;

	pos += sizeof(struct dns_header);

	for (; num_questions > 0; num_questions--) {
		if (!*found)
			*found = !strncmp(dhcps.spoofed_name, (char *)pos,
					  (base + SERVER_BUFFER_SIZE - pos));
		do {
			if (*pos > 0)
				pos += *pos + 1;
			if (pos >= base + SERVER_BUFFER_SIZE)
				return NULL;
		} while (*pos > 0);
		pos += 1 + sizeof(struct dns_question);
	}

	return (char *)pos;
}

static unsigned int make_answer_rr(char *base, char *query, char *dst)
{
	struct dns_question *q;
	struct dns_rr *rr = (struct dns_rr *)dst;
	char *query_start = query;

	rr->name_ptr = htons(((uint16_t) (query - base) | 0xC000));

	/* skip past the qname (label) field */
	do {
		if (*query > 0)
			query += *query + 1;
	} while (*query > 0);
	query++;

	q = (struct dns_question *)query;
	query += sizeof(struct dns_question);

	rr->type = q->type;
	rr->class = q->class;
	rr->ttl = htonl(60U * 60U * 1U);	/* 1 hour */
	rr->rdlength = htons(4);
	rr->rd = dhcps.my_ip;

	return (unsigned int)(query - query_start);
}

static int process_dns_message(char *msg, int len, struct sockaddr_in *fromaddr)
{
	struct dns_header *hdr;
	char *pos;
	char *outp = msg + len;
	int i, nq;
	int found = 0;

	hdr = (struct dns_header *)msg;

	dhcp_d("DNS transaction id: 0x%x", htons(hdr->id));

	if (hdr->flags.fields.qr) {
		dhcp_e("ignoring this dns message (not a query)");
		return -WM_E_DHCPD_DNS_IGNORE;
	}

	nq = ntohs(hdr->num_questions);
	dhcp_d("we were asked %d questions", nq);

	if (nq <= 0) {
		dhcp_e("ignoring this dns msg (not a query or 0 questions)");
		return -WM_E_DHCPD_DNS_IGNORE;
	}

	/* make the header represent a response */
	hdr->flags.fields.qr = 1;	/* response */
	hdr->flags.fields.aa = 1;	/* authoritative */
	hdr->flags.fields.rcode = 0;
	hdr->flags.num = htons(hdr->flags.num);

	/* "answer" each question (query) */
	pos = msg + sizeof(struct dns_header);
	outp = parse_questions(nq, (uint8_t *) msg, &found);
	if (!found) {
		dhcp_w("ignored request not for our spoofed name");
		return -WM_E_DHCPD_DNS_IGNORE;
	}
	if (!outp) {
		dhcp_w("didn't find answers location, skipping this message");
		return -WM_E_DHCPD_BUFFER_FULL;
	}
	for (i = 0; i < nq; i++) {
		if (outp + sizeof(struct dns_rr) >= msg + SERVER_BUFFER_SIZE) {
			dhcp_d("no room for more answers, stopping here");
			break;
		}
		pos += make_answer_rr(msg, pos, outp);
		outp += sizeof(struct dns_rr);
	}
	hdr->answer_rrs = htons(i);

	/* the response consists of:
	 * - 1 x DNS header
	 * - num_questions x query fields from the message we're parsing
	 * - num_answers x answer fields that we've appended
	 */

	return SEND_RESPONSE(dhcps.dnsock, (struct sockaddr *)fromaddr,
			     msg, outp - msg);
}

static int process_dhcp_message(char *msg, int len)
{
	struct bootp_header *hdr;
	struct bootp_option *opt;
	uint8_t response_type = DHCP_NO_RESPONSE;
	unsigned int consumed = 0;
	bool got_ip = 0;
	bool need_ip = 0;
	int ret = WM_SUCCESS;
	bool got_client_ip = 0;
	uint32_t new_ip;

	if (!msg ||
	    len < sizeof(struct bootp_header) + sizeof(struct bootp_option) + 1)
		return -WM_E_DHCPD_INVALID_INPUT;

	hdr = (struct bootp_header *)msg;

	switch (hdr->op) {
	case BOOTP_OP_REQUEST:
		dhcp_d("bootp request");
		break;
	case BOOTP_OP_RESPONSE:
		dhcp_d("bootp response");
		break;
	default:
		dhcp_e("invalid op code: %d", hdr->op);
		return -WM_E_DHCPD_INVALID_OPCODE;
	}

	if (hdr->htype != 1 || hdr->hlen != 6) {
		dhcp_e("invalid htype or hlen");
		return -WM_E_DHCPD_INCORRECT_HEADER;
	}

	dhcp_d("client MAC: %02X:%02X:%02X:%02X:%02X:%02X", hdr->chaddr[0],
	    hdr->chaddr[1], hdr->chaddr[2], hdr->chaddr[3], hdr->chaddr[4],
	    hdr->chaddr[5]);

	dhcp_d("magic cookie: 0x%X", hdr->cookie);

	len -= sizeof(struct bootp_header);
	opt = (struct bootp_option *)(msg + sizeof(struct bootp_header));
	while (len > 0 && opt->type != BOOTP_END_OPTION) {
		if (opt->type == BOOTP_OPTION_DHCP_MESSAGE && opt->length == 1) {
			dhcp_d("found DHCP message option");
			switch (*(uint8_t *) opt->value) {
			case DHCP_MESSAGE_DISCOVER:
				dhcp_d("DHCP discover");
				response_type = DHCP_MESSAGE_OFFER;
				break;

			case DHCP_MESSAGE_REQUEST:
				dhcp_d("DHCP request");
				need_ip = 1;
				if (hdr->ciaddr != 0x0000000) {
					dhcps.client_ip = hdr->ciaddr;
					got_client_ip = 1;
				}
				break;

			default:
				dhcp_d("ignoring message type %d",
				    *(uint8_t *) opt->value);
				break;
			}
		}
		if (opt->type == BOOTP_OPTION_REQUESTED_IP && opt->length == 4) {
			dhcp_d("found REQUESTED IP option %hhu.%hhu.%hhu.%hhu",
			    opt->value[0],
			    opt->value[1], opt->value[2], opt->value[3]);
			memcpy((uint8_t *) &dhcps.client_ip,
			       (uint8_t *) opt->value, 4);
			got_client_ip = 1;
		}

		if (got_client_ip) {
			/* requested address outside of subnet */
			if ((dhcps.client_ip & dhcps.netmask) ==
			    (dhcps.my_ip & dhcps.netmask)) {

				/* When client requests an IP address,
				 * DHCP-server checks if the valid
				 * IP-MAC entry is present in the
				 * ip-mac cache, if yes, also checks
				 * if the requested IP is same as the
				 * IP address present in IP-MAC entry,
				 * if yes, it allows the device to
				 * continue with the requested IP
				 * address.
				 */
				new_ip = ac_lookup_mac(hdr->chaddr);
				if (new_ip != (0x00000000)) {
					/* if new_ip is equal to requested ip */
					if (new_ip == dhcps.client_ip) {
						got_ip = 1;
					} else {
						got_ip = 0;
					}
				} else if (ac_valid_ip
					   (ntohl(dhcps.client_ip))) {
					/* When client requests with an IP
					 * address that is within subnet range
					 * and not assigned to any other client,
					 * then dhcp-server allows that device
					 * to continue with that IP address.
					 * And if IP-MAC cache is not full then
					 * adds this entry in cache.
					 */
					if (ac_not_full()) {
						ac_add(hdr->chaddr,
						       dhcps.client_ip);
					} else {
						dhcp_w("No space to store new "
						       "mapping..");
					}
					got_ip = 1;
				}
			}
		}

		/* look at the next option (if any) */
		consumed = sizeof(struct bootp_option) + opt->length;
		len -= consumed;
		opt = (struct bootp_option *)((char *)opt + consumed);
		if (need_ip)
			response_type = got_ip ? DHCP_MESSAGE_ACK :
			    DHCP_MESSAGE_NAK;
	}

	if (response_type != DHCP_NO_RESPONSE) {
		ret = make_response(msg, (enum dhcp_message_type)response_type);
		ret = SEND_RESPONSE(dhcps.sock,
				    (struct sockaddr *)&dhcps.baddr, msg, ret);
		if (response_type == DHCP_MESSAGE_ACK)
			send_gratuitous_arp(dhcps.my_ip);
		return WM_SUCCESS;
	}

	dhcp_d("ignoring DHCP packet");
	return WM_SUCCESS;
}

void dhcp_clean_sockets()
{
	int ret;

	if (ctrl != -1) {
		ret = net_close(ctrl);
		if (ret != 0) {
			dhcp_w("Failed to close control socket: %d",
			       net_get_sock_error(ctrl));
		}
		ctrl = -1;
	}
	if (dhcps.sock != -1) {
		ret = net_close(dhcps.sock);
		if (ret != 0) {
			dhcp_w("Failed to close dhcp socket: %d",
			       net_get_sock_error(dhcps.sock));
		}
		dhcps.sock = -1;
	}
	if (dhcps.dnsock != -1) {
		ret = net_close(dhcps.dnsock);
		if (ret != 0) {
			dhcp_w("Failed to close dns socket: %d",
			       net_get_sock_error(dhcps.dnsock));
		}
		dhcps.dnsock = -1;
	}
}

void dhcp_server(os_thread_arg_t data)
{
	int ret;
	static int one = 1;
	struct sockaddr_in caddr;
	struct sockaddr_in ctrl_listen;
	int addr_len;
	int max_sock;
	int len;
	socklen_t flen = sizeof(caddr);
	fd_set rfds;

	/* create listening control socket */
	ctrl = net_socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (ctrl < 0) {
		ret = net_get_sock_error(ctrl);
		dhcp_e("Failed to create control socket: %d.",
			       ret);
		goto done;
	}
	setsockopt(ctrl, SOL_SOCKET, SO_REUSEADDR, (char *)&one, sizeof(one));
	ctrl_listen.sin_family = PF_INET;
	ctrl_listen.sin_port = htons(CTRL_PORT);
	ctrl_listen.sin_addr.s_addr = net_inet_aton("127.0.0.1");
	addr_len = sizeof(struct sockaddr_in);
	ret = net_bind(ctrl, (struct sockaddr *)&ctrl_listen, addr_len);
	if (ret < 0) {
		dhcp_e("Failed to bind control socket", ctrl);
		dhcp_clean_sockets();
		os_thread_self_complete(NULL);
	}

	os_mutex_get(&dhcp_mutex, OS_WAIT_FOREVER);

	while (1) {
		FD_ZERO(&rfds);
		FD_SET(dhcps.sock, &rfds);
		FD_SET(dhcps.dnsock, &rfds);
		FD_SET(ctrl, &rfds);

		max_sock = (dhcps.sock > dhcps.dnsock ?
			dhcps.sock : dhcps.dnsock);

		max_sock = (max_sock > ctrl) ? max_sock : ctrl;

		ret = net_select(max_sock + 1, &rfds, NULL, NULL, NULL);

		/* Error in select? */
		if (ret < 0) {
			dhcp_e("select failed", -1);
			goto done;
		}

		/* check the control socket */
		if (FD_ISSET(ctrl, &rfds)) {
			ret = recvfrom(ctrl, ctrl_msg, sizeof(ctrl_msg), 0,
					 (struct sockaddr *)0, (socklen_t *)0);
			if (ret == -1) {
				dhcp_e("Failed to get control"
				    " message: %d\r\n", ctrl);
			} else {
				if (strcmp(ctrl_msg, "HALT") == 0) {
					goto done;
				}
			}
		}

		if (FD_ISSET(dhcps.sock, &rfds)) {
			len = recvfrom(dhcps.sock, dhcps.msg,
				       sizeof(dhcps.msg),
				       0, (struct sockaddr *)&caddr, &flen);
			if (len > 0) {
				dhcp_d("len: %d", len);
				process_dhcp_message(dhcps.msg, len);
			}
		}

		if (FD_ISSET(dhcps.dnsock, &rfds)) {
			len = recvfrom(dhcps.dnsock, dhcps.msg,
				       sizeof(dhcps.msg),
				       0, (struct sockaddr *)&caddr, &flen);
			if (len > 0) {
				dhcp_d("len: %d", len);
				process_dns_message(dhcps.msg, len, &caddr);
			}
		}
	}

done:
	dhcp_clean_sockets();
	os_mutex_put(&dhcp_mutex);
	os_thread_self_complete(NULL);
}

static int create_and_bind_udp_socket(struct sockaddr_in *address,
					void *intrfc_handle)
{
	int one = 1;
	int ret;

	int sock = net_socket(PF_INET, SOCK_DGRAM, 0);
	if (sock == -1) {
		dhcp_e("failed to create a socket");
		return -WM_FAIL;
	}

	ret = setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (char *)&one,
			 sizeof(int));
	if (ret == -1) {
		/* This is unimplemented in lwIP, hence do not return */
		dhcp_e("failed to set SO_REUSEADDR");
	}

	if (setsockopt(sock, SOL_SOCKET, SO_BROADCAST,
		       (char *)&one, sizeof(one)) == -1) {
		dhcp_e("failed to set SO_BROADCAST");
		net_close(sock);
		return -WM_FAIL;
	}

	if (setsockopt(sock, SOL_SOCKET, SO_BINDTODEVICE,
			intrfc_handle, sizeof(intrfc_handle)) == -1) {
		dhcp_e("failed to set SO_BINDTODEVICE");
		net_close(sock);
		return -WM_FAIL;
	}

	net_socket_blocking(sock, NET_BLOCKING_OFF);

	ret = net_bind(sock, (struct sockaddr *)address,
					sizeof(struct sockaddr));

	if (ret) {
		dhcp_e("failed to bind server socket");
		dhcp_e("socket err: %d", net_get_sock_error(sock));
		net_close(sock);
		return -WM_FAIL;
	}
	return sock;
}

/* take a domain name and convert it into a DNS QNAME format, i.e.
 * foo.rats.com. -> 03 66 6f 6f 04 72 61 74 73 03 63 6f 6d 00
 *
 * The size of the QNAME will be one byte longer than the original string.
 */
static void format_qname(char *name_to_spoof)
{
	int i = 0;
	char *s = name_to_spoof;
	char *d = dhcps.spoofed_name + 1;
	while (*s != '\0') {
		s++;
		d++;
	}
	s--;
	d--;
	while (s >= name_to_spoof) {
		if (*s != '.') {
			*d = *s;
			i++;
		} else {
			*d = (char)i;
			i = 0;
		}
		s--;
		d--;
	}
	dhcps.spoofed_name[0] = i;
}

int dhcp_server_init(void *intrfc_handle)
{
	int ret = 0;

	/* A dummy name since this particular DNS server is never activated */
	char *name_to_spoof = "dummy";

	memset(&dhcps, 0, sizeof(dhcps));

	ret = os_mutex_create(&dhcp_mutex, "dhcp-mutex", OS_MUTEX_INHERIT);
	if (ret != WM_SUCCESS) {
		dhcp_e("failed to create mutex");
		return -WM_E_DHCPD_MUTEX_CREATE;
	}

	if (strlen(name_to_spoof) > MAX_QNAME_SIZE) {
		dhcp_e("Maximum spoof name is %d bytes!",
		    MAX_QNAME_SIZE);
		return -WM_E_DHCPD_SPOOF_NAME;
	}
	if (get_broadcast_addr(&dhcps.baddr) < 0) {
		dhcp_e("failed to get broadcast address");
		return -WM_E_DHCPD_BCAST_ADDR;
	}
	dhcps.baddr.sin_port = htons(DHCP_CLIENT_PORT);

	if (get_ip_addr_from_interface(&dhcps.my_ip, intrfc_handle) < 0) {
		dhcp_w("failed to look up our IP address from interface");
		return -WM_E_DHCPD_IP_ADDR;
	}

	if (get_netmask_from_interface(&dhcps.netmask, intrfc_handle) < 0) {
		dhcp_e("failed to look up our netmask from interface");
		return -WM_E_DHCPD_NETMASK;
	}

	dhcps.saddr.sin_family = AF_INET;
	dhcps.saddr.sin_addr.s_addr = INADDR_ANY;
	dhcps.saddr.sin_port = htons(DHCP_SERVER_PORT);
	dhcps.sock = create_and_bind_udp_socket(&dhcps.saddr, intrfc_handle);

	if (dhcps.sock < 0)
		return -WM_E_DHCPD_SOCKET;

	dhcps.dnaddr.sin_family = AF_INET;
	dhcps.dnaddr.sin_addr.s_addr = INADDR_ANY;
	dhcps.dnaddr.sin_port = htons(NAMESERVER_PORT);
	dhcps.dnsock = create_and_bind_udp_socket(&dhcps.dnaddr, intrfc_handle);

	if (dhcps.dnsock < 0)
		return -WM_E_DHCPD_SOCKET;

	format_qname(name_to_spoof);

	return WM_SUCCESS;
}

static int send_ctrl_msg(const char *msg)
{
	int ret;
	struct sockaddr_in to_addr;

	if (ctrl == -1)
		return -WM_E_DHCPD_SOCKET;

	memset((char *)&to_addr, 0, sizeof(to_addr));
	to_addr.sin_family = PF_INET;
	to_addr.sin_port = htons(CTRL_PORT);
	to_addr.sin_addr.s_addr = net_inet_aton("127.0.0.1");

	ret = sendto(ctrl, msg, strlen(msg) + 1, 0, (struct sockaddr *)&to_addr,
		     sizeof(to_addr));
	if (ret == -1)
		ret = net_get_sock_error(ctrl);
	else
		ret = WM_SUCCESS;

	return ret;
}

int dhcp_send_halt(void)
{
	int ret = WM_SUCCESS;

	ret = send_ctrl_msg("HALT");
	if (ret != 0) {
		dhcp_w("Failed to send HALT: %d.", ret);
		return -WM_FAIL;
	}

	os_mutex_get(&dhcp_mutex, OS_WAIT_FOREVER);
	dhcp_clean_sockets();
	os_mutex_put(&dhcp_mutex);
	os_mutex_delete(&dhcp_mutex);
	return ret;
}

static int send_gratuitous_arp(uint32_t ip)
{
	int sock;
	struct arp_packet pkt;
	struct sockaddr_in to_addr;
	to_addr.sin_family = AF_INET;
	to_addr.sin_addr.s_addr = ip;
	pkt.frame_type = htons(ARP_FRAME_TYPE);
	pkt.hw_type = htons(ETHER_HW_TYPE);
	pkt.prot_type = htons(IP_PROTO_TYPE);
	pkt.hw_addr_size = ETH_HW_ADDR_LEN;
	pkt.prot_addr_size = IP_ADDR_LEN;
	pkt.op = htons(OP_ARP_REQUEST);

	write_u32(pkt.sndr_ip_addr, ip);
	write_u32(pkt.rcpt_ip_addr, ip);

	memset(pkt.targ_hw_addr, 0xff, ETH_HW_ADDR_LEN);
	memset(pkt.rcpt_hw_addr, 0xff, ETH_HW_ADDR_LEN);
	wlan_get_mac_address(pkt.sndr_hw_addr);
	wlan_get_mac_address(pkt.src_hw_addr);
	sock = net_socket(AF_INET, SOCK_DGRAM, 0);
	if (sock < 0) {
		dhcp_e("Could not open socket to send Gratuitous ARP");
		return -WM_E_DHCPD_SOCKET;
	}
	memset(pkt.padding, 0, sizeof(pkt.padding));

	if (sendto(sock, (char *)&pkt, sizeof(pkt), 0,
		   (struct sockaddr *)&to_addr, sizeof(to_addr)) < 0) {
		dhcp_e("Failed to send Gratuitous ARP");
		net_close(sock);
		return -WM_E_DHCPD_ARP_SEND;
	}
	dhcp_d("Gratuitous ARP sent");
	net_close(sock);
	return WM_SUCCESS;
}

#ifdef CONFIG_DHCP_SERVER_TESTS
#include <test_utils.h>

static char DHCP_REQUEST_TEMPLATE[] = {
	0x01, 0x01, 0x06, 0x00, 0x60, 0xdb, 0xc9, 0x7e, 0x00, 0x29, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x19, 0xe3, 0xd4, 0x51, 0xa4, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x63, 0x82, 0x53, 0x63,
	/* options start below this line */
	0x35, 0x01, 0x01, 0x37, 0x0a, 0x01, 0x03, 0x06, 0x0f, 0x77, 0x5f, 0xfc,
	0x2c, 0x2e, 0x2f, 0x39, 0x02, 0x05, 0xdc, 0x3d, 0x07, 0x01, 0x00, 0x19,
	0xe3, 0xd4, 0x51, 0xa4, 0x33, 0x04, 0x00, 0x76, 0xa7, 0x00, 0x0c, 0x0b,
	0x58, 0x73, 0x2d, 0x43, 0x6f, 0x6d, 0x70, 0x75, 0x74, 0x65, 0x72, 0xff,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

static char DHCP_REQUEST__IPHONE[] = {
	0x01, 0x01, 0x06, 0x00, 0x14, 0xf8, 0x37, 0x1d, 0x00, 0x09, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x21, 0xe9, 0x87, 0xfa, 0x82, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x63, 0x82, 0x53, 0x63,
	/* options start below this line */
	0x35, 0x01, 0x03, 0x37, 0x06, 0x01, 0x03, 0x06, 0x0f, 0x77, 0xfc, 0x39,
	0x02, 0x05, 0xdc, 0x3d, 0x07, 0x01, 0x00, 0x21, 0xe9, 0x87, 0xfa, 0x82,
	0x32, 0x04, 0xc0, 0xa8, 0x07, 0x0c, 0x36, 0x04, 0xc0, 0xa8, 0x07, 0x01,
	0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

#define OPTIONS_START_OFFSET    (16 * 15)
#define YADDR_OFFSET			16

static int test_number;
static int test_pass;
static int capture_response(int sock, struct sockaddr *addr, char *msg, int len)
{
	struct in_addr ip;
	test_pass = 0;
	if (test_number == 1) {
		/* the capture_response function is called */
		test_pass = 1;
	}

	if (test_number == 2) {
		/* offered address is in subnet */
		uint32_t yaddr = *(uint32_t *) (msg + YADDR_OFFSET);
		test_pass = (yaddr & dhcps.netmask) ==
		    (dhcps.my_ip & dhcps.netmask);
		/* and not ours */
		test_pass |= (yaddr != dhcps.my_ip);
		/* and not net address */
		test_pass |= (yaddr != (dhcps.my_ip & dhcps.netmask));
		/* and not the bcast address */
		test_pass |= (yaddr != (dhcps.my_ip |
					(0xffffffff & ~dhcps.netmask)));
		ip.s_addr = yaddr;
		dhcp_d("Offered address is %s", inet_ntoa(ip));
	}

	if (test_number == 3) {
		/* request without requested ip: nak */
		uint8_t option_val = *(msg + OPTIONS_START_OFFSET + 2);
		test_pass = option_val == DHCP_MESSAGE_NAK;
		dhcp_d("Expecting NAK (6) : %d", option_val);
	}

	if (test_number == 4) {
		/* requested address outside of subnet: expect nak */
		uint8_t option_val = *(msg + OPTIONS_START_OFFSET + 2);
		test_pass = option_val == DHCP_MESSAGE_NAK;
		dhcp_d("Expecting NAK (6) : %d", option_val);
	}

	if (test_number == 5) {
		/* valid request : expect ack */
		uint8_t option_val = *(msg + OPTIONS_START_OFFSET + 2);
		uint32_t yaddr = *(uint32_t *) (msg + YADDR_OFFSET);
		test_pass = option_val == DHCP_MESSAGE_ACK;
		ip.s_addr = yaddr;
		dhcp_d("Offered address is %s", inet_ntoa(ip));
		dhcp_d("Expecting ACK (5) : %d", option_val);
	}

	/* call send_response to prevent unused warning */
	send_response(sock, addr, msg, 0);
	return len;
}

void dhcp_server_tests(int argc, char **argv)
{
	int i;

	/* setup */
	memset(&dhcps, 0, sizeof(dhcps));
	dhcps.my_ip = htonl(0xc0a80701);	/* 192.168.7.1 */
	dhcps.netmask = htonl(0xfffffff0);	/* 16 address subnet */

	/* tests */
	test_number = 1;
	memcpy(dhcps.msg, DHCP_REQUEST_TEMPLATE, sizeof(DHCP_REQUEST_TEMPLATE));
	process_dhcp_message(dhcps.msg, sizeof(DHCP_REQUEST_TEMPLATE));
	FAIL_UNLESS(test_pass, "null response ");

	test_number = 2;
	for (i = 0; i < 20; i++) {
		memcpy(dhcps.msg, DHCP_REQUEST_TEMPLATE,
		       sizeof(DHCP_REQUEST_TEMPLATE));
		process_dhcp_message(dhcps.msg, sizeof(DHCP_REQUEST_TEMPLATE));
		FAIL_UNLESS(test_pass, "invalid offered address");
	}

	test_number = 3;
	memcpy(dhcps.msg, DHCP_REQUEST_TEMPLATE, sizeof(DHCP_REQUEST_TEMPLATE));
	dhcps.msg[OPTIONS_START_OFFSET + 2] = DHCP_MESSAGE_REQUEST;
	process_dhcp_message(dhcps.msg, sizeof(DHCP_REQUEST_TEMPLATE));
	FAIL_UNLESS(test_pass, "invalid offered address");

	test_number = 4;
	memcpy(dhcps.msg, DHCP_REQUEST_TEMPLATE, sizeof(DHCP_REQUEST_TEMPLATE));
	dhcps.msg[OPTIONS_START_OFFSET + 2] = DHCP_MESSAGE_REQUEST;
	dhcps.msg[OPTIONS_START_OFFSET + 3] = BOOTP_OPTION_REQUESTED_IP;
	dhcps.msg[OPTIONS_START_OFFSET + 4] = 4;
	dhcps.msg[OPTIONS_START_OFFSET + 5] = 0xc0;
	dhcps.msg[OPTIONS_START_OFFSET + 6] = 0xa8;
	dhcps.msg[OPTIONS_START_OFFSET + 7] = 0x8;
	dhcps.msg[OPTIONS_START_OFFSET + 8] = 0x9;
	process_dhcp_message(dhcps.msg, sizeof(DHCP_REQUEST_TEMPLATE));
	FAIL_UNLESS(test_pass, "invalid offered address");

	test_number = 5;
	memcpy(dhcps.msg, DHCP_REQUEST__IPHONE, sizeof(DHCP_REQUEST__IPHONE));
	process_dhcp_message(dhcps.msg, sizeof(DHCP_REQUEST__IPHONE));
	FAIL_UNLESS(test_pass, "invalid offered address");
}
#endif

static int get_broadcast_addr(struct sockaddr_in *addr)
{
	addr->sin_family = AF_INET;
	/* limited broadcast addr (255.255.255.255) */
	addr->sin_addr.s_addr = 0xffffffff;
	addr->sin_len = sizeof(struct sockaddr_in);

	return WM_SUCCESS;
}

static int get_ip_addr_from_interface(uint32_t *ip, void *interface_handle)
{
	return net_get_if_ip_addr(ip, interface_handle);
}

static int get_netmask_from_interface(uint32_t *nm, void *interface_handle)
{
	return net_get_if_ip_mask(nm, interface_handle);
}
