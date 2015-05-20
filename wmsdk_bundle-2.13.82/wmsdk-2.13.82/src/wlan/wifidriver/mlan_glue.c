/*
 * This file acts as a glue between legacy wlan code and mlan based wlan
 * code. It is expected that this file will be completely removed or
 * atleast cut down to minimum eventually.
 */

#include <mlan_wmsdk.h>

/* Additional WMSDK header files */
#include <wmstdio.h>
#include <wmassert.h>
#include <wmerrno.h>
#include <wm_os.h>
#include <wifi_events.h>
#include "lwip/pbuf.h"
#include <wifi-debug.h>

#include <wifi.h>
#include "wifi-internal.h"
#include "wifi-sdio.h"

/* fixme: remove this as soon as there is no dependancy */
#define INCLUDE_FROM_MLAN
#include <wlan.h>

#ifdef CONFIG_WPS2
extern int wps_session_attempt;
#endif

#ifdef CONFIG_WPA2_ENTP
const uint8_t wpa2_akmp_oui[4] = { 0x00, 0x0f, 0xac, 0x01 };
extern int wpa_gen_wpa_ie(uint8_t *wpa_ie, size_t wpa_ie_len);
#endif
bool cal_data_valid;
uint8_t *cal_data;
uint32_t cal_data_len;

bool mac_addr_valid;
uint8_t *mac_addr;

extern bool wep_config_cmd_sent;
extern int current_mode;

/* fixme: This structure seems to have been removed from mlan. This was
   copied from userif_ext.h file temporarily. Change the handling of events to
   make it similar to what mlan does */
/** Event structure */
typedef MLAN_PACK_START struct _Event_Ext_t
{
	/** No of bytes in packet including this field */
	uint16_t length;
	/** Type: Event (3) */
	uint16_t type;
	/** Event ID */
	uint16_t event_id;
	/** BSS index number for multiple BSS support */
	uint8_t bss_index;
	/** BSS type */
	uint8_t bss_type;
	/** Reason code */
	uint16_t reason_code;
	/** Source MAC address */
	uint8_t src_mac_addr[MLAN_MAC_ADDR_LENGTH];
} MLAN_PACK_END Event_Ext_t;

/*
 *  fixme: This entry is not present in mlan_fw.h. It is likely possible
 *  that this code is no-op in the firmware. Keeping it as it is for
 *  maintaining backward compatibility. Can be removed later.
 */
#define host_CMD_802_11_AUTHENTICATE 0x0011

/* Following is allocated in mlan_register */
mlan_adapter *mlan_adap;

mlan_device mlan_dev;

uint8_t g_rssi;
int16_t g_bcn_nf_last;

/* fixme: This global variable is needed
 *  to save the correct event since SLP_CFRM command
 *  returns incorrect bitmap. This will be  further investigated
 *  and if possile global variable will be removed.*/
static t_u16 ps_event;

void deliver_packet_above(struct pbuf *p, int recv_interface);
struct pbuf *gen_pbuf_from_data(t_u8 *payload, t_u16 datalen);

#ifdef CONFIG_11N
/*
 * The command event received from the firmware (e.g. EVENT_ADDBA) cannot
 * be processed immediately. We need to do it the WLC manager context. For
 * that we will save the buffer lest some other event destroy it. WLC
 * manager will be notified of this event.
 *
 * Returns NULL if buffer allocation failed.
 */
static void *wifi_11n_save_request(Event_Ext_t *evt)
{
	void *dot11n_eventbuf = os_mem_alloc(evt->length);

	if (!dot11n_eventbuf) {
		wifi_w("11n eventbuf alloc failed %d", evt->length);
		return NULL;
	}

	return memcpy(dot11n_eventbuf, evt, evt->length);
}
#endif /* CONFIG_11N */

void wrapper_deliver_amsdu_subframe(pmlan_buffer amsdu_pmbuf,
			       t_u8 *data, t_u16 pkt_len)
{
	/* fixme: Check if a copyless operation can be done using PBUF_REF */
	struct pbuf *p = gen_pbuf_from_data(data, pkt_len);
	if (!p) {
		w_pkt_e("[amsdu] No pbuf available. Dropping packet");
		return;
	}

	RxPD *prx_pd = (RxPD *)amsdu_pmbuf->pbuf;
	w_pkt_d("[amsdu] [push]: BSS Type: %d L: %d",
		prx_pd->bss_type, pkt_len);
	deliver_packet_above(p, prx_pd->bss_type);
}

static mlan_status wrapper_moal_recv_packet(IN t_void * pmoal_handle,
				     IN pmlan_buffer pmbuf)
{
	ASSERT(pmbuf != NULL);

	RxPD *prx_pd = (RxPD *) (pmbuf->pbuf + pmbuf->data_offset);
	if (pmbuf->data_offset != 0) {
		wifi_e("pmbuf->data_offset != 0 (%d)?", pmbuf->data_offset);
		wmpanic();
	}

	struct pbuf *p = (struct pbuf *)pmbuf->lwip_pbuf;
	w_pkt_d("[push] %p + %d:  DELV: %p L: %d", pmbuf->pbuf,
		pmbuf->data_offset, p, pmbuf->data_len);
	deliver_packet_above(p, prx_pd->bss_type);

	/* Free RxPD */
	os_mem_free(pmbuf->pbuf);
	os_mem_free(pmbuf);
	return MLAN_STATUS_SUCCESS;
}

/** moal_init_timer*/
static mlan_status wrapper_moal_init_timer(IN t_void *pmoal_handle
				    __attribute__ ((unused)),
				    OUT t_void ** pptimer,
				    IN t_void(*callback) (t_void * pcontext),
				    IN t_void * pcontext)
{
	w_tmr_d("Creating timer");

	/*
	 * As can be seen from the arguments to our function, we do not
	 * have a timer period parameter. Nor do we have a parameter to
	 * specify whether the user wants a periodic or a one-shot
	 * timer. We have to wait for these paramters till the next call to
	 * wrapper_moal_start_timer.
	 *
	 * But a timer object cannot be created in
	 * wrapper_moal_start_timer. Because the timer handle is passed by
	 * value.
	 *
	 * Considering all this we will need to create the timer object in the
	 * current function. We will provide a dummy time here one and
	 * change in later. One assumption from the current observation is
	 * that only one shot timers are created. So we will assume this
	 * and raise assert in *timer_start if this is not the case.
	 */
#define DUMMY_TIME_PARAM 10

	int rv = os_timer_create(pptimer, NULL, DUMMY_TIME_PARAM,
				 callback, pcontext, OS_TIMER_ONE_SHOT,
				 OS_TIMER_NO_ACTIVATE);
	if (rv != WM_SUCCESS) {
		w_tmr_e("Unable to create timer");
		return MLAN_STATUS_FAILURE;
	}

	return MLAN_STATUS_SUCCESS;
}

/** moal_start_timer*/
static mlan_status wrapper_moal_start_timer(IN t_void * pmoal_handle
				     __attribute__ ((unused)),
				     IN t_void *ptimer,
				     IN t_u8 periodic, IN t_u32 msec)
{
	w_tmr_d("Starting timer: %p for %d mS", ptimer, msec);

	if (periodic == MTRUE) {
		/* Check note in wrapper_moal_init_timer for details */
		w_tmr_w("Periodic is not supported temporarily");
		wmpanic();
	}

	int rv = os_timer_change(&ptimer, os_msec_to_ticks(msec), 0);
	if (rv != WM_SUCCESS) {
		w_tmr_e("Unable to change period in timer");
		return MLAN_STATUS_FAILURE;
	}

	rv = os_timer_activate(&ptimer);
	if (rv != WM_SUCCESS) {
		w_tmr_e("Unable to start timer");
		return MLAN_STATUS_FAILURE;
	}

	return MLAN_STATUS_SUCCESS;
}

/** moal_stop_timer*/
static mlan_status wrapper_moal_stop_timer(IN t_void * pmoal_handle
				    __attribute__ ((unused)),
				    IN t_void *ptimer)
{
	w_tmr_d("Stopping timer: %p", ptimer);

	int rv = os_timer_deactivate(&ptimer);
	if (rv != WM_SUCCESS) {
		w_tmr_e("Unable to stop timer.");
		return MLAN_STATUS_FAILURE;
	}

	return MLAN_STATUS_SUCCESS;
}

/** moal_free_timer */
static mlan_status wrapper_moal_free_timer(IN t_void * pmoal_handle
				    __attribute__ ((unused)),
				    IN t_void * ptimer)
{
	w_tmr_d("Deleting timer: %p", ptimer);

	int rv = os_timer_delete(&ptimer);
	if (rv != WM_SUCCESS) {
		w_tmr_e("Unable to delete timer.");
		return MLAN_STATUS_FAILURE;
	}

	return MLAN_STATUS_SUCCESS;
}

static mlan_status wrapper_moal_init_lock(IN t_void * pmoal_handle,
				   OUT t_void ** pplock)
{
#ifdef DEBUG_DEVELOPMENT
	wmprintf("%s: Locks not needed for now\n\r", __func__);
#endif /* DEBUG_DEVELOPMENT */
	return MLAN_STATUS_SUCCESS;
}

/** moal_free_lock */
static mlan_status wrapper_moal_free_lock(IN t_void *pmoal_handle, IN t_void *plock)
{
#ifdef DEBUG_DEVELOPMENT
	wmprintf("%s: Locks not needed for now\n\r", __func__);
#endif /* DEBUG_DEVELOPMENT */
	return MLAN_STATUS_SUCCESS;
}

/** moal_spin_lock */
static mlan_status wrapper_moal_spin_lock(IN t_void * pmoal_handle,
					  IN t_void * plock)
{
#ifdef DEBUG_DEVELOPMENT
	wmprintf("%s: Locks not needed for now\n\r", __func__);
#endif /* DEBUG_DEVELOPMENT */
	return MLAN_STATUS_SUCCESS;
}

/** moal_spin_unlock */
static mlan_status wrapper_moal_spin_unlock(IN t_void * pmoal_handle,
				     IN t_void *plock)
{
#ifdef DEBUG_DEVELOPMENT
	wmprintf("%s: Locks not needed for now\n\r", __func__);
#endif /* DEBUG_DEVELOPMENT */
	return MLAN_STATUS_SUCCESS;
}

/** woal_callbacks */
static mlan_callbacks woal_callbacks = {
	.moal_malloc = wrapper_moal_malloc,
	.moal_mfree = wrapper_moal_mfree,
	.moal_recv_packet = wrapper_moal_recv_packet,
	.moal_init_timer = wrapper_moal_init_timer,
	.moal_free_timer = wrapper_moal_free_timer,
	.moal_start_timer = wrapper_moal_start_timer,
	.moal_stop_timer = wrapper_moal_stop_timer,
	.moal_init_lock = wrapper_moal_init_lock,
	.moal_free_lock = wrapper_moal_free_lock,
	.moal_spin_lock = wrapper_moal_spin_lock,
	.moal_spin_unlock = wrapper_moal_spin_unlock,
};

int mlan_subsys_init()
{
	memcpy(&mlan_dev.callbacks, &woal_callbacks, sizeof(mlan_callbacks));

	/* The mlinux driver has an array of these which is dynamically allocated
	 * in function woal_update_drv_tbl (moal_main.c). We have only one.
	 */
	/** BSS type */
	mlan_dev.bss_attr[0].bss_type = MLAN_BSS_TYPE_STA;
	/** Data frame type: Ethernet II, 802.11, etc. */
	mlan_dev.bss_attr[0].frame_type = MLAN_DATA_FRAME_TYPE_ETH_II;
	/** The BSS is active (non-0) or not (0). */
	mlan_dev.bss_attr[0].active = MTRUE;
	/** BSS Priority */
	mlan_dev.bss_attr[0].bss_priority = 0;
	/** BSS number */
	mlan_dev.bss_attr[0].bss_num = 0;
	/* 11d initialization */
	/* mlan_dev.cfg_11d = 0; */

	/* fixme: check return value above status */
	mlan_register(&mlan_dev, (t_void **) & mlan_adap);

	mlan_init_fw(mlan_adap);
	/* wlan_init_adapter(mlan_adap); */

	return WM_SUCCESS;
}

#ifdef DEBUG_11N_AGGR
int wifi_get_amsdu_status()
{
	wifi_get_command_lock();

	HostCmd_DS_COMMAND *cmd = wifi_get_command_buffer();

	memset(cmd, 0x00, sizeof(HostCmd_DS_COMMAND));

	cmd->seq_num = 0x0;
	cmd->result = 0x0;

	wlan_cmd_amsdu_aggr_ctrl((mlan_private *) mlan_adap->priv[0],
				 cmd, HostCmd_ACT_GEN_GET, NULL);

	wmprintf("Waiting for cmdresp\n\r");
	wifi_wait_for_cmdresp(NULL);
	wmprintf("DONE\n\r");

	mlan_ds_11n_tx_cfg txcfg;
	memset(&txcfg, 0x00, sizeof(mlan_ds_11n_tx_cfg));

	wifi_get_command_lock();
	wlan_cmd_11n_cfg((mlan_private *) mlan_adap->priv[0],
			 cmd, HostCmd_ACT_GEN_GET, &txcfg);

	wmprintf("Waiting for cmdresp\n\r");
	wifi_wait_for_cmdresp(NULL);
	wmprintf("DONE\n\r");

	return 0;
}
#endif /* DEBUG_11N_AGGR */

#ifdef CONFIG_11N
/* Note: The saved_event_buff is allocated by the cmd resp/event handler
   thread. We need to free it in this function */
int wrapper_wlan_cmd_11n_addba_rspgen(void *saved_event_buff)
{
	ASSERT(saved_event_buff != NULL);

	Event_Ext_t *evt = (Event_Ext_t *)saved_event_buff;

	wifi_get_command_lock();

	HostCmd_DS_COMMAND *cmd = wifi_get_command_buffer();

	memset(cmd, 0x00, sizeof(HostCmd_DS_COMMAND));

	cmd->seq_num = 0x0;
	cmd->result = 0x0;

	wlan_cmd_11n_addba_rspgen((mlan_private *) mlan_adap->priv[0],
				  cmd, &evt->reason_code);

#ifdef DEBUG_11N_AGGR
	wmprintf("ADDBA response\n\r");
	dump_addba_req_rsp_packet(cmd);
	wmprintf("Waiting for cmd resp\n\r");
#endif /* DEBUG_11N_AGGR */

	os_mem_free(saved_event_buff);
	wifi_wait_for_cmdresp(NULL);

	return 0;
}
#endif /* CONFIG_11N */

#ifdef CONFIG_11N
static mlan_status do_wlan_ret_11n_addba_resp(HostCmd_DS_COMMAND *resp)
{
#ifdef DEBUG_11N_AGGR
	wmprintf("ADDBA response result\n\r");
	dump_addba_req_rsp_packet(rspbuf);
#endif /* DEBUG_11N_AGGR */

	if (resp->result != HostCmd_RESULT_OK) {
		wifi_e("Failed: ADDBA RESULT: %d", resp->result);
		return -WM_FAIL;
	}

	mlan_private *pmpriv = (mlan_private *) mlan_adap->priv[0];
	mlan_status rv = wlan_ret_11n_addba_resp(pmpriv, resp);

#ifdef DEBUG_11N_AGGR
	wmprintf("ADDBA RESP RESP: %d\n\r", resp->result);
#endif /* DEBUG_11N_AGGR */

	return rv;
}
#endif /* CONFIG_11N */

#ifdef CONFIG_11D
int wrapper_wlan_cmd_11d_enable(HostCmd_DS_COMMAND *cmd)
{
	t_u32 enable_11d = ENABLE_11D;
	memset(cmd, 0x00, sizeof(HostCmd_DS_COMMAND));
	cmd->seq_num = 0x0;
	cmd->result = 0x0;
	return  wlan_ops_sta_prepare_cmd((mlan_private *) mlan_adap->priv[0],
					 HostCmd_CMD_802_11_SNMP_MIB,
					 HostCmd_ACT_GEN_SET,
					 Dot11D_i,
					 NULL, &enable_11d, cmd);
}
#endif /* CONFIG_11D */


int wrapper_wlan_cmd_get_hw_spec()
{
	wifi_get_command_lock();
	HostCmd_DS_COMMAND *cmd = wifi_get_command_buffer();
	memset(cmd, 0x00, sizeof(HostCmd_DS_COMMAND));
	cmd->seq_num = 0x0;
	cmd->result = 0x0;
	wlan_cmd_get_hw_spec((mlan_private *) mlan_adap->priv[0],
			     cmd);
	wifi_wait_for_cmdresp(NULL);
	return 0;
}

int wrapper_wlan_cmd_mgmt_ie(int bss_type, void *buffer,
				unsigned int len, unsigned int action)
{
	void *pdata_buf = NULL;
	HostCmd_DS_MGMT_IE_LIST_CFG ds_mgmt_ie_list_cfg;

	wifi_get_command_lock();

	HostCmd_DS_COMMAND *cmd = wifi_get_command_buffer();
	memset(cmd, 0x00, sizeof(HostCmd_DS_COMMAND));
#ifdef CONFIG_P2P
	cmd->seq_num = HostCmd_SET_SEQ_NO_BSS_INFO(
						     0 /* seq_num */,
						     0 /* bss_num */,
						     MLAN_BSS_TYPE_WIFIDIRECT);
#else
	cmd->seq_num = HostCmd_SET_SEQ_NO_BSS_INFO(
						   0 /* seq_num */,
						   0 /* bss_num */,
						   bss_type);
#endif
	cmd->result = 0x0;

	memset(&ds_mgmt_ie_list_cfg, 0x00, sizeof(HostCmd_DS_MGMT_IE_LIST_CFG));

	memcpy(&ds_mgmt_ie_list_cfg, buffer, len);

	pdata_buf = &ds_mgmt_ie_list_cfg;

	mlan_status status =
		wlan_ops_sta_prepare_cmd(
					 (mlan_private *) mlan_adap->priv[0],
					 HostCmd_CMD_MGMT_IE_LIST,
					 action,
					 0, NULL, pdata_buf, cmd);

	wifi_wait_for_cmdresp(buffer);
	return status;
}

void wrapper_wifi_ret_mib(HostCmd_DS_COMMAND *resp)
{
	wlan_ops_sta_process_cmdresp((mlan_private *) mlan_adap->priv[0],
				    HostCmd_CMD_802_11_SNMP_MIB,
				    resp, NULL);
}



int wrapper_wlan_handle_rx_packet(t_u16 datalen, RxPD *rxpd, struct pbuf *p)
{
	w_pkt_d("[recv]                  : %p T: %d L: %d", p,
		rxpd->rx_pkt_type, rxpd->rx_pkt_length);

	/* fixme: Check if mlan buffer can be allocated from standard mlan
	   function */
	pmlan_buffer pmbuf = os_mem_alloc(sizeof(mlan_buffer));
	if (!pmbuf) {
		/* No mlan_buffer available. Drop this packet */
		/* fixme: Need to note this event. */
		wifi_w("No memory available. Have to drop packet.");
		return -WM_FAIL;
	}
	memset(pmbuf, 0x00, sizeof(mlan_buffer));

	/** Buffer descriptor, e.g. skb in Linux */
	/* Note: We are storing payload member here. We need to unwind
	   pointer when passing pbuf to lwip */
	pmbuf->pdesc = p->payload;

	/* Store it for later retrivval. mlan stack does not know about this */
	pmbuf->lwip_pbuf = p;

	/* fixme: check that no data is accessed after this pointer
	  Note: Important: mlan requires RxPD in number of places. We
	already have duplicated the ethernet packet in the pbuf. We cannot
	afford to duplicate the entire SDIO buffer and reduplicate the
	ethernet part of it into pbuf which the lwip requires. So, we will
	duplicate only the RxPD for ourselves. We will have to change all
	the code which assumes that there is ethernet packet after RxPD */
	/** Pointer to buffer */
	/* fixme: CHK this*/
	pmbuf->pbuf = (t_u8 *) os_mem_alloc(sizeof(RxPD));
	if (!pmbuf->pbuf) {
		/* No buffer available. Drop this packet */
		/* fixme: Need to note this event. */
		os_mem_free(pmbuf);
		wifi_w("No memory available. Have to drop packet.");
		return -WM_FAIL;
	}
	memcpy(pmbuf->pbuf, rxpd, sizeof(RxPD));

	/** Offset to data */
	/* This should ideally be INTF_HEADER_LEN. But we not be storing
	   initial INTF_HEADER_LEN bytes. We will store RxPD directly */
	/* fixme: CHK this*/
	pmbuf->data_offset = 0;

	/** Data length */
	/* fixme: CHK this*/
	pmbuf->data_len = datalen;
	/** Buffer type: data, cmd, event etc. */
	pmbuf->buf_type = MLAN_BUF_TYPE_DATA;
	/** Fields below are valid for data packet only */
	/** QoS priority */
	/* t_u32 priority; */
	/** Time stamp when packet is received (seconds) */
	/* t_u32 in_ts_sec; */
	/** Time stamp when packet is received (micro seconds) */
	/* t_u32 in_ts_usec; */
	/** Time stamp when packet is processed (seconds) */
	/* t_u32 out_ts_sec; */
	/** Time stamp when packet is processed (micro seconds) */
	/* t_u32 out_ts_usec; */

	/** Fields below are valid for MLAN module only */
	/** Pointer to parent mlan_buffer */
	/* struct _mlan_buffer *pparent; */
	/** Use count for this buffer */
	/* t_u32 use_count; */

	mlan_status rv = wlan_handle_rx_packet(mlan_adap, pmbuf);
	if (rv != MLAN_STATUS_SUCCESS) {
		/* mlan did not accept this packet for some reason. Some
		   probable reasons are - duplicate packet - packet outside
		   the current 11n aggregation window.

		   We need to free allocated structures.
		*/
		os_mem_free(pmbuf->pbuf);
		os_mem_free(pmbuf);
		return -WM_FAIL;
	}

	return WM_SUCCESS;
}

/* fixme: This AMSDU special handling needs to be redesigned. We could
   create a copy of the SDIO packet buffer for _every_ packet AMSDU and
   non-AMSDU. We could then use RAW_PBUF allocation to allocate pbufs
   during final step of packet delivery to lwip stack.

   Note: This is not AMSDU inside AMPDU. This is a pure AMSDU.
   Note: We will not be duplicating the AMSDU packet from the SDIO
   buffer. So, we have to ensure that there are no race
   conditions. i.e. the SDIO buffer is not overwritten in the background.
*/
int wrapper_wlan_handle_amsdu_rx_packet(t_u8 *rcvdata, t_u16 datalen)
{
	w_pkt_d("[amsdu] [recv]       : L: %d", datalen);

	RxPD *rxpd = (RxPD *) ((t_u8 *) rcvdata + INTF_HEADER_LEN);

	ASSERT(rxpd->rx_pkt_type == PKT_TYPE_AMSDU);

	/* fixme: Check if mlan buffer can be allocated from standard mlan
	   function */
	pmlan_buffer pmbuf = os_mem_alloc(sizeof(mlan_buffer));
	if (!pmbuf) {
		/* No mlan_buffer available. Drop this packet */
		/* fixme: Need to note this event. */
		w_pkt_e("[amsdu] No memory available. Have to drop packet");
		return -WM_FAIL;
	}
	memset(pmbuf, 0x00, sizeof(mlan_buffer));

	/** Buffer descriptor, e.g. skb in Linux */
	/* Note: We are storing payload member here. We need to unwind
	   pointer when passing pbuf to lwip */
	pmbuf->pdesc = &rcvdata[INTF_HEADER_LEN + rxpd->rx_pkt_offset];

	/* Store it for later retrivval. mlan stack does not know about this */
	pmbuf->lwip_pbuf = NULL;

	/* Since this is an AMSDU packet we have the entire SDIO buffer
	   with us. So we do not need to duplicate RxPD
	*/
	pmbuf->pbuf = (t_u8 *) rxpd;
	/** Offset to data */
	pmbuf->data_offset = 0;

	/** Data length */
	/* fixme: CHK this*/
	pmbuf->data_len = datalen;
	/** Buffer type: data, cmd, event etc. */
	pmbuf->buf_type = MLAN_BUF_TYPE_DATA;
	/** Fields below are valid for data packet only */
	/** QoS priority */
	/* t_u32 priority; */
	/** Time stamp when packet is received (seconds) */
	/* t_u32 in_ts_sec; */
	/** Time stamp when packet is received (micro seconds) */
	/* t_u32 in_ts_usec; */
	/** Time stamp when packet is processed (seconds) */
	/* t_u32 out_ts_sec; */
	/** Time stamp when packet is processed (micro seconds) */
	/* t_u32 out_ts_usec; */

	/** Fields below are valid for MLAN module only */
	/** Pointer to parent mlan_buffer */
	/* struct _mlan_buffer *pparent; */
	/** Use count for this buffer */
	/* t_u32 use_count; */

	wlan_handle_rx_packet(mlan_adap, pmbuf);
	/* if (rv != MLAN_STATUS_SUCCESS) { */
	/*
	  We need to free allocated structures. In case of AMSDU this pmbuf
	  is not freed inside mlan
	*/
	os_mem_free(pmbuf);
	/* return -WM_FAIL; */
	/* } */

	return WM_SUCCESS;
}

void wrapper_wlan_cmd_11n_cfg(HostCmd_DS_COMMAND * cmd) {

	mlan_ds_11n_tx_cfg txcfg = {
		/** HTTxCap */
		.httxcap = DOT11N_CFG_ENABLE_GREENFIELD_XMIT |
		DOT11N_CFG_ENABLE_SHORT_GI_20MHZ |
		DOT11N_CFG_ENABLE_SHORT_GI_40MHZ,
		/** HTTxInfo */
		.httxinfo = DOT11N_CFG_ENABLE_RIFS,
		/** Band selection */
		.misc_cfg = BAND_SELECT_BOTH
	};

	wlan_cmd_11n_cfg((mlan_private *) mlan_adap->priv[0],
			 cmd, HostCmd_ACT_GEN_SET, &txcfg);
}


/*
  fixme: Following three functions will not be needed when
  wlan_11h_handle_event_chanswann is used to handle the relevant event */
void set_event_chanswann()
{
	mlan_adap->state_11h.recvd_chanswann_event = MTRUE;
}

void clear_event_chanswann()
{
	mlan_adap->state_11h.recvd_chanswann_event = MFALSE;
}

void wrapper_wlan_11d_clear_parsedtable()
{
	mlan_private *pmpriv = (mlan_private *) mlan_adap->priv[0];
	wlan_11d_clear_parsedtable(pmpriv);
}

/*
  This was added to solve the radar DFS issue. The function
  wlan_11h_radar_detect_required() checks for this and sets the scan to
  active if media is connected. The temporary problem with our current port
  of mlan is that after a deauth is sent after channel switch event from
  the AP, the media connected event is still MTRUE. Hence, during scan the
  old channel of the AP is marked as active scan which is not a correct way
  of scanning on that channel (since radar interference is detected on
  it). So, for now we clear this bit manually to avoid active scan on the
  old channel. This bit will be set again at the next association completion.

  fixme: Check this later.
 */
void wrapper_clear_media_connected_event()
{
	mlan_private *pmpriv = (mlan_private *) mlan_adap->priv[0];
	pmpriv->media_connected = MFALSE;
}

mlan_status wifi_prepare_and_send_cmd(IN mlan_private * pmpriv,
				      IN t_u16 cmd_no,
				      IN t_u16 cmd_action,
				      IN t_u32 cmd_oid,
				      IN t_void * pioctl_buf,
				      IN t_void * pdata_buf,
				      int bss_type, void *priv)
{
	pmlan_ioctl_req pioctl_req = (mlan_ioctl_req *) pioctl_buf;
	mlan_status rv = MLAN_STATUS_SUCCESS;

	wifi_get_command_lock();
	HostCmd_DS_COMMAND *cmd = wifi_get_command_buffer();

	if (pioctl_req)
		if (pioctl_req->bss_index == 1)
			bss_type = MLAN_BSS_TYPE_UAP;

#ifdef CONFIG_P2P
	cmd->seq_num = HostCmd_SET_SEQ_NO_BSS_INFO(
						   0 /* seq_num */,
						   0 /* bss_num */,
						   MLAN_BSS_TYPE_WIFIDIRECT);
#else
	cmd->seq_num = HostCmd_SET_SEQ_NO_BSS_INFO(
						   0 /* seq_num */,
						   0 /* bss_num */,
						   bss_type);
#endif /* CONFIG_P2P */
	cmd->result = 0x0;

	if (bss_type == MLAN_BSS_TYPE_UAP)
		rv = wlan_ops_uap_prepare_cmd(pmpriv, cmd_no,
				  cmd_action, cmd_oid, pioctl_buf, pdata_buf, cmd);
	else
		rv = wlan_ops_sta_prepare_cmd(pmpriv, cmd_no,
				  cmd_action, cmd_oid, pioctl_buf, pdata_buf, cmd);

	if (rv != MLAN_STATUS_SUCCESS) {
		wifi_e("Failed to prepare cmd.");
		return rv;
	}

	wifi_wait_for_cmdresp(priv);
	return MLAN_STATUS_SUCCESS;
}

bool wmsdk_is_11N_enabled()
{
#ifdef CONFIG_11N
	return true;
#else
	return false;
#endif /* CONFIG_11N */
}

/* static */ int wifi_send_bss_ioctl(mlan_ds_bss *bss)
{
	/* fixme: check if this needs to go on heap */
	mlan_ioctl_req req;

	memset(&req, 0x00, sizeof(mlan_ioctl_req));
	req.pbuf = (t_u8 *)bss;
	req.buf_len = sizeof(mlan_ds_bss);
	req.bss_index = 0;
	req.req_id = MLAN_IOCTL_BSS;

	mlan_status rv = wlan_ops_sta_ioctl(mlan_adap, &req);
	if (rv != MLAN_STATUS_SUCCESS && rv != MLAN_STATUS_PENDING) {
		return -WM_FAIL;
	}

	return WM_SUCCESS;
}

int wrapper_get_wpa_ie_in_assoc(uint8_t *wpa_ie)
{
	mlan_private *priv = (mlan_private *) mlan_adap->priv[0];

	/* Reset all state variables */
	memcpy(wpa_ie, &priv->wpa_ie, sizeof(priv->wpa_ie));

	return priv->wpa_ie_len;
}

static int wifi_send_tx_power_cfg_ioctl(int action, mlan_ds_power_cfg *ds_power_cfg)
{
	/* fixme: check if this needs to go on heap */
	mlan_ioctl_req req;

	memset(&req, 0x00, sizeof(mlan_ioctl_req));
	req.pbuf = (t_u8 *) ds_power_cfg;
	req.buf_len = sizeof(mlan_ds_power_cfg);
	req.bss_index = 0;
	req.req_id = MLAN_IOCTL_POWER_CFG;
	req.action = action;

	mlan_status rv = wlan_ops_sta_ioctl(mlan_adap, &req);
	if (rv != MLAN_STATUS_SUCCESS && rv != MLAN_STATUS_PENDING) {
		return -WM_FAIL;
	}

	return WM_SUCCESS;
}

int wifi_set_tx_power(int power_level)
{
	mlan_ds_power_cfg ds_power_cfg;

	memset(&ds_power_cfg, 0x00, sizeof(mlan_ds_power_cfg));

	ds_power_cfg.sub_command = MLAN_OID_POWER_CFG;
	ds_power_cfg.param.power_cfg.is_power_auto = false;
	ds_power_cfg.param.power_cfg.power_level = power_level;

	return wifi_send_tx_power_cfg_ioctl(MLAN_ACT_SET, &ds_power_cfg);
}

int wifi_set_tx_power_ext(uint32_t len, uint32_t *power_data)
{
	mlan_ds_power_cfg ds_power_cfg;

	memset(&ds_power_cfg, 0x00, sizeof(mlan_ds_power_cfg));

	ds_power_cfg.sub_command = MLAN_OID_POWER_CFG_EXT;
	ds_power_cfg.param.power_ext.len = len;
	memcpy(ds_power_cfg.param.power_ext.power_data, power_data, len);

	return wifi_send_tx_power_cfg_ioctl(MLAN_ACT_SET, &ds_power_cfg);
}

int wifi_get_tx_power()
{
	mlan_ds_power_cfg ds_power_cfg;

	memset(&ds_power_cfg, 0x00, sizeof(mlan_ds_power_cfg));

	ds_power_cfg.sub_command = MLAN_OID_POWER_CFG;
	ds_power_cfg.param.power_cfg.is_power_auto = false;

	return wifi_send_tx_power_cfg_ioctl(MLAN_ACT_GET, &ds_power_cfg);
}

/*
 * fixme: This function is temporarily present till the mlan transition is complete.
 */
int wrapper_wifi_assoc(const char *bssid, int wlan_security)
{
	mlan_private *priv = (mlan_private *) mlan_adap->priv[0];
	/* BSSDescriptor_t *bssDesc = os_mem_alloc(sizeof(BSSDescriptor_t)); */
	/* if (!bssDesc) */
	/* 	return -WM_FAIL; */
	int idx = wlan_find_bssid_in_list(priv, (unsigned char*)bssid, -1);
	if (idx == -1) {
		wifi_w("Could not find BSSID in mlan scan list");
		return -WLAN_BSSID_NOT_FOUND_IN_SCAN_LIST;
	}

	/* Reset all state variables */
	memset(&priv->wpa_ie, 0, sizeof(priv->wpa_ie));
	priv->wpa_ie_len = 0;
	priv->sec_info.wpa2_enabled = false;
	priv->sec_info.wapi_enabled = false;
	priv->sec_info.ewpa_enabled = false;
	priv->sec_info.wpa_enabled = false;

	BSSDescriptor_t *d = &mlan_adap->pscan_table[idx];
	/* fixme: This code is quite hacky and is present only because
	 * security part is yet not fully integrated into mlan. This will
	 * not be necessary after the integration is complete.
	*/
#ifdef CONFIG_WPA2_ENTP
	 if (d->prsn_ie && wlan_security == WLAN_SECURITY_EAP_TLS) {
		priv->sec_info.wpa2_enabled = true;
		if (d->rsn_ie_buff_len <= sizeof(priv->wpa_ie)) {
			memcpy(priv->wpa_ie, d->rsn_ie_buff, d->rsn_ie_buff_len);
			priv->wpa_ie_len = d->rsn_ie_buff_len;
			//wpa_gen_wpa_ie(priv->wpa_ie, d->rsn_ie_buff_len);
		}
		else {
			wmprintf("Error: Failed to copy RSN IE\n\r");
			return -WM_FAIL;
		}
	} else
#endif
	if (d->pwpa_ie && d->prsn_ie && (wlan_security ==
				WLAN_SECURITY_WPA_WPA2_MIXED)) {
		priv->sec_info.wpa2_enabled = true;
		priv->sec_info.wpa_enabled = true;
		if (d->rsn_ie_buff_len <= sizeof(priv->wpa_ie)) {
			memcpy(priv->wpa_ie, d->rsn_ie_buff, d->rsn_ie_buff_len);
			priv->wpa_ie_len = d->rsn_ie_buff_len;
		}
		else {
			wifi_e("Failed to copy RSN IE.");
			return -WM_FAIL;
		}
	} else if (d->pwpa_ie && (wlan_security == WLAN_SECURITY_WPA ||
			wlan_security == WLAN_SECURITY_WPA_WPA2_MIXED)) {
		priv->sec_info.wpa_enabled = true;
		if (d->prsn_ie)
			priv->sec_info.wpa2_enabled = true;
		if (d->wpa_ie_buff_len <= sizeof(priv->wpa_ie)) {
			memcpy(priv->wpa_ie, d->wpa_ie_buff, d->wpa_ie_buff_len);
			priv->wpa_ie_len = d->wpa_ie_buff_len;
		}
		else {
			wifi_e("Failed to copy WPA IE");
			return -WM_FAIL;
		}
	} else if (d->prsn_ie && (wlan_security == WLAN_SECURITY_WPA2 ||
			wlan_security == WLAN_SECURITY_WPA_WPA2_MIXED)) {
		priv->sec_info.wpa2_enabled = true;
		if (d->pwpa_ie)
			priv->sec_info.wpa_enabled = true;
		if (d->rsn_ie_buff_len <= sizeof(priv->wpa_ie)) {
			memcpy(priv->wpa_ie, d->rsn_ie_buff, d->rsn_ie_buff_len);
			priv->wpa_ie_len = d->rsn_ie_buff_len;
		}
		else {
			wifi_e("Failed to copy RSN IE.");
			return -WM_FAIL;
		}
	}

	/* The original assoc cmd handling function of mlan sends
	   additional two commands to the firmware; both
	   HostCmd_CMD_802_11D_DOMAIN_INFO. In the current wmsdk wifi
	   driver design we have only one command buffer. Thus, we cannot
	   new cmd while the current cmd is being sent. We will change the
	   default flow a little and send the 801.11D domain info commands
	   first and skip that step later */
#ifdef CONFIG_11D
	if (wlan_11d_create_dnld_countryinfo(priv, (t_u8) d->bss_band)) {
		PRINTM(MERROR, "Dnld_countryinfo_11d failed\n");
		return MLAN_STATUS_FAILURE;
	}

	if (wlan_11d_parse_dnld_countryinfo(priv, d))
		return MLAN_STATUS_FAILURE;

#endif /* CONFIG_11D */

	/* fixme: check if this needs to go on heap */
	mlan_ds_bss bss;
	memset(&bss, 0x00, sizeof(mlan_ds_bss));
	bss.sub_command = MLAN_OID_BSS_START;
	bss.param.ssid_bssid.idx = idx + 1; /* + 1 req. by mlan */
	return wifi_send_bss_ioctl(&bss);
}
static void compute_rssi_values(HostCmd_DS_802_11_RSSI_INFO_RSP *prssi_info_rsp,
				wifi_rssi_info_t *rssi_info)
{
	g_rssi = prssi_info_rsp->bcn_rssi_last;
	g_bcn_nf_last = prssi_info_rsp->bcn_nf_last;

	rssi_info->bcn_rssi_last = prssi_info_rsp->bcn_rssi_last;
	rssi_info->bcn_rssi_avg = prssi_info_rsp->bcn_rssi_avg;
	rssi_info->data_rssi_last = prssi_info_rsp->data_rssi_last;
	rssi_info->data_rssi_avg = prssi_info_rsp->data_rssi_avg;

	rssi_info->bcn_snr_last = CAL_SNR(prssi_info_rsp->bcn_rssi_last,
					prssi_info_rsp->bcn_nf_last);
	rssi_info->bcn_snr_avg = CAL_SNR(prssi_info_rsp->bcn_rssi_avg,
					prssi_info_rsp->bcn_nf_avg);
	rssi_info->data_snr_last = CAL_SNR(prssi_info_rsp->data_rssi_last,
					prssi_info_rsp->data_nf_last);
	rssi_info->data_snr_avg = CAL_SNR(prssi_info_rsp->data_rssi_avg,
					prssi_info_rsp->data_nf_avg);

	rssi_info->bcn_nf_last = prssi_info_rsp->bcn_nf_last;
	rssi_info->bcn_nf_avg = prssi_info_rsp->bcn_nf_avg;
	rssi_info->data_nf_last = prssi_info_rsp->data_nf_last;
	rssi_info->data_nf_avg = prssi_info_rsp->data_nf_avg;

}
static void compute_tx_rate(const mlan_private *pmpriv,
		const HostCmd_TX_RATE_QUERY *datarate, wifi_tx_rate_t *tx_r)
{
	if (datarate->ht_info & MBIT(0)) {
		/* HT rates detected */
		tx_r->tx_rate = datarate->tx_rate + MLAN_RATE_INDEX_MCS0;
	} else {
		/*
		 * For HostCmd_CMD_802_11_TX_RATE_QUERY, there is a hole in
		 * the rate table between HR/DSSS and OFDM rates, so minus
		 * 1 for OFDM rate index.
		 */
		tx_r->tx_rate = (datarate->tx_rate > MLAN_RATE_INDEX_OFDM0) ?
			datarate->tx_rate - 1 : datarate->tx_rate;
	}
	if (pmpriv->rxpd_htinfo & MBIT(0)) {
		tx_r->rx_rate = pmpriv->rxpd_rate + MLAN_RATE_INDEX_MCS0;
	} else {
		/*
		 * For rate index in RxPD, there is a hole in the
		 * rate table between HR/DSSS and OFDM rates, so
		 * minus 1 for OFDM rate index
		 */
		tx_r->rx_rate = (pmpriv->rxpd_rate > MLAN_RATE_INDEX_OFDM0)
			? pmpriv->rxpd_rate - 1 : pmpriv->rxpd_rate;
	}
}


static void load_bss_list(const HostCmd_DS_STA_LIST *sta_list)
{
	if (!wm_wifi.cmd_resp_priv)
		return;

	/* Initialize user pointer */
	*(sta_list_t **)wm_wifi.cmd_resp_priv = NULL;

	t_u16 c = sta_list->sta_count;
	if (c > MAX_NUM_CLIENTS) {
		wifi_w("Too many clients: %d", c);
		return;
	}

	sta_list_t *sl = os_mem_alloc(sizeof(sta_list_t) +
				     c * sizeof(sta_info_t));

	if (!sl)
		return;

	sl->count = c;
	sta_info_t *sta = (sta_info_t *)(&sl->count + 1);

	int i;
	const MrvlIEtypes_sta_info_t *si =
		(MrvlIEtypes_sta_info_t *)(&sta_list->sta_count + 1);
	for (i = 0; i < c && i < MAX_NUM_CLIENTS; i++) {
		if (si[i].rssi & 0x80)
			sta[i].rssi = -(256 - si[i].rssi);
		else
			sta[i].rssi = si[i].rssi;

		memcpy(sta[i].mac, si[i].mac_address, MLAN_MAC_ADDR_LENGTH);
		sta[i].power_mgmt_status = si[i].power_mfg_status;

		wifi_d("RSSI: 0x%x %d dbm",
			 sta[i].rssi, sta[i].rssi);
	}

	/* Note: Caller needs to free this */
	*(sta_list_t **)wm_wifi.cmd_resp_priv = sl;

	wm_wifi.cmd_resp_status = WM_SUCCESS;
	wm_wifi.cmd_resp_priv = NULL;
}

static void load_ver_ext(HostCmd_DS_COMMAND *resp)
{
	if (!wm_wifi.cmd_resp_priv)
		return;

	HostCmd_DS_VERSION_EXT *ver_ext = &resp->params.verext;
	wifi_fw_version_ext_t *user_ver_ext =
		(wifi_fw_version_ext_t *)wm_wifi.cmd_resp_priv;

	memcpy(user_ver_ext->version_str, ver_ext->version_str,
	       MLAN_MAX_VER_STR_LEN);

	wm_wifi.cmd_resp_status = WM_SUCCESS;
	wm_wifi.cmd_resp_priv = NULL;
}

static void load_rf_antenna_mode(HostCmd_DS_COMMAND *resp)
{
	if (!wm_wifi.cmd_resp_priv)
		return;

	/* For antenna "mode set" wm_wifi.cmd_resp_priv will be NULL */

	HostCmd_DS_802_11_RF_ANTENNA *antcfg =
		(HostCmd_DS_802_11_RF_ANTENNA *)
		&resp->params;

	uint16_t *ant_mode = (uint16_t *)wm_wifi.cmd_resp_priv;

	*ant_mode = antcfg->antenna_mode;

	wm_wifi.cmd_resp_status = WM_SUCCESS;
	wm_wifi.cmd_resp_priv = NULL;
}

int wifi_process_cmd_response(HostCmd_DS_COMMAND *resp)
{
	mlan_private *pmpriv = (mlan_private *) mlan_adap->priv[0];
	mlan_status rv = MLAN_STATUS_SUCCESS;
	int result = WIFI_EVENT_REASON_FAILURE;
	void *arg;

	int command = (resp->command & HostCmd_CMD_ID_MASK);

	wcmdr_d("[%-8u] CMD-RESP: 0x%-2x Size: %-4d Seq: %-4d Result: %d",
		os_get_timestamp(), command,
		resp->size, resp->seq_num, resp->result);

	switch (command) {
	case HOST_CMD_APCMD_BSS_STOP: {
		pmpriv->uap_bss_started = MFALSE;
		int bss_type = HostCmd_GET_BSS_TYPE(resp->seq_num);
		if ((bss_type == MLAN_BSS_TYPE_UAP)
#ifdef CONFIG_P2P
		|| (bss_type == MLAN_BSS_TYPE_WIFIDIRECT)
#endif /* CONFIG_P2P*/
		)
			wifi_event_completion(WIFI_EVENT_UAP_STOPPED,
					      WIFI_EVENT_REASON_SUCCESS,
					      NULL);
	}
		break;
	case HOST_CMD_APCMD_BSS_START: {
		int bss_type = HostCmd_GET_BSS_TYPE(resp->seq_num);
		if ((bss_type == MLAN_BSS_TYPE_UAP)
#ifdef CONFIG_P2P
		|| (bss_type == MLAN_BSS_TYPE_WIFIDIRECT)
#endif /* CONFIG_P2P*/
		)
			wifi_event_completion(WIFI_EVENT_UAP_STARTED,
					      WIFI_EVENT_REASON_SUCCESS, NULL);
	}
		break;
	case HOST_CMD_APCMD_STA_LIST: {
		const HostCmd_DS_STA_LIST *sta_list = &resp->params.sta_list;
		load_bss_list(sta_list);
	}
		break;
	case HostCmd_CMD_802_11_TX_RATE_QUERY: {
		rv = wlan_ops_sta_process_cmdresp(pmpriv, command,
						  resp, NULL);
		if (rv != MLAN_STATUS_SUCCESS)
			return -WM_FAIL;
		if (wm_wifi.cmd_resp_priv) {
			wifi_tx_rate_t *tx_r = (wifi_tx_rate_t *)
					wm_wifi.cmd_resp_priv;
			wm_wifi.cmd_resp_priv = NULL;
			compute_tx_rate(pmpriv, &resp->params.tx_rate,
				tx_r);
		}
		wm_wifi.cmd_resp_status = WM_SUCCESS;
	}
		break;
	case HostCmd_CMD_802_11_RF_TX_POWER: {
		const HostCmd_DS_802_11_RF_TX_POWER *txp = &resp->params.txp;
		if (txp->action == HostCmd_ACT_GEN_GET) {
			if (wm_wifi.cmd_resp_priv) {
				wifi_tx_power_t *tx_p = (wifi_tx_power_t *)
					wm_wifi.cmd_resp_priv;
				wm_wifi.cmd_resp_priv = NULL;
				tx_p->current_level = txp->current_level;
				tx_p->max_power = txp->max_power;
				tx_p->min_power = txp->min_power;
			}
		}
		wm_wifi.cmd_resp_status = WM_SUCCESS;
	}
		break;
	case HostCmd_CMD_802_11D_DOMAIN_INFO:
		/* No processing needed as of now */
		break;
	case HostCmd_CMD_GET_HW_SPEC:
		rv = wlan_ops_sta_process_cmdresp(pmpriv, command,
						  resp, NULL);
		if (rv != MLAN_STATUS_SUCCESS)
			return -WM_FAIL;

		wifi_event_completion(WIFI_EVENT_GET_HW_SPEC,
				      WIFI_EVENT_REASON_SUCCESS,
				      NULL);
		break;
	case HostCmd_CMD_802_11_SCAN:
		if (resp->result != 0) {
			wifi_w("Scan temporary failure");
			/*
			 * Abort the split scan. The firmware has returned
			 * scan failure.
			*/
			wlan_abort_split_scan();
			wifi_event_completion(WIFI_EVENT_SCAN_RESULT,
					      WIFI_EVENT_REASON_FAILURE,
					      (void *)-1);
			break;
		}

		rv = wlan_ops_sta_process_cmdresp(pmpriv, command, resp, NULL);
		if (rv != MLAN_STATUS_SUCCESS)
			return -WM_FAIL;

		if (is_split_scan_complete()) {
			wifi_d("Split scan complete");
			wifi_event_completion(WIFI_EVENT_SCAN_RESULT,
				      WIFI_EVENT_REASON_SUCCESS,
				      NULL);
		}
		break;
#ifdef EXT_SCAN_SUPPORT
	case HostCmd_CMD_802_11_SCAN_EXT:
		if (resp->result != 0) {
			wifi_w("Scan temporary failure");
			/*
			 * Abort the split scan. The firmware has returned
			 * scan failure.
			*/
			wlan_abort_split_scan();
			wifi_event_completion(WIFI_EVENT_SCAN_RESULT,
					      WIFI_EVENT_REASON_FAILURE,
					      (void *)-1);
		}
		break;
#endif
	case HostCmd_CMD_802_11_DEAUTHENTICATE:
		wlan_ret_802_11_deauthenticate(pmpriv, resp, NULL);
		wifi_event_completion(WIFI_EVENT_DEAUTHENTICATION,
				      WIFI_EVENT_REASON_SUCCESS, NULL);
		break;
	case HostCmd_CMD_802_11_HS_CFG_ENH:
		wifi_process_hs_cfg_resp((t_u8 *)resp);
		break;
	case HostCmd_CMD_802_11_PS_MODE_ENH: {
		uint16_t ps_action = 0;
		result = wifi_process_ps_enh_response((t_u8 *)resp, &ps_event,
						      &ps_action);
		arg = (void *) ((t_u32)ps_action);
		wifi_event_completion(ps_event, result, arg);
	}
		break;
#if 0
	case HostCmd_CMD_SUPPLICANT_PMK:
		wifi_event_completion(WIFI_EVENT_SUPPLICANT_PMK,
				      WIFI_EVENT_REASON_SUCCESS,
				      resp);
		break;
#endif
	case HostCmd_CMD_802_11_RF_CHANNEL: {
		const HostCmd_DS_802_11_RF_CHANNEL *ch =
				&resp->params.rf_channel;
		if (ch->action == HostCmd_ACT_GEN_GET) {
			if (wm_wifi.cmd_resp_priv) {
				wifi_rf_channel_t *rf_c = (wifi_rf_channel_t *)
					wm_wifi.cmd_resp_priv;
				wm_wifi.cmd_resp_priv = NULL;
				rf_c->current_channel = ch->current_channel;
				rf_c->rf_type = ch->rf_type;
			}
		}
		wm_wifi.cmd_resp_status = WM_SUCCESS;
		/* Is this required */
		rv = wlan_ops_sta_process_cmdresp(pmpriv, command,
				resp, NULL);
	}
		break;
#ifdef CONFIG_11N
	case HostCmd_CMD_11N_CFG:
		rv = wlan_ret_11n_cfg(pmpriv, resp, NULL);
		break;
	case HostCmd_CMD_11N_ADDBA_RSP:
		do_wlan_ret_11n_addba_resp(resp);
		break;
	case HostCmd_CMD_AMSDU_AGGR_CTRL:
		rv = wlan_ret_amsdu_aggr_ctrl(pmpriv, resp, NULL);
		break;
#endif /* CONFIG_11N */
	case HostCmd_CMD_MAC_MULTICAST_ADR:
		break;
	case HostCmd_CMD_802_11_ASSOCIATE: {
		rv = wlan_ops_sta_process_cmdresp(pmpriv, command, resp, NULL);
		if (rv != MLAN_STATUS_SUCCESS)
			return -WM_FAIL;

		IEEEtypes_AssocRsp_t *passoc_rsp =
			(IEEEtypes_AssocRsp_t *) &resp->params;

		if (!passoc_rsp->status_code)
			result = WIFI_EVENT_REASON_SUCCESS;
		else {
			wifi_w("Assoc failed: Status: %d\n\r", passoc_rsp->status_code);
			wifi_show_assoc_fail_reason(passoc_rsp->status_code);

			/* Since we have failed assoc attempt clear this */
			pmpriv->media_connected = MFALSE;
		}

		wifi_event_completion(WIFI_EVENT_ASSOCIATION, result, NULL);
	}
		break;
	case HostCmd_CMD_802_11_MAC_ADDRESS: {
		HostCmd_DS_802_11_MAC_ADDRESS *pmac_addr =
			&resp->params.mac_addr;
		arg = (uint8_t *)&pmac_addr->mac_addr;
		wifi_event_completion(WIFI_EVENT_MAC_ADDR_CONFIG,
				      WIFI_EVENT_REASON_SUCCESS, NULL);
	}
		break;
	case HostCmd_CMD_802_11_KEY_MATERIAL:
		if (resp->result == 0)
			result = WIFI_EVENT_REASON_SUCCESS;
		else
			result = WIFI_EVENT_REASON_FAILURE;
		break;
	case HostCmd_CMD_MAC_REG_ACCESS: {
		HostCmd_DS_MAC_REG_ACCESS *reg;
		reg = (HostCmd_DS_MAC_REG_ACCESS *) &resp->params.mac_reg;
		if (reg->action == HostCmd_ACT_GEN_GET) {
			if (wm_wifi.cmd_resp_priv) {
				uint32_t *mac_reg = (uint32_t *)
					wm_wifi.cmd_resp_priv;
				wm_wifi.cmd_resp_priv = NULL;
				*mac_reg = reg->value;
			}
		}
		wm_wifi.cmd_resp_status = WM_SUCCESS;
	}
		break;
	case HostCmd_CMD_BBP_REG_ACCESS: {
		HostCmd_DS_BBP_REG_ACCESS *reg;
		reg = (HostCmd_DS_BBP_REG_ACCESS *) &resp->params.bbp_reg;
		if (reg->action == HostCmd_ACT_GEN_GET) {
			if (wm_wifi.cmd_resp_priv) {
				uint32_t *bbp_reg = (uint32_t *)
					wm_wifi.cmd_resp_priv;
				wm_wifi.cmd_resp_priv = NULL;
				*bbp_reg = reg->value;
			}
		}
		wm_wifi.cmd_resp_status = WM_SUCCESS;
	}
		break;
	case HostCmd_CMD_RF_REG_ACCESS: {
		HostCmd_DS_RF_REG_ACCESS *reg;
		reg = (HostCmd_DS_RF_REG_ACCESS *) &resp->params.rf_reg;
		if (reg->action == HostCmd_ACT_GEN_GET) {
			if (wm_wifi.cmd_resp_priv) {
				uint32_t *rf_reg = (uint32_t *)
					wm_wifi.cmd_resp_priv;
				wm_wifi.cmd_resp_priv = NULL;
				*rf_reg = reg->value;
			}
		}
		wm_wifi.cmd_resp_status = WM_SUCCESS;
	}
		break;
	case HostCmd_CMD_802_11_EEPROM_ACCESS: {
		HostCmd_DS_802_11_EEPROM_ACCESS *eeprom;
		eeprom = (HostCmd_DS_802_11_EEPROM_ACCESS *) &resp->params.eeprom;
		if (wm_wifi.cmd_resp_priv) {
			uint8_t *buf = (uint8_t *)
				wm_wifi.cmd_resp_priv;
			wm_wifi.cmd_resp_priv = NULL;
			memcpy(buf, &eeprom->value, eeprom->byte_count);
		}
		wm_wifi.cmd_resp_status = WM_SUCCESS;
	}
		break;
	case HostCmd_CMD_MGMT_IE_LIST: {
		HostCmd_DS_MGMT_IE_LIST_CFG *ie_list_cfg;
		ie_list_cfg = (HostCmd_DS_MGMT_IE_LIST_CFG *) &resp->params.mgmt_ie_list;
                if (wm_wifi.cmd_resp_priv) {
		uint8_t *buf = (uint8_t *)
				wm_wifi.cmd_resp_priv;
			wm_wifi.cmd_resp_priv = NULL;
			memcpy(buf, (void *) &ie_list_cfg->ds_mgmt_ie.ie_data_list[0],
				ie_list_cfg->ds_mgmt_ie.len);
		}
		wm_wifi.cmd_resp_status = WM_SUCCESS;
	}
		break;
	case HostCmd_CMD_VERSION_EXT:
		load_ver_ext(resp);
		break;
	case HostCmd_CMD_802_11_RF_ANTENNA:
		load_rf_antenna_mode(resp);
		break;
	case HostCmd_CMD_RSSI_INFO: {
		HostCmd_DS_802_11_RSSI_INFO_RSP *prssi_info_rsp =
			(HostCmd_DS_802_11_RSSI_INFO_RSP *)&resp->params;
		if (wm_wifi.cmd_resp_priv) {
			wifi_rssi_info_t *rssi_info = (wifi_rssi_info_t *)
				wm_wifi.cmd_resp_priv;
			wm_wifi.cmd_resp_priv = NULL;
			compute_rssi_values(prssi_info_rsp, rssi_info);
		}
		wm_wifi.cmd_resp_status = WM_SUCCESS;
	}
		break;
	case HostCmd_CMD_802_11_NET_MONITOR:
		if (wm_wifi.cmd_resp_priv) {
			/*wifi_net_monitor_t *mon = (wifi_net_monitor_t *)
				wm_wifi.cmd_resp_priv;*/
			wm_wifi.cmd_resp_priv = NULL;
		}
		wm_wifi.cmd_resp_status = WM_SUCCESS;
		break;
	case HOST_CMD_APCMD_SYS_CONFIGURE:
		wifi_uap_handle_cmd_resp(resp);
		break;
	case HostCmd_CMD_TXPWR_CFG:
		rv = wlan_ops_sta_process_cmdresp(pmpriv, command,
						  resp, NULL);
		if (rv != MLAN_STATUS_SUCCESS)
			return -WM_FAIL;
		break;
	default:
		/* fixme: Currently handled by the legacy code. Change this
		   handling later. Also check the default return value then*/
		break;
	}

	if (rv != MLAN_STATUS_SUCCESS)
		return -WM_FAIL;

	return WM_SUCCESS;
}

/* fixme: duplicated from legacy. needs to be cleaned up later */
#define IEEEtypes_REASON_UNSPEC                   1
#define IEEEtypes_REASON_PRIOR_AUTH_INVALID       2
#define IEEEtypes_REASON_DEAUTH_LEAVING           3

int wifi_handle_fw_event(struct bus_message *msg)
{
	mlan_private *pmpriv = (mlan_private *) mlan_adap->priv[0];
#ifdef EXT_SCAN_SUPPORT
	mlan_status rv = MLAN_STATUS_SUCCESS;
#endif

	Event_Ext_t *evt = ((Event_Ext_t *) msg->data);
	t_u8 *sta_addr, *event_sta_addr;
#ifdef EXT_SCAN_SUPPORT
	mlan_event_scan_result *pext_scan_result;
#endif

	wevt_d(" [%-8u] EVENT - : 0x%x Len : %-4d Reason: %-4d",
	       os_get_timestamp(),
	       evt->event_id, evt->length, evt->reason_code);
	switch (evt->event_id) {
	case EVENT_LINK_LOST:
		wifi_event_completion(WIFI_EVENT_LINK_LOSS,
				      WIFI_EVENT_REASON_FAILURE,
				      (void *)IEEEtypes_REASON_DEAUTH_LEAVING);
#ifdef CONFIG_11N
		/* fixme: Should this be outside CONFIG_11N ? */
		wlan_handle_disconnect_event(pmpriv);
#endif
		break;
	case EVENT_DEAUTHENTICATED:
		if (!evt->reason_code)
			wifi_event_completion(WIFI_EVENT_LINK_LOSS,
					WIFI_EVENT_REASON_FAILURE,
					(void *)
					IEEEtypes_REASON_DEAUTH_LEAVING);
		else
			wifi_event_completion(WIFI_EVENT_AUTHENTICATION,
					WIFI_EVENT_REASON_FAILURE,
					NULL);
#ifdef CONFIG_11N
		/* fixme: Should this be outside CONFIG_11N ? */
		wlan_handle_disconnect_event(pmpriv);
#endif
		break;
	case EVENT_DISASSOCIATED:
		wifi_event_completion(WIFI_EVENT_DISASSOCIATION,
				WIFI_EVENT_REASON_FAILURE,
				(void *)
				IEEEtypes_REASON_DEAUTH_LEAVING);
#ifdef CONFIG_11N
		/* fixme: Should this be outside CONFIG_11N ? */
		wlan_handle_disconnect_event(pmpriv);
#endif
		break;
	case EVENT_PORT_RELEASE:
		wifi_event_completion(WIFI_EVENT_AUTHENTICATION,
				      WIFI_EVENT_REASON_SUCCESS, NULL);
		break;
	case EVENT_PS_SLEEP:
		wifi_event_completion(WIFI_EVENT_SLEEP,
				      WIFI_EVENT_REASON_SUCCESS, NULL);
		break;
	case EVENT_PS_AWAKE:
		wifi_event_completion(WIFI_EVENT_AWAKE,
				      WIFI_EVENT_REASON_SUCCESS, NULL);
		break;
#ifdef CONFIG_P2P
	case EVENT_WIFIDIRECT_GENERIC_EVENT:
	case EVENT_WIFIDIRECT_SERVICE_DISCOVERY:
		wifi_d("EVENT: WIFIDIRECT event 0x%x\r\n", evt->event_id);
		wifi_wfd_event(true, &evt->event_id);
		break;
#endif
	case EVENT_HS_ACT_REQ:
		wifi_event_completion(WIFI_EVENT_HS_CONFIG,
				      WIFI_EVENT_REASON_SUCCESS, NULL);
		break;
#ifdef CONFIG_11N
	case EVENT_ADDBA: {
		void *saved_event_buff = wifi_11n_save_request(evt);
		if (saved_event_buff)
			wifi_event_completion(WIFI_EVENT_11N_ADDBA,
					      WIFI_EVENT_REASON_SUCCESS,
					      saved_event_buff);

		/* If allocation failed ignore this event quietly ! */
	}
		break;
	case EVENT_DELBA:
		wlan_11n_delete_bastream(pmpriv, (t_u8 *)&evt->reason_code);
		break;
	case EVENT_AMSDU_AGGR_CTRL:
		/*
		 * Currently this event buffer isn't being used. Enable
		 * this when required.
		 */
		/* wifi_11n_save_request(evt); */
		wifi_event_completion(WIFI_EVENT_11N_AGGR_CTRL,
				      WIFI_EVENT_REASON_SUCCESS, NULL);
		break;
#endif /* CONFIG_11N */
#ifdef CONFIG_5GHz_SUPPORT
	case EVENT_CHANNEL_SWITCH_ANN:
		wifi_event_completion(WIFI_EVENT_CHAN_SWITCH,
				      WIFI_EVENT_REASON_SUCCESS, NULL);
		break;
#endif
	case EVENT_MICRO_AP_STA_ASSOC: {
		/*
		 * Alloc memory to store the STA mac id. This will be
		 * passed to event receiver thread. Freeing this is
		 * responsibility of the receiving thread.
		 */
		sta_addr = os_mem_alloc(MLAN_MAC_ADDR_LENGTH);
		if (!sta_addr) {
			wifi_w("No mem. Cannot process MAC address from assoc");
			break;
		}

		event_sta_addr = (t_u8 *)&evt->src_mac_addr;
		memcpy(sta_addr, event_sta_addr, MLAN_MAC_ADDR_LENGTH);
		wifi_event_completion(WIFI_EVENT_UAP_CLIENT_ASSOC,
				      WIFI_EVENT_REASON_SUCCESS, sta_addr);
		}
		break;
	case EVENT_MICRO_AP_STA_DEAUTH:
		if ((evt->reason_code ==
			IEEEtypes_REASON_PRIOR_AUTH_INVALID) ||
		    (evt->reason_code ==
			IEEEtypes_REASON_UNSPEC)) {
		/*
		 * Alloc memory to store the STA mac id. This will be
		 * passed to event receiver thread. Freeing this is
		 * responsibility of the receiving thread.
		 */
		sta_addr = os_mem_alloc(MLAN_MAC_ADDR_LENGTH);
		if (!sta_addr) {
			wifi_w("No mem. Cannot process MAC address from assoc");
			break;
		}
		event_sta_addr = (t_u8 *)&evt->src_mac_addr;
		memcpy(sta_addr, event_sta_addr, MLAN_MAC_ADDR_LENGTH);
		wifi_event_completion(WIFI_EVENT_UAP_CLIENT_DEAUTH,
				      WIFI_EVENT_REASON_SUCCESS, sta_addr);
		}
		break;
	case EVENT_MICRO_AP_BSS_START:
		wifi_d("uAP start event received");
		/*
		 * wmsdk: statement copied from
		 * mlan_uap_cmdevent.c. Necessary for other uAP functions.
		 */
		pmpriv->uap_bss_started = MTRUE;
		break;
#ifdef EXT_SCAN_SUPPORT
	case EVENT_EXT_SCAN_REPORT:
		pext_scan_result = (mlan_event_scan_result *) ((t_u8 *)msg->data + 4);

		rv = wlan_handle_event_ext_scan_report(pmpriv, (t_u8 *) pext_scan_result);

		if (rv != MLAN_STATUS_SUCCESS)
			return -WM_FAIL;

		if (is_split_scan_complete()) {
			wifi_d("Split scan complete");
			wifi_event_completion(WIFI_EVENT_SCAN_RESULT,
				      WIFI_EVENT_REASON_SUCCESS,
				      NULL);
		}
		break;
#endif
	default:
		wifi_d("Event %x not implemented."
		       " Please report the event hex value.\n\r",
		       evt->event_id);
		break;
	}

	return WM_SUCCESS;
}

const uint8_t wpa_oui_none[4] = {0x00, 0x50, 0xf2, 0x00};
const uint8_t wpa_oui01[4]    = {0x00, 0x50, 0xf2, 0x01};
const uint8_t wpa_oui02[4]    = {0x00, 0x50, 0xf2, 0x02};
const uint8_t wpa_oui03[4]    = {0x00, 0x50, 0xf2, 0x03};
const uint8_t wpa_oui04[4]    = {0x00, 0x50, 0xf2, 0x04};
const uint8_t wpa_oui05[4]    = {0x00, 0x50, 0xf2, 0x05};
const uint8_t wpa_oui06[4]    = {0x00, 0x50, 0xf2, 0x06};

const uint8_t wpa2_oui01[4]   = {0x00, 0x0f, 0xac, 0x01};
const uint8_t wpa2_oui02[4]   = {0x00, 0x0f, 0xac, 0x02};
const uint8_t wpa2_oui03[4]   = {0x00, 0x0f, 0xac, 0x03};
const uint8_t wpa2_oui04[4]   = {0x00, 0x0f, 0xac, 0x04};
const uint8_t wpa2_oui05[4]   = {0x00, 0x0f, 0xac, 0x05};
const uint8_t wpa2_oui06[4]   = {0x00, 0x0f, 0xac, 0x06};

static unsigned char process_rsn_ie(uint8_t *rsn_ie, _Cipher_t *mcstCipher,
			      _Cipher_t *ucstCipher)
{
	IEEEtypes_Rsn_t *prsn_ie = (IEEEtypes_Rsn_t *) rsn_ie;

	if (prsn_ie->pairwise_cipher.count == 2) {
		prsn_ie->len -= 4;
		prsn_ie->pairwise_cipher.count = 1;
		memcpy((((uint8_t *) &prsn_ie->pairwise_cipher.list) +
			sizeof(wpa_suite)),
			(uint8_t *) &prsn_ie->auth_key_mgmt, 6);
	}

	if (!memcmp((const void *) &prsn_ie->pairwise_cipher.list,
			wpa2_oui04, 4))
		ucstCipher->ccmp = true;
	else if (!memcmp((const void *) &prsn_ie->pairwise_cipher.list,
			wpa2_oui02, 4))
		ucstCipher->tkip = true;

	if (!memcmp((const void *) &prsn_ie->group_cipher, wpa2_oui04, 4))
		mcstCipher->ccmp = true;
	else if (!memcmp((const void *) &prsn_ie->group_cipher, wpa2_oui02, 4))
		mcstCipher->tkip = true;
	else if (!memcmp((const void *) &prsn_ie->group_cipher, wpa2_oui05, 4))
		mcstCipher->wep104 = true;
	else if (!memcmp((const void *) &prsn_ie->group_cipher, wpa2_oui01, 4))
		mcstCipher->wep40 = true;

	return prsn_ie->len + 2;
}

static unsigned char process_wpa_ie(uint8_t *wpa_ie, _Cipher_t *mcstCipher,
			      _Cipher_t *ucstCipher)
{
	IEEEtypes_Wpa_t *pwpa_ie = (IEEEtypes_Wpa_t *) wpa_ie;

	if (pwpa_ie->pairwise_cipher.count == 2) {
		pwpa_ie->len = 22;
		pwpa_ie->pairwise_cipher.count = 1;
		memcpy((((uint8_t *) &pwpa_ie->pairwise_cipher.list) +
			sizeof(wpa_suite)),
			(uint8_t *) &pwpa_ie->auth_key_mgmt, 6);
	}

	if (!memcmp((const void *) &pwpa_ie->pairwise_cipher.list,
			wpa_oui04, 4))
		ucstCipher->ccmp = true;
	else if (!memcmp((const void *) &pwpa_ie->pairwise_cipher.list,
			wpa_oui02, 4))
		ucstCipher->tkip = true;

	if (!memcmp((const void *) &pwpa_ie->group_cipher, wpa_oui04, 4))
		mcstCipher->ccmp = true;
	else if (!memcmp((const void *) &pwpa_ie->group_cipher, wpa_oui02, 4))
		mcstCipher->tkip = true;
	else if (!memcmp((const void *) &pwpa_ie->group_cipher, wpa_oui05, 4))
		mcstCipher->wep104 = true;
	else if (!memcmp((const void *) &pwpa_ie->group_cipher, wpa_oui01, 4))
		mcstCipher->wep40 = true;

	return pwpa_ie->len + 2;
}

int wrapper_bssdesc_first_set(int bss_index,
			      uint8_t *BssId,
			      bool *is_ibss_bit_set,
			      int *ssid_len, uint8_t *ssid,
			      uint8_t *Channel,
			      uint8_t *RSSI,
			      _SecurityMode_t *WPA_WPA2_WEP,
			      _Cipher_t *mcstCipher,
			      _Cipher_t *ucstCipher)
{
	if (bss_index >= mlan_adap->num_in_scan_table) {
		wifi_w("Unable to find given entry %d in BSS table",
		       bss_index);
		return -WM_FAIL;
	}

	/* TODO
	 * Removed : const BSSDescriptor_t *d = &mlan_adap->pscan_table[bss_index]; */
	BSSDescriptor_t *d = &mlan_adap->pscan_table[bss_index];

	memcpy(BssId, d->mac_address, MLAN_MAC_ADDR_LENGTH);

	if (d->cap_info.ibss)
		*is_ibss_bit_set = true;
	else
		*is_ibss_bit_set = false;

	if (d->ssid.ssid_len <= MLAN_MAX_SSID_LENGTH) {
		*ssid_len = (int)d->ssid.ssid_len;
		memcpy(ssid, d->ssid.ssid, d->ssid.ssid_len);
	}

	*Channel = d->channel;		/*!< Channel associated to the BSSID */

	*RSSI = (uint8_t)d->rssi;
	/* uint8_t RSSI; */		/*!< Received signal strength */

	if (d->pwpa_ie || d->prsn_ie) {
		/* Check if WPA or WPA2 */
		if (d->pwpa_ie)
			WPA_WPA2_WEP->wpa = 1;
		if (d->prsn_ie)
			WPA_WPA2_WEP->wpa2 = 1;
	} else {
		/* Check if WEP */
		if (d->cap_info.privacy)
			WPA_WPA2_WEP->wepStatic = 1;
	}

	if (d->pwpa_ie)
		d->wpa_ie_buff_len = process_wpa_ie(d->wpa_ie_buff,
					mcstCipher, ucstCipher);

	if (d->prsn_ie)
		d->rsn_ie_buff_len = process_rsn_ie(d->rsn_ie_buff,
					mcstCipher, ucstCipher);

	return WM_SUCCESS;
}

int wrapper_bssdesc_second_set(int bss_index,
			       bool *phtcap_ie_present,
			       bool *phtinfo_ie_present,
			       bool *wmm_ie_present,
			       uint8_t *band,
			       bool *wps_IE_exist,
			       uint16_t *wps_session, bool *wpa2_entp_IE_exist)
{
	if (bss_index >= mlan_adap->num_in_scan_table) {
		wifi_w("Unable to find given entry %d in BSS table",
			 bss_index);
		return -WM_FAIL;
	}
	const BSSDescriptor_t *d = &mlan_adap->pscan_table[bss_index];

	if (d->pht_cap)
		*phtcap_ie_present = true;
	else
		*phtcap_ie_present = false;

	if (d->pht_info)
		*phtinfo_ie_present = true;
	else
		*phtinfo_ie_present = false;

	if (d->wmm_ie.vend_hdr.element_id == WMM_IE)
		*wmm_ie_present = true;
	else
		*wmm_ie_present = false;

	*band = d->bss_band;

	if (wps_IE_exist)
		*wps_IE_exist = d->wps_IE_exist;
	if (wps_session)
		*wps_session = d->wps_session;

#ifdef CONFIG_WPA2_ENTP
	if (wpa2_entp_IE_exist)
		*wpa2_entp_IE_exist = d->wpa2_entp_IE_exist;
#endif

	return WM_SUCCESS;
}


int wifi_get_scan_result_count(unsigned *count)
{
	if (!count)
		return -WM_E_INVAL;

	*count = mlan_adap->num_in_scan_table;
	return WM_SUCCESS;
}


int wrapper_wlan_set_regiontable(t_u8 region, t_u8 band)
{
	mlan_private *pmpriv = (mlan_private *) mlan_adap->priv[0];
	int rv = wlan_set_regiontable(pmpriv, region, band);
	if (rv != MLAN_STATUS_SUCCESS)
		return -WM_FAIL;

	return WM_SUCCESS;
}

/*
 * This function is supposed to be called after scan is complete.
 */
void wifi_scan_process_results()
{
	mlan_private *pmpriv = (mlan_private *) mlan_adap->priv[0];
	/*
	 * This function is normally supposed to be called from inside
	 * mlan_scan.c during parsing of scanning results. But we do not
	 * have async. cmd send functionality in the current design. So, we
	 * have moved this outside.
	 */
	wlan_scan_process_results(pmpriv);
}

#ifdef CONFIG_WPA2_ENTP
void check_for_wpa2_entp_ie(bool *wpa2_entp_IE_exist, const void *element_data, unsigned element_len)
{
	uint8_t wpa2_ent_IE[4];
	uint16_t len;

	memcpy(&len, element_data, sizeof(len));

	if (len * 4 >= element_len)
		return;

	memcpy(&wpa2_ent_IE, element_data + len * 4 + 2 * sizeof(len), 4);

	if (!memcmp(wpa2_ent_IE, wpa2_akmp_oui, sizeof(wpa2_akmp_oui))) {

		/* WPA2 Enterprise IE is present in probe response. */
		wifi_d("WPA2 Enterprise AKMP IE :: %x:%x:%x:%x\r\n", wpa2_ent_IE[0], wpa2_ent_IE[1],
		       wpa2_ent_IE[2], wpa2_ent_IE[3]);

		*wpa2_entp_IE_exist = true;
	}
}
#endif

/*
 * The below wifi authenticate code, data structures and macros seems to be
 * completely absent from mlan. Our current code, including the wlc manager
 * currently depends on this command and its response (though it may be a
 * no-op).
 *
 * fixme: remove this later when appropriate.
 */
#define host_CMD_802_11_AUTHENTICATE 0x0011
#define AUTH_OPEN_SYSTEM 0

typedef PACK_START struct host_802_11Authenticate_t
{
	uint8_t MacAddr[MLAN_MAC_ADDR_LENGTH];
	uint8_t AuthType;
	uint16_t TimeOut;
	uint8_t Reserved[2];
} PACK_END host_802_11Authenticate_t;

/*
 * fixme: This function will be present till mlan integration is complete
 */
void wifi_get_mac_address_from_cmdresp(const HostCmd_DS_COMMAND *resp,
				       uint8_t *mac_addr)
{
	memcpy(mac_addr, &resp->params.mac_addr.mac_addr,
	       MLAN_MAC_ADDR_LENGTH);
}

void wifi_get_firmware_ver_ext_from_cmdresp(const HostCmd_DS_COMMAND *resp,
				       uint8_t *fw_ver_ext)
{
	memcpy(fw_ver_ext, &resp->params.verext.version_str,
	       MLAN_MAX_VER_STR_LEN);
}

void wifi_prepare_set_cal_data_cmd(HostCmd_DS_COMMAND *cmd,
				   int seq_number)
{
	cmd->command = HostCmd_CMD_CFG_DATA;
	cmd->size = sizeof(HostCmd_DS_802_11_CFG_DATA) + S_DS_GEN +
				cal_data_len - 1;
	cmd->seq_num = seq_number;
	cmd->result = 0;
	cmd->params.cfg_data.action = HostCmd_ACT_GEN_SET;
	cmd->params.cfg_data.type = 0x02;
	cmd->params.cfg_data.data_len = cal_data_len;
	memcpy(cmd->params.cfg_data.data, cal_data, cal_data_len);
}
void wifi_prepare_get_hw_spec_cmd(HostCmd_DS_COMMAND *cmd,
				   int seq_number)
{
	cmd->command = HostCmd_CMD_GET_HW_SPEC;
	cmd->size = sizeof(HostCmd_DS_GET_HW_SPEC) + S_DS_GEN;
	cmd->seq_num = seq_number;
	cmd->result = 0;
}
/*
 * fixme: This function will be present till mlan integration is complete
 */
void wifi_prepare_get_mac_addr_cmd(HostCmd_DS_COMMAND *cmd,
				   int seq_number)
{
	cmd->command = HostCmd_CMD_802_11_MAC_ADDRESS;
	cmd->size = sizeof(HostCmd_DS_802_11_MAC_ADDRESS) + S_DS_GEN;
	cmd->seq_num = seq_number;
	cmd->result = 0;
	cmd->params.mac_addr.action = HostCmd_ACT_GEN_GET;
}

void wifi_prepare_get_fw_ver_ext_cmd(HostCmd_DS_COMMAND *cmd,
				   int seq_number)
{
	cmd->command = HostCmd_CMD_VERSION_EXT;
	cmd->size = sizeof(HostCmd_DS_VERSION_EXT) + S_DS_GEN;
	cmd->seq_num = seq_number;
	cmd->result = 0;
	cmd->params.verext.version_str_sel = 0;
}

void wifi_prepare_set_mac_addr_cmd(HostCmd_DS_COMMAND *cmd,
				   int seq_number)
{
	cmd->command = HostCmd_CMD_802_11_MAC_ADDRESS;
	cmd->size = sizeof(HostCmd_DS_802_11_MAC_ADDRESS) + S_DS_GEN;
	cmd->seq_num = seq_number;
	cmd->result = 0;
	cmd->params.mac_addr.action = HostCmd_ACT_GEN_SET;
	memcpy(cmd->params.mac_addr.mac_addr, mac_addr, MLAN_MAC_ADDR_LENGTH);
}

void wifi_prepare_enable_amsdu_cmd(HostCmd_DS_COMMAND *cmd,
				   int seq_number)
{
	cmd->command = HostCmd_CMD_AMSDU_AGGR_CTRL;
	cmd->size = sizeof(HostCmd_CMD_AMSDU_AGGR_CTRL) + 8;
	cmd->seq_num = seq_number;
	cmd->result = 0;
	cmd->params.amsdu_aggr_ctrl.action = HostCmd_ACT_GEN_SET;
	cmd->params.amsdu_aggr_ctrl.enable = 0x1;
	cmd->params.amsdu_aggr_ctrl.curr_buf_size = 0x0;
}

void wlan_prepare_mac_control_cmd(HostCmd_DS_COMMAND *cmd,
			  int seq_number)
{
	cmd->command = HostCmd_CMD_MAC_CONTROL;
	cmd->size = sizeof(HostCmd_DS_MAC_CONTROL) + S_DS_GEN;
	cmd->seq_num = seq_number;
	cmd->result = 0;

	cmd->params.mac_ctrl.action =
	    HostCmd_ACT_MAC_RX_ON | HostCmd_ACT_MAC_TX_ON |
	    HostCmd_ACT_MAC_ETHERNETII_ENABLE;

	return;
}

void wifi_set_cal_data(uint8_t *cdata, unsigned int clen)
{
	cal_data = cdata;
	cal_data_len = clen;
	cal_data_valid = true;
}

void wifi_set_mac_addr(uint8_t *mac)
{
	mac_addr = mac;
	mac_addr_valid = true;
}
