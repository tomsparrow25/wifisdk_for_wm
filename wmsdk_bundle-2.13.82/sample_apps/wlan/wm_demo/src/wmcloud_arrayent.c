
#include <wmcloud.h>
#include <wmcloud_arrayent.h>
#include <board.h>
#include <httpc.h>
#include <wmcrypto.h>
#include <json.h>
#include <psm.h>


#define DEFAULT_PROBE_INTERVAL  (3)	/* in secs */


struct arrayent_cloud arr_cloud;
extern cloud_t c;
static os_semaphore_t sem;
static uint8_t is_cloud_started;

int cloud_wakeup_for_send()
{
	if (c.state != CLOUD_ACTIVE)
		return -WM_FAIL;

	os_semaphore_put(&sem);
	return WM_SUCCESS;
}

static void cloud_sleep(cloud_t *c)
{
	int interval = os_msec_to_ticks(arr_cloud.probe_interval_secs * 1000);
	os_semaphore_get(&sem, interval);
}


static int arrayent_configure(void)
{
	arrayent_return_e ret;

	char  *load_balancer[] = {
		arr_cloud.load_balancer_one,
		arr_cloud.load_balancer_two,
		arr_cloud.load_balancer_three};
	arrayent_config_t   config  = {
		.product_id                 = arr_cloud.product_id,
		.product_aes_key            = arr_cloud.product_aes,
		.load_balancer_domain_names = { load_balancer[0],
						load_balancer[1],
						load_balancer[2]},
		.load_balancer_udp_port     = LOAD_BALANCER_PORT,
		.load_balancer_tcp_port     = LOAD_BALANCER_PORT,
		.device_name                = arr_cloud.device_name,
		.device_password            = arr_cloud.device_password,
		.device_aes_key             = arr_cloud.device_aes
	};
	/*  Configure the product/device information for this target */
	if ((ret = ArrayentConfigure(&config)) != ARRAYENT_SUCCESS) {
		wmprintf("ArrayentConfigure Failed %d\r\n", ret);
		return -WM_FAIL;
	}
	return WM_SUCCESS;
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


static int arrayent_params_load()
{
	int status;

	status = cloud_get_probe_interval(&arr_cloud.probe_interval_secs);
	if (status != WM_SUCCESS) {
		/* We will be using the default interval */
		arr_cloud.probe_interval_secs = DEFAULT_PROBE_INTERVAL;
		status = WM_SUCCESS;
	}
	cl_dbg("Probe interval: %d", arr_cloud.probe_interval_secs);
	return status;
}

/* Function to wait for the Arrayent daemon to connect or reconnect in
 * case it had disconnected
 * because of inactivity.
 */

static int arrayent_connect_to_cloud(void)
{
	arrayent_net_status_t status;
	int firsttime = 1;

	cloud_sm(EVT_CONN_ERROR);

	while (!c.stop_request &&
	       (!(ArrayentNetStatus(&status) == ARRAYENT_SUCCESS)
		|| !(status.connected_to_server))) {
		if (firsttime) {
			cl_dbg("Arrayent daemon not connected to server.");
			cl_dbg("Waiting for good Arrayent network status...");
			cl_dbg("Arrayent network status: 0x%02X", status);
			firsttime = 0;
		} else
			 cl_dbg(" 0x%02X", status);

		/* Arbitrary polling rate */
		os_thread_sleep(os_msec_to_ticks(800));
	}
	if (c.stop_request)
		return -WM_FAIL;
	else
		return WM_SUCCESS;

}

int cloud_get_ui_link(httpd_request_t *req)
{
	char content[100];
	if (c.state != CLOUD_ACTIVE)
		return httpd_send_response(req, HTTP_RES_200, CLOUD_INACTIVE,
			strlen(CLOUD_INACTIVE), HTTP_CONTENT_PLAIN_TEXT_STR);

	snprintf(content, sizeof(content), "Moved to "
		"https://devkit-api.arrayent.com:8081/Utility/users/login");
	return httpd_send_response_301(req, content + 9,
		HTTP_CONTENT_PLAIN_TEXT_STR, content,
		strlen(content));

}

static void cloud_loop()
{
	int ret;
	if (WM_SUCCESS != arrayent_connect_to_cloud()) {
		return;
	}
	cloud_sm(EVT_OP_SUCCESS);
	if (arr_cloud.cloud_handler) {
		ret = arr_cloud.cloud_handler();
		if (ret == WM_SUCCESS)
			g_wm_stats.wm_cl_post_succ++;
		else
			g_wm_stats.wm_cl_post_fail++;
	}
}


void cloud_thread_main(os_thread_arg_t arg)
{
	if (WM_SUCCESS != arrayent_params_load()) {
		cl_dbg("Cloud: Load Parans error");
		goto out;
	}

	if (WM_SUCCESS != arrayent_configure()) {
		cl_dbg("Cloud: Configuration error");
		goto out;
	}
	cl_dbg("Cloud: Configuration done");

	bool is_init_done = false;

	/* Initialize the Arrayent layer */
	while (!c.stop_request && !is_init_done) {
		/* fixme : need to check  if needed*/
		os_thread_sleep(os_msec_to_ticks(100));
		cl_dbg("Initializing Arrayent daemon...");

		if (ArrayentInit() < ARRAYENT_SUCCESS) {
			cl_dbg("Initializing Arrayent daemon failed retrying!");
			os_thread_sleep(os_msec_to_ticks(100));
			continue;
		}

		is_init_done = true;
		cl_dbg("Arrayent daemon initialized.");
	}

	while (!c.stop_request) {
		cloud_loop();
		cloud_sleep(&c);
	}
 out:
	c.stop_request = false;
	os_thread_self_complete(NULL);
}

#define STACK_SIZE (1024 * 8)

int arrayent_cloud_start(const char *dev_class,
			 struct arrayent_cloud *arr_cloud_p)
{
	int ret;
	if (is_cloud_started)
		return WM_SUCCESS;

	if (NULL == arr_cloud_p)
		return -WM_FAIL;

	memcpy(&arr_cloud, arr_cloud_p, sizeof(arr_cloud));

	ret = os_semaphore_create(&sem, "cloud_sem");
	if (ret != WM_SUCCESS) {
		cl_dbg("Cloud semaphore creation error %d", ret);
		return -WM_FAIL;
	}
	os_semaphore_get(&sem, OS_WAIT_FOREVER);

	ret = cloud_actual_start(dev_class, NULL, NULL,
			STACK_SIZE);
	if (ret == WM_SUCCESS)
		is_cloud_started = true;
	else
		os_semaphore_delete(&sem);

	return ret;
}

int arrayent_cloud_stop()
{
	int ret = WM_SUCCESS;

	if (!is_cloud_started)
		return ret;

	os_semaphore_put(&sem);

	ret = cloud_actual_stop();
	os_semaphore_delete(&sem);
	is_cloud_started = false;

	return ret;
}
