
#ifndef _KEYMGMTAP_H_
#define _KEYMGMTAP_H_
//Authenticator related data structures, function prototypes 

#include "wltypes.h"
#include "w81connmgr.h"
#include "bufferMgmtLib.h"
#include "keyMgmtAp_rom.h"
#include "macMgmtMain.h"

//#ifdef AUTHENTICATOR

#define STA_PS_EAPOL_HSK_TIMEOUT 3000  //ms
#define AP_RETRY_EAPOL_HSK_TIMEOUT 1000  //ms
extern Status_e ProcessKeyMgmtDataAp(BufferDesc_t *pBufDesc);
extern Status_e GenerateApEapolMsg(cm_ConnectionInfo_t *connPtr, 
                                   keyMgmtState_e msgState,
                                   BufferDesc_t* pBufDesc);
extern void KeyMgmtInit(cm_ConnectionInfo_t *connPtr);
extern void ReInitGTK(cm_ConnectionInfo_t *connPtr);
extern BOOLEAN IsAuthenticatorEnabled(cm_ConnectionInfo_t *connPtr);
extern void KeyMgmtGrpRekeyCountUpdate(cm_ConnectionInfo_t *connPtr);
extern void KeyMgmtStartHskTimer(cm_ConnectionInfo_t *connPtr);
extern void KeyMgmtStopHskTimer(cm_ConnectionInfo_t *connPtr);
extern BOOLEAN SendEAPOLMsgUsingBufDesc(cm_ConnectionInfo_t *connPtr,
                                        BufferDesc_t* pBufDesc);
extern void InitGroupKey(cm_ConnectionInfo_t *connPtr);

//#endif //AUTHENTICATOR

//#ifdef AP_WPA
extern void ApDisAssocAllSta(cm_ConnectionInfo_t *connPtr);
extern void ApMicCounterMeasureInvoke(cm_ConnectionInfo_t *connPtr);
extern int keyMgmtAp_FormatWPARSN_IE(IEEEtypes_InfoElementHdr_t *pIe, 
                        UINT8 isRsn,
                        Cipher_t *pCipher,
                        UINT8 cipherCount,
                        Cipher_t *pMcastCipher,
                        UINT16 authKey,
                        UINT16 authKeyCount);
extern void ApMicErrTimerExpCb(os_timer_t timerId, UINT32 data);
extern void InitStaKeyInfo(void *pConn,
                    SecurityMode_t *secType,
                    Cipher_t *pwCipher,
                    UINT16 staRsnCap,
                    UINT8 akmType);
void RemoveStaKeyInfo(void *pConn,
                    IEEEtypes_MacAddr_t *peerMacAddr);
//#endif

#endif //_KEYMGMTAP_H_
