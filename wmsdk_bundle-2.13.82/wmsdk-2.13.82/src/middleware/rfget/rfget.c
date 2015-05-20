/* 
 *  Copyright 2008-2013, Marvell International Ltd.
 *  All Rights Reserved.
 */

/* rfget.c: The rfget API
 */

#include <string.h>

#include <cli.h>
#include <cli_utils.h>
#include <wmstdio.h>
#include <wm_os.h>
#include <wm_net.h>
#include <rfget.h>
#include <flash.h>
#include <crc32.h>
#include <httpc.h>
#include <httpd.h>
#include <ftfs.h>
#include <partition.h>
#include <arch/boot_flags.h>

#include "firmware.h"

static bool update_in_progress;

#define MAX_DOWNLOAD_DATA 32

static char rfget_init_done;

static void signal_update_start()
{
	update_in_progress = true;
}

static void signal_update_end()
{
	update_in_progress = false;
}

bool rfget_is_update_in_progress()
{
	return update_in_progress;
}

#define RFGET_PARSE_MAX_URL_SIZE 512
static int rfget_validate_url(const char *url_str)
{
	unsigned parse_buf_needed_size = strlen(url_str) + 10;

	if (parse_buf_needed_size >= RFGET_PARSE_MAX_URL_SIZE) {
		rf_e("URL size greater than allowed");
		return -WM_FAIL;
	}

	char *tmp_buf = os_mem_alloc(parse_buf_needed_size);
	if (!tmp_buf) {
		rf_e("Mem allocation failed. Tried size: %d",
			      parse_buf_needed_size);
		return -WM_E_NOMEM;
	}

	parsed_url_t parsed_url;
	int status = http_parse_URL(url_str, tmp_buf, parse_buf_needed_size,
				&parsed_url);
	if (status != WM_SUCCESS) {
		os_mem_free(tmp_buf);
		rf_e("URL parse failed");
		return status;
	}

	if (!strcmp(parsed_url.scheme, "https") || parsed_url.portno == 443) {
		os_mem_free(tmp_buf);
		rf_e("HTTPS URLs are currently not supported for rfprint");
		return -WM_FAIL;
	}

	os_mem_free(tmp_buf);
	return WM_SUCCESS;
}


static int request_http_get(const char *url_str, http_session_t * handle,
			    http_resp_t ** http_resp)
{
	int status;

	rf_d("Connecting to %s", url_str);

	status = rfget_validate_url(url_str);
	if (status != WM_SUCCESS)
		return status;

	status = http_open_session(handle, url_str, 0, NULL, 0);
	if (status != WM_SUCCESS) {
		rf_e("Open session failed URL: %s", url_str);
		goto http_session_open_fail;
	}

	http_req_t req;
	req.type = HTTP_GET;
	req.resource = url_str;
	req.version = HTTP_VER_1_1;

	status = http_prepare_req(*handle, &req, STANDARD_HDR_FLAGS);
	if (status != WM_SUCCESS) {
		rf_e("preparing request failed: %s\n\r", url_str);
		http_close_session(handle);
		return status;
	}

	status = http_send_request(*handle, &req);
	if (status != WM_SUCCESS) {
		rf_e("Request send failed: URL: %s", url_str);
		goto http_session_error;
	}

	http_resp_t *resp;
	status = http_get_response_hdr(*handle, &resp);
	if (status != WM_SUCCESS) {
		rf_e("Unable to get response header: %d", status);
		goto http_session_error;
	}
	if (resp->status_code == HTTP_BAD_REQUEST) {
		rf_e("HTTP Bad Request. HTTP Status code: %d",
		     resp->status_code);
		status = -WM_E_HTTPC_BAD_REQUEST;
		goto http_session_error;
	} else if (resp->status_code == HTTP_NOT_FOUND) {
		rf_e("HTTP File Not Found. HTTP status code: %d",
			      resp->status_code);
		status = -WM_E_HTTPC_FILE_NOT_FOUND;
		goto http_session_error;
	} else if (resp->status_code != HTTP_OK) {
		rf_e("Unexpected HTTP status code from server: %d",
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

static void _dump_resource(const char *url_str)
{
	int status, i, dsize;
	http_session_t handle;
	char buf[MAX_DOWNLOAD_DATA];

	status = request_http_get(url_str, &handle, NULL);
	if (status != 0) {
		rf_e("Unable to print resource");
		return;
	}

	rf_d("Resource dump begins");
	while (1) {
		dsize = http_read_content(handle, buf, MAX_DOWNLOAD_DATA);
		if (dsize < 0) {
			rf_e("Unable to read data on http session: %d",
				      dsize);
			break;
		}
		if (dsize == 0) {
			rf_d("********* All data read **********");
			break;
		}

		for (i = 0; i < dsize; i++) {
			if (buf[i] == '\r')
				continue;
			if (buf[i] == '\n') {
				wmprintf("\n\r");
				continue;
			}
			wmprintf("%c", buf[i]);
		}
	}

	http_close_session(&handle);
}

/**
 * This is called by the firmware/fs update function, to get next blocks of
 * data of a update file.
 * 
 * @param[in] priv Same as that was passed to the caller.
 * @param[in,out] buf Pointer to allocated buffer. Will be filled by callee
 * with the update data.
 * @param[in] max_len Maximum length of data to to be filled in buf
 *
 * @return On success returns the size of data returned in buf. On error,
 * returns -WM_FAIL.
 */
static size_t server_mode_data_fetch_cb(void *priv, void *buf, size_t max_len)
{
	int sockfd = (int)priv;
	rf_d("Server callback: Trying to read %d bytes from stream",
		max_len);
	int size_read = httpd_recv(sockfd, buf, max_len, 0);
	if (size_read < 0) {
		rf_d("Server mode: Could not read data from stream");
		return -WM_FAIL;
	}

	return size_read;
}

/**
 * This is called by the firmware/fs update function, to get next blocks of
 * data of a update file.
 * 
 * @param[in] priv Same as that was passed to the caller.
 * @param[in,out] buf Pointer to allocated buffer. Will be filled by callee
 * with the update data.
 * @param[in] max_len Maximum length of data to to be filled in buf
 *
 * @return On success returns the size of data returned in buf. If the size
 * returned is zero the caller should assume that all data is read. On error,
 * returns -WM_FAIL.
 */
static size_t client_mode_data_fetch_cb(void *priv, void *buf, size_t max_len)
{
	http_session_t handle = (http_session_t) priv;
	rf_d("Client callback: Trying to read %d bytes from stream",
		max_len);
	return http_read_content(handle, buf, max_len);
}

static int _update_system_image(data_fetch data_fetch_cb,
				void *priv, size_t firmware_length,
				struct partition_entry *passive)
{
	int status = -WM_FAIL;
	signal_update_start();

	if (passive->type == FC_COMP_FTFS) {
		status = write_ftfs_image(data_fetch_cb, priv, firmware_length,
					  passive);
		if (status != WM_SUCCESS) {
			rf_e("FS update failed");
		} else {
			rf_d("FS update succeeded");
		}
	} else if (passive->type == FC_COMP_FW) {
		status = update_firmware(data_fetch_cb, priv, firmware_length,
					 passive);
		if (status != WM_SUCCESS) {
			rf_e("Firmware update failed");
		} else {
			rf_d("Firmware update succeeded");
		}
#ifdef CONFIG_WIFI_FW_UPGRADE
	} else if (passive->type == FC_COMP_WLAN_FW) {
		status = update_wifi_firmware(data_fetch_cb, priv,
			firmware_length, passive);
		if (status != WM_SUCCESS) {
			rf_e("WiFi Firmware update failed");
		} else {
			rf_d("WiFi Firmware update succeeded");
		}
#endif
	}

	signal_update_end();
	return status;
}

/*
 * We have two seperate functions for client mode and server mode update
 * because in server mode the socket is already opened and we just have to
 * read 'filelength' number of bytes from the stream. In client mode we
 * have to handle HTTP protcol with help from HTTP client library.
 */
int rfget_server_mode_update(int sockfd, size_t filelength,
			     struct partition_entry *passive)
{
	int status;

	if (passive->type == FC_COMP_FTFS) {
		rf_d("Server mode update filesystem");
	} else if (passive->type == FC_COMP_FW) {
		rf_d("Server mode update firmware");
#ifdef CONFIG_WIFI_FW_UPGRADE
	} else if (passive->type == FC_COMP_WLAN_FW) {
		rf_d("Server mode update wifi firmware");
#endif
	} else {
		rf_e("%s: Unknown type: %d", __func__, passive->type);
		return -WM_FAIL;
	}
	rfget_init();
	status = _update_system_image(server_mode_data_fetch_cb,
				      (void *)sockfd, filelength,
		passive);

	if (status != WM_SUCCESS)
		return -WM_E_HTTPD_HANDLER_400;
	else
		return status;
}

int rfget_client_mode_update(const char *url_str,
			     struct partition_entry *passive)
{
	int status;
	http_session_t handle;
	http_resp_t *resp = NULL;

	if (!passive)
		return -WM_FAIL;

	rfget_init();
	status = request_http_get(url_str, &handle, &resp);
	if (status != 0) {
		rf_e("Unable to connect to server");
		return status;
	}

	if (resp->chunked) {
		rf_e("HTTP chunked fs update is not supported");
		goto skip_update;
	}

	if (passive->type == FC_COMP_FTFS) {
		rf_d("Client mode update filesystem: %s", url_str);
	} else if (passive->type == FC_COMP_FW) {
		rf_d("Client mode update firmware: %s", url_str);
#ifdef CONFIG_WIFI_FW_UPGRADE
	} else if (passive->type == FC_COMP_WLAN_FW) {
		rf_d("Client mode update wifi firmware: %s", url_str);
#endif
	} else {
		rf_e("%s: Unknown type: %d", __func__, passive->type);
		return -WM_FAIL;
	}
	status = _update_system_image(client_mode_data_fetch_cb,
				      (void *)handle, resp->content_length,
				      passive);

skip_update:
	http_close_session(&handle);
	return status;
}

struct partition_entry *rfget_get_passive_wifi_firmware(void)
{
	short history = 0;
	struct partition_entry *f1, *f2;

	f1 = part_get_layout_by_id(FC_COMP_WLAN_FW, &history);
	f2 = part_get_layout_by_id(FC_COMP_WLAN_FW, &history);

	if (f1 == NULL || f2 == NULL) {
		rf_e("Unable to retrieve flash layout");
		return NULL;
	}

	return f1->gen_level >= f2->gen_level ? f2 : f1;
}

struct partition_entry *rfget_get_passive_firmware(void)
{
	short history = 0;
	struct partition_entry *f1, *f2;

	f1 = part_get_layout_by_id(FC_COMP_FW, &history);
	f2 = part_get_layout_by_id(FC_COMP_FW, &history);

	if (f1 == NULL || f2 == NULL) {
		rf_e("Unable to retrieve flash layout");
		return NULL;
	}

	if (boot_get_partition_no() == 0) {
		/* If this is 0, it means the first entry is the booted
		 * firmware. Which means we want to write to the second entry
		 * and make it the active partition hence forth
		 */
		return f2;
	} else {
		return f1;
	}
}

static void cmd_updatefw(int argc, char **argv)
{
	const char *url_str = argv[1];
	struct partition_entry *p = rfget_get_passive_firmware();

	if (argc != 2) {
		wmprintf("Usage: %s <http-url>\r\n", argv[0]);
		wmprintf("Invalid number of arguments.\r\n");
		return;
	}

	if (rfget_client_mode_update(url_str, p) == WM_SUCCESS) {
		wmprintf("[rfget] Firmware update succeeded\r\n");
	} else {
		wmprintf("[rfget] Firmware update failed\r\n");
	}
}

#ifdef CONFIG_WIFI_FW_UPGRADE
static void cmd_update_wififw(int argc, char **argv)
{
	const char *url_str = argv[1];
	struct partition_entry *p = rfget_get_passive_wifi_firmware();

	if (argc != 2) {
		wmprintf("Usage: %s <http-url>\r\n", argv[0]);
		wmprintf("Invalid number of arguments.\r\n");
		return;
	}
	if (rfget_client_mode_update(url_str, p) == WM_SUCCESS) {
		wmprintf("[rfget] WiFi Firmware update succeeded\r\n");
	} else {
		wmprintf("[rfget] WiFi Firmware update failed\r\n");
	}
}
#endif

static void cmd_updatefs(int argc, char **argv)
{
	const char *url_str = argv[2];
	struct partition_entry *p;

	if (argc != 3) {
		wmprintf("Usage: %s <ftfs-name> <http-url>\r\n", argv[0]);
		wmprintf("Invalid number of arguments.\r\n");
		return;
	}

	p = part_get_passive_partition_by_name(argv[1]);
	if (p == NULL) {
		wmprintf("[rfget] No valid partition with the name '%s'\r\n",
			argv[1]);
	}

	if (rfget_client_mode_update(url_str, p) == WM_SUCCESS) {
		wmprintf("[rfget] FTFS update succeeded\r\n");
	} else {
		wmprintf("[rfget] FTFS update failed\r\n");
	}
}

static void cmd_rfprint(int argc, char **argv)
{
	const char *url_str = argv[1];

	if (argc != 2) {
		wmprintf("Usage: %s <http-url>\r\n", argv[0]);
		wmprintf("Invalid number of arguments.\r\n");
		return;
	}

	_dump_resource(url_str);
	return;
}

static struct cli_command rfget_commands[] = {
	{"rfprint", "<http_url>", cmd_rfprint},
	{"updatefw", "<http_url>", cmd_updatefw},
#ifdef CONFIG_WIFI_FW_UPGRADE
	{"updatewififw", "<http_url>", cmd_update_wififw},
#endif
	{"updatefs", "<fs-name> <http_url>", cmd_updatefs},
};

int rfget_cli_init(void)
{
	return cli_register_commands(rfget_commands,
				     sizeof(rfget_commands) /
				     sizeof(struct cli_command));
}

int rfget_init(void)
{
	if (rfget_init_done)
		return WM_SUCCESS;

	crc32_init();
	/* Initialise internal/external flash memory */
	flash_drv_init();

	rfget_init_done = 1;

	return WM_SUCCESS;
}

struct fs *rfget_ftfs_init(struct ftfs_super *sb, int be_ver, const char *name,
			   struct partition_entry **passive)
{
	struct partition_entry *f1, *f2, *active = NULL;
	FT_HEADER sec1, sec2;
	mdev_t *fl_dev;
	short history = 0;
	flash_desc_t fd;

	rfget_init();

	int ret = part_init();
	if (ret != WM_SUCCESS) {
		rf_e("rfget_ftfs_init: could not read partition table");
		return NULL;
	}
	*passive = NULL;
	f1 = part_get_layout_by_name(name, &history);
	f2 = part_get_layout_by_name(name, &history);

	if (f1 == NULL) {
		/* There are no FTFS partitions defined */
		return NULL;
	}

	/* Processing for partition 1 */
	fl_dev = flash_drv_open(f1->device);
	if (fl_dev == NULL) {
		ftfs_e("Flash driver init is required"
			"before open");
		return NULL;
	}

	/*   Read the FTFS Header */
	if (ft_read_header(&sec1, f1->start, fl_dev) != WM_SUCCESS)
		return NULL;
	ftfs_l("part1:  be_ver: %d", sec1.backend_version);
	/*   Validate be version */
	if (ft_is_valid_magic(sec1.magic) && sec1.backend_version == be_ver) {
		active = f1;
	}

	flash_drv_close(fl_dev);

	if (f2) {
		/* Processing for partition 2 */
		fl_dev = flash_drv_open(f2->device);
		if (fl_dev == NULL) {
			ftfs_e("Flash driver init is required"
				 "before open\r\n");
			return NULL;
		}

		/*   Read the FTFS Header */
		if (ft_read_header(&sec2, f2->start, fl_dev) != WM_SUCCESS)
			return NULL;
		ftfs_l("part2:  be_ver: %d", sec2.backend_version);
		/*   Validate be version */
		if (ft_is_valid_magic(sec2.magic) && sec2.backend_version ==
		    be_ver) {
			if (active) {
				/* If f1 is also valid then pick and choose */
				struct partition_entry *p =
					part_get_active_partition(f1, f2);
				/* Given 2 partitions, one of them has to be
				 * active */
				active = p;
			} else
				active = f2;
		}
		flash_drv_close(fl_dev);

		if (active == f1)
			*passive = f2;
		else
			*passive = f1;
	}

	if (active) {
		ftfs_l("be_ver = %d active = %x", be_ver, active->start);
		part_to_flash_desc(active, &fd);
		return ftfs_init(sb, &fd);
	} else {
		return NULL;
	}
}

void purge_stream_bytes(data_fetch data_fetch_cb, void *priv,
			size_t length, unsigned char *scratch_buf,
			const int scratch_buflen)
{
	int size_read, to_read;
	while (length > 0) {
		to_read = length > scratch_buflen ? scratch_buflen : length;
		size_read = data_fetch_cb(priv, scratch_buf, to_read);
		if (size_read <= 0) {
			rf_e("Unexpected size returned");
			break;
		}
		length -= size_read;
	}
}

int rfget_update_complete(update_desc_t *ud)
{
	flash_drv_close(ud->dev);
	return part_set_active_partition(ud->pe);
}

int rfget_update_data(update_desc_t *ud, const char *data, size_t datalen)
{
	if (datalen > ud->remaining) {
		rf_e("Update data greater than partition len");
		return -WM_E_RFGET_INVLEN;
	}

	/* Write the data */
	if (flash_drv_write(ud->dev, (void *)data, datalen,
			    ud->addr) != 0) {
		rf_e("Failed to write update data");
		return -WM_E_RFGET_FWR;
	}

	ud->addr += datalen;
	ud->remaining -= datalen;
	return WM_SUCCESS;
}

int rfget_update_begin(update_desc_t *ud, struct partition_entry *p)
{
	ud->pe = p;
	ud->addr = p->start;
	ud->remaining = p->size;
	ud->dev = flash_drv_open(p->device);
	if (ud->dev == NULL) {
		rf_e("Flash driver init is required before open");
		return -WM_E_RFGET_FUPDATE;
	}

	if (flash_drv_erase(ud->dev, ud->addr, ud->remaining) < 0) {
		rf_e("Failed to erase partition");
		return -WM_E_RFGET_FWR;
	}

	return WM_SUCCESS;
}

