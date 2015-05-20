/******************** (c) Marvell Semiconductor, Inc., 2001 *******************
 *
 * Purpose:
 *    This file contains the function prototypes and definitions for the
 *    MAC Management Service Module.
 *
 * Public Procedures:
 *    macMgmtMain_Init       Initialzies the MAC Management Service Task and
 *                           related components
 *    macMgmtMain_Start      Starts running the MAC Management Service Task
 *    macMgmtMain_SendEvent  Mechanism used to trigger an event indicating a
 *                           message has been received on one of the message
 *                           queues
 *
 * Notes:
 *    None.
 *
 *****************************************************************************/

#ifndef MACMGMTMAIN_H__
#define MACMGMTMAIN_H__

//=============================================================================
//                               INCLUDE FILES
//=============================================================================
#include "wltypes.h"
#include "IEEE_types.h"
//#include "os_if.h"
#include "wmTimer.h"
//#include "host.h"
//#include "wl_msgdefs.h"
//#include "dot11MgtFrame.h"
//#include "mrvlWlanPoolConfig.h"
//#include "osa.h"
//#include "wl_rx_frame.h"
#include "bufferMgmtLib.h"
//#include "mlme.h"
//#ifdef MICRO_AP_MODE
    //#include "wl_tx_frame.h"
    //#include "hal_txinfo.h"
    //#include "wl_Station.h"
    //#include "mlme.h"
    //#include "util.h"
    //#include "acl_types.h"
//#endif
#ifdef WEP_RSN_STATS_MIB
    #include "wl_mib.h"
#endif
    #include "pmkCache.h"
//    #include "passThroughBuf.h"
    #include "keyMgmtSta_rom.h"


#include "keyMgmtApTypes.h"
#include "keyMgmtStaHostTypes.h"

//=============================================================================
//                          PUBLIC TYPE DEFINITIONS
//=============================================================================

#define DEFAULT_LINK_LOSS_LIMIT 60

/*------------------------------------------------------------*/
/* Possible notifications handled by the MAC Management Task. */
/*------------------------------------------------------------*/
#define macMgmtMain_802_11_MGMT_MSG_RCVD   (1U<<0)
#define macMgmtMain_BCN_RCVD               (1U<<1)
#define macMgmtMain_SME_MSG_RCVD           (1U<<2)
#define macMgmtMain_LINK_MONITOR           (1U<<3)
#define macMgmtMain_TX_WATCHDOG            (1U<<4)
#define macMgmtMain_TIMER_CALLBACK         (1U<<5)
#define macMgmtMain_DOZER_EVENT            (1U<<6)
#define macMgmtMain_DOZER_UTIL_TIMER_EVENT (1U<<7)
#define macMgmtMain_TSM_EVENT              (1U<<8)
#define macMgmtMain_DPD_TRAINING_REQ       (1U<<9)
#define macMgmtMain_BCN_TXED               (1U<<10)
#define macMgmtMain_DPD_RESTART_REQ        (1U<<11)
#define macMgmtMain_RADAR_DETECTED         (1U<<12)
#define macMgmtMain_MGMT_DECRYPT_SUCCESS   (1U<<13)
#define macMgmtMain_MGMT_DECRYPT_FAIL      (1U<<14)
#define macMgmtMain_CHMGR_TIMER_CALLBACK   (1U<<15)
#define macMgmtMain_LEAKY_AP_DET            (1U<<16)
#define macMgmtMain_WFD_PS_STATE_CHANGE            (1U<<17)

extern UINT32 gMacMgmtEventPending;
#define MACMGMT_EVT_PEND_CH_SW      (1U << 0)
#define MACMGMT_EVT_PEND_WFD_PS_STATE_CHANGE      (1U << 1)

#define MACMGMT_SET_EVT_PENDING(ev)    gMacMgmtEventPending |= ev;

#define MACMGMT_CLR_EVT_PENDING(ev)    \
gMacMgmtEventPending &= ~ev;              \
if (!gMacMgmtEventPending){                   \
    TxMacDataService_SendEvent(TX_EV_MAC_DATA_PDU_RCVD  \
                       | TX_EV_MGMT_PDU_RCVD    \
                       | TX_EV_DATA_PDU_RCVD    \
                       | TX_EV_CTRL_MSG_RCVD);  \
}

#if 0

typedef struct
{
    OSAFlagRef * macMgmtMainQEvt;
    /* Q for sending msg from SME to MAC task */
    OSMsgQRef * macMgmtMainSmeQ;
    /* Q for sending msg from SME to CBProc task */
    OSMsgQRef * macCBProcSmeQ;
    OSMsgQRef * macMgmtMainTimerCBQ;
} MacMgmtMainCtx_t;

//=============================================================================
//                    PUBLIC PROCEDURES (ANSI Prototypes)
//=============================================================================

/******************************************************************************
 *
 * Name: macMgmtMain_Init
 *
 * Description:
 *   This routine is called to initialize the the MAC Management Service Task
 *   and related components.
 *
 * Conditions For Use:
 *   The timer component must be initialized first.
 *
 * Arguments:
 *   None.
 *
 * Return Value:
 *   Status indicating success or failure
 *
 * Notes:
 *   None.
 *
 *****************************************************************************/
extern WL_STATUS macMgmtMain_Init( void );


/******************************************************************************
 *
 * Name: macMgmtMain_Start
 *
 * Description:
 *   This routine is called to start the MAC Management Service Task running.
 *
 * Conditions For Use:
 *   The task and its associated queues have been initialized by calling
 *   macMgmtMain_Init().
 *
 * Arguments:
 *   None.
 *
 * Return Value:
 *   None.
 *
 * Notes:
 *   None.
 *
 *****************************************************************************/
extern void macMgmtMain_Start( void );


/******************************************************************************
 *
 * Name: macMgmtMain_SendEvent
 *
 * Description:
 *   This routine provides the mechanism to trigger an event indicating to
 *   the MAC Management Service Task that a message has been written to one
 *   of its queues.
 *
 * Conditions For Use:
 *   The task and its associated queues has been initialized by calling
 *   macMgmtMain_Init().
 *
 * Arguments:
 *   Arg1 (i  ): Event - value indicating which queue has received a message
 *
 * Return Value:
 *   Status indicating success or failure
 *
 * Notes:
 *   None.
 *
 *****************************************************************************/
extern WL_STATUS macMgmtMain_SendEvent( UINT16 Event );

typedef PACK_START struct
{
    IEEEtypes_TPCAdaptCmd_t  Cmd;
    UINT16                   TimeOut;
    UINT8                    RateIdx;
}
PACK_END macMgmtQ_TPCAdaptCmd_t;

typedef PACK_START union _macmgmtQ_SmeCmdBody_t
{
    IEEEtypes_JoinCmd_t      JoinCmd;
    IEEEtypes_StartCmd_t     StartCmd;
    macMgmtQ_TPCAdaptCmd_t   TPCAdaptCmd;
    SmeMeasurementRequest_t  MeasCmd;
#ifdef MICRO_AP_MODE
    IEEEtypes_DeauthInd_t    Deauth;
#endif
#if defined(VISTA_802_11_DRIVER_INTERFACE)
    host_802_11AssocDecision_t  AssocDecision;    
#endif
} PACK_END macmgmtQ_SmeCmdBody_t;

// This structure carries additional information, namely, TimeOut and Rate
// Index, in addition to the Peer Station Mac Address required by 802.11 Spec.
/*--------------*/
/* SME Commands */
/*--------------*/
typedef PACK_START struct macmgmtQ_SmeCmd_t
{
    IEEEtypes_SmeCmd_e    CmdType;
    host_MsgHdr_t         HostHdr;
    macmgmtQ_SmeCmdBody_t Body;

} PACK_END macmgmtQ_SmeCmd_t;

typedef PACK_START struct
{
    IEEEtypes_TPCAdaptCfrm_t Rsp;
    UINT8                    TxPwr;
    UINT8                    LinkMargin;
    UINT8                    RSSI;
}
PACK_END smeQ_TPCAdaptCfrm_t;

// This structure contains additional information, namely, Tx Power,
// Link Margin and RSSI, that are reported back to Driver in addition
// to the parameters required by IEEE Standard.

/*-----------------------------------------------------------------*/
/* Management messages coming from the MAC Management Service Task */
/*-----------------------------------------------------------------*/
typedef struct _smeQ_MgmtMsg_t
{
    /* This structure is allocated on the stack when passing messages.
    **   If you increase the size significantly, you will probably have to
    **   move to buffer pools for allocation.  Size at time of this note
    **   was 20 bytes for the entire struct.
    */
    UINT32                       MsgType;
    host_MsgHdr_t                HostHdr;
    union
    {
        IEEEtypes_JoinCfrm_t      JoinCfrm;
        IEEEtypes_StartCfrm_t     StartCfrm;
        smeQ_TPCAdaptCfrm_t       TPCAdaptCfrm;

        IEEEtypes_DeauthInd_t     DeauthInd;
        IEEEtypes_DisassocInd_t   DisassocInd;

    } Msg;
}
PACK_END smeQ_MgmtMsg_t;

typedef PACK_START struct
{
    IEEEtypes_TPCAdaptCmd_t  Cmd;
    UINT16                   TimeOut;
    UINT8                    RateIdx;
}
PACK_END smeQ_TPCAdaptCmd_t;

typedef PACK_START struct
{
    IEEEtypes_BssDesc_t BssDesc;
    UINT16 Reserved1;
    UINT16 Reserved2;

    /*
    ** Fixed binary fields for the host/driver command are above this point
    **  the IEBuffer[1] is left as a place holder for TLV arguments.
    */
    UINT8 IEBuffer[1];

} PACK_END smeQ_AdhocJoinCmd_t;


/*--------------------------------*/
/* Commands from the CB Processor */
/*--------------------------------*/
typedef PACK_START struct
{
    host_MsgHdr_t                HostHdr;
    PACK_START union
    {
        smeQ_AdhocJoinCmd_t       AdhocJoin;
        smeQ_AdhocStartCmd_t      AdhocStart;
        smeQ_TPCAdaptCmd_t        TPCAdapt;
        host_MeasurementReport_t  MeasReq;
#ifdef MICRO_AP_MODE
        host_SysConfCmd_t         SysConfigCmd;
        host_DeauthCmd_t          DeauthCmd;
#endif // MICRO_AP_MODE
    } Msg;
}
PACK_END smeQ_CbProcCmd_t;


///////////////////////////////////////////////////////////////////////////////

extern WL_STATUS tx80211MacQ_MgmtWriteNoBlock(BufferDesc_t *pBufDesc);
extern WL_STATUS macMgmtQ_LnkMonWriteNoBlock(UINT32 data);
extern WL_STATUS macMgmtQ_BcnTxWriteNoBlock(UINT32 data);
extern WL_STATUS macMgmtQ_SmeWriteNoBlock(BufferDesc_t *pSmeCmdBuf);
extern void SendCallBackEvent(Timer* t);
extern void MacMgmtMainInit(MacMgmtMainCtx_t *pCtx);
extern void MacMgmtTask(void* argv);
extern BOOLEAN wlan_HandleRxPkt(BufferDesc_t *bufDesc, WLAN_RX_INFO *rxInfo);
extern Status_e MgmtMlmeRestoreDefault(void);

extern void macMgmtMainMgmtDecryptFail(BufferDesc_t* pRxBufDesc);
extern void macMgmtMainMgmtDecryptSuccess(BufferDesc_t* pRxBufDesc);

extern UINT16 macMgmtMainGetStack(UINT8** ppStack);

#define MAX_SUPPORT_DATA_RATES  IEEEtypes_MAX_DATA_RATES_G

//
// Structure storing station data
//
typedef struct
{
    IEEEtypes_CapInfo_t       CapInfo;
    IEEEtypes_ExtCapability_t ExtCapInfo;

    IEEEtypes_DataRate_t      BssRates[MAX_SUPPORT_DATA_RATES];
    UINT8                     BssRatesLen;

    UINT16                    AP_RateLen;
    UINT16                    AP_gRateLen;
    UINT8                     DefUsedRateIdx;
    UINT8                     LinkLossLimit;

    UINT8                     PreBeaconLost;
#ifdef ROAM_AGENT
    UINT8                     PreBeaconLostForRA;
#endif
#ifdef DOT11N
    IEEEtypes_HT_Capability_t HtCapData;
#endif // DOT11N
#ifdef DOT11AC
    IEEEtypes_VHT_Capability_t VhtCapData;
#endif
} macMgmtMlme_StaData_t;
#endif

//Fields until ShortRetryLim are used in ROM
typedef struct
{
    /* The This section only used for initialization of the connPtr */
    IEEEtypes_SsId_t SsId;
    IEEEtypes_Len_t  SsIdLen;
    // odd-sized ele clubbed together to keep even alignment
    IEEEtypes_DtimPeriod_t DtimPeriod;
    IEEEtypes_BcnInterval_t BcnPeriod;
    
    IEEEtypes_MacAddr_t BssId;
    UINT16  RtsThresh;
    UINT16  FragThresh;
    UINT8   ShortRetryLim;
    UINT8   LongRetryLim;

    // Used in MBSS mode for software beacon suppression
    UINT8 MbssBcnIntFac;
    UINT8 MbssCurBcnIntCnt;
    UINT16 Reserved;
} CommonMlmeData_t;

typedef struct
{
    IEEEtypes_HT_Information_t HtInfo;    
    IEEEtypes_20N40_BSS_Coexist_t Ht2040Coexist;
    IEEEtypes_ExtCapability_t       HtExtCapData;
    IEEEtypes_OBSS_ScanParam_t HtScanParam;
    IEEEtypes_HT_Capability_t  HtCapData;
#ifdef DOT11AC
    IEEEtypes_VHT_Capability_t  VhtCapData;
    IEEEtypes_VHT_Operation_t VhtOper;    
#endif
} adhocInfo_t;

#if 0
/* Extern Data */
extern IEEEtypes_MacMgmtStates_e smeMain_State;
extern IEEEtypes_PwrMgmtMode_e   macMgmtMain_PwrMode;
extern MacMgmtMainCtx_t          locMacMgmtMainCtx;

//#ifdef MICRO_AP_MODE
/* IEEEtypes_MAX_CHANNELS is 14 channels. 
** MAX_TOTAL_SCAN_TIME/IEEEtypes_MAX_CHANNELS is always greater than 110TUs. 
**
** From now on, ACS will always take MAX_TOTAL_SCAN_TIME TUs for 
** completion, regardless of the number of scan channels so that
** when less channels are configured for ACS, FW will dwell for more time on 
** each of them.
**
** This code will need to be modified when cross band ACS support is added 
** (thereby increasing the number of channels).
*/
// default channel scan time for ACS 110ms since most APs use beacon 100 TU
#define ACS_SCAN_TIME       (110) /* 110 ms */

// In case number of channels is large, we do not want to exceed
// total scan time of 2000 ms. So this will be used to establish
// upper limit and scan time will be adjusted to the minimum
// of ACS_SCAN_TIME or (MAX_TOTAL_SCAN_TIME/num_of_scan_channels)
#define MAX_TOTAL_SCAN_TIME (2000) /* 2000 ms */

//TODO: move IEEE PS related definitions to correct file 
#define TIM_SIZE 251

typedef struct _txQingInfo_t txQingInfo_t;

typedef struct _fragListElem_t
{
    struct fragListElem_t  *nxt;
    struct fragListElem_t  *prv;    
    BufferDesc_t *bufDesc_p;
    struct _fragListElem_t * nextStn;
    struct _fragListElem_t * prevStn;
    txQingInfo_t *pPwrSaveQ;
}
fragListElem_t;
#endif

typedef struct _txQingInfo_t
{
#if 0
    UINT32 msgCnt; /* Number of messages in the Station's List */
    fragListElem_t *stnFragListHead;
    fragListElem_t *stnFragListTail;
#endif
    IEEEtypes_PwrMgmtMode_e mode;
#if 0
    UINT8 pwrSaveSupported;
    UINT8 pad[2];
#endif
}
txQingInfo_t;

#if 0
/*
  UAP Packet Forwarding Modes 
*/
#define MASTER_PACKET_FWD_CTL_MASK          BIT0
// FW forwards everything to host to decide
#define AP_FW_FWD_ALL_PKT_TO_HOST           0
// FW forwards BSS destined packets (ucast & mcast) automatically
#define AP_FW_HANDLES_INTRABSS_PKT          1

#define BCAST_INTRA_BSS_PKT_FWD_MASK        BIT1
#define BCAST_INTRA_BSS_FWD_ENABLED         0

#define UCAST_INTRA_BSS_PKT_FWD_MASK        BIT2
#define UCAST_INTRA_BSS_FWD_ENABLED         0

#define UCAST_INTER_BSS_PKT_FWD_MASK        BIT3
#define UCAST_INTER_BSS_FWD_ENABLED         0


#ifdef WAR_ROM_BUG47752_PKT_FWD_CTL_ENH    
/* Size of PktFwdConfMode and PktFwdMode has changed from 2 bits
** to 4 bits. To leave space for their future extensions, these fields are  
** defined 8 bits wide. BssOpSettings_t is used in BssConfig_t, and
** that is in turn used in ROM code. As suggested by Manish, all accesses
** to BssOpSettings_t elements are moved from ROM to RAM. This structure
** is kept only to ensure that ROM code can correctly access
** the subsequent fields from BssConfig_t.
*/
typedef PACK_START struct
{
    UINT16 useLongPreamble:1; //preamble type configured
    UINT16 EnSsidBcast:1;
    UINT16 RadioCtl:1;
    UINT16 PktFwdConfMode:2;  /* pkt fwd mode configured by user. */
    UINT16 PktFwdMode:2;      /* The actual pkt fwd mode currently
                              ** in use by the FW is present in
                              ** PktFwdMode (below).
                              */
    UINT16 TxAnt:3;
    UINT16 RxAnt:3;
}
PACK_END BssOpSettings_defunct_t;
#endif

typedef PACK_START struct
{
    UINT16 useLongPreamble:1; //preamble type configured
    UINT16 EnSsidBcast:1;
    UINT16 RadioCtl:1;
    UINT16 TxAnt:3;
    UINT16 RxAnt:3;
    UINT16 Padding:7;
    
    UINT8 PktFwdConfMode;   /* pkt fwd mode configured by user. */
    UINT8 PktFwdMode;       /* The actual pkt fwd mode currently
                            ** in use by the FW is present in
                            ** PktFwdMode (below).
                            */
}
PACK_END BssOpSettings_t;

typedef enum
{
    AP_BSS_STOP_RESULT_SUCCESS,
    AP_BSS_STOP_RESULT_FAILURE,
    AP_BSS_STOP_RESULT_NO_BSS_ACTIVE,
    AP_BSS_STOP_RESULT_PENDING,
} apBssStopResult_e;

typedef enum
{
    AP_BSS_RESET_SUCCESS,
    AP_BSS_RESET_FAILURE,
    AP_BSS_RESET_PENDING,
} apBssResetResult_e;

typedef enum _apStaAssocState_e
{
    AP_STASTATE_DEAUTHENTICATED = 0,
    AP_STASTATE_DEAUTHENTICATING,    
    AP_STASTATE_AUTHENTICATED,
    AP_STASTATE_ASSOCIATED,
} apStaAssocState_e;

/* This enum is used in ROM and the existing values
 * should not be changed. However,
 * new values can be added at the end.
 */
typedef enum
{
    ApState_Inactive = 0,
    ApState_Active,
    ApState_Scanning,  
    ApState_Stopping,
}ApState_e;

typedef struct
{
    UINT32    pwrSaveStaCnt;
    txQingInfo_t pwrSaveMcQinfo;
}apPwrSaveData_t;
#endif

typedef struct
{
    UINT16    keyExchange:1;
    UINT16    authenticate:1;
    UINT16    reserved:14;
}Operation_t;

typedef struct
{
    Cipher_t mcstCipher;
    UINT8 mcstCipherCount;

    Cipher_t wpaUcstCipher;
    UINT8 wpaUcstCipherCount;

    Cipher_t wpa2UcstCipher;
    UINT8 wpa2UcstCipherCount;

    UINT16 AuthKey;
    UINT16 AuthKeyCount;
    Operation_t Akmp;
    UINT32 GrpReKeyTime;
    UINT8  PSKPassPhrase[PSK_PASS_PHRASE_LEN_MAX];
    UINT8  PSKPassPhraseLen;
    UINT8  PSKValue[PMK_LEN_MAX];
    UINT8  MaxPwsHskRetries;
    UINT8  MaxGrpHskRetries;
    UINT32 PwsHskTimeOut;
    UINT32 GrpHskTimeOut;
#ifdef RSN_REPLAY_ATTACK_PROTECTION
    UINT8  RSNReplayProtEn;/* RSN Replay Attack Protection flag*/
#endif
} apRsnConfig_t;

typedef struct
{
    UINT16 enable;        // 0: disable
                          // 1: enable w/ config update
                          // 2: enable w/o config update
    UINT16 duration;      // number of beacons
    UINT16 stickyTimBitmask;
} StickyTimConfig_t;

typedef struct
{
    UINT16 enable;
    IEEEtypes_MacAddr_t staMacAddr;
} StickyTimStaConfig_t;

typedef struct
{
    UINT8      ieSet      ;
    UINT8      version    ;
/*  UINT8      akmCnt     ;  */
    UINT8      akmTypes   ;
/*  UINT8      uCastCnt   ;  */
    UINT8      uCastTypes ;
    UINT8      mCastTypes ;
    UINT8      capInfo    ;
} wapi_ie_cfg_t;
/************************************************************************
IMPORTANT:

While adding entries to packed structure below make sure that strucutures
that get used for memcpy stay aligned on even boundary.
*************************************************************************/
/* TODO:is PACK_START, PACK_END necessary??
 * aligment has changed because of 
 * added fields. so come back to check aligment
 */
/* This structure and the sub structures within, are used
 * in ROM and none of the existing fields 
 *  should be changed. However new fields can be added at
 *  the end of this structure or in-place of "Padding" (but not the
 *  sub structures).
 */
typedef struct
{
    UINT32  StaAgeOutTime;
    UINT32  PsStaAgeOutTime;
#if 0
#ifdef WAR_ROM_BUG47752_PKT_FWD_CTL_ENH
/* This structure is kept only to ensure that ROM code can correctly access
** the subsequent fields from BssConfig_t. The new definition of BssOpSet is 
** present at the end of ROMed structure BssConfig_t below.
*/
    BssOpSettings_defunct_t BssOpSet_defunct;
#endif
    /* If the BssAddr field is not aligned on word boundary the hal 
      functions which update mac registers are unsafe for non-word 
      aligned pointers. Avoid direct use of the pointer to BssId 
      field in the hal functions */
    /*  this field is no longer used and we use mibOpdata_p->StaMacAddr
        in its place now */
    IEEEtypes_MacAddr_t EepromMacAddr_defunct;
    IEEEtypes_DataRate_t OpRateSet[IEEEtypes_MAX_DATA_RATES_G];

    // odd-sized ele clubbed together to keep even alignment
    UINT8   AuthType;
    UINT8   TxPowerLevel;
    IEEEtypes_DataRate_t TxDataRate;
    IEEEtypes_DataRate_t TxMCBCDataRate;
    UINT8 MaxStaSupported;
    
    SecurityMode_t SecType;
    UINT8 Padding1[1]; //****** Use this for adding new members *******
    BOOLEAN apWmmEn;
    IEEEtypes_WMM_ParamElement_t apWmmParamSet;
    /* HtCap configured by host - currently used for Fixed TX Rate Setting */
    Ht_Tx_Cap_from_host_t  HtTxCap;
    BOOLEAN ap11nEn;
    IEEEtypes_HT_Capability_t HtCapCfg;
    IEEEtypes_HT_Information_t HtInfo;    
    cipher_key_buf_t *pWepKeyBuf;
    cipher_key_buf_t *pGtkKeyBuf;
    UINT8 ScanChanCount;
    //ACL
    Filter_Tbl_Mode AclFilterMode;
    UINT8 AclStaCnt;
    IEEEtypes_MacAddr_t AclMacTbl[MAX_FILTER_ENTRIES];
    UINT8 Padding3[1]; //****** Use this for adding new members *******
#endif
    apRsnConfig_t RsnConfig;
    CommonMlmeData_t comData;
#if 0
    BOOLEAN apWmmPsEn;
    channelInfo_t ScanChanList[IEEEtypes_MAX_CHANNELS];/* Channels to scan */
    CommonMlmeData_t comData;
    IEEEtypes_OBSS_ScanParam_t ObssScanParam;
    Ht_Tx_Info_from_host_t HtTxInfo;
    cipher_key_buf_t *piGtkKeyBuf;
    UINT32 mgmtFrameSubtypeFwdEn;
    UINT8 Ht2040CoexEn;        // Enable/Disable 2040 Coex feature in uAP

    BssOpSettings_t BssOpSet;
    UINT8 Padding4[1]; //****** Use this for adding new members *******

    StickyTimConfig_t stickyTimConfig;
    wapi_ie_cfg_t  wapiCfg;
    IEEEtypes_ExtCapability_t ExtCap;
    UINT8 Padding6[1]; //****** Use this for adding new members *******

#ifdef DOT11AC
    UINT8 ap11acEn;
    UINT8 ap11acForceHTBw;
    IEEEtypes_VHT_Capability_t VhtCap;
    IEEEtypes_VHT_Operation_t VhtOp;
    IEEEtypes_VHT_OpModeNotification_t VhtOpModeNotification;
#endif
#endif
} BssConfig_t;

/* BssData_t.ht2040CoexInitialTimerVal is used to control the initial value of 
** HT2040 coex timer based on whether Legacy OBSS scan was performed or not. If 
** legacy OBSS scan was performed, and legacy OBSS was found, it's set to
** HT2040COEX_TIMER_25MIN. If no legacy OBSS were seen, it's set to 
** HT2040COEX_TIMER_DISABLE. If OBSS scan was not done for some reason
** (mlan0/another uBSS is already active), it's set to 
** HT2040COEX_TIMER_5SEC.
*/
#define HT2040COEX_TIMER_DISABLE    0
#define HT2040COEX_TIMER_5SEC       1
#define HT2040COEX_TIMER_25MIN      2

/* BssData_t.htInfoSecChanInitialVal is used to control initial value of sec.
** chan offset advertized in HtInfo IE in AP beacons. It can have values of 
** HT_INFO_SEC_CHAN_DISABLE or HT_INFO_SEC_CHAN_ENABLE.
*/
#define HT_INFO_SEC_CHAN_DISABLE    0
#define HT_INFO_SEC_CHAN_ENABLE     1
//TODO:check if PACK_START and PACK_END is required for this structure
typedef struct
{
#if 0
    //SyncSrvSta mgtSync;
    IEEEtypes_CapInfo_t CapInfo;
    UINT32 AssocStationsCnt;
    ApMode_e ApMode;
    //ERP
    ap_NetworkCtrl_t NetworkCtrl;
    UINT8 ErpProtTimeout;
    UINT8 HtProtTimeout;
    volatile UINT8 probeRespStatus;

    IEEEtypes_HT_Capability_t HtCap;

    // HT 2040 Coex timeout is 1500 Sec by default. Won't work with 8 bit
    // counter. With 32bit counter and default BI=100mS, maximum timeout 
    // of 4.29496729e+5 Sec can be achieved.
    // Trim this counter to 16 bit later, if requird.
    UINT32 Ht2040CoexTimeout;
    
    //shared key auth
#ifndef USE_PER_STA_CHALLENGE
    os_timer_t authTimer;
    UINT8 chanText[IEEEtypes_CHALLENGE_TEXT_SIZE] ;
    UINT8 sharedkey_authinprogress;
#endif

    //AID
    UINT32 AidBitmap;

    //beacon
#ifndef BCN_UPDT_NO_DELAY
    BOOLEAN erp_bcn_update;
    BOOLEAN capinfo_bcn_update;
    BOOLEAN htinfo_bcn_update;
    BOOLEAN tim_bcn_update;
#ifdef DOT11AC
    BOOLEAN vhtOp_bcn_update;
    BOOLEAN vhtOpModeNotification_bcn_update;
#endif
#ifdef AP_WMM
    BOOLEAN wmm_bcn_update;
#endif 
#endif // BCN_UPDT_NO_DELAY

    //IEEE PS
    IEEEtypes_TimElement_t Tim;
    IEEEtypes_TimElement_t *TimPtr;

    ApState_e            state;
    apPwrSaveData_t     pwrSaveData;
    //WEP, RSN stats
#ifdef WEP_RSN_STATS_MIB
    MIB_RSNSTATS RSNStats;
#endif
    MIC_Error_t apMicError;
#endif
    BOOLEAN updatePassPhrase;
    os_timer_t apMicTimer;

    KeyData_t grpKeyData;
    UINT8 GNonce[32];

#if 0
    /* Following two variables contain that multiple of BI which is just 
    ** greater than user configured ageout time in normal and PS mode. These 
    ** variables get updated at bss_start, and then are used whenever FW
    ** resets STA age.
    */
    UINT32 staAgeOutBcnCnt;
    UINT32 psStaAgeOutBcnCnt;
#endif
#ifdef AUTHENTICATOR
    // Store group rekey time as a multiple of beacon interval.
    UINT32 grpRekeyBcnCntConfigured;
    UINT32 grpRekeyBcnCntRemaining;
#endif //AUTHENTICATOR
#if 0
    /* This variable is used to control the initial value of HT2040 coex timer
    ** based on whether Legacy OBSS scan was performed or not. If legacy OBSS 
    ** scan was performed, and legacy OBSS was found, it's set to
    ** HT2040COEX_TIMER_25MIN. If no legacy OBSS were seen, it's set to 
    ** HT2040COEX_TIMER_DISABLE. If OBSS scan was not done for some reason
    ** (mlan0/another uBSS is already active), it's set to 
    ** HT2040COEX_TIMER_5SEC.
    */
    UINT8 ht2040CoexInitialTimerVal:2;

    /* This variable is used to control initial value of secondary channel
    ** offset advertized in HtInfo IE in AP beacons. It can have values of 
    ** HT_INFO_SEC_CHAN_DISABLE or HT_INFO_SEC_CHAN_ENABLE.
    */    
    UINT8 htInfoSecChanInitialVal:1;

    UINT8 reserved:5;
#endif
} BssData_t;

//typedef rate_Change_t staRateTable[IEEEtypes_MAX_DATA_RATES_G];

#ifdef AP_WMM_PS
typedef struct
{
    UINT8 isUSPInProgress;
    UINT8 wmmPsNoOfFrames;
    UINT8 curSPLen;
    IEEEtypes_QOS_Info_t qosInfo;
    IEEEtypes_QOS_Info_t acTriggerInfo;
    IEEEtypes_QOS_Info_t acDeliveryInfo;
} apStaWmmPsInfo_t;
#endif

//TODO:check if PACK_START and PACK_END is required for this structure
typedef struct
{
#if 0
    UINT32 ageOutCnt;  
    UINT32 stateInfo;
#endif
    txQingInfo_t pwrSaveInfo;

    //key mgmt data
    apKeyMgmtInfoSta_t keyMgmtInfo;
#if 0
#ifdef USE_PER_STA_CHALLENGE
    MicroTimerId_t authTimer;
    UINT8 chanText[IEEEtypes_CHALLENGE_TEXT_SIZE] ;
    UINT8 sharedkey_authinprogress;
#endif
    apStaAssocState_e assocState;
    ClientMode_e clientMode;
    UINT16  deauthReason;
#ifdef WIFI_DIRECT
   UINT8 *sta_wfd_ie;
#endif
#ifdef AP_WMM_PS
    txQingInfo_t wmmPsQueueInfo;
    apStaWmmPsInfo_t wmmPsInfo;
#endif

    UINT8  txPauseState;
    //RateChangeInfo[] is used by MAC HW to decide the start TX rate. 
    //It should be placed in SQ. If staData_t is placed in ITCM/DTCM then put
    //staRateTable in SQ and use a pointer here
    //staRateTable RateChangeInfo;
#ifdef UAP_STICKY_TIM
    UINT16 stickyTimCount;
    BOOLEAN stickyTimEnabled;
#endif

#ifdef STREAM_2x2 
    IEEEtypes_SMPS_ParamSet_t SMPwrSaveParam;
#endif
#ifdef DOT11W
	/* Peer STA PMF capability */
    BOOLEAN peerPMFCapable;
#endif

#if defined(DOT11AC)
    IEEEtypes_VHT_OpMode_t vhtOpMode;
#endif
#endif
} staData_t;

typedef struct
{
    BssConfig_t bssConfig;
    BssData_t   bssData;
    UINT8       ApStop_Req_Pending;
} apInfo_t;
//#endif //MICRO_AP_MODE

#endif /* _MACMGMTMAIN_H_ */
