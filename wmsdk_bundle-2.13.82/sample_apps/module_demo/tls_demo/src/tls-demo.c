#include <openssl/ssl.h>

#include <wmtypes.h>
#include <wmstdio.h>
#include <arch/sys.h>
#include <string.h>
#include <wmstdio.h>
#include <wm_os.h>
#include <wm_net.h>

#include <cli.h>
#include <cli_utils.h>

#include <cyassl/ctaocrypt/settings.h>

#include <appln_dbg.h>
#include <wlan.h>
#include <wm-tls.h>
#include "tls-demo.h"
#include <mdev_aes.h>

/*
 * In this test case we do not run our code in the cli thread context. This
 * is because tls library requires more stack that what the cli thread
 * has. So we have our own thread.
 */

/* Buffer to be used as stack */
static os_thread_stack_define(tls_demo_stack, 8192);
static char url[256];

static void tls_demo_main(os_thread_arg_t data)
{
	/*
	 * We will create a new thread here to process the ssl test case
	 * functionality. This is because the cli thread has a limited
	 * stack and ssl test cases need stack size of atleast
	 * TLS_DEMO_THREAD_STACK_SIZE to work properly.
	 *
	 */
	tls_httpc_client(url);

	/* Delete myself */
	os_thread_delete(NULL);
}

/* Invoked in cli thread context. */
static void cmd_tls_httpc_client(int argc, char **argv)
{
	if (argc != 2) {
		wmprintf("\nUsage: tls-httpc-client <url>\n\n\r");
		return;
	}


	strncpy(url, argv[1], sizeof(url));
	/*
	 * tls requires about 8K of stack. So, we cannot run tls API's in
	 * the cli thread contex. Create a separate thread.
	 */
	int ret = os_thread_create(NULL,	/* thread handle */
				"tls_demo",	/* thread name */
				tls_demo_main,	/* entry function */
				0,	/* argument */
				&tls_demo_stack,	/* stack */
				OS_PRIO_2);	/* priority - medium low */
	if (ret != WM_SUCCESS) {
		dbg("Failed to create tls-demo thread");
		while (1)
			;
		/* do nothing -- stall */
	}

}

static struct cli_command tls_test_cmds[] = {
	{ "tls-http-client", "<url>", cmd_tls_httpc_client},
};

int tls_demo_init(void)
{
	int ret;

	/* Register all the commands */
	if (cli_register_commands(&tls_test_cmds[0],
		 sizeof(tls_test_cmds) / sizeof(struct cli_command))) {
		dbg("Unable to register CLI commands");
		while (1)
			;
		/* do nothing -- stall */
	}

	ret = aes_drv_init();
	if (ret != WM_SUCCESS) {
		while (1)
			;
		/* do nothing -- stall */
	}

	ret = tls_lib_init();
	if (ret != WM_SUCCESS) {
		while (1)
			;
		/* do nothing -- stall */
	}

	dbg("tls_demo application Started");

	return WM_SUCCESS;
}
