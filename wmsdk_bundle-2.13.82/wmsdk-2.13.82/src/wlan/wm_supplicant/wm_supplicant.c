/*
 *  Copyright 2008-2014, Marvell International Ltd.
 *  All Rights Reserved.
 */

/** wm_supplicant.c: The WMSDK supplicant state machine
 */
#include <stdio.h>
#include <string.h>

#include <wmtypes.h>
#include <wmstdio.h>
#include <wm_os.h>
#include <cli_utils.h>
#include <wm_supplicant.h>

#include <wltypes.h>
#include <w81connmgr.h>
#include <supplicant.h>
#include <supplicantApi.h>
#include <nohostSecurityParams.h>
#include <apMlme.h>
#include <keyMgmtAp.h>

#include "wm_supplicant_int.h"

#define MAX_EVENTS	8

/* message queue and thread for state machine */
static os_queue_pool_define(supplicant_queue_data,
			    sizeof(supp_msg_t *) * MAX_EVENTS);
static os_queue_t supplicant_queue;
static os_thread_stack_define(supplicant_stack, 2048);
static os_thread_t supplicant_thread;

int (*supp_cb) (enum supp_event event, void *data, uint16_t len);

char s_passphrase[64];
char s_ssid[IEEEtypes_SSID_SIZE + 1];
char s_bssid[ETH_ALEN];
uint8_t *s_buffer;
size_t s_data_len;

extern void supplicant_register_rx_callback(void (*EAPoLRxDataHandler)
				      (const t_u8 interface,
				const uint8_t *buf, const size_t len));

static cm_ConnectionInfo_t sta_conn_info;
static cm_ConnectionInfo_t *uap_conn_info;

static supp_msg_t *q_msg;
BufferDesc_t bufDesc;

SecurityMode_t sta_sec;
Cipher_t sta_cipher;

#ifdef CONFIG_SUPPLICANT_DEBUG
static const char *print_cmd_id(supplicant_cmd_id_t cmd_id)
{
	const char *ptr;

	switch (cmd_id) {
	case SUPP_STA_INIT:
		ptr = "SUPP_INIT";
		break;
	case SUPP_STA_ALLOC:
		ptr = "SUPP_ALLOC";
		break;
	case SUPP_STA_ENABLE:
		ptr = "SUPP_ENABLE";
		break;
	case SUPP_STA_SET_PASSPHRASE:
		ptr = "SUPP_SET_PASSPHRASE";
		break;
	case SUPP_STA_SESS_INIT:
		ptr = "SUPP_SESS_INIT";
		break;
	case SUPP_STA_DEAUTH_DATA:
		ptr = "SUPP_STA_DEAUTH_DATA";
		break;
	case SUPP_STA_DISABLE:
		ptr = "SUPP_DISABLE";
		break;
	case SUPP_UAP_INIT:
		ptr = "SUPP_UAP_INIT";
		break;
	case SUPP_UAP_ALLOC:
		ptr = "SUPP_UAP_ALLOC";
		break;
	case SUPP_UAP_FREE:
		ptr = "SUPP_UAP_FREE";
		break;
	case SUPP_UAP_ENABLE:
		ptr = "SUPP_UAP_ENABLE";
		break;
	case SUPP_UAP_SET_PASSPHRASE:
		ptr = "SUPP_UAP_SET_PASSPHRASE";
		break;
	case SUPP_UAP_SESS_INIT:
		ptr = "SUPP_UAP_SESS_INIT";
		break;
	case SUPP_UAP_DISABLE:
		ptr = "SUPP_UAP_DISABLE";
		break;
	case SUPP_UAP_ASSOC_DATA:
		ptr = "SUPP_UAP_ASSOC_DATA";
		break;
	case SUPP_UAP_DEAUTH_DATA:
		ptr = "SUPP_UAP_DEAUTH_DATA";
		break;
	case SUPP_DATA:
		ptr = "SUPP_DATA";
		break;
	default:
		ptr = "default";
		break;
	}
	return ptr;
}
#endif

void EAPoLRxDataHandler(const t_u8 interface,
			const uint8_t *buf, const size_t len)
{
	const uint8_t *etherhead = buf;
	const uint8_t *data = buf + sizeof(struct ethhdr);
	struct ethhdr *eh = (struct ethhdr *) etherhead;
	supp_msg_t *msg = NULL;

	if (!buf || !len)
		return;

	msg = (supp_msg_t *)
			os_mem_alloc(sizeof(supp_msg_t));

	if (msg == NULL) {
		supp_e("Failed to allocated memory for msg");
		return;
	}

	bzero(msg, sizeof(supp_msg_t));

	msg->cmd_id = SUPP_DATA;

	s_data_len = len - sizeof(struct ethhdr);

	s_buffer = (uint8_t *) os_mem_alloc(s_data_len);

	if (!s_buffer)
		return;

	memcpy(s_buffer, data, s_data_len);

	memcpy(msg->mac, eh->h_dest, ETH_ALEN);
	memcpy(msg->data.einfo.dst, eh->h_source, ETH_ALEN);
	msg->data.einfo.interface = interface;
	msg->data.einfo.len = s_data_len;
	msg->data.einfo.buffer = s_buffer;

	if (os_queue_send(&supplicant_queue, &msg, OS_NO_WAIT)
			!= WM_SUCCESS)
		supp_e("Data: Failed to send data");
}

int supplicant_send_cmd(supplicant_cmd_id_t cmd_id, char *ssid, int security,
		struct wlan_cipher *mcstCipher, struct wlan_cipher *ucstCipher,
		char *passphrase, char *bssid, uint8_t *sta_mac)
{
	supp_msg_t *msg = NULL;

	msg = (supp_msg_t *)
			os_mem_alloc(sizeof(supp_msg_t));

	if (msg == NULL) {
		supp_e("Failed to allocated memory for msg");
		return -WM_FAIL;
	}

	bzero(msg, sizeof(supp_msg_t));

	msg->cmd_id = cmd_id;

	if (ssid) {
		memcpy(s_ssid, ssid, IEEEtypes_SSID_SIZE + 1);
		msg->data.cinfo.ssid = s_ssid;
		msg->data.ainfo.ssid = s_ssid;
	}

	if (passphrase) {
		memcpy(s_passphrase, passphrase, 64);
		msg->data.ainfo.passphrase = s_passphrase;
	}

	if (bssid) {
		memcpy(s_bssid, bssid, ETH_ALEN);
		msg->data.cinfo.bssid = s_bssid;
	}

	if (security > 0) {
		msg->data.sinfo.security = security;
		memcpy(&msg->data.sinfo.mcstCipher, mcstCipher,
				sizeof(struct wlan_cipher));
		memcpy(&msg->data.sinfo.ucstCipher, ucstCipher,
				sizeof(struct wlan_cipher));
	}

	if (sta_mac)
		memcpy(msg->mac, sta_mac, ETH_ALEN);

	if (os_queue_send(&supplicant_queue, &msg, OS_NO_WAIT)
			!= WM_SUCCESS) {
		supp_e("Command %d: Failed to send data", cmd_id);
		return -WM_FAIL;
	}

	return WM_SUCCESS;
}

static void supp_state_machine(os_thread_arg_t unused)
{
	int ret;

	supp_d("supplicant state machine");

	supplicant_register_rx_callback(EAPoLRxDataHandler);

	while (1) {

		ret = os_queue_recv(&supplicant_queue, &q_msg, OS_WAIT_FOREVER);

		if (ret != WM_SUCCESS)
			continue;

		supp_d("Command Id recv :: %s", print_cmd_id(q_msg->cmd_id));

		switch (q_msg->cmd_id) {
		case SUPP_STA_INIT:
			bzero(&sta_conn_info, sizeof(cm_ConnectionInfo_t));
			sta_conn_info.conType = CONNECTION_TYPE_INFRA;
			supplicantFuncInit();
			break;
		case SUPP_STA_ALLOC:
			allocSupplicantData(&sta_conn_info);
			break;
		case SUPP_STA_FREE:
			freeSupplicantData(&sta_conn_info);
			break;
		case SUPP_STA_ENABLE:
			supplicantEnable(&sta_conn_info,
				q_msg->data.sinfo.security,
				&q_msg->data.sinfo.mcstCipher,
				&q_msg->data.sinfo.ucstCipher);
			break;
		case SUPP_STA_SET_PASSPHRASE:
			set_psk(q_msg->data.ainfo.passphrase);
			break;
		case SUPP_STA_SET_PMK:
			set_pmk(q_msg->data.ainfo.ssid,
				strlen(q_msg->data.ainfo.ssid),
				q_msg->data.ainfo.passphrase);
			break;
		case SUPP_STA_SESS_INIT:
			supplicantInitSession(&sta_conn_info,
					q_msg->data.cinfo.ssid,
					strlen(q_msg->data.cinfo.ssid),
					q_msg->data.cinfo.bssid, q_msg->mac);
			break;
		case SUPP_STA_SESS_FAILURE:
			supp_cb(SUPP_SESSION_FAILED, NULL, 0);
			break;
		case SUPP_STA_DEAUTH_DATA:
			supplicantRemoveKeyInfo(&sta_conn_info);
			break;
		case SUPP_STA_DISABLE:
			supplicantDisable(&sta_conn_info);
			break;
		case SUPP_UAP_INIT:
			uap_conn_info = cm_InitConnection(CONNECTION_TYPE_AP,
					0, 20,
					(IEEEtypes_MacAddr_t *) q_msg->mac,
					NULL, 0, NULL);

			if (!uap_conn_info)
				supp_e("Failed to uAP cm_InitConnection\r\n");
			break;
		case SUPP_UAP_SET_PASSPHRASE:
			ap_setpsk(uap_conn_info, q_msg->data.ainfo.ssid,
				q_msg->data.ainfo.passphrase);
			cm_SetComData(uap_conn_info, q_msg->data.cinfo.ssid);
			InitGroupKey(uap_conn_info);
			break;
		case SUPP_UAP_SESS_INIT:
			ReInitGTK(uap_conn_info);
			break;
		case SUPP_UAP_ASSOC_DATA:
			sta_sec.wpa2 = true;
			sta_cipher.ccmp = true;
			InitStaKeyInfo(uap_conn_info, &sta_sec,
					&sta_cipher, 0, 0);
			cm_SetPeerAddr(uap_conn_info, NULL,
					(IEEEtypes_MacAddr_t *) q_msg->mac);
			SendEAPOLMsgUsingBufDesc(uap_conn_info, NULL);
			break;
		case SUPP_UAP_DEAUTH_DATA:
			RemoveStaKeyInfo(uap_conn_info,
					(IEEEtypes_MacAddr_t *) q_msg->mac);
			break;
		case SUPP_UAP_DEINIT:
			if (uap_conn_info) {
				RemoveStaKeyInfo(uap_conn_info, NULL);
				cm_DeleteConnection(uap_conn_info);
				uap_conn_info = NULL;
			} else
				supp_e("Failed to uAP cm_DeleteConnection");
			break;
		case SUPP_DATA:
			bufDesc.intf.connPtr = &sta_conn_info;
			if (q_msg->data.einfo.interface)
				bufDesc.intf.connPtr = uap_conn_info;

			bufDesc.DataLen = q_msg->data.einfo.len;
			bufDesc.Buffer = q_msg->data.einfo.buffer;
			ProcessEAPoLPkt(&bufDesc,
				(IEEEtypes_MacAddr_t *) q_msg->mac,
				(IEEEtypes_MacAddr_t *) q_msg->data.einfo.dst);
			os_mem_free(q_msg->data.einfo.buffer);
			break;
		default:
			break;
		}

		if (q_msg)
			os_mem_free(q_msg);
	}
	os_thread_self_complete(&supplicant_thread);
}

int supplicant_start(int (*supp_callback) (enum supp_event event,
			void *data, uint16_t len))
{
	int ret;

	supp_d("supplicant start");

	ret = os_queue_create(&supplicant_queue, "supp_queue",
				sizeof(supp_msg_t *),
				&supplicant_queue_data);

	ret = os_thread_create(&supplicant_thread,
			       "supplicant",
			       supp_state_machine,
			       (void *) &supplicant_thread,
			       &supplicant_stack,
			       OS_PRIO_3);
	if (ret) {
		supp_e("failed to launch thread: %d", ret);
		goto fail;
	}

	supp_cb = supp_callback;

	supp_cb(SUPP_STARTED, NULL, 0);

	return WM_SUCCESS;

fail:
	if (supplicant_thread)
		os_thread_delete(&supplicant_thread);
	if (supplicant_queue)
		os_queue_delete(&supplicant_queue);

	supp_cb(SUPP_FAILED, NULL, 0);

	return -WM_FAIL;
}

void supplicant_finish(void)
{
	int ret;

	supp_d("supplicant finish");

	if (supplicant_thread) {
		ret = os_thread_delete(&supplicant_thread);
		if (ret != WM_SUCCESS)
			supp_e("failed to delete thread: %d", ret);
	}

	if (supplicant_queue) {
		ret = os_queue_delete(&supplicant_queue);
		if (ret != WM_SUCCESS)
			supp_w("failed to delete queue: %d", ret);
	}

	supp_cb(SUPP_FINISHED, NULL, 0);
}
