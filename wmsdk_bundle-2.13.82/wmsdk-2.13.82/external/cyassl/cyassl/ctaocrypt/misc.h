/* misc.h
 *
 * Copyright (C) 2006-2013 wolfSSL Inc.  All rights reserved.
 *
 * This file is part of CyaSSL.
 *
 * Contact licensing@yassl.com with any questions or comments.
 *
 * http://www.yassl.com
 */



#ifndef CTAO_CRYPT_MISC_H
#define CTAO_CRYPT_MISC_H


#include <cyassl/ctaocrypt/types.h>


#ifdef __cplusplus
    extern "C" {
#endif


#ifdef NO_INLINE
CYASSL_LOCAL
word32 rotlFixed(word32, word32);
CYASSL_LOCAL
word32 rotrFixed(word32, word32);

CYASSL_LOCAL
word32 ByteReverseWord32(word32);
CYASSL_LOCAL
void   ByteReverseWords(word32*, const word32*, word32);
CYASSL_LOCAL
void   ByteReverseBytes(byte*, const byte*, word32);

CYASSL_LOCAL
void XorWords(word*, const word*, word32);
CYASSL_LOCAL
void xorbuf(byte*, const byte*, word32);

#ifdef WORD64_AVAILABLE
CYASSL_LOCAL
word64 rotlFixed64(word64, word64);
CYASSL_LOCAL
word64 rotrFixed64(word64, word64);

CYASSL_LOCAL
word64 ByteReverseWord64(word64);
CYASSL_LOCAL
void   ByteReverseWords64(word64*, const word64*, word32);
#endif /* WORD64_AVAILABLE */

#endif /* NO_INLINE */


#ifdef __cplusplus
    }   /* extern "C" */
#endif


#endif /* CTAO_CRYPT_MISC_H */

