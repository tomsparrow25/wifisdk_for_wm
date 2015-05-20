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
#ifndef _CUSTOMAPP_MIB_H_
#define _CUSTOMAPP_MIB_H_
#include "IEEE_types.h"
#include "wltypes.h"
#include "keyMgmtStaTypes.h"

typedef struct
{
    /* This structure is ROM'd */

    UINT8 RSNEnabled           : 1;     /* WPA, WPA2 */
    UINT8 pmkidValid           : 1;     /* PMKID valid */
    UINT8 rsnCapValid          : 1;
    UINT8 grpMgmtCipherValid   : 1;
    UINT8 rsvd                 : 4;     /* rsvd */

    SecurityMode_t  wpaType;
    Cipher_t        mcstCipher;
    Cipher_t        ucstCipher;
    AkmSuite_t      AKM;
    UINT8           PMKID[16];

    IEEEtypes_RSNCapability_t  rsnCap;

    Cipher_t        grpMgmtCipher;

} RSNConfig_t;

typedef struct
{
    UINT8  RSNEnabled;     /* WPA, WPA2*/
    UINT8  MulticastCipher[4];
    UINT8  UnicastCipher[4];
    UINT8  AuthSuites[4];
    UINT8  PSKValue[40];      
    char   PSKPassPhrase[64]; 
} customMIB_RSNConfig_t;

#endif/* _CUSTOMAPP_MIB_H_ */  

