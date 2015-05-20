/*
 * Copyright 2008-2013 cozybit Inc.
 *
 * This file is part of libmdns.
 *
 * libmdns is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation, either version 3 of
 * the License, or (at your option) any later version.
 *
 * libmdns is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with libmdns. If not, see <http://www.gnu.org/licenses/>.
 *
 */

/*
 * This code implements the responder portion of mdns.
 */
#include <wmstdio.h>
#include <mdns.h>
#include <mdns_port.h>
#include <errno.h>

#include "mdns_private.h"

static char *hname;		/* this is a cstring, not a DNS string */
static const char *domname; /* this is a cstring, not a DNS string */
/* Our fully qualified domain name is something like 'node.local.' */
static uint8_t fqdn[MDNS_MAX_NAME_LEN + 1];
struct mdns_service_config config_g[MDNS_MAX_SERVICE_CONFIG];
int num_config_g;

/* global mdns state */
static void *responder_thread;
static int mc_sock;
static int ctrl_sock;
static int responder_enabled;

static struct mdns_message tx_msg;
static struct mdns_message rx_msg;

/* This is the common service for which mDNS-SD based browsers send the request
 * as the first step. It is our job to respond with all the domains of the
 * services that we host to this request.
 */
struct mdns_service services_dnssd = {
	.servname = "_services",
	.servtype = "dns-sd",
	.domain   = "local",
	.proto    = MDNS_PROTO_UDP,
	.port     = 8888,
	.keyvals  = NULL,
};
/* reset the fqdn to hostname.domain.
 */
static void reset_fqdn(void)
{
	uint8_t *p;

	p = dname_put_label(fqdn, hname);
	dname_put_label(p, domname);
}

/* reset the internal fully qualified service name of s to the values supplied
 * by the user at launch time.
 */
static void reset_fqsn(struct mdns_service *s)
{
	uint8_t *p;

	s->ptrname = dname_put_label((uint8_t *) s->fqsn, s->servname);
	/* we have to be a bit tricky when adding the service type, because we have
	 * to append a leading '_' and adjust the length.
	 */
	p = dname_put_label(s->ptrname + 1, s->servtype);
	*s->ptrname = *(s->ptrname + 1) + 1;
	*(s->ptrname + 1) = '_';

	p = dname_put_label(p, s->proto == MDNS_PROTO_TCP ? "_tcp" : "_udp");

	dname_put_label(p, domname);
}

/* set mdns hostname */
void mdns_set_hostname(char *hostname)
{
	hname = hostname;
	reset_fqdn();
}

/* return 0 for invalid, 1 for valid */
#ifdef MDNS_CHECK_ARGS
static int valid_label(const char *name)
{
	if (strlen(name) > MDNS_MAX_LABEL_LEN || strchr(name, '.') != NULL)
		return 0;
	return 1;
}
#else
#define valid_label(name) (1)
#endif

static int validate_service(struct mdns_service *s)
{
	int maxlen;

	if (!valid_label(s->servname)) {
		LOG("Invalid service name: %s\r\n", s->servname);
		return -WM_E_MDNS_INVAL;
	}

	if (!valid_label(s->servtype)) {
		LOG("Invalid service type: %s\r\n", s->servtype);
		return -WM_E_MDNS_INVAL;
	}

	if (s->proto != MDNS_PROTO_TCP && s->proto != MDNS_PROTO_UDP) {
		LOG("Invalid proto: %d\r\n", s->proto);
		return -WM_E_MDNS_INVAL;
	}

	maxlen = strlen(s->servname) + 1;	/* +1 for label len */
	maxlen += strlen(s->servtype) + 2;	/* +1 for label len, +1 for leading _ */
	maxlen += 4;		/* for _tcp or _udp */
	maxlen += strlen(domname) + 1;
	maxlen += 2;		/* may need to add a '-2' to the servname if it's not unique */
	maxlen += 1;		/* and we need a terminating 0 */
	if (maxlen > MDNS_MAX_NAME_LEN)
		return -WM_E_MDNS_TOOBIG;

	/* Our TXT basically look like dnames but without the terminating \0.
	 * Instead of depending on this \0 to find the end, we record the length.
	 * This allows us to use that \0 space from the user to accomodate the
	 * leading length.
	 */
	if (s->keyvals != NULL) {
		s->kvlen = dnameify(s->keyvals, s->kvlen, ':', NULL);
		if (s->kvlen > MDNS_MAX_KEYVAL_LEN) {
			LOG("key/value exceeds MDNS_MAX_KEYVAL_LEN\r\n");
			return -WM_E_MDNS_TOOBIG;
		}
	}

	return 0;
}

/* Figure out packet is came from which interface */
static int get_config_idx(uint32_t incoming_ip)
{
	int i;
	uint32_t interface_ip, interface_mask;
	for (i = 0; i < MDNS_MAX_SERVICE_CONFIG; i++) {
		net_get_if_ip_addr(&interface_ip, config_g[i].iface_handle);
		net_get_if_ip_mask(&interface_mask, config_g[i].iface_handle);
		if ((interface_ip & interface_mask) ==
		    (incoming_ip & interface_mask))
			return i;
	}
	return -1;
}

/* get interface index from interface list */
static int get_interface_idx(void *iface)
{
	int i;

	for (i = 0; i < MDNS_MAX_SERVICE_CONFIG; i++)
		if (iface == config_g[i].iface_handle)
			return i;
	return -WM_FAIL;
}

/* get ip address of interface from its interface index*/
static uint32_t get_interface_ip(int config_idx)
{
	uint32_t ip;

	net_get_if_ip_addr(&ip, config_g[config_idx].iface_handle);
	return ip;
}

/* create the inaddrarpa domain name DNS string for ipaddr.  Output buffer must
 * be at least MDNS_INADDRARPA_LEN.  ipaddr must be in host order.
 */
static void ipaddr_to_inaddrarpa(uint32_t ipaddr, uint8_t *out)
{
	uint8_t *ptr = out;
	uint8_t byte;
	int i, len;

	/* This is a DNS name of the form 45.1.168.192.in-addr.arpa */
	for (i = 0; i < 4; i++) {
		byte = ipaddr & 0xff;
		ipaddr >>= 8;
		len = sprintf((char *)ptr + 1, "%d", byte);
		*ptr = len;
		ptr += len + 1;
	}
	*ptr++ = 7;
	sprintf((char *)ptr, "in-addr");
	ptr += 7;
	*ptr++ = 4;
	sprintf((char *)ptr, "arpa");
}

/* add all of the services that we have to the specified section of the message
 * m.  Return 0 for success or -1 for error.
 */
int responder_add_all_services(struct mdns_message *m, int section,
			       uint32_t ttl, int config_idx)
{
	struct mdns_service **s;

	for (s = config_g[config_idx].services; s != NULL && *s != NULL; s++)
		if (mdns_add_srv_ptr_txt(m, *s, fqdn, section, ttl) != 0)
			return -1;
	return 0;
}

/* Make m a probe packet suitable for the initial probe sequence.  It must
 * contain a question for each resource record that we intend on advertising
 * and an authority with our propose answers.
 */
static int prepare_probe(struct mdns_message *m, int config_idx)
{
	uint32_t my_ipaddr;
	static uint8_t in_addr_arpa[MDNS_INADDRARPA_LEN];

	mdns_query_init(m);
	my_ipaddr = get_interface_ip(config_idx);
	ipaddr_to_inaddrarpa(ntohl(my_ipaddr), in_addr_arpa);
	if (mdns_add_question(m, fqdn, T_ANY, C_IN) != 0 ||
	    mdns_add_question(m, in_addr_arpa, T_ANY, C_IN) != 0 ||
	    responder_add_all_services(m, MDNS_SECTION_QUESTIONS, 0, config_idx) != 0 ||
	    mdns_add_authority(m, fqdn, T_A, C_FLUSH, 255) != 0 ||
	    mdns_add_uint32_t(m, htonl(my_ipaddr)) != 0 ||
	    mdns_add_authority(m, in_addr_arpa, T_PTR, C_FLUSH, 255) != 0 ||
	    mdns_add_name(m, fqdn) != 0 ||
	    responder_add_all_services(m, MDNS_SECTION_AUTHORITIES, 255, config_idx) != 0) {
		LOG("Resource records don't fit into probe packet.\r\n");
		return -1;
	}
	/* populate the internal data structures of m so we can easily compare this
	 * probe to a probe response later.  Be sure to leave the end pointer at
	 * the end of our buffer so we can grow the packet if necessary.
	 */
	if (mdns_parse_message(m, VALID_LENGTH(m)) == -1) {
		while (1) ;
	}
	m->end = m->data + sizeof(m->data) - 1;
	return 0;
}

static int prepare_announcement(struct mdns_message *m, uint32_t ttl, int config_idx)
{
	uint32_t my_ipaddr;
	static uint8_t in_addr_arpa[MDNS_INADDRARPA_LEN];

	mdns_response_init(m);
	my_ipaddr = get_interface_ip(config_idx);
	ipaddr_to_inaddrarpa(ntohl(my_ipaddr), in_addr_arpa);
	if (mdns_add_answer(m, fqdn, T_A, C_FLUSH, ttl) != 0 ||
	    mdns_add_uint32_t(m, htonl(my_ipaddr)) != 0 ||
	    mdns_add_answer(m, in_addr_arpa, T_PTR, C_FLUSH, ttl) != 0 ||
	    mdns_add_name(m, fqdn) != 0 ||
	    responder_add_all_services(m, MDNS_SECTION_ANSWERS, ttl, config_idx) != 0) {
		/* This is highly unlikely */
		return -1;
	}
	return 0;
}

/* if we detect a conflict during probe time, we must grow our host name and
 * service names with -2, or -3, etc.  Report the maximum number of extra bytes
 * that we would need to do this.
 */
static int max_probe_growth(int num_services)
{
	/* We need 2 bytes for each instance of the host name that appears in the
	 * probe (A record q, A record a, inaddrarpa PTR, one for each SRV PTR),
	 * and 2 for each service name.
	 */
	return 2 * 3 + 2 * 2 * num_services;
}

/* if we detect a conflict during probe time, we must grow our host name and
 * service names with -2, or -3, etc.  Report the maximum number of extra bytes
 * that we would need to do this.
 */
static int max_response_growth(int num_services)
{
	/* We need 2 bytes for each instance of the host name that appears in the:
	 * Query part:
	 * +2 A record q
	 * +2 * num services
	 *
	 * Answer part:
	 * +2 A record a
	 * +2 inaddrarpa PTR
	 * +2 * num services SRV
	 * +2 * num services SRV PTR
	 * +2 * num services TX
	 */

	return 2 * 3 + 4 * 2 * num_services;
}

/* prepare a response to the message rx.  Put it in tx.
 * Return RS_ERROR (-1) for error
 *        RS_NO_SEND (0) for nothing to send
 *        or bit 0 (RS_SEND) set if a response needs to be sent
 *        and/or bit 1 (RS_SEND_DELAY) set if the response needs to be sent
 *        with a delay
 */
static int prepare_response(struct mdns_message *rx, struct mdns_message *tx, int config_idx)
{
	int i;
	int ret = RS_NO_SEND;
	uint32_t my_ipaddr;
	struct mdns_question *q;
	struct mdns_service **s;
	static uint8_t in_addr_arpa[MDNS_INADDRARPA_LEN];

	my_ipaddr = get_interface_ip(config_idx);
	ipaddr_to_inaddrarpa(ntohl(my_ipaddr), in_addr_arpa);
	if (rx->header->flags.fields.qr != QUERY)
		return RS_NO_SEND;

	mdns_response_init(tx);
	tx->header->id = rx->header->id;

	/* We never want to include a record more than once.  To facilitate this,
	 * we track whether we've added a service's SRV and TXT.
	 */
	for (s = config_g[config_idx].services; s != NULL && *s != NULL; s++)
		(*s)->flags &= ~(SRV_ADDED | TXT_ADDED);

	for (i = 0; i < rx->num_questions; i++) {
		q = &rx->questions[i];

		if (q->qtype == T_ANY || q->qtype == T_A) {
			if (dname_cmp(rx->data, q->qname, NULL, fqdn) == 0) {
				if (mdns_add_answer(tx, fqdn, T_A,
						    C_FLUSH, 255) != 0 ||
				    mdns_add_uint32_t(tx,
					      htonl(my_ipaddr)) != 0)
					return RS_ERROR;
				ret |= RS_SEND;
			}
		}

		if (q->qtype == T_ANY || q->qtype == T_PTR) {
			if (dname_cmp(rx->data, q->qname, NULL, in_addr_arpa) ==
			    0) {
				if (mdns_add_answer
				    (tx, in_addr_arpa, T_PTR, C_FLUSH, 255) != 0
				    || mdns_add_name(tx, fqdn) != 0)
					return RS_ERROR;
				ret |= RS_SEND;
			}

			/* If the request is for _services._dns-sd._udp.local
			 * respond with a list of all services that we host.
			 */
			if (dname_cmp(rx->data, q->qname, NULL,
				      services_dnssd.fqsn) == 0) {

				services_dnssd.flags |= SRV_ADDED;
				for (s = config_g[config_idx].services; s != NULL && *s != NULL;
				     s++) {
					/* Skip if it is already added */
					if (((*s)->flags & SRV_ADDED))
						continue;

					/* Add our PTR entry to the
					 * listing. This should have the
					 * fqsn of the services_mdns-sd but
					 * have the ptrname of this particular
					 * entry. */
					if (mdns_add_answer
					    (tx, services_dnssd.fqsn, T_PTR,
					     C_IN, 255))
						return RS_ERROR;
					else if (mdns_add_name(tx, (*s)->ptrname))
						return RS_ERROR;
					(*s)->flags |= SRV_ADDED;

				}
				ret |= RS_SEND_DELAY;
			}

			/* if the querier wants PTRs to services that we offer, add those */
			for (s = config_g[config_idx].services; s != NULL && *s != NULL; s++) {
				if (dname_cmp
				    (rx->data, q->qname, NULL,
				     (*s)->ptrname) == 0 ||
				    dname_cmp(rx->data, q->qname, NULL,
					      (*s)->fqsn) == 0) {

					/* Only add records to the response as necessary */
					if (!((*s)->flags & SRV_ADDED)) {
						if (mdns_add_answer
						    (tx, (*s)->fqsn, T_SRV,
						     C_FLUSH, 255))
							return RS_ERROR;
						else if (mdns_add_srv
							 (tx, 0, 0, (*s)->port,
							  fqdn))
							return RS_ERROR;
						(*s)->flags |= SRV_ADDED;
					}

					if (mdns_add_answer
					    (tx, (*s)->ptrname, T_PTR, C_FLUSH,
					     255))
						return RS_ERROR;
					else if (mdns_add_name(tx, (*s)->fqsn))
						return RS_ERROR;

					if ((*s)->keyvals &&
					    !((*s)->flags & TXT_ADDED)) {
						if (mdns_add_answer
						    (tx, (*s)->fqsn, T_TXT,
						     C_FLUSH, 255))
							return RS_ERROR;
						else if (mdns_add_txt
							 (tx, (*s)->keyvals,
							  (*s)->kvlen) != 0)
							return RS_ERROR;
						(*s)->flags |= TXT_ADDED;
					}

					ret |= RS_SEND_DELAY;
				}
			}
		}

		if (q->qtype == T_SRV) {
			for (s = config_g[config_idx].services; s != NULL && *s != NULL; s++) {
				if (dname_cmp
				    (rx->data, q->qname, NULL, (*s)->fqsn) == 0
				    && !((*s)->flags & SRV_ADDED)) {
					if (mdns_add_answer
					    (tx, (*s)->fqsn, T_SRV, C_FLUSH,
					     255))
						return RS_ERROR;
					else if (mdns_add_srv
						 (tx, 0, 0, (*s)->port, fqdn))
						return RS_ERROR;
					ret |= RS_SEND_DELAY;
					(*s)->flags |= SRV_ADDED;
				}
			}
		}

		if (q->qtype == T_TXT) {
			for (s = config_g[config_idx].services; s != NULL && *s != NULL; s++) {
				if (dname_cmp
				    (rx->data, q->qname, NULL, (*s)->fqsn) == 0
				    && (*s)->keyvals &&
				    !((*s)->flags & TXT_ADDED)) {
					if (mdns_add_answer
					    (tx, (*s)->fqsn, T_TXT, C_FLUSH,
					     255))
						return RS_ERROR;
					else if (mdns_add_txt
						 (tx, (*s)->keyvals,
						  (*s)->kvlen) != 0)
						return RS_ERROR;
					ret |= RS_SEND_DELAY;
					(*s)->flags |= TXT_ADDED;
				}
			}
		}
	}
	return ret;
}

/* Build a response packet to check that everything will fit in the packet.
 * First need to build the query and then will simulate the response.
 */
static int check_max_response(struct mdns_message *rx, struct mdns_message *tx, int config_idx)
{
	uint32_t my_ipaddr;
	static uint8_t in_addr_arpa[MDNS_INADDRARPA_LEN];

	mdns_query_init(rx);
	my_ipaddr = get_interface_ip(config_idx);
	ipaddr_to_inaddrarpa(ntohl(my_ipaddr), in_addr_arpa);
	if (mdns_add_question(rx, fqdn, T_ANY, C_IN) != 0 ||
	    mdns_add_question(rx, in_addr_arpa, T_ANY, C_IN) != 0 ||
	    responder_add_all_services(rx, MDNS_SECTION_QUESTIONS, 0, config_idx) != 0) {
		LOG("Resource records don't fit into query of response packet.\r\n");
		return -1;
	}

	if (mdns_parse_message(rx, VALID_LENGTH(rx)) != 0) {
		LOG("Failed to parse biggest expected query.\r\n");
		return -1;
	}

	if (prepare_response(rx, tx, config_idx) < RS_SEND) {
		LOG("Resource records don't fit into response packet.\r\n");
		return -1;
	}

	return 0;
}

/* find all of the authority records in the message m that have the specified
 * name and return up to "size" of them sorted in order of increasing type.
 * This allows us to compare them in lexicographically increasing order as
 * required by the spec.  Return the number of valid records in the list.
 * "name" must be a complete dname without any pointers.
 */

/* in practice, we only expect a subset of records.  So we just make an array
 * of these to simplify the sorting.
 */
enum type_indicies {
	A_INDEX = 0,
	CNAME_INDEX,
	PTR_INDEX,
	TXT_INDEX,
	SRV_INDEX,
	MAX_INDEX,
};

static int find_authorities(struct mdns_message *m, uint8_t * name,
			    struct mdns_resource *results[MAX_INDEX])
{
	int i, n = 0;

	for (i = 0; i < MAX_INDEX; i++)
		results[i] = 0;

	for (i = 0; i < m->num_authorities; i++) {

		if (dname_cmp(m->data, m->authorities[i].name, NULL, name) != 0)
			continue;

		/* okay.  The authority matches.  Add it to the results */
		n++;
		switch (m->authorities[i].type) {
		case T_A:
			results[A_INDEX] = &m->authorities[i];
			break;
		case T_CNAME:
			results[CNAME_INDEX] = &m->authorities[i];
			break;
		case T_PTR:
			results[PTR_INDEX] = &m->authorities[i];
			break;
		case T_TXT:
			results[TXT_INDEX] = &m->authorities[i];
			break;
		case T_SRV:
			results[SRV_INDEX] = &m->authorities[i];
			break;
		default:
			LOG("Warning: unexpected record of type %d\r\n",
			    m->authorities[i].type);
			n--;
		}
	}
	return n;
}

/* is the resource a from message ma "lexicographically later" than b, earlier
 * than b, or equal to b?  Return 1, 0, or -1 respectively.
 */
static int rr_cmp(struct mdns_message *ma, struct mdns_resource *a,
		  struct mdns_message *mb, struct mdns_resource *b)
{
	int min, i, ret = 0;
	struct rr_srv *sa, *sb;

	/* check the fields in the order specified (class, type, then rdata).
	 * Normally, we would start with the name of the resource, but by the time
	 * we call this function, we know they are equal.
	 */
	if ((a->class & ~0x8000) > (b->class & ~0x8000))
		return 1;

	if ((a->class & ~0x8000) < (b->class & ~0x8000))
		return -1;

	if (a->type > b->type)
		return 1;

	if (a->type < b->type)
		return -1;

	/* Okay.  So far, everything is the same.  Now we must check the rdata.
	 * This part depends on the record type because we have to decompress any
	 * names.
	 */
	switch (a->type) {
	case T_A:
		/* A record always contains a 4-byte IP address */
		ret = memcmp(a->rdata, b->rdata, 4);
		break;

	case T_CNAME:
	case T_PTR:
		/* some records just have a dname */
		ret = dname_cmp(ma->data, a->rdata, mb->data, b->rdata);
		break;

	case T_SRV:
		/* first check the fixed part of the record */
		ret = memcmp(a->rdata, b->rdata, sizeof(struct rr_srv));
		if (ret != 0)
			break;
		sa = (struct rr_srv *)a->rdata;
		sb = (struct rr_srv *)b->rdata;
		ret = dname_cmp(ma->data, sa->target, mb->data, sb->target);
		break;

	case T_TXT:
	default:
		min = a->rdlength > b->rdlength ? b->rdlength : a->rdlength;
		for (i = 0; i < min; i++) {
			if (((unsigned char *)a->rdata)[i] >
			    ((unsigned char *)b->rdata)[i])
				return 1;
			if (((unsigned char *)a->rdata)[i] >
			    ((unsigned char *)b->rdata)[i])
				return -1;
		}
		/* if we get all the way here, one rdata is a substring of the other.  The
		 * longer one wins.
		 */
		if (a->rdlength > b->rdlength)
			return 1;
		if (a->rdlength < b->rdlength)
			return -1;
	}
	ret = ret > 0 ? 1 : ret;
	ret = ret < 0 ? -1 : ret;
	return ret;
}

/* Is the authority record set in p1 with the specified name greater, equal, or
 * lesser than the equivalent authority record set in p2?  Return 1, 0, or -1
 * respectively.
 */
static int authority_set_cmp(struct mdns_message *p1, struct mdns_message *p2,
			     uint8_t *name)
{
	struct mdns_resource *r1[MAX_INDEX], *r2[MAX_INDEX];
	int ret = 0, i1 = 0, i2 = 0, n1, n2;

	/* The spec says we have to check the records in increasing lexicographic
	 * order.  So get a list sorted as such.
	 */
	n1 = find_authorities(p1, name, r1);
	n2 = find_authorities(p2, name, r2);
	if (n1 == 0 && n2 == 0)
		return 0;

	/* now check them in order of increasing type */
	while (1) {
		/* advance to the next valid record */
		while (i1 < MAX_INDEX && r1[i1] == NULL)
			i1++;

		while (i2 < MAX_INDEX && r2[i2] == NULL)
			i2++;

		if (i1 == MAX_INDEX && i2 == MAX_INDEX) {
			/* The record sets are absolutely identical. */
			ret = 0;
			break;
		}

		if (i1 == MAX_INDEX) {
			/* we've gone through all of p1's records and p2 still has records.
			 * This means p2 is greater.
			 */
			ret = -1;
			break;
		}

		if (i2 == MAX_INDEX) {
			/* we've gone through all of p2's records and p1 still has records.
			 * This means p1 is greater.
			 */
			ret = 1;
			break;
		}

		ret = rr_cmp(p1, r1[i1], p2, r2[i2]);
		if (ret != 0)
			break;
		i1++;
		i2++;
	}
	return ret;
}

/* does the incoming message m conflict with our probe p?  If so, alter the
 * service/host names as necessary and return 1.  Otherwise return 0.  If we've
 * seen so many conflicts that we have tried all the possible names, return -1;
 */
static int fix_response_conflicts(struct mdns_message *m,
				  struct mdns_message *p, int config_idx)
{
	struct mdns_resource *r;
	struct mdns_question *q;
	struct mdns_service **s;
	int ret = 0, i, len, cmp;

	if (m->header->flags.fields.qr == QUERY && m->num_authorities > 0) {
		/* We only want to check the authorities of service names once because
		 * it is quite costly.  So we use the SERVICE_CHECKED_FLAG to know if
		 * we've already checked a service.  We start with the flag cleared.
		 */
		for (s = config_g[config_idx].services; s != NULL && *s != NULL; s++)
			(*s)->flags &= ~(SERVICE_CHECKED_FLAG);

		/* is this a probe from somebody with conflicting records? */
		for (i = 0; i < m->num_questions; i++) {
			q = &m->questions[i];

			/* ignore PTR records.  Other people are allowed to have PTR records
			 * with the same name as ours.
			 */
			if (q->qtype == T_PTR)
				continue;

			if (dname_cmp(m->data, q->qname, NULL, fqdn) == 0) {
				/* somebody wants our hostname. Check the authorities. */
				cmp = authority_set_cmp(m, p, fqdn);
				if (cmp == 1) {
					/* their authoritative RR is greater than ours */
					if (ret == 0)
						ret = 1;
					if (dname_increment(fqdn) == -1) {
						reset_fqdn();
						ret = -1;
					}
				}
			}

			for (s = config_g[config_idx].services; s != NULL && *s != NULL; s++) {

				if ((*s)->flags & SERVICE_CHECKED_FLAG)
					continue;

				if (dname_cmp
				    (m->data, q->qname, NULL,
				     (*s)->fqsn) == 0) {
					/* somebody wants one of our service names. */
					cmp =
					    authority_set_cmp(m, p, (*s)->fqsn);
					if (cmp == 1) {
						/* their authoritative RR is greater than ours */
						if (ret == 0)
							ret = 1;
						len = dname_size((*s)->fqsn);
						if (dname_increment((*s)->fqsn)
						    == -1) {
							reset_fqsn(*s);
							ret = -1;
						}
						/* remember that we must increment the pointer to the fully
						 * qualified service type if we added bytes to the service
						 * name.
						 */
						if (dname_size((*s)->fqsn) >
						    len)
							(*s)->ptrname += 2;
					}
					(*s)->flags |= SERVICE_CHECKED_FLAG;
				}
			}
		}
		return ret;
	}

	/* otherwise, this is a response.  Is it a response to our probe? */
	for (i = 0; i < m->num_answers; i++) {
		r = &m->answers[i];
		if (r->type == T_A &&
		    dname_cmp(m->data, r->name, NULL, fqdn) == 0) {
			/* try a different name */
			if (ret == 0)
				ret = 1;
			if (dname_increment(fqdn) == -1) {
				reset_fqdn();
				ret = -1;
			}
		}
		/* ensure that none of the service names conflict. */
		if (r->type == T_SRV) {
			for (s = config_g[config_idx].services; s != NULL && *s != NULL; s++) {
				if (dname_cmp
				    (m->data, r->name, NULL, (*s)->fqsn) == 0) {
					/* try a different service name */
					if (ret == 0)
						ret = 1;
					len = dname_size((*s)->fqsn);
					if (dname_increment((*s)->fqsn) == -1) {
						reset_fqsn(*s);
						ret = -1;
					}
					/* remember that we must increment the pointer to the fully
					 * qualified service type if we added bytes to the service
					 * name.
					 */
					if (dname_size((*s)->fqsn) > len)
						(*s)->ptrname += 2;
				}
			}
		}
	}

	if (ret == 1) {
		DBG("Responder detected conflict with this response:\r\n");
		debug_print_message(m);
		DBG("\r\n");
	}

	return ret;
}

/* We're in a probe state sending the probe tx, and we got a probe response
 * (rx).  Process it and return the next state.  Also, update the timeout with
 * the time until the next event.  state must be one of the probe states!
 */
static int process_probe_resp(struct mdns_message *tx, struct mdns_message *rx,
			      int state, struct timeval *timeout,
			      uint32_t start_wait, uint32_t stop_wait, int config_idx)
{
	int ret;

	ret = fix_response_conflicts(rx, tx, config_idx);
	if (ret == 1) {
		/* there were conflicts with some of our names.  The names have been
		 * adapted.  Go back to square one.
		 */
		SET_TIMEOUT(timeout, 0);
		return INIT;

	} else if (ret == -1) {
		/* we've tried lots of names.  Where applicable, we've reset original
		 * names.  Sleep for 5s, then try again.
		 */
		DBG("Responder tried all possible names.  Resetting.\r\n");
		SET_TIMEOUT(timeout, 5000);
		return INIT;
	} else {
		/* this was an unrelated message.  Remain in the same state.  Assume
		 * the caller has set the timeout properly.
		 */
		recalc_timeout(timeout, start_wait, stop_wait, 250);
		return state;
	}
}

static os_mutex_t mdns_mutex;
enum mdns_iface_state {
	STOPPED = 0,
	STARTED,
	RUNNING,
};
static enum mdns_iface_state interface_state[MDNS_MAX_SERVICE_CONFIG];

static int send_close_probe(int config_idx)
{
	int ret;

	ret = prepare_announcement(&tx_msg, 0, config_idx);
	if (ret != 0)
		return -1;
	mdns_send_msg(&tx_msg, mc_sock,
		      htons(5353), config_g[config_idx].iface_handle);
	interface_state[config_idx] = STOPPED;
	return 0;
}

static uint32_t get_msec_time(struct timeval t)
{
	return (t.tv_sec * 1000) + (t.tv_usec / 1000);
}
static int get_min_timeout(struct timeval *t, int num_config)
{
	int i, min_idx = 0;

	for (i = 0; i < num_config; i++) {
		if (interface_state[i] == STOPPED || get_msec_time(t[i]) == 0)
			continue;
		else {
			if (get_msec_time(t[min_idx]) == 0)
				min_idx = i;
			else if (get_msec_time(t[i]) < get_msec_time(t[min_idx]))
				min_idx = i;
		}
	}
	return min_idx;
}

/* Re-calculate timeout for all interface */
static void update_others_timeout(int idx, struct timeval *probe_wait_time, uint32_t start_wait, uint32_t stop_wait)
{
	int i;

	for (i = 0; i < MDNS_MAX_SERVICE_CONFIG; i++) {
		if (interface_state[i] != STOPPED)
			if (i != idx && config_g[i].iface_handle != NULL) {
				recalc_timeout(&probe_wait_time[i], start_wait,
				stop_wait,
				get_msec_time(probe_wait_time[i]));
			}
	}
}

static int responder_state_machine(struct sockaddr_in from ,int *state, int *event, int config_idx, struct timeval *probe_wait_time)
{
	int ret;
	int rand_time = 0;

	/* We will be in below state machine only after above is completed */
	switch (state[config_idx]) {
	case READY_TO_RESPOND:
		if (event[config_idx] == EVENT_RX) {
			/* prepare a response if necessary */
			ret = prepare_response(&rx_msg, &tx_msg, config_idx);
			if (ret <= RS_NO_SEND)
				break;

			if (ret & RS_SEND_DELAY) {
				/* Implment random delay from 20-120msec */
				DBG("delaying response\r\n");
				rand_time = mdns_rand_range(100);
				SET_TIMEOUT(&probe_wait_time[config_idx],
					    (rand_time + 20));
				state[config_idx] = READY_TO_SEND;
			} else if (ret & RS_SEND) {
				/* We send immedately */
				DBG("responding to query:\r\n");
				debug_print_message(&rx_msg);
				mdns_send_msg(&tx_msg, mc_sock,
				      from.sin_port,
				      config_g[config_idx].iface_handle);
				}
		}
		break;

	case READY_TO_SEND:
			/* We send, no matter if triggered by timeout or RX */
		DBG("responding to query:\r\n");
		debug_print_message(&rx_msg);
		mdns_send_msg(&tx_msg, mc_sock,
			      from.sin_port,
			      config_g[config_idx].iface_handle);

		if (event[config_idx] == EVENT_TIMEOUT) {
			/* Here due to timeout, so we're done, go back to RTR */
			SET_TIMEOUT(&probe_wait_time[config_idx], 0);
			state[config_idx] = READY_TO_RESPOND;
		} else if (event[config_idx] == EVENT_RX) {
			/* prepare a response if necessary */
			ret = prepare_response(&rx_msg, &tx_msg, config_idx);
			if (ret <= RS_NO_SEND) {
				/* Error or otherwise no response, go back to RTR */
				SET_TIMEOUT(&probe_wait_time[config_idx], 0);
				state[config_idx] = READY_TO_RESPOND;
				} else if (ret & RS_SEND_DELAY) {
				/* Implment random delay from 20-120msec */
				DBG("delaying response\r\n");
				rand_time = mdns_rand_range(100);
				SET_TIMEOUT(&probe_wait_time[config_idx],
					    (rand_time + 20));
				state[config_idx] = READY_TO_SEND;
			} else if (ret & RS_SEND) {
				/* We send immedately */
				DBG("responding to query:\r\n");
				debug_print_message(&rx_msg);
				mdns_send_msg(&tx_msg, mc_sock,
				      from.sin_port,
				      config_g[config_idx].iface_handle);

				/* No longer have a message queued up, so go back to RTR */
				SET_TIMEOUT(&probe_wait_time[config_idx], 0);
				state[config_idx] = READY_TO_RESPOND;
			}
		}
			break;
	}
	return 0;
}

static int send_init_probes(int idx, int *state, int *event, struct timeval *probe_wait_time)
{
	uint32_t start_wait, stop_wait;
	int rand_time = 0;
	struct timeval *timeout;
	fd_set fds;
	int active_fds, len;
	struct sockaddr_in from;
	socklen_t in_size = sizeof(struct sockaddr_in);
	int ret, config_idx, min_idx;

	os_mutex_get(&mdns_mutex, OS_WAIT_FOREVER);
	rand_time = mdns_rand_range(250);
	SET_TIMEOUT(&probe_wait_time[idx], rand_time);
	interface_state[idx] = STARTED;
	/* Per specification, section 8.1, wait a random ammount of time between
	 * 0 and 250 ms before the first probe.
	 */
	while (state[idx] != READY_TO_RESPOND) {
		FD_ZERO(&fds);
		FD_SET(mc_sock, &fds);
		min_idx = get_min_timeout(probe_wait_time,
					  MDNS_MAX_SERVICE_CONFIG);
		if (get_msec_time(probe_wait_time[min_idx]) != 0)
			timeout = &probe_wait_time[min_idx];
		else
			timeout = NULL;
		start_wait = mdns_time_ms();
		active_fds = net_select(mc_sock + 1, &fds, NULL, NULL,
				timeout);
		stop_wait = mdns_time_ms();
		if (active_fds < 0)
			LOG("error: net_select() failed: %d\r\n", active_fds);

		if (FD_ISSET(mc_sock, &fds)) {
			in_size = sizeof(struct sockaddr_in);
			len =
				recvfrom(mc_sock, (char *)rx_msg.data,
					 sizeof(rx_msg.data), MSG_DONTWAIT,
					 (struct sockaddr *)&from, &in_size);
			if (len < 0) {
				LOG("responder failed to recv packet\r\n");
				continue;
			}
			config_idx = get_config_idx(from.sin_addr.s_addr);
			update_others_timeout(config_idx, probe_wait_time, start_wait, stop_wait);

			ret = mdns_parse_message(&rx_msg, len);
			if (ret != 0)
				continue;
			if (interface_state[config_idx] == STARTED || interface_state[config_idx] == RUNNING)
				event[config_idx] = EVENT_RX;
			/* Check if packet is from interface where mdns is in RUNNING state*/
			if (interface_state[config_idx] == RUNNING) {
				responder_state_machine(from, state, event, config_idx, probe_wait_time);
				continue;
			}
		} else {
			event[min_idx] = EVENT_TIMEOUT;
			update_others_timeout(min_idx, probe_wait_time, start_wait, stop_wait);
			/* Check if packet is from interface where mdns is still RUNNING */
			if (interface_state[min_idx] == RUNNING) {
				responder_state_machine(from, state, event, min_idx, probe_wait_time);
				continue;
			}
		}
		switch (state[idx]) {
		case INIT:
			if (event[idx] == EVENT_TIMEOUT) {
				DBG("Sending first probe to index = %d \r\n", idx);
				prepare_probe(&tx_msg, idx);
				mdns_send_msg(&tx_msg, mc_sock, htons(5353),
					      config_g[idx].iface_handle);
				SET_TIMEOUT(&probe_wait_time[idx], 250);
				state[idx] = FIRST_PROBE_SENT;
			} else if (event[idx] == EVENT_RX) {
				recalc_timeout(&probe_wait_time[idx], start_wait,
					       stop_wait, 250);
			}
			break;

		case FIRST_PROBE_SENT:
			if (event[idx] == EVENT_TIMEOUT) {
				DBG("Sending second probe to index = %d\r\n",idx );
				prepare_probe(&tx_msg, idx);
				mdns_send_msg(&tx_msg, mc_sock, htons(5353),
					      config_g[idx].iface_handle);
				SET_TIMEOUT(&probe_wait_time[idx], 250);
				state[idx] = SECOND_PROBE_SENT;
			} else if (event[idx] == EVENT_RX &&
				   from.sin_addr.s_addr != get_interface_ip(idx)) {
				state[idx] =
					process_probe_resp(&tx_msg, &rx_msg, state[idx],
								   &probe_wait_time[idx],
							   start_wait, stop_wait, idx);
			} else {
				recalc_timeout(&probe_wait_time[idx], start_wait,
					       stop_wait, 250);
			}
			break;

		case SECOND_PROBE_SENT:
			if (event[idx] == EVENT_TIMEOUT) {
				DBG("Sending Third probe to index = %d\r\n",idx );
				prepare_probe(&tx_msg, idx);
				mdns_send_msg(&tx_msg, mc_sock, htons(5353),
					      config_g[idx].iface_handle);
				SET_TIMEOUT(&probe_wait_time[idx], 250);
				state[idx] = THIRD_PROBE_SENT;
			} else if (event[idx] == EVENT_RX &&
				   from.sin_addr.s_addr != get_interface_ip(idx)) {
				state[idx] =
					process_probe_resp(&tx_msg, &rx_msg, state[idx],
							   &probe_wait_time[idx],
							   start_wait, stop_wait, idx);
			} else {
				recalc_timeout(&probe_wait_time[idx], start_wait,
					       stop_wait, 250);
			}
			break;

		case THIRD_PROBE_SENT:
			if (event[idx] == EVENT_TIMEOUT) {
				/* Okay.  We now own our name.  Announce it. */
				ret = prepare_announcement(&tx_msg, 255, idx);
				if (ret != 0)
					break;
				mdns_send_msg(&tx_msg, mc_sock, htons(5353),
					      config_g[idx].iface_handle);
				timeout = NULL;
				/* Set Current timeout to infinite */
				SET_TIMEOUT(&probe_wait_time[idx], 0);
				state[idx] = READY_TO_RESPOND;
		} else if (event[idx] == EVENT_RX &&
			   from.sin_addr.s_addr != get_interface_ip(idx)){
				state[idx] =
					process_probe_resp(&tx_msg, &rx_msg, state[idx],
							   &probe_wait_time[idx],
							   start_wait, stop_wait, idx);
			} else {
				recalc_timeout(&probe_wait_time[idx], start_wait,
					       stop_wait, 250);
			}
			break;
		}
	}
	os_mutex_put(&mdns_mutex);
	interface_state[idx] = RUNNING;
	return event[idx];
}

/* This is the mdns thread function */
static void do_responder(void)
{
	int max_sock;
	int msg[2];
	int ret;
	struct sockaddr_in from;
	int active_fds;
	fd_set fds;
	int len = 0, i, config_idx = 0, min_idx;
	struct timeval *timeout;
	struct timeval probe_wait_time[MDNS_MAX_SERVICE_CONFIG];
	socklen_t in_size = sizeof(struct sockaddr_in);
	int state[MDNS_MAX_SERVICE_CONFIG] = {INIT, INIT};
	int event[MDNS_MAX_SERVICE_CONFIG];
	uint32_t start_wait, stop_wait;
	responder_enabled = 1;

	ret = os_mutex_create(&mdns_mutex, "mdns-mutex", OS_MUTEX_INHERIT);
	if (ret != WM_SUCCESS)
		return;
	while (responder_enabled) {

		FD_ZERO(&fds);
		FD_SET(mc_sock, &fds);
		FD_SET(ctrl_sock, &fds);
		max_sock = ctrl_sock > mc_sock ? ctrl_sock : mc_sock;
		min_idx = get_min_timeout(probe_wait_time,
					  MDNS_MAX_SERVICE_CONFIG);
		if (get_msec_time(probe_wait_time[min_idx]) != 0)
			timeout = &probe_wait_time[min_idx];
		else
			timeout = NULL;
		start_wait = mdns_time_ms();
		active_fds = net_select(max_sock + 1, &fds, NULL, NULL,
				timeout);
		stop_wait = mdns_time_ms();

		if (active_fds < 0)
			LOG("error: net_select() failed: %d\r\n", active_fds);

		/* handlde control events outside of the state machine. If we ever have
		 * more than just a HALT command, we may have to move it.
		 */
		if (FD_ISSET(ctrl_sock, &fds)) {
			DBG("Responder got control message.\r\n");
			in_size = sizeof(struct sockaddr_in);
			ret =
			    recvfrom(ctrl_sock, (char *)&msg, sizeof(msg),
				     MSG_DONTWAIT, (struct sockaddr *)&from,
				     &in_size);
			if (ret == -1) {
				LOG("Warning: responder failed to get control message\r\n");
			} else {
				if (msg[0] == MDNS_CTRL_HALT) {
					/* Send the goodbye packet.  This is same as announcement,
					 * but with a TTL of 0
					 */
				for (i = 0; i < MDNS_MAX_SERVICE_CONFIG; i++) {
					if (interface_state[i] != STOPPED) {
						ret = prepare_announcement(
							&tx_msg, 0, i);
					if (ret != 0)
						break;
					mdns_send_msg(&tx_msg, mc_sock,
					      htons(5353),
					      config_g[i].iface_handle);
					}
					interface_state[i] = STOPPED;
				}
				LOG("responder done.\r\n");
					responder_enabled = 0;
					break;
				} else if (msg[0] == MDNS_CTRL_IFACE_UP) {
					state[(int)msg[1]] = INIT;
					send_init_probes(msg[1], state, event, probe_wait_time);
				} else if (msg[0] == MDNS_CTRL_IFACE_DOWN) {
					send_close_probe(msg[1]);
				} else if (msg[0] == MDNS_CTRL_REMOVE_SERVICE) {
					if (interface_state[msg[1]] != STOPPED)
						send_close_probe(msg[1]);
					config_g[msg[1]].services = NULL;
					config_g[msg[1]].iface_handle = NULL;
					num_config_g--;
				}
			}
		}

		if (FD_ISSET(mc_sock, &fds)) {
			in_size = sizeof(struct sockaddr_in);
			len =
			    recvfrom(mc_sock, (char *)rx_msg.data,
				     sizeof(rx_msg.data), MSG_DONTWAIT,
				     (struct sockaddr *)&from, &in_size);
			if (len < 0) {
				LOG("responder failed to recv packet\r\n");
				continue;
			}
			config_idx = get_config_idx(from.sin_addr.s_addr);
			ret = mdns_parse_message(&rx_msg, len);
			if (ret != 0)
				continue;
			update_others_timeout(config_idx, probe_wait_time, start_wait, stop_wait);
			if (interface_state[config_idx] != RUNNING)
				continue;
			event[config_idx] = EVENT_RX;
		} else {
			update_others_timeout(min_idx, probe_wait_time, start_wait, stop_wait);
			if (interface_state[config_idx] != RUNNING)
				continue;
			else {
				event[min_idx] = EVENT_TIMEOUT;
				config_idx = min_idx;
			}
		}
		if (interface_state[config_idx] != STOPPED)
		/* We will be in below state machine only after above is completed */
			responder_state_machine(from, state, event, config_idx, probe_wait_time);
	}
	if (!responder_enabled) {
		LOG("Signalled to stop mdns_responder\r\n");
		os_mutex_delete(&mdns_mutex);
		os_thread_self_complete(NULL);
	}
}

int responder_launch(const char *domain, char *hostname)
{
	int i;

	for (i = 0; i < MDNS_MAX_SERVICE_CONFIG; i++)
		interface_state[i] = STOPPED;
        /* populate the fqdn */
	if (domain == NULL)
		domain = "local";
	if (!valid_label(hostname) || !valid_label(domain))
		return -WM_E_MDNS_INVAL;
	/* populate fqdn */
	hname = hostname;
	domname = domain;
	num_config_g = 0;
	/* number of mdns config opt will be used in do responder thread */
	reset_fqdn();

	mc_sock = mdns_socket_mcast(inet_addr("224.0.0.251"), htons(5353));
	if (mc_sock < 0) {
		LOG("error: unable to open multicast socket in responder\r\n");
		return mc_sock;
	}

	/* create both ends of the control socket */
	ctrl_sock = mdns_socket_loopback(htons(MDNS_CTRL_RESPONDER), 1);

	if (ctrl_sock < 0) {
		LOG("Failed to create responder control socket: %d\r\n",
		    ctrl_sock);
		return -WM_FAIL;
	}

	responder_thread =
	    mdns_thread_create(do_responder, MDNS_THREAD_RESPONDER);
	if (responder_thread == NULL)
		return -WM_FAIL;
	return WM_SUCCESS;
}

static int signal_and_wait_for_responder_halt()
{
	int total_wait_time = 100 * 100;    /* 10 seconds */
	int check_interval = 100;   /* 100 ms */
	int num_iterations = total_wait_time / check_interval;

	if (!responder_enabled) {
		LOG("Warning: mdns responder not running\r\n");
		return WM_SUCCESS;
	}

	while (responder_enabled && num_iterations--)
		os_thread_sleep(os_msec_to_ticks(check_interval));

	if (!num_iterations)
		LOG("Error: timed out waiting for mdns responder to stop\r\n");

	return responder_enabled ? -WM_FAIL : WM_SUCCESS;
}

int mdns_iface_state_change(void  *iface, int state)
{
	int ret, config_idx = 0;
	int msg[2];

	config_idx = get_interface_idx(iface);
	if (state == UP && interface_state[config_idx] != STOPPED) {
		return -WM_E_MDNS_INVAL;
	}
	if (state == DOWN && interface_state[config_idx] == STOPPED) {
		return -WM_E_MDNS_INVAL;
	}
	if (config_idx == -WM_FAIL) {
		return -WM_E_MDNS_INVAL;
	}

	if (state == UP || state == REANNOUNCE)
		msg[0] = MDNS_CTRL_IFACE_UP;
	else if (state == DOWN)
		msg[0] = MDNS_CTRL_IFACE_DOWN;
	else
		return -WM_E_MDNS_INVAL;
	msg[1] = config_idx;
	ret = mdns_send_ctrl_iface_msg(msg, MDNS_CTRL_RESPONDER, sizeof(msg));
	return ret;
}

int responder_halt(void)
{
	int ret, i;

	ret = mdns_send_ctrl_msg(MDNS_CTRL_HALT, MDNS_CTRL_RESPONDER);
	if (ret != 0) {
		LOG("Warning: failed to send HALT msg to mdns responder\r\n");
		return -WM_FAIL;
	}
	ret = signal_and_wait_for_responder_halt();

	if (ret != WM_SUCCESS)
		LOG("Warning: failed to HALT mdns responder\r\n");

	/* force a halt */
	if (responder_enabled != false) {
		LOG("Warning: failed to halt mdns responder, forcing.\r\n");
		responder_enabled = false;
	}

	ret = os_thread_delete(responder_thread);
	if (ret != WM_SUCCESS)
		LOG("Warning: failed to delete thread.\r\n");

	for (i = 0; i < MDNS_MAX_SERVICE_CONFIG; i++) {
		config_g[i].iface_handle = NULL;
		config_g[i].services = NULL;
		interface_state[i] = STOPPED;
	}
	ret = mdns_socket_close(ctrl_sock);
	ret = mdns_socket_close(mc_sock);

	return ret;
}

int mdns_add_service_iface(struct mdns_service_config config)
{
	int i, ret, num_services = 0;
	struct mdns_service **services;

	if (config.iface_handle == NULL)
		return -WM_E_MDNS_INVAL;

	if (num_config_g == MDNS_MAX_SERVICE_CONFIG)
		return -WM_FAIL;

	for (i = 0; i < MDNS_MAX_SERVICE_CONFIG; i++)
		if (config_g[i].iface_handle == NULL) {
			config_g[i].iface_handle = config.iface_handle;
			services = config.services;
			if (services != NULL) {
				config_g[i].services = services;
				for (; *services != NULL; services++) {
					ret = validate_service(*services);
					if (ret != 0)
						return ret;
					reset_fqsn(*services);
					num_services++;
				}
				/* check to make sure all of our names and services will fit */
			}
			reset_fqsn(&services_dnssd);
			ret = prepare_probe(&tx_msg, i);
			if (ret == -1) {
				config_g[i].iface_handle = NULL;
				config_g[i].services = NULL;
				return -WM_E_MDNS_TOOBIG;
			}
			if (max_probe_growth(num_services) > TAILROOM(&tx_msg)) {
				config_g[i].iface_handle = NULL;
				config_g[i].services = NULL;
				LOG("Insufficient space for host name and service names\r\n");
				return -WM_E_MDNS_TOOBIG;
			}
			
			ret = check_max_response(&rx_msg, &tx_msg, i);
			if (ret == -1) {
				config_g[i].iface_handle = NULL;
				config_g[i].services = NULL;
				LOG("Insufficient space for host name and service names in response\r\n");
				return -WM_E_MDNS_TOOBIG;
			}
			if (max_response_growth(num_services) > TAILROOM(&tx_msg)) {
				config_g[i].iface_handle = NULL;
				config_g[i].services = NULL;
				LOG("Insufficient growth for host name and service names in response\r\n");
				return -WM_E_MDNS_TOOBIG;
			}
			num_config_g++;
			return WM_SUCCESS;
		}
	return WM_SUCCESS;
}

int mdns_remove_service_iface(void *iface)
{
	int i, ret, msg[2];

	for (i = 0; i < MDNS_MAX_SERVICE_CONFIG; i++)
		if (iface == config_g[i].iface_handle) {
			msg[0] = MDNS_CTRL_REMOVE_SERVICE;
			msg[1] = i;
			ret = mdns_send_ctrl_iface_msg(msg,
						       MDNS_CTRL_RESPONDER,
						       sizeof(msg));
			return ret;
		}
	return -WM_E_MDNS_INVAL;
}

#ifdef MDNS_TESTS
/* This is a probe packet with a single query for foo.local and an authority ns
 * with a name pointer to foo.local
 */
char pkt0[] =
    { 0xE4, 0x53, 0x01, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01, 0x00,
	0x00, 0x03, 0x66, 0x6F, 0x6F, 0x05, 0x6C, 0x6F, 0x63, 0x61, 0x6C,
	0x00, 0x00, 0x01, 0x00, 0x01, 0xC0, 0x0C, 0x00, 0x01, 0x00, 0x01,
	0x00, 0x00, 0x00, 0xFF, 0x00, 0x04, 0xC0, 0xA8, 0x00, 0x07
};

/* here's a packet that was causing some trouble. */
char pkt1[] = {
	0x0, 0x0, 0x0, 0x84, 0x0, 0x0, 0x0, 0x7, 0x0, 0x0, 0x0, 0x0,
	0x1b, 0x6c, 0x69, 0x6e, 0x75, 0x78, 0x2d, 0x32, 0x20, 0x5b, 0x30,
	0x30, 0x3a, 0x31, 0x36, 0x3a, 0x62, 0x36, 0x3a, 0x65, 0x66, 0x3a,
	0x33, 0x30, 0x3a, 0x35, 0x37, 0x5d, 0xc, 0x5f, 0x77, 0x6f, 0x72,
	0x6b, 0x73, 0x74, 0x61, 0x74, 0x69, 0x6f, 0x6e, 0x4, 0x5f, 0x74,
	0x63, 0x70, 0x5, 0x6c, 0x6f, 0x63, 0x61, 0x6c, 0x0, 0x0, 0x10,
	0x80, 0x1, 0x0, 0x0, 0x11, 0x94, 0x0, 0x1, 0x0, 0x4, 0x5f, 0x73,
	0x73, 0x68, 0xc0, 0x35, 0x0, 0xc, 0x0, 0x1, 0x0, 0x0, 0x11, 0x94,
	0x0, 0xa, 0x7, 0x6c, 0x69, 0x6e, 0x75, 0x78, 0x2d, 0x32, 0xc0,
	0x4c, 0xc0, 0x5d, 0x0, 0x21, 0x80, 0x1, 0x0, 0x0, 0x0, 0x78, 0x0,
	0x10, 0x0, 0x0, 0x0, 0x0, 0x0, 0x16, 0x7, 0x6c, 0x69, 0x6e, 0x75,
	0x78, 0x2d, 0x32, 0xc0, 0x3a, 0xc0, 0x79, 0x0, 0x1c, 0x80, 0x1,
	0x0, 0x0, 0x0, 0x78, 0x0, 0x10, 0xfe, 0x80, 0x0, 0x0, 0x0, 0x0,
	0x0, 0x0, 0x2, 0x16, 0xb6, 0xff, 0xfe, 0xef, 0x30, 0x57, 0xc0,
	0x79, 0x0, 0x1, 0x80, 0x1, 0x0, 0x0, 0x0, 0x78, 0x0, 0x4, 0xc0,
	0xa8, 0x1, 0x3f, 0xc0, 0x5d, 0x0, 0x10, 0x80, 0x1, 0x0, 0x0, 0x11,
	0x94, 0x0, 0x1, 0x0, 0x9, 0x5f, 0x73, 0x65, 0x72, 0x76, 0x69, 0x63,
	0x65, 0x73, 0x7, 0x5f, 0x64, 0x6e, 0x73, 0x2d, 0x73, 0x64, 0x04,
	0x5f, 0x75, 0x64, 0x70, 0xc0, 0x3a, 0x00, 0x0c, 0x00, 0x01, 0x00,
	0x00, 0x11, 0x94, 0x00, 0x02, 0xc0, 0x28
};

/* and here's yet another */
char pkt2[] = {
	0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x05, 0x46, 0x72, 0x6f, 0x64, 0x6f, 0x05, 0x6c, 0x6f, 0x63,
	0x61, 0x6c, 0x00, 0x00, 0x01, 0x00, 0x01, 0xc0, 0x0c, 0x00, 0x1c,
	0x00, 0x01,
};

/* this one was causing a segfault in prepare_response */
char pkt3[] = {
	0x0, 0x0, 0x0, 0x0, 0x0, 0xb, 0x0, 0x3, 0x0, 0x6,
	0x0, 0x0, 0xb, 0x5f, 0x61, 0x66, 0x70, 0x6f, 0x76, 0x65,
	0x72, 0x74, 0x63, 0x70, 0x4, 0x5f, 0x74, 0x63, 0x70, 0x5,
	0x6c, 0x6f, 0x63, 0x61, 0x6c, 0x0, 0x0, 0xc, 0x80, 0x1,
	0x4, 0x5f, 0x73, 0x6d, 0x62, 0xc0, 0x18, 0x0, 0xc, 0x80,
	0x1, 0x4, 0x5f, 0x72, 0x66, 0x62, 0xc0, 0x18, 0x0, 0xc,
	0x80, 0x1, 0x6, 0x5f, 0x61, 0x64, 0x69, 0x73, 0x6b, 0xc0,
	0x18, 0x0, 0xc, 0x80, 0x1, 0x5, 0x5f, 0x62, 0x62, 0x73,
	0x6e, 0x4, 0x5f, 0x75, 0x64, 0x70, 0xc0, 0x1d, 0x0, 0xc,
	0x80, 0x1, 0x6, 0x6d, 0x61, 0x72, 0x76, 0x69, 0x6e, 0xc0,
	0x28, 0x0, 0xff, 0x80, 0x1, 0x6, 0x6d, 0x61, 0x72, 0x76,
	0x69, 0x6e, 0x4, 0x5f, 0x73, 0x73, 0x68, 0xc0, 0x18, 0x0,
	0xff, 0x80, 0x1, 0x6, 0x6d, 0x61, 0x72, 0x76, 0x69, 0x6e,
	0x9, 0x5f, 0x73, 0x66, 0x74, 0x70, 0x2d, 0x73, 0x73, 0x68,
	0xc0, 0x18, 0x0, 0xff, 0x80, 0x1, 0x6, 0x6d, 0x61, 0x72,
	0x76, 0x69, 0x6e, 0xc0, 0x4b, 0x0, 0xff, 0x80, 0x1, 0x6,
	0x6d, 0x61, 0x72, 0x76, 0x69, 0x6e, 0xc0, 0x1d, 0x0, 0xff,
	0x80, 0x1, 0xc0, 0x9f, 0x0, 0xff, 0x80, 0x1, 0xc0, 0x28,
	0x0, 0xc, 0x0, 0x1, 0x0, 0x0, 0x11, 0x90, 0x0, 0x2,
	0xc0, 0x5c, 0xc0, 0x33, 0x0, 0xc, 0x0, 0x1, 0x0, 0x0,
	0x11, 0x8b, 0x0, 0xc, 0x9, 0x6c, 0x75, 0x63, 0x6b, 0x79,
	0x6c, 0x75, 0x6b, 0x65, 0xc0, 0x33, 0xc0, 0x4b, 0x0, 0xc,
	0x0, 0x1, 0x0, 0x0, 0x11, 0x90, 0x0, 0x2, 0xc0, 0x92,
	0xc0, 0x5c, 0x0, 0x21, 0x0, 0x1, 0x0, 0x0, 0x0, 0x78,
	0x0, 0x8, 0x0, 0x0, 0x0, 0x0, 0x1, 0xbd, 0xc0, 0x9f,
	0xc0, 0x69, 0x0, 0x21, 0x0, 0x1, 0x0, 0x0, 0x0, 0x78,
	0x0, 0x8, 0x0, 0x0, 0x0, 0x0, 0x0, 0x16, 0xc0, 0x9f,
	0xc0, 0x7b, 0x0, 0x21, 0x0, 0x1, 0x0, 0x0, 0x0, 0x78,
	0x0, 0x8, 0x0, 0x0, 0x0, 0x0, 0x0, 0x16, 0xc0, 0x9f,
	0xc0, 0x92, 0x0, 0x21, 0x0, 0x1, 0x0, 0x0, 0x0, 0x78,
	0x0, 0x8, 0x0, 0x0, 0x0, 0x0, 0x0, 0x9, 0xc0, 0x9f,
	0xc0, 0x9f, 0x0, 0x1c, 0x0, 0x1, 0x0, 0x0, 0x0, 0x78,
	0x0, 0x10, 0xfe, 0x80, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
	0x2, 0x1b, 0x63, 0xff, 0xfe, 0xa3, 0xe5, 0xfa, 0xc0, 0x9f,
	0x0, 0x1, 0x0, 0x1, 0x0, 0x0, 0x0, 0x78, 0x0, 0x4,
	0xc0, 0xa8, 0x1, 0xfd,
};

/* this packet contains a dname pointer in a question off in the middle of
 * nowhere
 */
char pkt4[] = {
	0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x05, 0x46, 0x72, 0x6f, 0x64, 0x6f, 0x05, 0x6c, 0x6f, 0x63,
	0x61, 0x6c, 0x00, 0x00, 0x01, 0x00, 0x01, 0xc0, 0xff, 0x00, 0x1c,
	0x00, 0x01,
};

/* this packet has a bad pointer in an SRV answer */
char pkt5[] = {
	0x0, 0x0, 0x0, 0x84, 0x0, 0x0, 0x0, 0x3, 0x0, 0x0, 0x0, 0x0,
	0x1b, 0x6c, 0x69, 0x6e, 0x75, 0x78, 0x2d, 0x32, 0x20, 0x5b, 0x30,
	0x30, 0x3a, 0x31, 0x36, 0x3a, 0x62, 0x36, 0x3a, 0x65, 0x66, 0x3a,
	0x33, 0x30, 0x3a, 0x35, 0x37, 0x5d, 0xc, 0x5f, 0x77, 0x6f, 0x72,
	0x6b, 0x73, 0x74, 0x61, 0x74, 0x69, 0x6f, 0x6e, 0x4, 0x5f, 0x74,
	0x63, 0x70, 0x5, 0x6c, 0x6f, 0x63, 0x61, 0x6c, 0x0, 0x0, 0x10,
	0x80, 0x1, 0x0, 0x0, 0x11, 0x94, 0x0, 0x1, 0x0, 0x4, 0x5f, 0x73,
	0x73, 0x68, 0xc0, 0x35, 0x0, 0xc, 0x0, 0x1, 0x0, 0x0, 0x11, 0x94,
	0x0, 0xa, 0x7, 0x6c, 0x69, 0x6e, 0x75, 0x78, 0x2d, 0x32, 0xc0,
	0xFF, 0xc0, 0x5d, 0x0, 0x21, 0x80, 0x1, 0x0, 0x0, 0x0, 0x78, 0x0,
	0x10, 0x0, 0x0, 0x0, 0x0, 0x0, 0x16, 0x7, 0x6c, 0x69, 0x6e, 0x75,
	0x78, 0x2d, 0x32, 0xc0, 0x3a
};

uint8_t pkt6[] = {
	0x0, 0x0, 0x0, 0x0, 0x0, 0x18, 0x0, 0x1e,
	0x0, 0x0, 0x0, 0x0, 0x4, 0x5f, 0x73, 0x73, 0x68, 0x4, 0x5f,
	0x74, 0x63, 0x70, 0x5, 0x6c, 0x6f, 0x63, 0x61, 0x6c, 0x0,
	0x0, 0xc, 0x0, 0x1, 0x8, 0x5f, 0x6c, 0x69, 0x62, 0x76, 0x69,
	0x72, 0x74, 0xc0, 0x11, 0x0, 0xc, 0x0, 0x1, 0x9, 0x5f, 0x70,
	0x72, 0x65, 0x73, 0x65, 0x6e, 0x63, 0x65, 0xc0, 0x11, 0x0,
	0xc, 0x0, 0x1, 0xc, 0x5f, 0x77, 0x6f, 0x72, 0x6b, 0x73,
	0x74, 0x61, 0x74, 0x69, 0x6f, 0x6e, 0xc0, 0x11, 0x0, 0xc,
	0x0, 0x1, 0x6, 0x68, 0x74, 0x74, 0x70, 0x2d, 0x32, 0xc0,
	0x16, 0x0, 0x1c, 0x0, 0x1, 0xc0, 0x53, 0x0, 0x1, 0x0, 0x1,
	0xa, 0x77, 0x65, 0x62, 0x73, 0x69, 0x74, 0x65, 0x2d, 0x38,
	0x33, 0x5, 0x5f, 0x68, 0x74, 0x74, 0x70, 0xc0, 0x11, 0x0,
	0x10, 0x0, 0x1, 0xc0, 0x66, 0x0, 0x21, 0x0, 0x1, 0x6, 0x68,
	0x74, 0x74, 0x70, 0x2d, 0x33, 0xc0, 0x16, 0x0, 0x1c, 0x0,
	0x1, 0xc0, 0x83, 0x0, 0x1, 0x0, 0x1, 0xa, 0x77, 0x65, 0x62,
	0x73, 0x69, 0x74, 0x65, 0x2d, 0x38, 0x31, 0xc0, 0x71, 0x0,
	0x10, 0x0, 0x1, 0xc0, 0x96, 0x0, 0x21, 0x0, 0x1, 0x4, 0x68,
	0x74, 0x74, 0x70, 0xc0, 0x16, 0x0, 0x1c, 0x0, 0x1, 0xc0,
	0xad, 0x0, 0x1, 0x0, 0x1, 0xa, 0x77, 0x65, 0x62, 0x73, 0x69,
	0x74, 0x65, 0x2d, 0x38, 0x32, 0xc0, 0x71, 0x0, 0x10, 0x0,
	0x1, 0xc0, 0xbe, 0x0, 0x21, 0x0, 0x1, 0x6, 0x68, 0x74, 0x74,
	0x70, 0x2d, 0x34, 0xc0, 0x16, 0x0, 0x1c, 0x0, 0x1, 0xc0,
	0xd5, 0x0, 0x1, 0x0, 0x1, 0xa, 0x77, 0x65, 0x62, 0x73, 0x69,
	0x74, 0x65, 0x2d, 0x38, 0x30, 0xc0, 0x71, 0x0, 0x10, 0x0,
	0x1, 0xc0, 0xe8, 0x0, 0x21, 0x0, 0x1, 0x8, 0x5f, 0x70, 0x72,
	0x69, 0x6e, 0x74, 0x65, 0x72, 0xc0, 0x11, 0x0, 0xc, 0x0,
	0x1, 0xf, 0x5f, 0x70, 0x64, 0x6c, 0x2d, 0x64, 0x61, 0x74,
	0x61, 0x73, 0x74, 0x72, 0x65, 0x61, 0x6d, 0xc0, 0x11, 0x0,
	0xc, 0x0, 0x1, 0xc0, 0x71, 0x0, 0xc, 0x0, 0x1, 0x4, 0x5f,
	0x72, 0x66, 0x62, 0xc0, 0x11, 0x0, 0xc, 0x0, 0x1, 0xc1,
	0x2a, 0x0, 0xc, 0x0, 0x1, 0x0, 0x0, 0x11, 0x94, 0x0, 0xc,
	0x9, 0x6c, 0x75, 0x63, 0x6b, 0x79, 0x6c, 0x75, 0x6b, 0x65,
	0xc1, 0x2a, 0xc0, 0x71, 0x0, 0xc, 0x0, 0x1, 0x0, 0x0, 0x1c,
	0x20, 0x0, 0x1c, 0x19, 0x48, 0x50, 0x20, 0x4c, 0x61, 0x73,
	0x65, 0x72, 0x4a, 0x65, 0x74, 0x20, 0x33, 0x33, 0x39, 0x30,
	0x20, 0x28, 0x44, 0x34, 0x32, 0x31, 0x45, 0x33, 0x29, 0xc0,
	0x71, 0xc0, 0x71, 0x0, 0xc, 0x0, 0x1, 0x0, 0x0, 0x0, 0xff,
	0x0, 0x2, 0xc0, 0x66, 0xc0, 0x71, 0x0, 0xc, 0x0, 0x1, 0x0,
	0x0, 0x0, 0xff, 0x0, 0x2, 0xc0, 0x96, 0xc0, 0x71, 0x0, 0xc,
	0x0, 0x1, 0x0, 0x0, 0x0, 0xff, 0x0, 0x2, 0xc0, 0xbe, 0xc0,
	0x71, 0x0, 0xc, 0x0, 0x1, 0x0, 0x0, 0x0, 0xff, 0x0, 0x2,
	0xc0, 0xe8, 0xc1, 0xe, 0x0, 0xc, 0x0, 0x1, 0x0, 0x0, 0x1c,
	0x20, 0x0, 0x1c, 0x19, 0x48, 0x50, 0x20, 0x4c, 0x61, 0x73,
	0x65, 0x72, 0x4a, 0x65, 0x74, 0x20, 0x33, 0x33, 0x39, 0x30,
	0x20, 0x28, 0x44, 0x34, 0x32, 0x31, 0x45, 0x33, 0x29, 0xc1,
	0xe, 0xc0, 0xff, 0x0, 0xc, 0x0, 0x1, 0x0, 0x0, 0x1c, 0x20,
	0x0, 0x1c, 0x19, 0x48, 0x50, 0x20, 0x4c, 0x61, 0x73, 0x65,
	0x72, 0x4a, 0x65, 0x74, 0x20, 0x33, 0x33, 0x39, 0x30, 0x20,
	0x28, 0x44, 0x34, 0x32, 0x31, 0x45, 0x33, 0x29, 0xc0, 0xff,
	0xc0, 0xe8, 0x0, 0x21, 0x0, 0x1, 0x0, 0x0, 0x0, 0xff, 0x0,
	0x8, 0x0, 0x0, 0x0, 0x0, 0x0, 0x50, 0xc0, 0xd5, 0xc0, 0xd5,
	0x0, 0x1, 0x0, 0x1, 0x0, 0x0, 0x0, 0xff, 0x0, 0x4, 0xc0,
	0xa8, 0x1, 0x50, 0xc0, 0xbe, 0x0, 0x21, 0x0, 0x1, 0x0, 0x0,
	0x0, 0xff, 0x0, 0x8, 0x0, 0x0, 0x0, 0x0, 0x0, 0x50, 0xc0,
	0xad, 0xc0, 0xad, 0x0, 0x1, 0x0, 0x1, 0x0, 0x0, 0x0, 0xff,
	0x0, 0x4, 0xc0, 0xa8, 0x1, 0x52, 0xc0, 0x96, 0x0, 0x21, 0x0,
	0x1, 0x0, 0x0, 0x0, 0xff, 0x0, 0x8, 0x0, 0x0, 0x0, 0x0, 0x0,
	0x50, 0xc0, 0x83, 0xc0, 0x83, 0x0, 0x1, 0x0, 0x1, 0x0, 0x0,
	0x0, 0xff, 0x0, 0x4, 0xc0, 0xa8, 0x1, 0x51, 0xc0, 0x66, 0x0,
	0x21, 0x0, 0x1, 0x0, 0x0, 0x0, 0xff, 0x0, 0x8, 0x0, 0x0,
	0x0, 0x0, 0x0, 0x50, 0xc0, 0x53, 0xc0, 0x53, 0x0, 0x1, 0x0,
	0x1, 0x0, 0x0, 0x0, 0xff, 0x0, 0x4, 0xc0, 0xa8, 0x1, 0x53,
	0xc0, 0x40, 0x0, 0xc, 0x0, 0x1, 0x0, 0x0, 0x11, 0x94, 0x0,
	0x1e, 0x1b, 0x68, 0x65, 0x6e, 0x72, 0x69, 0x6f, 0x73, 0x20,
	0x5b, 0x30, 0x30, 0x3a, 0x32, 0x36, 0x3a, 0x32, 0x64, 0x3a,
	0x31, 0x61, 0x3a, 0x32, 0x64, 0x3a, 0x33, 0x31, 0x5d, 0xc0,
	0x40, 0xc0, 0x40, 0x0, 0xc, 0x0, 0x1, 0x0, 0x0, 0x11, 0x94,
	0x0, 0x1e, 0x1b, 0x6c, 0x69, 0x6e, 0x75, 0x78, 0x2d, 0x32,
	0x20, 0x5b, 0x30, 0x30, 0x3a, 0x31, 0x36, 0x3a, 0x62, 0x36,
	0x3a, 0x65, 0x66, 0x3a, 0x61, 0x66, 0x3a, 0x34, 0x66, 0x5d,
	0xc0, 0x40, 0xc0, 0x40, 0x0, 0xc, 0x0, 0x1, 0x0, 0x0, 0x11,
	0x94, 0x0, 0x1c, 0x19, 0x67, 0x6f, 0x6f, 0x66, 0x79, 0x20,
	0x5b, 0x30, 0x30, 0x3a, 0x33, 0x30, 0x3a, 0x62, 0x64, 0x3a,
	0x62, 0x33, 0x3a, 0x63, 0x61, 0x3a, 0x66, 0x32, 0x5d, 0xc0,
	0x40, 0xc0, 0x40, 0x0, 0xc, 0x0, 0x1, 0x0, 0x0, 0x11, 0x94,
	0x0, 0x1d, 0x1a, 0x70, 0x68, 0x6f, 0x6f, 0x65, 0x79, 0x20,
	0x5b, 0x30, 0x30, 0x3a, 0x32, 0x34, 0x3a, 0x37, 0x65, 0x3a,
	0x36, 0x66, 0x3a, 0x35, 0x34, 0x3a, 0x35, 0x34, 0x5d, 0xc0,
	0x40, 0xc0, 0x40, 0x0, 0xc, 0x0, 0x1, 0x0, 0x0, 0x11, 0x94,
	0x0, 0x23, 0x20, 0x73, 0x75, 0x70, 0x65, 0x72, 0x73, 0x6e,
	0x69, 0x66, 0x66, 0x65, 0x72, 0x20, 0x5b, 0x30, 0x30, 0x3a,
	0x30, 0x31, 0x3a, 0x38, 0x30, 0x3a, 0x36, 0x38, 0x3a, 0x61,
	0x65, 0x3a, 0x33, 0x39, 0x5d, 0xc0, 0x40, 0xc0, 0x40, 0x0,
	0xc, 0x0, 0x1, 0x0, 0x0, 0x11, 0x94, 0x0, 0x1d, 0x1a, 0x70,
	0x69, 0x72, 0x61, 0x74, 0x65, 0x20, 0x5b, 0x30, 0x30, 0x3a,
	0x32, 0x34, 0x3a, 0x37, 0x65, 0x3a, 0x36, 0x64, 0x3a, 0x33,
	0x32, 0x3a, 0x37, 0x33, 0x5d, 0xc0, 0x40, 0xc0, 0x40, 0x0,
	0xc, 0x0, 0x1, 0x0, 0x0, 0x11, 0x94, 0x0, 0x1d, 0x1a, 0x69,
	0x64, 0x65, 0x66, 0x69, 0x78, 0x20, 0x5b, 0x30, 0x30, 0x3a,
	0x31, 0x35, 0x3a, 0x35, 0x38, 0x3a, 0x64, 0x64, 0x3a, 0x30,
	0x31, 0x3a, 0x66, 0x37, 0x5d, 0xc0, 0x40, 0xc0, 0x40, 0x0,
	0xc, 0x0, 0x1, 0x0, 0x0, 0x11, 0x94, 0x0, 0x1d, 0x1a, 0x6f,
	0x62, 0x65, 0x6c, 0x69, 0x78, 0x20, 0x5b, 0x30, 0x30, 0x3a,
	0x31, 0x30, 0x3a, 0x63, 0x36, 0x3a, 0x62, 0x36, 0x3a, 0x39,
	0x62, 0x3a, 0x64, 0x34, 0x5d, 0xc0, 0x40, 0xc0, 0x40, 0x0,
	0xc, 0x0, 0x1, 0x0, 0x0, 0x11, 0x94, 0x0, 0x1e, 0x1b, 0x66,
	0x6f, 0x78, 0x74, 0x72, 0x6f, 0x74, 0x20, 0x5b, 0x30, 0x30,
	0x3a, 0x32, 0x34, 0x3a, 0x37, 0x65, 0x3a, 0x36, 0x39, 0x3a,
	0x64, 0x38, 0x3a, 0x39, 0x31, 0x5d, 0xc0, 0x40, 0xc0, 0x30,
	0x0, 0xc, 0x0, 0x1, 0x0, 0x0, 0x11, 0x94, 0x0, 0x13, 0x10,
	0x36, 0x36, 0x66, 0x61, 0x61, 0x65, 0x32, 0x64, 0x40, 0x6c,
	0x69, 0x6e, 0x75, 0x78, 0x2d, 0x32, 0xc0, 0x30, 0xc0, 0x21,
	0x0, 0xc, 0x0, 0x1, 0x0, 0x0, 0x11, 0x94, 0x0, 0x1d, 0x1a,
	0x56, 0x69, 0x72, 0x74, 0x75, 0x61, 0x6c, 0x69, 0x7a, 0x61,
	0x74, 0x69, 0x6f, 0x6e, 0x20, 0x48, 0x6f, 0x73, 0x74, 0x20,
	0x70, 0x68, 0x6f, 0x6f, 0x65, 0x79, 0xc0, 0x21, 0xc0, 0xc,
	0x0, 0xc, 0x0, 0x1, 0x0, 0x0, 0x11, 0x94, 0x0, 0xa, 0x7,
	0x6c, 0x69, 0x6e, 0x75, 0x78, 0x2d, 0x32, 0xc0, 0xc, 0xc0,
	0xc, 0x0, 0xc, 0x0, 0x1, 0x0, 0x0, 0x11, 0x94, 0x0, 0xc,
	0x9, 0x6c, 0x75, 0x63, 0x6b, 0x79, 0x6c, 0x75, 0x6b, 0x65,
	0xc0, 0xc, 0xc0, 0xc, 0x0, 0xc, 0x0, 0x1, 0x0, 0x0, 0x11,
	0x94, 0x0, 0xa, 0x7, 0x63, 0x61, 0x72, 0x74, 0x6d, 0x61,
	0x6e, 0xc0, 0xc,
};

void responder_tests(void)
{
	int ret, config_idx;
	struct mdns_service s;
	struct mdns_service_config config;

	memcpy(rx_msg.data, pkt0, sizeof(pkt0));
	ret = mdns_parse_message(&rx_msg, sizeof(pkt0));
	if (ret != 0)
		goto ERROR;	/*      Failed to parse probe-like packet       */

	if (VALID_LENGTH(&rx_msg) != sizeof(pkt0))
		goto ERROR;	/*      parsed packet has unexpected length     */

	/* prepare a probe, decode it, and check the values */
	config.iface_handle = net_get_mlan_handle();
	config.services = NULL;
	/* Since no other services are registered, config index in config_g
	 * wil be 1
	 */
	mdns_add_service_iface(config);
	config_idx = get_interface_idx(config.iface_handle);
	domname = "local";
	hname = "myname";
	reset_fqdn();
	ret = prepare_probe(&rx_msg, config_idx);
	if (ret != 0)
		goto ERROR;	/*      Failed to create probe packet   */

	DBG("Created probe:\r\n");
	debug_print_message(&rx_msg);
	if (max_probe_growth(0) > TAILROOM(&rx_msg))
		goto ERROR;	/* Insufficient tail room after creating probe  */

	memcpy(tx_msg.data, rx_msg.data, sizeof(rx_msg.data));
	ret = mdns_parse_message(&tx_msg, sizeof(tx_msg.data));
	if (ret != 0)
		goto ERROR;	/* Failed to parse probe packet */
	DBG("Parsed probe:\r\n");
	debug_print_message(&tx_msg);

	DBG("Size: %d\r\n", sizeof(pkt1));
	memcpy(rx_msg.data, pkt1, sizeof(pkt1));
	ret = mdns_parse_message(&rx_msg, sizeof(pkt1));
	if (ret != 0)
		goto ERROR;	/*      Failed to parse pkt1    */

	if (VALID_LENGTH(&rx_msg) != sizeof(pkt1))
		goto ERROR;	/*      parsed packet has unexpected length     */
	DBG("Parsed packet 1:\r\n");
	debug_print_message(&rx_msg);

	memcpy(rx_msg.data, pkt2, sizeof(pkt2));
	ret = mdns_parse_message(&rx_msg, sizeof(pkt2));
	if (ret != 0)
		goto ERROR;	/*      Failed to parse pkt2    */

	if (VALID_LENGTH(&rx_msg) != sizeof(pkt2))
		goto ERROR;	/*      parsed packet has unexpected length     */
	DBG("Parsed packet 2:\r\n");
	debug_print_message(&rx_msg);

	memcpy(rx_msg.data, pkt3, sizeof(pkt3));
	ret = mdns_parse_message(&rx_msg, sizeof(pkt3));
	if (ret != 0)
		goto ERROR;	/*      Failed to parse pkt3    */

	if (VALID_LENGTH(&rx_msg) != sizeof(pkt3))
		goto ERROR;	/*      parsed packet has unexpected length     */

	DBG("Parsed packet 3:\r\n");
	debug_print_message(&rx_msg);
	ret = prepare_response(&rx_msg, &tx_msg, config_idx);
	if (ret != 0)
		goto ERROR;	/*      Failed to prepare response to pkt3      */

	memcpy(rx_msg.data, pkt4, sizeof(pkt4));
	ret = mdns_parse_message(&rx_msg, sizeof(pkt4));
	if (ret != -1)
		goto ERROR;	/*      Yikes!  parsed packet with bad dnmame.  */

	memcpy(rx_msg.data, pkt4, sizeof(pkt4));
	ret = mdns_parse_message(&rx_msg, sizeof(pkt4));
	if (ret != -1)
		goto ERROR;	/*      Yikes!  parsed packet with bad dnmame.  */

	s.servname = "newservice";
	s.servtype = "servey";
	s.proto = 0;
	reset_fqsn(&s);
	ret = memcmp(s.fqsn, "\012newservice\7_servey\4_tcp\5local", 31);
	if (ret != 0)
		goto ERROR;	/*      Failed to reset fqsn    */

	memcpy(rx_msg.data, pkt6, sizeof(pkt6));
	ret = mdns_parse_message(&rx_msg, sizeof(pkt6));
	if (ret != 0)
		goto ERROR;	/*      Failed to parse pkt6    */
	if (VALID_LENGTH(&rx_msg) != sizeof(pkt6))
		goto ERROR;	/*      parsed packet has unexpected length */

	DBG("Parsed packet 6:\r\n");
	debug_print_message(&rx_msg);
	goto SUCCESS;

ERROR:
	LOG("Error");
SUCCESS:
	LOG("Success");
}
#endif
