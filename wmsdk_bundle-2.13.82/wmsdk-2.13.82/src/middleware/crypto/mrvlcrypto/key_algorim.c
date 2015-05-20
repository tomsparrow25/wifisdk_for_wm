/** @file  key_algorim.c 
  * @brief This file contains key derivation function for authentication key and
  *        key wrap key generation
  * 
  * Copyright (C) 2003-2008, Marvell International Ltd.
  * All Rights Reserved
  */

#include <stdio.h>
#include <string.h>

#include "encrypt.h"

/**
 * @brief Key derivation function to generate Authentication 
 *            Key and Key Wrap Key used in WPS registration protocol
 * @param Key		Input key to generate authentication key (KDK)
 * @param Key_size      Length of input key
 * @param TotalKeyLen   Length of output key
 * @param OutKey        Output key
 * @return 1 on success otherwise 0
 */
int
MrvKDF(u8 * Key, u32 Key_size, u32 TotalKeyLen, u8 * OutKey)
{
    u8 Mac[SHA256_DIGEST_SIZE];
    u32 MacSize = SHA256_DIGEST_SIZE;
    u8 Msg[100];
    u32 iterations =
        ((TotalKeyLen / 8) + SHA256_DIGEST_SIZE - 1) / SHA256_DIGEST_SIZE;
    u8 *pKeyAdd;
    u8 *pKeyEndAdd;
    u32 i;

    pKeyAdd = OutKey;
    pKeyEndAdd = pKeyAdd + TotalKeyLen / 8;

    for (i = 1; i < (iterations + 1); i++) {
        sprintf((char *) Msg,
                "%c%c%c%cWi-Fi Easy and Secure Key Derivation%c%c%c%c", 0x00,
                0x00, 0x00, i, 0x00, 0x00, TotalKeyLen / 0x100,
                TotalKeyLen % 0x100);

        MrvHMAC_SHA256(Key, Key_size, Msg, 44, Mac, MacSize);

        if ((pKeyAdd + MacSize) < pKeyEndAdd) {
            memcpy(pKeyAdd, Mac, MacSize);
            pKeyAdd += MacSize;
        } else {
            memcpy(pKeyAdd, Mac, (u32) (pKeyEndAdd - pKeyAdd));
            return 0;
        }

    }
    return 1;
}

/**
 * @brief  Find key length
 * @param a      Key
 * @param digits sizeof key
 * @return       key length
 */
unsigned int
N8_Digits(u8 * a, u32 digits)
{
    unsigned int i;

    for (i = 0; i < digits; i++) {
        if (a[i])
            break;
    }

    return (digits - i);
}
