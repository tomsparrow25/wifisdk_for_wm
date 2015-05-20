/* 
 *  Copyright 2008-2013, Marvell International Ltd.
 *  All Rights Reserved.
 */
#define INCLUDE_FROM_MLAN
#include <mlan_wmsdk.h>

#include <string.h>
#include <wifi.h>
#include <wmstdio.h>
#include <wm_os.h>
#include <wm_net.h>
#include <pwrmgr.h>

#include "wifi-internal.h"
#include "wifi-sdio.h"

#define WIFI_COMMAND_RESPONSE_WAIT_MS 10000

static os_thread_stack_define(wifi_drv_stack, 1024);

/* We don't see events coming in quick succession,
 * MAX_EVENTS = 10 is fairly big value */
#define MAX_EVENTS 20

static os_queue_pool_define(g_io_events_queue_data,
			    sizeof(struct bus_message) * MAX_EVENTS);

#define MAX_MCAST_LEN (MLAN_MAX_MULTICAST_LIST_SIZE * MLAN_MAC_ADDR_LENGTH)

int wifi_set_mac_multicast_addr(const char *mlist, uint32_t num_of_addr);
int wrapper_get_wpa_ie_in_assoc(uint8_t *wpa_ie);
KEY_TYPE_ID get_sec_info();

wm_wifi_t wm_wifi;

unsigned wifi_get_last_cmd_sent_ms()
{
	return wm_wifi.last_sent_cmd_msec;
}

void wifi_update_last_cmd_sent_ms()
{
	wm_wifi.last_sent_cmd_msec = os_ticks_to_msec(os_ticks_get());
}

static int wifi_get_command_resp_sem(unsigned long wait)
{
	wifi_d("Get Command resp sem");
	return os_semaphore_get(&wm_wifi.command_resp_sem, wait);
}

static int wifi_put_command_resp_sem(void)
{
	wifi_d("Put Command resp sem");
	return os_semaphore_put(&wm_wifi.command_resp_sem);
}

#define WL_ID_WIFI_CMD		"wifi_cmd"

int wifi_get_command_lock()
{
	wifi_d("Get Command lock");
	int rv = wakelock_get(WL_ID_WIFI_CMD);
	if (rv == WM_SUCCESS)
		rv = os_mutex_get(&wm_wifi.command_lock, OS_WAIT_FOREVER);

	return rv;
}

int wifi_get_mcastf_lock(void)
{
	wifi_d("Get Command lock");
	return os_mutex_get(&wm_wifi.mcastf_mutex, OS_WAIT_FOREVER);
}

int wifi_put_mcastf_lock(void)
{
	wifi_d("Put Command lock");
	return os_mutex_put(&wm_wifi.mcastf_mutex);
}

int wifi_put_command_lock()
{
	int rv = WM_SUCCESS;
	wifi_d("Put Command lock");
	rv = wakelock_put(WL_ID_WIFI_CMD);
	if (rv == WM_SUCCESS)
		rv = os_mutex_put(&wm_wifi.command_lock);

	return rv;
}

int wifi_wait_for_cmdresp(void *cmd_resp_priv)
{
	int ret;
	HostCmd_DS_COMMAND *cmd = wifi_get_command_buffer();
#if defined(CONFIG_ENABLE_WARNING_LOGS) || defined(CONFIG_WIFI_CMD_RESP_DEBUG)

	wcmdr_d("[%-8u] CMD --- : 0x%-2x Size: %-4d Seq: %-2d",
		os_get_timestamp(),
		cmd->command, cmd->size, cmd->seq_num);
#endif /* CONFIG_ENABLE_WARNING_LOGS || CONFIG_WIFI_CMD_RESP_DEBUG*/


	/*
	 * This is the private pointer. Only the command response handler
	 * function knows what it means or where it points to. It can be
	 * NULL.
	 */
	wm_wifi.cmd_resp_priv = cmd_resp_priv;

	if (cmd->size > MLAN_SDIO_BLOCK_SIZE) {
		/*
		 * This is a error added to be flagged during development
		 * cycle. It is not expected to occur in production. The
		 * legacy code below only sents out MLAN_SDIO_BLOCK_SIZE
		 * sized packet. If ever in future greater packet size
		 * generated then this error will help to localize the
		 * problem.
		 */
		wifi_e("cmd size greater than MLAN_SDIO_BLOCK_SIZE");
		return -WM_FAIL;
	}

	wifi_send_cmdbuffer(MLAN_SDIO_BLOCK_SIZE);
	/* Wait max 5 sec for the command response */
	ret = wifi_get_command_resp_sem(WIFI_COMMAND_RESPONSE_WAIT_MS);
	if (ret == WM_SUCCESS) {
		wifi_d("Got the command resp lock");
	} else {
#ifdef CONFIG_ENABLE_WARNING_LOGS
		wifi_w("Command response timed out. command = 0x%x",
		       cmd->command);
#endif /* CONFIG_ENABLE_WARNING_LOGS */
	}

	wifi_put_command_lock();
	return ret;
}

#ifdef CONFIG_P2P
void wifi_wfd_event(bool peer_event, void *data)
{
	struct wifi_wfd_event event;

	if (wm_wifi.wfd_event_queue) {
		event.peer_event = peer_event;
		event.data = data;
		os_queue_send(wm_wifi.wfd_event_queue, &event, OS_NO_WAIT);
	}
}
#endif

void wifi_event_completion(int event, enum wifi_event_reason result, void *data)
{
	struct wifi_message msg;
	if (!wm_wifi.wlc_mgr_event_queue) {
		wifi_d("No queue is registered. Ignoring");
		return;
	}

	msg.data = data;
	msg.reason = result;
	msg.event = event;
	os_queue_send(wm_wifi.wlc_mgr_event_queue, &msg, OS_NO_WAIT);
}

static int cmp_mac_addr(uint8_t *mac_addr1, uint8_t *mac_addr2)
{
	int i = 0;
	for (i = 0; i < MLAN_MAC_ADDR_LENGTH; i++)
		if (mac_addr1[i] != mac_addr2[i])
			return 1;
	return 0;
}

static int add_mcast_ip(uint8_t *mac_addr)
{
	mcast_filter *node_t, *new_node;
	wifi_get_mcastf_lock();
	node_t = wm_wifi.start_list;
	if (wm_wifi.start_list == NULL) {
		new_node = os_mem_alloc(sizeof(mcast_filter));
		if (new_node == NULL) {
			wifi_put_mcastf_lock();
			return -WM_FAIL;
		}
		memcpy(new_node->mac_addr, mac_addr, MLAN_MAC_ADDR_LENGTH);
		new_node->next = NULL;
		wm_wifi.start_list = new_node;
		wifi_put_mcastf_lock();
		return WM_SUCCESS;
	}
	while (node_t->next != NULL && cmp_mac_addr(node_t->mac_addr, mac_addr))
		node_t = node_t->next;

	if (!cmp_mac_addr(node_t->mac_addr, mac_addr)) {
		wifi_put_mcastf_lock();
		return -WM_FAIL;
	}
	new_node = os_mem_alloc(sizeof(mcast_filter));
	if (new_node == NULL) {
		wifi_put_mcastf_lock();
		return -WM_FAIL;
	}
	memcpy(new_node->mac_addr, mac_addr, MLAN_MAC_ADDR_LENGTH);
	new_node->next = NULL;
	node_t->next = new_node;
	wifi_put_mcastf_lock();
	return WM_SUCCESS;
}

static int remove_mcast_ip(uint8_t *mac_addr)
{
	mcast_filter *curr_node, *prev_node;
	wifi_get_mcastf_lock();
	curr_node = wm_wifi.start_list->next;
	prev_node = wm_wifi.start_list;
	if (wm_wifi.start_list == NULL) {
		wifi_put_mcastf_lock();
		return -WM_FAIL;
	}
	if (curr_node == NULL && cmp_mac_addr(prev_node->mac_addr, mac_addr)) {
		os_mem_free(prev_node);
		wm_wifi.start_list = NULL;
		wifi_put_mcastf_lock();
		return WM_SUCCESS;
	}
	/* If search element is at first location */
	if (!cmp_mac_addr(prev_node->mac_addr, mac_addr)) {
		wm_wifi.start_list = prev_node->next;
		os_mem_free(prev_node);
	}
	/* Find node in linked list */
	while (cmp_mac_addr(curr_node->mac_addr, mac_addr)
	       && curr_node->next != NULL) {
		prev_node = curr_node;
		curr_node = curr_node->next;
	}
	if (!cmp_mac_addr(curr_node->mac_addr, mac_addr)) {
		prev_node->next = curr_node->next;
		os_mem_free(curr_node);
		wifi_put_mcastf_lock();
		return WM_SUCCESS;
	}
	wifi_put_mcastf_lock();
	return -WM_FAIL;
}

static int make_filter_list(char *mlist, int maxlen)
{
	mcast_filter *node_t;
	uint8_t maddr_cnt = 0;
	wifi_get_mcastf_lock();
	node_t = wm_wifi.start_list;
	while (node_t != NULL) {
		memcpy(mlist, node_t->mac_addr, MLAN_MAC_ADDR_LENGTH);
		node_t = (struct mcast_filter *)node_t->next;
		mlist = mlist + MLAN_MAC_ADDR_LENGTH;
		maddr_cnt++;
		if (maddr_cnt > (maxlen / 6))
			break;
	}
	wifi_put_mcastf_lock();
	return maddr_cnt;
}

void wifi_get_ipv4_multicast_mac(uint32_t ipaddr, uint8_t *mac_addr)
{
	int i = 0, j = 0;
	uint32_t mac_addr_r = 0x01005E;
	ipaddr = ipaddr & 0x7FFFFF;
	/* Generate Multicast Mapped Mac Address for IPv4
	 * To get Multicast Mapped MAC address,
	 * To calculate 6 byte Multicast-Mapped MAC Address.
	 * 1) Fill higher 24-bits with IANA Multicast OUI (01-00-5E)
	 * 2) Set 24th bit as Zero
	 * 3) Fill lower 23-bits with from IP address (ignoring higher
	 * 9bits).
	 */
	for (i = 2; i >= 0; i--, j++)
		mac_addr[j] = (char)(mac_addr_r >> 8 * i) & 0xFF;

	for (i = 2; i >= 0; i--, j++)
		mac_addr[j] = (char)(ipaddr >> 8 * i) & 0xFF;
}

/*
  * fixme: MDNS works even without calling this API.
 */

int wifi_add_mcast_filter(uint8_t *mac_addr)
{
	char mlist[MAX_MCAST_LEN];
	int len, ret;
	/* If MAC address is 00:11:22:33:44:55,
	 * then pass mac_addr array in following format:
	 * mac_addr[0] = 00
	 * mac_addr[1] = 11
	 * mac_addr[2] = 22
	 * mac_addr[3] = 33
	 * mac_addr[4] = 44
	 * mac_addr[5] = 55
	 */
	ret = add_mcast_ip(mac_addr);
	if (ret != WM_SUCCESS)
		return ret;
	len = make_filter_list(mlist, MAX_MCAST_LEN);
	return wifi_set_mac_multicast_addr(mlist, len);
}

int wifi_remove_mcast_filter(uint8_t *mac_addr)
{
	char mlist[MAX_MCAST_LEN];
	int len, ret;
	/* If MAC address is 00:11:22:33:44:55,
	 * then pass mac_addr array in following format:
	 * mac_addr[0] = 00
	 * mac_addr[1] = 11
	 * mac_addr[2] = 22
	 * mac_addr[3] = 33
	 * mac_addr[4] = 44
	 * mac_addr[5] = 55
	 */
	ret = remove_mcast_ip(mac_addr);
	if (ret != WM_SUCCESS)
		return ret;
	len = make_filter_list(mlist, MAX_MCAST_LEN);
	ret = wifi_set_mac_multicast_addr(mlist, len);
	return ret;
}

/* Since we do not have the descriptor list we will using this adaptor function */
int wrapper_bssdesc_first_set(int bss_index,
			      uint8_t *BssId,
			      bool *is_ibss_bit_set,
			      int *ssid_len, uint8_t *ssid,
			      uint8_t *Channel,
			      uint8_t *RSSI,
			      _SecurityMode_t *WPA_WPA2_WEP,
			      _Cipher_t *mcstCipher,
			      _Cipher_t *ucstCipher);

int wrapper_bssdesc_second_set(int bss_index,
			       bool *phtcap_ie_present,
			       bool *htinfo_ie_present,
			       bool *wmm_ie_present,
			       uint8_t *band,
			       void *wps_IE_exist,
			       uint16_t *wps_session, void *wpa2_entp_IE_exist);

static struct wifi_scan_result common_desc;
int wifi_get_scan_result(unsigned int index, struct wifi_scan_result **desc)
{
	memset(&common_desc, 0x00, sizeof(struct wifi_scan_result));
	int rv = wrapper_bssdesc_first_set(index,
					   common_desc.bssid,
					   &common_desc.is_ibss_bit_set,
					   &common_desc.ssid_len,
					   common_desc.ssid,
					   &common_desc.Channel,
					   &common_desc.RSSI,
					   &common_desc.WPA_WPA2_WEP,
					   &common_desc.mcstCipher,
					   &common_desc.ucstCipher);
	if (rv != WM_SUCCESS) {
		wifi_e("wifi_get_scan_result failed");
		return rv;
	}

	/* Country info not populated */
	rv = wrapper_bssdesc_second_set(index,
					&common_desc.phtcap_ie_present,
					&common_desc.phtinfo_ie_present,
					&common_desc.wmm_ie_present,
					&common_desc.band,
					&common_desc.wps_IE_exist,
					&common_desc.wps_session,
					&common_desc.wpa2_entp_IE_exist
					);

	if (rv != WM_SUCCESS) {
		wifi_e("wifi_get_scan_result failed");
		return rv;
	}

	*desc = &common_desc;

	return WM_SUCCESS;
}

int wifi_register_event_queue(os_queue_t *event_queue)
{
	if (!event_queue)
		return -WM_E_INVAL;

	if (wm_wifi.wlc_mgr_event_queue)
		return -WM_FAIL;

	wm_wifi.wlc_mgr_event_queue = event_queue;
	return WM_SUCCESS;
}

int wifi_unregister_event_queue(os_queue_t * event_queue)
{
	if (!wm_wifi.wlc_mgr_event_queue
	    || wm_wifi.wlc_mgr_event_queue != event_queue)
		return -WM_FAIL;

	wm_wifi.wlc_mgr_event_queue = NULL;
	return WM_SUCCESS;
}

#ifdef CONFIG_P2P
int wifi_register_wfd_event_queue(os_queue_t *event_queue)
{

	if (wm_wifi.wfd_event_queue)
		return -WM_FAIL;

	wm_wifi.wfd_event_queue = event_queue;
	return WM_SUCCESS;
}

int wifi_unregister_wfd_event_queue(os_queue_t *event_queue)
{
	if (!wm_wifi.wfd_event_queue || wm_wifi.wfd_event_queue != event_queue)
		return -WM_FAIL;

	wm_wifi.wfd_event_queue = NULL;
	return WM_SUCCESS;
}
#endif

#define WL_ID_WIFI_MAIN_LOOP	"wifi_main_loop"

static void wifi_driver_main_loop(void *argv)
{
	int ret;
	struct bus_message msg;

	/* Main Loop */
	while (1) {
		ret = os_queue_recv(&wm_wifi.io_events, &msg, OS_WAIT_FOREVER);
		if (ret == WM_SUCCESS) {
			wakelock_get(WL_ID_WIFI_MAIN_LOOP);

			if (msg.event == MLAN_TYPE_EVENT) {
				wifi_handle_fw_event(&msg);
				/*
				 * Free the buffer after the event is
				 * handled.
				 */
				wifi_free_eventbuf(msg.data);
			} else if (msg.event == MLAN_TYPE_CMD) {
				wifi_process_cmd_response(msg.data +
							  INTF_HEADER_LEN);
				wifi_update_last_cmd_sent_ms();
				wifi_put_command_resp_sem();
			}

			wakelock_put(WL_ID_WIFI_MAIN_LOOP);
		}
	}
}

int wm_wifi_init(void)
{
	if (os_mutex_create(&wm_wifi.command_lock,
			    "userif cmd", OS_MUTEX_INHERIT)) {
		wifi_e("Create command lock failed");
		return -WM_FAIL;
	}

	if (os_semaphore_create(&wm_wifi.command_resp_sem, "userif cmdresp")) {
		wifi_e("Create command resp sem failed");
		os_mutex_delete(&wm_wifi.command_lock);
		return -WM_FAIL;
	}

	if (os_mutex_create(&wm_wifi.mcastf_mutex, "mcastf-mutex",
			    OS_MUTEX_INHERIT)) {
		wifi_e("Create multicast filter lock failed");
		os_mutex_delete(&wm_wifi.command_lock);
		os_semaphore_delete(&wm_wifi.command_resp_sem);
		return -WM_FAIL;
	}

	/*
	 * Take the cmd resp lock immediately so that we can later block on
	 * it.
	 */
	wifi_get_command_resp_sem(OS_WAIT_FOREVER);
	wm_wifi.io_events_queue_data = g_io_events_queue_data;
	int ret = os_queue_create(&wm_wifi.io_events, "io-events",
				  sizeof(struct bus_message),
				  &wm_wifi.io_events_queue_data);
	if (ret) {
		os_mutex_delete(&wm_wifi.command_lock);
		return -WM_FAIL;
	}

	ret = bus_register_event_queue(&wm_wifi.io_events);
	if (ret) {
		os_mutex_delete(&wm_wifi.command_lock);
		os_queue_delete(&wm_wifi.io_events);
		return -WM_FAIL;
	}

	ret = os_thread_create(&wm_wifi.wm_wifi_main_thread,
			       "wifi_driver",
			       wifi_driver_main_loop, NULL,
			       &wifi_drv_stack, OS_PRIO_3);
	if (ret) {
		os_mutex_delete(&wm_wifi.command_lock);
		os_queue_delete(&wm_wifi.io_events);
		return -WM_FAIL;
	}

	return WM_SUCCESS;
}

void wm_wifi_deinit()
{
	bus_deregister_event_queue();

	os_queue_delete(&wm_wifi.io_events);
	os_thread_delete(&wm_wifi.wm_wifi_main_thread);
	os_mutex_delete(&wm_wifi.command_lock);
	os_mutex_delete(&wm_wifi.mcastf_mutex);
}

int wifi_get_wpa_ie_in_assoc(uint8_t *wpa_ie)
{
	return wrapper_get_wpa_ie_in_assoc(wpa_ie);
}
