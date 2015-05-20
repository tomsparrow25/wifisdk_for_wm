/*
 * This file contains functions which provide P2P related
 * functionality.
 */

#include <mlan_wmsdk.h>

/* Additional WMSDK header files */
#include <wmstdio.h>
#include <wmassert.h>
#include <wmerrno.h>
#include <wm_os.h>

#include <wifi.h>

#include "wifi-sdio.h"

/* Following is allocated in mlan_register */
extern mlan_adapter *mlan_adap;

int wifi_cmd_uap_config(char *ssid, t_u8 *mac_addr, t_u8 security,
			char *passphrase, t_u8 channel,
			t_u16 beacon_period, t_u8 dtim_period,
			int bss_type);
int wifi_uap_prepare_and_send_cmd(mlan_private *pmpriv,
			 t_u16 cmd_no,
			 t_u16 cmd_action,
			 t_u32 cmd_oid,
			 t_void *pioctl_buf, t_void *pdata_buf,
			 int bss_type, void *priv);

char *wifidirect_config[] = {
	"wifidirect_config={",
	"Capability={",
	"DeviceCapability=0",
	"GroupCapability=0",
	"}",
	"GroupOwnerIntent={",
	"Intent=0",		/*           # 0-15. 15-> highest GO desire */
	"}",
	"Channel={",		/*               # Listen channel attribute. */
	"CountryString=\"US\"",
	"RegulatoryClass=81",
	"ChannelNumber=6",
	"}",
	"InfrastructureManageabilityInfo={",
	"Manageability=0",
	"}",
	"ChannelList={",
	"CountryString=\"US\"",
	/*# multiple attributes channel entry list*/
	"Regulatory_Class_1=81",	/*              # Regulatory class*/
	"NumofChannels_1=11",	/*                 # No of channels*/
	"ChanList_1=1,2,3,4,5,6,7,8,9,10,11",	/* # Scan channel list */
#ifdef CONFIG_5GHz_SUPPORT
	"Regulatory_Class_2=115",	       /*      # Regulatory class*/
	"NumofChannels_2=4",	               /*   # No of channels*/
	"ChanList_2=36,40,44,48",	       /*      # Scan channel list*/
	"Regulatory_Class_3=118",            /*# Regulatory class*/
	"NumofChannels_3=4",                 /*# No of channels*/
	"ChanList_3=52,56,60,64",            /*# Scan channel list*/
	"Regulatory_Class_4=121",            /*# Regulatory class*/
	"NumofChannels_4=11",                /*# No of channels*/
	"ChanList_4=100,104,108,112,116,120,124,128,132,136,140",
#endif
/*# Scan channel list
# Enable only one of the country blocks at a time
#CountryString="JP"
# multiple attributes channel entry list
#Regulatory_Class_1=81              # Regulatory class
#NumofChannels_1=13                 # No of channels
#ChanList_1=1,2,3,4,5,6,7,8,9,10,11,12,13 # Scan channel list
#Regulatory_Class_2=115             # Regulatory class
#NumofChannels_2=4                  # No of channels
#ChanList_2=36,40,44,48             # Scan channel list
#Regulatory_Class_3=118            # Regulatory class
#NumofChannels_3=4                 # No of channels
#ChanList_3=52,56,60,64            # Scan channel list
#Regulatory_Class_4=121            # Regulatory class
#NumofChannels_4=11                # No of channels
#ChanList_4=100,104,108,112,116,120,124,128,132,136,140
# Scan channel list*/
	"}",
	"NoticeOfAbsence={",
	"NoA_Index=0",		/*                 # Instance of NoA timing*/
	"OppPS=1",		/*               # Opportunistic Power save*/
	"CTWindow=10",		/*                 # Client Traffic Window*/
	"NoA_descriptor={",
	"CountType_1=255",	/* # Count for GO mode OR Type for client mode*/
	"Duration_1=51200",	/*      # Max absence duration for GO mode OR*/
	/*       # min acceptable presence period for client mode*/
	"Interval_1=102400",
	"StartTime_1=0",
	/*      #CountType_2=1    # Count for GO mode OR Type for client mode*/
	/*      #Duration_2=0    # Max absence duration for GO mode OR*/
	/*      # min acceptable presence period for client mode*/
/*                      #Interval_2=0*/
/*                      #StartTime_2=0*/
	"}",
	"}",
	"DeviceInfo={",
	"DeviceAddress=00:00:00:00:00:00",
	/*# categ: 2 bytes, OUI: 4 bytes, subcateg: 2 bytes*/
	"PrimaryDeviceTypeCategory=1",
	"PrimaryDeviceTypeOUI=0x00,0x50,0xF2,0x04",
	"PrimaryDeviceTypeSubCategory=1",
	"SecondaryDeviceCount=2",
	"SecondaryDeviceType={",
	"SecondaryDeviceTypeCategory_1=6",
	"SecondaryDeviceTypeOUI_1=0x00,0x50,0xF2,0x04",
	"SecondaryDeviceTypeSubCategory_1=1",
	"SecondaryDeviceTypeCategory_2=4",
	"SecondaryDeviceTypeOUI_2=0x00,0x50,0xF2,0x04",
	"SecondaryDeviceTypeSubCategory_2=1",
	"}",
	"DeviceName=WMdroid",
/*              # ConfigMethods USB= 0x01
//              # ConfigMethods Ethernet= 0x02
//              # ConfigMethods Label= 0x04
//              # ConfigMethods Display= 0x08
//              # ConfigMethods Ext_NFC_Token= 0x10
//              # ConfigMethods Int_NFC_Token= 0x20
//              # ConfigMethods NFC_Interface= 0x40
//              # ConfigMethods PushButton= 0x80
//              # ConfigMethods KeyPad= 0x100*/
	"WPSConfigMethods=0x0080",
	"}",
	"GroupId={",
	"GroupAddr=00:00:00:00:00:00",
	"GroupSsId=DIRECT-",
	"}",
	"GroupBSSId={",
/*                # using LAA for interface address by default*/
	"GroupBssId=00:00:00:00:00:00",
	"}",
	"DeviceId={",
	"WIFIDIRECT_MAC=00:00:00:00:00:00",	/*# MAC address of wifidirect
							device in Hex*/
	"}",
	"Interface={",
/*                # using LAA for interface addresses by default*/
	"InterfaceAddress=00:00:00:00:00:00",
	"InterfaceAddressCount=2",
	"InterfaceAddressList=00:00:00:00:00:00,00:00:00:00:00:00",
	"}",
	"ConfigurationTimeout={",
/*        # units of 10 milliseconds*/
	"GroupConfigurationTimeout=250",
	"ClientConfigurationTimeout=100",
	"}",
	"ExtendedListenTime={",
/*        # units of milliseconds*/
	"AvailabilityPeriod=1000",
	"AvailabilityInterval=1500",
	"}",
	"IntendedIntfAddress={",
/*                # using LAA for interface address by default*/
	"GroupInterfaceAddress=00:00:00:00:00:00",
	"}",
	"OperatingChannel={",	/*            # Operating channel attribute.*/
	"CountryString=\"US\"",
	"OpRegulatoryClass=81",
	"OpChannelNumber=6",
	"}",
	"InvitationFlagBitmap={",
	"InvitationFlag=0",	/*           # bit0: Invitation type:*/
	"}",			/* # 0: request to reinvoke a persistent group*/
/*                           # 1: request to join an active WIFIDIRECT group*/
	"WPSIE={",
	"WPSVersion=0x10",
	"WPSSetupState=0x1",
	"WPSRequestType=0x0",
	"WPSResponseType=0x0",
	"WPSSpecConfigMethods=0x0080",
	"WPSUUID=0x12,0x34,0x56,0x78,0x12,0x34,0x56,0x78,0x12,0x34,0x56,0x78,"
		"0x12,0x34,0x56,0x78",
	"WPSPrimaryDeviceType=0x00,0x01,0x00,0x50,0xF2,0x04,0x00,0x01",
	"WPSRFBand=0x01",
	"WPSAssociationState=0x00",
	"WPSConfigurationError=0x00",
	"WPSDevicePassword=0x00",
	"WPSDeviceName=WMdroid",
	"WPSManufacturer=Marvell",
	"WPSModelName=SD-8787",
	"WPSModelNumber=0x00,0x00,0x00,0x01",
	"WPSSerialNumber=0x00,0x00,0x00,0x01",
	"}",
	"}",
	"XXEODXX",		/* End of DATA*/
};

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
char *
config_get_line(char *s, int size, char *stream, int *line, char **_pos)
{
	char *pos = NULL;
	if (strcmp(stream, "wifidirect_config") == 0) {
		if (strcmp(wifidirect_config[*line], "XXEODXX") == 0)
			pos = NULL;
		else {
			strncpy(s, wifidirect_config[*line], size);
#ifdef CONFIG_P2P_DEBUG
			wmprintf("Line: %s\r\n", s);
#endif
			(*line)++;
			pos = s;
		}
	}
	return pos;
}

int wifi_wfd_bss_sta_list(sta_list_t **list)
{
	mlan_private *pmpriv = (mlan_private *) mlan_adap->priv[0];

	/* Start BSS */
	return wifi_uap_prepare_and_send_cmd(pmpriv,
				    HOST_CMD_APCMD_STA_LIST,
				    HostCmd_ACT_GEN_SET, 0, NULL, NULL,
					     MLAN_BSS_TYPE_WIFIDIRECT, list);
}

int wifi_get_wfd_mac_address(void)
{
	mlan_private *pmpriv = (mlan_private *) mlan_adap->priv[0];
	wifi_prepare_and_send_cmd(pmpriv,
				  HostCmd_CMD_802_11_MAC_ADDRESS,
				  HostCmd_ACT_GEN_GET, 0, NULL,
				  NULL, MLAN_BSS_TYPE_WIFIDIRECT,
				  NULL);
	return WM_SUCCESS;
}

int wifi_set_wfd_mac_address(uint8_t *address)
{
	if (!address)
		return -WM_E_INVAL;

	mlan_private *pmpriv = (mlan_private *) mlan_adap->priv[0];
	wifi_prepare_and_send_cmd(pmpriv,
				  HostCmd_CMD_802_11_MAC_ADDRESS,
				  HostCmd_ACT_GEN_SET, 0, NULL,
				  address, MLAN_BSS_TYPE_WIFIDIRECT,
				  NULL);
	return WM_SUCCESS;
}
