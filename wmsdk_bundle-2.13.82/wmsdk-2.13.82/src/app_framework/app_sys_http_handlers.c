/*
 *  Copyright 2008-2013, Marvell International Ltd.
 *  All Rights Reserved.
 */

/*
 * app_sys_http_handlers.c: Handlers for the /sys/ HTTP API
 */

#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include <wm_os.h>
#include <wmstdio.h>
#include <rfget.h>
#include <httpd.h>
#include <http_parse.h>
#include <http-strings.h>
#include <wmtime.h>
#include <pwrmgr.h>
#include <json.h>
#include <diagnostics.h>
#include <wm_net.h>
#include <wm_os.h>
#include <wlan.h>
#include <board.h>
#include <partition.h>
#include <wifi.h>

#include <provisioning.h>
#include "app_ctrl.h"
#include "app_dbg.h"
#include "app_http.h"
#include "app_network_config.h"
#include "app_provisioning.h"
#include "app_fs.h"

#ifdef HTTP_CONTENT_TYPE_JSON
const char *app_http_content_type_json = http_content_type_json_nocache;
const int app_http_content_type_len = sizeof(http_content_type_json_nocache) -
    1;
#else
const char *app_http_content_type_json = http_content_type_plain;
const int app_http_content_type_len = sizeof(http_content_type_plain) - 1;
#endif

int g_app_sys_handlers_no;

static os_thread_t app_reboot_thread;
os_thread_stack_define(app_reboot_stack, 1024);

struct httpd_wsgi_call g_sys_handlers[];

#define RFGET_HTTP_API_FWUPDATE "/sys/firmware"
#ifdef CONFIG_WIFI_FW_UPGRADE
#define RFGET_HTTP_API_WIFI_FWUPDATE "/sys/wifi-firmware"
#define J_NAME_WIFIFWURL  "wififw_url"
#endif
#define RFGET_HTTP_API_FSUPDATE "/sys/filesystem"
#define RFGET_HTTP_API_FWUPDATE2 "/sys/fw-image"
#define RFGET_HTTP_API_FSUPDATE2 "/sys/fs-image"

#define RFGET_HTTP_API_ALLUPDATE "/sys/updater"
#define SYS_HTTP_REBOOT_CMD "reboot"

#define J_NAME_SDK_VER    "sdk_ver"
#define J_NAME_UUID       "uuid"
#define J_NAME_MAC       "mac_addr"
#define J_NAME_APIVER     "api_version"
#define J_NAME_WLAN_FWVER "wlan_fw_version"
#define J_NAME_FSURL      "fs_url"
#define J_NAME_FWURL      "fw_url"

#define J_NAME_MODE		"mode"
#define J_NAME_PROV_FINISH      "finish"
#define J_NAME_DEVICE		"name"
#define J_NAME_EPOCH             "epoch"
#define J_NAME_SSID		"ssid"
#define J_NAME_UAP_SSID		"uap_ssid"
#define J_NAME_BSSID		"bssid"
#define J_NAME_CHANNEL		"channel"
#define J_NAME_SECURITY 	"security"
#define J_NAME_PASSPHRASE	"passphrase"
/* #define J_NAME_DHCP		"ip" */
#define	J_NAME_IP		"ipaddr"
#define J_NAME_NETMASK		"ipmask"
#define J_NAME_GW		"ipgw"
#define J_NAME_COMMAND		"command"
#define J_NAME_WD_TIMEOUT	"wd_timeout"
#define J_NAME_WD_STROBE	"wd_strobe"

#define J_NAME_ARCH             "arch"
#define J_NAME_STATS_XMIT     "xmit"
#define J_NAME_STATS_RECV     "recv"
#define J_NAME_STATS_NET_ERROR "network_errors"
#define J_NAME_CONN_LIST      "conn_list"
#define J_NAME_STATS_RSSI     "rssi"
#define J_NAME_SSID           "ssid"
#define J_NAME_STATS_VER      "verext"
#define J_NAME_STATS_HEAP_USG "heap_usage"
#define J_NAME_STATS_FREQ     "cpu_freq"
#define J_NAME_STATS_LLOST     "link_loss"
#define J_NAME_STATS_DHCP_REQ  "dhcp_served"
#define J_NAME_AUTH_FAILED     "auth_failed"
#define J_NAME_CONN_FAILED     "conn_failed"
#define J_NAME_DHCP_FAILED     "dhcp_failed"
#define J_NAME_CONN_SUCCESS    "conn_success"
#define J_NAME_STA_CONN_STATUS "sta_conn_state"
#define	STR_STATION		"station"
#define	STR_UAP			"uap"
#define J_NAME_INTERFACE        "interface"
#define J_NAME_CONNECTION       "connection"
#define J_NAME_STA              STR_STATION
#define J_NAME_UAP              STR_UAP
#define J_NAME_CONFIGURED       "configured"
#define J_NAME_STATUS           "status"
#define J_NAME_FAILURE          "failure"
#define J_NAME_FAILURE_CNT      "failure_cnt"
#define J_NAME_PROVISIONING     "prov"
#define J_NAME_PROV_TYPES       "types"
#define J_NAME_CLIENT_ACK       "client_ack"


#define J_NAME_DNS1             "ipdns1"
#define J_NAME_DNS2             "ipdns2"
#define J_NAME_RSSI             "rssi"
#define J_NAME_IPTYPE           "ip"

#define SYS_ARCH_STR "mc200"
#define MAX_MAC_BYTES 6
#define MAX_MAC_STRLEN 20

const char *app_http_content_type_mp_form = "multipart/form-data;";
const char *app_http_mp_form_boundary = "boundary=";

static long int app_http_access_time = 0;
static struct json_str jstr;
static struct json_object obj;
#define MAX_JSON_VAL 64
static char json_val[MAX_JSON_VAL];
static char update_url[200];

int app_consume_http_mp_end(int sock, char *scratch, int scratch_len)
{
	int req_line_len;

	/* Read multipart-data boundary "\r\n" */
	req_line_len = htsys_getln_soc(sock, scratch, scratch_len);
	app_d("http: mp end header line of length %d", req_line_len);
	if (req_line_len == -WM_FAIL) {
		app_e("could not read line from socket");
		return -WM_FAIL;
	}

	/* Read multipart-dat boundary end "--------------xxxxxxxxx--" */
	req_line_len = htsys_getln_soc(sock, scratch, scratch_len);
	app_d("http: mp end header line of length %d", req_line_len);
	if (req_line_len == -WM_FAIL) {
		app_e("error: could not read line from socket");
		return -WM_FAIL;
	}

	return 0;
}

int app_consume_http_headers(int sock, char *scratch, int scratch_len)
{
	int req_line_len, total_len = 0;

	while (true) {
		/* Read a single line of header */
		req_line_len = htsys_getln_soc(sock, scratch, scratch_len);
		app_d("http: read header line of length %d",
			req_line_len);
		if (req_line_len == -WM_FAIL) {
			app_e("could not read line from socket");
			return -WM_FAIL;
		}

		/* End of the headers */
		if ((*scratch == ISO_nl) || (*scratch == ISO_cr) ||
		    (*scratch == 0)) {
			/* 4 bytes for 2 CR-LF's */
			total_len += 4;
			/* Get out of the loop and return */
			break;
		}

		/* 2 bytes for CR-LF for every line read */
		total_len += req_line_len + 2;
	}

	app_d("http: size of header read %d", total_len);
	return total_len;
}

int app_http_get_mp_form_length(httpd_request_t * req, int sock,
				char *scratch, int scratch_len)
{
	char *str;
	int len = -WM_FAIL;

	/* We have multipart/form-data. get to boundary delimiter */
	str = req->content_type + strlen(app_http_content_type_mp_form);
	/* Skip spaces */
	while (*str == ' ')
		str++;

	if (!strncmp(app_http_mp_form_boundary, str,
		     strlen(app_http_mp_form_boundary)))
		str += strlen(app_http_mp_form_boundary);

	/* len will contain the size of the headers that we have consumed */
	len = app_consume_http_headers(sock, scratch, scratch_len);

	if (len != -WM_FAIL) {
		/* 2 bytes for CR-LF after data and 4 bytes of CR-LF (double) after boundary delimiter */
		len = req->body_nbytes - (len + strlen(str) + 2 + 4);
	}

	app_d("http: Content-Length %d adjusted fw size is %d",
		req->body_nbytes, len);
	return len;
}

int app_sys_http_update_all(struct json_object *obj, short *fs_done,
			    short *fw_done, short *wififw_done)
{
	int status, fs_valid = 1, fw_valid = 1;

#ifdef CONFIG_WIFI_FW_UPGRADE
	int wififw_valid = 1;

	if (wififw_done)
		*wififw_done = 0;
#endif
	if (fs_done)
		*fs_done = 0;
	if (fw_done)
		*fw_done = 0;

	/* For now, ignore the firmware version that came in the json text */

	/* NOTE: The order in which the upgrades are performed is important for
	 * handling atomic updates.
	 */
	if (json_get_val_str(obj, J_NAME_FSURL, update_url,
			     sizeof(update_url)) == WM_SUCCESS) {
		/* Perform FS update first */
		app_d("\r\nfs update url: %s", update_url);
		struct partition_entry *p = app_fs_get_passive();
		status = rfget_client_mode_update(update_url, p);
		if (status != WM_SUCCESS) {
			app_e("Failed to perform fs_update ");
			return -WM_FAIL;
		}
		/* Below condition gets executed only if the provided
		 * url and json variable for filesyste update was correct
		 * and also the actual update was successful, it will be
		 * used only by update from cloud */
		if (fs_done)
			*fs_done = 1;
	} else {
		fs_valid = 0;
		app_d("FS Update is not ordered");
	}

	if (json_get_val_str(obj, J_NAME_FWURL, update_url,
			     sizeof(update_url)) == WM_SUCCESS) {
		/* Perform FW update later */
		app_d("\r\nfw update url: %s", update_url);
		struct partition_entry *p = rfget_get_passive_firmware();
		if (p == NULL) {
			app_e("Failed to get passive partition");
			return -WM_FAIL;
		}
		status = rfget_client_mode_update(update_url, p);
		if (status != WM_SUCCESS) {
			app_e("Failed to perform fw_update ");
			return -WM_FAIL;
		}
		/* Below condition gets executed only if the provided
		 * url and json variable for firmware update was correct
		 * and also the actual update was successful, it will be
		 * used only by update from cloud */
		if (fw_done)
			*fw_done = 1;
	} else {
		fw_valid = 0;
		app_d("FW Update is not ordered");
	}

#ifdef CONFIG_WIFI_FW_UPGRADE
	if (json_get_val_str(obj, J_NAME_WIFIFWURL, update_url,
			     sizeof(update_url)) == WM_SUCCESS) {
		/* Perform WiFi FW update later */
		app_d("\r\nwifi fw update url: %s", update_url);
		struct partition_entry *p = rfget_get_passive_wifi_firmware();
		if (p == NULL) {
			app_e("Failed to get passive partition");
			return -WM_FAIL;
		}
		status = rfget_client_mode_update(update_url, p);
		if (status != WM_SUCCESS) {
			app_e("Failed to perform wififw_update ");
			return -WM_FAIL;
		}
		/* Below condition gets executed only if the provided
		 * url and json variable for firmware update was correct
		 * and also the actual update was successful, it will be
		 * used only by update from cloud */
		if (wififw_done)
			*wififw_done = 1;
	} else {
		wififw_valid = 0;
		app_d("WiFi FW Update is not ordered");
	}
#endif
	/* fs_valid & fw_valid variables checks whether the provided
	 * json variable names are correct or not for filesystem &
	 * firmware update */
	if ((fs_valid != 1) && (fw_valid != 1)
#ifdef CONFIG_WIFI_FW_UPGRADE
		&& (wififw_valid != 1)
#endif
	)
		return -WM_FAIL;
	else
		return WM_SUCCESS;
}

#define PERF_BUF_SIZE 512
static int sys_post_perf(httpd_request_t *req)
{
	size_t file_length, xfer;
	int err = -1;
	uint8_t mp_header = 0;
	uint32_t start_time, end_time;
	char perf_resp[50];

	char *buf = os_mem_alloc(PERF_BUF_SIZE);
	if (!buf) {
		snprintf(perf_resp, sizeof(perf_resp),
				"Failed to allocate memory");
		err = -WM_E_NOMEM;
		err = httpd_send_response(req, HTTP_RES_200, perf_resp,
			strlen(perf_resp), HTTP_CONTENT_JSON_STR);

		goto out;
	}


	err = httpd_parse_hdr_tags(req, req->sock, buf, PERF_BUF_SIZE);
	if (err != WM_SUCCESS)
		goto out1;

	if (!strncmp(app_http_content_type_mp_form, req->content_type,
		     strlen(app_http_content_type_mp_form))) {
		/*
		 * We require that the content length be available and set
		 * correctly If there was no trailing data after the firmware
		 * image contents, we can detect socket closure (rfget_recv
		 * returns -1) to read just contents correctly. However, since
		 * there is trailing data (boundary delimiter), we need to know
		 * the length so that we can stop reading from the stream
		 */
		if ((file_length =
		     app_http_get_mp_form_length(req, req->sock, buf,
						 PERF_BUF_SIZE)) == -WM_FAIL) {

			err = -WM_FAIL;
			goto out1;
		}

		/* multipart-header present in request */
		mp_header = 1;
	} else
		file_length = req->body_nbytes;

	xfer = file_length;
	start_time = os_ticks_get();
	while (file_length > 0) {
		err = recv(req->sock, buf,
		   (file_length > PERF_BUF_SIZE) ? PERF_BUF_SIZE : file_length,
		   0);
		if (err < 0) {
			err = -WM_FAIL;
			goto out1;
		}

		file_length -= err;
	}
	end_time = os_ticks_get();

	end_time = (start_time == end_time) ? end_time + 1 : end_time;


	/* Performance should be in bits/sec, so multiply by 8. Also end and
	 * start times are milliseconds so multiply numerator by 1000 */
	int perf = ((xfer * 8) / (end_time - start_time)) * 1000;

#define MEGA (1 << 10 << 10)
	if (perf > (MEGA * 8)) {
		int rest;
		int orig_perf;
		orig_perf = perf;
		/* Calculate stuff after the decimal pt, 100 gives us two
		 * decimal places */
		rest = (perf % MEGA * 100) / MEGA;
		/* Stuff before the decimal pt */
		perf = perf / MEGA;
		snprintf(perf_resp, sizeof(perf_resp), "time = %dms "
		 "transferred = %d perf = %d.%d Mbits/s (%d bits/s)",
			 end_time - start_time, xfer, perf, rest, orig_perf);
	} else {
		snprintf(perf_resp, sizeof(perf_resp), "time = %dms "
			 "transferred = %d perf = %d bits/s",
			 end_time - start_time, xfer, perf);
	}
out1:
	os_mem_free(buf);
out:
	if (mp_header) {
		err = app_consume_http_mp_end(req->sock, buf, PERF_BUF_SIZE);
		if (err != WM_SUCCESS)
			app_e("recv failed for mp header boundary");
		else
			err = httpd_send_response(req, HTTP_RES_200, perf_resp,
				strlen(perf_resp), HTTP_CONTENT_JSON_STR);
	}

	return err;
}


static int sys_get_perf(httpd_request_t *req)
{
	int err;
	char *buf;
	/* Default file length 1 MB */
#define DEFAULT_FILE_LEN  (1 << 10 << 10)
	int file_length = DEFAULT_FILE_LEN;
	char int_string[10];

	err = httpd_purge_headers(req->sock);
	if (err != WM_SUCCESS)
		goto out;

	err = httpd_send_hdr_from_code(req->sock, 0, HTTP_CONTENT_PLAIN_TEXT);
	if (err != WM_SUCCESS)
		goto out;

	err = httpd_send_default_headers(req->sock, req->wsgi->hdr_fields);
	if (err != WM_SUCCESS)
		goto out;

	/* Send Header end */
	err = httpd_send(req->sock, "\r\n", 2);
	if (err != WM_SUCCESS)
		goto out;

	/* Check if the user is looking for a specific size, if so use that.
	 */
	err = httpd_get_tag_from_url(req, "size", int_string,
				     sizeof(int_string));
	if (err != WM_SUCCESS && err != -WM_E_NOENT)
		goto out;

	if (err == WM_SUCCESS) {
		file_length = atoi(int_string);
		if (file_length <= 0 || file_length > (0xffffffff))
			file_length = DEFAULT_FILE_LEN;
	}

	/* We now have the file length to xfer by this time. */
	buf = os_mem_alloc(PERF_BUF_SIZE);
	if (!buf) {
		app_e("Failed to allocate memory");
		err = -WM_E_NOMEM;
		goto out;
	}
	memset(buf, 'a', PERF_BUF_SIZE);

	/* Although in most cases we could safely use httpd_send_chunk(), since
	 * this API will be used to get a sense of the performance, that
	 * function introduces too many calls. Instead, we just send a single
	 * chunk begin followed by the entire chunk followed by the end of chunk
	 * marker. This way, the measurement is closer to reality since we end
	 * up sending only small amount of metadata.
	 */
	httpd_send_chunk_begin(req->sock, file_length);
	int send_size;
	while (file_length > 0) {
		send_size = (file_length > PERF_BUF_SIZE) ?
			PERF_BUF_SIZE : file_length;
		err = httpd_send(req->sock, buf, send_size);
		if (err < 0) {
			goto out1;
		}
		file_length -= send_size;
	}

	/* Send chunk end marker */
	err = httpd_send(req->sock, "\r\n", 2);
	if (err < 0) {
		goto out1;
	}
	/* Send last chunk */
	err = httpd_send_chunk(req->sock, NULL, 0);


out1:
	os_mem_free(buf);
out:
	return err;
}


int sys_webupdate(httpd_request_t *req)
{
	const int size = 512;
	size_t file_length;
	int err = -1, ret, err1;
	uint8_t mp_header = 0;
	char *buf = os_mem_calloc(size);
	if (!buf) {
		app_e("Failed to allocate memory");
		return -WM_E_NOMEM;
	}

	if (!strncmp(req->wsgi->uri, RFGET_HTTP_API_ALLUPDATE,
		strlen(RFGET_HTTP_API_ALLUPDATE))) {
		/* Parse the headers and do httpd_recv for /sys/webupdater */
		ret = httpd_get_data(req, buf, size);
		if (ret < 0) {
			app_e("Failed to get post request data");
			goto out;
		}
	} else {
		/* Request for /sys/firmware or /sys/filesystem
		 * or /sys/wifi-firmware */
		ret = httpd_parse_hdr_tags(req, req->sock, buf,
				size);
		if (ret != WM_SUCCESS) {
			app_e("Failed to parse HTTP headers\r\n");
			goto out;
		}
	}
	/* Special handling for /sys/updater: httpd_recv: 512 bytes? */
	if (!strncmp(app_http_content_type_mp_form, req->content_type,
		     strlen(app_http_content_type_mp_form))) {
		/*
		 * We require that the content length be available and
		 * set correctly. If there was no trailing data after the
		 * firmware image contents, we can detect socket closure
		 * (rfget_recv returns -1) to read just contents correctly.
		 * However, since there is trailing data (boundary delimiter),
		 * we need to know the length so that we can stop reading from
		 * the stream
		 */
		if ((file_length =
		     app_http_get_mp_form_length(req, req->sock, buf,
					size)) == -WM_FAIL) {
			ret = -WM_FAIL;
			goto out;
		}

		/* multipart-header present in request */
		mp_header = 1;
	} else
		file_length = req->body_nbytes;

	app_d("http: update image size: %d", file_length);

	if (!httpd_validate_uri(req->filename,
				RFGET_HTTP_API_FWUPDATE, 0) ||
	    !httpd_validate_uri(req->filename, RFGET_HTTP_API_FWUPDATE2, 0)) {
		struct partition_entry *p = rfget_get_passive_firmware();
		err =
			rfget_server_mode_update(req->sock, file_length, p);
		goto end;
	} else if (!httpd_validate_uri(req->filename,
				       RFGET_HTTP_API_FSUPDATE, 0) ||
		   !httpd_validate_uri(req->filename,
				       RFGET_HTTP_API_FSUPDATE2, 0)) {
		struct partition_entry *p = app_fs_get_passive();
		err = rfget_server_mode_update(req->sock, file_length, p);
		goto end;
#ifdef CONFIG_WIFI_FW_UPGRADE
	} else if (!httpd_validate_uri(req->filename,
				       RFGET_HTTP_API_WIFI_FWUPDATE, 0)) {
		struct partition_entry *p = rfget_get_passive_wifi_firmware();
		if (p == NULL) {
			app_e("Failed to get passive partition");
			ret = -WM_FAIL;
			goto out;
		}
		err =
			rfget_server_mode_update(req->sock, file_length, p);
		goto end;
#endif
	} else if (!httpd_validate_uri(req->filename,
				       RFGET_HTTP_API_ALLUPDATE, 0)) {
		/* Although the above two URIs have no pre-process, that is not
		 * the case with us. The data is already in the in sbuf right
		 * now. */
		json_object_init(&obj, buf);
		err = app_sys_http_update_all(&obj, NULL, NULL, NULL);
		goto end;
	}
end:
	if (mp_header) {
		err1 = app_consume_http_mp_end(req->sock, buf,
				size);
		if (err1 != WM_SUCCESS)
			app_e("recv failed for mp header boundary");
	}

	if (err == WM_SUCCESS)
		err = httpd_send_response(req, HTTP_RES_200, HTTPD_JSON_SUCCESS,
			strlen(HTTPD_JSON_SUCCESS), HTTP_CONTENT_JSON_STR);
	else
		err = httpd_send_response(req, HTTP_RES_200, HTTPD_JSON_ERROR,
			strlen(HTTPD_JSON_ERROR), HTTP_CONTENT_JSON_STR);
out:
	os_mem_free(buf);
	return err;
}

#define HTTP_REBOOT_DELAY   2 /* 2 seconds */
static void app_reboot_main(os_thread_arg_t data)
{
	os_thread_sleep(os_msec_to_ticks(HTTP_REBOOT_DELAY * 1000));
	app_w("app_http: Rebooting SOC...");
	app_reboot(REASON_USER_REBOOT);
	os_thread_self_complete(NULL);
}

int sys_handle_webcmd(struct json_object *obj)
{
	int ret = -1;
	char *tag = json_val;

	if (json_get_val_str(obj, J_NAME_COMMAND, tag, MAX_JSON_VAL)
	    != WM_SUCCESS) {
		/* No command posted to the URI */
		app_d("app_http: command not received");
		ret = -1;
	} else {
		if ((strlen(SYS_HTTP_REBOOT_CMD) == strlen(tag)) &&
		    (!strncmp(tag, SYS_HTTP_REBOOT_CMD,
			      strlen(SYS_HTTP_REBOOT_CMD)))) {
			ret = os_thread_create(&app_reboot_thread,
					       "reboot_thread",
					       app_reboot_main,
					       0,
					       &app_reboot_stack,
					       OS_PRIO_3);
		} else {
			/* Unknown command was posted to the URI */
			app_w("app_http: unknown web command %s", tag);
			ret = -1;
		}
	}
	return ret;

}

int sys_webcmd(httpd_request_t *req)
{
	char sys_cmd_req[50];
	int ret;

	ret = httpd_get_data_json(req, sys_cmd_req, sizeof(sys_cmd_req), &obj);
	if (ret < 0) {
		app_e("Failed to get post request data");
		return ret;
	}

	ret = sys_handle_webcmd(&obj);
	if (ret == WM_SUCCESS)
		ret = httpd_send_response(req, HTTP_RES_200, HTTPD_JSON_SUCCESS,
			strlen(HTTPD_JSON_SUCCESS), HTTP_CONTENT_JSON_STR);
	else
		ret = httpd_send_response(req, HTTP_RES_200, HTTPD_JSON_ERROR,
			strlen(HTTPD_JSON_ERROR), HTTP_CONTENT_JSON_STR);
	return ret;
}

int sys_validate_name(char *name)
{
	int i = 0, len, num = 0;

	len = strlen(name);

	if (len > 63)
		return 1;

	if ((name[0] == '-') || (name[len-1] == '-'))
		return 1;

	for (i = 0; i < len; i++) {
		if ((name[i] != '-') && (!isalnum(name[i])))
			return 1;

		if (isdigit(name[i]))
			num++;
	}

	if (num == len)
		return 1;

	return 0;
}

static int sys_do_reset_prov(int mode)
{
	if ((app_ctrl_getmode() && mode == 1) ||
	    (!(app_ctrl_getmode()) && (mode == 0))) {
		/* If normal mode and mode value is given as 1 then do
		   nothing
		   OR
		   If provisioning mode and mode value is given as 0
		   then do nothing */
		return 0;
	} else if (mode == 0) {
		/* If normal mode and mode value is given as 0 then
		 * switch to provisioning mode and set it to 0*/
		app_ctrl_notify_event(AF_EVT_NORMAL_RESET_PROV, NULL);
		return 0;
	}
	return 0;
}

int sys_set_info(httpd_request_t *req)
{
	char sys_info_req[50];
	int reset, ack, ret;
	int handle_client_ack = 0, handle_configure = 0;
	ret = httpd_get_data_json(req, sys_info_req,
			sizeof(sys_info_req), &obj);
	/* check condition */
	if (ret < 0) {
		app_e("Failed to get post request data");
		return ret;
	}
	if (json_get_composite_object(&obj,
			    J_NAME_PROVISIONING) == WM_SUCCESS)	{
		if (json_get_val_int(&obj, J_NAME_CLIENT_ACK,
				     &ack) == WM_SUCCESS) {
			handle_client_ack = 1;
		}
		json_release_composite_object(&obj);
	}
	if (json_get_composite_object(&obj, J_NAME_CONNECTION) == WM_SUCCESS) {
		if (json_get_composite_object(&obj, J_NAME_STA) == WM_SUCCESS) {
			if (json_get_val_int(&obj, "configured",
					     &reset) == WM_SUCCESS) {
				handle_configure = 1;
			}
			json_release_composite_object(&obj);
		}
		json_release_composite_object(&obj);
	}

	ret = httpd_send_response(req, HTTP_RES_200, HTTPD_JSON_SUCCESS,
		strlen(HTTPD_JSON_SUCCESS), HTTP_CONTENT_JSON_STR);
	/* Delay added to send response to POST client_ack = 1 */
	os_thread_sleep(os_msec_to_ticks(1000));
	if (handle_client_ack)
		app_ctrl_notify_event(AF_EVT_PROV_CLIENT_DONE, NULL);
	if (handle_configure) {
		os_thread_sleep(os_msec_to_ticks(1000));
		sys_do_reset_prov(reset);
	}

	return ret;
}


int sys_handle_set_time(struct json_object *jobj)
{
	time_t time;
	int ret = -1;

	if (json_get_val_int(jobj, J_NAME_EPOCH,
			     (int *)&time) != WM_SUCCESS)
		return -1;
	else
		ret = wmtime_time_set_posix(time);
	return ret;
}

int sys_set_time(httpd_request_t *req)
{
	char sys_time_req[30];
	int ret;

	ret = httpd_get_data_json(req, sys_time_req,
			sizeof(sys_time_req), &obj);
	if (ret < 0) {
		app_e("Failed to get post request data");
		return ret;
	}
	ret = sys_handle_set_time(&obj);
	if (ret == WM_SUCCESS)
		ret = httpd_send_response(req, HTTP_RES_200, HTTPD_JSON_SUCCESS,
			strlen(HTTPD_JSON_SUCCESS), HTTP_CONTENT_JSON_STR);
	else
		ret = httpd_send_response(req, HTTP_RES_200, HTTPD_JSON_ERROR,
			strlen(HTTPD_JSON_ERROR), HTTP_CONTENT_JSON_STR);
	return ret;
}


int sys_set_mode(httpd_request_t *req)
{
	int mode;
	char sys_mode_req[20];
	int ret, err;

	ret = httpd_get_data_json(req, sys_mode_req,
			sizeof(sys_mode_req), &obj);
	if (ret < 0) {
		app_e("Failed to get post request data");
		return ret;
	}


	err = json_get_val_int(&obj, J_NAME_MODE, &mode);
	if (err != WM_SUCCESS) {
		ret = httpd_send_response(req, HTTP_RES_200, HTTPD_JSON_ERROR,
			strlen(HTTPD_JSON_ERROR), HTTP_CONTENT_JSON_STR);
	} else {
		ret = httpd_send_response(req, HTTP_RES_200, HTTPD_JSON_SUCCESS,
			strlen(HTTPD_JSON_SUCCESS), HTTP_CONTENT_JSON_STR);
		/* Wait for the response to be sent */
		os_thread_sleep(os_msec_to_ticks(1000));
		sys_do_reset_prov(mode);
	}

	return ret;
}


int app_sys_network_set_cb(struct wlan_network *network)
{
	app_d("network_set_cb called");
	app_ctrl_notify_event(AF_EVT_NORMAL_ADVANCED_SETTINGS, network);
	return 0;
}

static int sys_anchor_start(struct json_str *jptr, char *buf, int len)
{
	json_str_init(jptr, buf, len, 0);
	json_start_object(jptr);
	return 0;
}

static int sys_anchor_end(struct json_str *jptr)
{
	json_close_object(jptr);
	return 0;
}

static int sys_get_services(httpd_request_t *req)
{
	const int size = 512;
	int i, mode, ret;
	char *content = os_mem_calloc(size);

	if (!content) {
		app_e("Failed to allocate memory\r\n");
		return -WM_E_NOMEM;
	}
	sys_anchor_start(&jstr, content, size);

	json_push_object(&jstr, "httpd_handlers");

	mode = app_ctrl_getmode();
	if (!mode) {
		json_push_array_object(&jstr, "/sys/scan");
		json_set_array_int(&jstr, 1);
		json_set_array_int(&jstr, 0);
		json_pop_array_object(&jstr);

		json_push_array_object(&jstr, "/sys/scan-config");
		json_set_array_int(&jstr, 1);
		json_set_array_int(&jstr, 1);
		json_pop_array_object(&jstr);
	}

	json_push_array_object(&jstr, "/sys/network");
	json_set_array_int(&jstr, 1);
	json_set_array_int(&jstr, 1);
	json_pop_array_object(&jstr);

	for (i = 0; i < g_app_sys_handlers_no; i++) {

		if (g_sys_handlers[i].uri == NULL)
			continue;

		json_push_array_object(&jstr,
				       &g_sys_handlers[i].uri
				       [0]);
		if (g_sys_handlers[i].get_handler)
			json_set_array_int(&jstr, 1);
		else
			json_set_array_int(&jstr, 0);
		/** Hackish: In provisioning mode, POST operation on /sys/mode
		 *  is not allowed
		 */
		if (strcmp(g_sys_handlers[i].uri, "/sys/mode")
		    == 0) {
			if (app_ctrl_getmode() == 0)
				json_set_array_int(&jstr, 0);
			else
				json_set_array_int(&jstr, 1);
		} else {
			if (g_sys_handlers[i].set_handler)
				json_set_array_int(&jstr, 1);
			else
				json_set_array_int(&jstr, 0);
		}
		json_pop_array_object(&jstr);
	}

	json_pop_object(&jstr);
	sys_anchor_end(&jstr);
	ret = httpd_send_response(req, HTTP_RES_200, content, strlen(content),
				HTTP_CONTENT_JSON_STR);
	os_mem_free(content);
	return ret;
}

static int sys_get_mode(httpd_request_t *req)
{
	int mode;
	char content[20];
	mode = app_ctrl_getmode();

	sys_anchor_start(&jstr, content, sizeof(content));
	json_set_val_int(&jstr, J_NAME_MODE, mode);
	app_d("sending modestr %s", mode ? "NORMAL" : "PROVISIONING");

	/* Make a note of the http access time. In case the user opens up a browser
	 * session, this is used to extend the timed provisioning further and give
	 * the users some time.
	 */
	app_http_access_time = os_ticks_get();
	app_d("updating last access time %lu", app_http_access_time);

	sys_anchor_end(&jstr);
	return httpd_send_response(req, HTTP_RES_200, content, strlen(content),
				HTTP_CONTENT_JSON_STR);
}

static void sys_http_get_interface(struct json_str *jstr, httpd_request_t * req)
{
	if (net_sock_to_interface(req->sock) == net_get_mlan_handle()) {
		json_set_val_str(jstr, J_NAME_INTERFACE, STR_STATION);
	} else {
		json_set_val_str(jstr, J_NAME_INTERFACE, STR_UAP);
	}
}

static int sys_get_interface(httpd_request_t *req)
{
	char content[30];

	sys_anchor_start(&jstr, content, sizeof(content));
	sys_http_get_interface(&jstr, req);
	sys_anchor_end(&jstr);
	return httpd_send_response(req, HTTP_RES_200, content, strlen(content),
			HTTP_CONTENT_JSON_STR);
}

static int sys_get_conn_state(httpd_request_t *req)
{
	int ret;
	const int size = 100;
	char *content = os_mem_calloc(size);
	if (!content) {
		app_e("Failed to allocate memory\r\n");
		return -WM_E_NOMEM;
	}

	sys_anchor_start(&jstr, content, size);
	json_set_val_int(&jstr, J_NAME_AUTH_FAILED, conn_state.auth_failed);
	json_set_val_int(&jstr, J_NAME_CONN_FAILED, conn_state.conn_failed);
	json_set_val_int(&jstr, J_NAME_DHCP_FAILED, conn_state.dhcp_failed);
	json_set_val_int(&jstr, J_NAME_CONN_SUCCESS, conn_state.conn_success);
	sys_anchor_end(&jstr);
	ret = httpd_send_response(req, HTTP_RES_200, content, strlen(content),
			HTTP_CONTENT_JSON_STR);
	os_mem_free(content);
	return ret;
}

void sys_handle_get_time(struct json_str *jptr)
{
	time_t time;
	time = wmtime_time_get_posix();
	json_set_val_int(&jstr, J_NAME_EPOCH, time);
}

int sys_get_time(httpd_request_t *req)
{
	char content[30];
	sys_anchor_start(&jstr, content, sizeof(content));
	sys_handle_get_time(&jstr);
	sys_anchor_end(&jstr);
	return httpd_send_response(req, HTTP_RES_200, content,
		strlen(content), HTTP_CONTENT_JSON_STR);
}

void app_sys_get_uuid(char *output, int len)
{
	mdev_t *dev;
	uint64_t id;
	uint8_t mymac[6];
	wlan_get_mac_address(mymac);
	dev = flash_drv_open(FL_INT);
	flash_drv_get_id(dev, &id);
	flash_drv_close(dev);
	snprintf(output, len, "%016llx%02x%02x%02x%02x%02x%02x", id, mymac[0],
		 mymac[1], mymac[2], mymac[3], mymac[4], mymac[5]);
}

void app_sys_http_get_info(struct json_str *jptr)
{
	char tempstr[100] = { 0, };
	app_sys_get_uuid(tempstr, sizeof(tempstr));
	json_set_val_str(jptr, J_NAME_UUID, tempstr);
}

static int sys_get_info(httpd_request_t *req)
{
	const int size = 512;
	int ret;
	char *content = os_mem_calloc(size);

	if (!content) {
		app_e("Failed to allocate memory\r\n");
		return -WM_E_NOMEM;
	}

	sys_anchor_start(&jstr, content, size);

	app_sys_http_get_info(&jstr);
	sys_http_get_interface(&jstr, req);

	int mode = app_provisioning_get_type();
	json_push_object(&jstr, J_NAME_PROVISIONING);
	json_push_array_object(&jstr, J_NAME_PROV_TYPES);
	if (mode & PROVISIONING_WLANNW)
		json_set_array_str(&jstr, "marvell");
	if (mode & PROVISIONING_WPS)
		json_set_array_str(&jstr, "wps");
	json_pop_array_object(&jstr); /* J_NAME_PROV_TYPES */
	json_pop_object(&jstr); /* J_NAME_PROVISIONING */

	mode = app_ctrl_getmode();
	json_push_object(&jstr, J_NAME_CONNECTION);
	json_push_object(&jstr, J_NAME_STA);
	/* Adding MAC address */
	uint8_t my_mac[MAX_MAC_BYTES];
	char mac[MAX_MAC_STRLEN];
	ret = wlan_get_mac_address(my_mac);
	snprintf(mac, sizeof(mac), "%02X-%02X-%02X-%02X-%02X-%02X",
		 my_mac[0], my_mac[1], my_mac[2], my_mac[3],
		 my_mac[4], my_mac[5]);
	json_set_val_str(&jstr, J_NAME_MAC, mac);

	json_set_val_int(&jstr, J_NAME_CONFIGURED, mode);
	json_set_val_int(&jstr, J_NAME_STATUS, conn_state.state);
#define AUTH_FAILED "auth_failed"
#define DHCP_FAILED "dhcp_failed"
#define OTHER       "other"
	if (mode == 1 && conn_state.state != CONN_STATE_CONNECTED) {
		if (conn_state.auth_failed) {
			json_set_val_str(&jstr, J_NAME_FAILURE, AUTH_FAILED);
			json_set_val_int(&jstr, J_NAME_FAILURE_CNT,
					 conn_state.auth_failed);
		}
		if (conn_state.dhcp_failed) {
			json_set_val_str(&jstr, J_NAME_FAILURE, DHCP_FAILED);
			json_set_val_int(&jstr, J_NAME_FAILURE_CNT,
					 conn_state.dhcp_failed);
		}
		if (conn_state.conn_failed) {
			json_set_val_str(&jstr, J_NAME_FAILURE, OTHER);
			json_set_val_int(&jstr, J_NAME_FAILURE_CNT,
					 conn_state.conn_failed);
		}
	}
	struct wlan_network network;
	struct in_addr ip;
	char tempstr[50] = { 0, };

	if (is_sta_connected()) {
		if (wlan_get_current_network(&network))
			goto sta_err;

		short rssi;
		wlan_get_current_rssi(&rssi);
		json_set_val_str(&jstr, J_NAME_SSID, network.ssid);

		snprintf(tempstr,
			 sizeof(tempstr), "%.2x:%.2x:%.2x:%.2x:%.2x:%.2x",
			 network.bssid[0], network.bssid[1], network.bssid[2],
			 network.bssid[3], network.bssid[4], network.bssid[5]);

		json_set_val_str(&jstr, J_NAME_BSSID, tempstr);
		json_set_val_int(&jstr, J_NAME_CHANNEL, network.channel);
		json_set_val_int(&jstr, J_NAME_SECURITY, network.security.type);
		json_set_val_int
			(&jstr, J_NAME_IPTYPE, network.address.addr_type);

		json_set_val_int(&jstr, J_NAME_RSSI, rssi);
		if (network.address.addr_type == ADDR_TYPE_STATIC) {
			ip.s_addr = network.address.ip;
			json_set_val_str(&jstr, J_NAME_IP, inet_ntoa(ip));
			ip.s_addr = network.address.netmask;
			json_set_val_str(&jstr, J_NAME_NETMASK, inet_ntoa(ip));
			ip.s_addr = network.address.gw;
			json_set_val_str(&jstr, J_NAME_GW, inet_ntoa(ip));
			ip.s_addr = network.address.dns1;
			json_set_val_str(&jstr, J_NAME_DNS1, inet_ntoa(ip));
			ip.s_addr = network.address.dns2;
			json_set_val_str(&jstr, J_NAME_DNS2, inet_ntoa(ip));
		}
	}
 sta_err:
	json_pop_object(&jstr); /* J_NAME_STA */
	if (is_uap_started()) {
		if (wlan_get_current_uap_network(&network))
			goto uap_err;

		json_push_object(&jstr, J_NAME_UAP);
		/* Adding MAC address */
		json_set_val_str(&jstr, J_NAME_MAC, mac);

		json_set_val_str(&jstr, J_NAME_SSID, network.ssid);

		snprintf(tempstr,
			 sizeof(tempstr), "%.2x:%.2x:%.2x:%.2x:%.2x:%.2x",
			 network.bssid[0], network.bssid[1], network.bssid[2],
			 network.bssid[3], network.bssid[4], network.bssid[5]);

		json_set_val_str(&jstr, J_NAME_BSSID, tempstr);
		json_set_val_int(&jstr, J_NAME_CHANNEL, network.channel);
		json_set_val_int
			(&jstr, J_NAME_SECURITY, network.security.type);
		json_set_val_int
			(&jstr, J_NAME_IPTYPE, network.address.addr_type);

		json_pop_object(&jstr); /* J_NAME_UAP */
	}

 uap_err:
	json_pop_object(&jstr); /* J_NAME_CONNECTION */
	sys_anchor_end(&jstr);
	ret = httpd_send_response(req, HTTP_RES_200, content, strlen(content),
				HTTP_CONTENT_JSON_STR);
	os_mem_free(content);
	return ret;
}

int sys_get_diag_stats_live(httpd_request_t *req)
{
	const int size = 512;
	int ret;
	char *content = os_mem_calloc(size);

	if (!content) {
		app_e("Failed to allocate memory\r\n");
		return -WM_E_NOMEM;
	}
	sys_anchor_start(&jstr, content, size);
	ret = sys_handle_get_diag_stats_live(&jstr);
	sys_anchor_end(&jstr);
	ret = httpd_send_response(req, HTTP_RES_200, content, strlen(content),
				HTTP_CONTENT_JSON_STR);
	os_mem_free(content);
	return ret;
}

static void generate_json_sta_list(struct json_str *jstr)
{
	sta_list_t *sl;
	wifi_uap_bss_sta_list(&sl);
	if (!sl) {
		app_w("Failed to get sta list");
		return;
	}

	sta_info_t *si =  (sta_info_t *)(&sl->count + 1);

	json_push_array_object(jstr, J_NAME_CONN_LIST);

	int i;
	for (i = 0; i < sl->count; i++) {
		char mac_address[20];
		json_start_array(jstr);
		sprintf(mac_address, "%02X:%02X:%02X:%02X:%02X:%02X",
			si[i].mac[0], si[i].mac[1], si[i].mac[2],
			si[i].mac[3], si[i].mac[4], si[i].mac[5]);
		json_set_array_str(jstr, mac_address);
		json_set_array_int(jstr, si[i].rssi);
		json_close_array(jstr);
	}
	json_pop_array_object(jstr);

	os_mem_free(sl);
}

int sys_get_diag_info(httpd_request_t *req)
{
	int temp, mode;
	const int size = 512;
	short rssi;
	const char *arch_str = SYS_ARCH_STR;
	int ret;
	struct wlan_network network, uap_network;
	enum wlan_connection_state state;
	char *content = os_mem_calloc(size);

	if (!content) {
		app_e("Failed to allocate memory\r\n");
		return -WM_E_NOMEM;
	}
	sys_anchor_start(&jstr, content, size);

	temp = net_get_tx();
	json_set_val_int(&jstr, J_NAME_STATS_XMIT, temp);
	temp = net_get_rx();
	json_set_val_int(&jstr, J_NAME_STATS_RECV, temp);
	temp = net_get_error();
	json_set_val_int(&jstr, J_NAME_STATS_NET_ERROR, temp);
	json_set_val_str(&jstr, J_NAME_SDK_VER, SDK_VERSION);
	if (is_uap_started()) {
		wlan_get_current_uap_network(&uap_network);
		json_set_val_str(&jstr, J_NAME_UAP_SSID, uap_network.ssid);
	}
	mode = app_ctrl_getmode();
	if (mode) {
		wlan_get_current_rssi(&rssi);
		json_set_val_int(&jstr, J_NAME_STATS_RSSI, rssi);
		ret = wlan_get_connection_state(&state);
		if (ret == WLAN_ERROR_NONE && state == WLAN_CONNECTED) {
			wlan_get_current_network(&network);
			json_set_val_str(&jstr, J_NAME_SSID, network.ssid);
			json_set_val_str(&jstr, J_NAME_STA_CONN_STATUS,
					 "connected");
		} else {
			wlan_get_network_byname("default", &network);
			json_set_val_str(&jstr, J_NAME_SSID, network.ssid);
			json_set_val_str(&jstr, J_NAME_STA_CONN_STATUS,
					 "connecting");
		}
		json_set_val_str(&jstr, J_NAME_IP,
					 inet_ntoa(network.address.ip));
	} else {
		generate_json_sta_list(&jstr);
	}

	char version_str[MLAN_MAX_VER_STR_LEN];
	wlan_get_firmware_version_ext(version_str);
	json_set_val_str(&jstr, J_NAME_STATS_VER,
			version_str);
	temp = os_free_heap_percentage();
	json_set_val_int(&jstr, J_NAME_STATS_HEAP_USG, temp);
	temp = board_cpu_freq() / (1000 * 1000); /* MHz */
	json_set_val_int(&jstr, J_NAME_STATS_FREQ, temp);

	json_set_val_str(&jstr, J_NAME_ARCH, arch_str);
	if (net_sock_to_interface(req->sock) == net_get_mlan_handle()) {
		json_set_val_str(&jstr, J_NAME_MODE, STR_STATION);
		json_set_val_int(&jstr, J_NAME_STATS_LLOST,
			g_wm_stats.wm_lloss);
	} else {
		json_set_val_str(&jstr, J_NAME_MODE, STR_UAP);
		json_set_val_int(&jstr, J_NAME_STATS_DHCP_REQ,
			g_wm_stats.wm_dhcp_req_cnt);
	}
	sys_anchor_end(&jstr);
	ret = httpd_send_response(req, HTTP_RES_200, content, strlen(content),
				HTTP_CONTENT_JSON_STR);
	os_mem_free(content);

	return ret;
}

int sys_get_diag_stats_history(httpd_request_t *req)
{
	int ret;
	const int size = 512;
	char *content = os_mem_calloc(size);
	if (!content) {
		app_e("Failed to allocate memory\r\n");
		return -WM_E_NOMEM;
	}

	sys_anchor_start(&jstr, content, size);
	ret = sys_handle_get_diag_stats_history(&jstr);
	sys_anchor_end(&jstr);
	ret = httpd_send_response(req, HTTP_RES_200, content, strlen(content),
				HTTP_CONTENT_JSON_STR);
	os_mem_free(content);
	return ret;
}
int app_file_handler(httpd_request_t *req)
{
	return httpd_handle_file(req, app_fs_get());
}

static struct httpd_wsgi_call app_file_handler_wsgi = { "/",
	HTTPD_DEFAULT_HDR_FLAGS, APP_HTTP_FLAGS_NO_EXACT_MATCH,
	app_file_handler, NULL, NULL, NULL};

int app_sys_register_file_handler()
{
	int rc;

	if (app_fs_get() == NULL)
		return -WM_FAIL;

	rc = httpd_register_wsgi_handler(&app_file_handler_wsgi);
	if (rc) {
		app_e("app_http: failed to register httpd file"
			" handler");
	}
	return rc;
}

int app_sys_register_sys_handler()
{
	int rc = 0;
	rc = httpd_register_wsgi_handlers(g_sys_handlers,
			g_app_sys_handlers_no);
	if (rc)
		app_e("Failed to register app sys httpd handlers");
	return rc;
}

int app_sys_normal_set_network_cb()
{
	int rc;

	app_d("Registering normal mode network_set_cb");
	rc = set_network_cb_register(app_sys_network_set_cb);
	if (rc) {
		app_e("app_http: failed to register network settings"
			" callback function");
		return rc;
	}
	return 0;
}

static inline int app_sys_get_empty_handler(void)
{
	int i;
	for (i = 0; i < g_app_sys_handlers_no; i++) {
		if (g_sys_handlers[i].uri == NULL)
			return i;
	}
	return -1;
}

static inline int sys_set_prov_status(httpd_request_t *req)
{
	int status, ret;
	char sys_prov_st_req[20];

	ret = httpd_get_data_json(req, sys_prov_st_req,
			sizeof(sys_prov_st_req), &obj);
	/* check condition */
	if (ret < 0) {
		app_e("Failed to get post request data");
		return ret;
	}

	if (json_get_val_int(&obj, J_NAME_PROV_FINISH, &status) != WM_SUCCESS) {
		return -WM_FAIL;
	}

	if (status == 1) {
		app_ctrl_notify_event(AF_EVT_PROV_CLIENT_DONE, NULL);
		ret = httpd_send_response(req, HTTP_RES_200, HTTPD_JSON_SUCCESS,
			strlen(HTTPD_JSON_SUCCESS), HTTP_CONTENT_JSON_STR);
	} else {
		ret = httpd_send_response(req, HTTP_RES_200, HTTPD_JSON_ERROR,
			strlen(HTTPD_JSON_ERROR), HTTP_CONTENT_JSON_STR);
	}
	return ret;
}

static inline int app_sys_install_handler(char *w, int hdr_fields, int flags,
					 int (*g)(httpd_request_t *req),
					 int (*s)(httpd_request_t *req),
					 int (*p)(),
					 int (*d)())

{
	int i = app_sys_get_empty_handler();
	if (i == -1)
		return -1;

	g_sys_handlers[i].uri = w;
	g_sys_handlers[i].hdr_fields = hdr_fields;
	g_sys_handlers[i].http_flags = flags;
	g_sys_handlers[i].get_handler = g;
	g_sys_handlers[i].set_handler = s;
	g_sys_handlers[i].put_handler = p;
	g_sys_handlers[i].delete_handler = d;

	return WM_SUCCESS;
}


int app_sys_register_upgrade_handler()
{
	app_sys_install_handler("/sys/updater", HTTPD_DEFAULT_HDR_FLAGS, 0,
			NULL, sys_webupdate, NULL, NULL);
	app_sys_install_handler("/sys/filesystem", HTTPD_DEFAULT_HDR_FLAGS,
			0, NULL, sys_webupdate, NULL, NULL);
	app_sys_install_handler("/sys/firmware", HTTPD_DEFAULT_HDR_FLAGS,
			0, NULL, sys_webupdate, NULL, NULL);
#ifdef CONFIG_WIFI_FW_UPGRADE
	app_sys_install_handler("/sys/wifi-firmware", HTTPD_DEFAULT_HDR_FLAGS,
			0, NULL, sys_webupdate, NULL, NULL);
#endif
	return WM_SUCCESS;
}

int app_sys_register_diag_handler()
{
	app_sys_install_handler("/sys/diag/stats/live",
			HTTPD_DEFAULT_HDR_FLAGS, 0,
			sys_get_diag_stats_live, NULL, NULL, NULL);
	app_sys_install_handler("/sys/diag/stats/history",
			HTTPD_DEFAULT_HDR_FLAGS, 0,
			sys_get_diag_stats_history, NULL, NULL, NULL);
	app_sys_install_handler("/sys/diag/info",
			HTTPD_DEFAULT_HDR_FLAGS, 0,
			sys_get_diag_info, NULL, NULL, NULL);

	return WM_SUCCESS;
}

int app_sys_register_perf_handler()
{
	app_sys_install_handler("/sys/perf", HTTPD_DEFAULT_HDR_FLAGS,
				0, sys_get_perf, sys_post_perf, NULL, NULL);
	return WM_SUCCESS;
}

unsigned long app_http_get_last_access_time()
{
	return app_http_access_time;
}

struct httpd_wsgi_call g_sys_handlers[] = {
	{"/sys/time", HTTPD_DEFAULT_HDR_FLAGS, 0,
		sys_get_time, sys_set_time, NULL, NULL},
	{"/sys/command", HTTPD_DEFAULT_HDR_FLAGS, 0,
			NULL, sys_webcmd, NULL, NULL},
	{"/sys/connection", HTTPD_DEFAULT_HDR_FLAGS, 0,
			sys_get_conn_state, NULL, NULL, NULL},
	{"/sys/services", HTTPD_DEFAULT_HDR_FLAGS, 0,
			sys_get_services, NULL, NULL, NULL},
	{"/sys/prov_status", HTTPD_DEFAULT_HDR_FLAGS, 0, NULL,
			sys_set_prov_status, NULL, NULL},
	{"/sys/interface", HTTPD_DEFAULT_HDR_FLAGS, 0,
			sys_get_interface, NULL, NULL, NULL},
	{"/sys", HTTPD_DEFAULT_HDR_FLAGS, 0, sys_get_info,
			sys_set_info, NULL, NULL},
	{"/sys/mode", HTTPD_DEFAULT_HDR_FLAGS, 0, sys_get_mode,
			sys_set_mode, NULL, NULL},
	{NULL, },
	{NULL, },
	{NULL, },
	{NULL, },
	{NULL, },
	{NULL, },
	{NULL, },
	{NULL, },
};

int g_app_sys_handlers_no = sizeof(g_sys_handlers) /
	sizeof(struct httpd_wsgi_call);
