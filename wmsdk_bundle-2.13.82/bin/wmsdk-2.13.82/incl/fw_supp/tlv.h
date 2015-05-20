/*****************************************************************************
 *
 * File: tlv.h
 *
 *
 *
 * Author(s):    Kapil Chhabra
 * Date:         2005-01-27
 * Description:  Definitions of the Marvell TLV and parsing functions.
 *
 *****************************************************************************/
#ifndef TLV_H__
#define TLV_H__

#include "tlv_id.h"
#include "IEEE_types.h"
#include "IEEE_action_types.h"
#ifdef MESH
#include "meshDef.h"
#endif

/*
** TLV IDs are assigned in tlv_id.h.
*/
/* This struct is used in ROM and should not be changed at all */
typedef PACK_START struct
{
    UINT16 Type;
    UINT16 Length;
} PACK_END MrvlIEParamSet_t;

typedef PACK_START struct
{
    MrvlIEParamSet_t IEParam;
    UINT8 Value[1];
} PACK_END MrvlIEGeneric_t;

/* MultiChannel TLV*/
typedef PACK_START struct
{
    MrvlIEParamSet_t IEParam;
    UINT16           Status; // 1 = Active, 0 = Inactive
    UINT8            TlvBuffer[1];            
}
PACK_END MrvlIEMultiChanInfo_t;

typedef PACK_START struct
{
    MrvlIEParamSet_t IEParam;
    UINT8            ChanGroupId;
    UINT8            ChanBufWt;
    ChanBandInfo_t   ChanBandInfo;
    UINT32           ChanTime;
    UINT32           Reserved;
    UINT8            HidPortNum;
    UINT8            NumIntf;
    UINT8            BssTypeNumList[1];
}
PACK_END MrvlIEMultiChanGroupInfo_t;

/* Key Material TLV */
typedef PACK_START struct MrvlIEKeyParamSet_t
{
    MrvlIEParamSet_t        hdr;
    UINT16                  keyMgtId;
} PACK_END MrvlIEKeyParamSet_t;


#ifdef KEY_MATERIAL_V2

typedef PACK_START struct wep_key_t
{
    UINT16 len;
    UINT8  key[1];
} PACK_END wep_key_t;

typedef PACK_START struct wpax_key_t
{
    UINT8  pn[8];
    UINT16 len;
    UINT8  key[1];
} PACK_END wpax_key_t;

typedef PACK_START struct wapi_key_t
{
    UINT8  pn[16];
    UINT16 len;
    UINT8  key[16];
    UINT8  micKey[16];
} PACK_END wapi_key_t;

typedef PACK_START struct MrvlIEKeyParamSet_v2_t
{
    MrvlIEParamSet_t        hdr;
    IEEEtypes_MacAddr_t     macAddr;
    UINT8                   keyIdx;
    UINT8                   keyType;
    UINT16                  keyInfo;
    
    PACK_START union 
    {
        wep_key_t           wep;
        wpax_key_t          wpax;
        wapi_key_t          wapi;
    } PACK_END keySet;

} PACK_END MrvlIEKeyParamSet_v2_t;
#endif

/* Marvell Power Constraint TLV */
typedef PACK_START struct MrvlIEPowerConstraint_t
{
    MrvlIEParamSet_t  IEParam;
    UINT8  channel;
    UINT8  dBm;
} PACK_END  MrvlIEPowerConstraint_t;

/* Marvell WSC Selected Registar TLV */
typedef PACK_START struct MrvlIEWSCSelectedRegistrar_t
{
    MrvlIEParamSet_t  IEParam;
    UINT16            devPwdID;
} PACK_END  MrvlIEWSCSelectedRegistrar_t;

/* Marvell WSC Enrollee TMO TLV */
typedef PACK_START struct MrvlIEWSCEnrolleeTmo_t
{
    MrvlIEParamSet_t  IEParam;
    UINT16            tmo;
} PACK_END  MrvlIEWSCEnrolleeTmo_t;

/****************
 * AES CRYPTION FEATURE
 *
 * DEFINE STARTS --------------
 */
typedef PACK_START struct MrvlIEAesCrypt_t
{
    MrvlIEParamSet_t    hdr;
    UINT8           payload[40];
} PACK_END MrvlIEAesCrypt_t;

/* DEFINE ENDS ----------------
 */

/* Marvell Power Capability TLV */
typedef PACK_START struct MrvlIEPowerCapability_t
{
    MrvlIEParamSet_t  IEParam;
    UINT8  minPwr;
    UINT8  maxPwr;
} PACK_END  MrvlIEPowerCapability_t;

/* Marvell TLV for OMNI Serial Number and Hw Revision Information. */
typedef PACK_START struct MrvlIE_OMNI_t
{
    MrvlIEParamSet_t  IEParam;
    UINT8  SerialNumber[16];
    UINT8  HWRev;
    UINT8  Reserved[3];
} PACK_END  MrvlIE_OMNI_t;

/* Marvell LED Behavior TLV */
typedef PACK_START struct MrvlIELedBehavior_t
{
    MrvlIEParamSet_t IEParam;
    UINT8 FirmwareState;
    UINT8 LedNumber;
    UINT8 LedState;
    UINT8 LedArgs;
} PACK_END  MrvlIELedBehavior_t;

/* Marvell LED GPIO Mapping TLV */
typedef PACK_START struct MrvlIELedGpio_t
{
    MrvlIEParamSet_t IEParam;
    UINT8 LEDNumber;
    UINT8 GPIONumber;
} PACK_END MrvlIELedGpio_t;

typedef PACK_START struct
{
    MrvlIEParamSet_t IEParam;
    /*
     ** Set a place holder for the TSF values.  Sized to max BSS for message
     **   allocation. The TLV will return a variable number of TSF values.
     */
    UINT64 TSFValue[IEEEtypes_MAX_BSS_DESCRIPTS];
} PACK_END MrvlIETsfArray_t;

typedef PACK_START struct
{
    MrvlIEParamSet_t IEParam;
    UINT8            maxLen;
    IEEEtypes_SsId_t ssid;
} PACK_END MrvlIEWildcardSsid_t;

typedef PACK_START struct
{
    MrvlIEParamSet_t IEParam;
    UINT8            snrThreshold;
    UINT8            reportFrequency;
} PACK_END MrvlIELowSnrThreshold_t;

typedef PACK_START struct
{
    MrvlIEParamSet_t IEParam;
    UINT8            rssiThreshold;
    UINT8            reportFrequency;
} PACK_END MrvlIELowRssiThreshold_t;

/* Marvell AutoTx TLV */
#define MAX_KEEPALIVE_PKT_LEN   (0x60)
typedef PACK_START struct
{
    MrvlIEParamSet_t IEParam;
    UINT16 Interval; /* in seconds */
    UINT8  Priority;
    UINT8  Reserved;
    UINT16 EtherFrmLen;
    UINT8  DestAddr[6];
    UINT8  SrcAddr[6];
    UINT8  EtherFrmBody[MAX_KEEPALIVE_PKT_LEN]; //Last 4 bytes are 32bit FCS
} PACK_END MrvlIEAutoTx_t;

typedef PACK_START struct
{
    MrvlIEParamSet_t    IEParam;
    IEEEtypes_DFS_Map_t map;
} PACK_END MrvlIEChanRpt11hBasic_t;

typedef PACK_START struct
{
    MrvlIEParamSet_t  IEParam;
    UINT8             scanReqId;
} PACK_END MrvlIEChanRptBeacon_t;

typedef PACK_START struct
{
    MrvlIEParamSet_t  IEParam;
    UINT8             ccaBusyFraction;
} PACK_END MrvlIEChanRptChanLoad_t;

typedef PACK_START struct
{
    MrvlIEParamSet_t  IEParam;
    SINT16            anpi;
    UINT8             rpiDensities[11];
} PACK_END MrvlIEChanRptNoiseHist_t;

typedef PACK_START struct
{
    MrvlIEParamSet_t    IEParam;
    IEEEtypes_MacAddr_t sourceAddr;
    IEEEtypes_MacAddr_t bssid;
    SINT16              rssi;
    UINT16              frameCnt;
} PACK_END MrvlIEChanRptFrame_t;

typedef PACK_START struct
{
    MrvlIEParamSet_t    IEParam;
    SINT16              rssi;
    SINT16              anpi;
    UINT8               ccaBusyFraction;
#ifdef SCAN_REPORT_THROUGH_EVENT
    BandConfig_t        band;
    UINT8               channel;
    UINT8               reserved;
    UINT64              tsf;
#endif
} PACK_END MrvlIEBssScanStats_t;

typedef PACK_START struct
{
    /** Header */
    MrvlIEParamSet_t IEParam;
    UINT32           mode;
    UINT32           maxOff;
    UINT32           maxOn;
} PACK_END MrvlIETypes_MeasurementTiming_t;

typedef PACK_START struct
{
    /** Header */
    MrvlIEParamSet_t IEParam;
    UINT32           mode;
    UINT32           dwell;
    UINT32           maxOff;
    UINT32           minLink;
    UINT32           rspTimeout;
} PACK_END MrvlIETypes_ConfigScanTiming_t;

typedef PACK_START struct
{
    MrvlIEParamSet_t  IEParam;
    UINT8     KeyIndex;
    UINT8     IsDefaultIndex;
    UINT8     Value[1];
}PACK_END MrvlIETypes_WepKey_t;

typedef PACK_START struct
{
    MrvlIEParamSet_t  IEParam;
    UINT8             PMK[32];
} PACK_END MrvlIETypes_PMK_t;

typedef PACK_START struct
{
    MrvlIEParamSet_t  IEParam;
    UINT8             ssid[1];
} PACK_END MrvlIETypes_Ssid_Param_t;

typedef PACK_START  struct
{
    MrvlIEParamSet_t  IEParam;
    UINT8             Passphrase[64];
} PACK_END MrvlIETypes_Passphrase_t;

typedef PACK_START  struct
{
    MrvlIEParamSet_t  IEParam;
    UINT8             BSSID[6];
} PACK_END MrvlIETypes_BSSID_t;

typedef PACK_START struct
{
    UINT16 Type;
    UINT16 Length;
    IEEEtypes_MacAddr_t  Bssid;
    UINT16 Rsvd;
    SINT16 Rssi;   //Signal strength
    UINT16 Age;
    UINT32 QualifiedNeighborBitmap;
    UINT32 BlackListDuration;
} PACK_END MrvlIETypes_NeighbourEntry_t;

typedef PACK_START struct
{
    MrvlIEParamSet_t IEParam;
    UINT16 SearchMode;
    UINT16 State;
    UINT32 ScanPeriod;
} PACK_END MrvlIETypes_NeighbourScanPeriod_t;

typedef PACK_START struct
{
    MrvlIEParamSet_t IEParam;
    UINT8  Rssi;
    UINT8  Frequency;
} PACK_END MrvlIETypes_BeaconHighRssiThreshold_t;

typedef PACK_START struct
{
    MrvlIEParamSet_t IEParam;
    UINT8  Rssi;
    UINT8  Frequency;
} PACK_END MrvlIETypes_BeaconLowRssiThreshold_t;

typedef PACK_START struct
{
    MrvlIEParamSet_t IEParam;
    UINT8   Value;
    UINT8   Frequency;
} PACK_END MrvlIETypes_RoamingAgent_Threshold_t;

typedef PACK_START struct
{
    MrvlIEParamSet_t IEParam;
    UINT8   AssocReason;
} PACK_END MrvlIETypes_AssocReason_t;

typedef PACK_START struct
{
    MrvlIEParamSet_t IEParam;
    IEEEtypes_MacAddr_t macAddr;
    UINT8 txPauseState;
    UINT8 totalQueued;
} PACK_END MrvlIETypes_TxDataPause_t;

typedef PACK_START struct
{
    UINT16  startFreq;
    UINT8   chanWidth;
    UINT8   chanNum;
} PACK_END MrvlIEChannelDesc_t;

typedef PACK_START struct
{
    MrvlIEParamSet_t     IEParam;
    MrvlIEChannelDesc_t  chanDesc;
    UINT16               controlFlags;
    UINT16               reserved;
    UINT8                activePower;
    UINT8                mdMinPower;
    UINT8                mdMaxPower;
    UINT8                mdPower;
} PACK_END MrvlIETypes_OpChanControlDesc_t;

typedef PACK_START struct
{
    MrvlIEParamSet_t     IEParam;
    UINT32               chanGroupBitmap;
    ChanScanMode_t       scanMode;
    UINT8                numChan;
    MrvlIEChannelDesc_t  chanDesc[50];
} PACK_END MrvlIETypes_ChanGroupControl_t;

typedef PACK_START struct
{
    MrvlIEParamSet_t  IEParam;
    ChanBandInfo_t    ChanBandInfo[IEEEtypes_MAX_BSS_DESCRIPTS];
} PACK_END MrvlIEChanBandList_t;

typedef PACK_START struct
{
    MrvlIEParamSet_t         IEParam;
    IEEEtypes_MacAddr_t      srcAddr;
    IEEEtypes_MacAddr_t      dstAddr;
    IEEEtypes_ActionFrame_t  actionFrame;
} PACK_END MrvlIEActionFrame_t;

typedef PACK_START struct
{
    MrvlIEParamSet_t  IEParam;
    HtEntry_t         htEntry[IEEEtypes_MAX_BSS_DESCRIPTS];
} PACK_END MrvlIEHtList_t;

/* This struct is used in ROM code and all the fields of
** this should be kept intact
*/
typedef PACK_START struct
{
    MrvlIEParamSet_t    IEParam;
    UINT16 bmpRateOfHRDSSS;
    UINT16 bmpRateOfOFDM;
    UINT32 bmpRateOfHT_DW0;
    UINT32 bmpRateOfHT_DW1;
    UINT32 bmpRateOfHT_DW2;
    UINT32 bmpRateOfHT_DW3;
#ifdef DOT11AC
    UINT16 bmpRateOfVHT[8]; //per SS
#endif
} PACK_END MrvlIE_TxRateScope_t;

typedef PACK_START struct
{
    UINT8         mod_class;
    UINT8         rate;
    UINT8         attemptLimit;
    UINT8         reserved;
} PACK_END MrvlIE_RateInfoEntry_t;

typedef PACK_START struct
{
    MrvlIEParamSet_t    IEParam;
    MrvlIE_RateInfoEntry_t rate_info[8];
} PACK_END MrvlIE_RateDropTable_t;

typedef PACK_START struct
{
    MrvlIEParamSet_t        IEParam;
    UINT32                  mode;
    // for 1x1 11n, 9 HT rate, 8 OFDM rate, 4 DSSS rate
    MrvlIE_RateDropTable_t rateDropTbls[9+8+4];
} PACK_END MrvlIE_RateDropPattern_t;

#ifdef USB_FRAME_AGGR

#define USB_TX_AGGR_ENABLE ( 1 << 1 )
#define USB_RX_AGGR_ENABLE ( 1 << 0 )

#define USB_RX_AGGR_MODE_MASK   ( 1 << 0 )
#define USB_RX_AGGR_MODE_SIZE  (1)
#define USB_RX_AGGR_MODE_NUM   (0)

typedef PACK_START struct
{
    MrvlIEParamSet_t IEParam;    
    UINT16  enable;
    UINT16  rx_mode;
    UINT16  rx_align;
    UINT16  rx_max;
    UINT16  rx_timeout;
    UINT16  tx_mode;
    UINT16  tx_align;
} PACK_END MrvlIE_USBAggrTLV_t;

extern MrvlIE_USBAggrTLV_t g_Aggr_Conf ;

#endif

typedef PACK_START struct
{
    UINT8               mod_class;
    UINT8               firstRateCode;
    UINT8               lastRateCode;
    SINT8               power_step;
    SINT8               min_power;
    SINT8               max_power;
    UINT8               ht_bandwidth;
    UINT8               reserved[1];
} PACK_END MrvlIE_PowerGroupEntry_t;

#define MRVL_MAX_PWR_GROUP       15
typedef PACK_START struct
{
    MrvlIEParamSet_t         IEParam;
    MrvlIE_PowerGroupEntry_t PowerGroup[MRVL_MAX_PWR_GROUP];
} PACK_END MrvlIE_PowerGroup_t;

#ifdef AP_STA_PS
typedef PACK_START struct
{
    MrvlIEParamSet_t    IEParam;
    UINT16              NullPktInterval;
    UINT16              numDtims;
    UINT16              BCNMissTimeOut;
    UINT16              LocalListenInterval;
    UINT16              AdhocAwakePeriod;
    UINT16              PS_mode;
    UINT16              DelayToPS;
} MrvlIETypes_StaSleepParams_t ;

typedef PACK_START struct
{
    MrvlIEParamSet_t    IEParam;
    UINT16              idleTime;
} MrvlIETypes_AutoDeepSleepParams_t ;
#endif

#ifdef MESH
typedef PACK_START struct _MrvlMeshIE_Tlv_t
{
    MrvlIEParamSet_t hdr;
    IEEEtypes_VendorSpecific_MeshIE_t meshIE;

} PACK_END MrvlMeshIE_Tlv_t;
#endif

typedef PACK_START struct
{
    MrvlIEParamSet_t IEParam;
    UINT8 RBCMode;
    UINT8 Reserved[3];
} PACK_END MrvlIERobustCoex_t;

typedef PACK_START struct
{
    MrvlIEParamSet_t IEParam;
    UINT16 Mode;
    UINT16 Reserved;
    UINT32 BTTime;
    UINT32 Period;
} PACK_END MrvlIERobustCoexPeriod_t;

typedef PACK_START struct
{
    MrvlIEParamSet_t IEParam;
    UINT8        staMacAddr[IEEEtypes_ADDRESS_SIZE];
    IEEEtypes_IE_Param_t    IeBuf;
}
PACK_END MrvlIEHostWakeStaDBCfg;

typedef PACK_START struct
{
    MrvlIEParamSet_t IEParam;
    UINT16           ouiCmpLen;
    UINT8            ouiBuf[6];
}
PACK_END MrvlIEHostWakeOuiCfg;

#ifdef MICRO_AP_MODE
/* This struct is used in ROM and should not be changed at all */
typedef PACK_START struct
{
    MrvlIEParamSet_t hdr;
    IEEEtypes_MacAddr_t macAddr;
    UINT8 pwrMode;
    SINT8 rssi;
} PACK_END MrvlIEStaInfo_t;
#endif

typedef struct
{
    MrvlIEParamSet_t Hdr;
    uint16 protocol;
    uint8 cipher;
    uint8 reserved;
} MrvlIETypes_PwkCipher_t;

typedef struct
{
    MrvlIEParamSet_t Hdr;
    uint8 cipher;
    uint8 reserved;
} MrvlIETypes_GwkCipher_t;

typedef PACK_START struct
{
    uint8  modGroup;
    uint8  txPower;
} PACK_END MrvlIE_ChanTrpcEntry_t;

typedef PACK_START struct
{
    MrvlIEParamSet_t Hdr;
    MrvlIEChannelDesc_t  chanDesc;
    MrvlIE_ChanTrpcEntry_t  data[1];
}
PACK_END MrvlIETypes_ChanTrpcCfg_t;

typedef PACK_START struct
{
    MrvlIEParamSet_t    IEParam;
    IEEEtypes_MacAddr_t mac[IEEEtypes_MAX_BSS_DESCRIPTS];
} PACK_END MrvlIETypes_MacAddr_t;

#ifdef AP_BTCOEX
typedef enum _tagScoCoexBtTraffic
{
    ONLY_SCO,
    ACL_BEFORE_SCO,
    ACL_AFTER_SCO,
    BT_TRAFFIC_RESERVED,
    BT_TRAFFIC_MAX
} ScoCoexBtTraffic_e;

typedef PACK_START struct
{
    MrvlIEParamSet_t IEParam;
    UINT32 configBitmap; /* Bit    0 : overrideCts2SelfProtection
                         ** Bit 1-31 : Reserved
                         */
    UINT32 apStaBtCoexEnabled;
    UINT32 reserved[3]; /* For future use. */
} PACK_END MrvlIETypes_ApBTCoexCommonConfig_t;

typedef PACK_START struct
{
    MrvlIEParamSet_t IEParam;
    UINT16 protectionFrmQTime[BT_TRAFFIC_MAX]; /* Index 0 for ONLY_SCO
                                               **       1 for ACL_BEFORE_SCO
                                               **       2 for ACL_AFTER_SCO
                                               **       3 is Reserved
                                               */
    UINT16 protectionFrmRate;
    UINT16 aclFrequency;
    UINT32 reserved[4]; /* For future use. */
} PACK_END MrvlIETypes_ApBTCoexScoConfig_t;

typedef PACK_START struct
{
    MrvlIEParamSet_t IEParam;
    UINT16 enabled;
    UINT16 btTime;
    UINT16 wlanTime;
    UINT16 protectionFrmRate;
    UINT32 reserved[4]; /* For future use. */
} PACK_END MrvlIETypes_ApBTCoexAclConfig_t;

typedef PACK_START struct
{
    MrvlIEParamSet_t IEParam;
    UINT32 nullNotSent;
    UINT32 numOfNullQueued;
    UINT32 nullNotQueued;
    UINT32 numOfCfEndQueued;
    UINT32 cfEndNotQueued;
    UINT32 nullAllocationFail;
    UINT32 cfEndAllocationFail;
    UINT32 reserved[8]; /* For future use. */
} PACK_END MrvlIETypes_ApBTCoexStats_t;
#endif //AP_BTCOEX

typedef PACK_START struct
{
    MrvlIEParamSet_t Hdr;

    IEEEtypes_MacAddr_t macAddr;
    UINT8 tid;
    UINT8 reserved;
    UINT16 startSeqNum;

    UINT16 bitMapLen;
    UINT8 bitMap[1];
}
PACK_END MrvlIETypes_RxBaSync_t;

#if defined(ADHOC_OVER_IP)
typedef PACK_START struct
{
    MrvlIEParamSet_t Hdr;

    // 0 - Standard Adhoc Mode;; 1 - Ad-hoc over IP mode
    UINT16 ibssMode;
} PACK_END MrvlIETypes_AoIPIbssMode_t;

typedef PACK_START struct
{
    MrvlIEParamSet_t Hdr;

    UINT8 state;
    UINT8 reserved;
    IEEEtypes_MacAddr_t realMacAddr;
    IEEEtypes_MacAddr_t virtualMacAddr;
} PACK_END MrvlIETypes_AoIPStaInfo_t;

typedef PACK_START struct
{
    MrvlIEParamSet_t Hdr;

    // 0 - Use real mac address; 1 - Use locally administrated address
    UINT8 addrMode;
} PACK_END MrvlIETypes_AoIPRemoteAddrMode_t;

typedef PACK_START struct
{
    MrvlIEParamSet_t Hdr;

    // 0 - Get Peers
    // 1 - Add Peers
    // 2 - Delete Peers
    UINT16 action;

    // Only used if action == delete/get, ignored otherwise
    // BIT0 - all local peers
    // BIT1 - all remote peers
    // BIT2 - delete specified entries
    UINT8 option;

    // Number of addresses to add or remove
    UINT8 numAddr;

    UINT8 body[1];
} PACK_END MrvlIETypes_AoIPManagePeers_t;

#endif // ADHOC_OVER_IP

#endif //_TLV_H_
