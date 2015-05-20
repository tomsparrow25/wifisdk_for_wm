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
#ifndef _KEYMGMT_COMMON_H_
#define _KEYMGMT_COMMON_H_

#include "wltypes.h"
#include "IEEE_types.h"

#define NONCE_SIZE            32
#define EAPOL_MIC_KEY_SIZE    16
#define EAPOL_MIC_SIZE        16
#define EAPOL_ENCR_KEY_SIZE   16
#define MAC_ADDR_SIZE         6
#define TK_SIZE               16
#define HDR_8021x_LEN         4
#define KEYMGMTTIMEOUTVAL     10
#define TDLS_MIC_KEY_SIZE     16

#define EAPOL_PROTOCOL_V1 1
#define EAPOL_PROTOCOL_V2 2

typedef PACK_START struct
{
    UINT8                         protocol_ver;
    IEEEtypes_8021x_PacketType_e  pckt_type;
    UINT16                        pckt_body_len;
}
PACK_END Hdr_8021x_t;

typedef PACK_START struct
{
    /* don't change this order.  It is set to match the 
    ** endianness of the message
    */

    /* Byte 1 */
    UINT16     KeyMIC               : 1;  /* Bit  8     */
    UINT16     Secure               : 1;  /* Bit  9     */
    UINT16     Error                : 1;  /* Bit  10    */
    UINT16     Request              : 1;  /* Bit  11    */
    UINT16     EncryptedKeyData     : 1;  /* Bit  12    */
    UINT16     Reserved             : 3;  /* Bits 13-15 */

    /* Byte 0 */
    UINT16     KeyDescriptorVersion : 3;  /* Bits 0-2   */
    UINT16     KeyType              : 1;  /* Bit  3     */
    UINT16     KeyIndex             : 2;  /* Bits 4-5   */
    UINT16     Install              : 1;  /* Bit  6     */
    UINT16     KeyAck               : 1;  /* Bit  7     */

} PACK_END key_info_t;

#define KEY_DESCRIPTOR_HMAC_MD5_RC4  (1U << 0)
#define KEY_DESCRIPTOR_HMAC_SHA1_AES (1U << 1)

/* WPA2 GTK IE */
typedef PACK_START struct
{
    UINT8 KeyID:2;
    UINT8 Tx   :1;
    UINT8 rsvd :5;
    UINT8 rsvd1;
    UINT8 GTK[1];
}
PACK_END GTK_KDE_t;

/* WPA2 Key Data */
typedef PACK_START struct
{
    UINT8 type;
    UINT8 length;
    UINT8 OUI[3];
    UINT8 dataType;
    UINT8 data[1];
}
PACK_END KDE_t;

typedef PACK_START struct
{
    uint8   llc[3];
    uint8   snap_oui[3];
    uint16  snap_type;
}
PACK_END llc_snap_t;

typedef PACK_START struct
{
    Hdr_8021x_t hdr_8021x;
    UINT8       desc_type;
    key_info_t  key_info;
    UINT16      key_length;
    UINT32      replay_cnt[2];
    UINT8       key_nonce[NONCE_SIZE]; /*32 bytes */
    UINT8       EAPOL_key_IV[16];
    UINT8       key_RSC[8];
    UINT8       key_ID[8];
    UINT8       key_MIC[EAPOL_MIC_KEY_SIZE];
    UINT16      key_material_len;
    UINT8       key_data[1];
}
PACK_END EAPOL_KeyMsg_t;


typedef PACK_START struct
{
    Hdr_8021x_t                 hdr_8021x;
    IEEEtypes_8021x_CodeType_e  code;
    UINT8                       identifier;
    UINT16                      length;
    UINT8                       data[1];
}
PACK_END EAP_PacketMsg_t;


typedef PACK_START struct
{
    ether_hdr_t    ethHdr;
    EAPOL_KeyMsg_t keyMsg;
}
PACK_END EAPOL_KeyMsg_Tx_t;

#endif
