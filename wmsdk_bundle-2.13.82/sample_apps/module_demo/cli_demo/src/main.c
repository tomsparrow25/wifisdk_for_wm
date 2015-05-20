/*
 *  Copyright (C) 2008-2013, Marvell International Ltd.
 *  All Rights Reserved.
 */

/*
 * Application to demonstrate the use of CLI commands
 *
 * Summary:
 *
 * This application registers a CLI command 'echo' that echoes the input
 * parameter.
 *
 * Description:
 *
 * This application registers a command with the CLI module. The command is
 * named 'echo' command. It simply echoes whatever arguments are passed to it as
 * its input.
 *
 * Use following command from console:
 * echo <text>
 */

#include <wmstdio.h>
#include <wm_os.h>

#include <board.h>

#include <cli.h>


static void cmd_echo_text(int argc, char *argv[])
{
	if (argc != 2) {
		wmprintf("Usage: echo <text>\r\n");
		return;
	}

	wmprintf("Echo response : %s\r\n", argv[1]);
	return;
}

static struct cli_command echo_commands[] = {
	/*  {command_name, Command info, handler function} */
	{"echo", "<text>", cmd_echo_text},
};

/* This is an entry point for the application.
   All application specific initialization is performed here. */
int main(void)
{
	/* Initializes console on uart0 */
	int ret = wmstdio_init(UART0_ID, 0);
	if (ret == -WM_FAIL) {
		wmprintf("Failed to initialize console on uart0\r\n");
		return -1;
	}
	/* Initialize the command line interface module */
	ret = cli_init();
	if (ret == -WM_FAIL) {
		wmprintf("Failed to initialize cli\r\n");
		return -1;
	}

	wmprintf("CLI Demo application started\r\n");

	/* Register the cli commands for use */
	ret = cli_register_commands(echo_commands, 1);
	if (ret != 0) {
		wmprintf("Failed to register cli commands\r\n");
		return -1;
	}

	return 0;
}
