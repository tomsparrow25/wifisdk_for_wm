/* hc128.h
 *
 * Copyright (C) 2006-2013 wolfSSL Inc.  All rights reserved.
 *
 * This file is part of CyaSSL.
 *
 * Contact licensing@yassl.com with any questions or comments.
 *
 * http://www.yassl.com
 */



#ifndef NO_HC128

#ifndef CTAO_CRYPT_HC128_H
#define CTAO_CRYPT_HC128_H

#include <cyassl/ctaocrypt/types.h>

#ifdef __cplusplus
    extern "C" {
#endif


enum {
	HC128_ENC_TYPE    =  6    /* cipher unique type */
};

/* HC-128 stream cipher */
typedef struct HC128 {
    word32 T[1024];             /* P[i] = T[i];  Q[i] = T[1024 + i ]; */
    word32 X[16];
    word32 Y[16];
    word32 counter1024;         /* counter1024 = i mod 1024 at the ith step */
    word32 key[8];
    word32 iv[8];
} HC128;


CYASSL_API int Hc128_Process(HC128*, byte*, const byte*, word32);
CYASSL_API int Hc128_SetKey(HC128*, const byte* key, const byte* iv);


#ifdef __cplusplus
    } /* extern "C" */
#endif

#endif /* CTAO_CRYPT_HC128_H */

#endif /* HAVE_HC128 */
