/*
 * Copyright 2008-2013, Marvell International Ltd.
 * All Rights Reserved.
 */
/** httpd_test.c: CLI based tests for the HTTPD server
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <httpd.h>
#include <http_parse.h>
#include <http-strings.h>
#include <cli.h>
#include <cli_utils.h>
#include <ftfs.h>
#include <rfget.h>
#include <fs.h>

#include "httpd_priv.h"
#define FTFS_PART_NAME "ftfs"
#define FTFS_API_VERSION 114

static struct fs *filesystem;

#ifdef CONFIG_HTTPD_TESTS

static int handler_memdump(httpd_request_t *req, int sock, char *scratch)
{
	char some_mem[64];
	int err, num_chunks, chunk_size;

	/* Convenience string that sends a 200 OK status and sets the
	 * Transfer-Encoding to chunked.                                      */
	if (httpd_send(sock, http_header_200,
		       sizeof(http_header_200) - 1) != WM_SUCCESS)
		return -WM_FAIL;

	/* Set content type to binary */
	if (httpd_send(sock, http_content_type_binary,
		       sizeof(http_content_type_binary) - 1) != WM_SUCCESS)
		return -WM_FAIL;

	if (req->type == HTTPD_REQ_TYPE_GET) {
		/* Send data in 16-byte chunks */
		if (httpd_send_chunk(sock, some_mem, 16) != WM_SUCCESS ||
		    httpd_send_chunk(sock, some_mem + 16, 16) != WM_SUCCESS
		    || httpd_send_chunk(sock, some_mem + 32,
					16) != WM_SUCCESS ||
		    httpd_send_chunk(sock, some_mem + 48,
				     16) != WM_SUCCESS) {
			return -WM_FAIL;
		}
	} else {
		err = httpd_parse_hdr_tags(req, sock, scratch,
					   HTTPD_MAX_MESSAGE);
		if (err != WM_SUCCESS)
			return err;

		if (req->body_nbytes > HTTPD_MAX_MESSAGE) {
			wmprintf("POST data too long. Ignoring");
			return -WM_E_HTTPD_TOOLONG;
		}

		unsigned req_line_len;
		unsigned nbytes;
		req_line_len = httpd_recv(req->sock, scratch,
					  req->body_nbytes, 0);
		if (req_line_len == -1) {
			return -WM_FAIL;
		}
		nbytes = req_line_len;
		scratch[nbytes] = 0;    /* null terminate it */

		/* Send "num" chunks each of size "size" */
		char buf[10];
		httpd_get_tag_from_post_data(scratch, "size", buf, sizeof(buf));
		chunk_size = atoi(buf);
		httpd_get_tag_from_post_data(scratch, "num", buf, sizeof(buf));
		num_chunks = atoi(buf);
		if (chunk_size != 0 &&
		    num_chunks != 0 &&
		    chunk_size <= HTTPD_MAX_MESSAGE && chunk_size > 0) {

			memset(scratch, 'x', chunk_size);
			while (num_chunks--) {
				if (httpd_send_chunk(sock, scratch, chunk_size)
				    != WM_SUCCESS ||
				    httpd_send_chunk(sock, "\r\n",
						     2) != WM_SUCCESS) {
					return -WM_FAIL;
				}
			}
		}
	}

	/* Send last chunk to terminate body */
	if (httpd_send_chunk(sock, NULL, 0) != WM_SUCCESS)
		return -WM_FAIL;
	/* No other handler should be invoked after this one. */
	return HTTPD_DONE;
}

HTTPD_WSGI_CALL(memdump_wsgi_record, HTTPD_REQ_TYPE_GET | HTTPD_REQ_TYPE_POST,
		"/memdump", handler_memdump);

int test_file_handler(httpd_request_t *req, int sock, char *scratch)
{
	return httpd_handle_file(req, filesystem, "/", sock,
				 scratch, HTTPD_MAX_MESSAGE - 1);
}
HTTPD_WSGI_CALL(test_file_handler_wsgi, HTTPD_REQ_TYPE_GET |
		HTTPD_REQ_TYPE_POST, "/", test_file_handler);

static int handler_postecho(httpd_request_t *req, int sock, char *scratch)
{
	int err, len, i;

	err = httpd_parse_hdr_tags(req, sock, scratch, HTTPD_MAX_MESSAGE);
	if (err != WM_SUCCESS)
		return err;

	unsigned req_line_len;
	unsigned nbytes;
	req_line_len = httpd_recv(req->sock, scratch,
				  req->body_nbytes, 0);
	if (req_line_len == -1) {
		return -WM_FAIL;
	}
	nbytes = req_line_len;
	scratch[nbytes] = 0;    /* null terminate it */

#define TAG_VAL_SIZE 3
	/* Send "num" chunks each of size "size" */
	char *tag_val[TAG_VAL_SIZE][2];
	char foo[10], this[10];

	memset(tag_val, 0, sizeof(tag_val));
	httpd_get_tag_from_post_data(scratch, "foo", foo, sizeof(foo));
	tag_val[0][0] = "foo";
	tag_val[0][1] = foo;
	httpd_get_tag_from_post_data(scratch, "this", this, sizeof(this));
	tag_val[1][0] = "this";
	tag_val[1][1] = this;

	err = httpd_send(sock, http_header_200,
			 sizeof(http_header_200) - 1);
	if (err != WM_SUCCESS)
		return -WM_FAIL;

	err = httpd_send(sock, http_content_type_plain,
			 sizeof(http_content_type_plain) - 1);
	if (err != WM_SUCCESS)
		return -WM_FAIL;

	/* echo tag/value pairs in their own chunks */
	for (i = 0; i < TAG_VAL_SIZE; i++) {
		if (!tag_val[i][0])
			break;
		len = snprintf(scratch, HTTPD_MAX_MESSAGE, "%s=%s\r\n",
			       tag_val[i][0], tag_val[i][1]);
		if (len < 0)
			return -WM_FAIL;
		if (len > HTTPD_MAX_MESSAGE - 1) {
			/* tag/value pair didn't fit into our buffer */
			return -WM_FAIL;
		}
		err = httpd_send_chunk(sock, scratch, len);
		if (err != WM_SUCCESS)
			return -WM_FAIL;
	}
	err = httpd_send_chunk(sock, NULL, 0);
	if (err != WM_SUCCESS)
		return -WM_FAIL;

	return HTTPD_DONE;
}

HTTPD_WSGI_CALL(postecho_wsgi_record, HTTPD_REQ_TYPE_POST, "/postecho",
		handler_postecho);

struct httpd_wsgi_call *wsgi_handlers[] = {
	&test_file_handler_wsgi,
	&memdump_wsgi_record,
	&postecho_wsgi_record,
	NULL			/* terminating entry */
};

int echo_foobar(const httpd_request_t *req, const char *args, int conn,
		char *scratch)
{
	/* Very simple.  Just spit out foobar and we're done. */
	return httpd_send_chunk(conn, "foobar\r\n", sizeof("foobar\r\n") - 1);
}

HTTPD_SSI_CALL(ssi_foobar, "foobar", echo_foobar);

int echo_args(const httpd_request_t *req, const char *args, int conn,
	      char *scratch)
{
	/* echo the space-delimited, null-terminated args list */
	if (args == NULL)
		return WM_SUCCESS;
	if ((httpd_send_chunk(conn, args, strlen(args)) != WM_SUCCESS) ||
	    httpd_send_chunk(conn, "\r\n", 2) != WM_SUCCESS)
		return -WM_FAIL;
	return WM_SUCCESS;
}

HTTPD_SSI_CALL(ssi_echo, "echo", echo_args);


struct httpd_ssi_call *ssi_handlers[] = {
	&ssi_foobar,
	&ssi_echo,
	NULL			/* terminating entry */
};

int httpd_test_setup(void)
{
	int ret;
	struct httpd_wsgi_call *h;
	struct httpd_ssi_call *ssi;
	int i = 0;

	while ((h = wsgi_handlers[i++]) != NULL) {
		ret = httpd_register_wsgi_handler(h);
		if (ret != WM_SUCCESS) {
			httpd_e
			    ("Failed to register WSGI handler: %s (err=%d)",
			     h->url_prefix, ret);
			return ret;
		}
	}

	i = 0;
	while ((ssi = ssi_handlers[i++]) != NULL) {
		ret = httpd_register_ssi(ssi);
		if (ret != WM_SUCCESS) {
			httpd_e("Failed to register SSI handler: %s (err=%d)",
			      ssi->name, ret);
			return ret;
		}
	}

	return WM_SUCCESS;
}

static void test_useragent(int argc, char **argv)
{
	httpd_useragent_t agent;

	if (argc != 2) {
		wmprintf("Error: usage: %s \"<user-agent-string>\"\r\n",
			       argv[0]);
		return;
	}

	httpd_parse_useragent(argv[1], &agent);
	wmprintf("%s %s\r\n", agent.product, agent.version);
}

#else				/* CONFIG_HTTPD_TESTS */

int httpd_test_setup(void)
{
	return WM_SUCCESS;
}

#endif

static void httpd_help(int argc, char **argv)
{
	wmprintf("httpd usage: httpd [options] <command>\r\n\r\n");
	wmprintf("<command> is required and can be one of:\r\n");
	wmprintf("init: initialize the httpd\r\n");
	wmprintf("shutdown: tear down httpd\r\n");
	wmprintf("start: launch the httpd\r\n");
	wmprintf("stop: halt the httpd\r\n");
	wmprintf("\r\n");
	wmprintf("Options: \r\n");
	wmprintf
	    ("-f <fsname>     Initialize with a filesystem. Valid values"
	     " for \r\n");
	wmprintf("                fsname are:\r\n");
	wmprintf("                ftfs: whatever fs is in eeprom.\r\n");
#ifdef CONFIG_HTTPD_TESTS
	wmprintf
		("                testfs: test files for use with automated"
		 " tests\r\n");
#endif
	wmprintf
		("-w              Register the wsgi test handlers at init"
		 " time.\r\n");
	wmprintf("\r\n");
}

struct ftfs_super sb;
struct partition_entry *passive;

static void httpd_cli(int argc, char **argv)
{

	int c;
	char *command = NULL;
	int use_wsgi = FALSE;

	cli_optind = 1;
	while ((c = cli_getopt(argc, argv, "f:w")) != -1) {
		switch (c) {
		case 'f':
			if (cli_optarg &&
			    strcmp(cli_optarg, FTFS_PART_NAME) == 0) {
				filesystem = rfget_ftfs_init(&sb,
							     FTFS_API_VERSION,
							     FTFS_PART_NAME,
							     &passive);
				if (filesystem == 0) {
					wmprintf
					    ("Error: Failed to initialize"
					     " ftfs\r\n");
					return;
				}
			} else {
				wmprintf("Error: invalid fsname: %s\r\n",
					       cli_optarg ? cli_optarg :
					       "NULL");
				return;
			}
			break;
		case 'w':
			use_wsgi = TRUE;
			break;
		default:
			wmprintf("Error: unexpected option: %c\r\n", c);
			return;
		}
	}

	if (cli_optind >= argc) {
		wmprintf("Error: No command specified.\r\n");
		return;
	}
	command = argv[cli_optind];

	if (strcmp(command, "init") == 0) {
		if (httpd_init() != WM_SUCCESS) {
			wmprintf("Error: failed to init httpd\r\n");
			return;
		}
		if (use_wsgi && httpd_test_setup() != WM_SUCCESS) {
			wmprintf
			    ("Error: failed to register wsgi handlers.\r\n");
			/* make a best effort to clean up. */
			httpd_shutdown();
			return;
		}
		wmprintf("httpd init'd\r\n");

	} else if (strcmp(command, "shutdown") == 0) {
		if (httpd_shutdown() != WM_SUCCESS) {
			wmprintf("Error: failed to shutdown httpd\r\n");
			return;
		}
		wmprintf("httpd shutdown\r\n");

	} else if (strcmp(command, "start") == 0) {
		if (httpd_start() != WM_SUCCESS) {
			wmprintf("Error: failed to start httpd\r\n");
			return;
		}
		wmprintf("httpd started\r\n");

	} else if (strcmp(command, "stop") == 0) {
		if (httpd_stop() != WM_SUCCESS) {
			wmprintf("Error: failed to stop httpd\r\n");
			return;
		}
		wmprintf("httpd stopped\r\n");

	} else {
		wmprintf("Error: unsupported command %s\r\n", command);
	}
}

static struct cli_command cli[] = {
	{"httpd", "(see httpd-help)", httpd_cli},
	{"httpd-help", NULL, httpd_help},
#ifdef CONFIG_HTTPD_TESTS
	{"httpd-useragent", NULL, test_useragent},
#endif
};

int httpd_cli_init(void)
{
	static bool inited;
	if (inited)
		return WM_SUCCESS;

	if (cli_register_commands
	    (&cli[0], sizeof(cli) / sizeof(struct cli_command)))
		return -WM_FAIL;

	inited = 1;
	return WM_SUCCESS;
}
