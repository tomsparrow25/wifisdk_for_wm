/* md5.h
 *
 * Copyright (C) 2006-2013 wolfSSL Inc.  All rights reserved.
 *
 * This file is part of CyaSSL.
 *
 * Contact licensing@yassl.com with any questions or comments.
 *
 * http://www.yassl.com
 */


#ifndef NO_MD5

#ifndef CTAO_CRYPT_MD5_H
#define CTAO_CRYPT_MD5_H

#include <cyassl/ctaocrypt/types.h>

#ifdef __cplusplus
    extern "C" {
#endif


/* in bytes */
enum {
#ifdef STM32F2_HASH
    MD5_REG_SIZE    =  4,      /* STM32 register size, bytes */
#endif
    MD5             =  0,      /* hash type unique */
    MD5_BLOCK_SIZE  = 64,
    MD5_DIGEST_SIZE = 16,
    MD5_PAD_SIZE    = 56
};


/* MD5 digest */
typedef struct Md5 {
    word32  buffLen;   /* in bytes          */
    word32  loLen;     /* length in bytes   */
    word32  hiLen;     /* length in bytes   */
    word32  digest[MD5_DIGEST_SIZE / sizeof(word32)];
    word32  buffer[MD5_BLOCK_SIZE  / sizeof(word32)];
} Md5;


CYASSL_API void InitMd5(Md5*);
CYASSL_API void Md5Update(Md5*, const byte*, word32);
CYASSL_API void Md5Final(Md5*, byte*);


#ifdef __cplusplus
    } /* extern "C" */
#endif

#endif /* CTAO_CRYPT_MD5_H */
#endif /* NO_MD5 */
