/** @file wps_state.c
 *  @brief This file contains functions for WPS state machine.
 *
 *  Copyright (C) 2003-2010, Marvell International Ltd.
 *  All Rights Reserved
 */
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <lwip/sys.h>
#include <wmtypes.h>
#include <signal.h>
#include <lwip/inet.h>
#include <wm_net.h>
#include <wm_os.h>
#include <wlan.h>
#include <wps.h>

#include "wps_msg.h"
#include "wps_util.h"
#include "wps_eapol.h"
#include "wps_def.h"
#include "wps_wlan.h"
#include "wps_os.h"
#include "wps_mem.h"
#include <wifi.h>
#ifdef CONFIG_P2P
#include "wlan_hostcmd.h"
#include "wlan_wfd.h"
#endif
#ifdef CONFIG_CYASSL
#include <wm-tls.h>
#endif

extern struct wps_thread_t wps;
#ifdef CONFIG_P2P
extern struct wlan_network p2p_uap_network;
extern struct wlan_network p2p_network;
char ipaddr[16];
extern u8 auto_go;
#endif

#ifdef CONFIG_WPA2_ENTP
#define SSL_WAIT	5000
#define TLS_RECV_BUFFER_SIZE	5000
extern WPA_SM wpa_sm;
extern void wps_deregister_rx_callback();
extern int err_count, req_len, wpa2_failure;

os_thread_t wps_tls_thread;
static os_thread_stack_define(wps_tls_stack, 8192);

#ifdef CONFIG_FREE_RADIUS
extern bool flag;
extern int more_frags;
#endif
u8 *rbuf;
static int msg_length, fragment = 0;
unsigned int rlen =0, used_len = 0;
static void signal_receive();
int wait_for_receive();
extern uint8_t *key_data;

extern void wps_deregister_cyassl_tx_callback();
extern void wps_deregister_cyassl_rx_callback();
#endif

extern struct wlan_network wps_network;

int wps_registrar_state_check(PWPS_INFO pwps_info, u16 msg_type);
int wps_registrar_state_transition(PWPS_INFO pwps_info, u16 msg_type);

extern int wps_install_current_ipv4_cfg(char *ifname, u32 ip_addr,
					u32 subnet_mask);

#ifndef CONFIG_WPA2_ENTP
/********************************************************
        Local Variables
********************************************************/
static int (*wps_msg_process[17]) (PWPS_INFO, u8 *, u16) = {
wps_dummy,
	    wps_dummy,
	    wps_eap_response_identity_process,
	    wps_eap_request_start_process,
	    wps_eap_M1_frame_process,
	    wps_eap_M2_frame_process,
	    wps_eap_M2D_frame_process,
	    wps_eap_M3_frame_process,
	    wps_eap_M4_frame_process,
	    wps_eap_M5_frame_process,
	    wps_eap_M6_frame_process,
	    wps_eap_M7_frame_process,
	    wps_eap_M8_frame_process,
	    wps_ack_message_process,
	    wps_nack_message_process, wps_done_message_process, wps_dummy};
#endif
static int (*wps_msg_prepare[18]) (PWPS_INFO) = {
#ifndef CONFIG_WPA2_ENTP
wps_eapol_start_prepare,
	    wps_eap_request_identity_prepare,
#else
	    NULL,
	    NULL,
#endif
	    wps_eap_response_identity_prepare,
#ifndef CONFIG_WPA2_ENTP
	    wps_eap_request_start_prepare,
	    wps_eap_M1_frame_prepare,
	    wps_eap_M2_frame_prepare,
	    wps_eap_M2D_frame_prepare,
	    wps_eap_M3_frame_prepare,
	    wps_eap_M4_frame_prepare,
	    wps_eap_M5_frame_prepare,
	    wps_eap_M6_frame_prepare,
	    wps_eap_M7_frame_prepare,
	    wps_eap_M8_frame_prepare,
	    wps_ack_message_prepare,
	    wps_nack_message_remap,
	    wps_done_message_prepare, wps_eap_fail_frame_prepare, NULL
#endif
};

/********************************************************
        Global Variables
********************************************************/
/** WPS global */
extern WPS_DATA wps_global;
extern int wps_eapol_txStart(void);
/** IE buffer index */
extern short ie_index;
extern short assoc_ie_index;

/** IE buffer index */
extern short ap_assocresp_ie_index;
extern u8 *g_ssid;
extern struct wlan_network network;

/********************************************************
        Local Functions
********************************************************/
#ifdef STDOUT_DEBUG_MSG
/**
 *  @brief Print WPS message type information for debugging
 *
 *  @param str      Print string
 *  @param msg      Message type
 *  @return         None
 */
void wps_debug_print_msgtype(const char *str, int msg)
{
	ENTER();

	switch (msg) {
	case WPS_EAPOL_MSG_START:
		WPS_LOG("%s: EAPOL_START", str);
		break;
	case WPS_EAP_MSG_REQUEST_IDENTITY:
		WPS_LOG("%s: EAP_REQUEST_IDENTITY", str);
		break;
	case WPS_EAP_MSG_RESPONSE_IDENTITY:
		WPS_LOG("%s: EAP_RESPONSE_IDENTITY", str);
		break;
#ifndef CONFIG_WPA2_ENTP
	case WPS_EAP_START:
		WPS_LOG("%s: WPS_START", str);
		break;
	case WPS_EAP_M1:
		WPS_LOG("%s: WPS_M1", str);
		break;
	case WPS_EAP_M2:
		WPS_LOG("%s: WPS_M2", str);
		break;
	case WPS_EAP_M2D:
		WPS_LOG("%s: WPS_M2D", str);
		break;
	case WPS_EAP_M3:
		WPS_LOG("%s: WPS_M3", str);
		break;
	case WPS_EAP_M4:
		WPS_LOG("%s: WPS_M4", str);
		break;
	case WPS_EAP_M5:
		WPS_LOG("%s: WPS_M5", str);
		break;
	case WPS_EAP_M6:
		WPS_LOG("%s: WPS_M6", str);
		break;
	case WPS_EAP_M7:
		WPS_LOG("%s: WPS_M7", str);
		break;
	case WPS_EAP_M8:
		WPS_LOG("%s: WPS_M8", str);
		break;
	case WPS_MSG_DONE:
		WPS_LOG("%s: WPS_DONE", str);
		break;
	case WPS_MSG_ACK:
		WPS_LOG("%s: WPS_ACK", str);
		break;
	case WPS_MSG_NACK:
		WPS_LOG("%s: WPS_NACK ", str);
		break;
#endif /*ifndef CONFIG_WPA2_ENTP*/
	case WPS_EAP_MSG_FAIL:
		WPS_LOG("%s: EAP_FAIL ", str);
		break;
	case WPS_MSG_FRAG_ACK:
		WPS_LOG("%s: WPS_FRAG_ACK ", str);
		break;
	default:
		WPS_LOG("%s: ERROR - UNKNOWN TYPE ! ", str);
		break;
	}
	LEAVE();
}

/**
 *  @brief Print WPS state information for debugging
 *
 *  @param str      Print string
 *  @param state    State information
 *  @return         None
 */
static void wps_debug_print_state(const char *str, int state)
{
	ENTER();

	switch (state) {
	case WPS_STATE_A:
		wps_printf(DEBUG_WPS_STATE, "%s = A", str);
		break;
	case WPS_STATE_B:
		wps_printf(DEBUG_WPS_STATE, "%s = B", str);
		break;
#ifndef CONFIG_WPA2_ENTP
	case WPS_STATE_C:
		wps_printf(DEBUG_WPS_STATE, "%s = C", str);
		break;
	case WPS_STATE_D:
		wps_printf(DEBUG_WPS_STATE, "%s = D", str);
		break;
	case WPS_STATE_E:
		wps_printf(DEBUG_WPS_STATE, "%s = E", str);
		break;
	case WPS_STATE_F:
		wps_printf(DEBUG_WPS_STATE, "%s = F", str);
		break;
	case WPS_STATE_G:
		wps_printf(DEBUG_WPS_STATE, "%s = G", str);
		break;
	case WPS_STATE_H:
		wps_printf(DEBUG_WPS_STATE, "%s = H", str);
		break;
	case WPS_STATE_I:
		wps_printf(DEBUG_WPS_STATE, "%s = I", str);
		break;
#endif /*ifndef CONFIG_WPA2_ENTP*/
	default:
		wps_printf(DEBUG_WPS_STATE, "%s = Unknown WPS State", str);
		break;
	}

	LEAVE();
}
#else
void wps_debug_print_msgtype(const char *str, int msg)
{
}

static void wps_debug_print_state(const char *str, int state)
{
}
#endif /* STDOUT_DEBUG_MSG */

#ifndef CONFIG_WPA2_ENTP
/**
 *  @brief Extract the message type field from EAP packet
 *
 *  @param peap     A pointer to EAP frame header
 *  @return         Message type
 */
static int wps_get_message_type(PEAP_FRAME_HEADER peap)
{
	u8 *buffer;
	int msg_type = 0;
	PTLV_DATA_HEADER ptlv;
	int type, length;

	ENTER();

	switch (peap->op_code) {
	case WPS_Start:
		msg_type = WPS_EAP_START;
		break;
	case WPS_Ack:
	case WPS_Nack:
	case WPS_Msg:
	case WPS_Done:
		buffer = (u8 *) peap;
		buffer += SZ_EAP_WPS_FRAME_HEADER;
		ptlv = (PTLV_DATA_HEADER) buffer;
		length = ntohs(ptlv->length);
		buffer += (SZ_TLV_HEADER + length);
		ptlv = (PTLV_DATA_HEADER) buffer;
		type = ntohs(ptlv->type);
		if (type == SC_Message_Type) {
			buffer += SZ_TLV_HEADER;
			msg_type = (u8) *buffer;
		}
		break;
	default:
		wps_printf(MSG_WARNING, "Unknown opcode: 0x%x", peap->op_code);
		break;
	}

	wps_debug_print_msgtype("RX WPS Message Type", msg_type);

	LEAVE();
	return msg_type;
}
#else
static void signal_receive()
{
	WPS_DATA *wps_s = (WPS_DATA *) &wps_global;

	os_semaphore_put(&wps_s->ssl_sync_sem);
}

int wait_for_receive()
{
	WPS_DATA *wps_s = (WPS_DATA *) &wps_global;

	return os_semaphore_get(&wps_s->ssl_sync_sem, SSL_WAIT);
}
#endif

/**
 *  @brief Process Enrollee state machine checking
 *
 *  @param pwps_info    A pointer to WPS_INFO structure
 *  @param msg_type     Message type to be checked
 *  @return             Message type to be sent in current state
 */
static int wps_enrollee_state_check(PWPS_INFO pwps_info, u16 msg_type)
{
	int msg_next = WPS_EAP_UNKNOWN;

	ENTER();

	switch (msg_type) {
	case WPS_EAP_MSG_REQUEST_IDENTITY:
		pwps_info->eap_identity_count++;
		if (pwps_info->eap_identity_count < 1)
			msg_next = WPS_EAPOL_MSG_START;
		else if ((pwps_info->state == WPS_STATE_A) ||
			 (pwps_info->state == WPS_STATE_B))
			msg_next = WPS_EAP_MSG_RESPONSE_IDENTITY;
		break;

	case WPS_EAP_MSG_RESPONSE_IDENTITY:
		break;

#ifndef CONFIG_WPA2_ENTP
	case WPS_EAP_START:
		pwps_info->eap_identity_count = 0;
		if ((pwps_info->state == WPS_STATE_B) ||
		    (pwps_info->state == WPS_STATE_C) ||
		    (pwps_info->state == WPS_STATE_D))
			msg_next = WPS_EAP_M1;
		break;
	case WPS_EAP_M2:

		if ((pwps_info->state == WPS_STATE_D) ||
		    (pwps_info->state == WPS_STATE_E) ||
		    (pwps_info->state == WPS_STATE_F))
			msg_next = WPS_EAP_M3;
		break;
	case WPS_EAP_M2D:
		if ((pwps_info->state == WPS_STATE_D) ||
		    (pwps_info->state == WPS_STATE_B) ||
		    (pwps_info->state == WPS_STATE_E))
			msg_next = WPS_MSG_ACK;
		break;
	case WPS_EAP_M4:
		if ((pwps_info->state == WPS_STATE_E) ||
		    (pwps_info->state == WPS_STATE_F))
			msg_next = WPS_EAP_M5;
		break;
	case WPS_EAP_M6:
		if ((pwps_info->state == WPS_STATE_E) ||
		    (pwps_info->state == WPS_STATE_F))
			msg_next = WPS_EAP_M7;
		break;
	case WPS_EAP_M8:
		if ((pwps_info->state == WPS_STATE_E) ||
		    (pwps_info->state == WPS_STATE_F) ||
		    (pwps_info->state == WPS_STATE_H))
			msg_next = WPS_MSG_DONE;
		break;
#endif /*ifndef CONFIG_WPA2_ENTP*/
	case WPS_MSG_NACK:
		if (pwps_info->state == WPS_STATE_I)
			msg_next = WPS_MSG_NACK;
		break;
	case WPS_MSG_ACK:
		break;
	case WPS_EAP_MSG_FAIL:
		if ((pwps_info->state == WPS_STATE_A) ||
		    (pwps_info->state == WPS_STATE_B))
			msg_next = WPS_EAP_MSG_RESPONSE_IDENTITY;
		break;
	default:
		break;
	}			/* end of swtich */

	wps_debug_print_msgtype("WPS Next Message", msg_next);

	LEAVE();
	return msg_next;
}

/**
 *  @brief Process Enrollee state machine transition
 *
 *  @param pwps_info    A pointer to WPS_INFO structure
 *  @param msg_type     Message type for triggering state transition
 *  @return             WPS_STATUS_SUCCESS--success, otherwise--fail
 */
static int wps_enrollee_state_transition(PWPS_INFO pwps_info, u16 msg_type)
{
	WPS_DATA *wps_s = (WPS_DATA *) &wps_global;
	int status = WPS_STATUS_SUCCESS;

	ENTER();

	/* Current WPS State */
	wps_debug_print_state("State Transition Before", pwps_info->state);

	switch (msg_type) {
	case WPS_EAPOL_MSG_START:
		wps_printf(DEBUG_WPS_STATE, "Event Trigger : EAPOL_START");

		pwps_info->last_recv_wps_msg = -1;

		/* Reset Public keys and E-Hash, R-Hash */
		wps_reset_wps_state(pwps_info);
		break;
	case WPS_EAP_MSG_RESPONSE_IDENTITY:
		wps_printf(DEBUG_WPS_STATE,
			   "Event Trigger : EAP_RESPONSE_IDENTITY");

		pwps_info->register_completed = WPS_CANCEL;
		pwps_info->registration_in_progress = WPS_SET;
		wps_printf(DEBUG_WPS_STATE, "Set registration in progress %d",
			   pwps_info->registration_in_progress);

		if (pwps_info->state == WPS_STATE_A) {
			pwps_info->state = WPS_STATE_B;
			wps_printf(DEBUG_WPS_STATE, "State A->B");
		}
		break;
#ifndef CONFIG_WPA2_ENTP
	case WPS_EAP_START:
		wps_printf(DEBUG_WPS_STATE, "Event Trigger : WPS_START");

		pwps_info->register_completed = WPS_CANCEL;
		pwps_info->registration_in_progress = WPS_SET;
		wps_printf(DEBUG_WPS_STATE, "Set registration in progress %d",
			   pwps_info->registration_in_progress);

		if (pwps_info->state == WPS_STATE_A)
			pwps_info->state = WPS_STATE_B;

		if (pwps_info->state == WPS_STATE_B) {
			pwps_info->state = WPS_STATE_C;
			wps_printf(DEBUG_WPS_STATE, "State B->C");
		}
		break;
	case WPS_EAP_M1:
		wps_printf(DEBUG_WPS_STATE, "Event Trigger : WPS_M1");
		if (pwps_info->state == WPS_STATE_C) {
			pwps_info->state = WPS_STATE_D;
			wps_printf(DEBUG_WPS_STATE, "State C->D");
		}
		break;
	case WPS_EAP_M2:
		wps_printf(DEBUG_WPS_STATE, "Event Trigger : WPS_M2");
		if (pwps_info->state == WPS_STATE_D) {
			pwps_info->state = WPS_STATE_E;
			wps_printf(DEBUG_WPS_STATE, "State D->E");
		}
		break;
	case WPS_EAP_M2D:
		wps_printf(DEBUG_WPS_STATE, "Event Trigger : WPS_M2D");
		if ((pwps_info->state == WPS_STATE_D) ||
			   (pwps_info->state == WPS_STATE_B))
			pwps_info->state = WPS_STATE_E;
		break;
	case WPS_EAP_M3:
		wps_printf(DEBUG_WPS_STATE, "Event Trigger : WPS_M3");
		if (pwps_info->state == WPS_STATE_E) {
			pwps_info->state = WPS_STATE_F;
			wps_printf(DEBUG_WPS_STATE, "State E->F");
		}
		break;
	case WPS_EAP_M4:
		wps_printf(DEBUG_WPS_STATE, "Event Trigger : WPS_M4");
		if (pwps_info->state == WPS_STATE_F) {
			pwps_info->state = WPS_STATE_E;
			wps_printf(DEBUG_WPS_STATE, "State F->E");
		}
		break;
	case WPS_EAP_M5:
		wps_printf(DEBUG_WPS_STATE, "Event Trigger : WPS_M5");
		if (pwps_info->state == WPS_STATE_E) {
			pwps_info->state = WPS_STATE_F;
			wps_printf(DEBUG_WPS_STATE, "State E->F");
		}
		break;
	case WPS_EAP_M6:
		wps_printf(DEBUG_WPS_STATE, "Event Trigger : WPS_M6");
		if (pwps_info->state == WPS_STATE_F) {
			pwps_info->state = WPS_STATE_E;
			wps_printf(DEBUG_WPS_STATE, "State F->E");
		}
		break;
	case WPS_EAP_M7:
		wps_printf(DEBUG_WPS_STATE, "Event Trigger : WPS_M7");
		if (pwps_info->state == WPS_STATE_E) {
			pwps_info->state = WPS_STATE_F;
			wps_printf(DEBUG_WPS_STATE, "State E->F");
		}
		break;
	case WPS_EAP_M8:
		wps_printf(DEBUG_WPS_STATE, "Event Trigger : WPS_M8");
		pwps_info->register_completed = WPS_SET;
		pwps_info->registration_in_progress = WPS_CANCEL;
		wps_printf(DEBUG_WPS_STATE, "Set registration in progress %d",
			   pwps_info->registration_in_progress);

		if (pwps_info->state == WPS_STATE_F) {
			pwps_info->state = WPS_STATE_E;
			wps_printf(DEBUG_WPS_STATE, "State F->E");
		}
		break;
#endif /*ifndef CONFIG_WPA2_ENTP*/
	case WPS_MSG_DONE:
		wps_printf(DEBUG_WPS_STATE, "Event Trigger : WPS_DONE");
		if (pwps_info->state == WPS_STATE_E) {
			pwps_info->state = WPS_STATE_H;
			wps_printf(DEBUG_WPS_STATE, "State E->H");
		}
		break;
	case WPS_MSG_ACK:
		wps_printf(DEBUG_WPS_STATE, "Event Trigger : WPS_ACK");
		if (pwps_info->state == WPS_STATE_E) {
			pwps_info->state = WPS_STATE_D;
			wps_printf(DEBUG_WPS_STATE, "State E->D");
		}
		break;
	case WPS_MSG_NACK:
		wps_printf(DEBUG_WPS_STATE, "Event Trigger : WPS_NACK");
		switch (pwps_info->state) {
		case WPS_STATE_D:
		case WPS_STATE_H:
			break;
		case WPS_STATE_E:
			pwps_info->state = WPS_STATE_G;
			wps_printf(DEBUG_WPS_STATE, "State E->G");
			break;
		case WPS_STATE_I:
			if (wps_s->bss_type != BSS_TYPE_UAP) {
				pwps_info->state = WPS_STATE_G;
				wps_printf(DEBUG_WPS_STATE, "State I->G");
			}
			break;
		case WPS_STATE_F:
			if (wps_s->bss_type != BSS_TYPE_UAP) {
				pwps_info->state = WPS_STATE_I;
				wps_printf(DEBUG_WPS_STATE, "State F->I");
			}
			break;
		default:
			break;
		}
		break;
	case WPS_EAP_MSG_FAIL:
		wps_printf(DEBUG_WPS_STATE, "Event Trigger : EAP_FAIL");

		pwps_info->last_recv_wps_msg = -1;
		pwps_info->registration_in_progress = WPS_CANCEL;
		wps_printf(DEBUG_WPS_STATE, "Set registration in progress %d",
			   pwps_info->registration_in_progress);

		switch (pwps_info->state) {
#ifdef CONFIG_WPA2_ENTP
		case WPS_STATE_B:
			pwps_info->state = WPS_STATE_A;
			wps_printf(DEBUG_WPS_STATE, "State B->A");
			break;
#else
		case WPS_STATE_F:
			break;
		case WPS_STATE_D:
			pwps_info->state = WPS_STATE_A;
			wps_printf(DEBUG_WPS_STATE, "State D->A");
			break;
		case WPS_STATE_G:
			pwps_info->state = WPS_STATE_A;
			wps_printf(DEBUG_WPS_STATE, "State G->A");
			break;
		case WPS_STATE_H:
			pwps_info->state = WPS_STATE_A;
			wps_printf(DEBUG_WPS_STATE, "State H->A");
			break;
		case WPS_STATE_I:
			pwps_info->state = WPS_STATE_A;
			wps_printf(DEBUG_WPS_STATE, "State I->A");
			break;
#endif /*CONFIG_WPA2_ENTP*/
		default:
			break;
		}
		break;

	default:
		break;

	}			/* end of switch */

	/* New WPS State */
	wps_debug_print_state("State Transition After", pwps_info->state);

	LEAVE();
	return status;
}

#ifdef CONFIG_WPA2_ENTP
void wpa2_session_success()
{
	WPS_DATA *wps_s = (WPS_DATA *) &wps_global;

	PWPS_INFO pwps_info;

	pwps_info = wps_get_private_info();

	err_count = 0;
	req_len = 0;
	wpa2_failure = 0;

	pwps_info->register_completed = WPS_SET;
	pwps_info->registration_in_progress = WPS_CANCEL;
	
	/* Cancel WPS Registration Timer */
	wps_printf(DEBUG_WPS_STATE, "Cancelling registration timer!");
	wps_cancel_timer(wps_registration_time_handler, pwps_info);
	
	WPS_LOG("WPA2 Enterprise (EAP-TLS) Protocol Completed Successfully !");

	pwps_info->wps_session = 0;

	wps_s->tls_session_active = false;
	wps_s->tls_session_key_valid = false;
	if (wps_s->tls_handle)
		tls_close(&wps_s->tls_handle);

	if (pwps_info->set_timer == WPS_SET) {
		wps_cancel_timer(wps_txTimer_handler,
				pwps_info);
		pwps_info->set_timer = WPS_CANCEL;
		pwps_info->wps_msg_resent_count = 0;
	}

	pwps_info->registration_fail = WPS_SET;
	pwps_info->registration_in_progress = WPS_CANCEL;
	wps_printf(DEBUG_WPS_STATE,
			"Set registration in progress %d",
			pwps_info->registration_in_progress);

	wps_printf(DEBUG_WPS_STATE, "State *->A");
	pwps_info->state = WPS_STATE_A;

	wps_deregister_cyassl_tx_callback();
	wps_deregister_cyassl_rx_callback();
}

void wpa2_session_clean()
{
	int status;
	WPS_DATA *wps_s = (WPS_DATA *) &wps_global;

	if (wps_s->tls_session_init) {
		wps_s->tls_session_init = false;

		if (rbuf)
			wps_mem_free(rbuf);

		status = os_semaphore_delete(&wps_s->ssl_sync_sem);
		if (status != WM_SUCCESS)
			WPS_LOG("Warning: failed to delete mutex.\r\n");

		status = os_thread_delete(&wps_tls_thread);
		if (status != WM_SUCCESS)
			WPS_LOG("Warning: failed to delete thread.\r\n");
	}
}

void wpa2_session_complete()
{
	WPS_DATA *wps_s = (WPS_DATA *) &wps_global;
	WPA_SM *wpa_sm_s = (WPA_SM *) &wpa_sm;
	PWPS_INFO pwps_info;

	err_count = 0;
	req_len = 0;
	wpa2_failure = 0;

	pwps_info = wps_get_private_info();

	if (pwps_info && pwps_info->wps_session) {

		if (wps.cb(WPS_SESSION_FAILED, NULL, 0) == -WM_FAIL)
			WPS_LOG("WPS Callback failed for event:"
				"%d", WPS_SESSION_FAILED);

		pwps_info->register_completed = WPS_CANCEL;
		pwps_info->registration_in_progress = WPS_CANCEL;

		/* Cancel WPS Registration Timer */
		wps_printf(DEBUG_WPS_STATE, "Cancelling registration timer!");
		wps_cancel_timer(wps_registration_time_handler, pwps_info);

		WPS_LOG("WPA2 Enterprise (EAP-TLS) Registration "
			"protocol failed !");

		pwps_info->wps_session = 0;

		if (pwps_info->set_timer == WPS_SET) {
			wps_cancel_timer(wps_txTimer_handler,
					pwps_info);
			pwps_info->set_timer = WPS_CANCEL;
			pwps_info->wps_msg_resent_count = 0;
		}

		pwps_info->registration_fail = WPS_SET;
		pwps_info->registration_in_progress = WPS_CANCEL;
		wps_printf(DEBUG_WPS_STATE,
				"Set registration in progress %d",
				pwps_info->registration_in_progress);

		wps_printf(DEBUG_WPS_STATE, "State *->A");
		pwps_info->state = WPS_STATE_A;
		wps_s->tls_session_active = false;
		wps_s->tls_session_key_valid = false;
		if (wps_s->tls_handle)
			tls_close(&wps_s->tls_handle);
	}
	if (pwps_info) {
		/* Reset Public keys and E-Hash, R-Hash */
		wps_reset_wps_state(pwps_info);

		wps_deregister_rx_callback();
		wps_session_deinit();

		memset(wpa_sm_s, 0, sizeof(WPA_SM));

		wpa_sm_s->pairwise_cipher = WPA_CIPHER_CCMP;
		wpa_sm_s->group_cipher = WPA_CIPHER_CCMP;
		wpa_sm_s->proto = WPA_PROTO_RSN;
		wpa_sm_s->key_mgmt = WPA_KEY_MGMT_IEEE8021X;
		wpa_sm_s->renew_snonce = 1;
		memcpy(wpa_sm_s->own_addr, wps_s->l2->my_mac_addr, ETH_ALEN);
	}
}

void wps_tls_init()
{
	int status = WPS_STATUS_SUCCESS;
	int key_len;
	WPS_DATA *wps_s = (WPS_DATA *) &wps_global;

	status = os_semaphore_create(&wps_s->ssl_sync_sem,
				"ssl-synch-sem");

	if (status != WM_SUCCESS) {
		goto fail;
	}
	/*Take  semaphore immediatly so that we can later block on it */
	os_semaphore_get(&wps_s->ssl_sync_sem, OS_WAIT_FOREVER);

	/* Initialize the TLS configuration structure */
	wps_s->tls_cfg.flags = TLS_USE_CLIENT_CERT | TLS_CHECK_SERVER_CERT;
	wps_s->tls_cfg.flags |= TLS_WPA2_ENTP;
	wps_s->tls_cfg.tls.client.ca_cert =
			wps_s->wpa2_network.security.ca_cert;
	wps_s->tls_cfg.tls.client.ca_cert_size =
			wps_s->wpa2_network.security.ca_cert_size - 1;
	wps_s->tls_cfg.tls.client.client_cert =
			wps_s->wpa2_network.security.client_cert;
	wps_s->tls_cfg.tls.client.client_cert_size =
			wps_s->wpa2_network.security.client_cert_size - 1;
	wps_s->tls_cfg.tls.client.client_key =
			wps_s->wpa2_network.security.client_key;
	wps_s->tls_cfg.tls.client.client_key_size =
			wps_s->wpa2_network.security.client_key_size - 1;

	WPS_LOG("EAP-TLS init session");
	wps_s->tls_session_init = true;
	wps_s->tls_session_active = false;
	wps_s->tls_session_key_valid = false;

	status = tls_session_init(&wps_s->tls_handle, wps_s->sockfd, &wps_s->tls_cfg);

	if (status != WM_SUCCESS) {
		WPS_LOG("TLS session init failed");
		wpa2_session_complete();
		goto fail;
	}

	wps_eap_tls_response_send(NULL, 0);
	status = wait_for_receive();

	if (status != WM_SUCCESS) {
		WPS_LOG("TLS session failed");
		wpa2_session_complete();
		goto fail;
	}

	key_len = tls_session_key(wps_s->tls_handle, wps_s->tls_session_key,
					PMK_LEN);

	if (!key_len) {
		WPS_LOG("TLS: Failed to derive key");
		wpa2_session_complete();
		goto fail;
	}

	if (wps.cb(WPS_SESSION_SUCCESSFUL, wps_s->tls_session_key,
				PMK_LEN) == -WM_FAIL)
		WPS_LOG("WPS Callback failed for event: %d",
			WPS_SESSION_SUCCESSFUL);

	WPS_LOG("EAP-TLS Session Successful");
	wps_s->tls_session_active = true;
	wps_s->tls_session_key_valid = true;
fail:
	wpa2_session_clean();
}

int get_session_key(u8 *key, int len)
{
	WPS_DATA *wps_s = (WPS_DATA *) &wps_global;

	if (wps_s->tls_session_key_valid) {
		memcpy(key, wps_s->tls_session_key, len);
		return PMK_LEN;
	}

	return 0;
}
#endif

/**
 *  @brief Enrollee EAP packet process handling
 *
 *  @param pwps_info    A pointer to WPS_INFO structure
 *  @param peap         A pointer to EAP frame header
 *  @return             WPS_STATUS_SUCCESS--success, otherwise--fail
 */
static int
wps_eap_request_message_handler(PWPS_INFO pwps_info, PEAP_FRAME_HEADER peap)
{
	int status = WPS_STATUS_SUCCESS;
	int msg_next, msg_type;
	WPS_DATA *wps_s = (WPS_DATA *) &wps_global;
#ifdef CONFIG_WPA2_ENTP
	PEAP_TLS_FRAME_HEADER peap_tls;
#endif

	ENTER();

	pwps_info->peer_id = peap->identifier;

	if (peap->type == EAP_TYPE_IDENTITY) {
		wps_printf(DEBUG_WPS_STATE, "EAP_REQUEST_IDENTITY Received");
		wps_debug_print_msgtype("RX WPS Message Type",
					WPS_EAP_MSG_REQUEST_IDENTITY);

		if (pwps_info->set_timer == WPS_SET) {
			wps_cancel_timer(wps_txTimer_handler, pwps_info);
			pwps_info->set_timer = WPS_CANCEL;
			pwps_info->wps_msg_resent_count = 0;
		}

		msg_type = WPS_EAP_MSG_REQUEST_IDENTITY;
		pwps_info->last_recv_wps_msg = msg_type;

		if (pwps_info->role == WPS_ENROLLEE
		    || (IS_DISCOVERY_ENROLLEE(pwps_info))
		    ) {
			msg_next = wps_enrollee_state_check(pwps_info,
					    WPS_EAP_MSG_REQUEST_IDENTITY);
		} else {
			msg_next = wps_registrar_state_check(pwps_info,
					     WPS_EAP_MSG_REQUEST_IDENTITY);
		}
		if (msg_next != WPS_EAP_UNKNOWN)
			status =
			    wps_msg_transmit(pwps_info, msg_next,
					     pwps_info->wps_msg_timeout);
	} else if (peap->type == EAP_TYPE_WPS) {
#ifndef CONFIG_WPA2_ENTP
		switch (peap->op_code) {
		case WPS_Msg:
			{

				msg_type = wps_get_message_type(peap);

				if ((msg_type > pwps_info->last_recv_wps_msg) ||
				    (msg_type == WPS_M2D) ||
				    ((pwps_info->last_recv_wps_msg == WPS_M2D)
				     && (msg_type == WPS_M2))) {

					wps_printf(DEBUG_WPS_STATE,
						   "Last Message received 0x%x",
					   pwps_info->last_recv_wps_msg);
					wps_printf(DEBUG_WPS_STATE,
						   "Current message 0x%x",
						   msg_type);

					if ((msg_type ==
					     pwps_info->last_recv_wps_msg)
					    && (msg_type != WPS_M2D)) {
			/** recevied the retransmitted message
			*  use the previous transmitted message,
			*  instead of current tx message
			*/
						wps_printf(DEBUG_WPS_STATE,
				   "Received reTx EAPOL Req message: 0x%x",
							   msg_type);
						memcpy(&pwps_info->last_message,
					       &pwps_info->last_tx_message,
						       sizeof(MESSAGE_BUFFER));
					}

					pwps_info->last_recv_wps_msg = msg_type;

				} else {
					WPS_LOG
					    ("rx_msg: Dropping unexpected msg! "
				     "(prev_msg: 0x%x, curr_msg: 0x%x)",
					     pwps_info->last_recv_wps_msg,
					     msg_type);
					break;
				}

				if (pwps_info->set_timer == WPS_SET) {
					wps_cancel_timer(wps_txTimer_handler,
							 pwps_info);
					pwps_info->set_timer = WPS_CANCEL;
					pwps_info->wps_msg_resent_count = 0;
				}

				if (msg_type != WPS_EAP_UNKNOWN)
					status = (*wps_msg_process[msg_type])
					    (pwps_info, (u8 *) peap,
					     peap->length);
				else
					status = WPS_STATUS_FAIL;

				if (status == WPS_STATUS_SUCCESS) {
					if (pwps_info->role == WPS_ENROLLEE
					    ||
					    (IS_DISCOVERY_ENROLLEE(pwps_info))
					    ) {
						wps_enrollee_state_transition
						    (pwps_info, msg_type);
						msg_next =
						    wps_enrollee_state_check
						    (pwps_info, msg_type);

						if (wps_s->bss_type ==
						    BSS_TYPE_UAP
						    &&
						  pwps_info->wps_ap_setup_locked
						    == WPS_SET &&
						    msg_next == WPS_MSG_NACK &&
						    msg_type == WPS_EAP_M2) {
							WPS_LOG
							    ("WPS AP Setup is "
				"locked because of probable attack.");
							WPS_LOG
							    ("WPS Registration "
				"for AP Configuration will not continue.");
						}
					} else {
						wps_registrar_state_transition
						    (pwps_info, msg_type);
						msg_next =
						    wps_registrar_state_check
						    (pwps_info, msg_type);
					}
					if (msg_next != WPS_EAP_UNKNOWN)
						status =
						    wps_msg_transmit(pwps_info,
								     msg_next,
					     pwps_info->wps_msg_timeout);
					else
						status =
						    wps_msg_transmit(pwps_info,
							     WPS_MSG_NACK,
					     pwps_info->wps_msg_timeout);
				} else
					status =
					    wps_msg_transmit(pwps_info,
							     WPS_MSG_NACK,
					     pwps_info->wps_msg_timeout);

				break;
			}

		case WPS_Done:
			{
				if (pwps_info->role == WPS_REGISTRAR) {
					msg_type = wps_get_message_type(peap);
					status =
					    wps_done_message_process(pwps_info,
								     (u8 *)
								     peap,
							     peap->length);
					if (status == WPS_STATUS_SUCCESS) {
						wps_registrar_state_transition
						    (pwps_info, msg_type);
						msg_next =
						    wps_registrar_state_check
						    (pwps_info, msg_type);
						if (msg_next != WPS_EAP_UNKNOWN)
							status =
							    wps_msg_transmit
							    (pwps_info,
							     msg_next,
							     pwps_info->
							     wps_msg_timeout);
					} else
						status =
						    wps_msg_transmit(pwps_info,
							     WPS_MSG_NACK,
					     pwps_info->wps_msg_timeout);
				}
				break;
			}

		case WPS_Start:
			{
				msg_type = wps_get_message_type(peap);

				if ((msg_type > pwps_info->last_recv_wps_msg) ||
				    (msg_type == WPS_M2D) ||
				    ((pwps_info->last_recv_wps_msg == WPS_M2D)
				     && (msg_type == WPS_M2))) {

					wps_printf(DEBUG_WPS_STATE,
						   "Last Message received 0x%x",
					   pwps_info->last_recv_wps_msg);
					wps_printf(DEBUG_WPS_STATE,
						   "Current message 0x%x",
						   msg_type);

					if ((msg_type ==
					     pwps_info->last_recv_wps_msg)
					    && (msg_type != WPS_M2D)) {
			/** recevied the retransmitted message
			*  use the previous transmitted message,
			*  instead of current tx message
			*/
						wps_printf(DEBUG_WPS_STATE,
				   "Received reTx EAPOL Req message: 0x%x",
							   msg_type);
						memcpy(&pwps_info->last_message,
					       &pwps_info->last_tx_message,
						       sizeof(MESSAGE_BUFFER));
					}

					pwps_info->last_recv_wps_msg = msg_type;

				} else {
					WPS_LOG
					    ("rx_msg: Dropping unexpected msg! "
				     "(prev_msg: 0x%x, curr_msg: 0x%x)",
					     pwps_info->last_recv_wps_msg,
					     msg_type);
					break;
				}

				if (pwps_info->set_timer == WPS_SET) {
					wps_cancel_timer(wps_txTimer_handler,
							 pwps_info);
					pwps_info->set_timer = WPS_CANCEL;
					pwps_info->wps_msg_resent_count = 0;
				}

				status =
				    wps_eap_request_start_process(pwps_info,
								  (u8 *) peap,
								  peap->length);
				if (status == WPS_STATUS_SUCCESS) {

					wps_enrollee_state_transition(pwps_info,
							      WPS_EAP_START);

					msg_next =
					    wps_enrollee_state_check(pwps_info,
							     WPS_EAP_START);
					if (msg_next != WPS_EAP_UNKNOWN)
						status =
						    wps_msg_transmit(pwps_info,
								     msg_next,
					     pwps_info->wps_msg_timeout);
					else
						status =
						    wps_msg_transmit(pwps_info,
							     WPS_MSG_NACK,
					     pwps_info->wps_msg_timeout);
				} else {
					wps_printf(DEBUG_WPS_STATE,
						   "Process WPS_Start Fail !");
				}

				break;
			}

		case WPS_Ack:
			break;

		case WPS_Nack:
			msg_type = wps_get_message_type(peap);

			status =
			    wps_nack_message_process(pwps_info, (u8 *) peap,
						     peap->length);
			if (status == WPS_STATUS_SUCCESS) {

				if (pwps_info->set_timer == WPS_SET) {
					wps_cancel_timer(wps_txTimer_handler,
							 pwps_info);
					pwps_info->set_timer = WPS_CANCEL;
				}
				if (pwps_info->role == WPS_ENROLLEE
				    || (IS_DISCOVERY_ENROLLEE(pwps_info))
				    ) {
					wps_enrollee_state_transition(pwps_info,
								      msg_type);
					msg_next =
					    wps_enrollee_state_check(pwps_info,
								     msg_type);
				} else {
					wps_registrar_state_transition
					    (pwps_info, msg_type);
					msg_next =
					    wps_registrar_state_check(pwps_info,
								      msg_type);
				}
				if (msg_next != WPS_EAP_UNKNOWN)
					status =
					    wps_msg_transmit(pwps_info,
							     msg_next,
					     pwps_info->wps_msg_timeout);
			} else
				status =
				    wps_msg_transmit(pwps_info, WPS_MSG_NACK,
					     pwps_info->wps_msg_timeout);
			break;

		default:
			break;
		}
#endif
	}
#ifdef CONFIG_WPA2_ENTP
	else if (peap->type == EAP_TYPE_TLS) {
		peap_tls = (PEAP_TLS_FRAME_HEADER) ((void*) peap);

		if (peap_tls->flags & EAP_TLS_FLAGS_START) {

			if (!wps_s->tls_session_init) {
				WPS_LOG("EAP-TLS Start request received");

#ifdef CONFIG_FREE_RADIUS
				flag = false;
				more_frags = 0;
#endif
				err_count = 0;
				req_len = 0;
				wpa2_failure = 0;

				msg_type = WPS_EAP_TLS_START;
				pwps_info->last_recv_wps_msg = msg_type;

				status = os_thread_create(&wps_tls_thread, "wps_tls",
						wps_tls_init, 0,
						&wps_tls_stack, OS_PRIO_3);

				if (status != WM_SUCCESS) {
					WPS_LOG("Error: Failed to create wps_tls"
							" thread: %d", status);
					status = WPS_STATUS_FAIL;
				}

				rbuf = (u8 *)
					wps_mem_malloc(TLS_RECV_BUFFER_SIZE *
							sizeof(u8));
				if (!rbuf) {
					WPS_LOG("Error: Failed to allocate buffer"
							" for tls session");
					status = WPS_STATUS_FAIL;
				}
				rlen = 0;
				used_len = 0;
				fragment = 0;
			}
		} else {
			msg_type = WPS_EAP_TLS_MSG;
			pwps_info->last_recv_wps_msg = msg_type;


			if ((peap_tls->flags & EAP_TLS_FLAGS_MORE_FRAGMENTS) &&
				(!fragment)) {

				if (!(peap_tls->flags &
						EAP_TLS_FLAGS_MORE_FRAGMENTS))
					used_len = 0;

				msg_length = ntohs(peap_tls->length) -
						sizeof(EAP_TLS_FRAME_HEADER);

				if (msg_length) {
					if (peap_tls->flags &
						EAP_TLS_FLAGS_LENGTH_INCLUDED) {
						msg_length -= sizeof(u32);
						memcpy(rbuf, (u8 *) peap +
						sizeof(EAP_TLS_FRAME_HEADER) +
							sizeof(u32),
								msg_length);
					} else
						memcpy(rbuf, (u8 *) peap +
						sizeof(EAP_TLS_FRAME_HEADER),
								msg_length);

					fragment++;

					rlen = msg_length;

					wps_eap_tls_response_send(NULL, 0);
				}
			} else if ((peap_tls->flags &
					EAP_TLS_FLAGS_LENGTH_INCLUDED) &&
					fragment) {

				msg_length = ntohs(peap_tls->length) -
					sizeof(EAP_TLS_FRAME_HEADER) -
						sizeof(u32);

				if (msg_length) {
					memcpy(rbuf + used_len + rlen,
							(u8 *) peap +
					sizeof(EAP_TLS_FRAME_HEADER) +
							sizeof(u32),
								msg_length);
					rlen += msg_length;

					if (peap_tls->flags &
						EAP_TLS_FLAGS_MORE_FRAGMENTS)
						wps_eap_tls_response_send(NULL,
								0);

					if (!(peap_tls->flags &
						EAP_TLS_FLAGS_MORE_FRAGMENTS)) {
						fragment--;
						signal_receive();
					}
				}
			} else if (fragment) {

				fragment --;
				msg_length = ntohs(peap_tls->length) -
						sizeof(EAP_TLS_FRAME_HEADER);

				memcpy(rbuf + used_len + rlen, (u8 *) peap +
						sizeof(EAP_TLS_FRAME_HEADER),
							msg_length);

				rlen += msg_length;

				if (!fragment)
					signal_receive();
			} else {
				used_len = 0;

				msg_length = ntohs(peap_tls->length) -
						sizeof(EAP_TLS_FRAME_HEADER);

				if (msg_length) {
					if (peap_tls->flags &
						EAP_TLS_FLAGS_LENGTH_INCLUDED) {
						msg_length -= sizeof(u32);
						memcpy(rbuf + used_len + rlen,
							(u8 *) peap +
						sizeof(EAP_TLS_FRAME_HEADER) +
							sizeof(u32),
							msg_length);
					} else
						memcpy(rbuf + used_len + rlen,
							(u8 *) peap +
						sizeof(EAP_TLS_FRAME_HEADER),
								msg_length);
					rlen += msg_length;
					signal_receive();
				}
			}
		}
	}
#endif
	LEAVE();
	return status;
}

#ifdef CONFIG_P2P
/**
 *  @brief Process Registrar state machine checking
 *
 *  @param pwps_info    A pointer to WPS_INFO structure
 *  @param msg_type     Message type to be checked
 *  @return             Message type to be sent in current state
 */
int wps_registrar_state_check(PWPS_INFO pwps_info, u16 msg_type)
{
	int msg_next = WPS_EAP_UNKNOWN;
	WPS_DATA *wps_s = (WPS_DATA *) &wps_global;

	ENTER();

	switch (msg_type) {

	case WPS_EAP_MSG_REQUEST_IDENTITY:
		break;

	case WPS_EAPOL_MSG_START:
		if (pwps_info->state == WPS_STATE_A)
			msg_next = WPS_EAP_MSG_REQUEST_IDENTITY;
		break;
	case WPS_EAP_MSG_RESPONSE_IDENTITY:
		if ((pwps_info->state == WPS_STATE_A) ||
		    (pwps_info->state == WPS_STATE_B) ||
		    (pwps_info->state == WPS_STATE_C))
			msg_next = WPS_EAP_START;
		break;
	case WPS_EAP_M1:
		if ((pwps_info->state == WPS_STATE_C) ||
		    (pwps_info->state == WPS_STATE_D) ||
		    (pwps_info->state == WPS_STATE_E)) {
			if (pwps_info->role != WFD_ROLE) {
				if ((pwps_info->
				     registrar.updated_device_password_id !=
				     pwps_info->enrollee.device_password_id)
				    || (pwps_info->pin_pbc_set == WPS_CANCEL)) {
					wps_printf(DEBUG_WPS_STATE,
						   "Enrollee and Registrar "
					"Device password ID dont match."
						"Registar %d. Enrollee %d",
						   pwps_info->
					   registrar.updated_device_password_id,
						   pwps_info->
						   enrollee.device_password_id);
					wps_printf(DEBUG_WPS_STATE,
						   "Pin PBC set value %d",
						   pwps_info->pin_pbc_set);
					msg_next = WPS_EAP_M2D;
				} else
					msg_next = WPS_EAP_M2;
			} else {
				if (pwps_info->pin_pbc_set == WPS_CANCEL) {

					wps_printf(DEBUG_WPS_STATE,
						   "Pin PBC set value %d",
						   pwps_info->pin_pbc_set);
					msg_next = WPS_EAP_M2D;
				} else {
					wps_printf(DEBUG_WPS_STATE,
						   "Ignoring Device password "
							"ID check for WFD");
					wps_printf(DEBUG_WPS_STATE,
						   "Enrollee and Registrar "
					"Device password ID dont match."
						"Registar %d. Enrollee %d",
						   pwps_info->
					   registrar.updated_device_password_id,
						   pwps_info->
						   enrollee.device_password_id);
					msg_next = WPS_EAP_M2;
				}
			}
		}
		break;
	case WPS_EAP_M3:
		if ((pwps_info->state == WPS_STATE_E) ||
		    (pwps_info->state == WPS_STATE_F))
			msg_next = WPS_EAP_M4;
		break;
	case WPS_EAP_M5:
		if ((pwps_info->state == WPS_STATE_E) ||
		    (pwps_info->state == WPS_STATE_F))
			msg_next = WPS_EAP_M6;
		break;
	case WPS_EAP_M7:
		if (pwps_info->state == WPS_STATE_F)
			msg_next = WPS_EAP_M8;
		break;
	case WPS_MSG_DONE:
		if (wps_s->bss_type == BSS_TYPE_STA &&
		    (pwps_info->role == WPS_REGISTRAR) &&
		    pwps_info->state == WPS_STATE_E) {
			msg_next = WPS_MSG_ACK;
		} else if ((pwps_info->state == WPS_STATE_E) ||
			   (pwps_info->state == WPS_STATE_H) ||
			   (pwps_info->state == WPS_STATE_A))
			msg_next = WPS_EAP_MSG_FAIL;
		break;
	case WPS_MSG_ACK:
		if (pwps_info->state == WPS_STATE_D)
			msg_next = WPS_EAP_MSG_FAIL;
		break;
	case WPS_MSG_NACK:
		if ((wps_s->bss_type == BSS_TYPE_STA)
		    && (pwps_info->role == WPS_REGISTRAR) &&
		    pwps_info->state == WPS_STATE_H) {
			msg_next = WPS_MSG_NACK;
		} else if ((pwps_info->state == WPS_STATE_E) ||
			   (pwps_info->state == WPS_STATE_I) ||
			   (pwps_info->state == WPS_STATE_G))
			msg_next = WPS_EAP_MSG_FAIL;
		break;
	default:
		break;
	}			/* end of swtich */

	wps_debug_print_msgtype("WPS Next Message", msg_next);

	LEAVE();
	return msg_next;
}

/**
 *  @brief Process Registrar state machine transition
 *
 *  @param pwps_info    A pointer to WPS_INFO structure
 *  @param msg_type     Message type for triggering state transition
 *  @return             WPS_STATUS_SUCCESS--success, otherwise--fail
 */
int wps_registrar_state_transition(PWPS_INFO pwps_info, u16 msg_type)
{
	int status = WPS_STATUS_SUCCESS;

	ENTER();

	/* Current WPS State */
	wps_debug_print_state("State Transition Before", pwps_info->state);

	switch (msg_type) {
	case WPS_EAPOL_MSG_START:
		wps_printf(DEBUG_WPS_STATE, "Event Trigger : EAPOL_START");

		pwps_info->last_recv_wps_msg = -1;
		/* Reset Public keys and E-Hash, R-Hash */
		wps_reset_wps_state(pwps_info);
		break;
	case WPS_EAP_MSG_RESPONSE_IDENTITY:
		wps_printf(DEBUG_WPS_STATE,
			   "Event Trigger : EAP_RESPONSE_IDENTITY");

		pwps_info->register_completed = WPS_CANCEL;
		pwps_info->registration_in_progress = WPS_SET;
		wps_printf(DEBUG_WPS_STATE, "Set registration in progress %d",
			   pwps_info->registration_in_progress);

		if (pwps_info->state == WPS_STATE_A) {
			pwps_info->state = WPS_STATE_B;
			wps_printf(DEBUG_WPS_STATE, "State A->B");
		}
		break;
	case WPS_EAP_START:
		wps_printf(DEBUG_WPS_STATE, "Event Trigger : WPS_START");
		if (pwps_info->state == WPS_STATE_B) {
			pwps_info->state = WPS_STATE_C;
			wps_printf(DEBUG_WPS_STATE, "State B->C");
		}
		break;
	case WPS_EAP_M1:
		wps_printf(DEBUG_WPS_STATE, "Event Trigger : WPS_M1");

		if (pwps_info->state == WPS_STATE_C) {
			pwps_info->state = WPS_STATE_D;
			wps_printf(DEBUG_WPS_STATE, "State C->D");
		}
		break;
	case WPS_EAP_M2:
		wps_printf(DEBUG_WPS_STATE, "Event Trigger : WPS_M2");
		if (pwps_info->state == WPS_STATE_D) {
			pwps_info->state = WPS_STATE_E;
			wps_printf(DEBUG_WPS_STATE, "State D->E");
		}
		break;
	case WPS_EAP_M2D:
		wps_printf(DEBUG_WPS_STATE, "Event Trigger : WPS_M2D");
		if (pwps_info->state == WPS_STATE_D)
			pwps_info->state = WPS_STATE_E;
		break;
	case WPS_EAP_M3:
		wps_printf(DEBUG_WPS_STATE, "Event Trigger : WPS_M3");
		if (pwps_info->state == WPS_STATE_E) {
			pwps_info->state = WPS_STATE_F;
			wps_printf(DEBUG_WPS_STATE, "State E->F");
		}
		break;
	case WPS_EAP_M4:
		wps_printf(DEBUG_WPS_STATE, "Event Trigger : WPS_M4");
		if (pwps_info->state == WPS_STATE_F) {
			pwps_info->state = WPS_STATE_E;
			wps_printf(DEBUG_WPS_STATE, "State F->E");
		}
		break;
	case WPS_EAP_M5:
		wps_printf(DEBUG_WPS_STATE, "Event Trigger : WPS_M5");
		if (pwps_info->state == WPS_STATE_E) {
			pwps_info->state = WPS_STATE_F;
			wps_printf(DEBUG_WPS_STATE, "State E->F");
		}
		break;
	case WPS_EAP_M6:
		wps_printf(DEBUG_WPS_STATE, "Event Trigger : WPS_M6");
		if (pwps_info->state == WPS_STATE_F) {
			pwps_info->state = WPS_STATE_E;
			wps_printf(DEBUG_WPS_STATE, "State F->E");
		}
		break;
	case WPS_EAP_M7:
		wps_printf(DEBUG_WPS_STATE, "Event Trigger : WPS_M7");
		if (pwps_info->state == WPS_STATE_E) {
			pwps_info->state = WPS_STATE_F;
			wps_printf(DEBUG_WPS_STATE, "State E->F");
		}
		break;
	case WPS_EAP_M8:
		wps_printf(DEBUG_WPS_STATE, "Event Trigger : WPS_M8");
		if (pwps_info->state == WPS_STATE_F) {
			pwps_info->state = WPS_STATE_E;
			wps_printf(DEBUG_WPS_STATE, "State F->E");
		}
		break;
	case WPS_MSG_DONE:
		wps_printf(DEBUG_WPS_STATE, "Event Trigger : WPS_DONE");
		pwps_info->register_completed = WPS_SET;
		pwps_info->registration_in_progress = WPS_CANCEL;
		wps_printf(DEBUG_WPS_STATE, "Set registration in progress %d",
			   pwps_info->registration_in_progress);

		if (pwps_info->state == WPS_STATE_E) {
			pwps_info->state = WPS_STATE_H;
			wps_printf(DEBUG_WPS_STATE, "State E->H");
		}
		break;
	case WPS_MSG_ACK:
		wps_printf(DEBUG_WPS_STATE, "Event Trigger : WPS_ACK");
		if (pwps_info->state == WPS_STATE_E) {
			pwps_info->state = WPS_STATE_D;
			wps_printf(DEBUG_WPS_STATE, "State E->D");
		}
		break;
	case WPS_MSG_NACK:
		wps_printf(DEBUG_WPS_STATE, "Event Trigger : WPS_NACK");
		switch (pwps_info->state) {
		case WPS_STATE_F:
			pwps_info->state = WPS_STATE_I;
			wps_printf(DEBUG_WPS_STATE, "State F->I");
			break;
		case WPS_STATE_E:
			pwps_info->state = WPS_STATE_G;
			wps_printf(DEBUG_WPS_STATE, "State E->G");
			break;
		}
		break;
	case WPS_EAP_MSG_FAIL:
		wps_printf(DEBUG_WPS_STATE, "Event Trigger : EAP_FAIL");

		pwps_info->last_recv_wps_msg = -1;

		/* Reset Public keys and E-Hash, R-Hash */
		wps_reset_wps_state(pwps_info);

		switch (pwps_info->state) {
		case WPS_STATE_D:
			pwps_info->state = WPS_STATE_A;
			wps_printf(DEBUG_WPS_STATE, "State D->A");
			break;
		case WPS_STATE_G:
			pwps_info->state = WPS_STATE_A;
			wps_printf(DEBUG_WPS_STATE, "State G->A");
			break;
		case WPS_STATE_H:
			pwps_info->state = WPS_STATE_A;
			wps_printf(DEBUG_WPS_STATE, "State H->A");
			break;
		case WPS_STATE_I:
			pwps_info->state = WPS_STATE_A;
			wps_printf(DEBUG_WPS_STATE, "State I->A");
			break;
		case WPS_STATE_A:
			pwps_info->state = WPS_STATE_A;
			wps_printf(DEBUG_WPS_STATE, "State A->A");
			break;
		case WPS_STATE_F:
			pwps_info->state = WPS_STATE_A;
			wps_printf(DEBUG_WPS_STATE, "State F->A");
			break;
		default:
			break;
		}

		break;

	default:
		break;
	}			/* end of switch */

	/* New WPS State */
	wps_debug_print_state("State Transition After", pwps_info->state);

	LEAVE();
	return status;
}
#else
int wps_registrar_state_check(PWPS_INFO pwps_info, u16 msg_type)
{
	return WPS_STATUS_SUCCESS;
}

int wps_registrar_state_transition(PWPS_INFO pwps_info, u16 msg_type)
{
	return WPS_STATUS_SUCCESS;
}
#endif

/**
 *  @brief Maximum NACK error handler
 *
 *  @param pwps_info    A pointer to WPS_INFO structure
 *  @param wps_s        A pointer to WPS_DATA structure
 *  @return             WPS_STATUS_SUCCESS--success, otherwise--fail
 */
int wps_maximum_nack_handler(PWPS_INFO pwps_info, WPS_DATA *wps_s)
{
	int status = WPS_STATUS_SUCCESS;
	ENTER();
#if 0
	pwps_info->nack_error_count = 0;

	if (pwps_info->set_timer == WPS_SET) {
		wps_cancel_timer(wps_txTimer_handler, pwps_info);
		pwps_info->set_timer = WPS_CANCEL;
		pwps_info->wps_msg_resent_count = 0;
	}
#ifdef UAP_SUPPORT
	if (pwps_info->role == WPS_ENROLLEE) {

		/* clear IE buffer for registrar */
		/* shutdown the main processing loop */
		/* unregister user abort handler */
#if defined(WSC2) && defined(UAP_SUPPORT)
		if (wps_s->bss_type == BSS_TYPE_UAP &&
		    pwps_info->registrar.version >= WPS_VERSION_2DOT0) {
			/* Clear session start IEs for Assoc response */
			wps_wlan_ie_config(CLEAR_AR_WPS_IE,
					   &ap_assocresp_ie_index);
		}
#endif
		/*
		 * Send APCMD_SYS_CONFIGURE command to set WPS IE
		 */
		status = wps_wlan_ie_config(SET_WPS_IE, &ie_index);
		if (status != WPS_STATUS_SUCCESS) {
			wps_printf(MSG_WARNING, "Setting WPS IE failed!!!");
		}
#if defined(WSC2) && defined(UAP_SUPPORT)
		if (pwps_info->registrar.version >= WPS_VERSION_2DOT0) {
			status =
			    wps_wlan_ie_config
			    (SET_WPS_AP_SESSION_INACTIVE_AR_IE,
			     &ap_assocresp_ie_index);
			if (status != WPS_STATUS_SUCCESS) {
				wps_printf(MSG_WARNING,
					   "WPS IE configuration failure.\n");
			}
		}
#endif
		pwps_info->input_state = WPS_INPUT_STATE_ROLE;
		pwps_info->pin_pbc_set = WPS_CANCEL;

		wps_print_uap_role_menu(pwps_info);

		LEAVE();
		return status;
	} else if (pwps_info->role == WPS_REGISTRAR) {

		wps_printf(DEBUG_WPS_STATE, "Cancelling registration timer!");
		wps_cancel_timer(wps_registration_time_handler, pwps_info);
	/**
	* Read uAP configuration for next connection
	*/
		wps_load_uap_cred(wps_s, pwps_info);

		/* For UAP mode */
		wps_s->current_ssid.mode = IEEE80211_MODE_INFRA;
		pwps_info->mode = IEEE80211_MODE_INFRA;

		/*
		 * Send APCMD_SYS_CONFIGURE command to set WPS IE
		 */
		status = wps_wlan_ie_config(SET_WPS_IE, &ie_index);
		if (status != WPS_STATUS_SUCCESS) {
			wps_printf(MSG_WARNING, "Setting WPS IE failed!!!");
		}
		if (pwps_info->registrar.version >= WPS_VERSION_2DOT0) {
			status =
			    wps_wlan_ie_config
			    (SET_WPS_AP_SESSION_INACTIVE_AR_IE,
			     &ap_assocresp_ie_index);
			if (status != WPS_STATUS_SUCCESS) {
				wps_printf(MSG_WARNING,
					   "WPS IE configuration failure.\n");
			}
		}
		pwps_info->input_state = WPS_INPUT_STATE_ROLE;
		pwps_info->pin_pbc_set = WPS_CANCEL;

		wps_print_uap_role_menu(pwps_info);

	}
#endif
#endif
	LEAVE();
	return status;
}

#ifdef CONFIG_P2P
/**
 *  @brief Registrar EAP packet process handling
 *
 *  @param pwps_info    A pointer to WPS_INFO structure
 *  @param peap         A pointer to EAP frame header
 *  @return             WPS_STATUS_SUCCESS--success, otherwise--fail
 */
static int
wps_eap_response_message_handler(PWPS_INFO pwps_info, PEAP_FRAME_HEADER peap)
{
	int status = WPS_STATUS_SUCCESS;
	u16 msg_next, msg_type = 0;

	ENTER();

	pwps_info->peer_id = peap->identifier + 1;

	if (peap->type == EAP_TYPE_IDENTITY) {
		wps_debug_print_msgtype("RX WPS Message Type",
					WPS_EAP_MSG_RESPONSE_IDENTITY);
		if (pwps_info->set_timer == WPS_SET) {
			wps_cancel_timer(wps_txTimer_handler, pwps_info);
			pwps_info->set_timer = WPS_CANCEL;
			pwps_info->wps_msg_resent_count = 0;
		}

		status =
		    wps_eap_response_identity_process(pwps_info, (u8 *) peap,
						      peap->length);
		if (status == WPS_STATUS_SUCCESS) {
			msg_type = WPS_EAP_MSG_RESPONSE_IDENTITY;
			pwps_info->last_recv_wps_msg = msg_type;

			if (pwps_info->role == WPS_ENROLLEE
			    || (IS_DISCOVERY_ENROLLEE(pwps_info))
			    ) {
				wps_enrollee_state_transition(pwps_info,
							      msg_type);
				msg_next =
				    wps_enrollee_state_check(pwps_info,
							     msg_type);
			} else {
				wps_registrar_state_transition(pwps_info,
							       msg_type);
				msg_next =
				    wps_registrar_state_check(pwps_info,
							      msg_type);
			}

			if (msg_next != WPS_EAP_UNKNOWN)
				status =
				    wps_msg_transmit(pwps_info, msg_next,
					     pwps_info->wps_msg_timeout);
		}
	} else if (peap->type == EAP_TYPE_WPS) {
		switch (peap->op_code) {
		case WPS_Msg:

			msg_type = wps_get_message_type(peap);

			if ((msg_type > pwps_info->last_recv_wps_msg) ||
			    (msg_type == WPS_M2D) ||
			    ((pwps_info->last_recv_wps_msg == WPS_M2D) &&
			     (msg_type == WPS_M2))) {
				wps_printf(DEBUG_WPS_STATE,
					   "Last Message received 0x%x",
					   pwps_info->last_recv_wps_msg);
				wps_printf(DEBUG_WPS_STATE,
					   "Current message 0x%x", msg_type);
				if ((msg_type == pwps_info->last_recv_wps_msg)
				    && (msg_type != WPS_M2D)) {
			/** recevied the retransmitted message
			*  use the previous transmitted message for auth,
			*  instead of current tx message
			*/
					wps_printf(DEBUG_WPS_STATE,
				   "Received reTx EAPOL Rsp message: 0x%x",
						   msg_type);
					memcpy(&pwps_info->last_message,
					       &pwps_info->last_tx_message,
					       sizeof(MESSAGE_BUFFER));
				}

				pwps_info->last_recv_wps_msg = msg_type;

			} else {
				wps_printf(MSG_INFO,
					   "rx_msg: Dropping unexpected msg! "
					   "(prev_msg: 0x%x, curr_msg: 0x%x)\n",
					   pwps_info->last_recv_wps_msg,
					   msg_type);
				break;
			}

			if (pwps_info->set_timer == WPS_SET) {
				wps_cancel_timer(wps_txTimer_handler,
						 pwps_info);
				pwps_info->set_timer = WPS_CANCEL;
				pwps_info->wps_msg_resent_count = 0;
			}
			if (msg_type != WPS_EAP_UNKNOWN)
				status =
				    (*wps_msg_process[msg_type]) (pwps_info,
								  (u8 *) peap,
								  peap->length);
			else
				status = WPS_STATUS_FAIL;

			if (status == WPS_STATUS_SUCCESS) {

				if (pwps_info->role == WPS_ENROLLEE
				    || (IS_DISCOVERY_ENROLLEE(pwps_info))
				    ) {
					wps_enrollee_state_transition(pwps_info,
								      msg_type);
					msg_next =
					    wps_enrollee_state_check(pwps_info,
								     msg_type);
				} else {
					wps_registrar_state_transition
					    (pwps_info, msg_type);
					msg_next =
					    wps_registrar_state_check(pwps_info,
								      msg_type);
				}

				if (msg_next != WPS_EAP_UNKNOWN)
					status =
					    wps_msg_transmit(pwps_info,
							     msg_next,
					     pwps_info->wps_msg_timeout);
				else
					status =
					    wps_msg_transmit(pwps_info,
							     WPS_MSG_NACK,
					     pwps_info->wps_msg_timeout);
			} else
				status =
				    wps_msg_transmit(pwps_info, WPS_MSG_NACK,
					     pwps_info->wps_msg_timeout);

			break;

		case WPS_Done:
			msg_type = wps_get_message_type(peap);

			status =
			    wps_done_message_process(pwps_info, (u8 *) peap,
						     peap->length);

			if (status == WPS_STATUS_SUCCESS) {

				wps_registrar_state_transition(pwps_info,
							       msg_type);

				msg_next =
				    wps_registrar_state_check(pwps_info,
							      msg_type);

				if (msg_next != WPS_EAP_UNKNOWN)
					status =
					    wps_msg_transmit(pwps_info,
							     msg_next,
					     pwps_info->wps_msg_timeout);
			} else
				status =
				    wps_msg_transmit(pwps_info, WPS_MSG_NACK,
					     pwps_info->wps_msg_timeout);

			break;

		case WPS_Ack:
			msg_type = wps_get_message_type(peap);
			status =
			    wps_ack_message_process(pwps_info, (u8 *) peap,
						    peap->length);
			if (status == WPS_STATUS_SUCCESS) {

				if (pwps_info->role == WPS_ENROLLEE
				    || (IS_DISCOVERY_ENROLLEE(pwps_info))
				    ) {
					wps_enrollee_state_transition(pwps_info,
								      msg_type);
					msg_next =
					    wps_enrollee_state_check(pwps_info,
								     msg_type);
				} else {
					wps_registrar_state_transition
					    (pwps_info, msg_type);
					msg_next =
					    wps_registrar_state_check(pwps_info,
								      msg_type);
				}
				if (msg_next != WPS_EAP_UNKNOWN)
					status =
					    wps_msg_transmit(pwps_info,
							     msg_next,
					     pwps_info->wps_msg_timeout);
			} else
				status =
				    wps_msg_transmit(pwps_info, WPS_MSG_NACK,
					     pwps_info->wps_msg_timeout);

			break;

		case WPS_Nack:
			msg_type = wps_get_message_type(peap);
			status =
			    wps_nack_message_process(pwps_info, (u8 *) peap,
						     peap->length);
			if (status == WPS_STATUS_SUCCESS) {

				if (pwps_info->set_timer == WPS_SET) {
					wps_cancel_timer(wps_txTimer_handler,
							 pwps_info);
					pwps_info->set_timer = WPS_CANCEL;
				}
				if (pwps_info->role == WPS_ENROLLEE
				    || (IS_DISCOVERY_ENROLLEE(pwps_info))
				    ) {
					wps_enrollee_state_transition(pwps_info,
								      msg_type);
					msg_next =
					    wps_enrollee_state_check(pwps_info,
								     msg_type);
				} else {
					wps_registrar_state_transition
					    (pwps_info, msg_type);
					msg_next =
					    wps_registrar_state_check(pwps_info,
								      msg_type);
				}
				if (msg_next != WPS_EAP_UNKNOWN)
					status =
					    wps_msg_transmit(pwps_info,
							     msg_next,
					     pwps_info->wps_msg_timeout);
			} else
				status =
				    wps_msg_transmit(pwps_info, WPS_MSG_NACK,
					     pwps_info->wps_msg_timeout);

			break;

		default:
			break;
		}
	}

	LEAVE();
	return status;
}
#endif

/********************************************************
        Global Functions
********************************************************/

/**
 *  @brief EAP frame handling function
 *
 *  @param buffer       A pointer to frame received buffer
 *  @param src_addr     A pointer to source address of received frame
 *  @return             None
 */
void wps_message_handler(u8 *buffer, u8 *src_addr)
{
	PEAPOL_FRAME_HEADER peapol;
	PEAP_FRAME_HEADER peap;
	PWPS_INFO pwps_info;
	WPS_DATA *wps_s = (WPS_DATA *) &wps_global;
#ifndef CONFIG_WPA2_ENTP
	int status = WPS_STATUS_SUCCESS;
	int ret;
#endif

	ENTER();

	pwps_info = wps_get_private_info();

	peapol = (PEAPOL_FRAME_HEADER) buffer;
	switch (peapol->packet_type) {
	case EAP_PACKET:
		{
			peap =
			    (PEAP_FRAME_HEADER) (buffer +
						 sizeof(EAPOL_FRAME_HEADER));

			pwps_info->peer_id = peap->identifier;

			switch (peap->code) {
			case EAP_REQUEST:
				wps_printf(DEBUG_WPS_STATE,
					   "EAP_REQUEST received");

				if (wps_s->bss_type == BSS_TYPE_UAP) {
					if ((pwps_info->role == WPS_REGISTRAR &&
					     memcmp(pwps_info->
						    enrollee.mac_address,
						    src_addr, ETH_ALEN))
					    || (pwps_info->role == WPS_ENROLLEE
						&& memcmp(pwps_info->registrar.
							  mac_address, src_addr,
							  ETH_ALEN))) {
						wps_printf(MSG_WARNING,
						   "EAP_REQUEST received "
							"from invalid source "
							   MACSTR " dropped!",
							   MAC2STR(src_addr));
						break;
					}
				}

				wps_eap_request_message_handler(pwps_info,
								peap);
				break;
#ifdef CONFIG_P2P
			case EAP_RESPONSE:
				wps_printf(DEBUG_WPS_STATE,
					   "EAP_RESPONSE received");

				if (wps_s->bss_type == BSS_TYPE_UAP) {
					if ((pwps_info->role == WPS_REGISTRAR &&
					     memcmp(pwps_info->
						    enrollee.mac_address,
						    src_addr, ETH_ALEN))
					    || (pwps_info->role == WPS_ENROLLEE
						&& memcmp(pwps_info->registrar.
							  mac_address, src_addr,
							  ETH_ALEN))) {
						wps_printf(MSG_WARNING,
						   "EAP_RESPONSE received "
							"from invalid source "
							   MACSTR " dropped!",
							   MAC2STR(src_addr));
						break;
					}
				}

				wps_eap_response_message_handler(pwps_info,
								 peap);
				break;
#endif
#ifdef CONFIG_WPA2_ENTP
			case EAP_SUCCESS:
				{
					wps_printf(DEBUG_WPS_STATE,
						   "EAP_SUCCESS received");
					wps_printf(DEBUG_WPS_STATE,
						   "RX WPS Message Type: "
							"EAP_SUCCESS");

					if (pwps_info->set_timer == WPS_SET) {
						wps_cancel_timer
						    (wps_txTimer_handler,
						     pwps_info);
						pwps_info->set_timer =
						    WPS_CANCEL;
						pwps_info->wps_msg_resent_count
						    = 0;
					}
					if (pwps_info->role == WPS_ENROLLEE
					    ||
					    (IS_DISCOVERY_ENROLLEE(pwps_info))
					    ) {
						wps_enrollee_state_transition
						    (pwps_info,
						     WPS_EAP_MSG_FAIL);
					}
				}
				break;
#endif
#ifndef CONFIG_WPA2_ENTP
			case EAP_FAILURE:
				{
					wps_printf(DEBUG_WPS_STATE,
						   "EAP_FAILURE received");
					wps_printf(DEBUG_WPS_STATE,
						   "RX WPS Message Type: "
							"EAP_FAIL");

					if (pwps_info->set_timer == WPS_SET) {
						wps_cancel_timer
						    (wps_txTimer_handler,
						     pwps_info);
						pwps_info->set_timer =
						    WPS_CANCEL;
						pwps_info->wps_msg_resent_count
						    = 0;
					}
					if (pwps_info->role == WPS_ENROLLEE
					    ||
					    (IS_DISCOVERY_ENROLLEE(pwps_info))
					    ) {
						wps_enrollee_state_transition
						    (pwps_info,
						     WPS_EAP_MSG_FAIL);
					} else {
						wps_registrar_state_transition
						    (pwps_info,
						     WPS_EAP_MSG_FAIL);
					}

					if (pwps_info->invalid_credential ==
					    WPS_SET) {
					if ((pwps_info->role == WPS_ENROLLEE)
				    || IS_DISCOVERY_ENROLLEE(pwps_info)) {
					/* clear IE buffer for registrar */
					/* shutdown the main processing loop */
					/* unregister user abort handler */
							wps_shutdown();
						}

					}
					if (pwps_info->state == WPS_STATE_A &&
					    pwps_info->register_completed ==
					    WPS_SET) {
						pwps_info->wps_session = 0;

				/*
				 * Registration complete with M8 received.
				 */
#ifdef CONFIG_P2P
						WPS_LOG
						    ("Registration Protocol"
					" Completed Successfully !");
#else
					assoc_ie_index = 0x0002;
					ret =
					wps_wlan_ie_config(CLEAR_STA_WPS_IE,
						&assoc_ie_index);
					if (ret != WPS_STATUS_SUCCESS) {
						wps_printf(MSG_WARNING,
					"WPS IE configuration failure.\n");
					}
					WPS_LOG
					    ("WPS Registration Protocol"
					" Completed Successfully !");
#endif
						pwps_info->wps_session = 0;

						ret = wlan_disconnect();
						if (ret != 0)
							WPS_LOG
					    ("Failed to disconnect");

						ret =
					    wlan_remove_network(wps_network.
									name);

						if (ret != 0)
							WPS_LOG
					    ("Failed to remove network");

						/* Single Credential */
						status =
							wps_write_credentials
							(pwps_info);

						if (pwps_info->role ==
						    WPS_REGISTRAR) {
							memset
					    (&pwps_info->enrollee.mac_address,
							     0, ETH_ALEN);
						}
						/* message exchange completed */
						if (status !=
						    WPS_STATUS_SUCCESS) {
							WPS_LOG
						    ("Failed to generate WPS "
						"Registration configuration "
						"file !");

						}
					/* Cancel WPS Registration Timer */

						wps_printf(DEBUG_WPS_STATE,
					   "Cancelling registration timer!");
						wps_cancel_timer
					    (wps_registration_time_handler,
						     pwps_info);
#ifdef CONFIG_P2P
				/* Read network data to connect to */
				memset(&p2p_network, 0,
					sizeof(struct wlan_network));
				memcpy(&p2p_network, &network,
					sizeof(struct wlan_network));
						if (wps.cb
						    (P2P_SESSION_SUCCESSFUL,
						     &p2p_network,
						     sizeof(struct
							    wlan_network))
						    == -WM_FAIL)
							P2P_LOG
				    ("Callback failed for event: %d",
						     P2P_SESSION_SUCCESSFUL);
#else
						if (wps.cb
						    (WPS_SESSION_SUCCESSFUL,
						     &network,
						     sizeof(struct
							    wlan_network))
						    == -WM_FAIL)
							WPS_LOG
				    ("Callback failed for event: %d",
						     WPS_SESSION_SUCCESSFUL);
#endif
						wps_shutdown();

					} else {
						WPS_LOG("WPS Registration "
							"Protocol Failed !");

						pwps_info->wps_session = 0;
						pwps_info->p2p_session = 0;

						ret = wlan_disconnect();
						if (ret != 0)
							WPS_LOG
					    ("Failed to disconnect");

						ret =
					    wlan_remove_network(wps_network.
									name);
						if (ret != 0)
							WPS_LOG
					    ("Failed to remove network");

						wps_printf(DEBUG_WPS_STATE,
							   "Cancelling "
						   "registration timer!");
						wps_cancel_timer
					    (wps_registration_time_handler,
						     pwps_info);
#ifdef CONFIG_P2P
						wfd_reset();

						if (wps.cb
						    (P2P_SESSION_FAILED, NULL,
						     0) == -WM_FAIL)
							P2P_LOG
					    ("P2P Callback failed for event:"
							     "%d",
						     P2P_SESSION_FAILED);

#else
						if (wps.cb
						    (WPS_SESSION_FAILED, NULL,
						     0) == -WM_FAIL)
							WPS_LOG
					    ("WPS Callback failed for event:"
							     "%d\r\n",
						     WPS_SESSION_FAILED);
#endif
					}
					break;
				}	/* EAP_FAILURE */
#else
				case EAP_FAILURE:
					wpa2_failure = 1;
					break;
#endif

			default:
				wps_printf(MSG_ERROR, "Unknown Code received");
				break;
			}
			break;
		}		/* EAP_PACKET */
#ifndef CONFIG_WPA2_ENTP
	case EAPOL_START:
		{
			wps_printf(DEBUG_WPS_STATE, "EAPOL_START received");

			if (pwps_info->registration_in_progress == WPS_SET) {

				if (memcmp
				    (pwps_info->enrollee.mac_address, src_addr,
				     ETH_ALEN)) {

					wps_printf(MSG_WARNING,
						   "EAPOL_START received from "
						   MACSTR
					   " while session is in progress with "
						   MACSTR, MAC2STR(src_addr),
						   MAC2STR(pwps_info->
						   enrollee.mac_address));
					wps_printf(MSG_WARNING,
						   "EAPOL_START dropped!!");
					return;
				}
			}
			if ((pwps_info->role == WPS_REGISTRAR)
			    || (IS_DISCOVERY_REGISTRAR(pwps_info))) {
				/* Get Enrollee Peer Address */
				memcpy(pwps_info->enrollee.mac_address,
				       src_addr, ETH_ALEN);
				wps_printf(DEBUG_WLAN, "Peer Address Get: ");
				wps_hexdump(DEBUG_WLAN,
					    "pwps_info->enrollee.mac_address",
					    pwps_info->enrollee.mac_address,
					    ETH_ALEN);

				wps_eapol_start_handler(pwps_info);
				wps_eap_request_identity_prepare(pwps_info);

				wps_printf(DEBUG_WPS_STATE,
					   "eap_msg_sent = "
					"WPS_EAP_MSG_REQUEST_IDENTITY");
				pwps_info->eap_msg_sent =
				    WPS_EAP_MSG_REQUEST_IDENTITY;
			}

			break;
		}		/* EAPOL_START */
#else
#if 0
	case EAPOL_KEY_PACKET:
			wpa_rx_eapol(src_addr, buffer,
			sizeof(EAPOL_FRAME_HEADER) + ntohs(peapol->length));
		break;
#endif
#endif
	default:
		wps_printf(MSG_ERROR, "Unknown Packet Type %d",
			   peapol->packet_type);
		wps_hexdump(MSG_ERROR, "Packet Dump", buffer,
			    sizeof(EAPOL_FRAME_HEADER) + ntohs(peapol->length));
		break;
	}

	LEAVE();
}

/**
 *  @brief WPS EAP frame transmit function
 *
 *  @param pwps_info    A pointer to PWPS_INFO structure
 *  @param msg_type     message type
 *  @param timeout      timeout
 *  @return             WPS_STATUS_SUCCESS--success, otherwise--fail
 */
int wps_msg_transmit(PWPS_INFO pwps_info, u16 msg_type, u32 timeout)
{
	int status = WPS_STATUS_SUCCESS;
	u32 sec, usec;
#ifdef CONFIG_P2P
	WPS_DATA *wps_s = &wps_global;
#endif

	ENTER();
	wps_debug_print_msgtype("TX WPS Message Type", msg_type);

	status = (*wps_msg_prepare[msg_type]) (pwps_info);

	if (status == WPS_STATUS_SUCCESS) {
		pwps_info->eap_msg_sent = msg_type;

		if (pwps_info->role == WPS_ENROLLEE) {
			wps_enrollee_state_transition(pwps_info, msg_type);
		}
#ifdef CONFIG_P2P
		else if (pwps_info->role == WPS_REGISTRAR) {
			wps_registrar_state_transition(pwps_info, msg_type);
		} else if (pwps_info->role == WFD_ROLE) {
			if (pwps_info->discovery_role == WPS_REGISTRAR) {
				wps_registrar_state_transition(pwps_info,
							       msg_type);
			} else {
				wps_enrollee_state_transition(pwps_info,
							      msg_type);
			}
		}
#endif
	}

	if (msg_type != WPS_EAP_MSG_FAIL) {
		sec = timeout / 1000;
		usec = (timeout % 1000) * 1000;
		wps_start_timer(sec, usec, wps_txTimer_handler, pwps_info);
		pwps_info->set_timer = WPS_SET;
		pwps_info->wps_msg_resent_count++;
	} else {
#ifdef CONFIG_P2P
		wps_eap_fail_transmit_handler(pwps_info, wps_s);
#endif
	}

	LEAVE();
	return status;
}

#ifdef CONFIG_P2P
/**
 *  @brief WPS EAP fail frame transmit handler
 *
 *  @param pwps_info    A pointer to PWPS_INFO structure
 *  @param wps_s        A pointer to WPS_DATA structure
 *  @return             WPS_STATUS_SUCCESS--success, otherwise--fail
 */
int wps_eap_fail_transmit_handler(PWPS_INFO pwps_info, WPS_DATA *wps_s)
{

	int status = WPS_STATUS_SUCCESS;
	u8 *sta_mac_addr = NULL;
	ENTER();
	/* registrar send out EAP-FAIL */
	/* WPS process complete */

	pwps_info->registration_in_progress = WPS_CANCEL;
	wps_printf(DEBUG_WPS_STATE, "Set registration in progress %d",
		   pwps_info->registration_in_progress);

	if (pwps_info->set_timer == WPS_SET) {
		wps_cancel_timer(wps_txTimer_handler, pwps_info);
		pwps_info->set_timer = WPS_CANCEL;
		pwps_info->wps_msg_resent_count = 0;
	}
	if (wps_s->bss_type == BSS_TYPE_UAP) {
		if (pwps_info->role == WPS_REGISTRAR
		    || (IS_DISCOVERY_REGISTRAR(pwps_info))
		    )
			sta_mac_addr = pwps_info->enrollee.mac_address;
		else
			sta_mac_addr = pwps_info->registrar.mac_address;
		if (sta_mac_addr != NULL) {
			wps_printf(DEBUG_WPS_STATE,
			   "Delay sending DEAUTH by 200 milli-seconds!");
#if 0
			usleep(200000);
			wps_hexdump(DEBUG_WPS_STATE, "Deauth Mac Address",
				    sta_mac_addr, ETH_ALEN);
			status = apcmd_station_deauth(sta_mac_addr);
#endif
		}
	}
	status = wps_uap_session_complete_handler(pwps_info, wps_s);

	LEAVE();
	return status;
}

/**
 *  @brief WPS uAP session complete handler
 *
 *  @param pwps_info    A pointer to PWPS_INFO structure
 *  @param wps_s        A pointer to WPS_DATA structure
 *  @return             WPS_STATUS_SUCCESS--success, otherwise--fail
 */
int wps_uap_session_complete_handler(PWPS_INFO pwps_info, WPS_DATA *wps_s)
{
	int status = WPS_STATUS_SUCCESS;

	ENTER();

	if (wps_s->bss_type == BSS_TYPE_UAP
	    && pwps_info->register_completed == WPS_SET) {

		if ((pwps_info->role == WPS_REGISTRAR) ||
		    (pwps_info->role == WFD_ROLE)) {
			/* Cancel WPS Registration Timer */
			wps_printf(DEBUG_WPS_STATE,
				   "Cancelling registration timer!");
			wps_cancel_timer(wps_registration_time_handler,
					 pwps_info);
		}

		if (pwps_info->register_completed == WPS_SET) {
			pwps_info->wps_session = 0;
			P2P_LOG
		    ("Registration Process Completed Successfully !");

			if (wps.cb(P2P_AP_SESSION_SUCCESSFUL, NULL, 0)
			    == -WM_FAIL)
				P2P_LOG("Callback failed for event: %d",
					P2P_AP_SESSION_SUCCESSFUL);

		}
#if 0
		status = wps_wlan_reset_ap_config(wps_s, pwps_info);
#endif

	}
	return status;
	LEAVE();
}
#endif

/**
 *  @brief Check WLAN Link Status & Reconnect if disconnected
 *
 *  @param wps_s        A pointer to PWPS_DATA structure
 *  @param pwps_info    A pointer to PWPS_INFO structure
 *  @param reconnected  A pointer to variable to indicate if STA re-connected
 *  @return             1-connected, 0-not connected
 */

static int
wps_sta_check_link_active(WPS_DATA *wps_s,
			  PWPS_INFO pwps_info, int *reconnected)
{
#if !defined(CONFIG_P2P) && !defined(CONFIG_WPA2_ENTP)
	int ret = WPS_STATUS_SUCCESS;
	int retry_cnt = 5;
	int connect_retry;
#endif
	int link_active = 0;
	enum wlan_connection_state state;

	*reconnected = 0;

	if (wlan_get_connection_state(&state)) {
		WPS_LOG("Error: unable to get connection state");
		return 0;
	}

	if (state == WLAN_ASSOCIATED) {
		/* Link not lost */
		link_active = 1;
		return link_active;
	}

#if defined(CONFIG_P2P) || defined(CONFIG_WPA2_ENTP)
	WPS_LOG("Connection with AP lost..... ");
#else
	*reconnected = 1;

	do {
		WPS_LOG("Connection lost, try to re-connect to AP .....");

		ret = wlan_connect(wps_network.name);

		if (ret != 0) {
			WPS_LOG("Failed to connect %d\r\n", ret);
			goto retry;
		}

		connect_retry = 200;

		do {
			/* wait for interface up */
			os_thread_sleep(os_msec_to_ticks(50));

			if (wlan_get_connection_state(&state)) {
				WPS_LOG("Error: unable to get "
					"connection state\r\n");
				continue;
			}

			if (state == WLAN_ASSOCIATED) {
				link_active = 1;
				goto done;
			}

		} while (connect_retry--);

retry:
		retry_cnt--;

	} while (retry_cnt != 0);
done:
#endif
	return link_active;
}

/**
 *  @brief Re-Transmit the last TX'ed packet
 *
 *  @param pwps_info    A pointer to PWPS_INFO structure
 *  @return             WPS_STATUS_SUCCESS--retransmit success, otherwise--fail
 */

static int retransmit_last_tx_pkt(PWPS_INFO pwps_info)
{
	PEAP_FRAME_HEADER peap_wps_frame =
	    (PEAP_FRAME_HEADER) pwps_info->buffer;
	int pkt_len = ntohs(peap_wps_frame->length);
	int ret;

	wps_debug_print_msgtype("ReTX WPS Message Type",
				pwps_info->eap_msg_sent);

#ifndef CONFIG_WPA2_ENTP
	if (pwps_info->eap_msg_sent != WPS_EAPOL_MSG_START) {
#endif /*ifndef CONFIG_WPA2_ENTP*/
		ret = wps_eapol_txPacket((u8 *) pwps_info->buffer, pkt_len);

		wps_start_timer(WPS_MSG_DEFAULT_TIMEOUT / 1000, 0,
				wps_txTimer_handler, pwps_info);
		pwps_info->set_timer = WPS_SET;
		pwps_info->wps_msg_resent_count++;
#ifndef CONFIG_WPA2_ENTP
	} else {
		ret = wps_msg_transmit(pwps_info, WPS_EAPOL_MSG_START,
				       WPS_MSG_DEFAULT_TIMEOUT);
	}
#endif /*ifndef CONFIG_WPA2_ENTP*/

	return ret;
}

/**
 *  @brief WPS frame transmit time-out handler
 *
 *  @param user_data    User private data
 *  @return             None
 */
void wps_txTimer_handler(void *user_data)
{
	PWPS_INFO pwps_info;
	WPS_DATA *wps_s = (WPS_DATA *) &wps_global;
	int status = WPS_STATUS_SUCCESS;
	int reconnected = 0, link_active = 0;

	ENTER();

	pwps_info = (PWPS_INFO) user_data;

	if (pwps_info->set_timer == WPS_SET) {
		wps_cancel_timer(wps_txTimer_handler, pwps_info);
		pwps_info->set_timer = WPS_CANCEL;
	}
#ifdef CONFIG_P2P
	if (wps_s->bss_type == BSS_TYPE_UAP
	    && (pwps_info->wps_msg_resent_count >=
		pwps_info->wps_msg_max_retry)) {

		switch (pwps_info->role) {
		case WPS_ENROLLEE:
			status = WPS_STATUS_FAIL;
			break;
		case WPS_REGISTRAR:
			if (pwps_info->eap_msg_sent == WPS_MSG_NACK)
				status = WPS_STATUS_FAIL;
			else
				status = WPS_STATUS_SUCCESS;
			break;
		}
		goto done;
	}
#endif
	/*
	 * Some AP/External Registrar will disconnect after EAP-FAIL
	 * packet sent without M2D being sent before that.
	 * So, polling link status of AP to restart registration here.
	 */
	if (wps_s->bss_type == BSS_TYPE_STA) {

		link_active = wps_sta_check_link_active(wps_s, pwps_info,
							&reconnected);

		if (!link_active) {
#ifdef CONFIG_WPA2_ENTP
			wpa2_failure = 1;
#else
			WPS_LOG("WPS Registration protocol failed !");
#endif
			/* shutdown the main processing loop */
			wps_main_loop_shutdown();
			pwps_info->wps_session = 0;

#ifndef CONFIG_WPA2_ENTP
			int ret = wlan_disconnect();
			if (ret != 0)
				WPS_LOG("Failed to disconnect");

			ret = wlan_remove_network(wps_network.name);
			if (ret != 0)
				WPS_LOG("Failed to remove network");
#endif

#ifdef CONFIG_P2P
			if (wps.cb(P2P_SESSION_FAILED, NULL, 0) == -WM_FAIL)
				WPS_LOG("P2P Callback failed for event:"
					"%d", P2P_SESSION_FAILED);
#else
#ifndef CONFIG_WPA2_ENTP
			if (wps.cb(WPS_SESSION_FAILED, NULL, 0) == -WM_FAIL)
				WPS_LOG("WPS Callback failed for event:"
					"%d", WPS_SESSION_FAILED);
#endif
#endif
			status = WPS_STATUS_FAIL;
			goto done;
		}

#ifndef CONFIG_WPA2_ENTP
		if (reconnected) {
			pwps_info->restart_link_lost = WPS_SET;
			WPS_LOG("Connected to following BSS (or IBSS) :");
			WPS_LOG
			("SSID = [%s], BSSID = [%02x:%02x:%02x:%02x:%02x:%02x]",
				 wps_network.ssid,
				 wps_network.bssid[0],
				 wps_network.bssid[1],
				 wps_network.bssid[2],
				 wps_network.bssid[3],
				 wps_network.bssid[4], wps_network.bssid[5]);
			WPS_LOG("WPS Registration Protocol Restarted .....");
		}

		if (pwps_info->restart_link_lost || pwps_info->restart_by_M2D) {
			pwps_info->restart_link_lost = WPS_CANCEL;
			pwps_info->restart_by_M2D = WPS_CANCEL;

			pwps_info->state = WPS_STATE_A;
			pwps_info->id = 1;

			/* Reset Public keys and E-Hash, R-Hash */
			wps_reset_wps_state(pwps_info);

			pwps_info->last_recv_wps_msg = -1;
			pwps_info->wps_msg_resent_count = 0;
			pwps_info->eap_msg_sent = WPS_EAPOL_MSG_START;
			status =
			    wps_msg_transmit(pwps_info, WPS_EAPOL_MSG_START,
					     WPS_MSG_DEFAULT_TIMEOUT);

			goto done;
		}
#endif /*ifndef CONFIG_WPA2_ENTP*/
	}
	retransmit_last_tx_pkt(pwps_info);

done:
	if (status != WPS_STATUS_SUCCESS) {
#ifdef CONFIG_P2P
		wfd_reset();
#endif
		if (wps_s->bss_type == BSS_TYPE_UAP) {
			pwps_info->wps_session = 0;
		} else {
			/* Cancel WPS Registration Timer */
			wps_printf(DEBUG_WPS_STATE,
				   "Cancelling registration timer!");
			wps_cancel_timer(wps_registration_time_handler,
					 pwps_info);

			if (pwps_info->set_timer == WPS_SET) {
				wps_cancel_timer(wps_txTimer_handler,
						 pwps_info);
				pwps_info->set_timer = WPS_CANCEL;
				pwps_info->wps_msg_resent_count = 0;
			}

			pwps_info->registration_fail = WPS_SET;
			pwps_info->registration_in_progress = WPS_CANCEL;
			wps_printf(DEBUG_WPS_STATE,
				   "Set registration in progress %d",
				   pwps_info->registration_in_progress);

			wps_printf(DEBUG_WPS_STATE, "State *->A");
			pwps_info->state = WPS_STATE_A;
			/* Reset Public keys and E-Hash, R-Hash */
			wps_reset_wps_state(pwps_info);
		}
	}

	LEAVE();
	return;
}

/**
 *  @brief Start WPS registration timer
 *
 *  @param pwps_info    A pointer to WPS_INFO structure
 *  @return             WPS_STATUS_SUCCESS--success, otherwise--fail
 */
void wps_start_registration_timer(PWPS_INFO pwps_info)
{
	u32 sec, usec;

	/* Start a timer for WPS registration protocol timeout */
	wps_printf(DEBUG_WALK_TIME, "Start Timer for WPS Registration ... ");
	sec = WPS_REGISTRATION_TIME;
	usec = 0;
	wps_start_timer(sec, usec, wps_registration_time_handler, pwps_info);

	return;

}

/**
 *  @brief Start WPS state machine
 *
 *  @param pwps_info    A pointer to WPS_INFO structure
 *  @return             WPS_STATUS_SUCCESS--success, otherwise--fail
 */
int wps_state_machine_start(PWPS_INFO pwps_info)
{
	int status = WPS_STATUS_SUCCESS;

#ifndef CONFIG_WPA2_ENTP
	WPS_DATA *wps_s = (WPS_DATA *) &wps_global;

	if (((pwps_info->role == WPS_ENROLLEE)
	     && wps_s->bss_type == BSS_TYPE_STA)
	    || (IS_DISCOVERY_ENROLLEE(pwps_info))
	    ) {
		/*
		 * Start to exchange WPS Message by sending EAPOL_START packet
		 */
		pwps_info->eap_msg_sent = WPS_EAPOL_MSG_START;
		status =
		    wps_msg_transmit(pwps_info, WPS_EAPOL_MSG_START,
				     WPS_MSG_DEFAULT_TIMEOUT);
	}
#endif
	return status;
}

/**
 *  @brief Time-out handler of WPS Registration (300 seconds), program will
 *          exit if registration not completed within this time.
 *
 *  @param user_data    A pointer to user data of timer functions
 *  @return             None
 */
void wps_registration_time_handler(void *user_data)
{
#ifndef CONFIG_WPA2_ENTP
	int ret;
#endif
	PWPS_INFO pwps_info;
	WPS_DATA *wps_s = (WPS_DATA *) &wps_global;
	ENTER();

	pwps_info = (PWPS_INFO) user_data;

	wps_printf(DEBUG_WALK_TIME, "WPS Registration Timeout !");

	/* Cancel WPS Registration Timer */
	wps_printf(DEBUG_WPS_STATE, "Cancelling registration timer!");
	wps_cancel_timer(wps_registration_time_handler, pwps_info);

	/* Cancel packet Tx timer */
	if (pwps_info->set_timer == WPS_SET) {
		wps_cancel_timer(wps_txTimer_handler, pwps_info);
		pwps_info->set_timer = WPS_CANCEL;
		pwps_info->wps_msg_resent_count = 0;
	}

	if ((wps_s->bss_type == BSS_TYPE_STA && pwps_info->role == WPS_ENROLLEE)
	    || (IS_DISCOVERY_ENROLLEE(pwps_info))
	    ) {
#ifdef CONFIG_P2P
		if (wps.cb(P2P_SESSION_TIMEOUT, NULL, 0) == -WM_FAIL)
			WPS_LOG("P2P Callback failed for event: %d",
				P2P_SESSION_TIMEOUT);
#else
#ifndef CONFIG_WPA2_ENTP
		if (wps.cb(WPS_SESSION_TIMEOUT, NULL, 0) == -WM_FAIL)
			WPS_LOG("WPS Callback failed for event: %d",
				WPS_SESSION_TIMEOUT);
#endif
#endif

#ifndef CONFIG_WPA2_ENTP
		ret = wlan_disconnect();
		if (ret != 0)
			WPS_LOG("Failed to disconnect");

		ret = wlan_remove_network(wps_network.name);
		if (ret != 0)
			WPS_LOG("Failed to remove network");
#ifdef CONFIG_P2P
		wfd_reset();
#endif
		WPS_LOG("WPS registration timer time-out.");
		WPS_LOG("WPS Registration Protocol Not Completed!");

		/* shutdown the main processing loop */
		wps_main_loop_shutdown();
#else
		wpa2_failure = 1;

		/* shutdown the main processing loop */
		wps_main_loop_shutdown();
#endif
		pwps_info->wps_session = 0;

	} else
	    if ((wps_s->bss_type == BSS_TYPE_UAP
		 && pwps_info->role == WPS_REGISTRAR)
		|| (IS_DISCOVERY_REGISTRAR(pwps_info))) {

		WPS_LOG("WPS Registration timeout!!");
		WPS_LOG("Restarting WPS Registration.");

		pwps_info->wps_session = 0;

		/* shutdown the main processing loop */
		wps_main_loop_shutdown();
#ifdef CONFIG_P2P
		if (!auto_go) {
			ret = wlan_remove_network(p2p_uap_network.name);
			if (ret != 0)
				WPS_LOG("Failed to remove network");
		}

		if (wps.cb(P2P_SESSION_TIMEOUT, NULL, 0) == -WM_FAIL)
			WPS_LOG("P2P Callback failed for event: %d",
				P2P_SESSION_TIMEOUT);
#else
		if (wps.cb(WPS_SESSION_TIMEOUT, NULL, 0) == -WM_FAIL)
			WPS_LOG("WPS Callback failed for event: %d",
				WPS_SESSION_TIMEOUT);
#endif

		/* Shutdown main loop will cause registrar restart */
#if 0
		pwps_info->input_state = WPS_INPUT_STATE_ROLE;
		pwps_info->pin_pbc_set = WPS_CANCEL;

		wps_print_uap_role_menu(pwps_info);

		ret = wps_wlan_ie_config(SET_WPS_IE, &ie_index);
		if (ret != WPS_STATUS_SUCCESS) {
			wps_printf(MSG_WARNING, "Setting WPS IE failed!!!");
		}
		if (pwps_info->role == WPS_REGISTRAR) {
			if (pwps_info->registrar.version >= WPS_VERSION_2DOT0) {
				ret =
				    wps_wlan_ie_config
				    (SET_WPS_AP_SESSION_INACTIVE_AR_IE,
				     &ap_assocresp_ie_index);
				if (ret != WPS_STATUS_SUCCESS) {
					wps_printf(MSG_WARNING,
					   "WPS IE configuration failure.\n");
				}
			}
		} else if (pwps_info->role == WPS_ENROLLEE) {
			if (pwps_info->enrollee.version >= WPS_VERSION_2DOT0) {
				ret =
				    wps_wlan_ie_config
				    (SET_WPS_AP_SESSION_INACTIVE_AR_IE,
				     &ap_assocresp_ie_index);
				if (ret != WPS_STATUS_SUCCESS) {
					wps_printf(MSG_WARNING,
					   "WPS IE configuration failure.\n");
				}
			}
		}
#endif
	}

	LEAVE();
}

/**
 *  @brief EAPOL-Start handler of WPS Registration
 *  @param pwps_info    A pointer to WPS_INFO structure
 *  @return             None
 */
void wps_eapol_start_handler(WPS_INFO *pwps_info)
{
	ENTER();

	wps_printf(MSG_INFO,
		   "EAPOL-Start received. Aborting EAPOL State machine...");
	/* Cancel packet Tx timer */
	if (pwps_info->set_timer == WPS_SET) {
		wps_cancel_timer(wps_txTimer_handler, pwps_info);
		pwps_info->set_timer = WPS_CANCEL;
		pwps_info->wps_msg_resent_count = 0;
	}
	wps_printf(DEBUG_WPS_STATE, "State *->A");
	pwps_info->state = WPS_STATE_A;
	/* Reset Public keys and E-Hash, R-Hash */
	wps_reset_wps_state(pwps_info);
	LEAVE();
}

/**
 *  @brief EAPOL-Start handler of WPS Registration
 *  @param pwps_info    A pointer to WPS_INFO structure
 *  @return             None
 */
void wps_reset_wps_state(WPS_INFO *pwps_info)
{

	ENTER();
	wps_printf(MSG_INFO, "Resetting E-HASH, R-HASH and public keys");
	memset(pwps_info->enrollee.e_hash1, 0, 32);
	memset(pwps_info->enrollee.e_hash2, 0, 32);
	memset(pwps_info->enrollee.r_hash1, 0, 32);
	memset(pwps_info->enrollee.r_hash2, 0, 32);
	memset(pwps_info->enrollee.public_key, 0, 192);
	memset(pwps_info->registrar.public_key, 0, 192);
	if (pwps_info->last_message.length) {
		memset(pwps_info->last_message.message, 0,
		       pwps_info->last_message.length);
	}
	pwps_info->last_message.length = 0;
	pwps_info->last_recv_wps_msg = 0;
	LEAVE();
}
