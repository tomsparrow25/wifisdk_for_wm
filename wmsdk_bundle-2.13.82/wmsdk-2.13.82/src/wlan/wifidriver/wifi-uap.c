/*
 * This file contains functions which provide uAP related
 * functionality. This file mostly is an interface between mlan UAP code
 * and rest of WMSDK stack.
 */

#include <mlan_wmsdk.h>

/* Additional WMSDK header files */
#include <wmstdio.h>
#include <wmassert.h>
#include <wmerrno.h>
#include <wm_os.h>
#include <cli_utils.h>

#include <wifi.h>
#include "wifi-sdio.h"
#include "wifi-internal.h"

/* fixme: Some of the following options could be added to kconfig. While
   adding the ranges in kconfig use the ones given as macros in
   mlan_uap_cmdevent.c */
#define UAP_BEACON_PERIOD 200
#define UAP_DTIM_PERIOD 10
#define MAX_RATES 12

static unsigned int uap_max_sta_count = 0x08;

int wifi_uap_prepare_and_send_cmd(mlan_private *pmpriv,
			 t_u16 cmd_no,
			 t_u16 cmd_action,
			 t_u32 cmd_oid,
			 t_void *pioctl_buf, t_void *pdata_buf,
				  int bss_type, void *priv)
{
	wifi_get_command_lock();
	HostCmd_DS_COMMAND *cmd =  wifi_get_command_buffer();

	cmd->seq_num = HostCmd_SET_SEQ_NO_BSS_INFO(0 /* seq_num */ ,
						   0 /* bss_num */ ,
						   bss_type);
	cmd->result = 0x0;

	mlan_status rv = wlan_ops_uap_prepare_cmd(pmpriv, cmd_no,
						  cmd_action, cmd_oid,
						  pioctl_buf, pdata_buf, cmd);
	if (rv != MLAN_STATUS_SUCCESS) {
		wifi_w("Failed to prepare cmd for uAP");
		wifi_put_command_lock();
		return rv;
	}

	wifi_wait_for_cmdresp(priv);
	return WM_SUCCESS;
}
#ifdef CONFIG_11D
/*
 * We only save the params to be used later when BSS is (re-)started.
 */
int wifi_uap_set_domain_params(wifi_domain_param_t *dp)
{
	if (!dp)
		return -WM_E_INVAL;

	if (wm_wifi.dp)
		wuap_w("Overwriting previous configuration");

	/*
	 * We have domain information from caller. Allocate tlv, fill it
	 * up and store as pointer in global wifi structure. It will be
	 * used later by the uap start code.
	 */
	int sz = sizeof(MrvlIEtypes_DomainParamSet_t) +
		(sizeof(IEEEtypes_SubbandSet_t) * dp->no_of_sub_band);
	MrvlIEtypes_DomainParamSet_t *n_dp = os_mem_alloc(sz);
	if (!n_dp)
		return -WM_E_NOMEM;

	n_dp->header.type = TLV_TYPE_DOMAIN;
	n_dp->header.len = sizeof(MrvlIEtypes_DomainParamSet_t) -
		4 + (dp->no_of_sub_band *
		     sizeof(IEEEtypes_SubbandSet_t));

	memcpy(n_dp->country_code, dp->country_code, COUNTRY_CODE_LEN);

	wifi_sub_band_set_t *is = dp->sub_band;
	IEEEtypes_SubbandSet_t *s = n_dp->sub_band;
	int i;
	for (i = 0; i < dp->no_of_sub_band; i++) {
		s[i].first_chan = is[i].first_chan;
		s[i].no_of_chan = is[i].no_of_chan;
		s[i].max_tx_pwr = is[i].max_tx_pwr;
	}

	/* Store for use later */
	wuap_d("Saved domain info for later use");
	wm_wifi.dp = n_dp;
	return 0;
}

/*
 * Note: wlan_uap_domain_info() and wlan_uap_callback_domain_info() are the
 * original function which handles this functionality. However, it does it
 * through and IOCTL and its control flow will not work in our case. To
 * elaborate, it first gets the channel number by sending a command to
 * firmware. Then in the cmd resp handler to sends the domain info
 * command. As per the current design of our driver we cannot send command
 * from command resp handler. Hence, we have modified the control flow to
 * suit our design. The wifi_get_uap_channel() function also helps us in
 * this by updating pmpriv->uap_state_chan_cb.band_config and
 * pmpriv->uap_state_chan_cb.channel.
 *
 * fixme: Need to check how ACS works with this.
 */
static int wifi_uap_downld_domain_params(MrvlIEtypes_DomainParamSet_t *dp)
{
	if (!dp)
		return -WM_E_INVAL;

	mlan_private *pmpriv = (mlan_private *) mlan_adap->priv[0];
	wifi_get_uap_channel(NULL);

	t_u8 band = (pmpriv->uap_state_chan_cb.band_config
		     & BAND_CONFIG_5GHZ) ? BAND_A : BAND_B;

	int rv = wlan_11d_handle_uap_domain_info(pmpriv, band,
						 (t_u8 *)dp, NULL);
	if (rv != MLAN_STATUS_SUCCESS)
		return -WM_FAIL;

	return 0;
}
#endif /* CONFIG_11D */

int wifi_cmd_uap_config(char *ssid, t_u8 *mac_addr, t_u8 security,
			char *passphrase, t_u8 channel,
			t_u16 beacon_period, t_u8 dtim_period,
			int bss_type)
{
	int ssid_len = strlen(ssid);
	ASSERT(ssid_len <= MLAN_MAX_SSID_LENGTH);

	ASSERT(channel <= MLAN_MAX_CHANNEL);

	if (!(security == 0 || security == 4))
		return -WM_E_INVAL;

#ifndef CONFIG_WiFi_8801
	int passphrase_len = strlen(passphrase);
	ASSERT(passphrase_len <= 64);
#endif
	mlan_private *pmpriv = (mlan_private *) mlan_adap->priv[0];

	/* fixme: check if this needs to go on heap */
	mlan_ds_bss bss;
	memset(&bss, 0x00, sizeof(mlan_ds_bss));

	bss.sub_command = MLAN_OID_UAP_BSS_CONFIG;

	bss.param.bss_config.ssid.ssid_len = ssid_len;
	memcpy(bss.param.bss_config.ssid.ssid, ssid, ssid_len);

	if (bss_type == MLAN_BSS_TYPE_UAP) {	/* Not required for WFD */
#ifdef CONFIG_WiFi_8801
		memcpy(bss.param.bss_config.mac_addr, mac_addr,
			MLAN_MAC_ADDR_LENGTH);
		bss.param.bss_config.bcast_ssid_ctl = 0x01;
#endif
		bss.param.bss_config.beacon_period = beacon_period;
		bss.param.bss_config.dtim_period = dtim_period;
#ifdef CONFIG_WiFi_8801
		bss.param.bss_config.tx_data_rate = 0x0;
		bss.param.bss_config.mcbc_data_rate = 0x0;
		bss.param.bss_config.tx_power_level = 0x12;
		bss.param.bss_config.tx_antenna = 0x0;
		bss.param.bss_config.rx_antenna = 0x0;
		bss.param.bss_config.pkt_forward_ctl = 0x1;
		bss.param.bss_config.max_sta_count = uap_max_sta_count;
		bss.param.bss_config.sta_ageout_timer = 1800;
		bss.param.bss_config.ps_sta_ageout_timer = 400;
		bss.param.bss_config.rts_threshold = 2347;
		bss.param.bss_config.frag_threshold = 2346;
		bss.param.bss_config.retry_limit = 0x07;
		bss.param.bss_config.pairwise_update_timeout = 100;
		bss.param.bss_config.pwk_retries = 3;
		bss.param.bss_config.groupwise_update_timeout = 100;
		bss.param.bss_config.gwk_retries = 3;
		bss.param.bss_config.filter.filter_mode = 0x00;
		bss.param.bss_config.filter.mac_count = 0x00;
		bss.param.bss_config.auth_mode = 0x0;
		bss.param.bss_config.mgmt_ie_passthru_mask = 0x00;
		bss.param.bss_config.enable_2040coex = 0x00;
#endif
	}

	if (channel) {
		bss.param.bss_config.band_cfg = BAND_CONFIG_MANUAL;
		bss.param.bss_config.channel = channel;
	} else {
		/* Auto channel selection */
		bss.param.bss_config.band_cfg = BAND_CONFIG_ACS_MODE;
		bss.param.bss_config.channel = 0;
	}

	if (security == 0)
		bss.param.bss_config.protocol = PROTOCOL_NO_SECURITY;
	if (security == 4)
		bss.param.bss_config.protocol = PROTOCOL_WPA2;

	if (security == 4) {
		bss.param.bss_config.wpa_cfg.pairwise_cipher_wpa2 =
							CIPHER_AES_CCMP;
		bss.param.bss_config.wpa_cfg.group_cipher = CIPHER_AES_CCMP;
#ifdef CONFIG_WiFi_8801
		bss.param.bss_config.wpa_cfg.rsn_protection = 0;
#endif

		bss.param.bss_config.key_mgmt = KEY_MGMT_PSK;
		/***************************************
		 * Operation UINT16 Bits[15:2]: Reserved      *
		 *                                  Bit[1]: Authenticator     *
		 *                                  Bit[0]: KeyExchange      *
		 ****************************************/
		bss.param.bss_config.key_mgmt_operation = 0x00;


#ifndef CONFIG_WiFi_8801
		bss.param.bss_config.wpa_cfg.length = passphrase_len;
		memcpy(bss.param.bss_config.wpa_cfg.passphrase, passphrase,
				passphrase_len);
#endif
	}

	/*
	 * Note that we are leaving htcap info set to zero by default. This
	 *  will ensure that 11N is disabled.
	 */

	mlan_ioctl_req ioctl_buf;
	memset(&ioctl_buf, 0x00, sizeof(mlan_ioctl_req));

	ioctl_buf.req_id = MLAN_IOCTL_BSS;
	/** Pointer to buffer */
	ioctl_buf.pbuf = (t_u8 *) &bss;

	return wifi_uap_prepare_and_send_cmd(pmpriv,
				    HOST_CMD_APCMD_SYS_CONFIGURE,
				    HostCmd_ACT_GEN_SET, 0, &ioctl_buf, NULL,
					     bss_type, NULL);
}

#ifdef CONFIG_11D
static int wifi_uap_enable_11d()
{
	state_11d_t enable = ENABLE_11D;

	mlan_private *pmpriv = (mlan_private *) mlan_adap->priv[0];
	return wifi_uap_prepare_and_send_cmd(pmpriv,
				      HostCmd_CMD_802_11_SNMP_MIB,
				      HostCmd_ACT_GEN_SET, Dot11D_i,
				      NULL, &enable,
				      MLAN_BSS_TYPE_UAP, NULL);
}
#endif /* CONFIG_11D */

int wifi_uap_start(int type, char *ssid, uint8_t *mac_addr,
			int security, char *passphrase,
			int channel)
{
	wuap_d("Configuring");
	/* Configure SSID */
	int rv = wifi_cmd_uap_config(ssid, mac_addr, security, passphrase,
				     channel, UAP_BEACON_PERIOD,
				     UAP_DTIM_PERIOD,
				     type);
	if (rv != WM_SUCCESS) {
		wuap_e("config failed. Cannot start");
		return rv;
	}

	mlan_private *pmpriv = (mlan_private *) mlan_adap->priv[0];



#ifdef CONFIG_11D
	if (wm_wifi.dp && type == MLAN_BSS_TYPE_UAP) {
		wuap_d("Downloading domain params");
		wifi_uap_downld_domain_params(wm_wifi.dp);
		wuap_d("Enabling 11d");
		wifi_uap_enable_11d();
	}
#endif /* CONFIG_11D */

	wuap_d("Starting BSS");
	/* Start BSS */
	return wifi_uap_prepare_and_send_cmd(pmpriv,
				    HOST_CMD_APCMD_BSS_START,
				    HostCmd_ACT_GEN_SET, 0, NULL, NULL,
					     type, NULL);
}

int wifi_uap_bss_sta_list(sta_list_t **list)
{
	if (!list)
		return -WM_E_INVAL;

	mlan_private *pmpriv = (mlan_private *) mlan_adap->priv[0];

	/* Start BSS */
	return wifi_uap_prepare_and_send_cmd(pmpriv,
				    HOST_CMD_APCMD_STA_LIST,
				    HostCmd_ACT_GEN_SET, 0, NULL, NULL,
					     MLAN_BSS_TYPE_UAP, list);

	/* *list must have been filled now if everything went well */
}

int wifi_uap_stop(int type)
{
	mlan_private *pmpriv = (mlan_private *) mlan_adap->priv[0];

	/* Start BSS */
	return wifi_uap_prepare_and_send_cmd(pmpriv,
				    HOST_CMD_APCMD_BSS_STOP,
				    HostCmd_ACT_GEN_SET, 0, NULL, NULL,
					     type, NULL);
}

#ifdef CONFIG_WIFI_UAP_WORKAROUND_STICKY_TIM
/*
 * The following configuration was added because of a particular
 * bug on 8787 and 8782 firmware. The firmware bugzilla ID for the
 * bug is 39609. The bug causes the firmware to send packets to the
 * STA even after giving ACK for IEEE PS. The prov. mode scanning
 * coupled with this bug causes large scale packet losses and TCP
 * backoffs finally resulting in long load times for HTTP served
 * pages for prov. UI.
 */

/** TLV header length */
#define TLVHEADER_LEN       4

#define MRVL_STICKY_TIM_CONFIG_TLV_ID       (PROPRIETARY_TLV_BASE_ID + 0x96)
/** TLV : Sticky TIM MAC address */
#define MRVL_STICKY_TIM_STA_MAC_ADDR_TLV_ID (PROPRIETARY_TLV_BASE_ID + 0x97)

/** TLV buffer : sticky tim config */
typedef PACK_START struct _tlvbuf_sticky_tim_config {
	/** Header */
	t_u16 tag;
	/** Length */
	t_u16 length;
	/** Enable */
	t_u16 enable;
	/** Duration */
	t_u16 duration;
	/** Sticky Bitmask */
	t_u16 sticky_bitmask;
} PACK_END tlvbuf_sticky_tim_config;

/** TLV buffer : sticky tim sta mac address */
typedef PACK_START struct _tlvbuf_sticky_tim_sta_mac_addr {
	/** Header */
	t_u16 tag;
	/** Length */
	t_u16 length;
	/** Control */
	t_u16 control;
	/** Station MAC address */
	t_u8 sta_mac_address[MLAN_MAC_ADDR_LENGTH];
} PACK_END tlvbuf_sticky_tim_sta_mac_addr;

/*
 * This configures sticky TIM configuration for given MAC.
 *
 * Note that this is not present in mlan. So we have to add it here.
 */
void wifi_uap_enable_sticky_bit(const uint8_t *mac_addr)
{
	static bool global_sticky_enabled;

	uint32_t size = S_DS_GEN + sizeof(HostCmd_DS_SYS_CONFIG) - 1;

	wifi_get_command_lock();
	HostCmd_DS_COMMAND *cmd = wifi_get_command_buffer();

	cmd->command = wlan_cpu_to_le16(HOST_CMD_APCMD_SYS_CONFIGURE);
	HostCmd_DS_SYS_CONFIG *sys_config_cmd =
	    (HostCmd_DS_SYS_CONFIG *) ((uint32_t) cmd + S_DS_GEN);
	sys_config_cmd->action = HostCmd_ACT_GEN_SET;
	uint8_t *tlv = sys_config_cmd->tlv_buffer;

	if (!global_sticky_enabled) {
		/*
		 * This enables the global sticky TIM bit enable. This
		 * needs to be done only once.
		 */
		tlvbuf_sticky_tim_config *tlv_sticky_tim_cfg =
		    (tlvbuf_sticky_tim_config *) tlv;

		tlv_sticky_tim_cfg->tag = MRVL_STICKY_TIM_CONFIG_TLV_ID;
		tlv_sticky_tim_cfg->length = sizeof(tlvbuf_sticky_tim_config)
		    - TLVHEADER_LEN;
		tlv_sticky_tim_cfg->enable = 1;
		/* Set it permanently */
		tlv_sticky_tim_cfg->duration = 0;
		/* MoreData bit and TIM bit is made sticky */
		tlv_sticky_tim_cfg->sticky_bitmask = (t_u16) 0x3;

		size +=
		    sizeof(MrvlIEtypesHeader_t) + tlv_sticky_tim_cfg->length;
		tlv += sizeof(MrvlIEtypesHeader_t) + tlv_sticky_tim_cfg->length;

		global_sticky_enabled = true;
	}

	tlvbuf_sticky_tim_sta_mac_addr *tim_cfg =
	    (tlvbuf_sticky_tim_sta_mac_addr *) tlv;

	tim_cfg->tag = MRVL_STICKY_TIM_STA_MAC_ADDR_TLV_ID;
	tim_cfg->length = sizeof(tlvbuf_sticky_tim_sta_mac_addr)
	    - TLVHEADER_LEN;
	memcpy(tim_cfg->sta_mac_address, mac_addr, MLAN_MAC_ADDR_LENGTH);
	tim_cfg->control = 1;

	size += sizeof(MrvlIEtypesHeader_t) + tim_cfg->length;

	cmd->size = size;
	cmd->seq_num = (0x01) << 12;
	cmd->result = 0x00;

	wifi_wait_for_cmdresp(NULL);
}
#endif /* CONFIG_WIFI_UAP_WORKAROUND_STICKY_TIM */

/*
 * Note: This function handles only one (first) TLV from the response.
 */
void wifi_uap_handle_cmd_resp(HostCmd_DS_COMMAND *resp)
{
	mlan_private *pmpriv = (mlan_private *) mlan_adap->priv[0];
	HostCmd_DS_SYS_CONFIG *sys_config = (HostCmd_DS_SYS_CONFIG *)
					&resp->params.sys_config;
	uint8_t *tlv = sys_config->tlv_buffer;
	MrvlIEtypesHeader_t *header = (MrvlIEtypesHeader_t *) tlv;
	if (resp->result == 0) {
		switch (header->type) {
		case TLV_TYPE_UAP_TX_POWER: {
			MrvlIEtypes_tx_power_t *tlv_tx_power =
				(MrvlIEtypes_tx_power_t *) tlv;
			if (sys_config->action == HostCmd_ACT_GEN_GET) {
				if (wm_wifi.cmd_resp_priv) {
					uint8_t *tx_power = (uint8_t *)
						wm_wifi.cmd_resp_priv;
					wm_wifi.cmd_resp_priv = NULL;
					*tx_power = tlv_tx_power->tx_power;
				}
			}
			wm_wifi.cmd_resp_status = WM_SUCCESS;
		}
		break;
		case TLV_TYPE_RATES: {
			MrvlIEtypes_RatesParamSet_t *tlv_rates =
				(MrvlIEtypes_RatesParamSet_t *) tlv;

			if (sys_config->action == HostCmd_ACT_GEN_GET) {
				if (wm_wifi.cmd_resp_priv) {
					char *rates = (char *)
						wm_wifi.cmd_resp_priv;
					wm_wifi.cmd_resp_priv = NULL;
					memset(rates, 0, MAX_RATES);
					memcpy(rates, tlv_rates->rates,
						MIN(MAX_RATES,
						tlv_rates->header.len));
				}
			}
			wm_wifi.cmd_resp_status = WM_SUCCESS;
		}
		break;
		case TLV_TYPE_UAP_CHAN_BAND_CONFIG: {
			MrvlIEtypes_channel_band_t *tlv_cb =
				(MrvlIEtypes_channel_band_t *) tlv;
			pmpriv->uap_state_chan_cb.band_config =
				tlv_cb->band_config;
			pmpriv->uap_state_chan_cb.channel = tlv_cb->channel;
			if (!(sys_config->action == HostCmd_ACT_GEN_GET))
				break;
			if (wm_wifi.cmd_resp_priv) {
				int *channel = (int *)wm_wifi.cmd_resp_priv;
				wm_wifi.cmd_resp_priv = NULL;
				*channel = tlv_cb->channel;
			}
			wm_wifi.cmd_resp_status = WM_SUCCESS;
		}
		break;
		case TLV_TYPE_UAP_MAX_STA_CNT: {
			MrvlIEtypes_max_sta_count_t *tlv_sta_cnt =
				(MrvlIEtypes_max_sta_count_t *) tlv;
			pmpriv->uap_state_chan_cb.max_sta_count =
					tlv_sta_cnt->max_sta_count;
			if (!(sys_config->action == HostCmd_ACT_GEN_GET))
				break;
			if (wm_wifi.cmd_resp_priv) {
				int *sta_count = (int *)wm_wifi.cmd_resp_priv;
				wm_wifi.cmd_resp_priv = NULL;
					*sta_count = tlv_sta_cnt->max_sta_count;
			}
			wm_wifi.cmd_resp_status = WM_SUCCESS;
		}
		break;
		default:
			wm_wifi.cmd_resp_status = -WM_FAIL;
		}
	} else {
		wm_wifi.cmd_resp_status = -WM_FAIL;
	}
}

int wifi_uap_rates_getset(uint8_t action, char *rates, uint8_t num_rates)
{
	int i = 0;
	uint32_t size = S_DS_GEN + sizeof(HostCmd_DS_SYS_CONFIG) - 1;

	wifi_get_command_lock();
	HostCmd_DS_COMMAND *cmd = wifi_get_command_buffer();

	cmd->command = wlan_cpu_to_le16(HOST_CMD_APCMD_SYS_CONFIGURE);
	HostCmd_DS_SYS_CONFIG *sys_config_cmd =
	    (HostCmd_DS_SYS_CONFIG *) ((uint32_t) cmd + S_DS_GEN);
	uint8_t *tlv = sys_config_cmd->tlv_buffer;

	MrvlIEtypes_RatesParamSet_t *tlv_rates =
				(MrvlIEtypes_RatesParamSet_t *) tlv;

	if (action == HostCmd_ACT_GEN_GET)
		sys_config_cmd->action = HostCmd_ACT_GEN_GET;
	else {
		sys_config_cmd->action = HostCmd_ACT_GEN_SET;
		for (i = 0; i < num_rates; i++) {
			tlv_rates->rates[i] = rates[i];
		}
	}

	tlv_rates->header.type = wlan_cpu_to_le16(TLV_TYPE_RATES);
	tlv_rates->header.len = wlan_cpu_to_le16(i);

	size += sizeof(MrvlIEtypesHeader_t) + i;
	tlv += sizeof(MrvlIEtypesHeader_t) + i;

	cmd->size = size;
	cmd->seq_num = (0x01) << 12;
	cmd->result = 0x00;

	wifi_wait_for_cmdresp(action == HostCmd_ACT_GEN_GET ?
						rates : NULL);

	return wm_wifi.cmd_resp_status;
}

int wifi_uap_tx_power_getset(uint8_t action, uint8_t *tx_power_dbm)
{
	uint32_t size = S_DS_GEN + sizeof(HostCmd_DS_SYS_CONFIG) - 1;

	wifi_get_command_lock();
	HostCmd_DS_COMMAND *cmd = wifi_get_command_buffer();

	cmd->command = wlan_cpu_to_le16(HOST_CMD_APCMD_SYS_CONFIGURE);
	HostCmd_DS_SYS_CONFIG *sys_config_cmd =
	    (HostCmd_DS_SYS_CONFIG *) ((uint32_t) cmd + S_DS_GEN);
	uint8_t *tlv = sys_config_cmd->tlv_buffer;

	 MrvlIEtypes_tx_power_t *tlv_tx_power = (MrvlIEtypes_tx_power_t *) tlv;

	if (action == HostCmd_ACT_GEN_GET)
		sys_config_cmd->action = HostCmd_ACT_GEN_GET;
	else {
		sys_config_cmd->action = HostCmd_ACT_GEN_SET;
		tlv_tx_power->tx_power = *tx_power_dbm;
	}
	tlv_tx_power->header.type = wlan_cpu_to_le16(TLV_TYPE_UAP_TX_POWER);
	tlv_tx_power->header.len = wlan_cpu_to_le16(sizeof(t_u8));

	size += sizeof(MrvlIEtypes_tx_power_t);
	tlv += sizeof(MrvlIEtypes_tx_power_t);

	cmd->size = size;
	cmd->seq_num = (0x01) << 12;
	cmd->result = 0x00;

	wifi_wait_for_cmdresp(action == HostCmd_ACT_GEN_GET ?
						tx_power_dbm : NULL);

	return wm_wifi.cmd_resp_status;
}

/* Content reproduced from wlan_uap_get_channel() */
static int wifi_send_uap_get_channel_cmd(int *channel)
{
	mlan_private *pmpriv = (mlan_private *) mlan_adap->priv[0];
	t_u16 tlv_type = TLV_TYPE_UAP_CHAN_BAND_CONFIG;

	int rv = wifi_uap_prepare_and_send_cmd(pmpriv,
					  HOST_CMD_APCMD_SYS_CONFIGURE,
					  HostCmd_ACT_GEN_GET, 0,
					  MNULL, &tlv_type,
					  MLAN_BSS_TYPE_UAP, channel);
	if (rv != WM_SUCCESS)
		return rv;

	if (wm_wifi.cmd_resp_status) {
		wifi_w("Unable to uap channel");
		return wm_wifi.cmd_resp_status;
	}

	return rv;
}

/* Content reproduced from wlan_uap_get_channel() */
static int wifi_send_uap_max_sta_num_cmd(uint8_t action,
				unsigned int *max_sta_num)
{
	mlan_private *pmpriv = (mlan_private *) mlan_adap->priv[0];
	MrvlIEtypes_max_sta_count_t max_sta_count_tlv;

	memset(&max_sta_count_tlv, 0, sizeof(MrvlIEtypes_max_sta_count_t));

	max_sta_count_tlv.header.type = TLV_TYPE_UAP_MAX_STA_CNT;

	if (action == HostCmd_ACT_GEN_SET) {
		max_sta_count_tlv.header.len =
				sizeof(MrvlIEtypes_max_sta_count_t);
		max_sta_count_tlv.max_sta_count = *max_sta_num;
	}

	int rv = wifi_uap_prepare_and_send_cmd(pmpriv,
					  HOST_CMD_APCMD_SYS_CONFIGURE,
					  action, 0,
					  MNULL, &max_sta_count_tlv,
					  MLAN_BSS_TYPE_UAP, max_sta_num);
	if (rv != WM_SUCCESS)
		return rv;

	if (wm_wifi.cmd_resp_status) {
		wifi_w("Unable to uap max_sta_num");
		return wm_wifi.cmd_resp_status;
	}

	uap_max_sta_count = *max_sta_num;

	return rv;
}

int wifi_get_uap_max_clients(unsigned int *max_sta_num)
{
	return wifi_send_uap_max_sta_num_cmd(HostCmd_ACT_GEN_GET, max_sta_num);
}

int wifi_set_uap_max_clients(unsigned int *max_sta_num)
{
	return wifi_send_uap_max_sta_num_cmd(HostCmd_ACT_GEN_SET, max_sta_num);
}

/*
 * Note: channel can be NULL. The side effects of this function also are of
 * interest to us as pmpriv->uap_state_chan_cb.band_config and
 * pmpriv->uap_state_chan_cb.channel are updated.
 */
int wifi_get_uap_channel(int *channel)
{
	return wifi_send_uap_get_channel_cmd(channel);
}

