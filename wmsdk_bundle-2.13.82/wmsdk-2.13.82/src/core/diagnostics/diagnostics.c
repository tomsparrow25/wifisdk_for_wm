/*
 * Copyright 2008-2013 Marvell International Ltd.
 * All Rights Reserved.
 */

#include <stdio.h>
#include <string.h>
#include <rtc.h>
#include <json.h>
#include <psm.h>
#include <wlan.h>
#include <wm_utils.h>
#include <wmtime.h>
#include <wm_net.h>
#include <diagnostics.h>

#define DIAG_STATS_LENGTH	64

#define J_NAME_DIAG		"diag"
#define VAR_REBOOT_REASON	"reboot_reason"
#define VAR_EPOCH		"epoch"
#define VAR_CONN_STATS        	"conn"
#define VAR_DHCP_STATS        	"dhcp"
#define VAR_HTTP_CLIENT_STATS  	"httpc"
#define VAR_CLOUD_STATS       	"cloud"
#define VAR_CLOUD_CUMUL_STATS	"cloud_cumul"
#define VAR_HTTPD_STATS      	"httpd"
#define VAR_NET_STATS		"net"
#define VAR_IP_ADDR		"ip_addr"
#define VAR_TIME		"time"
#define VAR_PROV_TYPE		"prov_type"

#define STATS_MOD_NAME		"stats"

#ifdef ENABLE_DBG
#define dbg(...)	wmprintf(__VA_ARGS__)
#else
#define dbg(...)
#endif

int get_stats(char *string, int len, wm_stat_type_t stat_type)
{
	struct wlan_network network;
	struct in_addr ip;
	char *psm_val;
	short rssi;
	int ret;
	psm_handle_t handle;

	if (string == NULL)
		return -1;

	switch (stat_type) {
	case CONN_STATS:
		wlan_get_current_rssi(&rssi);
		snprintf(string, len, "%u %u %u %u %u %u %d",
			 g_wm_stats.wm_lloss, g_wm_stats.wm_conn_att,
			 g_wm_stats.wm_conn_succ, g_wm_stats.wm_conn_fail,
			 g_wm_stats.wm_auth_fail, g_wm_stats.wm_nwnt_found,
			 rssi);
		break;
	case DHCP_STATS:
		snprintf(string, len, "%u %u %u %u %u", g_wm_stats.wm_dhcp_succ,
			 g_wm_stats.wm_dhcp_fail, g_wm_stats.wm_leas_succ,
			 g_wm_stats.wm_leas_fail, g_wm_stats.wm_addr_type);
		break;
	case HTTP_CLIENT_STATS:
		snprintf(string, len, "%u %u %u %u %u %u",
			 g_wm_stats.wm_ht_dns_fail, g_wm_stats.wm_ht_sock_fail,
			 g_wm_stats.wm_ht_conn_no_route,
			 g_wm_stats.wm_ht_conn_timeout,
			 g_wm_stats.wm_ht_conn_reset,
			 g_wm_stats.wm_ht_conn_other);
		break;
	case CLOUD_STATS:
		g_wm_stats.wm_cl_total = g_wm_stats.wm_cl_post_succ +
			g_wm_stats.wm_cl_post_fail;
		snprintf(string, len, "%u %u %u", g_wm_stats.wm_cl_post_succ,
			g_wm_stats.wm_cl_post_fail, g_wm_stats.wm_cl_total);
		break;
	/* fixme: Currently, cloud cumulative stats are not reported in
	 * diagnostics */
	case CLOUD_CUMUL_STATS:
		psm_val = string;
		g_wm_stats.wm_cl_cum_total = 0;
		if ((ret = psm_open(&handle, STATS_MOD_NAME)) == 0) {
			if (psm_get
			    (&handle, VAR_CLOUD_CUMUL_STATS, psm_val,
			     len) == 0) {
				g_wm_stats.wm_cl_cum_total = atoi(psm_val);
			}
			psm_close(&handle);
		} else {
			if (ret == -WM_E_PSM_METADATA_CRC)
				psm_erase_and_init();
			if (ret == -WM_E_CRC)
				psm_erase_partition
					(psm_get_partition_id(STATS_MOD_NAME));
		}
		g_wm_stats.wm_cl_cum_total =
		    g_wm_stats.wm_cl_cum_total + g_wm_stats.wm_cl_total;
		snprintf(string, len, "%u", g_wm_stats.wm_cl_cum_total);
		break;
	case HTTPD_STATS:
		snprintf(string, len, "%u %u %u %s-%s",
			 g_wm_stats.wm_hd_wsgi_call, g_wm_stats.wm_hd_file,
			 g_wm_stats.wm_hd_time,
			 g_wm_stats.wm_hd_useragent.product,
			 g_wm_stats.wm_hd_useragent.version);
		break;
	case NET_STATS:
		net_diag_stats(string, len);
		break;
	case IP_ADDR:
		if (wlan_get_current_network(&network)) {
			snprintf(string, len, "");
			return -1;
		}
		ip.s_addr = network.address.ip;
		snprintf(string, len, "%s", inet_ntoa(ip));
		break;
	case TIME:
		snprintf(string, len, "%u", wmtime_time_get_posix());
		break;
	case PROV_TYPE:
		snprintf(string, len, "%u", g_wm_stats.wm_prov_type);
		break;
		break;
	default:
		snprintf(string, len, "Invalid");
	}
	return 0;
}
int diagnostics_read_stats(struct json_str *jptr)
{
	char temp[DIAG_STATS_LENGTH];
	int json_int;

	if (jptr == NULL)
		return -1;

	json_set_val_int(jptr, VAR_EPOCH, sys_get_epoch());

	get_stats(temp, sizeof(temp), CONN_STATS);
	json_set_val_str(jptr, VAR_CONN_STATS, temp);

	get_stats(temp, sizeof(temp), DHCP_STATS);
	json_set_val_str(jptr, VAR_DHCP_STATS, temp);

	get_stats(temp, sizeof(temp), HTTP_CLIENT_STATS);
	json_set_val_str(jptr, VAR_HTTP_CLIENT_STATS, temp);

	get_stats(temp, sizeof(temp), CLOUD_STATS);
	json_set_val_str(jptr, VAR_CLOUD_STATS, temp);

	/* fixme: Removing cloud cumulative stats from the output */
#if 0
	get_stats(temp, sizeof(temp), CLOUD_CUMUL_STATS);
	json_set_val_str(jptr, VAR_CLOUD_CUMUL_STATS, temp);
#endif

	get_stats(temp, sizeof(temp), HTTPD_STATS);
	json_set_val_str(jptr, VAR_HTTPD_STATS, temp);

	get_stats(temp, sizeof(temp), NET_STATS);
	json_set_val_str(jptr, VAR_NET_STATS, temp);

	get_stats(temp, sizeof(temp), IP_ADDR);
	json_set_val_str(jptr, VAR_IP_ADDR, temp);

	get_stats(temp, sizeof(temp), TIME);
	json_int = atoi(temp);
	json_set_val_int(jptr, VAR_TIME, json_int);

	get_stats(temp, sizeof(temp), PROV_TYPE);
	json_int = atoi(temp);
	json_set_val_int(jptr, VAR_PROV_TYPE, json_int);

	return 0;
}

int diagnostics_write_stats()
{
	int ret;
	psm_handle_t handle;
	char psm_val[DIAG_STATS_LENGTH];

	ret = psm_register_module(STATS_MOD_NAME, "common_part", PSM_CREAT);
	if (ret != WM_SUCCESS && ret != -WM_E_EXIST) {
		dbg("Failed to register stats module with psm\r\n");
		return -1;
	}
	/* Query for cumulative statistics first, since this has to open psm
	 * itself.
	 */
	get_stats(psm_val, sizeof(psm_val), CLOUD_CUMUL_STATS);

	ret = psm_open(&handle, STATS_MOD_NAME);
	if (ret != WM_SUCCESS) {
		dbg("Failed to open psm module. Error: %d\r\n", ret);
		if (ret == -WM_E_PSM_METADATA_CRC)
			psm_erase_and_init();
		if (ret == -WM_E_CRC)
			psm_erase_partition
				(psm_get_partition_id(STATS_MOD_NAME));
		return ret;
	}

	psm_set(&handle, VAR_CLOUD_CUMUL_STATS, psm_val);

	snprintf(psm_val, sizeof(psm_val), "%d", g_wm_stats.reboot_reason);
	psm_set(&handle, VAR_REBOOT_REASON, psm_val);

	snprintf(psm_val, sizeof(psm_val), "%u", sys_get_epoch());
	psm_set(&handle, VAR_EPOCH, psm_val);

	get_stats(psm_val, sizeof(psm_val), CONN_STATS);
	psm_set(&handle, VAR_CONN_STATS, psm_val);

	get_stats(psm_val, sizeof(psm_val), DHCP_STATS);
	psm_set(&handle, VAR_DHCP_STATS, psm_val);

	get_stats(psm_val, sizeof(psm_val), HTTP_CLIENT_STATS);
	psm_set(&handle, VAR_HTTP_CLIENT_STATS, psm_val);

	get_stats(psm_val, sizeof(psm_val), CLOUD_STATS);
	psm_set(&handle, VAR_CLOUD_STATS, psm_val);

	get_stats(psm_val, sizeof(psm_val), HTTPD_STATS);
	psm_set(&handle, VAR_HTTPD_STATS, psm_val);

	get_stats(psm_val, sizeof(psm_val), NET_STATS);
	psm_set(&handle, VAR_NET_STATS, psm_val);

	get_stats(psm_val, sizeof(psm_val), IP_ADDR);
	psm_set(&handle, VAR_IP_ADDR, psm_val);

	get_stats(psm_val, sizeof(psm_val), TIME);
	psm_set(&handle, VAR_TIME, psm_val);

	psm_close(&handle);

	return 0;
}

int diagnostics_read_stats_psm(struct json_str *jptr)
{
	int ret;
	int json_int;
	char temp[DIAG_STATS_LENGTH];
	psm_handle_t handle;

	ret = psm_open(&handle, STATS_MOD_NAME);
	if (ret != WM_SUCCESS) {
		dbg("Failed to open psm module. Error: %d\r\n", ret);
		if (ret == -WM_E_PSM_METADATA_CRC)
			psm_erase_and_init();
		if (ret == -WM_E_CRC)
			psm_erase_partition
				(psm_get_partition_id(STATS_MOD_NAME));
		return ret;
	}
	psm_get(&handle, VAR_REBOOT_REASON, temp, sizeof(temp));
	json_int = atoi(temp);
	json_set_val_int(jptr, VAR_REBOOT_REASON, json_int);

	psm_get(&handle, VAR_EPOCH, temp, sizeof(temp));
	json_int = atoi(temp);
	json_set_val_int(jptr, VAR_EPOCH, json_int);

	psm_get(&handle, VAR_CONN_STATS, temp, sizeof(temp));
	json_set_val_str(jptr, VAR_CONN_STATS, temp);

	psm_get(&handle, VAR_DHCP_STATS, temp, sizeof(temp));
	json_set_val_str(jptr, VAR_DHCP_STATS, temp);

	psm_get(&handle, VAR_HTTP_CLIENT_STATS, temp, sizeof(temp));
	json_set_val_str(jptr, VAR_HTTP_CLIENT_STATS, temp);

	psm_get(&handle, VAR_CLOUD_STATS, temp, sizeof(temp));
	json_set_val_str(jptr, VAR_CLOUD_STATS, temp);

	/* fixme: Removing cloud cumulative stats from the output */
#if 0
	psm_get(&handle, VAR_CLOUD_CUMUL_STATS, temp, sizeof(temp));
	json_set_val_str(jptr, VAR_CLOUD_CUMUL_STATS, temp);
#endif

	psm_get(&handle, VAR_HTTPD_STATS, temp, sizeof(temp));
	json_set_val_str(jptr, VAR_HTTPD_STATS, temp);

	psm_get(&handle, VAR_NET_STATS, temp, sizeof(temp));
	json_set_val_str(jptr, VAR_NET_STATS, temp);

	psm_get(&handle, VAR_IP_ADDR, temp, sizeof(temp));
	json_set_val_str(jptr, VAR_IP_ADDR, temp);

	psm_get(&handle, VAR_TIME, temp, sizeof(temp));
	json_int = atoi(temp);
	json_set_val_int(jptr, VAR_TIME, json_int);

	psm_close(&handle);

	return 0;
}

void diagnostics_set_reboot_reason(wm_reboot_reason_t reason)
{
	g_wm_stats.reboot_reason = reason;
}
