/******************** (c) Marvell Semiconductor, Inc., 2001 *******************
 *
 * Purpose:
 *    This file contains the definitions of the initialization file
 *
 * Public Procedures:
 *
 * Private Procedures:
 *
 * Notes:
 *    None.
 *
 *****************************************************************************/

#ifndef W81CONNMGR_H__
#define W81CONNMGR_H__

//=============================================================================
//                               INCLUDE FILES
//=============================================================================
#if 0
#include "mdev.h"
#include "mlme.h"
#include "microTimer.h"
#endif
#include "keyApiStaTypes.h"
#include "wltypes.h"
#include "keyMgmtStaTypes.h"
#include "keyMgmtSta.h"
#include "macMgmtMain.h"
#if 0
#include "linkadapt_types.h"
#include "subscribe_event_types.h"
#include "linkstats_types.h"
#include "wmm_types.h"
#include "dot11n_types.h"
#include "wl_hal_rom.h"
#include "led_drv_rom.h"

#ifdef BFMER_SUPPORT
#include "txbf_types.h"
#endif

#include "assocInfoType.h"

#ifdef DOT11AC
#include "dot11ac_types.h"
#endif

#include "led.h"
//=============================================================================
//                            PUBLIC DEFINITIONS
//=============================================================================

// Beacon/PrbRsp buffers for STA only
#define NUM_STA_BCN         1
#define NUM_AP_BCN          0
#define NUM_BTAMP_BCN       0
#define NUM_WFD_BCN         0
#define NUM_WFD_PORT        0
#define NUM_SIMUL_INFRA_ADHOC_BCN 0


// Overrides for each feature
#if defined(MICRO_AP_MODE)
#undef NUM_AP_BCN
#ifdef USER_MAX_UAP_NUM
#define NUM_AP_BCN      USER_MAX_UAP_NUM
#elif defined(REDSTART) 
#define NUM_AP_BCN          1
#else
#define NUM_AP_BCN          2
#endif
#endif

#if defined(BTAMP)
#undef NUM_BTAMP_BCN
#define NUM_BTAMP_BCN       1
#endif

#if defined(WIFI_DIRECT)
#undef NUM_WFD_BCN
#undef NUM_WFD_PORT
#ifndef MAX_WFD_INTFS
#define MAX_WFD_INTFS       1
#endif
#define NUM_WFD_BCN         1         
#define NUM_WFD_PORT        MAX_WFD_INTFS 

#if defined(WIN8_WIFIDIRECT)
#undef NUM_AP_BCN
#define NUM_AP_BCN          1

#undef NUM_WFD_BCN
#define NUM_WFD_BCN  3
#undef NUM_WFD_PORT
#define NUM_WFD_PORT 3

#endif

#endif

#if defined(SIMUL_INFRA_ADHOC)
#undef NUM_SIMUL_INFRA_ADHOC_BCN
#define NUM_SIMUL_INFRA_ADHOC_BCN 1
#endif

//No of interfaces supported by AP/BTAMP/WFD/Adhoc can not be more than its
// beacon buffers. Here NUM_STA_BCN represents adhoc beacon requirement.
// +1 for infra interface
#define MAX_SUPPORTED_INTERFACES  (NUM_AP_BCN + NUM_BTAMP_BCN \
                                   + NUM_WFD_PORT + NUM_STA_BCN + 1)

#define MAX_SUPPORTED_PARENTS   (NUM_AP_BCN + NUM_BTAMP_BCN \
                                   + NUM_WFD_PORT + NUM_STA_BCN + NUM_SIMUL_INFRA_ADHOC_BCN )

//Since uAP and adhoc can not co-exist as MBSS mode does not support adhoc
// distributed beaconing, MAX_SUPPORTED_BSS can be defined as MAX of
// NUM_STA_BCN and NUM_AP_BCN
#define MAX_SUPPORTED_BSS     (MAX(NUM_STA_BCN + NUM_BTAMP_BCN + NUM_WFD_BCN, \
                                   NUM_AP_BCN  + NUM_BTAMP_BCN + NUM_WFD_BCN))

#define MAX_SUPPORTED_APINFO      (NUM_AP_BCN + NUM_WFD_PORT)
#define MAX_PROBE_RSP_BUF         (MAX_SUPPORTED_BSS + NUM_WFD_PORT - NUM_WFD_BCN)

#if (MAX_SUPPORTED_BSS > MBSS_MAX_HW_SUPP_BSS)
#error "MAX_SUPPORTED_BSS should not be more than MBSS_MAX_HW_SUPP_BSS(8)."
#endif

#if (MAX_SUPPORTED_BSS > MAX_PROBE_RSP_BUF)
#error "MAX_SUPPORTED_BSS should not be more than MAX_PROBE_RSP_BUF."
#endif

// max connections supported by AP
#if defined(MICRO_AP_MODE)

#ifdef USER_AP_MAX_STA_CNT
#if USER_AP_MAX_STA_CNT > 32
#error "USER_AP_MAX_STA_CNT should not be more then 32"
#else
#define AP_MAX_STATION      USER_AP_MAX_STA_CNT
#endif    
#else
#if defined(REDSTART) || defined(W8801)
#ifdef VISTA_802_11_DRIVER_INTERFACE
#define AP_MAX_STATION      6
#else
#define AP_MAX_STATION      8
#endif
#else
#define AP_MAX_STATION      10
#endif
#endif
#define CM_UAP_CNT          (AP_MAX_STATION + MAX_SUPPORTED_PARENTS - 1)

#else

#define AP_MAX_STATION      0
#define CM_UAP_CNT          0

#endif
#endif
//use compile time desired station count if possible
#ifndef MAX_STA_CNT
#if defined (VISTA_802_11_DRIVER_INTERFACE)
#define MAX_STA_CNT          8
#elif defined (ASSOC_AGENT_OFFLOAD)
#define MAX_STA_CNT          0
#elif defined(ENABLE_16_ADHOC_PEERS)
#define MAX_STA_CNT          16 
#else
#define MAX_STA_CNT          9   // Keeping the default count to 9
                                 // If any feature pack needs different value, then the macro MAX_STA_CNT needs to be
                                 // defined accordingly in FP definition file
#endif
#endif
                             /* -1, as the first peer connPtr will be 
                              * shared with Parents connPtr */
#define CM_STA_CNT          (MAX_STA_CNT + MAX_SUPPORTED_PARENTS - 1)

#define CM_MAX_CONNECTIONS  2

#if 0
#define MAX_TX_MDEVS        2
#define MAX_RX_MDEVS        2

// Saad TBD: Find a better place for below

#if defined(TORUS_FIXED_TX_POWER)
#define MAX_DEFAULT_TPC        9
#else
#define MAX_DEFAULT_TPC        0x00FF
#endif

#define AP_DEFAULT_CHANNEL_BAND Band_2_4_GHz
#define AP_DEFAULT_WIDTH        ChanWidth_20_MHz
#define AP_DEFAULT_CHANNEL      6

struct WifiDirect_Globals;

typedef RxFrameAction_e (*mgtMsgHandler_t)(BufferDesc_t *pMgtFrameBuf,
                                           WLAN_RX_INFO* pRxInfo);


typedef void (*ctlMsgHandler_t)(struct cm_ConnectionInfo *connPtr, 
                                WLAN_RX_FRAME *pRxFrame);
typedef struct
{
    AssocInfo_t     priorAp;
    UINT8           priorAssocState;
}priorAssocData_t;

//redefined here as including dozer.h is creating circular dependency
typedef UINT32 DozerId_t;

typedef struct cm_interface_t
{
    struct cm_interface_t *next;  // Let it be same as that of ListItem structure
    struct cm_interface_t *prev;
    struct cm_ConnectionInfo *connPtr;

    UINT8 Channel;
    BandConfig_t BandConfig;
    UINT8 BssStoppedInternal;
    UINT8 BssPaused;

    // should start at 8byte boundary otherwise unnecessary
    // padding will be introduced by compiler
    UINT32 ChanTime;
    UINT64 BcnTsf;      //ex-ap tsf
    UINT64 BcnLocalTsf; //local tsf when bcn is tx'ed/rx'ed
    UINT8 HwQPktCnt;    //no of pkts in HwQ belonging to this BSS
    UINT8 IntfBufWeight;
    UINT8 Reserved[2];
    DozerId_t chanRemainDozerId;
    UINT32 chanRemainActiveCnt;

#ifdef DOT11N
    Ht_Tx_Cap_from_host_t  HtTxCap;
    Ht_Tx_Cap_from_host_t  HtTxCap_5G;

    Ht_Tx_Info_from_host_t HtTxInfo;
    Ht_Tx_Info_from_host_t HtTxInfo_5G;
    IEEEtypes_HT_Information_t HtInfoData;

    host_RejAddbaReq_Bitmap_t   RejAddbaReq_conditions;
#endif

#ifdef DOT11AC
    IEEEtypes_VHT_Cap_Info_t  VhtTxCap_2G;
    IEEEtypes_VHT_Cap_Info_t  VhtTxCap_5G;
    IEEEtypes_VHT_Operation_t VhtOperData;

    UINT8 VhtTxCapBwEn_5G  :1;
    UINT8 VhtTxCapReserved :7;
#endif

#ifdef DOT11W    
    UINT8  dot11RSNAProtectedMgmtFramesEnabled : 1;
    UINT8  dot11RSNAUnprotectedMgmtFramesAllowed : 1;    
    UINT8  reserved    : 6;
    UINT16 saq_action_trans_id;
#endif

#ifdef MIB_STATS
    MRVL_MIB_STATS mibStats;
#endif

#ifdef HOST_TXRX_MGMT_FRAME
    UINT32 mgmtFrameSubtypeFwdEn; 
    UINT32 mgmtFrameInternalEn; 
#endif

    /* LED Specific properties */
    UINT8 LEDNum;
    LEDBehaviour_t LEDBehaviour[MAX_FW_STATE];
    UINT8 FWState;
#ifdef AUTO_TX
    UINT16 selfDetectThreshold;
    UINT16 selfDetectAccumulate;
    UINT64 lastUpdateTsf;   
    UINT32 maxDetectPeriod; /* seconds */
#endif

    UINT64 sendNullPktTsf;   
    UINT32 sendNullPktPeriod; /* seconds */
#ifdef  HOST_SAFE_MODE
    Boolean HostSafeMode;
#endif
    UINT16 floorRate;
#ifdef HOST_BASED_TDLS
    UINT8 activeTdlsLinks;
#endif
}cm_interface_t;

typedef struct
{
    dot11nConfig_t Dot11nConfig;
    UINT8   BcnIndicated;

    UINT8           txQ;
    UINT8           txUserPri;    
    UINT8           streamId;

    priorAssocData_t     assocData;
#ifdef DOT11AC
    dot11acConfig_t Dot11acConfig;
    IEEEtypes_VHT_OpMode_t vhtOpMode;
#endif
}infraSpecificData_t;

typedef struct
{
    adhocInfo_t                *adhocInfo;

    /* control flags */
    UINT32 enableCoalesce  : 1;
    UINT32 bcn_update      : 1;
    UINT32 wepIcvEventSent : 4;
    UINT32 reserved        :26;

    UINT64 coalesceTsf;
	
    //Last Seen TSF
    UINT32                      LastActivityTSF;
    UINT8                       BcnIndicated;
    UINT8                       peerRateSetAvailable;
#ifdef ADHOC_OVER_IP
    void                        *pAoIPGameInfo;
    UINT8                       bcnPrbRspRxed;
#endif
    Cipher_t mKeyType;
    Cipher_t uKeyType;
    IEEEtypes_CapInfo_t CapInfo;
#if defined(ADHOC_WPA2) || defined(WPSE) 
    cipher_key_buf_t *pGtkKeyBuf;
#endif
}adhocSpecificData_t;
#endif

//#ifdef MICRO_AP_MODE
typedef struct
{
    apInfo_t *apInfo;
    BufferDesc_t *apInfoBuffDesc;
    ChanBandInfo_t chanBandInfo;
    staData_t staData;
} apSpecificData_t;
//#endif

#if 0
typedef enum
{
    WIFI_DIRECT_MODE_NONE=0,
    WIFI_DIRECT_MODE_DEVICE,
    WIFI_DIRECT_MODE_GO,
    WIFI_DIRECT_MODE_CLIENT,
    WIFI_DIRECT_MODE_NOT_SPECIFIED,
} CON_OP_MODE_e;

#ifdef WIFI_DIRECT
typedef enum
{
    WIFI_DIRECT_STATE_NONE=0,
    WIFI_DIRECT_STATE_SCAN,
    WIFI_DIRECT_STATE_LISTEN,
    WIFI_DIRECT_STATE_SEARCH,
    WIFI_DIRECT_STATE_GO_FORMATION,
    WIFI_DIRECT_STATE_PROCEDURE_ON,
    WIFI_DIRECT_STATE_LISTEN_ONLY
}WIFI_DIRECT_STATE_e;

typedef enum
{
    INFO_NONE=0,
    INFO_LISTEN_ONLY, 
    INFO_FIND,

    INFO_GO_NEG_REQ_SENT,
    INFO_GO_NEG_REQ_RCVD,
    INFO_GO_NEG_RSP_SENT,
    INFO_GO_NEG_RSP_RCVD,
    INFO_GO_NEG_CFM_SENT,
    INFO_GO_NEG_CFM_RCVD,

    INFO_SD_REQ_SENT,
    INFO_SD_REQ_RCVD,
    INFO_SD_RSP_SENT,
    INFO_SD_RSP_RCVD,

    INFO_PD_REQ_SENT,
    INFO_PD_REQ_RCVD,
    INFO_PD_RSP_SENT,
    INFO_PD_RSP_RCVD,

}WIFI_DIRECT_STATE_INFO_e;

typedef enum
{
    evt_none=0,
    evt_go_neg_req_sent,
    evt_go_neg_req_rcvd,
    evt_go_neg_rsp_sent,
    evt_go_neg_rsp_rcvd,
    evt_go_neg_cnfm_sent,
    evt_go_neg_cnfm_rcvd,

    evt_sd_req_sent,
    evt_sd_req_rcvd,
    evt_sd_rsp_sent,
    evt_sd_rsp_rcvd,

    evt_pd_req_sent,
    evt_pd_req_rcvd,
    evt_pd_rsp_sent,
    evt_pd_rsp_rcvd
}WIFI_DIRECT_STATE_EVT_e;

typedef struct
{
    txQingInfo_t psQueue;
} wfdStaSpecific_t;

typedef struct
{
    uint8 temp;
} wfdGOSpecific_t;

typedef struct
{
    union
    {
        wfdStaSpecific_t    staData;
        wfdGOSpecific_t     goData;
    } specDat;
} wfdCommonData_t;

typedef struct
{
    WIFI_DIRECT_STATE_e         DeviceState;
//    WIFI_DIRECT_STATE_e         prevDeviceState;
//    WIFI_DIRECT_STATE_e         nextDeviceState;
    WIFI_DIRECT_STATE_INFO_e    DeviceStateInfo;
    struct WifiDirect_Globals*  pWifiDirectGlobals;
    apSpecificData_t apData;
    infraSpecificData_t infraData;
    wfdCommonData_t wfdCommData;
} wfdSpecificData_t;

#endif /* WIFI_DIRECT */

typedef struct
{
    UINT8 dummy;
    UINT8 dummy1;
}meshSpecificData_t;

#if (defined TDLS)
typedef struct tdlsSpecificData {
    BOOLEAN                              isUsed;
    dot11nConfig_t                       Dot11nConfig;
    IEEEtypes_HT_Information_t           HtInfoData;
    MicroTimerId_t                       setupTimer;
    UINT32                               setupTimeout;
    UINT32                               succTxFailureCount;
    UINT32                               keyLifeTime;
    Timer                                keyLifetimeExpiryTimer;
    UINT8                                dialogToken;
    UINT8                                initiator;
    IEEEtypes_RSNElement_t               RSNIE;
    IEEEtypes_TimeoutIntervalElement_t   TimeoutIE;
    IEEEtypes_FastBssTransElement_t      FTE;
    IEEEtypes_HT_Capability_t            peerHtCapIE;
    tdlsKeyMgmtInfoSta_t                 keyMgmtInfo;
    UINT16                               selfPwrMode;
    /* Seq number reuired for duplicate 
    detection of TDLS management frames: 
    CSReq, CSRsp, PTIReq, PTIResp*/
    UINT16                                seqNum[4];
#ifdef TDLS_UAPSD
    staData_t                            staData;
    IEEEtypes_PUBufferStatus_t           PUBufStatus;
    MicroTimerId_t                       PTIRespTimeoutTimer;
    MicroTimerId_t                       SendPTITimer;
    BOOLEAN                              needToRestartSendPTITimer;
    BOOLEAN                              lastPktIndFromHost;
    UINT8                                isRevUSPInProgress;
    BOOLEAN                              PTISentOut;
#endif
#ifdef TDLS_CHANNELSWITCH
    BOOLEAN                              peerCSCap;
#endif
#ifdef DOT11AC
    IEEEtypes_VHT_Operation_t            VhtOperData;
    IEEEtypes_VHT_OpMode_t               vhtOpMode;
#endif
}tdlsSpecificData_t;
#endif

#ifdef HOST_BASED_TDLS
typedef struct 
{
    IEEEtypes_HT_Information_t           HtInfoData;
    IEEEtypes_HT_Capability_t            peerHtCapIE;
    IEEEtypes_QOS_Capability_t           QosCap;
    UINT16                               selfPwrMode;
    UINT16                               succTxFailureCount;
    UINT16                               sendNullPktIntvlCfg;
    UINT16                               sendNullPktIntvl;
    UINT32                               sndRecvPktCnt;    
    UINT64                               sndRecvPktTsf;    
#ifdef DOT11AC
    IEEEtypes_VHT_Operation_t            VhtOperData;
    IEEEtypes_VHT_OpMode_t               vhtOpMode;
#endif
}tdlsSpecificData_t;
#endif
//
// AMP Specific Data
//
#define PRE_FILLED_DATA_HDR_LEN                     50

typedef PACK_START struct
{
    UINT8 ScheduleKnown;
    UINT8 NumReports;
    UINT32 StartTime;
    UINT32 Duration;
    UINT32 Periodicity;
} PACK_END Pal802ActivityReport_t;

#if defined(BTAMP)
typedef struct
{
    UINT8                   SsIdLen;
    char                    *SsId;
    char                    peerSsId[32];
    UINT8                   Role;
    IEEEtypes_BcnInterval_t bcn_interval;
    UINT64                  peerTSF;
    UINT8                   peerTSFUpdateEnabled;
    Pal802ActivityReport_t  ActivityReport;
    UINT8                   State;
    UINT8                   PreFilledDataHdr[PRE_FILLED_DATA_HDR_LEN];
    Timer                   LinkSupervisionTimer;
    MicroTimerId_t          ActivityReportTimer;
    UINT8                   LinkTimeoutCount;
    IEEEtypes_RSNElement_t  RSNIE;
    dot11nConfig_t          Dot11nConfig;
    priorAssocData_t     assocData;
#ifdef DOT11AC
    dot11acConfig_t          Dot11acConfig;
#endif
} btAmpSpecificData_t;
#endif

typedef enum
{
    HW_UPDATE_REASON_ASSOC_SETUP,
    HW_UPDATE_REASON_STA_IDLE,
    HW_UPDATE_REASON_UBSS_START,
    HW_UPDATE_REASON_UBSS_STOP,
    HW_UPDATE_REASON_MESH_START,
    HW_UPDATE_REASON_MESH_STOP,
    HW_UPDATE_REASON_DOZER_START,
    HW_UPDATE_REASON_DOZER_RESTORE,
    HW_UPDATE_REASON_ADHOC_START,
    HW_UPDATE_REASON_ADHOC_JOIN,
    HW_UPDATE_REASON_ADHOC_COALESCING,
    HW_UPDATE_REASON_ADHOC_STOP,
    HW_UPDATE_REASON_EXIT_FROM_SYNC_IDLE,
    HW_UPDATE_REASON_BTAMP_START,
    HW_UPDATE_REASON_BTAMP_STOP,
    HW_UPDATE_REASON_WIFIDIRECT_DEVICE_START,
    HW_UPDATE_REASON_WIFIDIRECT_DEVICE_STOP,
    HW_UPDATE_REASON_TDLS_LINK_SETUP,
    HW_UPDATE_REASON_TDLS_LINK_TEARDOWN
} HwUpdateReason_e;
#endif

typedef struct cmFlags
{
    UINT32  gDataTrafficEnabled              :1;
    UINT32  bCurPktTxEnabled                 :1;
    UINT32  RSNEnabled                       :1;
    UINT32  AlwaysSendRTS                    :1;
    UINT32  WAPIEnabled                      :1;
    UINT32  Dot11nEnabled                    :1;
    UINT32  dot11HighThroughputOptionEnabled :1;
    UINT32  dot11FortyMHzOperationEnabled    :1;
    UINT32  dot11dEn                         :1;
    UINT32  dot11hEn                         :1;
    UINT32  isParent                         :1;
    UINT32  WMMcapable                       :1;
    UINT32  AssociatedFlag                   :1;
    UINT32  dot11VHTOptionEnabled            :1;
    UINT32  dot11BW80MHzOperationEnabled     :1;
    UINT32  protect_mgt_frames               :1;
    UINT32  selfDetect                       :1;
    UINT32  controlledPortOpen               :1;
    UINT32  reserved                         :14;
} cmFlags_t;
//=============================================================================
//                                DATA STRUCTURES
//=============================================================================i
#if 0
typedef struct cm_ConnectionInfo
{
    // Common too ALL connections
    // ---------------------------
    //struct cm_ConnectionInfo *prevDBElement;
    //struct cm_ConnectionInfo *nextDBElement;
    UINT8 conType;
    CON_OP_MODE_e DeviceMode;
    UINT8 BssType;
    UINT8 BssNum;
    cmFlags_t cmFlags;
    // Host properties
    mdev_t *hostMdev;
    // State machines - Temporarily needed for scans etc. Offload wont need it
    // and then size of pseudo connection will become tiny
    SyncSrvSta mgtSync;
    mgtMsgHandler_t pMgtMsgHandler;
    ctlMsgHandler_t pCtlMsgHandler;
    // ---------------------------
    // Data below is for NON-pseudo connections

    UINT8 DataRate;
    sta_MacCtrl_t MacCtrl;
    macMgmtMlme_StaData_t macMgmtMlme_ThisStaData;
    CommonMlmeData_t comData;

    IEEEtypes_AuthAlg_t usedAuthType;
    IEEEtypes_AuthTransSeq_t nextAuthRxSeq;
    UINT8  hwBssIndex;

    txBcnInfo_t *pBcnTxInfo;
    BufferDesc_t *pBcnBufferDesc;

    txBcnInfo_t *pPrbRspTxInfo;
    BufferDesc_t *pPrbRspBufDesc;

    MRVL_RATEID_BITMAP bitmapBssBasicRateSet;
    UINT8  max11agBssBasicRATEID;
    UINT8  min11agBssBasicRATEID;
    UINT8  max11bBssBasicRATEID;
    UINT8  min11bBssBasicRATEID;
    // Beacons stats
    sint16 BeaconNF;            // Last Beacon Frame NF
    sint16 BeaconRSSI;          // Last Beacon Frame RSSI
    sint16 BeaconSNR;           // Last Beacon Frame SNR
#ifdef WINCE_OID_SUPPORT
    uint32 AvgRateIn1kbps[2];
    uint16 Rate_AvgWeight[2];
    UINT32 TxDataFramesCnt;
    UINT32 RxDataFramesCnt;
    uint16 Rate_Last[2];        //!< [0] for Tx, [1] for Rx
#endif

    // Link stats
    sint16 DataNF;
    sint16 DataRSSI;
    sint16 DataSNR;
    sint16 DataRssiAvg;
    sint16 DataNFAvg;
    // parameters from linkstats.c
    linkMonitorData_t linkMonitorData;
#ifdef LINKSTATS
    linkStatsData_t linkStatsData;
#endif
    NF_DEBOUNCE_FIFO NfDebounceFifo;
    RSSI_NF_STORAGE4AVG  AccuStor4RssiNf;

    uint16 Data_RssiAvgWeightExponent;  // default is 3 => 2^3=8
    uint16 Bcn_RssiAvgWeightExponent;   // default is 3 => 2^3=8
    uint16 mgmt_seqnum_prev;
    uint16 seqnum_prev;
    uint16 fragnum_prev;
    uint8 tid_prev;
    uint8 wep_icvEvent_info;                 //used for wep icv event
    IEEEtypes_MacMgmtStates_e smeMain_State;
//    IEEEtypes_MacMgmtStates_e smeMain_NextState;
//    IEEEtypes_SmeCmd_e smeCurrentCmd;
    IEEEtypes_MacAddr_t peerMacAddr;
#if defined(DOT11AC)
    UINT16 peerGidPAid; // bit[0:5] = GID, bit[6:14]: PAid, as in TxInfo
#endif
    IEEEtypes_AId_t AId;
    //linkadapt parameters
    UINT8 LinkSets_MaximalRate;
    UINT8 LinkSets_MinimalRate;
    UINT8 LinkSets_LowestCurrentRate;
    UINT8 LinkSets_HighestCurrentRate;
    LinkInfo_t PeerStnLinkInfo;
    Timer LinkAdaptTimer;
//    UINT32 LastActiveTxBA;
    UINT8 LA_Control;

    cipher_key_buf_t *pwTxCipherKeyBuf;
    cipher_key_buf_t *pwRxCipherKeyBuf;
    cipher_key_buf_t *gwCipherKeyBuf;
    cipher_key_buf_t *pwTxRxCipherKeyBuf;
#ifdef DOT11W
    /* IGTK key used for PMF (Protected Management Frames)   */ 
    cipher_key_buf_t *igwCipherKeyBuf;
	/* SAQuery Rsp timeout */   
	MicroTimerId_t   saqRspTimer;  
#ifdef RSN_REPLAY_ATTACK_PROTECTION
    UINT32           hiUniRxMgmtReplayCnt32;
    UINT16           loUniRxMgmtReplayCnt16;
#endif
#endif    
//    uint8 KeyId;
#ifdef MIB_STATS
    MRVL_MIB_STATS *pMibStats;
#endif
    SubscribeEventInfo_t * pSubscribeEventInfo[subscribeEvent_maxSubscribers];
    UINT16 report_subscribe_events;

    PeerChannelRecord_t lastPeerInfo;

    //Sequence number per TID
    UINT16 SequenceNum[WMM_MAX_TIDS];

#if defined(RSN_REPLAY_ATTACK_PROTECTION) && defined(WMM_IMPLEMENTED)
    /* Only support WMM not 802.11e, so, only need 4 replay counters
     */
    UINT32  hiWmmReplayCounter32[WMM_MAX_RX_PN_SUPPORTED];
    UINT16  loWmmReplayCounter16[WMM_MAX_RX_PN_SUPPORTED];    
#endif


#ifdef DOT11H
    UINT8 localConstraint;
#endif
    PWR_in_dBm  PeerTxPowerMax;

#ifdef AP_WMM_PS
    BOOLEAN WMMPScapable;
#endif

    IEEEtypes_MacAddr_t localMacAddr;
    UINT8 multiAddrTableInd;
#ifdef BFMER_SUPPORT
    TxBfInfo_t PeerTxBfInfo;
#endif
#ifdef TDLS
    UINT16 txActivity;
    UINT16 rxActivity;
    UINT16 pastTxActivity;
    UINT16 pastRxActivity;
#endif
    struct supplicantData *suppData;
    cm_interface_t *pParent;

    //This pointer is STA specific but is placed here so 
    //that we can avoid one more level of check between WFD / infraSTA
    WMMGlobals *pWmmGlobal;

#ifdef MULTI_CH_SW
    BOOLEAN chanRemainActive;
#endif
    BOOLEAN wpsAssoc;

    union
    {
        infraSpecificData_t infraData;
        adhocSpecificData_t adhocData;
#if (defined TDLS)
        struct tdlsSpecificData* tdlsData;
#endif
#ifdef HOST_BASED_TDLS
        tdlsSpecificData_t  tdlsData;
#endif
#ifdef MICRO_AP_MODE
        apSpecificData_t    apData;
#endif
#ifdef WIFI_DIRECT
        wfdSpecificData_t   wfdData;
#endif /* WIFI_DIRECT */

        meshSpecificData_t  meshData;
#if defined(BTAMP)
        btAmpSpecificData_t btAmpData;
#endif
    } specDat;
} cm_ConnectionInfo_t;
#endif

typedef PACK_START struct cm_ConnectionInfo
{
    UINT8 conType;

    struct supplicantData *suppData;
    CommonMlmeData_t comData;
    IEEEtypes_MacAddr_t peerMacAddr;
    IEEEtypes_MacAddr_t localMacAddr;
    union PACK_START
    {
//#ifdef MICRO_AP_MODE
	apSpecificData_t    apData;
//#endif
    } PACK_END specDat;
    cipher_key_buf_t TxRxCipherKeyBuf;
} PACK_END cm_ConnectionInfo_t;

typedef enum
{
    CONNECTION_TYPE_INFRA = 0,
    CONNECTION_TYPE_ADHOC,
    CONNECTION_TYPE_AP,
    CONNECTION_TYPE_WFD,
    CONNECTION_TYPE_BTAMP,
    CONNECTION_TYPE_PSEUDO,
    CONNECTION_TYPE_TDLS,
    /* Add new type above this line */
    CONNECTION_TYPE_MAX
} ConnectionTypes_t;

#if 0
typedef enum
{
    BSS_TYPE_CLIENT = 0,
    BSS_TYPE_AP,
    BSS_TYPE_WFD,
    BSS_TYPE_BTAMP = 0x7,
} HostBssType_t;

typedef enum
{
    PS_TYPE_NONE = 0,
    PS_TYPE_INFRA,
    PS_TYPE_ADHOC,
    PS_TYPE_AP
} PowersaveTypes_t;

typedef struct cm_mbssResourceInfo
{
    cm_ConnectionInfo_t *mbssConnPtr;
} cm_mbssResourceInfo_t;

typedef struct cm_HwInfoStruct
{
    UINT8 allocatedHwBssMap;   // Add more bits if we need to
                               //support more than 8 BSS
    UINT32 allocatedMultiAddrMap;
    UINT32 noOfBssStartedByHost;
    UINT32 noOfAPBssPaused;
    UINT32 mbssHcfBcnPeriod;
    cm_mbssResourceInfo_t mbssResourceInfo[MAX_SUPPORTED_BSS];
} cm_HwInfoStruct_t;
//=============================================================================
//                               FUNCTION PROTOTYPES
//=============================================================================
// Connection manager API function prototypes
#endif
extern Status_e cm_InitConnectionManager(void);

extern cm_ConnectionInfo_t* cm_InitConnection(UINT8               conType,
                                              UINT8               bssType,
                                              UINT8               bssNum,
                                              IEEEtypes_MacAddr_t *bssId,
                                              IEEEtypes_MacAddr_t *peerMacAddr,
                                              UINT8               channel,
                                              mdev_t              *hostMdev);

extern Status_e cm_UpdateConnection(cm_ConnectionInfo_t *connPtr,
                                    UINT8                conType,
                                    IEEEtypes_MacAddr_t *bssId,
                                    IEEEtypes_MacAddr_t *peerMacAddr);
#ifdef MICRO_AP_MODE
extern cm_ConnectionInfo_t* cm_DupApConnection(IEEEtypes_MacAddr_t peerAddr,
                                               cm_ConnectionInfo_t* connPtr);
#endif
extern void cm_DeleteConnection(cm_ConnectionInfo_t *connPtr);

extern cm_ConnectionInfo_t* cm_FindConnection(IEEEtypes_MacAddr_t ,
                                              UINT8 conType);

extern cm_ConnectionInfo_t* cm_FindConnectionByHostMdev(mdev_t *mdev);

extern cm_ConnectionInfo_t* cm_FindConnectionByPeerMacAddr(
                                  IEEEtypes_MacAddr_t *peerMacAddr
                                                          );
extern cm_ConnectionInfo_t* cm_FindConnectionByBssTypeAndBssNum(UINT8 BssType,
                                                                UINT8 BssNum);

extern cm_ConnectionInfo_t* cm_FindNextConnectionByBssTypeAndBssNum(
                                                cm_ConnectionInfo_t *connPtr,
                                                UINT8 BssType,
                                                UINT8 BssNum);

extern cm_ConnectionInfo_t* cm_FindConnectionByConTypeAndBssId(UINT8 conType,
                                                IEEEtypes_MacAddr_t *BssId);

extern cm_ConnectionInfo_t* cm_FindConnectionByAdhocSSID(
                                                IEEEtypes_SsIdElement_t *SsId);


extern cm_ConnectionInfo_t* cm_FindConnectionByPeerMacAddrAndConType(
                                            IEEEtypes_MacAddr_t *peerMacAddr,
                                            UINT8 conType);

extern Status_e cm_DeCloneByBssNumAndBssType(cm_ConnectionInfo_t *connPtr,
                                             UINT8 BssNum,
                                             UINT8 BssType
                                            );
extern cm_ConnectionInfo_t* cm_FindConnectionByConTypeAndBssNum(UINT8 conType,
                                                         UINT8 BssNum);

extern cm_ConnectionInfo_t*  cm_FindParentConnectionByConType(UINT8 conType);
extern cm_ConnectionInfo_t *cm_FindNextParentConnectionByConType(
    cm_ConnectionInfo_t *connPtr,
    UINT8 conType);
extern cm_ConnectionInfo_t* cm_FindNextConnectionByConTypeAndBssNum(
                                                             cm_ConnectionInfo_t *connPtr,
                                                             UINT8 conType,
                                                             UINT8 BssNum);
#if 0
extern BOOLEAN cm_IsTxDataEnabled(void);

extern BOOLEAN cm_IsRxIdle(void);

extern BOOLEAN cm_IsPromiscuousEnabled(void);

extern BOOLEAN cm_IsConnectionTypePresent(UINT8 conType);

extern BOOLEAN cm_IsBcnBufInUse(void);

extern BOOLEAN cm_IsPrbRspBufInUse(void);

extern cm_ConnectionInfo_t * cm_IsAnyConnectionActive(void);

#if defined(VISTA_802_11_DRIVER_INTERFACE)
extern BOOLEAN cm_RxEnabled(cm_ConnectionInfo_t *connPtr,
                      IEEEtypes_GenHdr_t *pHdr);
extern BOOLEAN cm_ForwardMgt( cm_ConnectionInfo_t *connPtr, uint8 conType);
#endif

extern BOOLEAN cm_IsAllConnectionIdled(void);

extern BOOLEAN cm_AreAllConnectionIdle(void);

extern BOOLEAN cm_IsAnyConnectionScanning(void);

extern BOOLEAN cm_IsConnectionBssId(cm_ConnectionInfo_t *,
                                    IEEEtypes_MacAddr_t *);

extern BOOLEAN cm_IsConnectionTypeConnected(UINT8 conType);

extern BOOLEAN cm_IsWMMEnabled(cm_ConnectionInfo_t *connPtr);

extern BOOLEAN cm_IsInfraSecurityEnabled(void);

extern BOOLEAN cm_IsWEPEnabledOnAnyConnection(void);
#ifdef DOT11D
extern BOOLEAN cm_Is11DEnabledOnActiveConnection(void);
#endif
#ifdef DOT11H
extern BOOLEAN cm_Is11hEnabledOnActiveConnection(BOOLEAN isMaster);
#endif
   // PS needs to be figured out
extern cm_ConnectionInfo_t * cm_GetActPwrSvConnection(void);

extern IEEEtypes_MacAddr_t *cm_GetStationMacAddr(void);

extern cm_ConnectionInfo_t *cm_GetActivityReportConnection(void);

extern Status_e cm_SetActivityReportConnection(cm_ConnectionInfo_t *connPtr);

extern void cm_SetPerformanceParams(void);

extern cm_ConnectionInfo_t *cm_GetPsMgrConnection( UINT8 conType );

extern Status_e cm_SetPsMgrConnection(cm_ConnectionInfo_t *);

extern cm_ConnectionInfo_t *cm_RemoveConnPSMgr(cm_ConnectionInfo_t * connPtr);

extern UINT8 cm_IsEthIIEnabled(cm_ConnectionInfo_t *);

extern BOOLEAN cm_IsPSTypePresent(UINT8);

IEEEtypes_MacAddr_t * cm_GetConnectionBSSID(cm_ConnectionInfo_t* );

UINT8 cm_GetConnectionType(cm_ConnectionInfo_t *);

extern void cm_SetPeerRateSet(cm_ConnectionInfo_t *connPtr,
                              MRVL_RATEID_BITMAP  bitmapRateSet);
#endif

extern void cm_SetPeerAddr(cm_ConnectionInfo_t *connPtr,
                           IEEEtypes_MacAddr_t *bssId,
                           IEEEtypes_MacAddr_t *peerMacAddr);
extern void cm_SetComData(cm_ConnectionInfo_t *connPtr,
			   char* ssid);
#if 0
extern void cm_SetPeerWMMCapability(cm_ConnectionInfo_t * newConnPtr,
                                    BOOLEAN WMMCapable);

extern cm_ConnectionInfo_t* cm_DupConnection(IEEEtypes_MacAddr_t peerAddr,
                                             cm_ConnectionInfo_t* connPtr);

extern cm_ConnectionInfo_t* cm_DupAdhocConnection(
    IEEEtypes_MacAddr_t peerAddr,
    cm_ConnectionInfo_t* connPtr);

#if (defined TDLS) ||(defined HOST_BASED_TDLS)
extern cm_ConnectionInfo_t* cm_DupTDLSConnection(IEEEtypes_MacAddr_t  peerAddr,
                                                 cm_ConnectionInfo_t* connPtr);
#endif
extern  BOOLEAN cm_ReturnWMMCap(cm_ConnectionInfo_t * connPtr,
                                IEEEtypes_MacAddr_t *peerMacAddr);

extern UINT16 cm_GetSequenceNum(cm_ConnectionInfo_t * connPtr,
                                UINT8 Prio,
                                BOOLEAN doIncrement);
extern void cm_DeleteAdhocConnection(cm_ConnectionInfo_t *connPtr);
extern void cm_DeleteAllAdhocConnection(UINT8 parentReset);
extern void cm_AgeOutAdhoClient(void);
extern void cm_SetLastActivityTSF(cm_ConnectionInfo_t * connPtr);

#ifdef AP_PS
extern UINT32 cm_IsFrameBufferedForAnySTA(UINT8 conType);
#endif
#endif
//#ifdef MICRO_AP_MODE
extern apInfo_t* cm_GetApInfo(cm_ConnectionInfo_t *connPtr);
extern apSpecificData_t* cm_GetApData(cm_ConnectionInfo_t *connPtr);
//#endif
#if 0
extern infraSpecificData_t* cm_GetInfraData(cm_ConnectionInfo_t *connPtr);
extern BOOLEAN cm_IsWfdConnType(cm_ConnectionInfo_t *connPtr);
extern BOOLEAN cm_IsAPConnType(cm_ConnectionInfo_t *connPtr);

#ifdef WIFI_DIRECT
extern struct WifiDirect_Globals *cm_GetWifiDirectGlobalsPtr(
    cm_ConnectionInfo_t *connPtr);
extern Status_e cm_WfdDeAllocMultiAddrEntry(UINT8 *addr);
extern Status_e cm_WfdAllocMultiAddrEntry(UINT8 *addr);
extern void cm_InitWiFiDirectCustomIEIndex(cm_ConnectionInfo_t *connPtr);
#endif

extern cm_ConnectionInfo_t *cm_FindParentConnectionByBssId(
    IEEEtypes_MacAddr_t *pBssId);
extern Status_e cm_DeAllocBssResources(cm_ConnectionInfo_t *connPtr);
extern Status_e cm_AllocBssResources(cm_ConnectionInfo_t *connPtr);
extern Status_e cm_UpdateMultiAddrEntry(cm_ConnectionInfo_t *pConn);
extern BOOLEAN cm_IsAnyStaConnectedToBss(UINT8 conType);
extern UINT32 cm_GetNoOfBssStartedByHost(void);
extern void cm_IncrNoOfBssStartedByHost(void);
extern void cm_DecrNoOfBssStartedByHost(void);
extern cm_ConnectionInfo_t *cm_FindConnectionByMbssIndex(UINT8 apIndex);
extern Status_e cm_ConfigureAndEnableMbss(void);
extern Status_e cm_DisableMbssMode(void);
extern void cm_PauseBss(cm_ConnectionInfo_t *connPtr);
extern void cm_ResumeBss(cm_ConnectionInfo_t *connPtr);
extern UINT32 cm_GetNoOfAPBssPaused(void);
extern Status_e cm_ResetConnections(void);
extern void cm_ResetSecurityInfo(cm_ConnectionInfo_t* connPtr);
extern void cm_ResetAllAdhocSecurityInfo(void);
extern cm_ConnectionInfo_t *cm_FindConnectionByPeerMacAddrAndBssId(
    IEEEtypes_MacAddr_t *peerMacAddr,
    IEEEtypes_MacAddr_t *bssId);
Status_e cm_AllocBcnPrbRspBuf(cm_ConnectionInfo_t *connPtr);
Status_e cm_DeAllocBcnPrbRspBuf(cm_ConnectionInfo_t *connPtr);
extern cm_ConnectionInfo_t* cm_IsAnyInterfaceActive(UINT8 conType, 
                                                    CON_OP_MODE_e connOpMode);

/*************************************************************
** Functions for hardware arbitration
**************************************************************/
extern BOOLEAN cm_UpdateTsfTimerVal(UINT8 conType,
                                    MCU_CORE_TIMER_PARAMS * pTmrParams);

extern BOOLEAN cm_UpdateHWTsfTimerRunCtrl(HwUpdateReason_e reasonCode,
                                          BOOLEAN timerEn);

extern BOOLEAN cm_UpdateHWTxMode(HwUpdateReason_e reasonCode, UINT32 txMode);

extern BOOLEAN cm_UpdateHWMacBssidReg(HwUpdateReason_e reasonCode,
                                      IEEEtypes_MacAddr_t* pBssId);
extern BOOLEAN cm_EnableUnicastFilter(UINT8 conType);
extern UINT32 cm_GetPeerCnt(cm_ConnectionInfo_t *connPtr);
extern UINT32 cm_GetTotalPeerCnt(UINT8 deviceMode);

extern BOOLEAN cm_IsReplayAttackEnabled(cm_ConnectionInfo_t *connPtr);
extern PWR_in_dBm cm_GetPeersTxPowerMax(void);

extern int cm_GetConnTypeCount(void);

extern BOOLEAN cm_IsLastConnection(cm_ConnectionInfo_t * connPtr);

extern void cm_DisconnectAdhoc(cm_ConnectionInfo_t* connPtr, 
                               UINT8 parentReset);

Status_e cm_SetNetMonitorConnection(cm_ConnectionInfo_t * connPtr);
cm_ConnectionInfo_t *cm_GetNetMonitorConnection( void );

BOOLEAN cm_GetRSNStats(cm_ConnectionInfo_t *connPtr,
                       host_802_11RSNStats *pRSNStats);

#ifdef ADHOC_COALESCING
extern BOOLEAN cm_UpdateCoalescingEnableInfo(UINT8 BssType, UINT8 BssNum,
                                       UINT16 EnableCoalescing);
#endif

void cm_processMacControl(cm_ConnectionInfo_t *connPtr);

BOOLEAN cm_IsAnyAPWfdAdhocConnection(void);
#ifdef BFMER_SUPPORT
extern cm_ConnectionInfo_t* cm_GetNextConnection(cm_ConnectionInfo_t *connPtr);
#endif
#if defined (TDLS) || defined (ENABLE_FAST_CH_TRPC_TBL)
extern cm_ConnectionInfo_t* getHeadConnPtr();
#endif
#if defined (ENABLE_FAST_CH_TRPC_TBL)
cm_ConnectionInfo_t* FindConnPtrForChannel(const UINT8 Chan);
#endif /*(ENABLE_FAST_CH_TRPC_TBL)*/

extern BOOLEAN cm_IsPacketPendingTx(cm_ConnectionInfo_t *connPtr);
BOOLEAN cm_IsConnectionActive(cm_ConnectionInfo_t *connPtr);

extern int init_cmIntfBuff(void);
extern cm_interface_t *cmIntfListGetBuf(cm_ConnectionInfo_t *connPtr);
extern void cmIntfListFreeBuf(cm_interface_t *buf);
extern cm_interface_t *cmIntfGetUsedListHead(void);

#ifdef HOST_TXRX_MGMT_FRAME
extern void cm_mgmtFrameSubtypeFwdCheck(dot11MgtFrame_t *pFrame);
extern void cm_getRxUnBlockSetting(cm_ConnectionInfo_t *connPtr, 
                                   UINT8 userEnable,
                                   UINT32 value, 
                                   UINT32 *newCfg, 
                                   UINT32 *oldCfg);
#endif
priorAssocData_t* cm_GetPriorAssocData (cm_ConnectionInfo_t *connPtr);

extern BOOLEAN cm_IsMultipleStaActive(void);

#if defined(DOT11AC)
void cm_UpdateRxPAidFilter(cm_ConnectionInfo_t *connPtr,
                         HwUpdateReason_e reasonCode);
#endif // defined(DOT11AC)

extern adhocSpecificData_t * cm_GetAdhocData(cm_ConnectionInfo_t *connPtr);

#if defined(ADHOC_OVER_IP)
extern void cm_UpdateMacRxForRemotePeers(
    cm_ConnectionInfo_t *connPtr,
    BOOLEAN bRxEnable);

extern Status_e cm_AllocAoIPMultiAddrEntry(
    cm_ConnectionInfo_t *connPtr,
    UINT8 *pMacAddr,
    UINT8 *pAddrIndex);

extern Status_e cm_DeAllocAoIPMultiAddrEntry(
    cm_ConnectionInfo_t *connPtr,
    UINT8 *pMacAddr,
    UINT8 *pAddrIndex);

#endif

extern void cm_controlledPortOpen(cm_ConnectionInfo_t *connPtr, 
                                  BOOLEAN isEmbedded);

extern int init_connMgrBuff(void);
extern cm_ConnectionInfo_t *connMgrListGetBuf(void);
extern void connMgrListFreeBuf(cm_ConnectionInfo_t *buf);
extern void cm_idleDeleteFunction(void);
extern void cm_setConList(cm_ConnectionInfo_t *connPtr,
                          cm_ConnectionInfo_t *next,
                          cm_ConnectionInfo_t *prev);

#ifdef HOST_BASED_TDLS
BOOLEAN cm_IsTdlsOnAnyConnection(void);
#endif
#endif
#endif /* W81CONNMGR_H__ */
