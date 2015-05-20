/* md4.h
 *
 * Copyright (C) 2006-2013 wolfSSL Inc.  All rights reserved.
 *
 * This file is part of CyaSSL.
 *
 * Contact licensing@yassl.com with any questions or comments.
 *
 * http://www.yassl.com
 */



#ifndef NO_MD4

#ifndef CTAO_CRYPT_MD4_H
#define CTAO_CRYPT_MD4_H

#include <cyassl/ctaocrypt/types.h>

#ifdef __cplusplus
    extern "C" {
#endif


/* in bytes */
enum {
    MD4_BLOCK_SIZE  = 64,
    MD4_DIGEST_SIZE = 16,
    MD4_PAD_SIZE    = 56
};


/* MD4 digest */
typedef struct Md4 {
    word32  buffLen;   /* in bytes          */
    word32  loLen;     /* length in bytes   */
    word32  hiLen;     /* length in bytes   */
    word32  digest[MD4_DIGEST_SIZE / sizeof(word32)];
    word32  buffer[MD4_BLOCK_SIZE  / sizeof(word32)];
} Md4;


CYASSL_API void InitMd4(Md4*);
CYASSL_API void Md4Update(Md4*, const byte*, word32);
CYASSL_API void Md4Final(Md4*, byte*);


#ifdef __cplusplus
    } /* extern "C" */
#endif

#endif /* CTAO_CRYPT_MD4_H */

#endif /* NO_MD4 */

