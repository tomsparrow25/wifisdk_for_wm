/************************************************************************
*
* File: keyApiStaTypes.h
*
* NOTE:
    Some of the macros and types defined by this sources
    are used by ROM routines.
    Please do not change those macros and types, unless you got
    opportunity to change corresponding ROM code without
    breaking anything working.
*
* Author(s):
* Date:
* Description:
*
*************************************************************************/
#ifndef KEY_API_STA_TYPES_ROM_H__
#define KEY_API_STA_TYPES_ROM_H__

#include "IEEE_types.h"
//#include "wl_mib_rom.h"
#include "keyMgmtCommon.h"
#include "keyMgmtStaHostTypes_rom.h"
#include "KeyApiStaDefs.h"

#if 0
#define MAX_WEP_KEYS                        4
/* This structure is used in rom and existing fields should not be changed */
/* This structure is already aligned and hence packing is not needed */

typedef PACK_START struct cipher_key_hdr_t
{
    IEEEtypes_MacAddr_t         macAddr;
    UINT8                       keyDirection;
    UINT8                       keyType:4;
    UINT8                       version:4;
    UINT16                      keyLen;
    UINT8                       keyState;
    UINT8                       keyInfo;
} PACK_END cipher_key_hdr_t;

/* This structure is used in rom and existing fields should not be changed */
typedef struct tkip_aes_key_data_t
{
    // key material information (TKIP/AES/WEP)
    UINT8                       key[CRYPTO_KEY_LEN_MAX];
    UINT8                       txMICKey[MIC_KEY_LEN_MAX];
    UINT8                       rxMICKey[MIC_KEY_LEN_MAX];
    UINT32                      hiReplayCounter32;  //!< initialized by host
    UINT16                      loReplayCounter16;  //!< initialized by host
    UINT32                      txIV32;             //!< controlled by FW
    UINT16                      txIV16;             //!< controlled by FW
    UINT32                      TKIPMicLeftValue;
    UINT32                      TKIPMicRightValue;

    /* HW new design for 8682 only to support interleaving
     * FW need to save these value and
     * restore for next fragment
     */
    UINT32                      TKIPMicData0Value;
    UINT32                      TKIPMicData1Value;
    UINT32                      TKIPMicData2Value;
    UINT8                       keyIdx;
    UINT8                       reserved[3];

}
tkip_aes_key_data_t;

/* This structure is used in rom and existing fields should not be changed */
typedef struct wep_key_data_t
{
    MIB_WEP_DEFAULT_KEYS        WepDefaultKeys[MAX_WEP_KEYS];
    UINT8                       default_key_idx;
    UINT8                       keyCfg;
    UINT8                       Reserved;
}
wep_key_data_t;

/* This structure is used in rom and existing fields should not be changed */
typedef struct
{
    UINT8                       key_idx;
    UINT8                       mickey[WAPI_MIC_LEN];
    UINT8                       rawkey[WAPI_KEY_LEN];
} wapi_key_detail_t;

/* cipher_key_t -> tkip_aes is much bigger than wapi_key_data_t and 
 * since wapi_key_data_t is not used by ROM it is ok to change this size. */

typedef struct
{
    wapi_key_detail_t           key;
    UINT8                       pn_inc;
    UINT8                       TxPN[WAPI_PN_LEN];
    UINT8                       RxPN[WAPI_PN_LEN];
    UINT8                      *pLastKey; //keep the orig cipher_key_t pointer
} wapi_key_data_t;
#endif

typedef PACK_START struct
{
    UINT8 ANonce[NONCE_SIZE];
    KeyData_t pwsKeyData;
} PACK_END eapolHskData_t;

/* This structure is used in rom and existing fields should not be changed */
typedef PACK_START struct cipher_key_t
{
#if 0
    cipher_key_hdr_t            hdr;
#endif
    union PACK_START ckd
    {
#if 0
         tkip_aes_key_data_t    tkip_aes;
         wep_key_data_t         wep;
         wapi_key_data_t        wapi;
#endif
         eapolHskData_t         hskData;
    } PACK_END ckd;
} PACK_END cipher_key_t;


#endif /* KEY_API_STA_TYPES_ROM_H__ end */
