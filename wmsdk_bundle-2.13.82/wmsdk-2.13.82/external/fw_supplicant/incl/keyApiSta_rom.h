/************************************************************************
*
* File: keyApiSta.h
*
*
*
* Author(s):
* Date:
* Description:
*
*************************************************************************/
#ifndef _KEY_API_STA_ROM_H_
#define _KEY_API_STA_ROM_H_

#include "KeyApiStaDefs.h"
#include "keyApiStaTypes.h"
//#include "wl_mib.h"
//#include "hash.h"
#include "bufferMgmtLib.h"
#include "keyMgmtStaHostTypes.h"

extern void keyApi_DisableKey_internal(void *         connPtr, 
                                       cipher_key_t * pwkey,
                                       cipher_key_t * gwkey,
                                       BOOLEAN        disableUnicast,
                                       BOOLEAN        disableMulticast);

extern UINT32 keyApi_UpdateKeyMaterial_internal(void              * connPtr,
                                                cipher_key_t      * pwkey,
                                                cipher_key_t      * gwkey,
#ifndef WAR_ROM_BUG54733_PMF_SUPPORT
		  									    cipher_key_t	  * igwkey,
#endif		  									    
                                                key_MgtMaterial_t * keyMgtData_p);

extern UINT16 keyApi_GetKeyMaterial_internal(cipher_key_t      * pwkey,
                                             cipher_key_t      * gwkey,
                                             key_MgtMaterial_t *keyMgtData_p,
                                             BOOLEAN getUnicastKey);

extern int keyApi_NWF_UpdateKey(cipher_key_t *pDst, uint8 *pSrc);
extern BOOLEAN (*keyApi_NWF_UpdateKey_hook)(cipher_key_t *pDst, uint8 *pSrc);

extern void keyApi_ApUpdateKeyMaterial_internal( BOOLEAN updateGrpKey,
                                  cipher_key_t *pCipherKey,
                                  Cipher_t *pCipher,
                                  KeyData_t *pKeyData);

extern UINT8 convertMrvlCipherToIEEECipher(UINT8 mrvlcipher);
extern UINT8 convertMrvlAuthToIEEEAuth(UINT32 mrvlauth);

#if 0 //not ready yet
extern UINT32 keyApi_UpdateKey_WAPI(void* connPtr,
                BOOLEAN isAPConn,
                IEEEtypes_MacAddr_t peerMacAddr,
                cipher_key_t        * pwkey,
                cipher_key_t        * gwkey,
                key_MgtMaterial_t   * keyMgtData_p);
#endif

extern void (*ramHook_keyApi_PalladiumHook1)(void *connPtr);
extern void (*ramHook_keyApi_PalladiumHook2)(void *connPtr);

extern void  (*ramHook_keyApiSta_setConnMacCtrlWepEn)(void * connPtr, UINT8 wepEn);
extern UINT8 (*ramHook_keyApiSta_getConnMacCtrlWepEn)(void * connPtr);
extern void  (*ramHook_keyApiSta_setConnMacCtrlProtection)(void * connPtr, UINT8 prot);
extern void  (*ramHook_keyApiSta_setConnDataTrafficEnabled)(void * connPtr, UINT8 state);
extern void  (*ramHook_keyApiSta_setConnCurPktTxEnabled)(void * connPtr, UINT8 state);
extern void (*ramHook_keyApiSta_setConnWAPIEnabled)(void * connPtr, UINT8  state);

#endif /* _KEY_API_STA_ROM_H_ end */

