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
#ifndef _SUPPLICANT_API_H_
#define _SUPPLICANT_API_H_

#include "keyMgmtStaTypes.h"

extern UINT8 supplicantIsEnabled(void *connPtr);
extern UINT8 supplicantIsCounterMeasuresActive(
                                         struct cm_ConnectionInfo *connPtr);
extern void supplicantFuncInit(void);
extern Status_e supplicantRestoreDefaults(void);

extern
void supplicantMICCounterMeasureInvoke(struct cm_ConnectionInfo *connPtr,
                                       BOOLEAN isUnicast);


extern void supplicantSetAssocRsn(void *connPtr,
                                  SecurityMode_t wpaType,
                                  Cipher_t* pMcstCipher,
                                  Cipher_t* pUcstCipher,
                                  AkmSuite_t* pAkm,
                                  IEEEtypes_RSNCapability_t* pRsnCap,
                                  Cipher_t* pGrpMgmtCipher);
extern void supplicantGenerateRand(UINT8* dataOut, UINT32 length);

extern BOOLEAN (*supplicantParseWpaIe_hook)(IEEEtypes_WPAElement_t* pIe,
                                            SecurityMode_t* pWpaType,
                                            Cipher_t* pMcstCipher,
                                            Cipher_t* pUcstCipher,
                                            AkmSuite_t* pAkmList,
                                            UINT8      numAkm);

extern void supplicantParseWpaIe(IEEEtypes_WPAElement_t* pIe,
                                 SecurityMode_t* pWpaType,
                                 Cipher_t* pMcstCipher,
                                 Cipher_t* pUcstCipher,
                                 AkmSuite_t* pAkmList,
                                 UINT8      numAkm);

extern BOOLEAN (*supplicantParseRsnIe_hook)(IEEEtypes_RSNElement_t* pIe,
                                            SecurityMode_t* pWpaType,
                                            Cipher_t* pMcstCipher,
                                            Cipher_t* pUcstCipher,
                                            AkmSuite_t* pAkmList,
                                            UINT8      numAkm,
                                            IEEEtypes_RSNCapability_t* pRsnCap,
                                            Cipher_t* pGrpMgmtCipher);


extern void supplicantParseRsnIe(IEEEtypes_RSNElement_t* pIe,
                                 SecurityMode_t* pWpaType,
                                 Cipher_t* pMcstCipher,
                                 Cipher_t* pUcstCipher,
                                 AkmSuite_t* pAkmList,
                                 UINT8      numAkm,
                                 IEEEtypes_RSNCapability_t* pRsnCap,
                                 Cipher_t* pGrpMgmtCipher);

void supplicantEnable(void *connectionPtr, int security_mode,
		void *mcstCipher, void *ucstCipher);
//extern void supplicantEnable(void *connPtr);
extern void supplicantDisable(struct cm_ConnectionInfo *connPtr);
extern void supplicantRemoveKeyInfo(struct cm_ConnectionInfo *connPtr);

extern void supplicantStopSessionTimer(struct cm_ConnectionInfo *connPtr);
extern void supplicantSmeResetNotification(struct cm_ConnectionInfo *connPtr);

extern void supplicantInitSession(struct cm_ConnectionInfo *connPtr, 
                                  CHAR* pSsid,
                                  UINT16 len,
                                  CHAR* pBssid,
                                  UINT8* pStaAddr);

extern void supplicantGetProfile(struct cm_ConnectionInfo* connPtr,
                                 SecurityMode_t* pWpaType,
                                 Cipher_t* pMcstCipher,
                                 Cipher_t* pUcstCipher);

extern void supplicantSetProfile(struct cm_ConnectionInfo* connPtr,
                                 SecurityMode_t wpaType,
                                 Cipher_t mcstCipher,
                                 Cipher_t ucstCipher);

extern void supplicantGetProfileCurrent(struct cm_ConnectionInfo* connPtr,
                                        SecurityMode_t* pWpaType,
                                        Cipher_t* pMcstCipher,
                                        Cipher_t* pUcstCipher);

extern SecurityMode_t supplicantCurrentSecurityMode(
                                          struct cm_ConnectionInfo *connPtr);

extern AkmSuite_t* supplicantCurrentAkmSuite(
                                          struct cm_ConnectionInfo *connPtr);


extern const uint8 wpa_oui02[4];   /* WPA TKIP */
extern const uint8 wpa_oui04[4];   /* WPA AES */
extern const uint8 wpa_oui01[4];   /* WPA WEP-40 */
extern const uint8 wpa_oui05[4];   /* WPA WEP-104 */
extern const uint8 wpa_oui_none[4];   /* WPA NONE */

extern const uint8 wpa2_oui02[4];   /* WPA2 TKIP */
extern const uint8 wpa2_oui04[4];   /* WPA2 AES */
extern const uint8 wpa2_oui01[4];   /* WPA2 WEP-40 */
extern const uint8 wpa2_oui05[4];   /* WPA2 WEP-104 */

extern const uint8 wpa_oui[3];
extern const uint8 kde_oui[3];

extern void supplicantInstallWpaNoneKeys(struct cm_ConnectionInfo *connPtr);

BOOLEAN is_rx_eapol_pkt(uint16 protocol_type);

#if defined(BTAMP)
UINT16 btampAddRsnIe(struct cm_ConnectionInfo *connPtr,
                     IEEEtypes_RSNElement_t* pRsnIe);
extern void AmpKeyMgmtInfoInitSession(struct cm_ConnectionInfo *connPtr);
#endif
#endif

