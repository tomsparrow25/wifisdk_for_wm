/*
 *  Copyright 2008-2013, Marvell International Ltd.
 *  All Rights Reserved.
 */

/** provisioning_ezconnect.c: The EZConnect provisioning state machine
 */

/* Protocol description:
 * In this provisioning method, the WLAN radio starts sniffing each channel
 * for CHANNEL_SWITCH_INTERVAL. It looks for multicast MAC frames destined to
 * different IP addresses. It attempts to extract information from the least
 * significant 23 bits of multicast destination MAC address. Each 23 bits are
 * organized in following format
 *
 *	| 00           |  00000  |   00000000 |  00000000 |
 *	| Frame Type   |  Seq no |   data1    |  data0    |
 *
 * Preamble -
 * Frame Type	= 0b00
 * Seq no	= 0-2
 * data		= bssid bytes (0, 1), (2, 3), (4, 5) in (data0, data1)
 *
 * Length - Length of passphrase
 * Frame Type   = 0b01
 * Seq no       = 0
 * data		= (len, len) in (data0, data1)
 *
 * Passphrasse - Encrypted or plaintext passphrase
 * Frame Type	= 0b10
 * Seq no	= 0 - (len/2)
 * data		= Encrypted or plaintext passphrase byte by byte (LSB in data0)
 *
 * CRC - CRC32 value of plaintext passphrase
 * Frame Type   = 0b11
 * Seq no       = 0-1
 * data		= CRC32 value of plain text passphrase (LSB in data0)
 *
 */

#include <stdio.h>
#include <string.h>

#include <wmtypes.h>
#include <wmstdio.h>
#include <wm_os.h>
#include <wm_net.h>
#include <provisioning.h>
#include <mdev_aes.h>
#include "provisioning_int.h"

#define CHANNEL_SWITCH_INTERVAL		400 /* milliseconds */
extern struct prov_gdata prov_g;

static struct ezconn_state {
	uint8_t state;
	uint8_t substate;
	uint8_t pass_len;
	char cipher_pass[WLAN_PSK_MAX_LENGTH];
	char plain_pass[WLAN_PSK_MAX_LENGTH];
	uint32_t crc;
	uint8_t BSSID[6];
	uint8_t source[6];
	char ssid[33];
	int8_t security;
} es;

uint8_t ezconn_device_key[16];
static uint8_t ezconn_device_key_len;

static uint8_t iv[16];

os_semaphore_t ezconn_cs_sem;
os_timer_t ezconn_cs_timer;

#define DECRYPT(key, keylen, iv, cipher, plain, size)		\
	{							\
		aes_t enc_aes;					\
		mdev_t *aes_dev;				\
		aes_dev = aes_drv_open("MDEV_AES_0");		\
		aes_drv_setkey(aes_dev, &enc_aes, key, keylen,	\
			       iv, AES_DECRYPTION,		\
			       HW_AES_MODE_CBC);		\
		aes_drv_encrypt(aes_dev, &enc_aes,		\
				(uint8_t *) cipher,		\
				(uint8_t *) plain, size);	\
		aes_drv_close(aes_dev);				\
	}


int prov_ezconn_set_device_key(uint8_t *key, int len)
{
	if (len != 16) {
		prov_e("Invalid provisioning key length");
		return -WM_FAIL;
	}

	memcpy(ezconn_device_key, key, len);
	ezconn_device_key_len = len;
	return WM_SUCCESS;
}

void prov_ezconn_unset_device_key(void)
{
	memset(ezconn_device_key, 0, sizeof(ezconn_device_key));
	ezconn_device_key_len = 0;
	return;
}

static uint32_t crc32_tab[] = {
	0x00000000, 0x77073096, 0xee0e612c, 0x990951ba, 0x076dc419, 0x706af48f,
	0xe963a535, 0x9e6495a3,	0x0edb8832, 0x79dcb8a4, 0xe0d5e91e, 0x97d2d988,
	0x09b64c2b, 0x7eb17cbd, 0xe7b82d07, 0x90bf1d91, 0x1db71064, 0x6ab020f2,
	0xf3b97148, 0x84be41de,	0x1adad47d, 0x6ddde4eb, 0xf4d4b551, 0x83d385c7,
	0x136c9856, 0x646ba8c0, 0xfd62f97a, 0x8a65c9ec,	0x14015c4f, 0x63066cd9,
	0xfa0f3d63, 0x8d080df5,	0x3b6e20c8, 0x4c69105e, 0xd56041e4, 0xa2677172,
	0x3c03e4d1, 0x4b04d447, 0xd20d85fd, 0xa50ab56b, 0x35b5a8fa, 0x42b2986c,
	0xdbbbc9d6, 0xacbcf940,	0x32d86ce3, 0x45df5c75, 0xdcd60dcf, 0xabd13d59,
	0x26d930ac, 0x51de003a, 0xc8d75180, 0xbfd06116, 0x21b4f4b5, 0x56b3c423,
	0xcfba9599, 0xb8bda50f, 0x2802b89e, 0x5f058808, 0xc60cd9b2, 0xb10be924,
	0x2f6f7c87, 0x58684c11, 0xc1611dab, 0xb6662d3d, 0x76dc4190, 0x01db7106,
	0x98d220bc, 0xefd5102a, 0x71b18589, 0x06b6b51f, 0x9fbfe4a5, 0xe8b8d433,
	0x7807c9a2, 0x0f00f934, 0x9609a88e, 0xe10e9818, 0x7f6a0dbb, 0x086d3d2d,
	0x91646c97, 0xe6635c01, 0x6b6b51f4, 0x1c6c6162, 0x856530d8, 0xf262004e,
	0x6c0695ed, 0x1b01a57b, 0x8208f4c1, 0xf50fc457, 0x65b0d9c6, 0x12b7e950,
	0x8bbeb8ea, 0xfcb9887c, 0x62dd1ddf, 0x15da2d49, 0x8cd37cf3, 0xfbd44c65,
	0x4db26158, 0x3ab551ce, 0xa3bc0074, 0xd4bb30e2, 0x4adfa541, 0x3dd895d7,
	0xa4d1c46d, 0xd3d6f4fb, 0x4369e96a, 0x346ed9fc, 0xad678846, 0xda60b8d0,
	0x44042d73, 0x33031de5, 0xaa0a4c5f, 0xdd0d7cc9, 0x5005713c, 0x270241aa,
	0xbe0b1010, 0xc90c2086, 0x5768b525, 0x206f85b3, 0xb966d409, 0xce61e49f,
	0x5edef90e, 0x29d9c998, 0xb0d09822, 0xc7d7a8b4, 0x59b33d17, 0x2eb40d81,
	0xb7bd5c3b, 0xc0ba6cad, 0xedb88320, 0x9abfb3b6, 0x03b6e20c, 0x74b1d29a,
	0xead54739, 0x9dd277af, 0x04db2615, 0x73dc1683, 0xe3630b12, 0x94643b84,
	0x0d6d6a3e, 0x7a6a5aa8, 0xe40ecf0b, 0x9309ff9d, 0x0a00ae27, 0x7d079eb1,
	0xf00f9344, 0x8708a3d2, 0x1e01f268, 0x6906c2fe, 0xf762575d, 0x806567cb,
	0x196c3671, 0x6e6b06e7, 0xfed41b76, 0x89d32be0, 0x10da7a5a, 0x67dd4acc,
	0xf9b9df6f, 0x8ebeeff9, 0x17b7be43, 0x60b08ed5, 0xd6d6a3e8, 0xa1d1937e,
	0x38d8c2c4, 0x4fdff252, 0xd1bb67f1, 0xa6bc5767, 0x3fb506dd, 0x48b2364b,
	0xd80d2bda, 0xaf0a1b4c, 0x36034af6, 0x41047a60, 0xdf60efc3, 0xa867df55,
	0x316e8eef, 0x4669be79, 0xcb61b38c, 0xbc66831a, 0x256fd2a0, 0x5268e236,
	0xcc0c7795, 0xbb0b4703, 0x220216b9, 0x5505262f, 0xc5ba3bbe, 0xb2bd0b28,
	0x2bb45a92, 0x5cb36a04, 0xc2d7ffa7, 0xb5d0cf31, 0x2cd99e8b, 0x5bdeae1d,
	0x9b64c2b0, 0xec63f226, 0x756aa39c, 0x026d930a, 0x9c0906a9, 0xeb0e363f,
	0x72076785, 0x05005713, 0x95bf4a82, 0xe2b87a14, 0x7bb12bae, 0x0cb61b38,
	0x92d28e9b, 0xe5d5be0d, 0x7cdcefb7, 0x0bdbdf21,	0x86d3d2d4, 0xf1d4e242,
	0x68ddb3f8, 0x1fda836e, 0x81be16cd, 0xf6b9265b, 0x6fb077e1, 0x18b74777,
	0x88085ae6, 0xff0f6a70,	0x66063bca, 0x11010b5c, 0x8f659eff, 0xf862ae69,
	0x616bffd3, 0x166ccf45,	0xa00ae278, 0xd70dd2ee, 0x4e048354, 0x3903b3c2,
	0xa7672661, 0xd06016f7, 0x4969474d, 0x3e6e77db,	0xaed16a4a, 0xd9d65adc,
	0x40df0b66, 0x37d83bf0,	0xa9bcae53, 0xdebb9ec5, 0x47b2cf7f, 0x30b5ffe9,
	0xbdbdf21c, 0xcabac28a,	0x53b39330, 0x24b4a3a6,	0xbad03605, 0xcdd70693,
	0x54de5729, 0x23d967bf, 0xb3667a2e, 0xc4614ab8, 0x5d681b02, 0x2a6f2b94,
	0xb40bbe37, 0xc30c8ea1, 0x5a05df1b, 0x2d02ef8d
};

static uint32_t ezconn_crc32(uint32_t crc, const void *buf, size_t size)
{
	const uint8_t *p;

	p = buf;
	crc = crc ^ ~0U;

	while (size--)
		crc = crc32_tab[(crc ^ *p++) & 0xFF] ^ (crc >> 8);

	return crc ^ ~0U;
}


static inline int ezconn_match_crc(void)
{
	if (es.crc == ezconn_crc32(0, es.plain_pass, es.pass_len))
		return WM_SUCCESS;

	return -WM_FAIL;
}

static int ezconnect_sm_s0(const wlan_frame_t *frame, const uint16_t len)
{
	const char *dest, *bssid;

	dest = frame->frame_data.data_info.dest;
	bssid = frame->frame_data.data_info.bssid;

	if ((dest[3] == es.substate) &&
	    (dest[4] == bssid[es.substate * 2 + 1]) &&
	    (dest[5] == bssid[es.substate * 2])) {
		if (es.substate == 0) {
			memcpy(&es.BSSID, frame->frame_data.data_info.bssid, 6);
			memcpy(&es.source, frame->frame_data.data_info.src, 6);

		} else if (es.substate == 2) {
			es.state = 1;
			es.substate = 0;
			return 0;
		}
		es.substate++;
		return 0;
	}
	return 1;
}

static int ezconnect_sm_s1(const wlan_frame_t *frame, const uint16_t len)
{
	const char *dest;

	dest = frame->frame_data.data_info.dest;

	if ((dest[3] & 0x20) && (dest[4] == dest[5])) {
		es.pass_len = dest[4];
		if (es.pass_len == 0)
			es.state = 3;
		else
			es.state = 2;
		es.substate = 0;
		return 0;
	}
	return 1;
}

static int ezconnect_sm_s2(const wlan_frame_t *frame, const uint16_t len)
{
	const char *dest;
	dest = frame->frame_data.data_info.dest;

	if (dest[3] == (es.substate | 0x40)) {
		es.cipher_pass[es.substate * 2] = dest[5];
		es.cipher_pass[es.substate * 2 + 1] = dest[4];
		es.substate++;
		if ((es.substate * 2) >= es.pass_len) {
			es.state = 3;
			es.substate = 0;
		}
		return 0;
	}

	return 1;
}

static int ezconnect_sm_s3(const wlan_frame_t *frame, const uint16_t len)
{
	const char *dest;
	dest = frame->frame_data.data_info.dest;

	if (dest[3] == (es.substate | 0x60)) {
		es.crc |= (int) (dest[5] << ((es.substate * 2) * 8));
		es.crc |= (int) (dest[4] << (((es.substate * 2) + 1) * 8));
		es.substate++;
		if (es.substate == 2) {
			es.state = 4;
			es.substate = 0;
		}
		return 0;
	}

	return 1;
}

static void ezconnect_sm(const wlan_frame_t *frame, const uint16_t len)
{
	int ret = 1;

	/* We are not interested in non-multicast packets */
	if (!(frame->frame_data.data_info.dest[0] == 0x01 &&
	      frame->frame_data.data_info.dest[1] == 0x00 &&
	      frame->frame_data.data_info.dest[2] == 0x5e)) {
		return;
	}

	/* We are not interested in frames coming out of AP */
	if (!(frame->frame_data.data_info.frame_ctrl_flags & 0x01))
		return;

	/* Once we get the first packet of interest, we'll ignore other
	   packets from other sources */
	if (es.state + es.substate > 0) {
		if (memcmp(&frame->frame_data.data_info.src,
			   &es.source, 6) != 0)
			return;
	}

	switch (es.state) {
	case 0:
		ret = ezconnect_sm_s0(frame, len);
		break;
	case 1:
		ret = ezconnect_sm_s1(frame, len);
		break;
	case 2:
		ret = ezconnect_sm_s2(frame, len);
		break;
	case 3:
		ret = ezconnect_sm_s3(frame, len);
		if (es.state == 4) {
			if (ezconn_device_key_len) {
				if (es.pass_len % 16 != 0) {
					prov_e("Invalid passphrase length");
					return;
				}

				os_timer_deactivate(&ezconn_cs_timer);

				DECRYPT(ezconn_device_key,
					ezconn_device_key_len, iv,
					es.cipher_pass, es.plain_pass,
					es.pass_len);
				if (strlen(es.plain_pass) < es.pass_len) {
					es.pass_len = strlen(es.plain_pass);
				}
			} else {
				memcpy(es.plain_pass, es.cipher_pass,
				       es.pass_len);
			}
			if (ezconn_match_crc() == WM_SUCCESS) {
				es.state = 5;
				os_semaphore_put(&ezconn_cs_sem);
			} else {
				os_timer_activate(&ezconn_cs_timer);
				prov_e("Checksum mismatch");
				es.state = 0;
				es.substate = 0;
				return;
			}
		}
		break;
	default:
		break;
	}

	/* If state transition is successful, let's wait on the same channel
	   for more time */
	if (ret == WM_SUCCESS)
		os_timer_reset(&ezconn_cs_timer);
}

static void reinit_sm()
{
	memset((void *)&es, 0, sizeof(es));
}

static void ezconn_timer_cb(os_timer_arg_t handle)
{
	os_semaphore_put(&ezconn_cs_sem);
}

static int ezconnect_scan_cb(unsigned int count)
{
	int i;
	struct wlan_scan_result res;

	for (i = 0; i < count; i++) {
		wlan_get_scan_result(i, &res);
		if (memcmp(res.bssid, es.BSSID, 6) == 0) {
			strncpy(es.ssid, res.ssid, 32);
			es.security = WLAN_SECURITY_NONE;
			if (res.wpa && res.wpa2) {
				es.security = WLAN_SECURITY_WPA_WPA2_MIXED;
			} else if (res.wpa2) {
				es.security = WLAN_SECURITY_WPA2;
			} else if (res.wpa) {
				es.security = WLAN_SECURITY_WPA;
			} else if (res.wep) {
				es.security = WLAN_SECURITY_WEP_OPEN;
			}
			break;
		}
	}
	os_semaphore_put(&ezconn_cs_sem);
	return 0;
}

#define PROV_EZCONNECT_LOCK "prov_ezconnect_lock"
void prov_ezconnect(os_thread_arg_t thandle)
{
	int ret;
	int channel = 1;

	struct wlan_network *net;

	ret = os_semaphore_create(&ezconn_cs_sem, "ezconn_cs_sem");
	os_semaphore_get(&ezconn_cs_sem, OS_WAIT_FOREVER);

	ret = os_timer_create(&ezconn_cs_timer, "ezconn_cs_timer",
			      CHANNEL_SWITCH_INTERVAL,
			      ezconn_timer_cb, NULL, OS_TIMER_PERIODIC,
			      OS_TIMER_AUTO_ACTIVATE);

	ret = aes_drv_init();
	if (ret != WM_SUCCESS) {
		prov_e("Unable to initialize AES engine.\r\n");
		return;
	}
	wakelock_get(PROV_EZCONNECT_LOCK);
	while (1) {
		reinit_sm();

		ret = wlan_sniffer_start(0x04, 0x00, channel, ezconnect_sm);
		if (ret != WM_SUCCESS)
			prov_e("Error: wlan_sniffer_start failed.");

		os_semaphore_get(&ezconn_cs_sem, OS_WAIT_FOREVER);
		wlan_sniffer_stop();
		if (es.state == 5) {
			wmprintf("[prov] EZConnect provisioning done.\r\n");
			break;
		}

		channel++;
		if (channel == 12)
			channel = 1;
	}

	os_timer_deactivate(&ezconn_cs_timer);
	os_timer_delete(&ezconn_cs_timer);

	es.security = -1;

	while (es.security == -1) {
		wmprintf("[prov] WLAN Scanning...\r\n");
		ret = wlan_scan(ezconnect_scan_cb);
		if (ret != WM_SUCCESS) {
			prov_e("Scan request failed. Retrying.");
			os_thread_sleep(os_msec_to_ticks(100));
			continue;
		}

		os_semaphore_get(&ezconn_cs_sem, OS_WAIT_FOREVER);
	}

	net = &prov_g.current_net;

	snprintf(net->name, 8, "default");
	strncpy(net->ssid, es.ssid, 33);
	memcpy(net->bssid, es.BSSID, 6);
	net->security.type = es.security;
	if (es.security == WLAN_SECURITY_WEP_OPEN) {
		ret = load_wep_key((const uint8_t *) es.plain_pass,
				   (uint8_t *) net->security.psk,
				   (uint8_t *) &net->security.psk_len,
				   WLAN_PSK_MAX_LENGTH);
	} else {
		strncpy(net->security.psk, es.plain_pass, WLAN_PSK_MAX_LENGTH);
		net->security.psk_len = es.pass_len;
	}
	net->address.addr_type = 1; /* DHCP */
	prov_g.state = PROVISIONING_STATE_DONE;

	wmprintf("[prov] SSID: %s\r\n", es.ssid);
	wmprintf("[prov] Passphrase %s\r\n", es.plain_pass);
	wmprintf("[prov] Security %d\r\n", es.security);

	(prov_g.pc)->provisioning_event_handler(PROV_NETWORK_SET_CB,
						&prov_g.current_net,
						sizeof(struct wlan_network));
	wakelock_put(PROV_EZCONNECT_LOCK);
	os_thread_self_complete((os_thread_t *)thandle);
	return;
}
