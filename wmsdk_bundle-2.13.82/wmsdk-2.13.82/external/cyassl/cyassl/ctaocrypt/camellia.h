/* camellia.h ver 1.2.0
 *
 * Copyright (C) 2006-2013 wolfSSL Inc.  All rights reserved.
 *
 * This file is part of CyaSSL.
 *
 * Contact licensing@yassl.com with any questions or comments.
 *
 * http://www.yassl.com
 */


/* camellia.h
 *
 * Copyright (C) 2006-2013 wolfSSL Inc.  All rights reserved.
 *
 * This file is part of CyaSSL.
 *
 * Contact licensing@yassl.com with any questions or comments.
 *
 * http://www.yassl.com
 */


#ifdef HAVE_CAMELLIA

#ifndef CTAO_CRYPT_CAMELLIA_H
#define CTAO_CRYPT_CAMELLIA_H


#include <cyassl/ctaocrypt/types.h>

#ifdef __cplusplus
    extern "C" {
#endif


enum {
    CAMELLIA_BLOCK_SIZE = 16
};

#define CAMELLIA_TABLE_BYTE_LEN 272
#define CAMELLIA_TABLE_WORD_LEN (CAMELLIA_TABLE_BYTE_LEN / sizeof(word32))

typedef word32 KEY_TABLE_TYPE[CAMELLIA_TABLE_WORD_LEN];

typedef struct Camellia {
    word32 keySz;
    KEY_TABLE_TYPE key;
    word32 reg[CAMELLIA_BLOCK_SIZE / sizeof(word32)]; /* for CBC mode */
    word32 tmp[CAMELLIA_BLOCK_SIZE / sizeof(word32)]; /* for CBC mode */
} Camellia;


CYASSL_API int  CamelliaSetKey(Camellia* cam,
                                   const byte* key, word32 len, const byte* iv);
CYASSL_API int  CamelliaSetIV(Camellia* cam, const byte* iv);
CYASSL_API void CamelliaEncryptDirect(Camellia* cam, byte* out, const byte* in);
CYASSL_API void CamelliaDecryptDirect(Camellia* cam, byte* out, const byte* in);
CYASSL_API void CamelliaCbcEncrypt(Camellia* cam,
                                          byte* out, const byte* in, word32 sz);
CYASSL_API void CamelliaCbcDecrypt(Camellia* cam,
                                          byte* out, const byte* in, word32 sz);


#ifdef __cplusplus
    } /* extern "C" */
#endif

#endif /* CTAO_CRYPT_AES_H */
#endif /* HAVE_CAMELLIA */

