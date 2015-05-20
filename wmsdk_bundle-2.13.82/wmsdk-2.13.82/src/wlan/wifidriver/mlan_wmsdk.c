/*
 * This file contains functions which provide more functionality over mlan
 */

#include <mlan_wmsdk.h>

/* Additional WMSDK header files */
#include <wmstdio.h>
#include <wmassert.h>
#include <wmerrno.h>
#include <wm_os.h>

#include <wifi.h>

#include "wifi-sdio.h"
#include "wifi-internal.h"

/*
 * Bit 0 : Assoc Req
 * Bit 1 : Assoc Resp
 * Bit 2 : ReAssoc Req
 * Bit 3 : ReAssoc Resp
 * Bit 4 : Probe Req
 * Bit 5 : Probe Resp
 * Bit 8 : Beacon
 */
/** Mask for Assoc request frame */
#define MGMT_MASK_ASSOC_REQ  0x01
/** Mask for ReAssoc request frame */
#define MGMT_MASK_REASSOC_REQ  0x04
/** Mask for Assoc response frame */
#define MGMT_MASK_ASSOC_RESP  0x02
/** Mask for ReAssoc response frame */
#define MGMT_MASK_REASSOC_RESP  0x08
/** Mask for probe request frame */
#define MGMT_MASK_PROBE_REQ  0x10
/** Mask for probe response frame */
#define MGMT_MASK_PROBE_RESP 0x20
/** Mask for beacon frame */
#define MGMT_MASK_BEACON 0x100
/** Mask to clear previous settings */
#define MGMT_MASK_CLEAR 0x000

static const char driver_version_format[] = "SD878x-%s-%s-WM";
static const char driver_version[] = "702.1.0";

static unsigned int mgmt_ie_index_bitmap = 0x00;

extern mlan_adapter *mlan_adap;

/* This were static functions in mlan file */
mlan_status wlan_cmd_802_11_deauthenticate(IN pmlan_private pmpriv,
                               IN HostCmd_DS_COMMAND * cmd,
                               IN t_void * pdata_buf);
mlan_status wlan_cmd_reg_access(IN HostCmd_DS_COMMAND * cmd,
				IN t_u16 cmd_action, IN t_void * pdata_buf);
mlan_status wlan_misc_ioctl_region(IN pmlan_adapter pmadapter,
				   IN pmlan_ioctl_req pioctl_req);

extern int wrapper_wlan_cmd_mgmt_ie(unsigned int bss_type, void *buffer,
				unsigned int len, unsigned int action);

/* fixme: Remove the necessity of below global if possible */
/* This is of type wlan_mode */
int current_mode;


int wifi_deauthenticate(uint8_t *bssid)
{
	HostCmd_DS_COMMAND *cmd = wifi_get_command_buffer();

	wifi_get_command_lock();

	 /* fixme: check if this static selection is ok */
#ifdef CONFIG_P2P
	cmd->seq_num = (0x01) << 13;
#else
	cmd->seq_num = 0x0;
#endif
	cmd->result = 0x0;
	
	wlan_cmd_802_11_deauthenticate((mlan_private *) mlan_adap->priv[0],
					cmd, bssid);
	wifi_wait_for_cmdresp(NULL);
	return WM_SUCCESS;
}

int wifi_get_eeprom_data(uint32_t offset, uint32_t byte_count, uint8_t *buf)
{
	mlan_ds_read_eeprom eeprom_rd;
	eeprom_rd.offset = offset;
	eeprom_rd.byte_count = byte_count;

	wifi_get_command_lock();
	HostCmd_DS_COMMAND *cmd = wifi_get_command_buffer();

	cmd->command = HostCmd_CMD_802_11_EEPROM_ACCESS;
	cmd->seq_num = 0x0;
	cmd->result = 0x0;

	wlan_cmd_reg_access(cmd, HostCmd_ACT_GEN_GET, &eeprom_rd);
	wifi_wait_for_cmdresp(buf);
	return wm_wifi.cmd_resp_status;
}

int wifi_reg_access(wifi_reg_t reg_type, uint16_t action,
		    uint32_t offset, uint32_t *value)
{
	mlan_ds_reg_rw reg_rw;
	reg_rw.offset = offset;
	reg_rw.value = *value;
	uint16_t hostcmd;
	switch (reg_type) {
	case REG_MAC:
		hostcmd = HostCmd_CMD_MAC_REG_ACCESS;
		break;
	case REG_BBP:
		hostcmd = HostCmd_CMD_BBP_REG_ACCESS;
		break;
	case REG_RF:
		hostcmd = HostCmd_CMD_RF_REG_ACCESS;
		break;
	default:
		wifi_e("Incorrect register type");
		return -WM_FAIL;
		break;
	}
	
	wifi_get_command_lock();
	HostCmd_DS_COMMAND *cmd = wifi_get_command_buffer();

	cmd->command = hostcmd;
	cmd->seq_num = 0x0;
	cmd->result = 0x0;
	
	wlan_cmd_reg_access(cmd, action, &reg_rw);
	wifi_wait_for_cmdresp(action == HostCmd_ACT_GEN_GET ? value : NULL);
	return wm_wifi.cmd_resp_status;
}

int wifi_send_rssi_info_cmd(wifi_rssi_info_t *rssi_info)
{
	wifi_get_command_lock();
	HostCmd_DS_COMMAND *cmd = wifi_get_command_buffer();
	
	cmd->seq_num = 0x0;
	cmd->result = 0x0;

	mlan_status rv = wlan_ops_sta_prepare_cmd(
						  (mlan_private *) mlan_adap->priv[0],
						  HostCmd_CMD_RSSI_INFO,
						  HostCmd_ACT_GEN_GET,
						  0, NULL, NULL,  cmd);
	if (rv != MLAN_STATUS_SUCCESS)
		return -WM_FAIL;

	wifi_wait_for_cmdresp(rssi_info);
	return wm_wifi.cmd_resp_status;
}

int wifi_send_netmon_cmd(int action, wifi_net_monitor_t *net_mon)
{
	wifi_get_command_lock();
	HostCmd_DS_COMMAND *cmd = wifi_get_command_buffer();

	cmd->seq_num = 0x0;
	cmd->result = 0x0;

	mlan_status rv = wlan_ops_sta_prepare_cmd(
						  (mlan_private *) mlan_adap->priv[0],
						  HostCmd_CMD_802_11_NET_MONITOR,
						  action,
						  0, NULL, net_mon,  cmd);
	if (rv != MLAN_STATUS_SUCCESS)
		return -WM_FAIL;

	wifi_wait_for_cmdresp(net_mon);
	return wm_wifi.cmd_resp_status;
}

int wifi_sniffer_start(const t_u16 filter_flags, const t_u8 radio_type,
			const t_u8 channel)
{
	wifi_net_monitor_t net_mon;

	memset(&net_mon, 0, sizeof(wifi_net_monitor_t));

	net_mon.monitor_activity = 0x01;
	net_mon.filter_flags = filter_flags;
	net_mon.radio_type = radio_type;
	net_mon.chan_number = channel;

	return wifi_send_netmon_cmd(HostCmd_ACT_GEN_SET, &net_mon);
}

int wifi_sniffer_status()
{
	wifi_net_monitor_t net_mon;

	memset(&net_mon, 0, sizeof(wifi_net_monitor_t));

	return wifi_send_netmon_cmd(HostCmd_ACT_GEN_GET, &net_mon);
}

int wifi_sniffer_stop()
{
	wifi_net_monitor_t net_mon;

	memset(&net_mon, 0, sizeof(wifi_net_monitor_t));

	net_mon.monitor_activity = 0x00;

	return wifi_send_netmon_cmd(HostCmd_ACT_GEN_SET, &net_mon);
}

int wifi_send_rf_channel_cmd(wifi_rf_channel_t *rf_channel)
{
	wifi_get_command_lock();
	HostCmd_DS_COMMAND *cmd = wifi_get_command_buffer();

	cmd->seq_num = 0x0;
	cmd->result = 0x0;

	/*
	  SET operation is not supported according to spec. So we are
	  sending NULL as one param below.
	*/
	mlan_status rv = wlan_ops_sta_prepare_cmd(
						  (mlan_private *) mlan_adap->priv[0],
						  HostCmd_CMD_802_11_RF_CHANNEL,
						  HostCmd_ACT_GEN_GET,
						  0, NULL, NULL, cmd);
	if (rv != MLAN_STATUS_SUCCESS)
		return -WM_FAIL;

	wifi_wait_for_cmdresp(rf_channel);
	return wm_wifi.cmd_resp_status;
}


/* power_level is not used when cmd_action is GET */
int wifi_get_set_rf_tx_power(t_u16 cmd_action, wifi_tx_power_t *tx_power)
{
	wifi_get_command_lock();
	HostCmd_DS_COMMAND *cmd = wifi_get_command_buffer();

	cmd->seq_num = 0x0;
	cmd->result = 0x0;
	mlan_status rv = wlan_ops_sta_prepare_cmd(
						  (mlan_private *) mlan_adap->priv[0],
						  HostCmd_CMD_802_11_RF_TX_POWER,
						  cmd_action,
						  0, NULL,
						&tx_power->current_level, cmd);
	if (rv != MLAN_STATUS_SUCCESS)
		return -WM_FAIL;

	wifi_wait_for_cmdresp(cmd_action == HostCmd_ACT_GEN_GET ?
					tx_power : NULL);
	return wm_wifi.cmd_resp_status;
}


int wifi_send_get_datarate(wifi_tx_rate_t *tx_rate)
{
	wifi_get_command_lock();
	HostCmd_DS_COMMAND *cmd = wifi_get_command_buffer();

	cmd->seq_num = 0x0;
	cmd->result = 0x0;

	mlan_status rv = wlan_ops_sta_prepare_cmd(
						  (mlan_private *) mlan_adap->priv[0],
						  HostCmd_CMD_802_11_TX_RATE_QUERY,
						  0, 0, NULL, NULL, cmd);
	if (rv != MLAN_STATUS_SUCCESS)
		return -WM_FAIL;

	wifi_wait_for_cmdresp(tx_rate);
	return WM_SUCCESS;
}

/* 
 * fixme: Currently, we support only single SSID based scan. We can extend
 * this to a list of multiple SSIDs. The mlan API supports this.
 */
int wifi_send_scan_cmd(t_u8 bss_mode, const t_u8 *specific_bssid,
		       const char *ssid, t_u8 chan_number, t_u32 scan_time,
		       bool keep_previous_scan)
{
	int ssid_len = 0;
	if (ssid) {
		ssid_len = strlen(ssid);
		if (ssid_len > MLAN_MAX_SSID_LENGTH)
			return -WM_E_INVAL;
	}

	wlan_user_scan_cfg *user_scan_cfg =
		os_mem_alloc(sizeof(wlan_user_scan_cfg));
	if (!user_scan_cfg)
		return -WM_E_NOMEM;

	memset(user_scan_cfg, 0x00, sizeof(wlan_user_scan_cfg));
	user_scan_cfg->bss_mode = bss_mode;
	user_scan_cfg->keep_previous_scan = keep_previous_scan;

	if (specific_bssid) {
		memcpy(user_scan_cfg->specific_bssid, specific_bssid,
		       MLAN_MAC_ADDR_LENGTH);
	}

	if (ssid) {
		memcpy(user_scan_cfg->ssid_list[0].ssid, ssid, ssid_len);
		/* For Wildcard SSID do not set max len */
		/* user_scan_cfg->ssid_list[0].max_len = ssid_len; */
	}

	if (chan_number > 0) {
		/** Channel Number to scan */
		user_scan_cfg->chan_list[0].chan_number = chan_number;
		/** Radio type: 'B/G' Band = 0, 'A' Band = 1 */
		/* fixme: B/G is hardcoded here. Ask the caller first to
		   send the radio type and then change here */
		if (chan_number > 14)
			user_scan_cfg->chan_list[0].radio_type = 1;
		/** Scan type: Active = 1, Passive = 2 */
		/* fixme: Active is hardcoded here. Ask the caller first to
		   send the  type and then change here */
		user_scan_cfg->chan_list[0].scan_type = 1;
		/** Scan duration in milliseconds; if 0 default used */
		user_scan_cfg->chan_list[0].scan_time = scan_time;
	}

	mlan_status rv = wlan_scan_networks(
					    (mlan_private *) mlan_adap->priv[0],
					    NULL, user_scan_cfg);
	if (rv != MLAN_STATUS_SUCCESS) {
		wifi_e("Scan command failed");
		os_mem_free(user_scan_cfg);
		return -WM_FAIL;
	}
	
	/* fixme: Can we free this immediately after wlan_scan_networks
	   call returns */
	os_mem_free(user_scan_cfg);
	return WM_SUCCESS;
}

static int wifi_send_key_material_cmd(int bss_index, mlan_ds_sec_cfg *sec)
{
	/* fixme: check if this needs to go on heap */
	mlan_ioctl_req req;
	mlan_status rv = MLAN_STATUS_SUCCESS;

	memset(&req, 0x00, sizeof(mlan_ioctl_req));
	req.pbuf = (t_u8 *)sec;
	req.buf_len = sizeof(mlan_ds_sec_cfg);
	req.bss_index = bss_index;
	req.req_id = MLAN_IOCTL_SEC_CFG;
	req.action = MLAN_ACT_SET;

	if (bss_index)
		rv = wlan_ops_uap_ioctl(mlan_adap, &req);
	else
		rv = wlan_ops_sta_ioctl(mlan_adap, &req);

	if (rv != MLAN_STATUS_SUCCESS && rv != MLAN_STATUS_PENDING) {
		return -WM_FAIL;
	}

	return WM_SUCCESS;
}

uint8_t broadcast_mac_addr[] ={ 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };

int wifi_set_key(int bss_index, bool is_pairwise, const uint8_t key_index,
		const uint8_t *key, unsigned key_len, const uint8_t *mac_addr)
{
	/* fixme: check if this needs to go on heap */
	mlan_ds_sec_cfg sec;

	memset(&sec, 0x00, sizeof(mlan_ds_sec_cfg));
	sec.sub_command = MLAN_OID_SEC_CFG_ENCRYPT_KEY;

	if (key_len > MAX_WEP_KEY_SIZE) {
		sec.param.encrypt_key.key_flags = KEY_FLAG_TX_SEQ_VALID |
					KEY_FLAG_RX_SEQ_VALID;
		if (is_pairwise) {
#ifndef CONFIG_WiFi_8801
			sec.param.encrypt_key.key_index = MLAN_KEY_INDEX_UNICAST;
#endif
			sec.param.encrypt_key.key_flags |= KEY_FLAG_SET_TX_KEY;
		} else {
#ifndef CONFIG_WiFi_8801
			sec.param.encrypt_key.key_index = 0;
			memcpy(sec.param.encrypt_key.mac_addr, broadcast_mac_addr,
				MLAN_MAC_ADDR_LENGTH);
#endif
			sec.param.encrypt_key.key_flags |= KEY_FLAG_GROUP_KEY;
		}
#ifdef CONFIG_WiFi_8801
		sec.param.encrypt_key.key_index = key_index;
		memcpy(sec.param.encrypt_key.mac_addr, mac_addr,
			MLAN_MAC_ADDR_LENGTH);
#endif
	} else {
		sec.param.encrypt_key.key_index = MLAN_KEY_INDEX_DEFAULT;
		sec.param.encrypt_key.is_current_wep_key = MTRUE;
	}

	sec.param.encrypt_key.key_len = key_len;
	memcpy(sec.param.encrypt_key.key_material, key, key_len);

	return wifi_send_key_material_cmd(bss_index, &sec);
}

int wifi_remove_key(int bss_index, bool is_pairwise,
		const uint8_t key_index, const uint8_t *mac_addr)
{
	/* fixme: check if this needs to go on heap */
	mlan_ds_sec_cfg sec;

	memset(&sec, 0x00, sizeof(mlan_ds_sec_cfg));
	sec.sub_command = MLAN_OID_SEC_CFG_ENCRYPT_KEY;

	sec.param.encrypt_key.key_flags = KEY_FLAG_REMOVE_KEY;
	sec.param.encrypt_key.key_remove = MTRUE;

	sec.param.encrypt_key.key_index = key_index;
	memcpy(sec.param.encrypt_key.mac_addr, mac_addr,
			MLAN_MAC_ADDR_LENGTH);

	sec.param.encrypt_key.key_len = WPA_AES_KEY_LEN;

	return wifi_send_key_material_cmd(bss_index, &sec);
}

static int wifi_send_rf_antenna_cmd(t_u16 action, uint16_t *ant_mode)
{
	mlan_private *pmpriv = (mlan_private *) mlan_adap->priv[0];

	if (action != HostCmd_ACT_GEN_GET && action != HostCmd_ACT_GEN_SET)
		return -WM_FAIL;

	wifi_get_command_lock();
	HostCmd_DS_COMMAND *cmd = wifi_get_command_buffer();

	cmd->seq_num = 0x0;
	cmd->result = 0x0;
	

	mlan_status rv =
		wlan_ops_sta_prepare_cmd(pmpriv,
					 HostCmd_CMD_802_11_RF_ANTENNA,
					 action, 0, NULL, ant_mode, cmd);
	if (rv != MLAN_STATUS_SUCCESS)
		return -WM_FAIL;

	return wifi_wait_for_cmdresp(action == HostCmd_ACT_GEN_GET ?
				     ant_mode : NULL);
}

int wifi_get_antenna(uint16_t *ant_mode)
{
	if (!ant_mode)
		return -WM_E_INVAL;

	int rv = wifi_send_rf_antenna_cmd(HostCmd_ACT_GEN_GET, ant_mode);
	if (rv != WM_SUCCESS || wm_wifi.cmd_resp_status != WM_SUCCESS)
		return -WM_FAIL;

	return WM_SUCCESS;
}

int wifi_set_antenna(uint16_t ant_mode)
{
	return wifi_send_rf_antenna_cmd(HostCmd_ACT_GEN_SET, &ant_mode);
}


static int wifi_send_cmd_802_11_supplicant_pmk(int mode,
					       mlan_ds_sec_cfg *sec,
					       t_u32 action)
{
	/* fixme: check if this needs to go on heap */
	mlan_ioctl_req req;

	memset(&req, 0x00, sizeof(mlan_ioctl_req));
	req.pbuf = (t_u8 *)sec;
	req.buf_len = sizeof(mlan_ds_sec_cfg);
	req.bss_index = 0;
	req.req_id = MLAN_IOCTL_SEC_CFG;
	req.action = action;

	current_mode = mode;

	mlan_status rv = wlan_ops_sta_ioctl(mlan_adap, &req);
	if (rv != MLAN_STATUS_SUCCESS && rv != MLAN_STATUS_PENDING) {
		return -WM_FAIL;
	}

	return WM_SUCCESS;
}

int wifi_send_add_wpa_pmk(int mode, char *ssid, char *pmk, unsigned int len)
{
	if (!ssid || (len != MLAN_MAX_KEY_LENGTH))
		return -WM_E_INVAL;

	mlan_ds_sec_cfg sec;

	memset(&sec, 0x00, sizeof(mlan_ds_sec_cfg));
	sec.sub_command = MLAN_OID_SEC_CFG_PASSPHRASE;

	/* SSID */
	int ssid_len = strlen(ssid);
	if (ssid_len > MLAN_MAX_SSID_LENGTH)
		return -WM_E_INVAL;

	mlan_ds_passphrase *pp = &sec.param.passphrase;
	pp->ssid.ssid_len = ssid_len;
	memcpy(pp->ssid.ssid, ssid, ssid_len);

	/* Zero MAC */

	/* PMK */
	pp->psk_type = MLAN_PSK_PMK;
	memcpy(pp->psk.pmk.pmk, pmk, len);

	return wifi_send_cmd_802_11_supplicant_pmk(mode, &sec, MLAN_ACT_SET);
}

/* fixme: This function has not been tested because of known issue in
   calling function. The calling function has been disabled for that */
int wifi_send_get_wpa_pmk(int mode, char *ssid)
{
	if (!ssid)
		return -WM_E_INVAL;

	mlan_ds_sec_cfg sec;

	memset(&sec, 0x00, sizeof(mlan_ds_sec_cfg));
	sec.sub_command = MLAN_OID_SEC_CFG_PASSPHRASE;

	/* SSID */
	int ssid_len = strlen(ssid);
	if (ssid_len > MLAN_MAX_SSID_LENGTH)
		return -WM_E_INVAL;

	mlan_ds_passphrase *pp = &sec.param.passphrase;
	pp->ssid.ssid_len = ssid_len;
	memcpy(pp->ssid.ssid, ssid, ssid_len);

	/* Zero MAC */

	pp->psk_type = MLAN_PSK_QUERY;

	return wifi_send_cmd_802_11_supplicant_pmk(mode, &sec, MLAN_ACT_GET);
}

/*
Note:
Passphrase can be between 8 to 63 if it is ASCII and 64 if its PSK
hexstring
*/
int wifi_send_add_wpa_psk(int mode, char *ssid,
			  char *passphrase, unsigned int len)
{
	if (!ssid || (len > 64))
		return -WM_E_INVAL;

	mlan_ds_sec_cfg sec;

	memset(&sec, 0x00, sizeof(mlan_ds_sec_cfg));
	sec.sub_command = MLAN_OID_SEC_CFG_PASSPHRASE;

	/* SSID */
	int ssid_len = strlen(ssid);
	if (ssid_len > MLAN_MAX_SSID_LENGTH)
		return -WM_E_INVAL;

	mlan_ds_passphrase *pp = &sec.param.passphrase;
	pp->ssid.ssid_len = ssid_len;
	memcpy(pp->ssid.ssid, ssid, ssid_len);

	/* Zero MAC */

	/* Passphrase */
	pp->psk_type = MLAN_PSK_PASSPHRASE;
	pp->psk.passphrase.passphrase_len = len;
	memcpy(pp->psk.passphrase.passphrase, passphrase, len);

	return wifi_send_cmd_802_11_supplicant_pmk(mode, &sec, MLAN_ACT_SET);
}

int wifi_send_clear_wpa_psk(int mode, const char *ssid)
{
	if (!ssid)
		return -WM_E_INVAL;

	mlan_ds_sec_cfg sec;

	memset(&sec, 0x00, sizeof(mlan_ds_sec_cfg));
	sec.sub_command = MLAN_OID_SEC_CFG_PASSPHRASE;

	/* SSID */
	int ssid_len = strlen(ssid);
	if (ssid_len > MLAN_MAX_SSID_LENGTH)
		return -WM_E_INVAL;

	sec.param.passphrase.ssid.ssid_len = ssid_len;
	strcpy((char *)sec.param.passphrase.ssid.ssid, ssid);

	/* Zero MAC */

	sec.param.passphrase.psk_type = MLAN_PSK_CLEAR;
	return wifi_send_cmd_802_11_supplicant_pmk(mode, &sec, MLAN_ACT_SET);
}

int wifi_send_disable_supplicant(int mode)
{
	mlan_ds_sec_cfg sec;

	memset(&sec, 0x00, sizeof(mlan_ds_sec_cfg));
	sec.sub_command = MLAN_OID_SEC_CFG_PASSPHRASE;

	sec.param.passphrase.psk_type = MLAN_PSK_CLEAR;

	return wifi_send_cmd_802_11_supplicant_pmk(mode, &sec, MLAN_ACT_SET);
}

int wifi_set_mac_multicast_addr(const char *mlist, t_u32 num_of_addr)
{
	if (!mlist)
		return -WM_E_INVAL;
	if (num_of_addr > MLAN_MAX_MULTICAST_LIST_SIZE)
		return -WM_E_INVAL;

	mlan_multicast_list *pmcast_list;
	pmcast_list = os_mem_alloc(sizeof(mlan_multicast_list));
	if (!pmcast_list)
		return -WM_E_NOMEM;

	memcpy(pmcast_list->mac_list, mlist,
	       num_of_addr * MLAN_MAC_ADDR_LENGTH);
	pmcast_list->num_multicast_addr = num_of_addr;
	wifi_get_command_lock();
	HostCmd_DS_COMMAND *cmd = wifi_get_command_buffer();

	mlan_status rv = wlan_ops_sta_prepare_cmd(
						  (mlan_private *)
						  mlan_adap->priv[0],
						  HostCmd_CMD_MAC_MULTICAST_ADR,
						  HostCmd_ACT_GEN_SET,
						  0, NULL, pmcast_list,  cmd);

	if (rv != MLAN_STATUS_SUCCESS) {
		os_mem_free(pmcast_list);
		return -WM_FAIL;
	}
	wifi_wait_for_cmdresp(NULL);
	os_mem_free(pmcast_list);
	return WM_SUCCESS;
}

int wifi_get_firmware_version_ext(wifi_fw_version_ext_t *version_ext)
{
	if (!version_ext)
		return -WM_E_INVAL;

	t_u32 version_sel = 0;
	mlan_private *pmpriv = (mlan_private *) mlan_adap->priv[0];
	mlan_status rv =
		wifi_prepare_and_send_cmd(pmpriv,
					  HostCmd_CMD_VERSION_EXT,
					  HostCmd_ACT_GEN_GET, 0, NULL,
					  &version_sel,
					  MLAN_BSS_TYPE_STA,
					  version_ext);
	return (rv == MLAN_STATUS_SUCCESS ? WM_SUCCESS : -WM_FAIL);
}

int wifi_get_firmware_version(wifi_fw_version_t *ver)
{
	if (!ver)
		return -WM_E_INVAL;

	union {
		uint32_t l;
		uint8_t c[4];
	} u_ver;
	char fw_ver[32];

	u_ver.l = mlan_adap->fw_release_number;
	sprintf(fw_ver, "%u.%u.%u.p%u", u_ver.c[2], u_ver.c[1],
		u_ver.c[0], u_ver.c[3]);

	snprintf(ver->version_str, MLAN_MAX_VER_STR_LEN,
		 driver_version_format, fw_ver, driver_version);

	return WM_SUCCESS;
}

int wifi_get_region_code(t_u32 *region_code)
{
	mlan_ds_misc_cfg misc;

	mlan_ioctl_req req = {
		.bss_index = 0,
		.pbuf = (t_u8 *)&misc,
		.action = MLAN_ACT_GET,
	};

	mlan_status mrv = wlan_misc_ioctl_region(mlan_adap, &req);
	if (mrv != MLAN_STATUS_SUCCESS) {
		wifi_w("Unable to get region code");
		return -WM_FAIL;
	}

	*region_code = misc.param.region_code;
	return WM_SUCCESS;
}

int wifi_set_region_code(t_u32 region_code)
{
	mlan_ds_misc_cfg misc = {
		.param.region_code = region_code,
	};

	mlan_ioctl_req req = {
		.bss_index = 0,
		.pbuf = (t_u8 *)&misc,
		.action = MLAN_ACT_SET,
	};

	mlan_status mrv = wlan_misc_ioctl_region(mlan_adap, &req);
	if (mrv != MLAN_STATUS_SUCCESS) {
		wifi_w("Unable to set region code");
		return -WM_FAIL;
	}

	return WM_SUCCESS;
}

#ifdef CONFIG_11D
static int wifi_send_11d_cfg_ioctl(mlan_ds_11d_cfg *d_cfg)
{
	/* fixme: check if this needs to go on heap */
	mlan_ioctl_req req;

	memset(&req, 0x00, sizeof(mlan_ioctl_req));
	req.pbuf = (t_u8 *)d_cfg;
	req.buf_len = sizeof(mlan_ds_11d_cfg);
	req.bss_index = 0;
	req.req_id = MLAN_IOCTL_11D_CFG;
	req.action = HostCmd_ACT_GEN_SET;

	mlan_status rv = wlan_ops_sta_ioctl(mlan_adap, &req);
	if (rv != MLAN_STATUS_SUCCESS && rv != MLAN_STATUS_PENDING) {
		return -WM_FAIL;
	}

	return WM_SUCCESS;
}
int wifi_set_domain_params(wifi_domain_param_t *dp)
{
	if (!dp)
		return -WM_E_INVAL;

	mlan_ds_11d_cfg d_cfg;

	memset(&d_cfg, 0x00, sizeof(mlan_ds_11d_cfg));

	d_cfg.sub_command = MLAN_OID_11D_DOMAIN_INFO;

	memcpy(&d_cfg.param.domain_info.country_code, dp->country_code,
			COUNTRY_CODE_LEN);

	d_cfg.param.domain_info.band = BAND_A;
	d_cfg.param.domain_info.no_of_sub_band = dp->no_of_sub_band;

	wifi_sub_band_set_t *is = dp->sub_band;
	mlan_ds_subband_set_t *s = d_cfg.param.domain_info.sub_band;
	int i;
	for (i = 0; i < dp->no_of_sub_band; i++) {
		s[i].first_chan = is[i].first_chan;
		s[i].no_of_chan = is[i].no_of_chan;
		s[i].max_tx_pwr = is[i].max_tx_pwr;
	}

	return wifi_send_11d_cfg_ioctl(&d_cfg);
}
int wifi_set_11d(bool enable)
{
	mlan_ds_11d_cfg d_cfg;

	memset(&d_cfg, 0x00, sizeof(mlan_ds_11d_cfg));

	d_cfg.sub_command = MLAN_OID_11D_CFG_ENABLE;
	d_cfg.param.enable_11d = enable;

	return wifi_send_11d_cfg_ioctl(&d_cfg);
}
#endif

static int get_free_mgmt_ie_index()
{
	if (!(mgmt_ie_index_bitmap & MBIT(0)))
		return 0;
	else if (!(mgmt_ie_index_bitmap & MBIT(1)))
		return 1;
	else if (!(mgmt_ie_index_bitmap & MBIT(2)))
		return 2;
	else if (!(mgmt_ie_index_bitmap & MBIT(3)))
		return 3;
	return -1;
}

static void set_ie_index(int index)
{
	mgmt_ie_index_bitmap |= (MBIT(index));
}

static void clear_ie_index(int index)
{
	mgmt_ie_index_bitmap &= ~(MBIT(index));
}

int wifi_config_mgmt_ie(unsigned int bss_type, int action, unsigned int index,
				void *buffer, unsigned int *ie_len)
{
	uint8_t *buf, *pos;
	IEEEtypes_Header_t *ptlv_header = NULL;
	uint16_t buf_len = 0;
	tlvbuf_custom_ie *tlv = NULL;
	custom_ie *ie_ptr = NULL;
	int mgmt_ie_index = -1;

	buf = (uint8_t *) os_mem_alloc(MRVDRV_SIZE_OF_CMD_BUFFER);
	if (!buf) {
		wmprintf("ERR:Cannot allocate memory!\n");
		return -WM_FAIL;
	}

	memset(buf, 0, MRVDRV_SIZE_OF_CMD_BUFFER);

	tlv = (tlvbuf_custom_ie *) buf;
	tlv->type = MRVL_MGMT_IE_LIST_TLV_ID;

	/* Locate headers */
	ie_ptr = (custom_ie *) (tlv->ie_data);
	/* Set TLV fields */

	buf_len = sizeof(tlvbuf_custom_ie);

	if (action == HostCmd_ACT_GEN_SET) {
		if (*ie_len == 0) {
			if (index < 0)
				return -WM_FAIL;

			/* Clear WPS IE */
			ie_ptr->mgmt_subtype_mask = MGMT_MASK_CLEAR;
			ie_ptr->ie_length = 0;
			ie_ptr->ie_index = index;

			ie_ptr =
			    (custom_ie *) (((uint8_t *) ie_ptr)
					+ sizeof(custom_ie));
			ie_ptr->mgmt_subtype_mask = MGMT_MASK_CLEAR;
			ie_ptr->ie_length = 0;
			ie_ptr->ie_index = index + 1;
			tlv->length = 2 * (sizeof(custom_ie) - MAX_IE_SIZE);
			buf_len += tlv->length;
			clear_ie_index(index);
		} else {
			/* Set WPS IE */
			mgmt_ie_index = get_free_mgmt_ie_index();

			if (mgmt_ie_index < 0)
				return -WM_FAIL;

			pos = ie_ptr->ie_buffer;
			ptlv_header = (IEEEtypes_Header_t *) pos;
			pos += sizeof(IEEEtypes_Header_t);

			ptlv_header->element_id = index;
			ptlv_header->len = *ie_len;
			if (bss_type == MLAN_BSS_TYPE_UAP)
				ie_ptr->mgmt_subtype_mask = MGMT_MASK_BEACON |
							MGMT_MASK_PROBE_RESP |
							MGMT_MASK_ASSOC_RESP |
							MGMT_MASK_REASSOC_RESP;
			else if (bss_type == MLAN_BSS_TYPE_STA)
				ie_ptr->mgmt_subtype_mask =
							MGMT_MASK_PROBE_REQ |
							MGMT_MASK_ASSOC_REQ |
							MGMT_MASK_REASSOC_REQ;

			tlv->length =
				sizeof(custom_ie) + sizeof(IEEEtypes_Header_t)
					+ *ie_len - MAX_IE_SIZE;
			ie_ptr->ie_length = sizeof(IEEEtypes_Header_t) + *ie_len;
			ie_ptr->ie_index = mgmt_ie_index;

			buf_len += tlv->length;

			memcpy(pos, buffer, *ie_len);
		}
	}
	else if (action == HostCmd_ACT_GEN_GET) {
		/* Get WPS IE */
		tlv->length = 0;
	}

	mlan_status rv = wrapper_wlan_cmd_mgmt_ie(bss_type, buf,
						buf_len, action);

        if (rv != MLAN_STATUS_SUCCESS && rv != MLAN_STATUS_PENDING) {
		os_mem_free(buf);
                return -WM_FAIL;
	}

	if (action == HostCmd_ACT_GEN_GET) {
		if (wm_wifi.cmd_resp_status) {
			wifi_w("Unable to get mgmt ie buffer");
			os_mem_free(buf);
			return wm_wifi.cmd_resp_status;
		}
		ie_ptr = (custom_ie *) (buf);
		memcpy(buffer, ie_ptr->ie_buffer, ie_ptr->ie_length);
		*ie_len = ie_ptr->ie_length;
	}

	if (buf)
		os_mem_free(buf);

	if ((action == HostCmd_ACT_GEN_SET) && *ie_len)
	{
		set_ie_index(mgmt_ie_index);
		return mgmt_ie_index;
	}
	else
		return WM_SUCCESS;
}

int wifi_get_mgmt_ie(unsigned int bss_type, unsigned int index,
			void *buf, unsigned int *buf_len)
{
	return wifi_config_mgmt_ie(bss_type, HostCmd_ACT_GEN_GET,
					index, buf, buf_len);
}

int wifi_set_mgmt_ie(unsigned int bss_type, unsigned int id,
			void *buf, unsigned int buf_len)
{
	unsigned int data_len = buf_len;

	return wifi_config_mgmt_ie(bss_type, HostCmd_ACT_GEN_SET,
					id, buf, &data_len);
}

int wifi_clear_mgmt_ie(unsigned int bss_type, unsigned int index)
{
	unsigned int data_len = 0;
	return wifi_config_mgmt_ie(bss_type, HostCmd_ACT_GEN_SET,
					index, NULL, &data_len);
}
