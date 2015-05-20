/* des3.h
 *
 * Copyright (C) 2006-2013 wolfSSL Inc.  All rights reserved.
 *
 * This file is part of CyaSSL.
 *
 * Contact licensing@yassl.com with any questions or comments.
 *
 * http://www.yassl.com
 */



#ifndef NO_DES3

#ifndef CTAO_CRYPT_DES3_H
#define CTAO_CRYPT_DES3_H


#include <cyassl/ctaocrypt/types.h>


#ifdef __cplusplus
    extern "C" {
#endif

#define CYASSL_3DES_CAVIUM_MAGIC 0xBEEF0003

enum {
    DES_ENC_TYPE    = 2,     /* cipher unique type */
    DES3_ENC_TYPE   = 3,     /* cipher unique type */
    DES_BLOCK_SIZE  = 8,
    DES_KS_SIZE     = 32,

    DES_ENCRYPTION  = 0,
    DES_DECRYPTION  = 1
};

#ifdef STM32F2_CRYPTO
enum {
    DES_CBC = 0,
    DES_ECB = 1
};
#endif


/* DES encryption and decryption */
typedef struct Des {
    word32 key[DES_KS_SIZE];
    word32 reg[DES_BLOCK_SIZE / sizeof(word32)];      /* for CBC mode */
    word32 tmp[DES_BLOCK_SIZE / sizeof(word32)];      /* same         */
} Des;


/* DES3 encryption and decryption */
typedef struct Des3 {
    word32 key[3][DES_KS_SIZE];
    word32 reg[DES_BLOCK_SIZE / sizeof(word32)];      /* for CBC mode */
    word32 tmp[DES_BLOCK_SIZE / sizeof(word32)];      /* same         */
#ifdef HAVE_CAVIUM
    int     devId;           /* nitrox device id */
    word32  magic;           /* using cavium magic */
    word64  contextHandle;   /* nitrox context memory handle */
#endif
} Des3;


CYASSL_API void Des_SetKey(Des* des, const byte* key, const byte* iv, int dir);
CYASSL_API void Des_SetIV(Des* des, const byte* iv);
CYASSL_API void Des_CbcEncrypt(Des* des, byte* out, const byte* in, word32 sz);
CYASSL_API void Des_CbcDecrypt(Des* des, byte* out, const byte* in, word32 sz);
CYASSL_API void Des_EcbEncrypt(Des* des, byte* out, const byte* in, word32 sz);

CYASSL_API void Des3_SetKey(Des3* des, const byte* key, const byte* iv,int dir);
CYASSL_API void Des3_SetIV(Des3* des, const byte* iv);
CYASSL_API void Des3_CbcEncrypt(Des3* des, byte* out, const byte* in,word32 sz);
CYASSL_API void Des3_CbcDecrypt(Des3* des, byte* out, const byte* in,word32 sz);


#ifdef HAVE_CAVIUM
    CYASSL_API int  Des3_InitCavium(Des3*, int);
    CYASSL_API void Des3_FreeCavium(Des3*);
#endif


#ifdef __cplusplus
    } /* extern "C" */
#endif

#endif /* NO_DES3 */
#endif /* CTAO_CRYPT_DES3_H */

