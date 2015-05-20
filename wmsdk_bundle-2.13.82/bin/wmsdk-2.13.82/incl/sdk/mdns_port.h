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


/*
 *
 * mdns_port.h: porting layer for mdns
 *
 * The porting layer is comprised of functions that must be implemented and
 * linked with mdns.  Various compiler and system defines are also implemented
 * in this file.  Feel free to expand these to work with you system.
 */

#ifndef __MDNS_PORT_H__
#define __MDNS_PORT_H__

#if 0
/* system-dependent definitions */
#if MDNS_SYSTEM_LINUX
/* bsd socket stuff */
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h> /* close */

#include <stdio.h>  /* sprintf */
#include <stdint.h> /* for uint8_t and friends */
#include <string.h> /* memset, memcpy, memmove, strlen, strchr, strcpy */

#else
#error "mdns target system is not defined"
#endif

/* compiler-dependent definitions */
#ifdef __GNUC__
/* wrappers to define packed structures */
#define BEGIN_PACK
#define END_PACK __attribute__((__packed__))

#else
#error "mdns compiler is not defined"
#endif

#endif
#include <wmtypes.h>
#include <stdio.h>

/* For little endian systems */
#ifndef MDNS_ENDIAN_LITTLE
#define MDNS_ENDIAN_LITTLE
#endif

/* For Big endian systems */
/*
#ifndef MDNS_ENDIAN_BIG
#define MDNS_ENDIAN_BIG
#endif
*/

#if defined(MDNS_ENDIAN_BIG) && defined(MDNS_ENDIAN_LITTLE)
#error "Please define only one endianness"
#elif !defined(MDNS_ENDIAN_BIG) && !defined(MDNS_ENDIAN_LITTLE)
#error "Please define endianness of target"
#endif

/*
 * mdsn_thread_entry: Thread entry point function
 */
typedef void (*mdns_thread_entry)(void);

/*
 * mdns_thread_create: Create a thread
 *
 * mdns_thread_create should create and launch the thread.  mdns launches one
 * thread for the responder and one for the querier.
 *
 * entry: thread entry point function
 * data: data to be passed to entry when thread is launched
 *
 * id: some targets may wish to know which thread is being created with a
 * specific call to mdns_thread_create.  To facilitate this, a thread id is
 * passed at thread creation time.  One of the MDNS_THREAD_* id values will be
 * passed at thread creation time.
 *
 * Returns: NULL on failure; a pointer to an opaque type that represents the
 * thread on success.  This return type is passed to the other thread
 * functions.
 *
 */
#define MDNS_THREAD_RESPONDER 1
#define MDNS_THREAD_QUERIER	2
void *mdns_thread_create(mdns_thread_entry entry, int id);

/*
 * mdns_thread_delete: Delete a thread
 *
 * t: pointer to thread to be deleted.  If NULL, no action is taken.
 *
 */
void mdns_thread_delete(void *t);

/*
 * mdns_thread_yield: yield to other runnable threads
 *
 * Some mdns routines need to yeild after sending commands to the mdns thread.
 * This allows the mdns thread to run and respond to the command.
 */
void mdns_thread_yield(void *t);

/*
 * mdns_log: printf-like function to write log messages
 *
 * The mdns daemon will write log messages depending on its build-time
 * log-level.  See mdns.h for details.
 */
#define mdns_log wmprintf
//void mdns_log(const char *fmt, ...);

/*
 * mdns_time_ms: get current time in milliseconds
 *
 * The mdns daemon needs a millisecond up counter for calculating timeouts.
 * The base of the count is arbitrary.  For example, this function could return
 * the number of milliseconds since boot, or since the beginning of the epoch,
 * etc.  Wrap-around is handled internally.  The precision should be to the
 * nearest 10ms.
 */
uint32_t mdns_time_ms(void);

/*
 * mdns_rand_range: get random number between 0 and n
 *
 * The mdns daemon needs to generate random numbers for various timeout ranges.
 * This function is needed to return a random number between 0 and n. It should
 * provide a uniform random field.
 *
 * The program must initialize the random number generator before starting the
 * mdns thread.
 */
int mdns_rand_range(int n);

/* mdns_socket_mcast: create a multicast socket
 *
 * More specifically, this function must create a non-blocking socket bound to
 * the to the specified IPv4 multicast address and port on the interface on
 * which mdns is to be running.  The multicast TTL should be set to 255 per the
 * mdns specification.
 *
 * mcast_addr: multicast address to join in network byte order
 *
 * port: multicast port in network byte order.
 *
 * Returns the socket descriptor suitable for use with FD_SET, select,
 * recvfrom, etc.  Returns -1 on error.
 *
 * Note: if available, the SO_REUSEADDR sockopt should be enabled.  This allows
 * for the stopping and restarting of mdns without waiting for the socket
 * timeout.
 *
 * Note: when recvfrom is called on this socket, the MSG_DONTWAIT flag will be
 * passed.  This may be sufficient to ensure non-blocking behavior.
 */
int mdns_socket_mcast(uint32_t mcast_addr, uint16_t port);

/* mdns_socket_loopback: create a loopback datagram (e.g., UDP) socket
 *
 * port: desired port in network byte order.
 *
 * listen: boolean value.  1 indicates that the socket should be a non-blocking
 * listening socket.  Accordingly, it should be bound to the specified port on
 * the loopback address.  0 indicates that the socket will be used to send
 * packets to a listening loopback socket (i.e., a client socket).  client
 * sockets should block on send and recv.
 *
 * Returns the socket descriptor.  If listen is 1, the socket should be
 * suitable for use with FD_SET, select, recvfrom, etc.  Otherwise, the socket
 * should be suitable for calls to sendto.  Returns -1 on error.
 *
 * Note: if available, the SO_REUSEADDR sockopt should be enabled if listen is
 * 1.  This allows for the stopping and restarting of mdns without waiting for
 * the socket timeout.
 *
 * Note: when recvfrom is called on this socket and non-blocking behavior is
 * desired, the MSG_DONTWAIT flag will be passed.  This may be sufficient to
 * ensure non-blocking behavior.
 */
int mdns_socket_loopback(uint16_t port, int listen);

/* mdns_socket_close: close a socket
 *
 * s: non-negative control socket returned from either mdns_socket_mcast or
 * mdns_socket_loopback.
 */
int mdns_socket_close(int s);

#endif /* __MDNS_PORT_H__ */
