#ifndef IEEE_ACTION_TYPES_H__
#define IEEE_ACTION_TYPES_H__

#include "IEEE_types.h"
#include "IEEE_WMM_types.h"

/*
*****************************************************************************
**
**
**                     802.11h Measurement definitions
**
**
*****************************************************************************
*/

typedef PACK_START struct
{
    UINT8  channelNumber;
    UINT64 startTime;
    UINT16 duration;

} PACK_END IEEEtypes_Common11hHdr_MeasReq_t;

/*
**
**  Basic Request/Report
**
*/
typedef PACK_START struct
{
    UINT8  channelNumber;
    UINT64 startTime;
    UINT16 duration;

} PACK_END IEEEtypes_Basic_MeasReq_t;

typedef PACK_START struct
{
    UINT8 bss     : 1;
    UINT8 ofdm    : 1;
    UINT8 unident : 1;
    UINT8 radar   : 1;
    UINT8 unmeas  : 1;

    UINT8 reserved : 3;

} PACK_END IEEEtypes_Basic_MeasRpt_Map_t;

typedef PACK_START struct
{
    UINT8  channelNumber;
    UINT64 startTime;
    UINT16 duration;

    IEEEtypes_Basic_MeasRpt_Map_t map;

} PACK_END IEEEtypes_Basic_MeasRpt_t;

/*
**
**  CCA Request/Report
**
*/
typedef PACK_START struct
{
    UINT8  channelNumber;
    UINT64 startTime;
    UINT16 duration;

} PACK_END IEEEtypes_CCA_MeasReq_t;

typedef PACK_START struct
{
    UINT8  channelNumber;
    UINT64 startTime;
    UINT16 duration;
    UINT8  busyFraction;

} PACK_END IEEEtypes_CCA_MeasRpt_t;

/*
**
**  RPI Request/Report
**
*/
typedef PACK_START struct
{
    UINT8  channelNumber;
    UINT64 startTime;
    UINT16 duration;

} PACK_END IEEEtypes_RPI_MeasReq_t;

typedef PACK_START struct
{
    UINT8  channelNumber;
    UINT64 startTime;
    UINT16 duration;
    UINT8  rpiDensity[8];

} PACK_END IEEEtypes_RPI_MeasRpt_t;

/*
*****************************************************************************
**
**
**                     802.11k Measurement definitions
**
**
*****************************************************************************
*/

typedef PACK_START struct
{
    UINT8  regulatoryClass;
    UINT8  channelNumber;
    UINT16 randomInterval;
    UINT16 duration;

} PACK_END IEEEtypes_Common11kHdr_MeasReq_t;

/*
**
**  Channel Load Request/Report
**
*/
typedef PACK_START struct
{
    UINT8  regulatoryClass;
    UINT8  channelNumber;
    UINT16 randomInterval;
    UINT16 duration;

    UINT8  subElem[1];

} PACK_END IEEEtypes_ChanLoad_MeasReq_t;

typedef PACK_START struct
{
    UINT8  regulatoryClass;
    UINT8  channelNumber;
    UINT64 startTime;
    UINT16 duration;
    UINT8  chanLoad;

    UINT8  subElem[1];

} PACK_END IEEEtypes_ChanLoad_MeasRpt_t;

/*
**
**  Noise Histogram Request/Report
**
*/
typedef PACK_START struct
{
    UINT8  regulatoryClass;
    UINT8  channelNumber;
    UINT16 randomInterval;
    UINT16 duration;

    UINT8  subElem[1];

} PACK_END IEEEtypes_NoiseHist_MeasReq_t;

typedef PACK_START struct
{
    UINT8  regulatoryClass;
    UINT8  channelNumber;
    UINT64 startTime;
    UINT16 duration;

    UINT8  antennaId;
    UINT8  anpi;

    UINT8  ipiDensities[11];

    UINT8  subElem[1];

} PACK_END IEEEtypes_NoiseHist_MeasRpt_t;

/*
**
**  Beacon Request/Report
**
*/
typedef PACK_START enum
{
    BEACON_MEAS_MODE_PASSIVE = 0,
    BEACON_MEAS_MODE_ACTIVE  = 1,
    BEACON_MEAS_MODE_TABLE   = 2,

} PACK_END IEEEtypes_Beacon_MeasReqMode_e;

typedef PACK_START struct
{
    UINT8  regulatoryClass;
    UINT8  channelNumber;
    UINT16 randomInterval;
    UINT16 duration;

    IEEEtypes_Beacon_MeasReqMode_e measMode;
    IEEEtypes_MacAddr_t bssid;

    UINT8  subElem[1];

} PACK_END IEEEtypes_Beacon_MeasReq_t;

typedef PACK_START enum
{
    IEEE_BEACON_FRAME_TYPE_BCN_PRB = 0,
    IEEE_BEACON_FRAME_TYPE_PILOT   = 1,

} PACK_END IEEEtypes_Beacon_FrameType_e;

typedef PACK_START struct
{
    IEEEtypes_PhyType_e          phyType : 7;
    IEEEtypes_Beacon_FrameType_e frameType : 1;

} PACK_END IEEEtypes_Beacon_MeasRptFrameInfo_t;

typedef PACK_START struct
{
    UINT8  regulatoryClass;
    UINT8  channelNumber;
    UINT64 startTime;
    UINT16 duration;

    IEEEtypes_Beacon_MeasRptFrameInfo_t frameInfo;
    UINT8  rcpi;
    UINT8  rsni;

    IEEEtypes_MacAddr_t bssid;
    UINT8  antennaId;

    UINT8  parentTsf[4];

    UINT8  subElem[1];

} PACK_END IEEEtypes_Beacon_MeasRpt_t;

/*
**
**  Frame Request/Report
**
*/
typedef PACK_START enum
{
    FRAME_MEAS_REQ_TYPE_FRAME_COUNT = 1,

} PACK_END IEEEtypes_Frame_MeasReqType_e;

typedef PACK_START struct
{
    UINT8  regulatoryClass;
    UINT8  channelNumber;
    UINT16 randomInterval;
    UINT16 duration;

    IEEEtypes_Frame_MeasReqType_e reqType;
    IEEEtypes_MacAddr_t bssid;

    UINT8  subElem[1];

} PACK_END IEEEtypes_Frame_MeasReq_t;

typedef PACK_START struct
{
    IEEEtypes_MacAddr_t txAddr;
    IEEEtypes_MacAddr_t bssid;

    IEEEtypes_PhyType_e phyType;

    UINT8  avgRcpi;
    UINT8  lastRsni;
    UINT8  lastRcpi;
    UINT8  antennaId;
    UINT16 frameCount;

} PACK_END IEEEtypes_Frame_MeasRptEntryField_t;

typedef PACK_START struct
{
    IEEEtypes_InfoElementHdr_t          ieHdr;
    IEEEtypes_Frame_MeasRptEntryField_t rptEntry[1];

} PACK_END IEEEtypes_Frame_MeasRptEntry_t;

typedef PACK_START struct
{
    UINT8  regulatoryClass;
    UINT8  channelNumber;
    UINT64 startTime;
    UINT16 duration;

    UINT8  subElem[1];

} PACK_END IEEEtypes_Frame_MeasRpt_t;

/*
**
**  STA Stats Request/Report
**
*/
typedef PACK_START enum
{
    STA_STATS_ID_COUNTERS     = 0,
    STA_STATS_ID_MAC_STATS    = 1,
    STA_STATS_ID_QOS_UP0      = 2,
    STA_STATS_ID_QOS_UP1      = 3,
    STA_STATS_ID_QOS_UP2      = 4,
    STA_STATS_ID_QOS_UP3      = 5,
    STA_STATS_ID_QOS_UP4      = 6,
    STA_STATS_ID_QOS_UP5      = 7,
    STA_STATS_ID_QOS_UP6      = 8,
    STA_STATS_ID_QOS_UP7      = 9,
    STA_STATS_ID_ACCESS_DELAY = 10

} PACK_END IEEEtypes_StaStats_MeasIdentity_e;

typedef PACK_START struct
{
    IEEEtypes_MacAddr_t bssid;
    UINT16 randomInterval;
    UINT16 duration;
    IEEEtypes_StaStats_MeasIdentity_e  groupIdentity;

    UINT8  subElem[1];

} PACK_END IEEEtypes_StaStats_MeasReq_t;

typedef PACK_START struct
{
    UINT16 duration;
    IEEEtypes_StaStats_MeasIdentity_e  groupIdentity;

    UINT8  groupStatistics[1]; /* Variable depending on group Id */

    UINT8  subElem[1];

} PACK_END IEEEtypes_StaStats_MeasRpt_t;

/*
**
**  LCI Request/Report
**
*/
typedef PACK_START struct
{
    UINT8 locationSubject;
    UINT8 latResolution;
    UINT8 longResolution;
    UINT8 altitudeResolution;

    UINT8 subElem[1];

} PACK_END IEEEtypes_LCI_MeasReq_t;

typedef PACK_START struct
{
    UINT32 latitudeResolution  : 6;
    UINT32 latitudeFraction    : 25;
    UINT32 latitudeInteger     : 9;

    UINT32 longitudeResolution : 6;
    UINT32 longitudeFraction   : 25;
    UINT32 longitudeInteger    : 9;

    UINT32 altitudeResolution  : 6;
    UINT32 altitudeFraction    : 8;
    UINT32 altitudeInteger     : 22;

    UINT32 datum               : 8;

} PACK_END IEEEtypes_LCI_MeasRptInfo;

typedef PACK_START struct
{
    IEEEtypes_InfoElementHdr_t ieHdr;
    IEEEtypes_LCI_MeasRptInfo  lciInfo;
    UINT8                      subElem[1];

} PACK_END IEEEtypes_LCI_MeasRpt_t;

/*
**
**  TX Stream Request/Report
**
*/
typedef PACK_START struct
{
    UINT16 randomInterval;
    UINT16 duration;
    IEEEtypes_MacAddr_t bssid;

    UINT8  trafficId;
    UINT8  binZeroRange;

    UINT8  subElem[1];

} PACK_END IEEEtypes_TxStream_MeasReq_t;

typedef PACK_START struct
{
    UINT8 avgTrigger         : 1;
    UINT8 consecutiveTrigger : 1;
    UINT8 delayTrigger       : 1;
    UINT8 reserved           : 4;

} PACK_END IEEEtypes_TxStream_MeasRptReason_t;

typedef PACK_START struct
{
    UINT64 startTime;
    UINT16 duration;
    IEEEtypes_MacAddr_t peerAddr;
    UINT8  trafficId;
    IEEEtypes_TxStream_MeasRptReason_t reportReason;
    UINT32 msduTxCount;
    UINT32 msduDiscardCount;
    UINT32 msduFailCount;
    UINT32 msduMultiRetryCount;
    UINT32 qosCfPollLostCount;
    UINT32 avgQueueDelay;
    UINT32 avgTxDelay;
    UINT8  binRange;

    UINT32 binValues[6];

    UINT8  subElem[1];

} PACK_END IEEEtypes_TxStream_MeasRpt_t;

/*
**
**  Pause Request
**
*/
typedef PACK_START struct
{
    UINT16 pauseTime;

    UINT8  subElem[1];

} PACK_END IEEEtypes_Pause_MeasReq_t;

/*
*****************************************************************************
**
**
**                   802.11h and 802.11k Measurements
**
**
*****************************************************************************
*/
typedef PACK_START enum
{
    /* 802.11h Spectrum Management defined measurement types */
    IEEE_MEAS_TYPE_BASIC        = 0,
    IEEE_MEAS_TYPE_CCA          = 1,
    IEEE_MEAS_TYPE_RPI          = 2,

    /* 802.11k Radio Resource Measurement types */
    IEEE_MEAS_TYPE_CHANNEL_LOAD = 3,
    IEEE_MEAS_TYPE_NOISE_HIST   = 4,
    IEEE_MEAS_TYPE_BEACON       = 5,
    IEEE_MEAS_TYPE_FRAME        = 6,
    IEEE_MEAS_TYPE_STA_STATS    = 7,
    IEEE_MEAS_TYPE_LCI          = 8,
    IEEE_MEAS_TYPE_TX_STREAM    = 9,

    /* 802.11v Wireless Network Measurement types */
    IEEE_MEAS_TYPE_MCAST_DIAG_REQ = 10,
    IEEE_MEAS_TYPE_LOC_CIVIC_REQ  = 11,
    IEEE_MEAS_TYPE_LOC_ID_REQ     = 12,

    IEEE_MEAS_TYPE_PAUSE        = 255,

} PACK_END IEEEtypes_MeasType_e;

typedef PACK_START struct
{
    UINT8 parallel          : 1;
    UINT8 enable            : 1;
    UINT8 request           : 1;
    UINT8 report            : 1;
    UINT8 durationMandatory : 1;

    UINT8 reserved1         : 3;

} PACK_END IEEEtypes_MeasReqMode_t;

typedef PACK_START struct
{
    UINT8                   measToken;
    IEEEtypes_MeasReqMode_t measReqMode;
    IEEEtypes_MeasType_e    measType;

} PACK_END IEEEtypes_MeasReqElemHdr_t;

typedef PACK_START struct
{
    IEEEtypes_InfoElementHdr_t ieHdr;
    IEEEtypes_MeasReqElemHdr_t reqHdr;

    PACK_START union
    {
        /* 11h Specturm Management Measurements */
        IEEEtypes_Basic_MeasReq_t     basic;
        IEEEtypes_CCA_MeasReq_t       cca;
        IEEEtypes_RPI_MeasReq_t       rpi;

        /* 11k Radio Resource Measurements */
        IEEEtypes_ChanLoad_MeasReq_t  chanLoad;
        IEEEtypes_NoiseHist_MeasReq_t noiseHist;
        IEEEtypes_Beacon_MeasReq_t    beacon;
        IEEEtypes_Frame_MeasReq_t     frame;
        IEEEtypes_StaStats_MeasReq_t  staStats;
        IEEEtypes_LCI_MeasReq_t       lci;
        IEEEtypes_TxStream_MeasReq_t  txStream;
        IEEEtypes_Pause_MeasReq_t     pause;

    } PACK_END req;

} PACK_END IEEEtypes_MeasReqElement_t;

typedef PACK_START struct
{
    UINT8 late      : 1;
    UINT8 incapable : 1;
    UINT8 refused   : 1;

    UINT8 reserved1 : 4;

} PACK_END IEEEtypes_MeasRptMode_t;

typedef PACK_START struct
{
    UINT8                   measToken;
    IEEEtypes_MeasRptMode_t measRptMode;
    IEEEtypes_MeasType_e    measType;

} PACK_END IEEEtypes_MeasRptElemHdr_t;

typedef PACK_START struct
{
    IEEEtypes_InfoElementHdr_t ieHdr;
    IEEEtypes_MeasRptElemHdr_t rptHdr;

    PACK_START union
    {
        /* 11h Specturm Management Measurements */
        IEEEtypes_Basic_MeasRpt_t     basic;
        IEEEtypes_CCA_MeasRpt_t       cca;
        IEEEtypes_RPI_MeasRpt_t       rpi;

        /* 11k Radio Resource Measurements */
        IEEEtypes_ChanLoad_MeasRpt_t  chanLoad;
        IEEEtypes_NoiseHist_MeasRpt_t noiseHist;
        IEEEtypes_Beacon_MeasRpt_t    beacon;
        IEEEtypes_Frame_MeasRpt_t     frame;
        IEEEtypes_StaStats_MeasRpt_t  staStats;
        IEEEtypes_LCI_MeasRpt_t       lci;
        IEEEtypes_TxStream_MeasRpt_t  txStream;

    } PACK_END rpt;

} PACK_END IEEEtypes_MeasRptElement_t;

/*
*****************************************************************************
**
**
**         802.11h Spectrum Management Action Frame types/structs
**
**
*****************************************************************************
*/
typedef PACK_START enum
{
    SPEC_MGMT_ACTION_MEAS_REQ    = 0,
    SPEC_MGMT_ACTION_MEAS_RPT    = 1,

    SPEC_MGMT_ACTION_TPC_REQ     = 2,
    SPEC_MGMT_ACTION_TPC_RPT     = 3,

    SPEC_MGMT_ACTION_CHAN_SW_ANN = 4,

} PACK_END IEEEtypes_SpectrumMgmt_Action_e;

typedef PACK_START struct
{
    IEEEtypes_ActionCategory_e      category;
    IEEEtypes_SpectrumMgmt_Action_e action;

} PACK_END IEEEtypes_Action_Base_SpecMgmt_t;

typedef PACK_START struct
{
    IEEEtypes_Action_Base_SpecMgmt_t smAct;
    UINT8                            dialogToken;
    IEEEtypes_MeasRptElement_t       measRptElem[1];

} PACK_END IEEEtypes_Action_SpecMeasRpt_t;

typedef PACK_START struct
{
    IEEEtypes_Action_Base_SpecMgmt_t smAct;
    UINT8                            dialogToken;
    IEEEtypes_MeasReqElement_t       measReqElem[1];

} PACK_END IEEEtypes_Action_SpecMeasReq_t;

typedef PACK_START struct
{
    IEEEtypes_Action_Base_SpecMgmt_t smAct;
    UINT8                            dialogToken;
    IEEEtypes_TPCRequestElement_t    tpcReqElem;

} PACK_END IEEEtypes_Action_TpcReq_t;

typedef PACK_START struct
{
    IEEEtypes_Action_Base_SpecMgmt_t smAct;
    UINT8                            dialogToken;
    IEEEtypes_TPCReportElement_t     tpcRptElem;

} PACK_END IEEEtypes_Action_TpcRpt_t;

typedef PACK_START struct
{
    IEEEtypes_Action_Base_SpecMgmt_t smAct;
    IEEEtypes_ChannelSwitchElement_t chanSwElem;

} PACK_END IEEEtypes_Action_ChanSwitchAnn_t;

typedef PACK_START union
{
    IEEEtypes_Action_Base_SpecMgmt_t smAct;

    IEEEtypes_Action_SpecMeasReq_t   measReq;
    IEEEtypes_Action_SpecMeasRpt_t   measRpt;

    IEEEtypes_Action_TpcReq_t        tpcReq;
    IEEEtypes_Action_TpcRpt_t        tpcRpt;

    IEEEtypes_Action_ChanSwitchAnn_t chanSwAnn;

} PACK_END IEEEtypes_Action_SpectrumMgmt_t;

/*
*****************************************************************************
**
**
**         802.11k Radio Measurement Action Frame types/structs
**
**
*****************************************************************************
*/
typedef PACK_START enum
{
    RSRC_ACTION_RADIO_MEAS_REQ = 0,
    RSRC_ACTION_RADIO_MEAS_RPT = 1,

    RSRC_ACTION_LINK_MEAS_REQ  = 2,
    RSRC_ACTION_LINK_MEAS_RPT  = 3,

    RSRC_ACTION_NBOR_RPT_REQ   = 4,
    RSRC_ACTION_NBOR_RPT_RSP   = 5,

} PACK_END IEEEtypes_RadioRsrc_Action_e;

typedef PACK_START struct
{
    IEEEtypes_ActionCategory_e   category;
    IEEEtypes_RadioRsrc_Action_e action;
    UINT8                        dialogToken;

} PACK_END IEEEtypes_Action_Base_RadioRsrc_t;

typedef PACK_START struct
{
    IEEEtypes_Action_Base_RadioRsrc_t rrAct;

    UINT16                            numberOfReps;
    IEEEtypes_MeasReqElement_t        measReqElem[1];

} PACK_END IEEEtypes_Action_RadioMeasReq_t;

typedef PACK_START struct
{
    IEEEtypes_Action_Base_RadioRsrc_t rrAct;

    IEEEtypes_MeasRptElement_t        measRptElem[1];

} PACK_END IEEEtypes_Action_RadioMeasRpt_t;

typedef PACK_START struct
{
    IEEEtypes_Action_Base_RadioRsrc_t rrAct;

    UINT8 txPowerUsed;
    UINT8 maxTxPower;
    UINT8 subElem[1];

} PACK_END IEEEtypes_Action_LinkMeasReq_t;

typedef PACK_START struct
{
    IEEEtypes_Action_Base_RadioRsrc_t rrAct;

    IEEEtypes_TPCReportElement_t tpcRptElem;
    UINT8                        rxAntId;
    UINT8                        txAntId;
    UINT8                        rcpi;
    UINT8                        rsni;
    UINT8                        subElem[1];

} PACK_END IEEEtypes_Action_LinkMeasRpt_t;

typedef PACK_START struct
{
    IEEEtypes_Action_Base_RadioRsrc_t rrAct;

    UINT8                             subElem[1]; /* SSID, Vendor Specific */

} PACK_END IEEEtypes_Action_NeighborReq_t;

typedef PACK_START struct
{
    UINT32 apReachability       : 2;
    UINT32 security             : 1;
    UINT32 keyScope             : 1;

    UINT32 capSpectrumMgmt      : 1;
    UINT32 capQos               : 1;
    UINT32 capAPSD              : 1;
    UINT32 capRadioMeasurement  : 1;
    UINT32 capDelayedBlockAck   : 1;
    UINT32 capImmediateBlockAck : 1;

    UINT32 mobilityDomain       : 1;

    UINT32 reserved             : 21;

} PACK_END IEEEtypes_BSSID_Info_t;

typedef PACK_START struct
{
    IEEEtypes_InfoElementHdr_t ieHdr;

    IEEEtypes_MacAddr_t        bssid;

    IEEEtypes_BSSID_Info_t     bssInfo;
    UINT8                      regulatoryClass;
    UINT8                      channelNumber;
    IEEEtypes_PhyType_e        phyType;

    UINT8                      subElem[1];

} PACK_END IEEEtypes_NeighborRptElem_t;

typedef PACK_START struct
{
    IEEEtypes_Action_Base_RadioRsrc_t rrAct;

    IEEEtypes_NeighborRptElem_t       rptElem[1];

} PACK_END IEEEtypes_Action_NeighborRsp_t;

typedef PACK_START union
{
    IEEEtypes_Action_Base_RadioRsrc_t rrAct;

    IEEEtypes_Action_RadioMeasReq_t   rmReq;
    IEEEtypes_Action_RadioMeasRpt_t   rmRpt;

    IEEEtypes_Action_LinkMeasReq_t    lmReq;
    IEEEtypes_Action_LinkMeasRpt_t    lmRpt;

    IEEEtypes_Action_NeighborReq_t    nborReq;
    IEEEtypes_Action_NeighborRsp_t    nborRsp;

} PACK_END IEEEtypes_Action_RadioRsrc_t;

/*
*****************************************************************************
**
**
**           802.11r Fast Transition Action Frame types/structs
**
**
*****************************************************************************
*/
typedef PACK_START enum
{
    FT_ACTION_RESERVED = 0,

    FT_ACTION_REQUEST  = 1,
    FT_ACTION_RESPONSE = 2,
    FT_ACTION_CONFIRM  = 3,
    FT_ACTION_ACK      = 4,

} PACK_END IEEEtypes_FastBssTrans_Action_e;


typedef PACK_START struct
{
    IEEEtypes_ActionCategory_e             category;

    IEEEtypes_FastBssTrans_Action_e 	   action;

    IEEEtypes_MacAddr_t                    staAddr;
    IEEEtypes_MacAddr_t                    targetAp;

} PACK_END IEEEtypes_Action_Base_FastBssTrans_t;

typedef PACK_START struct
{
    IEEEtypes_Action_Base_FastBssTrans_t ftAct;

    UINT8 subElem[1];

} PACK_END IEEEtypes_Action_FastBssTransReq_t;

typedef PACK_START struct
{
    IEEEtypes_Action_Base_FastBssTrans_t ftAct;

    IEEEtypes_StatusCode_t statusCode;
    UINT8                  subElem[1];

} PACK_END IEEEtypes_Action_FastBssTransRsp_t;

typedef PACK_START struct
{
    IEEEtypes_Action_Base_FastBssTrans_t ftAct;

    UINT8 subElem[1];

} PACK_END IEEEtypes_Action_FastBssTransCfrm_t;

typedef PACK_START struct
{
    IEEEtypes_Action_Base_FastBssTrans_t ftAct;

    IEEEtypes_StatusCode_t statusCode;
    UINT8                  subElem[1];

} PACK_END IEEEtypes_Action_FastBssTransAck_t;

typedef PACK_START union
{
    IEEEtypes_Action_Base_FastBssTrans_t ftAct;

    IEEEtypes_Action_FastBssTransReq_t   ftReq;
    IEEEtypes_Action_FastBssTransRsp_t   ftRsp;
    IEEEtypes_Action_FastBssTransCfrm_t  ftCfrm;
    IEEEtypes_Action_FastBssTransAck_t   ftAck;

} PACK_END IEEEtypes_Action_FastBssTrans_t;

/*
*****************************************************************************
**
**
**              802.11e Block Ack Action Frame types/structs
**
**
*****************************************************************************
*/
typedef PACK_START enum
{
    BA_ACTION_CODE_ADDBA_REQ = 0,
    BA_ACTION_CODE_ADDBA_RSP = 1,
    BA_ACTION_CODE_DELBA     = 2,

} PACK_END IEEEtypes_BlockAck_ActionCode_e;

typedef PACK_START struct
{
    IEEEtypes_ActionCategory_e      category;
    IEEEtypes_BlockAck_ActionCode_e action;

} PACK_END IEEEtypes_Action_Base_BlockAck_t;

typedef PACK_START struct
{
    UINT16  reserved       : 1;
    UINT16  blockAckPolicy : 1;
    UINT16  tid            : 4;
    UINT16  bufferSize     : 10;

} PACK_END IEEEtypes_ADDBA_ParamSet_t;

typedef PACK_START struct
{
    UINT16  reserved  : 11;
    UINT16  initiator : 1;
    UINT16  tid       : 4;

} PACK_END IEEEtypes_DELBA_ParamSet_t;

typedef PACK_START struct
{
    IEEEtypes_Action_Base_BlockAck_t baAct;

    UINT8                        dialogToken;
    IEEEtypes_ADDBA_ParamSet_t   addBaParamSet;
    UINT16                       timeout;
    IEEEtypes_SeqCtl_t           startSeqCtl;

} PACK_END IEEEtypes_Action_AddBaReq_t;

typedef PACK_START struct
{
    IEEEtypes_Action_Base_BlockAck_t baAct;

    UINT8                      dialogToken;
    UINT16                     statusCode;
    IEEEtypes_ADDBA_ParamSet_t addBaParamSet;
    UINT16                     timeout;

} PACK_END IEEEtypes_Action_AddBaRsp_t;

typedef PACK_START struct
{
    IEEEtypes_Action_Base_BlockAck_t baAct;

    IEEEtypes_DELBA_ParamSet_t delBaParamSet;
    UINT16                     reasonCode;

} PACK_END IEEEtypes_Action_DelBa_t;

typedef PACK_START union
{
    IEEEtypes_Action_Base_BlockAck_t baAct;

    IEEEtypes_Action_AddBaReq_t      addBaReq;
    IEEEtypes_Action_AddBaRsp_t      addBaRsp;
    IEEEtypes_Action_DelBa_t         delBa;

} PACK_END IEEEtypes_Action_BlockAck_t;


/****************************************************************************
**
**              802.11ac VHT Action Frame types/structs
**
****************************************************************************/
typedef enum
{
    VHT_ACTION_CODE_COMPRESSED_BF = 0,
    VHT_ACTION_CODE_GID_MGMT,
    VHT_ACTION_CODE_OP_MODE_NOTIFICATION,
} IEEEtypes_VHT_ActionCode_e;

typedef PACK_START struct
{
    IEEEtypes_ActionCategory_e category;
    IEEEtypes_VHT_ActionCode_e action;
    
} PACK_END IEEEtypes_VHT_ActionBase_t;

typedef PACK_START struct
{
    IEEEtypes_VHT_ActionBase_t      vhtAct;
    IEEEtypes_VHT_OpMode_t          vhtOpMode;
    
} PACK_END IEEEtypes_VHT_Action_OpMode_t;

typedef PACK_START union
{
    IEEEtypes_VHT_ActionBase_t      vhtAct;
    IEEEtypes_VHT_Action_OpMode_t   opMode;
    
} PACK_END IEEEtypes_Action_VHT_t;


/*
*****************************************************************************
**
**
**               802.11n HT Action Frame types/structs
**
**
*****************************************************************************
*/
typedef PACK_START enum
{
    HT_ACTION_CODE_NOTIFY_CHAN_WIDTH = 0,
    HT_ACTION_CODE_SM_PS,
    HT_ACTION_CODE_PSMP,
    HT_ACTION_CODE_SET_PCO_PAHSE,
    HT_ACTION_CODE_CSI,
    HT_ACTION_CODE_NONE_COMPRESSED_BF,
    HT_ACTION_CODE_COMPRESSED_BF,
    HT_ACTION_CODE_ASEL_FEEDBACK

} PACK_END IEEEtypes_HT_ActionCode_e;

typedef PACK_START struct
{
    IEEEtypes_ActionCategory_e      category;
    IEEEtypes_HT_ActionCode_e       action;

} PACK_END IEEEtypes_HT_Action_Base_t;

typedef PACK_START struct
{
    UINT8   Enable         : 1;
    UINT8   Mode           : 1;
    UINT8   Reserved       : 6;

} PACK_END IEEEtypes_SMPS_ParamSet_t;

typedef PACK_START struct
{
    IEEEtypes_HT_Action_Base_t          htAct;
    IEEEtypes_SMPS_ParamSet_t           SMPSParamSet;

} PACK_END IEEEtypes_HT_Action_SMPS_t;

typedef PACK_START union
{
    IEEEtypes_HT_Action_Base_t          htAction;
    IEEEtypes_HT_Action_SMPS_t          SMPowerSave;

} PACK_END IEEEtypes_Action_HT_t;

/*
*****************************************************************************
**
**
**      802.11v Wireless Network Managment Action Frame types/structs
**
**
*****************************************************************************
*/
typedef PACK_START enum
{
    WNM_ACTION_EVENT_REQ           = 0,
    WNM_ACTION_EVENT_RPT           = 1,
    WNM_ACTION_DIAG_REQ            = 2,
    WNM_ACTION_DIAG_RPT            = 3,
    WNM_ACTION_LOC_CFG_REQ         = 4,
    WNM_ACTION_LOC_CFG_RSP         = 5,
    WNM_ACTION_BSS_TRANS_QUERY     = 6,
    WNM_ACTION_BSS_TRANS_REQ       = 7,
    WNM_ACTION_BSS_TRANS_RSP       = 8,
    WNM_ACTION_FMS_REQ             = 9,
    WNM_ACTION_FMS_RSP             = 10,
    WNM_ACTION_COLLOC_INTF_REQ     = 11,
    WNM_ACTION_COLLOC_INTF_RSP     = 12,
    WNM_ACTION_TFS_REQ             = 13,
    WNM_ACTION_TFS_RSP             = 14,
    WNM_ACTION_TFS_NOTIFY          = 15,
    WNM_ACTION_SLEEP_MODE_REQ      = 16,
    WNM_ACTION_SLEEP_MODE_RSP      = 17,
    WNM_ACTION_TIM_BCAST_REQ       = 18,
    WNM_ACTION_TIM_BCAST_RSP       = 19,
    WNM_ACTION_QOS_TRAFFIC_CAP_UPD = 20,
    WNM_ACTION_CHAN_USAGE_REQ      = 21,
    WNM_ACTION_CHAN_USAGE_RSP      = 22,
    WNM_ACTION_CHAN_DMS_REQ        = 23,
    WNM_ACTION_CHAN_DMS_RSP        = 24,
    WNM_ACTION_TIMING_MEAS_REQ     = 25,
    WNM_ACTION_NOTIFICATION_REQ    = 26,
    WNM_ACTION_NOTIFICATION_RSP    = 27

} PACK_END IEEEtypes_WNM_Action_e;

typedef PACK_START enum
{
    WNM_ACTION_STATUS_ACCEPT                 = 0,
    WNM_ACTION_STATUS_REJECT_UNSPEC          = 1,
    WNM_ACTION_STATUS_REJECT_INSF_FRAMES     = 2,
    WNM_ACTION_STATUS_REJECT_INSF_CAPACITY   = 3,
    WNM_ACTION_STATUS_REJECT_TERM_NOTHANKS   = 4,
    WNM_ACTION_STATUS_REJECT_TERM_DELAY_REQ  = 5,
    WNM_ACTION_STATUS_REJECT_CAND_LIST_GIVEN = 6,
    WNM_ACTION_STATUS_REJECT_NO_GOOD_CAND    = 7,
    WNM_ACTION_STATUS_REJECT_LEAVING_ESS     = 8,

} PACK_END IEEEtypes_WNM_Action_StatusCode_e;

typedef PACK_START enum
{
    WMM_ACTION_REASON_UNSPEC                   = 0,
    WMM_ACTION_REASON_EXC_LOSS_POOR_COND       = 1,
    WMM_ACTION_REASON_EXC_DELAY                = 2,
    WMM_ACTION_REASON_INSF_QOS_CAP             = 3,
    WMM_ACTION_REASON_FIRST_ESS_ASSOC          = 4,
    WMM_ACTION_REASON_LOAD_BALANCE             = 5,
    WMM_ACTION_REASON_BETTER_AP_FOUND          = 6,
    WMM_ACTION_REASON_PRIOR_AP_DEAUTH          = 7,
    WMM_ACTION_REASON_AP_FAILED_1X_AUTH        = 8,
    WMM_ACTION_REASON_AP_FAILED_4WAY_HSK       = 9,
    WMM_ACTION_REASON_REPLAY_COUNTER_FAIL      = 10,
    WMM_ACTION_REASON_DATA_MIC_FAIL            = 11,
    WMM_ACTION_REASON_EXCD_MAX_RETRANS         = 12,
    WMM_ACTION_REASON_TOO_MANY_BRDCST_DISASSOC = 13,
    WMM_ACTION_REASON_TOO_MANY_BRDCST_DEAUTH   = 14,
    WMM_ACTION_REASON_PRIOR_TRANSITION_FAIL    = 15,
    WMM_ACTION_REASON_LOW_RSSI                 = 16,
    WMM_ACTION_REASON_ROAM_FROM_NON_802_11     = 17,
    WMM_ACTION_REASON_TRANS_FROM_BSS_TRANS_REQ = 18,
    WMM_ACTION_REASON_BSS_TRANS_CAND_LIST_INC  = 19,
    WMM_ACTION_REASON_LEAVING_ESS              = 20,

} PACK_END IEEEtypes_WNM_Action_TransReasonCode_e;

typedef PACK_START struct
{
    IEEEtypes_ActionCategory_e  category;
    IEEEtypes_WNM_Action_e      action;
    UINT8                       dialogToken;

} PACK_END IEEEtypes_Action_Base_WNM_t;

typedef PACK_START struct
{
    IEEEtypes_Action_Base_WNM_t      wnmAct;

    IEEEtypes_WNM_Action_TransReasonCode_e queryReason;

    /* TODO: Optional BSS Transition Candidiate List */

} PACK_END IEEEtypes_Action_WNM_BssTransQuery_t;

typedef PACK_START struct
{
    IEEEtypes_Action_Base_WNM_t      wnmAct;

    UINT8    requestMode;
    UINT16   disassocTimer;
    UINT8    validityInterval;

#if 0
    UINT8[12] bssTermDuration; /* Optional */

    /* TODO: Optional SessionInfoURL, BSS Transition Candidiate List */
#endif

} PACK_END IEEEtypes_Action_WNM_BssTransRequest_t;

typedef PACK_START struct
{
    IEEEtypes_Action_Base_WNM_t      wnmAct;

    IEEEtypes_WNM_Action_StatusCode_e statusCode;
    UINT16 bssTerminationDelay;

#if 0
    UINT8[6] targetBssid;  /* Optional */

    /* TODO: Optional SessionInfoURL, BSS Transition Candidiate List */
#endif

} PACK_END IEEEtypes_Action_WNM_BssTransResponse_t;

typedef PACK_START union
{
    IEEEtypes_Action_Base_WNM_t wnmAct;

    IEEEtypes_Action_WNM_BssTransQuery_t    bssTransQuery;
    IEEEtypes_Action_WNM_BssTransRequest_t  bssTransReq;
    IEEEtypes_Action_WNM_BssTransResponse_t bssTransRsp;

} PACK_END IEEEtypes_Action_WNM_t;

/*
*****************************************************************************
**
**
**            802.11w SA Query Action Frame types/structs
**
**
*****************************************************************************
*/
typedef PACK_START enum
{
    SAQ_ACTION_SA_QUERY_REQUEST  = 0,
    SAQ_ACTION_SA_QUERY_RESPONSE = 1,

} PACK_END IEEEtypes_SAQ_Action_e;

typedef PACK_START struct
{
    IEEEtypes_ActionCategory_e                 category;
    IEEEtypes_SAQ_Action_e 		       action;

} PACK_END IEEEtypes_Action_Base_SAQ_t;


typedef PACK_START struct
{
    IEEEtypes_Action_Base_SAQ_t saqAct;

    UINT16 transactionId;

} PACK_END IEEEtypes_Action_SAQ_ReqRsp_t;

/******************************************************************************
*                                                                             *
*                                                                             *
*                802.11z TDLS Action Frame types/structs                      *
*                                                                             *
*                                                                             *
*******************************************************************************/
typedef PACK_START struct
{
    IEEEtypes_ActionCategory_e category;
    UINT8                      actionDetail;
} PACK_END IEEEtypes_Action_Base_TDLS_t;

typedef PACK_START struct
{
    IEEEtypes_ElementId_e ElementId;
    IEEEtypes_Len_t       Len;
    IEEEtypes_MacAddr_t   bssid;
    IEEEtypes_MacAddr_t   initStaAddr;
    IEEEtypes_MacAddr_t   rspnStaAddr;
} PACK_END IEEEtypes_TDLS_LinkIDElement_t;

typedef PACK_START struct
{
    IEEEtypes_Action_Base_TDLS_t   TDLSAct;
    UINT8                          dialogToken;
    IEEEtypes_TDLS_LinkIDElement_t linkid;
} PACK_END IEEEtypes_Action_TDLS_DscvryReq_t;

typedef PACK_START struct
{
    IEEEtypes_Action_Base_TDLS_t   TDLSAct;
    UINT8                          dialogToken;
    IEEEtypes_CapInfo_t            capInfo;
} PACK_END IEEEtypes_Action_TDLS_SetupReq_t;

typedef PACK_START struct
{
    IEEEtypes_Action_Base_TDLS_t  TDLSAct;
    IEEEtypes_StatusCode_t        statusCode;
    UINT8                         dialogToken;
    IEEEtypes_CapInfo_t           capInfo;
} PACK_END IEEEtypes_Action_TDLS_SetupRsp_t;

typedef PACK_START struct
{
    IEEEtypes_Action_Base_TDLS_t  TDLSAct;
    IEEEtypes_StatusCode_t        statusCode;
    UINT8                         dialogToken;
} PACK_END IEEEtypes_Action_TDLS_SetupCfm_t;

typedef PACK_START struct
{
    IEEEtypes_Action_Base_TDLS_t  TDLSAct;
    IEEEtypes_ReasonCode_t        reasonCode;
} PACK_END IEEEtypes_Action_TDLS_Teardown_t;

typedef PACK_START struct
{
    IEEEtypes_ElementId_e ElementId;
    IEEEtypes_Len_t       Len;
    UINT8   tid;
    IEEEtypes_SeqCtl_t   seqCtrl;
} PACK_END IEEEtypes_PTIControlElement_t;

typedef PACK_START union
{
    PACK_START struct
    {
        UINT8 BkAvailable:1;
        UINT8 BeAvailable:1;
        UINT8 ViAvailable:1;
        UINT8 VoAvailable:1;
        UINT8 reserved:4;
    }status_bits;
    UINT8 status;
} PACK_END IEEEtypes_PUBufferStatus_t;

typedef PACK_START struct
{
    IEEEtypes_ElementId_e       ElementId;
    IEEEtypes_Len_t                 Len;
    IEEEtypes_PUBufferStatus_t puBufferStatus;
    IEEEtypes_SeqCtl_t             seqCtrl;
} PACK_END IEEEtypes_PUBufferStatusElement_t;

typedef PACK_START struct
{
    IEEEtypes_Action_Base_TDLS_t  TDLSAct;
    UINT8                         dialogToken;
    IEEEtypes_TDLS_LinkIDElement_t linkid;
} PACK_END IEEEtypes_Action_PeerTrafficIndication_t;

typedef PACK_START struct
{
    IEEEtypes_Action_Base_TDLS_t  TDLSAct;
    UINT8                         dialogToken;
    IEEEtypes_TDLS_LinkIDElement_t linkid;
} PACK_END IEEEtypes_Action_PeerTrafficRsp_t;

typedef PACK_START struct
{
    IEEEtypes_ElementId_e       ElementId;
    IEEEtypes_Len_t             Len;
    UINT16                      switchTime;
    UINT16                      switchTimeout;
} PACK_END IEEEtypes_TDLS_CSTimingElement_t;

typedef PACK_START struct
{
    IEEEtypes_Action_Base_TDLS_t  TDLSAct;
    UINT8                         targetChannel;
    UINT8                         regulatoryClass;
} PACK_END IEEEtypes_Action_TDLS_CSReq_t;

typedef PACK_START struct
{
    IEEEtypes_Action_Base_TDLS_t                TDLSAct;
    IEEEtypes_StatusCode_t                      statusCode;
    IEEEtypes_TDLS_LinkIDElement_t              linkid;
    IEEEtypes_TDLS_CSTimingElement_t            channelSwitchTiming;
} PACK_END IEEEtypes_Action_TDLS_CSRsp_t;
/*
*****************************************************************************
**
**
**            802.11 Public Action Frame types/structs
**
**
*****************************************************************************
*/
typedef PACK_START enum
{
    PUBLIC_ACTION_CODE_2040_COEX             = 0,
    PUBLIC_ACTION_CODE_DSE_ENABLE            = 1,
    PUBLIC_ACTION_CODE_DSE_DISABLE           = 2,
    PUBLIC_ACTION_CODE_DSE_REGLOC_ANN        = 3,
    PUBLIC_ACTION_CODE_EXT_CHSW_ANN          = 4,
    PUBLIC_ACTION_CODE_DSE_MEAS_REQ          = 5,
    PUBLIC_ACTION_CODE_DSE_MEAS_RPT          = 6,
    PUBLIC_ACTION_CODE_MEAS_PILOT            = 7,
    PUBLIC_ACTION_CODE_DSE_PWR_CONSTRAINT    = 8,
    PUBLIC_ACTION_CODE_VENDOR_SPECIFIC       = 9,
    /* The definitions for these public action frames are based
    ** on 802.11u Draft 8.  They have moved in subsequent 11u drafts.
    ** WFD uses the 802.11u Draft 8 as a basis, so these are needed
    ** this way for WFD until the conflict for WFD and 11u is
    ** resolved
    */
    PUBLIC_ACTION_CODE_GAS_INITIAL_REQUEST   = 10,
    PUBLIC_ACTION_CODE_GAS_INITIAL_RESPONSE  = 11,
    PUBLIC_ACTION_CODE_GAS_COMEBACK_REQUEST  = 12,
    PUBLIC_ACTION_CODE_GAS_COMEBACK_RESPONSE = 13,
    PUBLIC_ACTION_CODE_TDLS_DSCVRY_RSP       = 14,
} PACK_END IEEEtypes_Public_Action_e;

typedef PACK_START enum
{
    PUBLIC_ACTION_OUI_TYPE_P2P  =  9,
    PUBLIC_ACTION_OUI_TYPE_WPSE = 16,
} PACK_END IEEEtypes_Public_Action_OUI_Type_e;

typedef PACK_START struct
{
    IEEEtypes_ActionCategory_e  category;
    IEEEtypes_Public_Action_e   action;
} PACK_END IEEEtypes_Action_Base_Public_t;

typedef PACK_START struct
{
    IEEEtypes_ActionCategory_e  category;
    IEEEtypes_Public_Action_e   action;

    UINT8 ChannelSwitchMode;
    UINT8 RegClass;
    UINT8 ChannelNumber;
    UINT8 ChannelSwitchCount;

} PACK_END IEEEtypes_Action_Public_ExtChannelSwitchAnn_t;

typedef PACK_START struct
{
    IEEEtypes_ActionCategory_e  category;
    IEEEtypes_Public_Action_e   action;

    UINT8 Oui[3];
    UINT8 OuiType;
    UINT8 OuiSubType;
    UINT8 DialogToken;

    UINT8 Data[1];  /* Variable length vendor specific content */

} PACK_END IEEEtypes_Action_Public_VendorSpecific_t;

typedef PACK_START struct
{
    IEEEtypes_ActionCategory_e  category;
    IEEEtypes_Public_Action_e   action;

    UINT8 Oui[3];

    UINT8 OuiType;
    UINT8 OuiSubType;
    UINT8 DialogToken;
    UINT8 Elements[1];

} PACK_END IEEEtypes_Action_Public_P2P_t;
#ifdef TDLS
typedef PACK_START struct
{
    IEEEtypes_Action_Base_Public_t publicAct;
    UINT8                          dialogToken;
    IEEEtypes_CapInfo_t            capInfo;
} PACK_END IEEEtypes_Action_Public_TDLS_DscvryRsp_t;
#endif

typedef PACK_START struct
{
    IEEEtypes_ElementId_e ElementId;
    IEEEtypes_Len_t       Length;
    UINT8                 CoexElement;

} PACK_END IEEEtypes_2040CoexElement_t;

typedef PACK_START struct
{
    IEEEtypes_ElementId_e ElementId;
    IEEEtypes_Len_t       Length;
    UINT8                 RegulatoryDomain;
    UINT8                 ChannelList[1];

} PACK_END IEEEtypes_2040CoexChannelReportElement_t;


typedef PACK_START struct
{
    IEEEtypes_Action_Base_Public_t           publicAct;
    IEEEtypes_2040CoexElement_t              Coex2040Element;
    IEEEtypes_2040CoexChannelReportElement_t Coex2040ChannelReport;

} PACK_END IEEEtypes_Action_2040CoexFrame_t;

/*
**  11u GAS frames
*/
typedef PACK_START struct
{
    UINT8 GasQueryRspFragId:7;
    UINT8 MoreGasFrag:1;
}PACK_END IEEEtypes_GAS_QueryRspFragId_t;

typedef PACK_START struct
{
    UINT8 QueryRspLenLimit:7;
    UINT8 PAME_BI:1;
    UINT8 AdvtProtoID[1]; /* Can be variable length if Vendor specific
                          ** ID is used otherwise it is length 1
                          */
}PACK_END IEEEtypes_AdvtProtoTuple_t;

typedef PACK_START struct
{
    IEEEtypes_ElementId_e      ElementId;
    IEEEtypes_Len_t            Len;
    IEEEtypes_AdvtProtoTuple_t AdvtProtocolTuple[1]; /* There is at least
                                                     ** one tuple, but
                                                     ** optionally could
                                                     ** be more
                                                     */
}PACK_END IEEEtypes_AdvtProtocolIE_t;

typedef PACK_START struct
{
    IEEEtypes_ActionCategory_e  category;
    IEEEtypes_Public_Action_e   action;
    UINT8                       DialogToken;
    IEEEtypes_AdvtProtocolIE_t  AdvtProtocolIE;
    UINT16                      QueryReqLen;
    UINT8                       QueryReq[1]; /* Variable length
                                             ** container field */
}PACK_END IEEEtypes_Action_Public_GAS_InitialRqst_t;

typedef PACK_START struct
{
    IEEEtypes_ActionCategory_e  category;
    IEEEtypes_Public_Action_e   action;
    UINT8                       DialogToken;
    UINT8                       Status;
    UINT16                      GasComebackDelay;
    IEEEtypes_AdvtProtocolIE_t  AdvtProtocolIE;
    UINT16                      QueryRspLen;
    UINT8                       QueryRsp[1]; /* Variable length
                                             ** container field
                                             ** Optional */
}PACK_END IEEEtypes_Action_Public_GAS_InitialResp_t;

typedef PACK_START struct
{
    IEEEtypes_ActionCategory_e     category;
    IEEEtypes_Public_Action_e      action;
    UINT8                          DialogToken;
}PACK_END IEEEtypes_Action_Public_GAS_ComebackRqst_t;

typedef PACK_START struct
{
    IEEEtypes_ActionCategory_e     category;
    IEEEtypes_Public_Action_e      action;
    UINT8                          DialogToken;
    UINT8                          Status;
    IEEEtypes_GAS_QueryRspFragId_t GasQueryRspFragId;
    UINT16                         GasComebackDelay;
    IEEEtypes_AdvtProtocolIE_t     AdvtProtocolIE;
    UINT16                         QueryReqLen;
    UINT8                          QueryReq[1]; /* Variable length
                                                ** container field
                                                ** Optional */
}PACK_END IEEEtypes_Action_Public_GAS_ComebackResp_t;

typedef PACK_START union
{
    IEEEtypes_Action_Base_Public_t                publicAct;

    IEEEtypes_Action_Public_VendorSpecific_t      pubVendSpecific;
    IEEEtypes_Action_Public_P2P_t                 pubP2P;

    IEEEtypes_Action_Public_ExtChannelSwitchAnn_t pubExtChanSwAnn;

    IEEEtypes_Action_2040CoexFrame_t              pubCoex2040Frame;

    IEEEtypes_Action_Public_GAS_InitialRqst_t     pubGasReq;
    IEEEtypes_Action_Public_GAS_InitialResp_t     pubGasRsp;
    IEEEtypes_Action_Public_GAS_ComebackRqst_t    pubGasComebackReq;
    IEEEtypes_Action_Public_GAS_ComebackResp_t    pubGasComebackRsp;
#ifdef TDLS
    IEEEtypes_Action_Public_TDLS_DscvryRsp_t      pubTDLSDscvryRsp;
#endif
} PACK_END IEEEtypes_Action_Public_t;


/*
*****************************************************************************
**
**
**            802.11 Vendor Specific Action Frame types/structs
**
**
*****************************************************************************
*/
typedef PACK_START struct
{
    IEEEtypes_ActionCategory_e  category;
    UINT8                       Oui[3];
    UINT8                       Data[1];  /* Variable length vendor
                                          ** specific content
                                          */

} PACK_END IEEEtypes_Action_Base_VendorSpecific_t;

typedef PACK_START struct
{
    IEEEtypes_ActionCategory_e  category;
    UINT8                       Oui[3];
    UINT8                       OuiType;
    UINT8                       OuiSubType;
    UINT8                       DialogToken;
    UINT8                       Elements[1];
} PACK_END IEEEtypes_Action_Vendor_P2P_t;

typedef PACK_START union
{
    IEEEtypes_Action_Base_VendorSpecific_t        vendAct;
    IEEEtypes_Action_Vendor_P2P_t                 vendP2P;
} PACK_END IEEEtypes_Action_VendorSpecific_t;

typedef PACK_START struct IEEEtypes_Action_WPS_t
{
    UINT8 Category;
    UINT8 Action;
    UINT8 Oui[3];
    UINT8 OuiType;
    UINT8 OuiSubtype;
    UINT8 DialogToken;
    UINT8 IE_List[1];
} PACK_END IEEEtypes_Action_WPS_t;

/*
*****************************************************************************
**
**
**  Top level action frame (11k, 11h, WMM, 11n, 11r, 11v, 11w, public, etc)
**
**
*****************************************************************************
*/
typedef PACK_START union
{
    IEEEtypes_ActionCategory_e        category;

    IEEEtypes_Action_SpectrumMgmt_t   specMgmt;
    IEEEtypes_Action_RadioRsrc_t      radioRsrc;
    IEEEtypes_Action_WMMAC_t          wmmAc;
    IEEEtypes_Action_FastBssTrans_t   fastBssTrans;
    IEEEtypes_Action_BlockAck_t       blockAck;
    IEEEtypes_Action_WNM_t            wnm;
    IEEEtypes_Action_Public_t         public;
    IEEEtypes_Action_SAQ_ReqRsp_t     saQuery;
    IEEEtypes_Action_VendorSpecific_t vendor;
    IEEEtypes_Action_HT_t             htAction;
    IEEEtypes_Action_WPS_t            wps;
    IEEEtypes_Action_VHT_t            vhtAction;    
} PACK_END IEEEtypes_ActionFrame_t;

#endif

