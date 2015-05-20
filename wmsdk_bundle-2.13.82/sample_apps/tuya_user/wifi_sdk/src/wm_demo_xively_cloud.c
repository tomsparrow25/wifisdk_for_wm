/*
 *  Copyright (C) 2008-2014, Marvell International Ltd.
 *  All Rights Reserved.
 */
#include <wmstdio.h>
#include <wm_os.h>
#include <json.h>
#include <board.h>
#include <appln_dbg.h>
#include <psm.h>
#include <wmcloud_xively.h>

#if APPCONFIG_DEMO_CLOUD
#define DEVICE_CLASS	"wm_demo"
#endif  /* APPCONFIG_DEMO_CLOUD */
static int led_state;

char xively_product_secret[] = { 0x15, 0xe1, 0x03, 0xeb, 0xba, 0x62, 0x02, 0x45,
				0x2e, 0x9d, 0xa6, 0xfb, 0x62, 0x38, 0xe1, 0x69,
				0x40, 0x33, 0xad, 0x91 };

int do_cloud_post(xi_context_t *xi)
{
	int err = 0;
	xi_feed_t output_channels, input_channels;
	xi_datastream_t *current_state_ds, *target_state_ds,
				*tx_target_state_ds;
	xi_datapoint_t *current_state, *target_state, *tx_target_state;

	memset(&output_channels, 0, sizeof(xi_feed_t));
	memset(&input_channels, 0, sizeof(xi_feed_t));

	output_channels.feed_id = xi->feed_id;
	input_channels.feed_id = xi->feed_id;

	output_channels.datastream_count = 2;
	output_channels.datastreams[0].datapoint_count = 1;
	output_channels.datastreams[1].datapoint_count = 1;

	current_state_ds = &output_channels.datastreams[0];
	strcpy(current_state_ds->datastream_id, "current_state");
	current_state = &current_state_ds->datapoints[0];

	tx_target_state_ds = &output_channels.datastreams[1];
	strcpy(tx_target_state_ds->datastream_id, "target_state");
	tx_target_state = &tx_target_state_ds->datapoints[0];

	input_channels.datastream_count = 1;
	input_channels.datastreams[0].datapoint_count = 1;

	target_state_ds = &input_channels.datastreams[0];
	strcpy(target_state_ds->datastream_id, "target_state");
	target_state = &target_state_ds->datapoints[0];

	if (!xi_feed_get(xi, &input_channels)) {
		err++;
		cl_dbg("Failed to get data from xively cloud\r\n");
	}
	cl_dbg("%s", xi_get_error_string(xi_get_last_error()));
	int target_state_val = xi_get_value_i32(target_state);
	cl_dbg("Target State: %d\r\n", target_state_val);

	if (target_state_val == 1) {
		board_led_on(board_led_2());
		led_state = 1;
	} else if (target_state_val == 0) {
		board_led_off(board_led_2());
		led_state = 0;
	}
	cl_dbg("Sending Current State: %d\r\n", led_state);

	xi_set_value_i32(current_state, led_state);
	xi_set_value_i32(tx_target_state, -1);
	if (!xi_feed_update(xi, &output_channels)) {
		err++;
		cl_dbg("Failed to send data to xively cloud\r\n");
	}
	cl_dbg("%s", xi_get_error_string(xi_get_last_error()));

	if (err)
		return -WM_FAIL;
	else
		return WM_SUCCESS;
}


#if APPCONFIG_DEMO_CLOUD

static void get_xively_serial_no(unsigned char *serial, int serial_len)
{
	unsigned char mac[6];
	if (serial_len < 6)
		return;
	wlan_get_mac_address(mac);
	snprintf((char *)serial, serial_len, "%02x%02x%02x%02x%02x%02x",
		mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
	wmprintf("serial = %s\r\n", serial);
}

void wm_demo_cloud_start()
{
	int ret;
	char serial_no[XIVELY_SERIAL_NO_MAX];
	get_xively_serial_no((unsigned char *)serial_no,
					sizeof(serial_no));
	/* Starting cloud thread if enabled */
	ret = xively_cloud_start(DEVICE_CLASS, serial_no,
			xively_product_secret, sizeof(xively_product_secret),
			do_cloud_post);
	if (ret != WM_SUCCESS)
		cl_dbg("Unable to start the cloud service\r\n");
}
void wm_demo_cloud_stop()
{

	int ret;
	/* Stop cloud server */
	ret = xively_cloud_stop();
	if (ret != WM_SUCCESS)
		cl_dbg("Unable to stop the cloud service\r\n");
}
#else
void wm_demo_cloud_start() {}
void wm_demo_cloud_stop() {}
#endif /* APPCONFIG_DEMO_CLOUD */
