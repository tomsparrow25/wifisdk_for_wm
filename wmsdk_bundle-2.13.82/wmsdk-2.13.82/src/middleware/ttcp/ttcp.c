/*
 * Copyright 2008-2013, Marvell International Ltd.
 *
 * Ported to threadx/treck/WMSDK
 */
/*
 *	T T C P . C
 *
 * Test TCP connection.  Makes a connection on port 5001
 * and transfers fabricated buffers or data copied from stdin.
 *
 * Usable on 4.2, 4.3, and 4.1a systems by defining one of
 * BSD42 BSD43 (BSD41a)
 * Machines using System V with BSD sockets should define SYSV.
 *
 * Modified for operation under 4.2BSD, 18 Dec 84
 *      T.C. Slattery, USNA
 * Minor improvements, Mike Muuss and Terry Slattery, 16-Oct-85.
 * Modified in 1989 at Silicon Graphics, Inc.
 *	catch SIGPIPE to be able to print stats when receiver has died 
 *	for tcp, don't look for sentinel during reads to allow small transfers
 *	increased default buffer size to 8K, nbuf to 2K to transfer 16MB
 *	moved default port to 5001, beyond IPPORT_USERRESERVED
 *	make sinkmode default because it is more popular, 
 *		-s now means don't sink/source 
 *	count number of read/write system calls to see effects of 
 *		blocking from full socket buffers
 *	for tcp, -D option turns off buffered writes (sets TCP_NODELAY sockopt)
 *	buffer alignment options, -A and -O
 *	print stats in a format that's a bit easier to use with grep & awk
 *	for SYSV, mimic BSD routines to use most of the existing timing code
 * Modified by Steve Miller of the University of Maryland, College Park
 *	-b sets the socket buffer size (SO_SNDBUF/SO_RCVBUF)
 * Modified Sept. 1989 at Silicon Graphics, Inc.
 *	restored -s sense at request of tcs@brl
 * Modified Oct. 1991 at Silicon Graphics, Inc.
 *	use getopt(3) for option processing, add -f and -T options.
 *	SGI IRIX 3.3 and 4.0 releases don't need #define SYSV.
 * Modified April 19. 2010 at Printing Communications Assoc., Inc. (PCAUSA)
 *	modification sufficient to allow build under gcc and Ubuntu 9.10.
 * Distribution Status -
 *      Public Domain.  Distribution Unlimited.
 */

#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include <cli.h>
#include <cli_utils.h>
#include <wmstdio.h>
#include <ttcp.h>
#include <wm_os.h>
#include <wm_net.h>

#define MAX_NBUF 4294967295lU   /* Maximum value that nbuf can
				   hold. This is set to such a large
				   value to enable execution of ttcp
				   tx for a specified time using -L
				   option */
struct sockaddr_in sinme;
struct sockaddr_in sinhim;
struct sockaddr_in frominet;

static unsigned long  fromlen;
static int fd_tx = -1;			/* fd of network transmitter socket */
static int fd_rx = -1;			/* fd of network receiver socket */
static int new_fd_rx = -1;		/* fd of network receiver socket */

static int buflen = (8*1024);	/* length of buffer */
static char *buf;		/* ptr to dynamic buffer */
static char *savebufaddr;
static unsigned long nbuf = MAX_NBUF;   /* number of buffers
					   to send in sinkmode */
static int noiterations = 1;    /* Default only 1 iteration */
static int interval = 100;      /* Default interval between
				   transmissions */

static int bufoffset;	/* align buffer to this */
static int bufalign = 8;	/* modulo this */

static int udp;		/* 0 = tcp, !0 = udp */
static int options;		/* socket options */
static int one = 1;		/* for 4.3 BSD style setsockopt() */
static short port = 5001;	/* TCP port number */
static char *host;		/* ptr to name of host */
static int trans;		/* 0=receive, !0=transmit mode */
static int sinkmode = 1;	/* 0=normal I/O, !0=sink/source mode */
static int verbose;		/* 0=print basic info, 1=print cpu rate, proc
				 * resource usage. */
static int nodelay;		/* set TCP_NODELAY socket option */
static int b_flag;		/* use mread() */
/* Restrict send buffer size to 2048 bytes. Default of 8k has treck stability issues. */
static int sockbufsize = (8*1024);	/* socket buffer size to use */
/*static int sockbufsize = 0;*/ /* socket buffer size to use */
static char fmt = 'K';		/* output format: k = kilobits, K = kilobytes,
				 *  m = megabits, M = megabytes, 
				 *  g = gigabits, G = gigabytes */
static int touchdata;	        /* access data after reading */
static long wait;               /* usecs to wait between each write */
static long delaycount;         /* number of times delay is put
				   between writes */
static long xmitTime;           /* secs to run xmit for */
static os_timer_t ttcp_timer;   /* timer to control xmit time */
static char timer_running;

static void usage(void)
{
	wmprintf("Usage: ttcp -t [-options] host [ < in ]\r\n");
	wmprintf("       ttcp -r [-options > out]\r\n");
	wmprintf("Common options:\r\n");
	wmprintf
	    ("	-l ##	length of bufs read from or written to network"
	     " (default 1024)\r\n");
	wmprintf("	-u	use UDP instead of TCP\r\n");
	wmprintf
	    ("	-p ##	port number to send to or listen at "
	     "(default 5001)\r\n");
	wmprintf("	-s	-t: source a pattern to network\r\n");
	wmprintf
	    ("		-r: sink (discard) all data from network\r\n");
	wmprintf
	    ("	-A	align the start of buffers to this modulus (default 8)"
	     "\r\n");
	wmprintf
	    ("	-O	start buffers at this offset from the modulus "
	     "(default 0)\r\n");
	wmprintf("	-v	verbose: print more statistics\r\n");
	wmprintf("	-d	set SO_DEBUG socket option\r\n");
	wmprintf
	    ("	-b ##	set socket buffer size (if supported)\r\n");
	wmprintf
	    ("	-f X	format for rate: k,K = kilo{bit,byte};"
	     " m,M = mega; g,G = giga\r\n");
	wmprintf
	    ("        -c ##   number of iterations for tx or rx(default"
	     "1)\r\n");
	wmprintf("Options specific to -t:\r\n");
	wmprintf
	    ("	-n ##	number of source bufs written to network"
	     " (default 2048)\r\n");
	wmprintf
	    ("	-D	don't buffer TCP writes (sets TCP_NODELAY socket"
	     " option)\r\n");
	wmprintf
	    ("        -i ##   interval in msec between two transmissions"
	     "(default 100msec)\r\n");
	wmprintf
	    ("        -w ##   number of microseconds to wait between "
	     "each write\r\n");
	wmprintf
	    ("        -L ##   length in seconds to run the transmit for\r\n");
	wmprintf("Options specific to -r:\r\n");
	wmprintf
	    ("	-B	for -s, only output full blocks as specified by -l"
	     " (for TAR)\r\n");
	wmprintf
	    ("	-T	\"touch\": access each byte as it's read\r\n");
	wmprintf("	-k	kill the receiver thread.\r\n");
}

static char stats[128];
static unsigned int nbytes;	/* bytes on net */
static unsigned long numCalls;	/* # of I/O system calls */
static unsigned int realt;	/* user, real time (milli seconds) */

static void ttcp_e_more(const char *s, int fd);
static void pattern(char *cp, int cnt);
static void prep_timer();
static void read_timer();
static int Nread(int fd, void *buf, int count);
static int Nwrite(int fd, void *buf, int count);
static void delay();
static int mread();
static char *outfmt();

#define CTRL_PORT 12678
static char ctrl_msg[16];

static int ttcp_enabled;

static os_thread_t tx_thread;
static os_thread_t rx_thread;
static os_thread_stack_define(rx_stack, 1024);
static os_thread_stack_define(tx_stack, 1024);

static int ctrl = -1;
static int rx_stop(void);
static int tx_stop(void);
static int finish;
static int os_thread_run;

static void report(void)
{
	read_timer(stats, sizeof(stats));
	if (realt == 0)
		realt = 1;
	wmprintf("ttcp%s: %d bytes in %d ms = %s/sec +++\r\n",
		       trans ? "-t" : "-r",
		       nbytes, realt, outfmt((nbytes / realt) * 1000));
	wmprintf
	    ("ttcp%s: %lu I/O calls, msec/call = %d, calls/sec = %d\r\n",
	     trans ? "-t" : "-r", numCalls, realt / (numCalls),
	     numCalls * 1000 / realt);
	if (verbose) {
		wmprintf("ttcp%s: buffer address %x\r\n",
			       trans ? "-t" : "-r", (unsigned)buf);
	}
}

static void ttcp_clean_sockets()
{
	int ret;

	if (ctrl != -1) {
		ret = net_close(ctrl);
		if (ret != 0) {
			ttcp_w("Failed to close control socket: %d",
			       net_get_sock_error(ctrl));
		}
		ctrl = -1;
	}

	if (new_fd_rx != -1) {
		ret = net_close(new_fd_rx);
		if (ret != 0) {
			ttcp_w("Failed to close data socket: %d",
			       net_get_sock_error(new_fd_rx));
		}

		new_fd_rx = -1;
	}

	if (fd_rx != -1) {
		ret = net_close(fd_rx);
		if (ret != 0) {
			ttcp_w("Failed to close data socket: %d",
			       net_get_sock_error(fd_rx));
		}

		fd_rx = -1;
	}

	if (fd_tx != -1) {
		ret = net_close(fd_tx);
		if (ret != 0) {
			ttcp_w("Failed to close data socket: %d",
			       net_get_sock_error(fd_tx));
		}

		fd_tx = -1;
	}
}

static void rx_main(unsigned long data)
{
	int error;
	int active_fds;
	fd_set fds;
	struct sockaddr_in ctrl_listen;
	int addr_len;
	int max_sock;
	struct sockaddr_in peer;
#ifndef CONFIG_LWIP_STACK
	int   peerlen = sizeof(peer);
#else
	unsigned long  peerlen = sizeof(peer);
#endif
	int cnt;
	char *addrstr;
	int ret = -1;

	nbytes = 0;
	numCalls = 0;
	realt = 0;

	/* create listening control socket */
	ctrl = net_socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (ctrl < 0) {
		error = net_get_sock_error(ctrl);
		ttcp_e("Failed to create control socket: %d.",
			       error);
		goto done;
	}
	setsockopt(ctrl, SOL_SOCKET, SO_REUSEADDR, (char *)&one, sizeof(one));
	ctrl_listen.sin_family = PF_INET;
	ctrl_listen.sin_port = htons(CTRL_PORT);
	ctrl_listen.sin_addr.s_addr = net_inet_aton("127.0.0.1");
	addr_len = sizeof(struct sockaddr_in);
	error = net_bind(ctrl, (struct sockaddr *)&ctrl_listen, addr_len);
	if (error < 0) {
		ttcp_e_more("Failed to bind control socket", ctrl);
		os_thread_self_complete(NULL);
	}

	if (udp == 0) {
		/* We are the TCP server and
		 * should listen for the connections
		 */
		if (net_listen(fd_rx, 1) < 0) {
			ttcp_e_more("listen", fd_rx);
			goto done;
		}
	}

	while (1) {

		wmprintf("ttcp-r: waiting for connection\r\n");
		nbytes = 0;
		numCalls = 0;
		realt = 0;

		FD_ZERO(&fds);
		FD_SET(fd_rx, &fds);
		FD_SET(ctrl, &fds);

		max_sock = (fd_rx > ctrl) ? fd_rx : ctrl;

		active_fds = net_select(max_sock + 1, &fds, NULL, NULL, NULL);
		/* Error in select? */
		if (active_fds < 0) {
			ttcp_e_more("select failed", -1);
			goto done;
		}

		/* check the control socket */
		if (FD_ISSET(ctrl, &fds)) {
			error = recvfrom(ctrl, ctrl_msg, sizeof(ctrl_msg), 0,
					 (struct sockaddr *)0, (socklen_t *)0);
			if (error == -1) {
				ttcp_e_more("Failed to get control"
				    " message: %d\r\n", ctrl);
			} else {
				if (strcmp(ctrl_msg, "HALT") == 0) {
					goto done;
				}
			}
		}

		if (FD_ISSET(fd_rx, &fds)) {
			fromlen = sizeof(frominet);

			if (udp == 0) {
				new_fd_rx  = net_accept(fd_rx, (struct sockaddr *)&frominet,
					       &fromlen);
				if (new_fd_rx < 0) {
					ttcp_e_more("net_accept", new_fd_rx);
					goto done;
				}

				if (getpeername(new_fd_rx, (struct sockaddr *)&peer,
					&peerlen) < 0) {
					ttcp_e_more("getpeername", fd_rx);
					goto done;
				}
				addrstr = inet_ntoa(peer.sin_addr);
				wmprintf("ttcp-r: net_accept from %s\r\n",
					       addrstr ? addrstr : "?");
				prep_timer();
				while ((cnt = Nread(new_fd_rx, buf, buflen)) > 0)
					nbytes += cnt;
			} else {
				/* udp */
				int going = 0;
				while ((cnt = Nread(fd_rx, buf, buflen)) > 0) {
					if (cnt <= 4) {
						if (going)
							break;
						going = 1;
						prep_timer();
					} else {
						nbytes += cnt;
					}
				}
			}
			report();
#ifndef CONFIG_LWIP_STACK

			if (tfGetSocketError(fd_rx)) {
				ttcp_e_more("IO", fd_rx);
				goto done;
			}
#endif

			if (new_fd_rx != -1) {
				ret = net_close(new_fd_rx);
				if (ret != 0) {
					ttcp_w("Failed to close data "
					       "socket: %d",
					       net_get_sock_error(new_fd_rx));
				}

				new_fd_rx = -1;
			}

			if ((--noiterations) == 0) {
				break;
			}
		}
	}


 done:
	ttcp_clean_sockets();

	finish = 1;
	ttcp_enabled = 0;
	os_mem_free(savebufaddr);
	savebufaddr = NULL;
	os_thread_self_complete(NULL);
}

int create_and_bind_socket()
{
	int fd = -1;

	fd = net_socket(AF_INET, udp ? SOCK_DGRAM : SOCK_STREAM,
			0);
	if (fd < 0) {
		ttcp_e_more("socket", fd);
		goto done;
	}

	if (options) {
		if (setsockopt
		    (fd, SOL_SOCKET, options, (void *)&one,
		     sizeof(one)) < 0) {
			ttcp_e_more("setsockopt", fd);
			goto done;
		}
	 }

	if (net_bind(fd, (struct sockaddr *)&sinme, sizeof(sinme))
	    < 0) {
		ttcp_e_more("bind", fd);
		goto done;
	}

	return fd;

done:
	if (fd != -1) {
		net_close(fd);
	}
	return -1;
}

void ttcp_timer_handler(os_timer_arg_t arg)
{
	timer_running = 0;
}

void tx_main(unsigned long data)
{
	int numbuf;
	while (ttcp_enabled && noiterations) {
		nbytes = 0;
		numCalls = 0;
		realt = 0;
		numbuf = nbuf;

		if (interval) {
			os_thread_sleep(os_msec_to_ticks(interval));
		}

		if (udp) {
			wmprintf("ttcp-t: starting udp stream\r\n");
		} else {
			wmprintf("ttcp-t: connecting to server\r\n");

			/* We are the client if transmitting */
#ifdef TCP_NODELAY
			if (nodelay) {
				if (setsockopt(fd_tx, IPPROTO_TCP, TCP_NODELAY,
					       (void *)&one, sizeof(one)) < 0) {
					ttcp_e_more("setsockopt: nodelay",
						    fd_tx);
					goto done;
				}
			}
#endif
			if (connect
			    (fd_tx, (struct sockaddr *)&sinhim,
			     sizeof(sinhim)) < 0) {
				ttcp_e_more("connect", fd_tx);
				goto done;
			}
		}

		prep_timer();
		delaycount = 0;
		timer_running = 1;
		if (xmitTime) {
			if (os_timer_create(&ttcp_timer,
					    "TTCP Timer",
					    os_msec_to_ticks(xmitTime),
					    &ttcp_timer_handler,
					    NULL,
					    OS_TIMER_ONE_SHOT,
					    OS_TIMER_AUTO_ACTIVATE) < 0) {
				ttcp_e("error in creating timer\r\n");
			}
		}

		if (sinkmode) {
			pattern(buf, buflen);
			if (udp)
				Nwrite(fd_tx, buf, 4);	/* rcvr start */
			while (ttcp_enabled &&
			       timer_running &&
			       numbuf--) {
				int cnt;

				cnt = Nwrite(fd_tx, buf, buflen);
				if (cnt > 0) {
					nbytes += cnt;
				}
			}
			if (udp)
				Nwrite(fd_tx, buf, 4);	/* rcvr end */
		} else {
			/* Should never get here.  We don't have stdin/stdout,
			 * so we only support sinkmode.
			 */
			goto done;
		}
#ifndef CONFIG_LWIP_STACK

		if (tfGetSocketError(fd_tx)) {
			ttcp_e_more("IO", fd_tx);
			goto done;
		}
#endif

		report();
		net_close(fd_tx);

		if ((fd_tx = create_and_bind_socket()) == -1) {
			goto done;
		}

		noiterations--;
	}

	if (udp) {
		Nwrite(fd_tx, buf, 4);	/* rcvr end */
		Nwrite(fd_tx, buf, 4);	/* rcvr end */
		Nwrite(fd_tx, buf, 4);	/* rcvr end */
		Nwrite(fd_tx, buf, 4);	/* rcvr end */
	}

 done:
	if (fd_tx != -1) {
		net_close(fd_tx);
		fd_tx = -1;
	}

	finish = 1;
	ttcp_enabled = 0;
	os_mem_free(savebufaddr);
	savebufaddr = NULL;
	os_thread_self_complete(NULL);
}

static int rx_start(int fd)
{
	int ret;
	
	fd_rx = fd;
	ttcp_enabled = 1;
	ret = os_thread_create(&rx_thread, "ttcp-r", (void *)rx_main, 0,
			       &rx_stack, OS_PRIO_3);
	if (ret != NET_SUCCESS) {
		ttcp_e("Failed to create rx thread: %d", ret);
		ttcp_enabled = 0;
		os_thread_run = 0;
		return -WM_E_TTCP_RX_THREAD_CREATE;
	}
	os_thread_run = 1;
	return WM_SUCCESS;
}

static int tx_start(int fd)
{
	int ret;

	fd_tx = fd;
	ttcp_enabled = 1;
	ret = os_thread_create(&tx_thread, "ttcp-t", (void *)tx_main, 0,
			       &tx_stack, OS_PRIO_3);
	if (ret != NET_SUCCESS) {
		ttcp_e("Failed to create tx thread: %d", ret);
		ttcp_enabled = 0;
		os_thread_run = 0;
		return -WM_E_TTCP_TX_THREAD_CREATE;
	}
	os_thread_run = 1;
	return WM_SUCCESS;
}

static int send_ctrl_msg(const char *msg)
{
	int ret;
	struct sockaddr_in to_addr;

	if (ctrl == -1)
		return -WM_E_TTCP_SOCKET;

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

static int rx_stop(void)
{
	int ret, final = 0;

	if (ttcp_enabled) {
		ret = send_ctrl_msg("HALT");
		if (ret != 0) {
			ttcp_w("Failed to send HALT: %d.", ret);
			final = 1;
		} 	
	}

	if(os_thread_run == 1) {
        	os_thread_sleep(os_msec_to_ticks(100));
        	ret = os_thread_delete(&rx_thread);
        	if(ret != WM_SUCCESS)
        	{
			ttcp_w("Failed to delete rx_thread");
			final = 1;
        	}
		else
		{
			os_thread_run = 0;
		}
	}

	return final;
}

static int tx_stop(void)
{
	int ret, final = 0;

	if (ttcp_enabled) {
		ttcp_enabled = 0;
	}

	if (os_thread_run == 1) {
		os_thread_sleep(os_msec_to_ticks(200));
		ret = os_thread_delete(&tx_thread);
		if (ret != WM_SUCCESS) {
			ttcp_w("Failed to delete tx_thread");
			final = 1;
		} else {
			os_thread_run = 0;
		}
	}
	return final;
}

static int ttcp_kill_thread()
{
	int ret;

	if (trans) {
		ret = tx_stop();
	} else {
		ret = rx_stop();
	}

	ttcp_clean_sockets();

	ttcp_enabled = 0;
	finish = 0;

	if (savebufaddr) {
		os_mem_free(savebufaddr);
		savebufaddr = NULL;
	}
	return ret;
}

static void ttcp_init_default()
{
	udp = 0;
	nbuf = MAX_NBUF;
	buflen = 8 * 1024;
	verbose = 0;
	nodelay = 0;
	bufoffset = 0;
	touchdata = 0;
	port = 5001;
	bufalign = 0;
	noiterations = 1;
	interval = 100;
	wait = 0;
	xmitTime = 0;
}

static void main(int argc, char **argv)
{
	struct hostent *addr_tmp = NULL;
	struct sockaddr_in tmp;
	int c;
	int ret;
	options = SO_REUSEADDR;
	int fd;

	/* Restrict send buffer size to 2048 bytes. Default of 8k has treck stability issues. */
#ifdef CONFIG_LWIP_STACK
	sockbufsize = (8*1024);
#else
	sockbufsize = (2*1024);
#endif

	if (argc < 2) {
		usage();
		return;
	}

	cli_optind = 1;
	while ((c = cli_getopt(argc, argv, "drstuvBDTb:f:l:n:c:i:p:w:L:A:O:k"))
	       != -1) {
		switch (c) {

		case 'B':
			ttcp_e("-B not supported. We don't have stdin/stdout");
			return;
		case 't':
			if (ttcp_enabled) {
				ttcp_e("ttcp already running."
				       "Kill thread using ttcp -k");
				return;
			}
			if (finish) {
				ttcp_kill_thread();
			}
			ttcp_init_default();
			trans = 1;
			break;
		case 'r':
			if (ttcp_enabled) {
				ttcp_e("ttcp already running."
				       "Kill thread using ttcp -k");
				return;
			}
			if (finish) {
				ttcp_kill_thread();
			}
			ttcp_init_default();
			trans = 0;
			break;
		case 'd':
			options |= SO_DEBUG;
			break;
		case 'D':
#ifdef TCP_NODELAY
			nodelay = 1;
#else
			ttcp_w("-D option ignored: TCP_NODELAY socket "
			     "option not supported");
#endif
			break;
		case 'n':
			nbuf = atoi(cli_optarg);
			break;
		case 'c':
			noiterations = atoi(cli_optarg);
			break;
		case 'i':
			interval = atoi(cli_optarg);
			break;
		case 'l':
			buflen = atoi(cli_optarg);
			break;
		case 's':
			ttcp_e("-s not supported. We don't have stdin/stdout");
			return;
		case 'p':
			port = atoi(cli_optarg);
			break;
		case 'u':
			udp = 1;
			break;
		case 'v':
			verbose = 1;
			break;
		case 'w':
			wait = atoi(cli_optarg);
			break;
		case 'L':
			xmitTime = atoi(cli_optarg) * 1000;
			break;
		case 'A':
			bufalign = atoi(cli_optarg);
			break;
		case 'O':
			bufoffset = atoi(cli_optarg);
			break;
		case 'b':
#if defined(SO_SNDBUF) || defined(SO_RCVBUF)
			sockbufsize = atoi(cli_optarg);
#else
			ttcp_w("-b option ignored: SO_SNDBUF/SO_RCVBUF"
			     " socket options not supported");
#endif
			break;
		case 'f':
			fmt = *cli_optarg;
			break;
		case 'T':
			touchdata = 1;
			break;
		case 'k':
			if (ttcp_kill_thread()) {
				ttcp_e("ttcp:Failed to kill ttcp thread\r\n");
			} else {
				wmprintf("ttcp:Killed ttcp thread\r\n");
			}
			return;

		default:
			usage();
			return;
		}
	}

	if (trans) {
		/* xmitr */
		if (cli_optind == argc) {
			usage();
			return;
		}
		memset((char *)&sinhim, 0, sizeof(sinhim));
		host = argv[cli_optind];
		if (atoi(host) > 0) {
			/* Numeric */
			sinhim.sin_family = AF_INET;
			sinhim.sin_addr.s_addr = inet_addr(host);
		} else {
			ret = net_gethostbyname(host, &addr_tmp);
			if (ret != WM_SUCCESS)
				ttcp_e("Failed to gethostbyname: err=%d", ret);
                        memset(&tmp, 0, sizeof(struct sockaddr_in));
                        memcpy(&tmp.sin_addr.s_addr, addr_tmp->h_addr_list[0],
                               addr_tmp->h_length);

			sinhim.sin_family = AF_INET;
			sinhim.sin_addr.s_addr = tmp.sin_addr.s_addr;
		}
		sinhim.sin_port = htons(port);
		sinme.sin_port = 0;	/* free choice */
	} else {
		/* rcvr */
		sinme.sin_port = htons(port);
	}
	sinme.sin_family = AF_INET;

	savebufaddr = buf = os_mem_alloc(buflen + bufalign);
	if (!buf) {
		ttcp_e("Can't allocate buffer with size %d\r\n",
		    buflen + bufalign);
		return;
	}

	if (bufalign != 0)
		buf +=
		    (bufalign - ((int)buf % bufalign) + bufoffset) % bufalign;

#if defined(SO_SNDBUF) || defined(SO_RCVBUF)
#ifndef CONFIG_LWIP_STACK
	if (sockbufsize) {
		if (trans) {
			options |= SO_SNDBUF;
		} else {
			options |= SO_RCVBUF;
		}
	}
#endif
#endif

	fd = create_and_bind_socket();
	if (fd != -1) {
		if (trans) {
			tx_start(fd);
		} else {
			rx_start(fd);
		}
	} else {

		ttcp_enabled = 0;
		os_mem_free(savebufaddr);
		savebufaddr = NULL;
	}
}

static void ttcp_e_more(const char *s, int fd)
{
	ttcp_e("ttcp%s: %s: %d\r\n", trans ? "-t" : "-r", s,
		fd == -1 ? -1 : net_get_sock_error(fd));
	return;
}

static void pattern(char *cp, int cnt)
{
	register char c;
	c = 0;
	while (cnt-- > 0) {
		while (!isprint((c & 0x7F)))
			c++;
		*cp++ = (c++ & 0x7F);
	}
}

static char *outfmt(int b)
{
	static char obuf[50];
	switch (fmt) {
	case 'G':
		sprintf(obuf, "%d GB", b / 1024 / 1024 / 1024);
		break;
	default:
	case 'K':
		sprintf(obuf, "%d KB", b / 1024);
		break;
	case 'M':
		sprintf(obuf, "%d MB", b / 1024 / 1024);
		break;
	case 'g':
		sprintf(obuf, "%d Gbit", b * 8 / 1024 / 1024 / 1024);
		break;
	case 'k':
		sprintf(obuf, "%d Kbit", b * 8 / 1024);
		break;
	case 'm':
		sprintf(obuf, "%d Mbit", b * 8 / 1024 / 1024);
		break;
	}
	return obuf;
}

static unsigned int time0;	/* Time at which timing started */

static void prusage();

#if defined(SYSV)
/*ARGSUSED*/
static getrusage(ignored, ru)
int ignored;
register struct rusage *ru;
{
	struct tms buf;

	times(&buf);

	/* Assumption: HZ <= 2147 (LONG_MAX/1000000) */
	ru->ru_stime.tv_sec = buf.tms_stime / HZ;
	ru->ru_stime.tv_usec = ((buf.tms_stime % HZ) * 1000000) / HZ;
	ru->ru_utime.tv_sec = buf.tms_utime / HZ;
	ru->ru_utime.tv_usec = ((buf.tms_utime % HZ) * 1000000) / HZ;
}

/*ARGSUSED*/
static gettimeofday(tp, zp)
struct timeval *tp;
struct timezone *zp;
{
	tp->tv_sec = time(0);
	tp->tv_usec = 0;
}
#endif				/* SYSV */

/*
 *			P R E P _ T I M E R
 */
static void prep_timer()
{
	time0 = os_ticks_get();
}

/*
 *			R E A D _ T I M E R
 * 
 */
static void read_timer(char *str, int len)
{
	unsigned int timedol;
	char line[132];

	timedol = os_ticks_get();
	prusage(&timedol, &time0, line);
	(void)strncpy(str, line, len);

	realt = (timedol - time0) * os_ticks_to_msec(1)
		- wait * delaycount / 1000;
}

static void prusage(unsigned int *e, unsigned int *b, char *outp)
{
	sprintf(outp, "%d ms", (*e - *b) * 10);
}

/*
 *			N R E A D
 */
static int Nread(int fd, void *buf, int count)
{
	struct sockaddr_in from;
	socklen_t len = sizeof(from);
	register int cnt;
	if (udp) {
		cnt =
		    recvfrom(fd, buf, count, 0, (struct sockaddr *)&from, &len);
		numCalls++;
	} else {
		if (b_flag)
			/* Should never get here.
			 * We don't support the -B option */
			return -WM_E_TTCP_NREAD_B_OPTION;
		else {
			cnt = mread(fd, buf, count);
			numCalls++;
		}
		if (touchdata && cnt > 0) {
			register int c = cnt, sum;
			register char *b = buf;
			while (c--)
				sum += *b++;
		}
	}
	return cnt;
}

/*
 *			N W R I T E
 */
static int Nwrite(int fd, void *buf, int count)
{
	register int cnt;
	if (udp) {
 again:
		cnt =
		    sendto(fd, buf, count, 0, (struct sockaddr *)&sinhim,
			   sizeof(sinhim));
		numCalls++;
		if (cnt < 0 && net_get_sock_error(fd) == NET_ENOBUFS) {
			delay(18000);
			goto again;
		}
	} else {
		cnt = send(fd, buf, count, 0);
		numCalls++;
	}
	if (wait) {
		delaycount++;
		delay(wait);
	}
	return cnt;
}

static void delay(unsigned int us)
{
	struct timeval tv;

	tv.tv_sec = 0;
	tv.tv_usec = us;
	net_select(1, (fd_set *) 0, (fd_set *) 0, (fd_set *) 0, &tv);
}

/*
 *			M R E A D
 *
 * This function performs the function of a read(II) but will
 * call read(II) multiple times in order to get the requested
 * number of characters.  This can be necessary because
 * network connections don't deliver data with the same
 * grouping as it is written with.  Written by Robert S. Miles, BRL.
 */
static int mread(int fd, register char *bufp, unsigned n)
{
	register unsigned count = 0;
	register int nread;

	do {
		nread = recv(fd, bufp, n - count, 0);
		numCalls++;
		if (nread < 0) {
			wmprintf("ttcp_mread: %d\r\n",
				       net_get_sock_error(fd));
			return -WM_E_TTCP_MREAD_RECV;
		}
		if (nread == 0)
			return (int)count;
		count += (unsigned)nread;
		bufp += nread;
	} while (count < n);

	return (int)count;
}

static struct cli_command cli[] = {
	{"ttcp", "(see ttcp -h for details)", main},
};

int ttcp_init(void)
{
	if (cli_register_commands
	    (&cli[0], sizeof(cli) / sizeof(struct cli_command)))
		return -WM_E_TTCP_REGISTER_CMDS;
	return WM_SUCCESS;
}
