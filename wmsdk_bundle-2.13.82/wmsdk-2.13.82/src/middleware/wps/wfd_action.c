/** @file wfd_action.c
 *  @brief This file contains WFD action frame, WLAN commands
 *
 *  Copyright (C) 2003-2010, Marvell International Ltd.
 *  All Rights Reserved
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>
#include <wm_net.h>

#include "wps_mem.h"
#include "wps_util.h"
#include "wps_def.h"
#include "wps_wlan.h"
#include "wps_os.h"
#include "wlan_hostcmd.h"
#include "wps_wlan.h"
#include "wlan_wfd.h"

/********************************************************
	Local Variables
********************************************************/
extern u8 wps_oui_type[4];
extern u8 wifidir_oui_type[4];

/********************************************************
	Global Variables
********************************************************/
/** Global pwps information */
extern PWPS_INFO gpwps_info;
/** wps global */
extern WPS_DATA wps_global;

/**
 *  @brief Process and send ie config command
 *  @param ie_index  A pointer to the IE buffer index
 *  @param data_len_wfd  Length of P2P data, 0 to get, else set.
 *  @param data_len_wps  Length of WPS data, 0 to get, else set.
 *  @param buf      Pointer to buffer to set.
 *  @return          WPS_STATUS_SUCCESS--success, WPS_STATUS_FAIL--fail
 */
int wfd_ie_config(u16 *ie_index, u16 data_len_wfd, u16 data_len_wps, u8 *buf)
{
	int i, ret = WPS_STATUS_SUCCESS;
	tlvbuf_custom_ie *tlv = NULL;
	custom_ie *ie_ptr = NULL;
	u16 buf_len = 0;
	u8 action;

	ENTER();
	tlv = (tlvbuf_custom_ie *) buf;
	tlv->tag = MRVL_MGMT_IE_LIST_TLV_ID;
	/* Locate headers */
	ie_ptr = (custom_ie *) (tlv->ie_data);

	buf_len += sizeof(tlvbuf_custom_ie);

	if (data_len_wfd == 0 && data_len_wps == 0)
		action = ACTION_GET;
	else
		action = ACTION_SET;

	/* Set TLV fields : WFD IE parameters */
	if (data_len_wfd) {
		/* Set IE */
		ie_ptr->mgmt_subtype_mask = MASK_WFD_AUTO;
		tlv->length = sizeof(custom_ie) + data_len_wfd;
		ie_ptr->ie_length = data_len_wfd;
		ie_ptr->ie_index = *ie_index;
		buf_len += sizeof(custom_ie) + data_len_wfd;
	} else {
		/* Get WPS IE */
		tlv->length = 0;
	}

	(*ie_index)++;

	/* Locate headers */
	ie_ptr =
	    (custom_ie *) ((u8 *) (tlv->ie_data) + sizeof(custom_ie) +
			   data_len_wfd);

	/* Set WPS IE parameters */
	if (data_len_wps) {
		/* Set IE */
		ie_ptr->mgmt_subtype_mask = MASK_WFD_AUTO;
		tlv->length += sizeof(custom_ie) + data_len_wps;
		ie_ptr->ie_length = data_len_wps;
		ie_ptr->ie_index = *ie_index;
		buf_len += sizeof(custom_ie) + data_len_wps;
	}

	ret = wlan_sys_config((u8 *) buf, &buf_len, action);

	if (ret == WPS_STATUS_SUCCESS) {
		wps_hexdump(DEBUG_WLAN, "Password IE",
			    (unsigned const char *)buf, buf_len);
		wps_printf(DEBUG_WLAN, "custom IE ioctl success.\n");

		if (!data_len_wfd) {
			/* Get the IE buffer index number for
			MGMT_IE_LIST_TLV */
			tlv = (tlvbuf_custom_ie *) buf;
			*ie_index = 0xFFFF;
			if (tlv->tag == MRVL_MGMT_IE_LIST_TLV_ID) {
				ie_ptr = (custom_ie *) (tlv->ie_data);
				for (i = 0; i < MAX_MGMT_IE_INDEX; i++) {
					/* mask 0xFFFF indicates a wfd IE,
					return index */
					if (ie_ptr->mgmt_subtype_mask ==
					    MASK_WFD_AUTO
					    && ie_ptr->ie_length) {
						*ie_index = ie_ptr->ie_index;
						wps_printf(DEBUG_WLAN,
							   "Found Index %d\n",
							   i);
						break;
					}
					if (i < (MAX_MGMT_IE_INDEX - 1))
						ie_ptr =
						    (custom_ie *) ((u8 *) ie_ptr
								   +
								   sizeof
								   (custom_ie) +
								   ie_ptr->
								   ie_length);
				}
			}
			if (*ie_index == 0xFFFF) {
				wps_printf(MSG_WARNING,
					   "No free IE buffer available\n");
				ret = WPS_STATUS_FAIL;
			}
		}
	} else {
		wps_printf(DEBUG_WLAN, "custom IE ioctl failed.\n");
	}

	LEAVE();
	return ret;
}

/**
 *  @brief  Check if intended address field is configured.
 *          If yes, update the corresponding device ID.
 *  @param  wfd_buf      WFD buffer pointer
 *  @param  wfd_buf_len  WFD buffer length
 *
 *  @return  WPS_STATUS_SUCCESS--success, WPS_STATUS_FAIL--fail
 */
static int wfd_check_set_intended_addr(u8 *wfd_buf, u16 wfd_buf_len)
{
	tlvbuf_wfd_intended_addr *wfd_tlv;
	u8 *ptr = wfd_buf;
	s16 left_len = wfd_buf_len;
	u16 len = 0;

	ENTER();

	ptr += sizeof(wifidir_oui_type);

	while (left_len > WFD_IE_HEADER_LEN) {

		if (*ptr == TLV_TYPE_WFD_INTENDED_ADDRESS) {
			wfd_tlv = (tlvbuf_wfd_intended_addr *) ptr;

			/* Address is found, set it now */
			wps_printf(DEBUG_WFD_DISCOVERY,
				   "Intended address. " MACSTR,
				   MAC2STR(wfd_tlv->group_address));

			memcpy(gpwps_info->wfd_intended_addr,
			       wfd_tlv->group_address, ETH_ALEN);
			LEAVE();
			return WPS_STATUS_SUCCESS;
		}

		memcpy(&len, ptr + 1, sizeof(u16));
		len = wlan_le16_to_cpu(len);

		ptr += len + WFD_IE_HEADER_LEN;
		left_len -= len + WFD_IE_HEADER_LEN;
	}

	LEAVE();
	return WPS_STATUS_FAIL;
}

/**
 *  @brief  Check if channel list is configured.
 *          If yes, update the self_channel_list of global wfd_data structure.
 *  @param  wfd_buf      WFD buffer pointer
 *  @param  wfd_buf_len  WFD buffer length
 *
 *  @return  WPS_STATUS_SUCCESS--success, WPS_STATUS_FAIL--fail
 */
static int wfd_check_self_channel_list(u8 *wfd_buf, u16 wfd_buf_len)
{
	tlvbuf_wfd_channel_list *wfd_tlv;
	u8 *ptr = wfd_buf;
	s16 left_len = wfd_buf_len;
	u16 len = 0, len_wifidirect = 0;
	chan_entry *temp_ptr;
	WFD_DATA *pwfd_data = &(wps_global.wfd_data);
	int i = 0;

	ENTER();

	ptr += sizeof(wifidir_oui_type);

	while (left_len > WFD_IE_HEADER_LEN) {

		if (*ptr == TLV_TYPE_WFD_CHANNEL_LIST) {
			wfd_tlv = (tlvbuf_wfd_channel_list *) ptr;
			temp_ptr = (chan_entry *) wfd_tlv->wfd_chan_entry_list;

			len_wifidirect = wlan_le16_to_cpu(wfd_tlv->length) -
			    (sizeof(tlvbuf_wfd_channel_list) -
			     WFD_IE_HEADER_LEN);
			/* Multiple channel entries */
			if (len_wifidirect >
			    (sizeof(chan_entry) + temp_ptr->num_of_channels)) {
				while (len_wifidirect) {
					if ((int)(temp_ptr->regulatory_class) ==
					    WIFI_REG_CLASS_81) {
						pwfd_data->self_channel_list.
						    num_of_chan =
						    temp_ptr->num_of_channels;
						for (i = 0;
						     i <
						     temp_ptr->num_of_channels;
						     i++) {
							pwfd_data->
							    self_channel_list.
							    chan[i] =
							    *(temp_ptr->
							      chan_list + i);
						}
						break;
					}
					len_wifidirect -=
					    sizeof(chan_entry) +
					    temp_ptr->num_of_channels;
					temp_ptr +=
					    sizeof(chan_entry) +
					    temp_ptr->num_of_channels;
				}
			} else {
				/* Only one channel entry */
				pwfd_data->self_channel_list.num_of_chan =
				    temp_ptr->num_of_channels;
				for (i = 0; i < temp_ptr->num_of_channels;
					i++) {
					pwfd_data->self_channel_list.chan[i] =
					    *(temp_ptr->chan_list + i);
				}
			}

			LEAVE();
			return WPS_STATUS_SUCCESS;
		}

		memcpy(&len, ptr + 1, sizeof(u16));
		len = wlan_le16_to_cpu(len);
		ptr += len + WFD_IE_HEADER_LEN;
	}

	LEAVE();
	return WPS_STATUS_FAIL;
}

/**
 *  @brief  Check if P2P capability attr is present in the P2P IE.
 *          If yes, return the value of it.
 *  @param  wfd_buf      WFD buffer pointer
 *  @param  wfd_buf_len  WFD buffer length
 *  @param  dev_cap pointer to device capability
 *  @param  grp_cap pointer to group capability
 *
 *  @return  WPS_STATUS_SUCCESS--success, WPS_STATUS_FAIL--fail
 */
static int
wfd_get_p2p_cap_attribute(u8 *wfd_buf, u16 wfd_buf_len,
			  u8 *dev_cap, u8 *grp_cap)
{
	tlvbuf_wfd_capability *wfd_tlv;
	u8 *ptr = wfd_buf;
	s16 left_len = wfd_buf_len;
	u16 len = 0;

	ENTER();

	wps_hexdump(DEBUG_WLAN, "WFD IE:", (u8 *) wfd_buf, wfd_buf_len);

	ptr += sizeof(wifidir_oui_type);

	while (left_len > WFD_IE_HEADER_LEN) {

		if (*ptr == TLV_TYPE_WFD_CAPABILITY) {
			wfd_tlv = (tlvbuf_wfd_capability *) ptr;
			*dev_cap = wfd_tlv->dev_capability;
			*grp_cap = wfd_tlv->group_capability;

			LEAVE();
			return WPS_STATUS_SUCCESS;
		}

		memcpy(&len, ptr + 1, sizeof(u16));
		len = wlan_le16_to_cpu(len);

		ptr += len + WFD_IE_HEADER_LEN;
		left_len -= len + WFD_IE_HEADER_LEN;
	}

	LEAVE();
	return WPS_STATUS_FAIL;
}

/**
 *  @brief  Check if device password is received from FW.
 *          If yes, update the corresponding device ID.
 *  @param  wps_buf      WPS buffer pointer
 *  @param  wps_buf_len  WPS buffer length
 *
 *  @return  WPS_STATUS_SUCCESS--success, WPS_STATUS_FAIL--fail
 */
static int wfd_wps_check_device_password(u8 *wps_buf, u16 wps_buf_len)
{
	u8 *ptr = wps_buf;
	s16 left_len = wps_buf_len;
	u16 wps_len = 0, wps_type = 0, WPS_devicepassword = 0;

	ENTER();

	ptr += sizeof(wps_oui_type);

	/* Look for WPS Device Password */
	while (left_len > sizeof(tlvbuf_wps_ie)) {
		memcpy(&wps_type, ptr, sizeof(u16));
		wps_type = ntohs(wps_type);
		memcpy(&wps_len, ptr + 2, sizeof(u16));
		wps_len = ntohs(wps_len);

		if (wps_type == SC_Device_Password_ID) {
			tlvbuf_wps_ie *wps_tlv = (tlvbuf_wps_ie *) ptr;
			WPS_devicepassword =
			    htons(gpwps_info->enrollee.device_password_id);

			/* htons done, now do memcpy */
			memcpy(wps_tlv->data, &WPS_devicepassword, sizeof(u16));
			wps_printf(DEBUG_WFD_DISCOVERY,
				   "Updated Device Password directly in "
					"FW response.\n");
			LEAVE();
			return WPS_STATUS_SUCCESS;
		}
		ptr += wps_len + sizeof(tlvbuf_wps_ie);
		left_len -= wps_len + sizeof(tlvbuf_wps_ie);
	}

	LEAVE();
	return WPS_STATUS_FAIL;
}

/**
 *  @brief  Update the password IE in the device configuration.
 *          First Get the config and update with password.
 *  @param  None
 *
 *  @return  WPS_STATUS_SUCCESS--success, WPS_STATUS_FAIL--fail
 */
int wps_wlan_update_password_ie_config(void)
{
	int i, ret = WPS_STATUS_SUCCESS;
	short ie_index = 0;
	u8 buf[MRVDRV_SIZE_OF_CMD_BUFFER];
	u16 WPS_devicepassword, tlv_len;
	u16 ie_len_wfd, ie_len_wps;
	tlvbuf_wps_ie *wps_ptr;
	tlvbuf_custom_ie *tlv = NULL;
	custom_ie *ie_ptr = NULL;

	ENTER();
	memset(buf, 0, MRVDRV_SIZE_OF_CMD_BUFFER);
	ret = wfd_ie_config((u16 *) &ie_index, 0, 0, buf);

	if (ret == WPS_STATUS_SUCCESS && ie_index < (MAX_MGMT_IE_INDEX - 1)) {
		tlv = (tlvbuf_custom_ie *) buf;
		if (tlv->tag == MRVL_MGMT_IE_LIST_TLV_ID) {
			ie_ptr = (custom_ie *) (tlv->ie_data);
			/* Goto appropriate Ie Index */
			for (i = 0; i < ie_index; i++) {
				ie_ptr =
				    (custom_ie *) ((u8 *) ie_ptr +
						   sizeof(custom_ie) +
						   ie_ptr->ie_length);
			}
			ie_len_wfd = ie_ptr->ie_length;
			/*
			 * Find out if the interface address is configured,
			 * it needs to be set using SIOCSIFHWADDR
			 */
			wfd_check_set_intended_addr((u8 *) (ie_ptr->ie_buffer),
						    ie_len_wfd);

			/*
			 * Store the self channel list to compare with peer.
			 */
			wfd_check_self_channel_list((u8 *) (ie_ptr->ie_buffer),
						    ie_len_wfd);

			/*
			 * Store the self dev, grp capabilities.
			 */
			wfd_get_p2p_cap_attribute((u8 *) (ie_ptr->ie_buffer),
						  ie_len_wfd,
						  &gpwps_info->self_p2p_dev_cap,
						  &gpwps_info->
						  self_p2p_grp_cap);

			ie_ptr =
			    (custom_ie *) ((u8 *) ie_ptr + sizeof(custom_ie) +
					   ie_len_wfd);
			if (!
			    (ie_ptr->mgmt_subtype_mask == MASK_WFD_AUTO
			     && ie_ptr->ie_length)) {
				wps_printf(MSG_WARNING,
					   "Failed to get WPS IE to update "
						"device password.\n");
				return WPS_STATUS_FAIL;
			}

			ie_len_wps = ie_ptr->ie_length;
			wps_ptr =
			    (tlvbuf_wps_ie *) ((u8 *) (ie_ptr->ie_buffer) +
					       ie_len_wps);

			/* Search for Device passwork in existing WPS
			attributes, if not found append password IE here,
			update ie_len_wps */
			ret =
			    wfd_wps_check_device_password((u8 *) (ie_ptr->
								  ie_buffer),
							  ie_len_wps);

			if (ret != WPS_STATUS_SUCCESS) {
				WPS_devicepassword =
				    htons(gpwps_info->enrollee.
					  device_password_id);
				tlv_len =
				    sizeof(tlvbuf_wps_ie) +
				    sizeof(WPS_devicepassword);
				wps_ptr->tag = htons(SC_Device_Password_ID);
				wps_ptr->length =
				    htons(sizeof(WPS_devicepassword));

				/* htons done, now do memcpy */
				memcpy(wps_ptr->data, &WPS_devicepassword,
				       sizeof(u16));
				ie_len_wps += tlv_len;
			}

			/* Update New IE now */
			ret =
			    wfd_ie_config((u16 *) &ie_index, ie_len_wfd,
					  ie_len_wps, buf);
			if (ret != WPS_STATUS_SUCCESS) {
				wps_printf(MSG_WARNING,
				   "Failed to update WFD device password.\n");
			}
		}
	}
	LEAVE();
	return ret;
}

/**
 *  @brief Performs the ioctl operation to send the command to
 *  the driver to set BSS mode.
 *
 *  @param  mode Mode value to set
 *  @return WPS_STATUS_SUCCESS or WPS_STATUS_FAIL
 */
int wfd_wlan_update_bss_mode(u16 mode)
{
	int ret = WPS_STATUS_SUCCESS;
	u16 cmd_len = 0;
	wfdcmdbuf *header = NULL;
	u8 *buffer = NULL, *cmd;
	wfd_bss_mode_config *cmd_buf = NULL;

	ENTER();

	/* send hostcmd now */
	cmd_len = sizeof(wfd_bss_mode_config);
	buffer = (u8 *) wps_mem_malloc(cmd_len);
	if (!buffer) {
		wmprintf("ERR:Cannot allocate memory!\r\n");
		return WPS_STATUS_FAIL;
	}

	cmd_buf = (wfd_bss_mode_config *) buffer;
	cmd_buf->cmd_head.cmd_code = HostCmd_CMD_SET_BSS_MODE;
	cmd_buf->cmd_head.size = cmd_len - BUF_HEADER_SIZE;
	cmd_buf->cmd_head.seq_num = (0x01) << 13;
	cmd_buf->cmd_head.result = 0;
	cmd_buf->con_type = mode;

	cmd = (u8 *) buffer;

	*(u32 *) cmd = cmd_len - BUF_HEADER_SIZE;

	header = (wfdcmdbuf *) cmd;
	header->cmd_head.size = cmd_len - BUF_HEADER_SIZE;
	header->cmd_head.buf_size = header->cmd_head.size + BUF_HEADER_SIZE;
	endian_convert_request_header(header->cmd_head);

	if (wfd_ioctl((u8 *) cmd, cmd_len) == WPS_STATUS_SUCCESS)
		wps_printf(DEBUG_WFD_DISCOVERY,
			   "BSS Mode command successfully sent.\n");
	else {
		wps_printf(MSG_WARNING, "BSS Mode command send failed.\n");
		ret = WPS_STATUS_FAIL;
	}

	if (buffer)
		wps_mem_free(buffer);

	LEAVE();
	return ret;
}

/**
 *  @brief Performs the ioctl operation to send the command to
 *  the driver for various modes.
 *
 *  @param  mode Mode value to set
 *  @return WPS_STATUS_SUCCESS or WPS_STATUS_FAIL
 */
int wfd_set_mode(u16 mode)
{
	int ret = WPS_STATUS_SUCCESS;
	u16 cmd_len = 0;
	wfdcmdbuf *header = NULL;
	u8 *buffer = NULL, *cmd;
	wfd_mode_config *cmd_buf = NULL;
	u16 data = mode;

	ENTER();

	/* send hostcmd now */
	cmd_len = sizeof(wfd_mode_config);
	buffer = (u8 *) wps_mem_malloc(cmd_len);
	if (!buffer) {
		wmprintf("ERR:Cannot allocate memory!\r\n");
		return WPS_STATUS_FAIL;
	}

	cmd_buf = (wfd_mode_config *) buffer;
	cmd_buf->cmd_head.cmd_code = HostCmd_CMD_WFD_MODE_CONFIG;
	cmd_buf->cmd_head.size = cmd_len - BUF_HEADER_SIZE;
	cmd_buf->cmd_head.seq_num = (0x01) << 13;
	cmd_buf->cmd_head.result = 0;
	cmd_buf->action = ACTION_SET;
	cmd_buf->mode = wlan_cpu_to_le16(data);
	cmd_buf->action = wlan_cpu_to_le16(cmd_buf->action);

	cmd = (u8 *) buffer;

	*(u32 *) cmd = cmd_len - BUF_HEADER_SIZE;

	header = (wfdcmdbuf *) cmd;
	header->cmd_head.size = cmd_len - BUF_HEADER_SIZE;
	header->cmd_head.buf_size = header->cmd_head.size + BUF_HEADER_SIZE;
	endian_convert_request_header(header->cmd_head);

	ret = wfd_ioctl((u8 *) cmd, cmd_len);

	if (buffer)
		wps_mem_free(buffer);

	LEAVE();
	return ret;
}

/**
 *  @brief Creates a wfd_action_frame request and sends to the driver
 *
 *  Usage: "Usage : wfd_action_frame "
 *
 *  @param sel_index Index selected by user
 *  @param frame_sub_type  Frame Sub type for generic action frame
 *  @return          WPS_STATUS_SUCCESS--success, WPS_STATUS_FAIL--fail
 */
int wfdcmd_action_frame(int sel_index, int frame_sub_type)
{
	int ret = WPS_STATUS_SUCCESS;
	wfd_action_frame *action_buf = NULL;
	tlvbuf_mrvl_wfd_channel *ch_tlv;
	wfdcmdbuf *header = NULL;
	WFD_DATA *pwfd_data = &wps_global.wfd_data;
	u8 *buffer, *cmd;
	u16 cmd_len = 0, ie_len = 0;

	ENTER();
	cmd_len = sizeof(wfd_action_frame);
	buffer = (u8 *) wps_mem_malloc(cmd_len +
			2 * sizeof(tlvbuf_mrvl_wfd_channel));
	if (!buffer) {
		wmprintf("ERR:Cannot allocate memory!\r\n");
		return WPS_STATUS_FAIL;
	}

	memset(buffer, 0, cmd_len);
	action_buf = (wfd_action_frame *) buffer;
	/* update the command header parameters */
	action_buf->cmd_head.cmd_code = HostCmd_CMD_802_11_ACTION_FRAME;
	action_buf->cmd_head.seq_num = (0x01) << 13;
	action_buf->cmd_head.result = 0;

	/* update the command data parameters */
	action_buf->category = WIFI_CATEGORY_PUBLIC_ACTION_FRAME;

	if (memcmp(pwfd_data->find_results[sel_index].device_address,
		   "\x00\x00\x00\x00\x00\x00", ETH_ALEN))
		/*
		 * For action frame processing device_address should be used
		 */
		memcpy(action_buf->peer_mac_addr,
		       pwfd_data->find_results[sel_index].device_address,
		       ETH_ALEN);
	else
		memcpy(action_buf->peer_mac_addr,
		       pwfd_data->find_results[sel_index].device_id, ETH_ALEN);

	action_buf->action = 0;

	action_buf->dialog_taken = WIFI_DIALOG_TOKEN_IGNORE + 1;

	action_buf->oui[0] = 0x50;
	action_buf->oui[1] = 0x6F;
	action_buf->oui[2] = 0x9A;

	action_buf->oui_type = WFD_OUI_TYPE;

	action_buf->oui_sub_type = frame_sub_type;

	if (pwfd_data->find_results[sel_index].op_channel) {
		ch_tlv =
		    (tlvbuf_mrvl_wfd_channel *) &(action_buf->ie_list[ie_len]);
		ch_tlv->tag = MRVL_WFD_OPERATING_CHANNEL_TLV_ID;
		ch_tlv->length =
		    sizeof(tlvbuf_mrvl_wfd_channel) - 2 * sizeof(u16);
		ch_tlv->channel_num =
		    pwfd_data->find_results[sel_index].op_channel;
		ie_len += sizeof(tlvbuf_mrvl_wfd_channel);
		cmd_len += sizeof(tlvbuf_mrvl_wfd_channel);
	}

	if (pwfd_data->find_results[sel_index].listen_channel) {
		ch_tlv =
		    (tlvbuf_mrvl_wfd_channel *) &(action_buf->ie_list[ie_len]);
		ch_tlv->tag = MRVL_WFD_LISTEN_CHANNEL_TLV_ID;
		ch_tlv->length =
		    sizeof(tlvbuf_mrvl_wfd_channel) - 2 * sizeof(u16);
		ch_tlv->channel_num =
		    pwfd_data->find_results[sel_index].listen_channel;
		ie_len += sizeof(tlvbuf_mrvl_wfd_channel);
		cmd_len += sizeof(tlvbuf_mrvl_wfd_channel);
	}

	cmd = (u8 *) buffer;

	*(u32 *) cmd = cmd_len - BUF_HEADER_SIZE;

	header = (wfdcmdbuf *) cmd;
	header->cmd_head.size = cmd_len - BUF_HEADER_SIZE;
	header->cmd_head.buf_size = header->cmd_head.size + BUF_HEADER_SIZE;
	endian_convert_request_header(header->cmd_head);

	wps_hexdump(DEBUG_WFD_DISCOVERY, "Action Frame", (unsigned char *)cmd,
		    cmd_len);

	/* Send collective command */
	if (wfd_ioctl((u8 *) cmd, cmd_len) == WPS_STATUS_SUCCESS)
		wps_printf(DEBUG_WFD_DISCOVERY,
			   "WFD action frame successfully sent.\n");
	else {
		wps_printf(MSG_WARNING, "WFD action frame send failed.\n");
		ret = WPS_STATUS_FAIL;
	}

	if (buffer)
		wps_mem_free(buffer);

	LEAVE();
	return ret;
}

/* borrowed from mlanconfig.c and bg_scan_wifidir.conf.  This function is hard
 * coded for our WIFIDIR set up and may not be suitable for all applications.
 */
int mlanconfig_bgscan(int mode)
{
	unsigned char *buf;
	wfdcmdbuf *header = NULL;
	mrvl_cmd_head_buf *hostcmd;
	int ret = WPS_STATUS_SUCCESS;
	mlanconfig_bgscfg_cmd_hdr *hdr;
	ssid_header_tlv *ssid_tlv;
	probe_header_tlv *probe_tlv;
	channel_header_tlv *chan_tlv;
	snr_header_tlv *snr_tlv;
	start_later_header_tlv *start_later_tlv;
	u8 *cmd;

	buf = (unsigned char *)wps_mem_malloc(MRVDRV_SIZE_OF_CMD_BUFFER);
	if (buf == NULL) {
		wmprintf("allocate memory for hostcmd failed\r\n");
		return WPS_STATUS_FAIL;
	}

	/* prepare the bgscan command */
	memset(buf, 0, MRVDRV_SIZE_OF_CMD_BUFFER);
	hostcmd = (mrvl_cmd_head_buf *) buf;
	hostcmd->cmd_code = wlan_cpu_to_le16(HostCmd_CMD_BGSCAN_CFG);
	hostcmd->seq_num = (0x01) << 13;
	hostcmd->result = 0;
	hostcmd->size = sizeof(mrvl_cmd_head_buf);

	/* These are all magic numbers from the bg_scan_wifidir.conf
	file supplied by Marvell */
	hdr = (mlanconfig_bgscfg_cmd_hdr *) (buf + sizeof(mrvl_cmd_head_buf));
	hdr->Action = 1;
	hdr->Enable = mode;
	hdr->BssType = 3;
	hdr->ChannelsPerScan = 3;
	hdr->ScanInterval = wlan_cpu_to_le32(1000);
	hdr->StoreCondition = wlan_cpu_to_le32(1);
	hdr->ReportConditions = wlan_cpu_to_le32(1);
	hostcmd->size += sizeof(mlanconfig_bgscfg_cmd_hdr);

	ssid_tlv = (ssid_header_tlv *) (buf + hostcmd->size);
	ssid_tlv->type = MLANCONFIG_SSID_HEADER_TYPE;
	ssid_tlv->len = wlan_cpu_to_le16(SIZEOF_VALUE(ssid_header_tlv));
	ssid_tlv->MaxSSIDLen = wlan_cpu_to_le16(0);
	memcpy(ssid_tlv->ssid, "DIRECT-", sizeof(ssid_tlv->ssid));
	hostcmd->size += sizeof(tlv) + ssid_tlv->len;

	probe_tlv = (probe_header_tlv *) (buf + hostcmd->size);
	probe_tlv->type = MLANCONFIG_PROBE_HEADER_TYPE;
	probe_tlv->len = wlan_cpu_to_le16(SIZEOF_VALUE(probe_header_tlv));
	probe_tlv->NumProbes = wlan_cpu_to_le16(2);
	hostcmd->size += sizeof(tlv) + probe_tlv->len;

	chan_tlv = (channel_header_tlv *) (buf + hostcmd->size);
	chan_tlv->type = MLANCONFIG_CHANNEL_HEADER_TYPE;
	chan_tlv->len = wlan_cpu_to_le16(SIZEOF_VALUE(channel_header_tlv));
	chan_tlv->Chan1_RadioType = 0;
	chan_tlv->Chan1_ChanNumber = 1;
	chan_tlv->Chan1_ScanType = 2;
	chan_tlv->Chan1_MinScanTime = wlan_cpu_to_le16(50);
	chan_tlv->Chan1_ScanTime = wlan_cpu_to_le16(100);
	chan_tlv->Chan2_RadioType = 0;
	chan_tlv->Chan2_ChanNumber = 6;
	chan_tlv->Chan2_ScanType = 2;
	chan_tlv->Chan2_MinScanTime = wlan_cpu_to_le16(50);
	chan_tlv->Chan2_ScanTime = wlan_cpu_to_le16(100);
	chan_tlv->Chan3_RadioType = 0;
	chan_tlv->Chan3_ChanNumber = 11;
	chan_tlv->Chan3_ScanType = 2;
	chan_tlv->Chan3_MinScanTime = wlan_cpu_to_le16(50);
	chan_tlv->Chan3_ScanTime = wlan_cpu_to_le16(100);
	hostcmd->size += sizeof(tlv) + chan_tlv->len;

	snr_tlv = (snr_header_tlv *) (buf + hostcmd->size);
	snr_tlv->type = MLANCONFIG_SNR_HEADER_TYPE;
	snr_tlv->len = wlan_cpu_to_le16(SIZEOF_VALUE(snr_header_tlv));
	snr_tlv->SNRValue = 40;
	hostcmd->size += sizeof(tlv) + snr_tlv->len;

	start_later_tlv = (start_later_header_tlv *) (buf + hostcmd->size);
	start_later_tlv->type = MLANCONFIG_START_LATER_HEADER_TYPE;
	start_later_tlv->len =
	    wlan_cpu_to_le16(SIZEOF_VALUE(start_later_header_tlv));
	start_later_tlv->StartLaterValue = 0;
	hostcmd->size += sizeof(tlv) + start_later_tlv->len;

	hostcmd->size = wlan_cpu_to_le16(hostcmd->size);

	cmd = (u8 *) buf;

	*(u32 *) cmd = hostcmd->size - BUF_HEADER_SIZE;

	header = (wfdcmdbuf *) cmd;
	header->cmd_head.size = hostcmd->size;
	header->cmd_head.buf_size = header->cmd_head.size + BUF_HEADER_SIZE;
	endian_convert_request_header(header->cmd_head);

	wps_hexdump(DEBUG_WFD_DISCOVERY, "Scan config", (unsigned char *)buf,
		    hostcmd->size);

	if (wfd_ioctl((u8 *) buf, hostcmd->size) == WPS_STATUS_SUCCESS) {
		wps_printf(DEBUG_WFD_DISCOVERY, "Scan config command sent.\n");
	} else {
		wps_printf(DEBUG_WFD_DISCOVERY,
			   "Scan config command send failed.\n");
		ret = WPS_STATUS_FAIL;
	}

	if (buf)
		wps_mem_free(buf);

	return ret;
}
