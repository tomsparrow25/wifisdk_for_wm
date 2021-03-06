/** @file wps_l2.c
 *  @brief This file contains functions handling layer 2 socket read/write.
 *  
 *  Copyright (C) 2003-2010, Marvell International Ltd.
 *  All Rights Reserved
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <lwip/sys.h>
#include <lwip/inet.h>
#include <wlan.h>
#include <wmstdio.h>

#include "wps_mem.h"
#include "wps_def.h"
#include "wps_util.h"
#include "wps_l2.h"
#include "wps_os.h"
#include "wps_eapol.h"

/********************************************************
        Local Variables
********************************************************/

/********************************************************
        Global Variables
********************************************************/
/** Global pwps information */
extern PWPS_INFO gpwps_info;
/** wps global */
extern WPS_DATA wps_global;

/********************************************************
        Local Functions
********************************************************/
/** 
 *  @brief Process Layer 2 socket receive function
 *  
 *  @param sock         Socket number for receiving
 *  @param l2_ctx       A pointer to user private information 
 *  @return             None
 */
void wps_l2_receive(const u8 *src_addr, const u8 *buf, size_t len)
{

	ENTER();

	wps_rx_eapol(src_addr, buf, len);

	LEAVE();
}

/********************************************************
        Global Functions
********************************************************/
/** 
 *  @brief Get Layer 2 MAC Address
 *  
 *  @param l2           A pointer to structure of layer 2 information
 *  @param addr         A pointer to returned buffer
 *  @return             WPS_STATUS_SUCCESS--success, otherwise--fail
 */
int wps_l2_get_mac(WPS_L2_INFO *l2, u8 *addr)
{
	ENTER();
	memcpy(addr, l2->my_mac_addr, ETH_ALEN);
	LEAVE();
	return WPS_STATUS_SUCCESS;
}

extern int wps_low_level_output(const u8 interface, const u8 *buf, u32 len);
/** 
 *  @brief Process Layer 2 socket send function
 *  
 *  @param l2           A pointer to structure of layer 2 information
 *  @param dst_addr     Destination address to send
 *  @param proto        Protocol number for layer 2 packet
 *  @param buf          A pointer to sending packet buffer
 *  @param len          Packet length
 *  @return             WPS_STATUS_SUCCESS--success, otherwise--fail
 */
int
wps_l2_send(WPS_L2_INFO *l2, const u8 *dst_addr, u16 proto,
	    const u8 *buf, size_t len)
{
	int ret = WPS_STATUS_SUCCESS, retry_cnt;
	void *buffer = (void *)wps_mem_malloc(ETH_FRAME_LEN);
	unsigned char *etherhead = buffer;
	unsigned char *data = buffer + 14;
	struct l2_ethhdr *eh = (struct l2_ethhdr *)etherhead;
	u16 protocol;

	ENTER();

	if (l2 == NULL || buffer == NULL) {
		LEAVE();
		return WPS_STATUS_FAIL;
	}

	memcpy((void *)(buffer), (void *)dst_addr, ETH_ALEN);
	memcpy((void *)(buffer + ETH_ALEN), (void *)l2->my_mac_addr, ETH_ALEN);
	protocol = (proto >> 8) | (proto << 8);
	eh->h_proto = protocol;
	memcpy(data, buf, len);
	len += 14;

	retry_cnt = 5;

	do {
		if (gpwps_info->role == WFD_ROLE)
			ret = wps_low_level_output(BSS_TYPE_WFD, buffer, len);
		else if (gpwps_info->role == WPS_REGISTRAR)
			ret = wps_low_level_output(BSS_TYPE_UAP, buffer, len);
		else
			ret = wps_low_level_output(BSS_TYPE_STA, buffer, len);

		if (ret < 0) {
			WPS_LOG("wps_l2_send failed... retrying\r\n", ret);
			retry_cnt--;
		} else
			retry_cnt = 0;

		os_thread_sleep(200);

	} while (retry_cnt != 0);

	if (buffer)
		wps_mem_free(buffer);

	LEAVE();
	return ret;
}

/** 
 *  @brief Process Layer 2 socket initialization
 *  
 *  @param ifname       Interface name
 *  @param protocol     Ethernet protocol number in host byte order
 *  @param rx_callback  Callback function that will be called for each received packet
 *  @param l2_hdr       1 = include layer 2 header, 0 = do not include header
 *  @return             A pointer to l2 structure
 */
WPS_L2_INFO *wps_l2_init(const char *ifname,
			 unsigned short protocol,
			 void (*rx_callback) (const u8 *src_addr,
					      const u8 *buf, size_t len))
{
	WPS_L2_INFO *l2;
	uint8_t mac[6];

	ENTER();

	l2 = wps_mem_malloc(sizeof(WPS_L2_INFO));
	if (l2 == NULL) {
		LEAVE();
		return NULL;
	}

	memset(l2, 0, sizeof(*l2));
	strncpy(l2->ifname, ifname, sizeof(l2->ifname));

#ifdef CONFIG_P2P
	if (wlan_get_wfd_mac_address(mac)) {
		WPS_LOG("Error: unable to retrieve MAC address\r\n");
		wps_mem_free(l2);
		return NULL;
	} else
		wps_printf(MSG_INFO, "%02X:%02X:%02X:%02X:%02X:%02X\r\n",
			   mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
#else
	if (wlan_get_mac_address(mac)) {
		WPS_LOG("Error: unable to retrieve MAC address\r\n");
		wps_mem_free(l2);
		return NULL;
	} else
		wps_printf(MSG_INFO, "%02X:%02X:%02X:%02X:%02X:%02X\r\n",
			   mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

#endif

	memcpy(l2->my_mac_addr, mac, ETH_ALEN);

	LEAVE();
	return l2;
}

/** 
 *  @brief Process Layer 2 socket free
 *  
 *  @param l2       A pointer to user private information 
 *  @return         None
 */
void wps_l2_deinit(WPS_L2_INFO *l2)
{
	ENTER();

	if (l2 == NULL) {
		LEAVE();
		return;
	}

	wps_mem_free(l2);

	LEAVE();
}
