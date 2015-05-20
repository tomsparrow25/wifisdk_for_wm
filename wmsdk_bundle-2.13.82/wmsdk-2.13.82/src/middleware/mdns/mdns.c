/*
 * Copyright (C) 2007-2011 cozybit Inc.
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

#include <mdns.h>
#include <stdint.h>
#include <mdns_port.h>

#include "mdns_private.h"

static bool is_responder_started, is_querier_started;

/* parse a resource record from the current pointer and put it in the resource
 * structure r.  Return 0 for success, -1 for failure.
 */
static int parse_resource(struct mdns_message *m, struct mdns_resource *r)
{
	int len;

	r->name = m->cur;
	len = dname_size(m->cur);
	if (len == 0 || len == -1 || len > MDNS_MAX_NAME_LEN) {
		DBG("Warning: invalid name in resource\r\n");
		return -1;
	}
	CHECK_TAILROOM(m, len);
	if (dname_overrun(m->data, m->end, m->cur) != 0) {
		DBG("Warning: bad pointer in resource name\r\n");
		DBG("%c %c %c %c %c %c %c %c %c %c\r\n",
		    m->cur[0], m->cur[1], m->cur[2], m->cur[3], m->cur[4],
		    m->cur[5], m->cur[6], m->cur[7], m->cur[8], m->cur[9]);
		return -1;
	}
	m->cur += len;
	CHECK_TAILROOM(m, 3 * sizeof(uint16_t) + sizeof(uint32_t));
	r->type = get_uint16_t(m->cur);
	m->cur += sizeof(uint16_t);
	r->class = get_uint16_t(m->cur);
	m->cur += sizeof(uint16_t);
	r->ttl = get_uint32_t(m->cur);
	m->cur += sizeof(uint32_t);
	r->rdlength = get_uint16_t(m->cur);
	m->cur += sizeof(uint16_t);
	CHECK_TAILROOM(m, r->rdlength);
	r->rdata = m->cur;
	m->cur += r->rdlength;

	/* validate the resource data */
	switch (r->type) {
	case T_A:
		if (r->rdlength != 4) {
			DBG("Error: insufficient data in A record\r\n");
			return -1;
		}
		break;
	case T_SRV:
	case T_PTR:
		if (dname_overrun(m->data, m->end, r->rdata) != 0) {
			DBG("Warning: bad pointer in resource data\r\n");
			return -1;
		}
		break;
	}

#ifdef MDNS_QUERY_API
	/* sort the records */
	switch (r->type) {
	case T_A:
		SLIST_INSERT_HEAD(&m->as, r, list_item);
		break;
	case T_SRV:
		SLIST_INSERT_HEAD(&m->srvs, r, list_item);
		break;
	case T_TXT:
		SLIST_INSERT_HEAD(&m->txts, r, list_item);
		break;
	case T_PTR:
		SLIST_INSERT_HEAD(&m->ptrs, r, list_item);
		break;
	}
#endif
	return 0;
}

/* Parse an incoming dns message of length blen into the struct mdns_message m.
 * Return 0 for success, -1 for failure.
 */
int mdns_parse_message(struct mdns_message *m, int mlen)
{
	uint16_t t;
	int len, i;

	if (mlen < sizeof(struct mdns_header)) {
		LOG("Warning: DNS message too short.\r\n");
		return -1;
	}
#ifdef MDNS_QUERY_API
	/* The querier needs to look at all PTRs, then all TXTs, then all SRVs,
	 * then all As.  We create these lists here since we're stepping through
	 * all of the answers anyway.
	 */
	SLIST_INIT(&m->as);
	SLIST_INIT(&m->srvs);
	SLIST_INIT(&m->txts);
	SLIST_INIT(&m->ptrs);
#endif

	m->header = (struct mdns_header *)m->data;
	m->len = mlen;
	m->end = m->data + mlen - 1;
	m->cur = (uint8_t *) m->header + sizeof(struct mdns_header);
	m->num_questions = ntohs(m->header->qdcount);
	m->num_answers = ntohs(m->header->ancount);
	m->num_authorities = ntohs(m->header->nscount);
	m->header->flags.num = ntohs(m->header->flags.num);

	if (m->header->flags.fields.opcode != DNS_OPCODE_QUERY) {
		DBG("Ignoring message with opcode != QUERY\r\n");
		return -1;
	}

	if (m->num_questions > MDNS_MAX_QUESTIONS) {
		LOG("Warning: Only parsing first %d questions of %d\r\n",
		    MDNS_MAX_QUESTIONS, m->num_questions);
		m->num_questions = MDNS_MAX_QUESTIONS;
	}
	for (i = 0; i < m->num_questions; i++) {
		/* get qname */
		m->questions[i].qname = m->cur;
		len = dname_size(m->cur);
		if (len == 0 || len == -1 || len > MDNS_MAX_NAME_LEN) {
			DBG("Warning: invalid name in question %d\r\n", i);
			return -1;
		}
		CHECK_TAILROOM(m, len);
		if (dname_overrun(m->data, m->end, m->cur) != 0) {
			DBG("Warning: bad pointer in question name\r\n");
			return -1;
		}

		m->cur += len;

		/* get qtype and qclass */
		CHECK_TAILROOM(m, 2 * sizeof(uint16_t));
		t = get_uint16_t(m->cur);
		m->cur += sizeof(uint16_t);
		if (t > T_ANY)
			DBG("Warning: unexpected type in question: %u\r\n", t);
		m->questions[i].qtype = t;

		/* get qclass */
		t = get_uint16_t(m->cur);
		m->cur += sizeof(uint16_t);
		if (t != C_FLUSH && t != C_IN)
			DBG("Warning: unexpected class in question: %u\r\n", t);
		m->questions[i].qclass = t;
	}

	if (m->num_answers > MDNS_MAX_ANSWERS) {
		LOG("Warning: Only parsing first %d answers of %d\r\n",
		    MDNS_MAX_ANSWERS, m->num_answers);
		m->num_answers = MDNS_MAX_ANSWERS;
	}
	for (i = 0; i < m->num_answers; i++) {
		len = parse_resource(m, &m->answers[i]);
		if (len == -1) {
			DBG("Failed to parse answer %d\r\n", i);
			return -1;
		}
	}

	if (m->num_authorities > MDNS_MAX_AUTHORITIES) {
		LOG("Warning: Only parsing first %d authorities of %d\r\n",
		    MDNS_MAX_ANSWERS, m->num_authorities);
		m->num_authorities = MDNS_MAX_AUTHORITIES;
	}
	for (i = 0; i < m->num_authorities; i++) {
		len = parse_resource(m, &m->authorities[i]);
		if (len == -1) {
			DBG("Failed to parse authority %d.\r\n", i);
			return -1;
		}
	}

	if (ntohs(m->header->arcount) != 0)
		LOG("Warning: Ignroing additional RRs\r\n");

	return 0;
}

/* prepare an empty query message m.  Return 0 for success and -1 for
 * failure.
 */
int mdns_query_init(struct mdns_message *m)
{
	m->len = sizeof(m->data);
	m->header = (struct mdns_header *)m->data;
	m->cur = m->data + sizeof(struct mdns_header);
	m->end = m->data + sizeof(m->data) - 1;
	memset(m->header, 0x00, sizeof(struct mdns_header));
	m->num_questions = 0;
	m->num_answers = 0;
	m->num_authorities = 0;
	return 0;
}

/* prepare an empty response message m.  Return 0 for success and -1 for
 * failure.
 */
int mdns_response_init(struct mdns_message *m)
{
	/* a response is just like a query at first */
	if (mdns_query_init(m) != 0)
		return -1;

	m->header->flags.fields.qr = 1;	/* response */
	m->header->flags.fields.aa = 1;	/* authoritative */
	m->header->flags.fields.rcode = 0;
	m->header->flags.num = htons(m->header->flags.num);
	return 0;
}

/* add a name, type, class, ttl tuple to the message m.  If the ttl is
 * (uint32_t)-1, then we only add the name, type, and class.  This is common
 * functionality used to add questions, answers, and authorities.  No error
 * checking is performed; the caller is responsible for ensuring that all
 * values will fit.  Return 0 for success or -1 for failure.
 */
static int __mdns_add_tuple(struct mdns_message *m, uint8_t * name,
			    uint16_t type, uint16_t class, uint32_t ttl)
{
	uint16_t len = (uint16_t) dname_size(name);
	int size = ttl == (uint32_t) -1 ? len + 2 * sizeof(uint16_t) :
	    len + 2 * sizeof(uint16_t) + sizeof(uint32_t);
	CHECK_TAILROOM(m, size);
	memcpy(m->cur, name, len);
	m->cur += len;
	set_uint16_t(m->cur, type);
	m->cur += sizeof(uint16_t);
	set_uint16_t(m->cur, class);
	m->cur += sizeof(uint16_t);
	if (ttl != (uint32_t) -1) {
		set_uint32_t(m->cur, ttl);
		m->cur += sizeof(uint32_t);
	}
	return 0;
}

/* add a question with name qname, type qtype, and class qclass to the message
 * m.  Return 0 for success, -1 for failure.
 */
int mdns_add_question(struct mdns_message *m, uint8_t * qname, uint16_t qtype,
		      uint16_t qclass)
{
	if (__mdns_add_tuple(m, qname, qtype, qclass, (uint32_t) -1) == -1)
		return -1;
	m->header->qdcount = htons((htons(m->header->qdcount) + 1));
	m->num_questions += 1;
	return 0;
}

/* add an answer to name to the message m.  Return 0 for success, -1 for
 * failure.  Note that this function does not add the resource record data.  It
 * just populates the common header.
 */
int mdns_add_answer(struct mdns_message *m, uint8_t * name, uint16_t type,
		    uint16_t class, uint32_t ttl)
{
	if (__mdns_add_tuple(m, name, type, class, ttl))
		return -1;
	m->header->ancount = htons((htons(m->header->ancount) + 1));
	m->num_answers += 1;
	return 0;
}

/* This is just like mdns_add_answer, but instead of specifying a name, the
 * caller specifies a leading label (which is a C string) and an offset to the
 * suffix.  This allows us to use a bit of compression when we have lots of
 * records with the same label (and we know the offset to that label!)
 */
int mdns_add_answer_lo(struct mdns_message *m, uint8_t * label, uint16_t offset,
		       uint16_t type, uint16_t class, uint32_t ttl)
{
	uint16_t len = (uint16_t) strlen((char *)label);
	/* the size includes 1 byte for the label len and 2 for the offset */
	int size = len + 2 * sizeof(uint16_t) + sizeof(uint32_t) + 3;
	CHECK_TAILROOM(m, size);
	*m->cur++ = len;
	memcpy(m->cur, label, len);
	m->cur += len;
	set_uint16_t(m->cur, 0xC000 | offset);
	m->cur += sizeof(uint16_t);
	set_uint16_t(m->cur, type);
	m->cur += sizeof(uint16_t);
	set_uint16_t(m->cur, class);
	m->cur += sizeof(uint16_t);
	set_uint32_t(m->cur, ttl);
	m->cur += sizeof(uint32_t);

	m->header->ancount = htons((htons(m->header->ancount) + 1));
	m->num_answers += 1;
	return 0;
}

/* Like mdns_add_answer_lo, but instead of specifying a name, the caller
 * specifies just an offset.
 */
int mdns_add_answer_o(struct mdns_message *m, uint16_t offset, uint16_t type,
		      uint16_t class, uint32_t ttl)
{
	/* len includes a 2-byte offset */
	uint16_t size = 3 * sizeof(uint16_t) + sizeof(uint32_t);
	CHECK_TAILROOM(m, size);
	set_uint16_t(m->cur, 0xC000 | offset);
	m->cur += sizeof(uint16_t);
	set_uint16_t(m->cur, type);
	m->cur += sizeof(uint16_t);
	set_uint16_t(m->cur, class);
	m->cur += sizeof(uint16_t);
	set_uint32_t(m->cur, ttl);
	m->cur += sizeof(uint32_t);

	m->header->ancount = htons((htons(m->header->ancount) + 1));
	m->num_answers += 1;
	return 0;
}

/* add a proposed answer for name to the authority section of the message m.
 * Return 0 for success, -1 for failure.  Note that this function does not add
 * the resource record data.  It just populates the common header.
 */
int mdns_add_authority(struct mdns_message *m, uint8_t * name, uint16_t type,
		       uint16_t class, uint32_t ttl)
{
	if (__mdns_add_tuple(m, name, type, class, ttl))
		return -1;
	m->header->nscount = htons((htons(m->header->nscount) + 1));
	m->num_authorities += 1;
	return 0;
}

/* add a 4-byte record containing i to the message m.  This is used for A
 * records.
 */
int mdns_add_uint32_t(struct mdns_message *m, uint32_t i)
{
	CHECK_TAILROOM(m, sizeof(uint16_t) + sizeof(uint32_t));
	set_uint16_t(m->cur, sizeof(uint32_t));
	m->cur += sizeof(uint16_t);
	set_uint32_t(m->cur, i);
	m->cur += sizeof(uint32_t);
	return 0;
}

/* add a dns name containing name to the message m.  This is used for cname,
 * ns, and ptr.
 */
int mdns_add_name(struct mdns_message *m, uint8_t * name)
{
	int len = dname_size(name);
	CHECK_TAILROOM(m, len + sizeof(uint16_t));
	set_uint16_t(m->cur, len);
	m->cur += sizeof(uint16_t);
	memcpy(m->cur, name, len);
	m->cur += len;
	return 0;
}

/* like add name, but instead of taking a dname, it takes a leading label
 * (which is a C-string) and an offset to the suffix.
 */
int mdns_add_name_lo(struct mdns_message *m, uint8_t * label, uint16_t offset)
{
	int len = strlen((char *)label);
	CHECK_TAILROOM(m, len + 1 + 2 * sizeof(uint16_t));
	set_uint16_t(m->cur, len + 1 + sizeof(uint16_t));
	m->cur += sizeof(uint16_t);
	*m->cur++ = len;
	memcpy(m->cur, label, len);
	m->cur += len;
	set_uint16_t(m->cur, 0xC000 | offset);
	m->cur += sizeof(uint16_t);
	return 0;
}

/* add a txt string containing txt to the message m.  This is nearly identical
 * to mdns_add_name, but it doesn't have the trailing 0.  That is implied by the
 * length of the record.
 */
int mdns_add_txt(struct mdns_message *m, char *txt, uint16_t len)
{
	CHECK_TAILROOM(m, len + sizeof(uint16_t));
	set_uint16_t(m->cur, len);
	m->cur += sizeof(uint16_t);
	memcpy(m->cur, txt, len);
	m->cur += len;
	return 0;
}

/* add a dns SRV record to message m with given priority, weight, port, and
 * target name.
 */
int mdns_add_srv(struct mdns_message *m, uint16_t priority,
		 uint16_t weight, uint16_t port, uint8_t *target)
{
	int len = dname_size(target);
	CHECK_TAILROOM(m, len + 4 * sizeof(uint16_t));
	set_uint16_t(m->cur, len + 3 * sizeof(uint16_t));
	m->cur += sizeof(uint16_t);
	set_uint16_t(m->cur, priority);
	m->cur += sizeof(uint16_t);
	set_uint16_t(m->cur, weight);
	m->cur += sizeof(uint16_t);
	set_uint16_t(m->cur, port);
	m->cur += sizeof(uint16_t);
	memcpy(m->cur, target, len);
	m->cur += len;
	return 0;
}

/* add all of the required RRs that represent the service s to the specified
 * section of the message m.  Return 0 for success or -1 for error.
 */
int mdns_add_srv_ptr_txt(struct mdns_message *m, struct mdns_service *s,
			 uint8_t *fqdn, int section, uint32_t ttl)
{

	if (section == MDNS_SECTION_ANSWERS) {
		/* If we're populating the answer section, we put all of the data */
		if (mdns_add_answer(m, (uint8_t *) s->fqsn, T_SRV, C_FLUSH, ttl)
		    != 0 || mdns_add_srv(m, 0, 0, s->port, fqdn) != 0)
			return -1;
		if (mdns_add_answer
		    (m, (uint8_t *) s->ptrname, T_PTR, C_FLUSH, ttl) != 0 ||
		    mdns_add_name(m, (uint8_t *) s->fqsn) != 0)
			return -1;
		if (s->keyvals) {
			if (mdns_add_answer
			    (m, (uint8_t *) s->fqsn, T_TXT,
			     C_FLUSH, ttl) != 0 ||
			    mdns_add_txt(m, s->keyvals, s->kvlen) != 0)
				return -1;
		}

	} else if (section == MDNS_SECTION_AUTHORITIES) {
		/* For the authority section , we only need srv and txt */
		if (mdns_add_authority
		    (m, (uint8_t *) s->fqsn, T_SRV, C_FLUSH, ttl) != 0 ||
		    mdns_add_srv(m, 0, 0, s->port, fqdn) != 0)
			return -1;
		if (s->keyvals) {
			if (mdns_add_authority
			    (m, (uint8_t *) s->fqsn,
			     T_TXT, C_FLUSH, ttl) != 0 ||
			    mdns_add_txt(m, s->keyvals, s->kvlen) != 0)
				return -1;
		}

	} else if (section == MDNS_SECTION_QUESTIONS) {
		/* we only add SRV records to the question section when we are probing.
		 * And in this case we just add the fqsn.
		 */
		return mdns_add_question(m, (uint8_t *) s->fqsn, T_ANY, C_IN);
	} else {
		return -1;
	}

	return 0;
}

int mdns_send_msg(struct mdns_message *m, int sock, unsigned short port, void *out_interface)
{
	struct sockaddr_in to;
	int size, len;
	uint32_t ip;
	/* get message length */
	size = (unsigned int)m->cur - (unsigned int)m->header;

	to.sin_family = AF_INET;
	to.sin_port = port;
	to.sin_addr.s_addr = inet_addr("224.0.0.251");

	net_get_if_ip_addr(&ip, out_interface);

	/* If IP address is not set, then either interface is not up 
	 * or interface didn't got the IP from DHCP. In both case Packet
	 * shouldn't be transmitted
	*/
	if (!ip) {
		DBG("Interface is not up\n\r");
		return -WM_FAIL;
	}
	if (setsockopt(sock, IPPROTO_IP, IP_MULTICAST_IF,
		       (char *)&ip, sizeof(ip)) == -1) {
		net_close(sock);
		return -WM_FAIL;
	}

	len = sendto(sock, (char *)m->header, size, 0, (struct sockaddr *)&to,
		     sizeof(struct sockaddr_in));

	if (len < size) {
		LOG("error: failed to send message\r\n");
		return 0;
	}

	DBG("sent %u-byte message\r\n", size);
	return 1;
}

int mdns_send_ctrl_msg(int msg, uint16_t port)
{
	int ret;
	struct sockaddr_in to;
	int s;

	s = mdns_socket_loopback(htons(port), 0);
	if (s < 0) {
		LOG("error: failed to create loopback socket\r\n");
		return s;
	}

	memset((char *)&to, 0, sizeof(to));
	to.sin_family = PF_INET;
	to.sin_port = htons(port);
	to.sin_addr.s_addr = inet_addr("127.0.0.1");
	ret =
	    sendto(s, (char *)&msg, sizeof(msg), 0, (struct sockaddr *)&to,
		   sizeof(to));
	if (ret != -1)
		ret = 0;

	ret = mdns_socket_close(s);
	return ret;
}

int mdns_send_ctrl_iface_msg(int *msg, uint16_t port, int size)
{
	int ret;
	struct sockaddr_in to;
	int s;

	s = mdns_socket_loopback(htons(port), 0);
	if (s < 0) {
		LOG("error: failed to create loopback socket\r\n");
		return s;
	}

	memset((char *)&to, 0, sizeof(to));
	to.sin_family = PF_INET;
	to.sin_port = htons(port);
	to.sin_addr.s_addr = inet_addr("127.0.0.1");

	ret =
	    sendto(s, (char *)msg, size, 0, (struct sockaddr *)&to,
		   sizeof(to));
	if (ret != -1)
		ret = 0;

	ret = mdns_socket_close(s);
	return ret;
}

/* calculate the interval between the start and stop timestamps accounting for
 * wrap.
 */
uint32_t interval(uint32_t start, uint32_t stop)
{
	if (stop >= start)
		return stop - start;
	return UINT32_MAX - start + stop;
}

/* We wanted to timeout after "target" ms, but we got interrupted.  Calculate
 * the new timeout "t" given that we started our timeout at "start" and were
 * interupted at "stop".
 */
void recalc_timeout(struct timeval *t, uint32_t start, uint32_t stop,
		    uint32_t target)
{
	uint32_t waited, remaining;

	waited = interval(start, stop);
	if (target <= waited) {
		SET_TIMEOUT(t, 0);
		return;
	}
	remaining = target - waited;
	SET_TIMEOUT(t, remaining);
}

int mdns_start(const char *domain, char *hostname)
{
	int ret;

	if (hostname) {
		if (is_responder_started != true) {
			ret = responder_launch(domain, hostname);
			if (ret != 0)
				return ret;
			is_responder_started = true;
		} else {
			LOG("mdns responder already started\n\r");
		}
	}
	if (is_querier_started != true) {
		ret = query_launch();
		if (ret != 0)
			return ret;
		is_querier_started = true;
	} else {
		LOG("mdns querier already started\n\r");
	}
	return 0;
}

void mdns_stop(void)
{
	LOG("Stopping mdns.\r\n");
	if (is_responder_started == true) {
		responder_halt();
		is_responder_started = false;
	} else {
		LOG("Can't stop mdns responder; responder not started");
	}
	if (is_querier_started == true) {
		query_halt();
		is_querier_started = false;
	} else {
		LOG("Can't stop mdns querier; querier not started");
	}
}

#ifdef MDNS_TESTS
void mdns_tests(void)
{
	dname_tests();
	responder_tests();
}
#else
void mdns_tests(void)
{
	return;
}
#endif
