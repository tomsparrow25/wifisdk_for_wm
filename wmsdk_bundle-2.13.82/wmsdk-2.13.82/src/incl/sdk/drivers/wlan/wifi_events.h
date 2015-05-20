/*! \file wifi_events.h
 *
 *  Copyright 2008-2013, Marvell International Ltd.
 *  All Rights Reserved.
 */

#ifndef __WIFI_EVENTS_H__
#define __WIFI_EVENTS_H__

/** Wifi events */
enum wifi_event {
	WIFI_EVENT_UAP_STARTED = 0,
	WIFI_EVENT_UAP_CLIENT_ASSOC,
	WIFI_EVENT_UAP_CLIENT_DEAUTH,
	WIFI_EVENT_UAP_NET_ADDR_CONFIG,
	WIFI_EVENT_UAP_DTIM_PS,
	WIFI_EVENT_UAP_INACT_PS,
	WIFI_EVENT_UAP_STOPPED,
	WIFI_EVENT_UAP_LAST,
	/* All the uAP releted events need to be above and STA related events
	   below */
	WIFI_EVENT_SCAN_RESULT,
	WIFI_EVENT_GET_HW_SPEC,
	WIFI_EVENT_ASSOCIATION,
	WIFI_EVENT_PMK,
	WIFI_EVENT_AUTHENTICATION,
	WIFI_EVENT_DISASSOCIATION,
	WIFI_EVENT_DEAUTHENTICATION,
	WIFI_EVENT_LINK_LOSS,
	WIFI_EVENT_NET_STA_ADDR_CONFIG,
	WIFI_EVENT_NET_INTERFACE_CONFIG,
	WIFI_EVENT_WEP_CONFIG,
	WIFI_EVENT_MAC_ADDR_CONFIG,
	WIFI_EVENT_NET_DHCP_CONFIG,
	WIFI_EVENT_SUPPLICANT_PMK,
	WIFI_EVENT_SLEEP,
	WIFI_EVENT_AWAKE,
	WIFI_EVENT_IEEE_PS,
	WIFI_EVENT_DEEP_SLEEP,
	WIFI_EVENT_PS_INVALID,
	WIFI_EVENT_HS_CONFIG,
	/* Add Block Ack */
	WIFI_EVENT_11N_ADDBA,
	WIFI_EVENT_11N_AGGR_CTRL,
	/* Channel switch */
	WIFI_EVENT_CHAN_SWITCH,
	WIFI_EVENT_LAST,
/* other events can be added after this, however this must
   be the last event in the wifi module */
};

enum wifi_event_reason {
	WIFI_EVENT_REASON_SUCCESS,
	WIFI_EVENT_REASON_TIMEOUT,
	WIFI_EVENT_REASON_FAILURE,
};

/** Network wireless BSS Type */
enum wlan_bss_type {
	WLAN_BSS_TYPE_STA = 0,
	WLAN_BSS_TYPE_UAP = 1,
	WLAN_BSS_TYPE_WIFIDIRECT = 2,
	WLAN_BSS_TYPE_ANY = 0xff,
};

/** Network wireless BSS Role */
enum wlan_bss_role {
	/** Infrastructure network. The system will act as a station connected
	 *  to an Access Point. */
	WLAN_BSS_ROLE_STA = 0,
	/** uAP (micro-AP) network.  The system will act as an uAP node to
	 * which other Wireless clients can connect. */
	WLAN_BSS_ROLE_UAP = 1,
	/** Either Infrastructure network or micro-AP network */
	WLAN_BSS_ROLE_ANY = 0xff,
};

/* fixme: Move this to wifi.h*/
/** This enum defines various wakeup events
 * for which wakeup will occur */
enum wifi_wakeup_event_t {
	/** Wakeup on ARP broad cast  */
	WIFI_WAKE_ON_ALL_BROADCAST = 1,
	/** Wakeup on uniccast  */
	WIFI_WAKE_ON_UNICAST = 1<<1,
	/** Wakeup on MAC event  */
	WIFI_WAKE_ON_MAC_EVENT = 1<<2,
	/** Wakeup on multicast  */
	WIFI_WAKE_ON_MULTICAST = 1<<3,
	/** Wakeup on  broad cast  */
	WIFI_WAKE_ON_ARP_BROADCAST = 1 << 4,
	/** Wakeup on receiving a management frame  */
	WIFI_WAKE_ON_MGMT_FRAME = 1<<6,
};


/* fixme : This enum should be mapped to enum of WLAN state.
 * This enum is used to check  power state on exit from PM4.
 * If WIFI is in PDN and reboot is caused due to PM4 exit
 * do firmware download on WIFI chip.
 * If WIFI is in DS and reboot is caused due to PM4 exit
 * skip firmware download on WIFI chip.
 * This enum is a replica of wlan_ps_mode in wlna.h
 * Replica will be removed later.
 *
 */

/** Enum for wifi card state */
enum wifi_state {
	/** Un-initialized state */
	WIFI_UNINIT = 0,
	/** Idle state */
	WIFI_INACTIVE,
	/** Active state*/
	WIFI_ACTIVE ,
	/** Deep sleep power save state */
	WIFI_DEEP_SLEEP,
	/** Power down power state */
	WIFI_PDN,
};

#endif  /*__WIFI_EVENTS_H__*/
