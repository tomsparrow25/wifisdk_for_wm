/*
 *  Copyright (C) 2008-2014, Marvell International Ltd.
 *  All Rights Reserved.
 */

/*
 * Sample Application demonstrating the use of Xively Cloud
 *
 * This application communicates with Xively cloud once the device is
 * provisioned. Device can be provisioned using the psm CLIs as mentioned below.
 * After that, it periodically gets/updates (toggles) the state of board_led_1()
 * from/to the Xively cloud. Xively UI can be accessed at:
 * http://wmsdk.github.io/xively-js/wm_demo/index.html#feed_id=1296444087&api_key=uKFGVvl2S3UDuyHQ3tVOW2E4IlncVNJbA9zqokdiBgVc386V
 * On that UI, it can be seen that the state changes as per the led state on the
 * device
 */
#include <wm_os.h>
#include <app_framework.h>
#include <wmtime.h>
#include <cli.h>
#include <wmstdio.h>
#include <board.h>
#include <wmtime.h>
#include <psm.h>
#include "libxively/xively.h"
#include "libxively/xi_err.h"

#define FEED_ID 1296444087
#define API_KEY "uKFGVvl2S3UDuyHQ3tVOW2E4IlncVNJbA9zqokdiBgVc386V"

static int dummy;
/* Thread handle */
static os_thread_t app_thread;
/* Buffer to be used as stack */
static os_thread_stack_define(app_stack, 24 * 1024);

/* This function is defined for handling critical error.
 * For this application, we just stall and do nothing when
 * a critical error occurs.
 *
 */
void appln_critical_error_handler(void *data)
{
	while (1)
		;
	/* do nothing -- stall */
}


void do_cloud_post(xi_context_t *xi)
{
	int target_state_val;
	xi_feed_t output_channels, input_channels;
	xi_datastream_t *current_state_ds, *target_state_ds;
	xi_datapoint_t *current_state, *target_state;

	/* Create the input and output channels */
	memset(&output_channels, 0, sizeof(xi_feed_t));
	memset(&input_channels, 0, sizeof(xi_feed_t));

	output_channels.feed_id = xi->feed_id;
	input_channels.feed_id = xi->feed_id;

	/* Add the datastream "current_state" in the output channel */
	output_channels.datastream_count = 1;
	output_channels.datastreams[0].datapoint_count = 1;

	current_state_ds = &output_channels.datastreams[0];
	strcpy(current_state_ds->datastream_id, "current_state");
	current_state = &current_state_ds->datapoints[0];

	/* Add the same datastream "current_state" in the input channel too */
	input_channels.datastream_count = 1;
	input_channels.datastreams[0].datapoint_count = 1;

	target_state_ds = &input_channels.datastreams[0];
	strcpy(target_state_ds->datastream_id, "current_state");
	target_state = &target_state_ds->datapoints[0];


	/* Take appropriate action based on the value */
	if (dummy) {
		board_led_off(board_led_1());
		/* Toggle the value */
		dummy = 0;
	} else {
		/* Toggle the value */
		dummy = 1;
		board_led_on(board_led_1());
	}

	/* Send the updated value to the Xively cloud */
	wmprintf("Sending value to xi_feed_update: %d\r\n", dummy);
	xi_set_value_i32(current_state, dummy);
	xi_feed_update(xi, &output_channels);
	wmprintf("xi_feed_update status: %s\r\n\n",
		xi_get_error_string(xi_get_last_error()));
	/* Get the value of the datastream from Xively cloud */
	xi_feed_get(xi, &input_channels);
	wmprintf("xi_feed_get status: %s\r\n",
		xi_get_error_string(xi_get_last_error()));
	target_state_val = xi_get_value_i32(target_state);
	wmprintf("Received value from xi_feed_get: %d\r\n", target_state_val);
}

static void xively_demo_main(os_thread_arg_t data)
{
	xi_context_t *xi_cont;
	wmprintf("\r\n --- Xively Cloud Demo Application Started ---\r\n");
	xi_cont = xi_create_context(XI_HTTP, API_KEY, FEED_ID);
	if (xi_cont) {
		while (1) {
			do_cloud_post(xi_cont);
			os_thread_sleep(os_msec_to_ticks(2000));
		}
		/* The code will never reach here. This is just to show the
		 * usage of xi context delete after the desired operation is
		 * performed */
		xi_delete_context(xi_cont);
	} else {
		wmprintf("Failed to create xively context\r\n");
		appln_critical_error_handler((void *) -WM_FAIL);
	}
}

/* This is the main event handler for this project. The application framework
 * calls this function in response to the various events in the system.
 */
int common_event_handler(int event, void *data)
{
	int ret;
	static bool is_cloud_started;
	switch (event) {
	case AF_EVT_WLAN_INIT_DONE:
		ret = psm_cli_init();
		if (ret != WM_SUCCESS)
			wmprintf("Error: psm_cli_init failed\r\n");
		int i = (int) data;

		if (i == APP_NETWORK_NOT_PROVISIONED) {
			wmprintf("\r\nPlease provision the device "
				"and then reboot it:\r\n\r\n");
			wmprintf("psm-set network ssid <ssid>\r\n");
			wmprintf("psm-set network security <security_type>"
				"\r\n");
			wmprintf("    where: security_type: 0 if open,"
				" 3 if wpa, 4 if wpa2\r\n");
			wmprintf("psm-set network passphrase <passphrase>"
				" [valid only for WPA and WPA2 security]\r\n");
			wmprintf("psm-set network configured 1\r\n");
			wmprintf("pm-reboot\r\n\r\n");
		} else
			app_sta_start();

		break;
	case AF_EVT_NORMAL_CONNECTED:
		if (!is_cloud_started) {
			ret = os_thread_create(&app_thread, /* thread handle */
				"xively_demo_thread",/* thread name */
				xively_demo_main,  /* entry function */
				0,          /* argument */
				&app_stack,     /* stack */
				OS_PRIO_2);     /* priority - medium low */
			is_cloud_started = true;
		}
		break;
	default:
		break;
	}

	return 0;
}

static void modules_init()
{
	int ret;

	ret = wmstdio_init(UART0_ID, 0);
	if (ret != WM_SUCCESS) {
		wmprintf("Error: wmstdio_init failed\r\n");
		appln_critical_error_handler((void *) -WM_FAIL);
	}

	ret = cli_init();
	if (ret != WM_SUCCESS) {
		wmprintf("Error: cli_init failed\r\n");
		appln_critical_error_handler((void *) -WM_FAIL);
	}

	ret = pm_cli_init();
	if (ret != WM_SUCCESS) {
		wmprintf("Error: pm_cli_init failed\r\n");
		appln_critical_error_handler((void *) -WM_FAIL);
	}
	/* Initialize time subsystem.
	 *
	 * Initializes time to 1/1/1970 epoch 0.
	 */
	ret = wmtime_init();
	if (ret != WM_SUCCESS) {
		wmprintf("Error: wmtime_init failed\r\n");
		appln_critical_error_handler((void *) -WM_FAIL);
	}
	return;
}

int main()
{
	modules_init();

	wmprintf("Build Time: " __DATE__ " " __TIME__ "\r\n");

	/* Start the application framework */
	if (app_framework_start(common_event_handler) != WM_SUCCESS) {
		wmprintf("Failed to start application framework\r\n");
		appln_critical_error_handler((void *) -WM_FAIL);
	}
	return 0;
}
