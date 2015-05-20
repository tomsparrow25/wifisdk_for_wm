/* dsa.h
 *
 * Copyright (C) 2006-2013 wolfSSL Inc.  All rights reserved.
 *
 * This file is part of CyaSSL.
 *
 * Contact licensing@yassl.com with any questions or comments.
 *
 * http://www.yassl.com
 */



#ifndef NO_DSA

#ifndef CTAO_CRYPT_DSA_H
#define CTAO_CRYPT_DSA_H

#include <cyassl/ctaocrypt/types.h>
#include <cyassl/ctaocrypt/integer.h>
#include <cyassl/ctaocrypt/random.h>

#ifdef __cplusplus
    extern "C" {
#endif


enum {
    DSA_PUBLIC   = 0,
    DSA_PRIVATE  = 1
};

/* DSA */
typedef struct DsaKey {
    mp_int p, q, g, y, x;
    int type;                               /* public or private */
} DsaKey;


CYASSL_API void InitDsaKey(DsaKey* key);
CYASSL_API void FreeDsaKey(DsaKey* key);

CYASSL_API int DsaSign(const byte* digest, byte* out, DsaKey* key, RNG* rng);
CYASSL_API int DsaVerify(const byte* digest, const byte* sig, DsaKey* key,
                         int* answer);

CYASSL_API int DsaPublicKeyDecode(const byte* input, word32* inOutIdx, DsaKey*,
                                  word32);
CYASSL_API int DsaPrivateKeyDecode(const byte* input, word32* inOutIdx, DsaKey*,
                                   word32);

#ifdef __cplusplus
    } /* extern "C" */
#endif

#endif /* CTAO_CRYPT_DSA_H */
#endif /* NO_DSA */

