/*
 *  Copyright 2008-2013, Marvell International Ltd.
 *  All Rights Reserved.
 */

#include <wifi.h>
#include <cli_utils.h>
#include <wifidirectutl.h>
#include "wifi_common.h"

#ifndef MLAN_MAC_ADDR_LENGTH
#define MLAN_MAC_ADDR_LENGTH 6
#endif /* MLAN_MAC_ADDR_LENGTH */

/* fixme: This is duplicated for now. Remove after migration complete */
struct bus_message {
	uint16_t event;
	uint16_t reason;
	void *data;
};

/** TLV: Management IE list */
#define MRVL_MGMT_IE_LIST_TLV_ID        (PROPRIETARY_TLV_BASE_ID + 0x69)

extern int wifi_get_command_lock(void);
extern int wifi_put_command_lock(void);
extern uint8_t dev_mac_addr[MLAN_MAC_ADDR_LENGTH];
char *
config_get_line(char *s, int size, char *stream, int *line, char **_pos);
int wlan_send_gen_sdio_cmd(uint8_t *buf, uint32_t buflen);
int bus_register_special_queue(xQueueHandle *special_queue);
void bus_deregister_special_queue();

#define INTERFACE_ADDR_FLIP_BIT (0x1 << 1)

const uint8_t wps_oui_type[4] = { 0x00, 0x50, 0xf2, 0x04 };	/* WPS */
const uint8_t wifidir_oui_type[4] = { 0x50, 0x6F, 0x9A, 0x09 };	/* WIFIDirect */

char WFD_devicename[33];
char op_channel_number;

os_queue_t wfd_commad_resp_q;
static os_queue_pool_define(wfd_commad_resp_q_data,
			sizeof(struct bus_message));
int wfd_ioctl(t_u8 *cmd, t_u16 buf_size)
{
	int ret;
	struct bus_message msg;
	static bool Q_created;

	if (!Q_created) {
		ret = os_queue_create(&wfd_commad_resp_q, "commad_resp",
		      sizeof(struct bus_message), &wfd_commad_resp_q_data);
		if (ret) {
			wfd_e("commad_resp Q failed to create");
			return -WM_FAIL;
		}
		Q_created = 1;
	}
	wifi_get_command_lock();
	ret = bus_register_special_queue(&wfd_commad_resp_q);
	if (ret) {
		wfd_e("bus_register_special_queue failed\n\r");
		ret = -WM_FAIL;
		goto done;
	}

	wfd_d("wfd_ioctl: Sent");
#ifdef CONFIG_P2P_DEBUG
	hexdump(cmd, buf_size, ' ');
#endif
	wlan_send_gen_sdio_cmd(cmd, buf_size);
	ret = os_queue_recv(&wfd_commad_resp_q, &msg, OS_WAIT_FOREVER);
	if (ret != WM_SUCCESS) {
		wfd_e("os_queue_recv &uap_commad_resp_q failed\n\r");
		ret = -WM_FAIL;
		goto done;
	}
	memcpy(cmd + 4, msg.data, ((HostCmd_DS_GEN *) msg.data)->size);

	wfd_d("wfd_ioctl: Received\n");
#ifdef CONFIG_P2P_DEBUG
	if (buf_size < ((HostCmd_DS_GEN *) msg.data)->size) {
		wfd_e("Truncating wfd_ioctl return buffer");
		hexdump(cmd, buf_size, ' ');
	} else
		hexdump(cmd, ((HostCmd_DS_GEN *) msg.data)->size, ' ');
#endif
	ret = WM_SUCCESS;
done:
	bus_deregister_special_queue();
	wifi_put_command_lock();
	os_mem_free(msg.data);
	return ret;
}

t_u16 htons(t_u16 n)
{
	return ((n & 0xff) << 8) | ((n & 0xff00) >> 8);
}

/**
 *  @brief Check whether the mac address is all zero
 *  @param dev_addr    the mac address to be checked
 *  @return SUCCESS--all zero, FAILURE--not all zero
 */
static int
is_mac_all_zero(uint8_t *dev_addr)
{
	int i;
	for (i = 0; i < MLAN_MAC_ADDR_LENGTH; ++i) {
		if (dev_addr[i] != 0)
			return FAILURE;
	}
	return SUCCESS;
}

/**
 *  @brief Checkes a particular input for validatation.
 *
 *  @param cmd      Type of input
 *  @param argc     Number of arguments
 *  @param argv     Pointer to the arguments
 *  @return         SUCCESS or FAILURE
 */
static int is_input_valid(valid_inputs cmd, int argc, char *argv[])
{
	int i;
	int ret = SUCCESS;
	char country[6] = { ' ', ' ', 0, 0, 0, 0 };
	char wifidirect_dev_name[34];
	if (argc == 0)
		return FAILURE;
	switch (cmd) {
	case WIFIDIRECT_MINDISCOVERYINT:
		if (argc != 1) {
			wmprintf
			    ("ERR:Incorrect number of arguments for "
				"MinDiscovery Interval\r\n");
			ret = FAILURE;
		} else {
			if ((ISDIGIT(argv[0]) == 0)
			    || (atoi(argv[0]) >= (1 << 16))) {
				wmprintf
				    ("ERR:MinDiscoveryInterval must be "
					"2 bytes\r\n");
				ret = FAILURE;
			}
		}
		break;
	case WIFIDIRECT_MAXDISCOVERYINT:
		if (argc != 1) {
			wmprintf
			    ("ERR:Incorrect number of arguments for "
				"MaxDiscoveryInterval\r\n");
			ret = FAILURE;
		} else {
			if ((ISDIGIT(argv[0]) == 0)
			    || (atoi(argv[0]) >= (1 << 16))) {
				wmprintf
				    ("ERR:MaxDiscoveryInterval must be "
					"2 bytes\r\n");
				ret = FAILURE;
			}
		}
		break;
	case WIFIDIRECT_DEVICECAPABILITY:
		if (argc != 1) {
			wmprintf
			    ("ERR:Incorrect number of arguments for "
				"DeviceCapability\r\n");
			ret = FAILURE;
		} else {
			if ((ISDIGIT(argv[0]) == 0)
			    || (atoi(argv[0]) > MAX_DEV_CAPABILITY)
			    || (atoi(argv[0]) < 0)) {
				wmprintf
				    ("ERR:DeviceCapability must be in the "
					"range [0:%d]\r\n",
				     MAX_DEV_CAPABILITY);
				ret = FAILURE;
			}
		}
		break;
	case WIFIDIRECT_GROUPCAPABILITY:
		if (argc != 1) {
			wmprintf
			    ("ERR:Incorrect number of arguments for "
					"GroupCapability\r\n");
			ret = FAILURE;
		} else {
			if ((ISDIGIT(argv[0]) == 0)
			    || (atoi(argv[0]) > MAX_GRP_CAPABILITY)
			    || (atoi(argv[0]) < 0)) {
				wmprintf
				    ("ERR:GroupCapability must be in the "
					"range [0:%d]\r\n",
				     MAX_GRP_CAPABILITY);
				ret = FAILURE;
			}
		}
		break;
	case CHANNEL:
		if ((argc != 1) && (argc != 2)) {
			wmprintf("ERR: Incorrect arguments for channel.\r\n");
			ret = FAILURE;
		} else {
			if (argc == 2) {
				if ((ISDIGIT(argv[1]) == 0)
				    || (atoi(argv[1]) < 0)
				    || (atoi(argv[1]) > 1)) {
					wmprintf
					    ("ERR: MODE must be either "
							"0 or 1\r\n");
					ret = FAILURE;
				}
				if ((atoi(argv[1]) == 1)
				    && (atoi(argv[0]) != 0)) {
					wmprintf
					    ("ERR: Channel must be 0 for "
						"ACS; MODE = 1.\r\n");
					ret = FAILURE;
				}
			}
			if ((argc == 1) || (atoi(argv[1]) == 0)) {
				if ((ISDIGIT(argv[0]) == 0)
				    || (atoi(argv[0]) < 1)
				    || (atoi(argv[0]) > MAX_CHANNELS)) {
					wmprintf
					    ("ERR: Channel must be in the "
						"range of 1 to %d\r\n",
					     MAX_CHANNELS);
					ret = FAILURE;
				}
			}
		}
		break;
	case SCANCHANNELS:
		if (argc > MAX_CHANNELS) {
			wmprintf("ERR: Invalid List of Channels\r\n");
			ret = FAILURE;
		} else {
			for (i = 0; i < argc; i++) {
				if ((ISDIGIT(argv[i]) == 0)
				    || (atoi(argv[i]) < 1)
				    || (atoi(argv[i]) > MAX_CHANNELS)) {
					wmprintf
					    ("ERR: Channel must be in the "
						"range of 1 to %d\r\n",
					     MAX_CHANNELS);
					ret = FAILURE;
					break;
				}
			}
			if ((ret != FAILURE)
			    && (has_dup_channel(argc, argv) != SUCCESS)) {
				wmprintf
				    ("ERR: Duplicate channel values "
						"entered\r\n");
				ret = FAILURE;
			}
		}
		break;
	case WIFIDIRECT_INTENT:
		if (argc != 1) {
			wmprintf
			    ("ERR:Incorrect number of arguments "
					"for intent\r\n");
			ret = FAILURE;
		} else {
			if ((ISDIGIT(argv[0]) == 0)
			    || (atoi(argv[0]) > MAX_INTENT)
			    || (atoi(argv[0]) < 0)) {
				wmprintf
				    ("ERR:Intent must be in the "
						"range [0:%d]\r\n",
				     MAX_INTENT);
				ret = FAILURE;
			}
		}
		break;
	case WIFIDIRECT_MANAGEABILITY:
		if (argc != 1) {
			wmprintf
			    ("ERR:Incorrect number of arguments for "
					"manageability\r\n");
			ret = FAILURE;
		} else {
			if ((ISDIGIT(argv[0]) == 0) || (atoi(argv[0]) < 0) ||
			    (atoi(argv[0]) > 1)) {
				wmprintf
				    ("ERR:Manageability must be either "
					"0 or 1\r\n");
				ret = FAILURE;
			}
		}
		break;
	case WIFIDIRECT_GROUP_WIFIDIRECT_DEVICE_NAME:
		/* 2 extra characters for quotes around device name */
		if ((strlen(argv[0]) > 34)) {
			wmprintf
			    ("ERR:WIFIDIRECT Device name string length must "
					"not be more than 32\r\n");
			ret = FAILURE;
		} else {
			strncpy(wifidirect_dev_name, argv[0],
				sizeof(wifidirect_dev_name) - 1);
			if ((wifidirect_dev_name[0] != '"') ||
			    (wifidirect_dev_name
			     [strlen(wifidirect_dev_name) - 1] != '"')) {
				wmprintf
				    ("ERR:WIFIDIRECT Device name must be "
						"within double quotes!\r\n");
				ret = FAILURE;
			}
		}
		break;
	case WIFIDIRECT_COUNTRY:
		/* 2 extra characters for quotes around country */
		if ((strlen(argv[0]) > 5) || (strlen(argv[0]) < 4)) {
			wmprintf
			    ("ERR:Country string must have length 2 or 3\r\n");
			ret = FAILURE;
		} else {
			strncpy(country, argv[0], sizeof(country) - 1);
			if ((country[0] != '"')
			    || (country[strlen(country) - 1] != '"')) {
				wmprintf
				    ("ERR:country code must be within "
						"double quotes!\r\n");
				ret = FAILURE;
			} else {
				for (i = 1; i < strlen(country) - 2; i++) {
					if ((toupper(country[i]) < 'A') ||
					    (toupper(country[i]) > 'Z')) {
						wmprintf
						    ("ERR:Invalid Country "
							"Code\r\n");
						ret = FAILURE;
					}
				}
			}
		}
		break;
	case WIFIDIRECT_NO_OF_CHANNELS:
		if (argc != 1) {
			wmprintf
			    ("ERR:Incorrect number of arguments for "
					"num of channels\r\n");
			ret = FAILURE;
		} else {
			if ((ISDIGIT(argv[0]) == 0)
			    || (atoi(argv[0]) > MAX_CHANNELS)) {
				wmprintf
				    ("ERR:Number of channels should be "
					"less than %d\r\n",
				     MAX_CHANNELS);
				ret = FAILURE;
			}
		}
		break;
	case WIFIDIRECT_NOA_INDEX:
		if (argc != 1) {
			wmprintf
			    ("ERR:Incorrect number of arguments "
					"for NoA Index\r\n");
			ret = FAILURE;
		} else {
			if ((ISDIGIT(argv[0]) == 0)
			    || (atoi(argv[0]) < MIN_NOA_INDEX)
			    || (atoi(argv[0]) > MAX_NOA_INDEX)) {
				wmprintf
				    ("ERR:NoA index should be in the "
					"range [%d:%d]\r\n",
				     MIN_NOA_INDEX, MAX_NOA_INDEX);
				ret = FAILURE;
			}
		}
		break;
	case WIFIDIRECT_OPP_PS:
		if (argc != 1) {
			wmprintf
			    ("ERR:Incorrect number of arguments "
					"for Opp PS\r\n");
			ret = FAILURE;
		} else {
			if ((ISDIGIT(argv[0]) == 0) || ((atoi(argv[0]) != 0) &&
							(atoi(argv[0]) != 1))) {
				wmprintf("ERR:Opp PS must be either "
						"0 or 1\r\n");
				ret = FAILURE;
			}
		}
		break;
	case WIFIDIRECT_CTWINDOW:
		if (argc != 1) {
			wmprintf
			    ("ERR:Incorrect number of arguments for "
					"CTWindow\r\n");
			ret = FAILURE;
		} else {
			if ((ISDIGIT(argv[0]) == 0)
			    || (atoi(argv[0]) > MAX_CTWINDOW)
			    || (atoi(argv[0]) < MIN_CTWINDOW)) {
				wmprintf
				    ("ERR:CT Window must be in the "
					"range [%d:%d]\r\n",
				     MIN_CTWINDOW, MAX_CTWINDOW);
				ret = FAILURE;
			}
		}
		break;
	case WIFIDIRECT_COUNT_TYPE:
		if (argc != 1) {
			wmprintf
			    ("ERR:Incorrect number of arguments for "
					"Count/Type\r\n");
			ret = FAILURE;
		} else {
			if ((ISDIGIT(argv[0]) == 0) || (atoi(argv[0]) == 0) ||
			    (atoi(argv[0]) > MAX_COUNT_TYPE) ||
			    (atoi(argv[0]) < MIN_COUNT_TYPE)) {
				wmprintf
				    ("ERR:Count/Type must be in the "
						"range [%d:%d] or 255\r\n",
				     MIN_COUNT_TYPE, MAX_COUNT_TYPE);
				ret = FAILURE;
			}
		}
		break;
	case WIFIDIRECT_PRESENCE_REQ_TYPE:
		if (argc != 1) {
			wmprintf
			    ("ERR:Incorrect number of arguments for "
						"Presence request type\r\n");
			ret = FAILURE;
		} else {
			if ((ISDIGIT(argv[0]) == 0) || ((atoi(argv[0]) != 1)
							&& (atoi(argv[0]) !=
							    2))) {
				wmprintf("ERR:Presence Type must be "
						"1 or 2.\r\n");
				ret = FAILURE;
			}
		}
		break;
	case WIFIDIRECT_DURATION:
		if (argc != 1) {
			wmprintf
			    ("ERR:Incorrect number of arguments for "
					"Duration\r\n");
			ret = FAILURE;
		}
		break;
	case WIFIDIRECT_INTERVAL:
		if (argc != 1) {
			wmprintf
			    ("ERR:Incorrect number of arguments "
				"for Interval\r\n");
			ret = FAILURE;
		}
		break;
	case WIFIDIRECT_START_TIME:
		if (argc != 1) {
			wmprintf
			    ("ERR:Incorrect number of arguments for "
					"Start Time\r\n");
			ret = FAILURE;
		}
		break;
	case WIFIDIRECT_PRIDEVTYPEOUI:
		if (argc > MAX_PRIMARY_OUI_LEN) {
			wmprintf
			    ("ERR: Incorrect number of PrimaryDeviceTypeOUI "
					"arguments.\r\n");
			ret = FAILURE;
			break;
		}
		for (i = 0; i < argc; i++) {
			if (IS_HEX_OR_DIGIT(argv[i]) == FAILURE) {
				wmprintf("ERR:Unsupported OUI\r\n");
				ret = FAILURE;
				break;
			}
		}
		if (!((A2HEXDECIMAL(argv[0]) == 0x00)
		      && (A2HEXDECIMAL(argv[1]) == 0x50)
		      && (A2HEXDECIMAL(argv[2]) == 0xF2)
		      && (A2HEXDECIMAL(argv[3]) == 0x04))) {
			wmprintf("ERR:Unsupported OUI\r\n");
			ret = FAILURE;
			break;
		}
		break;
	case WIFIDIRECT_REGULATORYCLASS:
		if (argc != 1) {
			wmprintf
			    ("ERR:Incorrect number of arguments for "
				"RegulatoryClass\r\n");
			ret = FAILURE;
		} else {
			if ((ISDIGIT(argv[0]) == 0)
			    || (atoi(argv[0]) > MAX_REG_CLASS)
			    || (atoi(argv[0]) < MIN_REG_CLASS)) {
				wmprintf
				    ("ERR:RegulatoryClass must be in the "
					"range [%d:%d] or 255\r\n",
				     MIN_REG_CLASS, MAX_REG_CLASS);
				ret = FAILURE;
			}
		}
		break;
	case WIFIDIRECT_PRIDEVTYPECATEGORY:
		if (argc != 1) {
			wmprintf
			    ("ERR:Incorrect number of arguments for "
				"PrimaryDeviceTypeCategory\r\n");
			ret = FAILURE;
		} else {
			if ((ISDIGIT(argv[0]) == 0)
			    || (atoi(argv[0]) > MAX_PRIDEV_TYPE_CAT)
			    || (atoi(argv[0]) < MIN_PRIDEV_TYPE_CAT)) {
				wmprintf
				    ("ERR:PrimaryDeviceTypeCategory must "
					"be in the range [%d:%d]\r\n",
				     MIN_PRIDEV_TYPE_CAT, MAX_PRIDEV_TYPE_CAT);
				ret = FAILURE;
			}
		}
		break;
	case WIFIDIRECT_SECONDARYDEVCOUNT:
		if (argc != 1) {
			wmprintf
			    ("ERR:Incorrect number of arguments for "
				"SecondaryDeviceCount\r\n");
			ret = FAILURE;
		} else {
			if ((ISDIGIT(argv[0]) == 0) ||
			    (atoi(argv[0]) > MAX_SECONDARY_DEVICE_COUNT)) {
				wmprintf
				    ("ERR:SecondaryDeviceCount must be less "
					"than 15.\r\n");
				ret = FAILURE;
			}
		}
		break;
	case WIFIDIRECT_INTERFACECOUNT:
		if (argc != 1) {
			wmprintf
			    ("ERR:Incorrect number of arguments for "
				"InterfaceCount\r\n");
			ret = FAILURE;
		} else {
			if ((ISDIGIT(argv[0]) == 0) ||
			    (atoi(argv[0]) > MAX_INTERFACE_ADDR_COUNT)) {
				wmprintf
				    ("ERR:IntefaceCount must be in "
					"range.[0-41]\r\n");
				ret = FAILURE;
			}
		}
		break;
	case WIFIDIRECT_GROUP_SECONDARYDEVCOUNT:
		if (argc != 1) {
			wmprintf
			    ("ERR:Incorrect number of arguments for "
				"SecondaryDeviceCount\r\n");
			ret = FAILURE;
		} else {
			if ((ISDIGIT(argv[0]) == 0) ||
			    (atoi(argv[0]) >
			     MAX_GROUP_SECONDARY_DEVICE_COUNT)) {
				wmprintf
				    ("ERR:SecondaryDeviceCount must be "
					"less than 2.\r\n");
				ret = FAILURE;
			}
		}
		break;
	case WIFIDIRECT_ATTR_CONFIG_TIMEOUT:
		if (argc != 1) {
			wmprintf
			    ("ERR:Incorrect number of arguments for Timeout "
				"Configuration\r\n");
			ret = FAILURE;
		} else {
			if ((ISDIGIT(argv[0]) == 0) || (atoi(argv[0]) > 255)) {
				wmprintf
				    ("ERR:TimeoutConfig must be in the "
					"range [0:255]\r\n");
				ret = FAILURE;
			}
		}
		break;
	case WIFIDIRECT_ATTR_EXTENDED_TIME:
		if (argc != 1) {
			wmprintf
			    ("ERR:Incorrect number of arguments for "
				"Extended time.\r\n");
			ret = FAILURE;
		} else {
			if ((ISDIGIT(argv[0]) == 0) || (atoi(argv[0]) > 65535)
			    || (atoi(argv[0]) < 1)) {
				wmprintf
				    ("ERR:Extended Time must be in the "
					"range [1:65535]\r\n");
				ret = FAILURE;
			}
		}
		break;

	case WIFIDIRECT_PRIDEVTYPESUBCATEGORY:
		if (argc != 1) {
			wmprintf
			    ("ERR:Incorrect number of arguments for "
				"PrimaryDeviceTypeSubCategory\r\n");
			ret = FAILURE;
		} else {
			if ((ISDIGIT(argv[0]) == 0) ||
			    (atoi(argv[0]) > MAX_PRIDEV_TYPE_SUBCATEGORY) ||
			    (atoi(argv[0]) < MIN_PRIDEV_TYPE_SUBCATEGORY)) {
				wmprintf
				    ("ERR:PrimaryDeviceTypeSubCategory must "
					"be in the range [%d:%d]\r\n",
				     MIN_PRIDEV_TYPE_SUBCATEGORY,
				     MAX_PRIDEV_TYPE_SUBCATEGORY);
				ret = FAILURE;
			}
		}
		break;
	case WIFIDIRECT_INVITATIONFLAG:
		if (argc != 1) {
			wmprintf
			    ("ERR:Incorrect number of arguments for "
				"InvitationFlag\r\n");
			ret = FAILURE;
		} else {
			if ((ISDIGIT(argv[0]) == 0) || (atoi(argv[0]) < 0) ||
			    (atoi(argv[0]) > 1)) {
				wmprintf
				    ("ERR:Invitation flag must be "
					"either 0 or 1\r\n");
				ret = FAILURE;
			}
		}
		break;
	case WIFIDIRECT_WPSCONFMETHODS:
		if (argc != 1) {
			wmprintf
			    ("ERR:Incorrect number of arguments for "
				"WPSConfigMethods\r\n");
			ret = FAILURE;
		} else {
			if ((IS_HEX_OR_DIGIT(argv[0]) == 0) ||
			    (A2HEXDECIMAL(argv[0]) > MAX_WPS_CONF_METHODS) ||
			    (A2HEXDECIMAL(argv[0]) < MIN_WPS_CONF_METHODS)) {
				wmprintf
				    ("ERR:WPSConfigMethods must be in the "
					"range [%d:%d]\r\n",
				     MIN_WPS_CONF_METHODS,
				     MAX_WPS_CONF_METHODS);
				ret = FAILURE;
			}
		}
		break;
	case WIFIDIRECT_WPSVERSION:
		if (argc != 1) {
			wmprintf
			    ("ERR:Incorrect number of arguments for "
				"WPSVersion\r\n");
			ret = FAILURE;
		} else {
			if ((A2HEXDECIMAL(argv[0]) < 0x10) &&
			    (A2HEXDECIMAL(argv[0]) > 0x20)) {
				wmprintf("ERR:Incorrect WPS Version %s\r\n",
					 argv[0]);
				ret = FAILURE;
			}
		}
		break;
	case WIFIDIRECT_WPSSETUPSTATE:
		if (argc != 1) {
			wmprintf
			    ("ERR:Incorrect number of arguments for "
				"WPSSetupState\r\n");
			ret = FAILURE;
		} else {
			if ((IS_HEX_OR_DIGIT(argv[0]) == 0) ||
			    ((A2HEXDECIMAL(argv[0]) != 0x01)
			     && (A2HEXDECIMAL(argv[0]) != 0x02))) {
				wmprintf("ERR:Incorrect WPSSetupState %s\r\n",
					 argv[0]);
				ret = FAILURE;
			}
		}
		break;
	case WIFIDIRECT_WPSREQRESPTYPE:
		if (argc != 1) {
			wmprintf
			    ("ERR:Incorrect number of arguments for "
				"WPSRequestType\r\n");
			ret = FAILURE;
		} else {
			if ((IS_HEX_OR_DIGIT(argv[0]) == 0) ||
			    (A2HEXDECIMAL(argv[0]) > WPS_MAX_REQUESTTYPE)) {
				wmprintf("ERR:Incorrect WPSRequestType %s\r\n",
					 argv[0]);
				ret = FAILURE;
			}
		}
		break;
	case WIFIDIRECT_WPSSPECCONFMETHODS:
		if (argc != 1) {
			wmprintf
			    ("ERR:Incorrect number of arguments for "
				"WPSSpecConfigMethods\r\n");
			ret = FAILURE;
		} else {
			if ((IS_HEX_OR_DIGIT(argv[0]) == 0) ||
			    ((t_u16) A2HEXDECIMAL(argv[0]) >
			     WPS_MAX_SPECCONFMETHODS)
			    || ((t_u16) A2HEXDECIMAL(argv[0]) <
				WPS_MIN_SPECCONFMETHODS)) {
				wmprintf
				    ("ERR:WPSSpecConfigMethods must be in "
					"the range [%d:%d]\r\n",
				     WPS_MIN_SPECCONFMETHODS,
				     WPS_MAX_SPECCONFMETHODS);
				ret = FAILURE;
			}
		}
		break;
	case WIFIDIRECT_WPSDEVICENAME:
		if (argc != 1) {
			wmprintf("ERR:Incorrect number of arguments\r\n");
			ret = FAILURE;
		} else {
			if (strlen(argv[0]) > WPS_DEVICE_NAME_MAX_LEN) {
				wmprintf("ERR:Device name should contain"
					 " less than 32 charactors.\r\n");
				ret = FAILURE;
			}
		}
		break;
	case WIFIDIRECT_WPSMANUFACTURER:
		if (argc != 1) {
			wmprintf("ERR:Incorrect number of arguments\r\n");
			ret = FAILURE;
		} else {
			if (strlen(argv[0]) > WPS_MANUFACT_MAX_LEN) {
				wmprintf("ERR:Manufacturer name should contain"
					 "less than 64 charactors.\r\n");
				ret = FAILURE;
			}
		}
		break;
	case WIFIDIRECT_WPSMODELNAME:
		if (argc != 1) {
			wmprintf("ERR:Incorrect number of arguments\r\n");
			ret = FAILURE;
		} else {
			if (strlen(argv[0]) > WPS_MODEL_MAX_LEN) {
				wmprintf("ERR:Model name should contain"
					 " less than 64 charactors.\r\n");
				ret = FAILURE;
			}
		}
		break;
	case WIFIDIRECT_WPSUUID:
		if (argc > WPS_UUID_MAX_LEN) {
			wmprintf
			    ("ERR: Incorrect number of WPSUUID arguments.\r\n");
			ret = FAILURE;
		} else {
			for (i = 0; i < argc; i++) {
				if (IS_HEX_OR_DIGIT(argv[i]) == FAILURE) {
					wmprintf("ERR:Unsupported UUID\r\n");
					ret = FAILURE;
					break;
				}
			}
		}
		break;
	case WIFIDIRECT_WPSPRIMARYDEVICETYPE:
		if (argc > WPS_DEVICE_TYPE_MAX_LEN) {
			wmprintf
			    ("ERR: Incorrect number of WPSPrimaryDeviceType "
				"arguments.\r\n");
			ret = FAILURE;
			break;
		}
		for (i = 0; i < argc; i++) {
			if (IS_HEX_OR_DIGIT(argv[i]) == FAILURE) {
				wmprintf
				    ("ERR:Unsupported primary device type\r\n");
				ret = FAILURE;
				break;
			}
		}
		if (!((A2HEXDECIMAL(argv[2]) == 0x00)
		      && (A2HEXDECIMAL(argv[3]) == 0x50)
		      && (A2HEXDECIMAL(argv[4]) == 0xF2)
		      && (A2HEXDECIMAL(argv[5]) == 0x04))) {
			wmprintf("ERR:Unsupported OUI\r\n");
			ret = FAILURE;
			break;
		}
		break;
	case WIFIDIRECT_WPSRFBAND:
		if (argc != 1) {
			wmprintf
			    ("ERR:Incorrect number of arguments for "
				"WPSRFBand\r\n");
			ret = FAILURE;
		} else {
			if ((IS_HEX_OR_DIGIT(argv[0]) == 0) ||
			    ((A2HEXDECIMAL(argv[0]) != 0x01)
			     && (A2HEXDECIMAL(argv[0]) != 0x02))) {
				wmprintf("ERR:Incorrect WPSRFBand %s\r\n",
					 argv[0]);
				ret = FAILURE;
			}
		}
		break;
	case WIFIDIRECT_WPSASSOCIATIONSTATE:
		if (argc != 1) {
			wmprintf
			    ("ERR:Incorrect number of arguments for "
				"WPSAssociationState\r\n");
			ret = FAILURE;
		} else {
			if ((IS_HEX_OR_DIGIT(argv[0]) == 0) ||
			    ((t_u16) A2HEXDECIMAL(argv[0]) >
			     WPS_MAX_ASSOCIATIONSTATE)
			    || ((t_u16) A2HEXDECIMAL(argv[0]) <
				WPS_MIN_ASSOCIATIONSTATE)) {
				wmprintf
				    ("ERR:WPSAssociationState must be in the "
					"range [%d:%d]\r\n",
				     WPS_MIN_ASSOCIATIONSTATE,
				     WPS_MAX_ASSOCIATIONSTATE);
				ret = FAILURE;
			}
		}
		break;
	case WIFIDIRECT_WPSCONFIGURATIONERROR:
		if (argc != 1) {
			wmprintf
			    ("ERR:Incorrect number of arguments for "
				"WPSConfigurationError\r\n");
			ret = FAILURE;
		} else {
			if ((IS_HEX_OR_DIGIT(argv[0]) == 0) ||
			    ((t_u16) A2HEXDECIMAL(argv[0]) >
			     WPS_MAX_CONFIGURATIONERROR)
			    || ((t_u16) A2HEXDECIMAL(argv[0]) <
				WPS_MIN_CONFIGURATIONERROR)) {
				wmprintf
				    ("ERR:WPSConfigurationError must be "
					"in the range [%d:%d]\r\n",
				     WPS_MIN_CONFIGURATIONERROR,
				     WPS_MAX_CONFIGURATIONERROR);
				ret = FAILURE;
			}
		}
		break;
	case WIFIDIRECT_WPSDEVICEPASSWORD:
		if (argc != 1) {
			wmprintf
			    ("ERR:Incorrect number of arguments for "
				"WPSDevicePassword\r\n");
			ret = FAILURE;
		} else {
			if ((IS_HEX_OR_DIGIT(argv[0]) == 0) ||
			    ((t_u16) A2HEXDECIMAL(argv[0]) >
			     WPS_MAX_DEVICEPASSWORD)
			    || ((t_u16) A2HEXDECIMAL(argv[0]) <
				WPS_MIN_DEVICEPASSWORD)) {
				wmprintf
				    ("ERR:WPSDevicePassword must be in "
					"the range [%d:%d]\r\n",
				     WPS_MIN_DEVICEPASSWORD,
				     WPS_MAX_DEVICEPASSWORD);
				ret = FAILURE;
			}
		}
		break;
	case WIFIDIRECT_WPSMODELNUMBER:
		if (argc > WPS_MODEL_MAX_LEN) {
			wmprintf
			    ("ERR: Incorrect number of WPSModelNumber "
					"arguments.\r\n");
			ret = FAILURE;
		} else {
			for (i = 0; i < argc; i++) {
				if (IS_HEX_OR_DIGIT(argv[i]) == FAILURE) {
					wmprintf
					    ("ERR:Unsupported "
						"WPSModelNumber\r\n");
					ret = FAILURE;
					break;
				}
			}
		}
		break;
	case WIFIDIRECT_WPSSERIALNUMBER:
		if (argc > WPS_SERIAL_MAX_LEN) {
			wmprintf
			    ("ERR: Incorrect number of WPSSerialNumber "
					"arguments.\r\n");
			ret = FAILURE;
		} else {
			for (i = 0; i < argc; i++) {
				if (IS_HEX_OR_DIGIT(argv[i]) == FAILURE) {
					wmprintf
					    ("ERR:Unsupported "
						"WPSSerialNumber\r\n");
					ret = FAILURE;
					break;
				}
			}
		}
		break;
	case WIFIDIRECT_CATEGORY:
		if (argc != 1) {
			wmprintf
			    ("ERR:Incorrect number of arguments for "
					"Category\r\n");
			ret = FAILURE;
		} else {
			if (ISDIGIT(argv[0]) == 0) {
				wmprintf("ERR:Category incorrect value\r\n");
				ret = FAILURE;
			}
		}
		break;
	case WIFIDIRECT_ACTION:
		if (argc != 1) {
			wmprintf
			    ("ERR:Incorrect number of arguments "
				"for Action\r\n");
			ret = FAILURE;
		} else {
			if ((ISDIGIT(argv[0]) == 0)
			    || ((atoi(argv[0]) > 0x10)
				&& (atoi(argv[0]) != 0xDD))) {
				wmprintf
				    ("ERR:Action must be less than "
					"0x10 or 0xDD\r\n");
				ret = FAILURE;
			}
		}
		break;
	case WIFIDIRECT_DIALOGTOKEN:
		if (argc != 1) {
			wmprintf
			    ("ERR:Incorrect number of arguments for "
					"DialogToken\r\n");
			ret = FAILURE;
		} else {
			if (ISDIGIT(argv[0]) == 0) {
				wmprintf("ERR:DialogToken incorrect value\r\n");
				ret = FAILURE;
			}
		}
		break;
	case WIFIDIRECT_GAS_COMEBACK_DELAY:
		if (argc != 1) {
			wmprintf
			    ("ERR:Incorrect number of arguments for GAS "
					"Comeback Delay\r\n");
			ret = FAILURE;
		}
		break;
	case WIFIDIRECT_DISC_ADPROTOIE:
		if (argc > MAX_ADPROTOIE_LEN) {
			wmprintf
			    ("ERR: Incorrect number of "
					"AdvertisementProtocolIE "
					"arguments.\r\n");
			ret = FAILURE;
			break;
		}
		for (i = 0; i < argc; i++) {
			if (IS_HEX_OR_DIGIT(argv[i]) == FAILURE) {
				wmprintf
				    ("ERR:Unsupported "
					"AdvertisementProtocolIE\r\n");
				ret = FAILURE;
				break;
			}
		}
		break;
	case WIFIDIRECT_DISC_INFOID:
		if (argc > MAX_INFOID_LEN) {
			wmprintf
			    ("ERR: Incorrect number of "
					"DiscoveryInformationID "
					"arguments.\r\n");
			ret = FAILURE;
			break;
		}
		if (!((A2HEXDECIMAL(argv[0]) == 0xDD)
		      && (A2HEXDECIMAL(argv[1]) == 0xDD))
		    && !((A2HEXDECIMAL(argv[0]) == 0xDE)
			 && (A2HEXDECIMAL(argv[1]) == 0x92))) {
			wmprintf("ERR:Unsupported DiscoveryInformationID\r\n");
			ret = FAILURE;
		}
		break;
	case WIFIDIRECT_OUI:
		if (argc > MAX_OUI_LEN) {
			wmprintf("ERR: Incorrect number of OUI arguments.\r\n");
			ret = FAILURE;
			break;
		}
		for (i = 0; i < argc; i++) {
			if (IS_HEX_OR_DIGIT(argv[i]) == FAILURE) {
				wmprintf("ERR:Unsupported OUI\r\n");
				ret = FAILURE;
				break;
			}
		}
		break;
	case WIFIDIRECT_OUITYPE:
		if (argc != 1) {
			wmprintf
			    ("ERR:Incorrect number of arguments for "
				"OUIType\r\n");
			ret = FAILURE;
		} else {
			if (ISDIGIT(argv[0]) == 0) {
				wmprintf("ERR:OUIType incorrect value\r\n");
				ret = FAILURE;
			}
		}
		break;
	case WIFIDIRECT_OUISUBTYPE:
		if (argc != 1) {
			wmprintf
			    ("ERR:Incorrect number of arguments for "
				"OUISubtype\r\n");
			ret = FAILURE;
		} else {
			if (ISDIGIT(argv[0]) == 0) {
				wmprintf("ERR:OUISubtype incorrect value\r\n");
				ret = FAILURE;
			}
		}
		break;
	case WIFIDIRECT_DISC_SERVICEPROTO:
		if (argc != 1) {
			wmprintf
			    ("ERR:Incorrect number of arguments for "
					"DiscoveryServiceProtocol\r\n");
			ret = FAILURE;
		} else {
			if ((ISDIGIT(argv[0]) == 0) || ((atoi(argv[0]) >= 4) &&
							(atoi(argv[0]) !=
							 0xFF))) {
				wmprintf
				    ("ERR:DiscoveryServiceProtocol incorrect "
					"value\r\n");
				ret = FAILURE;
			}
		}
		break;
	case WIFIDIRECT_SERVICEUPDATE_INDICATOR:
		if (argc != 1) {
			wmprintf
			    ("ERR:Incorrect number of arguments for "
				"ServiceUpdateIndicator\r\n");
			ret = FAILURE;
		} else {
			if (ISDIGIT(argv[0]) == 0) {
				wmprintf
				    ("ERR:ServiceUpdateIndicator incorrect "
					"value\r\n");
				ret = FAILURE;
			}
		}
		break;
	case WIFIDIRECT_DISC_SERVICETRANSACID:
		if (argc != 1) {
			wmprintf
			    ("ERR:Incorrect number of arguments for "
					"DiscoveryServiceTransactionID\r\n");
			ret = FAILURE;
		} else {
			if (ISDIGIT(argv[0]) == 0) {
				wmprintf
				    ("ERR:DiscoveryServiceTransactionID "
					"incorrect value\r\n");
				ret = FAILURE;
			}
		}
		break;
	case WIFIDIRECT_DISC_SERVICE_STATUS:
		if (argc != 1) {
			wmprintf
			    ("ERR:Incorrect number of arguments for "
				"ServiceStatus\r\n");
			ret = FAILURE;
		} else {
			if ((ISDIGIT(argv[0]) == 0) || (atoi(argv[0]) >= 4)) {
				wmprintf("ERR:ServiceStatus incorrect "
						"value\r\n");
				ret = FAILURE;
			}
		}
		break;
	case WIFIDIRECT_DISC_DNSTYPE:
		if (argc != 1) {
			wmprintf
			    ("ERR:Incorrect number of arguments for "
				"DiscoveryDNSType\r\n");
			ret = FAILURE;
		} else {
			if (ISDIGIT(argv[0]) == 0) {
				wmprintf
				    ("ERR:DiscoveryDNSType incorrect "
						"value\r\n");
				ret = FAILURE;
			}
		}
		break;
	case WIFIDIRECT_DISC_BONJOUR_VERSION:
		if (argc != 1) {
			wmprintf
			    ("ERR:Incorrect number of arguments for "
					"Version\r\n");
			ret = FAILURE;
		} else {
			if (ISDIGIT(argv[0]) == 0) {
				wmprintf("ERR:Version incorrect value\r\n");
				ret = FAILURE;
			}
		}
		break;
	case WIFIDIRECT_DISC_UPNP_VERSION:
		if (argc != 1) {
			wmprintf
			    ("ERR:Incorrect number of arguments for "
					"Version\r\n");
			ret = FAILURE;
		}
		break;
	case WIFIDIRECT_ENABLE_SCAN:
		if (argc != 1) {
			wmprintf
			    ("ERR:Incorrect number of arguments for "
				"EnableScan\r\n");
			ret = FAILURE;
		} else {
			if ((ISDIGIT(argv[0]) == 0) || ((atoi(argv[0]) != 0) &&
							(atoi(argv[0]) != 1))) {
				wmprintf("ERR:EnableScan must be 0 or 1.\r\n");
				ret = FAILURE;
			}
		}
		break;

	case WIFIDIRECT_DEVICE_STATE:
		if (argc != 1) {
			wmprintf
			    ("ERR:Incorrect number of arguments for "
				"DeviceState\r\n");
			ret = FAILURE;
		} else {
			if (ISDIGIT(argv[0]) == 0) {
				wmprintf("ERR:Incorrect DeviceState.\r\n");
				ret = FAILURE;
			}
		}
		break;

	default:
		wmprintf("Parameter validity for %d ignored\r\n", cmd);
		break;
	}
	return ret;
}

/**
 *  @brief Read the wifidirect parameters and sends to the driver
 *
 *  @param cmd_name  Command Name for which parameters are read.
 *  @param pbuf      Pointer to output buffer
 *  @param ie_len_wifidirect Length of wifidirect parameters to return
 *  @param ie_len_wps Length of WPS parameters to return
 *  @return          SUCCESS or FAILURE
 */
static void
wifidirect_file_params_config(char *cmd_name, t_u8 *pbuf,
			      t_u16 *ie_len_wifidirect, t_u16 *ie_len_wps)
{
	char *line = NULL;
	int i = 0, li = 0, arg_num = 0, ret = 0, wifidirect_level =
	    0, no_of_chan_entries = 0, no_of_noa = 0;
	int secondary_index = -1, flag = 1, group_secondary_index = -1;
	char **args = NULL;
	char *pos = NULL;
	char wifidirect_mac[20], country[4], wifidirect_ssid[33];
	char WPS_manufacturer[33], WPS_modelname[33], WPS_devicename[33],
	    wifi_group_direct_ssid[33];
	t_u8 dev_channels[MAX_CHANNELS];
	t_u8 iface_list[MLAN_MAC_ADDR_LENGTH * MAX_INTERFACE_ADDR_COUNT];
	t_u8 dev_address[MLAN_MAC_ADDR_LENGTH];
	t_u8 group_dev_address[MLAN_MAC_ADDR_LENGTH];
	t_u8 device_mac_addr[MLAN_MAC_ADDR_LENGTH];
	t_u8 *extra = NULL;
	t_u8 *buffer = pbuf;
	t_u16 cmd_len_wifidirect = 0, cmd_len_wps = 0, tlv_len = 0, extra_len =
	    0, temp = 0;
	t_u16 wps_model_len = 0, wps_serial_len = 0;
	t_u16 pri_category = 0, pri_sub_category = 0, config_methods = 0;
	t_u16 sec_category = 0, sec_sub_category = 0, group_sec_sub_category =
	    0, group_sec_category = 0;
	t_u16 avail_period = 0, avail_interval = 0;
	t_u8 secondary_oui[4], group_secondary_oui[4];
	t_u16 WPS_specconfigmethods = 0, WPS_associationstate = 0,
	    WPS_configurationerror = 0, WPS_devicepassword = 0;
	t_u8 dev_capability = 0, group_capability = 0, cmd_found = 0,
	    group_owner_intent = 0, primary_oui[4], iface_count = 0,
	    regulatory_class = 0, channel_number = 0, manageability = 0,
	    op_regulatory_class = 0, invitation_flag = 0;
	t_u8 WPS_version = 0, WPS_setupstate = 0, WPS_requesttype =
	    0, WPS_responsetype =
	    0, WPS_UUID[WPS_UUID_MAX_LEN],
	    WPS_primarydevicetype[WPS_DEVICE_TYPE_MAX_LEN], WPS_RFband =
	    0, WPS_modelnumber[4], WPS_serialnumber[4];
	t_u8 go_config_timeout = 0, client_config_timeout = 0;
	t_u8 secondary_dev_count = 0, group_secondary_dev_count = 0;
	t_u16 temp16 = 0;
	t_u8 secondary_dev_info[WPS_DEVICE_TYPE_LEN *
				MAX_SECONDARY_DEVICE_COUNT];
	t_u8 group_secondary_dev_info[WPS_DEVICE_TYPE_LEN *
				      MAX_GROUP_SECONDARY_DEVICE_COUNT];
	t_u8 wifidirect_client_dev_count = 0, wifidirect_client_dev_index =
	    0, temp8 = 0;
	wifidirect_client_dev_info
	    wifidirect_client_dev_info_list[MAX_SECONDARY_DEVICE_COUNT];
/*    t_u8 wifidirect_secondary_dev_count = 0;*/
	t_u8 wifidirect_total_secondary_dev_count = 0;
	t_u8 wifidirect_group_total_ssid_len = 0, tlv_offset =
	    0, temp_dev_size = 0;
	t_u8 noa_index = 0, opp_ps = 0, ctwindow_opp_ps = 0, count_type = 0;
	t_u32 duration = 0, interval = 0, start_time = 0;
	t_u16 total_chan_len = 0;
	t_u8 chan_entry_regulatory_class = 0, chan_entry_num_of_channels = 0;
	t_u8 *chan_entry_list = NULL;
	t_u8 *chan_buf = NULL;
	noa_descriptor noa_descriptor_list[MAX_NOA_DESCRIPTORS];

	/* Memory allocations */
	line = os_mem_alloc(MAX_CONFIG_LINE);
	if (!line) {
		wmprintf("ERR:Cannot allocate memory for line\r\n");
		goto done;
	}
	memset(line, 0, MAX_CONFIG_LINE);

	extra = os_mem_alloc(MAX_CONFIG_LINE);
	if (!extra) {
		wmprintf("ERR:Cannot allocate memory for extra\r\n");
		goto done;
	}
	memset(extra, 0, MAX_CONFIG_LINE);

	args = (char **)os_mem_alloc(sizeof(char *) * MAX_ARGS_NUM);
	if (!args) {
		wmprintf("ERR:Cannot allocate memory for args\r\n");
		goto done;
	}
	memset(args, 0, (sizeof(char *) * MAX_ARGS_NUM));

	memcpy(buffer, wifidir_oui_type, sizeof(wifidir_oui_type));
	cmd_len_wifidirect += sizeof(wifidir_oui_type);

	/* Parse array and process */
	while (config_get_line
	       (line, MAX_CONFIG_LINE, "wifidirect_config", &li, &pos)) {
#if 0
		arg_num = parse_line(line, args);
		wmprintf("%s \r\r\n", line);
	}
#endif
#if 1
	arg_num = parse_line(line, args);
	if (!cmd_found && strncmp(args[0], cmd_name, strlen(args[0])))
		continue;
	cmd_found = 1;
	if (strcmp(args[0], "wifidirect_config") == 0) {
		wifidirect_level = WIFIDIRECT_PARAMS_CONFIG;
	} else if (strcmp(args[0], "Capability") == 0) {
		wifidirect_level = WIFIDIRECT_CAPABILITY_CONFIG;
	} else if (strcmp(args[0], "GroupOwnerIntent") == 0) {
		wifidirect_level = WIFIDIRECT_GROUP_OWNER_INTENT_CONFIG;
	} else if (strcmp(args[0], "Channel") == 0) {
		wifidirect_level = WIFIDIRECT_CHANNEL_CONFIG;
	} else if (strcmp(args[0], "OperatingChannel") == 0) {
		wifidirect_level = WIFIDIRECT_OPCHANNEL_CONFIG;
	} else if (strcmp(args[0], "InfrastructureManageabilityInfo") == 0) {
		wifidirect_level = WIFIDIRECT_MANAGEABILITY_CONFIG;
	} else if (strcmp(args[0], "InvitationFlagBitmap") == 0) {
		wifidirect_level = WIFIDIRECT_INVITATION_FLAG_CONFIG;
	} else if (strcmp(args[0], "ChannelList") == 0) {
		wifidirect_level = WIFIDIRECT_CHANNEL_LIST_CONFIG;
	} else if (strcmp(args[0], "NoticeOfAbsence") == 0) {
		wifidirect_level = WIFIDIRECT_NOTICE_OF_ABSENCE;
	} else if (strcmp(args[0], "NoA_descriptor") == 0) {
		wifidirect_level = WIFIDIRECT_NOA_DESCRIPTOR;
	} else if (strcmp(args[0], "DeviceInfo") == 0) {
		wifidirect_level = WIFIDIRECT_DEVICE_INFO_CONFIG;
	} else if (strcmp(args[0], "SecondaryDeviceType") == 0) {
		wifidirect_level = WIFIDIRECT_DEVICE_SEC_INFO_CONFIG;
	} else if (strcmp(args[0], "GroupInfo") == 0) {
		wifidirect_level = WIFIDIRECT_GROUP_INFO_CONFIG;
	} else if (strcmp(args[0], "GroupSecondaryDeviceTypes") == 0) {
		wifidirect_level = WIFIDIRECT_GROUP_SEC_INFO_CONFIG;
	} else if (strcmp(args[0], "GroupWifiDirectDeviceTypes") == 0) {
		wifidirect_level = WIFIDIRECT_GROUP_CLIENT_INFO_CONFIG;
	} else if (strcmp(args[0], "GroupId") == 0) {
		wifidirect_level = WIFIDIRECT_GROUP_ID_CONFIG;
	} else if (strcmp(args[0], "GroupBSSId") == 0) {
		wifidirect_level = WIFIDIRECT_GROUP_BSS_ID_CONFIG;
	} else if (strcmp(args[0], "DeviceId") == 0) {
		wifidirect_level = WIFIDIRECT_DEVICE_ID_CONFIG;
	} else if (strcmp(args[0], "Interface") == 0) {
		wifidirect_level = WIFIDIRECT_INTERFACE_CONFIG;
	} else if (strcmp(args[0], "ConfigurationTimeout") == 0) {
		wifidirect_level = WIFIDIRECT_TIMEOUT_CONFIG;
	} else if (strcmp(args[0], "ExtendedListenTime") == 0) {
		wifidirect_level = WIFIDIRECT_EXTENDED_TIME_CONFIG;
	} else if (strcmp(args[0], "IntendedIntfAddress") == 0) {
		wifidirect_level = WIFIDIRECT_INTENDED_ADDR_CONFIG;
	} else if (strcmp(args[0], "WPSIE") == 0) {
		wifidirect_level = WIFIDIRECT_WPSIE;
	} else if (strcmp(args[0], "Extra") == 0) {
		wifidirect_level = WIFIDIRECT_EXTRA;
	} else if (strcmp(args[0], "WIFIDIRECT_MAC") == 0 ||
			strcmp(args[0], "GroupAddr") == 0 ||
			strcmp(args[0], "GroupBssId") == 0 ||
			strcmp(args[0], "InterfaceAddress") == 0 ||
			strcmp(args[0], "GroupInterfaceAddress") == 0 ||
			strcmp(args[0], "DeviceAddress") == 0) {
		strncpy(wifidirect_mac, args[1], 20 - 1);
		if ((ret = mac2raw(wifidirect_mac, dev_address)) != SUCCESS) {
			wmprintf("ERR: %s Address\r\n",
					ret == FAILURE ? "Invalid MAC" : ret ==
					WIFIDIRECT_RET_MAC_BROADCAST ?
					"Broadcast" :
					"Multicast");
			goto done;
		}
		if (is_mac_all_zero(dev_address) == SUCCESS) {
			/* If for some reason we start with interface
			* address, set it back to dev address */
			memcpy(device_mac_addr, dev_mac_addr,
			       MLAN_MAC_ADDR_LENGTH);
			device_mac_addr[0] |= INTERFACE_ADDR_FLIP_BIT;
			memcpy(dev_address, device_mac_addr,
			       MLAN_MAC_ADDR_LENGTH);
		}

	} else if (strncmp(args[0], "GroupWifiDirectDeviceAddress", 21) == 0) {
		strncpy(wifidirect_mac, args[1], 20 - 1);
		if ((ret =
		     mac2raw(wifidirect_mac, group_dev_address)) != SUCCESS) {
			wmprintf("ERR: %s Address\r\n",
				ret == FAILURE ? "Invalid MAC" : ret ==
				 WIFIDIRECT_RET_MAC_BROADCAST ? "Broadcast" :
				 "Multicast");
			goto done;
		}
		wifidirect_client_dev_index++;
		if (wifidirect_client_dev_index > wifidirect_client_dev_count) {
			wmprintf
			    ("ERR: No of Client Dev count is less "
				"than no of client dev configs!!\r\n");
			goto done;
		}
		group_secondary_index = 0;
/*            wifidirect_secondary_dev_count = 0;*/
		tlv_offset =
		    wifidirect_group_total_ssid_len +
		    wifidirect_total_secondary_dev_count * WPS_DEVICE_TYPE_LEN;
		memcpy(wifidirect_client_dev_info_list
		       [wifidirect_client_dev_index -
			1].wifidirect_dev_address + tlv_offset,
		       group_dev_address, MLAN_MAC_ADDR_LENGTH);
	} else if (strncmp(args[0], "GroupWifiDirectIntfAddress", 19) == 0) {
		strncpy(wifidirect_mac, args[1], 20 - 1);
		if ((ret =
		     mac2raw(wifidirect_mac, group_dev_address)) != SUCCESS) {
			wmprintf("ERR: %s Address\r\n",
				ret == FAILURE ? "Invalid MAC" : ret ==
				 WIFIDIRECT_RET_MAC_BROADCAST ? "Broadcast" :
				 "Multicast");
			goto done;
		}
		memcpy(wifidirect_client_dev_info_list
		       [wifidirect_client_dev_index -
			1].wifidirect_intf_address + tlv_offset,
		       group_dev_address, MLAN_MAC_ADDR_LENGTH);
	} else if (strncmp(args[0], "GroupWifiDirectDeviceCapab", 19) == 0) {
		if (is_input_valid
		    (WIFIDIRECT_DEVICECAPABILITY, arg_num - 1,
		     args + 1) != SUCCESS) {
			goto done;
		}
		temp8 = (t_u8) atoi(args[1]);
		memcpy(&wifidirect_client_dev_info_list
		       [wifidirect_client_dev_index -
			1].wifidirect_dev_capability + tlv_offset, &temp8,
		       sizeof(temp8));
	} else if (strncmp(args[0], "GroupWifiDirectWPSConfigMethods", 24)
			== 0) {
		if (is_input_valid
		    (WIFIDIRECT_WPSCONFMETHODS, arg_num - 1, args + 1)
		    != SUCCESS) {
			goto done;
		}
		temp16 = (t_u16) A2HEXDECIMAL(args[1]);
		memcpy(&wifidirect_client_dev_info_list
		       [wifidirect_client_dev_index - 1].config_methods +
		       tlv_offset, &temp16, sizeof(temp16));
		(t_u16) A2HEXDECIMAL(args[1]);
	} else if (strncmp(args[0], "GroupPrimaryDeviceTypeCategory", 30)
			== 0) {
		if (is_input_valid
		    (WIFIDIRECT_PRIDEVTYPECATEGORY, arg_num - 1,
		     args + 1) != SUCCESS) {
			goto done;
		}
		pri_category = (t_u16) atoi(args[1]);
		temp16 = htons(pri_category);
		memcpy(&wifidirect_client_dev_info_list
		       [wifidirect_client_dev_index - 1].primary_category +
		       tlv_offset, &temp16, sizeof(temp16));
	} else if (strncmp(args[0], "GroupPrimaryDeviceTypeOUI", 25) == 0) {
		if (is_input_valid
		    (WIFIDIRECT_PRIDEVTYPEOUI, arg_num - 1, args + 1)
		    != SUCCESS) {
			goto done;
		}
		for (i = 0; i < 4; i++) {
			temp8 = (t_u8) A2HEXDECIMAL(args[i + 1]);
			memcpy(&wifidirect_client_dev_info_list
			       [wifidirect_client_dev_index - 1].primary_oui[i]
			       + tlv_offset, &temp8, sizeof(temp8));
		}
	} else if (strncmp(args[0], "GroupPrimaryDeviceTypeSubCategory", 33) ==
		   0) {
		if (is_input_valid
		    (WIFIDIRECT_PRIDEVTYPESUBCATEGORY, arg_num - 1,
		     args + 1) != SUCCESS) {
			goto done;
		}
		pri_sub_category = (t_u16) atoi(args[1]);
		temp16 = htons(pri_sub_category);
		memcpy(&wifidirect_client_dev_info_list
		       [wifidirect_client_dev_index - 1].primary_subcategory +
		       tlv_offset, &temp16, sizeof(temp16));
	} else if (strncmp(args[0], "GroupSecondaryDeviceCount", 25) == 0) {
		if (is_input_valid
		    (WIFIDIRECT_GROUP_SECONDARYDEVCOUNT, arg_num - 1,
		     args + 1) != SUCCESS) {
			goto done;
		}
		group_secondary_dev_count = (t_u8) atoi(args[1]);
		memcpy(&wifidirect_client_dev_info_list
		       [wifidirect_client_dev_index -
			1].wifidirect_secondary_dev_count + tlv_offset,
		       &group_secondary_dev_count,
		       sizeof(group_secondary_dev_count));
		wifidirect_total_secondary_dev_count +=
		    group_secondary_dev_count;
		if (group_secondary_dev_count)
			memset(group_secondary_dev_info, 0,
			       sizeof(group_secondary_dev_info));
	} else if (strncmp(args[0], "GroupSecondaryDeviceTypeCategory", 30) ==
		   0) {
		if (is_input_valid
		    (WIFIDIRECT_PRIDEVTYPECATEGORY, arg_num - 1,
		     args + 1) != SUCCESS) {
			goto done;
		}
		sec_category = (t_u16) atoi(args[1]);
		group_sec_category = cpu_to_le16(sec_category);
		group_secondary_index++;
	} else if (strncmp(args[0], "GroupSecondaryDeviceTypeOUI", 27) == 0) {
		if (is_input_valid
		    (WIFIDIRECT_PRIDEVTYPEOUI, arg_num - 1, args + 1)
		    != SUCCESS) {
			goto done;
		}
		for (i = 0; i < 4; i++)
			group_secondary_oui[i] =
			    (t_u8) A2HEXDECIMAL(args[i + 1]);
	} else if (strncmp(args[0], "GroupSecondaryDeviceTypeSubCategory", 35)
		   == 0) {
		if (is_input_valid
		    (WIFIDIRECT_PRIDEVTYPESUBCATEGORY, arg_num - 1,
		     args + 1) != SUCCESS) {
			goto done;
		}
		if (group_secondary_index < 0 ||
		    group_secondary_index >= MAX_SECONDARY_DEVICE_COUNT) {
			wmprintf("Error in configuration file %s:%d", cmd_name,
				 li);
			goto done;
		}
		sec_sub_category = (t_u16) atoi(args[1]);
		group_sec_sub_category = cpu_to_le16(group_sec_sub_category);
		if (group_secondary_dev_count) {
			memcpy(&group_secondary_dev_info
			       [(group_secondary_index -
				 1) * WPS_DEVICE_TYPE_LEN], &group_sec_category,
			       sizeof(t_u16));
			memcpy(&group_secondary_dev_info
			       [((group_secondary_index -
				  1) * WPS_DEVICE_TYPE_LEN) + 2],
			       group_secondary_oui, sizeof(secondary_oui));
			memcpy(&group_secondary_dev_info
			       [((group_secondary_index -
				  1) * WPS_DEVICE_TYPE_LEN) + 6],
			       &group_sec_sub_category, sizeof(t_u16));
		}

	} else if (strncmp(args[0], "GroupWifiDirectDeviceCount", 19) == 0) {
		if (is_input_valid
		    (WIFIDIRECT_SECONDARYDEVCOUNT, arg_num - 1,
		     args + 1) != SUCCESS) {
			goto done;
		}
		wifidirect_client_dev_count = (t_u8) atoi(args[1]);
	} else if (strncmp(args[0], "GroupWifiDirectDeviceName", 18) == 0) {

		if (is_input_valid
		    (WIFIDIRECT_GROUP_WIFIDIRECT_DEVICE_NAME, arg_num - 1,
		     args + 1) != SUCCESS) {
			goto done;
		}

		strncpy(wifi_group_direct_ssid, args[1] + 1,
			strlen(args[1]) - 2);
		wifi_group_direct_ssid[strlen(args[1]) - 2] = '\0';
		temp = htons(SC_Device_Name);
		memcpy(((t_u8 *) &
			wifidirect_client_dev_info_list
			[wifidirect_client_dev_index -
			 1].wifidirect_device_name_type + tlv_offset), &temp,
		       sizeof(temp));
		temp = htons(strlen(wifi_group_direct_ssid));
		memcpy(((t_u8 *) &
			wifidirect_client_dev_info_list
			[wifidirect_client_dev_index -
			 1].wifidirect_device_name_len + tlv_offset), &temp,
		       sizeof(temp));
		memset(((t_u8 *) &
			wifidirect_client_dev_info_list
			[wifidirect_client_dev_index -
			 1].wifidirect_device_name + tlv_offset), 0,
		       strlen(wifi_group_direct_ssid));
		memcpy(((t_u8 *) &
			wifidirect_client_dev_info_list
			[wifidirect_client_dev_index -
			 1].wifidirect_device_name + tlv_offset),
		       &wifi_group_direct_ssid, strlen(wifi_group_direct_ssid));
		wifidirect_group_total_ssid_len +=
		    strlen(wifi_group_direct_ssid);

		if (wifidirect_client_dev_index - 1) {
			temp_dev_size =
			    sizeof(wifidirect_client_dev_info) +
			    strlen(wifi_group_direct_ssid) +
			    group_secondary_dev_count * WPS_DEVICE_TYPE_LEN;
			memcpy(&wifidirect_client_dev_info_list
			       [wifidirect_client_dev_index - 1].dev_length +
			       (tlv_offset -
				(group_secondary_dev_count *
				 WPS_DEVICE_TYPE_LEN)), &temp_dev_size,
			       sizeof(temp_dev_size));
		} else {

			temp_dev_size =
			    sizeof(wifidirect_client_dev_info) +
			    strlen(wifi_group_direct_ssid) +
			    group_secondary_dev_count * WPS_DEVICE_TYPE_LEN;
			wifidirect_client_dev_info_list
			    [wifidirect_client_dev_index - 1].dev_length =
			    sizeof(wifidirect_client_dev_info) +
			    strlen(wifi_group_direct_ssid) +
			    group_secondary_dev_count * WPS_DEVICE_TYPE_LEN;
		}
	} else if (strcmp(args[0], "DeviceCapability") == 0) {
		if (is_input_valid
		    (WIFIDIRECT_DEVICECAPABILITY, arg_num - 1,
		     args + 1) != SUCCESS) {
			goto done;
		}
		dev_capability = (t_u8) atoi(args[1]);
	} else if (strcmp(args[0], "GroupCapability") == 0) {
		if (is_input_valid
		    (WIFIDIRECT_GROUPCAPABILITY, arg_num - 1,
		     args + 1) != SUCCESS) {
			goto done;
		}
		group_capability = (t_u8) atoi(args[1]);
	} else if (strcmp(args[0], "Intent") == 0) {
		/* Intent -> 0 - 15 */
		if (is_input_valid(WIFIDIRECT_INTENT, arg_num - 1, args + 1) !=
		    SUCCESS) {
			goto done;
		}
		group_owner_intent = (t_u8) atoi(args[1]);
	} else if (strcmp(args[0], "RegulatoryClass") == 0) {
		if (is_input_valid
		    (WIFIDIRECT_REGULATORYCLASS, arg_num - 1,
		     args + 1) != SUCCESS) {
			goto done;
		}
		regulatory_class = (t_u8) atoi(args[1]);
	} else if (strcmp(args[0], "ChannelNumber") == 0) {
		if (is_input_valid(CHANNEL, arg_num - 1, args + 1) != SUCCESS) {
			goto done;
		}
		channel_number = (t_u8) atoi(args[1]);
	} else if (strcmp(args[0], "OpRegulatoryClass") == 0) {
		if (is_input_valid
		    (WIFIDIRECT_REGULATORYCLASS, arg_num - 1,
		     args + 1) != SUCCESS) {
			goto done;
		}
		op_regulatory_class = (t_u8) atoi(args[1]);
	} else if (strcmp(args[0], "OpChannelNumber") == 0) {
		if (is_input_valid(CHANNEL, arg_num - 1, args + 1) != SUCCESS) {
			goto done;
		}
		op_channel_number = (t_u8) atoi(args[1]);
	} else if (strcmp(args[0], "Manageability") == 0) {
		if (is_input_valid
		    (WIFIDIRECT_MANAGEABILITY, arg_num - 1, args + 1)
		    != SUCCESS) {
			goto done;
		}
		manageability = (t_u8) atoi(args[1]);
	} else if (strcmp(args[0], "InvitationFlag") == 0) {
		if (is_input_valid
		    (WIFIDIRECT_INVITATIONFLAG, arg_num - 1, args + 1)
		    != SUCCESS) {
			goto done;
		}
		invitation_flag = (t_u8) atoi(args[1]);
	} else if (strcmp(args[0], "CountryString") == 0) {
		if (is_input_valid(WIFIDIRECT_COUNTRY, arg_num - 1, args + 1) !=
		    SUCCESS) {
			goto done;
		}
		strncpy(country, args[1] + 1, 3);
		country[strlen(args[1]) - 2] = '\0';
	} else if (strncmp(args[0], "Regulatory_Class_", 17) == 0) {
		if (is_input_valid
		    (WIFIDIRECT_REGULATORYCLASS, arg_num - 1,
		     args + 1) != SUCCESS) {
			goto done;
		}
		if (!no_of_chan_entries) {
			chan_entry_list =
			    (t_u8 *) os_mem_alloc(MAX_BUFFER_SIZE);
			if (!chan_entry_list) {
				wmprintf
				    ("ERR:cannot allocate memory for "
					"chan_entry_list!\r\n");
				goto done;
			}
			memset(chan_entry_list, 0, MAX_BUFFER_SIZE);
			chan_buf = chan_entry_list;
		}
		no_of_chan_entries++;
		chan_entry_regulatory_class = (t_u8) atoi(args[1]);
	} else if (strncmp(args[0], "NumofChannels", 13) == 0) {
		if (is_input_valid
		    (WIFIDIRECT_NO_OF_CHANNELS, arg_num - 1, args + 1)
		    != SUCCESS) {
			goto done;
		}
		chan_entry_num_of_channels = (t_u8) atoi(args[1]);
	} else if (strncmp(args[0], "ChanList", 8) == 0) {
		if (chan_entry_num_of_channels != (arg_num - 1)) {
			wmprintf
			    ("ERR:no of channels in ChanList and "
				"NumofChannels do not match!\r\n");
			goto done;
		}
		if (is_input_valid(SCANCHANNELS, arg_num - 1, args + 1) !=
		    SUCCESS) {
			goto done;
		}
		for (i = 0; i < chan_entry_num_of_channels; i++)
			dev_channels[i] = (t_u8) atoi(args[i + 1]);
		total_chan_len += chan_entry_num_of_channels;
		memcpy(chan_buf, &chan_entry_regulatory_class, sizeof(t_u8));
		memcpy(chan_buf + 1, &chan_entry_num_of_channels, sizeof(t_u8));
		memcpy(chan_buf + 2, dev_channels, chan_entry_num_of_channels);
		chan_buf +=
		    sizeof(t_u8) + sizeof(t_u8) + chan_entry_num_of_channels;
	} else if (strcmp(args[0], "NoA_Index") == 0) {
		if (is_input_valid(WIFIDIRECT_NOA_INDEX, arg_num - 1, args + 1)
		    != SUCCESS) {
			goto done;
		}
		noa_index = (t_u8) atoi(args[1]);
	} else if (strcmp(args[0], "OppPS") == 0) {
		if (is_input_valid(WIFIDIRECT_OPP_PS, arg_num - 1, args + 1) !=
		    SUCCESS) {
			goto done;
		}
		opp_ps = (t_u8) atoi(args[1]);
	} else if (strcmp(args[0], "CTWindow") == 0) {
		if (is_input_valid(WIFIDIRECT_CTWINDOW, arg_num - 1, args + 1)
		    != SUCCESS) {
			goto done;
		}
		if ((opp_ps != 0) && (atoi(args[1]) < 10)) {
			wmprintf
			    ("ERR: CTwindow should be greater than or equal "
				"to 10 if opp_ps is set!\r\n");
			goto done;
		}
		ctwindow_opp_ps = (t_u8) atoi(args[1]) | SET_OPP_PS(opp_ps);
	} else if (strncmp(args[0], "CountType", 9) == 0) {
		if (is_input_valid(WIFIDIRECT_COUNT_TYPE, arg_num - 1, args + 1)
		    != SUCCESS) {
			goto done;
		}
		no_of_noa++;
		if (no_of_noa > MAX_NOA_DESCRIPTORS) {
			wmprintf
			    ("Number of descriptors should not be greater "
				"than %d\r\n",
			     MAX_NOA_DESCRIPTORS);
			goto done;
		}
		count_type = (t_u8) atoi(args[1]);
		noa_descriptor_list[no_of_noa - 1].count_type = count_type;
	} else if (strncmp(args[0], "Duration", 8) == 0) {
		if (is_input_valid(WIFIDIRECT_DURATION, arg_num - 1, args + 1)
		    != SUCCESS) {
			goto done;
		}
		duration = (t_u32) atoi(args[1]);
		duration = cpu_to_le32(duration);
		noa_descriptor_list[no_of_noa - 1].duration = duration;
	} else if (strncmp(args[0], "Interval", 8) == 0) {
		if (is_input_valid(WIFIDIRECT_INTERVAL, arg_num - 1, args + 1)
		    != SUCCESS) {
			goto done;
		}
		interval = (t_u32) atoi(args[1]);
		interval = cpu_to_le32(interval);
		noa_descriptor_list[no_of_noa - 1].interval = interval;
	} else if (strncmp(args[0], "StartTime", 9) == 0) {
		if (is_input_valid(WIFIDIRECT_START_TIME, arg_num - 1, args + 1)
		    != SUCCESS) {
			goto done;
		}
		start_time = (t_u32) atoi(args[1]);
		start_time = cpu_to_le32(start_time);
		noa_descriptor_list[no_of_noa - 1].start_time = start_time;
	} else if (strcmp(args[0], "PrimaryDeviceTypeCategory") == 0) {
		if (is_input_valid
		    (WIFIDIRECT_PRIDEVTYPECATEGORY, arg_num - 1,
		     args + 1) != SUCCESS) {
			goto done;
		}
		pri_category = (t_u16) atoi(args[1]);
	} else if (strcmp(args[0], "PrimaryDeviceTypeOUI") == 0) {
		if (is_input_valid
		    (WIFIDIRECT_PRIDEVTYPEOUI, arg_num - 1, args + 1)
		    != SUCCESS) {
			goto done;
		}
		for (i = 0; i < 4; i++)
			primary_oui[i] = (t_u8) A2HEXDECIMAL(args[i + 1]);
	} else if (strcmp(args[0], "PrimaryDeviceTypeSubCategory") == 0) {
		if (is_input_valid
		    (WIFIDIRECT_PRIDEVTYPESUBCATEGORY, arg_num - 1,
		     args + 1) != SUCCESS) {
			goto done;
		}
		pri_sub_category = (t_u16) atoi(args[1]);
	} else if (strcmp(args[0], "SecondaryDeviceCount") == 0) {
		if (is_input_valid
		    (WIFIDIRECT_SECONDARYDEVCOUNT, arg_num - 1,
		     args + 1) != SUCCESS) {
			goto done;
		}
		secondary_dev_count = (t_u8) atoi(args[1]);
		if (secondary_dev_count)
			memset(secondary_dev_info, 0,
			       sizeof(secondary_dev_info));
	} else if (strncmp(args[0], "SecondaryDeviceTypeCategory", 27) == 0) {
		if (is_input_valid
		    (WIFIDIRECT_PRIDEVTYPECATEGORY, arg_num - 1,
		     args + 1) != SUCCESS) {
			goto done;
		}
		sec_category = (t_u16) atoi(args[1]);
		sec_category = htons(sec_category);
		secondary_index++;
	} else if (strncmp(args[0], "SecondaryDeviceTypeOUI", 22) == 0) {
		if (is_input_valid
		    (WIFIDIRECT_PRIDEVTYPEOUI, arg_num - 1, args + 1)
		    != SUCCESS) {
			goto done;
		}
		for (i = 0; i < 4; i++)
			secondary_oui[i] = (t_u8) A2HEXDECIMAL(args[i + 1]);
	} else if (strncmp(args[0], "SecondaryDeviceTypeSubCategory", 30)
			== 0) {
		if (is_input_valid
		    (WIFIDIRECT_PRIDEVTYPESUBCATEGORY, arg_num - 1,
		     args + 1) != SUCCESS) {
			goto done;
		}
		if (secondary_index < 0 ||
		    secondary_index >= MAX_SECONDARY_DEVICE_COUNT) {
			wmprintf("Error in configuration file %s:%d", cmd_name,
				 li);
			goto done;
		}
		sec_sub_category = (t_u16) atoi(args[1]);
		sec_sub_category = htons(sec_sub_category);
		if (secondary_dev_count) {
			memcpy(&secondary_dev_info
			       [secondary_index * WPS_DEVICE_TYPE_LEN],
			       &sec_category, sizeof(t_u16));
			memcpy(&secondary_dev_info
			       [(secondary_index * WPS_DEVICE_TYPE_LEN) + 2],
			       secondary_oui, sizeof(secondary_oui));
			memcpy(&secondary_dev_info
			       [(secondary_index * WPS_DEVICE_TYPE_LEN) + 6],
			       &sec_sub_category, sizeof(t_u16));
		}
	} else if (strcmp(args[0], "InterfaceAddressCount") == 0) {
		if (is_input_valid
		    (WIFIDIRECT_INTERFACECOUNT, arg_num - 1, args + 1)
		    != SUCCESS) {
			goto done;
		}
		iface_count = (t_u8) atoi(args[1]);
	} else if (strcmp(args[0], "InterfaceAddressList") == 0) {
		if (iface_count != (arg_num - 1)) {
			wmprintf("Incorrect address list for %d entries.\r\n",
				 iface_count);
			goto done;
		}
		for (i = 0; i < iface_count && i < MAX_INTERFACE_ADDR_COUNT;
		     i++) {
			if ((ret =
			     mac2raw(args[i + 1],
				     &iface_list[i * MLAN_MAC_ADDR_LENGTH]))
			    != SUCCESS) {
					wmprintf("ERR: %s Address\r\n",
					ret == FAILURE ? "Invalid MAC" : ret ==
					WIFIDIRECT_RET_MAC_BROADCAST ?
					 "Broadcast" : "Multicast");
				goto done;
			}
		}
	} else if (strcmp(args[0], "GroupConfigurationTimeout") == 0) {
		if (is_input_valid
		    (WIFIDIRECT_ATTR_CONFIG_TIMEOUT, arg_num - 1,
		     args + 1) != SUCCESS) {
			goto done;
		}
		go_config_timeout = (t_u8) atoi(args[1]);
	} else if (strcmp(args[0], "ClientConfigurationTimeout") == 0) {
		if (is_input_valid
		    (WIFIDIRECT_ATTR_CONFIG_TIMEOUT, arg_num - 1,
		     args + 1) != SUCCESS) {
			goto done;
		}
		client_config_timeout = (t_u8) atoi(args[1]);
	} else if (strcmp(args[0], "AvailabilityPeriod") == 0) {
		if (is_input_valid
		    (WIFIDIRECT_ATTR_EXTENDED_TIME, arg_num - 1,
		     args + 1) != SUCCESS) {
			goto done;
		}
		avail_period = (t_u16) atoi(args[1]);
	} else if (strcmp(args[0], "AvailabilityInterval") == 0) {
		if (is_input_valid
		    (WIFIDIRECT_ATTR_EXTENDED_TIME, arg_num - 1,
		     args + 1) != SUCCESS) {
			goto done;
		}
		avail_interval = (t_u16) atoi(args[1]);
	} else if (strcmp(args[0], "WPSConfigMethods") == 0) {
		if (is_input_valid
		    (WIFIDIRECT_WPSCONFMETHODS, arg_num - 1, args + 1)
		    != SUCCESS) {
			goto done;
		}
		config_methods = (t_u16) A2HEXDECIMAL(args[1]);
	} else if (strcmp(args[0], "DeviceName") == 0 ||
		   strcmp(args[0], "GroupSsId") == 0) {
		memset(wifidirect_ssid, 0, sizeof(wifidirect_ssid));
		strncpy(wifidirect_ssid, args[1], sizeof(wifidirect_ssid) - 1);
		if ((strcmp(args[0], "DeviceName") == 0) &&
		    ((strlen(args[1]) + 2) <  sizeof(wifidirect_ssid) - 1))
			snprintf(&wifidirect_ssid[strlen(args[1])],
				sizeof(wifidirect_ssid) - 1, "_%02X%02X",
				dev_mac_addr[4], dev_mac_addr[5]);
	} else if (strcmp(args[0], "WPSVersion") == 0) {
		if (is_input_valid(WIFIDIRECT_WPSVERSION, arg_num - 1, args + 1)
		    != SUCCESS) {
			goto done;
		}
		WPS_version = (t_u8) A2HEXDECIMAL(args[1]);
	} else if (strcmp(args[0], "WPSSetupState") == 0) {
		if (is_input_valid
		    (WIFIDIRECT_WPSSETUPSTATE, arg_num - 1, args + 1)
		    != SUCCESS) {
			goto done;
		}
		WPS_setupstate = (t_u8) A2HEXDECIMAL(args[1]);
	} else if (strcmp(args[0], "WPSDeviceName") == 0) {
		if (is_input_valid
		    (WIFIDIRECT_WPSDEVICENAME, arg_num - 1, args + 1)
		    != SUCCESS) {
			goto done;
		}
		memset(WPS_devicename, 0, sizeof(WPS_devicename));
		strncpy(WPS_devicename, args[1], sizeof(WPS_devicename));
		if ((strlen(args[1]) + 2) <  sizeof(WPS_devicename) - 1)
			snprintf(&WPS_devicename[strlen(args[1])],
				sizeof(WPS_devicename) - 1, "_%02X%02X",
				dev_mac_addr[4], dev_mac_addr[5]);

		strncpy(WFD_devicename, WPS_devicename, sizeof(WPS_devicename));

	} else if (strcmp(args[0], "WPSRequestType") == 0) {
		if (is_input_valid
		    (WIFIDIRECT_WPSREQRESPTYPE, arg_num - 1, args + 1)
		    != SUCCESS) {
			goto done;
		}
		WPS_requesttype = (t_u8) A2HEXDECIMAL(args[1]);
	} else if (strcmp(args[0], "WPSResponseType") == 0) {
		if (is_input_valid
		    (WIFIDIRECT_WPSREQRESPTYPE, arg_num - 1, args + 1)
		    != SUCCESS) {
			goto done;
		}
		WPS_responsetype = (t_u8) A2HEXDECIMAL(args[1]);
	} else if (strcmp(args[0], "WPSSpecConfigMethods") == 0) {
		if (is_input_valid
		    (WIFIDIRECT_WPSSPECCONFMETHODS, arg_num - 1,
		     args + 1) != SUCCESS) {
			goto done;
		}
		WPS_specconfigmethods = (t_u16) A2HEXDECIMAL(args[1]);
	} else if (strcmp(args[0], "WPSUUID") == 0) {
		if (is_input_valid(WIFIDIRECT_WPSUUID, arg_num - 1, args + 1) !=
		    SUCCESS) {
			goto done;
		}
		for (i = 0; i < WPS_UUID_MAX_LEN; i++)
			WPS_UUID[i] = (t_u8) A2HEXDECIMAL(args[i + 1]);
	} else if (strcmp(args[0], "WPSPrimaryDeviceType") == 0) {
		if (is_input_valid
		    (WIFIDIRECT_WPSPRIMARYDEVICETYPE, arg_num - 1,
		     args + 1) != SUCCESS) {
			goto done;
		}
		for (i = 0; i < WPS_DEVICE_TYPE_MAX_LEN; i++)
			WPS_primarydevicetype[i] =
			    (t_u8) A2HEXDECIMAL(args[i + 1]);
	} else if (strcmp(args[0], "WPSRFBand") == 0) {
		if (is_input_valid(WIFIDIRECT_WPSRFBAND, arg_num - 1, args + 1)
		    != SUCCESS) {
			goto done;
		}
		WPS_RFband = (t_u8) A2HEXDECIMAL(args[1]);
	} else if (strcmp(args[0], "WPSAssociationState") == 0) {
		if (is_input_valid
		    (WIFIDIRECT_WPSASSOCIATIONSTATE, arg_num - 1,
		     args + 1) != SUCCESS) {
			goto done;
		}
		WPS_associationstate = (t_u16) A2HEXDECIMAL(args[1]);
	} else if (strcmp(args[0], "WPSConfigurationError") == 0) {
		if (is_input_valid
		    (WIFIDIRECT_WPSCONFIGURATIONERROR, arg_num - 1,
		     args + 1) != SUCCESS) {
			goto done;
		}
		WPS_configurationerror = (t_u16) A2HEXDECIMAL(args[1]);
	} else if (strcmp(args[0], "WPSDevicePassword") == 0) {
		if (is_input_valid
		    (WIFIDIRECT_WPSDEVICEPASSWORD, arg_num - 1,
		     args + 1) != SUCCESS) {
			goto done;
		}
		WPS_devicepassword = (t_u16) A2HEXDECIMAL(args[1]);
	} else if (strcmp(args[0], "WPSManufacturer") == 0) {
		if (is_input_valid
		    (WIFIDIRECT_WPSMANUFACTURER, arg_num - 1,
		     args + 1) != SUCCESS) {
			goto done;
		}
		memset(WPS_manufacturer, 0, sizeof(WPS_manufacturer));
		strncpy(WPS_manufacturer, args[1], sizeof(WPS_manufacturer));
	} else if (strcmp(args[0], "WPSModelName") == 0) {
		if (is_input_valid
		    (WIFIDIRECT_WPSMODELNAME, arg_num - 1, args + 1)
		    != SUCCESS) {
			goto done;
		}
		memset(WPS_modelname, 0, sizeof(WPS_modelname));
		strncpy(WPS_modelname, args[1], sizeof(WPS_modelname));
	} else if (strcmp(args[0], "WPSModelNumber") == 0) {
		if (is_input_valid
		    (WIFIDIRECT_WPSMODELNUMBER, arg_num - 1, args + 1)
		    != SUCCESS) {
			goto done;
		}
		for (i = 0; i < arg_num - 1; i++)
			WPS_modelnumber[i] = (t_u8) A2HEXDECIMAL(args[i + 1]);
		wps_model_len = arg_num - 1;
	} else if (strcmp(args[0], "WPSSerialNumber") == 0) {
		if (is_input_valid
		    (WIFIDIRECT_WPSSERIALNUMBER, arg_num - 1,
		     args + 1) != SUCCESS) {
			goto done;
		}
		for (i = 0; i < arg_num - 1; i++)
			WPS_serialnumber[i] = (t_u8) A2HEXDECIMAL(args[i + 1]);
		wps_serial_len = arg_num - 1;
	} else if (strcmp(args[0], "Buffer") == 0) {
		for (i = 0; i < arg_num - 1; i++)
			extra[i] = (t_u8) A2HEXDECIMAL(args[i + 1]);
		extra_len = arg_num - 1;
	} else if (strcmp(args[0], "}") == 0) {
		/* Based on level, populate appropriate struct */
		switch (wifidirect_level) {
		case WIFIDIRECT_DEVICE_ID_CONFIG:
			{
				tlvbuf_wifidirect_device_id *tlv = NULL;
				/* Append a new TLV */
				tlv_len = sizeof(tlvbuf_wifidirect_device_id);
				tlv =
				    (tlvbuf_wifidirect_device_id *)
					(buffer + cmd_len_wifidirect);
				cmd_len_wifidirect += tlv_len;
				/* Set TLV fields */
				tlv->tag = TLV_TYPE_WIFIDIRECT_DEVICE_ID;
				tlv->length =
				    tlv_len - (sizeof(t_u8) + sizeof(t_u16));
				memcpy(tlv->dev_mac_address, dev_address,
				       MLAN_MAC_ADDR_LENGTH);
				endian_convert_tlv_wifidirect_header_out(tlv);
				break;
			}
		case WIFIDIRECT_CAPABILITY_CONFIG:
			{
				tlvbuf_wifidirect_capability *tlv = NULL;
				/* Append a new TLV */
				tlv_len = sizeof(tlvbuf_wifidirect_capability);
				tlv =
				    (tlvbuf_wifidirect_capability *)
					(buffer + cmd_len_wifidirect);
				cmd_len_wifidirect += tlv_len;
				/* Set TLV fields */
				tlv->tag = TLV_TYPE_WIFIDIRECT_CAPABILITY;
				tlv->length =
				    tlv_len - (sizeof(t_u8) + sizeof(t_u16));
				tlv->dev_capability = dev_capability;
				tlv->group_capability = group_capability;
				endian_convert_tlv_wifidirect_header_out(tlv);
				break;
			}
		case WIFIDIRECT_GROUP_OWNER_INTENT_CONFIG:
			{
				tlvbuf_wifidirect_group_owner_intent *tlv =
				    NULL;
				/* Append a new TLV */
				tlv_len =
				    sizeof
				    (tlvbuf_wifidirect_group_owner_intent);
				tlv =
				    (tlvbuf_wifidirect_group_owner_intent
				     *) (buffer + cmd_len_wifidirect);
				cmd_len_wifidirect += tlv_len;
				/* Set TLV fields */
				tlv->tag =
				    TLV_TYPE_WIFIDIRECT_GROUPOWNER_INTENT;
				tlv->length =
				    tlv_len - (sizeof(t_u8) + sizeof(t_u16));
				tlv->dev_intent = group_owner_intent;
				endian_convert_tlv_wifidirect_header_out(tlv);
				break;
			}
		case WIFIDIRECT_MANAGEABILITY_CONFIG:
			{
				tlvbuf_wifidirect_manageability *tlv = NULL;
				/* Append a new TLV */
				tlv_len =
				    sizeof(tlvbuf_wifidirect_manageability);
				tlv =
				    (tlvbuf_wifidirect_manageability *)
					(buffer + cmd_len_wifidirect);
				cmd_len_wifidirect += tlv_len;
				/* Set TLV fields */
				tlv->tag = TLV_TYPE_WIFIDIRECT_MANAGEABILITY;
				tlv->length =
				    tlv_len - (sizeof(t_u8) + sizeof(t_u16));
				tlv->manageability = manageability;
				endian_convert_tlv_wifidirect_header_out(tlv);
				break;
			}
		case WIFIDIRECT_INVITATION_FLAG_CONFIG:
			{
				tlvbuf_wifidirect_invitation_flag *tlv = NULL;
				/* Append a new TLV */
				tlv_len =
				    sizeof(tlvbuf_wifidirect_invitation_flag);
				tlv =
				    (tlvbuf_wifidirect_invitation_flag
				     *) (buffer + cmd_len_wifidirect);
				cmd_len_wifidirect += tlv_len;
				/* Set TLV fields */
				tlv->tag = TLV_TYPE_WIFIDIRECT_INVITATION_FLAG;
				tlv->length =
				    tlv_len - (sizeof(t_u8) + sizeof(t_u16));
				tlv->invitation_flag |= invitation_flag;
				endian_convert_tlv_wifidirect_header_out(tlv);
				break;
			}
		case WIFIDIRECT_CHANNEL_LIST_CONFIG:
			{
				tlvbuf_wifidirect_channel_list *tlv = NULL;
				/* Append a new TLV */
				tlv_len = sizeof(tlvbuf_wifidirect_channel_list)
				    + no_of_chan_entries * sizeof(chan_entry) +
				    total_chan_len;
				tlv =
				    (tlvbuf_wifidirect_channel_list *)
					(buffer + cmd_len_wifidirect);
				cmd_len_wifidirect += tlv_len;
				/* Set TLV fields */
				tlv->tag = TLV_TYPE_WIFIDIRECT_CHANNEL_LIST;
				tlv->length =
				    tlv_len - (sizeof(t_u8) + sizeof(t_u16));
				memcpy(tlv->country_string, country, 3);
				if (tlv->country_string[2] == 0)
					tlv->country_string[2] =
					    WIFIDIRECT_COUNTRY_LAST_BYTE;
				memcpy(tlv->wifidirect_chan_entry_list,
				       chan_entry_list, (tlv->length - 3));
				endian_convert_tlv_wifidirect_header_out(tlv);
				break;
			}
		case WIFIDIRECT_NOTICE_OF_ABSENCE:
			{
				tlvbuf_wifidirect_notice_of_absence *tlv = NULL;
				/* Append a new TLV */
				tlv_len =
				    sizeof(tlvbuf_wifidirect_notice_of_absence)
				    + no_of_noa * sizeof(noa_descriptor);
				tlv =
				    (tlvbuf_wifidirect_notice_of_absence
				     *) (buffer + cmd_len_wifidirect);
				cmd_len_wifidirect += tlv_len;
				/* Set TLV fields */
				tlv->tag =
				    TLV_TYPE_WIFIDIRECT_NOTICE_OF_ABSENCE;
				tlv->length =
				    tlv_len - (sizeof(t_u8) + sizeof(t_u16));
				tlv->noa_index = noa_index;
				tlv->ctwindow_opp_ps = ctwindow_opp_ps;
				memcpy(tlv->wifidirect_noa_descriptor_list,
				       noa_descriptor_list,
				       no_of_noa * sizeof(noa_descriptor));
				endian_convert_tlv_wifidirect_header_out(tlv);
				flag = 1;
				break;
			}
		case WIFIDIRECT_NOA_DESCRIPTOR:
			{
				wifidirect_level = WIFIDIRECT_NOTICE_OF_ABSENCE;
				flag = 0;
				break;
			}
		case WIFIDIRECT_CHANNEL_CONFIG:
			{
				tlvbuf_wifidirect_channel *tlv = NULL;
				/* Append a new TLV */
				tlv_len = sizeof(tlvbuf_wifidirect_channel);
				tlv =
				    (tlvbuf_wifidirect_channel *)
					(buffer + cmd_len_wifidirect);
				cmd_len_wifidirect += tlv_len;
				/* Set TLV fields */
				tlv->tag = TLV_TYPE_WIFIDIRECT_CHANNEL;
				tlv->length =
				    tlv_len - (sizeof(t_u8) + sizeof(t_u16));
				memcpy(tlv->country_string, country, 3);
				if (tlv->country_string[2] == 0)
					tlv->country_string[2] =
					    WIFIDIRECT_COUNTRY_LAST_BYTE;
				tlv->regulatory_class = regulatory_class;
				tlv->channel_number = channel_number;
				endian_convert_tlv_wifidirect_header_out(tlv);
				break;
			}
		case WIFIDIRECT_OPCHANNEL_CONFIG:
			{
				tlvbuf_wifidirect_channel *tlv = NULL;
				/* Append a new TLV */
				tlv_len = sizeof(tlvbuf_wifidirect_channel);
				tlv =
				    (tlvbuf_wifidirect_channel *)
					(buffer + cmd_len_wifidirect);
				cmd_len_wifidirect += tlv_len;
				/* Set TLV fields */
				tlv->tag = TLV_TYPE_WIFIDIRECT_OPCHANNEL;
				tlv->length =
				    tlv_len - (sizeof(t_u8) + sizeof(t_u16));
				memcpy(tlv->country_string, country, 3);
				if (tlv->country_string[2] == 0)
					tlv->country_string[2] =
					    WIFIDIRECT_COUNTRY_LAST_BYTE;
				tlv->regulatory_class = op_regulatory_class;
				tlv->channel_number = op_channel_number;
				endian_convert_tlv_wifidirect_header_out(tlv);
				break;
			}

		case WIFIDIRECT_DEVICE_INFO_CONFIG:
			{
				tlvbuf_wifidirect_device_info *tlv = NULL;
				/* Append a new TLV */
				tlv_len =
				    sizeof(tlvbuf_wifidirect_device_info) +
				    secondary_dev_count * WPS_DEVICE_TYPE_LEN +
				    strlen(wifidirect_ssid);
				tlv =
				    (tlvbuf_wifidirect_device_info *)
					(buffer + cmd_len_wifidirect);
				cmd_len_wifidirect += tlv_len;
				/* Set TLV fields */
				tlv->tag = TLV_TYPE_WIFIDIRECT_DEVICE_INFO;
				tlv->length =
				    tlv_len - (sizeof(t_u8) + sizeof(t_u16));
				memcpy(tlv->dev_address, dev_address,
				       MLAN_MAC_ADDR_LENGTH);
				tlv->config_methods = htons(config_methods);
				tlv->primary_category = htons(pri_category);
				memcpy(tlv->primary_oui, primary_oui, 4);
				tlv->primary_subcategory =
				    htons(pri_sub_category);
				tlv->secondary_dev_count = secondary_dev_count;
				endian_convert_tlv_wifidirect_header_out(tlv);
				/* Parameters within secondary_dev_info are
				already htons'ed */
				memcpy(tlv->secondary_dev_info,
				       secondary_dev_info,
				       secondary_dev_count *
				       WPS_DEVICE_TYPE_LEN);
				temp = htons(SC_Device_Name);
				memcpy(((t_u8 *) (&tlv->device_name_type)) +
				       secondary_dev_count *
				       WPS_DEVICE_TYPE_LEN, &temp,
				       sizeof(temp));
				temp = htons(strlen(wifidirect_ssid));
				memcpy(((t_u8 *) (&tlv->device_name_len)) +
				       secondary_dev_count *
				       WPS_DEVICE_TYPE_LEN, &temp,
				       sizeof(temp));
				memcpy(((t_u8 *) (&tlv->device_name)) +
				       secondary_dev_count *
				       WPS_DEVICE_TYPE_LEN, wifidirect_ssid,
				       strlen(wifidirect_ssid));
				flag = 1;
				break;
			}
		case WIFIDIRECT_GROUP_INFO_CONFIG:
			{
				tlvbuf_wifidirect_group_info *tlv = NULL;
				/* Append a new TLV */
				tlv_offset = wifidirect_group_total_ssid_len
				    +
				    wifidirect_total_secondary_dev_count *
				    WPS_DEVICE_TYPE_LEN;
				tlv_len = sizeof(tlvbuf_wifidirect_group_info)
				    +
				    wifidirect_client_dev_count *
				    sizeof(wifidirect_client_dev_info) +
				    tlv_offset;
				tlv =
				    (tlvbuf_wifidirect_group_info *)
					(buffer + cmd_len_wifidirect);
				cmd_len_wifidirect += tlv_len;
				/* Set TLV fields */
				tlv->tag = TLV_TYPE_WIFIDIRECT_GROUP_INFO;
				tlv->length = tlv_len;
				memcpy(tlv->wifidirect_client_dev_list,
				       wifidirect_client_dev_info_list,
				       wifidirect_client_dev_count *
				       sizeof(wifidirect_client_dev_info) +
				       tlv_offset);
				/* Parameters within secondary_dev_info are
				already htons'ed
				 wps_hexdump(DEBUG_WLAN,"Group Info Hexdump:",
				 (t_u8*)tlv, tlv_len);*/
				endian_convert_tlv_wifidirect_header_out(tlv);
				flag = 1;
				break;
			}
		case WIFIDIRECT_GROUP_SEC_INFO_CONFIG:
			{
				wifidirect_level =
				    WIFIDIRECT_GROUP_CLIENT_INFO_CONFIG;

				if (wifidirect_client_dev_index
				    && group_secondary_index) {
					memset(((t_u8 *) &
						wifidirect_client_dev_info_list
						[wifidirect_client_dev_index -
					 1].wifidirect_secondary_dev_info
						+ tlv_offset), 0,
					       group_secondary_index *
					       WPS_DEVICE_TYPE_LEN);
					memcpy(((t_u8 *) &
						wifidirect_client_dev_info_list
						[wifidirect_client_dev_index -
					 1].wifidirect_secondary_dev_info
						+ tlv_offset),
					       &group_secondary_dev_info,
					       group_secondary_index *
					       WPS_DEVICE_TYPE_LEN);
				}
				tlv_offset = wifidirect_group_total_ssid_len
				    +
				    wifidirect_total_secondary_dev_count *
				    WPS_DEVICE_TYPE_LEN;
				flag = 0;
				break;
			}
		case WIFIDIRECT_GROUP_CLIENT_INFO_CONFIG:
			{
				wifidirect_level = WIFIDIRECT_GROUP_INFO_CONFIG;
				flag = 0;
				break;
			}
		case WIFIDIRECT_DEVICE_SEC_INFO_CONFIG:
			{
				wifidirect_level =
				    WIFIDIRECT_DEVICE_INFO_CONFIG;
				flag = 0;
				break;
			}
		case WIFIDIRECT_GROUP_ID_CONFIG:
			{
				tlvbuf_wifidirect_group_id *tlv = NULL;
				/* Append a new TLV */
				tlv_len =
				    sizeof(tlvbuf_wifidirect_group_id) +
				    strlen(wifidirect_ssid);
				tlv =
				    (tlvbuf_wifidirect_group_id *)
					(buffer + cmd_len_wifidirect);
				cmd_len_wifidirect += tlv_len;
				/* Set TLV fields */
				tlv->tag = TLV_TYPE_WIFIDIRECT_GROUP_ID;
				tlv->length =
				    tlv_len - (sizeof(t_u8) + sizeof(t_u16));
				memcpy(tlv->group_address, dev_address,
				       MLAN_MAC_ADDR_LENGTH);
				memcpy(tlv->group_ssid, wifidirect_ssid,
				       strlen(wifidirect_ssid));
				endian_convert_tlv_wifidirect_header_out(tlv);
				break;
			}
		case WIFIDIRECT_GROUP_BSS_ID_CONFIG:
			{
				tlvbuf_wifidirect_group_bss_id *tlv = NULL;
				/* Append a new TLV */
				tlv_len =
				    sizeof(tlvbuf_wifidirect_group_bss_id);
				tlv =
				    (tlvbuf_wifidirect_group_bss_id *)
					(buffer + cmd_len_wifidirect);
				cmd_len_wifidirect += tlv_len;
				/* Set TLV fields */
				tlv->tag = TLV_TYPE_WIFIDIRECT_GROUP_BSS_ID;
				tlv->length =
				    tlv_len - (sizeof(t_u8) + sizeof(t_u16));
				memcpy(tlv->group_bssid, dev_address,
				       MLAN_MAC_ADDR_LENGTH);
				endian_convert_tlv_wifidirect_header_out(tlv);
				break;
			}
		case WIFIDIRECT_INTERFACE_CONFIG:
			{
				tlvbuf_wifidirect_interface *tlv = NULL;
				/* Append a new TLV */
				tlv_len = sizeof(tlvbuf_wifidirect_interface)
				    + iface_count * MLAN_MAC_ADDR_LENGTH;
				tlv =
				    (tlvbuf_wifidirect_interface *)
					(buffer + cmd_len_wifidirect);
				cmd_len_wifidirect += tlv_len;
				/* Set TLV fields */
				tlv->tag = TLV_TYPE_WIFIDIRECT_INTERFACE;
				tlv->length =
				    tlv_len - (sizeof(t_u8) + sizeof(t_u16));
				memcpy(tlv->interface_id, dev_address,
				       MLAN_MAC_ADDR_LENGTH);
				tlv->interface_count = iface_count;
				memcpy(tlv->interface_idlist, iface_list,
				       iface_count * MLAN_MAC_ADDR_LENGTH);
				endian_convert_tlv_wifidirect_header_out(tlv);
				break;
			}

		case WIFIDIRECT_TIMEOUT_CONFIG:
			{
				tlvbuf_wifidirect_config_timeout *tlv = NULL;
				/* Append a new TLV */
				tlv_len =
				    sizeof(tlvbuf_wifidirect_config_timeout);
				tlv =
				    (tlvbuf_wifidirect_config_timeout *)
					(buffer + cmd_len_wifidirect);
				cmd_len_wifidirect += tlv_len;
				/* Set TLV fields */
				tlv->tag = TLV_TYPE_WIFIDIRECT_CONFIG_TIMEOUT;
				tlv->length =
				    tlv_len - (sizeof(t_u8) + sizeof(t_u16));
				tlv->group_config_timeout = go_config_timeout;
				tlv->device_config_timeout =
				    client_config_timeout;
				endian_convert_tlv_wifidirect_header_out(tlv);
				break;
			}
		case WIFIDIRECT_EXTENDED_TIME_CONFIG:
			{
				tlvbuf_wifidirect_ext_listen_time *tlv = NULL;
				/* Append a new TLV */
				tlv_len =
				    sizeof(tlvbuf_wifidirect_ext_listen_time);
				tlv =
				    (tlvbuf_wifidirect_ext_listen_time
				     *) (buffer + cmd_len_wifidirect);
				cmd_len_wifidirect += tlv_len;
				/* Set TLV fields */
				tlv->tag =
				    TLV_TYPE_WIFIDIRECT_EXTENDED_LISTEN_TIME;
				tlv->length =
				    tlv_len - (sizeof(t_u8) + sizeof(t_u16));
				tlv->availability_period =
				    le16_to_cpu(avail_period);
				tlv->availability_interval =
				    le16_to_cpu(avail_interval);
				endian_convert_tlv_wifidirect_header_out(tlv);
				break;
			}
		case WIFIDIRECT_INTENDED_ADDR_CONFIG:
			{
				tlvbuf_wifidirect_intended_addr *tlv = NULL;
				/* Append a new TLV */
				tlv_len =
				    sizeof(tlvbuf_wifidirect_intended_addr);
				tlv =
				    (tlvbuf_wifidirect_intended_addr *)
					(buffer + cmd_len_wifidirect);
				cmd_len_wifidirect += tlv_len;
				/* Set TLV fields */
				tlv->tag = TLV_TYPE_WIFIDIRECT_INTENDED_ADDRESS;
				tlv->length =
				    tlv_len - (sizeof(t_u8) + sizeof(t_u16));
				memcpy(tlv->group_address, dev_address,
				       MLAN_MAC_ADDR_LENGTH);
				endian_convert_tlv_wifidirect_header_out(tlv);
				break;
			}

		case WIFIDIRECT_WPSIE:
			{
#ifdef CONFIG_P2P_DEBUG
				/* Debug print */
				hexdump(buffer, cmd_len_wifidirect, ' ');
#endif
				/* Append TLV for WPSVersion */
				tlvbuf_wps_ie *tlv = NULL;
				tlv_len =
				    sizeof(tlvbuf_wps_ie) + sizeof(WPS_version);
				pos = (char *)(buffer + cmd_len_wifidirect +
					sizeof(custom_ie) + cmd_len_wps);
				memcpy(pos, wps_oui_type, sizeof(wps_oui_type));
				cmd_len_wps += sizeof(wps_oui_type);
				tlv =
				    (tlvbuf_wps_ie *) (buffer +
						       cmd_len_wifidirect +
						       sizeof(custom_ie) +
						       cmd_len_wps);
				tlv->tag = SC_Version;
				tlv->length = tlv_len - 2 * sizeof(t_u16);
				*(tlv->data) = WPS_version;
				endian_convert_tlv_wps_header_out(tlv);
				cmd_len_wps += tlv_len;

				/* Append TLV for WPSSetupState */
				tlv_len =
				    sizeof(tlvbuf_wps_ie) +
				    sizeof(WPS_setupstate);
				tlv =
				    (tlvbuf_wps_ie *) (buffer +
						       cmd_len_wifidirect +
						       sizeof(custom_ie) +
						       cmd_len_wps);
				tlv->tag = SC_Simple_Config_State;
				tlv->length = tlv_len - 2 * sizeof(t_u16);
				*(tlv->data) = WPS_setupstate;
				endian_convert_tlv_wps_header_out(tlv);
				cmd_len_wps += tlv_len;

				/* Append TLV for WPSRequestType */
				tlv_len =
				    sizeof(tlvbuf_wps_ie) +
				    sizeof(WPS_requesttype);
				tlv =
				    (tlvbuf_wps_ie *) (buffer +
						       cmd_len_wifidirect +
						       sizeof(custom_ie) +
						       cmd_len_wps);
				tlv->tag = SC_Request_Type;
				tlv->length = tlv_len - 2 * sizeof(t_u16);
				*(tlv->data) = WPS_requesttype;
				endian_convert_tlv_wps_header_out(tlv);
				cmd_len_wps += tlv_len;

				/* Append TLV for WPSResponseType */
				tlv_len =
				    sizeof(tlvbuf_wps_ie) +
				    sizeof(WPS_responsetype);
				tlv =
				    (tlvbuf_wps_ie *) (buffer +
						       cmd_len_wifidirect +
						       sizeof(custom_ie) +
						       cmd_len_wps);
				tlv->tag = SC_Response_Type;
				tlv->length = tlv_len - 2 * sizeof(t_u16);
				*(tlv->data) = WPS_responsetype;
				endian_convert_tlv_wps_header_out(tlv);
				cmd_len_wps += tlv_len;

				/* Append TLV for WPSSpecConfigMethods */
				tlv_len =
				    sizeof(tlvbuf_wps_ie) +
				    sizeof(WPS_specconfigmethods);
				tlv =
				    (tlvbuf_wps_ie *) (buffer +
						       cmd_len_wifidirect +
						       sizeof(custom_ie) +
						       cmd_len_wps);
				tlv->tag = SC_Config_Methods;
				tlv->length = tlv_len - 2 * sizeof(t_u16);
				temp = htons(WPS_specconfigmethods);
				memcpy((t_u16 *) tlv->data, &temp,
				       sizeof(temp));
				endian_convert_tlv_wps_header_out(tlv);
				cmd_len_wps += tlv_len;

				/* Append TLV for WPSUUID */
				tlv_len =
				    sizeof(tlvbuf_wps_ie) + sizeof(WPS_UUID);
				tlv =
				    (tlvbuf_wps_ie *) (buffer +
						       cmd_len_wifidirect +
						       sizeof(custom_ie) +
						       cmd_len_wps);
				tlv->tag = SC_UUID_E;
				tlv->length = tlv_len - 2 * sizeof(t_u16);
				memcpy(tlv->data, WPS_UUID, WPS_UUID_MAX_LEN);
				endian_convert_tlv_wps_header_out(tlv);
				cmd_len_wps += tlv_len;

				/* Append TLV for WPSPrimaryDeviceType */
				tlv_len =
				    sizeof(tlvbuf_wps_ie) +
				    sizeof(WPS_primarydevicetype);
				tlv =
				    (tlvbuf_wps_ie *) (buffer +
						       cmd_len_wifidirect +
						       sizeof(custom_ie) +
						       cmd_len_wps);
				tlv->tag = SC_Primary_Device_Type;
				tlv->length = tlv_len - 2 * sizeof(t_u16);
				memcpy(tlv->data, WPS_primarydevicetype,
				       WPS_DEVICE_TYPE_MAX_LEN);
				endian_convert_tlv_wps_header_out(tlv);
				cmd_len_wps += tlv_len;

				/* Append TLV for WPSRFBand */
				tlv_len =
				    sizeof(tlvbuf_wps_ie) + sizeof(WPS_RFband);
				tlv =
				    (tlvbuf_wps_ie *) (buffer +
						       cmd_len_wifidirect +
						       sizeof(custom_ie) +
						       cmd_len_wps);
				tlv->tag = SC_RF_Band;
				tlv->length = tlv_len - 2 * sizeof(t_u16);
				*(tlv->data) = WPS_RFband;
				endian_convert_tlv_wps_header_out(tlv);
				cmd_len_wps += tlv_len;

				/* Append TLV for WPSAssociationState */
				tlv_len =
				    sizeof(tlvbuf_wps_ie) +
				    sizeof(WPS_associationstate);
				tlv =
				    (tlvbuf_wps_ie *) (buffer +
						       cmd_len_wifidirect +
						       sizeof(custom_ie) +
						       cmd_len_wps);
				tlv->tag = SC_Association_State;
				tlv->length = tlv_len - 2 * sizeof(t_u16);
				temp = htons(WPS_associationstate);
				memcpy((t_u16 *) tlv->data, &temp,
				       sizeof(temp));
				endian_convert_tlv_wps_header_out(tlv);
				cmd_len_wps += tlv_len;

				/* Append TLV for WPSConfigurationError */
				tlv_len =
				    sizeof(tlvbuf_wps_ie) +
				    sizeof(WPS_configurationerror);
				tlv =
				    (tlvbuf_wps_ie *) (buffer +
						       cmd_len_wifidirect +
						       sizeof(custom_ie) +
						       cmd_len_wps);
				tlv->tag = SC_Configuration_Error;
				tlv->length = tlv_len - 2 * sizeof(t_u16);
				temp = htons(WPS_configurationerror);
				memcpy((t_u16 *) tlv->data, &temp,
				       sizeof(temp));
				endian_convert_tlv_wps_header_out(tlv);
				cmd_len_wps += tlv_len;

				/* Append TLV for WPSDevicePassword */
				tlv_len =
				    sizeof(tlvbuf_wps_ie) +
				    sizeof(WPS_devicepassword);
				tlv =
				    (tlvbuf_wps_ie *) (buffer +
						       cmd_len_wifidirect +
						       sizeof(custom_ie) +
						       cmd_len_wps);
				tlv->tag = SC_Device_Password_ID;
				tlv->length = tlv_len - 2 * sizeof(t_u16);
				temp = htons(WPS_devicepassword);
				memcpy((t_u16 *) tlv->data, &temp,
				       sizeof(temp));
				endian_convert_tlv_wps_header_out(tlv);
				cmd_len_wps += tlv_len;

				/* Append TLV for WPSDeviceName */
				tlv_len =
				    sizeof(tlvbuf_wps_ie) +
				    strlen(WPS_devicename);
				tlv =
				    (tlvbuf_wps_ie *) (buffer +
						       cmd_len_wifidirect +
						       sizeof(custom_ie) +
						       cmd_len_wps);
				tlv->tag = SC_Device_Name;
				tlv->length = tlv_len - 2 * sizeof(t_u16);
				memcpy(tlv->data, WPS_devicename,
				       strlen(WPS_devicename));
				endian_convert_tlv_wps_header_out(tlv);
				cmd_len_wps += tlv_len;

				/* Append TLV for WPSManufacturer */
				tlv_len =
				    sizeof(tlvbuf_wps_ie) +
				    strlen(WPS_manufacturer);
				tlv =
				    (tlvbuf_wps_ie *) (buffer +
						       cmd_len_wifidirect +
						       sizeof(custom_ie) +
						       cmd_len_wps);
				tlv->tag = SC_Manufacturer;
				tlv->length = tlv_len - 2 * sizeof(t_u16);
				memcpy(tlv->data, WPS_manufacturer,
				       strlen(WPS_manufacturer));
				endian_convert_tlv_wps_header_out(tlv);
				cmd_len_wps += tlv_len;

				/* Append TLV for WPSModelName */
				tlv_len =
				    sizeof(tlvbuf_wps_ie) +
				    strlen(WPS_modelname);
				tlv =
				    (tlvbuf_wps_ie *) (buffer +
						       cmd_len_wifidirect +
						       sizeof(custom_ie) +
						       cmd_len_wps);
				tlv->tag = SC_Model_Name;
				tlv->length = tlv_len - 2 * sizeof(t_u16);
				memcpy(tlv->data, WPS_modelname,
				       strlen(WPS_modelname));
				endian_convert_tlv_wps_header_out(tlv);
				cmd_len_wps += tlv_len;

				/* Append TLV for WPSModelNumber */
				tlv_len = sizeof(tlvbuf_wps_ie) + wps_model_len;
				tlv =
				    (tlvbuf_wps_ie *) (buffer +
						       cmd_len_wifidirect +
						       sizeof(custom_ie) +
						       cmd_len_wps);
				tlv->tag = SC_Model_Number;
				tlv->length = tlv_len - 2 * sizeof(t_u16);
				memcpy(tlv->data, WPS_modelnumber,
				       wps_model_len);
				endian_convert_tlv_wps_header_out(tlv);
				cmd_len_wps += tlv_len;

				/* Append TLV for WPSSerialNumber */
				tlv_len =
				    sizeof(tlvbuf_wps_ie) + wps_serial_len;
				tlv =
				    (tlvbuf_wps_ie *) (buffer +
						       cmd_len_wifidirect +
						       sizeof(custom_ie) +
						       cmd_len_wps);
				tlv->tag = SC_Serial_Number;
				tlv->length = tlv_len - 2 * sizeof(t_u16);
				memcpy(tlv->data, WPS_serialnumber,
				       wps_serial_len);
				endian_convert_tlv_wps_header_out(tlv);
				cmd_len_wps += tlv_len;
#ifdef CONFIG_P2P_DEBUG
				/* Debug print */
				hexdump(buffer + sizeof(custom_ie) +
					cmd_len_wifidirect, cmd_len_wps, ' ');
#endif
				break;
			}
		case WIFIDIRECT_EXTRA:
			{
				memcpy(buffer + cmd_len_wifidirect, extra,
				       extra_len);
				cmd_len_wifidirect += extra_len;
				break;
			}
		default:
			*ie_len_wifidirect = cmd_len_wifidirect;
			if (ie_len_wps)
				*ie_len_wps = cmd_len_wps;
			break;
		}
		memset(country, 0, sizeof(country));
		if (wifidirect_level == 0)
			cmd_found = 0;
		if (flag)
			wifidirect_level = 0;
	}
}

#endif
done:
if (chan_entry_list)
	os_mem_free(chan_entry_list);
if (line)
	os_mem_free(line);
if (extra)
	os_mem_free(extra);
if (args)
	os_mem_free(args);
return;
}

/**
 *  @brief Process and send ie config command
 *  @param ie_index  A pointer to the IE buffer index
 *  @param data_len_wifidirect  Length of WIFIDIRECT data, 0 to get, else set.
 *  @param data_len_wps  Length of WPS data, 0 to get, else set.
 *  @param buf      Pointer to buffer to set.
 *  @return          SUCCESS--success, FAILURE--fail
 */
static int
wifidirect_ie_config(t_u16 *ie_index, t_u16 data_len_wifidirect,
		     t_u16 data_len_wps, t_u8 *buf)
{
	int ret = SUCCESS;
	tlvbuf_custom_ie *tlv = NULL;
	custom_ie *ie_ptr = NULL;
	wifidirectcmdbuf *cmd_buf;
	uint8_t *buffer;
	int cmd_len;

	tlv = (tlvbuf_custom_ie *)
		(buf + sizeof(wifidirectcmdbuf) - BUF_HEADER_SIZE);

	tlv->tag = MRVL_MGMT_IE_LIST_TLV_ID;
	/* Locate headers */
	ie_ptr = (custom_ie *) (tlv->ie_data);

	/* Set TLV fields : WIFIDIRECT IE parameters */
	if (data_len_wifidirect) {
		/* Set IE */
#define MGMT_MASK_AUTO	0xffff
		ie_ptr->mgmt_subtype_mask = MGMT_MASK_AUTO;
		tlv->length = sizeof(custom_ie) + data_len_wifidirect;
		ie_ptr->ie_length = data_len_wifidirect;
		ie_ptr->ie_index = *ie_index;
	} else {
		/* Get WPS IE */
		tlv->length = 0;
	}

	(*ie_index)++;

	/* Locate headers */
	ie_ptr =
	    (custom_ie *) ((t_u8 *) (tlv->ie_data) + sizeof(custom_ie) +
			   data_len_wifidirect);

	/* Set WPS IE parameters */
	if (data_len_wps) {
		/* Set IE */
		/* Firmware Handled IE - mask should be set to -1 */
		ie_ptr->mgmt_subtype_mask = MGMT_MASK_AUTO;
		tlv->length += sizeof(custom_ie) + data_len_wps;
		ie_ptr->ie_length = data_len_wps;
		ie_ptr->ie_index = *ie_index;
	}

	/* send hostcmd now */
	buffer = (uint8_t *) os_mem_alloc(MRVDRV_SIZE_OF_CMD_BUFFER);
	if (!buffer) {
		wmprintf("ERR:Cannot allocate memory!\r\n");
		return FAILURE;
	}

	memset(buffer, 0, MRVDRV_SIZE_OF_CMD_BUFFER);

	cmd_len = tlv->length + sizeof(wifidirectcmdbuf) +
				sizeof(tlvbuf_custom_ie);

	cmd_buf = (wifidirectcmdbuf *) buffer;
	cmd_buf->cmd_code = HostCmd_CMD_MGMT_IE_LIST;
	cmd_buf->size = cmd_len;
	cmd_buf->seq_num = (0x01) << 13;
	cmd_buf->result = 0;
	cmd_buf->action = ACTION_SET;

	memcpy(buffer + sizeof(wifidirectcmdbuf),
		buf + sizeof(wifidirectcmdbuf) - BUF_HEADER_SIZE, cmd_len);

	ret = wfd_ioctl(buffer, cmd_len);

	if (ret != WM_SUCCESS)
		ret = FAILURE;
	else
		ret = SUCCESS;

	os_mem_free(buffer);
	return ret;
}

void wifidirectcmd_config()
{
	t_u8 *buf = NULL;
/*      t_u8 *ptr, *dev_ptr, *array_ptr;*/
	t_u16 ie_len_wifidirect = 0, ie_len_wps = 0, ie_len;
	t_u16 ie_index = 0;
	int ret = SUCCESS;
/*      tlvbuf_custom_ie *tlv = NULL;
//      custom_ie *ie_ptr = NULL;
//      t_u16 len = 0, len_wifidirect = 0;
//      t_u8 type = 0;
//      t_u16 wps_len = 0, wps_type = 0;*/

	buf = os_mem_alloc(MRVDRV_SIZE_OF_CMD_BUFFER);
	if (!buf) {
		wmprintf("mem alloc failed\r\n");
		return;
	}

	/* Read parameters and send command to firmware */
	wifidirect_file_params_config("wifidirect_config", buf
				      + sizeof(HostCmd_DS_GEN) + 2
				      + sizeof(tlvbuf_custom_ie) +
				      sizeof(custom_ie), &ie_len_wifidirect,
				      &ie_len_wps);

	if (ie_len_wifidirect > MAX_SIZE_IE_BUFFER ||
	    ie_len_wps > MAX_SIZE_IE_BUFFER) {
		wmprintf("ERR:IE parameter size exceeds limit.\r\n");
		goto done;
	}
	ie_len =
	    ie_len_wifidirect + ie_len_wps + sizeof(tlvbuf_custom_ie) +
	    (2 * sizeof(custom_ie));
	if (ie_len >= MRVDRV_SIZE_OF_CMD_BUFFER) {
		wmprintf("ERR:Too much data in configuration file.\r\n");
		goto done;
	}

	ret =
	    wifidirect_ie_config(&ie_index, ie_len_wifidirect, ie_len_wps, buf);
	if (ret != SUCCESS) {
		wmprintf("ERR:Could not set wifidirect parameters\r\n");
	}

#ifdef CONFIG_P2P_DEBUG
	hexdump(buf, ie_len, ' ');
#endif
done:
	if (buf)
		os_mem_free(buf);
}
