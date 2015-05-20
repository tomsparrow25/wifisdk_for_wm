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

#ifndef __MDNS_PRIVATE_H__
#define __MDNS_PRIVATE_H__

#include <wmtypes.h>
#include <string.h>
#include <wmstdio.h>
#include <mdns.h>
#include "wm_os.h"
#include "wm_net.h"
#include "mdns_message.h"

enum mdns_status_t {
	INIT,
	FIRST_PROBE_SENT,	/* wait a random amount of time and probe */
	SECOND_PROBE_SENT,	/* wait 250ms and probe */
	THIRD_PROBE_SENT,	/* wait 250ms and probe */
	READY_TO_RESPOND,	/* we have claimed our name */
	READY_TO_SEND,		/* response prepared, waiting to send */
};

enum mdns_event_t {
	EVENT_RX,		/* recieved a DNS packet */
	EVENT_CTRL,		/* recieved a control message */
	EVENT_TIMEOUT,	/* timed out waiting for DNS packet */
};

/* internal control message types. */
enum mdns_commands_t {
	MDNS_CTRL_HALT = 0,
	MDNS_CTRL_MONITOR,
	MDNS_CTRL_UNMONITOR,
	MDNS_CTRL_IFACE_UP,
	MDNS_CTRL_IFACE_DOWN,
	MDNS_CTRL_REMOVE_SERVICE,
};

/* Return values from mdns_prepare_response */
#define RS_ERROR		-1		/* error in querry or response prep */
#define RS_NO_SEND		0		/* nothing to send */
#define RS_SEND			0x01	/* send */
#define RS_SEND_DELAY	0x02	/* this is a service query, delay 20-120ms */

#ifdef MDNS_DBG
extern char *statenames[];
extern char *eventnames[];
#endif

/* logging helpers */
#if defined(MDNS_LOG) && !defined(MDNS_DBG)
#define LOG mdns_log
#define debug_print_message(m) do {} while (0)
#define debug_print_name(m, n) do {} while (0)
#define DBG(...) do {} while (0)

#elif defined(MDNS_LOG) && defined(MDNS_DBG)
#define LOG mdns_log
void debug_print_message(struct mdns_message *m);
void debug_print_name(struct mdns_message *m, uint8_t *name);
#define DBG mdns_log

#elif !defined(MDNS_LOG) && !defined(MDNS_DBG)
#define debug_print_message(m) do {} while (0)
#define debug_print_name(m, n) do {} while (0)
#define DBG(...) do {} while (0)
#define LOG(...) do {} while (0)

#else
#error "MDNS_DBG is only supported if MDNS_LOG is enabled"
#endif

/* helpers for accessing elements.  value arguments are in host byte order */
#define get_uint16_t(p) ((*(uint8_t *)(p) << 8) | *((uint8_t *)(p) + 1))
#define get_uint32_t(p) ((*(uint8_t *)(p) << 24) | \
					   (*((uint8_t *)(p) + 1) << 16) | \
					   (*((uint8_t *)(p) + 2) << 8) |  \
					   (*((uint8_t *)(p) + 3)))
#define set_uint16_t(p, v) do { \
	*(uint8_t *)(p) = ((v) >> 8) & 0xff;	 \
	*((uint8_t *)(p) + 1) = (v) & 0xff;		 \
	} while (0)
#define set_uint32_t(p, v)  do { \
	*(uint8_t *)(p) = ((v) >> 24) & 0xff;	 \
	*((uint8_t *)(p) + 1) = ((v) >> 16) & 0xff;		\
	*((uint8_t *)(p) + 2) = ((v) >> 8) & 0xff;		\
	*((uint8_t *)(p) + 3) = (v) & 0xff;				\
	} while (0)

/* helpers for handling dns names */
uint8_t *dname_put_label(uint8_t *dst, const char *label);
int dname_size(uint8_t *dname);
int dname_increment(uint8_t *name);
int dname_cmp(uint8_t *p1, uint8_t *n1, uint8_t *p2, uint8_t *n2);
int dnameify(char *name, uint16_t kvlen, uint8_t sep, uint8_t *dest);
int dname_copy(uint8_t *dst, uint8_t *p, uint8_t *src);
uint8_t *dname_label_to_c(char *dst, uint8_t *p, uint8_t *src,
						  int keepuscores);
int dname_label_cmp(uint8_t *p1, uint8_t *l1, uint8_t *p2, uint8_t *l2);
uint8_t *dname_label_next(uint8_t *p, uint8_t *n);
void txt_to_c_ncpy(char *dst, int dlen, uint8_t *txt, int tlen);
int dname_overrun(uint8_t *p, uint8_t *e, uint8_t *n);
/* d points to a dname pointer in a message.  return the offset in the
 * packet.
 */
#define POINTER(d) ((((*(d) & ~0xC0) << 8) | *((d) + 1)) & 0xFFFFU)
#define IS_POINTER(c) ((c) & 0xC0)

void dname_tests(void);

#ifdef MDNS_TESTS

#define FAIL_UNLESS(condition, ...) \
	do { \
		if (!(condition)) { \
			mdns_log("FAIL: %s: %d: ", __FILE__, __LINE__); \
			mdns_log(__VA_ARGS__); \
			mdns_log("\r\n"); \
		} else { \
			mdns_log("PASS: %s: %d\r\n", __FILE__, __LINE__); \
		} \
	} while (0)


#define test_title(s) mdns_log("========= %s =========\r\n", s)

#endif


/* internal flags for service data */
#define SERVICE_CHECKED_FLAG	1
#define SERVICE_HAS_A_FLAG		2
#define SERVICE_HAS_SRV_FLAG	4
#define SRV_ADDED				8
#define TXT_ADDED				16

#define SERVICE_IS_READY(s) \
	(((s)->flags & (SERVICE_HAS_A_FLAG|SERVICE_HAS_SRV_FLAG)) ==	\
	 (SERVICE_HAS_A_FLAG|SERVICE_HAS_SRV_FLAG))

/* internal API functions for responder */
int responder_launch(const char *domain, char *hostname);
int responder_halt(void);
void responder_tests(void);

/* internal API functions for querier */
int query_launch();
int query_halt(void);

/* internal common functions used by responder and querier */
/* return the amount of tail room in the message m */
#define TAILROOM(m) ((m)->end - (m)->cur + 1)

/* ensure that message m has at least l bytes of room left */
#define CHECK_TAILROOM(m, l)										  \
	do {															  \
		if (TAILROOM(m) < l) {										  \
			DBG("Warning: truncated mdns message (%d).\r\n", __LINE__); \
			return -1;												  \
		}															  \
	} while (0)

/* return the number of valid bytes in message m's buffer */
#define VALID_LENGTH(m) ((m)->cur - (m)->data)

/* dumb macro to set a struct timeval to "ms" milliseconds. */
#define SET_TIMEOUT(t, ms)								\
	do {												\
		(t)->tv_sec = (ms)/1000;						\
		(t)->tv_usec = ((ms)%1000) * 1000;				\
	} while (0)

int mdns_send_msg(struct mdns_message *m, int sock, unsigned short port, void *config_id);
int mdns_send_ctrl_msg(int msg, uint16_t port);
int mdns_send_ctrl_iface_msg(int msg[], uint16_t port, int size);
int mdns_add_srv_ptr_txt(struct mdns_message *m, struct mdns_service *s,
			 uint8_t *fqdn, int section, uint32_t ttl);
int mdns_add_srv(struct mdns_message *m, uint16_t priority,
		 uint16_t weight, uint16_t port, uint8_t *target);
int mdns_add_txt(struct mdns_message *m, char *txt, uint16_t len);
int mdns_add_question(struct mdns_message *m, uint8_t *qname, uint16_t qtype,
					  uint16_t qclass);
int mdns_add_answer(struct mdns_message *m, uint8_t *name, uint16_t type,
					uint16_t class, uint32_t ttl);
int mdns_add_answer_o(struct mdns_message *m, uint16_t offset, uint16_t type,
					  uint16_t class, uint32_t ttl);
int mdns_add_answer_lo(struct mdns_message *m, uint8_t *label, uint16_t offset,
		       uint16_t type, uint16_t class, uint32_t ttl);
int mdns_add_authority(struct mdns_message *m, uint8_t *name, uint16_t type,
					   uint16_t class, uint32_t ttl);
int mdns_add_uint32_t(struct mdns_message *m, uint32_t i);
int mdns_add_name(struct mdns_message *m, uint8_t *name);
int mdns_add_name_lo(struct mdns_message *m, uint8_t *label, uint16_t offset);
int mdns_query_init(struct mdns_message *m);
int mdns_parse_message(struct mdns_message *m, int mlen);
int mdns_response_init(struct mdns_message *m);

uint32_t interval(uint32_t start, uint32_t stop);
void recalc_timeout(struct timeval *t, uint32_t start, uint32_t stop,
					uint32_t target);

#ifdef MDNS_DBG
#define ASSERT(condition) do { \
	if (!(condition)) { \
		DBG("%s: %d: ASSERTION FAILED\r\n", __FILE__, __LINE__); \
		while (1); \
	} \
} while (0)

#else
#define ASSERT(condition) do {} while (0)
#endif

#endif /* __MDNS_PRIVATE_H__ */
