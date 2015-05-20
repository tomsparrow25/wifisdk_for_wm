/******************** (c) Marvell Semiconductor, Inc., 2006 *******************
 *
 * Purpose:
 *
 * Public Procedures:
 *    None.
 *
 * Notes:
 *    None.
 *
 *****************************************************************************/

#ifndef _IEEE_WMM_TYPES_H_
#define _IEEE_WMM_TYPES_H_

#include "IEEE_types.h"

#define WMM_STATS_PKTS_HIST_BINS 7

typedef PACK_START enum
{
    AckPolicy_ImmediateAck = 0,
    AckPolicy_NoAck        = 1,
    AckPolicy_ExplicitAck  = 2,
    AckPolicy_BlockAck     = 3,
    
} PACK_END IEEEtypes_AckPolicy_e;

typedef PACK_START struct
{
    UINT16 userPriority             : 3;
    UINT16 reserved1                : 1;
    UINT16 eosp                     : 1;
    IEEEtypes_AckPolicy_e ackPolicy : 2;
    UINT16 amsdu                    : 1;
    UINT16 reserved2                : 8;

} PACK_END IEEEtypes_QosCtl_t;



typedef PACK_START struct
{
    UINT8 Version;
    UINT8 SourceIpAddr[4];
    UINT8 DestIpAddr[4];
    UINT8 SourcePort[2];
    UINT8 DestPort[2];
    UINT8 DSCP;
    UINT8 Protocol;
    UINT8 Reserved;

} PACK_END IEEEtypes_TCLAS_IPv4_t;


typedef PACK_START struct
{
    IEEEtypes_ElementId_e ElementId;
    IEEEtypes_Len_t Len;
    
    UINT8 UserPriority;

    UINT8 ClassifierType;
    UINT8 ClassifierMask;
    
    PACK_START union
    {
        IEEEtypes_TCLAS_IPv4_t ipv4;

    } PACK_END classifier;

} PACK_END IEEEtypes_TCLAS_t;

typedef enum
{
    AC_BE = 0x0,
    AC_BK,
    AC_VI,
    AC_VO,
    
    AC_MAX_TYPES

} IEEEtypes_WMM_AC_e;

#define WMM_MAX_TIDS              8
#define WMM_MAX_RX_PN_SUPPORTED   4

typedef PACK_START struct 
{
    UINT8 Aifsn : 4;
    UINT8 Acm   : 1;
    UINT8 Aci   : 2;
    UINT8 Rsvd1 : 1;

} PACK_END IEEEtypes_WMM_AC_ACI_AIFSN_t;

typedef PACK_START struct 
{
    UINT8 EcwMin : 4;
    UINT8 EcwMax : 4;

} PACK_END IEEEtypes_ECW_Min_Max_t;

typedef PACK_START struct 
{
    IEEEtypes_WMM_AC_ACI_AIFSN_t AciAifsn;
    IEEEtypes_ECW_Min_Max_t      EcwMinMax;
    UINT16 TxopLimit;

} PACK_END IEEEtypes_WMM_AC_Parameters_t;

typedef PACK_START struct
{
    UINT8 ParamSetCount : 4;
    UINT8 Reserved1 : 3;
    UINT8 QosInfoUAPSD : 1;

} PACK_END IEEEtypes_QAP_QOS_Info_t;

typedef PACK_START struct 
{
    UINT8 AC_VO:1;
    UINT8 AC_VI:1;
    UINT8 AC_BK:1;
    UINT8 AC_BE:1;
    UINT8 QAck:1;
    UINT8 MaxSP:2;
    UINT8 MoreDataAck:1;

} PACK_END IEEEtypes_QSTA_QOS_Info_t;

typedef PACK_START union
{
    IEEEtypes_QAP_QOS_Info_t  QAp;
    IEEEtypes_QSTA_QOS_Info_t QSta;

} PACK_END IEEEtypes_QOS_Info_t;
    

//added for TDLS
typedef PACK_START struct
{
    IEEEtypes_ElementId_e ElementId;
    IEEEtypes_Len_t Len;
    IEEEtypes_QOS_Info_t QosInfo;
} PACK_END IEEEtypes_QOS_Capability_t;    

typedef PACK_START struct 
{
    IEEEtypes_ElementId_e ElementId;
    IEEEtypes_Len_t Len;
    UINT8 OuiType[4];     /* 00:50:f2:02 */
    UINT8 OuiSubType;     /* 01 */
    UINT8 Version;

    IEEEtypes_QOS_Info_t QosInfo;
    UINT8 Reserved1;
    IEEEtypes_WMM_AC_Parameters_t AcParams[AC_MAX_TYPES];

} PACK_END IEEEtypes_WMM_ParamElement_t;

typedef PACK_START struct 
{
    IEEEtypes_ElementId_e ElementId;
    IEEEtypes_Len_t Len;
    UINT8 OuiType[4];     /* 00:50:f2:02 */
    UINT8 OuiSubType;     /* 00 */
    UINT8 Version;

    IEEEtypes_QOS_Info_t QosInfo;

} PACK_END IEEEtypes_WMM_InfoElement_t;


typedef PACK_START enum
{
    TSPEC_DIR_UPLINK   = 0,
    TSPEC_DIR_DOWNLINK = 1,
    /* 2 is a reserved value */
    TSPEC_DIR_BIDIRECT = 3,

} PACK_END IEEEtypes_WMM_TSPEC_TS_Info_Direction_e;


typedef PACK_START enum
{
    TSPEC_PSB_LEGACY = 0,
    TSPEC_PSB_TRIG   = 1,

} PACK_END IEEEtypes_WMM_TSPEC_TS_Info_PSB_e;

typedef PACK_START enum
{
    /* 0 is reserved */
    TSPEC_ACCESS_EDCA = 1,
    TSPEC_ACCESS_HCCA = 2,
    TSPEC_ACCESS_HEMM = 3,

} PACK_END IEEEtypes_WMM_TSPEC_TS_Info_AccessPolicy_e;


typedef PACK_START enum
{
    TSPEC_ACKPOLICY_NORMAL = 0,
    TSPEC_ACKPOLICY_NOACK  = 1,
    /* 2 is reserved */
    TSPEC_ACKPOLICY_BLOCKACK =  3,

} PACK_END IEEEtypes_WMM_TSPEC_TS_Info_AckPolicy_e;


typedef PACK_START enum
{
    TSPEC_TRAFFIC_APERIODIC = 0,
    TSPEC_TRAFFIC_PERIODIC  = 1,

} PACK_END IEEEtypes_WMM_TSPEC_TS_TRAFFIC_TYPE_e;

typedef PACK_START struct 
{
    IEEEtypes_WMM_TSPEC_TS_TRAFFIC_TYPE_e TrafficType : 1; 
    UINT8 TID : 4;        //! Unique identifier
    IEEEtypes_WMM_TSPEC_TS_Info_Direction_e Direction : 2;
    UINT8 acp_1 : 1;

    UINT8 acp_2 : 1;
    UINT8 Aggregation : 1; 
    IEEEtypes_WMM_TSPEC_TS_Info_PSB_e PSB : 1;  //! Legacy/Trigg
    UINT8 UserPriority: 3; //! 802.1d User Priority
    IEEEtypes_WMM_TSPEC_TS_Info_AckPolicy_e AckPolicy : 2;

    UINT8 tsinfo_0 : 8;
} PACK_END IEEEtypes_WMM_TSPEC_TS_Info_t;

typedef PACK_START struct 
{
    UINT16 Size : 15;  //! Nominal size in octets
    UINT16 Fixed : 1;  //! 1: Fixed size given in Size, 0: Var, size is nominal

} PACK_END IEEEtypes_WMM_TSPEC_NomMSDUSize_t;


typedef PACK_START struct 
{
    UINT16 Fractional : 13;  //! Fractional portion
    UINT16 Whole : 3;        //! Whole portion

} PACK_END IEEEtypes_WMM_TSPEC_SBWA;

typedef PACK_START struct 
{
    IEEEtypes_WMM_TSPEC_TS_Info_t TSInfo;
    IEEEtypes_WMM_TSPEC_NomMSDUSize_t NomMSDUSize;
    UINT16 MaximumMSDUSize;
    UINT32 MinServiceInterval;
    UINT32 MaxServiceInterval;
    UINT32 InactivityInterval;
    UINT32 SuspensionInterval;
    UINT32 ServiceStartTime;
    UINT32 MinimumDataRate;
    UINT32 MeanDataRate;
    UINT32 PeakDataRate;
    UINT32 MaxBurstSize;
    UINT32 DelayBound;
    UINT32 MinPHYRate;
    IEEEtypes_WMM_TSPEC_SBWA SurplusBWAllowance;
    UINT16 MediumTime;
} PACK_END IEEEtypes_WMM_TSPEC_Body_t;


typedef PACK_START struct 
{
    UINT8 ElementId;
    UINT8 Len;
    UINT8 OuiType[4];     /* 00:50:f2:02 */
    UINT8 OuiSubType;     /* 01 */
    UINT8 Version;

    IEEEtypes_WMM_TSPEC_Body_t TspecBody;

} PACK_END IEEEtypes_WMM_TSPEC_t;

typedef PACK_START enum
{
    TSPEC_ACTION_CODE_ADDTS_REQ = 0,
    TSPEC_ACTION_CODE_ADDTS_RSP = 1,
    TSPEC_ACTION_CODE_DELTS     = 2,
    
} PACK_END IEEEtypes_WMM_Tspec_Action_e;

typedef PACK_START struct
{
    IEEEtypes_ActionCategory_e     category;
    IEEEtypes_WMM_Tspec_Action_e   action;
    UINT8                          dialogToken;

} PACK_END IEEEtypes_WMM_Tspec_Action_Base_Tspec_t;

/* Allocate enough space for a V4 TCLASS + a small CCX or VendSpec IE */
#define WMM_TSPEC_EXTRA_IE_BUF_MAX  (sizeof(IEEEtypes_TCLAS_t) + 6)

typedef PACK_START struct
{
    IEEEtypes_WMM_Tspec_Action_Base_Tspec_t tspecAct;
    UINT8                                   statusCode;
    IEEEtypes_WMM_TSPEC_t                   tspecIE;

    /* Place holder for additional elements after the TSPEC */
    UINT8  subElem[WMM_TSPEC_EXTRA_IE_BUF_MAX];

} PACK_END IEEEtypes_Action_WMM_AddTsReq_t;


typedef PACK_START struct
{
    IEEEtypes_WMM_Tspec_Action_Base_Tspec_t tspecAct;
    UINT8                                   statusCode;
    IEEEtypes_WMM_TSPEC_t                   tspecIE;

    /* Place holder for additional elements after the TSPEC */
    UINT8                                   subElem[256];

} PACK_END IEEEtypes_Action_WMM_AddTsRsp_t;


typedef PACK_START struct
{
    IEEEtypes_WMM_Tspec_Action_Base_Tspec_t tspecAct;
    UINT8                                   reasonCode;
    IEEEtypes_WMM_TSPEC_t                   tspecIE;

} PACK_END IEEEtypes_Action_WMM_DelTs_t;


typedef PACK_START union
{
    IEEEtypes_WMM_Tspec_Action_Base_Tspec_t tspecAct;

    IEEEtypes_Action_WMM_AddTsReq_t         addTsReq;
    IEEEtypes_Action_WMM_AddTsRsp_t         addTsRsp;
    IEEEtypes_Action_WMM_DelTs_t            delTs;

} PACK_END IEEEtypes_Action_WMMAC_t;

#if defined(BTAMP)
typedef PACK_START struct
{
    IEEEtypes_ElementId_e ElementId;
    IEEEtypes_Len_t       Len;
    IEEEtypes_QOS_Info_t  QosInfo;
}
PACK_END IEEEtypes_QoSCapabilityElement_t;

typedef PACK_START struct
{
    IEEEtypes_QOS_Info_t          QosInfo;
    UINT8                         Reserved1;
    IEEEtypes_WMM_AC_Parameters_t AcParams[AC_MAX_TYPES];

} PACK_END IEEEtypes_EDCAParamSetBody_t;

typedef PACK_START struct
{
    IEEEtypes_ElementId_e        ElementId;
    IEEEtypes_Len_t              Len;
    IEEEtypes_EDCAParamSetBody_t Body;

} PACK_END IEEEtypes_EDCAParamSet_t;
#endif
    
#endif /* _IEEE_WMM_TYPES_H_ */
