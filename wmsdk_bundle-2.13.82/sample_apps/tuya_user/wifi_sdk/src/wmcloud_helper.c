
#include <json.h>
#include <diagnostics.h>
#include <rfget.h>
#include <wmtime.h>
#include <psm.h>
#include <httpc.h>
#include <app_framework.h>
#include <wmcloud.h>

extern cloud_t c;
static os_thread_t app_reboot_thread;
os_thread_stack_define(cloud_app_reboot_stack, 1024);
struct partition_entry *app_fs_get_passive();

static int cloud_get_url(char *url, int max_len)
{
	int status = psm_get_single(CLOUD_MOD_NAME,
				VAR_CLOUD_URL, url, CLOUD_MAX_URL_LEN);
	if (status != WM_SUCCESS)
		return status;

	if (strlen(url) == 0) {
		cl_dbg("Cloud variable in psm \"url\" empty");
		return -WM_FAIL;
	}

	return WM_SUCCESS;
}

static int cloud_get_device_name(char *name, int max_len)
{
	int status = psm_get_single(CLOUD_MOD_NAME, VAR_CLOUD_DEVICE_NAME,
				name, DEVICE_NAME_MAX_LEN);
	if (status != WM_SUCCESS)
		return status;

	if (strlen(name) == 0) {
		cl_dbg("Cloud variable in psm \"name\" empty");
		return -WM_FAIL;
	}

	return WM_SUCCESS;
}

static int cloud_validate_url(const char *url)
{
	unsigned parse_buf_needed_size = strlen(url) + 10;

	if (parse_buf_needed_size >= CLOUD_MAX_URL_LEN) {
		cl_dbg("Error: URL size greater than allowed");
		return -WM_FAIL;
	}

	char *tmp_buf = os_mem_alloc(parse_buf_needed_size);
	if (!tmp_buf) {
		cl_dbg("Error: Mem allocation failed. "
			    "Tried size: %d", parse_buf_needed_size);
		return -WM_E_NOMEM;
	}

	parsed_url_t parsed_url;
	int status = http_parse_URL(url, tmp_buf, parse_buf_needed_size,
				    &parsed_url);
	if (status != WM_SUCCESS) {
		os_mem_free(tmp_buf);
		cl_dbg("Error: URL parse failed");
		return status;
	}

	os_mem_free(tmp_buf);
	return WM_SUCCESS;
}

int cloud_url_set(const char *url)
{
	unsigned url_len = strlen(url);
	if (url_len < CLOUD_MIN_URL_LEN || url_len > CLOUD_MAX_URL_LEN) {
		cl_dbg("URL given: %s\n\r"
			    "It is not correct size. Size: %d", url,
			    url_len);
		return -WM_FAIL;
	}

	int status = cloud_validate_url(url);
	if (status != WM_SUCCESS)
		return status;

	status = psm_set_single(CLOUD_MOD_NAME, VAR_CLOUD_URL, url);
	if (status != WM_SUCCESS)
		return status;

	/* Cloud should update its parameters ASAP */
	cloud_params_load(&c);

	return status;
}

int cloud_device_name_set(const char *name)
{
	unsigned len = strlen(name);
	if (len > DEVICE_NAME_MAX_LEN || len <= 0) {
		cl_dbg("Name given: %s\n\r"
			    "It is not correct size. Size: %d", name,
			    len);
		return -WM_FAIL;
	}

	int status = psm_set_single(CLOUD_MOD_NAME,
				VAR_CLOUD_DEVICE_NAME, name);
	if (status != WM_SUCCESS)
		return status;

	/* Cloud should update its parameters ASAP */
	cloud_params_load(&c);

	return status;
}

static void cloud_handle_cloud(cloud_t *c, struct json_str *jstr,
		   struct json_object *obj, bool *repeat_POST)
{
	char buf[CLOUD_MAX_URL_LEN];
	bool url_requested = false, name_requested = false;

	if (json_get_composite_object(obj, J_NAME_CLOUD) == WM_SUCCESS) {
		if (json_get_val_str(obj, J_NAME_URL, buf, CLOUD_MAX_URL_LEN)
		    == WM_SUCCESS) {
			/* Query on cloud URL */
			if (strncmp(buf, QUERY_STR, CLOUD_MAX_URL_LEN) == 0) {
				url_requested = true;
			} else {
				if (!cloud_url_set(buf)) {
					url_requested = true;
				}
			}
		}
		if (json_get_val_str(obj, J_NAME_NAME, buf,
				DEVICE_NAME_MAX_LEN)
		    == WM_SUCCESS) {
			/* Query on device name */
			if (strncmp(buf, QUERY_STR, DEVICE_NAME_MAX_LEN) == 0) {
				name_requested = true;
			} else {
				if (!cloud_device_name_set(buf)) {
					name_requested = true;
				}
			}
		}
	}

	if (url_requested || name_requested) {
		json_push_object(jstr, J_NAME_CLOUD);
		if (url_requested)
			json_set_val_str(jstr, J_NAME_URL, c->url);
		if (name_requested)
			json_set_val_str(jstr, J_NAME_NAME, c->name);
		json_pop_object(jstr);
		*repeat_POST = true;
	}
}

static void cloud_handle_upgrades(cloud_t *c, struct json_str *jstr,
				struct json_object *obj, bool *repeat_POST)
{
	short fs_upgrade_done, fw_upgrade_done, wififw_upgrade_done;
	if (app_sys_http_update_all(obj, &fs_upgrade_done,
			&fw_upgrade_done, &wififw_upgrade_done)
	    == WM_SUCCESS) {
		/* Reboot only if the firmware was successfully
		 * updated */
		json_push_object(jstr, J_NAME_FIRMWARE);
		if (fs_upgrade_done)
			json_set_val_str(jstr, J_NAME_FS_URL, SUCCESS);
		if (fw_upgrade_done)
			json_set_val_str(jstr, J_NAME_FW_URL, SUCCESS);
		if (wififw_upgrade_done)
			json_set_val_str(jstr, J_NAME_WIFIFW_URL, SUCCESS);
		json_pop_object(jstr);

		if (fw_upgrade_done || wififw_upgrade_done)
			app_reboot(REASON_CLOUD_FW_UPDATE);
	} else {
		/* Send common reply also for failed case so that the cloud
		 * communication is not stalled */
		json_set_val_str(jstr, J_NAME_FIRMWARE, FAIL);
	}
	*repeat_POST = true;
}

static void cloud_handle_diag(cloud_t *c, struct json_str *jstr,
		  struct json_object *obj, bool *repeat_POST)
{
	char buf[16];

	json_push_object(jstr, J_NAME_DIAG);
	if (json_get_val_str(obj, J_NAME_DIAG_LIVE, buf, 16) == WM_SUCCESS) {
		if (strncmp(buf, QUERY_STR, 16) == 0) {
			json_push_object(jstr, J_NAME_DIAG_LIVE);
			diagnostics_read_stats(jstr);
			json_pop_object(jstr);
			*repeat_POST = true;
		}
	}

	if (json_get_val_str(obj, J_NAME_DIAG_HISTORY, buf, 16) == WM_SUCCESS) {
		if (strncmp(buf, QUERY_STR, 16) == 0) {
			json_push_object(jstr, J_NAME_DIAG_HISTORY);
			diagnostics_read_stats_psm(jstr);
			json_pop_object(jstr);
			*repeat_POST = true;
		}
	}
	json_pop_object(jstr);
}

#define HTTP_REBOOT_DELAY   2	/* 2 seconds */
static void app_reboot_main(os_thread_arg_t data)
{
	os_thread_sleep(os_msec_to_ticks(HTTP_REBOOT_DELAY * 1000));
	cl_dbg("Rebooting SOC...");
	app_reboot(REASON_USER_REBOOT);
	os_thread_self_complete(NULL);
}

static void cloud_handle_sys(cloud_t *c, struct json_str *jstr,
		 struct json_object *obj, bool *repeat_POST)
{
	char buf[16];
	time_t time;
	short rssi;

	if (json_get_composite_object(obj, J_NAME_SYS) != WM_SUCCESS)
		return;

	json_push_object(jstr, J_NAME_SYS);

	if (json_get_composite_object(obj, J_NAME_FIRMWARE) == WM_SUCCESS) {
		/* Do firmware handling here */
		cloud_handle_upgrades(c, jstr, obj, repeat_POST);
		json_release_composite_object(obj);
		json_get_composite_object(obj, J_NAME_SYS);
	}

	if (json_get_composite_object(obj, J_NAME_DIAG) == WM_SUCCESS) {
		cloud_handle_diag(c, jstr, obj, repeat_POST);
		json_release_composite_object(obj);
		json_get_composite_object(obj, J_NAME_SYS);
	}
	json_release_composite_object(obj);

	json_get_composite_object(obj, J_NAME_SYS);
	if (json_get_val_str(obj, J_NAME_REBOOT, buf, 16) == WM_SUCCESS) {
		if (strncmp(buf, "1", 16) == 0) {
			os_thread_create(&app_reboot_thread,
					 "reboot_thread",
					 app_reboot_main,
					 0, &cloud_app_reboot_stack, OS_PRIO_3);
		}
	}
	json_release_composite_object(obj);

	json_get_composite_object(obj, J_NAME_SYS);
	if (json_get_val_str(obj, J_NAME_RSSI, buf, 16) == WM_SUCCESS) {
		if (strncmp(buf, QUERY_STR, 16) == 0) {
			wlan_get_current_rssi(&rssi);
			json_set_val_int(jstr, J_NAME_RSSI, rssi);
			*repeat_POST = true;
		}
	}
	json_release_composite_object(obj);

	json_get_composite_object(obj, J_NAME_SYS);
	if (json_get_val_str(obj, J_NAME_TIME, buf, 16) == WM_SUCCESS) {
		if (strncmp(buf, QUERY_STR, 16) == 0) {
			time = wmtime_time_get_posix();
			json_set_val_int(jstr, J_NAME_TIME, time);
			*repeat_POST = true;
		}
	} else {

		json_release_composite_object(obj);
		json_get_composite_object(obj, J_NAME_SYS);

		if (json_get_val_int(obj, J_NAME_TIME, (int *)&time) ==
					WM_SUCCESS)
			wmtime_time_set_posix(time);
			time = wmtime_time_get_posix();
			json_set_val_int(jstr, J_NAME_TIME, time);
			*repeat_POST = true;
	}

	json_pop_object(jstr);
}

/* The function that processes any commands sent back by the cloud. */
void cloud_process_cmds(cloud_t *c, struct json_object *obj,
		   struct json_str *jstr, bool *repeat_POST)
{
	cloud_handle_cloud(c, jstr, obj, repeat_POST);

	cloud_handle_sys(c, jstr, obj, repeat_POST);

	if (c->app_cloud_handle_req)
		c->app_cloud_handle_req(jstr, obj, repeat_POST);
}

/*
 * Load the parameters from the PSM if present. If not then use the default
 * ones.
 */
int cloud_params_load(cloud_t *c)
{
	int status;
	static char psm_cloud_url[CLOUD_MAX_URL_LEN];
	static char psm_device_name[DEVICE_NAME_MAX_LEN];

	status = cloud_get_url(psm_cloud_url, CLOUD_MAX_URL_LEN - 1);
	if (status != WM_SUCCESS)
		c->url = DEFAULT_WMCLOUD_URL;
	else
		c->url = psm_cloud_url;
	cl_dbg("URL: %s", c->url);

	status = cloud_get_device_name(psm_device_name,
				DEVICE_NAME_MAX_LEN - 1);
	if (status != WM_SUCCESS)
		c->name = DEFAULT_DEVICE_NAME;
	else
		c->name = psm_device_name;
	cl_dbg("Device Name: %s", c->name);

	return WM_SUCCESS;
}
