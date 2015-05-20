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
#ifndef _KEY_MGMT_STA_ROM_H_
#define _KEY_MGMT_STA_ROM_H_

//#include "microTimer.h"
#include <wm_os.h>
#include "keyMgmtCommon.h"
#include "KeyApiStaDefs.h"
#include "tlv.h"
#include "hostMsgHdr.h"
#include "IEEE_types.h"
#include "keyApiStaTypes.h"
#include "customApp_mib.h"
#include "keyMgmtStaHostTypes.h"

typedef enum
{
    NO_MIC_FAILURE,
    FIRST_MIC_FAIL_IN_60_SEC,
    SECOND_MIC_FAIL_IN_60_SEC
} MIC_Fail_State_e;


typedef struct
{
    MIC_Fail_State_e status;
    BOOLEAN MICCounterMeasureEnabled;
    UINT32 disableStaAsso;
} MIC_Error_t;

typedef struct
{
    UINT8  TKIPICVErrors;
    UINT8  TKIPLocalMICFailures;
    UINT8  TKIPCounterMeasuresInvoked;

} customMIB_RSNStats_t;


typedef struct
{
    UINT8 ANonce[NONCE_SIZE];
    UINT8 SNonce[NONCE_SIZE];
    UINT8 EAPOL_MIC_Key[EAPOL_MIC_KEY_SIZE];
    UINT8 EAPOL_Encr_Key[EAPOL_ENCR_KEY_SIZE];
    UINT32 apCounterLo;       /* last valid replay counter from authenticator*/
    UINT32 apCounterHi;
    UINT32 apCounterZeroDone; /* have we processed replay == 0? */
    UINT32 staCounterLo;      /* counter used in request EAPOL frames */
    UINT32 staCounterHi;

    BOOLEAN RSNDataTrafficEnabled; /* Enabled after 4way handshake */
    BOOLEAN RSNSecured;            /* Enabled after group key is established */
    BOOLEAN pwkHandshakeComplete;
    cipher_key_t* pRxDecryptKey;

    KeyData_t PWKey;
    KeyData_t GRKey;

    KeyData_t newPWKey;

    MIC_Error_t    sta_MIC_Error;

    os_timer_t rsnTimer;
    os_timer_t micTimer;
    os_timer_t deauthDelayTimer;  /* hacked in to delay the deauth */

    struct cm_ConnectionInfo * connPtr;

    KeyData_t IGtk;
    
} keyMgmtInfoSta_t;

typedef struct
{
    UINT8 kck[16]; /* PTK_KCK = L(PTK,   0, 128); */
    UINT8 kek[16]; /* PTK_KEK = L(PTK, 128, 128); */
    UINT8 tk[16];  /* PTK_TK  = L(PTK, 256, 128); */
    
} CcmPtk_t;

typedef struct
{
    UINT8 kck[16]; /* PTK_KCK = L(PTK,   0, 128); */
    UINT8 kek[16]; /* PTK_KEK = L(PTK, 128, 128); */
    UINT8 tk[16];  /* PTK_TK  = L(PTK, 256, 128); */
    UINT8 rxMicKey[8];
    UINT8 txMicKey[8];

} TkipPtk_t;

#define PMKID_LEN 16

extern int isApReplayCounterFresh(keyMgmtInfoSta_t * pKeyMgmtInfoSta,
                           UINT8            * pRxReplayCount);

extern void updateApReplayCounter(keyMgmtInfoSta_t * pKeyMgmtStaInfo,
                           UINT8            * pRxReplayCount);

extern void formEAPOLEthHdr(EAPOL_KeyMsg_Tx_t* pTxEapol, 
                                            IEEEtypes_MacAddr_t *da, 
                                            IEEEtypes_MacAddr_t* sa);
extern KDE_t *parseKeyKDE(IEEEtypes_InfoElementHdr_t* pIe);

extern void supplicantGenerateSha1Pmkid(UINT8*               pPMK,
                                        IEEEtypes_MacAddr_t* pBssid,
                                        IEEEtypes_MacAddr_t* pSta,
                                        UINT8*               pPMKID);

extern BOOLEAN (*ComputeEAPOL_MIC_hook)(EAPOL_KeyMsg_t * pKeyMsg,
                                        UINT16           data_length,
                                        UINT8          * MIC_Key,
                                        UINT8            MIC_Key_length,
                                        UINT8            micKeyDescVersion);

extern void ComputeEAPOL_MIC(EAPOL_KeyMsg_t * pKeyMsg,
                             UINT16           data_length,
                             UINT8          * MIC_Key,
                             UINT8            MIC_Key_length,
                             UINT8            micKeyDescVersion);

extern BOOLEAN (*FillKeyMaterialStruct_internal_hook)(
    key_MgtMaterial_t * p_keyMgtData,
    UINT16              key_len,
    UINT8               isPairwise,
    KeyData_t         * pKey);

extern void FillKeyMaterialStruct_internal(key_MgtMaterial_t * p_keyMgtData,
                                           UINT16              key_len,
                                           UINT8               isPairwise,
                                           KeyData_t         * pKey);

extern BOOLEAN (*supplicantSetAssocRsn_internal_hook)(
    RSNConfig_t*      pRsnConfig,
    SecurityParams_t* pSecurityParams,
    SecurityMode_t    wpaType,
    Cipher_t*         pMcstCipher,
    Cipher_t*         pUcstCipher,
    AkmSuite_t*       pAkm,
    IEEEtypes_RSNCapability_t* pRsnCap,
    Cipher_t* pGrpMgmtCipher);

extern void supplicantSetAssocRsn_internal(RSNConfig_t*      pRsnConfig,
                                           SecurityParams_t* pSecurityParams,
                                           SecurityMode_t    wpaType,
                                           Cipher_t*         pMcstCipher,
                                           Cipher_t*         pUcstCipher,
                                           AkmSuite_t*       pAkm,
                                           IEEEtypes_RSNCapability_t* pRsnCap,
                                           Cipher_t* pGrpMgmtCipher);

extern BOOLEAN (*keyMgmtFormatWpaRsnIe_internal_hook)(
    RSNConfig_t*         pRsnConfig,
    UINT8*               pos,
    IEEEtypes_MacAddr_t* pBssid,
    IEEEtypes_MacAddr_t* pStaAddr,
    UINT8*               pPmkid,
    BOOLEAN              addPmkid,
    UINT16*              ptr_val);

extern UINT16 keyMgmtFormatWpaRsnIe_internal(RSNConfig_t*         pRsnConfig,
                                             UINT8*               pos,
                                             IEEEtypes_MacAddr_t* pBssid,
                                             IEEEtypes_MacAddr_t* pStaAddr,
                                             UINT8*               pPmkid,
                                             BOOLEAN              addPmkid);

extern BOOLEAN (*install_wpa_none_keys_internal_hook)(
    key_MgtMaterial_t* p_keyMgtData,
    UINT8             * pPMK,
    UINT8               type,
    UINT8               unicast);

extern void install_wpa_none_keys_internal(key_MgtMaterial_t * p_keyMgtData,
                                           UINT8             * pPMK,
                                           UINT8               type,
                                           UINT8               unicast);

extern BOOLEAN (*keyMgmtGetKeySize_internal_hook)(RSNConfig_t * pRsnConfig,
                                                  UINT8         isPairwise,
                                                  UINT16      * ptr_val);
extern UINT16 keyMgmtGetKeySize_internal(RSNConfig_t * pRsnConfig,
                                         UINT8         isPairwise);
extern BOOLEAN supplicantAkmIsWpaWpa2(AkmSuite_t* pAkm);
extern BOOLEAN supplicantAkmIsWpaWpa2Psk(AkmSuite_t* pAkm);
extern BOOLEAN supplicantAkmUsesKdf(AkmSuite_t* pAkm);
extern void supplicantConstructContext(IEEEtypes_MacAddr_t* pAddr1,
                                       IEEEtypes_MacAddr_t* pAddr2,
                                       UINT8* pNonce1,
                                       UINT8* pNonce2,
                                       UINT8* pContext);

extern
BOOLEAN (*parseKeyKDE_DataType_hook)(UINT8* pData, 
                                     SINT32  dataLen, 
                                     IEEEtypes_KDEDataType_e KDEDataType,
                                     UINT32 * ptr_val);

extern KDE_t *parseKeyKDE_DataType( UINT8* pData, 
                                    SINT32  dataLen, 
                                    IEEEtypes_KDEDataType_e KDEDataType);

extern BOOLEAN (*parseKeyDataGTK_hook)(UINT8* pKey,
                                       UINT16 len,
                                       KeyData_t* pGRKey,
                                       UINT32 * ptr_val);
extern KDE_t* parseKeyDataGTK(UINT8* pKey, UINT16 len, KeyData_t* pGRKey);

extern BOOLEAN IsEAPOL_MICValid(EAPOL_KeyMsg_t* pKeyMsg, UINT8 *pMICKey);

extern BOOLEAN (*KeyMgmtSta_ApplyKEK_hook)(EAPOL_KeyMsg_t* pKeyMsg, 
                                           KeyData_t* pGRKey, 
                                           UINT8* EAPOL_Encr_Key);
extern void KeyMgmtSta_ApplyKEK(EAPOL_KeyMsg_t* pKeyMsg, 
                                KeyData_t* pGRKey, 
                                UINT8* EAPOL_Encr_Key);

extern
BOOLEAN (*KeyMgmtSta_IsRxEAPOLValid_hook)(keyMgmtInfoSta_t* pKeyMgmtInfoSta,
                                          EAPOL_KeyMsg_t* pKeyMsg,
                                          BOOLEAN * ptr_val);

extern BOOLEAN KeyMgmtSta_IsRxEAPOLValid(keyMgmtInfoSta_t* pKeyMgmtInfoSta,
                                         EAPOL_KeyMsg_t* pKeyMsg);

extern 
BOOLEAN (*KeyMgmtSta_PrepareEAPOLFrame_hook)(EAPOL_KeyMsg_Tx_t* pTxEapol, 
                                             EAPOL_KeyMsg_t     *pRxEapol,
                                             IEEEtypes_MacAddr_t *da,
                                             IEEEtypes_MacAddr_t *sa,
                                             UINT8* pSNonce);
extern void KeyMgmtSta_PrepareEAPOLFrame(EAPOL_KeyMsg_Tx_t* pTxEapol, 
                                         EAPOL_KeyMsg_t     *pRxEapol,
                                         IEEEtypes_MacAddr_t *da,
                                         IEEEtypes_MacAddr_t *sa,
                                         UINT8* pSNonce);

extern
BOOLEAN (*KeyMgmtSta_PopulateEAPOLLengthMic_hook)(EAPOL_KeyMsg_Tx_t* pTxEapol,
                                                  UINT8* pEAPOLMICKey,
                                                  UINT8 eapolProtocolVersion,
                                                  UINT8  forceKeyDescVersion,
                                                  UINT16 * ptr_val);

extern UINT16 KeyMgmtSta_PopulateEAPOLLengthMic(EAPOL_KeyMsg_Tx_t* pTxEapol,
                                                UINT8* pEAPOLMICKey,
                                                UINT8 eapolProtocolVersion,
                                                UINT8  forceKeyDescVersion);

extern BOOLEAN (*KeyMgmtSta_PrepareEAPOLMicErrFrame_hook)(
    EAPOL_KeyMsg_Tx_t* pTxEapol,
    BOOLEAN isUnicast,
    IEEEtypes_MacAddr_t *da,
    IEEEtypes_MacAddr_t *sa,
    keyMgmtInfoSta_t* pKeyMgmtInfoSta);

extern
void KeyMgmtSta_PrepareEAPOLMicErrFrame(EAPOL_KeyMsg_Tx_t* pTxEapol, 
                                        BOOLEAN isUnicast,
                                        IEEEtypes_MacAddr_t *da,
                                        IEEEtypes_MacAddr_t *sa,
                                        keyMgmtInfoSta_t* pKeyMgmtInfoSta);

extern BOOLEAN supplicantAkmWpa2Ft( AkmSuite_t* pAkm );

extern BOOLEAN supplicantAkmUsesSha256Pmkid( AkmSuite_t* pAkm );

extern void supplicantGenerateSha256Pmkid(UINT8*               pPMK,
                                          IEEEtypes_MacAddr_t* pBssid,
                                          IEEEtypes_MacAddr_t* pSta,
                                          UINT8*               pPMKID);

extern BOOLEAN supplicantGetPmkid(IEEEtypes_MacAddr_t* pBssid,
                                  IEEEtypes_MacAddr_t* pStaAddr,
                                  AkmSuite_t*          pAkm,
                                  UINT8*               pPMKID);


extern void KeyMgmt_DerivePTK(IEEEtypes_MacAddr_t* pAddr1,
                              IEEEtypes_MacAddr_t* pAddr2,
                              UINT8*               pNonce1,
                              UINT8*               pNonce2,
                              UINT8*               pPTK,
                              UINT8*               pPMK,
                              BOOLEAN              use_kdf);

extern void KeyMgmtSta_DeriveKeys(UINT8 *pPMK, 
                                  IEEEtypes_MacAddr_t *da,
                                  IEEEtypes_MacAddr_t *sa,
                                  UINT8 *ANonce,
                                  UINT8 *SNonce,
                                  UINT8 *EAPOL_MIC_Key,
                                  UINT8 *EAPOL_Encr_Key,
                                  KeyData_t *newPWKey,
                                  BOOLEAN use_kdf);


extern void SetEAPOLKeyDescTypeVersion(EAPOL_KeyMsg_Tx_t* pTxEapol,
                                       BOOLEAN isWPA2,
                                       BOOLEAN isKDF,
                                       BOOLEAN nonTKIP);

extern void KeyMgmtResetCounter(keyMgmtInfoSta_t* pKeyMgmtInfo);

#if 0
extern void MicErrTimerExp_Sta(MicroTimerId_t timerId, UINT32 data);
#endif
extern void keyMgmtSta_StartSession_internal(keyMgmtInfoSta_t* pKeyMgmtInfoSta,
                                             void (*callback)(os_timer_arg_t),
                                             UINT32 expiry);

extern BOOLEAN (*ramHook_keyMgmtProcessMsgExt)(keyMgmtInfoSta_t* pKeyMgmtInfoSta,
                                               EAPOL_KeyMsg_t* pKeyMsg);
extern void (*ramHook_keyMgmtSendDeauth)(void *connPtr, UINT16 reason);

#if 0
extern void KeyMgmtSta_handleMICDeauthTimer(keyMgmtInfoSta_t* pKeyMgmtInfoSta,
                                            MicroTimerCallback_t callback,
                                            UINT32 expiry,
                                            UINT8 flags);

extern void KeyMgmtSta_handleMICErr(MIC_Fail_State_e state,
                                    keyMgmtInfoSta_t* pKeyMgmtInfoSta,
                                    MicroTimerCallback_t callback,
                                    UINT8 flags);

extern void DeauthDelayTimerExp_Sta(MicroTimerId_t timerId, UINT32 data);
#endif
extern void keyMgmtStaRsnSecuredTimeoutHandler(os_timer_t timerId);
                                               //UINT32 ctx);

extern EAPOL_KeyMsg_t* GetKeyMsgNonceFromEAPOL(
    BufferDesc_t* pEAPoLBufDesc,
    keyMgmtInfoSta_t* pKeyMgmtInfoSta);

extern EAPOL_KeyMsg_t  *ProcessRxEAPOL_PwkMsg3(
    BufferDesc_t *pEAPoLBufDesc,
    keyMgmtInfoSta_t* pKeyMgmtInfoSta);

extern EAPOL_KeyMsg_t *ProcessRxEAPOL_GrpMsg1(
    BufferDesc_t* pEAPoLBufDesc,
    keyMgmtInfoSta_t* pKeyMgmtInfoSta);

extern void KeyMgmtSta_InitSession(keyMgmtInfoSta_t* pKeyMgmtInfoSta);

extern void supplicantParseMcstCipher(Cipher_t* pMcstCipherOut, 
    UINT8* pGrpKeyCipher);


extern void supplicantParseUcstCipher(Cipher_t* pUcstCipherOut,
                               UINT8 pwsKeyCnt,
                               UINT8* pPwsKeyCipherList);

#endif

