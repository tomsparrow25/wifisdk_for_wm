/*
 * Copyright 2008-2013, Marvell International Ltd.
 * All Rights Reserved.
 *
 */

#ifndef __SUPPLICANT_H__
#define __SUPPLICANT_H__

#include <wlan.h>

typedef PACK_START enum
{
	SUPP_STA_INIT = 1,
	SUPP_STA_ALLOC,
	SUPP_STA_FREE,
	SUPP_STA_ENABLE,
	SUPP_STA_SET_PASSPHRASE,
	SUPP_STA_SET_PMK,
	SUPP_STA_SESS_INIT,
	SUPP_STA_SESS_FAILURE,
	SUPP_STA_DISABLE,
	SUPP_STA_DEAUTH_DATA,
	SUPP_UAP_INIT,
	SUPP_UAP_ALLOC,
	SUPP_UAP_FREE,
	SUPP_UAP_ENABLE,
	SUPP_UAP_SET_PASSPHRASE,
	SUPP_UAP_SESS_INIT,
	SUPP_UAP_DISABLE,
	SUPP_UAP_DEINIT,
	SUPP_UAP_ASSOC_DATA,
	SUPP_UAP_DEAUTH_DATA,
	SUPP_DATA,
} PACK_END supplicant_cmd_id_t;

/** enum : SUPP events */
enum supp_event {
	/** SUPP thread started */
	SUPP_STARTED = 0,
	/** SUPP PBC/PIN Session started */
	SUPP_SESSION_STARTED,
	/** SUPP Session registration timeout */
	SUPP_SESSION_TIMEOUT,
	/** SUPP Session attempt successful */
	SUPP_SESSION_SUCCESSFUL,
	/** SUPP Session failed */
	SUPP_SESSION_FAILED,
	/** SUPP start failed */
	SUPP_FAILED,
	/** SUPP thread stopped */
	SUPP_FINISHED
};

/* Supplicant APIs */

/** Start supplicant thread */
int supplicant_start(int (*supp_callback)
		(enum supp_event event, void *data, uint16_t len));

int supplicant_send_cmd(supplicant_cmd_id_t cmd_id, char *ssid, int security,
		struct wlan_cipher *mcstCipher, struct wlan_cipher *ucstCipher,
		char *passphrase, char *bssid, uint8_t *sta_mac);

/** Stop supplicant thread */
void supplicant_finish(void);

#endif
