/* hmac.h
 *
 * Copyright (C) 2006-2013 wolfSSL Inc.  All rights reserved.
 *
 * This file is part of CyaSSL.
 *
 * Contact licensing@yassl.com with any questions or comments.
 *
 * http://www.yassl.com
 */



/*  hmac.h defines mini hamc openssl compatibility layer 
 *
 */


#ifndef CYASSL_HMAC_H_
#define CYASSL_HMAC_H_

#include <cyassl/ctaocrypt/settings.h>

#ifdef YASSL_PREFIX
#include "prefix_hmac.h"
#endif

#include <cyassl/openssl/evp.h>
#include <cyassl/ctaocrypt/hmac.h>

#ifdef __cplusplus
    extern "C" {
#endif


CYASSL_API unsigned char* CyaSSL_HMAC(const CYASSL_EVP_MD* evp_md,
                               const void* key, int key_len,
                               const unsigned char* d, int n, unsigned char* md,
                               unsigned int* md_len);


typedef struct CYASSL_HMAC_CTX {
    Hmac hmac;
    int  type;
} CYASSL_HMAC_CTX;


CYASSL_API void CyaSSL_HMAC_Init(CYASSL_HMAC_CTX* ctx, const void* key,
                                 int keylen, const EVP_MD* type);
CYASSL_API void CyaSSL_HMAC_Update(CYASSL_HMAC_CTX* ctx,
                                   const unsigned char* data, int len);
CYASSL_API void CyaSSL_HMAC_Final(CYASSL_HMAC_CTX* ctx, unsigned char* hash,
                                  unsigned int* len);
CYASSL_API void CyaSSL_HMAC_cleanup(CYASSL_HMAC_CTX* ctx);


typedef struct CYASSL_HMAC_CTX HMAC_CTX;

#define HMAC(a,b,c,d,e,f,g) CyaSSL_HMAC((a),(b),(c),(d),(e),(f),(g))

#define HMAC_Init    CyaSSL_HMAC_Init
#define HMAC_Update  CyaSSL_HMAC_Update
#define HMAC_Final   CyaSSL_HMAC_Final
#define HMAC_cleanup CyaSSL_HMAC_cleanup


#ifdef __cplusplus
    } /* extern "C" */
#endif


#endif /* CYASSL_HMAC_H_ */
