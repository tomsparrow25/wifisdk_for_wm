/*
 *  Copyright (C) 2008-2013, Marvell International Ltd.
 *  All Rights Reserved.
 */

#include <string.h>
#include <wmstdio.h>
#include <wm_utils.h>
#include <wlan.h>
#include <wifi.h>
#include <wm_os.h>
#include <wm_net.h>
#include <wmtime.h>
#include <wmstats.h>
#include <stdint.h>
#include <healthmon.h>
#include <board.h>
#include <wps.h>
#include <mdev_sdio.h>
#include <crc32.h>
#ifdef CONFIG_HOST_SUPP
#include <wm_supplicant.h>
#endif

#define CMD_RESP_TIMEOUT 5000
#define DELAYED_SLP_CFM_DUR 10

/** sleep parameter : lower limit in micro-sec */
#define PS_SLEEP_PARAM_MIN                  6500
/** sleep parameter : upper limit in micro-sec */
#define PS_SLEEP_PARAM_MAX                  32000
/** power save awake period minimum value in micro-sec */
#define PS_AWAKE_PERIOD_MIN                 2000

#define PS_SLEEP_PERIOD_DEFAULT                 17000
#define PS_INACT_PERIOD_DEFAULT                 200000

#define WL_ID_DHCP_CLIENT "wifi_dhcp_client"
#define WL_ID_DEEPSLEEP_SM "wlcm_deepsleep_sm"
#define WL_ID_WIFI_RSSI "wifi_rssi"

/* Duplicated from wifi-sdio.h. Remove once that file can be included */
/** Unsigned long (4-bytes) */
typedef unsigned int t_u32;

typedef struct __nvram_backup_struct {
	t_u32 ioport;
	t_u32 curr_wr_port;
	t_u32 curr_rd_port;
	t_u32 mp_end_port;
	t_u32 bss_num;
	t_u32 sta_mac_addr1;
	t_u32 sta_mac_addr2;
	t_u32 wifi_state;
} nvram_backup_t;

typedef enum _mlan_status {
	MLAN_STATUS_FAILURE = 0xffffffff,
	MLAN_STATUS_SUCCESS = 0,
	MLAN_STATUS_PENDING,
	MLAN_STATUS_RESOURCE,
	MLAN_CARD_NOT_DETECTED,
	MLAN_STATUS_FW_DNLD_FAILED,
	MLAN_STATUS_FW_NOT_DETECTED,
	MLAN_STATUS_FW_NOT_READY,
	MLAN_CARD_CMD_TIMEOUT
} mlan_status;

bool g_req_sl_confirm;
int wrapper_wlan_cmd_get_hw_spec();
static int wlan_network_ip_get(char *ipaddr);
/* fixme: These need to be removed later after complete mlan integration */
void set_event_chanswann();
void clear_event_chanswann();
int  wifi_uap_ps_inactivity_sleep_enter(int type,
					unsigned int ctrl_bitmap,
					unsigned int inactivity_to,
					unsigned int min_sleep,
					unsigned int max_sleep,
					unsigned int min_awake,
					unsigned int max_awake);
int wifi_send_hs_cfg_cmd_sta(t_u32 ipv4_addr,
			 t_u16 action, t_u32 conditions);
int wifi_send_hs_cfg_cmd_uap(t_u32 ipv4_addr,
			 t_u16 action, t_u32 conditions);
void wrapper_wlan_11d_clear_parsedtable(void);
void wrapper_clear_media_connected_event(void);
int  wifi_uap_ps_inactivity_sleep_exit(int type);
int wifi_enter_ieee_power_save(void);
int wifi_exit_ieee_power_save(void);
int wifi_enter_deepsleep_power_save(void);
int wifi_exit_deepsleep_power_save(void);
void send_sleep_confirm_command();
int wlan_fw_heartbeat_register_healthmon(void);
int wlan_fw_heartbeat_unregister_healthmon();

static bool wlan_is_ieeeps_active();

#ifdef CONFIG_WPA2_ENTP
extern int wpa2_ent_connect(struct wlan_network *wpa2_network);
extern void wpa2_shutdown();
#endif

extern void sniffer_register_callback(void (*sniffer_cb)
				(const wlan_frame_t *frame,
				 const uint16_t len));

extern void sniffer_deregister_callback();

int wifi_send_rssi_info_cmd(wifi_rssi_info_t *rssi_info);
int wifi_configure_listen_interval(unsigned int time_in_ms);
void wifi_configure_null_pkt_interval(unsigned int time_in_secs);

int wifi_send_scan_cmd(uint8_t bss_mode, const void *specific_bssid,
		       const char *ssid, uint8_t chan_number,
		       uint32_t scan_time, bool keep_previous_scan);
int wrapper_wifi_assoc(const unsigned char *bssid, int wlan_security);
void wrapper_wlan_scan_process_results(void);
void wifi_uap_enable_sticky_bit(const uint8_t *mac_addr);

extern nvram_backup_t backup_s __attribute__ ((section(".nvram")));

extern int16_t g_bcn_nf_last;
struct wm_stats g_wm_stats;

/* fixme: remove this macro later */
#define MAC_ADDR_SIZE         6
extern uint8_t g_rssi;
extern uint16_t g_data_nf_last;
extern uint16_t g_data_snr_last;
char INVALID_PMK[WLAN_PMK_LENGTH] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

os_rw_lock_t ps_rwlock;
static int  ps_wakeup_card_cb(os_rw_lock_t *lock, unsigned int wait_time);


#ifdef CONFIG_WPS2
int wps_session_attempt;
#endif

#define MAX_EVENTS	20
#define CONNECTION_EVENT(r, data)	if (wlan.cb) wlan.cb(r, data)

#define ARRAY_SIZE(a)       (sizeof(a)/sizeof((a)[0]))

int wrapper_wlan_cmd_11n_addba_rspgen(void *data);

typedef struct _ps_sleep_param {
	/** control bitmap */
	unsigned int ctrl_bitmap;
	/** minimum sleep period (micro second) */
	unsigned int min_sleep;
	/** maximum sleep period (micro second) */
	unsigned int max_sleep;
} ps_sleep_param;

/** inactivity sleep_param */
typedef struct _inact_sleep_param {
	/** inactivity timeout (micro second) */
	unsigned int inactivity_to;
	/** minimum awake period (micro second) */
	unsigned int min_awake;
	/** maximum awake period (micro second) */
	unsigned int max_awake;
} inact_sleep_param;

typedef struct uap_power_save_params {
	uint16_t flags;
	uint16_t ps_mode;
	ps_sleep_param sleep_param;
	inact_sleep_param inact_param;
} uap_ps;

uap_ps uap_ps_params_obj;

enum wlan_uap_ps_mode uap_ps_mode = WLAN_UAP_ACTIVE;
bool ps_sleep_cb_sent;
enum user_request_type {
	/* we append our user-generated events to the wifi interface events and
	 * handle them accordingly */
	CM_STA_USER_REQUEST_CONNECT = WIFI_EVENT_LAST + 1,
	CM_STA_USER_REQUEST_DISCONNECT,
	CM_STA_USER_REQUEST_SCAN,
	CM_STA_USER_REQUEST_PS_ENTER,
	CM_STA_USER_REQUEST_PS_EXIT,
	CM_STA_USER_REQUEST_HEALTHMON,
	CM_STA_USER_REQUEST_LAST,
	/* All the STA related request are above and uAP related requests are
	   below */
	CM_UAP_USER_REQUEST_START,
	CM_UAP_USER_REQUEST_STOP,
	CM_UAP_USER_REQUEST_LAST,
	CM_UAP_USER_REQUEST_PS_ENTER,
	CM_UAP_USER_REQUEST_PS_EXIT,
};

enum cm_sta_state {
	CM_STA_INITIALIZING = 0,
	CM_STA_IDLE,
	CM_STA_SCANNING,
	CM_STA_SCANNING_USER,
	CM_STA_ASSOCIATING,
	CM_STA_ASSOCIATED,
	CM_STA_REQUESTING_ADDRESS,
	CM_STA_OBTAINING_ADDRESS,
	CM_STA_CONNECTED,
	CM_STA_DEEP_SLEEP,
	CM_STA_PDN,
};

enum cm_uap_state {
	CM_UAP_INITIALIZING = 0,
	CM_UAP_CONFIGURED,
	CM_UAP_STARTED,
	CM_UAP_IP_UP,
	CM_UAP_PS,
};

#define WL_ID_WIFI_AWAKE_IEEEPS		"wifi_awake_from_ieeeps"

enum wlan_ieeeps_event {
	IEEEPS_EVENT_ENTER,
	IEEEPS_EVENT_ENABLE,
	IEEEPS_EVENT_ENABLE_DONE,
	IEEEPS_EVENT_AWAKE,
	IEEEPS_EVENT_SLEEP,
	IEEEPS_EVENT_SLP_CFM,
	IEEEPS_EVENT_DISABLE,
	IEEEPS_EVENT_DISABLE_DONE,

};

enum wlan_ieeeps_state {
	IEEEPS_INIT,
	IEEEPS_CONFIGURING,
	IEEEPS_AWAKE ,
	IEEEPS_PRE_SLEEP,
	IEEEPS_SLEEP,
	IEEEPS_PRE_DISABLE,
	IEEEPS_DISABLING
};

enum wlan_deepsleepps_state {
	DEEPSLEEPPS_INIT,
	DEEPSLEEPPS_CONFIGURING,
	DEEPSLEEPPS_AWAKE,
	DEEPSLEEPPS_PRE_SLEEP,
	DEEPSLEEPPS_SLEEP,
	DEEPSLEEPPS_PRE_DISABLE,
	DEEPSLEEPPS_DISABLING
};

enum wlan_deepsleepps_event {
	DEEPSLEEPPS_EVENT_ENTER,
	DEEPSLEEPPS_EVENT_ENABLE,
	DEEPSLEEPPS_EVENT_ENABLE_DONE,
	DEEPSLEEPPS_EVENT_AWAKE,
	DEEPSLEEPPS_EVENT_SLEEP,
	DEEPSLEEPPS_EVENT_SLP_CFM,
	DEEPSLEEPPS_EVENT_DISABLE,
	DEEPSLEEPPS_EVENT_DISABLE_DONE,
};

static struct wifi_scan_params_t g_wifi_scan_params = {
	NULL, NULL, {0,}, BSS_ANY, 60, 153
};

static os_queue_pool_define(g_wlan_event_queue_data,
			    sizeof(struct wifi_message) * MAX_EVENTS);
static os_thread_stack_define(g_cm_stack, 2048);

typedef enum {
	WLCMGR_INACTIVE,
	WLCMGR_INIT_DONE,
	WLCMGR_ACTIVATED,
	WLCMGR_THREAD_STOPPED,
	WLCMGR_THREAD_DELETED,
} wlcmgr_status_t;

static struct {
	/* This lock enables the scan response data to be accessed by multiple
	 * threads with the expectation that any thread accessing the scan lock may
	 * have to sleep until it is available.  The lock is taken by calling
	 * wlan_scan() and it is released by the WLAN Connection Manager main
	 * thread when the scan response data has been handled and
	 * is therefore free for another user.  This lock must never be taken
	 * in the WLAN Connection Manager main thread and it must only be
	 * released by that thread. The lock count must be 0 or 1. */
	os_semaphore_t scan_lock;
	bool is_scan_lock;

	/* The WLAN Connection Manager event queue receives events (command
	 * responses, WiFi events, TCP stack events) from the wifi interface as
	 * well as user requests (connect, disconnect, scan).  This is the main
	 * blocking point for the main thread and acts as the state machine tick.*/
	os_queue_t events;
	os_queue_pool_t events_queue_data;

	/* internal state */
	enum cm_sta_state sta_state;
	enum cm_sta_state sta_return_to;
	enum cm_uap_state uap_state;
	enum cm_uap_state uap_return_to;

	uint8_t mac[6];
#ifdef CONFIG_P2P
	uint8_t wfd_mac[6];
#endif
	/* callbacks */
	int (*cb) (enum wlan_event_reason reason, void *data);
	int (*scan_cb) (unsigned int count);
	void (*event_cb) (int type);

	/* known networks list */
	struct wlan_network networks[WLAN_MAX_KNOWN_NETWORKS];
	unsigned int cur_network_idx;
	unsigned int cur_uap_network_idx;

	unsigned int num_networks;
	unsigned int scan_count;

	unsigned int uap_supported_max_sta_num;

	/* CM thread */
	os_thread_t cm_main_thread;
	os_thread_stack_t cm_stack;
	unsigned running:1;
	unsigned stop_request:1;
	wlcmgr_status_t status;

	/*
	 * Power save state configuration
	 * These are states corresponding to the network that we are currently
	 * connected to. Not relevant, when we are not connected.
	 */
	enum wlan_ps_state cm_ps_state;
	enum wlan_ieeeps_state ieeeps_state;
	enum wlan_deepsleepps_state deepsleepps_state;
	bool skip_ds_exit_cb:1;
	bool cm_ieeeps_configured:1;
	bool cm_deepsleepps_configured:1;
	bool cm_pdnps_configured:1;
	bool is_user_req_to_disable:1;
	bool dhcp_wakelock_taken:1;
	unsigned int wakeup_conditions;
	wifi_fw_version_ext_t fw_ver_ext;

#ifdef CONFIG_WLAN_FAST_PATH
	/* Data structures for fast path connection */
	bool auth_cache_valid:1;
	bool fast_path_cache_valid:1;
	uint8_t fast_path_bss[MLAN_MAC_ADDR_LENGTH];
	unsigned int fp_network;
#endif /* CONFIG_WLAN_FAST_PATH */
	int uap_rsn_ie_index;
} wlan;

static void wlan_power_down_card();
static void wlan_power_up_card();

static void wlan_ps_cb(power_save_event_t event, void *data);


void wlan_wake_up_card();
#ifdef CONFIG_WLCMGR_DEBUG
static char *dbg_sta_state_name(enum cm_sta_state state)
{
	switch (state) {
	case CM_STA_INITIALIZING:
		return "initializing";
	case CM_STA_IDLE:
		return "idle";
	case CM_STA_SCANNING:
		return "scanning";
	case CM_STA_SCANNING_USER:
		return "user scanning";
	case CM_STA_ASSOCIATING:
		return "associating";
	case CM_STA_ASSOCIATED:
		return "associated";
	case CM_STA_REQUESTING_ADDRESS:
		return "requesting address";
	case CM_STA_OBTAINING_ADDRESS:
		return "obtaining address";
	case CM_STA_CONNECTED:
		return "connected";
	case CM_STA_DEEP_SLEEP:
		return "deep sleep";
	case CM_STA_PDN:
		return "Power Down";
	default:
		return "unknown";
	}
}

static char *dbg_uap_state_name(enum cm_uap_state state)
{
	switch (state) {
	case CM_UAP_INITIALIZING:
		return "initializing";
	case CM_UAP_CONFIGURED:
		return "configured";
	case CM_UAP_STARTED:
		return "started";
	case CM_UAP_IP_UP:
		return "IP configured";
	default:
		return "unknown";
	}
}

static void dbg_lock_info(void)
{
#if 0
	int ret;
	char *name;
	unsigned long cur_val, suspended_count;
	TX_THREAD *first_suspended;
	TX_SEMAPHORE *next;

	ret = tx_semaphore_info_get(&wlan.scan_lock, &name, &cur_val,
				    &first_suspended, &suspended_count, &next);
	if (ret) {
		wlcm_e("unable to fetch scan lock info");
		return;
	}

	wlcm_d("scan lock info: count=%ld, suspended=%ld", cur_val,
	       suspended_count);
#endif
}
#else
#define dbg_lock_info(...)
#define dbg_sta_state_name(...)
#define dbg_uap_state_name(...)
#endif /* CONFIG_WLCMGR_DEBUG */

/*
 * Utility Functions
 */

int verify_scan_duration_value(int scan_duration)
{
	if (scan_duration >= 50 && scan_duration <= 500)
		return 0;
	return -1;
}

int verify_scan_channel_value(int channel)
{
	if (channel >= 0 && channel <= 11)
		return 0;
	return -1;
}

int verify_split_scan_delay(int delay)
{
	if (delay >= 30 && delay <= 300)
		return 0;
	return -1;
}

int set_scan_params(struct wifi_scan_params_t *wifi_scan_params)
{
	if (!verify_scan_duration_value(wifi_scan_params->scan_duration))
		g_wifi_scan_params.scan_duration =
		    wifi_scan_params->scan_duration;
	if (!verify_scan_channel_value(wifi_scan_params->channel[0]))
		g_wifi_scan_params.channel[0] = wifi_scan_params->channel[0];
	if (!verify_split_scan_delay(wifi_scan_params->split_scan_delay))
		g_wifi_scan_params.split_scan_delay =
		    wifi_scan_params->split_scan_delay;
	return 0;
}

int get_scan_params(struct wifi_scan_params_t *wifi_scan_params)
{
	wifi_scan_params->scan_duration = g_wifi_scan_params.scan_duration;
	wifi_scan_params->channel[0] = g_wifi_scan_params.channel[0];
	wifi_scan_params->split_scan_delay =
	    g_wifi_scan_params.split_scan_delay;
	return 0;
}

static uint32_t wlan_map_to_wifi_wakeup_condtions(const uint32_t
						   wlan_wakeup_condtions)
{
	uint32_t conditions = 0;
	if (wlan_wakeup_condtions & WAKE_ON_UNICAST)
		conditions |= WIFI_WAKE_ON_UNICAST;

	if (wlan_wakeup_condtions & WAKE_ON_ALL_BROADCAST)
		conditions |= WIFI_WAKE_ON_ALL_BROADCAST;

	if (wlan_wakeup_condtions & WAKE_ON_MULTICAST)
		conditions |= WIFI_WAKE_ON_MULTICAST;

	if (wlan_wakeup_condtions & WAKE_ON_ARP_BROADCAST)
		conditions |= WIFI_WAKE_ON_ARP_BROADCAST;

	if (wlan_wakeup_condtions & WAKE_ON_MAC_EVENT)
		conditions |= WIFI_WAKE_ON_MAC_EVENT;

	if (wlan_wakeup_condtions & WAKE_ON_MGMT_FRAME)
		conditions |= WIFI_WAKE_ON_MGMT_FRAME;

	return conditions;
}

static int wlan_get_ipv4_addr(unsigned int *ipv4_addr)
{
	struct wlan_network network;
	int ret;
	if (is_uap_started()) {
		ret = wlan_get_current_uap_network(&network);
	} else {
		ret = wlan_get_current_network(&network);
	}
	if (ret != WM_SUCCESS) {
		wlcm_e("cannot get network info");
		*ipv4_addr = 0;
		return -WM_FAIL;
	}
	*ipv4_addr = network.address.ip;
	return ret;
}

static int is_user_scanning(void)
{
	return (wlan.sta_state == CM_STA_SCANNING_USER);
}

static bool is_state(enum cm_sta_state state)
{
	if (is_user_scanning())
		return (wlan.sta_return_to == state);

	return (wlan.sta_state == state);
}

static int wlan_send_host_sleep()
{
	int ret;
	unsigned int ipv4_addr;
	ret = wlan_get_ipv4_addr(&ipv4_addr);
	if (ret != WM_SUCCESS) {
		wlcm_e("HS: cannot get IP");
		return -WM_FAIL;
	}
	/* If uap interface is up
	 * configure host sleep for uap interface
	 * else confiugre host sleep for station
	 * interface.
	 */
	if (is_uap_started()) {
		return wifi_send_hs_cfg_cmd_uap
			(ipv4_addr, HS_CONFIGURE,
			 wlan_map_to_wifi_wakeup_condtions
			 (wlan.wakeup_conditions));
	} else {
		return wifi_send_hs_cfg_cmd_sta
			(ipv4_addr, HS_CONFIGURE,
			 wlan_map_to_wifi_wakeup_condtions
			 (wlan.wakeup_conditions));
	}
}

static void wlan_host_sleep_and_sleep_confirm()
{
	int ret = WM_SUCCESS;
	ret = os_rwlock_write_lock(&ps_rwlock, OS_NO_WAIT);
	if (ret != WM_SUCCESS) {
		/* Couldn't get the semaphore, someone has already taken
		 * it. */
		g_req_sl_confirm = 1;
		return;
	}
	ret = wlan_send_host_sleep();
	if (ret != WM_SUCCESS || !is_state(CM_STA_CONNECTED)) {
		g_req_sl_confirm = 1;
		os_rwlock_write_unlock(&ps_rwlock);
		return;
	}
	/* tbdel */
	wlan.cm_ps_state = PS_STATE_SLEEP_CFM;
	send_sleep_confirm_command();
	g_req_sl_confirm = 0;
}

static void wlan_send_sleep_confirm()
{
       wlan.cm_ps_state = PS_STATE_SLEEP_CFM;
       send_sleep_confirm_command();
}

static void wlan_ieeeps_sm(uint32_t event)
{
	enum wlan_ieeeps_state next_state;
 begin:
	next_state = wlan.ieeeps_state;
	wlcm_d("IEEE PS Event : %d", event);

	switch(wlan.ieeeps_state) {
	case IEEEPS_INIT:
		if (event == IEEEPS_EVENT_ENABLE)
			wifi_enter_ieee_power_save();

		if (event == IEEEPS_EVENT_ENABLE_DONE)
			next_state = IEEEPS_CONFIGURING;

		break;
	case IEEEPS_CONFIGURING:
		if (event == IEEEPS_EVENT_AWAKE) {
			os_rwlock_write_lock(&ps_rwlock, OS_WAIT_FOREVER);
			next_state = IEEEPS_AWAKE;
		}
		if (event == IEEEPS_EVENT_SLEEP) {
			next_state = IEEEPS_PRE_SLEEP;
			wakelock_get(WL_ID_WIFI_AWAKE_IEEEPS);
		}

		break;
	case IEEEPS_AWAKE:
		if (event == IEEEPS_EVENT_ENTER) {
			wakelock_get(WL_ID_WIFI_AWAKE_IEEEPS);
			os_rwlock_write_unlock(&ps_rwlock);
			wlan.cm_ps_state = PS_STATE_AWAKE;
		}

		if (event == IEEEPS_EVENT_SLEEP)
			next_state = IEEEPS_PRE_SLEEP;

		if (event == IEEEPS_EVENT_DISABLE) {
			wakelock_put(WL_ID_WIFI_AWAKE_IEEEPS);
			next_state = IEEEPS_DISABLING;
		}
		break;
	case IEEEPS_PRE_SLEEP:
		if (event == IEEEPS_EVENT_ENTER)
			wlan_host_sleep_and_sleep_confirm();

		if (event == IEEEPS_EVENT_SLEEP)
			wlan_host_sleep_and_sleep_confirm();

		if (event == IEEEPS_EVENT_SLP_CFM)
			next_state = IEEEPS_SLEEP;

		if (event == IEEEPS_EVENT_DISABLE) {
			g_req_sl_confirm = 0;
			next_state = IEEEPS_DISABLING;
		}
		break;
	case IEEEPS_SLEEP:
		if (event == IEEEPS_EVENT_ENTER) {
			g_req_sl_confirm = 0;
			wakelock_put(WL_ID_WIFI_AWAKE_IEEEPS);
		}

		if (event == IEEEPS_EVENT_AWAKE)
			next_state = IEEEPS_AWAKE;

		if (event == IEEEPS_EVENT_SLEEP) {
			/* We already sent the sleep confirm but it appears that
			 * the firmware is still up */
			wakelock_get(WL_ID_WIFI_AWAKE_IEEEPS);
			os_rwlock_write_unlock(&ps_rwlock);
			next_state = IEEEPS_PRE_SLEEP;
		}

		if (event == IEEEPS_EVENT_DISABLE) {
			if (is_state(CM_STA_CONNECTED))
				next_state = IEEEPS_PRE_DISABLE;
			else
				next_state = IEEEPS_DISABLING;
			os_rwlock_write_unlock(&ps_rwlock);
		}
		break;
	case IEEEPS_PRE_DISABLE:
		if (event == IEEEPS_EVENT_ENTER)
			wlan_wake_up_card();

		if (event == IEEEPS_EVENT_AWAKE)
			next_state = IEEEPS_DISABLING;

		break;
	case IEEEPS_DISABLING:
		if (event == IEEEPS_EVENT_ENTER)
			wifi_exit_ieee_power_save();

		if (event == IEEEPS_EVENT_DISABLE_DONE)
			next_state = IEEEPS_INIT;

		break;

	} /* end of switch  */

	/* state change detected
	 * call the same function with event ENTER*/
	if (wlan.ieeeps_state != next_state) {
		wlcm_d("IEEE PS: %d ---> %d", wlan.ieeeps_state,
		       next_state);
		wlan.ieeeps_state = next_state;
		event = IEEEPS_EVENT_ENTER;
		goto begin;
	}
}

static void wlan_deepsleepps_sm(uint32_t event)
{
	enum wlan_deepsleepps_state next_state;

 begin:
	next_state = wlan.deepsleepps_state;
	wlcm_d("Deep Sleep Event : %d", event);

	switch(wlan.deepsleepps_state) {
	case DEEPSLEEPPS_INIT:
		if (event == DEEPSLEEPPS_EVENT_ENABLE) {
			wakelock_get(WL_ID_DEEPSLEEP_SM);
			wifi_enter_deepsleep_power_save();
		}
		if (event == DEEPSLEEPPS_EVENT_ENABLE_DONE)
			next_state = DEEPSLEEPPS_CONFIGURING;
		break;
	case DEEPSLEEPPS_CONFIGURING:
		if (event == DEEPSLEEPPS_EVENT_SLEEP)
			next_state = DEEPSLEEPPS_PRE_SLEEP;

		break;
	case DEEPSLEEPPS_AWAKE:
		if (event == DEEPSLEEPPS_EVENT_ENTER)
			wlan.cm_ps_state = PS_STATE_AWAKE;

		if (event == DEEPSLEEPPS_EVENT_SLEEP)
			next_state = DEEPSLEEPPS_PRE_SLEEP;
		break;
	case DEEPSLEEPPS_PRE_SLEEP:
		if (event == DEEPSLEEPPS_EVENT_ENTER)
			wlan_send_sleep_confirm();

		if (event == DEEPSLEEPPS_EVENT_SLP_CFM) {
			wakelock_put(WL_ID_DEEPSLEEP_SM);
			next_state = DEEPSLEEPPS_SLEEP;
		}
		break;
	case DEEPSLEEPPS_SLEEP:
		if (event == DEEPSLEEPPS_EVENT_AWAKE)
			next_state = DEEPSLEEPPS_AWAKE;

		if (event == DEEPSLEEPPS_EVENT_DISABLE)
			next_state = DEEPSLEEPPS_PRE_DISABLE;

		break;
	case DEEPSLEEPPS_PRE_DISABLE:
		if (event == DEEPSLEEPPS_EVENT_ENTER) {
			wakelock_get(WL_ID_DEEPSLEEP_SM);
			wlan_wake_up_card();
		}

		if (event == DEEPSLEEPPS_EVENT_AWAKE)
			next_state = DEEPSLEEPPS_DISABLING;

		break;
	case DEEPSLEEPPS_DISABLING:
		if (event == DEEPSLEEPPS_EVENT_ENTER)
			wifi_exit_deepsleep_power_save();

		if (event == DEEPSLEEPPS_EVENT_DISABLE_DONE) {
			wakelock_put(WL_ID_DEEPSLEEP_SM);
			next_state = DEEPSLEEPPS_INIT;
		}

		break;

	} /* end of switch  */

	/* state change detected
	 * call the same function with event ENTER*/
	if (wlan.deepsleepps_state != next_state) {
		wlcm_d("Deep Sleep: %d ---> %d", wlan.deepsleepps_state,
		       next_state);
		wlan.deepsleepps_state = next_state;
		event = DEEPSLEEPPS_EVENT_ENTER;
		goto begin;
	}
}

static int is_bssid_any(char *b)
{
	return (!b[0] && !b[1] && !b[2] && !b[3] && !b[4] && !b[5]);
}

/* Check to see if the security features of our network, 'config', match with
 * those of a scan result, 'res' and return 1 if they do, 0 if they do not. */
static int security_profile_matches(const struct wlan_network_security *config,
				    const struct wifi_scan_result *res)
{
	/* No security: just check that the scan result doesn't specify security */
	if (config->type == WLAN_SECURITY_NONE) {
#ifdef CONFIG_WPS2
		if (res->wps_session != WPS_SESSION_INACTIVE)
			return 1;
#endif
		if (res->WPA_WPA2_WEP.wepStatic || res->WPA_WPA2_WEP.wpa2 ||
		    res->WPA_WPA2_WEP.wpa)
			return 0;
		return 1;
	}

	/* WEP mode: if we are using WEP, the AP must use static WEP */
	if (config->type == WLAN_SECURITY_WEP_OPEN ||
	    config->type == WLAN_SECURITY_WEP_SHARED)
		return res->WPA_WPA2_WEP.wepStatic;

	/* WPA/WPA2 mode: if we are using WPA/WPA2, the AP must use WPA/WPA2 */
	if (config->type == WLAN_SECURITY_WPA_WPA2_MIXED)
		return res->WPA_WPA2_WEP.wpa | res->WPA_WPA2_WEP.wpa2;

	/* WPA2 mode: if we are using WPA2, the AP must use WPA2 */
	if (config->type == WLAN_SECURITY_WPA2)
		return res->WPA_WPA2_WEP.wpa2;

	/* WPA mode: if we are using WPA, the AP must use WPA */
	if (config->type == WLAN_SECURITY_WPA)
		return res->WPA_WPA2_WEP.wpa;

#ifdef CONFIG_WPA2_ENTP
	/* WPA2 Enterprise mode: if we are using WPA2 Enterprise,
	 * the AP must use WPA2 Enterpise */
	if (config->type == WLAN_SECURITY_EAP_TLS)
		return res->wpa2_entp_IE_exist;
#endif

	return 0;
}

/* Check if 'network' matches the scan result 'res' based on network properties
 * and the security profile.  If we found a match, update 'network' with any
 * new information discovered when parsing 'res'. 192 We may update the channel,
 * BSSID, or SSID but not the security profile. */
static int network_matches_scan_result(const struct wlan_network *network,
				       const struct wifi_scan_result *res)
{
	/* Check basic network information that we know */
	if (network->channel_specific && network->channel != res->Channel)
		return 0;
	if (network->bssid_specific && memcmp(network->bssid, res->bssid, 6))
		return 0;
	if (network->ssid_specific) {
		if ((!res->ssid_len) ||
		(strncmp(network->ssid, (char *)res->ssid, res->ssid_len)))
			return 0;
	}

	/* In case of CONFIG_WD_EXTERAL we don't parse and
	   store the security profile.
	   Instead the respective IE are stored in network struct as is */
	/* Check security information */
	if (network->security_specific) {
		if (!security_profile_matches(&network->security, res))
			return 0;
	}

	return 1;
}

/* fixme: move this to header file */
int wifi_send_clear_wpa_psk(int mode, const char *ssid);
int wifi_send_add_wpa_psk(int mode, char *ssid,
			  char *passphrase, unsigned int len);
int wifi_send_get_wpa_pmk(int mode, char *ssid);
int wifi_send_add_wpa_pmk(int mode, char *ssid, char *pmk, unsigned int len);
int wifi_send_disable_supplicant(int mode);

uint8_t rsn_ie[] = {0x01, 0x00, 0x00, 0x0f, 0xac, 0x04, 0x01, 0x00, 0x00,
			0x0f, 0xac, 0x04, 0x01, 0x00,
			0x00, 0x0f, 0xac, 0x02, 0x0c, 0x00};

/* Configure the firmware and PSK Supplicant for the security settings
 * specified in 'network'.  For WPA and WPA2 networks, we must chose between
 * the older TKIP cipher or the newer CCMP cipher.  We prefer CCMP, however we
 * will chose TKIP if the AP doesn't report CCMP support.  CCMP is optional for
 * WPA and required for WPA2, however a WPA2 AP may still have it disabled. */
static int configure_security(struct wlan_network *network,
			      struct wifi_scan_result *res)
{
	int ret;
	switch (network->security.type) {
#ifdef CONFIG_WPA2_ENTP
	case WLAN_SECURITY_EAP_TLS:
		wps_session_attempt = 1;
		break;
#endif
	case WLAN_SECURITY_WPA:
	case WLAN_SECURITY_WPA2:
	case WLAN_SECURITY_WPA_WPA2_MIXED:
#ifdef CONFIG_WPS2
		wps_session_attempt = 0;
#endif
		if (network->security.type == WLAN_SECURITY_WPA) {
			wlcm_d("configuring WPA security");
		} else if (network->security.type == WLAN_SECURITY_WPA2) {
			wlcm_d("configuring WPA2 security");
		} else {
			wlcm_d("configuring WPA/WPA2 Mixed security");
		}

		wlcm_d("adding SSID and PSK to supplicant cache");
		/* the handler for the "add PSK" command calls supplicantEnable() so we
		 * don't do it here
		 *
		 * TODO: at this time, the firmware will not generate a specific
		 * response to the command sent by wifi_add_wpa_psk().  The
		 * command+response sequence should be worked into the WLAN Connection
		 * Manager once this is fixed.
		 *
		 * TODO: at this time, the PSK Supplicant caches passphrases on the
		 * SSID however this won't work for networks with a hidden SSID.  The
		 * WLAN Connection manager will need to be updated once that's
		 * resolved. */
#ifdef CONFIG_HOST_SUPP
		if (network->role == WLAN_BSS_ROLE_STA) {
			supplicant_send_cmd(SUPP_STA_ENABLE, NULL,
				network->security.type,
				&network->security.mcstCipher,
				&network->security.ucstCipher,
				NULL, NULL, wlan.mac);

			supplicant_send_cmd(SUPP_STA_SET_PASSPHRASE, NULL, 0,
				NULL, NULL, network->security.psk,
				NULL, wlan.mac);

			supplicant_send_cmd(SUPP_STA_SESS_INIT, network->ssid,
				0, NULL, NULL, NULL,
				network->bssid, wlan.mac);
		} else if (network->role == WLAN_BSS_ROLE_UAP) {

			supplicant_send_cmd(SUPP_UAP_INIT, NULL, 0, NULL,
				NULL, NULL, NULL, wlan.mac);

			supplicant_send_cmd(SUPP_UAP_SET_PASSPHRASE,
				network->ssid, 0,
				NULL, NULL, network->security.psk,
				NULL, wlan.mac);
		}
		break;
#else
		if (network->security.pmk_valid) {
			ret = wifi_send_add_wpa_pmk(network->role,
						    network->ssid,
						    network->security.pmk,
						    WLAN_PMK_LENGTH);
		} else {
			ret = wifi_send_clear_wpa_psk(network->role,
						      network->ssid);
			if (ret != WM_SUCCESS)
				return WLAN_ERROR_ACTION;
			ret = wifi_send_add_wpa_psk(network->role,
					       network->ssid,
					       network->security.psk,
						    network->security.psk_len);
			if (ret != WM_SUCCESS)
				return WLAN_ERROR_ACTION;
			/* Get the Pairwise Master Key for caching*/
			ret = wifi_send_get_wpa_pmk(network->role,
						    network->ssid);
		}

		if (ret != WM_SUCCESS)
			return WLAN_ERROR_ACTION;
#endif
		break;

	case WLAN_SECURITY_WEP_OPEN:
	case WLAN_SECURITY_WEP_SHARED:
		ret = load_wep_key((const uint8_t *) network->security.psk,
				   (uint8_t *) network->security.psk,
				   (uint8_t *) &network->security.psk_len,
				   sizeof(network->security.psk));
		if (ret != WM_SUCCESS)
			return WLAN_ERROR_PARAM;

		ret = wifi_set_key(BSS_TYPE_STA, true, 0,
				(const uint8_t *)network->security.psk,
				network->security.psk_len,
				(const uint8_t *)network->bssid);

		if (ret != WM_SUCCESS)
			return WLAN_ERROR_ACTION;
		break;

	case WLAN_SECURITY_NONE:
#ifdef CONFIG_WPS2
		if (network->wps_specific &&
			res->wps_session != WPS_SESSION_INACTIVE)
			wps_session_attempt = 1;
#endif
	default:
		break;
	}

	return WLAN_ERROR_NONE;
}

static int is_running(void)
{
	return (wlan.running && wlan.sta_state >= CM_STA_IDLE);
}

static int is_uap_state(enum cm_uap_state state)
{
	return (wlan.uap_state == state);
}

static int is_sta_connecting(void)
{
	return ((wlan.sta_state > CM_STA_ASSOCIATING) &&
		(wlan.sta_state <= CM_STA_CONNECTED));
}

/* Check whether we are allowed to start a user-requested scan right now. */
static bool is_scanning_allowed(void)
{
	if (wlan.cm_ieeeps_configured || wlan.cm_deepsleepps_configured)
		return false;
	if (uap_ps_mode != WLAN_UAP_ACTIVE)
		return false;
	return (is_state(CM_STA_IDLE) || is_state(CM_STA_CONNECTED));
}

/*
 * Connection Manager actions
 */
static void do_scan(struct wlan_network *network)
{
	struct wifi_message msg;
	int ret;
	uint8_t *bssid = NULL;
	char *ssid = NULL;
	int channel = 0;
	IEEEtypes_Bss_t type;

	wlcm_d("initiating scan for network \"%s\"", network->name);

	if (network->bssid_specific)
		bssid = (uint8_t *) network->bssid;
	if (network->ssid_specific)
		ssid = network->ssid;
	if (network->channel_specific)
		channel = network->channel;

	switch (network->role) {
	default:
	case WLAN_BSS_ROLE_STA:
		type = BSS_INFRASTRUCTURE;
		break;
	}

	wlan.sta_state = CM_STA_SCANNING;

#ifdef CONFIG_11D
	if (wlan.scan_count < WLAN_11D_SCAN_LIMIT) {
		ret = wifi_send_scan_cmd(g_wifi_scan_params.bss_type,
				g_wifi_scan_params.bssid,
				g_wifi_scan_params.ssid,
				g_wifi_scan_params.channel[0],
				g_wifi_scan_params.scan_duration,
				false);
	} else
#endif
		ret = wifi_send_scan_cmd(type, bssid, ssid, channel,
				120, false);
	if (ret) {
		msg.event = WIFI_EVENT_SCAN_RESULT;
		msg.reason = WIFI_EVENT_REASON_FAILURE;
		msg.data = 0;

		wlcm_e("error: scan failed");
		os_queue_send(&wlan.events, &msg, OS_NO_WAIT);
	} else
		wlan.scan_count++;
}

static void do_connect_failed(enum wlan_event_reason reason);

/* Start a connection attempt.  To do this we choose a specific network to scan
 * for or the first of our list of known networks. If that network uses WEP
 * security, we first issue the WEP configuration command and enter the
 * CM_STA_CONFIGURING state, we'll get a response to this command which will
 * transition us to the CM_STA_SCANNING state.  Otherwise we issue a scan and
 * enter the CM_STA_SCANNING state now. */
static int do_connect(int netindex)
{
#ifdef CONFIG_WLAN_FAST_PATH
	int ret;
#endif /* CONFIG_WLAN_FAST_PATH */

	g_wm_stats.wm_conn_att++;

	/* try the specified network */
	if (wlan.networks[netindex].role != WLAN_BSS_ROLE_STA)
		return WLAN_ERROR_PARAM;

	wlan.cur_network_idx = netindex;
	wlan.scan_count = 0;

#ifdef CONFIG_WLAN_FAST_PATH
	if (((wlan.auth_cache_valid == true) ||
		(wlan.fast_path_cache_valid == true))
	    && (wlan.cur_network_idx == wlan.fp_network)) {
		wlcm_d("Fast Path: Taking fast path");

#ifdef CONFIG_HOST_SUPP
		ret = configure_security(&wlan.networks[wlan.cur_network_idx],
				NULL);
		if (ret) {
			wlcm_w("setting security params failed");
			return WLAN_ERROR_PARAM;
		}
#endif
		/* Fast path: Jump directly to associating state */
		wlan.sta_state = CM_STA_ASSOCIATING;
		ret = wrapper_wifi_assoc(wlan.fast_path_bss,
					   wlan.networks[wlan.cur_network_idx].
					   security.type);
		if (ret == WM_SUCCESS)
			return WLAN_ERROR_NONE;

		wlcm_w("Fast path assoc failed. Will try normal assoc");
		wlan.auth_cache_valid = false;
		wlan.fast_path_cache_valid = false;
	}

#endif /* CONFIG_WLAN_FAST_PATH */
	do_scan(&wlan.networks[netindex]);

	return WLAN_ERROR_NONE;
}

static int do_start(struct wlan_network *network)
{
	int ret;

	if (network->role == WLAN_BSS_ROLE_UAP) {

		if (!network->channel_specific)
			network->channel = UAP_DEFAULT_CHANNEL;

		wlan.uap_state = CM_UAP_CONFIGURED;

#ifdef CONFIG_HOST_SUPP
		if (network->security.type == WLAN_SECURITY_WPA2) {
			wlan.uap_rsn_ie_index = wlan_set_mgmt_ie(network->type,
					MGMT_RSN_IE, rsn_ie,
					sizeof(rsn_ie));
		}
#endif

		wlcm_d("starting our own network");
		ret = configure_security(network, NULL);

		if (ret) {
			wlcm_e("setting security params failed");
			CONNECTION_EVENT(WLAN_REASON_UAP_START_FAILED, NULL);
			return WLAN_ERROR_ACTION;
		}

		ret = wifi_uap_start(network->type, network->ssid,
				     wlan.mac,
				     network->security.type,
				     &network->security.psk[0],
				     network->channel);
		if (ret) {
			wlcm_e("uAP start failed, giving up");
			CONNECTION_EVENT(WLAN_REASON_UAP_START_FAILED, NULL);
			return WLAN_ERROR_ACTION;
		}
	}
	return WLAN_ERROR_NONE;
}

static int do_stop(struct wlan_network *network)
{
	int ret = WLAN_ERROR_NONE;

	wlcm_d("stopping our own network");

	if (network->role == WLAN_BSS_ROLE_UAP) {
#ifdef CONFIG_WPA2_ENTP
		ret = wifi_send_disable_supplicant(network->role);
#endif
#ifdef CONFIG_HOST_SUPP
		if (network->security.type == WLAN_SECURITY_WPA2) {
			wlan_clear_mgmt_ie(WLAN_BSS_TYPE_UAP,
					wlan.uap_rsn_ie_index);
			supplicant_send_cmd(SUPP_UAP_DEINIT, NULL, 0, NULL,
					NULL, NULL, NULL, wlan.mac);
		}
#endif
		ret = wifi_uap_stop(network->type);

		if (ret) {
			wlcm_e("uAP stop failed, giving up");
			CONNECTION_EVENT(WLAN_REASON_UAP_STOP_FAILED, NULL);
			return WLAN_ERROR_ACTION;
		}
		wlan.uap_state = CM_UAP_INITIALIZING;
	}

	return WLAN_ERROR_NONE;
}

/* A connection attempt has failed for 'reason', decide whether to try to
 * connect to another network (in that case, tell the state machine to
 * transition to CM_STA_CONFIGURING to try that network) or finish attempting to
 * connect by releasing the scan lock and informing the user. */
static void do_connect_failed(enum wlan_event_reason reason)
{
	g_wm_stats.wm_conn_fail++;
	switch (reason) {
	case WLAN_REASON_NETWORK_AUTH_FAILED:
		g_wm_stats.wm_auth_fail++;
		break;
	case WLAN_REASON_NETWORK_NOT_FOUND:
		g_wm_stats.wm_nwnt_found++;
		break;
	default:
		break;
	}

	if (wlan.cm_ieeeps_configured) {
		/* disable ieeeps mode*/
		wlan_ieeeps_sm(IEEEPS_EVENT_DISABLE);
	}
#ifdef CONFIG_HOST_SUPP
	supplicant_send_cmd(SUPP_STA_DEAUTH_DATA, NULL, 0, NULL,
			NULL, NULL, NULL, NULL);
#endif
	if (wlan.dhcp_wakelock_taken) {
		wakelock_put(WL_ID_DHCP_CLIENT);
		wlan.dhcp_wakelock_taken = false;
	}

	wlcm_w("connecting to \"%s\" failed",
	       wlan.networks[wlan.cur_network_idx].name);

	CONNECTION_EVENT(reason, NULL);
	wlan.sta_state = CM_STA_IDLE;

	if (wlan.sta_state == CM_STA_SCANNING_USER)
		wlan.sta_return_to = CM_STA_IDLE;
}

static void report_scan_results(void)
{
	unsigned int count;
	if (wlan.scan_cb) {
		if (wifi_get_scan_result_count(&count) != WM_SUCCESS)
			count = 0;
		wlan.scan_cb(count);
		wlan.scan_cb = NULL;
	}
}

static void update_network_params(struct wlan_network *network,
				  const struct wifi_scan_result *res)
{
	if (!network->security_specific) {
		/* Wildcard: If wildcard security is specified, copy the highest
		 * security available in the scan result to the configuration
		 * structure
		 */
		enum wlan_security_type t;
		if (res->WPA_WPA2_WEP.wpa2)
			t = WLAN_SECURITY_WPA2;
		else if (res->WPA_WPA2_WEP.wpa)
			t = WLAN_SECURITY_WPA;
		else if (res->WPA_WPA2_WEP.wepStatic)
			t = WLAN_SECURITY_WEP_OPEN;
		else
			t = 0;
		network->security.type = t;
	}

	/* We have a match based on the criteria we checked, update the known
	 * network with any additional information that we got from the scan but
	 * did not know before */
	if (!network->channel_specific)
		network->channel = res->Channel;
	if (!network->bssid_specific)
		memcpy((void *)network->bssid, res->bssid,
		       MLAN_MAC_ADDR_LENGTH);
	if (!network->ssid_specific)
		memcpy(network->ssid, res->ssid, res->ssid_len);

	memcpy(&network->security.mcstCipher, &res->mcstCipher,
			sizeof(struct wlan_cipher));
	memcpy(&network->security.ucstCipher, &res->ucstCipher,
			sizeof(struct wlan_cipher));

}

static void start_association(struct wlan_network *network,
			      struct wifi_scan_result *res)
{
	wlcm_d("starting association to \"%s\"", network->name);

	int ret = configure_security(network, res);
	if (ret) {
		wlcm_w("setting security params failed");
		do_connect_failed(WLAN_REASON_NETWORK_NOT_FOUND);
		return;
	}

	ret = wrapper_wifi_assoc(res->bssid, network->security.type);
	if (ret) {
		wlcm_w("association failed");
#ifdef CONFIG_WLAN_FAST_PATH
		wlan.auth_cache_valid = false;
		wlan.fast_path_cache_valid = false;
#endif /* CONFIG_WLAN_FAST_PATH */
		do_connect_failed(WLAN_REASON_NETWORK_NOT_FOUND);
		return;
	} else {
#ifdef CONFIG_WLAN_FAST_PATH
		/* cache the scan result for fast path.
		 * It will be marked as valid only on
		 * successful association and authentication */
		wlan.auth_cache_valid = false;
		wlan.fast_path_cache_valid = false;
		memcpy(wlan.fast_path_bss, res->bssid, MLAN_MAC_ADDR_LENGTH);
		wlan.fp_network = wlan.cur_network_idx;
#endif /* CONFIG_WLAN_FAST_PATH */
	}
}

static void handle_scan_results(void)
{
	unsigned int count;
	int ret;
	int i;
	struct wifi_scan_result *res;
	struct wlan_network *network = &wlan.networks[wlan.cur_network_idx];
	bool matching_ap_found = false;

#ifdef CONFIG_11D
	if (wlan.scan_count < WLAN_11D_SCAN_LIMIT) {
		wlcm_d("11D enabled, re-scanning");
		do_scan(network);
		return;
	}
#endif

	/*
	 * We need an allocation here because the lower layer puts all the
	 * results into the same global buffer. Hence, we need a private
	 * copy. fixme: Can be removed after this issue is fixed in the
	 * lower layer.
	 */
	struct wifi_scan_result *best_ap =
		os_mem_alloc(sizeof(struct wifi_scan_result));
	if (!best_ap) {
		wlcm_w("%s: Failed to alloc scan result object", __func__);
		return;
	}

	/* We're associating unless an error occurs, in which case we make a
	 * decision to rescan (transition to CM_STA_SCANNING) or fail
	 * (transition to CM_STA_IDLE or a new connection attempt) */
	wlan.sta_state = CM_STA_ASSOCIATING;

	ret = wifi_get_scan_result_count(&count);
	if (ret)
		count = 0;

	/* 'count' scan results available, is our network there? */
	for (i = 0; i < count; i++) {
		ret = wifi_get_scan_result(i, &res);
		if (ret == WM_SUCCESS &&
			network_matches_scan_result(network, res)) {
			if (!matching_ap_found) {
				/* First matching AP found */
				memcpy(best_ap, res,
				       sizeof(struct wifi_scan_result));
				matching_ap_found = true;
				/*
				 * Continue the search. There may be an AP
				 * with same name but better RSSI.
				 */
				continue;
			}

			if (best_ap->RSSI > res->RSSI) {
				/*
				 * We found a network better that current
				 * best_ap
				 */
				wlcm_d("Found better AP %s on channel %d",
				      res->ssid, res->Channel);
				/* Assign the new found as curr_best */
				memcpy(best_ap, res,
				       sizeof(struct wifi_scan_result));
			}

			/* Continue the search */
		}
	}

	if (matching_ap_found) {
		update_network_params(network, best_ap);
		start_association(network, best_ap);
		os_mem_free(best_ap);
		return;
	}

	os_mem_free(best_ap);

	/* We didn't find our network in the scan results set: rescan if we
	 * have rescan attempts remaining, otherwise give up.
	 */
	if (wlan.scan_count < WLAN_RESCAN_LIMIT) {
		wlcm_d("network not found, re-scanning");
		do_scan(network);
	} else {
		wlcm_d("rescan limit exceeded, giving up");
		do_connect_failed(WLAN_REASON_NETWORK_NOT_FOUND);
	}
}

static void wlan_disable_power_save(int action)
{
	switch (action) {
	case WLAN_DEEP_SLEEP:
		wlcm_d("stopping deep sleep ps mode");
		wlan_deepsleepps_sm(DEEPSLEEPPS_EVENT_DISABLE);
		break;
	case WLAN_IEEE:
		wlcm_d("stopping IEEE ps mode");
		wlan_ieeeps_sm(IEEEPS_EVENT_DISABLE);
		break;
	case WLAN_PDN:
		wlcm_d("stopping power down ps mode");
		wlan.cm_pdnps_configured = false;
		/* Exit power down mode */
		wlan_power_up_card();
		wmprintf("Disabled Power Down mode\n\r");
		while (wlan_init(NULL) != WM_SUCCESS)
			;
		int i = 0;
		for (i = 0; i < (board_cpu_freq() / 32); i++)
			;
		CONNECTION_EVENT(WLAN_REASON_PS_EXIT, (void *)action);
		break;
	}
}

static void wlan_enable_power_save(int action)
{
	switch (action) {
	case WLAN_DEEP_SLEEP:
		if (!is_state(CM_STA_IDLE)) {
			wlcm_d("entering deep sleep mode");
			break;
		}
		wlcm_d("starting deep sleep ps mode");
		wlan.cm_deepsleepps_configured = true;
		/* Enter Deep Sleep power save mode */
		wlan_deepsleepps_sm(DEEPSLEEPPS_EVENT_ENABLE);
		break;
	case WLAN_IEEE:
		wlcm_d("starting IEEE ps mode");
		wlan.cm_ieeeps_configured = true;
		/* Enter IEEE power save mode
		 * via the state machine
		 */
		wlan_ieeeps_sm(IEEEPS_EVENT_ENABLE);
		break;
	case WLAN_PDN:
		wlcm_d("starting power down ps mode");
		wlan.cm_pdnps_configured = true;
		/* Enter power down mode */
		wlan_deinit();
		wlan_power_down_card();
		int i = 0;
		for (i = 0; i < (board_cpu_freq() / 32); i++)
			;
		wmprintf(" Enabled Power Down mode\n\r");
		CONNECTION_EVENT(WLAN_REASON_PS_ENTER, (void *)action);
		break;
	}
}

static void wlcm_process_awake_event()
{
	if (wlan.is_user_req_to_disable) {
		if (uap_ps_mode == WLAN_UAP_INACTIVITY_SLEEP) {
			wifi_uap_ps_inactivity_sleep_exit(wlan.networks[
				wlan.cur_uap_network_idx].type);
		}
	}

	/* tbdel */
	wlan.cm_ps_state = PS_STATE_AWAKE;
	wlan_ieeeps_sm(IEEEPS_EVENT_AWAKE);
	wlan_deepsleepps_sm(DEEPSLEEPPS_EVENT_AWAKE);
}

static void wlcm_process_ieeeps_event(struct wifi_message *msg)
{
	uint16_t action = (uint16_t) ((uint32_t)msg->data);
	wlcm_d("got msg data :: %x", action);

	if (msg->reason == WIFI_EVENT_REASON_SUCCESS) {
		if (action == EN_AUTO_PS) {
			wlan_ieeeps_sm(IEEEPS_EVENT_ENABLE_DONE);
		} else if (action == DIS_AUTO_PS) {
			wlan.cm_ieeeps_configured = false;
			wlan.cm_ps_state = PS_STATE_AWAKE;
			ps_sleep_cb_sent = false;
			CONNECTION_EVENT(WLAN_REASON_PS_EXIT,
					 (void *)WLAN_IEEE);
			/* This sends event to state machine
			 * to finally set state to init*/
			wlan_ieeeps_sm(IEEEPS_EVENT_DISABLE_DONE);
		} else if (action == SLEEP_CONFIRM) {
			wlan.cm_ps_state = PS_STATE_SLEEP_CFM;

			wlan_ieeeps_sm(IEEEPS_EVENT_SLP_CFM);

			if (!ps_sleep_cb_sent) {
				CONNECTION_EVENT(WLAN_REASON_PS_ENTER,
						(void *)WLAN_IEEE);
				ps_sleep_cb_sent = true;
			}
		}
	}
}

static void wlcm_process_deepsleep_event(struct wifi_message *msg,
					 enum cm_sta_state *next)
{
	uint16_t action = (uint16_t) ((uint32_t)msg->data);
	wlcm_d("got msg data :: %x", action);

	if (msg->reason == WIFI_EVENT_REASON_SUCCESS) {
		if (action == EN_AUTO_PS) {
			wlan_deepsleepps_sm(DEEPSLEEPPS_EVENT_ENABLE_DONE);
		} else if (action == SLEEP_CONFIRM) {
			wlan.cm_ps_state = PS_STATE_SLEEP_CFM;
			*next = CM_STA_DEEP_SLEEP;

			wlan_deepsleepps_sm(DEEPSLEEPPS_EVENT_SLP_CFM);

			CONNECTION_EVENT(WLAN_REASON_PS_ENTER,
					 (void *)WLAN_DEEP_SLEEP);
		} else if (action == DIS_AUTO_PS) {
			wlan.cm_deepsleepps_configured = false;
			wlan.cm_ps_state = PS_STATE_AWAKE;
			*next = CM_STA_IDLE;
			wlan_deepsleepps_sm(DEEPSLEEPPS_EVENT_DISABLE_DONE);
#ifdef CONFIG_P2P
				/* This call is made to initiate WFD
				 * We are not interested in the response
				 * since mac addr is already populated
				 * in wlan_start
				 */
				wifi_get_wfd_mac_address();
#endif
				wrapper_wlan_cmd_get_hw_spec();
				/* Skip ps-exit event for the first time
				   after waking from PM4+DS. This will ensure
				   that we do not send ps-exit event until
				   wlan-init event has been sent */
				if (wlan.skip_ds_exit_cb)
					wlan.skip_ds_exit_cb = false;
				else
					CONNECTION_EVENT(WLAN_REASON_PS_EXIT,
							 (void *)
							 WLAN_DEEP_SLEEP);
		}
	}
}

#define WL_ID_STA_DISCONN       "sta_disconnected"

/* fixme: duplicated from legacy. Needs to be removed later. */
#define IEEEtypes_REASON_MIC_FAILURE              14
#define IEEEtypes_REASON_4WAY_HANDSHK_TIMEOUT     15
#define WPA2_ENTERPRISE_FAILED			  0xFF

static void wlcm_process_scan_result_event(struct wifi_message *msg,
					   enum cm_sta_state *next)
{
	if (wlan.sta_state == CM_STA_SCANNING_USER) {
		if (msg->reason == WIFI_EVENT_REASON_SUCCESS) {
			wifi_scan_process_results();
		}
		report_scan_results();
		*next = wlan.sta_return_to;
		wlcm_d("SM: returned to %s", dbg_sta_state_name(*next));
		wlcm_d("releasing scan lock (user scan)");
		os_semaphore_put(&wlan.scan_lock);
		wlan.is_scan_lock = 0;
		dbg_lock_info();
	} else if (wlan.sta_state == CM_STA_SCANNING) {
		if (msg->reason == WIFI_EVENT_REASON_SUCCESS) {
			wifi_scan_process_results();
		}
		handle_scan_results();
		*next = wlan.sta_state;
	} else {
		wlcm_d("ignoring scan result event");
		if (wlan.is_scan_lock) {
			wlcm_d("releasing scan lock (user scan)");
			os_semaphore_put(&wlan.scan_lock);
			dbg_lock_info();
			wlan.is_scan_lock = 0;
		}
	}
}

static void wlcm_process_sta_addr_config_event(struct wifi_message *msg,
					       enum cm_sta_state *next,
					       struct wlan_network *network)
{
	/* We have a response to our configuration request from the TCP stack.
	 * If we configured the stack with a static IP address and this was
	 * successful, we are effectively connected so we can report success
	 * and proceed to CM_STA_CONNECTED.  If we configured the stack with
	 * DHCP and the response is successful then the TCP stack is trying to
	 * obtain an address for us and we should proceed to the
	 * CM_STA_OBTAINING_ADDRESS state and wait for the
	 * WIFI_EVENT_DHCP_CONFIG event,
	 * which will tell us whether or not the DHCP address was
	 * obtained.  Otherwise, if we got an unsuccessful response then we
	 * must report a connection error.
	 *
	 * This event is only relevant if we are connecting and have requested
	 * a TCP stack configuration (that is, we are in the
	 * CM_STA_REQUESTING_ADDRESS state).  Otherwise, we ignore it. */

	if (!is_state(CM_STA_REQUESTING_ADDRESS)) {
		wlcm_w("ignoring TCP configure response");
		return;
	}
	if (msg->reason != WIFI_EVENT_REASON_SUCCESS) {
		do_connect_failed(WLAN_REASON_ADDRESS_FAILED);
		*next = wlan.sta_state;
		return;
	}
	void *if_handle = NULL;
	switch (network->address.addr_type) {
	case ADDR_TYPE_STATIC:
		g_wm_stats.wm_addr_type = 0;
		net_configure_dns(&network->address, network->role);
		if (network->type == WLAN_BSS_TYPE_STA)
			if_handle = net_get_mlan_handle();
#ifdef CONFIG_P2P
		else if (network->type == WLAN_BSS_TYPE_WIFIDIRECT)
			if_handle = net_get_wfd_handle();
#endif /* CONFIG_P2P */
		net_get_if_addr(&network->address, if_handle);
		wlan.sta_state = CM_STA_CONNECTED;
		*next = CM_STA_CONNECTED;
#ifdef CONFIG_P2P
		if (network->security.type != WLAN_SECURITY_NONE)
#endif
			CONNECTION_EVENT(WLAN_REASON_SUCCESS, NULL);
#ifdef CONFIG_P2P
		wifi_wfd_event(false, (void *)1);
#endif
		break;
	case ADDR_TYPE_DHCP:
	case ADDR_TYPE_LLA:
		wakelock_get(WL_ID_DHCP_CLIENT);
		wlan.dhcp_wakelock_taken = true;
		g_wm_stats.wm_addr_type = 1;
		*next = CM_STA_OBTAINING_ADDRESS;
		break;
	default:
		break;
	} /* end of switch */
}

#if 0
static void wlcm_process_supplicant_pmk_event(struct wifi_message *msg,
					      enum cm_sta_state *next)
{

	/* When we do have data, it's the entire response from the firmware.
	 * That is, it's a host_MsgHdr_t, followed by a
	 * host_SupplicantPMKCmd_t.  All we really want is the PMK, but that's
	 * buried in the TLVs of the host_SupplicantPMKCmd_t.  And the only way
	 * that we know the number of TLVs is from the Size element of the
	 * host_MsgHdr_t.  Why not just pass the PMK up from the lower layer?
	 * Because we also need the SSID so we can look up the proper place to
	 * store the PMK.
	 */
	host_MsgHdr_t *hdr = (host_MsgHdr_t *) msg->data;
	host_SupplicantPMKCmd_t *cmd = (host_SupplicantPMKCmd_t *)
		((uint8_t *) msg->data + sizeof(host_MsgHdr_t));
	MrvlIEGeneric_t *tlv =	(MrvlIEGeneric_t *) cmd->TlvBuffer;
	char *ssid = NULL;
	int ssid_len = 0, i;
	char *pmk = NULL;

	if (cmd->Action != host_ACT_GET)
		break;

	/* fixme: Since we are not duplicating the command
	   resp. buffer, at this point in the code we have
	   pointer to command buffer which can be
	   overwritten by the other command response in the
	   meantime we get to this point. We have seen this
	   happen and thus disabling this code for
	   now. This will not affect existing functionality
	   because we are not doing it right anyway. This
	   can be fixed when handing is transferred to
	   mlan. */
	while ((uint8_t *) tlv < (uint8_t *) msg->data + hdr->Size) {
		switch (tlv->hdr.Type) {
		case MRVL_CUSTOM_BSSID_LIST:
		case MRVL_WPA_PASSPHRASE_TLV_ID:
			continue;
		case MRVL_SSID_TLV_ID:
			ssid = (char *)tlv->Value;
			ssid_len = tlv->hdr.Length;
			break;
		case MRVL_SUPPLICANT_PMK_TLV_ID:
			pmk = (char *)tlv->Value;
			break;
		}
		tlv = (MrvlIEGeneric_t *) ((uint8_t *) tlv +
					   sizeof(tlv->hdr) +
					   tlv->hdr.Length);
	}
	if (ssid && pmk) {
		for (i = 0; i < WLAN_MAX_KNOWN_NETWORKS; i++) {
			struct wlan_network *n = &wlan.networks[i];
			if ((memcmp(n->ssid, ssid, ssid_len) ==
			     0) && (memcmp(pmk, INVALID_PMK,
					   WLAN_PMK_LENGTH))) {
				memcpy(n->security.pmk, pmk,
				       WLAN_PMK_LENGTH);
				n->security.pmk_valid = 1;
			}
		}
	}
}
#endif /* 0 */

#ifdef CONFIG_5GHz_SUPPORT
static void wlcm_process_channel_switch(enum cm_sta_state *next,
					struct wlan_network *network)
{
	if (is_state(CM_STA_CONNECTED)) {
		wlcm_w("Sending deauth because of channel switch");
		set_event_chanswann();
		wifi_deauthenticate((uint8_t *)network->bssid);
		/*
		  This function call is already present in
		  wlan_11h_handle_event_chanswann(). Remove it from here when
		  that function is used directly.
		*/
		wrapper_wlan_11d_clear_parsedtable();
		wrapper_clear_media_connected_event();
#ifdef CONFIG_WLAN_FAST_PATH
		/*
		 * Flush fast path cache. We do not want fast path code to
		 * push out assoc requests on the same channel on which
		 * radar interference was detected.
		 */
		wlan.auth_cache_valid = false;
		wlan.fast_path_cache_valid = false;
#endif /* CONFIG_WLAN_FAST_PATH */
		clear_event_chanswann();
		wlan.sta_state = CM_STA_IDLE;
		*next = CM_STA_IDLE;
		CONNECTION_EVENT(WLAN_REASON_CHAN_SWITCH, NULL);
	}
}
#endif  /* CONFIG_5GHz_SUPPORT */

static void wlcm_process_hs_config_event()
{
	/* host sleep config done event received */
	int ret = WM_SUCCESS;
	unsigned int ipv4_addr;

	ret = wlan_get_ipv4_addr(&ipv4_addr);
	if (ret != WM_SUCCESS) {
		wlcm_e("HS : Cannot get IP");
		return;
	}
	/* If uap interface is up
	 * configure host sleep for uap interface
	 * else confiugre host sleep for station
	 * interface.
	 */
	if (is_uap_started()) {
		wifi_send_hs_cfg_cmd_uap(ipv4_addr, HS_ACTIVATE, 0);
	} else {
		wifi_send_hs_cfg_cmd_sta(ipv4_addr, HS_ACTIVATE, 0);
	}
}

static void wlcm_process_association_event(struct wifi_message *msg,
					   enum cm_sta_state *next)
{
#ifdef CONFIG_WPA2_ENTP
	int ret;
	struct wlan_network *network = &wlan.networks[wlan.cur_network_idx];
#endif

	/* We have received a response to the association command.  We may now
	 * proceed to authenticating if it was successful, otherwise this
	 * connection attempt has failed.
	 *
	 * This event is only relevant if we have sent an association command
	 * while connecting (that is, we are in the CM_STA_ASSOCIATING state).
	 * Otherwise, it is ignored. */

	if (!is_state(CM_STA_ASSOCIATING)) {
		wlcm_w("ignoring association result event");
		return;
	}
	if (msg->reason == WIFI_EVENT_REASON_SUCCESS) {
		wlan.sta_state = CM_STA_ASSOCIATED;
		*next = CM_STA_ASSOCIATED;
#ifdef CONFIG_WPS2
		if (wps_session_attempt) {
#ifdef CONFIG_WPA2_ENTP
			ret = wpa2_ent_connect(network);
			if (ret != WM_SUCCESS)
				wlcm_e("wpa2_ent_connect failed");
#endif
		}
#endif
		wlan.scan_count = 0;
	} else if (wlan.scan_count < WLAN_RESCAN_LIMIT) {
		wlcm_w("association failed, re-scanning");
		do_scan(&wlan.networks[wlan.cur_network_idx]);
		*next = CM_STA_SCANNING;
	} else {
		do_connect_failed(WLAN_REASON_NETWORK_NOT_FOUND);
		*next = wlan.sta_state;
	}
}

static void wlcm_process_pmk_event(struct wifi_message *msg,
					      enum cm_sta_state *next,
					      struct wlan_network *network)
{
#ifdef CONFIG_HOST_SUPP
	if (msg->data) {
		network->security.pmk_valid = true;
		memcpy(network->security.pmk, msg->data, WLAN_PMK_LENGTH);
		if (network->role == WLAN_BSS_ROLE_STA) {
			supplicant_send_cmd(SUPP_STA_ENABLE, NULL,
				network->security.type,
				&network->security.mcstCipher,
				&network->security.ucstCipher,
				NULL, NULL, wlan.mac);

			supplicant_send_cmd(SUPP_STA_SET_PMK, network->ssid, 0,
				NULL, NULL, network->security.pmk,
				NULL, wlan.mac);

			supplicant_send_cmd(SUPP_STA_SESS_INIT, network->ssid,
				0, NULL, NULL, NULL,
				network->bssid, wlan.mac);
		}
	}
#endif
}

static void wlcm_process_authentication_event(struct wifi_message *msg,
					      enum cm_sta_state *next,
					      struct wlan_network *network)
{
	int ret = 0;
	void *if_handle = NULL;

#ifndef CONFIG_WPA2_ENTP
#ifdef CONFIG_WPS2
	if (wps_session_attempt)
		return;
#endif
#endif

	if (msg->reason == WIFI_EVENT_REASON_SUCCESS) {
		if (!is_state(CM_STA_ASSOCIATED)) {
			wlcm_d("ignoring authentication event");
			return;
		}

		g_wm_stats.wm_conn_succ++;
#ifdef CONFIG_WLAN_FAST_PATH
		if (wlan.fast_path_cache_valid == true
				&& (wlan.cur_network_idx == wlan.fp_network)) {
			if_handle = net_get_mlan_handle();
			net_interface_up(if_handle);
			*next = CM_STA_CONNECTED;
			CONNECTION_EVENT(WLAN_REASON_SUCCESS, NULL);
		} else {
#endif /* CONFIG_WLAN_FAST_PATH */
			if (network->type == WLAN_BSS_TYPE_STA)
				if_handle = net_get_mlan_handle();
#ifdef CONFIG_P2P
			else if (network->type == WLAN_BSS_TYPE_WIFIDIRECT)
				if_handle = net_get_wfd_handle();
#endif /* CONFIG_P2P */
			ret = net_configure_address(&network->address,
					if_handle);
			if (ret) {
				wlcm_e("Configure Address failed");
				do_connect_failed
					(WLAN_REASON_ADDRESS_FAILED);
				*next = wlan.sta_state;
			} else
				*next = CM_STA_REQUESTING_ADDRESS;
#ifdef CONFIG_WLAN_FAST_PATH
			wlan.auth_cache_valid = false;
			/* Mark the fast path cache valid after
			 * successful connection. */
			wlcm_d("Fast Path: validating settings");
			wlan.fast_path_cache_valid = true;
		}
#endif /* CONFIG_WLAN_FAST_PATH */

	} else {
#ifdef CONFIG_WLAN_FAST_PATH
		wlan.auth_cache_valid = true;
		/* Mark the fast path cache invalid. */
		wlan.fast_path_cache_valid = false;
#endif /* CONFIG_WLAN_FAST_PATH */
#ifdef CONFIG_WPA2_ENTP
		wpa2_shutdown();
#endif
		if (is_state(CM_STA_ASSOCIATED))
			wifi_deauthenticate((uint8_t *)network->bssid);
		wlan.sta_state = CM_STA_IDLE;
		do_connect_failed(WLAN_REASON_NETWORK_AUTH_FAILED);
		*next = wlan.sta_state;
#ifdef CONFIG_P2P
		wifi_wfd_event(false, NULL);
#endif
	}
}

static void wlcm_process_link_loss_event(struct wifi_message *msg,
					 enum cm_sta_state *next,
					 struct wlan_network *network)
{
	/* We're being informed about an asynchronous link loss (ex: beacon loss
	 * counter detected that the AP is gone, security failure, etc). If we
	 * were connected, we need to report this event and proceed to idle.
	 * If we were in the middle of a connection attempt, we need to treat
	 * this as a connection attempt failure via do_connect_fail() and
	 * proceed accordingly.
	 */
	g_wm_stats.wm_lloss++;
	if (is_state(CM_STA_IDLE)) {
		wlcm_w("ignoring link loss event in idle state");
		return;
	}
	/* If we were connected and lost the link, we must report that now and
	 * return to CM_STA_IDLE
	 */
	if (is_state(CM_STA_CONNECTED)) {
		if ((int)msg->data == IEEEtypes_REASON_MIC_FAILURE) {
			/* In case of a LINK loss because of bad MIC
			 * failure, directly send a deauth.
			 */
			wlcm_w("Sending deauth because of"
			       " successive bad MIC failures");
			wifi_deauthenticate((uint8_t *)network->bssid);
			*next = CM_STA_IDLE;
		}
		wlan.sta_state = CM_STA_IDLE;

		CONNECTION_EVENT(WLAN_REASON_LINK_LOST, NULL);
		if (is_user_scanning())
			wlan.sta_return_to = CM_STA_IDLE;
		else
			*next = CM_STA_IDLE;

		if (wlan.cm_ieeeps_configured) {
			/* on link loss disable ieeeps mode*/
			wlan_ieeeps_sm(IEEEPS_EVENT_DISABLE);
		}
#ifdef CONFIG_HOST_SUPP
		supplicant_send_cmd(SUPP_STA_DEAUTH_DATA, NULL, 0, NULL,
				NULL, NULL, NULL, NULL);
#endif
	} else {
		/* we were attempting a connection and lost the link,
		 * so treat this as a connection attempt failure
		 */
		switch ((int)msg->data) {
		case IEEEtypes_REASON_4WAY_HANDSHK_TIMEOUT:
			do_connect_failed(WLAN_REASON_NETWORK_AUTH_FAILED);
			break;
		default:
			do_connect_failed(WLAN_REASON_NETWORK_NOT_FOUND);
			break;
		}
		*next = wlan.sta_state;
	}
#ifdef CONFIG_WPA2_ENTP
	wpa2_shutdown();
#endif
#ifdef CONFIG_P2P
	wifi_wfd_event(false, NULL);
#endif
}

static void wlcm_process_disassoc_event(struct wifi_message *msg,
					enum cm_sta_state *next,
					struct wlan_network *network)
{
	/* We're being informed about an asynchronous disassociation from AP
	 * As were in the middle of a connection attempt, we need to treat
	 * this as a connection attempt failure via do_connect_fail() and
	 * proceed accordingly.
	 */
#ifdef CONFIG_WPA2_ENTP
	wpa2_shutdown();
#endif
	do_connect_failed(WLAN_REASON_NETWORK_AUTH_FAILED);
#ifdef CONFIG_P2P
	wifi_wfd_event(false, NULL);
#endif
	*next = CM_STA_IDLE;
	wlan.sta_state = CM_STA_IDLE;

	if (is_user_scanning())
		wlan.sta_return_to = CM_STA_IDLE;
}

static void wlcm_process_deauthentication_event(struct wifi_message *msg,
						enum cm_sta_state *next,
						struct wlan_network *network)
{
#ifdef CONFIG_WPA2_ENTP
	wpa2_shutdown();
#endif
	if (wlan.cm_ieeeps_configured) {
		/* disable ieeeps mode*/
		wlan_ieeeps_sm(IEEEPS_EVENT_DISABLE);
	}

#ifdef CONFIG_HOST_SUPP
	supplicant_send_cmd(SUPP_STA_DEAUTH_DATA, NULL, 0, NULL,
			NULL, NULL, NULL, NULL);
#endif

#ifdef CONFIG_P2P
	wifi_wfd_event(false, NULL);
#endif
}

static void wlcm_process_net_dhcp_config(struct wifi_message *msg,
					 enum cm_sta_state *next,
					 struct wlan_network *network)
{
	/* We have received a response from the embedded DHCP client.  If
	 * successful, we now have a DHCP lease and can update our IP address
	 * information and procceed to the CM_STA_CONNECTED state, otherwise we
	 * failed to obtain a DHCP lease and report that we failed to connect.
	 *
	 * This event is only relevant if we are attempting a connection and
	 * are in the CM_STA_OBTAINING_ADDRESS state (due to configuring the TCP
	 * stack with DHCP), otherwise we ignore it. */

	void *if_handle = NULL;
	wakelock_put(WL_ID_DHCP_CLIENT);
	wlan.dhcp_wakelock_taken = false;
	if (is_state(CM_STA_OBTAINING_ADDRESS)) {
		if (msg->reason != WIFI_EVENT_REASON_SUCCESS) {
			g_wm_stats.wm_dhcp_fail++;
			wlcm_d("got event: DHCP failure");
			wifi_deauthenticate((uint8_t *)network->bssid);
#ifdef CONFIG_WLAN_FAST_PATH
			/* Mark the fast path cache invalid. */
			wlan.auth_cache_valid = false;
			wlan.fast_path_cache_valid = false;
#endif /* CONFIG_WLAN_FAST_PATH */
			do_connect_failed(WLAN_REASON_ADDRESS_FAILED);
			*next = wlan.sta_state;
			return;
		}
		char ip[16];
		g_wm_stats.wm_dhcp_succ++;
		wlcm_d("got event: DHCP success");
		net_configure_dns(&network->address,
				  network->role);
		if (network->type == WLAN_BSS_TYPE_STA)
			if_handle = net_get_mlan_handle();
#ifdef CONFIG_P2P
		else if (network->type == WLAN_BSS_TYPE_WIFIDIRECT)
			if_handle = net_get_wfd_handle();
#endif /* CONFIG_P2P */
		net_get_if_addr(&network->address, if_handle);
		wlan.sta_state = CM_STA_CONNECTED;
		*next = CM_STA_CONNECTED;
#ifdef CONFIG_WPS2
		wps_session_attempt = 0;
#endif
		wlan_network_ip_get(ip);
		CONNECTION_EVENT(WLAN_REASON_SUCCESS, &ip);
#ifdef CONFIG_P2P
		wifi_wfd_event(false, (void *)1);
#endif
	} else {
		wlcm_d("got event from dhcp client %d", msg->reason);
		if (msg->reason != WIFI_EVENT_REASON_SUCCESS) {
			g_wm_stats.wm_leas_fail++;
			/* There was some problem with dhcp lease
			 * renewal, so just disconnect from wlan and
			 * reconnect to it.*/
			wlcm_e("Lease renewal failed, disconnecting");
			/* disconnect-connect to wlan network current
			 * profile */
			wlan_connect(network->name);
			return;
		}
		g_wm_stats.wm_leas_succ++;
		/* Successful in getting ip address, so update
		 * local wlan-info params */
		wlcm_d("update wlan-info params");
		if (network->type == WLAN_BSS_TYPE_STA)
			if_handle = net_get_mlan_handle();
#ifdef CONFIG_P2P
		else if (network->type == WLAN_BSS_TYPE_WIFIDIRECT)
			if_handle = net_get_wfd_handle();
#endif /* CONFIG_P2P */
		net_get_if_addr(&network->address, if_handle);
		CONNECTION_EVENT(WLAN_REASON_ADDRESS_SUCCESS, NULL);
	}
}


static void wlcm_process_net_if_config_event(struct wifi_message *msg,
					     enum cm_sta_state *next)
{
	if (wlan.sta_state != CM_STA_INITIALIZING) {
		wlcm_w("ignoring TCP configure response");
		return;
	}

	if (msg->reason != WIFI_EVENT_REASON_SUCCESS) {
		wlcm_e("Interfaces init failed");
		CONNECTION_EVENT
			(WLAN_REASON_INITIALIZATION_FAILED, NULL);
		/* stay here until user re-inits */
		*next = CM_STA_INITIALIZING;
		return;
	}

	wlan.sta_state = CM_STA_IDLE;
	*next = CM_STA_IDLE;

	/* If WIFI is in deepsleep on  exit from PM4 disable dee-psleep */
	if (backup_s.wifi_state == WIFI_DEEP_SLEEP) {
		*next = CM_STA_DEEP_SLEEP;
		wlan.cm_ps_state = PS_STATE_SLEEP_CFM;
		wlan.cm_deepsleepps_configured = true;
		wlan.deepsleepps_state = DEEPSLEEPPS_SLEEP;
		wlan.skip_ds_exit_cb = true;
		wlan_deepsleepps_off();
	} else {
		*next = CM_STA_IDLE;
#ifdef CONFIG_P2P
		/* This call is made to initiate WFD, We are not interested
		 * in the response since mac addr is already populated in
		 * wlan_start
		 */
		wifi_get_wfd_mac_address();
#endif
		wifi_get_uap_max_clients(&wlan.uap_supported_max_sta_num);
		wrapper_wlan_cmd_get_hw_spec();
	}

}

#define WL_ID_UAP_STARTED	"uap_started"

static enum cm_uap_state uap_state_machine(struct wifi_message *msg)
{
	struct wlan_network *network = &wlan.networks[wlan.cur_uap_network_idx];
	enum cm_uap_state next = wlan.uap_state;
	int ret = 0;
	void *if_handle = NULL;
	switch (msg->event) {
	case CM_UAP_USER_REQUEST_START:
		wakelock_get(WL_ID_UAP_STARTED);
		wlan.cur_uap_network_idx = (int)msg->data;
		wlan.scan_count = 0;

		do_start(&wlan.networks[wlan.cur_uap_network_idx]);
		next = wlan.uap_state;
		break;
	case CM_UAP_USER_REQUEST_STOP:
		if (wlan.uap_state < CM_UAP_STARTED)
			break;

		wakelock_put(WL_ID_UAP_STARTED);
		do_stop(&wlan.networks[wlan.cur_uap_network_idx]);
		next = wlan.uap_state;
		break;

	case CM_UAP_USER_REQUEST_PS_ENTER:{
			if (!is_uap_state(CM_UAP_IP_UP)) {
				break;
			}
			if (!msg->data) {
				wlcm_w
				    ("ignoring ps enter request with"
				     "NULL ps mode");
				break;
			}
			wlan.is_user_req_to_disable = false;
			uap_ps *uap_ps_ptr = (uap_ps *) &uap_ps_params_obj;
			wifi_uap_ps_inactivity_sleep_enter
			    (network->type,
			     uap_ps_ptr->sleep_param.ctrl_bitmap,
			     uap_ps_ptr->inact_param.inactivity_to,
			     uap_ps_ptr->sleep_param.min_sleep,
			     uap_ps_ptr->sleep_param.max_sleep,
			     uap_ps_ptr->inact_param.min_awake,
			     uap_ps_ptr->inact_param.max_awake);
		}
		break;
	case CM_UAP_USER_REQUEST_PS_EXIT:{
			if (!msg->data) {
				wlcm_w
				    ("ignoring ps enter request with"
				     " NULL ps mode");
				break;
			}
			if (wlan.cm_ps_state == PS_STATE_AWAKE) {
				wifi_uap_ps_inactivity_sleep_exit(
						network->type);
			} else {
				wlan.is_user_req_to_disable = true;
				wlan_wake_up_card();
			}
		}
		break;

	case WIFI_EVENT_UAP_STARTED:
		if (msg->reason == WIFI_EVENT_REASON_SUCCESS) {

			if (network->type == WLAN_BSS_TYPE_UAP) {
				memcpy(&network->bssid[0], &wlan.mac[0], 6);
				if_handle = net_get_uap_handle();
			}
#ifdef CONFIG_P2P
			else if (network->type == WLAN_BSS_TYPE_WIFIDIRECT) {
				memcpy(&network->bssid[0], &wlan.wfd_mac[0], 6);
				if_handle = net_get_wfd_handle();
			}
#endif /* CONFIG_P2P */

			ret = net_configure_address(&network->address,
						    if_handle);
			if (ret) {
				wlcm_e("TCP/IP stack setup failed");
				CONNECTION_EVENT(WLAN_REASON_ADDRESS_FAILED,
						 (void *)ret);
			} else
				next = CM_UAP_STARTED;
		} else {
			CONNECTION_EVENT(WLAN_REASON_UAP_START_FAILED, NULL);
		}
		break;
	case WIFI_EVENT_UAP_CLIENT_ASSOC:
#ifdef CONFIG_HOST_SUPP
		if (network->security.type == WLAN_SECURITY_WPA2) {
			supplicant_send_cmd(SUPP_UAP_SESS_INIT, NULL, 0, NULL,
					NULL, NULL, NULL, msg->data);
			supplicant_send_cmd(SUPP_UAP_ASSOC_DATA, NULL, 0, NULL,
					NULL, NULL, NULL, msg->data);
		}
#else
#ifdef CONFIG_WIFI_UAP_WORKAROUND_STICKY_TIM
		if (network->type == WLAN_BSS_TYPE_UAP) {
			wifi_uap_enable_sticky_bit(msg->data);
		}
#endif /* CONFIG_WIFI_UAP_WORKAROUND_STICKY_TIM */
#endif /* CONFIG_HOST_SUPP */
		/* This was allocated by the sender */
		os_mem_free(msg->data);
		break;
	case WIFI_EVENT_UAP_CLIENT_DEAUTH:
#ifdef CONFIG_HOST_SUPP
		if (network->security.type == WLAN_SECURITY_WPA2) {
			supplicant_send_cmd(SUPP_UAP_DEAUTH_DATA, NULL, 0, NULL,
					NULL, NULL, NULL, msg->data);
		}
#endif /* CONFIG_HOST_SUPP */
		/* This was allocated by the sender */
		os_mem_free(msg->data);
		break;
	case WIFI_EVENT_UAP_STOPPED:
		CONNECTION_EVENT(WLAN_REASON_UAP_STOPPED, NULL);
		break;
	case WIFI_EVENT_UAP_NET_ADDR_CONFIG:
		if (msg->reason == WIFI_EVENT_REASON_SUCCESS &&
		    network->address.addr_type == ADDR_TYPE_STATIC) {

			if (network->type == WLAN_BSS_TYPE_UAP)
				if_handle = net_get_uap_handle();
#ifdef CONFIG_P2P
			else if (network->type == WLAN_BSS_TYPE_WIFIDIRECT)
				if_handle = net_get_wfd_handle();
#endif /* CONFIG_P2P */

			net_get_if_addr(&network->address, if_handle);
			next = CM_UAP_IP_UP;
			CONNECTION_EVENT(WLAN_REASON_UAP_SUCCESS, NULL);
		} else {
			CONNECTION_EVENT(WLAN_REASON_ADDRESS_FAILED, NULL);
			next = CM_UAP_INITIALIZING;
		}
		break;
	case WIFI_EVENT_UAP_INACT_PS: {
		uint16_t action = (uint16_t) ((uint32_t)msg->data);

		wlcm_d("got msg data :: %x", action);
		if (action == SLEEP_CONFIRM) {
			wlan.cm_ps_state = PS_STATE_SLEEP_CFM;
			uap_ps_mode = WLAN_UAP_INACTIVITY_SLEEP;
			CONNECTION_EVENT(WLAN_REASON_PS_ENTER,
					 (void *)WLAN_UAP_INACTIVITY_SLEEP);
		}
		if (action == DIS_AUTO_PS) {
			wlan.cm_ps_state = PS_STATE_AWAKE;
			uap_ps_mode = WLAN_UAP_ACTIVE;
			CONNECTION_EVENT(WLAN_REASON_PS_EXIT,
					 (void *)WLAN_UAP_ACTIVE);
		}
		next = CM_UAP_IP_UP;
	}
		break;
	default:
		wlcm_w("got unknown message  UAP  : %d", msg->event);
		break;
	}
	return next;
}

static void wlcm_request_scan(struct wifi_message *msg,
			      enum cm_sta_state *next)
{

	if ((!is_scanning_allowed())
#ifdef CONFIG_WPS2
	 || (wps_session_attempt)
#endif
	) {
		wlcm_w("ignoring scan result in invalid state");
		wlcm_d("releasing scan lock");
		os_semaphore_put(&wlan.scan_lock);
		return;
	}

	if (!msg->data) {
		wlcm_w("ignoring scan request with NULL callback");
		wlcm_d("releasing scan lock");
		os_semaphore_put(&wlan.scan_lock);
		return;
	}

	wlcm_d("initiating wlan-scan (return to %s)",
	       dbg_sta_state_name(wlan.sta_state));
	int ret = wifi_send_scan_cmd(g_wifi_scan_params.bss_type,
				 g_wifi_scan_params.bssid,
				 g_wifi_scan_params.ssid,
				 g_wifi_scan_params.channel[0],
				 g_wifi_scan_params.scan_duration,
				 false);
	/*  fixme: if ret is non-zero it sets a new value in next and
	*   falls through where next is reset to a different value
	*/

	if (ret) {
		wlcm_e("wlan-scan failed");
		*next = wlan.sta_state;
	}
	wlan.scan_cb = (int (*)(unsigned int count))(msg->data);
	wlan.sta_return_to = wlan.sta_state;
	*next = CM_STA_SCANNING_USER;
}

static void wlcm_request_healthmon()
{
	if (uap_ps_mode != WLAN_UAP_ACTIVE
	    || is_state(CM_STA_DEEP_SLEEP)
	    || is_state(CM_STA_PDN))
		/* If power down or deep sleep state,
		   then update the cmd timer to current time */
		wifi_update_last_cmd_sent_ms();
	else {
		wifi_rssi_info_t rssi_info;
		wifi_send_rssi_info_cmd(&rssi_info);
		os_rwlock_read_unlock(&ps_rwlock);
	}
	wakelock_put(WL_ID_WIFI_RSSI);
}

static void wlcm_request_disconnect(enum cm_sta_state *next,
				    struct wlan_network *curr_nw)
{
	void *if_handle = NULL;
	/* On disconnect request, we need to take down the interface.
	   This is required to fix bug # 52964 */
	if (curr_nw->type == WLAN_BSS_TYPE_STA)
		if_handle = net_get_mlan_handle();
#ifdef CONFIG_P2P
	else if (curr_nw->type == WLAN_BSS_TYPE_WIFIDIRECT)
		if_handle = net_get_wfd_handle();
#endif /* CONFIG_P2P */
	net_interface_down(if_handle);
	/* Forcefully stop dhcp on given interface.
	 * net_interface_dhcp_stop internally does nothing
	 * if dhcp dhcp client is not started.
	 */
	net_interface_dhcp_stop(if_handle);

#ifdef CONFIG_WPA2_ENTP
	wifi_send_disable_supplicant(curr_nw->role);
#endif

	if (
#ifdef CONFIG_WPS2
	   (!wps_session_attempt) &&
#endif
	    (wlan.sta_state < CM_STA_IDLE || is_state(CM_STA_IDLE)
	    || is_state(CM_STA_DEEP_SLEEP) || is_state(CM_STA_PDN))) {
		os_rwlock_read_unlock(&ps_rwlock);
		wakelock_put(WL_ID_STA_DISCONN);
		return;
	}

	if (is_user_scanning() && wlan.sta_return_to != CM_STA_IDLE) {
		/* we're in a user scan: we'll need to disconnect and
		 * return to the idle state once this scan completes */
		if (wlan.sta_return_to >= CM_STA_ASSOCIATING) {
			wifi_deauthenticate((uint8_t *)
					    curr_nw->bssid);
			wlan.sta_return_to = CM_STA_IDLE;
			*next = CM_STA_IDLE;
		}
	} else if (wlan.sta_state >= CM_STA_ASSOCIATING) {
		/* if we aren't idle or user scanning,
		 * we'll need to actually
		 * disconnect */
		wifi_deauthenticate((uint8_t *)
				    curr_nw->bssid);
		wlan.sta_state = CM_STA_IDLE;
		*next = CM_STA_IDLE;
	} else if (wlan.sta_state == CM_STA_SCANNING) {
		wlan.sta_state = CM_STA_IDLE;
		*next = CM_STA_IDLE;
	}

#ifdef CONFIG_WPS2
	if (wps_session_attempt) {
		CONNECTION_EVENT(WLAN_REASON_WPS_DISCONNECT, NULL);
		wps_session_attempt = 0;
	} else
#endif
	CONNECTION_EVENT(WLAN_REASON_USER_DISCONNECT, NULL);

	if (wlan.dhcp_wakelock_taken) {
		wakelock_put(WL_ID_DHCP_CLIENT);
		wlan.dhcp_wakelock_taken = false;
	}
	os_rwlock_read_unlock(&ps_rwlock);
	wakelock_put(WL_ID_STA_DISCONN);
}

static void wlcm_request_connect(struct wifi_message *msg,
				 enum cm_sta_state *next,
				 struct wlan_network *network)
{
	int ret;

#ifdef CONFIG_WLAN_FAST_PATH
	/* Mark the fast path cache invalid. */
	if ((int)msg->data != wlan.cur_network_idx)
		wlan.auth_cache_valid = false;
#endif /* CONFIG_WLAN_FAST_PATH */

	if (is_state(CM_STA_DEEP_SLEEP) || is_state(CM_STA_PDN))
		return;
	if (wlan.sta_state >= CM_STA_ASSOCIATING) {
		struct wlan_network *new_network
			= &wlan.networks[(int)msg->data];
		if (new_network->role == WLAN_BSS_ROLE_STA) {
			wlcm_w("deauthenticating before"
			       " attempting new connection");
			wifi_deauthenticate((uint8_t *)network->bssid);
			CONNECTION_EVENT(WLAN_REASON_NETWORK_AUTH_FAILED, NULL);
		}
	}

	wlcm_d("starting connection to network: %d", (int)msg->data);

	ret = do_connect((int)msg->data);

	if (ret != WLAN_ERROR_NONE)
		CONNECTION_EVENT(WLAN_REASON_CONNECT_FAILED, NULL);

	*next = wlan.sta_state;
}

/*
 * Event Handlers
 */

static enum cm_sta_state handle_message(struct wifi_message *msg)
{
	enum cm_sta_state next = wlan.sta_state;
	struct wlan_network *network = &wlan.networks[wlan.cur_network_idx];

	switch (msg->event) {
	case CM_STA_USER_REQUEST_CONNECT:
		wlcm_request_connect(msg, &next, network);
		break;

	case CM_STA_USER_REQUEST_DISCONNECT:
		wlcm_request_disconnect(&next, network);
		break;

	case CM_STA_USER_REQUEST_SCAN:
		wlcm_request_scan(msg, &next);
		break;

	case CM_STA_USER_REQUEST_PS_ENTER:
		if (wlan.sta_state >= CM_STA_SCANNING &&
		    wlan.sta_state <= CM_STA_OBTAINING_ADDRESS) {
			wlcm_w("ignoring ps enter in invalid state");
			wlcm_e("Error entering power save mode");
			break;
		}
		if (!msg->data) {
			wlcm_w("ignoring ps enter request with NULL ps mode");
			wlcm_e("entering power save mode");
			break;
		}
		wlan_enable_power_save((int)msg->data);
		if ((int)msg->data == WLAN_PDN)
			next = CM_STA_PDN;
		break;
	case CM_STA_USER_REQUEST_PS_EXIT:
		if (!msg->data) {
			wlcm_w("ignoring ps exit request with NULL ps mode");
			break;
		}
		wlan_disable_power_save((int)msg->data);
		if ((int)msg->data == WLAN_PDN)
			next = CM_STA_IDLE;
		break;

	case CM_STA_USER_REQUEST_HEALTHMON:
		wlcm_request_healthmon();
		break;

	case WIFI_EVENT_SCAN_RESULT:
		wlcm_d("got event: scan result");
		wlcm_process_scan_result_event(msg, &next);
		break;

	case WIFI_EVENT_ASSOCIATION:
		wlcm_d("got event: association result: %s",
		       msg->reason ==
		       WIFI_EVENT_REASON_SUCCESS ? "success" : "failure");

		wlcm_process_association_event(msg, &next);
		break;
	case WIFI_EVENT_PMK:
		wlcm_d("got event: PMK result: %s",
		       msg->reason ==
		       WIFI_EVENT_REASON_SUCCESS ? "success" : "failure");
		wlcm_process_pmk_event(msg, &next, network);
		break;
		/* We have received a event from firmware whether
		 * authentication with given wireless credentials was successful
		 * or not. If successful, we are authenticated and can proceed
		 * to IP-level setup by configuring the TCP stack for our
		 * desired address and transitioning to the
		 * CM_STA_REQUESTING_ADDRESS state where we wait for the TCP
		 * stack configuration response.  Otherwise we have failed to
		 * connect to the network.
		 */
	case WIFI_EVENT_AUTHENTICATION:
		wlcm_d("got event: authentication result: %s",
		       msg->reason ==
		       WIFI_EVENT_REASON_SUCCESS ? "success" : "failure");
		wlcm_process_authentication_event(msg, &next, network);
		break;

	case WIFI_EVENT_LINK_LOSS:
		wlcm_w("got event: link loss, code=%d", (int)msg->data);
		wlcm_process_link_loss_event(msg, &next, network);
		break;

	case WIFI_EVENT_DISASSOCIATION:
		wlcm_w("got event: disassociation, code=%d", (int)(msg->data));
		wlcm_process_disassoc_event(msg, &next, network);
		break;

	case WIFI_EVENT_DEAUTHENTICATION:
		wlcm_w("got event: deauthentication");
		wlcm_process_deauthentication_event(msg, &next, network);
		break;

	case WIFI_EVENT_NET_STA_ADDR_CONFIG:
		wlcm_d("got event: TCP configured");
		wlcm_process_sta_addr_config_event(msg, &next, network);
		break;

	case WIFI_EVENT_GET_HW_SPEC:
		CONNECTION_EVENT(WLAN_REASON_INITIALIZED, NULL);
		break;

	case WIFI_EVENT_NET_INTERFACE_CONFIG:
		wlcm_d("got event: Interfaces configured");
		wlcm_process_net_if_config_event(msg, &next);
		break;

	case WIFI_EVENT_NET_DHCP_CONFIG:
		wlcm_process_net_dhcp_config(msg, &next, network);
		break;
#if 0
	case WIFI_EVENT_SUPPLICANT_PMK:
		if (!msg->data)
			break;
		wlcm_process_supplicant_pmk_event(msg, &next);
		break;
#endif
#ifdef CONFIG_5GHz_SUPPORT
	case WIFI_EVENT_CHAN_SWITCH:
		wlcm_d("got event: channel switch");
		wlcm_process_channel_switch(&next, network);
		break;
#endif
	case WIFI_EVENT_SLEEP:
		wlcm_d("got event: sleep");
		wlan_ieeeps_sm(IEEEPS_EVENT_SLEEP);
		wlan_deepsleepps_sm(DEEPSLEEPPS_EVENT_SLEEP);
		break;
	case WIFI_EVENT_AWAKE:
		wlcm_d("got event: awake");
		wlcm_process_awake_event();
		break;

	case WIFI_EVENT_IEEE_PS:
		wlcm_d("got event: IEEE ps result: %s",
		       msg->reason ==
		       WIFI_EVENT_REASON_SUCCESS ? "success" : "failure");
		wlcm_process_ieeeps_event(msg);
		break;

	case WIFI_EVENT_DEEP_SLEEP:
		wlcm_d("got event: deep sleep result: %s",
		       msg->reason ==
		       WIFI_EVENT_REASON_SUCCESS ? "success" : "failure");
		wlcm_process_deepsleep_event(msg, &next);
		break;
	/* fixme : This will be removed later
	 * We do not allow HS config without IEEEPS */
	case WIFI_EVENT_HS_CONFIG:
		wlcm_process_hs_config_event();
		break;

#ifdef CONFIG_11N
	case WIFI_EVENT_11N_ADDBA:
		wrapper_wlan_cmd_11n_addba_rspgen(msg->data);
		break;
	case WIFI_EVENT_11N_AGGR_CTRL:
		wlcm_d("AGGR_CTRL ignored for now");
		break;
#endif /* CONFIG_11N */
	case WIFI_EVENT_MAC_ADDR_CONFIG:
		wlcm_d("MAC ADDR CONFIG ignored for now");
		break;
	default:
		wlcm_w("got unknown message: %d", msg->event);
		break;
	}

	return next;
}

static int is_uap_msg(struct wifi_message *msg)
{
	return ((msg->event >= CM_UAP_USER_REQUEST_START) ||
		(msg->event <= WIFI_EVENT_UAP_LAST));
}

/*
 * Main Thread: the WLAN Connection Manager event queue handler and state
 * machine.
 */

static void cm_main(os_thread_arg_t data)
{
	int ret;
	struct wifi_message msg;
	enum cm_sta_state next_sta_state;
	enum cm_uap_state next_uap_state;

	/* Wait for all the data structures to be created */
	while (!wlan.running)
		os_thread_sleep(os_msec_to_ticks(500));

	net_wlan_init();

	while (1) {

		/* If delayed sleep confirmed should be performed, wait for
		   DELAYED_SLP_CFM_DUR else wait forever */
		ret = os_queue_recv(&wlan.events, &msg,
				    os_msec_to_ticks(
			g_req_sl_confirm ?
			DELAYED_SLP_CFM_DUR : OS_WAIT_FOREVER));
		if (wlan.stop_request) {
			wlcm_d("Received shutdown request\n\r");
			wlan.status = WLCMGR_THREAD_STOPPED;
			os_thread_self_complete(NULL);
		}

		if (ret == WM_SUCCESS) {
			wlcm_d("got wifi message: %d %d %p", msg.event,
			       msg.reason, msg.data);

			if (is_uap_msg(&msg)) {
				/* uAP related msg */
				next_uap_state = uap_state_machine(&msg);
				if (wlan.uap_state == next_uap_state)
					continue;

				wlcm_d("SM uAP %s -> %s",
				       dbg_uap_state_name(wlan.uap_state),
				       dbg_uap_state_name(next_uap_state));
				wlan.uap_state = next_uap_state;

			} else {
				/* STA related msg */
				next_sta_state = handle_message(&msg);
				if (wlan.sta_state == next_sta_state)
					continue;

				wlcm_d("SM STA %s -> %s",
				       dbg_sta_state_name(wlan.sta_state),
				       dbg_sta_state_name(next_sta_state));
				wlan.sta_state = next_sta_state;

			}
		} else {
			wlcm_d("SM queue recv Timed out ");

			if (!is_state(CM_STA_CONNECTED))
				continue;

			if (g_req_sl_confirm) {
				wlan_ieeeps_sm(IEEEPS_EVENT_SLEEP);

			}
		}
	}
}

/*
 * WLAN API
 */

static int send_user_request(enum user_request_type request, int data)
{
	struct wifi_message msg;
	msg.event = request;
	msg.reason = 0;
	msg.data = (void *)data;
	if (os_queue_send(&wlan.events, &msg, OS_NO_WAIT) == WM_SUCCESS)
		return WLAN_ERROR_NONE;
	return WLAN_ERROR_ACTION;
}

int wlan_healthmon_test_cmd_to_firmware()
{
	int ret = WM_SUCCESS;
	if (!wlan.running)
		ret = WLAN_ERROR_STATE;
	else {
		wakelock_get(WL_ID_WIFI_RSSI);
		ret = os_rwlock_read_lock(&ps_rwlock, OS_WAIT_FOREVER);
		if (ret != WM_SUCCESS) {
			wakelock_put(WL_ID_WIFI_RSSI);
			return ret;
		}
		ret = send_user_request(CM_STA_USER_REQUEST_HEALTHMON, 0);
	}
	if (ret != WM_SUCCESS)
		wakelock_put(WL_ID_WIFI_RSSI);
	return ret;
}

static void copy_network(struct wlan_network *dst, struct wlan_network *src)
{
	memcpy(dst, src, sizeof(struct wlan_network));
	/* Omit any information that was dynamically
	 * learned from the network so that users can
	 * see which parameters were actually
	 * configured by the user.
	 */
	if (!src->bssid_specific)
		memset(dst->bssid, 0, IEEEtypes_ADDRESS_SIZE);
	if (!src->ssid_specific)
		memset(dst->ssid, 0, IEEEtypes_SSID_SIZE);
	if (!src->channel_specific)
		dst->channel = 0;
	if (src->address.addr_type) {
		dst->address.ip = 0;
		dst->address.gw = 0;
		dst->address.netmask = 0;
		dst->address.dns1 = 0;
		dst->address.dns2 = 0;
	}
}

uint32_t wlan_random_entropy(void)
{
	unsigned char mac[6] = { 0 }, byte;
	short rssi, i;
	unsigned int feed_data;

	/* Start with a high prime number */
	feed_data = 98953;

	wlan_get_mac_address(mac);
	/* The nf and snr values are so varying per-packet (range-bound) that
	 * this should give some good randomness.
	 */

	rssi = g_data_nf_last - g_data_snr_last;

	feed_data *= (rssi);
	for (i = 0; i < 4; i++) {
		byte = (rssi & (0xff << (i * 8))) ^ mac[5 - i];
		feed_data ^= byte << (i * 8);
	}
	return feed_data;
}

static void wlan_ps_cb(power_save_event_t event, void *data)
{
	switch (event) {
	case ACTION_ENTER_PM4:{
		int ret = WM_SUCCESS;
		enum wlan_ps_mode ps_mode;
		backup_s.wifi_state = WIFI_ACTIVE;

		ret = wlan_get_ps_mode(&ps_mode);
		if (ret != WM_SUCCESS)
			break;
		if (is_state(CM_STA_IDLE)
		    && ps_mode != WLAN_PDN
		    && !is_uap_started())
			backup_s.wifi_state = WIFI_INACTIVE;

		if (ps_mode == WLAN_DEEP_SLEEP) {
			backup_s.wifi_state = WIFI_DEEP_SLEEP;
		} else if (ps_mode == WLAN_PDN) {
			backup_s.wifi_state = WIFI_PDN;
		}
	}
		break;
	default:
		break;
	}
}

static void wlan_extpin_cb()
{
}

mlan_status sd878x_init(enum wlan_type type, flash_desc_t *fl);

static int ps_wakeup_card_cb(os_rw_lock_t *plock,
			     unsigned int wait_time)
{
	int ret = os_semaphore_get(&(plock->rw_lock), 0);
	if (ret == -WM_FAIL) {
		wlan_wake_up_card();
		ret = os_semaphore_get(&(plock->rw_lock), wait_time);
	}
	return ret;
}

int wlan_init(flash_desc_t *fl)
{
	static flash_desc_t saved_fl;

	if (wlan.status != WLCMGR_INACTIVE)
		return WM_SUCCESS;

	random_register_handler(wlan_random_entropy);

	if (fl)
		saved_fl = *fl;

	int ret = sd878x_init(WLAN_TYPE_NORMAL, &saved_fl);
	if (ret) {
		wlcm_e("sd878x_init failed. status code %d", ret);
		switch (ret) {
		case MLAN_CARD_CMD_TIMEOUT:
		case MLAN_CARD_NOT_DETECTED:
			ret = -WLAN_ERROR_CARD_NOT_DETECTED;
			break;
		case MLAN_STATUS_FW_DNLD_FAILED:
			ret = -WLAN_ERROR_FW_DNLD_FAILED;
			break;
		case MLAN_STATUS_FW_NOT_DETECTED:
			ret = -WLAN_ERROR_FW_NOT_DETECTED;
			break;
		case MLAN_STATUS_FW_NOT_READY:
			ret = -WLAN_ERROR_FW_NOT_READY;
			break;
		}
		return ret;
	}

	ret = wm_wifi_init();
	if (ret) {
		wlcm_e("userif_init failed. status code %d", ret);
	}
	wlan.status = WLCMGR_INIT_DONE;

	mac_addr_t mac_addr;
	ret = wifi_get_device_mac_addr(&mac_addr);
	if (ret != WM_SUCCESS) {
		wlcm_e("Failed to get mac address");
		return ret;
	}

	memcpy(&wlan.mac[0], mac_addr.mac, MAC_ADDR_LENGTH);
#ifdef CONFIG_P2P
	memcpy(&wlan.wfd_mac[0], mac_addr.mac, MAC_ADDR_LENGTH);
	wlan.wfd_mac[0] |= (0x01 << 1);
#endif

	ret = wifi_get_device_firmware_version_ext(&wlan.fw_ver_ext);
	if (ret != WM_SUCCESS) {
		wlcm_e("Failed to get verext");
		return ret;
	}
	wlcm_d("WLAN FW ext_version: %s", wlan.fw_ver_ext.version_str);

	/* register a power save callback */
	pm_register_cb(ACTION_ENTER_PM4, wlan_ps_cb, NULL);


	ret = os_rwlock_create_with_cb(&ps_rwlock,
			       "ps_mutex",
			       "ps_lock",
			       ps_wakeup_card_cb);
	return ret;
}

void sd878x_deinit(void);
void wlan_deinit()
{
	if ((wlan.status != WLCMGR_ACTIVATED) &&
	    (wlan.status != WLCMGR_INIT_DONE)) {
		wlcm_e("cannot deinit wlcmgr. unexpected status: %d\n\r",
		       wlan.status);
		return;
	}

	wlan_fw_heartbeat_unregister_healthmon();
	random_unregister_handler(wlan_random_entropy);
	sd878x_deinit();
	wm_wifi_deinit();

	wlan.status = WLCMGR_INACTIVE;
}

#ifdef CONFIG_HOST_SUPP
static int wlcm_supp_callback(enum supp_event event, void *data, uint16_t len)
{
	int ret = WM_SUCCESS;
	struct wifi_message msg;

	wlcm_d("SUPP EVENT = %d data = %p len=%d", event, data, len);

	if (event == SUPP_SESSION_FAILED) {
		msg.event = WIFI_EVENT_AUTHENTICATION;
		msg.reason = WIFI_EVENT_REASON_FAILURE;
		msg.data = (void *) IEEEtypes_REASON_4WAY_HANDSHK_TIMEOUT;

		ret = os_queue_send(&wlan.events, &msg, OS_NO_WAIT);
	}
	return ret;
}
#endif

#ifdef CONFIG_WPA2_ENTP
#ifdef CONFIG_WPS2
static int wlcm_wps_callback(enum wps_event event, void *data, uint16_t len)
{
	int ret = WM_SUCCESS;
	struct wifi_message msg;

	wlcm_d("WPS EVENT = %d data = %p len=%d", event, data, len);

	if (event == WPS_SESSION_SUCCESSFUL) {

		msg.event = WIFI_EVENT_PMK;
		msg.reason = WIFI_EVENT_REASON_SUCCESS;
		msg.data = data;

		ret = os_queue_send(&wlan.events, &msg, OS_NO_WAIT);
	} else if (event == WPS_SESSION_TIMEOUT ||
		   event == WPS_SESSION_FAILED) {
		msg.event = WIFI_EVENT_AUTHENTICATION;
		msg.reason = WIFI_EVENT_REASON_FAILURE;
		msg.data = (void *)WPA2_ENTERPRISE_FAILED;

		ret = os_queue_send(&wlan.events, &msg, OS_NO_WAIT);
	}

	return ret;
}

static struct wps_config wps_conf = {
	.role = 1,
	.pin_generator = 1,
	.version = 0x20,
	.version2 = 0x20,
	.device_name = "Marvell-Embedded-Client",
	.manufacture = "Marvell",
	.model_name = "RD-88W-PLUG-8787-A0",
	.model_number = "0001",
	.serial_number = "0001",
	.config_methods = 0x2388,
	.primary_dev_category = 01,
	.primary_dev_subcategory = 01,
	.rf_bands = 1,
	.os_version = 0xFFFFFFFF,
	.wps_msg_max_retry = 5,
	.wps_msg_timeout = 5000,
	.pin_len = 8,
	.wps_callback = wlcm_wps_callback,
};
#endif
#endif

int wlan_start(int (*cb) (enum wlan_event_reason reason, void *data))
{
	int ret;

	if (wlan.status != WLCMGR_INIT_DONE) {
		wlcm_e("cannot start wlcmgr. unexpected status: %d\n\r",
		       wlan.status);
		return WLAN_ERROR_STATE;
	}

	if (cb == NULL)
		return WLAN_ERROR_PARAM;

	if (wlan.running)
		return WLAN_ERROR_STATE;

	wlan.sta_state = CM_STA_INITIALIZING;

	wlan.sta_return_to = CM_STA_IDLE;
	wlan.uap_state = CM_UAP_INITIALIZING;
	wlan.uap_return_to = CM_UAP_INITIALIZING;

	wlan.cm_ps_state = PS_STATE_AWAKE;
	wlan.cm_ieeeps_configured = false;

	wlan.cm_deepsleepps_configured = false;
	wlan.cm_pdnps_configured = false;

	uap_ps_mode = WLAN_UAP_ACTIVE;

	wlan.wakeup_conditions = WAKE_ON_UNICAST | WAKE_ON_MAC_EVENT |
				 WAKE_ON_MULTICAST | WAKE_ON_ARP_BROADCAST;

	wlan.cur_network_idx = -1;
	wlan.cur_uap_network_idx = -1;

	wlan.num_networks = 0;
	memset(&wlan.networks[0], 0, sizeof(wlan.networks));
	wlan.scan_count = 0;
	wlan.cb = cb;
	wlan.scan_cb = NULL;
	wlan.events_queue_data = g_wlan_event_queue_data;
	ret =
	    os_queue_create(&wlan.events, "wlan-events",
			    sizeof(struct wifi_message),
			    &wlan.events_queue_data);
	if (ret != WM_SUCCESS) {
		wlcm_e("unable to create event queue: %d", ret);
		return WLAN_ERROR_ACTION;
	}

	ret = wifi_register_event_queue(&wlan.events);
	if (ret) {
		wlcm_e("unable to register event queue");
		os_queue_delete(&wlan.events);
		return WLAN_ERROR_ACTION;
	}

	wlan.cm_stack = g_cm_stack;
	ret = os_thread_create(&wlan.cm_main_thread, "wlcmgr", cm_main, 0,
			       &wlan.cm_stack, OS_PRIO_3);

	if (ret) {
		wlan.cb = NULL;
		wifi_unregister_event_queue(&wlan.events);
		os_queue_delete(&wlan.events);
		return WLAN_ERROR_ACTION;
	}

	if (os_semaphore_create(&wlan.scan_lock, "wlan-scan")) {
		wifi_unregister_event_queue(&wlan.events);
		os_queue_delete(&wlan.events);
		os_thread_delete(&wlan.cm_main_thread);
		return WLAN_ERROR_ACTION;
	}
	wlan.running = 1;

	wlan.status = WLCMGR_ACTIVATED;

#ifdef CONFIG_WLAN_FW_HEARTBEAT
	/* Healthmon is required for FW Hearbeat */
	healthmon_init();
	ret = wlan_fw_heartbeat_register_healthmon();
	if (ret) {
		wlcm_e("failed to register heartbeat to healthmon");
		return ret;
	}
#endif

#ifdef CONFIG_WPA2_ENTP
#ifdef CONFIG_WPS2
	wlcm_d("WPS started");
	wps_start(&wps_conf);
#endif
#endif

#ifdef CONFIG_HOST_SUPP
	ret = supplicant_start(wlcm_supp_callback);
	if (ret != WM_SUCCESS)
		return WLAN_ERROR_ACTION;

	supplicant_send_cmd(SUPP_STA_INIT, NULL, 0, NULL,
				NULL, NULL, NULL, wlan.mac);
	supplicant_send_cmd(SUPP_STA_ALLOC, NULL, 0, NULL,
				NULL, NULL, NULL, wlan.mac);
#endif

	return WLAN_ERROR_NONE;
}

int wlan_stop(void)
{
	int ret = WLAN_ERROR_NONE;
	int total_wait_time = 1000;	/* millisecs */
	int check_interval = 200;	/* millisecs */
	int num_iterations = total_wait_time / check_interval;

	if (wlan.status != WLCMGR_ACTIVATED) {
		wlcm_e("cannot stop wlcmgr. unexpected status: %d\n\r",
		       wlan.status);
		return WLAN_ERROR_STATE;
	}

	if (!wlan.running)
		return WLAN_ERROR_STATE;
	wlan.running = 0;

	/* We need to wait for scan_lock as wifi scan might have been
	 * scheduled, so it must be completed before deleting cm_main_thread
	 * here. Otherwise deadlock situation might arrive as both of them
	 * share command_lock semaphore.
	 */
	ret = os_semaphore_get(&wlan.scan_lock, OS_WAIT_FOREVER);
	if (ret != WM_SUCCESS) {
		wlcm_w("failed to get scan lock: %d.", ret);
		return WLAN_ERROR_STATE;
	}

	ret = os_semaphore_delete(&wlan.scan_lock);
	if (ret != WM_SUCCESS) {
		wlcm_w("failed to delete scan lock: %d.", ret);
		return WLAN_ERROR_STATE;
	}

	wlan.is_scan_lock = 0;
	/* We need to tell the AP that we're going away, however we've already
	 * stopped the main thread so we can't do this by means of the state
	 * machine.  Unregister from the wifi interface and explicitly send a
	 * deauth request and then proceed to tearing the main thread down. */

	/* Set stop_request and wait for wlcmgr thread to acknowledge it */
	wlan.stop_request = true;

	wlcm_d("Sent wlcmgr shutdown request. Current State: %d\r\n",
	       wlan.status);

	while (wlan.status != WLCMGR_THREAD_STOPPED && --num_iterations) {
		os_thread_sleep(os_msec_to_ticks(check_interval));
	}

	if (wlan.status != WLCMGR_THREAD_STOPPED && !num_iterations) {
		wlcm_d("Timed out waiting for wlcmgr to stop\r\n");
		wlcm_d("Forcing halt for wlcmgr thread\r\n");
		/* Reinitiailize variable states */
		wlan.status = WLCMGR_THREAD_STOPPED;
	}

	wlan.stop_request = false;

	ret = wifi_unregister_event_queue(&wlan.events);

	if (ret != WM_SUCCESS) {
		wlcm_w("failed to unregister wifi event queue: %d", ret);
		return WLAN_ERROR_STATE;
	}

	ret = os_queue_delete(&wlan.events);

	if (ret != WM_SUCCESS) {
		wlcm_w("failed to delete event queue: %d", ret);
		return WLAN_ERROR_STATE;
	}

	if (wlan.sta_state > CM_STA_ASSOCIATING) {
		wifi_deauthenticate((uint8_t *)
				    wlan.networks[wlan.cur_network_idx].bssid);
		wlan.sta_return_to = CM_STA_IDLE;
	}
	if (wlan.uap_state > CM_UAP_CONFIGURED)
		wifi_uap_stop(wlan.networks[wlan.cur_uap_network_idx].type);

	ret = os_thread_delete(&wlan.cm_main_thread);

	if (ret != WM_SUCCESS) {
		wlcm_w("failed to terminate thread: %d", ret);
		return WLAN_ERROR_STATE;
	}

	wlan.status = WLCMGR_INIT_DONE;
	wlcm_d("WLCMGR thread deleted\n\r");

	return ret;
}

int wlan_add_network(struct wlan_network *network)
{
	int pos = -1;
	int i;
	unsigned int len;

	if (network == NULL)
		return WLAN_ERROR_PARAM;

	if (network->role == WLAN_BSS_ROLE_STA)
		if (is_running() && !is_state(CM_STA_IDLE) &&
		    !is_state(CM_STA_ASSOCIATED) &&
		    !is_state(CM_STA_CONNECTED))
			return WLAN_ERROR_STATE;

	/* make sure that the network name length is acceptable */
	len = strlen(network->name);
	if (len < WLAN_NETWORK_NAME_MIN_LENGTH ||
	    len >= WLAN_NETWORK_NAME_MAX_LENGTH)
		return WLAN_ERROR_PARAM;

	/* make sure that either the SSID or BSSID field is present */
	if (network->ssid[0] == '\0' && is_bssid_any(network->bssid))
		return WLAN_ERROR_PARAM;

	/* Make sure network type is set correctly if not
	 * set correct values as per role*/
	if ((network->type == WLAN_BSS_TYPE_STA) ||
		(network->type == WLAN_BSS_TYPE_ANY)) {
		if (network->role == WLAN_BSS_ROLE_UAP)
			network->type = WLAN_BSS_TYPE_UAP;
		else if (network->role == WLAN_BSS_ROLE_STA)
			network->type = WLAN_BSS_TYPE_STA;
	}

	/* Find a slot for the new network but check all existing networks in
	 * case the new one has a duplicate name, which is not allowed. */
	for (i = 0; i < ARRAY_SIZE(wlan.networks); i++)
		if (wlan.networks[i].name[0] != '\0') {
			if (strlen(wlan.networks[i].name) == len &&
			    !strncmp(wlan.networks[i].name, network->name, len))
				return WLAN_ERROR_PARAM;
		} else if (pos == -1)
			pos = i;

	if (pos < 0)
		return WLAN_ERROR_NOMEM;

	/* save and set private fields */
	memcpy(&wlan.networks[pos], network, sizeof(struct wlan_network));
	wlan.networks[pos].ssid_specific = (network->ssid[0] != '\0');
	wlan.networks[pos].bssid_specific = !is_bssid_any(network->bssid);
	wlan.networks[pos].channel_specific = (network->channel != 0);
	if (network->security.type != WLAN_SECURITY_WILDCARD) {
		wlan.networks[pos].security_specific = 1;
	}

	wlan.num_networks++;
#ifdef CONFIG_WLAN_FAST_PATH
	/* On station network addition,
	 * flush the FP cache since the index changes */
	if (network->role == WLAN_BSS_ROLE_STA) {
		wlan.auth_cache_valid = false;
		wlan.fast_path_cache_valid = false;
	}
#endif /* CONFIG_WLAN_FAST_PATH */

	return WLAN_ERROR_NONE;
}

int wlan_remove_network(const char *name)
{
	unsigned int len, i;

	if (is_running() && !is_state(CM_STA_IDLE) &&
	    !is_state(CM_STA_ASSOCIATED) &&
	    !is_state(CM_STA_CONNECTED))
		return WLAN_ERROR_STATE;

	if (name == NULL)
		return WLAN_ERROR_PARAM;

	len = strlen(name);

	/* find the first network whose name matches and clear it out */
	for (i = 0; i < ARRAY_SIZE(wlan.networks); i++) {
		if (wlan.networks[i].name[0] != '\0' &&
		    strlen(wlan.networks[i].name) == len &&
		    !strncmp(wlan.networks[i].name, name, len)) {
			if (wlan.running && wlan.cur_network_idx == i) {
#ifdef CONFIG_WLAN_FAST_PATH
				/* On station network removal,
				 * flush the FP cache since the index changes */
				wlan.auth_cache_valid = false;
				wlan.fast_path_cache_valid = false;
#endif /* CONFIG_WLAN_FAST_PATH */
				if (is_state(CM_STA_CONNECTED)) {
					if (wlan_disconnect())
						return WLAN_ERROR_ACTION;
				}
			}
			if (wlan.cur_uap_network_idx == i) {
				if (send_user_request
				    (CM_UAP_USER_REQUEST_STOP, 0))
					return WLAN_ERROR_ACTION;
			}

			memset(&wlan.networks[i], 0,
			       sizeof(struct wlan_network));
			wlan.num_networks--;
			return WLAN_ERROR_NONE;
		}
	}
	/* network name wasn't found */
	return WLAN_ERROR_PARAM;
}

int wlan_get_network_count(unsigned int *count)
{
	if (count == NULL)
		return WLAN_ERROR_PARAM;

	*count = wlan.num_networks;
	return WLAN_ERROR_NONE;
}

int wlan_get_current_network(struct wlan_network *network)
{
	if (network == NULL)
		return WLAN_ERROR_PARAM;

	if (wlan.running && (is_state(CM_STA_CONNECTED) ||
				is_state(CM_STA_ASSOCIATED))) {
		memcpy(network, &wlan.networks[wlan.cur_network_idx],
		       sizeof(struct wlan_network));
		return WLAN_ERROR_NONE;
	}

	return WLAN_ERROR_STATE;
}

int wlan_get_current_uap_network(struct wlan_network *network)
{
	if (network == NULL)
		return WLAN_ERROR_PARAM;

	if (wlan.running && is_uap_state(CM_UAP_IP_UP)) {
		memcpy(network, &wlan.networks[wlan.cur_uap_network_idx],
		       sizeof(struct wlan_network));
		return WLAN_ERROR_NONE;
	}
	return WLAN_ERROR_STATE;
}

int is_uap_started(void)
{
	return is_uap_state(CM_UAP_IP_UP);
}
bool is_sta_connected()
{
	return (wlan.sta_state == CM_STA_CONNECTED);
}

int wlan_get_network(unsigned int index, struct wlan_network *network)
{
	int i, pos = -1;

	if (network == NULL || index > ARRAY_SIZE(wlan.networks))
		return WLAN_ERROR_PARAM;

	for (i = 0; i < ARRAY_SIZE(wlan.networks); i++) {
		if (wlan.networks[i].name[0] != '\0' && ++pos == index) {
			copy_network(network, &wlan.networks[i]);
			return WLAN_ERROR_NONE;
		}
	}

	return WLAN_ERROR_PARAM;
}

int wlan_get_current_nf()
{
	wifi_rssi_info_t rssi_info;
	wifi_send_rssi_info_cmd(&rssi_info);
	return g_bcn_nf_last;
}

int wlan_get_current_rssi(short *rssi)
{
	g_rssi = g_data_snr_last - g_data_nf_last;
	*rssi = g_rssi - 256;
	return 0;
}

int wlan_get_network_byname(char *name, struct wlan_network *network)
{
	int i;

	if (network == NULL || name == NULL)
		return WLAN_ERROR_PARAM;

	for (i = 0; i < ARRAY_SIZE(wlan.networks); i++) {
		if (wlan.networks[i].name[0] != '\0' &&
		    !strcmp(wlan.networks[i].name, name)) {
			copy_network(network, &wlan.networks[i]);
			return WLAN_ERROR_NONE;
		}
	}

	return WLAN_ERROR_PARAM;
}

int wlan_disconnect(void)
{
	if (!wlan.running)
		return WLAN_ERROR_STATE;

	wakelock_get(WL_ID_STA_DISCONN);

	int ret = os_rwlock_read_lock(&ps_rwlock, OS_WAIT_FOREVER);
	if (ret != WM_SUCCESS) {
		wakelock_put(WL_ID_STA_DISCONN);
		return ret;
	}
	send_user_request(CM_STA_USER_REQUEST_DISCONNECT, 0);
	return WLAN_ERROR_NONE;
}

int wlan_connect(char *name)
{
	unsigned int len = name ? strlen(name) : 0;
	int i = 0;

	if (!wlan.running)
		return WLAN_ERROR_STATE;

	if (wlan.num_networks == 0 || len == 0)
		return WLAN_ERROR_PARAM;

	/* connect to a specific network */
	for (i = 0; i < ARRAY_SIZE(wlan.networks); i++)
		if (wlan.networks[i].name[0] != '\0' &&
		    strlen(wlan.networks[i].name) == len &&
		    !strncmp(wlan.networks[i].name, name, len))
			return send_user_request(CM_STA_USER_REQUEST_CONNECT,
						 i);

	/* specified network was not found */
	return WLAN_ERROR_PARAM;
}

int wlan_start_network(const char *name)
{
	int i;
	unsigned int len;

	if (!name)
		return WLAN_ERROR_PARAM;

	len = strlen(name);
	if (len == 0 || wlan.num_networks == 0)
		return WLAN_ERROR_PARAM;

	if (wlan_is_ieeeps_active()) {
		wlcm_w("Station in IEEE Power Save, UAP cannot be started");
		return WLAN_ERROR_STATE;
	}

	if (wlan.cm_deepsleepps_configured) {
		wlcm_w("Station in Deepsleep, UAP cannot be started");
		return WLAN_ERROR_STATE;
	}
	if (wlan.cm_pdnps_configured) {
		wlcm_w("Station in Power down state, UAP cannot be started");
		return WLAN_ERROR_STATE;
	}

	for (i = 0; i < ARRAY_SIZE(wlan.networks); i++)
		if (wlan.networks[i].name[0] != '\0' &&
		    strlen(wlan.networks[i].name) == len &&
		    !strncmp(wlan.networks[i].name, name, len) &&
		    (wlan.networks[i].role == WLAN_BSS_ROLE_UAP)
		     && wlan.networks[i].ssid_specific) {
			if (wlan.networks[i].channel_specific &&
			    is_sta_connecting()) {
				wlcm_e("uAP can not be started on specific "
				       "channel when station is connected."
				       "Please use channel 0 (auto) for uAP");
				return WLAN_ERROR_PARAM;
			}
			if ((wlan.networks[i].channel_specific) &&
			    (wlan.networks[i].channel != 0))
				wlcm_w("NOTE: uAP will automatically switch to"
				       " the channel that station is on.");
			if (wlan.networks[i].role == WLAN_BSS_ROLE_UAP)
				return
				    send_user_request(CM_UAP_USER_REQUEST_START,
						      i);
		}

	/* specified network was not found */
	return WLAN_ERROR_PARAM;
}

int wlan_stop_network(const char *name)
{
	int i;
	unsigned int len;

	if (!name)
		return WLAN_ERROR_PARAM;

	len = strlen(name);
	if (len == 0 || wlan.num_networks == 0)
		return WLAN_ERROR_PARAM;

	/* Search for matching SSID
	 * If found send stop request
	 */
	for (i = 0; i < ARRAY_SIZE(wlan.networks); i++)  {
		if ((wlan.networks[i].name[0] == '\0') ||
		    (strlen(wlan.networks[i].name) != len) ||
		    (strncmp(wlan.networks[i].name, name, len)) != 0)
			continue;

		if (wlan.networks[i].role == WLAN_BSS_ROLE_UAP
		    && wlan.networks[i].ssid_specific) {
			return send_user_request(CM_UAP_USER_REQUEST_STOP,
						 i);
		}
	} /* end of loop */
	/* specified network was not found */
	return WLAN_ERROR_PARAM;
}

int wlan_get_scan_result(unsigned int index, struct wlan_scan_result *res)
{
	struct wifi_scan_result *desc;

	if (res == NULL)
		return WLAN_ERROR_PARAM;

	if (!is_running())
		return WLAN_ERROR_STATE;

	if (wifi_get_scan_result(index, &desc))
		return WLAN_ERROR_ACTION;

	memset(res, 0, sizeof(struct wlan_scan_result));

	memcpy(&res->bssid[0], &desc->bssid[0], sizeof(res->bssid));
	memcpy(&res->ssid[0], (char *)&desc->ssid[0],
	       desc->ssid_len);
	res->ssid[desc->ssid_len] = 0;
	res->channel = desc->Channel;

	if (!desc->is_ibss_bit_set)
		res->role = WLAN_BSS_ROLE_STA;

	res->wmm = desc->wmm_ie_present;
#ifdef CONFIG_WPS2
	if (desc->wps_IE_exist == true) {
		res->wps = desc->wps_IE_exist;
		res->wps_session = desc->wps_session;
	}
#endif
#ifdef CONFIG_WPA2_ENTP
	if (desc->wpa2_entp_IE_exist) {
		res->wpa2_entp = desc->wpa2_entp_IE_exist;
	} else {
#endif
	if (desc->WPA_WPA2_WEP.wpa)
		res->wpa = 1;
	if (desc->WPA_WPA2_WEP.wpa2)
		res->wpa2 = 1;
	if (desc->WPA_WPA2_WEP.wepStatic || desc->WPA_WPA2_WEP.wepDynamic)
		res->wep = 1;
#ifdef CONFIG_WPA2_ENTP
	}
#endif

	res->rssi = desc->RSSI;

	return WLAN_ERROR_NONE;
}

void wlan_set_cal_data(uint8_t *cal_data, unsigned int cal_data_size)
{
	wifi_set_cal_data(cal_data, cal_data_size);
}

void wlan_set_mac_addr(uint8_t *mac)
{
	wifi_set_mac_addr(mac);
}

int wlan_scan(int (*cb) (unsigned int count))
{
	int ret;

	if (cb == NULL)
		return WLAN_ERROR_PARAM;

	if (!is_running() || !is_scanning_allowed())
		return WLAN_ERROR_STATE;

	wlcm_d("taking the scan lock (user scan)");
	dbg_lock_info();
	ret = os_semaphore_get(&wlan.scan_lock, OS_WAIT_FOREVER);
	if (ret != WM_SUCCESS) {
		wlcm_e("failed to get scan lock: 0x%X", ret);
		return WLAN_ERROR_ACTION;
	}
	wlcm_d("got the scan lock (user scan)");
	wlan.is_scan_lock = 1;

	return send_user_request(CM_STA_USER_REQUEST_SCAN, (int)cb);
}

int wlan_get_connection_state(enum wlan_connection_state *state)
{
	enum cm_sta_state cur;

	if (state == NULL)
		return WLAN_ERROR_PARAM;

	if (!is_running())
		return WLAN_ERROR_STATE;

	if (is_user_scanning())
		cur = wlan.sta_return_to;
	else
		cur = wlan.sta_state;

	switch (cur) {
	default:
	case CM_STA_IDLE:
		*state = WLAN_DISCONNECTED;
		break;
	case CM_STA_SCANNING:
		*state = WLAN_SCANNING;
		break;
	case CM_STA_ASSOCIATING:
		*state = WLAN_ASSOCIATING;
		break;
	case CM_STA_ASSOCIATED:
		*state = WLAN_ASSOCIATED;
		break;
	case CM_STA_REQUESTING_ADDRESS:
	case CM_STA_OBTAINING_ADDRESS:
		*state = WLAN_CONNECTING;
		break;
	case CM_STA_CONNECTED:
		*state = WLAN_CONNECTED;
		break;
	}
	return WLAN_ERROR_NONE;
}

static bool wlan_is_ieeeps_active()
{
	if (wlan.ieeeps_state >= IEEEPS_CONFIGURING
		&& wlan.ieeeps_state < IEEEPS_PRE_DISABLE)
		return true;
	else
		return false;
}
static bool wlan_is_deepsleepps_active()
{
	if (wlan.deepsleepps_state >= DEEPSLEEPPS_CONFIGURING)
		return true;
	else
		return false;
}


int wlan_get_ps_mode(enum wlan_ps_mode *ps_mode)
{
	if (ps_mode == NULL)
		return WLAN_ERROR_PARAM;

	*ps_mode = WLAN_ACTIVE;

	if (wlan_is_deepsleepps_active())
		*ps_mode = WLAN_DEEP_SLEEP;
	else if (wlan_is_ieeeps_active())
		*ps_mode = WLAN_IEEE;
	else if (wlan.cm_pdnps_configured)
		*ps_mode = WLAN_PDN;

	return WLAN_ERROR_NONE;
}

int wlan_get_uap_connection_state(enum wlan_connection_state *state)
{
	enum cm_uap_state cur;

	if (state == NULL)
		return WLAN_ERROR_PARAM;

	if (!is_running())
		return WLAN_ERROR_STATE;

	cur = wlan.uap_state;

	switch (cur) {
	default:
	case CM_UAP_INITIALIZING:
	case CM_UAP_CONFIGURED:
		*state = WLAN_UAP_STOPPED;
		break;
	case CM_UAP_STARTED:
	case CM_UAP_IP_UP:
		*state = WLAN_UAP_STARTED;
		break;
	}
	return WLAN_ERROR_NONE;
}

int wlan_get_uap_ps_mode(enum wlan_uap_ps_mode *ps_mode)
{
	if (ps_mode == NULL)
		return WLAN_ERROR_PARAM;

	*ps_mode = uap_ps_mode;
	return WLAN_ERROR_NONE;
}

int wlan_get_address(struct wlan_ip_config *addr)
{
	void *if_handle = NULL;
	if (addr == NULL)
		return WLAN_ERROR_PARAM;

	if (!is_running() || !is_state(CM_STA_CONNECTED))
		return WLAN_ERROR_STATE;

	if_handle = net_get_mlan_handle();
	if (net_get_if_addr(addr, if_handle))
		return WLAN_ERROR_ACTION;

	return WLAN_ERROR_NONE;
}

int wlan_get_uap_address(struct wlan_ip_config *addr)
{
	void *if_handle = NULL;
	if (addr == NULL)
		return WLAN_ERROR_PARAM;
	if (!is_running() || !is_uap_state(CM_UAP_IP_UP))
		return WLAN_ERROR_STATE;

	if_handle = net_get_uap_handle();
	if (net_get_if_addr(addr, if_handle))
		return WLAN_ERROR_ACTION;
	return WLAN_ERROR_NONE;
}

#ifdef CONFIG_P2P
int wlan_get_wfd_address(struct wlan_ip_config *addr)
{
	void *if_handle = NULL;
	if (addr == NULL)
		return WLAN_ERROR_PARAM;
	if (!is_running())
		return WLAN_ERROR_STATE;

	if_handle = net_get_wfd_handle();
	if (net_get_if_addr(addr, if_handle))
		return WLAN_ERROR_ACTION;
	return WLAN_ERROR_NONE;
}
#endif

int wlan_get_mac_address(unsigned char *dest)
{
	if (!dest)
		return WLAN_ERROR_PARAM;
	memset(dest, 0, MAC_ADDR_SIZE);
	memcpy(dest, &wlan.mac[0], MAC_ADDR_SIZE);
	return WLAN_ERROR_NONE;
}

#ifdef CONFIG_P2P
int wlan_get_wfd_mac_address(unsigned char *dest)
{

	if (!dest)
		return WLAN_ERROR_PARAM;
	memset(dest, 0, MAC_ADDR_SIZE);
	memcpy(dest, &wlan.wfd_mac[0], MAC_ADDR_SIZE);
	return WLAN_ERROR_NONE;
}
#endif

/* Get the current IP Address of the wlan station */
static int wlan_network_ip_get(char *ipaddr)
{
	int ret;
	struct wlan_ip_config addr;

	ret = wlan_get_address(&addr);
	if (ret != WLAN_ERROR_NONE) {
		wlcm_w("failed to get IP address");
		strcpy(ipaddr, "0.0.0.0");
		return -WM_FAIL;
	}

	net_inet_ntoa(addr.ip, ipaddr);
	return WM_SUCCESS;
}

void wlan_wake_up_card()
{
	uint32_t resp;

	sdio_drv_creg_write(NULL, 0x0, 1, 0x02, &resp);
}

static void wlan_power_down_card()
{
	if (board_wakeup0_wifi())
		NVIC_DisableIRQ(ExtPin0_IRQn);
	if (board_wakeup1_wifi())
		NVIC_DisableIRQ(ExtPin1_IRQn);
	board_sdio_pdn();
}

static void wlan_power_up_card()
{
	board_sdio_pwr();
	if (board_wakeup0_wifi())
		NVIC_EnableIRQ(ExtPin0_IRQn);
	if (board_wakeup1_wifi())
		NVIC_EnableIRQ(ExtPin1_IRQn);
}

int wlan_uap_inact_sleep_ps_on(unsigned int ctrl_bitmap,
			       unsigned int inactivity_to,
			       unsigned int min_sleep,
			       unsigned int max_sleep,
			       unsigned int min_awake, unsigned int max_awake)
{
	if (!wlan.running)
		return WLAN_ERROR_STATE;

	enum wlan_connection_state state;
	if (wlan_get_uap_connection_state(&state)) {
		wlcm_e("unable to get uAP connection state");
		return WLAN_ERROR_STATE;
	}
	if (state == WLAN_UAP_STOPPED) {
		wlcm_e("UAP not started");
		return WLAN_ERROR_STATE;
	}
	if (wlan.cm_deepsleepps_configured || wlan.cm_pdnps_configured)
		return WLAN_ERROR_STATE;

	if (ctrl_bitmap > 1 || ctrl_bitmap < 0)
		return WLAN_ERROR_PARAM;

	if (min_sleep == 0) {
		min_sleep = PS_SLEEP_PERIOD_DEFAULT;
	}
	if (max_sleep == 0) {
		max_sleep = PS_SLEEP_PERIOD_DEFAULT;
	}
	if (max_awake == 0) {
		max_awake = PS_AWAKE_PERIOD_MIN;
	}

	if (min_awake == 0) {
		min_awake = PS_AWAKE_PERIOD_MIN;
	}
	if (inactivity_to == 0) {
		inactivity_to = PS_INACT_PERIOD_DEFAULT;
	}
	if (max_sleep < min_sleep) {
		wlcm_e("Invalid sleep parameters");
		return WLAN_ERROR_PARAM;
	}

	if (min_sleep < PS_SLEEP_PARAM_MIN) {
		wlcm_e("Incorrect value of sleep period.");
		return WLAN_ERROR_PARAM;
	}

	if (max_awake < min_awake) {
		wlcm_e("Invalid awake parameters");
		return WLAN_ERROR_PARAM;
	}

	if (min_awake < PS_AWAKE_PERIOD_MIN) {
		wlcm_e("Incorrect value of MIN_AWAKE period");
		return WLAN_ERROR_PARAM;
	}
	if (ctrl_bitmap == 1 && max_sleep > PS_SLEEP_PARAM_MAX) {
		wlcm_e("Incorrect value of sleep period.");
		return WLAN_ERROR_PARAM;
	}
	uap_ps_params_obj.inact_param.min_awake = min_awake;
	uap_ps_params_obj.inact_param.max_awake = max_awake;
	uap_ps_params_obj.inact_param.inactivity_to = inactivity_to;

	uap_ps_params_obj.sleep_param.ctrl_bitmap = ctrl_bitmap;
	uap_ps_params_obj.sleep_param.min_sleep = min_sleep;
	uap_ps_params_obj.sleep_param.max_sleep = max_sleep;

	return send_user_request(CM_UAP_USER_REQUEST_PS_ENTER,
				 WLAN_UAP_INACTIVITY_SLEEP);
}

int wlan_uap_inact_sleep_ps_off()
{
	if (!wlan.running)
		return WLAN_ERROR_STATE;

	enum wlan_uap_ps_mode ps_mode_uap;
	wlan_get_uap_ps_mode(&ps_mode_uap);

	if (ps_mode_uap == WLAN_UAP_ACTIVE)
		return WLAN_ERROR_STATE;
	return send_user_request(CM_UAP_USER_REQUEST_PS_EXIT,
				 WLAN_UAP_INACTIVITY_SLEEP);
}

int wlan_configure_listen_interval(int time_in_ms)
{
	if (!is_state(CM_STA_CONNECTED))
		return WLAN_ERROR_STATE;
	else
		return wifi_configure_listen_interval(time_in_ms);
}

void wlan_configure_null_pkt_interval(int time_in_secs)
{
	wifi_configure_null_pkt_interval(time_in_secs);
}
static int wlan_ieeeps_on()
{
	if (!wlan.running)
		return WLAN_ERROR_STATE;
	enum wlan_connection_state state;
	if (wlan_get_uap_connection_state(&state)) {
		wlcm_e("unable to get uAP connection state");
		return WLAN_ERROR_STATE;
	}
	if (state == WLAN_UAP_STARTED)
		return WLAN_ERROR_PS_ACTION;
	if (wlan.cm_ieeeps_configured || wlan.cm_deepsleepps_configured
	    || wlan.cm_pdnps_configured)
		return WLAN_ERROR_STATE;
	return send_user_request(CM_STA_USER_REQUEST_PS_ENTER, WLAN_IEEE);
}

static int wlan_ieeeps_off()
{
	if (wlan.cm_ieeeps_configured)
		return send_user_request(CM_STA_USER_REQUEST_PS_EXIT,
					 WLAN_IEEE);
	else
		return WLAN_ERROR_STATE;
}

int wlan_deepsleepps_on()
{
	if (!wlan.running)
		return WLAN_ERROR_STATE;
	enum wlan_connection_state state;
	if (wlan_get_uap_connection_state(&state)) {
		wlcm_e("unable to get uAP connection state");
		return WLAN_ERROR_STATE;
	}
	if (state == WLAN_UAP_STARTED)
		return WLAN_ERROR_PS_ACTION;

	if (wlan.cm_ieeeps_configured || wlan.cm_deepsleepps_configured
	    || wlan.cm_pdnps_configured)
		return WLAN_ERROR_STATE;

	if (is_state(CM_STA_CONNECTED))
		return WLAN_ERROR_STATE;

	return send_user_request(CM_STA_USER_REQUEST_PS_ENTER, WLAN_DEEP_SLEEP);
}

int wlan_deepsleepps_off()
{
	if (wlan.cm_deepsleepps_configured)
		return send_user_request(CM_STA_USER_REQUEST_PS_EXIT,
					 WLAN_DEEP_SLEEP);
	else
		return WLAN_ERROR_STATE;
}

int wlan_pdnps_on()
{
	if (!wlan.running)
		return WLAN_ERROR_STATE;
	enum wlan_connection_state state;
	if (wlan_get_uap_connection_state(&state)) {
		wlcm_e("unable to get uAP connection state");
		return WLAN_ERROR_STATE;
	}
	if (state == WLAN_UAP_STARTED)
		return WLAN_ERROR_PS_ACTION;

	if (wlan.cm_ieeeps_configured || wlan.cm_deepsleepps_configured
	    || wlan.cm_pdnps_configured)
		return WLAN_ERROR_STATE;

	if (is_state(CM_STA_CONNECTED))
		return WLAN_ERROR_STATE;
	return send_user_request(CM_STA_USER_REQUEST_PS_ENTER, WLAN_PDN);
}

int wlan_pdnps_off()
{
	if (wlan.cm_pdnps_configured)
		return send_user_request(CM_STA_USER_REQUEST_PS_EXIT, WLAN_PDN);
	else
		return WLAN_ERROR_STATE;
}

void pm_ieeeps_hs_cfg(char enabled, unsigned int wakeup_conditions)
{
	int ret;

	if (!is_pm_init_done()) {
		wlcm_e("Power Manager Module is not initialized.");
		return;
	}

	if (enabled) {

		wlan.wakeup_conditions = wakeup_conditions;

		if (wlan.cm_ieeeps_configured)
			return;

		ret = wlan_ieeeps_on();
		if (ret != WM_SUCCESS) {
			wlcm_e("Failed to start IEEE power save mode\r\n");
			goto out;
		}
		if (board_wakeup0_wifi()) {
			install_int_callback(INT_EXTPIN0,
					     0, wlan_extpin_cb);
			NVIC_EnableIRQ(ExtPin0_IRQn);
		}
		if (board_wakeup1_wifi()) {
			install_int_callback(INT_EXTPIN1,
					     0, wlan_extpin_cb);
			NVIC_EnableIRQ(ExtPin1_IRQn);
		}
		return;
	} else
		wlan_ieeeps_off();
 out:
		if (board_wakeup0_wifi()) {
			NVIC_DisableIRQ(ExtPin0_IRQn);
		}
		if (board_wakeup1_wifi()) {
			NVIC_DisableIRQ(ExtPin1_IRQn);
		}
}

int wlan_get_current_ant(unsigned int *ant)
{
	uint16_t pant;
	int rv = wifi_get_antenna(&pant);
	if (rv != WM_SUCCESS) {
		wlcm_e("Unable to get current antenna");
		return WLAN_ERROR_STATE;
	}

	*ant = pant;

	return WLAN_ERROR_NONE;
}

int wlan_set_current_ant(unsigned int ant)
{
	int rv = wifi_set_antenna((uint16_t)ant);
	if (rv != WM_SUCCESS) {
		wlcm_e("Unable to set antenna");
		return WLAN_ERROR_STATE;
	}

	return WLAN_ERROR_NONE;
}

int wlan_wlcmgr_send_msg(enum wifi_event event, int reason,
			 void *data)
{
	struct wifi_message msg;

	msg.event = event;
	msg.reason = reason;
	msg.data = (void *)data;

	if (os_queue_send(&wlan.events, &msg, OS_NO_WAIT) == WM_SUCCESS)
		return WLAN_ERROR_NONE;
	return WLAN_ERROR_ACTION;
}

/*
  This function validates input string for a valid WEP key, converts
  it to appropriate format

  Returns positive integer equal to size of the output string
  for successful operation or -WM_FAIL in case of failure to convert. Note
  that the output string is not NULL terminated.
 */
int load_wep_key(const uint8_t *input,
		uint8_t *output, uint8_t *output_len,
		const unsigned max_output_len)
{
	if (!input || !output)
		return -WM_FAIL;

	unsigned len = strlen((const char *)input);

	/* fixme: add macros here after mlan integration */
	if (len == 10 || len == 26) {
		/* Looks like this is an hexadecimal key */
		int ret =  hex2bin(input, output, max_output_len);
		if (ret == 0)
			return -WM_FAIL;

		len = len / 2;
	} else if (len == 5 || len == 13) {
		/* Looks like this is ASCII key  */
		if (len > max_output_len)
			return -WM_FAIL;

		memcpy(output, input, len);
	} else
		return -WM_FAIL;

	*output_len = len;

	return WM_SUCCESS;
}

int get_split_scan_delay_ms()
{
	return g_wifi_scan_params.split_scan_delay;
}

void wlan_get_firmware_version_ext(char *version_str)
{
	memcpy(version_str, wlan.fw_ver_ext.version_str, MLAN_MAX_VER_STR_LEN);
}

int wlan_sniffer_start(const uint16_t filter_flags, const uint8_t radio_type,
			const uint8_t channel,
			void (*sniffer_callback)(const wlan_frame_t *frame,
						 const uint16_t len))
{
	sniffer_register_callback(sniffer_callback);
	return wifi_sniffer_start(filter_flags, radio_type, channel);
}

int wlan_sniffer_status()
{
	return wifi_sniffer_status();
}

int wlan_sniffer_stop()
{
	sniffer_deregister_callback();
	return wifi_sniffer_stop();
}

uint16_t wlan_get_seq_num(uint16_t seq_frag_num)
{
	return seq_frag_num >> 4;
}

uint16_t wlan_get_frag_num(uint16_t seq_frag_num)
{
	return seq_frag_num & 0x0f;
}

unsigned int wlan_get_uap_supported_max_clients()
{
	return wlan.uap_supported_max_sta_num;
}

int wlan_get_uap_max_clients(unsigned int *max_sta_num)
{
	return wifi_get_uap_max_clients(max_sta_num);
}

int wlan_set_uap_max_clients(unsigned int max_sta_num)
{
	if (is_uap_started()) {
		wlcm_e("Cannot set the max station number "
				"as the uAP is already running");
		return -WM_FAIL;
	} else if (max_sta_num > wlan.uap_supported_max_sta_num) {
		wlcm_e("Maximum supported station number "
			"limit is = %d", wlan.uap_supported_max_sta_num);
		return -WM_FAIL;
	}

	return wifi_set_uap_max_clients(&max_sta_num);
}

int wlan_get_mgmt_ie(enum wlan_bss_type bss_type, unsigned int index,
			void *buf, unsigned int *buf_len)
{
	return wifi_get_mgmt_ie(bss_type, index, buf, buf_len);
}

int wlan_set_mgmt_ie(enum wlan_bss_type bss_type, IEEEtypes_ElementId_t id,
			void *buf, unsigned int buf_len)
{
	return wifi_set_mgmt_ie(bss_type, id, buf, buf_len);
}

int wlan_clear_mgmt_ie(enum wlan_bss_type bss_type, unsigned int index)
{
	return wifi_clear_mgmt_ie(bss_type, index);
}

int wlan_set_sta_tx_power(int power_level)
{
	return wifi_set_tx_power(power_level);
}

int wlan_get_sta_tx_power()
{
	return wifi_get_tx_power();
}
