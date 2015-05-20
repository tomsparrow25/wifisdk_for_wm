/*
 *  Copyright 2008-2013, Marvell International Ltd.
 *  All Rights Reserved.
 */

#include <wifi.h>
#include <stdint.h>
#include <cli_utils.h>
#include <wifidirectuaputl.h>
#include <wm_os.h>
#include <wifi_common.h>

/**
 * Check of decimal or hex string
 * @param   num string
 */
#define IS_HEX_OR_DIGIT(num) \
	(strncasecmp("0x", (num), 2) ? ISDIGIT((num)) : ishexstring((num)))

/* fixme: duplicated */
struct bus_message {
	uint16_t event;
	uint16_t reason;
	void *data;
};

int bus_register_special_queue(xQueueHandle *special_queue);
void bus_deregister_special_queue(void);

extern int wifi_get_command_lock(void);
extern int wifi_put_command_lock(void);

/** Convert character to integer */
#define CHAR2INT(x) (((x) >= 'A') ? ((x) - 'A' + 10) : ((x) - '0'))

char *sys_config[] = {
	"ap_config={",
	"SSID=\"DIRECT-\"",	/* SSID for WifiDirect*/
	"BeaconPeriod=100",	/* Beacon period in TU*/
	"Channel=6",		/* Radio Channel 6*/
/*   Channel=0,1                     // auto channel select (MCS mode)*/
/*   Channel=6,2               // primary channel 6, secondary channel above.*/
/*   Channel=6,4               // primary channel 6, secondary channel below*/
	"ChanList=1,6,11",	/* Scan channel list */
/*   AP_MAC=00:34:22:77:46:41        // MAC address of AP*/
	"RxAntenna=0",		/* 0 for Antenna A*/
	/* 1 for Antenna B*/
	"TxAntenna=0",		/* 0 for Antenna A */
	/* 1 for Antenna B*/
	"Rate=0x8C,0x12,0x18,0x24,0x30,0x48,0x60,0x6c",
	/* Set of data rate that a station*/
	/* in the BSS may use*/
	/* (in unit of 500 kilobit/s)*/
	"TxPowerLevel=13",	/* Transmit power level in dBm*/
	"BroadcastSSID=1",	/* Broadcast SSID feature*/
	/* 1: Enable  0:Disable*/
	"RTSThreshold=2347",	/* RTS threshold value*/
	"FragThreshold=2346",	/* Fragmentation threshold value*/
	"DTIMPeriod=1",		/* DTIM period in beacon periods */
	"MCBCdataRate=0",	/* MCBC rate to use for packet transmission*/
	/* 0:auto*/
	/* >0 fix rate (in unit of 500 kilobit/s)    */
	"PktFwdCtl=1",		/* Packet forwarding control*/
	/* 0: forward all packets to the host*/
	/* 1: firmware handles intr-BSS packets*/
	"StaAgeoutTimer=1800",	/* Inactive client station age out
				timer value in units of 100ms*/
	"PSStaAgeoutTimer=400",	/* Inactive client PS station age out
				timer value in units of 100ms*/
	"MaxStaNum=8",		/* Max number of stations allowed to connect*/
	"Retrylimit=7",		/* Retry limit to use for
				packet transmissions*/
	"AuthMode=0",		/* 0:Open authentication*/
	/* 1:shared key authentication*/
	"Protocol=32",		/* protocol to use*/
	/* 1: No security   2: Static WEP*/
	/* 8: WPA   32: WPA2    40:WPA2 Mixed Mode*/
	"RSNReplayProtection=0",	/* RSN replay protection
					0: disabled, 1: enabled*/
	"PairwiseUpdateTimeout=2000",	/*Pairwise Handshake update
					timeout: 2000ms*/
	"PairwiseHandshakeRetries=3",	/*Pairwise Handshake retries: 3*/
	"GroupwiseUpdateTimeout=2000",	/*Groupwise Handshake update
					timeout: 2000ms*/
	"GroupwiseHandshakeRetries=3",	/*Groupwise Handshake retries: 3*/

	"PwkCipherWPA2=8",	/* Pairwise cipher type*/
	"GwkCipher=8",		/* group cipher type*/
	"PSK=\"1234567890\"",	/* WPA/WPA2 passphrase       */

	"GroupRekeyTime=86400",	/* Group key re-key interval, in second.*/
	/* 0 mean never re-key*/

/*    "Enable11n=0",                  // 1 to enable, 0 to disable */
/*    "HTCapInfo=0x111c",             // HTCapInfo */
	/* Bit 15-13: Reserved set to 0*/
	/* Bit 12: DSS/CCK mode in 40MHz enable/disable*/
	/* Bit 11-10: Reserved set to 0*/
	/* Bit 9-8: Reserved set to 0x01*/
	/* Bit 7: Reserved set to 0*/
	/* Bit 6: Short GI in 40 Mhz enable/disable*/
	/* Bit 5: Short GI in 20 Mhz enable/disable*/
	/* Bit 4: Green field enable/disble*/
	/* Bit 3-2: Reserved set to 1*/
	/* Bit 1: 20/40 Mhz enable disable.*/
	/* Bit 0: Reserved set to 0*/
/*    "AMPDU=0x03",                      // AMPDU*/
	/* Bit 7-5: Reserved set to 0*/
	/* Bit 4-2: Minimum MPDU Start spacing*/
	/*          Set to 0 for no restriction*/
	/*          Set to 1 for 1/4 us*/
	/*          Set to 2 for 1/2 us*/
	/*          Set to 3 for 1 us*/
	/*          Set to 4 for 2 us*/
	/*          Set to 5 for 4 us*/
	/*          Set to 6 for 8 us*/
	/*          Set to 7 for 16 us*/
	/* Bit 1-0: Max A-MPDU length*/
	/*802.11D specific configuration*/
/*    "11d_enable=0",                    // 0-disable 1-enable*/
	"}",
	"XXEODXX",		/* End of DATA*/
};

/**
 *  @brief Check protocol is valid or not
 *
 *  @param protocol Protocol
 *
 *  @return         UAP_SUCCESS or UAP_FAILURE
 */
int is_protocol_valid(int protocol)
{
	int ret = UAP_FAILURE;
	switch (protocol) {
	case PROTOCOL_NO_SECURITY:
	case PROTOCOL_STATIC_WEP:
	case PROTOCOL_WPA:
	case PROTOCOL_WPA2:
	case PROTOCOL_WPA2_MIXED:
		ret = UAP_SUCCESS;
		break;
	default:
		wmprintf("ERR: Invalid Protocol: %d\r\n", protocol);
		break;
	}
	return ret;
}

/**
 *  @brief Function to check valid rate
 *
 *
 *  @param  rate    Rate to verify
 *
 *  return          UAP_SUCCESS or UAP_FAILURE
 **/
int is_rate_valid(int rate)
{
	int ret = UAP_SUCCESS;
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
		ret = UAP_FAILURE;
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
 *  @return UAP_FAILURE or UAP_SUCCESS
 */
inline int has_dup_rate(int argc, char *argv[])
{
	int i, j;
	/* Check for duplicate */
	for (i = 0; i < (argc - 1); i++) {
		for (j = i + 1; j < argc; j++) {
			if ((A2HEXDECIMAL(argv[i]) & ~BASIC_RATE_SET_BIT) ==
			    (A2HEXDECIMAL(argv[j]) & ~BASIC_RATE_SET_BIT))
				return UAP_FAILURE;
		}
	}
	return UAP_SUCCESS;
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
 *  @return         UAP_FAILURE or UAP_SUCCESS
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
#ifdef WIFI_DIRECT_SUPPORT
	if (strncmp(dev_name, "wfd", 3))
#endif
		if ((rate_bitmap & B_RATE_MANDATORY) != B_RATE_MANDATORY) {
			if (cck_enable) {
				wmprintf
				    ("Basic Rates 2, 4, 11 and 22"
					" (500K units)\r\n"
				     "must be present in basic or"
					" non-basic rates\r\n");
				return UAP_FAILURE;
			}
		}
	if (ofdm_enable &&
	    ((rate_bitmap & G_RATE_MANDATORY) != G_RATE_MANDATORY)) {
		wmprintf("OFDM Rates 12, 24 and 48 ( 500Kb units)\r\n"
			 "must be present in basic or non-basic rates\r\n");
		return UAP_FAILURE;
	}
	return UAP_SUCCESS;
}

/**
 *  @brief  Detects if band is different across the list of scan channels
 *
 *  @param  argc    Number of elements
 *  @param  argv    Array of strings
 *  @return UAP_FAILURE or UAP_SUCCESS
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
			return UAP_FAILURE;
	}
	return UAP_SUCCESS;
}

/**
 * @brief Checks if given channel in 'a' band is valid or not.
 *
 * @param channel   Channel number
 * @return          UAP_SUCCESS or UAP_FAILURE
 */
int is_valid_a_band_channel(int channel)
{
	int ret = UAP_SUCCESS;
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
		ret = UAP_FAILURE;
		break;
	}
	return ret;
}

/**
 * @brief Checks if secondary channel can be set above given primary
 * channel in 'a' band or not.
 *
 * @param channel   Channel number
 * @return          UAP_SUCCESS or UAP_FAILURE
 */
int is_valid_a_band_channel_above(int channel)
{
	int ret = UAP_SUCCESS;
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
		ret = UAP_FAILURE;
		break;
	}
	return ret;
}

/**
 * @brief Checks if secondary channel can be set below given primary
 * channel in 'a' band or not.
 *
 * @param channel   Channel number
 * @return          UAP_SUCCESS or UAP_FAILURE
 */
int is_valid_a_band_channel_below(int channel)
{
	int ret = UAP_SUCCESS;
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
		ret = UAP_FAILURE;
		break;
	}
	return ret;
}

/**
 *  @brief Checkes a particular input for validatation.
 *
 *  @param cmd      Type of input
 *  @param argc     Number of arguments
 *  @param argv     Pointer to the arguments
 *  @return         UAP_SUCCESS or UAP_FAILURE
 */
int is_input_valid(valid_inputs cmd, int argc, char *argv[])
{
	int i;
	int chan_number = 0;
	int band = 0;
	int ch;
	int ret = UAP_SUCCESS;
	if (argc == 0)
		return UAP_FAILURE;
	switch (cmd) {
	case RDEEPROM:
		if (argc != 2) {
			wmprintf(" ERR: Argument count mismatch\r\n");
			ret = UAP_FAILURE;
		} else {
			if ((ISDIGIT(argv[0]) == 0) || (ISDIGIT(argv[1]) == 0)
			    || (A2HEXDECIMAL(argv[0]) & 0x03) ||
			    ((int)(A2HEXDECIMAL(argv[0])) < 0) ||
			    (A2HEXDECIMAL(argv[1]) & 0x03) ||
			    (A2HEXDECIMAL(argv[1]) < 4) ||
			    (A2HEXDECIMAL(argv[1]) > 20)) {
				wmprintf
			    (" ERR: Invalid inputs for Read EEPROM\r\n");
				ret = UAP_FAILURE;
			}
		}
		break;
	case SCANCHANNELS:
		if (argc > MAX_CHANNELS) {
			wmprintf("ERR: Invalid List of Channels\r\n");
			ret = UAP_FAILURE;
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
					ret = UAP_FAILURE;
					break;
				}
				if ((chan_number > MAX_CHANNELS_BG) &&
				    !(is_valid_a_band_channel(chan_number))) {
					wmprintf
					    ("ERR: Invalid Channel"
					" in 'a' band!\r\n");
					ret = UAP_FAILURE;
					break;
				}
				if ((band < -1) || (band > 1)) {
					wmprintf
					    ("ERR:Band must be"
					" either 0 or 1\r\n");
					ret = UAP_FAILURE;
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
						ret = UAP_FAILURE;
						break;
					}
				}
			}
			if ((ret != UAP_FAILURE) &&
			    (has_dup_channel(argc, argv) != UAP_SUCCESS)) {
				wmprintf
			    ("ERR: Duplicate channel values entered\r\n");
				ret = UAP_FAILURE;
			}
			if ((ret != UAP_FAILURE) &&
			    (has_diff_band(argc, argv) != UAP_SUCCESS)) {
				wmprintf
				    ("ERR: Scan channel list should contain"
				" channels from only one band\r\n");
				ret = UAP_FAILURE;
			}
		}
		break;
	case TXPOWER:
		if ((argc > 1) || (ISDIGIT(argv[0]) == 0)) {
			wmprintf("ERR:Invalid Transmit power\r\n");
			ret = UAP_FAILURE;
		} else {
			if ((atoi(argv[0]) < MIN_TX_POWER) ||
			    (atoi(argv[0]) > MAX_TX_POWER)) {
				wmprintf
				    ("ERR: TX Powar must be in the rage"
				" of %d to %d.\r\n",
				     MIN_TX_POWER, MAX_TX_POWER);
				ret = UAP_FAILURE;
			}
		}
		break;
	case PROTOCOL:
		if ((argc > 1) || (ISDIGIT(argv[0]) == 0)) {
			wmprintf("ERR:Invalid Protocol\r\n");
			ret = UAP_FAILURE;
		} else
			ret = is_protocol_valid(atoi(argv[0]));
		break;
	case CHANNEL:
		if ((argc != 1) && (argc != 2)) {
			wmprintf("ERR: Incorrect arguments for channel.\r\n");
			ret = UAP_FAILURE;
		} else {
			if (argc == 2) {
				if ((ISDIGIT(argv[1]) == 0) ||
				    (atoi(argv[1]) & ~CHANNEL_MODE_MASK)) {
					wmprintf("ERR: Invalid Mode\r\n");
					ret = UAP_FAILURE;
				}
				if ((atoi(argv[1]) & BITMAP_ACS_MODE) &&
				    (atoi(argv[0]) != 0)) {
					wmprintf
					    ("ERR: Channel must be 0 for ACS;"
					" MODE = 1.\r\n");
					ret = UAP_FAILURE;
				}
				if ((atoi(argv[1]) & BITMAP_CHANNEL_ABOVE) &&
				    (atoi(argv[1]) & BITMAP_CHANNEL_BELOW)) {
					wmprintf
					    ("ERR: secondary channel above"
				" and below both are enabled\r\n");
					ret = UAP_FAILURE;
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
					ret = UAP_FAILURE;
				}
				if ((atoi(argv[0]) > MAX_CHANNELS_BG) &&
				    !(is_valid_a_band_channel(atoi(argv[0])))) {
					wmprintf
					    ("ERR: Invalid Channel in"
					" 'a' band!\r\n");
					ret = UAP_FAILURE;
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
						ret = UAP_FAILURE;
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
						ret = UAP_FAILURE;
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
							ret = UAP_FAILURE;
						}
						if ((atoi(argv[1]) &
						     BITMAP_CHANNEL_ABOVE) &&
					    !is_valid_a_band_channel_above
						    (atoi(argv[0]))) {
							wmprintf
							    ("ERR: For given"
					" primary channel secondary channel "
					"can not be set above\r\n");
							ret = UAP_FAILURE;
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
			ret = UAP_FAILURE;
		} else {
			if (argc == 3) {
				if ((ISDIGIT(argv[2]) == 0) ||
				    (atoi(argv[2]) & ~CHANNEL_MODE_MASK)) {
					wmprintf("ERR: Invalid Mode\r\n");
					ret = UAP_FAILURE;
				}
				if ((atoi(argv[2]) & BITMAP_ACS_MODE) &&
				    (atoi(argv[0]) != 0)) {
					wmprintf
					    ("ERR: Channel must be 0 for ACS;"
					" MODE = 1.\r\n");
					ret = UAP_FAILURE;
				}
				if ((atoi(argv[2]) & BITMAP_CHANNEL_ABOVE) &&
				    (atoi(argv[2]) & BITMAP_CHANNEL_BELOW)) {
					wmprintf
					    ("ERR: secondary channel above and"
				" below both are enabled\r\n");
					ret = UAP_FAILURE;
				}
			}
			if ((argc == 2) &&
			    ((ISDIGIT(argv[1]) == 0) || (atoi(argv[1]) < 0) ||
			     atoi(argv[1]) > 1)) {
				wmprintf("ERR:Invalid band\r\n");
				ret = UAP_FAILURE;
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
					ret = UAP_FAILURE;
				}
				if ((atoi(argv[0]) > MAX_CHANNELS_BG) &&
				    !(is_valid_a_band_channel(atoi(argv[0])))) {
					wmprintf
					    ("ERR: Invalid Channel"
					" in 'a' band!\r\n");
					ret = UAP_FAILURE;
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
						ret = UAP_FAILURE;
					}
					if ((argc == 3) &&
					    (atoi(argv[2]) &
					     BITMAP_CHANNEL_BELOW) &&
					    (atoi(argv[0]) <
					     MIN_CHANNEL_BELOW)) {
						wmprintf
						    ("ERR: only allow channel"
				" 5-11 for secondary channel below\r\n");
						ret = UAP_FAILURE;
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
							ret = UAP_FAILURE;
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
							ret = UAP_FAILURE;
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
			ret = UAP_FAILURE;
		} else {
			if ((ISDIGIT(argv[0]) == 0) || (atoi(argv[0]) < 0) ||
			    (atoi(argv[0]) > 1)) {
				wmprintf("ERR: Invalid band.\r\n");
				ret = UAP_FAILURE;
			}
		}
		break;
	case RATE:
		if (argc > MAX_RATES) {
			wmprintf("ERR: Incorrect number of RATES "
				"arguments.\r\n");
			ret = UAP_FAILURE;
		} else {
			for (i = 0; i < argc; i++) {
				if ((IS_HEX_OR_DIGIT(argv[i]) == UAP_FAILURE) ||
				    (is_rate_valid
				     (A2HEXDECIMAL(argv[i]) &
				      ~BASIC_RATE_SET_BIT)
				     != UAP_SUCCESS)) {
					wmprintf("ERR:Unsupported rate.\r\n");
					ret = UAP_FAILURE;
					break;
				}
			}
			if ((ret != UAP_FAILURE) &&
			    (has_dup_rate(argc, argv) != UAP_SUCCESS)) {
				wmprintf
				    ("ERR: Duplicate rate values entered\r\n");
				ret = UAP_FAILURE;
			}
			if (check_mandatory_rates(argc, argv) != UAP_SUCCESS)
				ret = UAP_FAILURE;
		}
		break;
	case BROADCASTSSID:
		if (argc != 1) {
			wmprintf("ERR:wrong BROADCASTSSID arguments.\r\n");
			ret = UAP_FAILURE;
		} else {
			if ((ISDIGIT(argv[0]) == 0) ||
			    ((atoi(argv[0]) != 0) && (atoi(argv[0]) != 1))) {
				wmprintf
				    ("ERR:Illegal parameter %s for "
			"BROADCASTSSID. Must be either '0' or '1'.\r\n",
				     argv[0]);
				ret = UAP_FAILURE;
			}
		}
		break;
	case RTSTHRESH:
		if (argc != 1) {
			wmprintf
			    ("ERR:Incorrect number of arguments"
			" for RTSTHRESHOLD\r\n");
			ret = UAP_FAILURE;
		} else if ((ISDIGIT(argv[0]) == 0) || (atoi(argv[0]) < 0) ||
			   (atoi(argv[0]) > MAX_RTS_THRESHOLD)) {
			wmprintf
			    ("ERR:Illegal RTSTHRESHOLD %s. The value must"
			" between 0 and %d\r\n",
			     argv[0], MAX_RTS_THRESHOLD);
			ret = UAP_FAILURE;
		}
		break;
	case FRAGTHRESH:
		if (argc != 1) {
			wmprintf
			    ("ERR:Incorrect number of arguments"
			" for FRAGTHRESH\r\n");
			ret = UAP_FAILURE;
		} else if ((ISDIGIT(argv[0]) == 0) ||
			   (atoi(argv[0]) < MIN_FRAG_THRESHOLD) ||
			   (atoi(argv[0]) > MAX_FRAG_THRESHOLD)) {
			wmprintf
			    ("ERR:Illegal FRAGTHRESH %s. The value must"
			" between %d and %d\r\n",
			     argv[0], MIN_FRAG_THRESHOLD, MAX_FRAG_THRESHOLD);
			ret = UAP_FAILURE;
		}
		break;
	case DTIMPERIOD:
		if (argc != 1) {
			wmprintf
			    ("ERR:Incorrect number of"
			" arguments for DTIMPERIOD\r\n");
			ret = UAP_FAILURE;
		} else if ((ISDIGIT(argv[0]) == 0) || (atoi(argv[0]) < 1) ||
			   (atoi(argv[0]) > MAX_DTIM_PERIOD)) {
			wmprintf
			    ("ERR: DTIMPERIOD Value must be in"
			" range of 1 to %d\r\n",
			     MAX_DTIM_PERIOD);
			ret = UAP_FAILURE;
		}
		break;
	case RSNREPLAYPROT:
		if (argc != 1) {
			wmprintf
			    ("ERR:Incorrect number of arguments"
			" for RSNREPLAYPROT\r\n");
			ret = UAP_FAILURE;
		} else {
			if ((ISDIGIT(argv[0]) == 0) || (atoi(argv[0]) < 0) ||
			    (atoi(argv[0]) > 1)) {
				wmprintf
				    ("ERR:Illegal RSNREPLAYPROT parameter %s."
				" Must be either '0' or '1'.\r\n",
				     argv[0]);
				ret = UAP_FAILURE;
			}
		}
		break;
	case RADIOCONTROL:
		if (argc != 1) {
			wmprintf
			    ("ERR:Incorrect number of arguments"
				" for RADIOCONTROL\r\n");
			ret = UAP_FAILURE;
		} else {
			if ((ISDIGIT(argv[0]) == 0) || (atoi(argv[0]) < 0) ||
			    (atoi(argv[0]) > 1)) {
				wmprintf
				    ("ERR:Illegal RADIOCONTROL parameter"
				" %s. Must be either '0' or '1'.\r\n",
				     argv[0]);
				ret = UAP_FAILURE;
			}
		}
		break;
	case TXRATECFG:
		if (argc != 1) {
			wmprintf
			    ("ERR:Incorrect number of arguments"
				" for DATARATE\r\n");
			ret = UAP_FAILURE;
		} else {
			if (IS_HEX_OR_DIGIT(argv[0]) == UAP_FAILURE) {
				wmprintf("ERR: invalid Tx data rate\r\n");
				ret = UAP_FAILURE;
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
			ret = UAP_FAILURE;
		} else {
			if (IS_HEX_OR_DIGIT(argv[0]) == UAP_FAILURE) {
				wmprintf("ERR: invalid data rate\r\n");
				ret = UAP_FAILURE;
			} else if ((A2HEXDECIMAL(argv[0]) != 0) &&
				   (is_rate_valid
				    (A2HEXDECIMAL(argv[0]) &
				     ~BASIC_RATE_SET_BIT) != UAP_SUCCESS)) {
				wmprintf("ERR: invalid data rate\r\n");
				ret = UAP_FAILURE;
			}
		}
		break;
	case PKTFWD:
		if (argc != 1) {
			wmprintf
			    ("ERR:Incorrect number of arguments "
				"for PKTFWD.\r\n");
			ret = UAP_FAILURE;
		} else if ((ISDIGIT(argv[0]) == 0) ||
			   ((atoi(argv[0]) < 0) || (atoi(argv[0]) > 15))) {
			wmprintf
			    ("ERR:Illegal PKTFWD parameter %s. Must be within"
				" '0' and '15'.\r\n",
			     argv[0]);
			ret = UAP_FAILURE;
		}
		break;
	case STAAGEOUTTIMER:
		if (argc != 1) {
			wmprintf
			    ("ERR:Incorrect number of arguments for"
			" STAAGEOUTTIMER.\r\n");
			ret = UAP_FAILURE;
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
				ret = UAP_FAILURE;
			}
		}
		break;
	case PSSTAAGEOUTTIMER:
		if (argc != 1) {
			wmprintf
			    ("ERR:Incorrect number of arguments"
			" for PSSTAAGEOUTTIMER.\r\n");
			ret = UAP_FAILURE;
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
				ret = UAP_FAILURE;
			}
		}
		break;
	case AUTHMODE:
		if (argc != 1) {
			wmprintf
			    ("ERR:Incorrect number of"
			" arguments for AUTHMODE\r\n");
			ret = UAP_FAILURE;
		} else {
			if ((ISDIGIT(argv[0]) == 0) || ((atoi(argv[0]) != 0) &&
							(atoi(argv[0]) != 1) &&
							(atoi(argv[0]) !=
							 255))) {
				wmprintf
				    ("ERR:Illegal AUTHMODE parameter %s. Must"
				" be either '0','1' or 255''.\r\n",
				     argv[0]);
				ret = UAP_FAILURE;
			}
		}
		break;
	case GROUPREKEYTIMER:
		if (argc != 1) {
			wmprintf
			    ("ERR:Incorrect number of arguments"
			" for GROUPREKEYTIMER.\r\n");
			ret = UAP_FAILURE;
		} else {
			if ((ISDIGIT(argv[0]) == 0) || (atoi(argv[0]) < 0) ||
			    (atoi(argv[0]) > MAX_GRP_TIMER)) {
				wmprintf
				    ("ERR: GROUPREKEYTIMER range"
				" is [0:%d] (0 for disable)\r\n",
				     MAX_GRP_TIMER);
				ret = UAP_FAILURE;
			}
		}
		break;
	case MAXSTANUM:
		if (argc != 1) {
			wmprintf
			    ("ERR:Incorrect number of"
				" arguments for MAXSTANUM\r\n");
			ret = UAP_FAILURE;
		} else {
			if ((ISDIGIT(argv[0]) == 0) || (atoi(argv[0]) <= 0)) {
				wmprintf("ERR:Invalid STA_NUM argument %s.\r\n",
					 argv[0]);
				ret = UAP_FAILURE;
			}
		}
		break;
	case BEACONPERIOD:
		if (argc != 1) {
			wmprintf
			    ("ERR:Incorrect number of argument"
				" for BEACONPERIOD.\r\n");
			ret = UAP_FAILURE;
		} else {
			if ((ISDIGIT(argv[0]) == 0) ||
			    (atoi(argv[0]) < MIN_BEACON_PERIOD)
			    || (atoi(argv[0]) > MAX_BEACON_PERIOD)) {
				wmprintf
				    ("ERR: BEACONPERIOD must be"
					" in range of %d to %d.\r\n",
				     MIN_BEACON_PERIOD, MAX_BEACON_PERIOD);
				ret = UAP_FAILURE;
			}
		}
		break;
	case RETRYLIMIT:
		if (argc != 1) {
			wmprintf
			    ("ERR:Incorrect number of arguments"
				" for RETRY LIMIT\r\n");
			ret = UAP_FAILURE;
		} else {
			if ((ISDIGIT(argv[0]) == 0) ||
			    (atoi(argv[0]) > MAX_RETRY_LIMIT) ||
			    (atoi(argv[0]) < 0)) {
				wmprintf
				    ("ERR:RETRY_LIMIT must be in the range"
				" of [0:%d]. The  input was %s.\r\n",
				     MAX_RETRY_LIMIT, argv[0]);
				ret = UAP_FAILURE;
			}
		}
		break;
	case STICKYTIMCONFIG:
		if ((argc != 1) && (argc != 3)) {
			wmprintf
			    ("ERR:Incorrect number of arguments"
			" for STICKY_TIM_CONFIG\r\n");
			ret = UAP_FAILURE;
		} else {
			if ((ISDIGIT(argv[0]) == 0) || (atoi(argv[0]) < 0) ||
			    (atoi(argv[0]) > 2)) {
				wmprintf
				    ("ERR:Enable parameter must"
					" be 0, 1 or 2\r\n");
				ret = UAP_FAILURE;
				break;
			}
			if (((atoi(argv[0]) != 1) && (argc > 1))) {
				wmprintf("ERR: Invalid arguments\r\n");
				ret = UAP_FAILURE;
				break;
			}
			if ((atoi(argv[0]) == 1) && (argc != 3)) {
				wmprintf
				    ("ERR: Both duration and sticky bit mask"
				" must be provided for ENABLE = 1\r\n");
				ret = UAP_FAILURE;
				break;
			}
			if (argc > 1) {
				if ((ISDIGIT(argv[1]) == 0)) {
					wmprintf("ERR: Invalid duration\r\n");
					ret = UAP_FAILURE;
					break;
				}
				if ((ISDIGIT(argv[2]) == 0) ||
				    (atoi(argv[2]) < 1) ||
				    (atoi(argv[2]) > 3)) {
					wmprintf
					    ("ERR:Invalid sticky bit mask\r\n");
					ret = UAP_FAILURE;
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
			ret = UAP_FAILURE;
		} else {
			if ((argc == 2) &&
			    ((ISDIGIT(argv[0]) == 0) || (atoi(argv[0]) < 0) ||
			     (atoi(argv[0]) > 1))) {
				wmprintf("ERR:Invalid control parameter\r\n");
				ret = UAP_FAILURE;
				break;
			}
		}
		break;
	case COEX2040CONFIG:
		if (argc != 1) {
			wmprintf
			    ("ERR: Incorrect number of 2040 COEX"
				" CONFIG arguments\r\n");
			ret = UAP_FAILURE;
		} else {
			if ((ISDIGIT(argv[0]) == 0) || (atoi(argv[0]) < 0) ||
			    (atoi(argv[0]) > 1)) {
				wmprintf("ERR:Invalid enable parameter\r\n");
				ret = UAP_FAILURE;
				break;
			}
		}
		break;
	case EAPOL_PWK_HSK:
		if (argc != 2) {
			wmprintf
			    ("ERR:Incorrect number of"
				" EAPOL_PWK_HSK arguments.\r\n");
			ret = UAP_FAILURE;
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
			ret = UAP_FAILURE;
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
			ret = UAP_FAILURE;
		} else {
			if ((ISDIGIT(argv[0]) == 0) ||
			    (atoi(argv[0]) > MAX_PREAMBLE_TYPE)
			    || (atoi(argv[0]) < 0)) {
				wmprintf
				    ("ERR:PREAMBLE TYPE must be in the range"
					" of [0:%d]. The  input was %s.\r\n",
				     MAX_PREAMBLE_TYPE, argv[0]);
				ret = UAP_FAILURE;
			}
		}
		break;

	case COEX_COMM_BITMAP:
		if (argc != 1) {
			wmprintf
			    ("ERR:Incorrect number of argument "
				"for Bitmap.\r\n");
			ret = UAP_FAILURE;
		} else {
		/* Only bit 0 is supported now, hence check for 1 or 0 */
			if ((IS_HEX_OR_DIGIT(argv[0]) == 0) ||
			    (atoi(argv[0]) < 0) || (atoi(argv[0]) > 1)) {
				wmprintf
				    ("ERR: Bitmap must have"
				" value of 1 or 0.\r\n");
				ret = UAP_FAILURE;
			}
		}
		break;
	case COEX_COMM_AP_COEX:
		if (argc != 1) {
			wmprintf
			    ("ERR:Incorrect number"
				" of argument for APBTCoex.\r\n");
			ret = UAP_FAILURE;
		} else {
			if ((IS_HEX_OR_DIGIT(argv[0]) == 0) ||
			    (atoi(argv[0]) < 0) || (atoi(argv[0]) > 1)) {
				wmprintf
				    ("ERR: APBTCoex must have"
					" value of 1 or 0.\r\n");
				ret = UAP_FAILURE;
			}
		}
		break;
	case COEX_SCO_ACL_FREQ:
		if (argc != 1) {
			wmprintf
			    ("ERR:Incorrect number of arguments"
				" for aclFrequency.\r\n");
			ret = UAP_FAILURE;
		} else {
			if (ISDIGIT(argv[0]) == 0) {
				wmprintf
				    ("ERR: Incorrect value for"
					" aclFrequency.\r\n");
				ret = UAP_FAILURE;
			}
		}
		break;
	case COEX_ACL_ENABLED:
		if (argc != 1) {
			wmprintf
			    ("ERR:Incorrect number of arguments for"
				" (acl) enabled.\r\n");
			ret = UAP_FAILURE;
		} else {
			if ((ISDIGIT(argv[0]) == 0) || (atoi(argv[0]) < 0) ||
			    (atoi(argv[0]) > 1)) {
				wmprintf
				    ("ERR: (acl) enabled must have"
					" value of 1 or 0.\r\n");
				ret = UAP_FAILURE;
			}
		}
		break;
	case COEX_ACL_BT_TIME:
	case COEX_ACL_WLAN_TIME:
		if (argc != 1) {
			wmprintf
			    ("ERR:Incorrect number of arguments"
				" for bt/wlan time.\r\n");
			ret = UAP_FAILURE;
		} else {
			if (ISDIGIT(argv[0]) == 0) {
				wmprintf
				    ("ERR: Incorrect value for"
					" bt/wlan time.\r\n");
				ret = UAP_FAILURE;
			}
		}
		break;
	case COEX_PROTECTION:
		if (argc != 2) {
			wmprintf("ERR:Incorrect number of "
				"arguments for %s.\r\n",
				 argv[0]);
			ret = UAP_FAILURE;
		} else {
			if (ISDIGIT(argv[1]) == 0) {
				wmprintf("ERR: Incorrect value for %s.\r\n",
					 argv[0]);
				ret = UAP_FAILURE;
			}
		}
		break;
	case PWK_CIPHER:
		if ((argc != 1) && (argc != 2)) {
			wmprintf
			    ("ERR:Incorrect number of arguments for"
				" pwk_cipher.\r\n");
			ret = UAP_FAILURE;
		} else {
			if ((ISDIGIT(argv[0]) == 0) ||
			    (atoi(argv[0]) & ~PROTOCOL_BITMAP)) {
				wmprintf("Invalid Protocol paramter.\r\n");
				ret = UAP_FAILURE;
			}
			if (argc == 2) {
				if ((ISDIGIT(argv[1]) == 0) ||
				    (atoi(argv[1]) & ~CIPHER_BITMAP)) {
					wmprintf("Invalid pairwise "
						"cipher.\r\n");
					ret = UAP_FAILURE;
				}
			}
		}
		break;
	case GWK_CIPHER:
		if (argc != 1) {
			wmprintf
			    ("ERR:Incorrect number of arguments for"
				" gwk_cipher.\r\n");
			ret = UAP_FAILURE;
		} else {
			if ((ISDIGIT(argv[0]) == 0) ||
			    (atoi(argv[0]) & ~CIPHER_BITMAP) ||
			    (atoi(argv[0]) == AES_CCMP_TKIP)) {
				wmprintf("Invalid group cipher.\r\n");
				ret = UAP_FAILURE;
			}
		}
		break;
	default:
		ret = UAP_FAILURE;
		break;
	}
	return ret;
}

os_queue_t uap_commad_resp_q;

int wlan_send_gen_sdio_cmd(t_u8 *buf, t_u32 buflen);
int uap_ioctl(t_u8 *cmd, t_u16 *size, t_u16 buf_size)
{
	int ret;
	struct bus_message msg;

	os_queue_pool_t uap_commad_resp_q_data;
	uap_commad_resp_q_data.size = 32;
	static bool Q_created;

	if (!Q_created) {
		ret = os_queue_create(&uap_commad_resp_q, "commad_resp",
		      sizeof(struct bus_message), &uap_commad_resp_q_data);
		if (ret) {
			wfd_e("commad_resp Q failed to create\n\r");
			return -WM_FAIL;
		}
		Q_created = 1;
	}
	wifi_get_command_lock();
	ret = bus_register_special_queue(&uap_commad_resp_q);
	if (ret) {
		wfd_e("bus_register_special_queue failed\n\r");
		ret = UAP_FAILURE;
		goto done;
	}
	wfd_d("wfd_uap_ioctl: Sent");
#ifdef CONFIG_P2P_DEBUG
	hexdump(cmd, buf_size, ' ');
#endif

	wlan_send_gen_sdio_cmd(cmd, buf_size);
	ret = os_queue_recv(&uap_commad_resp_q, &msg, OS_WAIT_FOREVER);
	if (ret != WM_SUCCESS) {
		wfd_e("os_queue_recv &uap_commad_resp_q failed\n\r");
		ret = -WM_FAIL;
		goto done;
	}
	if (buf_size < ((HostCmd_DS_GEN *) msg.data)->size) {
		wfd_e("Truncating wfd_ioctl return buffer");
		memcpy(cmd + 4, msg.data, buf_size);
	} else
		memcpy(cmd + 4, msg.data, ((HostCmd_DS_GEN *) msg.data)->size);

	wfd_d("wfd_ioctl: Received\n");
#ifdef CONFIG_P2P_DEBUG
	if (buf_size < ((HostCmd_DS_GEN *) msg.data)->size)
		hexdump(cmd, buf_size, ' ');
	else
		hexdump(cmd, ((HostCmd_DS_GEN *) msg.data)->size, ' ');
#endif
	ret = UAP_SUCCESS;
done:
	bus_deregister_special_queue();
	wifi_put_command_lock();
	os_mem_free(msg.data);
	return ret;
}

/**
 *  @brief  Get Ht capability Info from firmware
 *
 *  @param  pHtCap  A pointer to HTCap_t structure
 *  @return         UAP_SUCCESS/UAP_FAILURE
 */
int get_sys_cfg_11n(HTCap_t *pHtCap)
{
	apcmdbuf_sys_configure *cmd_buf = NULL;
	tlvbuf_htcap_t *tlv = NULL;
	t_u8 *buffer = NULL;
	t_u16 cmd_len;
	int ret = UAP_FAILURE;
	/* Initialize the command length */
	cmd_len = sizeof(apcmdbuf_sys_configure) + sizeof(tlvbuf_htcap_t);
	/* Initialize the command buffer */
	buffer = (t_u8 *) os_mem_alloc(cmd_len);
	if (!buffer) {
		wmprintf("ERR:Cannot allocate buffer for command!\r\n");
		return -1;
	}
	memset(buffer, 0, cmd_len);

	/* Locate headers */
	cmd_buf = (apcmdbuf_sys_configure *) buffer;
	tlv = (tlvbuf_htcap_t *) (buffer + sizeof(apcmdbuf_sys_configure));

	/* Fill the command buffer */
	cmd_buf->cmd_code = APCMD_SYS_CONFIGURE;
	cmd_buf->size = cmd_len - BUF_HEADER_SIZE;
	cmd_buf->seq_num = (0x01) << 13;
	cmd_buf->result = 0;
	tlv->tag = HT_CAPABILITY_TLV_ID;
	cmd_buf->action = ACTION_GET;
	tlv->length = sizeof(HTCap_t);

	endian_convert_tlv_header_out(tlv);
	/* Send the command */
	ret = uap_ioctl((t_u8 *) cmd_buf, &cmd_len, cmd_len);
	endian_convert_tlv_header_in(tlv);
	/* Process response */
	if (ret == UAP_SUCCESS) {
		/* Verify response */
		if ((cmd_buf->cmd_code !=
		     (APCMD_SYS_CONFIGURE | APCMD_RESP_CHECK)) ||
		    (tlv->tag != HT_CAPABILITY_TLV_ID)) {
			wmprintf
			    ("ERR:Corrupted response! cmd_code=%x,"
				" Tlv->tag=%x\r\n",
			     cmd_buf->cmd_code, tlv->tag);
			os_mem_free(buffer);
			return UAP_FAILURE;
		}

		/* Copy response */
		if (cmd_buf->result == CMD_SUCCESS) {
			memcpy(pHtCap, &tlv->ht_cap, sizeof(HTCap_t));
			pHtCap->ht_cap_info =
			    uap_le16_to_cpu(pHtCap->ht_cap_info);
			pHtCap->tx_bf_cap = uap_le32_to_cpu(pHtCap->tx_bf_cap);
			ret = UAP_SUCCESS;
		} else {
			ret = UAP_FAILURE;
			wmprintf("ERR:Could not get HT capability!\r\n");
		}
	} else {
		wmprintf("ERR:Command sending failed!\r\n");
		ret = UAP_FAILURE;
	}
	if (buffer)
		os_mem_free(buffer);
	return ret;
}

/**
 *  @brief Check cipher is valid or not
 *
 *  @param pairwisecipher    Pairwise cipher
 *  @param groupcipher       Group cipher
 *  @param protocol          Protocol
 *  @param enable_11n        11n enabled or not.
 *  @return         UAP_SUCCESS or UAP_FAILURE
 */
int
is_cipher_valid_with_11n(int pairwisecipher, int groupcipher,
			 int protocol, int enable_11n)
{
	if ((pairwisecipher == CIPHER_NONE) && (groupcipher == CIPHER_NONE))
		return UAP_SUCCESS;
	if (pairwisecipher == CIPHER_TKIP) {
		/* Ok to have TKIP in mixed mode */
		if (enable_11n && protocol != PROTOCOL_WPA2_MIXED) {
			wmprintf
			    ("ERR: WPA/TKIP cannot be used when AP operates"
				" in 802.11n mode.\r\n");
			return UAP_FAILURE;
		}
	}
	if ((pairwisecipher == CIPHER_TKIP) && (groupcipher == CIPHER_TKIP))
		return UAP_SUCCESS;
	if ((pairwisecipher == CIPHER_AES_CCMP) &&
	    (groupcipher == CIPHER_AES_CCMP))
		return UAP_SUCCESS;
	if ((pairwisecipher == CIPHER_AES_CCMP) && (groupcipher == CIPHER_TKIP))
		return UAP_SUCCESS;
	if ((pairwisecipher == CIPHER_BITMAP) && (groupcipher == CIPHER_TKIP))
		return UAP_SUCCESS;
	return UAP_FAILURE;
}

/**
 *  @brief Check cipher is valid or not based on proto
 *
 *  @param pairwisecipher    Pairwise cipher
 *  @param groupcipher       Group cipher
 *  @param protocol          Protocol
 *  @return         UAP_SUCCESS or UAP_FAILURE
 */
int
is_cipher_valid_with_proto(int pairwisecipher, int groupcipher, int protocol)
{
	HTCap_t htcap;

	if ((pairwisecipher == CIPHER_NONE) && (groupcipher == CIPHER_NONE))
		return UAP_SUCCESS;
	if (pairwisecipher == CIPHER_TKIP) {
		/* Ok to have TKIP in mixed mode */
		if (protocol != PROTOCOL_WPA2_MIXED) {
			memset(&htcap, 0, sizeof(htcap));
			if (UAP_SUCCESS == get_sys_cfg_11n(&htcap)) {
				if (htcap.supported_mcs_set[0]) {
					wmprintf
					    ("ERR: WPA/TKIP cannot be used when"
					" AP operates in 802.11n mode.\r\n");
					return UAP_FAILURE;
				}
			}
		}
	}
	if ((pairwisecipher == CIPHER_TKIP) && (groupcipher == CIPHER_TKIP))
		return UAP_SUCCESS;
	if ((pairwisecipher == CIPHER_AES_CCMP) &&
	    (groupcipher == CIPHER_AES_CCMP))
		return UAP_SUCCESS;
	if ((pairwisecipher == CIPHER_AES_CCMP) && (groupcipher == CIPHER_TKIP))
		return UAP_SUCCESS;
	if ((pairwisecipher == CIPHER_BITMAP) && (groupcipher == CIPHER_TKIP))
		return UAP_SUCCESS;
	return UAP_FAILURE;
}

/**
 *  @brief Converts a string to hex value
 *
 *  @param str      A pointer to the string
 *  @param raw      A pointer to the raw data buffer
 *  @return         Number of bytes read
 */
int string2raw(char *str, unsigned char *raw)
{
	int len = (strlen(str) + 1) / 2;

	do {
		if (!isxdigit(*str))
			return -1;
		*str = toupper(*str);
		*raw = CHAR2INT(*str) << 4;
		++str;
		*str = toupper(*str);
		if (*str == '\0')
			break;
		*raw |= CHAR2INT(*str);
		++raw;
	} while (*++str != '\0');
	return len;
}

/**
 *  @brief  Get Max sta num from firmware
 *
 *  @return     max number of stations
 */
int get_max_sta_num_supported(t_u16 *max_sta_num_supported)
{
	apcmdbuf_sys_configure *cmd_buf = NULL;
	tlvbuf_max_sta_num *tlv = NULL;
	t_u8 *buffer = NULL;
	t_u16 cmd_len = 0;
	t_u16 buf_len = MRVDRV_SIZE_OF_CMD_BUFFER;
	int ret = UAP_SUCCESS;

	/* Initialize the command length */
	cmd_len = sizeof(apcmdbuf_sys_configure) + sizeof(tlvbuf_max_sta_num);

	/* Initialize the command buffer */
	buffer = (t_u8 *) os_mem_alloc(buf_len);
	if (!buffer) {
		wmprintf("ERR:Cannot allocate buffer for command!\r\n");
		return UAP_FAILURE;
	}
	memset(buffer, 0, buf_len);

	/* Locate headers */
	cmd_buf = (apcmdbuf_sys_configure *) buffer;
	tlv = (tlvbuf_max_sta_num *) (buffer + sizeof(apcmdbuf_sys_configure));

	/* Fill the command buffer */
	cmd_buf->cmd_code = APCMD_SYS_CONFIGURE;
	cmd_buf->size = cmd_len;
	cmd_buf->seq_num = (0x01) << 13;
	cmd_buf->result = 0;
	tlv->tag = MRVL_MAX_STA_CNT_TLV_ID;
	tlv->length = 4;
	cmd_buf->action = ACTION_GET;

	endian_convert_tlv_header_out(tlv);
	tlv->max_sta_num_configured =
	    uap_cpu_to_le16(tlv->max_sta_num_configured);
	tlv->max_sta_num_supported =
	    uap_cpu_to_le16(tlv->max_sta_num_supported);

	/* Send the command */
	ret = uap_ioctl((t_u8 *) cmd_buf, &cmd_len, buf_len);
	endian_convert_tlv_header_in(tlv);
	tlv->max_sta_num_configured =
	    uap_le16_to_cpu(tlv->max_sta_num_configured);
	tlv->max_sta_num_supported =
	    uap_le16_to_cpu(tlv->max_sta_num_supported);
	/* Process response */
	if (ret == UAP_SUCCESS) {
		/* Verify response */
		if ((cmd_buf->cmd_code !=
		     (APCMD_SYS_CONFIGURE | APCMD_RESP_CHECK)) ||
		    (tlv->tag != MRVL_MAX_STA_CNT_TLV_ID)) {
			wmprintf
			    ("ERR:Corrupted response!"
				" cmd_code=%x, Tlv->tag=%x\r\n",
			     cmd_buf->cmd_code, tlv->tag);
			os_mem_free(buffer);
			return UAP_FAILURE;
		}
		/* Print response */
		if (cmd_buf->result != CMD_SUCCESS) {
			wmprintf("ERR:Could not get max station number!\r\n");
			ret = UAP_FAILURE;
		} else {
			if (tlv->length == 4) {
				*max_sta_num_supported =
				    tlv->max_sta_num_supported;
			} else {
				*max_sta_num_supported = MAX_STA_COUNT;
			}
		}
	} else {
		wmprintf("ERR:Command sending failed!\r\n");
	}
	if (buffer)
		os_mem_free(buffer);
	return ret;
}

/**
 *  @brief      Parse function for a configuration line
 *
 *  @param s        Storage buffer for data
 *  @param size     Maximum size of data
 *  @param array    Which array to read
 *  @param line     Pointer to current line within the file
 *  @param _pos     Output string or NULL
 *  @return     String or NULL
 */
static char *config_get_line(char *s, int size, char *stream, int *line,
			     char **_pos)
{
	char *pos = NULL;
	if (strcmp(stream, "sys_config") == 0) {
		if (strcmp(sys_config[*line], "XXEODXX") == 0)
			pos = NULL;
		else {
			strncpy(s, sys_config[*line], size);
#ifdef CONFIG_P2P_DEBUG
			wmprintf("Line: %s\r\n", s);
#endif
			(*line)++;
			pos = s;
		}
	}
	return pos;
}

/**
 *  @brief Read the profile and sends to the driver
 *
 *  @return         UAP_SUCCESS or UAP_FAILURE
 */
int wifidirectapcmd_sys_config()
{
/*    FILE *config_file = NULL;*/
	char *line = NULL;
	int li = 0;
	char *pos = NULL;
	int arg_num = 0;
	char *args[30];
	int i;
	int is_ap_config = 0;
/*    int is_ap_mac_filter = 0;*/
	apcmdbuf_sys_configure *cmd_buf = NULL;
	HTCap_t htcap;
	int enable_11n = -1;
	t_u16 tlv_offset_11n = 0;
	t_u8 *buffer = NULL;
	t_u16 cmd_len = 0;
	t_u16 tlv_len = 0;
	int keyindex = -1;
	int protocol = -1;
	int pwkcipher_wpa = -1;
	int pwkcipher_wpa2 = -1;
	int gwkcipher = -1;
	/*   tlvbuf_sta_mac_addr_filter *filter_tlv = NULL;*/
	/*tlvbuf_channel_config *channel_band_tlv = NULL;*/
	/*  int filter_mac_count = -1;*/
	int tx_data_rate = -1;
	int mcbc_data_rate = -1;
	t_u8 rate[MAX_RATES];
	int found = 0;
	/*   char country_80211d[4];*/
	t_u8 state_80211d = 0;
	int chan_mode = 0;
	/*int band = 0;*/
	int band_flag = 0;
	int chan_number = 0;
	t_u16 max_sta_num_supported = 0;
	int ret = UAP_SUCCESS;
	memset(rate, 0, MAX_RATES);
	/* Check if file exists */
/*    config_file = fopen(argv[0], "r");*/
/*    if (config_file == NULL) {*/
/*       wmprintf("\r\nERR:Config file can not open.\r\n");*/
/*        return UAP_FAILURE;*/
/*    }*/
	line = os_mem_alloc(MAX_CONFIG_LINE);
	if (!line) {
		wmprintf("ERR:Cannot allocate memory for line\r\n");
		ret = UAP_FAILURE;
		goto done;
	}
	memset(line, 0, MAX_CONFIG_LINE);

	/* Parse file and process */
	while (config_get_line(line, MAX_CONFIG_LINE, "sys_config", &li,
		&pos)) {
#ifdef CONFIG_P2P_DEBUG
		wmprintf("DBG:Received config line (%d) = %s\r\n", li, line);
#endif
		arg_num = parse_line(line, args);
#ifdef CONFIG_P2P_DEBUG
		wmprintf("DBG:Number of arguments = %d\r\n", arg_num);
		for (i = 0; i < arg_num; i++)
			wmprintf("\tDBG:Argument %d. %s\r\n", i + 1, args[i]);
#endif
		/* Check for end of AP configurations */
		if (is_ap_config == 1) {
			if (strcmp(args[0], "}") == 0) {
				is_ap_config = 0;
				if (tx_data_rate != -1) {
#if 0
					if ((!rate[0]) && (tx_data_rate) &&
					    (is_tx_rate_valid
					     ((t_u8) tx_data_rate) !=
					     UAP_SUCCESS)) {
						wmprintf
						    ("ERR: Invalid Tx"
						" Data Rate\r\n");
						ret = UAP_FAILURE;
						goto done;
					}
#endif
					if (rate[0] && tx_data_rate) {
						for (i = 0; rate[i] != 0; i++) {
							if ((rate[i] &
						     ~BASIC_RATE_SET_BIT)
							    == tx_data_rate) {
								found = 1;
								break;
							}
						}
						if (!found) {
							wmprintf
							    ("ERR: Invalid Tx"
							" Data Rate\r\n");
							ret = UAP_FAILURE;
							goto done;
						}
					}
				}
				if (mcbc_data_rate != -1) {
#if 0
					if ((!rate[0]) && (mcbc_data_rate) &&
					    (is_mcbc_rate_valid
					     ((t_u8) mcbc_data_rate) !=
					     UAP_SUCCESS)) {
						wmprintf
						    ("ERR: Invalid Tx Data"
						" Rate\r\n");
						ret = UAP_FAILURE;
						goto done;
					}
#endif
					if (rate[0] && mcbc_data_rate) {
						for (i = 0; rate[i] != 0; i++) {
							if (rate[i] &
						    BASIC_RATE_SET_BIT) {
							if ((rate[i] &
						     ~BASIC_RATE_SET_BIT)
							    ==
							    mcbc_data_rate) {
								found = 1;
								break;
							}
							}
						}
						if (!found) {
							wmprintf
							    ("ERR: Invalid "
							"MCBC Data Rate\r\n");
							ret = UAP_FAILURE;
							goto done;
						}
					}

					/* Append a new TLV */
					tlvbuf_mcbc_data_rate *tlv = NULL;
					tlv_len = sizeof(tlvbuf_mcbc_data_rate);
					buffer =
					    os_mem_realloc(buffer,
							   cmd_len + tlv_len);
					if (!buffer) {
						wmprintf
						    ("ERR:Cannot append tx data"
						" rate TLV!\r\n");
						ret = UAP_FAILURE;
						goto done;
					}
					cmd_buf =
					    (apcmdbuf_sys_configure *) buffer;
					tlv =
					    (tlvbuf_mcbc_data_rate *) (buffer +
								       cmd_len);
					cmd_len += tlv_len;
					/* Set TLV fields */
					tlv->tag = MRVL_MCBC_DATA_RATE_TLV_ID;
					tlv->length = 2;
					tlv->mcbc_datarate = mcbc_data_rate;
					endian_convert_tlv_header_out(tlv);
					tlv->mcbc_datarate =
					   uap_cpu_to_le16(tlv->mcbc_datarate);
				}
				if ((protocol == PROTOCOL_STATIC_WEP) &&
				    (enable_11n == 1)) {
					wmprintf
					    ("ERR:WEP cannot be used when"
					" AP operates in 802.11n mode.\r\n");
					ret = UAP_FAILURE;
					goto done;
				}
				if ((protocol == PROTOCOL_WPA2_MIXED) &&
				    ((pwkcipher_wpa < 0) ||
				     (pwkcipher_wpa2 < 0))) {
					wmprintf
					    ("ERR:Both PwkCipherWPA and"
					" PwkCipherWPA2 should be defined"
					" for Mixed mode.\r\n");
					ret = UAP_FAILURE;
					goto done;
				}

				if (((pwkcipher_wpa >= 0) ||
				     (pwkcipher_wpa2 >= 0)) &&
				    (gwkcipher >= 0)) {
					if ((protocol == PROTOCOL_WPA) ||
					    (protocol == PROTOCOL_WPA2_MIXED)) {
						if (enable_11n != -1) {
						if (is_cipher_valid_with_11n(
							pwkcipher_wpa,
							gwkcipher,
							protocol, enable_11n)
							!= UAP_SUCCESS) {
							wmprintf
							    ("ERR:Wrong group"
						" cipher and WPA pairwise"
						" cipher combination!\r\n");
							ret =
							    UAP_FAILURE;
							goto done;
						}
						} else
						    if
						    (is_cipher_valid_with_proto
						     (pwkcipher_wpa, gwkcipher,
						      protocol) !=
						     UAP_SUCCESS) {
							wmprintf
							    ("ERR:Wrong group"
						" cipher and WPA pairwise"
						" cipher combination!\r\n");
							ret = UAP_FAILURE;
							goto done;
						}
					}
					if ((protocol == PROTOCOL_WPA2) ||
					    (protocol ==
						PROTOCOL_WPA2_MIXED)) {
						if (enable_11n != -1) {
						if (is_cipher_valid_with_11n
							(pwkcipher_wpa2,
							gwkcipher,
							protocol,
							enable_11n) !=
						UAP_SUCCESS) {
							wmprintf
							    ("ERR:Wrong group"
						" cipher and WPA2 pairwise"
						" cipher combination!\r\n");
							ret =
							    UAP_FAILURE;
							goto done;
						}
						} else
						    if
						    (is_cipher_valid_with_proto
						     (pwkcipher_wpa2, gwkcipher,
						      protocol) !=
						     UAP_SUCCESS) {
							wmprintf
							    ("ERR:Wrong group"
							" cipher and WPA2"
							" pairwise cipher"
							" combination!\r\n");
							ret = UAP_FAILURE;
							goto done;
						}
					}
				}
				if (protocol != -1) {
					tlvbuf_protocol *tlv = NULL;
					/* Append a new TLV */
					tlv_len = sizeof(tlvbuf_protocol);
					buffer =
					    os_mem_realloc(buffer,
							   cmd_len + tlv_len);
					if (!buffer) {
						wmprintf
						    ("ERR:Cannot append"
							" protocol TLV!\r\n");
						ret = UAP_FAILURE;
						goto done;
					}

					cmd_buf =
					    (apcmdbuf_sys_configure *) buffer;
					tlv =
					    (tlvbuf_protocol *) (buffer +
								 cmd_len);
					cmd_len += tlv_len;
					/* Set TLV fields */
					tlv->tag = MRVL_PROTOCOL_TLV_ID;
					tlv->length = 2;
					tlv->protocol = protocol;
					endian_convert_tlv_header_out(tlv);
					tlv->protocol =
					    uap_cpu_to_le16(tlv->protocol);

					if (protocol &
					    (PROTOCOL_WPA | PROTOCOL_WPA2)) {
						tlvbuf_akmp *tlv = NULL;
						/* Append a new TLV */
						tlv_len = sizeof(tlvbuf_akmp);
						buffer =
						    os_mem_realloc(buffer,
								   cmd_len +
								   tlv_len);
						if (!buffer) {
							wmprintf
							    ("ERR:Cannot"
						" append AKMP TLV!\r\n");
							ret = UAP_FAILURE;
							goto done;
						}
						cmd_buf =
						    (apcmdbuf_sys_configure *)
						    buffer;
						tlv =
						    (tlvbuf_akmp *) (buffer +
								     cmd_len);
						cmd_len += tlv_len;
						/* Set TLV fields */
						tlv->tag = MRVL_AKMP_TLV_ID;
						tlv->length = 4;
							/* sizeof(tlvbuf_akmp) -
								   TLVHEADER */
						tlv->key_mgmt = KEY_MGMT_PSK;
						endian_convert_tlv_header_out
						    (tlv);
						tlv->key_mgmt =
						    uap_cpu_to_le16(tlv->
								    key_mgmt);
						tlv->key_mgmt_operation =
						    uap_cpu_to_le16(0);
					}
				}
				if (pwkcipher_wpa >= 0) {
					tlvbuf_pwk_cipher *tlv = NULL;
					/* Append a new TLV */
					tlv_len = sizeof(tlvbuf_pwk_cipher);
					buffer =
					    os_mem_realloc(buffer,
							   cmd_len + tlv_len);
					if (!buffer) {
						wmprintf
						    ("ERR:Cannot append"
						" cipher TLV!\r\n");
						ret = UAP_FAILURE;
						goto done;
					}
					cmd_buf =
					    (apcmdbuf_sys_configure *) buffer;
					tlv =
					    (tlvbuf_pwk_cipher *) (buffer +
								   cmd_len);
					memset(tlv, 0, tlv_len);
					cmd_len += tlv_len;
					/* Set TLV fields */
					tlv->tag = MRVL_CIPHER_PWK_TLV_ID;
					tlv->length =
					    sizeof(tlvbuf_pwk_cipher) -
					    TLVHEADER_LEN;
					tlv->pairwise_cipher = pwkcipher_wpa;
					tlv->protocol = PROTOCOL_WPA;
					endian_convert_tlv_header_out(tlv);
					tlv->protocol =
					    uap_cpu_to_le16(tlv->protocol);
				}

				if (pwkcipher_wpa2 >= 0) {
					tlvbuf_pwk_cipher *tlv = NULL;
					/* Append a new TLV */
					tlv_len = sizeof(tlvbuf_pwk_cipher);
					buffer =
					    os_mem_realloc(buffer,
							   cmd_len + tlv_len);
					if (!buffer) {
						wmprintf
						    ("ERR:Cannot append"
						" cipher TLV!\r\n");
						ret = UAP_FAILURE;
						goto done;
					}
					cmd_buf =
					    (apcmdbuf_sys_configure *) buffer;
					tlv =
					    (tlvbuf_pwk_cipher *) (buffer +
								   cmd_len);
					memset(tlv, 0, tlv_len);
					cmd_len += tlv_len;
					/* Set TLV fields */
					tlv->tag = MRVL_CIPHER_PWK_TLV_ID;
					tlv->length =
					    sizeof(tlvbuf_pwk_cipher) -
					    TLVHEADER_LEN;
					tlv->pairwise_cipher = pwkcipher_wpa2;
					tlv->protocol = PROTOCOL_WPA2;
					endian_convert_tlv_header_out(tlv);
					tlv->protocol =
					    uap_cpu_to_le16(tlv->protocol);
				}

				if (gwkcipher >= 0) {
					tlvbuf_gwk_cipher *tlv = NULL;
					/* Append a new TLV */
					tlv_len = sizeof(tlvbuf_gwk_cipher);
					buffer =
					    os_mem_realloc(buffer,
							   cmd_len + tlv_len);
					if (!buffer) {
						wmprintf
						    ("ERR:Cannot append"
						" cipher TLV!\r\n");
						ret = UAP_FAILURE;
						goto done;
					}
					cmd_buf =
					    (apcmdbuf_sys_configure *) buffer;
					tlv =
					    (tlvbuf_gwk_cipher *) (buffer +
								   cmd_len);
					memset(tlv, 0, tlv_len);
					cmd_len += tlv_len;
					/* Set TLV fields */
					tlv->tag = MRVL_CIPHER_GWK_TLV_ID;
					tlv->length =
					    sizeof(tlvbuf_gwk_cipher) -
					    TLVHEADER_LEN;
					tlv->group_cipher = gwkcipher;
					endian_convert_tlv_header_out(tlv);
				}

				cmd_buf->size = cmd_len;
#ifdef CONFIG_P2P_DEBUG
				if (cmd_len != 0)
					hexdump((void *)cmd_buf, cmd_len, ' ');
#endif
				/* Send collective command */
				if (uap_ioctl
				    ((t_u8 *) cmd_buf, &cmd_len,
				     cmd_len) == UAP_SUCCESS) {
					if (cmd_buf->result != CMD_SUCCESS) {
						wmprintf
						    ("ERR: Failed to set the"
						" configuration!\r\n");
						ret = UAP_FAILURE;
						goto done;
					}
				} else {
					wmprintf
				    ("ERR: Command sending failed!\r\n");
					ret = UAP_FAILURE;
					goto done;
				}
				cmd_len = 0;
				if (buffer) {
					os_mem_free(buffer);
					buffer = NULL;
				}
				continue;
			}
		}

		/* Check for beginning of AP configurations */
		if (strcmp(args[0], "ap_config") == 0) {
			is_ap_config = 1;
			cmd_len = sizeof(apcmdbuf_sys_configure);
			if (buffer) {
				os_mem_free(buffer);
				buffer = NULL;
			}
			buffer = os_mem_alloc(cmd_len);
			if (!buffer) {
				wmprintf("ERR:Cannot allocate memory!\r\n");
				ret = UAP_FAILURE;
				goto done;
			}
			memset(buffer, 0, cmd_len);
			cmd_buf = (apcmdbuf_sys_configure *) buffer;
			cmd_buf->cmd_code = APCMD_SYS_CONFIGURE;
			cmd_buf->size = cmd_len;
			cmd_buf->seq_num = (0x01) << 13;
			cmd_buf->result = 0;
			cmd_buf->action = uap_cpu_to_le16(ACTION_SET);
			continue;
		}
#if 0
		/* Check for end of AP MAC address filter configurations */
		if (is_ap_mac_filter == 1) {
			if (strcmp(args[0], "}") == 0) {
				is_ap_mac_filter = 0;
				if (filter_tlv->count != filter_mac_count) {
					wmprintf
					    ("ERR:Number of MAC address "
					"provided does not match 'Count'\r\n");
					ret = UAP_FAILURE;
					goto done;
				}
				if (filter_tlv->count) {
					filter_tlv->length =
					    (filter_tlv->count * ETH_ALEN) + 2;
					cmd_len -=
					    (MAX_MAC_ONESHOT_FILTER -
					     filter_mac_count) * ETH_ALEN;
				} else {
					filter_tlv->length =
					    (MAX_MAC_ONESHOT_FILTER *
					     ETH_ALEN) + 2;
					memset(filter_tlv->mac_address, 0,
					       MAX_MAC_ONESHOT_FILTER *
					       ETH_ALEN);
				}
				cmd_buf->size = cmd_len;
				endian_convert_tlv_header_out(filter_tlv);
				if (uap_ioctl
				    ((t_u8 *) cmd_buf, &cmd_len,
				     cmd_len) == UAP_SUCCESS) {
					if (cmd_buf->result != CMD_SUCCESS) {
						wmprintf
						    ("ERR: Failed to set the"
						" configuration!\r\n");
						ret = UAP_FAILURE;
						goto done;
					}
				} else {
					wmprintf
				    ("ERR: Command sending failed!\r\n");
					ret = UAP_FAILURE;
					goto done;
				}

				cmd_len = 0;
				if (buffer) {
					/* os_mem_free(buffer);*/
					buffer = NULL;
				}
				continue;
			}
		}
#endif
		if (strcmp(args[0], "11d_enable") == 0) {
			if (IS_HEX_OR_DIGIT(args[1]) == UAP_FAILURE) {
				wmprintf
				    ("ERR: valid input for state are "
						"0 or 1\r\n");
				ret = UAP_FAILURE;
				goto done;
			}
			state_80211d = (t_u8) A2HEXDECIMAL(args[1]);

			if ((state_80211d != 0) && (state_80211d != 1)) {
				wmprintf
				    ("ERR: valid input for state "
				"are 0 or 1\r\n");
				ret = UAP_FAILURE;
				goto done;
			}
#if 0
			XXX compiled out, Might need later
			    if (sg_snmp_mib
				(ACTION_SET, OID_80211D_ENABLE,
				 sizeof(state_80211d), &state_80211d)
				== UAP_FAILURE) {
				ret = UAP_FAILURE;
				goto done;
			}
#endif
		}
#if 0
		if (strcmp(args[0], "country") == 0) {
			apcmdbuf_cfg_80211d *cmd_buf = NULL;
			ieeetypes_subband_set_t sub_bands[MAX_SUB_BANDS];
			t_u8 no_of_sub_band = 0;
			t_u16 buf_len;
			t_u16 cmdlen;
			t_u8 *buf = NULL;

			if ((strlen(args[1]) > 3) || (strlen(args[1]) == 0)) {
				wmprintf("In-correct country input\r\n");
				ret = UAP_FAILURE;
				goto done;
			}
			strncpy(country_80211d, args[1],
				sizeof(country_80211d) - 1);
			for (i = 0; (unsigned int)i < strlen(country_80211d);
			     i++) {
				if ((country_80211d[i] < 'A') ||
				    (country_80211d[i] > 'z')) {
					wmprintf("Invalid Country Code\r\n");
					ret = UAP_FAILURE;
					goto done;
				}
				if (country_80211d[i] > 'Z')
					country_80211d[i] =
					    country_80211d[i] - 'a' + 'A';
			}
			no_of_sub_band =
			    parse_domain_file(country_80211d, band, sub_bands);
			if (no_of_sub_band == UAP_FAILURE) {
				wmprintf("Parsing Failed\r\n");
				ret = UAP_FAILURE;
				goto done;
			}
			buf_len = sizeof(apcmdbuf_cfg_80211d);
			buf_len +=
			    no_of_sub_band * sizeof(ieeetypes_subband_set_t);
			buf = (t_u8 *) malloc(buf_len);
			if (!buf) {
				wmprintf
				    ("ERR:Cannot allocate buffer"
				" from command!\r\n");
				ret = UAP_FAILURE;
				goto done;
			}
			memset(buf, 0, buf_len);
			cmd_buf = (apcmdbuf_cfg_80211d *) buf;
			cmdlen = buf_len;
			cmd_buf->size = cmdlen - BUF_HEADER_SIZE;
			cmd_buf->result = 0;
			cmd_buf->seq_num = (0x01) << 13;
			cmd_buf->action = uap_cpu_to_le16(ACTION_SET);
			cmd_buf->cmd_code = HostCmd_CMD_802_11D_DOMAIN_INFO;
			cmd_buf->domain.tag = uap_cpu_to_le16(TLV_TYPE_DOMAIN);
			cmd_buf->domain.length =
			    uap_cpu_to_le16(sizeof(domain_param_t)
					    - BUF_HEADER_SIZE +
					    (no_of_sub_band *
					     sizeof(ieeetypes_subband_set_t)));

			memset(cmd_buf->domain.country_code, ' ',
			       sizeof(cmd_buf->domain.country_code));
			memcpy(cmd_buf->domain.country_code, country_80211d,
			       strlen(country_80211d));
			memcpy(cmd_buf->domain.subband, sub_bands,
			       no_of_sub_band *
			       sizeof(ieeetypes_subband_set_t));

			/* Send the command */
			if (uap_ioctl((t_u8 *) cmd_buf, &cmdlen, cmdlen) ==
			    UAP_SUCCESS) {
				if (cmd_buf->result != CMD_SUCCESS) {
					wmprintf
					    ("ERR: Failed to set the"
					" configuration!\r\n");
					ret = UAP_FAILURE;
					goto done;
				}
			} else {
				wmprintf("ERR: Command sending failed!\r\n");
				ret = UAP_FAILURE;
				goto done;
			}

			if (buf)
				os_mem_free(buf);
		}
	/* Check for beginning of AP MAC address filter configurations */
		if (strcmp(args[0], "ap_mac_filter") == 0) {
			is_ap_mac_filter = 1;
			cmd_len =
			    sizeof(apcmdbuf_sys_configure) +
			    sizeof(tlvbuf_sta_mac_addr_filter) +
			    (MAX_MAC_ONESHOT_FILTER * ETH_ALEN);
			if (buffer) {
				/* os_mem_free(buffer);*/
				buffer = NULL;
			}
			buffer = (t_u8 *) malloc(cmd_len);
			if (!buffer) {
				wmprintf("ERR:Cannot allocate memory!\r\n");
				ret = UAP_FAILURE;
				goto done;
			}
			cmd_buf = (apcmdbuf_sys_configure *) buffer;
			cmd_buf->cmd_code = APCMD_SYS_CONFIGURE;
			cmd_buf->size = cmd_len;
			cmd_buf->seq_num = (0x01) << 13;
			cmd_buf->result = 0;
			cmd_buf->action = uap_cpu_to_le16(ACTION_SET);
			filter_tlv =
			    (tlvbuf_sta_mac_addr_filter *) (buffer +
							    sizeof
						    (apcmdbuf_sys_configure));
			filter_tlv->tag = MRVL_STA_MAC_ADDR_FILTER_TLV_ID;
			filter_tlv->length = 2;
			filter_tlv->count = 0;
			filter_mac_count = 0;
			continue;
		}
		if ((strcmp(args[0], "FilterMode") == 0) && is_ap_mac_filter) {
			if ((ISDIGIT(args[1]) == 0) || (atoi(args[1]) < 0) ||
			    (atoi(args[1]) > 2)) {
				wmprintf
				    ("ERR:Illegal FilterMode paramter %d. Must"
				" be either '0', '1', or '2'.\r\n",
				     atoi(args[1]));
				ret = UAP_FAILURE;
				goto done;
			}
			filter_tlv->filter_mode = atoi(args[1]);
			continue;
		}
		if ((strcmp(args[0], "Count") == 0) && is_ap_mac_filter) {
			filter_tlv->count = atoi(args[1]);
			if ((ISDIGIT(args[1]) == 0) ||
			    (filter_tlv->count > MAX_MAC_ONESHOT_FILTER)) {
				wmprintf("ERR: Illegal Count parameter.\r\n");
				ret = UAP_FAILURE;
				goto done;
			}
		}
		if ((strncmp(args[0], "mac_", 4) == 0) && is_ap_mac_filter) {
			if (filter_mac_count < MAX_MAC_ONESHOT_FILTER) {
				if (mac2raw
				    (args[1],
				     &filter_tlv->mac_address[filter_mac_count *
							      ETH_ALEN]) !=
				    UAP_SUCCESS) {
					wmprintf
					    ("ERR: Invalid MAC address %s\r\n",
					     args[1]);
					ret = UAP_FAILURE;
					goto done;
				}
				filter_mac_count++;
			} else {
				wmprintf
				    ("ERR: Filter table can not have more than"
					" %d MAC addresses\r\n",
				     MAX_MAC_ONESHOT_FILTER);
				ret = UAP_FAILURE;
				goto done;
			}
		}
#endif
		if (strcmp(args[0], "SSID") == 0) {
			if (arg_num == 1) {
				wmprintf("ERR:SSID field is blank!\r\n");
				ret = UAP_FAILURE;
				goto done;
			} else {
				tlvbuf_ssid *tlv = NULL;
				if (args[1][0] == '"')
					args[1]++;
				if (args[1][strlen(args[1]) - 1] == '"')
					args[1][strlen(args[1]) - 1] = '\0';
				if ((strlen(args[1]) > MAX_SSID_LENGTH) ||
				    (strlen(args[1]) == 0)) {
					wmprintf
					    ("ERR:SSID length out of range"
					" (%d to %d).\r\n",
					     MIN_SSID_LENGTH, MAX_SSID_LENGTH);
					ret = UAP_FAILURE;
					goto done;
				}
				/* Append a new TLV */
				tlv_len = sizeof(tlvbuf_ssid) + strlen(args[1]);
				buffer =
				    os_mem_realloc(buffer, cmd_len + tlv_len);
/*				buffer = os_mem_alloc( cmd_len + tlv_len);*/
				if (!buffer) {
					wmprintf
					    ("ERR:Cannot os_mem_realloc"
					" SSID TLV!\r\n");
					ret = UAP_FAILURE;
					goto done;
				}
				cmd_buf = (apcmdbuf_sys_configure *) buffer;
				tlv = (tlvbuf_ssid *) (buffer + cmd_len);
				cmd_len += tlv_len;
				/* Set TLV fields */
				tlv->tag = MRVL_SSID_TLV_ID;
				tlv->length = strlen(args[1]);
				memcpy(tlv->ssid, args[1], tlv->length);
				endian_convert_tlv_header_out(tlv);
			}
		}
		if (strcmp(args[0], "BeaconPeriod") == 0) {
			if (is_input_valid(BEACONPERIOD, arg_num - 1, args + 1)
			    != UAP_SUCCESS) {
				ret = UAP_FAILURE;
				goto done;
			}
			tlvbuf_beacon_period *tlv = NULL;
			/* Append a new TLV */
			tlv_len = sizeof(tlvbuf_beacon_period);
			buffer = os_mem_realloc(buffer, cmd_len + tlv_len);
			if (!buffer) {
				wmprintf
				    ("ERR:Cannot os_mem_realloc beacon"
				" period TLV!\r\n");
				ret = UAP_FAILURE;
				goto done;
			}
			cmd_buf = (apcmdbuf_sys_configure *) buffer;
			tlv = (tlvbuf_beacon_period *) (buffer + cmd_len);
			cmd_len += tlv_len;
			/* Set TLV fields */
			tlv->tag = MRVL_BEACON_PERIOD_TLV_ID;
			tlv->length = 2;
			tlv->beacon_period_ms = (t_u16) atoi(args[1]);
			endian_convert_tlv_header_out(tlv);
			tlv->beacon_period_ms =
			    uap_cpu_to_le16(tlv->beacon_period_ms);
		}
		if (strcmp(args[0], "ChanList") == 0) {
			if (is_input_valid(SCANCHANNELS, arg_num - 1, args + 1)
			    != UAP_SUCCESS) {
				ret = UAP_FAILURE;
				goto done;
			}

			tlvbuf_channel_list *tlv = NULL;
			channel_list *pchan_list = NULL;
			/* Append a new TLV */
			tlv_len =
			    sizeof(tlvbuf_channel_list) +
			    ((arg_num - 1) * sizeof(channel_list));
			buffer = os_mem_realloc(buffer, cmd_len + tlv_len);
			if (!buffer) {
				wmprintf
				    ("ERR:Cannot append channel list TLV!\r\n");
				ret = UAP_FAILURE;
				goto done;
			}
			cmd_buf = (apcmdbuf_sys_configure *) buffer;
			tlv = (tlvbuf_channel_list *) (buffer + cmd_len);
			cmd_len += tlv_len;
			/* Set TLV fields */
			tlv->tag = MRVL_CHANNELLIST_TLV_ID;
			tlv->length = sizeof(channel_list) * (arg_num - 1);
			pchan_list = (channel_list *) tlv->chan_list;
			for (i = 0; i < (arg_num - 1); i++) {
				band_flag = -1;
				sscanf(args[i + 1], "%d.%d", &chan_number,
				       &band_flag);
				pchan_list->chan_number = chan_number;
				pchan_list->band_config_type &=
				    (~BAND_CONFIG_MASK);
				if (((band_flag != -1) && (band_flag)) ||
				    (chan_number > MAX_CHANNELS_BG)) {
					pchan_list->band_config_type |=
					    BAND_CONFIG_5GHZ;
				}
				pchan_list++;
			}
			endian_convert_tlv_header_out(tlv);
		}
		if (strcmp(args[0], "Channel") == 0) {
			if (is_input_valid(CHANNEL, arg_num - 1, args + 1) !=
			    UAP_SUCCESS) {
				ret = UAP_FAILURE;
				goto done;
			}
			tlvbuf_channel_config *tlv = NULL;
			/* Append a new TLV */
			tlv_len = sizeof(tlvbuf_channel_config);
			buffer = os_mem_realloc(buffer, cmd_len + tlv_len);
			if (!buffer) {
				wmprintf("ERR:Cannot append channel TLV!\r\n");
				ret = UAP_FAILURE;
				goto done;
			}
			cmd_buf = (apcmdbuf_sys_configure *) buffer;
			tlv = (tlvbuf_channel_config *) (buffer + cmd_len);
			/*channel_band_tlv = tlv;*/
			cmd_len += tlv_len;
			/* Set TLV fields */
			tlv->tag = MRVL_CHANNELCONFIG_TLV_ID;
			tlv->length = 2;
			tlv->chan_number = (t_u8) atoi(args[1]);
#if 0
			if (tlv->chan_number > MAX_CHANNELS_BG)
				band = BAND_A;
			else
				band = BAND_B | BAND_G;
#endif
			if ((arg_num - 1) == 2) {
				chan_mode = atoi(args[2]);
				tlv->band_config_type = 0;
				if (chan_mode & BITMAP_ACS_MODE)
					tlv->band_config_type |=
					    BAND_CONFIG_ACS_MODE;
				if (chan_mode & BITMAP_CHANNEL_ABOVE)
					tlv->band_config_type |=
					    SECOND_CHANNEL_ABOVE;
				if (chan_mode & BITMAP_CHANNEL_BELOW)
					tlv->band_config_type |=
					    SECOND_CHANNEL_BELOW;
			} else
				tlv->band_config_type = 0;
			if (tlv->chan_number > MAX_CHANNELS_BG)
				tlv->band_config_type |= BAND_CONFIG_5GHZ;
			endian_convert_tlv_header_out(tlv);
		}
#if 0
		if (strcmp(args[0], "Band") == 0) {
			if (is_input_valid(BAND, arg_num - 1, args + 1) !=
			    UAP_SUCCESS) {
				ret = UAP_FAILURE;
				goto done;
			}
			if (channel_band_tlv == NULL) {
				wmprintf
				    ("ERR: Channel parameter should be"
				" specified before Band\r\n");
				ret = UAP_FAILURE;
				goto done;
			}
			/* If band is provided, clear previous value of band */
			channel_band_tlv->band_config_type &=
			    (~BAND_CONFIG_MASK);
			if (atoi(args[1]) == 0) {
				band = BAND_B | BAND_G;
			} else {
				channel_band_tlv->band_config_type |=
				    BAND_CONFIG_5GHZ;
				band = BAND_A;
			}
		}
#endif
		if (strcmp(args[0], "AP_MAC") == 0) {
			int ret;
			tlvbuf_ap_mac_address *tlv = NULL;
			/* Append a new TLV */
			tlv_len = sizeof(tlvbuf_ap_mac_address);
			buffer = os_mem_realloc(buffer, cmd_len + tlv_len);
			if (!buffer) {
				wmprintf("ERR:Cannot append ap_mac TLV!\r\n");
				ret = UAP_FAILURE;
				goto done;
			}
			cmd_buf = (apcmdbuf_sys_configure *) buffer;
			tlv = (tlvbuf_ap_mac_address *) (buffer + cmd_len);
			cmd_len += tlv_len;
			cmd_buf->action = ACTION_SET;
			tlv->tag = MRVL_AP_MAC_ADDRESS_TLV_ID;
			tlv->length = ETH_ALEN;
			ret = mac2raw(args[1], tlv->ap_mac_addr);
			if (ret != UAP_SUCCESS) {
				wmprintf("ERR: %s Address\r\n",
					 ret ==
					 UAP_FAILURE ? "Invalid MAC" : ret ==
					 UAP_RET_MAC_BROADCAST ? "Broadcast" :
					 "Multicast");
				ret = UAP_FAILURE;
				goto done;
			}
			endian_convert_tlv_header_out(tlv);
		}

		if (strcmp(args[0], "RxAntenna") == 0) {
			if ((ISDIGIT(args[1]) != UAP_SUCCESS) ||
			    (atoi(args[1]) < 0) || (atoi(args[1]) > 1)) {
				wmprintf("ERR: Invalid Antenna value\r\n");
				ret = UAP_FAILURE;
				goto done;
			}
			tlvbuf_antenna_ctl *tlv = NULL;
			/* Append a new TLV */
			tlv_len = sizeof(tlvbuf_antenna_ctl);
			buffer = os_mem_realloc(buffer, cmd_len + tlv_len);
			if (!buffer) {
				wmprintf("ERR:Cannot append channel TLV!\r\n");
				ret = UAP_FAILURE;
				goto done;
			}
			cmd_buf = (apcmdbuf_sys_configure *) buffer;
			tlv = (tlvbuf_antenna_ctl *) (buffer + cmd_len);
			cmd_len += tlv_len;
			cmd_buf->action = ACTION_SET;
			tlv->tag = MRVL_ANTENNA_CTL_TLV_ID;
			tlv->length = 2;
			tlv->which_antenna = 0;
			tlv->antenna_mode = atoi(args[1]);
			endian_convert_tlv_header_out(tlv);
		}

		if (strcmp(args[0], "TxAntenna") == 0) {
			if ((ISDIGIT(args[1]) != UAP_SUCCESS) ||
			    (atoi(args[1]) < 0) || (atoi(args[1]) > 1)) {
				wmprintf("ERR: Invalid Antenna value\r\n");
				ret = UAP_FAILURE;
				goto done;
			}
			tlvbuf_antenna_ctl *tlv = NULL;
			/* Append a new TLV */
			tlv_len = sizeof(tlvbuf_antenna_ctl);
			buffer = os_mem_realloc(buffer, cmd_len + tlv_len);
			if (!buffer) {
				wmprintf("ERR:Cannot append channel TLV!\r\n");
				ret = UAP_FAILURE;
				goto done;
			}
			cmd_buf = (apcmdbuf_sys_configure *) buffer;
			tlv = (tlvbuf_antenna_ctl *) (buffer + cmd_len);
			cmd_len += tlv_len;
			cmd_buf->action = ACTION_SET;
			tlv->tag = MRVL_ANTENNA_CTL_TLV_ID;
			tlv->length = 2;
			tlv->which_antenna = 1;
			tlv->antenna_mode = atoi(args[1]);
			endian_convert_tlv_header_out(tlv);
		}
		if (strcmp(args[0], "Rate") == 0) {
			if (is_input_valid(RATE, arg_num - 1, args + 1) !=
			    UAP_SUCCESS) {
				wmprintf("ERR: Invalid Rate input\r\n");
				ret = UAP_FAILURE;
				goto done;
			}
			tlvbuf_rates *tlv = NULL;
			/* Append a new TLV */
			tlv_len = sizeof(tlvbuf_rates) + arg_num - 1;
			buffer = os_mem_realloc(buffer, cmd_len + tlv_len);
			if (!buffer) {
				wmprintf("ERR:Cannot append rates TLV!\r\n");
				ret = UAP_FAILURE;
				goto done;
			}
			cmd_buf = (apcmdbuf_sys_configure *) buffer;
			tlv = (tlvbuf_rates *) (buffer + cmd_len);
			cmd_len += tlv_len;
			/* Set TLV fields */
			tlv->tag = MRVL_RATES_TLV_ID;
			tlv->length = arg_num - 1;
			for (i = 0; i < tlv->length; i++) {
				rate[i] = tlv->operational_rates[i] =
				    (t_u8) A2HEXDECIMAL(args[i + 1]);
			}
			endian_convert_tlv_header_out(tlv);
		}
		if (strcmp(args[0], "TxPowerLevel") == 0) {
			if (is_input_valid(TXPOWER, arg_num - 1, args + 1) !=
			    UAP_SUCCESS) {
				wmprintf("ERR:Invalid TxPowerLevel\r\n");
				ret = UAP_FAILURE;
				goto done;
			} else {
				tlvbuf_tx_power *tlv = NULL;
				/* Append a new TLV */
				tlv_len = sizeof(tlvbuf_tx_power);
				buffer =
				    os_mem_realloc(buffer, cmd_len + tlv_len);
				if (!buffer) {
					wmprintf
					    ("ERR:Cannot append tx power"
					" level TLV!\r\n");
					ret = UAP_FAILURE;
					goto done;
				}
				cmd_buf = (apcmdbuf_sys_configure *) buffer;
				tlv = (tlvbuf_tx_power *) (buffer + cmd_len);
				cmd_len += tlv_len;
				/* Set TLV fields */
				tlv->tag = MRVL_TX_POWER_TLV_ID;
				tlv->length = 1;
				tlv->tx_power_dbm = (t_u8) atoi(args[1]);
				endian_convert_tlv_header_out(tlv);
			}
		}
		if (strcmp(args[0], "BroadcastSSID") == 0) {
			if (is_input_valid(BROADCASTSSID, arg_num - 1, args + 1)
			    != UAP_SUCCESS) {
				ret = UAP_FAILURE;
				goto done;
			}
			tlvbuf_bcast_ssid_ctl *tlv = NULL;
			/* Append a new TLV */
			tlv_len = sizeof(tlvbuf_bcast_ssid_ctl);
			buffer = os_mem_realloc(buffer, cmd_len + tlv_len);
			if (!buffer) {
				wmprintf
				    ("ERR:Cannot append SSID broadcast"
				" control TLV!\r\n");
				ret = UAP_FAILURE;
				goto done;
			}
			cmd_buf = (apcmdbuf_sys_configure *) buffer;
			tlv = (tlvbuf_bcast_ssid_ctl *) (buffer + cmd_len);
			cmd_len += tlv_len;
			/* Set TLV fields */
			tlv->tag = MRVL_BCAST_SSID_CTL_TLV_ID;
			tlv->length = 1;
			tlv->bcast_ssid_ctl = (t_u8) atoi(args[1]);
			endian_convert_tlv_header_out(tlv);
		}
		if (strcmp(args[0], "RTSThreshold") == 0) {
			if (is_input_valid(RTSTHRESH, arg_num - 1, args + 1) !=
			    UAP_SUCCESS) {
				ret = UAP_FAILURE;
				goto done;
			}
			tlvbuf_rts_threshold *tlv = NULL;
			/* Append a new TLV */
			tlv_len = sizeof(tlvbuf_rts_threshold);
			buffer = os_mem_realloc(buffer, cmd_len + tlv_len);
			if (!buffer) {
				wmprintf
				    ("ERR:Cannot append RTS "
					"threshold TLV!\r\n");
				ret = UAP_FAILURE;
				goto done;
			}
			cmd_buf = (apcmdbuf_sys_configure *) buffer;
			tlv = (tlvbuf_rts_threshold *) (buffer + cmd_len);
			cmd_len += tlv_len;
			/* Set TLV fields */
			tlv->tag = MRVL_RTS_THRESHOLD_TLV_ID;
			tlv->length = 2;
			tlv->rts_threshold = (t_u16) atoi(args[1]);
			endian_convert_tlv_header_out(tlv);
			tlv->rts_threshold =
			    uap_cpu_to_le16(tlv->rts_threshold);
		}
		if (strcmp(args[0], "FragThreshold") == 0) {
			if (is_input_valid(FRAGTHRESH, arg_num - 1, args + 1) !=
			    UAP_SUCCESS) {
				ret = UAP_FAILURE;
				goto done;
			}
			tlvbuf_frag_threshold *tlv = NULL;
			/* Append a new TLV */
			tlv_len = sizeof(tlvbuf_frag_threshold);
			buffer = os_mem_realloc(buffer, cmd_len + tlv_len);
			if (!buffer) {
				wmprintf
				    ("ERR:Cannot append Fragmentation"
				" threshold TLV!\r\n");
				ret = UAP_FAILURE;
				goto done;
			}
			cmd_buf = (apcmdbuf_sys_configure *) buffer;
			tlv = (tlvbuf_frag_threshold *) (buffer + cmd_len);
			cmd_len += tlv_len;
			/* Set TLV fields */
			tlv->tag = MRVL_FRAG_THRESHOLD_TLV_ID;
			tlv->length = 2;
			tlv->frag_threshold = (t_u16) atoi(args[1]);
			endian_convert_tlv_header_out(tlv);
			tlv->frag_threshold =
			    uap_cpu_to_le16(tlv->frag_threshold);
		}
		if (strcmp(args[0], "DTIMPeriod") == 0) {
			if (is_input_valid(DTIMPERIOD, arg_num - 1, args + 1) !=
			    UAP_SUCCESS) {
				ret = UAP_FAILURE;
				goto done;
			}
			tlvbuf_dtim_period *tlv = NULL;
			/* Append a new TLV */
			tlv_len = sizeof(tlvbuf_dtim_period);
			buffer = os_mem_realloc(buffer, cmd_len + tlv_len);
			if (!buffer) {
				wmprintf
				    ("ERR:Cannot append DTIM period TLV!\r\n");
				ret = UAP_FAILURE;
				goto done;
			}
			cmd_buf = (apcmdbuf_sys_configure *) buffer;
			tlv = (tlvbuf_dtim_period *) (buffer + cmd_len);
			cmd_len += tlv_len;
			/* Set TLV fields */
			tlv->tag = MRVL_DTIM_PERIOD_TLV_ID;
			tlv->length = 1;
			tlv->dtim_period = (t_u8) atoi(args[1]);
			endian_convert_tlv_header_out(tlv);
		}
		if (strcmp(args[0], "RSNReplayProtection") == 0) {
			if (is_input_valid(RSNREPLAYPROT, arg_num - 1, args + 1)
			    != UAP_SUCCESS) {
				ret = UAP_FAILURE;
				goto done;
			}
			tlvbuf_rsn_replay_prot *tlv = NULL;
			/* Append a new TLV */
			tlv_len = sizeof(tlvbuf_rsn_replay_prot);
			buffer = os_mem_realloc(buffer, cmd_len + tlv_len);
			if (!buffer) {
				wmprintf
				    ("ERR:Cannot append RSN replay "
				"protection TLV!\r\n");
				ret = UAP_FAILURE;
				goto done;
			}
			cmd_buf = (apcmdbuf_sys_configure *) buffer;
			tlv = (tlvbuf_rsn_replay_prot *) (buffer + cmd_len);
			cmd_len += tlv_len;
			/* Set TLV fields */
			tlv->tag = MRVL_RSN_REPLAY_PROT_TLV_ID;
			tlv->length = 1;
			tlv->rsn_replay_prot = (t_u8) atoi(args[1]);
			endian_convert_tlv_header_out(tlv);
		}
		if (strcmp(args[0], "MCBCdataRate") == 0) {
			if (is_input_valid(MCBCDATARATE, arg_num - 1, args + 1)
			    != UAP_SUCCESS) {
				ret = UAP_FAILURE;
				goto done;
			}
			mcbc_data_rate = (t_u16) A2HEXDECIMAL(args[1]);
		}
		if (strcmp(args[0], "PktFwdCtl") == 0) {
			if (is_input_valid(PKTFWD, arg_num - 1, args + 1) !=
			    UAP_SUCCESS) {
				ret = UAP_FAILURE;
				goto done;
			}
			tlvbuf_pkt_fwd_ctl *tlv = NULL;
			/* Append a new TLV */
			tlv_len = sizeof(tlvbuf_pkt_fwd_ctl);
			buffer = os_mem_realloc(buffer, cmd_len + tlv_len);
			if (!buffer) {
				wmprintf
				    ("ERR:Cannot append packet forwarding"
					" control TLV!\r\n");
				ret = UAP_FAILURE;
				goto done;
			}
			cmd_buf = (apcmdbuf_sys_configure *) buffer;
			tlv = (tlvbuf_pkt_fwd_ctl *) (buffer + cmd_len);
			cmd_len += tlv_len;
			/* Set TLV fields */
			tlv->tag = MRVL_PKT_FWD_CTL_TLV_ID;
			tlv->length = 1;
			tlv->pkt_fwd_ctl = (t_u8) atoi(args[1]);
			endian_convert_tlv_header_out(tlv);
		}
		if (strcmp(args[0], "StaAgeoutTimer") == 0) {
			if (is_input_valid
			    (STAAGEOUTTIMER, arg_num - 1,
			     args + 1) != UAP_SUCCESS) {
				ret = UAP_FAILURE;
				goto done;
			}
			tlvbuf_sta_ageout_timer *tlv = NULL;
			/* Append a new TLV */
			tlv_len = sizeof(tlvbuf_sta_ageout_timer);
			buffer = os_mem_realloc(buffer, cmd_len + tlv_len);
			if (!buffer) {
				wmprintf
				    ("ERR:Cannot append STA ageout"
				" timer TLV!\r\n");
				ret = UAP_FAILURE;
				goto done;
			}
			cmd_buf = (apcmdbuf_sys_configure *) buffer;
			tlv = (tlvbuf_sta_ageout_timer *) (buffer + cmd_len);
			cmd_len += tlv_len;
			/* Set TLV fields */
			tlv->tag = MRVL_STA_AGEOUT_TIMER_TLV_ID;
			tlv->length = 4;
			tlv->sta_ageout_timer_ms = (t_u32) atoi(args[1]);
			endian_convert_tlv_header_out(tlv);
			tlv->sta_ageout_timer_ms =
			    uap_cpu_to_le32(tlv->sta_ageout_timer_ms);
		}
		if (strcmp(args[0], "PSStaAgeoutTimer") == 0) {
			if (is_input_valid
			    (PSSTAAGEOUTTIMER, arg_num - 1,
			     args + 1) != UAP_SUCCESS) {
				ret = UAP_FAILURE;
				goto done;
			}
			tlvbuf_ps_sta_ageout_timer *tlv = NULL;
			/* Append a new TLV */
			tlv_len = sizeof(tlvbuf_ps_sta_ageout_timer);
			buffer = os_mem_realloc(buffer, cmd_len + tlv_len);
			if (!buffer) {
				wmprintf
				    ("ERR:Cannot append PS STA ageout timer"
					" TLV!\r\n");
				ret = UAP_FAILURE;
				goto done;
			}
			cmd_buf = (apcmdbuf_sys_configure *) buffer;
			tlv = (tlvbuf_ps_sta_ageout_timer *) (buffer + cmd_len);
			cmd_len += tlv_len;
			/* Set TLV fields */
			tlv->tag = MRVL_PS_STA_AGEOUT_TIMER_TLV_ID;
			tlv->length = 4;
			tlv->ps_sta_ageout_timer_ms = (t_u32) atoi(args[1]);
			endian_convert_tlv_header_out(tlv);
			tlv->ps_sta_ageout_timer_ms =
			    uap_cpu_to_le32(tlv->ps_sta_ageout_timer_ms);
		}
		if (strcmp(args[0], "AuthMode") == 0) {
			if (is_input_valid(AUTHMODE, arg_num - 1, args + 1) !=
			    UAP_SUCCESS) {
				ret = UAP_FAILURE;
				goto done;
			}
			tlvbuf_auth_mode *tlv = NULL;
			/* Append a new TLV */
			tlv_len = sizeof(tlvbuf_auth_mode);
			buffer = os_mem_realloc(buffer, cmd_len + tlv_len);
			if (!buffer) {
				wmprintf("ERR:Cannot append auth "
				"mode TLV!\r\n");
				ret = UAP_FAILURE;
				goto done;
			}
			cmd_buf = (apcmdbuf_sys_configure *) buffer;
			tlv = (tlvbuf_auth_mode *) (buffer + cmd_len);
			cmd_len += tlv_len;
			/* Set TLV fields */
			tlv->tag = MRVL_AUTH_TLV_ID;
			tlv->length = 1;
			tlv->auth_mode = (t_u8) atoi(args[1]);
			endian_convert_tlv_header_out(tlv);
		}
		if (strcmp(args[0], "KeyIndex") == 0) {
			if (arg_num == 1) {
				wmprintf("KeyIndex is blank!\r\n");
				ret = UAP_FAILURE;
				goto done;
			} else {
				if (ISDIGIT(args[1]) == 0) {
					wmprintf
					    ("ERR:Illegal KeyIndex parameter."
				" Must be either '0', '1', '2', or '3'.\r\n");
					ret = UAP_FAILURE;
					goto done;
				}
				keyindex = atoi(args[1]);
				if ((keyindex < 0) || (keyindex > 3)) {
					wmprintf
					    ("ERR:Illegal KeyIndex parameter."
				" Must be either '0', '1', '2', or '3'.\r\n");
					ret = UAP_FAILURE;
					goto done;
				}
			}
		}
		if (strncmp(args[0], "Key_", 4) == 0) {
			if (arg_num == 1) {
				wmprintf("ERR:%s is blank!\r\n", args[0]);
				ret = UAP_FAILURE;
				goto done;
			} else {
				tlvbuf_wep_key *tlv = NULL;
				int key_len = 0;
				if (args[1][0] == '"') {
					if ((strlen(args[1]) != 2) &&
					    (strlen(args[1]) != 7) &&
					    (strlen(args[1]) != 15)) {
						wmprintf
					    ("ERR:Wrong key length!\r\n");
						ret = UAP_FAILURE;
						goto done;
					}
					key_len = strlen(args[1]) - 2;
				} else {
					if ((strlen(args[1]) != 0) &&
					    (strlen(args[1]) != 10) &&
					    (strlen(args[1]) != 26)) {
						wmprintf
					    ("ERR:Wrong key length!\r\n");
						ret = UAP_FAILURE;
						goto done;
					}
					if (UAP_FAILURE ==
						 ishexstring(args[1])) {
						wmprintf
						    ("ERR:Only hex digits are"
						" allowed when key length is"
						" 10 or 26\r\n");
						ret = UAP_FAILURE;
						goto done;
					}
					key_len = strlen(args[1]) / 2;
				}

				/* Append a new TLV */
				tlv_len = sizeof(tlvbuf_wep_key) + key_len;
				buffer =
				    os_mem_realloc(buffer, cmd_len + tlv_len);
				if (!buffer) {
					wmprintf
					    ("ERR:Cannot append WEP key"
					" configurations TLV!\r\n");
					ret = UAP_FAILURE;
					goto done;
				}
				cmd_buf = (apcmdbuf_sys_configure *) buffer;
				tlv = (tlvbuf_wep_key *) (buffer + cmd_len);
				cmd_len += tlv_len;
				/* Set TLV fields */
				tlv->tag = MRVL_WEP_KEY_TLV_ID;
				tlv->length = key_len + 2;
				if (strcmp(args[0], "Key_0") == 0)
					tlv->key_index = 0;
				else if (strcmp(args[0], "Key_1") == 0)
					tlv->key_index = 1;
				else if (strcmp(args[0], "Key_2") == 0)
					tlv->key_index = 2;
				else if (strcmp(args[0], "Key_3") == 0)
					tlv->key_index = 3;
				if (keyindex == tlv->key_index)
					tlv->is_default = 1;
				else
					tlv->is_default = 0;

				if (args[1][0] == '"')
					memcpy(tlv->key, &args[1][1],
					       strlen(args[1]) - 2);
				else
					string2raw(args[1], tlv->key);
				endian_convert_tlv_header_out(tlv);
			}
		}
		if (strcmp(args[0], "PSK") == 0) {
			if (arg_num == 1) {
				wmprintf("ERR:PSK is blank!\r\n");
				ret = UAP_FAILURE;
				goto done;
			} else {
				tlvbuf_wpa_passphrase *tlv = NULL;
				if (args[1][0] == '"')
					args[1]++;
				if (args[1][strlen(args[1]) - 1] == '"')
					args[1][strlen(args[1]) - 1] = '\0';
				tlv_len =
				    sizeof(tlvbuf_wpa_passphrase) +
				    strlen(args[1]);
				if (strlen(args[1]) >
					MAX_WPA_PASSPHRASE_LENGTH) {
					wmprintf("ERR:PSK too long.\r\n");
					ret = UAP_FAILURE;
					goto done;
				}
				if (strlen(args[1]) <
					MIN_WPA_PASSPHRASE_LENGTH) {
					wmprintf("ERR:PSK too short.\r\n");
					ret = UAP_FAILURE;
					goto done;
				}
				if (strlen(args[1]) ==
				    MAX_WPA_PASSPHRASE_LENGTH) {
					if (UAP_FAILURE ==
						ishexstring(args[1])) {
						wmprintf
						    ("ERR:Only hex digits are"
						" allowed when passphrase's "
						"length is 64\r\n");
						ret = UAP_FAILURE;
						goto done;
					}
				}
				/* Append a new TLV */
				buffer =
				    os_mem_realloc(buffer, cmd_len + tlv_len);
				if (!buffer) {
					wmprintf
					    ("ERR:Cannot append WPA passphrase"
						" TLV!\r\n");
					ret = UAP_FAILURE;
					goto done;
				}
				cmd_buf = (apcmdbuf_sys_configure *) buffer;
				tlv =
				    (tlvbuf_wpa_passphrase *) (buffer +
							       cmd_len);
				cmd_len += tlv_len;
				/* Set TLV fields */
				tlv->tag = MRVL_WPA_PASSPHRASE_TLV_ID;
				tlv->length = strlen(args[1]);
				memcpy(tlv->passphrase, args[1], tlv->length);
				endian_convert_tlv_header_out(tlv);
			}
		}
		if (strcmp(args[0], "Protocol") == 0) {
			if (is_input_valid(PROTOCOL, arg_num - 1, args + 1) !=
			    UAP_SUCCESS) {
				ret = UAP_FAILURE;
				goto done;
			}
			protocol = (t_u16) atoi(args[1]);
		}
		if ((strcmp(args[0], "PairwiseCipher") == 0) ||
		    (strcmp(args[0], "GroupCipher") == 0)) {
			wmprintf
			    ("ERR:PairwiseCipher and GroupCipher are not"
			" supported.\r\nPlease configure pairwise cipher "
			"using parameters PwkCipherWPA or PwkCipherWPA2\r\n"
			"    and group cipher using GwkCipher in the"
			" config file.\r\n");
			ret = UAP_FAILURE;
			goto done;
		}

		if ((protocol == PROTOCOL_NO_SECURITY) ||
		    (protocol == PROTOCOL_STATIC_WEP)) {
			if ((strcmp(args[0], "PwkCipherWPA") == 0) ||
			    (strcmp(args[0], "PwkCipherWPA2") == 0)
			    || (strcmp(args[0], "GwkCipher") == 0)) {
				wmprintf
				    ("ERR:Pairwise cipher and group cipher"
				" should not be defined for "
				"Open and WEP mode.\r\n");
				ret = UAP_FAILURE;
				goto done;
			}
		}

		if (strcmp(args[0], "PwkCipherWPA") == 0) {
			if (arg_num == 1) {
				wmprintf("ERR:PwkCipherWPA is blank!\r\n");
				ret = UAP_FAILURE;
				goto done;
			} else {
				if (ISDIGIT(args[1]) == 0) {
					wmprintf
					    ("ERR:Illegal PwkCipherWPA"
					" parameter. Must be either"
					" bit '2' or '3'.\r\n");
					ret = UAP_FAILURE;
					goto done;
				}
				if (atoi(args[1]) & ~CIPHER_BITMAP) {
					wmprintf
					    ("ERR:Illegal PwkCipherWPA "
					"parameter. Must be either bit"
					" '2' or '3'.\r\n");
					ret = UAP_FAILURE;
					goto done;
				}
				pwkcipher_wpa = atoi(args[1]);
				if (enable_11n &&
				    protocol != PROTOCOL_WPA2_MIXED) {
					memset(&htcap, 0, sizeof(htcap));
					if (UAP_SUCCESS ==
					    get_sys_cfg_11n(&htcap)) {
						if (htcap.supported_mcs_set[0]
						    && (atoi(args[1]) ==
							CIPHER_TKIP)) {
							wmprintf
							    ("ERR: WPA/TKIP "
							"cannot be used when AP"
							" operates in 802.11n"
							" mode.\r\n");
							ret = UAP_FAILURE;
							goto done;
						}
					}
				}
			}
		}

		if (strcmp(args[0], "PwkCipherWPA2") == 0) {
			if (arg_num == 1) {
				wmprintf("ERR:PwkCipherWPA2 is blank!\r\n");
				ret = UAP_FAILURE;
				goto done;
			} else {
				if (ISDIGIT(args[1]) == 0) {
					wmprintf
					    ("ERR:Illegal PwkCipherWPA2"
						" parameter. Must be either"
						" bit '2' or '3'.\r\n");
					ret = UAP_FAILURE;
					goto done;
				}
				if (atoi(args[1]) & ~CIPHER_BITMAP) {
					wmprintf
					    ("ERR:Illegal PwkCipherWPA2"
						" parameter. Must be either"
						" bit '2' or '3'.\r\n");
					ret = UAP_FAILURE;
					goto done;
				}
				pwkcipher_wpa2 = atoi(args[1]);
				if (enable_11n &&
				    protocol != PROTOCOL_WPA2_MIXED) {
					memset(&htcap, 0, sizeof(htcap));
					if (UAP_SUCCESS ==
					    get_sys_cfg_11n(&htcap)) {
						if (htcap.supported_mcs_set[0]
						    && (atoi(args[1]) ==
							CIPHER_TKIP)) {
							wmprintf
							("ERR: WPA/TKIP cannot"
							" be used when AP"
							" operates in 802.11n"
							" mode.\r\n");
							ret = UAP_FAILURE;
							goto done;
						}
					}
				}
			}
		}
		if (strcmp(args[0], "GwkCipher") == 0) {
			if (is_input_valid(GWK_CIPHER, arg_num - 1, args + 1) !=
			    UAP_SUCCESS) {
				ret = UAP_FAILURE;
				goto done;
			}
			gwkcipher = atoi(args[1]);
		}
		if (strcmp(args[0], "GroupRekeyTime") == 0) {
			if (is_input_valid
			    (GROUPREKEYTIMER, arg_num - 1,
			     args + 1) != UAP_SUCCESS) {
				ret = UAP_FAILURE;
				goto done;
			}
			tlvbuf_group_rekey_timer *tlv = NULL;

			/* Append a new TLV */
			tlv_len = sizeof(tlvbuf_group_rekey_timer);
			buffer = os_mem_realloc(buffer, cmd_len + tlv_len);
			if (!buffer) {
				wmprintf
				    ("ERR:Cannot append group rekey timer"
					" TLV!\r\n");
				ret = UAP_FAILURE;
				goto done;
			}
			cmd_buf = (apcmdbuf_sys_configure *) buffer;
			tlv = (tlvbuf_group_rekey_timer *) (buffer + cmd_len);
			cmd_len += tlv_len;
			/* Set TLV fields */
			tlv->tag = MRVL_GRP_REKEY_TIME_TLV_ID;
			tlv->length = 4;
			tlv->group_rekey_time_sec = (t_u32) atoi(args[1]);
			endian_convert_tlv_header_out(tlv);
			tlv->group_rekey_time_sec =
			    uap_cpu_to_le32(tlv->group_rekey_time_sec);
		}
		if (strcmp(args[0], "MaxStaNum") == 0) {
			if (is_input_valid(MAXSTANUM, arg_num - 1, args + 1) !=
			    UAP_SUCCESS) {
				ret = UAP_FAILURE;
				goto done;
			}
#if 0			/* XXX remove explicit query for sta num supported*/
			if (get_max_sta_num_supported(&max_sta_num_supported)
				 == UAP_FAILURE) {
				ret = UAP_FAILURE;
				goto done;
			}
#else
			/* hard coded value*/
			max_sta_num_supported = 8;
#endif

			if (atoi(args[1]) > max_sta_num_supported) {
				wmprintf
			    ("ERR: MAX_STA_NUM must be less than %d\r\n",
				     max_sta_num_supported);
				ret = UAP_FAILURE;
				goto done;
			}
			tlvbuf_max_sta_num *tlv = NULL;

			/* Append a new TLV */
			tlv_len =
			    sizeof(tlvbuf_max_sta_num) -
			    sizeof(tlv->max_sta_num_supported);
			buffer = os_mem_realloc(buffer, cmd_len + tlv_len);
			if (!buffer) {
				wmprintf
				    ("ERR:Cannot os_mem_realloc max station"
					" number TLV!\r\n");
				ret = UAP_FAILURE;
				goto done;
			}
			cmd_buf = (apcmdbuf_sys_configure *) buffer;
			tlv = (tlvbuf_max_sta_num *) (buffer + cmd_len);
			cmd_len += tlv_len;
			/* Set TLV fields */
			tlv->tag = MRVL_MAX_STA_CNT_TLV_ID;
			tlv->length = 2;
			tlv->max_sta_num_configured = (t_u16) atoi(args[1]);
			endian_convert_tlv_header_out(tlv);
			tlv->max_sta_num_configured =
			    uap_cpu_to_le16(tlv->max_sta_num_configured);
		}
		if (strcmp(args[0], "Retrylimit") == 0) {
			if (is_input_valid(RETRYLIMIT, arg_num - 1, args + 1) !=
			    UAP_SUCCESS) {
				ret = UAP_FAILURE;
				goto done;
			}
			tlvbuf_retry_limit *tlv = NULL;

			/* Append a new TLV */
			tlv_len = sizeof(tlvbuf_retry_limit);
			buffer = os_mem_realloc(buffer, cmd_len + tlv_len);
			if (!buffer) {
				wmprintf
				    ("ERR:Cannot os_mem_realloc retry"
					" limit TLV!\r\n");
				ret = UAP_FAILURE;
				goto done;
			}
			cmd_buf = (apcmdbuf_sys_configure *) buffer;
			tlv = (tlvbuf_retry_limit *) (buffer + cmd_len);
			cmd_len += tlv_len;
			/* Set TLV fields */
			tlv->tag = MRVL_RETRY_LIMIT_TLV_ID;
			tlv->length = 1;
			tlv->retry_limit = (t_u8) atoi(args[1]);
			endian_convert_tlv_header_out(tlv);
		}
		if (strcmp(args[0], "PairwiseUpdateTimeout") == 0) {
			if ((ISDIGIT(args[1]) == 0) || (atoi(args[1]) < 0)) {
				ret = UAP_FAILURE;
				goto done;
			}
			tlvbuf_eapol_pwk_hsk_timeout *tlv = NULL;

			/* Append a new TLV */
			tlv_len = sizeof(tlvbuf_eapol_pwk_hsk_timeout);
			buffer = os_mem_realloc(buffer, cmd_len + tlv_len);
			if (!buffer) {
				wmprintf
				    ("ERR:Cannot append pairwise update"
					" timeout TLV!\r\n");
				ret = UAP_FAILURE;
				goto done;
			}
			cmd_buf = (apcmdbuf_sys_configure *) buffer;
			tlv =
			    (tlvbuf_eapol_pwk_hsk_timeout *) (buffer + cmd_len);
			cmd_len += tlv_len;
			/* Set TLV fields */
			tlv->tag = MRVL_EAPOL_PWK_HSK_TIMEOUT_TLV_ID;
			tlv->length = 4;
			tlv->pairwise_update_timeout = (t_u32) atoi(args[1]);
			endian_convert_tlv_header_out(tlv);
			tlv->pairwise_update_timeout =
			    uap_cpu_to_le32(tlv->pairwise_update_timeout);
		}
		if (strcmp(args[0], "PairwiseHandshakeRetries") == 0) {
			if ((ISDIGIT(args[1]) == 0) || (atoi(args[1]) < 0)) {
				ret = UAP_FAILURE;
				goto done;
			}
			tlvbuf_eapol_pwk_hsk_retries *tlv = NULL;

			/* Append a new TLV */
			tlv_len = sizeof(tlvbuf_eapol_pwk_hsk_retries);
			buffer = os_mem_realloc(buffer, cmd_len + tlv_len);
			if (!buffer) {
				wmprintf
				    ("ERR:Cannot append pairwise handshake"
					" retries TLV!\r\n");
				ret = UAP_FAILURE;
				goto done;
			}
			cmd_buf = (apcmdbuf_sys_configure *) buffer;
			tlv =
			    (tlvbuf_eapol_pwk_hsk_retries *) (buffer + cmd_len);
			cmd_len += tlv_len;
			/* Set TLV fields */
			tlv->tag = MRVL_EAPOL_PWK_HSK_RETRIES_TLV_ID;
			tlv->length = 4;
			tlv->pwk_retries = (t_u32) atoi(args[1]);
			endian_convert_tlv_header_out(tlv);
			tlv->pwk_retries = uap_cpu_to_le32(tlv->pwk_retries);
		}
		if (strcmp(args[0], "GroupwiseUpdateTimeout") == 0) {
			if ((ISDIGIT(args[1]) == 0) || (atoi(args[1]) < 0)) {
				ret = UAP_FAILURE;
				goto done;
			}
			tlvbuf_eapol_gwk_hsk_timeout *tlv = NULL;

			/* Append a new TLV */
			tlv_len = sizeof(tlvbuf_eapol_gwk_hsk_timeout);
			buffer = os_mem_realloc(buffer, cmd_len + tlv_len);
			if (!buffer) {
				wmprintf
				    ("ERR:Cannot append groupwise update"
					" timeout TLV!\r\n");
				ret = UAP_FAILURE;
				goto done;
			}
			cmd_buf = (apcmdbuf_sys_configure *) buffer;
			tlv =
			    (tlvbuf_eapol_gwk_hsk_timeout *) (buffer + cmd_len);
			cmd_len += tlv_len;
			/* Set TLV fields */
			tlv->tag = MRVL_EAPOL_GWK_HSK_TIMEOUT_TLV_ID;
			tlv->length = 4;
			tlv->groupwise_update_timeout = (t_u32) atoi(args[1]);
			endian_convert_tlv_header_out(tlv);
			tlv->groupwise_update_timeout =
			    uap_cpu_to_le32(tlv->groupwise_update_timeout);
		}
		if (strcmp(args[0], "GroupwiseHandshakeRetries") == 0) {
			if ((ISDIGIT(args[1]) == 0) || (atoi(args[1]) < 0)) {
				ret = UAP_FAILURE;
				goto done;
			}
			tlvbuf_eapol_gwk_hsk_retries *tlv = NULL;

			/* Append a new TLV */
			tlv_len = sizeof(tlvbuf_eapol_gwk_hsk_retries);
			buffer = os_mem_realloc(buffer, cmd_len + tlv_len);
			if (!buffer) {
				wmprintf
				    ("ERR:Cannot append groupwise handshake"
					" retries TLV!\r\n");
				ret = UAP_FAILURE;
				goto done;
			}
			cmd_buf = (apcmdbuf_sys_configure *) buffer;
			tlv =
			    (tlvbuf_eapol_gwk_hsk_retries *) (buffer + cmd_len);
			cmd_len += tlv_len;
			/* Set TLV fields */
			tlv->tag = MRVL_EAPOL_GWK_HSK_RETRIES_TLV_ID;
			tlv->length = 4;
			tlv->gwk_retries = (t_u32) atoi(args[1]);
			endian_convert_tlv_header_out(tlv);
			tlv->gwk_retries = uap_cpu_to_le32(tlv->gwk_retries);
		}

		if (strcmp(args[0], "Enable11n") == 0) {
			if ((ISDIGIT(args[1]) != UAP_SUCCESS) ||
			    (atoi(args[1]) < 0) || (atoi(args[1]) > 1)) {
				wmprintf("ERR: Invalid Enable11n value\r\n");
				ret = UAP_FAILURE;
				goto done;
			}
			tlvbuf_htcap_t *tlv = NULL;
			enable_11n = atoi(args[1]);

			memset(&htcap, 0, sizeof(htcap));
			if (UAP_SUCCESS != get_sys_cfg_11n(&htcap)) {
				wmprintf
				    ("ERR: Reading current 11n"
					" configuration.\r\n");
				ret = UAP_FAILURE;
				goto done;
			}

			/* Append a new TLV */
			tlv_len = sizeof(tlvbuf_htcap_t);
			buffer = os_mem_realloc(buffer, cmd_len + tlv_len);
			if (!buffer) {
				wmprintf("ERR:Cannot append HT Cap TLV!\r\n");
				ret = UAP_FAILURE;
				goto done;
			}
			cmd_buf = (apcmdbuf_sys_configure *) buffer;
			tlv = (tlvbuf_htcap_t *) (buffer + cmd_len);
			tlv_offset_11n = cmd_len;
			cmd_len += tlv_len;
			/* Set TLV fields */
			tlv->tag = HT_CAPABILITY_TLV_ID;
			tlv->length = sizeof(HTCap_t);
			memcpy(&tlv->ht_cap, &htcap, sizeof(HTCap_t));
			if (enable_11n == 1) {
				/* enable mcs rate */
				tlv->ht_cap.supported_mcs_set[0] =
				    DEFAULT_MCS_SET_0;
				tlv->ht_cap.supported_mcs_set[4] =
				    DEFAULT_MCS_SET_4;
			} else {
				/* disable mcs rate */
				tlv->ht_cap.supported_mcs_set[0] = 0;
				tlv->ht_cap.supported_mcs_set[4] = 0;
			}
			endian_convert_tlv_header_out(tlv);
		}
		if (strcmp(args[0], "HTCapInfo") == 0) {
			if (enable_11n <= 0) {
				wmprintf
				    ("ERR: Enable11n parameter should be set"
					" before HTCapInfo.\r\n");
				ret = UAP_FAILURE;
				goto done;
			}
			if ((IS_HEX_OR_DIGIT(args[1]) == UAP_FAILURE) ||
			    ((((t_u16) A2HEXDECIMAL(args[1])) &
			      (~HT_CAP_CONFIG_MASK)) != HT_CAP_CHECK_MASK)) {
				wmprintf("ERR: Invalid HTCapInfo value\r\n");
				ret = UAP_FAILURE;
				goto done;
			}

			/* Find HT tlv pointer in buffer and set HTCapInfo */
			tlvbuf_htcap_t *tlv = NULL;
			tlv = (tlvbuf_htcap_t *) (buffer + tlv_offset_11n);
			tlv->ht_cap.ht_cap_info =
			    DEFAULT_HT_CAP_VALUE & ~HT_CAP_CONFIG_MASK;
			tlv->ht_cap.ht_cap_info |=
			    (t_u16) A2HEXDECIMAL(args[1]) & HT_CAP_CONFIG_MASK;
			tlv->ht_cap.ht_cap_info =
			    uap_cpu_to_le16(tlv->ht_cap.ht_cap_info);
		}
		if (strcmp(args[0], "AMPDU") == 0) {
			if (enable_11n <= 0) {
				wmprintf
				    ("ERR: Enable11n parameter should be"
					" set before AMPDU.\r\n");
				ret = UAP_FAILURE;
				goto done;
			}
			if ((IS_HEX_OR_DIGIT(args[1]) == UAP_FAILURE) ||
			    ((A2HEXDECIMAL(args[1])) > AMPDU_CONFIG_MASK)) {
				wmprintf("ERR: Invalid AMPDU value\r\n");
				ret = UAP_FAILURE;
				goto done;
			}

			/* Find HT tlv pointer in buffer and set AMPDU */
			tlvbuf_htcap_t *tlv = NULL;
			tlv = (tlvbuf_htcap_t *) (buffer + tlv_offset_11n);
			tlv->ht_cap.ampdu_param =
			    (t_u8) A2HEXDECIMAL(args[1]) & AMPDU_CONFIG_MASK;
		}
		if (strcmp(args[0], "Enable2040Coex") == 0) {
			if ((ISDIGIT(args[1]) == 0) || (atoi(args[1]) < 0) ||
			    (atoi(args[1]) > 1)) {
				wmprintf("ERR: Invalid Enable2040Coex "
					"value\r\n");
				ret = UAP_FAILURE;
				goto done;
			}
			tlvbuf_2040_coex *tlv = NULL;
			/* Append a new TLV */
			tlv_len = sizeof(tlvbuf_2040_coex);
			buffer = os_mem_realloc(buffer, cmd_len + tlv_len);
			if (!buffer) {
				wmprintf("ERR:Cannot append 2040 "
					"coex TLV!\r\n");
				ret = UAP_FAILURE;
				goto done;
			}
			cmd_buf = (apcmdbuf_sys_configure *) buffer;
			tlv = (tlvbuf_2040_coex *) (buffer + cmd_len);
			cmd_len += tlv_len;
			/* Set TLV fields */
			tlv->tag = MRVL_2040_BSS_COEX_CONTROL_TLV_ID;
			tlv->length = 1;
			tlv->enable = (t_u8) atoi(args[1]);
			endian_convert_tlv_header_out(tlv);
		}
#ifdef CONFIG_P2P_DEBUG
		if (cmd_len != 0)
			hexdump((void *)cmd_buf, cmd_len, ' ');
#endif
	}
done:
/*    fclose(config_file);*/

	if (buffer)
		os_mem_free(buffer);
	if (line)
		os_mem_free(line);
	return ret;
}
