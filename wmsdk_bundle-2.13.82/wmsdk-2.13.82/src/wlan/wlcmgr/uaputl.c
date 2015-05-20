
/*
 *  Copyright 2008-2013, Marvell International Ltd.
 *  All Rights Reserved.
 */

#include <stdio.h>
#include <wlan.h>
#include <wifi.h>
#include <wlan_tests.h>
#include <cli.h>
#include <cli_utils.h>
#include <wmstdio.h>
#include <string.h>
#include <pwrmgr.h>
#include <stdint.h>
#include <wm_net.h>
#include <wifidirectuaputl.h>

#define UAP_PROFILE_NAME "uap"
#define MIN_WPA2_PASSPHRASE_LENGTH 8

/**
 * Check of decimal or hex string
 * @param   num string
 */
#define IS_HEX_OR_DIGIT(num) \
	(strncasecmp("0x", (num), 2) ? ISDIGIT((num)) : ishexstring((num)))


extern int wlan_regrdwr_getset(int argc, char *argv[]);
extern void test_wlan_stop_network(int argc, char **argv);

static struct wlan_network uap_config;

extern int wifi_uap_stop();

static void dump_uaputl_usage()
{
	wmprintf("\nUsage: uaputl <parameter> [<value>]\r\n");
	wmprintf("\r\nTo get/set the uAP ssid:\r\n");
	wmprintf("uaputl sys_cfg_ssid [<ssid_name>]\r\n");
	wmprintf("\r\nTo set wpa2 passphrase for uAP:\r\n");
	wmprintf("uaputl sys_cfg_wpa_passphrase <passphrase>\r\n");
	wmprintf("\r\nTo get/set uAP channel:\r\n");
	wmprintf("uaputl sys_cfg_channel [<channel number>]\r\n");
	wmprintf("\r\nTo get/set uAP max station number:\r\n");
	wmprintf("uaputl sys_cfg_max_sta_num [<max station number>]\r\n");
	wmprintf("\r\nTo get/set uAP Tx power (in dBm):\r\n");
	wmprintf("uaputl sys_cfg_tx_power [<tx_power>]\r\n");
	wmprintf("\r\nTo get/set uAP rates:\r\n");
	wmprintf("uaputl sys_cfg_rates <rates>\r\n");
	wmprintf("eg: uaputl sys_cfg_rates 0x82 0x84 0x8b 0x96 "
			"0x0c 0x12 0x18 0x30\r\n");
	wmprintf("\r\nTo read/write the MAC, BBP and RF registers "
			"from the card\r\n");
	wmprintf("uaputl regrdwr <TYPE> <OFFSET> [value]\r\n");
	wmprintf("TYPE: 1 - read/write MAC/SOC register\r\n");
	wmprintf("      2 - read/write BBP register\r\n");
	wmprintf("      3 - read/write RF register\r\n");
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
	wmprintf("\r\nTo start in uap mode:\r\n");
	wmprintf("uaputl bss_start\r\n");
	wmprintf("\r\nTo stop the uap mode:\r\n");
	wmprintf("uaputl bss_stop\r\n");
}


/**
 *  @brief Function to check valid rate
 *
 *
 *  @param  rate    Rate to verify
 *
 *  return          WM_SUCCESS or -WM_FAIL
 **/
static int is_rate_valid(int rate)
{
	int ret = WM_SUCCESS;
	switch (rate) {
	case 2:
	case 4:
	case 11:
	case 22:
	case 12:
	case 18:
	case 24:
	case 48:
	case 72:
	case 96:
	case 108:
	case 36:
		break;
	default:
		ret = -WM_FAIL;
		break;
	}
	return ret;
}

/**
 *  @brief  Detects duplicates rate in array of strings
 *          Note that 0x82 and 0x2 are same for rate
 *
 *  @param  argc    Number of elements
 *  @param  argv    Array of strings
 *  @return -WM_FAIL or WM_SUCCESS
 */
static inline int has_dup_rate(int argc, char *argv[])
{
	int i, j;
	/* Check for duplicate */
	for (i = 0; i < (argc - 1); i++) {
		for (j = i + 1; j < argc; j++) {
			if ((A2HEXDECIMAL(argv[i]) & ~BASIC_RATE_SET_BIT) ==
			    (A2HEXDECIMAL(argv[j]) & ~BASIC_RATE_SET_BIT))
				return -WM_FAIL;
		}
	}
	return WM_SUCCESS;
}

/**
 *  @brief Check for mandatory rates
 *
 *
 * 2, 4, 11, 22 must be present
 *
 * 6 12 and 24 must be present for ofdm
 *
 *  @param argc     Number of arguments
 *  @param argv     Pointer to the arguments
 *  @return         -WM_FAIL or WM_SUCCESS
 *
 */
static int check_mandatory_rates(int argc, char **argv)
{
	int i;
	int tmp;
	t_u32 rate_bitmap = 0;
	int cck_enable = 0;
	int ofdm_enable = 0;
#define BITMAP_RATE_1M         0x01
#define BITMAP_RATE_2M         0x02
#define BITMAP_RATE_5_5M       0x04
#define BITMAP_RATE_11M        0x8
#define B_RATE_MANDATORY       0x0f
#define BITMAP_RATE_6M         0x10
#define BITMAP_RATE_12M        0x20
#define BITMAP_RATE_24M        0x40
#define G_RATE_MANDATORY        0x70
	for (i = 0; i < argc; i++) {
		tmp = (A2HEXDECIMAL(argv[i]) & ~BASIC_RATE_SET_BIT);
		switch (tmp) {
		case 2:
			cck_enable = 1;
			rate_bitmap |= BITMAP_RATE_1M;
			break;
		case 4:
			cck_enable = 1;
			rate_bitmap |= BITMAP_RATE_2M;
			break;
		case 11:
			cck_enable = 1;
			rate_bitmap |= BITMAP_RATE_5_5M;
			break;
		case 22:
			cck_enable = 1;
			rate_bitmap |= BITMAP_RATE_11M;
			break;
		case 12:
			ofdm_enable = 1;
			rate_bitmap |= BITMAP_RATE_6M;
			break;
		case 24:
			ofdm_enable = 1;
			rate_bitmap |= BITMAP_RATE_12M;
			break;
		case 48:
			ofdm_enable = 1;
			rate_bitmap |= BITMAP_RATE_24M;
			break;
		case 18:
		case 36:
		case 72:
		case 96:
		case 108:
			ofdm_enable = 1;
			break;
		}
	}
	if ((rate_bitmap & B_RATE_MANDATORY) != B_RATE_MANDATORY) {
		if (cck_enable) {
			wmprintf
			    ("Basic Rates 2, 4, 11 and 22"
				" (500K units)\r\n"
			     "must be present in basic or"
				" non-basic rates\r\n");
			return -WM_FAIL;
		}
	}
	if (ofdm_enable &&
	    ((rate_bitmap & G_RATE_MANDATORY) != G_RATE_MANDATORY)) {
		wmprintf("OFDM Rates 12, 24 and 48 ( 500Kb units)\r\n"
			 "must be present in basic or non-basic rates\r\n");
		return -WM_FAIL;
	}
	return WM_SUCCESS;
}

#if 0

/**
 *  @brief Check protocol is valid or not
 *
 *  @param protocol Protocol
 *
 *  @return         WM_SUCCESS or -WM_FAIL
 */
int is_protocol_valid(int protocol)
{
	int ret = -WM_FAIL;
	switch (protocol) {
	case PROTOCOL_NO_SECURITY:
	case PROTOCOL_STATIC_WEP:
	case PROTOCOL_WPA:
	case PROTOCOL_WPA2:
	case PROTOCOL_WPA2_MIXED:
		ret = WM_SUCCESS;
		break;
	default:
		wmprintf("ERR: Invalid Protocol: %d\r\n", protocol);
		break;
	}
	return ret;
}

/**
 *  @brief  Detects if band is different across the list of scan channels
 *
 *  @param  argc    Number of elements
 *  @param  argv    Array of strings
 *  @return -WM_FAIL or WM_SUCCESS
 */
inline int has_diff_band(int argc, char *argv[])
{
	int i = 0;
	int channel = 0;
	int band[MAX_CHANNELS];
	/* Check for different bands */
	for (i = 0; i < argc; i++) {
		band[i] = -1;
		sscanf(argv[i], "%d.%d", &channel, &band[i]);
		if (band[i] == -1) {
			if (channel > MAX_CHANNELS_BG)
				band[i] = 1;
			else
				band[i] = 0;
		}
	}
	for (i = 0; i <= (argc - 2); i++) {
		if (band[i] != band[i + 1])
			return -WM_FAIL;
	}
	return WM_SUCCESS;
}

/**
 * @brief Checks if given channel in 'a' band is valid or not.
 *
 * @param channel   Channel number
 * @return          WM_SUCCESS or -WM_FAIL
 */
int is_valid_a_band_channel(int channel)
{
	int ret = WM_SUCCESS;
	switch (channel) {
	case 16:
	case 34:
	case 36:
	case 38:
	case 40:
	case 42:
	case 44:
	case 46:
	case 48:
	case 52:
	case 56:
	case 60:
	case 64:
	case 100:
	case 104:
	case 108:
	case 112:
	case 116:
	case 120:
	case 124:
	case 128:
	case 132:
	case 136:
	case 140:
	case 149:
	case 153:
	case 157:
	case 161:
	case 165:
		break;
	default:
		ret = -WM_FAIL;
		break;
	}
	return ret;
}

/**
 * @brief Checks if secondary channel can be set above given primary
 * channel in 'a' band or not.
 *
 * @param channel   Channel number
 * @return          WM_SUCCESS or -WM_FAIL
 */
int is_valid_a_band_channel_above(int channel)
{
	int ret = WM_SUCCESS;
	switch (channel) {
	case 36:
	case 44:
	case 52:
	case 60:
	case 100:
	case 108:
	case 116:
	case 124:
	case 132:
	case 149:
	case 157:
		break;
	default:
		ret = -WM_FAIL;
		break;
	}
	return ret;
}

/**
 * @brief Checks if secondary channel can be set below given primary
 * channel in 'a' band or not.
 *
 * @param channel   Channel number
 * @return          WM_SUCCESS or -WM_FAIL
 */
int is_valid_a_band_channel_below(int channel)
{
	int ret = WM_SUCCESS;
	switch (channel) {
	case 40:
	case 48:
	case 56:
	case 64:
	case 104:
	case 112:
	case 120:
	case 128:
	case 136:
	case 153:
	case 161:
		break;
	default:
		ret = -WM_FAIL;
		break;
	}
	return ret;
}

#endif

/**
 *  @brief Checks a particular input for validation.
 *
 *  @param cmd      Type of input
 *  @param argc     Number of arguments
 *  @param argv     Pointer to the arguments
 *  @return         WM_SUCCESS or -WM_FAIL
 */
static int is_input_valid(valid_inputs cmd, int argc, char *argv[])
{
	int i;
#if 0
	int chan_number = 0;
	int band = 0;
	int ch;
#endif
	int ret = WM_SUCCESS;
	if (argc == 0)
		return -WM_FAIL;
	switch (cmd) {
#if 0
	case RDEEPROM:
		if (argc != 2) {
			wmprintf(" ERR: Argument count mismatch\r\n");
			ret = -WM_FAIL;
		} else {
			if ((ISDIGIT(argv[0]) == 0) || (ISDIGIT(argv[1]) == 0)
			    || (A2HEXDECIMAL(argv[0]) & 0x03) ||
			    ((int)(A2HEXDECIMAL(argv[0])) < 0) ||
			    (A2HEXDECIMAL(argv[1]) & 0x03) ||
			    (A2HEXDECIMAL(argv[1]) < 4) ||
			    (A2HEXDECIMAL(argv[1]) > 20)) {
				wmprintf
			    (" ERR: Invalid inputs for Read EEPROM\r\n");
				ret = -WM_FAIL;
			}
		}
		break;
	case SCANCHANNELS:
		if (argc > MAX_CHANNELS) {
			wmprintf("ERR: Invalid List of Channels\r\n");
			ret = -WM_FAIL;
		} else {
			for (i = 0; i < argc; i++) {
				chan_number = -1;
				band = -1;
				sscanf(argv[i], "%d.%d", &chan_number, &band);
				if ((chan_number == -1) || (chan_number < 1) ||
				    (chan_number > MAX_CHANNELS)) {
					wmprintf
					    ("ERR: Channel must be in the"
					" range of 1 to %d\r\n",
					     MAX_CHANNELS);
					ret = -WM_FAIL;
					break;
				}
				if ((chan_number > MAX_CHANNELS_BG) &&
				    !(is_valid_a_band_channel(chan_number))) {
					wmprintf
					    ("ERR: Invalid Channel"
					" in 'a' band!\r\n");
					ret = -WM_FAIL;
					break;
				}
				if ((band < -1) || (band > 1)) {
					wmprintf
					    ("ERR:Band must be"
					" either 0 or 1\r\n");
					ret = -WM_FAIL;
					break;
				} else {
					if (((chan_number < MAX_CHANNELS_BG) &&
					     (chan_number != 8)
					     && (chan_number != 12) &&
					     (band == 1))
					    || ((chan_number > MAX_CHANNELS_BG)
						&& (band == 0))) {
						wmprintf
						    ("ERR:Invalid band for"
						" given channel\r\n");
						ret = -WM_FAIL;
						break;
					}
				}
			}
			if ((ret != -WM_FAIL) &&
			    (has_dup_channel(argc, argv) != WM_SUCCESS)) {
				wmprintf
			    ("ERR: Duplicate channel values entered\r\n");
				ret = -WM_FAIL;
			}
			if ((ret != -WM_FAIL) &&
			    (has_diff_band(argc, argv) != WM_SUCCESS)) {
				wmprintf
				    ("ERR: Scan channel list should contain"
				" channels from only one band\r\n");
				ret = -WM_FAIL;
			}
		}
		break;
	case TXPOWER:
		if ((argc > 1) || (ISDIGIT(argv[0]) == 0)) {
			wmprintf("ERR:Invalid Transmit power\r\n");
			ret = -WM_FAIL;
		} else {
			if ((atoi(argv[0]) < MIN_TX_POWER) ||
			    (atoi(argv[0]) > MAX_TX_POWER)) {
				wmprintf
				    ("ERR: TX Powar must be in the range"
				" of %d to %d.\r\n",
				     MIN_TX_POWER, MAX_TX_POWER);
				ret = -WM_FAIL;
			}
		}
		break;
	case PROTOCOL:
		if ((argc > 1) || (ISDIGIT(argv[0]) == 0)) {
			wmprintf("ERR:Invalid Protocol\r\n");
			ret = -WM_FAIL;
		} else
			ret = is_protocol_valid(atoi(argv[0]));
		break;
	case CHANNEL:
		if ((argc != 1) && (argc != 2)) {
			wmprintf("ERR: Incorrect arguments for channel.\r\n");
			ret = -WM_FAIL;
		} else {
			if (argc == 2) {
				if ((ISDIGIT(argv[1]) == 0) ||
				    (atoi(argv[1]) & ~CHANNEL_MODE_MASK)) {
					wmprintf("ERR: Invalid Mode\r\n");
					ret = -WM_FAIL;
				}
				if ((atoi(argv[1]) & BITMAP_ACS_MODE) &&
				    (atoi(argv[0]) != 0)) {
					wmprintf
					    ("ERR: Channel must be 0 for ACS;"
					" MODE = 1.\r\n");
					ret = -WM_FAIL;
				}
				if ((atoi(argv[1]) & BITMAP_CHANNEL_ABOVE) &&
				    (atoi(argv[1]) & BITMAP_CHANNEL_BELOW)) {
					wmprintf
					    ("ERR: secondary channel above"
				" and below both are enabled\r\n");
					ret = -WM_FAIL;
				}
			}
			if ((argc == 1) || (!(atoi(argv[1]) &
				 BITMAP_ACS_MODE))) {
				if ((ISDIGIT(argv[0]) == 0) ||
				    (atoi(argv[0]) < 1) ||
				    (atoi(argv[0]) > MAX_CHANNELS)) {
					wmprintf
					    ("ERR: Channel must be in the"
					" range of 1 to %d\r\n",
					     MAX_CHANNELS);
					ret = -WM_FAIL;
				}
				if ((atoi(argv[0]) > MAX_CHANNELS_BG) &&
				    !(is_valid_a_band_channel(atoi(argv[0])))) {
					wmprintf
					    ("ERR: Invalid Channel in"
					" 'a' band!\r\n");
					ret = -WM_FAIL;
				}
				ch = atoi(argv[0]);
				if (ch <= MAX_CHANNELS_BG) {
					if ((argc == 2) &&
					    (atoi(argv[1]) &
					     BITMAP_CHANNEL_ABOVE) &&
					    (atoi(argv[0]) >
					     MAX_CHANNEL_ABOVE)) {
						wmprintf
						    ("ERR: only allow channel"
					" 1-7 for secondary channel above\r\n");
						ret = -WM_FAIL;
					}
					if ((argc == 2) &&
					    (atoi(argv[1]) &
					     BITMAP_CHANNEL_BELOW) &&
					    (atoi(argv[0]) <
					     MIN_CHANNEL_BELOW)) {
						wmprintf
						    ("ERR: only allow channel"
					" 5-11 for secondary channel "
					"below\r\n");
						ret = -WM_FAIL;
					}
				} else {
					if (argc == 2) {
						if ((atoi(argv[1]) &
						     BITMAP_CHANNEL_BELOW) &&
					    !is_valid_a_band_channel_below
						    (atoi(argv[0]))) {
							wmprintf
							    ("ERR: For given"
					" primary channel secondary channel"
					" can not be set below\r\n");
							ret = -WM_FAIL;
						}
						if ((atoi(argv[1]) &
						     BITMAP_CHANNEL_ABOVE) &&
					    !is_valid_a_band_channel_above
						    (atoi(argv[0]))) {
							wmprintf
							    ("ERR: For given"
					" primary channel secondary channel "
					"can not be set above\r\n");
							ret = -WM_FAIL;
						}
					}
				}
			}
		}
		break;
	case CHANNEL_EXT:
		if (argc > 3) {
			wmprintf("ERR: Incorrect arguments for "
					"channel_ext.\r\n");
			ret = -WM_FAIL;
		} else {
			if (argc == 3) {
				if ((ISDIGIT(argv[2]) == 0) ||
				    (atoi(argv[2]) & ~CHANNEL_MODE_MASK)) {
					wmprintf("ERR: Invalid Mode\r\n");
					ret = -WM_FAIL;
				}
				if ((atoi(argv[2]) & BITMAP_ACS_MODE) &&
				    (atoi(argv[0]) != 0)) {
					wmprintf
					    ("ERR: Channel must be 0 for ACS;"
					" MODE = 1.\r\n");
					ret = -WM_FAIL;
				}
				if ((atoi(argv[2]) & BITMAP_CHANNEL_ABOVE) &&
				    (atoi(argv[2]) & BITMAP_CHANNEL_BELOW)) {
					wmprintf
					    ("ERR: secondary channel above and"
				" below both are enabled\r\n");
					ret = -WM_FAIL;
				}
			}
			if ((argc == 2) &&
			    ((ISDIGIT(argv[1]) == 0) || (atoi(argv[1]) < 0) ||
			     atoi(argv[1]) > 1)) {
				wmprintf("ERR:Invalid band\r\n");
				ret = -WM_FAIL;
			}
			if ((argc == 1) ||
			    ((argc == 3) &&
			     !(atoi(argv[2]) & BITMAP_ACS_MODE))) {
				if ((ISDIGIT(argv[0]) == 0) ||
				    (atoi(argv[0]) < 1) ||
				    (atoi(argv[0]) > MAX_CHANNELS)) {
					wmprintf
					    ("ERR: Channel must be in the"
					" range of 1 to %d\r\n",
					     MAX_CHANNELS);
					ret = -WM_FAIL;
				}
				if ((atoi(argv[0]) > MAX_CHANNELS_BG) &&
				    !(is_valid_a_band_channel(atoi(argv[0])))) {
					wmprintf
					    ("ERR: Invalid Channel"
					" in 'a' band!\r\n");
					ret = -WM_FAIL;
				}
				ch = atoi(argv[0]);
				if (ch <= MAX_CHANNELS_BG) {
					if ((argc == 3) &&
					    (atoi(argv[2]) &
					     BITMAP_CHANNEL_ABOVE) &&
					    (atoi(argv[0]) >
					     MAX_CHANNEL_ABOVE)) {
						wmprintf
						    ("ERR: only allow channel"
					" 1-7 for secondary channel above\r\n");
						ret = -WM_FAIL;
					}
					if ((argc == 3) &&
					    (atoi(argv[2]) &
					     BITMAP_CHANNEL_BELOW) &&
					    (atoi(argv[0]) <
					     MIN_CHANNEL_BELOW)) {
						wmprintf
						    ("ERR: only allow channel"
				" 5-11 for secondary channel below\r\n");
						ret = -WM_FAIL;
					}
				} else {
					if (argc == 3) {
						if ((atoi(argv[2]) &
						     BITMAP_CHANNEL_BELOW) &&
					    !is_valid_a_band_channel_below
						    (atoi(argv[0]))) {
							wmprintf
							    ("ERR: For given "
						"primary channel secondary "
						"channel can not be"
						" set below\r\n");
							ret = -WM_FAIL;
						}
						if ((atoi(argv[2]) &
						     BITMAP_CHANNEL_ABOVE) &&
						!is_valid_a_band_channel_above
						    (atoi(argv[0]))) {
							wmprintf
							    ("ERR: For given"
						" primary channel secondary"
						" channel can not be "
						"set above\r\n");
							ret = -WM_FAIL;
						}
					}
				}
			}
		}
		break;
	case BAND:
		if (argc > 1) {
			wmprintf("ERR: Incorrect number of BAND "
				"arguments.\r\n");
			ret = -WM_FAIL;
		} else {
			if ((ISDIGIT(argv[0]) == 0) || (atoi(argv[0]) < 0) ||
			    (atoi(argv[0]) > 1)) {
				wmprintf("ERR: Invalid band.\r\n");
				ret = -WM_FAIL;
			}
		}
		break;
#endif
	case RATE:
		if (argc > MAX_RATES) {
			wmprintf("ERR: Incorrect number of RATES "
				"arguments.\r\n");
			ret = -WM_FAIL;
		} else {
			for (i = 0; i < argc; i++) {
				if ((IS_HEX_OR_DIGIT(argv[i]) == -WM_FAIL) ||
				    (is_rate_valid
				     (A2HEXDECIMAL(argv[i]) &
				      ~BASIC_RATE_SET_BIT)
				     != WM_SUCCESS)) {
					wmprintf("ERR:Unsupported rate.\r\n");
					ret = -WM_FAIL;
					break;
				}
			}
			if ((ret != -WM_FAIL) &&
			    (has_dup_rate(argc, argv) != WM_SUCCESS)) {
				wmprintf
				    ("ERR: Duplicate rate values entered\r\n");
				ret = -WM_FAIL;
			}
			if (check_mandatory_rates(argc, argv) != WM_SUCCESS)
				ret = -WM_FAIL;
		}
		break;
#if 0
	case BROADCASTSSID:
		if (argc != 1) {
			wmprintf("ERR:wrong BROADCASTSSID arguments.\r\n");
			ret = -WM_FAIL;
		} else {
			if ((ISDIGIT(argv[0]) == 0) ||
			    ((atoi(argv[0]) != 0) && (atoi(argv[0]) != 1))) {
				wmprintf
				    ("ERR:Illegal parameter %s for "
			"BROADCASTSSID. Must be either '0' or '1'.\r\n",
				     argv[0]);
				ret = -WM_FAIL;
			}
		}
		break;
	case RTSTHRESH:
		if (argc != 1) {
			wmprintf
			    ("ERR:Incorrect number of arguments"
			" for RTSTHRESHOLD\r\n");
			ret = -WM_FAIL;
		} else if ((ISDIGIT(argv[0]) == 0) || (atoi(argv[0]) < 0) ||
			   (atoi(argv[0]) > MAX_RTS_THRESHOLD)) {
			wmprintf
			    ("ERR:Illegal RTSTHRESHOLD %s. The value must"
			" between 0 and %d\r\n",
			     argv[0], MAX_RTS_THRESHOLD);
			ret = -WM_FAIL;
		}
		break;
	case FRAGTHRESH:
		if (argc != 1) {
			wmprintf
			    ("ERR:Incorrect number of arguments"
			" for FRAGTHRESH\r\n");
			ret = -WM_FAIL;
		} else if ((ISDIGIT(argv[0]) == 0) ||
			   (atoi(argv[0]) < MIN_FRAG_THRESHOLD) ||
			   (atoi(argv[0]) > MAX_FRAG_THRESHOLD)) {
			wmprintf
			    ("ERR:Illegal FRAGTHRESH %s. The value must"
			" between %d and %d\r\n",
			     argv[0], MIN_FRAG_THRESHOLD, MAX_FRAG_THRESHOLD);
			ret = -WM_FAIL;
		}
		break;
	case DTIMPERIOD:
		if (argc != 1) {
			wmprintf
			    ("ERR:Incorrect number of"
			" arguments for DTIMPERIOD\r\n");
			ret = -WM_FAIL;
		} else if ((ISDIGIT(argv[0]) == 0) || (atoi(argv[0]) < 1) ||
			   (atoi(argv[0]) > MAX_DTIM_PERIOD)) {
			wmprintf
			    ("ERR: DTIMPERIOD Value must be in"
			" range of 1 to %d\r\n",
			     MAX_DTIM_PERIOD);
			ret = -WM_FAIL;
		}
		break;
	case RSNREPLAYPROT:
		if (argc != 1) {
			wmprintf
			    ("ERR:Incorrect number of arguments"
			" for RSNREPLAYPROT\r\n");
			ret = -WM_FAIL;
		} else {
			if ((ISDIGIT(argv[0]) == 0) || (atoi(argv[0]) < 0) ||
			    (atoi(argv[0]) > 1)) {
				wmprintf
				    ("ERR:Illegal RSNREPLAYPROT parameter %s."
				" Must be either '0' or '1'.\r\n",
				     argv[0]);
				ret = -WM_FAIL;
			}
		}
		break;
	case RADIOCONTROL:
		if (argc != 1) {
			wmprintf
			    ("ERR:Incorrect number of arguments"
				" for RADIOCONTROL\r\n");
			ret = -WM_FAIL;
		} else {
			if ((ISDIGIT(argv[0]) == 0) || (atoi(argv[0]) < 0) ||
			    (atoi(argv[0]) > 1)) {
				wmprintf
				    ("ERR:Illegal RADIOCONTROL parameter"
				" %s. Must be either '0' or '1'.\r\n",
				     argv[0]);
				ret = -WM_FAIL;
			}
		}
		break;
	case TXRATECFG:
		if (argc != 1) {
			wmprintf
			    ("ERR:Incorrect number of arguments"
				" for DATARATE\r\n");
			ret = -WM_FAIL;
		} else {
			if (IS_HEX_OR_DIGIT(argv[0]) == -WM_FAIL) {
				wmprintf("ERR: invalid Tx data rate\r\n");
				ret = -WM_FAIL;
			} else
			    if ((A2HEXDECIMAL(argv[0]) != 0) &&
				A2HEXDECIMAL(argv[0]) > 19 &&
				A2HEXDECIMAL(argv[0]) != 44 &&
				A2HEXDECIMAL(argv[0]) != 255)
				wmprintf("ERR:Incorrect TxRate %s.\r\n",
				argv[0]);
		}
		break;
	case MCBCDATARATE:
		if (argc != 1) {
			wmprintf
			    ("ERR:Incorrect number of arguments"
				" for DATARATE\r\n");
			ret = -WM_FAIL;
		} else {
			if (IS_HEX_OR_DIGIT(argv[0]) == -WM_FAIL) {
				wmprintf("ERR: invalid data rate\r\n");
				ret = -WM_FAIL;
			} else if ((A2HEXDECIMAL(argv[0]) != 0) &&
				   (is_rate_valid
				    (A2HEXDECIMAL(argv[0]) &
				     ~BASIC_RATE_SET_BIT) != WM_SUCCESS)) {
				wmprintf("ERR: invalid data rate\r\n");
				ret = -WM_FAIL;
			}
		}
		break;
	case PKTFWD:
		if (argc != 1) {
			wmprintf
			    ("ERR:Incorrect number of arguments "
				"for PKTFWD.\r\n");
			ret = -WM_FAIL;
		} else if ((ISDIGIT(argv[0]) == 0) ||
			   ((atoi(argv[0]) < 0) || (atoi(argv[0]) > 15))) {
			wmprintf
			    ("ERR:Illegal PKTFWD parameter %s. Must be within"
				" '0' and '15'.\r\n",
			     argv[0]);
			ret = -WM_FAIL;
		}
		break;
	case STAAGEOUTTIMER:
		if (argc != 1) {
			wmprintf
			    ("ERR:Incorrect number of arguments for"
			" STAAGEOUTTIMER.\r\n");
			ret = -WM_FAIL;
		} else {
			if ((ISDIGIT(argv[0]) == 0) || ((atoi(argv[0]) != 0) &&
							((atoi(argv[0]) <
							  MIN_STAGE_OUT_TIME) ||
							 (atoi(argv[0]) >
							  MAX_STAGE_OUT_TIME)))
			) {
				wmprintf
				    ("ERR:Illegal STAAGEOUTTIMER %s. Must be"
				" between %d and %d.\r\n",
				     argv[0], MIN_STAGE_OUT_TIME,
				     MAX_STAGE_OUT_TIME);
				ret = -WM_FAIL;
			}
		}
		break;
	case PSSTAAGEOUTTIMER:
		if (argc != 1) {
			wmprintf
			    ("ERR:Incorrect number of arguments"
			" for PSSTAAGEOUTTIMER.\r\n");
			ret = -WM_FAIL;
		} else {
			if ((ISDIGIT(argv[0]) == 0) || ((atoi(argv[0]) != 0) &&
							((atoi(argv[0]) <
							  MIN_STAGE_OUT_TIME) ||
							 (atoi(argv[0]) >
							  MAX_STAGE_OUT_TIME)))
			) {
				wmprintf
				    ("ERR:Illegal PSSTAAGEOUTTIMER %s."
				" Must be between %d and %d.\r\n",
				     argv[0], MIN_STAGE_OUT_TIME,
				     MAX_STAGE_OUT_TIME);
				ret = -WM_FAIL;
			}
		}
		break;
	case AUTHMODE:
		if (argc != 1) {
			wmprintf
			    ("ERR:Incorrect number of"
			" arguments for AUTHMODE\r\n");
			ret = -WM_FAIL;
		} else {
			if ((ISDIGIT(argv[0]) == 0) || ((atoi(argv[0]) != 0) &&
							(atoi(argv[0]) != 1) &&
							(atoi(argv[0]) !=
							 255))) {
				wmprintf
				    ("ERR:Illegal AUTHMODE parameter %s. Must"
				" be either '0','1' or 255''.\r\n",
				     argv[0]);
				ret = -WM_FAIL;
			}
		}
		break;
	case GROUPREKEYTIMER:
		if (argc != 1) {
			wmprintf
			    ("ERR:Incorrect number of arguments"
			" for GROUPREKEYTIMER.\r\n");
			ret = -WM_FAIL;
		} else {
			if ((ISDIGIT(argv[0]) == 0) || (atoi(argv[0]) < 0) ||
			    (atoi(argv[0]) > MAX_GRP_TIMER)) {
				wmprintf
				    ("ERR: GROUPREKEYTIMER range"
				" is [0:%d] (0 for disable)\r\n",
				     MAX_GRP_TIMER);
				ret = -WM_FAIL;
			}
		}
		break;
	case MAXSTANUM:
		if (argc != 1) {
			wmprintf
			    ("ERR:Incorrect number of"
				" arguments for MAXSTANUM\r\n");
			ret = -WM_FAIL;
		} else {
			if ((ISDIGIT(argv[0]) == 0) || (atoi(argv[0]) <= 0)) {
				wmprintf("ERR:Invalid STA_NUM argument %s.\r\n",
					 argv[0]);
				ret = -WM_FAIL;
			}
		}
		break;
	case BEACONPERIOD:
		if (argc != 1) {
			wmprintf
			    ("ERR:Incorrect number of argument"
				" for BEACONPERIOD.\r\n");
			ret = -WM_FAIL;
		} else {
			if ((ISDIGIT(argv[0]) == 0) ||
			    (atoi(argv[0]) < MIN_BEACON_PERIOD)
			    || (atoi(argv[0]) > MAX_BEACON_PERIOD)) {
				wmprintf
				    ("ERR: BEACONPERIOD must be"
					" in range of %d to %d.\r\n",
				     MIN_BEACON_PERIOD, MAX_BEACON_PERIOD);
				ret = -WM_FAIL;
			}
		}
		break;
	case RETRYLIMIT:
		if (argc != 1) {
			wmprintf
			    ("ERR:Incorrect number of arguments"
				" for RETRY LIMIT\r\n");
			ret = -WM_FAIL;
		} else {
			if ((ISDIGIT(argv[0]) == 0) ||
			    (atoi(argv[0]) > MAX_RETRY_LIMIT) ||
			    (atoi(argv[0]) < 0)) {
				wmprintf
				    ("ERR:RETRY_LIMIT must be in the range"
				" of [0:%d]. The  input was %s.\r\n",
				     MAX_RETRY_LIMIT, argv[0]);
				ret = -WM_FAIL;
			}
		}
		break;
	case STICKYTIMCONFIG:
		if ((argc != 1) && (argc != 3)) {
			wmprintf
			    ("ERR:Incorrect number of arguments"
			" for STICKY_TIM_CONFIG\r\n");
			ret = -WM_FAIL;
		} else {
			if ((ISDIGIT(argv[0]) == 0) || (atoi(argv[0]) < 0) ||
			    (atoi(argv[0]) > 2)) {
				wmprintf
				    ("ERR:Enable parameter must"
					" be 0, 1 or 2\r\n");
				ret = -WM_FAIL;
				break;
			}
			if (((atoi(argv[0]) != 1) && (argc > 1))) {
				wmprintf("ERR: Invalid arguments\r\n");
				ret = -WM_FAIL;
				break;
			}
			if ((atoi(argv[0]) == 1) && (argc != 3)) {
				wmprintf
				    ("ERR: Both duration and sticky bit mask"
				" must be provided for ENABLE = 1\r\n");
				ret = -WM_FAIL;
				break;
			}
			if (argc > 1) {
				if ((ISDIGIT(argv[1]) == 0)) {
					wmprintf("ERR: Invalid duration\r\n");
					ret = -WM_FAIL;
					break;
				}
				if ((ISDIGIT(argv[2]) == 0) ||
				    (atoi(argv[2]) < 1) ||
				    (atoi(argv[2]) > 3)) {
					wmprintf
					    ("ERR:Invalid sticky bit mask\r\n");
					ret = -WM_FAIL;
					break;
				}
			}
		}
		break;
	case STICKYTIMSTAMACADDR:
		if ((argc != 1) && (argc != 2)) {
			wmprintf
			    ("ERR:Incorrect number of"
				" STICKY_TIM_STA_MAC_ADDR arguments\r\n");
			ret = -WM_FAIL;
		} else {
			if ((argc == 2) &&
			    ((ISDIGIT(argv[0]) == 0) || (atoi(argv[0]) < 0) ||
			     (atoi(argv[0]) > 1))) {
				wmprintf("ERR:Invalid control parameter\r\n");
				ret = -WM_FAIL;
				break;
			}
		}
		break;
	case COEX2040CONFIG:
		if (argc != 1) {
			wmprintf
			    ("ERR: Incorrect number of 2040 COEX"
				" CONFIG arguments\r\n");
			ret = -WM_FAIL;
		} else {
			if ((ISDIGIT(argv[0]) == 0) || (atoi(argv[0]) < 0) ||
			    (atoi(argv[0]) > 1)) {
				wmprintf("ERR:Invalid enable parameter\r\n");
				ret = -WM_FAIL;
				break;
			}
		}
		break;
	case EAPOL_PWK_HSK:
		if (argc != 2) {
			wmprintf
			    ("ERR:Incorrect number of"
				" EAPOL_PWK_HSK arguments.\r\n");
			ret = -WM_FAIL;
		} else {
			if ((ISDIGIT(argv[0]) == 0) || (ISDIGIT(argv[1]) == 0)
			    || (atoi(argv[0]) < 0) || (atoi(argv[1]) < 0)) {
				wmprintf
				    ("ERR:Illegal parameters for "
				"EAPOL_PWK_HSK. Must be digits greater "
				"than equal to zero.\r\n");
			}
		}
		break;
	case EAPOL_GWK_HSK:
		if (argc != 2) {
			wmprintf
			    ("ERR:Incorrect number of"
				" EAPOL_GWK_HSK arguments.\r\n");
			ret = -WM_FAIL;
		} else {
			if ((ISDIGIT(argv[0]) == 0) || (ISDIGIT(argv[1]) == 0)
			    || (atoi(argv[0]) < 0) || (atoi(argv[1]) < 0)) {
				wmprintf
				    ("ERR:Illegal parameters for EAPOL_GWK_HSK."
				" Must be digits greater than "
				"equal to zero.\r\n");
			}
		}
		break;
	case PREAMBLETYPE:
		if (argc != 1) {
			wmprintf
			    ("ERR:Incorrect number of arguments"
				" for PREAMBLE TYPE\r\n");
			ret = -WM_FAIL;
		} else {
			if ((ISDIGIT(argv[0]) == 0) ||
			    (atoi(argv[0]) > MAX_PREAMBLE_TYPE)
			    || (atoi(argv[0]) < 0)) {
				wmprintf
				    ("ERR:PREAMBLE TYPE must be in the range"
					" of [0:%d]. The  input was %s.\r\n",
				     MAX_PREAMBLE_TYPE, argv[0]);
				ret = -WM_FAIL;
			}
		}
		break;

	case COEX_COMM_BITMAP:
		if (argc != 1) {
			wmprintf
			    ("ERR:Incorrect number of argument "
				"for Bitmap.\r\n");
			ret = -WM_FAIL;
		} else {
		/* Only bit 0 is supported now, hence check for 1 or 0 */
			if ((IS_HEX_OR_DIGIT(argv[0]) == 0) ||
			    (atoi(argv[0]) < 0) || (atoi(argv[0]) > 1)) {
				wmprintf
				    ("ERR: Bitmap must have"
				" value of 1 or 0.\r\n");
				ret = -WM_FAIL;
			}
		}
		break;
	case COEX_COMM_AP_COEX:
		if (argc != 1) {
			wmprintf
			    ("ERR:Incorrect number"
				" of argument for APBTCoex.\r\n");
			ret = -WM_FAIL;
		} else {
			if ((IS_HEX_OR_DIGIT(argv[0]) == 0) ||
			    (atoi(argv[0]) < 0) || (atoi(argv[0]) > 1)) {
				wmprintf
				    ("ERR: APBTCoex must have"
					" value of 1 or 0.\r\n");
				ret = -WM_FAIL;
			}
		}
		break;
	case COEX_SCO_ACL_FREQ:
		if (argc != 1) {
			wmprintf
			    ("ERR:Incorrect number of arguments"
				" for aclFrequency.\r\n");
			ret = -WM_FAIL;
		} else {
			if (ISDIGIT(argv[0]) == 0) {
				wmprintf
				    ("ERR: Incorrect value for"
					" aclFrequency.\r\n");
				ret = -WM_FAIL;
			}
		}
		break;
	case COEX_ACL_ENABLED:
		if (argc != 1) {
			wmprintf
			    ("ERR:Incorrect number of arguments for"
				" (acl) enabled.\r\n");
			ret = -WM_FAIL;
		} else {
			if ((ISDIGIT(argv[0]) == 0) || (atoi(argv[0]) < 0) ||
			    (atoi(argv[0]) > 1)) {
				wmprintf
				    ("ERR: (acl) enabled must have"
					" value of 1 or 0.\r\n");
				ret = -WM_FAIL;
			}
		}
		break;
	case COEX_ACL_BT_TIME:
	case COEX_ACL_WLAN_TIME:
		if (argc != 1) {
			wmprintf
			    ("ERR:Incorrect number of arguments"
				" for bt/wlan time.\r\n");
			ret = -WM_FAIL;
		} else {
			if (ISDIGIT(argv[0]) == 0) {
				wmprintf
				    ("ERR: Incorrect value for"
					" bt/wlan time.\r\n");
				ret = -WM_FAIL;
			}
		}
		break;
	case COEX_PROTECTION:
		if (argc != 2) {
			wmprintf("ERR:Incorrect number of "
				"arguments for %s.\r\n",
				 argv[0]);
			ret = -WM_FAIL;
		} else {
			if (ISDIGIT(argv[1]) == 0) {
				wmprintf("ERR: Incorrect value for %s.\r\n",
					 argv[0]);
				ret = -WM_FAIL;
			}
		}
		break;
	case PWK_CIPHER:
		if ((argc != 1) && (argc != 2)) {
			wmprintf
			    ("ERR:Incorrect number of arguments for"
				" pwk_cipher.\r\n");
			ret = -WM_FAIL;
		} else {
			if ((ISDIGIT(argv[0]) == 0) ||
			    (atoi(argv[0]) & ~PROTOCOL_BITMAP)) {
				wmprintf("Invalid Protocol paramter.\r\n");
				ret = -WM_FAIL;
			}
			if (argc == 2) {
				if ((ISDIGIT(argv[1]) == 0) ||
				    (atoi(argv[1]) & ~CIPHER_BITMAP)) {
					wmprintf("Invalid pairwise "
						"cipher.\r\n");
					ret = -WM_FAIL;
				}
			}
		}
		break;
	case GWK_CIPHER:
		if (argc != 1) {
			wmprintf
			    ("ERR:Incorrect number of arguments for"
				" gwk_cipher.\r\n");
			ret = -WM_FAIL;
		} else {
			if ((ISDIGIT(argv[0]) == 0) ||
			    (atoi(argv[0]) & ~CIPHER_BITMAP) ||
			    (atoi(argv[0]) == AES_CCMP_TKIP)) {
				wmprintf("Invalid group cipher.\r\n");
				ret = -WM_FAIL;
			}
		}
		break;
#endif
	default:
		ret = -WM_FAIL;
		break;
	}
	return ret;
}

static void print_rate(char *rates)
{
	int flag = 0;
	int i;

	wmprintf("Basic Rates =");
	for (i = 0; (i < MAX_RATES) && rates[i]; i++) {
		if (rates[i] > (BASIC_RATE_SET_BIT - 1)) {
			flag = flag ? : 1;
			wmprintf(" 0x%x", rates[i]);
		}
	}
	wmprintf("%s\r\nNon-Basic Rates =", flag ? "" : " ( none ) \r\n");
	for (flag = 0, i = 0; i < MAX_RATES && rates[i]; i++) {
		if (rates[i] < BASIC_RATE_SET_BIT) {
			flag = flag ? : 1;
			wmprintf(" 0x%x", rates[i]);
		}
	}
	wmprintf("%s\r\n", flag ? "" : " ( none ) \r\n");
}

static void uaputl_rates_getset(int argc, char **argv)
{

	int ret, i = 0;
	char rates[MAX_RATES];

	if (argc == 2) {
		ret = wifi_uap_rates_getset(ACTION_GET, rates, MAX_RATES);
		if (ret == WM_SUCCESS)
			print_rate(rates);
		else
			wlcm_e("Could not get operational rates!\r\n");

	} else {
		ret = is_input_valid(RATE, argc - 2 , argv + 2);
		for (i = 0; i < argc - 2; i++) {
			rates[i] = (t_u8) a2hex_or_atoi(argv[i + 2]);
		}

		if (ret == WM_SUCCESS) {
			ret = wifi_uap_rates_getset(ACTION_SET,
					rates, argc - 2);
			if (ret == WM_SUCCESS)
				wmprintf("Rates setting successful\r\n");
			else
				wlcm_e("Could not set operational rates!\r\n");
		}
	}
}

static void uaputl_tx_power_getset(int argc, char *argv[])
{
	unsigned char tx_power_dbm = 0;
	int ret;

#if 0
	/* Check arguments */
	if (argc && is_input_valid(TXPOWER, argc, argv) != WM_SUCCESS) {
		print_sys_cfg_tx_power_usage();
		return -WM_FAIL;
	}
	/* Check if tx power is valid for given domain. */
	if (argc) {
		ret = sg_snmp_mib(ACTION_GET, OID_80211D_ENABLE,
				sizeof(state), &state);
		if (state) {
			ret = check_tx_pwr_validity_11d(atoi(argv[0]));
			if (ret == -WM_FAIL) {
				print_sys_cfg_tx_power_usage();
				return -WM_FAIL;
			}
		}
	}
#endif
	if (argc == 2) {
		ret = wifi_uap_tx_power_getset(ACTION_GET, &tx_power_dbm);
		if (ret == WM_SUCCESS)
			wmprintf("Tx power = %d dBm", tx_power_dbm);
		else
			wlcm_e("Could not get tx power!");
	} else {
		tx_power_dbm = (unsigned char) atoi(argv[2]);
		ret = wifi_uap_tx_power_getset(ACTION_SET, &tx_power_dbm);
		if (ret == WM_SUCCESS)
			wmprintf("Tx power setting successful");
		else
			wlcm_e("Could not set tx power!");
	}
}

static int uap_add_network()
{
	int ret;
	ret = wlan_add_network(&uap_config);
	switch (ret) {
	case WLAN_ERROR_NONE:
		wmprintf("Added \"%s\"\r\n", uap_config.name);
		break;
	case WLAN_ERROR_PARAM:
		wlcm_e("network already exists");
		break;
	case WLAN_ERROR_NOMEM:
		wlcm_e("network list is full");
		break;
	case WLAN_ERROR_STATE:
		wlcm_e("can't add networks "
			"in this state");
		break;
	default:
		wlcm_e("unable to add network "
			"for unknown reason");
		break;
	}
	return ret;

}

static void uap_start_network(int argc, char **argv)
{
	int ret;

	memcpy(uap_config.name, UAP_PROFILE_NAME, strlen(UAP_PROFILE_NAME));
	uap_config.type = WLAN_BSS_TYPE_UAP;
	uap_config.role = WLAN_BSS_ROLE_UAP;
	uap_config.security.pmk_valid = 0;
	uap_config.address.addr_type = 0;
	uap_config.ssid_specific = 1;

	/* IP Addr - 192.168.10.1 */
	uap_config.address.ip = htonl(0xc0a80a01UL);
	/* Gateway - 192.168.10.1 */
	uap_config.address.gw = htonl(0xc0a80a01UL);
	/* Netmask - 255.255.255.0 */
	uap_config.address.netmask = htonl(0xffffff00UL);

	wlan_remove_network(UAP_PROFILE_NAME);

	ret = uap_add_network();
	if (ret != WLAN_ERROR_NONE) {
		wlcm_e("Cannot add the uap profile with"
			" the provided configuration");
		return;
	}

	ret = wlan_start_network(UAP_PROFILE_NAME);
	if (ret != WLAN_ERROR_NONE)
		wlcm_e("unable to start network");
}

static void uap_get_channel()
{
	struct wlan_network sta_network;
	struct wlan_network uap_network;
	unsigned char sta_found = 0;

	/* check if the STA is connected */
	if (wlan_get_current_network(&sta_network) == WM_SUCCESS)
		sta_found = 1;

	if (!is_uap_started()) {
		/* uAP not started */
		wmprintf("uAP Channel = %d\r\n", uap_config.channel);
	} else {
		if (wlan_get_current_uap_network(&uap_network) == WM_SUCCESS) {
			if (sta_found == 1) {
				/* uAP started and STA connected. So uAP channel
				 * is the same as STA channel */
				uap_network.channel = sta_network.channel;
			}
			wmprintf("uAP Channel = %d\r\n", uap_network.channel);
		}
	}
}

static void uap_get_max_sta_num()
{
	unsigned int max_sta_num;

	wlan_get_uap_max_clients(&max_sta_num);
	wmprintf("uAP Max Station number = %d\r\n", max_sta_num);
}

static void uaputl_get_param_values(int argc, char **argv)
{
	if (!strcmp(argv[1], "sys_cfg_ssid")) {
		wmprintf("uAP SSID = %s\r\n", uap_config.ssid);
		return;
	}
	if (!strcmp(argv[1], "bss_start")) {
		uap_start_network(argc, argv);
		return;
	}
	if (!strcmp(argv[1], "bss_stop")) {
		test_wlan_stop_network(argc, argv);
		return;
	}
	if (!strcmp(argv[1], "sys_cfg_channel")) {
		uap_get_channel();
		return;
	}
	if (!strcmp(argv[1], "sys_cfg_max_sta_num")) {
		uap_get_max_sta_num();
		return;
	}
	if (!strcmp(argv[1], "sys_cfg_tx_power")) {
		uaputl_tx_power_getset(argc, argv);
		return;
	}
	if (!strcmp(argv[1], "sys_cfg_rates")) {
		uaputl_rates_getset(argc, argv);
		return;
	}

	wlcm_e("Incorrect parameter");
	dump_uaputl_usage();
}

static void uaputl_set_param_values(int argc, char **argv)
{
	/* Only rates and regrdwr can allow argc > 3 */
	if (strcmp(argv[1], "sys_cfg_rates")
		&& strcmp(argv[1], "regrdwr") && argc != 3)
		goto done;

	if (!strcmp(argv[1], "sys_cfg_ssid")) {
		if (strlen(argv[2]) > IEEEtypes_SSID_SIZE) {
			wlcm_e("SSID is too long");
			return;
		}
		memset(&uap_config, 0, sizeof(uap_config));
		memcpy(uap_config.ssid, argv[2], strlen(argv[2]));
		wmprintf("SSID setting successful\r\n");
		wmprintf("A new profile named 'uap' has been created "
			"with ssid %s.\r\n", uap_config.ssid);
		return;
	}
	if (!strcmp(argv[1], "sys_cfg_wpa_passphrase")) {
		if (!strcmp(uap_config.ssid, NULL)) {
			wmprintf("Enter a ssid and then the"
				" passphrase for it.\r\n");
			return;
		}
		if (strlen(argv[2]) < MIN_WPA2_PASSPHRASE_LENGTH) {
			wlcm_e("WPA2 passphrase should atleast"
				" 8 characters in length\r\n");
			return;
		}
		uap_config.security.type = WLAN_SECURITY_WPA2;
		uap_config.security.psk_len = strlen(argv[2]);

		if (get_security(1, &argv[2], WLAN_SECURITY_WPA2,
			&uap_config.security))
			wlcm_e("Incorrect WPA2 passphrase\r\n");

		wmprintf("WPA passphrase setting successful");
		return;
	}
	if (!strcmp(argv[1], "sys_cfg_channel")) {
		if (is_uap_started()) {
			wlcm_e("Cannot set the channel "
				"as the uAP is already running\r\n");
		} else {
			uap_config.channel = (uint8_t) atoi(argv[2]);
			uap_config.channel_specific = 1;
		}
		return;
	}
	if (!strcmp(argv[1], "sys_cfg_max_sta_num")) {
		if (wlan_set_uap_max_clients((uint8_t) atoi(argv[2]))
						== WM_SUCCESS)
			wmprintf("uAP Max Station number "
				"= %d\r\n", (uint8_t) atoi(argv[2]));
		return;
	}
	if (!strcmp(argv[1], "sys_cfg_tx_power")) {
		uaputl_tx_power_getset(argc, argv);
		return;
	}

	if (!strcmp(argv[1], "sys_cfg_rates")) {
		uaputl_rates_getset(argc, argv);
		return;
	}

	if (!strcmp(argv[1], "regrdwr")) {
		if (wlan_regrdwr_getset(argc, argv) == WM_SUCCESS)
			return;
	}

done:
	wlcm_e("Incorrect parameter/usage");
	dump_uaputl_usage();
}

static void test_uaputl_commands(int argc, char **argv)
{
	switch (argc) {
	case 1:
		dump_uaputl_usage();
		break;
	case 2:
		uaputl_get_param_values(argc, argv);
		break;
	default:
		uaputl_set_param_values(argc, argv);
	}

}
static struct cli_command uaputl_tests[] = {
	{"uaputl", "Support for uaputl utility", test_uaputl_commands},
};

int wlan_uaputl_cli_init(void)
{
	int i;

	for (i = 0; i < sizeof(uaputl_tests) / sizeof(struct cli_command); i++)
		if (cli_register_command(&uaputl_tests[i]))
			return WLAN_ERROR_ACTION;
	return WLAN_ERROR_NONE;
}
