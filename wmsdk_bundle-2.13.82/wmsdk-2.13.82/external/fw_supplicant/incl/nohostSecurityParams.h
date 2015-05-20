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
#ifndef _NO_HOST_SECURITY_PARAMS_H_
#define _NO_HOST_SECURITY_PARAMS_H_

#include "wltypes.h"

typedef struct
{
    UINT8 CipherType;           //(0/1) (WPA/WPA2)
    UINT8 MulticastCipher;      //(0/1) (TKIP/AES)
    UINT8 UnicastCipher;        //(0/1) (TKIP/AES)
    char PSKPassPhrase[64];

} NoHostSecurityParams_t;

#if 0
typedef PACK_START enum
{
    TKIP = 2,
    AES = 4,
    TKIP_AES = 6
} PACK_END Encr_Type_e;
#endif

/* No Host Specific API calls */
extern void set_psk(char *phrase);
void set_pmk(char* pSsid, UINT8 ssidLen, char* pPMK);
extern void set_security_params(SecurityParams_t * currParams);
extern SecurityParams_t *get_security_params();

/* Commands to retrieve old structure */
extern void set_nohost_security_params(NoHostSecurityParams_t * currParams);
extern NoHostSecurityParams_t *get_nohost_security_params();
extern customMIB_RSNConfig_t *getMibRsnConfig();

#endif
