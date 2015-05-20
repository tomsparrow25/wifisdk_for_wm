/*
 *  Copyright (C) 2008-2012, Marvell International Ltd.
 *  All Rights Reserved.
 */
#include <wmstdio.h>
#include <wm_os.h>
#include <json.h>
#include <board.h>
#include <appln_dbg.h>

#include <wmcloud_lp_ws.h>
#define J_NAME_WM_DEMO	"wm_demo"
#define J_NAME_STATE          "led_state"

#if APPCONFIG_DEMO_CLOUD
#include <wmcloud.h>
#define DEVICE_CLASS	"wm_demo"
#endif  /* APPCONFIG_DEMO_CLOUD */
static int led_state;

void wm_demo_periodic_post(struct json_str *jstr)
{
	json_push_object(jstr, J_NAME_WM_DEMO);
	json_set_val_int(jstr, J_NAME_STATE, led_state);
	json_pop_object(jstr);
}

void
wm_demo_handle_req(struct json_str *jstr, struct json_object *obj,
			bool *repeat_POST)
{

	char buf[16];
	bool flag = false;
	int req_state;

	if (json_get_composite_object(obj, J_NAME_WM_DEMO) == WM_SUCCESS) {
		if (json_get_val_str(obj, J_NAME_STATE, buf, 16)
		    == WM_SUCCESS) {
			if (!strncmp(buf, QUERY_STR, 16)) {
				dbg("led state query");
				*repeat_POST = flag = true;
			}
		} else if (json_get_val_int(obj, J_NAME_STATE, &req_state) ==
			   WM_SUCCESS) {
			if (req_state == 0) {
				dbg("led state off");
				board_led_off(board_led_2());
				led_state = 0;
				*repeat_POST = flag = true;
			} else if (req_state == 1) {
				dbg("led state on");
				board_led_on(board_led_2());
				led_state = 1;
				*repeat_POST = flag = true;
			}

		}

		if (flag) {
			json_push_object(jstr, J_NAME_WM_DEMO);
			json_set_val_int(jstr, J_NAME_STATE,
					 led_state);
			json_pop_object(jstr);
		}
	}

}

#if APPCONFIG_DEMO_CLOUD
void wm_demo_cloud_start()
{
	int ret;
	/* Starting cloud thread if enabled */
	ret = cloud_start(DEVICE_CLASS, wm_demo_handle_req,
		wm_demo_periodic_post);
	if (ret != WM_SUCCESS)
		dbg("Unable to start the cloud service");
}
void wm_demo_cloud_stop()
{
	int ret;
	/* Stop cloud server */
	ret = cloud_stop();
	if (ret != WM_SUCCESS)
		dbg("Unable to stop the cloud service");
}
#else
void wm_demo_cloud_start() {}
void wm_demo_cloud_stop() {}
#endif /* APPCONFIG_DEMO_CLOUD */
