/* pwdbased.h
 *
 * Copyright (C) 2006-2013 wolfSSL Inc.  All rights reserved.
 *
 * This file is part of CyaSSL.
 *
 * Contact licensing@yassl.com with any questions or comments.
 *
 * http://www.yassl.com
 */



#ifndef NO_PWDBASED

#ifndef CTAO_CRYPT_PWDBASED_H
#define CTAO_CRYPT_PWDBASED_H

#include <cyassl/ctaocrypt/types.h>
#include <cyassl/ctaocrypt/md5.h>       /* for hash type */
#include <cyassl/ctaocrypt/sha.h>

#ifdef __cplusplus
    extern "C" {
#endif


CYASSL_API int PBKDF1(byte* output, const byte* passwd, int pLen,
                      const byte* salt, int sLen, int iterations, int kLen,
                      int hashType);
CYASSL_API int PBKDF2(byte* output, const byte* passwd, int pLen,
                      const byte* salt, int sLen, int iterations, int kLen,
                      int hashType);
CYASSL_API int PKCS12_PBKDF(byte* output, const byte* passwd, int pLen,
                            const byte* salt, int sLen, int iterations,
                            int kLen, int hashType, int purpose);


#ifdef __cplusplus
    } /* extern "C" */
#endif

#endif /* CTAO_CRYPT_PWDBASED_H */
#endif /* NO_PWDBASED */
