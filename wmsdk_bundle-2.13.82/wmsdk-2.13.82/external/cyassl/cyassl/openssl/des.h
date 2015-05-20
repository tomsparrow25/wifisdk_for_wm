/* des.h
 *
 * Copyright (C) 2006-2013 wolfSSL Inc.  All rights reserved.
 *
 * This file is part of CyaSSL.
 *
 * Contact licensing@yassl.com with any questions or comments.
 *
 * http://www.yassl.com
 */



/*  des.h defines mini des openssl compatibility layer 
 *
 */


#ifndef CYASSL_DES_H_
#define CYASSL_DES_H_

#include <cyassl/ctaocrypt/settings.h>

#ifdef YASSL_PREFIX
#include "prefix_des.h"
#endif


#ifdef __cplusplus
    extern "C" {
#endif

typedef unsigned char CYASSL_DES_cblock[8];
typedef /* const */ CYASSL_DES_cblock CYASSL_const_DES_cblock;
typedef CYASSL_DES_cblock CYASSL_DES_key_schedule;


enum {
    DES_ENCRYPT = 1,
    DES_DECRYPT = 0
};


CYASSL_API void CyaSSL_DES_set_key_unchecked(CYASSL_const_DES_cblock*,
                                             CYASSL_DES_key_schedule*);
CYASSL_API int  CyaSSL_DES_key_sched(CYASSL_const_DES_cblock* key,
                                     CYASSL_DES_key_schedule* schedule);
CYASSL_API void CyaSSL_DES_cbc_encrypt(const unsigned char* input,
                     unsigned char* output, long length,
                     CYASSL_DES_key_schedule* schedule, CYASSL_DES_cblock* ivec,
                     int enc);
CYASSL_API void CyaSSL_DES_ncbc_encrypt(const unsigned char* input,
                      unsigned char* output, long length,
                      CYASSL_DES_key_schedule* schedule,
                      CYASSL_DES_cblock* ivec, int enc);

CYASSL_API void CyaSSL_DES_set_odd_parity(CYASSL_DES_cblock*);
CYASSL_API void CyaSSL_DES_ecb_encrypt(CYASSL_DES_cblock*, CYASSL_DES_cblock*,
                                       CYASSL_DES_key_schedule*, int);


typedef CYASSL_DES_cblock DES_cblock;
typedef CYASSL_const_DES_cblock const_DES_cblock;
typedef CYASSL_DES_key_schedule DES_key_schedule;

#define DES_set_key_unchecked CyaSSL_DES_set_key_unchecked
#define DES_key_sched CyaSSL_DES_key_sched
#define DES_cbc_encrypt CyaSSL_DES_cbc_encrypt
#define DES_ncbc_encrypt CyaSSL_DES_ncbc_encrypt
#define DES_set_odd_parity CyaSSL_DES_set_odd_parity
#define DES_ecb_encrypt CyaSSL_DES_ecb_encrypt

#ifdef __cplusplus
    } /* extern "C" */
#endif


#endif /* CYASSL_DES_H_ */
