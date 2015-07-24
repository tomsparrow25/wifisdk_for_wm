/*
 *  Copyright (C) 2008-2009, Marvell International Ltd.
 *  All Rights Reserved.
 */

#include <stdio.h>		/* for sscanf */
#include <wlan.h>
#ifdef CONFIG_WPS2
#include <wps.h>
#endif
#include <cli.h>
#include <cli_utils.h>
#include <wmstdio.h>
#include <string.h>
#include <pwrmgr.h>
#include <wm_net.h>		/* for net_inet_aton */
#include <board.h>
#include <wifi.h>
#include <wifidirectuaputl.h>
#include <wlan_tests.h>

#define WL_REGS32(x)    (*(volatile unsigned long *)(x))
#define WL_REGS32_SETBITS(reg, val) (WL_REGS32(reg) |= (val))
#define WL_REGS32_CLRBITS(reg, val) (WL_REGS32(reg) = \
				(WL_REGS32(reg)&~(val)))

/** bitmap for get auto deepsleep */
#define BITMAP_AUTO_DS         0x01
/** bitmap for sta power save */
#define BITMAP_STA_PS          0x10

/** bitmap for uap inactivity based PS */
#define BITMAP_UAP_INACT_PS    0x100


int wifi_send_rf_channel_cmd(wifi_rf_channel_t *rf_channel);
int wifi_get_set_rf_tx_power(u16_t cmd_action, wifi_tx_power_t *tx_power);
int wifi_send_power_save_command(int action, int ps_bitmap,
				 void *pdata_buf);
void wifi_configure_null_pkt_interval(unsigned int time_in_secs);
void wifi_set_ps_cfg(u16_t multiple_dtims,
		     u16_t bcn_miss_timeout,
		     u16_t local_listen_interval,
		     u16_t adhoc_wake_period,
		     u16_t mode,
		     u16_t delay_to_ps);
/*
 * Marvell Test Framework (MTF) functions
 */

void dump_rf_channel_info(const wifi_rf_channel_t *prf_channel)
{
	wmprintf("Channel: %d\n\r", prf_channel->current_channel);

	int rf_type = prf_channel->rf_type;
	wmprintf("RF type: %p\n\r", rf_type);
	int band = rf_type & 0x3;
	int channel_width = rf_type & 0xC >> 2;
	int sec_channel_offset = rf_type & 0x30 >> 4;
	wmprintf("Band: ");
	switch (band) {
	case 0:
		wmprintf("2.4 GHz\n\r");
		break;
	case 1:
		wmprintf("5 GHz\n\r");
		break;
	case 2:
		wmprintf("2 GHz\n\r");
		break;
	case 3:
		wmprintf("Reserved\n\r");
		break;
	}

	wmprintf("Channel width: ");
	switch (channel_width) {
	case 0:
		wmprintf("20 MHz\n\r");
		break;
	case 1:
		wmprintf("40 MHz\n\r");
		break;
	default:
		wmprintf("Reserved\n\r");
		break;
	}

	wmprintf("Secondary channel offset:");
	switch (sec_channel_offset) {
	case 0:
		wmprintf("No offset\n\r");
		break;
	case 1:
		wmprintf("Sec. channel above primary\n\r");
		break;
	case 3:
		wmprintf("Reserved\n\r");
		break;
	case 4:
		wmprintf("Sec. channel below primary\n\r");
		break;
	}
}

int wlan_regrdwr_getset(int argc, char *argv[])
{
	uint8_t action;
	uint32_t value;
	wifi_reg_t reg_type;
	int ret;
	if (argc != 4 && argc != 5)
		return -WM_FAIL;

	reg_type = atoi(argv[2]);
	if (argc == 4) {
		action = ACTION_GET;
		value = 0;
	} else {
		action = ACTION_SET;
		value = a2hex_or_atoi(argv[4]);
	}
	ret = wifi_reg_access(reg_type, action, a2hex_or_atoi(argv[3]),
		&value);
	if (ret == WM_SUCCESS) {
		if (action == ACTION_GET) {
			switch (reg_type) {
			case REG_MAC:
				wmprintf("MAC Reg 0x%x\r\n", value);
				break;
			case REG_BBP:
				wmprintf("BBP Reg 0x%x", value);
				break;
			case REG_RF:
				wmprintf("RF Reg 0x%x", value);
				break;
			default:
				wlcm_e("Read/write register failed");
			}
		} else
			wmprintf("Set the register successfully\r\n");
	} else {
		wlcm_e("Read/write register failed");
		return -WM_FAIL;
	}
	return WM_SUCCESS;
}


static void print_address(struct wlan_ip_config *addr)
{
	struct in_addr ip, gw, nm, dns1, dns2;
	char addr_type[10];
	ip.s_addr = addr->ip;
	gw.s_addr = addr->gw;
	nm.s_addr = addr->netmask;
	dns1.s_addr = addr->dns1;
	dns2.s_addr = addr->dns2;
	if (addr->addr_type == ADDR_TYPE_STATIC)
		strncpy(addr_type, "STATIC", sizeof(addr_type));
	else if (addr->addr_type == ADDR_TYPE_STATIC)
		strncpy(addr_type, "AUTO IP", sizeof(addr_type));
	else
		strncpy(addr_type, "DHCP", sizeof(addr_type));

	wmprintf("\taddress: %s", addr_type);
	wmprintf("\r\n\t\tIP:\t\t%s", inet_ntoa(ip));
	wmprintf("\r\n\t\tgateway:\t%s", inet_ntoa(gw));
	wmprintf("\r\n\t\tnetmask:\t%s", inet_ntoa(nm));
	wmprintf("\r\n\t\tdns1:\t\t%s", inet_ntoa(dns1));
	wmprintf("\r\n\t\tdns2:\t\t%s", inet_ntoa(dns2));
	wmprintf("\r\n");
}

static const char *print_role(enum wlan_bss_role role)
{
	switch (role) {
	case WLAN_BSS_ROLE_STA:
		return "Infra";
	case WLAN_BSS_ROLE_UAP:
		return "uAP";
	case WLAN_BSS_ROLE_ANY:
		return "any";
	}

	return "unknown";
}

static void print_network(struct wlan_network *network)
{
	wmprintf("\"%s\"\r\n\tSSID: %s\r\n\tBSSID: ", network->name,
		 network->ssid[0] ? network->ssid : "(hidden)");
	print_mac(network->bssid);
	if (network->channel)
		wmprintf("\r\n\tchannel: %d", network->channel);
	else
		wmprintf("\r\n\tchannel: %s", "(Auto)");
	wmprintf("\r\n\trole: %s\r\n", print_role(network->role));

	char *sec_tag = "\tsecurity";
	if (!network->security_specific) {
		sec_tag = "\tsecurity [Wildcard]";
	}
	switch (network->security.type) {
	case WLAN_SECURITY_NONE:
		wmprintf("%s: none\r\n", sec_tag);
		break;
	case WLAN_SECURITY_WEP_OPEN:
		wmprintf("%s: WEP (open)\r\n", sec_tag);
		break;
	case WLAN_SECURITY_WEP_SHARED:
		wmprintf("%s: WEP (shared)\r\n", sec_tag);
		break;
	case WLAN_SECURITY_WPA:
		wmprintf("%s: WPA\r\n", sec_tag);
		break;
	case WLAN_SECURITY_WPA2:
		wmprintf("%s: WPA2\r\n", sec_tag);
		break;
	case WLAN_SECURITY_WPA_WPA2_MIXED:
		wmprintf("%s: WPA/WPA2 Mixed\r\n", sec_tag);
		break;
	case WLAN_SECURITY_EAP_TLS:
		wmprintf("%s: (WPA2)EAP-TLS\r\n", sec_tag);
		break;
	default:
		break;
	}

	print_address(&network->address);
}

/* Parse the 'arg' string as "ip:ipaddr,gwaddr,netmask,[dns1,dns2]" into
 * a wlan_ip_config data structure */
static int get_address(char *arg, struct wlan_ip_config *address)
{
	char *ipaddr = NULL, *gwaddr = NULL, *netmask = NULL;
	char *dns1 = NULL, *dns2 = NULL;

	ipaddr = strstr(arg, "ip:");
	if (ipaddr == NULL)
		return -1;
	ipaddr += 3;

	gwaddr = strstr(ipaddr, ",");
	if (gwaddr == NULL)
		return -1;
	*gwaddr++ = 0;

	netmask = strstr(gwaddr, ",");
	if (netmask == NULL)
		return -1;
	*netmask++ = 0;

	dns1 = strstr(netmask, ",");
	if (dns1 != NULL) {
		*dns1++ = 0;
		dns2 = strstr(dns1, ",");
	}
	address->ip = net_inet_aton(ipaddr);
	address->gw = net_inet_aton(gwaddr);
	address->netmask = net_inet_aton(netmask);

	if (dns1 != NULL)
		address->dns1 = net_inet_aton(dns1);

	if (dns2 != NULL)
		address->dns2 = net_inet_aton(dns2);

	return 0;
}

int get_security(int argc, char **argv, enum wlan_security_type type,
			struct wlan_network_security *sec)
{
	if (argc < 1)
		return 1;

	switch (type) {
	case WLAN_SECURITY_WEP_OPEN:
	case WLAN_SECURITY_WEP_SHARED:
		if (argc < 2)
			return 1;
		if (string_equal(argv[0], "open"))
			sec->type = WLAN_SECURITY_WEP_OPEN;
		else if (string_equal(argv[0], "shared"))
			sec->type = WLAN_SECURITY_WEP_SHARED;
		else
			return 1;

		/* copy the PSK, either WEP40 or WEP128 */
		int rv = load_wep_key((const uint8_t *) argv[1],
				      (uint8_t *) sec->psk,
				      (uint8_t *) &sec->psk_len,
				      sizeof(sec->psk));
		if (rv < 0)
			return 1;

		break;
	case WLAN_SECURITY_WPA:
	case WLAN_SECURITY_WPA2:
		if (argc < 1)
			return 1;
		/* copy the PSK phrase */
		sec->psk_len = strlen(argv[0]);
		if (sec->psk_len < sizeof(sec->psk))
			strcpy(sec->psk, argv[0]);
		else
			return 1;
		sec->type = type;
		break;
	default:
		return 1;
	}

	return 0;
}

static int get_role(char *arg, enum wlan_bss_role *role)
{
	if (!arg)
		return 1;

	if (string_equal(arg, "sta")) {
		*role = WLAN_BSS_ROLE_STA;
		return 0;
	} else if (string_equal(arg, "uap")) {
		*role = WLAN_BSS_ROLE_UAP;
		return 0;
	}
	return 1;
}

/*
 * MTF Shell Commands
 */
static void dump_wlan_add_usage()
{
	wmprintf("Usage:\r\n");
	wmprintf("For Station interface\r\n");
	wmprintf("  For DHCP IP Address assignment:\r\n");
	wmprintf("    wlan-add <profile_name> ssid <ssid> [wpa2 <secret>]"
		"\r\n");
	wmprintf("  For static IP address assignment:\r\n");
	wmprintf("    wlan-add <profile_name> ssid <ssid>\r\n"
		 "    ip:<ip_addr>,<gateway_ip>,<netmask>\r\n");
	wmprintf("    [bssid <bssid>] [channel <channel number>]\r\n"
		 "    [wpa2 <secret>]\r\n");

	wmprintf("For Micro-AP interface\r\n");
	wmprintf("    wlan-add <profile_name> ssid <ssid>\r\n"
		 "    ip:<ip_addr>,<gateway_ip>,<netmask>\r\n");
	wmprintf("    role uap [bssid <bssid>]\r\n"
		 "    [channel <channelnumber>]\r\n");
	wmprintf("    [wpa2 <secret>]\r\n");

}

static void dump_wlan_set_regioncode_usage()
{
	wmprintf("Usage:\r\n");
	wmprintf("wlan-set-regioncode <region-code>\r\n");
	wmprintf("where, region code =\r\n");
	wmprintf("0x10 : US FCC, Singapore\r\n");
	wmprintf("0x20 : IC Canada\r\n");
	wmprintf("0x30 : ETSI, Australia, Republic of Korea\r\n");
	wmprintf("0x32 : France\r\n");
	wmprintf("0x40 : Japan\r\n");
	wmprintf("0x41 : Japan\r\n");
	wmprintf("0x50 : China\r\n");
	wmprintf("0xFE : Japan\r\n");
	wmprintf("0xFF : Special\r\n");
}

void test_wlan_add(int argc, char **argv)
{
	struct wlan_network network;
	int ret = 0;
	int arg = 1;
	struct {
		unsigned ssid:1;
		unsigned bssid:1;
		unsigned channel:1;
		unsigned address:2;
		unsigned security:1;
		unsigned role:1;
	} info;

	memset(&info, 0, sizeof(info));
	memset(&network, 0, sizeof(struct wlan_network));

	if (argc < 4) {
		dump_wlan_add_usage();
		wmprintf("Error: invalid number of arguments\r\n");
		return;
	}

	if (strlen(argv[arg]) >= WLAN_NETWORK_NAME_MAX_LENGTH) {
		wmprintf("Error: network name too long\r\n");
		return;
	}

	memcpy(network.name, argv[arg], strlen(argv[arg]));
	arg++;
	info.address = ADDR_TYPE_DHCP;
	do {
		if (!info.ssid && string_equal("ssid", argv[arg])) {
			if (strlen(argv[arg + 1]) > IEEEtypes_SSID_SIZE) {
				wmprintf("Error: SSID is too long\r\n");
				return;
			}
			memcpy(network.ssid, argv[arg + 1],
			       strlen(argv[arg + 1]));
			arg += 2;
			info.ssid = 1;
		} else if (!info.bssid && string_equal("bssid", argv[arg])) {
			ret = get_mac(argv[arg + 1], network.bssid, ':');
			if (ret) {
				wmprintf("Error: invalid BSSID argument"
					"\r\n");
				return;
			}
			arg += 2;
			info.bssid = 1;
		} else if (!info.channel && string_equal("channel", argv[arg])) {
			if (arg + 1 >= argc ||
			    get_uint(argv[arg + 1], &network.channel,
				     strlen(argv[arg + 1]))) {
				wmprintf("Error: invalid channel"
					" argument\n");
				return;
			}
			arg += 2;
			info.channel = 1;
		} else if (!strncmp(argv[arg], "ip:", 3)) {
			ret = get_address(argv[arg], &network.address);
			if (ret) {
				wmprintf("Error: invalid address"
					" argument\n");
				return;
			}
			arg++;
			info.address = ADDR_TYPE_STATIC;
		} else if (!info.security && string_equal("wep", argv[arg])) {
			ret =
				get_security(argc - arg - 1,
					     (char **)(argv + arg + 1),
					     WLAN_SECURITY_WEP_OPEN,
					     &network.security);
			if (ret) {
				wmprintf("Error: invalid WEP security"
					" argument\r\n");
				return;
			}
			arg += 3;
			info.security++;
		} else if (!info.security && string_equal("wpa", argv[arg])) {
			ret =
			    get_security(argc - arg - 1, argv + arg + 1,
					 WLAN_SECURITY_WPA, &network.security);
			if (ret) {
				wmprintf("Error: invalid WPA security"
					" argument\r\n");
				return;
			}
			arg += 2;
			info.security++;
		} else if (!info.security && string_equal("wpa2", argv[arg])) {
			ret =
			    get_security(argc - arg - 1, argv + arg + 1,
					 WLAN_SECURITY_WPA2, &network.security);
			if (ret) {
				wmprintf("Error: invalid WPA2 security"
					" argument\r\n");
				return;
			}
			arg += 2;
			info.security++;
		} else if (!info.role && string_equal("role", argv[arg])) {
			if (arg + 1 >= argc ||
			    get_role(argv[arg + 1], &network.role)) {
				wmprintf("Error: invalid wireless"
					" network role\r\n");
				return;
			}
			arg += 2;
			info.role++;
		} else if (!strncmp(argv[arg], "autoip", 6)) {
			info.address = ADDR_TYPE_LLA;
			arg++;
		} else {
			dump_wlan_add_usage();
			wmprintf("Error: argument %d is invalid\r\n", arg);
			return;
		}
	} while (arg < argc);

	if (!info.ssid && !info.bssid) {
		dump_wlan_add_usage();
		wmprintf("Error: specify at least the SSID or BSSID\r\n");
		return;
	}

	network.address.addr_type = info.address;

	ret = wlan_add_network(&network);
	switch (ret) {
	case WLAN_ERROR_NONE:
		wmprintf("Added \"%s\"\r\n", network.name);
		break;
	case WLAN_ERROR_PARAM:
		wmprintf("Error: that network already exists\r\n");
		break;
	case WLAN_ERROR_NOMEM:
		wmprintf("Error: network list is full\r\n");
		break;
	case WLAN_ERROR_STATE:
		wmprintf("Error: can't add networks in this state\r\n");
		break;
	default:
		wmprintf("Error: unable to add network for unknown"
			" reason\r\n");
		break;
	}
}

int __scan_cb(unsigned int count)
{
	struct wlan_scan_result res;
	int i;
	int err;

	if (count == 0) {
		wmprintf("no networks found\r\n");
		return 0;
	}

	wmprintf("%d network%s found:\r\n", count, count == 1 ? "" : "s");

	for (i = 0; i < count; i++) {
		err = wlan_get_scan_result(i, &res);
		if (err) {
			wmprintf("Error: can't get scan res %d\r\n", i);
			continue;
		}

		print_mac(res.bssid);

		if (res.ssid[0])
			wmprintf(" \"%s\" %s\r\n", res.ssid,
				       print_role(res.role));
		else
			wmprintf(" (hidden) %s\r\n",
				       print_role(res.role));

		wmprintf("\tchannel: %d\r\n", res.channel);
		wmprintf("\trssi: -%d dBm\r\n", res.rssi);
		wmprintf("\tsecurity: ");
		if (res.wep)
			wmprintf("WEP ");
		if (res.wpa && res.wpa2)
			wmprintf("WPA/WPA2 Mixed ");
		else {
			if (res.wpa)
				wmprintf("WPA ");
			if (res.wpa2)
				wmprintf("WPA2 ");
#ifdef CONFIG_WPA2_ENTP
			if (res.wpa2_entp)
				wmprintf("WPA2(EAP-TLS)");
#endif
		}
		if (!(res.wep || res.wpa || res.wpa2
#ifdef CONFIG_WPA2_ENTP
|| res.wpa2_entp
#endif
		))
			wmprintf("OPEN ");
		wmprintf("\r\n");

		wmprintf("\tWMM: %s\r\n", res.wmm ? "YES" : "NO");
#ifdef CONFIG_WPS2
		if (res.wps) {
			if (res.wps_session == WPS_SESSION_PBC)
				wmprintf("\tWPS: %s, Session: %s\r\n", "YES", "Push Button");
			else if (res.wps_session == WPS_SESSION_PIN)
				wmprintf("\tWPS: %s, Session: %s\r\n", "YES", "PIN");
			else
				wmprintf("\tWPS: %s, Session: %s\r\n", "YES", "Not active");
		}
		else
			wmprintf("\tWPS: %s \r\n", "NO");
#endif
	}

	return 0;
}

void test_wlan_scan(int argc, char **argv)
{
	if (wlan_scan(__scan_cb))
		wmprintf("Error: scan request failed\r\n");
	else
		wmprintf("Scan scheduled...\r\n");
}

static void test_wlan_remove(int argc, char **argv)
{
	int ret;

	if (argc < 2) {
		wmprintf("Usage: %s <profile_name>\r\n", argv[0]);
		wmprintf("Error: specify network to remove\r\n");
		return;
	}

	ret = wlan_remove_network(argv[1]);
	switch (ret) {
	case WLAN_ERROR_NONE:
		wmprintf("Removed \"%s\"\r\n", argv[1]);
		break;
	case WLAN_ERROR_PARAM:
		wmprintf("Error: network not found\r\n");
		break;
	case WLAN_ERROR_STATE:
		wmprintf("Error: can't remove network in this state\r\n");
		break;
	default:
	case WLAN_ERROR_ACTION:
		wmprintf("Error: unable to remove network\r\n");
		break;
	}
}

#if 0
extern int wlan_set_channel(char *name,int channel);
#endif

static void test_wlan_connect(int argc, char **argv)
{
    #if 0
    // usr for choose channel test
    if(argc >= 3) {
        int channel = atoi(argv[2]);
        wlan_set_channel(argc >= 2 ? argv[1] : NULL,channel);
    }
    #endif

	int ret = wlan_connect(argc >= 2 ? argv[1] : NULL);

	if (ret == WLAN_ERROR_STATE) {
		wmprintf("Error: connect manager not running\r\n");
		return;
	}

	if (ret == WLAN_ERROR_PARAM) {
		wmprintf("Usage: %s <profile_name>\r\n", argv[0]);
		wmprintf("Error: specify a network to connect\r\n");
		return;
	}
	wmprintf("Connecting to network...\r\nUse 'wlan-stat' for "
		"current connection status.\r\n");
}

static void test_wlan_start_network(int argc, char **argv)
{
	int ret;

	if (argc < 2) {
		wmprintf("Usage: %s <profile_name>\r\n", argv[0]);
		wmprintf("Error: specify a network to start\r\n");
		return;
	}

	ret = wlan_start_network(argv[1]);
	if (ret != WLAN_ERROR_NONE)
		wmprintf("Error: unable to start network\r\n");
}

void test_wlan_stop_network(int argc, char **argv)
{
	int ret;
	struct wlan_network network;

	wlan_get_current_uap_network(&network);
	ret = wlan_stop_network(network.name);
	if (ret != WLAN_ERROR_NONE)
		wmprintf("Error: unable to stop network\r\n");
}

void test_wlan_disconnect(int argc, char **argv)
{
	if (wlan_disconnect() != WLAN_ERROR_NONE)
		wmprintf("Error: unable to disconnect\r\n");
}

static void test_wlan_stat(int argc, char **argv)
{
	enum wlan_connection_state state;
	enum wlan_ps_mode ps_mode;
	enum wlan_uap_ps_mode ps_mode_uap;
	char ps_mode_str[12];

	if (wlan_get_ps_mode(&ps_mode)) {
		wmprintf("Error: unable to get power save"
				" mode\r\n");
		return;
	}

	switch (ps_mode) {
	case WLAN_IEEE:
		strcpy(ps_mode_str, "IEEE ps");
		break;
	case WLAN_DEEP_SLEEP:
		strcpy(ps_mode_str, "Deep sleep");
		break;
	case WLAN_PDN:
		strcpy(ps_mode_str, "Power down");
		break;
	case WLAN_ACTIVE:
	default:
		strcpy(ps_mode_str, "Active");
		break;
	}

	if (wlan_get_connection_state(&state)) {
		wmprintf("Error: unable to get STA connection"
						" state\r\n");
	} else {
		if (ps_mode != WLAN_PDN) {
			switch (state) {
			case WLAN_DISCONNECTED:
				wmprintf("Station disconnected (%s)\r\n",
					 ps_mode_str);
				break;
			case WLAN_SCANNING:
				wmprintf("Station scanning (%s)\r\n",
					ps_mode_str);
				break;
			case WLAN_ASSOCIATING:
				wmprintf("Station associating (%s)\r\n",
					ps_mode_str);
				break;
			case WLAN_ASSOCIATED:
				wmprintf("Station associated (%s)\r\n",
					 ps_mode_str);
				break;
			case WLAN_CONNECTING:
				wmprintf("Station connecting (%s)\r\n",
					ps_mode_str);
				break;
			case WLAN_CONNECTED:
				wmprintf("Station connected (%s)\r\n",
					ps_mode_str);
				break;
			default:
				wmprintf("Error: invalid STA state"
						" %d\r\n", state);
			}
		} else {
			wmprintf
			("Wireless card is in power down mode\r\n");
		}
	}
	if (wlan_get_uap_connection_state(&state)) {
		wmprintf("Error: unable to get uAP connection"
						" state\r\n");
	} else {
		if (ps_mode != WLAN_PDN) {
			switch (state) {
			case WLAN_UAP_STARTED:
				wlan_get_uap_ps_mode(&ps_mode_uap);
				switch (ps_mode_uap) {
				case WLAN_UAP_INACTIVITY_SLEEP:
					strcpy(ps_mode_str, "Inactivity PS");
					break;
				case WLAN_UAP_ACTIVE:
				default:
					strcpy(ps_mode_str, "Active");
					break;
				}
				wmprintf("uAP started (%s)\r\n",
					       ps_mode_str);
				break;
			case WLAN_UAP_STOPPED:
				wmprintf("uAP stopped\r\n");
				break;
			default:
				wmprintf("Error: invalid uAP state"
						" %d\r\n", state);
			}
		}
	}
}

static void test_wlan_list(int argc, char **argv)
{
	struct wlan_network network;
	unsigned int count;
	int i;

	if (wlan_get_network_count(&count)) {
		wmprintf("Error: unable to get number of networks\r\n");
		return;
	}

	wmprintf("%d network%s%s\r\n", count, count == 1 ? "" : "s",
		       count > 0 ? ":" : "");
	for (i = 0; i < count; i++) {
		if (wlan_get_network(i, &network) == WLAN_ERROR_NONE)
			print_network(&network);
	}
}

static void test_wlan_info(int argc, char **argv)
{
	enum wlan_connection_state state;
	struct wlan_network sta_network;
	struct wlan_network uap_network;
	int sta_found = 0;

	if (wlan_get_connection_state(&state)) {
		wmprintf("Error: unable to get STA connection"
						" state\r\n");
	} else {
		switch (state) {
		case WLAN_CONNECTED:
			if (!wlan_get_current_network(&sta_network)) {
				wmprintf("Station connected to:\r\n");
				print_network(&sta_network);
				sta_found = 1;
			} else
				wmprintf("Station not connected\r\n");
			break;
		default:
			wmprintf("Station not connected\r\n");
			break;
		}
	}

	if (wlan_get_current_uap_network(&uap_network))
		wmprintf("uAP not started\r\n");
	else {
		/* Since uAP automatically changes the channel to the one that
		 * STA is on */
		if (sta_found == 1)
			uap_network.channel = sta_network.channel;

		if (uap_network.role == WLAN_BSS_ROLE_UAP)
			wmprintf("uAP started as:\r\n");

		print_network(&uap_network);
	}
}

static void test_wlan_address(int argc, char **argv)
{
	struct wlan_network network;

	if (wlan_get_current_network(&network)) {
		wmprintf("not connected\r\n");
		return;
	}
	print_address(&network.address);
}

static void test_wlan_get_mac_address(int argc, char **argv)
{
	uint8_t mac[6];

	wmprintf("MAC address\r\n");
	if (wlan_get_mac_address(mac))
		wmprintf("Error: unable to retrieve MAC address\r\n");
	else
		wmprintf("%02X:%02X:%02X:%02X:%02X:%02X\r\n",
			       mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
#ifdef CONFIG_P2P
	wmprintf("P2P MAC address\r\n");
	if (wlan_get_wfd_mac_address(mac))
		wmprintf("Error: unable to retrieve P2P MAC address\r\n");
	else
		wmprintf("%02X:%02X:%02X:%02X:%02X:%02X\r\n",
			       mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
#endif
}

static void test_wlan_set_regioncode(int argc, char **argv)
{
	if (argc != 2) {
		dump_wlan_set_regioncode_usage();
		return;
	}

	t_u32 region_code = strtol(argv[1], NULL, 0);
	int rv = wifi_set_region_code(region_code);
	if (rv != WM_SUCCESS)
		wmprintf("Unable to set region code: 0x%x\r\n", region_code);
	else
		wmprintf("Region code: 0x%x set\r\n", region_code);
}

static void test_wlan_get_regioncode(int argc, char **argv)
{
	t_u32 region_code = 0;
	int rv = wifi_get_region_code(&region_code);
	if (rv != WM_SUCCESS)
		wmprintf("Unable to get region code: 0x%x\r\n", region_code);
	else
		wmprintf("Region code: 0x%x\r\n", region_code);
}

static void test_wlan_get_uapchannel(int argc, char **argv)
{
	int channel;
	int rv = wifi_get_uap_channel(&channel);
	if (rv != WM_SUCCESS)
		wmprintf("Unable to get channel: %d\r\n", rv);
	else
		wmprintf("uAP channel: %d\r\n", channel);
}

static void print_usage(void)
{
	wmprintf("\r\nUsage : pm-wifi-uap-inactivity-ps-enter");
	wmprintf(" CTRL INACTTO MIN_SLEEP MAX_SLEEP");
	wmprintf(" MIN_AWAKE MAX_AWAKE\r\n");
	wmprintf("CTRL: 0 - disable protection frame Tx before PS\r\n");
	wmprintf("      1 - enable protection frame Tx before PS\r\n");
	wmprintf("INACTTO: Inactivity timeout in miroseconds\r\n");
	wmprintf("MIN_SLEEP: Minimum sleep duration in microseconds\r\n");
	wmprintf("MAX_SLEEP: Maximum sleep duration in miroseconds\r\n");
	wmprintf("MIN_AWAKE: Minimum awake duration in microseconds\r\n");
	wmprintf("MAX_AWAKE: Maximum awake duration in microseconds\r\n");
}


static void cmd_wlan_uap_inactivity_on(int argc, char **argv)
{
	unsigned int params[6];
	int cnt = 0;
	for (cnt = 0; cnt < 6; cnt++) {
		params[cnt] = 0;
	}
	for (cnt = 0; cnt < argc ; cnt++) {
		params[cnt] = atoi(argv[cnt+1]);
	}
	int ret = wlan_uap_inact_sleep_ps_on(params[0], params[1], params[2],
				   params[3], params[4] , params[5]);
	if (ret == WLAN_ERROR_PARAM) {
		print_usage();
	}
}

static void cmd_wlan_uap_inactivity_off(int argc, char **argv)
{
	wlan_uap_inact_sleep_ps_off();
}

static void wlan_ps_usage_help()
{
	wmprintf("Pre-conditions :\r\n");
	wmprintf(" UAP network should be stopped"
		 " using command wlan-stop-network\r\n");
	wmprintf(" Station should be disconnected using "
		 "command wlan-disconnect\r\n");
	wmprintf(" Use command wlan-stat to see status\r\n");
}

static void cmd_wlan_deepsleepps_off(int argc, char **argv)
{
	if (wlan_deepsleepps_off() == WLAN_ERROR_STATE) {
		wmprintf("Wireless card is not in deep sleep mode\r\n");
		return;
	}
}

static void cmd_wlan_deepsleepps_on(int argc, char **argv)
{
	if (argc == 2) {
		if (!strcmp(argv[1], "-h")) {
			wlan_ps_usage_help();
			return;
		}
	}
	int retcode = WLAN_ERROR_NONE;
	retcode = wlan_deepsleepps_on();

	if (retcode == WLAN_ERROR_STATE) {
		wmprintf("Error entering  deep sleep mode\r\n");
		wlan_ps_usage_help();
	} else if (retcode == WLAN_ERROR_PS_ACTION) {
		wmprintf("Error entering deep sleep  mode UAP is up\r\n");
		wlan_ps_usage_help();
	}
}

static void wlan_ieeeps_usage_help()
{
	wmprintf("Usage:\r\n");
	wmprintf("pm-ieeeps-hs-cfg <enabled> "
		 "<wakeup condition>\r\n");
	wmprintf("enabled: 1 to enable\r\n");
	wmprintf("\t 0 to disable\r\n");
	wmprintf("wakeup conditions :");
	wmprintf("host wakeup conditions\r\n");
	wmprintf("\tbit0=1: broadcast data\r\n");
	wmprintf("\tbit1=1: unicast data\r\n");
	wmprintf("\tbit2=1: mac events\r\n");
	wmprintf("\tbit3=1: multicast data\r\n");
	wmprintf("\tbit4=1: arp broadcast data\r\n");
}

static void cmd_pm_ieeeps_hs_cfg(int argc, char **argv)
{
	if (argc < 2) {
		wlan_ieeeps_usage_help();
		return;
	}
	bool enabled = atoi(argv[1]);
	if ((argc < 3  && enabled) ||
	    !strcmp(argv[1], "-h")) {
		wlan_ieeeps_usage_help();
		return;
	}
	if (enabled > 1) {
		wlan_ieeeps_usage_help();
		return;
	}
	uint32_t conditions =  atoi(argv[2]);
	pm_ieeeps_hs_cfg(enabled, conditions);
}

static void cmd_wlan_pdnps_off(int argc, char **argv)
{
	if (wlan_pdnps_off() == WLAN_ERROR_STATE) {
		wmprintf("Wireless card is not in power down mode\r\n");
		return;
	}
}

static void cmd_wlan_pdnps_on(int argc, char **argv)
{
	if (argc == 2) {
		if (!strcmp(argv[1], "-h")) {
			wlan_ps_usage_help();
			return;
		}
	}
	int retcode = WLAN_ERROR_NONE;
	retcode = wlan_pdnps_on();
	if (retcode == WLAN_ERROR_STATE) {
		wmprintf("Error entering power down mode\r\n");
		wlan_ps_usage_help();

	} else if (retcode == WLAN_ERROR_PS_ACTION) {
		wmprintf("Error entering power down mode UAP is up\r\n");
		wlan_ps_usage_help();
	}
}

static void cmd_wlan_configure_listen_interval(int argc, char *argv[])
{
	unsigned time_in_milli_secs = 0;
	time_in_milli_secs = atoi(argv[1]);
	int ret = wlan_configure_listen_interval(time_in_milli_secs);
	if (ret != WM_SUCCESS)
		wmprintf(" Listen interval cannot be set \r\b");

}

static void cmd_wlan_gethostbyname(int argc, char **argv)
{
	enum wlan_connection_state state;
	int err;
	struct hostent *he;
	unsigned long ipaddr;
	char ipaddr_str[16];

	if (argc != 2) {
		wmprintf("Usage: %s <hostname>\r\n", argv[0]);
		wmprintf("Error: no hostname specified\r\n");
		return;
	}

	if (wlan_get_connection_state(&state)) {
		wmprintf("Error: unable to get connection state\r\n");
		return;
	}

	if (state != WLAN_CONNECTED) {
		wmprintf("Error: failed to gethostbyname."
			"  Not connected.\r\n");
		return;
	}

	err = net_gethostbyname(argv[1], &he);
	if (err != NET_SUCCESS) {
		wmprintf("Error: failed to gethostbyname: err=%d\r\n",
			       err);
		return;
	}
	memcpy(&ipaddr, he->h_addr_list[0], he->h_length);
	net_inet_ntoa(ipaddr, ipaddr_str);
	wmprintf("%s\r\n", ipaddr_str);
}

void cmd_wlan_deepsleep(int argc, char **argv)
{
	unsigned int status = 0;
	unsigned int duration = 0;
	int action = 0;

	if (argc == 2) {
		get_uint(argv[1], &status, strlen(argv[1]));
		action = (status == 1) ? EN_AUTO_PS : DIS_AUTO_PS;
	} else if (argc == 3) {
		get_uint(argv[1], &status, strlen(argv[1]));
		get_uint(argv[2], &duration, strlen(argv[2]));
		wmprintf("Entering Deep Sleep for %u milliseconds\r\n",
					duration);
		action = (status == 1) ? EN_AUTO_PS : DIS_AUTO_PS;
	} else {
		action = GET_PS;
	}

	wifi_send_power_save_command(action, BITMAP_AUTO_DS,
					(void *) &duration);
}

void cmd_wlan_pscfg(int argc, char **argv)
{
	int action = 0;
	unsigned int tmp;
	int i = 1;
	u16_t null_pkt_interval = 0;
	u16_t multiple_dtims = 0;
	u16_t bcn_miss_timeout = 0;
	u16_t local_listen_interval = 0;
	u16_t adhoc_wake_period = 0;
	u16_t mode = 0;
	u16_t delay_to_ps = 0;

	if (argc == 0)
		action = GET_PS;
	else {
		action = EN_AUTO_PS;
		if (i++ < argc) {
			get_uint(argv[i-1], &tmp, strlen(argv[i-1]));
			null_pkt_interval = tmp;
		}
		if (i++ < argc) {
			get_uint(argv[i-1], &tmp, strlen(argv[i-1]));
			multiple_dtims = tmp;
		}
		if (i++ < argc) {
			get_uint(argv[i-1], &tmp, strlen(argv[i-1]));
			local_listen_interval = tmp;
		}
		if (i++ < argc) {
			get_uint(argv[i-1], &tmp, strlen(argv[i-1]));
			bcn_miss_timeout = tmp;
		}

		if (i++ < argc) {
			get_uint(argv[i-1], &tmp, strlen(argv[i-1]));
			delay_to_ps = tmp;
		}
		if (i++ < argc) {
			get_uint(argv[i-1], &tmp, strlen(argv[i-1]));
			mode = tmp;
		}
	}

	wifi_configure_null_pkt_interval(null_pkt_interval);
	wifi_set_ps_cfg(multiple_dtims,
			bcn_miss_timeout,
			local_listen_interval,
			adhoc_wake_period,
			mode,
			delay_to_ps);

	wifi_send_power_save_command(action, BITMAP_STA_PS, NULL);
}

void cmd_wlan_get_rf_channel(int argc, char **argv)
{
	int ret;
	wifi_rf_channel_t rf_channel;
	ret = wifi_send_rf_channel_cmd(&rf_channel);

	if (ret == WM_SUCCESS)
		dump_rf_channel_info(&rf_channel);
	else
		wlcm_e("Failed to get rf channel");
}


/* fixme: remove this duplication when migration is complete */
/** General purpose action : Get */
#define HostCmd_ACT_GEN_GET                   0x0000
/** General purpose action : Set */
#define HostCmd_ACT_GEN_SET                   0x0001

void cmd_wlan_getset_tx_power(int argc, char **argv)
{
	int ret;
	int power_level;

	if (argc == 3) {
		/* Get the tx-power configuration */
		wlan_get_sta_tx_power();
	} else if (argc == 4) {
		/* Set the tx-power configuration */
		power_level = (uint8_t) atoi(argv[3]);
		ret = wlan_set_sta_tx_power(power_level);
		if (ret != WM_SUCCESS) {
			wlcm_e("Error in setting the tx power cfg");
		}
	}
}

const uint8_t dataRate[] = {
	0x0A, 0x14, 0x37, 0x6E, 0xDC,
	0x0B, 0x0F, 0x0A, 0x0E, 0x09,
	0x0D, 0x08, 0x0C, 0x07
};

#define DATARATE_1M  0
    // data rate 1 Mbps
#define DATARATE_2M  1
    // data rate 2 Mbps
#define DATARATE_5M  2
    // data rate 5.5 Mbps
#define DATARATE_11M 3
    // data rate 11 Mbps
#define DATARATE_22M 4
    // data rate 22 Mbps
#define DATARATE_6M  5
    // data rate 6 Mbps
#define DATARATE_9M  6
    // data rate 9 Mbps
#define DATARATE_12M 7
    // data rate 12 Mbps
#define DATARATE_18M 8
    // data rate 18 Mbps
#define DATARATE_24M 9
    // data rate 24 Mbps
#define DATARATE_36M  10
    // data rate 36 Mbps
#define DATARATE_48M  11
    // data rate 48 Mbps
#define DATARATE_54M  12
    // data rate 54 Mbps
#define DATARATE_72M 13
    // data rate 72 Mbps

static void cmd_wlan_set_tx_modulation(int argc, char **argv)
{
	if (argc < 2) {
		wmprintf("Usage: wlan-set-tx-modulation <cck/ofdm>\r\n");
		wmprintf("\t Mode can be one of: cck or ofdm");
		wmprintf("Error: invalid number of arguments\r\n");
		return;
	}
#define MTD_BY_PASS 0x8000a824
#define MTD_BY_PASS_VALUE 0x8000a828
#define BIT13	(0x00000001 << 13)
#define BIT7	(0x00000001 << 7)

	if (!strcmp(argv[1], "cck")) {
		WL_REGS32_SETBITS(MTD_BY_PASS, BIT13);
		WL_REGS32_SETBITS(MTD_BY_PASS_VALUE, BIT7);
	} else if (!strcmp(argv[1], "ofdm")) {
		// Set default rate to 54M
		*((uint16_t *) (0x8000a888)) = dataRate[DATARATE_54M];
		WL_REGS32_SETBITS(MTD_BY_PASS, BIT13);
		WL_REGS32_CLRBITS(MTD_BY_PASS_VALUE, BIT7);
	} else {
		wmprintf("Error: unknown mode\r\n");
		return;
	}
}

static void cmd_wlan_set_tx_test_rate(int argc, char **argv)
{
	uint16_t rateid;
	if (argc < 2) {
		wmprintf("Usage: wlan-set-tx-test-rate <rate_id>\r\n");
		wmprintf("\t rate_id is a valid 802.11 b/g rate");
		wmprintf("Error: invalid number of arguments\r\n");
		return;
	}

	sscanf(argv[1], "%x", &rateid);
	wmprintf("tx-test-rate: setting rateid 0x%x value 0x%x\r\n",
		       rateid, dataRate[rateid]);

#define MTD_TEST_TXRATE 0x8000a888

	*((uint16_t *) (0x8000a888)) = dataRate[rateid];
}


static struct cli_command tests[] = {
	{"wlan-scan", NULL, test_wlan_scan},
	{"wlan-add", "<profile_name> ssid <ssid> bssid...", test_wlan_add},
	{"wlan-remove", "<profile_name>", test_wlan_remove},
	{"wlan-list", NULL, test_wlan_list},
	{"wlan-connect", "<profile_name>", test_wlan_connect},
	{"wlan-start-network", "<profile_name>", test_wlan_start_network},
	{"wlan-stop-network", NULL, test_wlan_stop_network},
	{"wlan-disconnect", NULL, test_wlan_disconnect},
	{"wlan-stat", NULL, test_wlan_stat},
	{"wlan-info", NULL, test_wlan_info},
	{"wlan-address", NULL, test_wlan_address},
	{"wlan-mac", NULL, test_wlan_get_mac_address},
	{"wlan-set-regioncode", NULL, test_wlan_set_regioncode},
	{"wlan-get-regioncode", NULL, test_wlan_get_regioncode},
	{"wlan-get-uap-channel", NULL, test_wlan_get_uapchannel},
	{"pm-ieeeps-hs-cfg",
	 "(see pm-ieeeps-hs-cfg -h for details)",
	 cmd_pm_ieeeps_hs_cfg},
	{"pm-wifi-deepsleep-enter",
	 "(see pm-wifi-deepsleep-enter -h for details)",
	 cmd_wlan_deepsleepps_on},
	{"pm-wifi-deepsleep-exit", NULL, cmd_wlan_deepsleepps_off},
	{"pm-wifi-configure-listen-interval", "<time in milliseconds>",
	 cmd_wlan_configure_listen_interval},
	{"pm-wifi-uap-inactivity-ps-enter",
	"<min time in microseconds> <max time in microseconds>",
	 cmd_wlan_uap_inactivity_on},
	{"pm-wifi-uap-inactivity-ps-exit", NULL, cmd_wlan_uap_inactivity_off},
	{"wlan-gethostbyname", "<hostname>", cmd_wlan_gethostbyname},
};

static struct cli_command wlan_enhanced_commands[] = {
	{"wlan-get-rf-channel", NULL, cmd_wlan_get_rf_channel},
	{"wlan-set-tx-modulation", "<mode>", cmd_wlan_set_tx_modulation},
	{"wlan-set-tx-test-rate", "<rate_id>", cmd_wlan_set_tx_test_rate},
};

static struct cli_command pdn_tests[] = {
	{"pm-wifi-pdn-enter", "(see pm-wifi-pdn-enter -h for details)",
	 cmd_wlan_pdnps_on},
	{"pm-wifi-pdn-exit", NULL, cmd_wlan_pdnps_off},
};

/* Register our commands with the MTF. */
int wlan_cli_init(void)
{
	int i;

	for (i = 0; i < sizeof(tests) / sizeof(struct cli_command); i++)
		if (cli_register_command(&tests[i]))
			return WLAN_ERROR_ACTION;

	if (board_sdio_pdn_support())
		for (i = 0; i < sizeof(pdn_tests) / sizeof(struct cli_command);
									i++)
			if (cli_register_command(&pdn_tests[i]))
				return WLAN_ERROR_ACTION;
	return WLAN_ERROR_NONE;
}

int wlan_enhanced_cli_init(void)
{
		return WLAN_ERROR_NONE;
	if (cli_register_commands(wlan_enhanced_commands,
				  sizeof(wlan_enhanced_commands) /
				  sizeof(struct cli_command)))
		return WLAN_ERROR_ACTION;
	return WLAN_ERROR_NONE;
}
