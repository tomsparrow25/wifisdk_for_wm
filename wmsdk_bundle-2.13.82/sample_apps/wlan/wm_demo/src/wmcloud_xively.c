
#include <wmcloud.h>
#include <wmcloud_xively.h>
#include <board.h>
#include <httpc.h>
#include <httpd.h>
#include <wmcrypto.h>
#include <json.h>
#include <psm.h>

#include "libxively/xively.h"
#include "libxively/xi_err.h"

#define DEFAULT_PROBE_INTERVAL  (5)	/* in secs */

static struct xively_cloud xi;
extern cloud_t c;
static os_semaphore_t sem;

static char buffer[256];
static uint8_t is_cloud_started;

int cloud_wakeup_for_send()
{
	if (c.state != CLOUD_ACTIVE)
		return -WM_FAIL;

	os_semaphore_put(&sem);
	return WM_SUCCESS;
}

static int cloud_set_feed_id(int feed_id)
{
	snprintf(buffer, sizeof(buffer),
		 "%d", feed_id);
	return psm_set_single(CLOUD_MOD_NAME,
			      VAR_CLOUD_FEED_ID, buffer);
}

static int cloud_set_api_key(char *api_key)
{
	return psm_set_single(CLOUD_MOD_NAME, VAR_CLOUD_API_KEY,
				api_key);
}


static int request_http_get(const char *url_str, http_session_t * handle,
			    http_resp_t **http_resp)
{
	int status;

	cl_dbg("Connecting to %s", url_str);

#if XIVELY_TLS
	/* Initialize the TLS configuration structure */
	tls_init_config_t cfg = {
		.flags = 0,
		.tls.client.client_cert = NULL,
		.tls.client.client_cert_size = 0,
		.tls.client.ca_cert = NULL,
		.tls.client.ca_cert_size = 0,
	};

	/*
	 * Open an HTTP Session with the specified URL.
	 */
	status = http_open_session(handle, url_str, 0, &cfg, 0);
#else
	status = http_open_session(handle, url_str, 0, NULL, 0);
#endif
	if (status != WM_SUCCESS) {
		cl_dbg("Failed to open session: URL: %s", url_str);
		goto http_session_open_fail;
	}

	/* Setup the parameters for the HTTP Request. This only prepares the
	 * request to be sent out.  */
	http_req_t req;
	req.type = HTTP_GET;
	req.resource = url_str;
	req.version = HTTP_VER_1_1;
	req.content = NULL;

	status = http_prepare_req(*handle, &req, STANDARD_HDR_FLAGS);
	if (status != WM_SUCCESS) {
		cl_dbg("Failed to prepare request: URL: %s", url_str);
		goto http_session_error;
	}

	/* Send the HTTP GET request, that was prepared earlier, to the web
	 * server */
	status = http_send_request(*handle, &req);
	if (status != WM_SUCCESS) {
		cl_dbg("Failed to send request: URL: %s", url_str);
		goto http_session_error;
	}

	/* Read the response and make sure that an HTTP_OK response is
	 * received */
	http_resp_t *resp;
	status = http_get_response_hdr(*handle, &resp);
	if (status != WM_SUCCESS) {
		cl_dbg("Unable to get response header: %d", status);
		goto http_session_error;
	}

	if (resp->status_code != HTTP_OK) {
		cl_dbg("Unexpected status code received from server: %d",
			  resp->status_code);
		status = -WM_FAIL;
		goto http_session_error;
	}

	if (http_resp)
		*http_resp = resp;

	return WM_SUCCESS;
http_session_error:
	http_close_session(handle);
http_session_open_fail:
	return status;
}

/* +1 for trailing '\0' */
#define HMAC_SHA1_LEN   (20 + 1)

int get_xively_activation_code(char *activation_code,
				int len)
{
	char bin_hmac[20];

	if (len < HMAC_SHA1_LEN) {
		cl_dbg("insufficient activation code buffer");
		return -WM_FAIL;
	}

	cl_dbg("Serial No.: %s", xi.serial_no);
	memset(activation_code, 0, len);

	hmac_sha1((uint8_t *) xi.product_secret,
		xi.product_secret_len,
		(uint8_t *) xi.serial_no, strlen(xi.serial_no),
		(uint8_t *) bin_hmac, sizeof(bin_hmac));
	bin2hex((uint8_t *)bin_hmac, activation_code, sizeof(bin_hmac), len);
	cl_dbg("hmac_sha1: %s", activation_code);
	return WM_SUCCESS;
}

int do_xively_setup(void)
{
	char activation_code[100];
	char url[100];
	http_resp_t *resp;

	cl_dbg("Performing Xively Provisioning");

	if (get_xively_activation_code(activation_code,
				       sizeof(activation_code)) != WM_SUCCESS) {
		return -WM_FAIL;
	}

	snprintf(url, sizeof(url),
#ifdef XIVELY_TLS
		 "https"
#else
		 "http"
#endif
		 "://api.xively.com/v2/devices/%s/activate", activation_code);

	if (request_http_get(url, &c.hS, &resp) != WM_SUCCESS) {
		cl_dbg("Activation HTTP request failed");
		return -WM_FAIL;
	}

	int dsize = 0;
	while (1) {

		/* Keep reading data over the HTTPS session and print it out on
		 * the console. */
		dsize = http_read_content(c.hS, buffer + dsize,
					  sizeof(buffer) - dsize);
		if (dsize < 0) {
			cl_dbg("Unable to read data on http session: %d",
				   dsize);
			return -WM_FAIL;
		}
		if (dsize == 0)
			break;
		cl_dbg("Content length: %d\r\n", dsize);
		cl_dbg("Content: %s\r\n", buffer);
	}

	struct json_object jobj;

	json_object_init(&jobj, buffer);
	if (json_get_val_str(&jobj, "apikey", xi.api_key,
			     sizeof(xi.api_key)) != WM_SUCCESS) {
		return -WM_FAIL;
	}
	cloud_set_api_key(xi.api_key);
	cl_dbg("API Key: %s\r\n", xi.api_key);

	if (json_get_val_int(&jobj, "feed_id", &xi.feed_id)
	    != WM_SUCCESS) {
		return -WM_FAIL;
	}
	cloud_set_feed_id(xi.feed_id);
	cl_dbg("Feed ID: %d\r\n", xi.feed_id);

	if (c.hS)
		http_close_session(&c.hS);

	return WM_SUCCESS;
}

static void cloud_sleep(cloud_t *c)
{
	int interval;

	interval = os_msec_to_ticks(xi.probe_interval_secs * 1000);
	os_semaphore_get(&sem, interval);
}

static int cloud_get_probe_interval(unsigned *psm_var_interval)
{
	char probe_interval[10];

	int status = psm_get_single(CLOUD_MOD_NAME,
				    VAR_CLOUD_INTERVAL, probe_interval,
				    sizeof(probe_interval));
	if (status != WM_SUCCESS)
		return status;

	if (strlen(probe_interval) == 0) {
		cl_dbg("Cloud variable \"probe interval\" empty");
		return -WM_FAIL;
	}

	*psm_var_interval = strtol(probe_interval, NULL, 10);
	if (*psm_var_interval < 0) {
		cl_dbg("Cloud variable \"probe interval\"value "
			    "invalid: %s", probe_interval);
		return -WM_FAIL;
	}

	return WM_SUCCESS;
}

static int cloud_get_feed_id(int *feed_id)
{
	*feed_id = 0;

	char feed_id_str[CLOUD_FEED_ID_LEN];
	int status = psm_get_single(CLOUD_MOD_NAME,
				    VAR_CLOUD_FEED_ID, feed_id_str,
				    sizeof(feed_id_str));
	if (status != WM_SUCCESS) {
		return status;
	}

	if (strlen(feed_id_str) == 0) {
		cl_dbg("Cloud variable in psm \"feed_id\" empty");
		return -WM_FAIL;
	}
	*feed_id = atoi(feed_id_str);
	return WM_SUCCESS;
}

static int cloud_get_api_key(char *api_key, int max_len)
{
	int status = psm_get_single(CLOUD_MOD_NAME, VAR_CLOUD_API_KEY,
				api_key, max_len);
	if (status != WM_SUCCESS) {
		return status;
	}

	if (strlen(api_key) == 0) {
		cl_dbg("Cloud variable in psm \"api_key\" empty");
		return -WM_FAIL;
	}
	return WM_SUCCESS;
}

static int xively_params_load()
{
	int status;

	status = cloud_get_probe_interval(&xi.probe_interval_secs);
	if (status != WM_SUCCESS) {
		/* We will be using the default interval */
		xi.probe_interval_secs = DEFAULT_PROBE_INTERVAL;
	}
	cl_dbg("Probe interval: %d", xi.probe_interval_secs);

	status = cloud_get_feed_id(&xi.feed_id);
	if (status != WM_SUCCESS)
		cl_dbg("No feed id\r\n");
	else
		cl_dbg("Feed ID: %d", xi.feed_id);

	status = cloud_get_api_key(xi.api_key,
			CLOUD_API_KEY_LEN - 1);
	if (status != WM_SUCCESS)
		cl_dbg("No API key\r\n");
	else
		cl_dbg("API KEY: %s", xi.api_key);

	return WM_SUCCESS;
}

int cloud_get_ui_link(httpd_request_t *req)
{

	char content[150];
	if (c.state != CLOUD_ACTIVE || (strlen(xi.api_key) == 0) || !xi.feed_id)
		return httpd_send_response(req, HTTP_RES_200, CLOUD_INACTIVE,
			strlen(CLOUD_INACTIVE), HTTP_CONTENT_PLAIN_TEXT_STR);

	snprintf(content, sizeof(content), "Moved to http://wmsdk.github.io"
		"/xively-js/%s/index.html#feed_id=%d&api_key=%s",
		c.dev_class, xi.feed_id, xi.api_key);
	return httpd_send_response_301(req, content + 9,
		HTTP_CONTENT_PLAIN_TEXT_STR, content,
		strlen(content));
}
static void cloud_loop(xi_context_t *xi_cont)
{
	int ret;
	/* Connection is not established yet, so initialize the state machine
	 * with connection error */
	cloud_sm(EVT_CONN_ERROR);

	if ((strlen(xi.api_key) == 0) || !xi.feed_id) {
		if (do_xively_setup() != WM_SUCCESS) {
			cloud_sm(EVT_CONN_ERROR);
			return;
		}
	}

	cloud_sm(EVT_OP_SUCCESS);

	if (xi.cloud_handler) {
		ret = xi.cloud_handler(xi_cont);
		if (ret == WM_SUCCESS)
			g_wm_stats.wm_cl_post_succ++;
		else
			g_wm_stats.wm_cl_post_fail++;
	}
}

void cloud_thread_main(os_thread_arg_t arg)
{
	xi_context_t *xi_cont;

	xively_params_load();
	xi_cont = xi_create_context(XI_HTTP, xi.api_key,
				xi.feed_id);
	while (!c.stop_request) {
		cloud_loop(xi_cont);
		cloud_sleep(&c);
	}
	xi_delete_context(xi_cont);
	c.stop_request = false;
	os_thread_self_complete(NULL);
}

#define STACK_SIZE (1024 * 24)
int xively_cloud_start(const char *dev_class, char *serial_no,
		char *product_secret, unsigned int product_secret_len,
		int (*cloud_handler)(xi_context_t *xi))
{
	int ret;

	if (is_cloud_started)
		return WM_SUCCESS;

	memset(&xi, 0, sizeof(xi));
	if (serial_no)
		strncpy(xi.serial_no, serial_no, XIVELY_SERIAL_NO_MAX);

	if (product_secret) {
		xi.product_secret = product_secret;
		xi.product_secret_len = product_secret_len;
	}
	xi.cloud_handler = cloud_handler;

	ret = os_semaphore_create(&sem, "cloud_sem");
	if (ret != WM_SUCCESS) {
		cl_dbg("Cloud semaphore creation error %d", ret);
		return -WM_FAIL;
	}
	os_semaphore_get(&sem, OS_WAIT_FOREVER);

	ret = cloud_actual_start(dev_class, NULL, NULL,
			STACK_SIZE);
	if (ret == WM_SUCCESS)
		is_cloud_started = 1;
	else
		os_semaphore_delete(&sem);

	return ret;
}

int xively_cloud_stop()
{
	int ret;

	if (!is_cloud_started)
		return WM_SUCCESS;

	os_semaphore_put(&sem);

	ret = cloud_actual_stop();
	os_semaphore_delete(&sem);
	is_cloud_started = 0;

	return ret;
}
