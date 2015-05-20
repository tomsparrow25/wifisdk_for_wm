/* arc4.h
 *
 * Copyright (C) 2006-2013 wolfSSL Inc.  All rights reserved.
 *
 * This file is part of CyaSSL.
 *
 * Contact licensing@yassl.com with any questions or comments.
 *
 * http://www.yassl.com
 */



#ifndef CTAO_CRYPT_ARC4_H
#define CTAO_CRYPT_ARC4_H


#include <cyassl/ctaocrypt/types.h>


#ifdef __cplusplus
    extern "C" {
#endif


#define CYASSL_ARC4_CAVIUM_MAGIC 0xBEEF0001

enum {
	ARC4_ENC_TYPE   = 4,    /* cipher unique type */
    ARC4_STATE_SIZE = 256
};

/* ARC4 encryption and decryption */
typedef struct Arc4 {
    byte x;
    byte y;
    byte state[ARC4_STATE_SIZE];
#ifdef HAVE_CAVIUM
    int    devId;           /* nitrox device id */
    word32 magic;           /* using cavium magic */
    word64 contextHandle;   /* nitrox context memory handle */
#endif
} Arc4;

CYASSL_API void Arc4Process(Arc4*, byte*, const byte*, word32);
CYASSL_API void Arc4SetKey(Arc4*, const byte*, word32);

#ifdef HAVE_CAVIUM
    CYASSL_API int  Arc4InitCavium(Arc4*, int);
    CYASSL_API void Arc4FreeCavium(Arc4*);
#endif

#ifdef __cplusplus
    } /* extern "C" */
#endif


#endif /* CTAO_CRYPT_ARC4_H */

