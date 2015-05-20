
/*
 *  Copyright 2008-2013, Marvell International Ltd.
 *  All Rights Reserved.
 */

#include <stdio.h>
#include <wlan.h>
#include <wlan_tests.h>
#include <cli.h>
#include <cli_utils.h>
#include <wmstdio.h>
#include <string.h>
#include <pwrmgr.h>
#include <stdint.h>
#include <wm_net.h>
#include <wifi.h>

/*fixme*/
/** General purpose action : Get */
#define HostCmd_ACT_GEN_GET                   0x0000
/** General purpose action : Set */
#define HostCmd_ACT_GEN_SET                   0x0001

int wifi_send_rssi_info_cmd(wifi_rssi_info_t *rssi_info);
int wifi_send_get_datarate(wifi_tx_rate_t *tx_rate);
extern int wlan_regrdwr_getset(int argc, char *argv[]);
extern int cmd_wlan_getset_tx_power(int argc, char *argv[]);
/*
 * Marvell Test Framework (MTF) functions
 */

static void dump_ifconfig_usage()
{
	wmprintf("Usage: ifconfig <mlan0/uap0> <up/down>\r\n");
}

static void dump_iwlist_usage()
{
	wmprintf("Usage: iwlist <interface> <command>\r\n");
	wmprintf("\r\nFor scanning the networks:\r\n");
	wmprintf("iwlist <interface> scan\r\n");
	wmprintf("\r\nFor getting the current channel number:\r\n");
	wmprintf("iwlist <interface> channel\r\n");

}

static void dump_iwpriv_usage()
{
	wmprintf("Usage: iwpriv <interface> <command>\r\n");
	wmprintf("\r\nFor getting the Firmware version:\r\n");
	wmprintf("iwpriv <interface> version\r\n");
	wmprintf("\r\nFor getting the Firmware version ext:\r\n");
	wmprintf("iwpriv <interface> verext\r\n");
	wmprintf("\r\nFor getting the stations list:\r\n");
	wmprintf("iwpriv <interface> sta_list\r\n");
	wmprintf("\r\nFor disconnecting from the network:\r\n");
	wmprintf("iwpriv <interface> deauth\r\n");
	wmprintf("\r\nFor connecting to a network with wpa2 security:\r\n");
	wmprintf("iwpriv <interface> passphrase \"1;ssid=<ssid>;"
		"passphrase=<passphrase>\"\r\n");
	wmprintf("\r\nTo set/get the mode of Tx/Rx antenna:\r\n");
	wmprintf("iwpriv <interface> antcfg [m]\r\n");
	wmprintf("where value of m is:\r\nBit 0   -- Tx/Rx antenna 1.\r\n"
		"Bit 1   -- Tx/Rx antenna 2.\r\n0xFFFF (65535)  -- "
		"Tx/Rx antenna diversity.\r\n");
	wmprintf("Examples:\r\n"
		"iwpriv <interface> antcfg : Get Tx/Rx antenna"
		" mode\r\niwpriv <interface> antcfg 1 : Set Tx/Rx antenna 1");
	wmprintf("\r\niwpriv <interface> antcfg 65535 : "
		"Set Tx/Rx antenna diversity\r\n");
	wmprintf("\r\nTo get the last and average value of RSSI, SNR"
		" and NF of Beacon and Data:\r\n");
	wmprintf("iwpriv <interface> getsignal\r\n");
	wmprintf("<interface>     getsignal:-32  -33  -35  -36  67  59  "
		"63  56  -99  -92  -98  -92\r\nRSSI info: beacon last -32, ");
	wmprintf("beacon average -33, data last -35, data average -36\r\n"
		"SNR info: beacon last 67, beacon average 59, data last 63,");
	wmprintf(" data average 56\r\nNF info: beacon last -99,"
		"beacon average -92, data last -98, data average -92\r\n");
	wmprintf("Note: This command should be used only when "
		"STA is connected to some access point.\r\n");
	wmprintf("iwpriv <interface> regrdwr\r\n");
	wmprintf("This command is used to read/write the adapter register\r\n");
	wmprintf("Usage:\r\n");
	wmprintf("iwpriv mlan0 regrdwr <type> <offset> [value]\r\n");
	wmprintf("where the parameters are,\r\n");
	wmprintf("<type>:     1:MAC/SOC, 2:BBP, 3:RF\r\n");
	wmprintf("<offset>:   offset of register\r\n");
	wmprintf("[value]:    value to be written\r\n");
	wmprintf("Note: If highest bit of a 32-bit value needs "
			"to be set, use negative input.\r\n");
	wmprintf("the calculation is -(0xffffffff - value + 1). "
			"Range from -0x80000000 to -1.\r\n");
	wmprintf("Examples:\r\n");
	wmprintf("uaputl regrdwr 3 0x0123 - read RF register\r\n");
	wmprintf("uaputl regrdwr 1 0xa123 0xaa - "
		"write 0xaa to MAC register\r\n");

	wmprintf("\r\nFor getting the current and min/max power:\r\n");
	wmprintf("iwpriv <interface> txpowercfg\r\n");
	wmprintf("\r\nTo get the data rate (index) being used "
		"in last Tx packet and last Rx packet:\r\n");
	wmprintf("iwpriv <interface> getdatarate\r\n");
	wmprintf("\r\nFor set/get auto deep sleep mode:\r\n");
	wmprintf("iwpriv <interface> deepsleep [n] [m]\r\n");
	wmprintf("where the parameters are:\r\n");
	wmprintf("[n]: Enable/disable auto deep sleep mode (1/0)\r\n");
	wmprintf("[m]: Idle time in milliseconds after which firmware"
				" will put the device\r\n");
	wmprintf("     in deep sleep mode. Default value is 100 ms.\r\n");
	wmprintf("\r\nFor set/get STA PS configuration parameters:\r\n");
	wmprintf("iwpriv <interface> pscfg [k] [d] [l] ...\r\n");
	wmprintf("Where the parameters:\r\n");
	wmprintf("[k]: Keep alive null packet interval (0: Unchanged, "
				"-1: Disable, n: Interval in seconds)\r\n");
	wmprintf("[d]: DTIM interval (    0: Unchanged,\r\n");
	wmprintf("                      1-5: Value,\r\n");
	wmprintf("                    65534: DTIM will be ignored, listen"
						" interval will be used,\r\n");
	wmprintf("                    65533: Closest DTIM to the listen"
					" interval period will be used )\r\n");
	wmprintf("[l]: Local listen interval (     0: Unchanged,\r\n");
	wmprintf("                                -1: Disable,\r\n");
	wmprintf("                              1-49: Value in"
							" beacon intervals,\r\n");
	wmprintf("                            >= 50: Value in TUs )\r\n");
	wmprintf("[b]: Beacon miss timeout (0: Unchanged, 1-50: Value in"
				" milliseconds, 65535: Disable)\r\n");
	wmprintf("[p]: Delay to PS (0-65535: Value in milliseconds,"
						" default 1000ms)\r\n");
	wmprintf("[m]: PS mode (0: Unchanged, 1: Auto mode, 2: "
				" PS-Poll mode, 3: PS Null mode)\r\n");
	wmprintf("No change if parameters are not provided.\r\n");
	wmprintf("\r\nTo read the EEPROM contents of the card:\r\n");
	wmprintf("iwpriv <interface> rdeeprom <offset> <length>\r\n");
	wmprintf("where, the parameters are:\r\n");
	wmprintf("<offset>:   multiples of 4\r\n<length>:   4-20, multiples "
			"of 4\r\n");

}

static void dump_iwconfig_usage()
{

	wmprintf("Usage: iwconfig <interface> <command> <value>\r\n");
	wmprintf("\r\nFor connecting to a network with open security:\r\n");
	wmprintf("iwconfig <interface> essid <ssid>\r\n");
	wmprintf("\r\nFor connecting to a network with wep security:\r\n");
	wmprintf("iwconfig <interface> essid <ssid> "
		"key <10/26 digit hex key>\r\n");
	wmprintf("\r\nFor setting a specific channel:\r\n");
	wmprintf("iwconfig <interface> channel <channel_number>\r\n");
}

static void print_eeprom_content(uint8_t *buf, int len)
{
	int i;
	wmprintf("mlan0     rdeeprom:");
	for (i = 0; i < len; i++)
		wmprintf("%u ", buf[i]);
	wmprintf("\r\n");
}


static void config_network_params(int argc, char **argv)
{
	struct wlan_network network;
	int ret = 0, arg = 3, count = 1;
	char *result = NULL, *ptr = NULL;
	const char *ptr_array[2];
	char ssid[IEEEtypes_SSID_SIZE + 1],
		secret[sizeof(network.security.psk) + 1];
	if (strcmp(argv[0], "iwpriv") && strlen(argv[arg]) >
		IEEEtypes_SSID_SIZE) {
		wlcm_e("SSID is too long");
		return;
	}

	if (argc > 4 && strlen(argv[arg + 2]) != 10 &&
		strlen(argv[arg + 2]) != 26) {
		wlcm_e("The key should be 10/26 bits in length");
		return;
	}

	if (argc == 4) {
		if (strcmp(argv[0], "iwpriv")) {
			/* OPEN SECURITY */
			memset(&network, 0, sizeof(network));
			memcpy(network.name, argv[arg], strlen(argv[arg]));
			memcpy(network.ssid, argv[arg], strlen(argv[arg]));
			network.security.type = WLAN_SECURITY_NONE;
			network.security.psk_len = 0;
		} else {
			/* WPA2 SECURITY */
			/* Parsing "1;ssid<ssid>;passphrase=<passphrase>" */
			result = strtok(argv[3], ";");
			while (result != NULL) {
				if (count == 1) {
					if (strcmp(result, "1")) {
						wlcm_e("incorrect argument");
						dump_iwpriv_usage();
						return;
					}
				} else {
					if (!strncmp(result, "ssid=", 5)) {
						strcpy(ssid, result + 5);
					} else {
						if (!strncmp(result,
							"passphrase=", 11))
							strcpy(secret,
								result + 11);
						else {
							wlcm_e("incorrect "
							       "argument");
							dump_iwpriv_usage();
							return;
						}
					}
				}

				count++;
				result = strtok(NULL, ";");
				if (count > 4) {
					wlcm_e("incorrect argument");
					dump_iwpriv_usage();
					return;
				}
			}
			if (count < 4) {
				wlcm_e("incorrect argument");
				dump_iwpriv_usage();
				return;
			}

			memset(&network, 0, sizeof(network));
			memcpy(network.name, ssid, strlen(ssid));
			memcpy(network.ssid, ssid, strlen(ssid));

			network.security.type = WLAN_SECURITY_WPA2;
			network.security.psk_len = strlen(secret);
			ptr = secret;
			ret = get_security(1, &ptr, WLAN_SECURITY_WPA2,
				&network.security);
		}
	} else {
		/* WEP SECURITY */
		memset(&network, 0, sizeof(network));
		memcpy(network.name, argv[arg], strlen(argv[arg]));
		memcpy(network.ssid, argv[arg], strlen(argv[arg]));
		arg += 2;
		network.security.type = WLAN_SECURITY_WEP_OPEN;
		network.security.psk_len = strlen(argv[arg]) / 2;
		ptr_array[0] = "open";
		ptr_array[1] = argv[arg];
		ret = get_security(2, (char **)ptr_array,
				   WLAN_SECURITY_WEP_OPEN,
				   &network.security);
		if (ret) {
			wlcm_e("invalid WEP security argument");
			return;
		}
	}

	network.channel = 0;
	network.type = WLAN_BSS_TYPE_STA;
	network.role = WLAN_BSS_ROLE_UAP;

	network.security.pmk_valid = 0;
	network.address.addr_type = 1;

	ret = wlan_add_network(&network);
	switch (ret) {
	case WLAN_ERROR_NONE:
		wlcm_d("Added \"%s\"", network.name);
		break;
	case WLAN_ERROR_PARAM:
		wlcm_e("that network already exists");
		break;
	case WLAN_ERROR_NOMEM:
		wlcm_e("network list is full");
		break;
	case WLAN_ERROR_STATE:
		wlcm_e("can't add networks in this state");
		break;
	default:
		wlcm_e("unable to add network for unknown reason");
		break;
	}
	ret = wlan_connect(network.name);

	if (ret == WLAN_ERROR_STATE) {
		wlcm_e("connection manager not running");
		return;
	}

	if (ret == WLAN_ERROR_PARAM) {
		if (argc >= 2)
			wlcm_e("network not found");
		else
			return;
	}
	wmprintf("Connecting to network..."
		"Use 'wlan-stat' for current connection status.");

}

static void cmd_wlan_read_eeprom(int argc, char **argv)
{
	unsigned int offset, length;
	/* The maximum bytes that can be read by this command is 20
	 * Hence, using a buffer of size 20 bytes */
	uint8_t buf[20];

	offset =  a2hex_or_atoi(argv[3]);

	/* Validate the length value */
	get_uint(argv[4], &length, strlen(argv[4]));
	if (length % 4 != 0 || length < 4 || length > 20) {
		wmprintf("Error: The length value should be a multiple of 4 and"
			" between 4-20\r\n");
		return;
	}
	if (wifi_get_eeprom_data(offset, length, buf) == WM_SUCCESS)
		print_eeprom_content(buf, length);
	else
		wmprintf("Failed to read eeprom content\r\n");
}

static void test_wlan_version(int argc, char **argv)
{
	wifi_fw_version_t ver;
	int ret = wifi_get_firmware_version(&ver);
	if (ret == WM_SUCCESS)
		wmprintf("Version: %s\n\r", ver.version_str);
	else
		wmprintf("Failed to get version\n\r");
}

static void test_wlan_version_ext(int argc, char **argv)
{
	wifi_fw_version_ext_t ver_ext;
	int ret = wifi_get_firmware_version_ext(&ver_ext);
	if (ret == WM_SUCCESS)
		wmprintf("Version ext: %s\n\r", ver_ext.version_str);
	else
		wmprintf("Failed to get ext version\n\r");
}

static void test_wlan_sta_list(int argc, char **argv)
{
	int i;
	sta_list_t *sl;
#ifdef CONFIG_P2P
	wifi_wfd_bss_sta_list(&sl);
#else
	wifi_uap_bss_sta_list(&sl);
#endif

	if (!sl) {
		wmprintf("Failed to get sta list\n\r");
		return;
	}

	sta_info_t *si =  (sta_info_t *)(&sl->count + 1);

	wmprintf("Number of STA = %d \r\n\r\n", sl->count);
	for (i = 0; i < sl->count; i++) {
		wmprintf("STA %d information:\r\n", i + 1);
		wmprintf("=====================\r\n");
		wmprintf("MAC Address: %02X:%02X:%02X:%02X:%02X:%02X\r\n",
				si[i].mac[0], si[i].mac[1], si[i].mac[2],
				si[i].mac[3], si[i].mac[4], si[i].mac[5]);

		wmprintf("\nPower mfg status: %s\r\n",
				(si[i].power_mgmt_status == 0) ?
				"active" : "power save");

		wmprintf("Rssi : %d dBm\r\n\r\n", (signed char)si[i].rssi);
	}

	os_mem_free(sl);
}

static void cmd_wlan_get_antenna(int argc, char **argv)
{
	unsigned int ant;
	int rv = wlan_get_current_ant(&ant);
	if (rv != WM_SUCCESS) {
		wmprintf("Failed to get antenna\n\r");
		return;
	}

	wmprintf("mlan0     antcfg:%d\r\n", ant);
}

void wlan_print_frame_info(const wlan_frame_t *frame)
{
	char ssid[33];

	if (frame->frame_type == BEACON) {
		wmprintf("********* Beacon Info *********");
		wmprintf("\r\nType: 0x%x", frame->frame_type);
		wmprintf("\r\nFrame Control flags: 0x%x",
				frame->frame_data.beacon_info.frame_ctrl_flags);
		wmprintf("\r\nSequence Number: %d",
				wlan_get_seq_num(frame->frame_data.
				beacon_info.seq_frag_num));
		wmprintf("\r\nFragmentation Number: %d",
				wlan_get_frag_num(frame->frame_data.
				beacon_info.seq_frag_num));
		snprintf(ssid, frame->frame_data.beacon_info.ssid_len + 1,
				"%s", frame->frame_data.beacon_info.ssid);
		wmprintf("\r\nSSID: %s", ssid);
		wmprintf("\r\n*******************************\r\n");
	} else if (frame->frame_type == DATA || frame->frame_type == QOS_DATA) {
		wmprintf("********* Data Packet Info *********");
		if (frame->frame_data.data_info.frame_ctrl_flags & 0x08)
			wmprintf("\r\nThis is a retransmission\r\n");
		wmprintf("\r\nType: 0x%x", frame->frame_type);
		wmprintf("\r\nFrame Control flags: 0x%x",
				frame->frame_data.data_info.frame_ctrl_flags);
		if (frame->frame_data.data_info.frame_ctrl_flags && 0x01) {
			wmprintf("\r\nBSSID: ");
			print_mac(frame->frame_data.data_info.bssid);
			wmprintf("\r\nSource: ");
			print_mac(frame->frame_data.data_info.src);
			wmprintf("\r\nDestination: ");
			print_mac(frame->frame_data.data_info.dest);
		} else {
			wmprintf("\r\nBSSID: ");
			print_mac(frame->frame_data.data_info.src);
			wmprintf("\r\nSource: ");
			print_mac(frame->frame_data.data_info.dest);
			wmprintf("\r\nDestination: ");
			print_mac(frame->frame_data.data_info.bssid);
		}
		wmprintf("\r\nSequence Number: %d",
				wlan_get_seq_num(frame->frame_data.
				data_info.seq_frag_num));
		wmprintf("\r\nFragmentation Number: %d",
				wlan_get_frag_num(frame->frame_data.
				data_info.seq_frag_num));
		wmprintf("\r\nQoS Control Flags: 0x%x",
				frame->frame_data.data_info.qos_ctrl);
		wmprintf("\r\n*******************************\r\n");
	}
}

void wlan_sniffer_cb(const wlan_frame_t *frame, const uint16_t len)
{
	if (frame) {
		wlan_print_frame_info(frame);
	}
}

static void cmd_netmon_enable_disable(int argc, char **argv)
{
	wifi_net_monitor_t net_mon;
	memset(&net_mon, 0, sizeof(net_mon));

	if (argc == 3) {
		wlan_sniffer_status();
	} else {

		if (argc > 4) {
			net_mon.monitor_activity = a2hex_or_atoi(argv[4]);
		}
		if (argc > 5)
			net_mon.filter_flags = a2hex_or_atoi(argv[5]);
		if (argc > 6)
			net_mon.radio_type = a2hex_or_atoi(argv[6]);
		if (argc > 7)
			net_mon.chan_number = a2hex_or_atoi(argv[7]);
		if (argc > 8) {
			wmprintf("Error\r\n");
			return;
		}

		if (net_mon.monitor_activity)
			wlan_sniffer_start(net_mon.filter_flags,
					net_mon.radio_type,
					net_mon.chan_number, wlan_sniffer_cb);
		else
			wlan_sniffer_stop();
	}
}

static void cmd_wlan_set_antenna(int argc, char **argv)
{
	wmprintf("setting antenna\n\r");
	unsigned int ant;
	get_uint(argv[3], &ant, strlen(argv[3]));
	if (ant == 1 || ant == 2 || ant == 65535) {
		int rv = wlan_set_current_ant(ant);
		if (rv != WM_SUCCESS) {
			wlcm_e("Unable to set antenna");
		}
	}
	else {
		wlcm_e("Incorrect value for antenna."
			"Should be 1 or 2 or 65535 (0xffff)");
	}
}

static void cmd_wlan_get_signal(int argc, char **argv)
{
	int ret;
	wifi_rssi_info_t rssi_info;
	ret = wifi_send_rssi_info_cmd(&rssi_info);
	if (ret == WM_SUCCESS) {
			wmprintf("mlan0     getsignal:%d  %d  "
				 "%d  %d  %d  %d  %d  %d  %d  %d  %d  %d\r\n",
				rssi_info.bcn_rssi_last,
				rssi_info.bcn_rssi_avg,
				rssi_info.data_rssi_last,
				rssi_info.data_rssi_avg,
				rssi_info.bcn_snr_last,
				rssi_info.bcn_snr_avg,
				rssi_info.data_snr_last,
				rssi_info.data_snr_avg,
				rssi_info.bcn_nf_last,
				rssi_info.bcn_nf_avg,
				rssi_info.data_nf_last,
				rssi_info.data_nf_avg);
	} else
		wlcm_e("Failed to send getsignal command");
}

static void cmd_wlan_get_datarate(int argc, char **argv)
{
	int ret;
	wifi_tx_rate_t tx_r;
	ret = wifi_send_get_datarate(&tx_r);
	if (ret == WM_SUCCESS)
		wmprintf("mlan0     getdatarate:%d  %d\r\n", tx_r.tx_rate,
				tx_r.rx_rate);
	else
		wlcm_e("Failed to get datarate");

}
static void test_ifconfig_commands(int argc, char **argv)
{
	void *intrfc_handle = NULL;

	if (argc != 3) {
		wlcm_e("invalid number of arguments");
		dump_ifconfig_usage();
		return;
	}

	if (!strcmp("up", argv[2])) {
		if (!strcmp("mlan0", argv[1])) {
			intrfc_handle = net_get_mlan_handle();
			net_interface_up(intrfc_handle);
		}
#ifdef CONFIG_P2P
		else if (!strcmp("wfd0", argv[1])) {
			intrfc_handle = net_get_wfd_handle();
			net_interface_up(intrfc_handle);
		}
#else
		else if (!strcmp("uap0", argv[1])) {
			intrfc_handle = net_get_uap_handle();
			net_interface_up(intrfc_handle);
		}
#endif
	} else if (!strcmp("down", argv[2])) {
		if (!strcmp("mlan0", argv[1])) {
			intrfc_handle = net_get_mlan_handle();
			net_interface_down(intrfc_handle);
		}
#ifdef CONFIG_P2P
		else if (!strcmp("wfd0", argv[1])) {
			intrfc_handle = net_get_wfd_handle();
			net_interface_down(intrfc_handle);
		}
#else
		else if (!strcmp("uap0", argv[1])) {
			intrfc_handle = net_get_uap_handle();
			net_interface_down(intrfc_handle);
		}
#endif
	}
	return;
}

static void test_iwlist_commands(int argc, char **argv)
{
	if (argc != 3) {
		wlcm_e("invalid number of arguments");
		dump_iwlist_usage();
		return;
	}

	if (!strcmp("scan", argv[2])) {
		test_wlan_scan(argc, argv);
		return;
	}

	if (!strcmp("channel", argv[2])) {
		cmd_wlan_get_rf_channel(argc, argv);
		return;
	}
	wlcm_e("incorrect argument");
	dump_iwlist_usage();
}

static void test_iwpriv_commands(int argc, char **argv)
{
	if (argc < 3 && argc > 10) {
		wlcm_e("invalid number of arguments");
		dump_iwpriv_usage();
		return;
	}

	if (argc == 3) {
		if (!strcmp(argv[2], "netmon")) {
			cmd_netmon_enable_disable(argc, argv);
			return;
		}
		if (!strcmp(argv[2], "txpowercfg")) {
			cmd_wlan_getset_tx_power(argc, argv);
			return;
		}
		if (!strcmp(argv[2], "deauth")) {
			test_wlan_disconnect(argc, argv);
			return;
		}
		if (!strcmp(argv[2], "getdatarate")) {
			cmd_wlan_get_datarate(argc, argv);
			return;
		}
		if (!strcmp(argv[2], "antcfg")) {
			cmd_wlan_get_antenna(argc, argv);
			return;
		}
		if (!strcmp(argv[2], "getsignal")) {
			cmd_wlan_get_signal(argc, argv);
			return;
		}
		if (!strcmp(argv[2], "deepsleep")) {
			cmd_wlan_deepsleep(0, NULL);
			return;
		}
		if (!strcmp(argv[2], "pscfg")) {
			cmd_wlan_pscfg(0, NULL);
			return;
		}
		if (!strcmp(argv[2], "version")) {
			test_wlan_version(argc, argv);
			return;
		}
		if (!strcmp(argv[2], "verext")) {
			test_wlan_version_ext(argc, argv);
			return;
		}
		if (!strcmp(argv[2], "sta_list")) {
			test_wlan_sta_list(argc, argv);
			return;
		}
	} else {
		if (!strcmp(argv[2], "txpowercfg")) {
			cmd_wlan_getset_tx_power(argc, argv);
			return;
		}
		if (!strcmp(argv[2], "netmon")) {
			cmd_netmon_enable_disable(argc, argv);
			return;
		}
		if (!strcmp(argv[2], "passphrase")) {
			config_network_params(argc, argv);
			return;
		}
		if (!strcmp(argv[2], "antcfg")) {
			cmd_wlan_set_antenna(argc, argv);
			return;
		}
		if (!strcmp(argv[2], "deepsleep")) {
			cmd_wlan_deepsleep(argc - 2, argv + 2);
			return;
		}
		if (!strcmp(argv[2], "pscfg")) {
			cmd_wlan_pscfg(argc - 2, argv + 2);
			return;
		}
		if (!strcmp(argv[2], "regrdwr")) {
			wlan_regrdwr_getset(argc - 1, argv + 1);
			return;
		}
		if (!strcmp(argv[2], "rdeeprom")) {
			if (argc == 5) {
				cmd_wlan_read_eeprom(argc, argv);
				return;
			}
		}

	}
	dump_iwpriv_usage();
}

static void test_iwconfig_commands(int argc, char **argv)
{
	if (argc != 4 && argc != 6) {
		wlcm_e("invalid number of arguments");
		dump_iwconfig_usage();
		return;
	}

	if (argc > 4 && strcmp("key", argv[4])) {
		wlcm_e("incorrect argument");
		dump_iwconfig_usage();
		return;
	}

	if (!strcmp("essid", argv[2])) {
		config_network_params(argc, argv);
		return;
	}

	wlcm_e("incorrect argument");
	dump_iwconfig_usage();
	return;

}

static struct cli_command wifi_tests[] = {
	{"ifconfig", "Support for ifconfig commands", test_ifconfig_commands},
	{"iwlist", "Support for iwlist commands", test_iwlist_commands},
	{"iwpriv", "Support for iwpriv commands", test_iwpriv_commands},
	{"iwconfig", "Support for iwconfig commands", test_iwconfig_commands},
};

int wlan_iw_cli_init(void)
{
	int i;

	for (i = 0; i < sizeof(wifi_tests) / sizeof(struct cli_command); i++)
		if (cli_register_command(&wifi_tests[i]))
			return WLAN_ERROR_ACTION;
	return WLAN_ERROR_NONE;
}
