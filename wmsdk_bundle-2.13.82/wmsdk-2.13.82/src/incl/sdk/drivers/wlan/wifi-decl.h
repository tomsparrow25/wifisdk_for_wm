/*! \file wifi-decl.h
 *  \brief Wifi structure declarations
 */

#ifndef __WIFI_DECL_H__
#define __WIFI_DECL_H__

#include <stdint.h>
#include <stdbool.h>
#include <wm_utils.h>

/* fixme: remove these after complete integration with mlan */
#define MLAN_MAC_ADDR_LENGTH     (6)
/** Version string buffer length */
#define MLAN_MAX_VER_STR_LEN    128

#define MAC_ADDR_LENGTH 6

#ifndef INCLUDE_FROM_MLAN
typedef char t_s8;
typedef unsigned char t_u8;
#endif /* INCLUDE_FROM_MLAN */


typedef struct {
	t_u8 mac[MLAN_MAC_ADDR_LENGTH];
	/*
	 * Power management status
	 * 0 = active (not in power save)
	 * 1 = in power save status
	 */
	t_u8 power_mgmt_status;
	/** RSSI: dBm */
	t_s8 rssi;
} sta_info_t;

/*
 *  Note: This is variable length structure. The size of array mac_list is
 *  equal to count. The caller of the API which returns this structure does
 *  not need to seperately free the array mac_list. It only needs to free
 *  the sta_list_t object after use.
 */
typedef struct {
	int count;
	/*
	 * Variable length array. Max size is MAX_NUM_CLIENTS.
	 */
	/* sta_info_t *list; */
} sta_list_t;

/** BSS type : STA */
#define BSS_TYPE_STA 0
/** BSS type : UAP */
#define BSS_TYPE_UAP 1

#define UAP_DEFAULT_CHANNEL 0

enum wifi_bss_security {
	WIFI_SECURITY_NONE = 0,
	WIFI_SECURITY_WEP_STATIC,
	WIFI_SECURITY_WEP_DYNAMIC,
	WIFI_SECURITY_WPA,
	WIFI_SECURITY_WPA2,
};

enum wifi_bss_features {
	WIFI_BSS_FEATURE_WMM = 0,
	WIFI_BSS_FEATURE_WPS = 1,
};

struct wifi_message {
	uint16_t event;
	uint16_t reason;
	void *data;
};

#ifdef CONFIG_P2P
struct wifi_wfd_event {
	bool peer_event;
	void *data;
};
#endif

typedef struct {
	uint8_t   wep40:1;
	uint8_t   wep104:1;
	uint8_t   tkip:1;
	uint8_t   ccmp:1;
	uint8_t   rsvd:4;
} _Cipher_t;

typedef struct {
	uint16_t     noRsn:1;
	uint16_t     wepStatic:1;
	uint16_t     wepDynamic:1;
	uint16_t     wpa:1;
	uint16_t     wpaNone:1;
	uint16_t     wpa2:1;
	uint16_t     rsvd:10;
} _SecurityMode_t;

/* TODO: clean up the parts brought over from the Host SME BSSDescriptor_t,
 * remove ifdefs, consolidate security info */

#ifndef INCLUDE_FROM_MLAN
/* fixme: remove this during cleanup */
typedef unsigned short t_u16;
typedef unsigned int t_u32;
#endif /* INCLUDE_FROM_MLAN */

/** MLAN Maximum SSID Length */
#define MLAN_MAX_SSID_LENGTH     (32)

struct wifi_scan_result {
	uint8_t bssid[MLAN_MAC_ADDR_LENGTH];
	bool is_ibss_bit_set;

	uint8_t ssid[MLAN_MAX_SSID_LENGTH];
	int ssid_len;
	uint8_t Channel;		/*!< Channel associated to the BSSID */
	uint8_t RSSI;		/*!< Received signal strength */
	_SecurityMode_t WPA_WPA2_WEP;
	_Cipher_t mcstCipher;
	_Cipher_t ucstCipher;

	/*!<
	 **  WPA_WPA2 = 0 => Security not enabled
	 **  = 1 => WPA mode
	 **  = 2 => WPA2 mode
	 **  = 3 => WEP mode
	 */
	bool phtcap_ie_present;
	bool phtinfo_ie_present;

	bool wmm_ie_present;
	uint8_t band;

	bool wps_IE_exist;
	uint16_t wps_session;
	bool wpa2_entp_IE_exist;
};

typedef struct {
	char mac[MAC_ADDR_LENGTH];
} mac_addr_t;

typedef struct {
	char version_str[MLAN_MAX_VER_STR_LEN];
} wifi_fw_version_t;

typedef struct {
	char version_str[MLAN_MAX_VER_STR_LEN];
} wifi_fw_version_ext_t;


enum wlan_type {
	WLAN_TYPE_NORMAL = 0,
	WLAN_TYPE_WIFI_CALIB,
	WLAN_TYPE_FCC_CERTIFICATION,
};

typedef struct {
	uint16_t current_level;
	uint8_t max_power;
	uint8_t min_power;

} wifi_tx_power_t;

typedef struct {
	uint16_t current_channel;
	uint16_t rf_type;
} wifi_rf_channel_t;

typedef struct {
	uint8_t tx_rate;
	uint8_t rx_rate;
} wifi_tx_rate_t;

typedef struct {
	int16_t data_rssi_last;
	int16_t data_nf_last;
	int16_t data_rssi_avg;
	int16_t data_nf_avg;

	int16_t bcn_snr_last;
	int16_t bcn_snr_avg;
	int16_t data_snr_last;
	int16_t data_snr_avg;

	int16_t bcn_rssi_last;
	int16_t bcn_nf_last;
	int16_t bcn_rssi_avg;
	int16_t bcn_nf_avg;
} wifi_rssi_info_t;

/**
 * Data structure for subband set
 *
 * For uAP 11d support
 */
typedef struct {
	/** First channel */
	t_u8 first_chan;
	/** Number of channels */
	t_u8 no_of_chan;
	/** Maximum Tx power in dBm */
	t_u8 max_tx_pwr;
} wifi_sub_band_set_t;

#define COUNTRY_CODE_LEN    3

/**
 * Data structure for domain parameters
 *
 * This structure is accepted by wlan_uap_set_domain_params() API. This
 * information is used to generate the country info IE.
 */
typedef struct {
	/** Country code */
	t_u8 country_code[COUNTRY_CODE_LEN];
	/** subbands count */
	int no_of_sub_band;
	/** Set of subbands of no_of_sub_band number of elements */
	wifi_sub_band_set_t sub_band[1];
} wifi_domain_param_t;

typedef struct {
	uint16_t monitor_activity;
	uint16_t filter_flags;
	uint8_t radio_type;
	uint8_t chan_number;
} wifi_net_monitor_t;

typedef PACK_START struct {
	uint8_t frame_ctrl_flags;
	uint16_t duration;
	char dest[MLAN_MAC_ADDR_LENGTH];
	char src[MLAN_MAC_ADDR_LENGTH];
	char bssid[MLAN_MAC_ADDR_LENGTH];
	uint16_t seq_frag_num;
	uint8_t timestamp[8];
	uint16_t beacon_interval;
	uint16_t cap_info;
	uint8_t ssid_element_id;
	uint8_t ssid_len;
	char ssid[MLAN_MAX_SSID_LENGTH];
}  PACK_END wifi_beacon_info_t;

typedef PACK_START struct {
	uint8_t frame_ctrl_flags;
	uint16_t duration;
	char bssid[MLAN_MAC_ADDR_LENGTH];
	char src[MLAN_MAC_ADDR_LENGTH];
	char dest[MLAN_MAC_ADDR_LENGTH];
	uint16_t seq_frag_num;
	uint16_t qos_ctrl;
} PACK_END wifi_data_info_t;

typedef enum {
	BEACON_FRAME = 0x80,
	DATA_FRAME = 0x08,
	QOS_DATA_FRAME = 0x88,
} wifi_frame_type_t;

typedef PACK_START struct {
	wifi_frame_type_t frame_type;
	union {
		wifi_beacon_info_t beacon_info;
		wifi_data_info_t data_info;
	} frame_data;
} PACK_END wifi_frame_t;
#endif /* __WIFI_DECL_H__ */
