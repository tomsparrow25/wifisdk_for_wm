/** @file wfd_main.c
 *  @brief This file contains core WFD state machine code.
 *           and functions for implementation.
 *
 *  Copyright (C) 2003-2010, Marvell International Ltd.
 *  All Rights Reserved
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wifi.h>
#include <wm_net.h>
#include <wps.h>
#include <p2p.h>

#include "wps_mem.h"
#include "wps_util.h"
#include "wps_def.h"
#include "wps_wlan.h"
#include "wps_os.h"
#include "wlan_hostcmd.h"
#include "wlan_wfd.h"
#include "wps_msg.h"

#define difftime(t1, t0) (double)(t1 - t0)

/********************************************************
	Local Variables
********************************************************/

/********************************************************
	Global Variables
********************************************************/
/** Global pwps information */
extern PWPS_INFO gpwps_info;
/** wps global */
extern WPS_DATA wps_global;
/** IE buffer index */
static short ie_index;
extern struct wps_thread_t wps;
extern char WFD_devicename[33];
extern struct wlan_network p2p_uap_network;
extern struct wlan_network p2p_network;
extern u8 p2p_scan_on;
extern u8 go_request;
u8 auto_go;
int wfd_bss_type;
u8 evt_buffer[EVENT_MAX_BUF_SIZE];

struct wfd_wlan_event wfd_other_event;
u8 intended_mac_addr[6];

extern u8 *g_ssid;
extern u8 *g_bssid;
extern u8 g_channel;
extern char op_channel_number;

/** Interface switch required or not */
u8 wps_device_name[WFD_MAX_FIND_DEVICES][MAX_DEVICE_NAME_LEN + 1];
u16 device_name_len[WFD_MAX_FIND_DEVICES];
void wfd_reprint_find_table(void);
void wfd_start_peer_ageout_timer(WFD_DATA *pwfd_data);
void wfd_peer_ageout_time_handler(void *user_data);
void wfd_peer_selected_ageout_time_handler(void *user_data);
int wfd_handle_persistent_group_invoked_event(PWPS_INFO pwps_info,
					      WPS_DATA *wps_s,
					      apeventbuf_wfd_generic *
					      wfd_event);

static int wfd_update_find_table(apeventbuf_wfd_generic *wfd_event);
/********************************************************
	Local Functions
********************************************************/
/**
 *  @brief Prints a MAC address in colon separated form from raw data
 *
 *  @param raw      A pointer to the hex data buffer
 *  @return         N/A
 */
void print_mac1(u8 *raw)
{
	wmprintf("%02x:%02x:%02x:%02x:%02x:%02x", (unsigned int)raw[0],
		 (unsigned int)raw[1], (unsigned int)raw[2],
		 (unsigned int)raw[3], (unsigned int)raw[4],
		 (unsigned int)raw[5]);
	return;
}

/**
 *  @brief  Clean up an existing entry from DB, if error found.
 *  @param  pwfd_data  Data pointer to clean.
 *  @param  index      Index into the DB table
 *
 *  @return  WPS_STATUS_SUCCESS--success, WPS_STATUS_FAIL--fail
 */
static void wfd_cleanup_peer_entry(WFD_DATA *pwfd_data, int index)
{
	ENTER();
	memset(pwfd_data->find_results[index].device_id, 0, ETH_ALEN);
	memset(pwfd_data->find_results[index].device_address, 0, ETH_ALEN);
	memset(pwfd_data->find_results[index].go_ssid, 0, MAX_SSID_LEN + 1);
	pwfd_data->find_results[index].is_valid = 0;
	pwfd_data->num_find_results--;
	LEAVE();
}

/**
 *  @brief  Check if peer event has status attribute and print its value
 *  @param  req_resp Request or response flag
 *  @param  wfd_event  New event data pointer
 *
 *  @return  None.
 */
static void
wfd_check_go_neg_status(u8 req_resp, apeventbuf_wfd_generic *wfd_event)
{
	const u8 wifi_oui[3] = { 0x50, 0x6F, 0x9A };
	tlvbuf_wfd_status *wfd_tlv;
	wfd_ie_header *wfd_wps_header;
	WFD_DATA *pwfd_data = &wps_global.wfd_data;
	u8 *ptr;
	s16 left_len = 0;
	u16 len = 0, wfd_wps_len = 0;

	ENTER();

	wfd_wps_header = (wfd_ie_header *) (wfd_event->entire_ie_list);
	wfd_wps_len = wlan_le16_to_cpu(wfd_event->event_length)
	    - sizeof(apeventbuf_wfd_generic);

	while (wfd_wps_len >= sizeof(wfd_ie_header)) {

		/* 50:6F:9A:09 */
		if (!memcmp(wfd_wps_header->oui, wifi_oui, sizeof(wifi_oui)) &&
		    (wfd_wps_header->oui_type == WFD_OUI_TYPE)) {

			ptr = wfd_wps_header->ie_list;
			left_len =
			    wfd_wps_header->ie_length -
			    sizeof(wfd_wps_header->oui)
			    - sizeof(wfd_wps_header->oui_type);

			while (left_len > WFD_IE_HEADER_LEN) {

				if (*ptr == TLV_TYPE_WFD_STATUS) {
					wfd_tlv = (tlvbuf_wfd_status *) ptr;

					/* print status Code */
					if (wfd_tlv->status_code)
						wps_printf(DEBUG_WFD_DISCOVERY,
						    "Go Negotiation %s "
							"result = %d.",
						     (req_resp) ? "response" :
						     "request",
						     wfd_tlv->status_code);

					/* reset dev_index selected here */
					if (wfd_tlv->status_code)
						pwfd_data->dev_index = -1;

					LEAVE();
					return;
				}

				memcpy(&len, ptr + 1, sizeof(u16));
				len = wlan_le16_to_cpu(len);

				ptr += len + WFD_IE_HEADER_LEN;
				left_len -= len + WFD_IE_HEADER_LEN;
			}
		}
		wfd_wps_len -= wfd_wps_header->ie_length + IE_HEADER_LEN;
		wfd_wps_header = (wfd_ie_header *) (((u8 *) wfd_wps_header) +
						    wfd_wps_header->ie_length +
						    IE_HEADER_LEN);
	}

	LEAVE();
	return;
}

/**
 *  @brief  Update the find table from FW events information.
 *  @param  wfd_event  New event data pointer
 *
 *  @return  WPS_STATUS_SUCCESS--success, WPS_STATUS_FAIL--fail
 */
static int wfd_update_find_table(apeventbuf_wfd_generic *wfd_event)
{
	const u8 wifi_oui[3] = { 0x50, 0x6F, 0x9A };
	const u8 wps_oui[3] = { 0x00, 0x50, 0xF2 };
	int ret = WPS_STATUS_SUCCESS;
	int i, index = 0;
	int found = 0, first_WFD_WPS_IE = 0, updates = 0;
	tlvbuf_wfd_capability *wfd_tlv1 = NULL;
	tlvbuf_wfd_device_info *wfd_tlv2;
	tlvbuf_wfd_device_id *wfd_tlv3;
	tlvbuf_wfd_group_id *wfd_tlv4;
	tlvbuf_wfd_intended_addr *wfd_tlv5;
	tlvbuf_wfd_channel *wfd_tlv6;
	tlvbuf_wfd_channel_list *wfd_tlv7;
	wfd_ie_header *wfd_wps_header;
	IEEE_DsParamSet *ds_param = NULL;
	WFD_DATA *pwfd_data = &wps_global.wfd_data;
	u8 *ptr, *array_ptr;
	u8 type = 0;
	s16 left_len = 0;
	u16 len = 0, wfd_wps_len = 0, wps_len = 0, wps_type =
	    0, len_wifidirect = 0, event_type = 0;

	ENTER();

	event_type = wlan_le16_to_cpu(wfd_event->event_type);
	/* FW error ignored */
	if (!memcmp
	    (wfd_event->peer_mac_addr, "\x00\x00\x00\x00\x00\x00", ETH_ALEN)) {
		LEAVE();
		return WPS_STATUS_FAIL;
	}

	if (pwfd_data->num_find_results >= WFD_MAX_FIND_DEVICES) {
		P2P_LOG
		    ("Peer Device limit reached, "
			"dropping the information.");
		LEAVE();
		return WPS_STATUS_FAIL;
	}

	/* IF peer is Auto-GO, device_id (src from FW) = Intended Intf address*/
	/* IF peer is Device , device_id (src from FW) = Device address */

	/* Look up the existing table and add if !found */
	for (i = 0; i < pwfd_data->num_find_results; i++) {
		if ((!(memcmp(wfd_event->peer_mac_addr,
			     pwfd_data->find_results[i].device_id, ETH_ALEN))
		   || ((pwfd_data->find_results[i].device_address)
			&& (!(memcmp(wfd_event->peer_mac_addr,
		     pwfd_data->find_results[i].device_address, ETH_ALEN)))))
		    && pwfd_data->find_results[i].is_valid == 1) {
			found = 1;
			index = i;
			if (event_type == EV_TYPE_WFD_PEER_DETECTED) {
				pwfd_data->find_results[i].is_valid = 1;
		/** Update the current timestamp for the event */
				pwfd_data->find_results[i].timestamp =
					os_ticks_to_msec(os_ticks_get()) / 1000;
			}
			break;
		}
	}

	wfd_wps_header = (wfd_ie_header *) (wfd_event->entire_ie_list);
	wfd_wps_len = wlan_le16_to_cpu(wfd_event->event_length)
	    - sizeof(apeventbuf_wfd_generic);

	while (wfd_wps_len >= sizeof(wfd_ie_header)) {

		if (wfd_wps_header->element_id == DS_PARAM_SET &&
		    wfd_wps_header->ie_length == 1) {
			ds_param = (IEEE_DsParamSet *) wfd_wps_header;
			wfd_wps_header =
			    (wfd_ie_header *) (((u8 *) wfd_wps_header) +
					       sizeof(IEEE_DsParamSet));
			wfd_wps_len -= sizeof(IEEE_DsParamSet);
		}

		if (!memcmp(wfd_wps_header->oui, wifi_oui, sizeof(wifi_oui)) ||
		    !(memcmp(wfd_wps_header->oui, wps_oui, sizeof(wps_oui)))
		    ) {

			if (first_WFD_WPS_IE == 0 && !found) {
				/* copy for first P2P IEs */
				first_WFD_WPS_IE = 1;
				index = pwfd_data->num_find_results;
				device_name_len[index] = 0;

				pwfd_data->find_results[index].is_valid = 1;
				memcpy(pwfd_data->find_results[index].device_id,
				       wfd_event->peer_mac_addr, ETH_ALEN);
				pwfd_data->num_find_results++;
			}

			switch (wfd_wps_header->oui_type) {
			case WFD_OUI_TYPE:

				ptr = wfd_wps_header->ie_list;
				left_len =
				    wfd_wps_header->ie_length -
				    sizeof(wfd_wps_header->oui)
				    - sizeof(wfd_wps_header->oui_type);

				while (left_len > WFD_IE_HEADER_LEN) {
					type = *ptr;
					memcpy(&len, ptr + 1, sizeof(u16));
					len = wlan_le16_to_cpu(len);

					/* Sanity check on data length */
					if (left_len < WFD_IE_HEADER_LEN +
						len) {
						wps_printf(MSG_WARNING,
							   "Corrupt IE, type = "
						"0x%x, len = %d, left = %d\n",
							   type, len, left_len);
						wfd_cleanup_peer_entry
						    (pwfd_data, index);
						return WPS_STATUS_FAIL;
					}

					switch (type) {
					case TLV_TYPE_WFD_CAPABILITY:
						wfd_tlv1 =
						    (tlvbuf_wfd_capability *)
						    ptr;
						pwfd_data->find_results[index].
						    group_capability =
						    wfd_tlv1->group_capability;
						break;

					case TLV_TYPE_WFD_DEVICE_ID:
						wfd_tlv3 =
						    (tlvbuf_wfd_device_id *)
						    ptr;
						memcpy(pwfd_data->
						       find_results[index].
						       device_address,
						       wfd_tlv3->
						       dev_mac_address,
						       ETH_ALEN);
						break;

					case TLV_TYPE_WFD_INTENDED_ADDRESS:
						wfd_tlv5 =
						    (tlvbuf_wfd_intended_addr *)
						    ptr;
						memcpy(pwfd_data->
						       find_results[index].
						       intended_address,
						       wfd_tlv5->group_address,
						       ETH_ALEN);
						break;

					case TLV_TYPE_WFD_GROUP_ID:
						wfd_tlv4 =
						    (tlvbuf_wfd_group_id *) ptr;
						if (found
						    && !strlen((char *)
							       pwfd_data->
							       find_results
							       [index].go_ssid))
							updates = 1;
						memcpy(pwfd_data->
						       find_results[index].
						       go_ssid,
						       wfd_tlv4->group_ssid,
						       len -
						       (sizeof
							(tlvbuf_wfd_group_id) -
							WFD_IE_HEADER_LEN));
						break;

					case TLV_TYPE_WFD_DEVICE_INFO:
						wfd_tlv2 =
						    (tlvbuf_wfd_device_info *)
						    ptr;

						memcpy(pwfd_data->
						       find_results[index].
						       device_address,
						       wfd_tlv2->dev_address,
						       ETH_ALEN);
						/* device name */
						array_ptr =
						    wfd_tlv2->device_name +
						    wfd_tlv2->
						    secondary_dev_count *
						    WPS_DEVICE_TYPE_LEN;

						memcpy(&device_name_len[index],
						       (((u8 *) (&wfd_tlv2->
							 device_name_len))
							+
							wfd_tlv2->
							secondary_dev_count *
							WPS_DEVICE_TYPE_LEN),
						       sizeof(u16));
						device_name_len[index] =
						    ntohs(device_name_len
							  [index]);

						memset(wps_device_name[index],
						       0, MAX_DEVICE_NAME_LEN);
						if (device_name_len[index] <=
						    MAX_DEVICE_NAME_LEN)
							memcpy(wps_device_name
							       [index],
							       array_ptr,
							       device_name_len
							       [index]);
						else {
							wfd_cleanup_peer_entry
							    (pwfd_data, index);
							return WPS_STATUS_FAIL;
						}
						break;
					case TLV_TYPE_WFD_OPCHANNEL:
						wfd_tlv6 =
						    (tlvbuf_wfd_channel *) ptr;
						pwfd_data->find_results[index].
						    op_channel =
						    wfd_tlv6->channel_number;
						break;
					case TLV_TYPE_WFD_CHANNEL:
						wfd_tlv6 =
						    (tlvbuf_wfd_channel *) ptr;
						pwfd_data->find_results[index].
						    listen_channel =
						    wfd_tlv6->channel_number;
						break;
					case TLV_TYPE_WFD_CHANNEL_LIST:
						wfd_tlv7 =
						    (tlvbuf_wfd_channel_list *)
						    ptr;
						chan_entry *temp_ptr;
						temp_ptr =
						    (chan_entry *) wfd_tlv7->
						    wfd_chan_entry_list;
						len_wifidirect =
						    wlan_le16_to_cpu(wfd_tlv7->
								     length) -
						    (sizeof
						     (tlvbuf_wfd_channel_list) -
						     WFD_IE_HEADER_LEN);
						/* Multiple channel entries */
						if (len_wifidirect >
						    (sizeof(chan_entry) +
						     temp_ptr->
						     num_of_channels)) {
							while (len_wifidirect) {
								if ((int)
							    (temp_ptr->
							     regulatory_class)
							    ==
							    WIFI_REG_CLASS_81) {
								pwfd_data->
								    find_results
						    [index].peer_channel_list.
								    num_of_chan
								    =
								    temp_ptr->
							    num_of_channels;
								for (i = 0; i <
								     temp_ptr->
							     num_of_channels;
								     i++) {
								pwfd_data->
								    find_results
						    [index].peer_channel_list.
								    chan
								    [i]
								    =
								    *
								    (temp_ptr->
								     chan_list
								     +
								     i);
								}
								break;
								}
								len_wifidirect
								    -=
								    sizeof
								    (chan_entry)
								    +
								    temp_ptr->
							    num_of_channels;
								temp_ptr +=
								    sizeof
								    (chan_entry)
								    +
								    temp_ptr->
							    num_of_channels;
							}
						} else {
						/* Only one channel entry */
							pwfd_data->
							    find_results[index].
					    peer_channel_list.num_of_chan
							    =
							    temp_ptr->
							    num_of_channels;
							for (i = 0;
							     i <
							     temp_ptr->
							     num_of_channels;
							     i++) {
								pwfd_data->
								    find_results
						    [index].peer_channel_list.
								    chan[i] =
								    *(temp_ptr->
								      chan_list
								      + i);
							}
						}
						break;
					default:
						break;
					}
					ptr += len + WFD_IE_HEADER_LEN;
					left_len -= len + WFD_IE_HEADER_LEN;
				}
				break;
			case WIFI_WPS_OUI_TYPE:
				/*
				 * If device name is not found, check it in
				 * the WPS IEs
				 */
				if (device_name_len[index] == 0) {

					if (found)
						updates = 1;
					ptr = wfd_wps_header->ie_list;
					left_len =
					    wfd_wps_header->ie_length -
					    sizeof(wfd_wps_header->oui)
					    - sizeof(wfd_wps_header->oui_type);

					/* Look for WPS Device Name Len */
					while (left_len >
						sizeof(tlvbuf_wps_ie)) {
						memcpy(&wps_type, ptr,
						       sizeof(u16));
						wps_type = ntohs(wps_type);
						memcpy(&wps_len, ptr + 2,
						       sizeof(u16));
						wps_len = ntohs(wps_len);
						wps_printf(MSG_WARNING,
							   "WPS IE, type = "
							"0x%x, len %d\n",
							   wps_type, wps_len);

						/* sanity check */
						if ((left_len <
						     sizeof(tlvbuf_wps_ie) +
						     wps_len)
						    || (wps_len >
							MAX_DEVICE_NAME_LEN)) {
							wps_printf(MSG_WARNING,
								   "Corrupt "
						"WPS IE, type = 0x%x, len %d\n",
								   wps_type,
								   wps_len);
							wfd_cleanup_peer_entry
							    (pwfd_data, index);
							return WPS_STATUS_FAIL;
						}
						if (wps_type ==
							SC_Device_Name) {
							tlvbuf_wps_ie *wps_tlv =
							    (tlvbuf_wps_ie *)
							    ptr;
							array_ptr =
							    wps_tlv->data;
							device_name_len[index] =
							    wps_len;
							memset(wps_device_name
							       [index], 0,
						       MAX_DEVICE_NAME_LEN);
							memcpy(wps_device_name
							       [index],
							       array_ptr,
							       wps_len);
							wps_printf
						    (DEBUG_WFD_DISCOVERY,
							     "Read Device "
						"name from WPS parameters.\n");
						}
						ptr +=
						    wps_len +
						    sizeof(tlvbuf_wps_ie);
						left_len -=
						    wps_len +
						    sizeof(tlvbuf_wps_ie);
					}
				}
				break;
			}
		}
		wfd_wps_len -= wfd_wps_header->ie_length + IE_HEADER_LEN;
		wfd_wps_header = (wfd_ie_header *) (((u8 *) wfd_wps_header) +
						    wfd_wps_header->ie_length +
						    IE_HEADER_LEN);
	}

	if (ds_param) {
		if (wfd_tlv1
		    && wfd_tlv1->group_capability & WFD_GROUP_OWNER_MASK)
			pwfd_data->find_results[index].op_channel =
			    ds_param->current_chan;
		else
			pwfd_data->find_results[index].listen_channel =
			    ds_param->current_chan;
	}
	if (found && !updates) {
		LEAVE();
		return ret;
	}

	wfd_reprint_find_table();

	LEAVE();
	return ret;
}

/*set/get wifidirect provisioning protocol related parameters
 *
 *  Usage: "Usage : wfd_cfg_provisioning_params [<action> <configMethod>
 *                  <devPassword>]"
 *
 *  @param action         : ACTION_GET/ACTION_SET
 *  @param config_method  : WPS Config Method
 *  @param dev_password   : WPS Device Password ID
 *  @return         None
 */
void
wfd_cfg_cmd_provisioning_params(u16 action, u16 config_method, u16 dev_password)
{
	wfd_params_config *cmd_buf = NULL;
	wfd_provisioning_params *tlv = NULL;
	u8 *buffer = NULL;
	u16 cmd_len = 0;

	ENTER();

	cmd_len = sizeof(wfd_params_config) + sizeof(wfd_provisioning_params);

	buffer = (u8 *) wps_mem_malloc(MRVDRV_SIZE_OF_CMD_BUFFER);

	if (!buffer) {
		P2P_LOG("ERR:Cannot allocate memory!");
		return;
	}

	memset(buffer, 0, MRVDRV_SIZE_OF_CMD_BUFFER);

	cmd_buf = (wfd_params_config *) buffer;
	tlv = (wfd_provisioning_params *) (buffer + sizeof(wfd_params_config));

	cmd_buf->cmd_head.cmd_code = HostCmd_CMD_WFD_PARAMS_CONFIG;
	cmd_buf->cmd_head.size = cmd_len - BUF_HEADER_SIZE;
	cmd_buf->cmd_head.seq_num = (0x01) << 13;
	cmd_buf->cmd_head.result = 0;

	tlv->tag = MRVL_WFD_PROVISIONING_PARAMS_TLV_ID;
	tlv->length = sizeof(wfd_provisioning_params) - TLV_HEADER_SIZE;
	if (action == ACTION_GET) {
		cmd_buf->action = ACTION_GET;
	} else {
		cmd_buf->action = wlan_cpu_to_le16(ACTION_SET);
		tlv->action = wlan_cpu_to_le16((u16) action);
		tlv->config_methods = wlan_cpu_to_le16((u16) config_method);
		tlv->dev_password = wlan_cpu_to_le16((u16) dev_password);
	}
	endian_convert_tlv_header_out(tlv);
	if (wfd_ioctl((u8 *) buffer, cmd_len) == WPS_STATUS_SUCCESS) {
		wps_printf(DEBUG_WFD_DISCOVERY,
				"WFD Provisioning params get/set "
				"command successful.");
	} else {
		wps_printf(DEBUG_WFD_DISCOVERY,
				"WFD Provisioning params get/set "
				"command successful.");
	}
	endian_convert_tlv_header_in(tlv);
	if (buffer)
		wps_mem_free(buffer);

	LEAVE();

	return;
}

/**
 *  @brief Set/get wifidirect WPS parametters
 *
 *  Usage: "Usage : wfd_cfg_wps_params [pin/pbc/none]"
 *
 *  @param action : Action GET/SET parameter
 *  @param wps_param : Set pin/pbc or none wps parameter based on user action
 *  @return         None
 */
void wfd_cfg_cmd_wps_params(u16 action, char *wps_param)
{
	wfd_params_config *cmd_buf = NULL;
	wfd_wps_params *tlv = NULL;
	u8 *buffer = NULL;
	u16 cmd_len = 0;
	unsigned int param = 0x0;

	ENTER();

	cmd_len = sizeof(wfd_params_config) + sizeof(wfd_wps_params);

	buffer = (u8 *) wps_mem_malloc(MRVDRV_SIZE_OF_CMD_BUFFER);

	if (!buffer) {
		wmprintf("ERR:Cannot allocate memory!\n");
		return;
	}

	memset(buffer, 0, MRVDRV_SIZE_OF_CMD_BUFFER);

	cmd_buf = (wfd_params_config *) buffer;
	tlv = (wfd_wps_params *) (buffer + sizeof(wfd_params_config));

	cmd_buf->cmd_head.cmd_code = HostCmd_CMD_WFD_PARAMS_CONFIG;
	cmd_buf->cmd_head.size = cmd_len - BUF_HEADER_SIZE;
	cmd_buf->cmd_head.seq_num = (0x01) << 13;
	cmd_buf->cmd_head.result = 0;

	tlv->tag = MRVL_WFD_WPS_PARAMS_TLV_ID;
	tlv->length = sizeof(wfd_wps_params) - TLV_HEADER_SIZE;

	if (action == ACTION_GET) {
		cmd_buf->action = ACTION_GET;
	} else {
		if (strncmp(wps_param, "pin", 3) == 0)
			param = 0x0000;
		else if (strncmp(wps_param, "pbc", 3) == 0)
			param = 0x0004;
		else if (strncmp(wps_param, "none", 4)) {
			param = 0xFFFF;
		}
		cmd_buf->action = wlan_cpu_to_le16(ACTION_SET);
		tlv->action = wlan_cpu_to_le16(param);
	}
	endian_convert_tlv_header_out(tlv);
	if (wfd_ioctl((u8 *) buffer, cmd_len) == WPS_STATUS_SUCCESS)
		wps_printf(DEBUG_WFD_DISCOVERY,
				"WPS params setting successful.");
	else
		wps_printf(DEBUG_WFD_DISCOVERY, "WPS params setting failed.");

	endian_convert_tlv_header_in(tlv);
	if (buffer)
		wps_mem_free(buffer);

	LEAVE();

	return;
}

/**
 *  @brief  Check the find table for peer
 *  @param  peer Peer mac address
 *
 *  @return  index --success, -1 --fail
 */
static int wfd_corelate_event_peer(u8 *peer)
{
	int i;
	WFD_DATA *pwfd_data = &wps_global.wfd_data;
	ENTER();
	/* Look up the existing table for this peer */
	for (i = 0; i < pwfd_data->num_find_results; i++) {
		if (!
		    (memcmp
		     (peer, pwfd_data->find_results[i].device_id, ETH_ALEN))) {
			LEAVE();
			return i;
		}
	}
	LEAVE();
	return -1;
}

/*
 * @brief process Device selection input from user
 *
 * @param sel_index Index of device selected.
 * @return          Status
 */
int wfd_peer_device_selected()
{
	int i, ret = WPS_STATUS_SUCCESS;
	WFD_DATA *pwfd_data = &wps_global.wfd_data;

	ENTER();

	ret = os_mutex_get(&wps.p2p_session, 10);

	if (ret == WM_SUCCESS) {

		/* Service only if find is in progress */
		if (pwfd_data->dev_index != -1 ||
			pwfd_data->num_find_results == 0) {
			LEAVE();
			return WPS_STATUS_FAIL;
		}

		/** Map the selected index to actual array index */
		for (i = 0; i < pwfd_data->num_find_results; i++) {
			if (!(memcmp(g_bssid,
				pwfd_data->find_results[i].device_id, ETH_ALEN))
				&& pwfd_data->find_results[i].is_valid == 1) {
				break;
			}
		}

		/** value of i is actual index here */
		pwfd_data->dev_index = i;

		if (pwfd_data->dev_index < pwfd_data->num_find_results) {
			/** Cancel the peer ageout timer here */
			wps_cancel_timer(wfd_peer_ageout_time_handler, pwfd_data);
			wps_printf(DEBUG_WFD_DISCOVERY,
					"Index Selected : %d\r\n", i);

			wps_printf(DEBUG_WFD_DISCOVERY,
					"Device %d] selected for connection using "
					"display_index.\n",
					i);
			wps_printf(DEBUG_WFD_DISCOVERY,
					"Device %d] selected for connection "
					"using actual_index.\n",
					pwfd_data->dev_index);

			if (!(pwfd_data->find_results[pwfd_data->dev_index].
						group_capability &
						WFD_GROUP_OWNER_MASK)) {
				/* start FIND phase */
				wps_printf(DEBUG_WFD_DISCOVERY, "Sending GO "
						"negotiation request.\n");

				ret = wfdcmd_action_frame(pwfd_data->dev_index,
						WFD_GO_NEG_REQ_ACTION_SUB_TYPE);

			} else {
				/* start provision discovery */
				wps_printf(DEBUG_WFD_DISCOVERY,
						"Sending provision discovery.\n");

				ret = wfdcmd_action_frame(pwfd_data->dev_index,
					WFD_PROVISION_DISCOVERY_REQ_SUB_TYPE);
			}
		}
	}
	LEAVE();

	return ret;
}

/*
 * @brief process WFD PEER Detected event data
 *
 * @param buffer   Pointer to received buffer
 * @param size     Length of the received event data
 * @return         N/A
 */
void wfd_process_peer_detected_event(u8 *buffer, u16 size)
{
	WPS_DATA *wps_s = (WPS_DATA *) &wps_global;
	WFD_DATA *pwfd_data = &wps_global.wfd_data;
	apeventbuf_wfd_generic *wfd_event;
	u16 event_type = 0;
	u16 event_sub_type = 0;
	u8 device_role = 0;
	wfd_event = (apeventbuf_wfd_generic *) (buffer);
	event_type = wlan_le16_to_cpu(wfd_event->event_type);
	u16 ret;

	ENTER();

	memcpy(&event_sub_type, (u8 *) &(wfd_event->event_sub_type), 2);
	event_sub_type = wlan_le16_to_cpu(event_sub_type);
	device_role = event_sub_type & DEVICE_ROLE_MASK;

	switch (event_type) {
	case EV_TYPE_WFD_PEER_DETECTED:
		/* Service event only if find is in progress */
		if (pwfd_data->dev_index != -1) {
			break;
		}
		ret = os_mutex_get(&wps.p2p_session, 10);
		if (ret == WM_SUCCESS) {
			/* Update find table information for this event */
			wfd_update_find_table(wfd_event);

			os_mutex_put(&wps.p2p_session);
		}
		break;

	case EV_TYPE_WFD_NEGOTIATION_REQUEST:

		ret = os_mutex_get(&wps.p2p_session, 10);
		if (ret == WM_SUCCESS) {

			go_request = 1;
			if (device_role == EV_SUBTYPE_WFD_GO_NEG_FAILED) {
				/* Go back to discovery */
				break;
			}

			memset(&wps_s->wfd_data, 0, sizeof(WFD_DATA));

			wfd_check_go_neg_status(event_type, wfd_event);

			/*
			 * Update find table information for this event, if
			 * device was in silent mode during find phase
			 */
			wfd_update_find_table(wfd_event);

			/* Find device index correspoding to this peer */
			if (pwfd_data->dev_index == -1) {
				pwfd_data->dev_index = wfd_corelate_event_peer
					(wfd_event->peer_mac_addr);
			}
			/*
			 * The intended interface address is also updated while
			 * updating find phase table, copy it to registrar,
			 * enrollee mac address. Use it based on the GO
			 * negotiation result event.
			 */
			memcpy(gpwps_info->registrar.mac_address,
				pwfd_data->find_results[pwfd_data->dev_index].
				intended_address, ETH_ALEN);
			memcpy(gpwps_info->enrollee.mac_address,
				pwfd_data->find_results[pwfd_data->dev_index].
				intended_address, ETH_ALEN);
			memcpy(intended_mac_addr,
				pwfd_data->find_results[pwfd_data->dev_index].
				intended_address, ETH_ALEN);
			wps_printf(DEBUG_WFD_DISCOVERY,
				"Peer Enrollee/Registrar: " MACSTR,
				MAC2STR(gpwps_info->registrar.mac_address));

			os_mutex_put(&wps.p2p_session);
		}
		break;

	default:
		memcpy(evt_buffer, buffer, size);

		if (&wps.event_queue) {
			wfd_other_event.buffer = evt_buffer;
			os_queue_send(&wps.event_queue,
				&wfd_other_event, OS_NO_WAIT);
		}
		break;
	}
	LEAVE();
}

/*
 * @brief process WFD generic event data
 *
 * @param buffer   Pointer to received buffer
 * @param size     Length of the received event data
 * @return         N/A
 */
void wfd_process_generic_event(u8 *buffer, u16 size)
{
	int ret = WPS_STATUS_SUCCESS;
	int sel_index = 0;
	WFD_DATA *pwfd_data = &wps_global.wfd_data;
	apeventbuf_wfd_generic *wfd_event;
	u16 event_type = 0;
	u16 event_sub_type = 0;
	u8 device_role = 0;
	bss_config_t bss_config;
	wfd_event = (apeventbuf_wfd_generic *) (buffer);
	event_type = wlan_le16_to_cpu(wfd_event->event_type);
	WPS_DATA *wps_s = (WPS_DATA *) &wps_global;

	ENTER();

	memcpy(&event_sub_type, (u8 *) &(wfd_event->event_sub_type), 2);
	event_sub_type = wlan_le16_to_cpu(event_sub_type);
	device_role = event_sub_type & DEVICE_ROLE_MASK;

	switch (event_type) {
	case EV_TYPE_WFD_NEGOTIATION_REQUEST:
	case EV_TYPE_WFD_NEGOTIATION_RESPONSE:
		if (device_role == EV_SUBTYPE_WFD_GO_NEG_FAILED) {
			/* Go back to discovery */
			break;
		}

		wfd_check_go_neg_status(event_type, wfd_event);

		/*
		 * Update find table information for this event, if
		 * device was in silent mode during find phase
		 */
		wfd_update_find_table(wfd_event);

		/* Find device index correspoding to this peer */
		if (pwfd_data->dev_index == -1) {
			pwfd_data->dev_index = wfd_corelate_event_peer
			    (wfd_event->peer_mac_addr);
		}
		/*
		 * The intended interface address is also updated while updating
		 * find phase table, copy it to registrar, enrollee mac address.
		 * Use it based on the GO negotiation result event.
		 */
		memcpy(gpwps_info->registrar.mac_address,
		       pwfd_data->find_results[pwfd_data->dev_index].
		       intended_address, ETH_ALEN);
		memcpy(gpwps_info->enrollee.mac_address,
		       pwfd_data->find_results[pwfd_data->dev_index].
		       intended_address, ETH_ALEN);
		memcpy(intended_mac_addr,
		       pwfd_data->find_results[pwfd_data->dev_index].
		       intended_address, ETH_ALEN);
		wps_printf(DEBUG_WFD_DISCOVERY,
			   "Peer Enrollee/Registrar: " MACSTR,
			   MAC2STR(gpwps_info->registrar.mac_address));

		break;

	case EV_TYPE_WFD_NEGOTIATION_RESULT:

		/*
		 * Update find table information for this event, if
		 * device was in silent mode during find phase
		 */
		wfd_update_find_table(wfd_event);

		wps_cancel_timer(wfd_peer_selected_ageout_time_handler,
					pwfd_data);

		if (wps.cb(P2P_SESSION_STARTED, NULL, 0)
				== -WM_FAIL)
			P2P_LOG("WFD Callback failed for "
					"event: %d",
					P2P_SESSION_STARTED);

		if (device_role == EV_SUBTYPE_WFD_GO_ROLE) {
			P2P_LOG("Starting Role Negotiation now");

			/* WFD Registrar */
			P2P_LOG("Group Owner Role");
			wps_printf(DEBUG_WFD_DISCOVERY,
				   "Event SubType = Group Owner Role");
			gpwps_info->discovery_role = WPS_REGISTRAR;
			wfd_bss_type = BSS_TYPE_UAP;

			if ((gpwps_info->registrar.device_password_id ==
			     DEVICE_PASSWORD_ID_PIN)
			    && (gpwps_info->pin_generator ==
				WFD_AUTO_DISPLAY_PIN)) {

				gpwps_info->enrollee.
				    updated_device_password_id =
				    DEVICE_PASSWORD_REG_SPECIFIED;
				gpwps_info->registrar.
				    updated_device_password_id =
				    DEVICE_PASSWORD_REG_SPECIFIED;
			}

			/*
			 * Update Group formation bit when starting as registrar
			 */
#if 0
			if (wps_wlan_update_group_formation_config(WPS_SET) !=
			    WPS_STATUS_SUCCESS) {
				wps_printf(DEBUG_WFD_DISCOVERY,
				   "Failed to update Group formation bit.\n");
				break;
			}
#endif
			wps_printf(DEBUG_WFD_DISCOVERY,
				   "Updated Group formation bit.");

			/* Find device index correspoding to this peer */
			if (pwfd_data->dev_index == -1) {
				pwfd_data->dev_index = wfd_corelate_event_peer
				    (wfd_event->peer_mac_addr);
			}
			/* Registrar Process */
			ie_index = 0x0002;

			ret =
			    wps_wlan_ie_config(SET_WPS_AP_SESSION_ACTIVE_IE,
					       &ie_index);
			if (ret != WPS_STATUS_SUCCESS) {
				wps_printf(MSG_WARNING,
					   "WPS IE configuration failure.\n");
			}

			/*
			 * Read AP configuration now and switch to AP interface
			 */
#if 0
			if (wps_wlan_set_bss_type(BSS_TYPE_UAP) !=
			    WPS_STATUS_SUCCESS) {
				wps_printf(DEBUG_WFD_DISCOVERY,
					   "Failed to set uAP role.\n");
				break;
			}
#endif
			/* Update BSS type */
			wps_s->bss_type = BSS_TYPE_UAP;

			/* Set intended address before starting GO */
			if (memcmp
			    (gpwps_info->wfd_intended_addr,
			     "\x00\x00\x00\x00\x00\x00", ETH_ALEN)) {
#if 0
				if (wlan_set_intended_mac_addr
				    (gpwps_info->wfd_intended_addr)) {
					wps_printf(DEBUG_WFD_DISCOVERY,
					   "Failed to set mac address.\n");
					break;
				}
				memcpy(gpwps_info->registrar.mac_address,
				       gpwps_info->wfd_intended_addr, ETH_ALEN);
#endif
			}
			if (wfd_wlan_update_bss_mode(BSS_MODE_WIFIDIRECT_GO)
					!= WPS_STATUS_SUCCESS) {
				wps_printf(MSG_ERROR, "ERROR - Fail to "
						"initialize BSS Mode!\n");
				break;
			}
			if (wfd_set_mode(WFD_MODE_START_GROUP_OWNER)
						!= WPS_STATUS_SUCCESS) {
				wps_printf(MSG_ERROR, "ERROR - Fail to "
						"initialize WFD!\n");
				break;
			}
			wps_printf(DEBUG_WFD_DISCOVERY,
				   "Issued Start GO mode command.");
			if (!wps_s->current_ssid.ssid_len) {
				apcmd_get_bss_config(&bss_config);

				load_cred_info(wps_s, gpwps_info,
						&bss_config);
				/*
				 * For Wi-Fi Direct, we need to
				 * convert the passphrase to PSK.
				 *
				 * Hence update the default
				 * passphrase
				 */

				wlan_generate_psk(gpwps_info);
			}

			if (wps.cb(P2P_GO_STARTED, &WFD_devicename,
						strlen(WFD_devicename))
					== -WM_FAIL)
				P2P_LOG("WFD Callback failed for "
						"event: %d",
						P2P_GO_STARTED);

			p2p_uap_network.channel_specific = 1;
			p2p_uap_network.channel = op_channel_number;

			if (wlan_add_network(&p2p_uap_network) !=
					WLAN_ERROR_NONE) {
				wps_printf(MSG_ERROR, "failed to add "
						"wfd network\r\n");
				if (wps.cb(P2P_FAILED, NULL, 0) == -WM_FAIL)
					P2P_LOG("WPS Callback failed for event:"
						" %d\r\n", P2P_FAILED);
				break;
			}
			if (wlan_start_network(p2p_uap_network.name) !=
					WLAN_ERROR_NONE) {
				wps_printf(MSG_ERROR, "failed to start "
						"wfd network\r\n");
				if (wps.cb(P2P_FAILED, NULL, 0) == -WM_FAIL)
					P2P_LOG("WPS Callback failed for "
						"event: %d\r\n", P2P_FAILED);
				break;
			}

			gpwps_info->wps_session = 1;

			auto_go = 1;

			ret = wps_registrar_start(gpwps_info, &wps_global);
		}

		else if (device_role == EV_SUBTYPE_WFD_CLIENT_ROLE) {

			P2P_LOG("Starting Role Negotiation now");
			mlanconfig_bgscan(0);

			sel_index = pwfd_data->dev_index;
			/*
			 * Start an enrollee, as peer has GO capability and
			 * Provision discovery response is +ve
			 */
			gpwps_info->discovery_role = WPS_ENROLLEE;

			g_bssid = intended_mac_addr;
			g_ssid = pwfd_data->find_results[sel_index].go_ssid;
			g_channel = 0;
			if (pwfd_data->find_results[sel_index].op_channel > 0
				&& pwfd_data->find_results[sel_index].
				op_channel <= 11)
				g_channel =
				pwfd_data->find_results[sel_index].op_channel;

			/* P2P Enrollee */
			P2P_LOG("Client Role");
			wps_printf(DEBUG_WFD_DISCOVERY,
				   "Event SubType = Client Role");
			wfd_bss_type = BSS_TYPE_STA;


			/* Intended address will be used for connection */
			memcpy(pwfd_data->find_results[sel_index].
			       intended_address,
			       pwfd_data->find_results[sel_index].device_id,
			       ETH_ALEN);
			memcpy(gpwps_info->registrar.mac_address,
			       pwfd_data->find_results[sel_index].device_id,
			       ETH_ALEN);

			/*
			 * Send command to FW to stop Find phase, [mode 5];
			 * else bg_scan causes issues in 802_11_ret_scan
			 */
			if (wfd_set_mode(WFD_MODE_STOP_FIND_PHASE) !=
			    WPS_STATUS_SUCCESS) {
				wps_printf(DEBUG_WFD_DISCOVERY,
					   "Failed to stop find phase.\n");
				break;
			}

			if (wfd_wlan_update_bss_mode(BSS_MODE_WIFIDIRECT_CLIENT)
					!= WPS_STATUS_SUCCESS) {
				wps_printf(MSG_ERROR, "ERROR - Fail to "
						"initialize BSS Mode!\n");
			}

			/* Find device index correspoding to this peer */
			if (pwfd_data->dev_index == -1) {
				pwfd_data->dev_index = wfd_corelate_event_peer
				    (wfd_event->peer_mac_addr);
			}
			/*
			 * Check for Intended Interface address
			 * and update the corresponding device Id.
			 */
			if (memcmp
			    (gpwps_info->wfd_intended_addr,
			     "\x00\x00\x00\x00\x00\x00", ETH_ALEN)) {
#if 0
				if (wlan_set_intended_mac_addr
				    (gpwps_info->wfd_intended_addr)) {
					wps_printf(DEBUG_WFD_DISCOVERY,
					   "Failed to set mac address.\n");
				}
				memcpy(gpwps_info->enrollee.mac_address,
				       gpwps_info->wfd_intended_addr, ETH_ALEN);
#endif
			}

			/* Check peer intended address before connection */
			if (memcmp
			    (pwfd_data->find_results[pwfd_data->dev_index].
			     intended_address, "\x00\x00\x00\x00\x00\x00",
			     ETH_ALEN) == 0) {
				memcpy(pwfd_data->
				       find_results[pwfd_data->
						    dev_index].intended_address,
				       pwfd_data->find_results[pwfd_data->
							       dev_index].
				       device_id, ETH_ALEN);
			}

			/*
			 * Send command to Station for starting as Device,
			 * [mode 3]
			 */
			if (wfd_set_mode(WFD_MODE_START_DEVICE) !=
			    WPS_STATUS_SUCCESS) {
				wps_printf(DEBUG_WFD_DISCOVERY,
					   "Failed to start device mode.\n");
				break;
			}

			if ((gpwps_info->enrollee.device_password_id ==
			     DEVICE_PASSWORD_ID_PIN)
			    && (gpwps_info->pin_generator ==
				WFD_AUTO_ENTER_PIN)) {
				wps_printf(DEBUG_WFD_DISCOVERY,
				   "Changing Device Password ID "
					"to REG_SPECIFIED");
				gpwps_info->enrollee.
				    updated_device_password_id =
				    DEVICE_PASSWORD_REG_SPECIFIED;
				gpwps_info->registrar.
				    updated_device_password_id =
				    DEVICE_PASSWORD_REG_SPECIFIED;

			}

			if (wps.cb(P2P_CLIENT_STARTED, NULL, 0)
					== -WM_FAIL)
				P2P_LOG("WFD Callback failed for "
						"event: %d",
						P2P_CLIENT_STARTED);

			gpwps_info->wps_session = 1;

			wps_printf(DEBUG_WFD_DISCOVERY,
				   "Starting enrollee now.");

			ret = wps_enrollee_start(gpwps_info, &wps_global);
			if (ret == WPS_STATUS_FAIL)
				wfd_reset();
		} else if (device_role == EV_SUBTYPE_WFD_GO_NEG_FAILED) {
			wps_printf(MSG_ERROR, "WFD GO Negotiation Failed!");
			ret = WPS_STATUS_FAIL;
			/** Restart peer ageout timer */
			/* wfd_start_peer_ageout_timer(pwfd_data); */
			/* Go back to discovery */
			pwfd_data->dev_index = -1;
		}

		if (pwfd_data->dev_index != -1 && ret != WPS_STATUS_SUCCESS) {
			wps_printf(MSG_ERROR,
				   "WFD Registrar, Enrollee failure...");

			/* Shutdown the main processing loop */
			wps_main_loop_shutdown();
		}
		break;
	case EV_TYPE_WFD_PROVISION_DISCOVERY_RESP:

		if (pwfd_data->dev_index == -1) {
			break;
		}
		/*
		 * Check if provision discovery response is successful and
		 * has right config methods.
		 */
		wps_cancel_timer(wfd_peer_selected_ageout_time_handler,
					pwfd_data);

		if (ret == WPS_STATUS_SUCCESS) {
			if (wps.cb(P2P_SESSION_STARTED, NULL, 0)
				== -WM_FAIL)
				P2P_LOG("WFD Callback failed for "
					"event: %d",
					P2P_SESSION_STARTED);

			sel_index = pwfd_data->dev_index;
			/*
			 * Start an enrollee, as peer has GO capability and
			 * Provision discovery response is +ve
			 */
			gpwps_info->discovery_role = WPS_ENROLLEE;

			g_bssid = pwfd_data->find_results[sel_index].device_id;
			g_ssid = pwfd_data->find_results[sel_index].go_ssid;
			g_channel = 0;
			if (pwfd_data->find_results[sel_index].op_channel > 0
				&& pwfd_data->find_results[sel_index].
				op_channel <= 11)
				g_channel =
				pwfd_data->find_results[sel_index].op_channel;
			P2P_LOG
			    ("Starting Role Negotiation with : %s "
				"[%02x:%02x:%02x:%02x:%02x:%02x]",
			     g_ssid,
			     g_bssid[0],
			     g_bssid[1],
			     g_bssid[2], g_bssid[3], g_bssid[4], g_bssid[5]);

			/* sleep as per peer's GO config timeout, not needed. */
			wps_printf(DEBUG_WFD_DISCOVERY,
				   "Starting enrollee now.");
			/* Intended address will be used for connection */
			memcpy(pwfd_data->find_results[sel_index].
			       intended_address,
			       pwfd_data->find_results[sel_index].device_id,
			       ETH_ALEN);
			memcpy(gpwps_info->registrar.mac_address,
			       pwfd_data->find_results[sel_index].device_id,
			       ETH_ALEN);

			/*
			 * Send command to FW to stop Find phase, [mode 5];
			 * else bg_scan causes issues in 802_11_ret_scan
			 */
			if (wfd_set_mode(WFD_MODE_STOP_FIND_PHASE) !=
			    WPS_STATUS_SUCCESS) {
				wps_printf(DEBUG_WFD_DISCOVERY,
					   "Failed to stop find phase.\n");
				break;
			}

			/*
			 * Check for Intended Interface address
			 * and update the corresponding device Id.
			 */
			if (memcmp
			    (gpwps_info->wfd_intended_addr,
			     "\x00\x00\x00\x00\x00\x00", ETH_ALEN)) {
#if 0
				if (wlan_set_intended_mac_addr
				    (gpwps_info->wfd_intended_addr)) {
					wps_printf(DEBUG_WFD_DISCOVERY,
					   "Failed to set mac address.\n");
				}
				memcpy(gpwps_info->enrollee.mac_address,
				       gpwps_info->wfd_intended_addr, ETH_ALEN);
#endif
			}

			/*
			 * Send command to Station for starting as Device,
			 * [mode 3]
			 */
			if (wfd_set_mode(WFD_MODE_START_DEVICE) !=
			    WPS_STATUS_SUCCESS) {
				wps_printf(DEBUG_WFD_DISCOVERY,
					   "Failed to start device mode.\n");
				break;
			}

			if ((gpwps_info->enrollee.device_password_id ==
			     DEVICE_PASSWORD_ID_PIN)
			    && (gpwps_info->pin_generator ==
				WFD_AUTO_ENTER_PIN)) {
				wps_printf(DEBUG_WFD_DISCOVERY,
				   "Changing Device Password ID "
					"to REG_SPECIFIED");
				gpwps_info->enrollee.
				    updated_device_password_id =
				    DEVICE_PASSWORD_REG_SPECIFIED;
				gpwps_info->registrar.
				    updated_device_password_id =
				    DEVICE_PASSWORD_REG_SPECIFIED;

			}

			P2P_LOG("Client");

			gpwps_info->wps_session = 1;

			ret = wps_enrollee_start(gpwps_info, &wps_global);
		}
		break;

	case EV_TYPE_WFD_INVITATION_REQ:
	case EV_TYPE_WFD_INVITATION_RESP:
		{
			ret =
			    wfd_handle_persistent_group_invoked_event
			    (gpwps_info, &wps_global, wfd_event);

			if (ret == WPS_STATUS_SUCCESS) {
				/* We are done now, exit gracefully */
				/* Unregister user abort handler */
				/*wps_unregister_rdsock_handler(STDIN_FILENO);*/

				/* Shutdown the main processing loop */
				wps_main_loop_shutdown();
			}
			break;
		}


	default:
		break;
	}

	LEAVE();
}

/*
 *  @brief Process WFD Persistent group invoked event
 *  @param pwps_info A pointer to the PWPS_INFO structure
 *  @param wps_s     A pointer to the WPS_DATA structure
 *  @param wfd_event Event data pointer
 *  @return          WPS_STATUS_SUCCESS--success, WPS_STATUS_FAIL--fail
 */
int
wfd_handle_persistent_group_invoked_event(PWPS_INFO pwps_info,
					  WPS_DATA *wps_s,
					  apeventbuf_wfd_generic *wfd_event)
{
#if 0
	u8 index, dev_role, process_state;
	int ret = WPS_STATUS_SUCCESS;
	wfd_persistent_record prec;
	s8 psk_hexstr[64];
	u16 event_sub_type = 0;

	ENTER();

	memcpy(&event_sub_type, (u8 *) &(wfd_event->event_sub_type), 2);
	event_sub_type = wlan_le16_to_cpu(event_sub_type);

	dev_role = event_sub_type & DEVICE_ROLE_MASK;

	process_state = (event_sub_type & PKT_PROCESS_STATE_MASK) >> 2;

	index = wlan_le32_to_cpu(*(u32 *) wfd_event->entire_ie_list);

	wps_printf(DEBUG_WFD_DISCOVERY,
	   "EV: PERSISTENT_GROUP_INVOKED dev_role: %d, state: %d, index: %d\n",
		   dev_role, process_state, index);

	if (process_state != EV_SUBTYPE_WFD_REQRECV_PROCESSING_SUCCESS) {
		/* Ignore other states now */
		ret = WPS_STATUS_FAIL;
		goto done;
	}

	/* 1st read the persistent ssid/psk from the FW */
	memset(&prec, 0, sizeof(wfd_persistent_record));
	prec.index = index;

#if 0
	if ((ret = wfd_cfg_cmd_persistent_group_record(ACTION_GET, &prec)) !=
	    WPS_STATUS_SUCCESS) {
		wps_printf(MSG_ERROR,
		   "EV: PERSISTENT_GROUP_INVOKED read group rec failed\n");
		goto done;
	}
#endif

	/* Convert PSK in bin form to hex str */
	bin2hexstr(prec.psk, psk_hexstr, prec.psk_len);

	wps_printf(DEBUG_WFD_DISCOVERY, "got per rec: dev_role: %d\n",
		   prec.dev_role);
#if 0
	if ((prec.psk_len * 2) != 64) {
		wps_create_supp_file((u8 *) "wfd.conf", prec.ssid,
				     prec.ssid_len, (u8 *) psk_hexstr,
				     prec.psk_len);
	} else {
		wps_create_supp_file((u8 *) "wfd.conf", prec.ssid,
				     prec.ssid_len, (u8 *) psk_hexstr, 64);
	}
#endif
	wps_printf(DEBUG_WFD_DISCOVERY, "got per rec: dev_role: %d\n",
		   prec.dev_role);

	if (prec.dev_role == EV_SUBTYPE_WFD_CLIENT_ROLE) {
		gpwps_info->discovery_role = WPS_ENROLLEE;
#if 0
		/* Send command to Station for starting as Device, [mode 3] */
		if ((ret = wfd_wlan_update_mode(WFD_MODE_START_DEVICE)) !=
		    WPS_STATUS_SUCCESS) {
			wps_printf(MSG_ERROR, "Failed to start device mode.\n");
			goto done;
		}
#endif
	} else if (prec.dev_role == EV_SUBTYPE_WFD_GO_ROLE) {
		gpwps_info->discovery_role = WPS_REGISTRAR;
		wps_printf(DEBUG_WLAN, "Set BSS Type to UAP: %d\n",
			   prec.dev_role);
#if 0
		if (wps_wlan_set_bss_type(BSS_TYPE_UAP) != WPS_STATUS_SUCCESS) {
			wps_printf(MSG_ERROR, "Failed to set uAP role.\n");
			goto done;
		}
#endif
		/* Update BSS type */
		wps_s->bss_type = BSS_TYPE_UAP;

		wps_printf(DEBUG_WLAN, "Set WFD Mode to GO\n");
		/* Send command to AP for starting in GO mode, [mode 2] */
#if 0
		if ((ret = wfd_wlan_update_mode(WFD_MODE_START_GROUP_OWNER)) !=
		    WPS_STATUS_SUCCESS) {
			wps_printf(MSG_ERROR, "Failed to start GO.\n");
			goto done;
		}
#endif
		wps_printf(DEBUG_WLAN, "Starting BSS!\n");
		os_thread_sleep(3);
		/* Start BSS now. */
#if 0
		if ((ret = apcmd_start_bss()) == WPS_STATUS_FAIL) {
			wps_printf(MSG_ERROR, "Failed to start GO AP.\n");
			goto done;
		}
#endif
	}

done:
	LEAVE();
	return ret;
#endif
	return WPS_STATUS_SUCCESS;
}

/*
 *  @brief       Read/Write WFD Persistent group database in FW
 *  @action      ACTION_GET or ACTION_SET
 *  @param prec  Pointer to wfd_persistent_record structure
 *  @return      WPS_STATUS_SUCCESS--success, WPS_STATUS_FAIL--fail
 */
int
wfd_cfg_cmd_persistent_group_record(int action, wfd_persistent_record *prec)
{
	wfd_params_config *cmd_buf = NULL;
	tlvbuf_wfd_persistent_group *tlv = NULL;
	u8 index = 0, var_len = 0;
	u8 *buffer = NULL;
	u16 cmd_len = 0, buf_len = 0;
	int pi;
	ENTER();

	/* error checks */
	if (prec->index >= WFD_PERSISTENT_GROUP_MAX) {
		wps_printf(MSG_ERROR,
			   "ERR:wrong index. Value values are [0-3]\n");
		LEAVE();
		return WPS_STATUS_FAIL;
	}

	cmd_len = sizeof(wfd_params_config);
	buffer = (u8 *) wps_mem_malloc(MRVDRV_SIZE_OF_CMD_BUFFER);
	if (!buffer) {
		wps_printf(MSG_ERROR, "ERR:Cannot allocate memory!\n");
		LEAVE();
		return WPS_STATUS_FAIL;
	}

	memset(buffer, 0, MRVDRV_SIZE_OF_CMD_BUFFER);
	cmd_buf = (wfd_params_config *) buffer;
	cmd_buf->cmd_head.cmd_code = HostCmd_CMD_WFD_PARAMS_CONFIG;
	cmd_buf->cmd_head.seq_num = 0;
	cmd_buf->cmd_head.result = 0;
	tlv =
	    (tlvbuf_wfd_persistent_group *) (buffer +
					     sizeof(wfd_params_config));

	tlv->tag = MRVL_WFD_PERSISTENT_GROUP_TLV_ID;

	if (action == ACTION_GET) {
		cmd_buf->action = ACTION_GET;
		tlv->rec_index = prec->index;
		tlv->length = sizeof(prec->index);
		cmd_len += TLV_HEADER_SIZE + tlv->length;
		cmd_buf->cmd_head.size = cmd_len - BUF_HEADER_SIZE;
		buf_len = MRVDRV_SIZE_OF_CMD_BUFFER;
		goto send_cmd;
	}

	cmd_buf->action = wlan_cpu_to_le16(ACTION_SET);
	tlv->rec_index = prec->index;
	tlv->dev_role = prec->dev_role;
	memcpy(tlv->group_bssid, prec->bssid, ETH_ALEN);
	memcpy(tlv->dev_mac_address, prec->groupdevid, ETH_ALEN);

	/* ssid */
	tlv->group_ssid_len = prec->ssid_len;
	var_len += tlv->group_ssid_len;
	memcpy(tlv->group_ssid, prec->ssid, tlv->group_ssid_len);

	/* adjust pointer from here */
	/* psk */
	*(&tlv->psk_len + var_len) = prec->psk_len;
	memcpy(tlv->psk + var_len, prec->psk, prec->psk_len);
	var_len += prec->psk_len;

	tlv->length = sizeof(tlvbuf_wfd_persistent_group) -
	    TLV_HEADER_SIZE + var_len;

	*(&tlv->num_peers + var_len) = prec->num_peers;

	for (pi = 0; pi < prec->num_peers; pi++) {
		memcpy(tlv->peer_mac_addrs[pi] + var_len, prec->peers[pi],
		       ETH_ALEN);
	}

	tlv->length += prec->num_peers * ETH_ALEN;

	cmd_len += TLV_HEADER_SIZE + tlv->length;
	buf_len = cmd_len;
	cmd_buf->cmd_head.size = cmd_len - BUF_HEADER_SIZE;

send_cmd:
	endian_convert_tlv_header_out(tlv);
	if (wfd_ioctl((u8 *) buffer, cmd_len) == WPS_STATUS_SUCCESS)
		wps_printf(DEBUG_WFD_DISCOVERY,
			   "WFD Persistent group reading successful.");
	else {
		wps_printf(MSG_WARNING,
			   "WFD Persistent group reading failed");
	}

	endian_convert_tlv_header_in(tlv);

	if (action == ACTION_GET) {
		if (tlv->length >
		    (sizeof(tlvbuf_wfd_persistent_group) - sizeof(u8)))
			wmprintf("Persistent group information =>\r\n");
		else
			wps_printf(DEBUG_WFD_DISCOVERY,
				   "Persistent group information is empty.");
		buf_len = tlv->length;
		while (buf_len >
		       (sizeof(tlvbuf_wfd_persistent_group) - sizeof(u8))) {
			wmprintf("\r\n\t Index = [%d]\r\n", tlv->rec_index);
			prec->index = tlv->rec_index;
			wmprintf("\r\n\t Dev_Role = [%d]\r\n", tlv->dev_role);
			wmprintf("\t Role  = %s\r\n",
				 (tlv->dev_role ==
				  EV_SUBTYPE_WFD_GO_ROLE) ? "Group owner" :
				 "Client");
			prec->dev_role = tlv->dev_role;
			wmprintf("\t GroupBssId - ");
			print_mac1(tlv->group_bssid);
			memcpy(prec->bssid, tlv->group_bssid, ETH_ALEN);
			wmprintf("\r\n\t DeviceId - ");
			print_mac1(tlv->dev_mac_address);
			memcpy(prec->groupdevid, tlv->dev_mac_address,
			       ETH_ALEN);
			wmprintf("\r\n\t SSID = ");
			for (index = 0; index < tlv->group_ssid_len; index++)
				wmprintf("%c", tlv->group_ssid[index]);

			prec->ssid_len = tlv->group_ssid_len;
			memcpy(prec->ssid, tlv->group_ssid,
			       tlv->group_ssid_len);

			var_len = tlv->group_ssid_len;
			wmprintf("\r\n\t PSK = ");
			for (index = 0; index < *(&tlv->psk_len + var_len);
			     index++) {
				if (index == 16)
					wmprintf("\r\n\t       ");
				wmprintf("%02x ",
					 *(&tlv->psk[index] + var_len));
			}

			prec->psk_len = *(&tlv->psk_len + var_len);
			memcpy(prec->psk, tlv->psk + var_len, prec->psk_len);

			var_len += *(&tlv->psk_len + var_len);
			if (tlv->dev_role == EV_SUBTYPE_WFD_GO_ROLE) {
				prec->num_peers = *(&tlv->num_peers + var_len);
				for (pi = 0; pi < prec->num_peers; pi++) {
					memcpy(prec->peers[pi],
					       tlv->peer_mac_addrs[pi] +
					       var_len, ETH_ALEN);
					wmprintf("\r\n\t Peer "
					"Mac address(%d) = ",
						 pi);
					print_mac1(prec->peers[pi]);
				}
			}
			if (action == ACTION_GET)
				wmprintf
			    ("\r\n\t --------------------------------------");
			if (tlv->dev_role == EV_SUBTYPE_WFD_GO_ROLE) {
				buf_len -= sizeof(tlvbuf_wfd_persistent_group) -
				    TLV_HEADER_SIZE + var_len;
				tlv =
				    (tlvbuf_wfd_persistent_group
				     *) (((u8 *) (tlv)) +
					 (sizeof(tlvbuf_wfd_persistent_group)
					  - TLV_HEADER_SIZE + var_len));
			} else {
				/* num_peer field willnt be present */
				buf_len -= sizeof(tlvbuf_wfd_persistent_group) -
				    TLV_HEADER_SIZE - sizeof(u8) + var_len;
				tlv =
				    (tlvbuf_wfd_persistent_group
				     *) (((u8 *) (tlv)) +
					 (sizeof(tlvbuf_wfd_persistent_group)
					  - TLV_HEADER_SIZE - sizeof(u8) +
					  var_len));
			}
		}
		wmprintf("\n");
	} else {
		wps_printf(DEBUG_WFD_DISCOVERY,
			   "Setting persistent group information "
				"successful!");
	}

	if (buffer)
		wps_mem_free(buffer);

	LEAVE();
	return WPS_STATUS_SUCCESS;
}

/*
 *  @brief Return index of persistent group record matching the given SSID
 *  @param ssid_length Length of ssid to lookup
 *  @param prec        ssid to lookup
 *  @param pr_index    Index of matching record
 *  @return      WPS_STATUS_SUCCESS--success, WPS_STATUS_FAIL--fail
 */

int
wfd_get_persistent_rec_ind_by_ssid(int ssid_length, u8 *ssid, s8 *pr_index)
{
	int index, ret = WPS_STATUS_SUCCESS;
	wfd_persistent_record prec;

	ENTER();

	for (index = 0; index < WFD_PERSISTENT_GROUP_MAX; index++) {
		memset(&prec, 0, sizeof(wfd_persistent_record));

		prec.index = index;
		if ((ret =
		     wfd_cfg_cmd_persistent_group_record(ACTION_GET,
							 &prec)) !=
		    WPS_STATUS_SUCCESS) {
			goto done;
		}

		if ((prec.ssid_len == ssid_length) &&
		    !memcmp(prec.ssid, ssid, ssid_length)) {
			/* correct record found */
			*pr_index = index;
			goto done;
		}
	}

	/* record not found */
	ret = WPS_STATUS_FAIL;

done:
	LEAVE();
	return ret;
}

/*
 *  @brief Return index of a free persistent group record
 *  @param pr_index    Index of matching record
 *  @return      WPS_STATUS_SUCCESS--success, WPS_STATUS_FAIL--fail
 */
int wfd_get_free_persistent_rec_index(s8 *pr_index)
{
	int index, ret = WPS_STATUS_SUCCESS;
	wfd_persistent_record prec;

	ENTER();

	for (index = 0; index < WFD_PERSISTENT_GROUP_MAX; index++) {
		memset(&prec, 0, sizeof(wfd_persistent_record));

		prec.index = index;
		if ((ret =
		     wfd_cfg_cmd_persistent_group_record(ACTION_GET,
							 &prec)) !=
		    WPS_STATUS_SUCCESS) {
			goto done;
		}

		if (prec.ssid_len == 0) {
			/* free record found */
			*pr_index = index;
			goto done;
		}
	}

	/* record not found */
	ret = WPS_STATUS_FAIL;

done:
	LEAVE();
	return ret;
}

/*
 *  @brief    Update a peristent record based on current info
 *  @param pwps_info   A pointer to the PWPS_INFO structure
 *  @return            WPS_STATUS_SUCCESS--success, WPS_STATUS_FAIL--fail
 */

int wfd_update_persistent_record(PWPS_INFO pwps_info)
{
	int ret = WPS_STATUS_SUCCESS;
	int registrar = (IS_DISCOVERY_REGISTRAR(pwps_info)) ? 1 : 0;
	wfd_persistent_record prec;
	WFD_DATA *pwfd_data = &wps_global.wfd_data;
	CREDENTIAL_DATA *reg_cred = (CREDENTIAL_DATA *)
	    &pwps_info->registrar.cred_data[0];

	ENTER();

/*XXX: Check disabled for now*/
#if 0
	if (!(pwps_info->self_p2p_grp_cap & WFD_GROUP_CAP_PERSIST_GROUP_BIT)) {
		/* Persistent group not enabled */
		return WPS_STATUS_FAIL;
	}
#endif

	memset(&prec, 0, sizeof(wfd_persistent_record));

	/* Pick the correct record */
	if ((ret = wfd_get_persistent_rec_ind_by_ssid(reg_cred->ssid_length,
						      reg_cred->ssid,
						      &prec.index)) !=
	    WPS_STATUS_SUCCESS) {
		/* FW didnt create a Persistent record entry */
		wps_printf(DEBUG_WFD_DISCOVERY,
			   "No persistent rec with req ssid found\n");
		prec.index = -1;
		if (pwps_info->discovery_role != WPS_ENROLLEE)
			return ret;
	}

	if (prec.index != -1) {
		/* First, read the record from FW */
		wps_printf(DEBUG_WFD_DISCOVERY, "Reading Rec from FW\n");
		wfd_cfg_cmd_persistent_group_record(ACTION_GET, &prec);
		wps_hexdump(MSG_INFO, "Prec:", (u8 *) &prec, sizeof(prec));
	} else {
		if (wfd_get_free_persistent_rec_index(&prec.index) ==
		    WPS_STATUS_FAIL) {
			/* No free persistent record entry found */
			return ret;
		}
		wps_printf(DEBUG_WFD_DISCOVERY,
			   "Creating new Persistent record @ index: %d\n",
			   prec.index);

		prec.dev_role = EV_SUBTYPE_WFD_CLIENT_ROLE;
		memcpy(prec.groupdevid, pwps_info->registrar.mac_address,
		       ETH_ALEN);
		prec.ssid_len = reg_cred->ssid_length;
		memcpy(prec.ssid, reg_cred->ssid, reg_cred->ssid_length);
	}

	if (!registrar) {
		/* For Enrollee, just update the PSK info */
		if (reg_cred->network_key_len == 64)
			prec.psk_len = reg_cred->network_key_len / 2;
		else
			prec.psk_len = reg_cred->network_key_len;

		hexstr2bin((s8 *) reg_cred->network_key,
			   prec.psk, reg_cred->network_key_len);
	} else {
		/* For Registrar, update the Client Device Address * Do not use
		   intended address */
		if (pwfd_data->dev_index != -1) {
			prec.num_peers = 1;
			memcpy(prec.peers[0],
			       pwfd_data->find_results[pwfd_data->dev_index].
			       device_address, ETH_ALEN);
		}
	}

	wps_printf(DEBUG_WFD_DISCOVERY, "Update Rec to FW\n");
	/* Write back to the FW */
	wfd_cfg_cmd_persistent_group_record(ACTION_SET, &prec);

	LEAVE();
	return ret;
}

/**
 *  @brief Start WFD Peer ageout timer
 *
 *  @param pwfd_data     A pointer to WFD_DATA
 */
void wfd_start_peer_ageout_timer(WFD_DATA *pwfd_data)
{
	u32 sec, usec;
	sec = WFD_PEER_AGEOUT_TIME;
	usec = 0;

	ENTER();

	wps_start_timer(sec, usec, wfd_peer_ageout_time_handler, pwfd_data);

	LEAVE();

	return;
}

/**
 *  @brief WFD peer ageout time time-out handler
 *
 *  @param user_data    User private data
 *  @return             None
 */
void wfd_peer_ageout_time_handler(void *user_data)
{
	WFD_DATA *pwfd_data = user_data;
	time_t current_time;
	u32 time_diff;
	int i = 0;

	ENTER();

	current_time = os_ticks_to_msec(os_ticks_get()) / 1000;

	for (i = 0; i < pwfd_data->num_find_results; i++) {
		time_diff =
		    difftime(current_time,
			     pwfd_data->find_results[i].timestamp);
		if (time_diff > WFD_PEER_AGEOUT_TIME) {
			pwfd_data->find_results[i].is_valid = 0;
			wps_printf(DEBUG_WLAN, "Last event from ");
			wps_printf(DEBUG_WLAN, "%02x:%02x:%02x:%02x:%02x:%02x",
				   (unsigned int)pwfd_data->find_results[i].
				   device_id[0],
				   (unsigned int)pwfd_data->find_results[i].
				   device_id[1],
				   (unsigned int)pwfd_data->find_results[i].
				   device_id[2],
				   (unsigned int)pwfd_data->find_results[i].
				   device_id[3],
				   (unsigned int)pwfd_data->find_results[i].
				   device_id[4],
				   (unsigned int)pwfd_data->find_results[i].
				   device_id[5]);
			wfd_cleanup_peer_entry(pwfd_data, i);
			wps_printf(DEBUG_WLAN, " came %d seconds ago.\n",
				   time_diff);
			/** Re print the table */
			wfd_reprint_find_table();
		}
	}

	if (!gpwps_info->wps_session) {
		/** Restart the timer */
		wps_start_timer(WFD_PEER_AGEOUT_TIME, 0,
				wfd_peer_ageout_time_handler, pwfd_data);
	}

	LEAVE();

	return;
}

/**
 *  @brief Start WFD Peer ageout timer
 *
 *  @param pwfd_data     A pointer to WFD_DATA
 */
void wfd_start_peer_selected_ageout_timer(WFD_DATA *pwfd_data)
{
	u32 sec, usec;
	sec = WFD_PEER_SELECTED_AGEOUT_TIME;
	usec = 0;

	ENTER();

	wps_start_timer(sec, usec, wfd_peer_selected_ageout_time_handler,
				pwfd_data);

	LEAVE();

	return;
}

/**
 *  @brief WFD peer ageout time time-out handler
 *
 *  @param user_data    User private data
 *  @return             None
 */
void wfd_peer_selected_ageout_time_handler(void *user_data)
{
	WFD_DATA *pwfd_data = user_data;
	time_t current_time;
	u32 time_diff;
	int i = 0;

	ENTER();
	WPS_DATA *wps_s = (WPS_DATA *) &wps_global;

	current_time = os_ticks_to_msec(os_ticks_get()) / 1000;

	for (i = 0; i < pwfd_data->num_find_results; i++) {
		time_diff =
			difftime(current_time,
					pwfd_data->find_results[i].timestamp);

		if (time_diff >= WFD_PEER_SELECTED_AGEOUT_TIME) {
			pwfd_data->find_results[i].is_valid = 0;
			wps_printf(DEBUG_WLAN, "Last event from ");
			wps_printf(DEBUG_WLAN, "%02x:%02x:%02x:%02x:%02x:%02x",
				(unsigned int)pwfd_data->find_results[i].
				device_id[0],
				(unsigned int)pwfd_data->find_results[i].
				device_id[1],
				(unsigned int)pwfd_data->find_results[i].
				device_id[2],
				(unsigned int)pwfd_data->find_results[i].
				device_id[3],
				(unsigned int)pwfd_data->find_results[i].
				device_id[4],
				(unsigned int)pwfd_data->find_results[i].
				device_id[5]);
			wps_printf(DEBUG_WLAN, " came %d seconds ago.\n",
					time_diff);

			wfd_cleanup_peer_entry(pwfd_data, i);
			/** Re print the table */
			wfd_reprint_find_table();

			memset(&wps_s->wfd_data, 0, sizeof(WFD_DATA));
			wps_s->wfd_data.dev_index = -1;
			wps_s->wfd_data.dev_found = -1;

			if (wps.cb(P2P_SESSION_TIMEOUT, NULL, 0) == -WM_FAIL)
				P2P_LOG("P2P Callback failed for event: %d",
						P2P_SESSION_TIMEOUT);

			os_mutex_put(&wps.p2p_session);
		}
	}

	LEAVE();

	return;
}

/**
 *  @brief Re-print the find table
 *
 *  @return             None
 */
void wfd_reprint_find_table(void)
{
#ifdef STDOUT_DEBUG

	WFD_DATA *pwfd_data = &wps_global.wfd_data;
	int i = 0, j = 0, updated_num_find_results = 0, len = 0, display_index;

	int k = 0;

	ENTER();

	for (i = 0; i < pwfd_data->num_find_results; i++) {
		if (pwfd_data->find_results[i].is_valid)
			updated_num_find_results++;
	}
	/* Clear screen system("clear"); */
	if (updated_num_find_results > 0) {
		wmprintf("\r\nFind Result : %d entries in List\r\n",
			 updated_num_find_results);

		wmprintf
		    ("----------------------------------------------------"
		     "-------------------------------------------\r\n");
		wmprintf
		    ("  # |    Intf Address   |          Device Name         "
			"  |  Op mode |   Dev Address     |  SSID |\r\n");
		wmprintf
		    ("----------------------------------------------------"
		     "-------------------------------------------\r\n");

		for (j = 0, display_index = 0; j < pwfd_data->num_find_results;
		     j++) {
			if (pwfd_data->find_results[j].is_valid) {

				/* Index Number */
				wmprintf(" %02d | ", display_index);

				/* Device ID */
				print_mac1(pwfd_data->find_results[j].
					   device_id);
				wmprintf(" |");

		/** Device Name printing in smart way  */
				len =
				    (MAX_DEVICE_NAME_LEN -
				     device_name_len[j]) / 2;
				for (i = 0; i < len; i++)
					wmprintf(" ");
				for (i = 0; i < device_name_len[j]; i++)
					wmprintf("%c", wps_device_name[j][i]);
				for (i = 0; i < len; i++)
					wmprintf(" ");

				wmprintf("|");

				/* Group Capability */
				if (!
				    (pwfd_data->
				     find_results[j].group_capability &
				     WFD_GROUP_OWNER_MASK))
					wmprintf("  Device ");
				else
					wmprintf("    GO   ");

				if (memcmp
				    (pwfd_data->find_results[j].device_address,
				     "\x00\x00\x00\x00\x00\x00", ETH_ALEN)) {
					wmprintf(" | ");
					print_mac1(pwfd_data->find_results[j].
						   device_address);
					wmprintf(" |");
				}
				if (pwfd_data->find_results[j].
				    group_capability & WFD_GROUP_OWNER_MASK)
					wmprintf("  %s",
						 pwfd_data->find_results[j].
						 go_ssid);

				wmprintf("\r\n");
				display_index++;
			}
		}

		if (gpwps_info->enrollee.device_password_id ==
		    DEVICE_PASSWORD_ID_PIN) {
			if (gpwps_info->wfd_pin_generated == WPS_SET) {
				wmprintf("\r\nPIN Number is : ");
				for (k = 0; k < gpwps_info->PINLen; k++)
					wmprintf("%c", gpwps_info->PIN[k]);
				wmprintf("\r\n");
			}
		}
	}
	LEAVE();
#endif
}

void wfd_reset()
{
	WPS_DATA *wps_s = (WPS_DATA *) &wps_global;

	mlanconfig_bgscan(1);

	/* Set WFD configuration */
	wifidirectcmd_config();

	if (wfd_set_mode(WFD_MODE_START) != WPS_STATUS_SUCCESS) {
		wps_printf(MSG_ERROR, "ERROR - Fail to "
				"initialize WFD!\n");
	}


	if (wfd_set_mode(WFD_MODE_START_FIND_PHASE) !=
			WPS_STATUS_SUCCESS) {
		wps_printf(MSG_ERROR, "ERROR - Fail to "
				"initialize WFD!\n");
	}

	wps_set_wfd_cfg();

	memset(&wps_s->wfd_data, 0, sizeof(WFD_DATA));
	wps_s->wfd_data.dev_index = -1;
	wps_s->wfd_data.dev_found = -1;

	p2p_scan_on = 1;

	gpwps_info->wps_session = 0;
	gpwps_info->p2p_session = 0;

	os_mutex_put(&wps.p2p_session);

}

int p2p_start(struct p2p_config *p2p_conf)
{
	static struct wps_config wps_conf;

	memset(&wps_conf, 0, sizeof(struct wps_config));

	wps_conf.role = p2p_conf->role;
	auto_go = p2p_conf->auto_go;

	if (auto_go)
		wfd_bss_type = BSS_TYPE_UAP;

	wps_conf.pin_generator = p2p_conf->pin_generator;
	wps_conf.version = p2p_conf->version;
	wps_conf.version2 = p2p_conf->version2;
	strncpy((char *)wps_conf.device_name,
		(const char *)p2p_conf->device_name, 32);
	strncpy((char *)wps_conf.manufacture,
		(const char *)p2p_conf->manufacture, 64);
	strncpy((char *)wps_conf.model_name,
		(const char *)p2p_conf->model_name, 32);
	strncpy((char *)wps_conf.model_number,
		(const char *)p2p_conf->model_number, 32);
	strncpy((char *)wps_conf.serial_number,
		(const char *)p2p_conf->serial_number, 32);
	wps_conf.config_methods = p2p_conf->config_methods;
	wps_conf.primary_dev_category = p2p_conf->primary_dev_category;
	wps_conf.primary_dev_subcategory = p2p_conf->primary_dev_subcategory;
	wps_conf.rf_bands = p2p_conf->rf_bands;
	wps_conf.os_version = p2p_conf->os_version;
	wps_conf.wps_msg_max_retry = p2p_conf->p2p_msg_max_retry;
	wps_conf.wps_msg_timeout = p2p_conf->p2p_msg_timeout;
	wps_conf.pin_len = p2p_conf->pin_len;
	wps_conf.wps_callback =
	    (int (*)(enum wps_event event, void *data, uint16_t len))p2p_conf->
	    p2p_callback;

	return wps_start(&wps_conf);
}

int p2p_stop()
{
	WPS_DATA *wps_s = (WPS_DATA *) &wps_global;

	mlanconfig_bgscan(0);

	if (wfd_set_mode(WFD_MODE_STOP_FIND_PHASE) != WPS_STATUS_SUCCESS) {
		wps_printf(MSG_ERROR, "ERROR - Fail to "
				"stop WFD!\n");
	}

	if (wfd_set_mode(WFD_MODE_STOP) != WPS_STATUS_SUCCESS) {
		wps_printf(MSG_ERROR, "ERROR - Fail to "
				"stop WFD!\n");
	}

	memset(&wps_s->wfd_data, 0, sizeof(WFD_DATA));
	wps_s->wfd_data.dev_index = -1;
	wps_s->wfd_data.dev_found = -1;

	p2p_scan_on = 0;

	gpwps_info->wps_session = 0;
	gpwps_info->p2p_session = 0;

	os_mutex_put(&wps.p2p_session);

	return wps_stop();
}

int p2p_connect(enum p2p_session_command pbc, uint32_t pin,
			struct p2p_scan_result *remote)
{
	struct wlan_scan_result res;

	memcpy(res.bssid, remote->device_id, ETH_ALEN);
	strncpy(res.ssid, (const char *)remote->go_ssid,
			MAX_SSID_LEN + 1);

	return wps_connect(pbc, pin, &res);
}

int p2p_disconnect()
{
	int ret;

	if (gpwps_info->p2p_session) {
		gpwps_info->p2p_session = 0;

		if (gpwps_info->discovery_role == WPS_REGISTRAR) {

			ret = wlan_remove_network(p2p_uap_network.name);

			if (ret != 0) {
				P2P_LOG("Failed to remove network");
				return -WM_FAIL;
			}

		} else {

			ret = wlan_disconnect();
			if (ret != 0) {
				P2P_LOG("Failed to disconnect");
				return -WM_FAIL;
			}

			ret = wlan_remove_network(p2p_network.name);

			if (ret != 0) {
				P2P_LOG("Failed to remove network ");
				return -WM_FAIL;
			}
		}

		wfd_reset();

		if (wps.cb(P2P_SESSION_FAILED, NULL, 0) == -WM_FAIL) {
			WPS_LOG("P2P Callback failed for event: %d",
					P2P_SESSION_FAILED);
			return -WM_FAIL;
		}
	}
	return WM_SUCCESS;
}

int p2p_generate_pin(uint32_t *p2p_pin)
{
	return wps_generate_pin(p2p_pin);
}

int p2p_validate_pin(uint32_t p2p_pin)
{
	return wps_validate_pin(p2p_pin);
}

struct p2p_scan_result *p2p_scan(int *no)
{

	WFD_DATA *pwfd_data = &wps_global.wfd_data;
	int i = 0, j = 0, updated_num_find_results = 0;

	ENTER();

	pwfd_data->valid_devices = 0;
	memset(pwfd_data->valid_find_results, 0,
			sizeof(pwfd_data->valid_find_results));

	/* Service only if find is in progress */
	if (pwfd_data->dev_index != -1 || pwfd_data->num_find_results == 0) {
		*no = 0;
		LEAVE();
		return NULL;
	}
	for (i = 0; i < pwfd_data->num_find_results; i++) {
		if (pwfd_data->find_results[i].is_valid)
			updated_num_find_results++;
	}

	*no = updated_num_find_results;

	/* Clear screen system("clear"); */
	if (updated_num_find_results > 0) {
		for (j = 0; j < pwfd_data->num_find_results; j++) {
			if (pwfd_data->find_results[j].is_valid) {
				pwfd_data->valid_find_results[pwfd_data->
					valid_devices].group_capability =
					pwfd_data->find_results[j].
					group_capability;
				memcpy(pwfd_data->
					valid_find_results[pwfd_data->
						valid_devices].device_id,
					pwfd_data->find_results[j].device_id,
					ETH_ALEN);

				strncpy(pwfd_data->
						valid_find_results[pwfd_data->
						valid_devices].device_name,
					(const char *)wps_device_name[j],
					device_name_len[j]);
				pwfd_data->
					valid_find_results[pwfd_data->
					valid_devices].
				device_name[device_name_len[j] + 1] = '\0';
				memcpy(pwfd_data->
					valid_find_results[pwfd_data->
						valid_devices].device_address,
					pwfd_data->find_results[j].
					device_address,
					ETH_ALEN);
				memcpy(pwfd_data->
					valid_find_results[pwfd_data->
						valid_devices].go_ssid,
					pwfd_data->find_results[j].go_ssid,
					MAX_SSID_LEN + 1);
				memcpy(pwfd_data->
					valid_find_results[pwfd_data->
						valid_devices].intended_address,
					pwfd_data->find_results[j].
					intended_address, ETH_ALEN);
				pwfd_data->
					valid_find_results[pwfd_data->
					valid_devices].op_channel =
					pwfd_data->find_results[j].op_channel;
				pwfd_data->
					valid_find_results[pwfd_data->
					valid_devices].listen_channel =
					pwfd_data->find_results[j].
					listen_channel;
				pwfd_data->valid_devices++;
			}
		}
	}

	*no = pwfd_data->valid_devices;

	return pwfd_data->valid_find_results;
}
