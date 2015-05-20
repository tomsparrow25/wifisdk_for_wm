/** @file wlan_wfd.h
 *  @brief This file contains definitions for Marvell WLAN driver host command,
 *         for wfd specific commands.
 *
 *  Copyright (C) 2003-2010, Marvell International Ltd.
 *  All Rights Reserved
 */

#ifndef _WLAN_WFD_H_
#define _WLAN_WFD_H_

/*
 *  ctype from older glib installations defines BIG_ENDIAN.  Check it
 *   and undef it if necessary to correctly process the wlan header
 *   files
 */
#if (BYTE_ORDER == LITTLE_ENDIAN)
#undef BIG_ENDIAN
#endif
#ifdef BIG_ENDIAN

/** Convert TLV header to little endian format from CPU format */
#define endian_convert_tlv_header_out(x);           \
do {                                               \
	(x)->tag = wlan_cpu_to_le16((x)->tag);       \
	(x)->length = wlan_cpu_to_le16((x)->length); \
} while (0);

/** Convert TLV header from little endian format to CPU format */
#define endian_convert_tlv_header_in(x)             \
do {                                               \
	(x)->tag = wlan_le16_to_cpu((x)->tag);       \
	(x)->length = wlan_le16_to_cpu((x)->length); \
} while (0);

#else /* BIG_ENDIAN */

#define endian_convert_tlv_header_out(x)
/** Do nothing */
#define endian_convert_tlv_header_in(x)

#endif /* BIG_ENDIAN */

/** Event ID for generic wfd event */
#define EV_ID_WFD_GENERIC               0x02000049
/** Event ID for peer link lost */
#define EV_ID_WFD_PEER_LINK_LOST	0x02000003
/** Event ID for peer deauth */
#define EV_ID_WFD_PEER_DEAUTH		0x02000008
/** Event ID for peer disassoc */
#define EV_ID_WFD_PEER_DISASSOC		0x02000009
/** Event ID for peer connected */
#define EV_ID_WFD_PEER_CONNECTED	0x0200000A

/** Event type for peer negotiation request */
#define EV_TYPE_WFD_NEGOTIATION_REQUEST          0
/** Event type for peer negotiation response */
#define EV_TYPE_WFD_NEGOTIATION_RESPONSE         1
/** Event type for peer negotiation result */
#define EV_TYPE_WFD_NEGOTIATION_RESULT           2
/** Event type for Invitation Request Processing */
#define EV_TYPE_WFD_INVITATION_REQ               3
/** Event type for Invitation Response Processing */
#define EV_TYPE_WFD_INVITATION_RESP              4
/** Event type for provision discovery request */
#define EV_TYPE_WFD_PROVISION_DISCOVERY_REQ      7
/** Event type for provision discovery response */
#define EV_TYPE_WFD_PROVISION_DISCOVERY_RESP     8
/** Event type for peer detected */
#define EV_TYPE_WFD_PEER_DETECTED               14

/** Event subtype for GO Negotiation fail */
#define EV_SUBTYPE_WFD_GO_NEG_FAILED             0
/** Event subtype for GO role */
#define EV_SUBTYPE_WFD_GO_ROLE                   1
/** Event subtype for client role */
#define EV_SUBTYPE_WFD_CLIENT_ROLE               2

/** Event subtype for None */
#define EV_SUBTYPE_WFD_REQRECV_NONE                     0
/** Event subtype for Req Recv & Processing */
#define EV_SUBTYPE_WFD_REQRECV_PROCESSING               1
/** Event subtype for Req Dropped Insufficient Information */
#define EV_SUBTYPE_WFD_REQRECV_DROP_INSUFFICIENT_INFO   2
/** Event subtype for Req Recv & Processed successfully */
#define EV_SUBTYPE_WFD_REQRECV_PROCESSING_SUCCESS       3
/** Event subtype for Req Recv & Processing failed */
#define EV_SUBTYPE_WFD_REQRECV_PROCESSING_FAIL          4

/** Host Command ioctl number */
#define WFDHOSTCMD          (SIOCDEVPRIVATE + 1)
/** Private command ID to BSS control */
#define UAP_BSS_CTRL                (SIOCDEVPRIVATE + 4)
/** Private command ID for mode config */
#define WFD_MODE_CONFIG     (SIOCDEVPRIVATE + 5)
/** OUI Type WFD */
#define WFD_OUI_TYPE                    9
/** OUI Type WPS  */
#define WIFI_WPS_OUI_TYPE                       4

/** category: Public Action frame */
#define WIFI_CATEGORY_PUBLIC_ACTION_FRAME       4
/** Dialog Token : Ignore */
#define WIFI_DIALOG_TOKEN_IGNORE                0

/** WFD IE header len */
#define WFD_IE_HEADER_LEN                       3
/** IE header len */
#define IE_HEADER_LEN                           2

/** command to stop as wfd device. */
#define WFD_MODE_STOP                           0
/** command to start as wfd device. */
#define WFD_MODE_START                          1
/** command to start as group owner from host. */
#define WFD_MODE_START_GROUP_OWNER              2
/** command to start as device from host. */
#define WFD_MODE_START_DEVICE                   3
/** command to start find phase  */
#define WFD_MODE_START_FIND_PHASE               4
/** command to stop find phase  */
#define WFD_MODE_STOP_FIND_PHASE                5
/** command to get WFD mode   */
#define WFD_MODE_QUERY                  0xFF
/** command to get WFD GO mode   */
#define WFD_GO_QUERY                    0xFFFF

/** 4 byte header to store buf len*/
#define BUF_HEADER_SIZE                         4
/** Maximum length of lines in configuration file */
#define MAX_CONFIG_LINE                         1024
/** WFDCMD response check */
#define WFDCMD_RESP_CHECK                       0x8000
/** Host Command ID bit mask (bit 11:0) */
#define HostCmd_CMD_ID_MASK                      0x0fff
/** Host Command ID:  WFD_ACTION_FRAME */
#define HostCmd_CMD_802_11_ACTION_FRAME         0x00f4
/** Host Command ID : wfd mode config */
#define HostCmd_CMD_WFD_MODE_CONFIG             0x00eb
/** Host Command ID:  WFD_SET_PARAMS */
#define HostCmd_CMD_WFD_PARAMS_CONFIG           0x00ea
/** Host Command ID : Set BSS_MODE */
#define HostCmd_CMD_SET_BSS_MODE               0x00f7
/** Host Command id: SYS_CONFIGURE  */
#define HOST_CMD_APCMD_SYS_CONFIGURE           0x00b0
/** Host Command id: BSS_START */
#define HOST_CMD_APCMD_BSS_START               0x00b1
/** Check Bit GO set by peer Bit 0 */
#define WFD_GROUP_OWNER_MASK                    0x01
/** Check Bit GO set by peer Bit 0 */
#define WFD_GROUP_CAP_PERSIST_GROUP_BIT         0x02
/** Check Bit formation Bit 6 */
#define WFD_GROUP_FORMATION_OR_MASK             0x40
/** Check Bit formation Bit 6 */
#define WFD_GROUP_FORMATION_AND_MASK            0xBF

/** BSS Mode: WIFIDIRECT Client */
#define BSS_MODE_WIFIDIRECT_CLIENT      0
/** BSS Mode: WIFIDIRECT GO */
#define BSS_MODE_WIFIDIRECT_GO          2

/** C Success */
#define SUCCESS                         1
/** C Failure */
#define FAILURE                         0

/** Wifi reg class = 81 */
#define WIFI_REG_CLASS_81               81

/** MAC BROADCAST */
#define WFD_RET_MAC_BROADCAST   0x1FF
/** MAC MULTICAST */
#define WFD_RET_MAC_MULTICAST   0x1FE
/**
 * Hex or Decimal to Integer
 * @param   num string to convert into decimal or hex
 */
#define A2HEXDECIMAL(num)  \
	(strncasecmp("0x", (num), 2) ? (unsigned int) strtoll((num), NULL, 0) \
	 : a2hex((num)))\
/**
 * Check of decimal or hex string
 * @param   num string
 */
#define IS_HEX_OR_DIGIT(num) \
	(strncasecmp("0x", (num), 2) ? ISDIGIT((num)) : ishexstring((num)))\

/** Set OPP_PS variable */
#define SET_OPP_PS(x) ((x) << 7)

/** Convert WFD header to little endian format from CPU format */
#define endian_convert_tlv_wfd_header_out(x);           \
do {                                               \
	(x)->length = wlan_cpu_to_le16((x)->length); \
} while (0);

/** Convert WPS TLV header to network order */
#define endian_convert_tlv_wps_header_out(x);           \
do {                                               \
	(x)->tag = htons((x)->tag);       \
	(x)->length = htons((x)->length); \
} while (0);

/** OUI SubType FIND phase action DIRECT */
#define WFD_GO_NEG_REQ_ACTION_SUB_TYPE        0
/** OUI SubType Provision discovery request */
#define WFD_PROVISION_DISCOVERY_REQ_SUB_TYPE        7
/** WPS Device Type maximum length */
#define WPS_DEVICE_TYPE_MAX_LEN                 8

/** TLV : Wfd status */
#define TLV_TYPE_WFD_STATUS             0
/** TLV : Wfd param capability */
#define TLV_TYPE_WFD_CAPABILITY         2
/** TLV : Wfd param device Id */
#define TLV_TYPE_WFD_DEVICE_ID          3
/** TLV : Wfd param group owner intent */
#define TLV_TYPE_WFD_GROUPOWNER_INTENT  4
/** TLV : Wfd param config timeout */
#define TLV_TYPE_WFD_CONFIG_TIMEOUT     5
/** TLV : Wfd param channel */
#define TLV_TYPE_WFD_CHANNEL            6
/** TLV : Wfd param group bssId */
#define TLV_TYPE_WFD_GROUP_BSS_ID       7
/** TLV : Wfd param extended listen time */
#define TLV_TYPE_WFD_EXTENDED_LISTEN_TIME 8
/** TLV : Wfd param intended address */
#define TLV_TYPE_WFD_INTENDED_ADDRESS   9
/** TLV : Wfd param manageability */
#define TLV_TYPE_WFD_MANAGEABILITY      10
/** TLV : Wfd param channel list */
#define TLV_TYPE_WFD_CHANNEL_LIST       11
/** TLV : Wfd Notice of Absence */
#define TLV_TYPE_WFD_NOTICE_OF_ABSENCE  12
/** TLV : Wfd param device Info */
#define TLV_TYPE_WFD_DEVICE_INFO        13
/** TLV : Wfd param Group Info */
#define TLV_TYPE_WFD_GROUP_INFO         14
/** TLV : Wfd param group Id */
#define TLV_TYPE_WFD_GROUP_ID           15
/** TLV : Wfd param interface */
#define TLV_TYPE_WFD_INTERFACE          16
/** TLV : Wfd param operating channel */
#define TLV_TYPE_WFD_OPCHANNEL          17
/** TLV : Wfd param invitation flag */
#define TLV_TYPE_WFD_INVITATION_FLAG    18

/** TLV: Wfd Discovery Period */
#define MRVL_WFD_DISC_PERIOD_TLV_ID       (PROPRIETARY_TLV_BASE_ID + 124)
/** TLV: Wfd Scan Enable */
#define MRVL_WFD_SCAN_ENABLE_TLV_ID       (PROPRIETARY_TLV_BASE_ID + 125)
/** TLV: Wfd Peer Device  */
#define MRVL_WFD_PEER_DEVICE_TLV_ID       (PROPRIETARY_TLV_BASE_ID + 126)
/** TLV: Wfd Scan Request Peer Device  */
#define MRVL_WFD_SCAN_REQ_DEVICE_TLV_ID   (PROPRIETARY_TLV_BASE_ID + 127)
/** TLV: Wfd Device State */
#define MRVL_WFD_DEVICE_STATE_TLV_ID      (PROPRIETARY_TLV_BASE_ID + 128)
/** TLV: WifiDirect Listen channel */
#define MRVL_WFD_LISTEN_CHANNEL_TLV_ID       (PROPRIETARY_TLV_BASE_ID + 0x86)
/** TLV: WifiDirect Operating Channel */
#define MRVL_WFD_OPERATING_CHANNEL_TLV_ID    (PROPRIETARY_TLV_BASE_ID + 0x87)
/** TLV: Wfd Persistent Group */
#define MRVL_WFD_PERSISTENT_GROUP_TLV_ID     (PROPRIETARY_TLV_BASE_ID + 0x88)
/** TLV: Wfd Provisioning Params */
#define MRVL_WFD_PROVISIONING_PARAMS_TLV_ID  (PROPRIETARY_TLV_BASE_ID + 0x8f)
/** TLV: Wfd Provisioning WPS params */
#define MRVL_WFD_WPS_PARAMS_TLV_ID           (PROPRIETARY_TLV_BASE_ID + 0x90)

/** Max Device capability */
#define MAX_DEV_CAPABILITY                 255
/** Max group capability */
#define MAX_GRP_CAPABILITY                 255
/** Max Intent */
#define MAX_INTENT                        15
/** Max length of Primary device type OUI */
#define MAX_PRIMARY_OUI_LEN               4
/** Min value of Regulatory class */
#define MIN_REG_CLASS                      1
/** Max value of Regulatory class */
#define MAX_REG_CLASS                    255
/** Min Primary Device type category */
#define MIN_PRIDEV_TYPE_CAT                1
/** Max Primary Device type category */
#define MAX_PRIDEV_TYPE_CAT               11
/** Min value of NoA index */
#define MIN_NOA_INDEX                   0
/** Max value of NoA index */
#define MAX_NOA_INDEX                   255
/** Min value of CTwindow */
#define MIN_CTWINDOW                    10
/** Max value of CTwindow */
#define MAX_CTWINDOW                    63
/** Min value of Count/Type */
#define MIN_COUNT_TYPE                  1
/** Max value of Count/Type */
#define MAX_COUNT_TYPE                  255
/** Min Primary Device type subcategory */
#define MIN_PRIDEV_TYPE_SUBCATEGORY        1
/** Max Primary Device type subcategory */
#define MAX_PRIDEV_TYPE_SUBCATEGORY        9
/** Min value of WPS config method */
#define MIN_WPS_CONF_METHODS            0x01
/** Max value of WPS config method */
#define MAX_WPS_CONF_METHODS            0xffff
/** Max length of Advertisement Protocol IE  */
#define MAX_ADPROTOIE_LEN               4
/** Max length of Discovery Information ID  */
#define MAX_INFOID_LEN                  2
/** Max length of OUI  */
#define MAX_OUI_LEN                     3
/** Max count of interface list */
#define MAX_INTERFACE_ADDR_COUNT        41
/** Max count of secondary device types */
#define MAX_SECONDARY_DEVICE_COUNT      15
/** Max count of group secondary device types*/
#define MAX_GROUP_SECONDARY_DEVICE_COUNT  2
/** Maximum length of lines in configuration file */
#define MAX_CONFIG_LINE                 1024
/** Maximum channels */
#define MAX_CHANNELS                    14
/** Maximum number of NoA descriptors */
#define MAX_NOA_DESCRIPTORS            8
/** Maximum number of channel list entries */
#define MAX_CHAN_LIST    8
/** Maximum buffer size for channel entries */
#define MAX_BUFFER_SIZE         64
/** WPS Minimum version number */
#define WPS_MIN_VERSION            0x10
/** WPS Maximum version number */
#define WPS_MAX_VERSION                 0x20
/** WPS Minimum request type */
#define WPS_MIN_REQUESTTYPE        0x00
/** WPS Maximum request type */
#define WPS_MAX_REQUESTTYPE        0x04
/** WPS UUID maximum length */
#define WPS_UUID_MAX_LEN                16
/** WPS Device Type maximum length */
#define WPS_DEVICE_TYPE_MAX_LEN         8
/** WPS Minimum association state */
#define WPS_MIN_ASSOCIATIONSTATE    0x0000
/** WPS Maximum association state */
#define WPS_MAX_ASSOCIATIONSTATE    0x0004
/** WPS Minimum configuration error */
#define WPS_MIN_CONFIGURATIONERROR    0x0000
/** WPS Maximum configuration error */
#define WPS_MAX_CONFIGURATIONERROR    0x0012
/** WPS Minimum Device password ID */
#define WPS_MIN_DEVICEPASSWORD        0x0000
/** WPS Maximum Device password ID */
#define WPS_MAX_DEVICEPASSWORD        0x000f
/** WPS Device Name maximum length */
#define WPS_DEVICE_NAME_MAX_LEN         32
/** WPS Model maximum length */
#define WPS_MODEL_MAX_LEN               32
/** WPS Serial maximum length */
#define WPS_SERIAL_MAX_LEN              32
/** WPS Manufacturer maximum length */
#define WPS_MANUFACT_MAX_LEN            64
/** WPS Device Info OUI+Type+SubType Length */
#define WPS_DEVICE_TYPE_LEN             8
/** Max size of custom IE buffer */
#define MAX_SIZE_IE_BUFFER              (256)
/** Country string last byte 0x04 */
#define WFD_COUNTRY_LAST_BYTE           0x04
/** Mask in WFD IE */
#define MASK_WFD_AUTO			0xFFFF
/** Wfd IE Header */
typedef struct _wfd_ie_header {
    /** Element ID */
	u8 element_id;
    /** IE Length */
	u8 ie_length;
    /** OUI */
	u8 oui[3];
    /** OUI type */
	u8 oui_type;
    /** IE List of TLV */
	u8 ie_list[0];
} __ATTRIB_PACK__ wfd_ie_header;

/** IEEEtypes_DsParamSet_t */
typedef struct _IEEE_DsParamSet_t {
    /** DS parameter : Element ID */
	u8 element_id;
    /** DS parameter : Length */
	u8 ie_length;
    /** DS parameter : Current channel */
	u8 current_chan;
} __ATTRIB_PACK__ IEEE_DsParamSet;

/** TLV buffer : WifiDirect Operating Channel */
typedef struct _tlvbuf_mrvl_wfd_channel {
    /** Tag */
	u16 tag;
    /** Length */
	u16 length;
    /** Operating Channel */
	u8 channel_num;
} __ATTRIB_PACK__ tlvbuf_mrvl_wfd_channel;

/** Mask for device role in sub_event_type */
#define DEVICE_ROLE_MASK 0x0003
/** Mask for packet process state in sub_event_type */
#define PKT_PROCESS_STATE_MASK 0x001c

/** Event : Wfd event subtype fields */
#ifdef BIG_ENDIAN
typedef struct _wfd_event_subtype {
    /** Reserved */
	u16 reserved:11;
    /** Packet Process State */
	u16 pkt_process_state:3;
    /** Device Role */
	u16 device_role:2;
} __ATTRIB_PACK__ wfd_event_subtype;
#else
typedef struct _wfd_event_subtype {
    /** Device Role */
	u16 device_role:2;
    /** Packet Process State */
	u16 pkt_process_state:3;
    /** Reserved */
	u16 reserved:11;
} __ATTRIB_PACK__ wfd_event_subtype;
#endif

/** Event : Wfd Generic event */
typedef struct _apeventbuf_wfd_generic {
    /** Event Length */
	u16 event_length;
    /** Event Type */
	u16 event_type;
    /** Event SubType */
	wfd_event_subtype event_sub_type;
    /** Peer mac address */
	u8 peer_mac_addr[ETH_ALEN];
    /** IE List of TLV */
	u8 entire_ie_list[0];
} __ATTRIB_PACK__ apeventbuf_wfd_generic;

/** TLV buffer : Wfd IE device Id */
typedef struct _tlvbuf_wfd_device_id {
    /** TLV Header tag */
	u8 tag;
    /** TLV Header length */
	u16 length;
    /** WFD device MAC address */
	u8 dev_mac_address[ETH_ALEN];
} __ATTRIB_PACK__ tlvbuf_wfd_device_id;

/** TLV buffer : Wfd Status */
typedef struct _tlvbuf_wfd_status {
    /** TLV Header tag */
	u8 tag;
    /** TLV Header length */
	u16 length;
    /** WFD status code */
	u8 status_code;
} __ATTRIB_PACK__ tlvbuf_wfd_status;

/** TLV buffer : Wfd IE capability */
typedef struct _tlvbuf_wfd_capability {
    /** TLV Header tag */
	u8 tag;
    /** TLV Header length */
	u16 length;
    /** WFD device capability */
	u8 dev_capability;
    /** WFD group capability */
	u8 group_capability;
} __ATTRIB_PACK__ tlvbuf_wfd_capability;

/** TLV buffer : Wfd extended listen time */
typedef struct _tlvbuf_wfd_ext_listen_time {
    /** TLV Header tag */
	u8 tag;
    /** TLV Header length */
	u16 length;
    /** Availability period */
	u16 availability_period;
    /** Availability interval */
	u16 availability_interval;
} __ATTRIB_PACK__ tlvbuf_wfd_ext_listen_time;

/** TLV buffer : Wfd Intended Interface Address */
typedef struct _tlvbuf_wfd_intended_addr {
    /** TLV Header tag */
	u8 tag;
    /** TLV Header length */
	u16 length;
    /** WFD Group interface address */
	u8 group_address[ETH_ALEN];
} __ATTRIB_PACK__ tlvbuf_wfd_intended_addr;

/** TLV buffer : Wfd IE Interface */
typedef struct _tlvbuf_wfd_interface {
    /** TLV Header tag */
	u8 tag;
    /** TLV Header length */
	u16 length;
    /** WFD interface Id */
	u8 interface_id[ETH_ALEN];
    /** WFD interface count */
	u8 interface_count;
    /** WFD interface addresslist */
	u8 interface_idlist[0];
} __ATTRIB_PACK__ tlvbuf_wfd_interface;

/** TLV buffer : Wfd configuration timeout */
typedef struct _tlvbuf_wfd_config_timeout {
    /** TLV Header tag */
	u8 tag;
    /** TLV Header length */
	u16 length;
    /** Group configuration timeout */
	u8 group_config_timeout;
    /** Device configuration timeout */
	u8 device_config_timeout;
} __ATTRIB_PACK__ tlvbuf_wfd_config_timeout;

/** TLV buffer : Wfd IE Group owner intent */
typedef struct _tlvbuf_wfd_group_owner_intent {
    /** TLV Header tag */
	u8 tag;
    /** TLV Header length */
	u16 length;
    /** WFD device group owner intent */
	u8 dev_intent;
} __ATTRIB_PACK__ tlvbuf_wfd_group_owner_intent;

/** TLV buffer : Wfd IE channel */
typedef struct _tlvbuf_wfd_channel {
    /** TLV Header tag */
	u8 tag;
    /** TLV Header length */
	u16 length;
    /** WFD country string */
	u8 country_string[3];
    /** WFD regulatory class */
	u8 regulatory_class;
    /** WFD channel number */
	u8 channel_number;
} __ATTRIB_PACK__ tlvbuf_wfd_channel;

/** TLV buffer : Wfd IE manageability */
typedef struct _tlvbuf_wfd_manageability {
    /** TLV Header tag */
	u8 tag;
    /** TLV Header length */
	u16 length;
    /** WFD manageability */
	u8 manageability;
} __ATTRIB_PACK__ tlvbuf_wfd_manageability;

/** TLV buffer : Wfd IE Invitation Flag */
typedef struct _tlvbuf_wfd_invitation_flag {
    /** TLV Header tag */
	u8 tag;
    /** TLV Header length */
	u16 length;
    /** WFD Invitation Flag */
	u8 invitation_flag;
} __ATTRIB_PACK__ tlvbuf_wfd_invitation_flag;

/** Channel Entry */
typedef struct _chan_entry {
    /** WFD regulatory class */
	u8 regulatory_class;
    /** WFD no of channels */
	u8 num_of_channels;
    /** WFD channel number */
	u8 chan_list[0];
} __ATTRIB_PACK__ chan_entry;

/** TLV buffer : Wfd IE channel list */
typedef struct _tlvbuf_wfd_channel_list {
    /** TLV Header tag */
	u8 tag;
    /** TLV Header length */
	u16 length;
    /** WFD country string */
	u8 country_string[3];
    /** WFD channel entry list */
	chan_entry wfd_chan_entry_list[0];
} __ATTRIB_PACK__ tlvbuf_wfd_channel_list;

/** NoA Descriptor */
typedef struct _noa_descriptor {
    /** WFD count OR type */
	u8 count_type;
    /** WFD duration */
	u32 duration;
    /** WFD interval */
	u32 interval;
    /** WFD start time */
	u32 start_time;
} __ATTRIB_PACK__ noa_descriptor;

/** TLV buffer : Wfd IE Notice of Absence */
typedef struct _tlvbuf_wfd_notice_of_absence {
    /** TLV Header tag */
	u8 tag;
    /** TLV Header length */
	u16 length;
    /** WFD NoA Index */
	u8 noa_index;
    /** WFD CTWindow and OppPS parameters */
	u8 ctwindow_opp_ps;
    /** WFD NoA Descriptor list */
	noa_descriptor wfd_noa_descriptor_list[0];
} __ATTRIB_PACK__ tlvbuf_wfd_notice_of_absence;

/** TLV buffer : Wfd IE group Id */
typedef struct _tlvbuf_wfd_group_id {
    /** TLV Header tag */
	u8 tag;
    /** TLV Header length */
	u16 length;
    /** WFD group MAC address */
	u8 group_address[ETH_ALEN];
    /** WFD group SSID */
	u8 group_ssid[0];
} __ATTRIB_PACK__ tlvbuf_wfd_group_id;

/** TLV buffer : Wfd IE group BSS Id */
typedef struct _tlvbuf_wfd_group_bss_id {
    /** TLV Header tag */
	u8 tag;
    /** TLV Header length */
	u16 length;
    /** WFD group Bss Id */
	u8 group_bssid[ETH_ALEN];
} __ATTRIB_PACK__ tlvbuf_wfd_group_bss_id;

/** TLV buffer : Wfd IE device Info */
typedef struct _tlvbuf_wfd_device_info {
    /** TLV Header tag */
	u8 tag;
    /** TLV Header length */
	u16 length;
    /** WFD device address */
	u8 dev_address[ETH_ALEN];
    /** WPS config methods */
	u16 config_methods;
    /** Primary device type : category */
	u16 primary_category;
    /** Primary device type : OUI */
	u8 primary_oui[4];
    /** Primary device type : sub-category */
	u16 primary_subcategory;
    /** Secondary Device Count */
	u8 secondary_dev_count;
    /** Secondary Device Info */
	u8 secondary_dev_info[0];
    /** WPS Device Name Tag */
	u16 device_name_type;
    /** WPS Device Name Length */
	u16 device_name_len;
    /** Device name */
	u8 device_name[0];
} __ATTRIB_PACK__ tlvbuf_wfd_device_info;

typedef struct _wfd_client_dev_info {
    /** Length of each device */
	u8 dev_length;
    /** WFD device address */
	u8 wfd_dev_address[ETH_ALEN];
    /** WFD Interface  address */
	u8 wfd_intf_address[ETH_ALEN];
    /** WFD Device capability*/
	u8 wfd_dev_capability;
    /** WPS config methods */
	u16 config_methods;
    /** Primary device type : category */
	u16 primary_category;
    /** Primary device type : OUI */
	u8 primary_oui[4];
    /** Primary device type : sub-category */
	u16 primary_subcategory;
    /** Secondary Device Count */
	u8 wfd_secondary_dev_count;
    /** Secondary Device Info */
	u8 wfd_secondary_dev_info[0];
    /** WPS WFD Device Name Tag */
	u16 device_name_type;
    /** WPS WFD Device Name Length */
	u16 wfd_device_name_len;
    /** WFD Device name */
	u8 wfd_device_name[0];
} __ATTRIB_PACK__ wfd_client_dev_info;

typedef struct _tlvbuf_wfd_group_info {
    /** TLV Header tag */
	u8 tag;
    /** TLV Header length */
	u16 length;
    /** Secondary Device Info */
	u8 wfd_client_dev_list[0];
} __ATTRIB_PACK__ tlvbuf_wfd_group_info;

/** TLV buffer : Wfd WPS IE */
typedef struct _tlvbuf_wfd_wps_ie {
    /** TLV Header tag */
	u16 tag;
    /** TLV Header length */
	u16 length;
    /** WFD WPS IE data */
	u8 data[0];
} __ATTRIB_PACK__ tlvbuf_wps_ie;

/** WFDCMD buffer */
typedef struct _wfdcmdbuf {
    /** Command header */
	mrvl_cmd_head_buf cmd_head;
} __ATTRIB_PACK__ wfdcmdbuf;

/** HostCmd_CMD_WFD_ACTION_FRAME request */
typedef struct _wfd_action_frame {
    /** Command header */
	mrvl_cmd_head_buf cmd_head;
    /** Peer mac address */
	u8 peer_mac_addr[ETH_ALEN];
    /** Category */
	u8 category;
    /** Action */
	u8 action;
    /** OUI */
	u8 oui[3];
    /** OUI type */
	u8 oui_type;
    /** OUI sub type */
	u8 oui_sub_type;
    /** Dialog taken */
	u8 dialog_taken;
    /** IE List of TLVs */
	u8 ie_list[0];
} __ATTRIB_PACK__ wfd_action_frame;

/** HostCmd_CMD_WFD_PARAMS_CONFIG struct */
typedef struct _wfd_params_config {
    /** Command header */
	mrvl_cmd_head_buf cmd_head;
    /** Action */
	u16 action;		/* 0 = ACT_GET; 1 = ACT_SET; */
    /** TLV data */
	u8 wfd_tlv[0];
} __ATTRIB_PACK__ wfd_params_config;

/** HostCmd_CMD_WFD_MODE_CONFIG structure */
typedef struct _wfd_mode_config {
    /** Header */
	mrvl_cmd_head_buf cmd_head;
    /** Action */
	u16 action;		/* 0 = ACT_GET; 1 = ACT_SET; */
    /** wfd mode data */
	u16 mode;
} __ATTRIB_PACK__ wfd_mode_config;

/** HostCmd_CMD_SET_BSS_MODE structure */
typedef struct _wfd_bss_mode_config {
    /** Header */
	mrvl_cmd_head_buf cmd_head;
    /** connection type */
	u8 con_type;
} __ATTRIB_PACK__ wfd_bss_mode_config;

/** HostCmd_CMD_WFD_PARAMS_CONFIG struct */
typedef struct _sys_config {
    /** Command header */
	mrvl_cmd_head_buf cmd_head;
    /** Action */
	u16 action;		/* 0 = ACT_GET; 1 = ACT_SET; */
    /** TLV data */
	u8 buf[0];
} __ATTRIB_PACK__ sys_config;

/** TLV buffer : WifiDirect Provisioning parameters*/
typedef struct _wfd_provisioning_params {
    /** Tag */
	u16 tag;
    /** Length */
	u16 length;
    /** action */
	u16 action;
    /** config methods */
	u16 config_methods;
    /** config methods */
	u16 dev_password;
} __ATTRIB_PACK__ wfd_provisioning_params;

/** TLV buffer : WifiDirect WPS parameters*/
typedef struct _tlvbuf_wfd_wps_params {
    /** Tag */
	u16 tag;
    /** Length */
	u16 length;
    /** action */
	u16 action;
} __ATTRIB_PACK__ wfd_wps_params;

/** Valid Input Commands */
typedef enum {
	SCANCHANNELS,
	CHANNEL,
	WFD_DEVICECAPABILITY,
	WFD_GROUPCAPABILITY,
	WFD_INTENT,
	WFD_REGULATORYCLASS,
	WFD_MANAGEABILITY,
	WFD_INVITATIONFLAG,
	WFD_COUNTRY,
	WFD_NO_OF_CHANNELS,
	WFD_NOA_INDEX,
	WFD_OPP_PS,
	WFD_CTWINDOW,
	WFD_COUNT_TYPE,
	WFD_DURATION,
	WFD_INTERVAL,
	WFD_START_TIME,
	WFD_PRIDEVTYPECATEGORY,
	WFD_PRIDEVTYPEOUI,
	WFD_PRIDEVTYPESUBCATEGORY,
	WFD_SECONDARYDEVCOUNT,
	WFD_GROUP_SECONDARYDEVCOUNT,
	WFD_GROUP_WFD_DEVICE_NAME,
	WFD_INTERFACECOUNT,
	WFD_ATTR_CONFIG_TIMEOUT,
	WFD_ATTR_EXTENDED_TIME,
	WFD_WPSCONFMETHODS,
	WFD_WPSVERSION,
	WFD_WPSSETUPSTATE,
	WFD_WPSREQRESPTYPE,
	WFD_WPSSPECCONFMETHODS,
	WFD_WPSUUID,
	WFD_WPSPRIMARYDEVICETYPE,
	WFD_WPSRFBAND,
	WFD_WPSASSOCIATIONSTATE,
	WFD_WPSCONFIGURATIONERROR,
	WFD_WPSDEVICENAME,
	WFD_WPSDEVICEPASSWORD,
	WFD_WPSMANUFACTURER,
	WFD_WPSMODELNAME,
	WFD_WPSMODELNUMBER,
	WFD_WPSSERIALNUMBER,
	WFD_MINDISCOVERYINT,
	WFD_MAXDISCOVERYINT,
	WFD_ENABLE_SCAN,
	WFD_DEVICE_STATE,
} valid_inputs;

/** Level of wfd parameters in the wfd.conf file */
typedef enum {
	WFD_PARAMS_CONFIG = 1,
	WFD_CAPABILITY_CONFIG,
	WFD_GROUP_OWNER_INTENT_CONFIG,
	WFD_CHANNEL_CONFIG,
	WFD_MANAGEABILITY_CONFIG,
	WFD_INVITATION_FLAG_CONFIG,
	WFD_CHANNEL_LIST_CONFIG,
	WFD_NOTICE_OF_ABSENCE,
	WFD_NOA_DESCRIPTOR,
	WFD_DEVICE_INFO_CONFIG,
	WFD_GROUP_INFO_CONFIG,
	WFD_GROUP_SEC_INFO_CONFIG,
	WFD_GROUP_CLIENT_INFO_CONFIG,
	WFD_DEVICE_SEC_INFO_CONFIG,
	WFD_GROUP_ID_CONFIG,
	WFD_GROUP_BSS_ID_CONFIG,
	WFD_DEVICE_ID_CONFIG,
	WFD_INTERFACE_CONFIG,
	WFD_TIMEOUT_CONFIG,
	WFD_EXTENDED_TIME_CONFIG,
	WFD_INTENDED_ADDR_CONFIG,
	WFD_OPCHANNEL_CONFIG,
	WFD_WPSIE,
	WFD_DISCOVERY_REQUEST_RESPONSE = 0x20,
	WFD_DISCOVERY_QUERY,
	WFD_DISCOVERY_SERVICE,
	WFD_DISCOVERY_VENDOR,
	WFD_DISCOVERY_QUERY_RESPONSE_PER_PROTOCOL,
	WFD_EXTRA,
} wfd_param_level;

/* Needed for passphrase to PSK conversion */
void pbkdf2_sha1(const char *passphrase, const char *ssid, size_t ssid_len,
		 int iterations, u8 *buf, size_t buflen);

/**  Max Number of Peristent group possible in FW */
#define WFD_PERSISTENT_GROUP_MAX        (4)

/** TLV buffer : persistent group */
typedef struct _tlvbuf_wfd_persistent_group {
    /** Tag */
	u16 tag;
    /** Length */
	u16 length;
    /** Record Index */
	u8 rec_index;
    /** Device Role */
	u8 dev_role;
    /** wfd group Bss Id */
	u8 group_bssid[ETH_ALEN];
    /** wfd device MAC address */
	u8 dev_mac_address[ETH_ALEN];
    /** wfd group SSID length */
	u8 group_ssid_len;
    /** wfd group SSID */
	u8 group_ssid[0];
    /** wfd PSK length */
	u8 psk_len;
    /** wfd PSK */
	u8 psk[0];
    /** Num of PEER MAC Addresses */
	u8 num_peers;
    /** PEER MAC Addresses List */
	u8 peer_mac_addrs[0][ETH_ALEN];
} __ATTRIB_PACK__ tlvbuf_wfd_persistent_group;

void wfd_process_peer_detected_event(u8 *buffer, u16 size);
void wfd_peer_ageout_time_handler(void *user_data);
void wfd_peer_selected_ageout_time_handler(void *user_data);
void wfd_reset();
/** wfd IOCTL */
int wfd_ioctl(u8 *cmd, u16 buf_size);

int wfd_wlan_update_bss_mode(u16 mode);
void wps_set_wfd_cfg();

/** Return index of persistent group record */
int wfd_get_persistent_rec_ind_by_ssid(int ssid_length,
				       u8 *ssid, s8 *pr_index);
/** Process received service discovery request */
void wfd_process_service_discovery(u8 *buffer, u16 size);
/**
 *  @brief  Set private ioctl command "setuserscan" to WLAN driver
 *
 *  @param channel    channel number for active scan
 *  @return           WPS_STATUS_SUCCESS--success, WPS_STATUS_FAIL--fail
 */
/*int wps_wlan_set_user_scan(WFD_DATA *pwfd_data, int channel,
				int scan_on_channel_list);*/

/*static inline u16 ntohs(u16 n)
{
  return ((n & 0xFF) << 8) | ((n & 0xFF00) >> 8);
}*/
#endif /* _WLAN_WFD_H_ */
