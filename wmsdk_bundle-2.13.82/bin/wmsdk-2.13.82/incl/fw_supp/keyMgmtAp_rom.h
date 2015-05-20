#ifndef KEYMGMTAP_ROM_H__
#define KEYMGMTAP_ROM_H__
//Authenticator related data structures, function prototypes 

#include "wltypes.h"
#include "IEEE_types.h"
//#include "sha1.h"
#include "keyMgmtSta_rom.h"
#include "keyMgmtStaTypes.h"
#include "supplicantApi.h"
//#include "mrvlWlanPoolConfig.h"
#include "keyApiSta_rom.h"
#include "tkip_rom.h"
#include "wl_macros.h"
#include "keyMgmtApTypes_rom.h"
//#include "rc4_rom.h"

/* This flags if the Secure flag in EAPOL key frame must be set */
#define SECURE_HANDSHAKE_FLAG 0x0080
/* Whether the EAPOL frame is for pairwise or group Key */
#define PAIRWISE_KEY_MSG    0x0800
/* Flags when WPA2 is enabled, not used for WPA */
#define WPA2_HANDSHAKE      0x8000

extern void GenerateGTK_internal(KeyData_t *grpKeyData, UINT8* nonce, 
                                 IEEEtypes_MacAddr_t StaMacAddr);

extern void PopulateKeyMsg(EAPOL_KeyMsg_Tx_t *tx_eapol_ptr,
                           Cipher_t * Cipher,
                           UINT16 Type,
                           UINT32  replay_cnt[2],
                           UINT8* Nonce);

extern void prepareKDE(EAPOL_KeyMsg_Tx_t *tx_eapol_ptr,
                       KeyData_t *grKey,
                       Cipher_t *cipher);

extern BOOLEAN Encrypt_keyData(EAPOL_KeyMsg_Tx_t *tx_eapol_ptr,
                               UINT8 * EAPOL_Encr_Key,
                               Cipher_t *cipher);

extern void KeyMgmtAp_DerivePTK(UINT8 *pPMK, 
                                IEEEtypes_MacAddr_t *da,
                                IEEEtypes_MacAddr_t *sa,
                                UINT8 *ANonce,
                                UINT8 *SNonce,
                                UINT8 *EAPOL_MIC_Key,
                                UINT8 *EAPOL_Encr_Key,
                                KeyData_t *newPWKey,
                                BOOLEAN use_kdf);

extern BOOLEAN (*PopulateKeyMsg_hook)(EAPOL_KeyMsg_Tx_t *tx_eapol_ptr,
                                      Cipher_t * Cipher,
                                      UINT16 Type,
                                      UINT32  replay_cnt[2],
                                      UINT8* Nonce);

extern BOOLEAN (*prepareKDE_hook)(EAPOL_KeyMsg_Tx_t *tx_eapol_ptr,
                                  KeyData_t *grKey,
                                  Cipher_t *cipher);

extern BOOLEAN (*Encrypt_keyData_hook)(EAPOL_KeyMsg_Tx_t *tx_eapol_ptr,
                                       UINT8 * EAPOL_Encr_Key,
                                       Cipher_t *cipher,
                                       BOOLEAN * ptr_val);

extern int keyMgmtAp_FormatWPARSN_IE_internal(IEEEtypes_InfoElementHdr_t *pIe, 
                                              UINT8 isRsn,
                                              Cipher_t* pCipher,
                                              UINT16 cipherCnt,
                                              Cipher_t* pMcastCipher,
                                              UINT16 authKey,
                                              UINT16 authKeyCnt);

extern void ROM_InitGTK(KeyData_t *grpKeyData,
                        UINT8* nonce, 
                        IEEEtypes_MacAddr_t StaMacAddr);

extern void KeyData_AddGTK(EAPOL_KeyMsg_Tx_t *pTxEAPOL,
                           KeyData_t *grKey,
                           Cipher_t *cipher);

extern BOOLEAN KeyData_AddKey(EAPOL_KeyMsg_Tx_t *pTxEAPOL,
                              SecurityMode_t *pSecType,
                              KeyData_t *grKey,
                              Cipher_t *cipher);

extern BOOLEAN KeyData_CopyWPAWP2(EAPOL_KeyMsg_Tx_t *pTxEAPOL,
                                  void *pIe);

extern BOOLEAN KeyData_UpdateKeyMaterial(EAPOL_KeyMsg_Tx_t *pTxEAPOL,
                                         SecurityMode_t *pSecType,
                                         void* pWPA,
                                         void* pWPA2 );

extern void InitKeyMgmtInfo(apKeyMgmtInfoStaRom_t *pKeyMgmtInfo,
                               SecurityMode_t *secType,
                               Cipher_t *pwCipher,
                               UINT16 staRsnCap,
                               UINT8 akmType);
#endif //_KEYMGMTAP_ROM_H_

