/*
 *  Copyright (C) 2008-2014, Marvell International Ltd.
 *  All Rights Reserved.
 */
#include <wmstdio.h>
#include <board.h>
#include <wmcrypto.h>
#include <wm_os.h>
#include <json.h>
#include <wmcloud_arrayent.h>
#include <appln_dbg.h>
#include <psm.h>
#include <app_framework.h>

#if APPCONFIG_DEMO_CLOUD
#define DEVICE_CLASS	"wm_demo"

#define APP_CLOUD_MOD_NAME     "app-cloud"      /* app cloud module name */


#define VAR_APP_CLOUD_ARR_DEVICE_NAME "device-name"   /* cloud.device-name */
#define VAR_APP_CLOUD_DEVICE_PWD "device-password"   /* cloud.device-password */
#define VAR_APP_CLOUD_DEVICE_AES "device-aes"   /* cloud.device-aes */
#define VAR_APP_CLOUD_PRODUCT_ID "product-id"   /* cloud.product-id */
#define VAR_APP_CLOUD_PRODUCT_AES "product-aes"   /* cloud.product-aes */


 /* Device credentials & server info as assigned by Arrayent */
#define DEVICE_NAME         "MWF002"
#define DEVICE_PASSWORD     "AA"
#define PRODUCT_ID          2017
#define LOAD_BALANCER_ONE   "devkit-ds1.arrayent.com"
#define LOAD_BALANCER_TWO   "devkit-ds2.arrayent.com"
#define LOAD_BALANCER_THREE "devkit-ds3.arrayent.com"
#define DEVICE_AES_KEY      "2530E47B6201B210853423C6C2EC48D0"
#define PRODUCT_AES_KEY     "658598F5F482768C137D2729EE89F034"

#define RECEIVE_MAX_SIZE    255
#define PROPERTY_BUF_SIZE   32

/*------------------------------------------------------------*/

static uint32_t curr_led_state;
static bool is_psm_registered;

static int
wm_demo_get_arrayent_device_params(struct arrayent_cloud *p_arr_cloud)
{
	int status = WM_SUCCESS;
	status = psm_get_single(APP_CLOUD_MOD_NAME,
				    VAR_APP_CLOUD_ARR_DEVICE_NAME,
				    p_arr_cloud->device_name,
				    DEVICE_NAME_LEN);
	if (status != WM_SUCCESS ||
	    strlen(p_arr_cloud->device_name) ==  0) {
		return -WM_FAIL;
	}
	cl_dbg("Device Name: %s", p_arr_cloud->device_name);

	status = psm_get_single(APP_CLOUD_MOD_NAME,
				VAR_APP_CLOUD_DEVICE_PWD,
				p_arr_cloud->device_password,
				DEVICE_PWD_LEN);
	if (status != WM_SUCCESS ||
	    strlen(p_arr_cloud->device_password) == 0) {
		return -WM_FAIL;
	}
	cl_dbg("Device Password: %s", p_arr_cloud->device_password);

	status = psm_get_single(APP_CLOUD_MOD_NAME,
				VAR_APP_CLOUD_DEVICE_AES,
				p_arr_cloud->device_aes,
				AES_KEY_LEN);
	if (status != WM_SUCCESS ||
	    strlen(p_arr_cloud->device_aes) == 0) {
		return -WM_FAIL;
	}
	cl_dbg("Device AES Key : %s", p_arr_cloud->device_aes);
	return status;
}

static int
wm_demo_get_arrayent_product_params(struct arrayent_cloud *p_arr_cloud)
{
	char product_id[5];
	int status = psm_get_single(APP_CLOUD_MOD_NAME,
				VAR_APP_CLOUD_PRODUCT_ID, product_id,
				PRODUCT_ID_LEN);
	if (status != WM_SUCCESS ||
	    strlen(product_id) == 0) {
		return -WM_FAIL;
	} else {
		sscanf(product_id, "%d", &p_arr_cloud->product_id);
	}
	cl_dbg("Product id : %d", p_arr_cloud->product_id);

	status = psm_get_single(APP_CLOUD_MOD_NAME,
			    VAR_APP_CLOUD_PRODUCT_AES,
				p_arr_cloud->product_aes,
				    AES_KEY_LEN);
	if (status != WM_SUCCESS ||
	    strlen(p_arr_cloud->product_aes) == 0) {
		return -WM_FAIL;
	}
	cl_dbg("Product AES key : %s", p_arr_cloud->product_aes);
	return  WM_SUCCESS;
}

int do_cloud_post()
{
	int err = 0;
	char led_property[] = "led";
	/* One extra for the NULL at the end of a MAX received */
	char recv_data[RECEIVE_MAX_SIZE+1];
	char property_buf[PROPERTY_BUF_SIZE];
	char *property = NULL;

	uint32_t property_in_value;
	uint16_t recv_data_len = RECEIVE_MAX_SIZE;

	property = led_property;

	memset(property_buf, 0, PROPERTY_BUF_SIZE);
	memset(recv_data, 0, RECEIVE_MAX_SIZE);

	/* Check for any property updates from the Arrayent Cloud */
	if (ARRAYENT_SUCCESS ==
	    ArrayentRecvProperty(recv_data, &recv_data_len, 0)) {
		/* NULL Terminate the data received so that its
		 * treated like a string */
		recv_data[recv_data_len] = 0;

		sscanf(recv_data, "%s %d", property_buf,
		       &property_in_value);

		if (!strcmp(property_buf,
			    led_property)) {
			if (curr_led_state != property_in_value) {
				curr_led_state = property_in_value;
				if (property_in_value == 1)
					board_led_on(board_led_2());
				else if (property_in_value == 0)
					board_led_off(board_led_2());
			}
		}
	}

	sprintf(property_buf, "%d", curr_led_state);
	if (ArrayentSetProperty(property, property_buf) !=  WM_SUCCESS) {
		cl_dbg("Sending data failed");
		err++;
	}
	if (err)
		return -WM_FAIL;
	else
		return WM_SUCCESS;
}

static void populate_default_device_config(struct arrayent_cloud *p_arr_cloud)
{
	/* fill default values */
	memcpy(p_arr_cloud->device_name, DEVICE_NAME, DEVICE_NAME_LEN);
	memcpy(p_arr_cloud->device_password,
	       DEVICE_PASSWORD, DEVICE_PWD_LEN);
	memcpy(p_arr_cloud->device_aes, DEVICE_AES_KEY, AES_KEY_LEN);

	cl_dbg("Device Name: %s", p_arr_cloud->device_name);
	cl_dbg("Device Password: %s", p_arr_cloud->device_password);
	cl_dbg("Device AES Key : %s", p_arr_cloud->device_aes);
}


static void populate_default_product_config(struct arrayent_cloud *p_arr_cloud)
{
	p_arr_cloud->product_id = PRODUCT_ID;
	memcpy(p_arr_cloud->product_aes, PRODUCT_AES_KEY, AES_KEY_LEN);

	cl_dbg("Product AES key : %s", p_arr_cloud->product_aes);
	cl_dbg("Product id : %d", p_arr_cloud->product_id);
}

void wm_demo_cloud_start()
{
	int ret = WM_SUCCESS;
	struct arrayent_cloud *p_arr_cloud = NULL;

	p_arr_cloud = os_mem_alloc(sizeof(struct arrayent_cloud));
	if (NULL == p_arr_cloud)
		return;

	p_arr_cloud->cloud_handler = do_cloud_post;


	if (!is_psm_registered) {
		ret = psm_register_module(APP_CLOUD_MOD_NAME,
						 COMMON_PARTITION,
						 PSM_CREAT);
		if (ret != WM_SUCCESS
		    && ret != -WM_E_EXIST) {
			cl_dbg("Register app-cloud module fauiled : %d", ret);
		}
		is_psm_registered = true;
	}
	if (WM_SUCCESS != ret &&
	    -WM_E_EXIST != ret) {
		populate_default_device_config(p_arr_cloud);
		populate_default_product_config(p_arr_cloud);

	} else {
		/* Get  values form PSM or fill default values */
		ret = wm_demo_get_arrayent_device_params(p_arr_cloud);
		if (WM_SUCCESS != ret)
			populate_default_device_config(p_arr_cloud);
		ret = wm_demo_get_arrayent_product_params(p_arr_cloud);
		if (WM_SUCCESS != ret)
			populate_default_product_config(p_arr_cloud);
	}
	/* These are Load balancing urls*/
	memcpy(&p_arr_cloud->load_balancer_three,
	       LOAD_BALANCER_THREE, MAX_URL_LEN);
	memcpy(&p_arr_cloud->load_balancer_two,
	       LOAD_BALANCER_TWO, MAX_URL_LEN);
	memcpy(&p_arr_cloud->load_balancer_one,
	       LOAD_BALANCER_ONE, MAX_URL_LEN);

	/* Starting cloud thread if enabled */
	ret = arrayent_cloud_start(DEVICE_CLASS,
				   p_arr_cloud);
	if (ret != WM_SUCCESS)
		cl_dbg("Unable to start the cloud service\r\n");

	os_mem_free(p_arr_cloud);
}
void wm_demo_cloud_stop()
{
	int ret;
	/* Stop cloud server */
	ret = arrayent_cloud_stop();
	if (ret != WM_SUCCESS)
		cl_dbg("Unable to stop the cloud service\r\n");
}
#else
void wm_demo_cloud_start() {}
void wm_demo_cloud_stop() {}
#endif /* APPCONFIG_DEMO_CLOUD */
