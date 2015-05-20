/*
 * Copyright 2008-2013, Marvell International Ltd.
 * All Rights Reserved.
 */

/** sysinfo.c: Dump various statistics
 */
#include <string.h>
#include <stdlib.h>

#include <cli.h>
#include <cli_utils.h>
#include <wmsysinfo.h>
#include <wmstdio.h>
#include <wm_os.h>
#include <wm_net.h>
#include <wm_utils.h>
#include <wmtime.h>
#include <wmstats.h>

static void sysinfo_help(void)
{
	wmprintf
	    ("sysinfo usage: sysinfo [options] <service> [command]\r\n\r\n");
	wmprintf
	    ("sysinfo is a diagnostic utility that reports info about\r\n");
	wmprintf
	    ("operating system services, socket descriptor usage etc. \r\n");
	wmprintf("For example, to list info about threads,\r\n");
	wmprintf("say:\r\n");
	wmprintf("\tsysinfo thread\r\n");
	wmprintf("For example, to list info about sockets,\r\n");
	wmprintf("say:\r\n");
	wmprintf("\tsysinfo socket\r\n");
	wmprintf("\r\n");
	wmprintf("For example, to list info about network stack,\r\n");
	wmprintf("say:\r\n");
	wmprintf("\tsysinfo netstat <link/ip/tcp/udp>\r\n");
	wmprintf("\tNote: No argument to netstat displays "
		"all available info about the network stack. \r\n");
	wmprintf("\r\n");
	wmprintf("The following services are supported:\r\n");
	wmprintf("\r\n");
	wmprintf("thread: Dump info about operating system "
		       "threads. In addition to\r\n");
	wmprintf("        dumping info, the 'stackmark' command "
		       "writes a\r\n");
	wmprintf("        pattern to the unused portion of "
		       "stacks' threads.\r\n");
	wmprintf("        After marking the stacks, dumping "
		       "the info will\r\n");
	wmprintf("        will report the highwater mark of "
		       "the stack.\r\n");
	wmprintf("        This is useful for detecting stack "
		       "overruns.\r\n");
	wmprintf("\r\n");
	wmprintf("socket: Dump statistics of socket descriptor "
		       "usage including\r\n");
	wmprintf("	available sockets, used sockets, tcp "
		       "sockets etc\r\n");
	wmprintf("\r\n");
	wmprintf("stats:  Dump statistics related to the WMSDK.\r\n");
	wmprintf("\r\n");
	wmprintf("heap:   Dump the heap metadata in this "
		       "following format\n\r");
	wmprintf("\tHST:<next-free-block>:<prev-block>:<block-size>");
	wmprintf(":<block-type>\n\r");
	wmprintf("\t  HST            : A tag to seperate other "
		       "logs from this log\n\r");
	wmprintf("\t  next-free-block: The address of the block "
		       "which is free.\r\n");
	wmprintf("\t                   This list is sorted by "
		       "block size- lowest size block first\n\r");
	wmprintf("\t  prev-block     : Address of the previous "
		       "block in the serial list\n\r");
	wmprintf("\t  block-size     : Size of the current "
		       "block\n\r");
	wmprintf("\t  block-type     : The type of the block:"
		       "\n\r");
	wmprintf("                     X: First block. This is "
		       "not a part of the heap and points to\n\r");
	wmprintf("                        first real block in "
		       "this heap\n\r");
	wmprintf("                     F: Free block\n\r");
	wmprintf("                     A: Allocated block\n\r\r\n");
	wmprintf("Options: \r\n");
	wmprintf("-h           Print this message\r\n");
	wmprintf("-n <name>    Only operate on the instance of "
		       "the service\r\n");
	wmprintf("             with this name.  E.g., 'sysinfo "
		       "-n foo thread'\r\n");
	wmprintf("             will only dump info for the "
		       "foo thread.\r\n");
}

static void statinfo_dump()
{
	wmprintf("=== WM Statistics:\r\n");
	wmprintf("====== Basic information:\r\n");
	wmprintf("Epoch		     \t%lu\r\n", sys_get_epoch());
	wmprintf("Time since bootup    \t%lu s\r\n",
		       wmtime_time_get_posix());
	wmprintf("====== Connectivity Stats:\r\n");
	wmprintf("Link Loss Events     \t%lu\r\n", g_wm_stats.wm_lloss);
	wmprintf("Connection Attempts  \t%lu\r\n",
		       g_wm_stats.wm_conn_att);
	wmprintf("Connection Success  \t%lu\r\n",
		       g_wm_stats.wm_conn_succ);
	wmprintf("Connection Failures  \t%lu\r\n",
		       g_wm_stats.wm_conn_fail);
	wmprintf("  - Net Not Found    \t%lu\r\n",
		       g_wm_stats.wm_nwnt_found);
	wmprintf("  - Auth Failure     \t%lu\r\n",
		       g_wm_stats.wm_auth_fail);
	wmprintf("DHCP Success         \t%lu\r\n",
		       g_wm_stats.wm_dhcp_succ);
	wmprintf("DHCP Fail            \t%lu\r\n",
		       g_wm_stats.wm_dhcp_fail);
	wmprintf("DHCP Lease Success   \t%lu\r\n",
		       g_wm_stats.wm_leas_succ);
	wmprintf("DHCP Lease Fail      \t%lu\r\n",
		       g_wm_stats.wm_leas_fail);
	wmprintf("Address Type         \t%s\r\n",
		       g_wm_stats.wm_addr_type ? "DHCP" : "Static");
	wmprintf("\r\n====== HTTP Client Stats:\r\n");
	wmprintf("DNS Fail             \t%lu\r\n",
		       g_wm_stats.wm_ht_dns_fail);
	wmprintf("Socket Fail          \t%lu\r\n",
		       g_wm_stats.wm_ht_sock_fail);
	wmprintf("No Route To Host     \t%lu\r\n",
		       g_wm_stats.wm_ht_conn_no_route);
	wmprintf("Connection Timeout   \t%lu\r\n",
		       g_wm_stats.wm_ht_conn_timeout);
	wmprintf("Connection Reset   \t%lu\r\n",
		       g_wm_stats.wm_ht_conn_reset);
	wmprintf("Other Conn. Error    \t%lu\r\n",
		       g_wm_stats.wm_ht_conn_other);
	wmprintf("\r\n====== Cloud Stats:\r\n");
	wmprintf("Success Posts        \t%lu\r\n",
		       g_wm_stats.wm_cl_post_succ);
	wmprintf("Failed Posts         \t%lu\r\n",
		       g_wm_stats.wm_cl_post_fail);
	wmprintf("\r\n====== HTTP Stats:\r\n");
	wmprintf("WSGI Calls           \t%lu\r\n",
		       g_wm_stats.wm_hd_wsgi_call);
	wmprintf("   - FTFS File       \t%lu\r\n", g_wm_stats.wm_hd_file);
	wmprintf("User agent	     \t%s-%s\r\n",
		       g_wm_stats.wm_hd_useragent.product,
		       g_wm_stats.wm_hd_useragent.version);
	wmprintf("   - Time of access  \t%lu s\r\n",
		       g_wm_stats.wm_hd_time);
}

static void sysinfo_cli(int argc, char **argv)
{
	int c;
	char *name = NULL, *service = NULL, *command = NULL;

	cli_optind = 1;
	while ((c = cli_getopt(argc, argv, "hn:")) != -1) {
		switch (c) {
		case 'h':
			sysinfo_help();
			return;

		case 'n':
			name = cli_optarg;
			break;

		default:
			wmprintf("ERROR: unexpected option: %c\r\n", c);
			return;
		}
	}

	/* parse the service name */
	if (cli_optind >= argc) {
		wmprintf
		    ("ERROR: please specify a service"
		     " [thread|socket|netstat|heap|stats]\n\r");
		return;
	}
	service = argv[cli_optind++];

	/* parse the command name (if any) */
	if (cli_optind < argc) {
		command = argv[cli_optind++];
	}

	if (strcmp(service, "thread") == 0) {
		if (!command) {
			os_dump_threadinfo(name);
		} else if (strcmp(command, "stackmark") == 0) {
			os_thread_stackmark(name);
		} else {
			wmprintf
			    ("ERROR: unsupported thread command \"%s\"\r\n",
			     command);
		}
	} else if (strcmp(service, "socket") == 0) {
		net_sockinfo_dump();
	} else if (strcmp(service, "netstat") == 0) {
		net_stat(command);
	} else if (strcmp(service, "stats") == 0) {
		statinfo_dump();
	} else if (strcmp(service, "heap") == 0) {
		os_dump_mem_stats();
	} else {
		wmprintf("ERROR: unsupported service \"%s\"\r\n",
			       service);
		return;
	}
}

#ifdef CONFIG_sysinfo_TESTS
/* To test the stack monitoring features, we implement a thread whose only duty
 * is to calculate a fibonacci number recursively and return it to a response
 * queue.  This allows us to explore various scenarios such as stack profiling
 * and stack overflow.  See the README for details.
 */
static os_queue_pool_define(fib_input, 4);
static os_queue_t fib_input_queue;
static os_queue_pool_define(fib_output, 4);
static os_queue_t fib_output_queue;

/* We pad the fibonacci stack so that we can tolerate some overrun without
 * crashing.  This is for experimental purposes.
 */
#define FIBONACCI_STACK_PADDING 128
#define FIBONACCI_STACK_SIZE 256
static os_thread_stack_define(fib_stack,
		FIBONACCI_STACK_PADDING + FIBONACCI_STACK_SIZE);
static os_thread_t fib_thread;

unsigned int do_fibonacci(unsigned int num)
{
	if (num == 0)
		return 1;

	if (num == 1)
		return 1;

	return do_fibonacci(num - 2) + do_fibonacci(num - 1);
}

static void fibonacci(os_thread_t unused)
{
	unsigned int ret, msg;

	while (1) {

		ret = os_queue_recv(&fib_input_queue, &msg, OS_WAIT_FOREVER);
		if (ret != WM_SUCCESS) {
			wmprintf
			    ("WARNING: Failed to get message"
			     " from queue: %d\r\n",
			     ret);
			continue;
		}
		ret = do_fibonacci(msg);
		ret = os_queue_send(&fib_output_queue, &ret, OS_NO_WAIT);
		if (ret != WM_SUCCESS) {
			wmprintf
				("WARNING: Failed to queue fibonacci"
				 " result: %d\r\n",
			     ret);
		}
	}
}

int fibonacci_init(void)
{
	unsigned int ret;

	ret = os_queue_create(&fib_input_queue, "fibonacci input",
			      sizeof(void *), &fib_input);
	if (ret) {
		wmprintf
		    ("ERROR: Failed to create fib input queue: %d\r\n", ret);
		goto fail;
	}

	ret = os_queue_create(&fib_output_queue, "fibonacci output",
			      sizeof(void *), &fib_output);
	if (ret) {
		wmprintf
		    ("ERROR: Failed to create fib output queue: %d\r\n", ret);
		goto fail;
	}
	ret = os_thread_create(&fib_thread, "fibonacci",
			       fibonacci,
			       0,
			       &fib_stack,
			       OS_PRIO_3
			       );
	if (ret) {
		wmprintf
		    ("ERROR: Failed to launch fibonacci thread: %d\r\n", ret);
		goto fail;
	}
	return WM_SUCCESS;
 fail:
	if (fib_thread)
		os_thread_delete(&fib_thread);
	if (fib_output_queue)
		os_queue_delete(&fib_output_queue);
	if (fib_input_queue)
		os_queue_delete(&fib_input_queue);

	return -WM_FAIL;
}

static void fibonacci_send(int argc, char **argv)
{
	unsigned int msg, ret;

	if (argc != 2)
		wmprintf("Usage: fib <n>\r\n");

	/* Purge any stale results from the output queue */
	while (1) {
		ret = os_queue_recv(&fib_input_queue, &msg, OS_NO_WAIT);
		if (ret == -WM_FAIL)
			break;

		if (ret != WM_SUCCESS) {
			wmprintf
			    ("ERROR: Failed to purge output queue: %d\r\n",
			     ret);
			return;
		}
	}

	/* send N to the fibonacci thread */
	msg = atoi(argv[1]);
	ret = os_queue_send(&fib_input_queue, &msg, OS_NO_WAIT);
	if (ret != WM_SUCCESS) {
		wmprintf
		    ("ERROR: Failed to queue fibonacci result: %d\r\n", ret);
		return;
	}

	/* Now wait for the result.  If we don't get it after 3 seconds */
	ret =
	    os_queue_recv(&fib_output_queue, &msg,
			     3 * 100/* 10ms ticks */);
	if (ret == -WM_FAIL) {
		wmprintf("ERROR: Timed out waiting for response.\r\n");
		return;
	}

	if (ret != WM_SUCCESS) {
		wmprintf
		    ("ERROR: Failed to get response from fib thread: %d\r\n",
		     ret);
		return;
	}
	wmprintf("%d", msg);
}
#endif

static int cmd_get_data_size(char *arg, int default_size)
{
	/* Check for a size specification .b, .h or .w */
	int len = strlen(arg);
	if (len > 2 && arg[len-2] == '.') {
		switch (arg[len-1]) {
		case 'b':
			return 1;
		case 'h':
			return 2;
		case 'w':
			return 4;
		default:
			return -1;
		}
	}
	return default_size;
}

void do_mem_write(int argc, char **argv)
{
	uint32_t addr, writeval, count;
	int size, sta;

	if ((argc < 3) || (argc > 4)) {
		wmprintf("Usage: memwrite.[b|h|w] <address> <value>"
				" [count]\r\n");
		return;
	}

	size = cmd_get_data_size(argv[0], 4);
	if (size < 0)
		return;

	addr = strtoul(argv[1], NULL, 16);
	writeval = strtoul(argv[2], NULL, 16);

	if (argc == 4)
		count = strtoul(argv[3], NULL, 16);
	else
		count = 1;

	sta = os_enter_critical_section();
	while (count-- > 0) {
		if (size == 4)
			*((volatile uint32_t *)addr) = (uint32_t) writeval;
		else if (size == 2)
			*((volatile uint16_t *)addr) = (uint16_t) writeval;
		else
			*((volatile uint8_t *)addr) = (uint8_t) writeval;
		addr += size;
	}
	os_exit_critical_section(sta);

	return;
}

void do_mem_dump(int argc, char **argv)
{
	int i, sta;
	void *buf;
	uint32_t addr, size, x;
	unsigned char d_size;

	if (argc != 3) {
		wmprintf("Usage: memdump.[b|h|w] <address>"
				" <# of objects> \r\n");
		return;
	}

	d_size = cmd_get_data_size(argv[0], 4);
	if (d_size  < 0)
		return;

	addr = strtoul(argv[1], NULL, 16);
	size = strtoul(argv[2], NULL, 16);

	buf = (void *)addr;

	sta = os_enter_critical_section();
	for (i = 0; i < size ; i++) {
		if ((i % (16 / d_size)) == 0)
			wmprintf("\r\n%p: ", buf);
		if (d_size == 4)
			x = *(volatile uint32_t *)buf;
		else if (d_size == 2)
			x = *(volatile uint16_t *)buf;
		else
			x = *(volatile uint8_t *)buf;
		wmprintf("%0*x ", d_size * 2, x);
		buf += d_size;
	}
	os_exit_critical_section(sta);
}

static struct cli_command cli[] = {
	{"sysinfo", "(see sysinfo -h for details)", sysinfo_cli},
	{"memdump", "(memdump.[b|h|w] <address> <# of objects>)", do_mem_dump},
	{"memwrite", "(memwrite.[b|h|w] <address> <value> [count])",
		do_mem_write},
#ifdef CONFIG_sysinfo_TESTS
	{"fib", "<n>: Calculate the nth fibonacci number", fibonacci_send},
#endif
};

int sysinfo_init(void)
{
	if (cli_register_commands
	    (&cli[0], sizeof(cli) / sizeof(struct cli_command)))
		return 1;

#ifdef CONFIG_sysinfo_TESTS
	fibonacci_init();
#endif

	/* Mark the stacks of any existing threads */
	os_thread_stackmark(NULL);

	return 0;
}
