/*
 * Copyright (C) 2008-2013, Marvell International Ltd.
 * All Rights Reserved.
 */

#ifndef _WMCLOUD_H_
#define _WMCLOUD_H_

#if APPCONFIG_HTTPS_CLOUD
#include <wm-tls.h>
#endif
#include <wm_os.h>
#include <json.h>
#include <httpc.h>
#include <httpd.h>
#include <wmstats.h>

#define		DEBUG	1

#if DEBUG
#include <wmlog.h>
#define cl_dbg(...)	wmlog("appln", ##__VA_ARGS__)
#else
#define cl_dbg(...)
#endif

#define CLOUD_MIN_URL_LEN 8
#define CLOUD_MAX_URL_LEN 200
#define DEVICE_NAME_MAX_LEN 64
#if APPCONFIG_HTTPS_CLOUD
#define DEFAULT_WMCLOUD_URL "https://10.31.130.219/cloud"
#else
#define DEFAULT_WMCLOUD_URL "http://10.31.130.219/cloud"
#endif
#define DEFAULT_CLOUD_SOCKET_TIMEOUT  (5)	/* in secs */
#define DEFAULT_DEVICE_NAME "unknown"

#define CLOUD_PACKET_CONTENT_TYPE "application/json"
#define CLOUD_INACTIVE "Cloud inactive"

#define UUID_MAX_LEN 32
#define CLASS_NAME_MAX 16
#define CLOUD_PACKET_MAXSIZE 1024


typedef enum {
	CLOUD_UNINIT,
	CLOUD_ACTIVE,
	CLOUD_HALT,
} cloud_state_t;

typedef enum {
	RUNNING = 1,
	INT_ERROR,
	CONN_ERROR,
	TX_ERROR,
	STOPPED,
} cloud_sub_state_t;

typedef enum {
	EVT_ENTER,
	EVT_STRT,
	EVT_STOP,
	EVT_INT_ERROR,
	EVT_CONN_ERROR,
	EVT_TX_ERROR,
	EVT_OP_SUCCESS,
} cloud_event_t;

#define QUERY_STR		"?"

/*
 *The cloud configuration structure maintains
 *information related to the cloud.
 */
typedef struct cloud cloud_t;

struct cloud {
	http_session_t hS;
	char send_packet[CLOUD_PACKET_MAXSIZE];
	char recv_packet[CLOUD_PACKET_MAXSIZE];
	/** The UUID of the device. This is used by the remote cloud server to
	 *identify the device. */
	char uuid[UUID_MAX_LEN];
	char dev_class[CLASS_NAME_MAX];
	cloud_state_t state;
	unsigned int stack_size;
	/*
	 *The url will be either set to default
	 * value or loaded from PSM.
	 */
	const char *url;
	const char *name;

	/* TLS configuration: Set this to NULL if non TLS cloud server. */
#if APPCONFIG_HTTPS_CLOUD
	tls_init_config_t tls_cfg;
#endif
	os_thread_t thread_hnd;
	os_mutex_t mutex;
	bool stop_request;
	long long sequence;

	/* Application Functions  */
	void (*app_cloud_periodic_post) (struct json_str *jstr);
	void (*app_cloud_handle_req) (struct json_str *jstr, struct json_object
				      *obj, bool *repeat_POST);
};

/* PSM variables for the cloud */

#define CLOUD_MOD_NAME     "cloud"	/* cloud module name */
#define VAR_CLOUD_URL      "url"	/* cloud.url */
#define VAR_CLOUD_DEVICE_NAME      "name"	/* cloud.name */

#define J_NAME_HEADER		"header"
#define J_NAME_DATA		"data"
#define J_NAME_TYPE		"type"
#define J_NAME_SYS              "sys"
#define J_NAME_RSSI             "rssi"
#define J_NAME_REBOOT           "reboot"
#define J_NAME_DIAG_LIVE        "diag_live"
#define J_NAME_DIAG_HISTORY     "diag_history"
#define J_NAME_UUID             "uuid"
#define J_NAME_CLOUD            "cloud"
#define J_NAME_SEQUENCE         "sequence"
#define J_NAME_URL              "url"
#define J_NAME_NAME             "name"
#define J_NAME_EPOCH            "epoch"
#define J_NAME_ENABLED          "enabled"
#define J_NAME_TIME		"time"
#define J_NAME_FIRMWARE         "firmware"
#define J_NAME_FS_URL           "fs_url"
#define J_NAME_FW_URL           "fw_url"
#define J_NAME_WIFIFW_URL       "wififw_url"
#define SUCCESS			"success"
#define FAIL			"fail"
#define J_NAME_DIAG		"diag"

/* Cloud's Exported Functions */
int cloud_actual_start(const char *dev_class, void (*handle_req)(struct json_str
		*jstr, struct json_object *obj, bool *repeat_POST),
		void (*periodic_post)(struct json_str *jstr),
		unsigned int stack_size);
int cloud_actual_stop(void);
int cloud_params_load(cloud_t *c);
int cloud_wakeup_for_send();
void cloud_process_server_response(cloud_t *c, unsigned len,
					bool *repeat_POST);
int create_transmit_packet(cloud_t *c);
int cloud_sm(cloud_event_t event);
int cloud_get_ui_link(httpd_request_t *req);
#endif
