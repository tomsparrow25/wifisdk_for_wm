/* dh.h
 *
 * Copyright (C) 2006-2013 wolfSSL Inc.  All rights reserved.
 *
 * This file is part of CyaSSL.
 *
 * Contact licensing@yassl.com with any questions or comments.
 *
 * http://www.yassl.com
 */



#ifndef NO_DH

#ifndef CTAO_CRYPT_DH_H
#define CTAO_CRYPT_DH_H

#include <cyassl/ctaocrypt/types.h>
#include <cyassl/ctaocrypt/integer.h>
#include <cyassl/ctaocrypt/random.h>

#ifdef __cplusplus
    extern "C" {
#endif


/* Diffie-Hellman Key */
typedef struct DhKey {
    mp_int p, g;                            /* group parameters  */
} DhKey;


CYASSL_API void InitDhKey(DhKey* key);
CYASSL_API void FreeDhKey(DhKey* key);

CYASSL_API int DhGenerateKeyPair(DhKey* key, RNG* rng, byte* priv,
                                 word32* privSz, byte* pub, word32* pubSz);
CYASSL_API int DhAgree(DhKey* key, byte* agree, word32* agreeSz,
                       const byte* priv, word32 privSz, const byte* otherPub,
                       word32 pubSz);

CYASSL_API int DhKeyDecode(const byte* input, word32* inOutIdx, DhKey* key,
                           word32);
CYASSL_API int DhSetKey(DhKey* key, const byte* p, word32 pSz, const byte* g,
                        word32 gSz);
CYASSL_API int DhParamsLoad(const byte* input, word32 inSz, byte* p,
                            word32* pInOutSz, byte* g, word32* gInOutSz);


#ifdef __cplusplus
    } /* extern "C" */
#endif

#endif /* CTAO_CRYPT_DH_H */

#endif /* NO_DH */

