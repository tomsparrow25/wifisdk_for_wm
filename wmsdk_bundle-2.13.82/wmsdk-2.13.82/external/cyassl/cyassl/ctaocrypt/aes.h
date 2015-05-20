/* aes.h
 *
 * Copyright (C) 2006-2013 wolfSSL Inc.  All rights reserved.
 *
 * This file is part of CyaSSL.
 *
 * Contact licensing@yassl.com with any questions or comments.
 *
 * http://www.yassl.com
 */



#ifndef NO_AES

#ifndef CTAO_CRYPT_AES_H
#define CTAO_CRYPT_AES_H

/*
  wmsdk: Enable AES counter mode
*/
#define CYASSL_AES_COUNTER


#include <cyassl/ctaocrypt/types.h>

#ifdef HAVE_CAVIUM
    #include <cyassl/ctaocrypt/logging.h>
    #include "cavium_common.h"
#endif

#ifdef CYASSL_AESNI

#include <wmmintrin.h>

#if !defined (ALIGN16)
    #if defined (__GNUC__)
        #define ALIGN16 __attribute__ ( (aligned (16)))
    #elif defined(_MSC_VER)
        #define ALIGN16 __declspec (align (16))
    #else
        #define ALIGN16
    #endif
#endif

#endif /* CYASSL_AESNI */

#if !defined (ALIGN16)
    #define ALIGN16
#endif

#ifdef __cplusplus
    extern "C" {
#endif


#define CYASSL_AES_CAVIUM_MAGIC 0xBEEF0002
#ifndef _AES_ENUM_DEFINED_
enum {
    AES_ENC_TYPE   = 1,   /* cipher unique type */
    AES_ENCRYPTION = 0,
    AES_DECRYPTION = 1,
    AES_BLOCK_SIZE = 16
};

#define _AES_ENUM_DEFINED_
#endif /* _AES_ENUM_DEFINED_ */

#ifdef MC200_AES_HW_ACCL
typedef enum {
  HW_AES_MODE_ECB    = 0,	                /*!< AES mode: ECB */
  HW_AES_MODE_CBC    = 1,                  /*!< AES mode: CBC */
  HW_AES_MODE_CTR    = 2,                  /*!< AES mode: CTR */
  HW_AES_MODE_CCM    = 5,                  /*!< AES mode: CCM */ 
  HW_AES_MODE_MMO    = 6,                  /*!< AES mode: MMO */
  HW_AES_MODE_BYPASS = 7,                  /*!< AES mode: Bypass */
}hw_aes_mode_t;
#endif /* MC200_AES_HW_ACCL */

typedef struct Aes {
#ifdef MC200_AES_HW_ACCL
	/* 
	 * The following parameters will be cached in the call to *setkey*.
	 * Ensure that this remains in sync with AES_Config_Type. Only
	 * required members are cached. Add if necessary in future.
	 */ 
	hw_aes_mode_t mode;
	/* of the type AES_EncDecSel_Type. Use directly*/
	int dir;
	/* Already processed as required by h/w */
	word32 initVect[4];
	word32 keySize;
	/* Already processed as required by h/w */
	word32 saved_key[8];
	int micLen;
	int micEn;

	/* For ctr mode */
	unsigned ctr_mod; /* counter modular */

	/* for CCM mode */
	unsigned aStrLen; /* associated auth. data len */
#else /* ! MC200_AES_HW_ACCL */
    /* AESNI needs key first, rounds 2nd, not sure why yet */
    ALIGN16 word32 key[60];
    word32  rounds;

    ALIGN16 word32 reg[AES_BLOCK_SIZE / sizeof(word32)];      /* for CBC mode */
    ALIGN16 word32 tmp[AES_BLOCK_SIZE / sizeof(word32)];      /* same         */

#ifdef HAVE_AESGCM
    ALIGN16 byte H[AES_BLOCK_SIZE];
#ifdef GCM_TABLE
    /* key-based fast multiplication table. */
    ALIGN16 byte M0[256][AES_BLOCK_SIZE];
#endif /* GCM_TABLE */
#endif /* HAVE_AESGCM */
#ifdef CYASSL_AESNI
    byte use_aesni;
#endif /* CYASSL_AESNI */
#ifdef HAVE_CAVIUM
    AesType type;            /* aes key type */
    int     devId;           /* nitrox device id */
    word32  magic;           /* using cavium magic */
    word64  contextHandle;   /* nitrox context memory handle */
#endif
#endif /* MC200_AES_HW_ACCL */
} Aes;


CYASSL_API int  AesSetKey(Aes* aes, const byte* key, word32 len, const byte* iv,
                          int dir);
CYASSL_API int  AesSetIV(Aes* aes, const byte* iv);
CYASSL_API int  AesCbcEncrypt(Aes* aes, byte* out, const byte* in, word32 sz);
CYASSL_API int  AesCbcDecrypt(Aes* aes, byte* out, const byte* in, word32 sz);
CYASSL_API void AesCtrEncrypt(Aes* aes, byte* out, const byte* in, word32 sz);
CYASSL_API void AesEncryptDirect(Aes* aes, byte* out, const byte* in);
CYASSL_API void AesDecryptDirect(Aes* aes, byte* out, const byte* in);
CYASSL_API int  AesSetKeyDirect(Aes* aes, const byte* key, word32 len,
                                const byte* iv, int dir);
#ifdef HAVE_AESGCM
CYASSL_API void AesGcmSetKey(Aes* aes, const byte* key, word32 len);
CYASSL_API void AesGcmEncrypt(Aes* aes, byte* out, const byte* in, word32 sz,
                              const byte* iv, word32 ivSz,
                              byte* authTag, word32 authTagSz,
                              const byte* authIn, word32 authInSz);
CYASSL_API int  AesGcmDecrypt(Aes* aes, byte* out, const byte* in, word32 sz,
                              const byte* iv, word32 ivSz,
                              const byte* authTag, word32 authTagSz,
                              const byte* authIn, word32 authInSz);
#endif /* HAVE_AESGCM */
#ifdef HAVE_AESCCM
CYASSL_API void AesCcmSetKey(Aes* aes, const byte* key, word32 keySz);
CYASSL_API void AesCcmEncrypt(Aes* aes, byte* out, const byte* in, word32 inSz,
                              const byte* nonce, word32 nonceSz,
                              byte* authTag, word32 authTagSz,
                              const byte* authIn, word32 authInSz);
CYASSL_API int  AesCcmDecrypt(Aes* aes, byte* out, const byte* in, word32 inSz,
                              const byte* nonce, word32 nonceSz,
                              const byte* authTag, word32 authTagSz,
                              const byte* authIn, word32 authInSz);
#endif /* HAVE_AESCCM */

#ifdef HAVE_CAVIUM
    CYASSL_API int  AesInitCavium(Aes*, int);
    CYASSL_API void AesFreeCavium(Aes*);
#endif

#ifdef __cplusplus
    } /* extern "C" */
#endif


#endif /* CTAO_CRYPT_AES_H */
#endif /* NO_AES */

