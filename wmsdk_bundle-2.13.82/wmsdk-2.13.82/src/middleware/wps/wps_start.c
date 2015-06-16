/** @file wps_main.c
 *  @brief This file contains WPS application program entry function
 *           and functions for initialization setting.
 *
 *  Copyright (C) 2003-2010, Marvell International Ltd.
 *  All Rights Reserved
 */

#include <stdio.h>
#include <stdlib.h>
#include <lwip/sys.h>
#include <string.h>
#include <ctype.h>
#include <sys/types.h>
#include <lwip/sys.h>
#include <lwip/inet.h>
#include <wm_os.h>
#include <wlan.h>
#include <wps.h>

#include "wps_mem.h"
#include "wps_msg.h"
#include "wps_eapol.h"
#include "wps_util.h"
#include "wps_def.h"
#include "wps_wlan.h"
#include "wps_os.h"

/********************************************************
        Local Variables
********************************************************/

/********************************************************
        Global Variables
********************************************************/
extern u8 *g_ssid;
extern u8 *g_bssid;
extern u8 g_channel;
extern u32 g_gen_pin;

extern void wfd_reset();

extern struct wps_thread_t wps;
struct wlan_network wps_network;
struct wlan_network sta_network;
/** Global pwps information */
extern PWPS_INFO gpwps_info;

/********************************************************
        Local Functions
********************************************************/
/**
 *  @brief Check whether the mac address is all zero
 *  @param dev_addr    the mac address to be checked
 *  @return SUCCESS--all zero, FAILURE--not all zero
 */
static int is_mac_all_zero(u8 *dev_addr)
{
	int i;
	for (i = 0; i < ETH_ALEN; ++i) {
		if (dev_addr[i] != 0)
			return 0;
	}
	return 1;
}

#ifndef CONFIG_WPA2_ENTP
/**
 *  @brief WPS PIN value Generate
 *
 *  @param pwps_info    A pointer to WPS_INFO structure
 *  @return             None
 */
void wps_generate_PIN(PWPS_INFO pwps_info)
{
	int i;
	char pin_str[10];

	ENTER();

	if ((pwps_info->enrollee.device_password_id == DEVICE_PASSWORD_ID_PIN)
	    || (pwps_info->registrar.device_password_id ==
		DEVICE_PASSWORD_ID_PIN)) {
		/* Generate PIN */
		u32 pin;
		pin = g_gen_pin;
		sprintf(pin_str, "%d", pin);
		pwps_info->PINLen = strlen(pin_str);
		{
			u32 j, temppin = pin;
			u8 quotient;

			j = ((pwps_info->PINLen == 8) ? 10000000 : 1000);
			for (i = 0; i < pwps_info->PINLen; i++) {
				quotient = temppin / j;
				pwps_info->PIN[i] = quotient;
				/* Convert to numeric digit */
				pwps_info->PIN[i] += 0x30;
				temppin -= j * quotient;
				j = j / 10;
			}
		}
		wps_hexdump(DEBUG_INIT, "WPS PIN", (u8 *) pwps_info->PIN,
			    pwps_info->PINLen);

		WPS_LOG("WPS PIN is : ");
		for (i = 0; i < pwps_info->PINLen; i++)
			wmprintf("%c", pwps_info->PIN[i]);
		wmprintf("\r\n");
	}

	LEAVE();
}

/**
 * @brief  Validate checksum of PIN
 *
 * @param PIN WPS PIN value
 *
 * @returns WM_SUCCESS if successful, -WM_FAIL otherwise
 **/
int wps_validate_pin(uint32_t PIN)
{
	u32 accum = 0;
	char pin_str[10];

	ENTER();

	sprintf(pin_str, "%d", PIN);

	if (strlen(pin_str) == 4)
		return WM_SUCCESS;

	if (strlen(pin_str) != 8)
		return -WM_FAIL;

	accum += 3 * ((PIN / 10000000) % 10);
	accum += 1 * ((PIN / 1000000) % 10);
	accum += 3 * ((PIN / 100000) % 10);
	accum += 1 * ((PIN / 10000) % 10);
	accum += 3 * ((PIN / 1000) % 10);
	accum += 1 * ((PIN / 100) % 10);
	accum += 3 * ((PIN / 10) % 10);
	accum += 1 * ((PIN / 1) % 10);

	LEAVE();

	if (0 == (accum % 10))
		return WM_SUCCESS;
	else
		return -WM_FAIL;
}
#endif

/**
 *  @brief Process WPS Enrollee PIN mode and PBC user selection operations
 *
 *  @param pwps_info    A pointer to WPS_INFO structure
 *  @param wps_s        A pointer to global WPS structure
 *  @return             WPS_STATUS_SUCCESS--success, otherwise--fail
 */
int wps_enrollee_start(PWPS_INFO pwps_info, WPS_DATA *wps_s)
{
	int ret = WPS_STATUS_SUCCESS;
#ifndef CONFIG_WPA2_ENTP
	int retry_cnt = 5;
	int connect_retry = 10;
	enum wlan_connection_state state;
#endif

	ENTER();

	pwps_info->enrollee.auth_type_flag = AUTHENTICATION_TYPE_ALL;
	pwps_info->enrollee.encry_type_flag = ENCRYPTION_TYPE_ALL;

#ifndef CONFIG_WPA2_ENTP
	if (pwps_info->enrollee.device_password_id == DEVICE_PASSWORD_ID_PIN) {
		/* Generate PIN */
		wps_generate_PIN(pwps_info);

		if (pwps_info->pin_generator == WPS_ENROLLEE) {
			pwps_info->input_state = WPS_INPUT_STATE_NO_INPUT;
			pwps_info->pin_pbc_set = WPS_SET;
		} else {
			pwps_info->enrollee.updated_device_password_id =
			    DEVICE_PASSWORD_REG_SPECIFIED;
		}
	}

	memset(&wps_network, 0, sizeof(struct wlan_network));

	if (g_ssid && strlen((const char *)g_ssid))
		strncpy(wps_network.name, (const char *) g_ssid,
				MAX_SSID_LEN + 1);
	else
		strncpy(wps_network.name, "p2p_network", MAX_SSID_LEN + 1);
	memcpy(wps_network.ssid, (const char *) g_ssid, MAX_SSID_LEN + 1);
	memcpy(wps_network.bssid, (const char *) g_bssid, ETH_ALEN);
	wps_network.channel = g_channel;
	wps_network.address.addr_type = ADDR_TYPE_DHCP;
    #ifdef CONFIG_WPS2
	wps_network.wps_specific = 1;
    #endif
	if (g_channel)
		wps_network.channel_specific = 1;
	else if (is_mac_all_zero(g_bssid))
		wps_network.ssid_specific = 1;
	else
		wps_network.bssid_specific = 1;

	wps_network.security.type = WLAN_SECURITY_NONE;
#ifdef CONFIG_P2P
	wps_network.type = WLAN_BSS_TYPE_WIFIDIRECT;
	wps_network.role = WLAN_BSS_ROLE_STA;
#endif
	ret = wlan_add_network(&wps_network);

	if (ret != 0) {
		WPS_LOG(" Failed to add network %d\r\n", ret);
		goto fail;
	}

	do {
		WPS_LOG("Connecting to %s .....", wps_network.ssid);

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
				P2P_LOG("Error: unable to get "
					"connection state\r\n");
				continue;
			}

			if (state == WLAN_ASSOCIATED || connect_retry == 0)
				break;
#ifdef CONFIG_P2P
			connect_retry--;

		} while (state != WLAN_DISCONNECTED);
#else
		} while (connect_retry--);
#endif

		if (state != WLAN_ASSOCIATED) {
			WPS_LOG("Error: Not connected.\r\n");
			goto retry;
		}

		WPS_LOG("Connected to following BSS (or IBSS) :");
		WPS_LOG
		    ("SSID = [%s], BSSID = [%02x:%02x:%02x:%02x:%02x:%02x]",
		     wps_network.ssid,
		     wps_network.bssid[0],
		     wps_network.bssid[1],
		     wps_network.bssid[2],
		     wps_network.bssid[3],
		     wps_network.bssid[4], wps_network.bssid[5]);

		/* Save information to global structure */
		wps_s->current_ssid.ssid_len =
			strlen(wps_network.ssid);
		memcpy(wps_s->current_ssid.ssid,
		       wps_network.ssid, wps_s->current_ssid.ssid_len);

		memcpy(wps_s->current_ssid.bssid, wps_network.bssid, ETH_ALEN);

		/* Store Peer MAC Address */
		if (pwps_info->role == WPS_ENROLLEE
		    || (IS_DISCOVERY_ENROLLEE(pwps_info))
		    ) {
			if (is_mac_all_zero((u8 *) wps_network.bssid)) {
				if (wlan_get_current_network(&sta_network))
					goto fail;
				memcpy(pwps_info->registrar.mac_address,
					sta_network.bssid, ETH_ALEN);
			} else
				memcpy(pwps_info->registrar.mac_address,
					wps_network.bssid, ETH_ALEN);
		}
#else
		/* Save information to global structure */
		wps_s->current_ssid.ssid_len =
			strlen(wps_s->wpa2_network.ssid);
		memcpy(wps_s->current_ssid.ssid,
		       wps_s->wpa2_network.ssid, wps_s->current_ssid.ssid_len);

		memcpy(wps_s->current_ssid.bssid,
			wps_s->wpa2_network.bssid, ETH_ALEN);

		/* Store Peer MAC Address */
		if (pwps_info->role == WPS_ENROLLEE
		    || (IS_DISCOVERY_ENROLLEE(pwps_info))
		    ) {
			if (is_mac_all_zero((u8 *) wps_s->wpa2_network.bssid)) {
				if (wlan_get_current_network(&sta_network))
					goto fail;
				memcpy(pwps_info->registrar.mac_address,
					sta_network.bssid, ETH_ALEN);
			} else
				memcpy(pwps_info->registrar.mac_address,
					wps_s->wpa2_network.bssid, ETH_ALEN);
		}
#endif
#ifdef CONFIG_P2P
		WPS_LOG("Registration Process Started....");
#elif CONFIG_WPA2_ENTP
		WPS_LOG("Connected to following BSS (or IBSS) :");
		WPS_LOG
		    ("SSID = [%s], BSSID = [%02x:%02x:%02x:%02x:%02x:%02x]",
		     wps_s->wpa2_network.ssid,
		     wps_s->wpa2_network.bssid[0],
		     wps_s->wpa2_network.bssid[1],
		     wps_s->wpa2_network.bssid[2],
		     wps_s->wpa2_network.bssid[3],
		     wps_s->wpa2_network.bssid[4],
		     wps_s->wpa2_network.bssid[5]);
		WPS_LOG("WPA2 Enterprise (EAP-TLS) Protocol Started .....");
#else
		WPS_LOG("WPS Registration Protocol Started .....");

		if (wps.cb(WPS_SESSION_STARTED, NULL, 0) == -WM_FAIL)
			WPS_LOG("WPS Callback failed for event: %d\r\n",
				WPS_SESSION_STARTED);
#endif
		/* Start WPS registration timer */
		wps_start_registration_timer(pwps_info);

		/* Starting WPS Message Exchange Engine */

		wps_state_machine_start(pwps_info);

		ret = WPS_STATUS_SUCCESS;
#ifndef CONFIG_WPA2_ENTP
		break;

retry:
		retry_cnt--;
		WPS_LOG("Connect to AP FAIL ! Retrying ..... \r\n");
		wps_printf(DEBUG_WLAN | DEBUG_INIT, "Retry Count = %d",
			   retry_cnt);

	} while (retry_cnt != 0);

	if (retry_cnt == 0) {
fail:
		os_thread_sleep(os_msec_to_ticks(500));

#ifdef CONFIG_P2P
		WPS_LOG("Registration Protocol Failed !");
#elif CONFIG_WPA2_ENTP
		WPS_LOG("WPA2 Enterprise (EAP-TLS) Protocol Failed !");
#else
		WPS_LOG("WPS Registration Protocol Failed !");
#endif

		ret = wlan_disconnect();
		if (ret != 0)
			WPS_LOG("Failed to disconnect \r\n");

		ret = wlan_remove_network(wps_network.name);
		if (ret != 0)
			WPS_LOG("Failed to remove network %d\r\n", ret);
#ifdef CONFIG_P2P
		wfd_reset();

		gpwps_info->wps_session = 0;

		if (wps.cb(P2P_SESSION_FAILED, NULL, 0) == -WM_FAIL)
			WPS_LOG("P2P Callback failed for event: %d\r\n",
				P2P_SESSION_FAILED);
#else
		if (wps.cb(WPS_SESSION_FAILED, NULL, 0) == -WM_FAIL)
			WPS_LOG("WPS Callback failed for event: %d\r\n",
				WPS_SESSION_FAILED);
#endif
		return WPS_STATUS_FAIL;
	}
#else
fail:
#endif
	LEAVE();

	return ret;
}

#ifdef CONFIG_P2P
/**
 *  @brief Process WPS Registrar operations
 *
 *  @param pwps_info    A pointer to WPS_INFO structure
 *  @param wps_s        A pointer to global WPS structure
 *  @return             WPS_STATUS_SUCCESS--success, otherwise--fail
 */
int wps_registrar_start(PWPS_INFO pwps_info, WPS_DATA *wps_s)
{
	int ret = WPS_STATUS_SUCCESS;

	ENTER();

	if (!pwps_info->registrar.auth_type_flag)
		pwps_info->registrar.auth_type_flag = AUTHENTICATION_TYPE_ALL;
	if (!pwps_info->registrar.encry_type_flag)
		pwps_info->registrar.encry_type_flag = ENCRYPTION_TYPE_ALL;

	if (pwps_info->registrar.device_password_id == DEVICE_PASSWORD_ID_PIN) {
		/* Generate PIN */
		wps_generate_PIN(pwps_info);

		if (pwps_info->pin_generator == WPS_ENROLLEE) {
			pwps_info->input_state = WPS_INPUT_STATE_NO_INPUT;
			pwps_info->pin_pbc_set = WPS_SET;
		} else {
			pwps_info->enrollee.updated_device_password_id =
			    DEVICE_PASSWORD_REG_SPECIFIED;
		}
	}
#ifdef CONFIG_P2P
	WPS_LOG("Registration Process Started....");
#else
	WPS_LOG("WPS Registration Protocol Started .....");
#endif

	/* Start WPS registration timer */
	wps_start_registration_timer(pwps_info);

	/* Starting WPS Message Exchange Engine */
	wps_state_machine_start(pwps_info);

	LEAVE();
	return ret;
}
#else
int wps_registrar_start(PWPS_INFO pwps_info, WPS_DATA *wps_s)
{
	return WPS_STATUS_SUCCESS;
}
#endif
