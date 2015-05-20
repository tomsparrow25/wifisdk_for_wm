/** @file wps_os.c
 *  @brief This file contains timer and socket read functions.
 *
 *  Copyright (C) 2003-2008, Marvell International Ltd.
 *  All Rights Reserved
 */

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <sys/types.h>
#include <wmtypes.h>
#include <wmtime.h>
#include <signal.h>
#include <ctype.h>
#include <lwip/sys.h>
#include <lwip/inet.h>
#include <wm_os.h>
#include <wps.h>

#include "wps_mem.h"
#include "wps_def.h"
#include "wps_util.h"
#include "wps_l2.h"
#include "wps_eapol.h"
#include "wps_os.h"
#include "wps_wlan.h"
#ifdef CONFIG_P2P
#include "wlan_hostcmd.h"
#include "wlan_wfd.h"
#endif

#ifdef CONFIG_WPA2_ENTP
void wpa2_session_complete();
#endif

/********************************************************
        Local Variables
********************************************************/
WPS_LOOP_S wps_loop;
extern struct wps_thread_t wps;
extern u8 auto_go;
extern u8 p2p_scan_on;
/********************************************************
        Global Variables
********************************************************/
/** WPS global */
extern WPS_DATA wps_global;
/** IE buffer index */
extern short ie_index;
/** IE buffer index */
extern short ap_assocresp_ie_index;

#ifdef CONFIG_P2P
extern void wfd_peer_ageout_time_handler(void *user_data);
#endif

/** Global pwps information */
extern PWPS_INFO gpwps_info;

/********************************************************
        Local Functions
********************************************************/
/**
 *  @brief Free structure used in main loop function
 *
 *  @return         None
 */
static void wps_main_loop_free(void)
{
	struct wps_timeout_s *timeout, *prev;

	ENTER();

	timeout = wps_loop.timeout;
	while (timeout != NULL) {
		prev = timeout;
		timeout = timeout->next;
		wps_mem_free(prev);
	}
	wps_mem_free(wps_loop.readers);

	LEAVE();
}

/********************************************************
        Global Functions
********************************************************/
#ifdef CONFIG_P2P
/**
 *  @brief Process reception of event from wlan
 *
 *  @param fd           File descriptor for reading
 *  @param context      A pointer to user private information
 *  @return             None
 */
void wps_peer_event_receive()
{
	u8 evt_buffer[EVENT_MAX_BUF_SIZE];
	int ret;
	WPS_DATA *wps_s = (WPS_DATA *) &wps_global;
	struct wfd_wlan_event event;
	u16 event_len;
	event_header *event_hdr;

	ENTER();

	while (1) {
		ret = os_queue_recv(&wps.peer_event_queue, &event,
					OS_WAIT_FOREVER);

		if (ret == WM_SUCCESS) {
			if (event.peer_event) {
				memcpy(&event_len, event.buffer +
						2 * sizeof(u16),
						sizeof(event_len));

				event_len += 2 * sizeof(u16);

				memcpy(evt_buffer, event.buffer, event_len);

				wps_wlan_peer_event_parser(wps_s,
					(char *) evt_buffer, event_len);
			} else {
				event_hdr = (event_header *) evt_buffer;
				if (!event.buffer)
					event_hdr->event_id =
						EV_ID_WFD_PEER_LINK_LOST;
				else
					event_hdr->event_id =
						EV_ID_WFD_PEER_CONNECTED;
				event_len = 4;
				wps_wlan_peer_event_parser(wps_s,
					(char *) evt_buffer, event_len);
			}
		}
	}
	LEAVE();
	return;
}

/**
 *  @brief Process reception of event from wlan
 *
 *  @param fd           File descriptor for reading
 *  @param context      A pointer to user private information
 *  @return             None
 */
void wps_event_receive(WPS_DATA *wps_s, WFD_DATA *pwfd_data)
{
	struct timeval tv, now;
	u8 evt_buffer[EVENT_MAX_BUF_SIZE];
	int ret;
	struct wfd_wlan_event event;
	u16 event_len;

	ENTER();

	wps_cancel_timer(wfd_peer_ageout_time_handler, pwfd_data);

	while (!gpwps_info->wps_session && wps_loop.timeout) {
		if (wps_loop.timeout) {

			now.tv_sec = os_ticks_to_msec(os_ticks_get()) / 1000;
			now.tv_usec =
			    (os_ticks_to_msec(os_ticks_get()) % 1000) * 1000;

			if (timer_cmp(&now, &wps_loop.timeout->time))
				timersub(&wps_loop.timeout->time, &now, &tv);
			else
				tv.tv_sec = tv.tv_usec = 0;
		}

		/* check if some registered timeouts have occurred */
		if (wps_loop.timeout) {
			struct wps_timeout_s *tmp;

			now.tv_sec = os_ticks_to_msec(os_ticks_get()) / 1000;
			now.tv_usec =
			    (os_ticks_to_msec(os_ticks_get()) % 1000) * 1000;

			if (!timer_cmp(&now, &wps_loop.timeout->time)) {
				tmp = wps_loop.timeout;
				wps_loop.timeout = wps_loop.timeout->next;
				tmp->handler(tmp->callback_data);
				wps_mem_free(tmp);
			}
		}

		ret = os_queue_recv(&wps.event_queue, &event,
						os_msec_to_ticks(50));

		if (ret == WM_SUCCESS) {
			memcpy(&event_len, event.buffer,
				sizeof(event_len));

			memcpy(evt_buffer, event.buffer, event_len);

			wps_wlan_event_parser(evt_buffer, event_len);
		}
	}

	wps_cancel_timer(wfd_peer_selected_ageout_time_handler, pwfd_data);

	if (!auto_go) {
		wfd_start_peer_ageout_timer(pwfd_data);
		p2p_scan_on = 1;
	}

	LEAVE();
	return;
}

#endif

/**
 *  @brief Process main loop initialization
 *
 *  @param wps_s    A pointer to global WPS structure
 *  @return         WPS_STATUS_SUCCESS--success, otherwise--fail
 */
int wps_loop_init(WPS_DATA *wps_s)
{
	ENTER();

	/* clear data structure for main processing loop */
	memset(&wps_loop, 0, sizeof(wps_loop));

	/* Initial and hook l2 callback function */
	wps_s->l2 = wps_l2_init(wps_s->ifname, ETH_P_EAPOL, wps_rx_eapol);

	if (wps_s->l2 == NULL) {
		wps_printf(DEBUG_INIT, "Init l2 return NULL !\n");
		LEAVE();
		return WPS_STATUS_FAIL;
	}

	if (wps_s->l2 && wps_l2_get_mac(wps_s->l2, wps_s->my_mac_addr)) {
		wps_printf(DEBUG_INIT, "Failed to get own L2 address\n");
		LEAVE();
		return WPS_STATUS_FAIL;
	}

	wps_printf(DEBUG_INIT, "Own MAC address: " MACSTR,
		   MAC2STR(wps_s->my_mac_addr));

	LEAVE();
	return WPS_STATUS_SUCCESS;
}

/**
 *  @brief Process interface deinit
 *
 *  @param wps_s        A pointer to global WPS structure
 *  @return             None
 */
void wps_intf_deinit(WPS_DATA *wps_s)
{
	ENTER();

	if (wps_s == NULL) {
		LEAVE();
		return;
	}

	wps_l2_deinit(wps_s->l2);

	LEAVE();
}

/**
 *  @brief Process main loop free
 *
 *  @param wps_s        A pointer to global WPS structure
 *  @return             None
 */
void wps_loop_deinit(WPS_DATA *wps_s)
{
	ENTER();

	if (wps_s == NULL) {
		LEAVE();
		return;
	}

	wps_main_loop_free();

	LEAVE();
}

/**
 *  @brief Disable main loop
 *
 *  @return         None
 */
void wps_main_loop_shutdown(void)
{
	ENTER();
	wps_loop.terminate = WPS_SET;
	LEAVE();
}

/**
 *  @brief Enable main loop
 *
 *  @return         None
 */
void wps_main_loop_enable(void)
{
	ENTER();
	wps_loop.terminate = WPS_CANCEL;
	LEAVE();
}

/**
 *  @brief Register a time-out handler to main loop timer function
 *
 *  @param secs             Time-out value in seconds
 *  @param usecs            Time-out value in micro-seconds
 *  @param handler          A function pointer of time-out handler
 *  @param callback_data    Private data for callback
 *  @return         WPS_STATUS_SUCCESS--success, otherwise--fail
 */
int
wps_start_timer(unsigned int secs,
		unsigned int usecs,
		void (*handler) (void *user_data), void *callback_data)
{
	struct wps_timeout_s *timeout, *tmp, *prev;

	ENTER();

	timeout = (struct wps_timeout_s *)wps_mem_malloc(sizeof(*timeout));
	if (timeout == NULL)
		return WPS_STATUS_FAIL;

	timeout->time.tv_sec = os_ticks_to_msec(os_ticks_get()) / 1000;
	timeout->time.tv_usec = (os_ticks_to_msec(os_ticks_get()) % 1000)
		* 1000;
	timeout->time.tv_sec += secs;
	timeout->time.tv_usec += usecs;
	while (timeout->time.tv_usec >= 1000000) {
		timeout->time.tv_sec++;
		timeout->time.tv_usec -= 1000000;
	}

	timeout->callback_data = callback_data;
	timeout->handler = handler;
	timeout->next = NULL;

	if (wps_loop.timeout == NULL) {
		wps_loop.timeout = timeout;
		LEAVE();
		return WPS_STATUS_SUCCESS;
	}

	prev = NULL;
	tmp = wps_loop.timeout;
	while (tmp != NULL) {
		if (timer_cmp(&timeout->time, &tmp->time))
			break;
		prev = tmp;
		tmp = tmp->next;
	}

	if (prev == NULL) {
		timeout->next = wps_loop.timeout;
		wps_loop.timeout = timeout;
	} else {
		timeout->next = prev->next;
		prev->next = timeout;
	}

	LEAVE();
	return WPS_STATUS_SUCCESS;
}

/**
 *  @brief Cancel time-out handler to main loop timer function
 *
 *  @param handler          Time-out handler to be canceled
 *  @param callback_data    Private data for callback
 *  @return         Number of timer being removed
 */
int wps_cancel_timer(void (*handler) (void *timeout_ctx), void *callback_data)
{
	struct wps_timeout_s *timeout, *prev, *next;
	int removed = 0;

	ENTER();

	prev = NULL;
	timeout = wps_loop.timeout;
	while (timeout != NULL) {
		next = timeout->next;

		if ((timeout->handler == handler) &&
		    (timeout->callback_data == callback_data)) {
			if (prev == NULL)
				wps_loop.timeout = next;
			else
				prev->next = next;
			wps_mem_free(timeout);
			removed++;
		} else
			prev = timeout;

		timeout = next;
	}

	LEAVE();
	return removed;
}

/**
 *  @brief Main loop procedure for socket read and timer functions
 *
 *  @return             None
 */
void wps_main_loop_proc(void)
{
	struct timeval tv, now;
	struct wps_msg msg;
	int ret;

	ENTER();

	while (!wps_loop.terminate) {
		if (wps_loop.timeout) {

			now.tv_sec = os_ticks_to_msec(os_ticks_get()) / 1000;
			now.tv_usec =
			    (os_ticks_to_msec(os_ticks_get()) % 1000) * 1000;

			if (timer_cmp(&now, &wps_loop.timeout->time))
				timersub(&wps_loop.timeout->time, &now, &tv);
			else
				tv.tv_sec = tv.tv_usec = 0;
		}

		/* check if some registered timeouts have occurred */
		if (wps_loop.timeout) {
			struct wps_timeout_s *tmp;

			now.tv_sec = os_ticks_to_msec(os_ticks_get()) / 1000;
			now.tv_usec =
			    (os_ticks_to_msec(os_ticks_get()) % 1000) * 1000;

			if (!timer_cmp(&now, &wps_loop.timeout->time)) {
				tmp = wps_loop.timeout;
				wps_loop.timeout = wps_loop.timeout->next;
				tmp->handler(tmp->callback_data);
				wps_mem_free(tmp);
			}
		}

#ifdef CONFIG_WPA2_ENTP
		ret = os_queue_recv(&wps.data_queue, &msg,
					os_msec_to_ticks(500));
#else
		ret = os_queue_recv(&wps.data_queue, &msg,
					os_msec_to_ticks(50));
#endif
		if (ret == WM_SUCCESS) {
#ifndef CONFIG_WPA2_ENTP
			if (gpwps_info->wps_session) {
#endif
#ifdef CONFIG_WPA2_ENTP
				if ((msg.new_msg_type >
					gpwps_info->last_recv_wps_msg)
				    || (msg.new_msg_type == WPS_EAP_TLS_MSG))
#else
				if ((msg.new_msg_type >
					gpwps_info->last_recv_wps_msg)
				    || (msg.new_msg_type <
					gpwps_info->eap_msg_sent))
#endif
				{
					wps_l2_receive(msg.mac,
							msg.buffer, msg.len);
				}
#ifndef CONFIG_WPA2_ENTP
			}
#endif
			wps_mem_free(msg.buffer);
		}
	}

#ifdef CONFIG_WPA2_ENTP
	wpa2_session_complete();
#endif
	/*Drain queue.*/
	while (os_queue_get_msgs_waiting(&wps.data_queue))
		os_queue_recv(&wps.data_queue, &msg,
					os_msec_to_ticks(0));

	LEAVE();
}
