/** @file crypto_mrvl.c
 *
 *  @brief This file contains Marvell encrypt wrap function
 *
 *  Copyright 2008-2013, Marvell International Ltd.
 *  All Rights Reserved
 */

#include <wmstdio.h>
#include <stdio.h>
#include <stdlib.h>
#include <wps.h>
#include <wmcrypto.h>

#include "mrvlcrypto/sha1.h"
#include "mrvlcrypto/md5.h"
#include "mrvlcrypto/crypto.h"
#include "mrvlcrypto/encrypt.h"

void *mrvl_dh_setup_key(u8 *public_key, u32 public_len,
			u8 *private_key, u32 private_len,
			DH_PG_PARAMS *dh_params)
{
	MrvDH_Setup(public_key,
                    public_len,
                    private_key,
                    private_len,
                    dh_params);

	return NULL;
}

int mrvl_dh_compute_key(void *dh, u8 *shared_key, u32 shared_len,
			u8 *public_key, u32 public_len,
			u8 *private_key, u32 private_len,
			DH_PG_PARAMS *dh_params)
{
	return MrvDH_Compute(shared_key, public_key, public_len,
                  private_key, private_len, dh_params);
}

void mrvl_dh_free(void *dh_context)
{
}

u32 mrvl_sha1_vector(size_t nmsg, const u8 *msg[], const size_t msglen[],
			u8 *mac, size_t maclen)
{
	sha1_vector(nmsg, msg, msglen, mac);
	return 0;
}

u32 mrvl_sha256_vector(size_t nmsg, const u8 *msg[], const size_t msglen[],
			u8 *mac, size_t maclen)
{
	Mrv_SHA256(*msg, msglen[0], mac);
	return maclen;
}

u32 mrvl_hmac_sha256(const u8 *key, u32 keylen, u8 *msg, u32 msglen,
		     u8 *mac, u32 maclen)
{
	MrvHMAC_SHA256(key, keylen, msg, msglen, mac, maclen);
	return maclen;
}

/**
 *  * @brief Key derivation function to generate Authentication
 *            Key and Key Wrap Key used in WPS registration protocol
 * @param key		Input key to generate authentication key (KDK)
 * @param keylen        Length of input key
 * @param result        result buffer
 * @param result_len    Length of result buffer
 * @return 0 on success otherwise 1
 */
int mrvl_kdf(u8 *key, u32 key_len, u8 *result, u32 result_len)
{
	MrvKDF(key, key_len, result_len * 8, result);
	return 0;
}

/**********************************************************
 *  ***   AES Key Wrap Key
 *   **********************************************************/
/**
 *  @brief  Wrap keys with AES Key Wrap Algorithm (128-bit KEK)
 *
 *  @param pPlainTxt    Plaintext key to be wrapped
 *  @param TextLen      Length of the plain key in bytes (16 bytes)
 *  @param pCipTxt      Wrapped key
 *  @param pKEK         Key encryption key (KEK)
 *  @param KeyLen       Length of KEK in bytes (must be divisible by 16)
 *  @param IV           Encryption IV for CBC mode (16 bytes)
 *  @return             0 on success, -1 on failure
 */
int mrvl_aes_wrap(u8 *pPlainTxt, u32 TextLen, u8 *pCipTxt, u8 *pKEK,
		  u32 KeyLen, u8 *IV)
{
	MrvAES_Wrap(pPlainTxt, TextLen, pCipTxt, pKEK, KeyLen, IV);
	return 0;
}

/**
 *  @brief  Unwrap key with AES Key Wrap Algorithm (128-bit KEK)
 *
 *  @param pCipTxt      Wrapped key to be unwrapped
 *  @param TextLen      Length of the wrapped key in bytes (16 bytes)
 *  @param pPlainTxt    Plaintext key
 *  @param pKEK         Key encryption key (KEK)
 *  @param KeyLen       Length of KEK in bytes (must be divisible by 16)
 *  @param IV           Encryption IV for CBC mode (16 bytes)
 *  @return             0 on success, -1 on failure
 */
int mrvl_aes_unwrap(u8 *pCipTxt, u32 TextLen, u8 *pPlainTxt, u8 *pKEK,
		    u32 KeyLen, u8 *IV)
{
	MrvAES_UnWrap(pCipTxt, TextLen, pPlainTxt, pKEK, KeyLen, IV);
	return 0;
}

int mrvl_aes_wrap_ext(u8 *pPlainTxt, u32 TextLen, u8 *pCipTxt,
		      u8 *pKEK, u32 KeyLen, u8 *IV)
{
	return MrvAES_Wrap_Ext(pPlainTxt, TextLen, pCipTxt,
        pKEK, KeyLen, IV);
}

int mrvl_aes_unwrap_ext(u8 *pCipTxt, u32 TextLen, u8 *pPlainTxt, u8 *pKEK,
			u32 KeyLen, u8 *IV)
{
	return MrvAES_UnWrap_Ext(pCipTxt, TextLen, pPlainTxt,
	pKEK, KeyLen, IV);
}

int mrvl_sha1_prf(const u8 *key, size_t key_len, const char *label,
	     const u8 *data, size_t data_len, u8 *buf, size_t buf_len)
{
	sha1_prf(key, key_len, label, data, data_len, buf, buf_len);
	return 0;
}

void mrvl_hmac_md5(const u8 *key, size_t key_len, const u8 *data,
		size_t data_len, u8 *mac, size_t maclen)
{
	hmac_md5(key, key_len, data, data_len, mac);
}
