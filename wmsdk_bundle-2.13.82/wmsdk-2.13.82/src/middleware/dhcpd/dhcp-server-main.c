/*
 *  Copyright (C) 2008-2012, Marvell International Ltd.
 *  All Rights Reserved.
 */

/** dhcp-server-main.c: CLI based APIs for the DHCP Server
 */
#include <string.h>

#include <wmstdio.h>
#include <wm_os.h>
#include <wm_net.h>
#include <cli.h>
#include <cli_utils.h>
#include <dhcp-server.h>

#include "dhcp-priv.h"

os_thread_t main_thread;
static os_thread_stack_define(dhcp_stack, 1024);
static int running = 0;
/*
 * API
 */

int dhcp_server_start(void *intrfc_handle)
{
	int ret;

	if (running || dhcp_server_init(intrfc_handle))
		return -WM_E_DHCPD_SERVER_RUNNING;

	ret = os_thread_create(&main_thread, "dhcp-server", dhcp_server, 0,
			       &dhcp_stack, OS_PRIO_3);
	if (ret) {
		dhcp_clean_sockets();
		return -WM_E_DHCPD_THREAD_CREATE;
	}

	running = 1;
	return WM_SUCCESS;
}

void dhcp_server_stop(void)
{
	if (running) {
		if (dhcp_send_halt() != WM_SUCCESS) {
			dhcp_w("failed to send halt to DHCP thread");
			return;
		}
		if (os_thread_delete(&main_thread) != WM_SUCCESS)
			dhcp_w("failed to delete thread");
		running = 0;
	} else {
		dhcp_w("server not running.");
	}
}

/*
 * Command-Line Interface
 */

#define DHCPS_COMMAND	"dhcp-server"
#define DHCPS_HELP	"<mlan0/uap0/wfd0> <start|stop>"
#define WAIT_FOR_UAP_START	5

static void dhcp_cli(int argc, char **argv)
{
	void *intrfc_handle = NULL;
	int wait_for_sec = WAIT_FOR_UAP_START;

	if (argc >= 3 && string_equal(argv[2], "start")) {
		if (!strncmp(argv[1], "mlan0", 5))
			intrfc_handle = net_get_mlan_handle();
		else if (!strncmp(argv[1], "uap0", 4)) {
			while(wait_for_sec) {
				wait_for_sec--;
				if (!is_uap_started()) {
					if (wait_for_sec == 0) {
						dhcp_e("unable"
						" to start DHCP server. Retry"
						" after uAP is started");
						return;
					}
				} else
					break;
				os_thread_sleep(os_msec_to_ticks(1000));
			}
			intrfc_handle = net_get_uap_handle();
		}

		if (dhcp_server_start(intrfc_handle))
			dhcp_e("unable to "
				       "start DHCP server");
	} else if (argc >= 3 && string_equal(argv[2], "stop"))
		dhcp_server_stop();
	else {
		wmprintf("Usage: %s %s\r\n", DHCPS_COMMAND, DHCPS_HELP);
		wmprintf("Error: invalid argument\r\n");
	}
}

static struct cli_command dhcp_cmds[] = {
	{DHCPS_COMMAND, DHCPS_HELP, dhcp_cli},
	{"dhcp-stat", NULL, dhcp_stat},
#ifdef CONFIG_DHCP_SERVER_TESTS
	{"dhcp-server-tests", NULL, dhcp_server_tests},
#endif
};

int dhcpd_cli_init(void)
{
	int i;

	for (i = 0; i < sizeof(dhcp_cmds) / sizeof(struct cli_command); i++)
		if (cli_register_command(&dhcp_cmds[i]))
			return -WM_E_DHCPD_REGISTER_CMDS;

	return WM_SUCCESS;
}
