/* ripemd.h
 *
 * Copyright (C) 2006-2013 wolfSSL Inc.  All rights reserved.
 *
 * This file is part of CyaSSL.
 *
 * Contact licensing@yassl.com with any questions or comments.
 *
 * http://www.yassl.com
 */



#ifdef CYASSL_RIPEMD

#ifndef CTAO_CRYPT_RIPEMD_H
#define CTAO_CRYPT_RIPEME_H

#include <cyassl/ctaocrypt/types.h>

#ifdef __cplusplus
    extern "C" {
#endif


/* in bytes */
enum {
    RIPEMD             =  3,    /* hash type unique */
    RIPEMD_BLOCK_SIZE  = 64,
    RIPEMD_DIGEST_SIZE = 20,
    RIPEMD_PAD_SIZE    = 56
};


/* RipeMd 160 digest */
typedef struct RipeMd {
    word32  buffLen;   /* in bytes          */
    word32  loLen;     /* length in bytes   */
    word32  hiLen;     /* length in bytes   */
    word32  digest[RIPEMD_DIGEST_SIZE / sizeof(word32)];
    word32  buffer[RIPEMD_BLOCK_SIZE  / sizeof(word32)];
} RipeMd;


CYASSL_API void InitRipeMd(RipeMd*);
CYASSL_API void RipeMdUpdate(RipeMd*, const byte*, word32);
CYASSL_API void RipeMdFinal(RipeMd*, byte*);


#ifdef __cplusplus
    } /* extern "C" */
#endif

#endif /* CTAO_CRYPT_RIPEMD_H */
#endif /* CYASSL_RIPEMD */
