/* 
 *  Copyright 2008-2013, Marvell International Ltd.
 *  All Rights Reserved.
 */

#ifndef __WIFI_H__
#define __WIFI_H__

#include <wifi-decl.h>
#include <wifi_events.h>
#include <pwrmgr.h>
#include <wm_os.h>
#include <wmerrno.h>

enum {
	WM_E_WIFI_ERRNO_START = MOD_ERROR_START(MOD_WIFI),
};


int wm_wifi_init(void);
void wm_wifi_deinit(void);



/**
 * Get the device MAC address
 */
int wifi_get_device_mac_addr(mac_addr_t *mac_addr);

/**
 * Get the string representation of the wlan firmware version.
 */
int wifi_get_firmware_version(wifi_fw_version_t *ver);

/**
 * Get the string representation of the wlan firmware extended version.
 */
int wifi_get_firmware_version_ext(wifi_fw_version_ext_t *version_ext);

/**
 * Get the cached string representation of the wlan firmware extended version.
 */
int wifi_get_device_firmware_version_ext(wifi_fw_version_ext_t *fw_ver_ext);

/**
 * Get the timestamp of the last command sent to the firmware
 *
 * @return Timestamp in millisec of the last command sent
 */
unsigned wifi_get_last_cmd_sent_ms(void);


/**
 * This will update the last command sent variable value to current
 * time. This is used for power management.
 */
void wifi_update_last_cmd_sent_ms();

/**
 * Register an event queue with the wifi driver to receive events
 *
 * The list of events which can be received from the wifi driver are
 * enumerated in the file wifi_events.h
 *
 * @param[in] event_queue The queue to which wifi driver will post events.
 *
 * @note Only one queue can be registered. If the registered queue needs to
 * be changed unregister the earlier queue first.
 *
 * @return Standard SDK return codes
 */
int wifi_register_event_queue(os_queue_t *event_queue);

/**
 * Unregister an event queue from the wifi driver.
 *
 * @param[in] event_queue The queue to which was registered earlier with
 * the wifi driver.
 *
 * @return Standard SDK return codes
 */
int wifi_unregister_event_queue(os_queue_t *event_queue);

/**
 * Get the count of elements in the scan list
 *
 * @param[in,out] count Pointer to a variable which will hold the count after
 * this call returns
 *
 * @warn The count returned by this function is the current count of the
 * elements. A scan command given to the driver or some other background
 * event may change this count in the wifi driver. Thus when the API
 * \ref wifi_get_scan_result is used to get individual elements of the scan
 * list, do not assume that it will return exactly 'count' number of
 * elements. Your application should not consider such situations as a
 * major event.
 *
 * @return Standard SDK return codes.
 */
int wifi_get_scan_result_count(unsigned *count);

int wifi_get_scan_result(unsigned int index, struct wifi_scan_result **desc);
int wifi_deauthenticate(uint8_t *bssid);

int wifi_uap_start(int type, char *ssid, uint8_t *mac_addr,
		   int security, char *passphrase,
		   int chan_number);
int wifi_uap_stop(int type);

int wifi_get_uap_max_clients(unsigned int *max_sta_num);
int wifi_set_uap_max_clients(unsigned int *max_sta_num);

int wifi_get_mgmt_ie(unsigned int bss_type, unsigned int index,
			void *buf, unsigned int *buf_len);
int wifi_set_mgmt_ie(unsigned int bss_type, unsigned int index,
			void *buf, unsigned int buf_len);
int wifi_clear_mgmt_ie(unsigned int bss_type, unsigned int index);
/**
 * Returns the current STA list connected to our uAP
 *
 * This function gets its information after querying the firmware. It will
 * block till the response is received from firmware or a timeout.
 *
 * @param[in, out] list After this call returns this points to the
 * structure \ref sta_list_t allocated by the callee. This is variable
 * length structure and depends on count variable inside it. <b> The caller
 * needs to free this buffer after use.</b>. If this function is unable to
 * get the sta list, the value of list parameter will be NULL
 *
 * \note The caller needs to explicitly free the buffer returned by this
 * function.
 *
 * @return void
 */
int wifi_uap_bss_sta_list(sta_list_t **list);

/** Set wifi calibration data in firmware.
 *
 * This function may be used to set wifi calibration data in firmware.
 *
 * @param[in] cdata The calibration data
 * @param[in] clen Length of calibration data
 *
 */
void wifi_set_cal_data(uint8_t *cdata, unsigned int clen);

/** Set wifi MAC address in firmware.
 *
 * This function may be used to set wifi MAC address in firmware.
 *
 * @param[in] mac The new MAC Address
 *
 */
void wifi_set_mac_addr(uint8_t *mac);

int wifi_sniffer_start(const t_u16 filter_flags, const t_u8 radio_type,
			const t_u8 channel);
int wifi_sniffer_status();
int wifi_sniffer_stop();

int wifi_set_key(int bss_index, bool is_pairwise, const uint8_t key_index,
		const uint8_t *key, unsigned key_len, const uint8_t *mac_addr);
int wifi_remove_key(int bss_index, bool is_pairwise,
		const uint8_t key_index, const uint8_t *mac_addr);
#ifdef CONFIG_P2P
int wifi_register_wfd_event_queue(os_queue_t *event_queue);
int wifi_unregister_wfd_event_queue(os_queue_t *event_queue);
void wifi_wfd_event(bool peer_event, void *data);
int wifi_wfd_start(char *ssid, int security, char *passphrase, int channel);
int wifi_wfd_stop(void);

/**
 * Returns the current STA list connected to our WFD
 *
 * This function gets its information after querying the firmware. It will
 * block till the response is received from firmware or a timeout.
 *
 * @param[in, out] list After this call returns this points to the
 * structure \ref sta_list_t allocated by the callee. This is variable
 * length structure and depends on count variable inside it. <b> The caller
 * needs to free this buffer after use.</b>. If this function is unable to
 * get the sta list, the value of list parameter will be NULL
 *
 * \note The caller needs to explicitly free the buffer returned by this
 * function.
 *
 * @return void
 */
int wifi_wfd_bss_sta_list(sta_list_t **list);


int wifi_get_wfd_mac_address(void);
int  wifi_wfd_ps_inactivity_sleep_enter(unsigned int ctrl_bitmap,
			       unsigned int inactivity_to,
			       unsigned int min_sleep,
			       unsigned int max_sleep,
				unsigned int min_awake, unsigned int max_awake);

int  wifi_wfd_ps_inactivity_sleep_exit();
int wifidirectapcmd_sys_config();
void wifidirectcmd_config();
#endif

int wifi_get_wpa_ie_in_assoc(uint8_t *wpa_ie);

extern os_semaphore_t g_wakeup_sem;
/* #define WAKE_SEM_DBG    1*/

#ifdef WAKE_SEM_DBG
static inline int __wakeup_sem_get(unsigned long wait, const char *func)
{
	wmprintf("[wksem] Get %s\r\n", func);
	int ret = os_mutex_get(&g_wakeup_sem, wait);
	wmprintf("[wksem] Get ret = %d %s\r\n", ret, func);
	return ret;
}

static inline int __wakeup_sem_put(const char *func)
{
	wmprintf("[wksem] Put %s\r\n", func);
	return os_mutex_put(&g_wakeup_sem);
}

#define wakeup_sem_get(wait) __wakeup_sem_get((wait), __func__)
#define wakeup_sem_put(wait) __wakeup_sem_put(__func__)
#else
static inline int wakeup_sem_get(unsigned long wait)
{
	return os_mutex_get(&g_wakeup_sem, wait);
}

static inline int wakeup_sem_put()
{
	return os_mutex_put(&g_wakeup_sem);
}
#endif


/** Add Multicast Filter by MAC Address
 *
 * Multicast filters should be registered with the WiFi driver for IP-level
 * multicast addresses to work. This API allows for registration of such filters
 * with the WiFi driver.
 *
 * If multicast-mapped MAC address is 00:12:23:34:45:56 then pass mac_addr as
 * below:
 * mac_add[0] = 0x00
 * mac_add[1] = 0x12
 * mac_add[2] = 0x23
 * mac_add[3] = 0x34
 * mac_add[4] = 0x45
 * mac_add[5] = 0x56
 *
 * \param[in] mac_addr multicast mapped MAC address
 *
 * \return 0 on Success or else Error
 */
int wifi_add_mcast_filter(uint8_t *mac_addr);

/** Remove Multicast Filter by MAC Address
 *
 * This function removes multicast filters for the given multicast-mapped
 * MAC address. If multicast-mapped MAC address is 00:12:23:34:45:56
 * then pass mac_addr as below:
 * mac_add[0] = 0x00
 * mac_add[1] = 0x12
 * mac_add[2] = 0x23
 * mac_add[3] = 0x34
 * mac_add[4] = 0x45
 * mac_add[5] = 0x56
 *
 * \param[in] mac_addr multicast mapped MAC address
 *
 * \return  0 on Success or else Error
 */
int wifi_remove_mcast_filter(uint8_t *mac_addr);

/** Get Multicast Mapped Mac address from IPv4
 *
 * This function will generate Multicast Mapped MAC address from IPv4
 * Multicast Mapped MAC address will be in following format:
 * 1) Higher 24-bits filled with IANA Multicast OUI (01-00-5E)
 * 2) 24th bit set as Zero
 * 3) Lower 23-bits filled with IP address (ignoring higher 9bits).
 *
 * \param[in] mac_addr ipaddress(input)
 * \param[in] mac_addr multicast mapped MAC address(output)
 *
 * \return  void
 */
void wifi_get_ipv4_multicast_mac(uint32_t ipaddr, uint8_t *mac_addr);


int wifi_get_antenna(uint16_t *ant_mode);
int wifi_set_antenna(uint16_t ant_mode);

void wifi_wfd_event(bool peer_event, void *data);
void wifi_process_hs_cfg_resp(t_u8 *cmd_res_buffer);
int wifi_process_ps_enh_response(t_u8 *cmd_res_buffer, t_u16 *ps_event,
				 t_u16 *action);

int wifi_uap_tx_power_getset(uint8_t action, uint8_t *tx_power_dbm);
int wifi_uap_rates_getset(uint8_t action, char *rates, uint8_t num_rates);

typedef enum {
	REG_MAC = 1,
	REG_BBP,
	REG_RF
} wifi_reg_t;

int wifi_reg_access(wifi_reg_t reg_type, uint16_t action,
			uint32_t offset, uint32_t *value);

int wifi_get_eeprom_data(uint32_t offset, uint32_t byte_count, uint8_t *buf);
/*
 * This function is supposed to be called after scan is complete from wlc
 * manager.
 */
void wifi_scan_process_results(void);

/**
 * Get the wifi region code
 *
 * This function will return one of the following values in the region_code
 * variable.\n
 * 0x10 : US FCC\n
 * 0x20 : CANADA\n
 * 0x30 : EU\n
 * 0x32 : FRANCE\n
 * 0x40 : JAPAN\n
 * 0x41 : JAPAN\n
 * 0x50 : China\n
 * 0xfe : JAPAN\n
 * 0xff : Special\n
 *
 * @return Standard WMSDK return codes.
 */
int wifi_get_region_code(t_u32 *region_code);

/**
 * Set the wifi region code.
 *
 * This function takes one of the values from the following array.\n
 * 0x10 : US FCC\n
 * 0x20 : CANADA\n
 * 0x30 : EU\n
 * 0x32 : FRANCE\n
 * 0x40 : JAPAN\n
 * 0x41 : JAPAN\n
 * 0x50 : China\n
 * 0xfe : JAPAN\n
 * 0xff : Special\n
 *
 * @return Standard WMSDK return codes.
 */
int wifi_set_region_code(t_u32 region_code);

/**
 * Get the uAP channel number
 *
 *
 * @param[in] channel Pointer to channel number. Will be initialized by
 * callee
 * @return Standard WMSDK return code
 */
int wifi_get_uap_channel(int *channel);

/**
 * Sets the domain parameters for the uAP.
 *
 * @note This API only saves the domain params inside the driver internal
 * structures. The actual application of the params will happen only during
 * starting phase of uAP. So, if the uAP is already started then the
 * configuration will not apply till uAP re-start.
 *
 * @ To use this API you will need to fill up the structure
 * \ref wifi_domain_param_t with correct parameters.
 *
 * E.g. Programming for US country code
 * ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
 *	wifi_sub_band_set_t sb = {
 *		.first_chan = 1,
 *		.no_of_chan= 11,
 *		.max_tx_pwr = 30,
 *	};
 *
 *	wifi_domain_param_t *dp = os_mem_alloc(sizeof(wifi_domain_param_t) +
 *					       sizeof(wifi_sub_band_set_t));
 *
 *	memcpy(dp->country_code, "US\0", COUNTRY_CODE_LEN);
 *	dp->no_of_sub_band = 1;
 *	memcpy(dp->sub_band, &sb, sizeof(wifi_sub_band_set_t));
 *
 *	wmprintf("wifi uap set domain params\n\r");
 *	wifi_uap_set_domain_params(dp);
 *	os_mem_free(dp);
 *
 */
int wifi_uap_set_domain_params(wifi_domain_param_t *dp);
int wifi_set_domain_params(wifi_domain_param_t *dp);
int wifi_set_11d(bool enable);
int wifi_set_region_code(uint32_t region_code);
int wifi_get_tx_power();
int wifi_set_tx_power(int power_level);

int wifi_set_mgmt_ind(uint32_t mgmt_ind);
#endif
