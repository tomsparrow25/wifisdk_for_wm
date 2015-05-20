/*
 * Copyright 2008-2013, Marvell International Ltd.
 * All Rights Reserved.
 *
 */

#ifndef __SUPPLICANT_INT_H__
#define __SUPPLICANT_INT_H__

#ifndef ETH_ALEN
/** Ethernet address length */
#define ETH_ALEN 6
#endif

struct PACK_START ethhdr {
    /** Ethernet destination address */
	uint8_t h_dest[ETH_ALEN];
    /** Ethernet source address */
	uint8_t h_source[ETH_ALEN];
    /** Protocol */
	uint16_t h_proto;
} PACK_END;

typedef struct PACK_START supp_msg {
	supplicant_cmd_id_t cmd_id;
	union PACK_START msg_data {
		struct PACK_START cinfo_t {
			char *ssid;
			char *bssid;
		} PACK_END cinfo;
		struct PACK_START einfo_t {
			uint8_t interface;
			uint16_t len;
			uint8_t *buffer;
			uint8_t dst[ETH_ALEN];
		} PACK_END einfo;
		struct PACK_START sinfo_t {
			int security;
			struct wlan_cipher mcstCipher;
			struct wlan_cipher ucstCipher;
		} PACK_END sinfo;
		struct PACK_START ainfo_t {
			char *ssid;
			char *passphrase;
		} PACK_END ainfo;
	} PACK_END data;
	uint8_t mac[ETH_ALEN];
} PACK_END supp_msg_t;

#include <wmlog.h>

#define supp_e(...)				\
	wmlog_e("supp", ##__VA_ARGS__)
#define supp_w(...)				\
	wmlog_w("supp", ##__VA_ARGS__)

#ifdef CONFIG_SUPPLICANT_DEBUG
#define supp_d(...)				\
	wmlog("supp", ##__VA_ARGS__)
#else
#define supp_d(...)
#endif /* ! CONFIG_SUPPLICANT_DEBUG */

#endif	/* __SUPPLICANT_INT_H__ */
