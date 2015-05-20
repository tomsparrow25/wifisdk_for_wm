/*
**                Copyright 2007, Marvell International Ltd.
** This code contains confidential information of Marvell Semiconductor, Inc.
** No rights are granted herein under any patent, mask work right or copyright
** of Marvell or any third party.
** Marvell reserves the right at its sole discretion to request that this code
** be immediately returned to Marvell. This code is provided "as is".
** Marvell makes no warranties, express, implied or otherwise, regarding its
** accuracy, completeness or performance.
*/
#ifndef _KEY_MGMT_STA_H_
#define _KEY_MGMT_STA_H_

//#include "timer.h"
#include "keyMgmtCommon.h"
#include "KeyApiStaDefs.h"
#include "tlv.h"
#include "hostMsgHdr.h"
#include "IEEE_types.h"
#include "keyMgmtSta_rom.h"
#include "w81connmgr.h"
#include "keyMgmtStaTypes.h"

#ifdef BTAMP
#include "btamp_config.h"
#define BTAMP_SUPPLICATNT_SESSIONS  AMPHCI_MAX_PHYSICAL_LINK_SUPPORTED
#else
#define BTAMP_SUPPLICATNT_SESSIONS  0
#endif

#ifdef MULTI_PSK_SUPPLICANT
#define MAX_SUPPLICANT_SESSIONS     (2 + BTAMP_SUPPLICATNT_SESSIONS) 
#else
#define MAX_SUPPLICANT_SESSIONS     (1 + BTAMP_SUPPLICATNT_SESSIONS)
#endif

typedef struct supplicantData
{
    BOOLEAN                     inUse;
    IEEEtypes_SsIdElement_t     hashSsId;
    IEEEtypes_MacAddr_t         localBssid;
    IEEEtypes_MacAddr_t         localStaAddr;
    customMIB_RSNStats_t        customMIB_RSNStats;
    RSNConfig_t                 customMIB_RSNConfig;
    keyMgmtInfoSta_t            keyMgmtInfoSta;
    SecurityParams_t            currParams;
} supplicantData_t;

extern BOOLEAN supplicantAkmIsWpaWpa2(AkmSuite_t* pAkm);
extern BOOLEAN supplicantAkmIsWpaWpa2Psk(AkmSuite_t* pAkm);
extern BOOLEAN supplicantAkmUsesKdf(AkmSuite_t* pAkm);
extern BOOLEAN supplicantGetPmkid(IEEEtypes_MacAddr_t* pBssid,
                                  IEEEtypes_MacAddr_t* pSta,
                                  AkmSuite_t* pAkm,
                                  UINT8* pPMKID);

extern void supplicantConstructContext(IEEEtypes_MacAddr_t* pAddr1,
                                       IEEEtypes_MacAddr_t* pAddr2,
                                       UINT8* pNonce1,
                                       UINT8* pNonce2,
                                       UINT8* pContext);

extern void keyMgmtSetIGtk(keyMgmtInfoSta_t* pKeyMgmtInfoSta,
                           IGtkKde_t* pIGtkKde,
                           UINT8 iGtkKdeLen);

extern UINT8* keyMgmtGetIGtk(struct cm_ConnectionInfo* connPtr);

extern void keyMgmtSta_RomInit(void);


extern void UpdateEAPOLWcbLenAndTransmit(BufferDesc_t* pBufDesc,
                                         UINT16 frameLen);

extern BufferDesc_t* GetTxEAPOLBuffer(struct cm_ConnectionInfo* connPtr,
                                      EAPOL_KeyMsg_Tx_t** ppTxEapol,
                                      BufferDesc_t* pBufDesc);
extern void allocSupplicantData(void *connPtr);
extern void freeSupplicantData(void *connPtr);
extern void supplicantInit(supplicantData_t* suppData);
extern BOOLEAN keyMgmtProcessMsgExt(keyMgmtInfoSta_t* pKeyMgmtInfoSta,
                                    EAPOL_KeyMsg_t* pKeyMsg);

void supplicantEnable(void *connectionPtr, int security_mode,
		void *mcstCipher, void *ucstCipher);
#endif
