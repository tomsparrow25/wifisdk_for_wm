#include <cli.h>
#include <httpc.h>

#define dbg(_fmt_, ...)				\
	wmprintf(_fmt_"\n\r", ##__VA_ARGS__)

static struct cli_command httpc_test_cmds[];

static void cmd_httpc_post(int argc, char **argv)
{
	if (argc < 3) {
		dbg("\nUsage: %s\n", httpc_test_cmds[0].help);
		return;
	}

	const char *url = argv[1];
	const char *data = argv[2];
	int len = strlen(data);
	int count = 1;		/* Default value */

	/* Check if user has given count */
	if (argc > 3) {
		count = strtol(argv[3], NULL, 0);
	}

	http_session_t hnd;
	int rv = http_open_session(&hnd, url, 0, NULL, 0);
	if (rv != 0) {
		dbg("Open session failed: %s (%d)", url, rv);
		return;
	}

	while (count--) {
		http_req_t req = {
			.type = HTTP_POST,
			.resource = url,
			.version = HTTP_VER_1_1,
			.content = data,
			.content_len = len,
		};

		rv = http_prepare_req(hnd, &req,
				      STANDARD_HDR_FLAGS |
				      HDR_ADD_CONN_KEEP_ALIVE);
		if (rv != 0) {
			dbg("Prepare request failed: %d", rv);
			break;
		}

		rv = http_send_request(hnd, &req);
		if (rv != 0) {
			dbg("Send request failed: %d", rv);
			break;
		}

		http_resp_t *resp;
		rv = http_get_response_hdr(hnd, &resp);
		if (rv != 0) {
			dbg("Get resp header failed: %d", rv);
			break;
		}

		dbg("Content length: %d", resp->content_length);
		if (resp->content_length == 0) {
			continue;
		}

		dbg("------------Content------------");
		while (1) {
			char buf[32];
			rv = http_read_content(hnd, buf, sizeof(buf));
			if (rv == 0 || rv < 0) {
				break;
			}
			wmprintf("%s", buf);
		}

		if (rv < 0) {
			/* Error condition */
			break;
		}
	}
}

static struct cli_command httpc_test_cmds[] = {
	{"httpc-post", "httpc-post <url> <data string> [count]",
	 cmd_httpc_post},
};

int httpc_test_cli_init()
{
	/* Register all the commands */
	if (cli_register_commands(&httpc_test_cmds[0],
				  sizeof(httpc_test_cmds) /
				  sizeof(struct cli_command))) {
		dbg("Unable to register CLI commands");
		while (1)
			;
		/* do nothing -- stall */
	}

	return 0;
}
