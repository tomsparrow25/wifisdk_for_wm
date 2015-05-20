/*
 *  Copyright 2008-2013, Marvell International Ltd.
 *  All Rights Reserved.
 */

/*
 * app_provisioning.c: Functions that deal with the provisioning of the device.
 */
#include <string.h>

#include <wmstdio.h>
#include <wm_net.h>
#include <wlan.h>
#include <provisioning.h>
#include <psm.h>
#include <diagnostics.h>
#include <app_framework.h>

#include "app_fs.h"
#include "app_ctrl.h"
#include "app_http.h"
#include "app_dbg.h"
#include "app_network_config.h"
#include "app_provisioning.h"
#include "app_test_http_handlers.h"

static struct wlan_network home_nw;
#ifdef CONFIG_APP_FRM_PROV_WPS
#include <wps.h>
static int wps_pin_enabled, wps_pbc_enabled;
extern os_semaphore_t app_wps_sem;
#endif


int app_set_scan_config_psm(struct prov_scan_config *scancfg)
{
	int ret = WM_SUCCESS;
	psm_handle_t handle;
	char psm_val[MAX_PSM_VAL];

	if ((ret = psm_open(&handle, SCAN_CONFIG_MOD_NAME)) !=  WM_SUCCESS) {
		app_e("prov: error opening psm handle %d", ret);
		app_psm_handle_partition_error(ret, SCAN_CONFIG_MOD_NAME);
		return -1;
	}
	if (!verify_scan_duration_value
	    ((scancfg->wifi_scan_params).scan_duration)) {
		snprintf(psm_val, sizeof(psm_val), "%d",
			 (scancfg->wifi_scan_params).scan_duration);
		ret = psm_set(&handle, VAR_SCAN_DURATION, psm_val);
	}
	if (!verify_split_scan_delay
	    ((scancfg->wifi_scan_params).split_scan_delay)) {
		snprintf(psm_val, sizeof(psm_val), "%d",
			 (scancfg->wifi_scan_params).split_scan_delay);
		ret |= psm_set(&handle, VAR_SPLIT_SCAN_DELAY, psm_val);
	}

	if (!verify_scan_interval_value(scancfg->scan_interval)) {
		snprintf(psm_val, sizeof(psm_val), "%d",
			 scancfg->scan_interval);
		ret |= psm_set(&handle, VAR_SCAN_INTERVAL, psm_val);
	}
	if (!verify_scan_channel_value(
			(scancfg->wifi_scan_params).channel[0])) {
		snprintf(psm_val, sizeof(psm_val), "%d",
			 (scancfg->wifi_scan_params).channel[0]);
		ret |= psm_set(&handle, VAR_SCAN_CHANNEL, psm_val);
	}
	psm_close(&handle);
	if (ret != WM_SUCCESS)
		ret = -WM_E_AF_NW_WR;

	return ret;
}

int app_load_scan_config(struct prov_scan_config *scancfg)
{
	char psm_val[MAX_PSM_VAL];
	psm_handle_t handle;
	int ret;

	(scancfg->wifi_scan_params).bssid = NULL;
	(scancfg->wifi_scan_params).ssid = NULL;
	(scancfg->wifi_scan_params).channel[0] = 0;
	(scancfg->wifi_scan_params).bss_type = BSS_ANY;

	if ((ret = psm_open(&handle, SCAN_CONFIG_MOD_NAME)) != 0) {
		app_e("prov: error opening psm handle %d", ret);
		app_psm_handle_partition_error(ret, SCAN_CONFIG_MOD_NAME);
		return -WM_FAIL;
	}
	if (!psm_get(&handle, VAR_SCAN_DURATION, psm_val, sizeof(psm_val))) {
		(scancfg->wifi_scan_params).scan_duration = atoi(psm_val);
	}
	if (!psm_get(&handle, VAR_SPLIT_SCAN_DELAY, psm_val, sizeof(psm_val))) {
		(scancfg->wifi_scan_params).split_scan_delay = atoi(psm_val);
	}
	if (!psm_get(&handle, VAR_SCAN_INTERVAL, psm_val, sizeof(psm_val))) {
		scancfg->scan_interval = atoi(psm_val);
	}
	if (!psm_get(&handle, VAR_SCAN_CHANNEL, psm_val, sizeof(psm_val))) {
		(scancfg->wifi_scan_params).channel[0] = atoi(psm_val);
	}
	psm_close(&handle);

	return ret;
}

static int app_prov_event_handler(enum prov_event event, void *arg, int len)
{
	int ret = WM_SUCCESS;
#ifdef CONFIG_APP_FRM_PROV_WPS
	int nr_scan_res;
	struct wlan_scan_result *scan_result;
	int i;
#endif

	switch (event) {
	case PROV_START:
		/* Do nothing */
		break;
	case PROV_NETWORK_SET_CB:
		if (len != sizeof(struct wlan_network))
			return -WM_FAIL;
		memcpy(&home_nw, arg, sizeof(struct wlan_network));
		app_configure_network(&home_nw);
		break;
	case PROV_SCAN_CFG_SET_CB:
		if (len != sizeof(struct prov_scan_config))
			return -WM_FAIL;
		app_set_scan_config_psm((struct prov_scan_config *)arg);
		break;
#ifdef CONFIG_APP_FRM_PROV_WPS
	case PROV_WPS_SSID_SELECT_REQ:
		/* If you have any specific constraints on selecting the SSID
		 * you should modify this code to enforce that.
		 */
		if (len % sizeof(struct wlan_scan_result) != 0) {
			app_e("Invalid scan results ");
			return -1;
		}

		nr_scan_res = len / sizeof(struct wlan_scan_result);
		scan_result = (struct wlan_scan_result *)arg;
		for (i = 0; i < nr_scan_res; i++, scan_result++) {

			if (!scan_result->wps)
				continue;

			/* PIN is given the first priority, if both PIN and PBC
			 * are active */
			/* When we get wps enabled AP in scan result, we start
			 * wps session. For WPS session we need more heap
			 * to get started. AF_EVT_PROV_WPS_SSID_SELECT_REQ
			 * is passed to application when we find wps enabled
			 * AP. In this even application can shutdown/stop some
			 * threads (like httpd) so that heap can be given
			 * to wps and such threads can be restarted when
			 * we are connect in normal mode or there is wps
			 * time out.
			 * Since we are sending events through queue, we need
			 * to make sure that thread deletion should take place
			 * before wps session starts. Below semaphore blocks
			 * here till application deletes threads which are
			 * not required for wps session and after deletion
			 * application release this semaphore so that below
			 * semaphore will be unblocked and can move further.
			 */

			if (wps_pin_enabled &&
			    (scan_result->wps_session == WPS_SESSION_PIN)) {
				app_ctrl_notify_event_wait(
					AF_EVT_PROV_WPS_SSID_SELECT_REQ, NULL);
				os_semaphore_get(&app_wps_sem, OS_WAIT_FOREVER);
				return i;
			}

			if (wps_pbc_enabled &&
			    (scan_result->wps_session == WPS_SESSION_PBC)) {
				app_ctrl_notify_event_wait(
					AF_EVT_PROV_WPS_SSID_SELECT_REQ, NULL);
				os_semaphore_get(&app_wps_sem, OS_WAIT_FOREVER);
				return i;
			}
		}
		break;
	case PROV_WPS_SESSION_STARTED:
		app_ctrl_notify_event(AF_EVT_PROV_WPS_START, NULL);
		break;
	case PROV_WPS_SESSION_UNSUCCESSFUL:
		wps_pbc_enabled = 0;
		wps_pin_enabled = 0;
		app_ctrl_notify_event(AF_EVT_PROV_WPS_UNSUCCESSFUL, NULL);
		break;
	case PROV_WPS_SESSION_SUCCESSFUL:
		app_ctrl_notify_event(AF_EVT_PROV_WPS_SUCCESSFUL, NULL);
		break;
	case PROV_WPS_PIN_TIMEOUT:
		wps_pin_enabled = 0;
		app_ctrl_notify_event(AF_EVT_PROV_WPS_REQ_TIMEOUT, NULL);
		break;
	case PROV_WPS_PBC_TIMEOUT:
		wps_pbc_enabled = 0;
		app_ctrl_notify_event(AF_EVT_PROV_WPS_REQ_TIMEOUT, NULL);
		break;
#endif
	default:
		app_w("Invalid Provisioning Event %d", event);
		break;
	}

	return ret;

}

static struct provisioning_config pc;

int app_provisioning_get_type()
{
	return pc.prov_mode;
}

int app_provisioning_start(char prov_mode)
{
	struct prov_scan_config scancfg;

	pc.prov_mode = prov_mode;
	pc.provisioning_event_handler = app_prov_event_handler;

#ifdef CONFIG_APP_FRM_PROV_WPS
	int ret = os_semaphore_create(&app_wps_sem, "wps_semaphore");
	if (ret) {
		app_e("app_ctrl: failed to create wps semaphore: %d",
		      ret);
		return ret;
	}
	/* Get semaphore for first time so that next get will be
	   blocking*/
	os_semaphore_get(&app_wps_sem, OS_WAIT_FOREVER);
#endif

	if (prov_start(&pc) != 0) {
		app_e("prov: failed to launch provisioning app.");
		return -WM_FAIL;
	}

	/* Load the scan configuration, if any, from psm */
	app_load_scan_config(&scancfg);
	prov_set_scan_config_no_cb(&scancfg);

	register_provisioning_web_handlers();

	app_ctrl_notify_event(AF_EVT_INTERNAL_PROV_REQUESTED, (void *)NULL);

	return WM_SUCCESS;
}

int app_ezconnect_provisioning_start(uint8_t *prov_key, int prov_key_len)
{

	pc.prov_mode = PROVISIONING_EZCONNECT;
	pc.provisioning_event_handler = app_prov_event_handler;

	if (prov_ezconnect_start(&pc, prov_key, prov_key_len) != 0) {
		app_e("prov: failed to launch provisioning app.");
		return -WM_FAIL;
	}

	app_ctrl_notify_event(AF_EVT_INTERNAL_PROV_REQUESTED, (void *)NULL);

	return WM_SUCCESS;
}

void app_ezconnect_provisioning_stop()
{
	prov_ezconnect_finish();
}

void app_provisioning_stop()
{

	unregister_provisioning_web_handlers();
	prov_finish();
#ifdef CONFIG_APP_FRM_PROV_WPS
	os_semaphore_delete(&app_wps_sem);
#endif /* CONFIG_APP_FRM_PROV_WPS */
}

#ifdef CONFIG_APP_FRM_PROV_WPS

int app_prov_wps_session_start(char *pin)
{
	int ret;

	if (pin) {
		unsigned int ipin;
		ipin = atoi(pin);
		/* This is a pin session */
		ret = prov_wps_provide_pin(ipin);
		if (ret == WM_SUCCESS) {
			wps_pin_enabled = 1;
		}
	} else {
		/* This is a PBC session */
		ret = prov_wps_pushbutton_press();
		if (ret == WM_SUCCESS) {
			wps_pbc_enabled = 1;
		}
	}
	return ret;
}

#endif
