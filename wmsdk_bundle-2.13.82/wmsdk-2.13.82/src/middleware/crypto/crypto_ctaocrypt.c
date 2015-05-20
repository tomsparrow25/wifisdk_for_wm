/** @file crypto.c
 *
 *  @brief This file contains CYASSL wrap function
 *
 *  Copyright 2008-2013, Marvell International Ltd.
 *  All Rights Reserved
 */

#include <dh.h>
#include <md5.h>
#include <sha.h>
#include <sha256.h>
#include <hmac.h>

#include <wmstdio.h>
#include <stdio.h>
#include <stdlib.h>
#include <wmcrypto.h>
#include <mdev_aes.h>
#include "wmcrypto_mem.h"

#define BLOCK_SIZE 16

void *mrvl_dh_setup_key(u8 *public_key, u32 public_len,
			u8 *private_key, u32 private_len,
			DH_PG_PARAMS *dh_params)
{
	DhKey *dh = NULL;
	RNG rng;
	u8 *priv_key = NULL;
	u8 *pub_key = NULL;
	u32 privkey_len;
	u32 publkey_len;
	u8 ret;

	dh = crypto_mem_malloc(sizeof(DhKey));

	if (dh == NULL)
		return NULL;

	InitDhKey(dh);

	ret = DhSetKey(dh, dh_params->prime, dh_params->primeLen,
			dh_params->generator, dh_params->generatorLen);

	if (ret != 0)
		goto error;

	ret = InitRng(&rng);

	if (ret != 0)
		goto error;

	pub_key = crypto_mem_malloc(public_len);
	if (pub_key == NULL) {
		crypto_mem_free(pub_key);
		goto error;
	}

	priv_key = crypto_mem_malloc(private_len);
	if (priv_key == NULL) {
		crypto_mem_free(priv_key);
		goto error;
	}

	ret = DhGenerateKeyPair(dh, &rng, priv_key, &privkey_len, pub_key,
				&publkey_len);

	if (ret != 0)
		goto error;

	memcpy(public_key, pub_key, publkey_len);
	memcpy(private_key, priv_key, privkey_len);

	crypto_mem_free(pub_key);
	crypto_mem_free(priv_key);

	return dh;
error:
	FreeDhKey(dh);

	return NULL;
}

int mrvl_dh_compute_key(void *dh, u8 *shared_key, u32 shared_len,
			u8 *public_key, u32 public_len,
			u8 *private_key, u32 private_len,
			DH_PG_PARAMS *dh_params)
{
	u8 *key = NULL;
	u32 key_len = 0;
	u32 ret = 0;

	key_len = shared_len;
	key = crypto_mem_malloc(key_len);
	if (key == NULL)
		return ret;

	ret = DhAgree(dh, key, &key_len, private_key, private_len, public_key,
			public_len);

	if (ret != 0)
		goto error;

	memcpy(shared_key, key, key_len);

	ret = key_len;

error:
	crypto_mem_free(key);
	return ret;
}

void mrvl_dh_free(void *dh_context)
{
	DhKey *dh = (DhKey *) dh_context;
	if (dh)
		FreeDhKey(dh);
}

static void sha1_vector(size_t num_elem, const u8 *addr[],
			const size_t *len, u8 *mac)
{
	Sha sha;
	size_t i;

	InitSha(&sha);
	for (i = 0; i < num_elem; i++)
		ShaUpdate(&sha, addr[i], len[i]);
	ShaFinal(&sha, mac);
}

u32 mrvl_sha1_vector(size_t nmsg, const u8 *msg[], const size_t msglen[],
			u8 *mac, size_t maclen)
{
	sha1_vector(nmsg, msg, msglen, mac);
	return 0;
}

static void md5_vector(size_t num_elem, const u8 *addr[],
			const size_t *len, u8 *mac)
{
	Md5 md5;
	size_t i;

	InitMd5(&md5);
	for (i = 0; i < num_elem; i++)
		Md5Update(&md5, addr[i], len[i]);
	Md5Final(&md5, mac);
}

static u32 mrvl_md5_vector(size_t nmsg, const u8 *msg[], const size_t msglen[],
			u8 *mac, size_t maclen)
{
	md5_vector(nmsg, msg, msglen, mac);
	return 0;
}

u32 mrvl_sha256_vector(size_t nmsg, const u8 *msg[], const size_t msglen[],
			u8 *mac, size_t maclen)
{
	const u8 *addr[2];
	size_t len[2], i;

	for (i = 0; i < nmsg; i++) {
		addr[i] = (u8 *) msg[i];
		len[i] = (size_t) msglen[i];
	}

	mrvl_sha256(nmsg, addr, len, mac);

	return maclen;
}

void mrvl_sha256(size_t num_elem, const u8 *addr[], const size_t *len,
		  u8 *mac)
{
	Sha256 sha;
	size_t i;

	InitSha256(&sha);

	for (i = 0; i < num_elem; i++)
		Sha256Update(&sha, addr[i], len[i]);

	Sha256Final(&sha, mac);
}

u32 mrvl_hmac_sha256(const u8 *key, u32 keylen, u8 *msg, u32 msglen,
		     u8 *mac, u32 maclen)
{
	u8 k_pad[64];	/* padding - key XORd with ipad/opad */
	u8 tk[SHA256_DIGEST_SIZE];
	const u8 *_addr[2];
	size_t _len[2];
	u32 i;
	u32 ret = 0;

	if ((mac == NULL) || (maclen < SHA256_DIGEST_SIZE))
		return 0;

	/* if key is longer than 64 bytes reset it to key = SHA256(key) */
	if (keylen > 64) {
		mrvl_sha256_vector(1, &key, &keylen, tk, SHA256_DIGEST_SIZE);
		key = tk;
		keylen = SHA256_DIGEST_SIZE;
	}

	/* the HMAC_SHA256 transform looks like:
	 *
	 *
	 * SHA256(K XOR opad, SHA256(K XOR ipad, text))
	 *
	 * where K is an n byte key
	 * ipad is the byte 0x36 repeated 64 times
	 * opad is the byte 0x5c repeated 64 times
	 * and text is the data being protected */

	/* start out by storing key in ipad */
	memset(k_pad, 0, sizeof(k_pad));
	memcpy(k_pad, key, keylen);
	/* XOR key with ipad values */
	for (i = 0; i < 64; i++)
		k_pad[i] ^= 0x36;

	/* perform inner SHA256 */
	_addr[0] = k_pad;
	_len[0] = 64;
	_addr[1] = msg;
	_len[1] = msglen;

	mrvl_sha256_vector(2, _addr, _len, mac, maclen);

	memset(k_pad, 0, sizeof(k_pad));
	memcpy(k_pad, key, keylen);
	/* XOR key with opad values */
	for (i = 0; i < 64; i++)
		k_pad[i] ^= 0x5c;

	/* perform outer SHA256 */
	_addr[0] = k_pad;
	_len[0] = 64;
	_addr[1] = mac;
	_len[1] = SHA256_DIGEST_SIZE;

	ret = mrvl_sha256_vector(2, _addr, _len, mac, maclen);
	return ret;
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
	u8 mac[SHA256_DIGEST_SIZE];
	u32 mac_len = SHA256_DIGEST_SIZE;
	u8 msg[100];
	u32 iter =
	    (result_len * 8 + SHA256_DIGEST_SIZE - 1) / SHA256_DIGEST_SIZE;
	u8 *start;
	u8 *end;
	u32 i;

	start = result;
	end = start + result_len;

	for (i = 1; i < (iter + 1); i++) {
		sprintf((char *)msg,
			"%c%c%c%cWi-Fi Easy and Secure Key Derivation%c%c%c%c",
			0x00, 0x00, 0x00, i, 0x00, 0x00,
			(result_len * 8) / 0x100, (result_len * 8) % 0x100);

		mrvl_hmac_sha256(key, key_len, msg, 44, mac, mac_len);

		if ((start + mac_len) < end) {
			memcpy(start, mac, mac_len);
			start += mac_len;
		} else {
			memcpy(start, mac, (u32) (end - start));
			return 0;
		}

	}
	return 1;
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
	u8 *buf;
	aes_t enc;

	mdev_t *aes_dev = aes_drv_open(MDEV_AES_0);
	if (aes_dev == NULL) {
		crypto_e("AES driver init is required before open");
		return -1;
	}

	aes_drv_setkey(aes_dev, &enc, pKEK, KeyLen, IV,
		       AES_ENCRYPTION, HW_AES_MODE_CBC);

	if ((TextLen/16)%2) {
		buf = (u8 *)crypto_mem_malloc(TextLen);
		if (buf == NULL) {
			crypto_e("mrvl_aes_wrap malloc failed!");
			aes_drv_close(aes_dev);
			return -1;
		}

		memcpy(buf, pPlainTxt, TextLen);
		aes_drv_encrypt(aes_dev, &enc, buf, pCipTxt, TextLen);
	} else {
		buf = (u8 *)crypto_mem_malloc(TextLen+16);
		if (buf == NULL) {
			crypto_e("mrvl_aes_wrap malloc failed!");
			aes_drv_close(aes_dev);
			return -1;
		}

		memcpy(buf, pPlainTxt, TextLen);
		memset(&buf[TextLen], 0x10, 16);
		aes_drv_encrypt(aes_dev, &enc, buf, pCipTxt, TextLen + 16);
	}

	crypto_mem_free(buf);
	aes_drv_close(aes_dev);
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
	aes_t dec;

	mdev_t *aes_dev = aes_drv_open(MDEV_AES_0);
	if (aes_dev == NULL) {
		crypto_e("AES driver init is required before open");
		return -1;
	}

	aes_drv_setkey(aes_dev, &dec, pKEK, KeyLen, IV,
		       AES_DECRYPTION, HW_AES_MODE_CBC);
	aes_drv_decrypt(aes_dev, &dec, pCipTxt, pPlainTxt, TextLen);
	aes_drv_close(aes_dev);

	return 0;
}

int mrvl_aes_wrap_ext(u8 *pPlainTxt, u32 TextLen, u8 *pCipTxt,
		      u8 *pKEK, u32 KeyLen, u8 *IV)
{
	u32 OrigLen = TextLen;
	int i;

	if (TextLen % 32 == 0) {
		mrvl_aes_wrap(pPlainTxt, TextLen, pCipTxt, pKEK, KeyLen, IV);
		TextLen += 16;
	} else {
		if (TextLen % 16 == 0 && TextLen % 32 != 0)
			TextLen = TextLen - TextLen % 32 + 32;
		else if (TextLen % 16 != 0)
			TextLen = TextLen - TextLen % 16 + 16;

		/* Padding the value of # of paddings */
		for (i = 0; i < TextLen - OrigLen; i++)
			*(pPlainTxt + OrigLen + i) = TextLen - OrigLen;

		mrvl_aes_wrap(pPlainTxt, TextLen, pCipTxt, pKEK, KeyLen, IV);
	}

	return TextLen;
}

int mrvl_aes_unwrap_ext(u8 *pCipTxt, u32 TextLen, u8 *pPlainTxt, u8 *pKEK,
			u32 KeyLen, u8 *IV)
{
	u8 *ptr;
	int i, pad;

	/* Decrypt */
	mrvl_aes_unwrap(pCipTxt, TextLen, pPlainTxt, pKEK, KeyLen, IV);

	/* Remove padding */
	ptr = &pPlainTxt[TextLen - 1];

	pad = *ptr;

	if (pad > TextLen)
		return -1;

	for (i = 0; i < pad; i++) {
		if (*ptr-- != pad)
			return -1;
	}

	return TextLen - pad;
}

void mrvl_crypto_hmac_md5(u8 *input, int len, u8 *hash, char *hash_key)
{
	Hmac hmac;

	HmacSetKey(&hmac, MD5, (u8 *)hash_key, 16);
	HmacUpdate(&hmac, input, len);
	HmacFinal(&hmac, hash);
}

void mrvl_crypto_md5(u8 *input, int len, u8 *hash, int hlen)
{
	Md5 md5;

	InitMd5(&md5);
	Md5Update(&md5, input, len);
	Md5Final(&md5, hash);
}

void mrvl_crypto_pass_to_key(char *password, unsigned char *ssid,
					int ssidlength, int iterations,
					int count, unsigned char *output)
{
	PBKDF2((u8 *)output, (u8 *)password, strlen(password),
		(u8 *)ssid, ssidlength, iterations, 16, SHA);
}

void pbkdf2_sha1(const char *passphrase, const char *ssid, size_t ssid_len,
		int iterations, u8 *buf, size_t buflen)
{
	PBKDF2((u8 *)buf, (u8 *)passphrase, strlen(passphrase),
		(u8 *)ssid, ssid_len, iterations, 32, SHA);

}
/*
 * Following three functions are wrappers over AES mdev layer and are
 * present solely for the use of CyaSSL. This is because CyaSSL does not
 * support session based AES usage.
 *
 * All other users of AES should use the mdev API's directly.
 */
int cyassl_wrapper_aes_setkey(aes_t *aes, const u8 *userKey,
			      u32 key_len, const u8 *IV, int dir,
			      hw_aes_mode_t hw_mode)
{
	return aes_drv_setkey(NULL, aes, userKey, key_len, IV, dir, hw_mode);
}


int cyassl_wrapper_aes_ccm_setkey(aes_t *aes, const u8 *userKey, u32 key_len)
{
	return aes_drv_ccm_setkey(NULL, aes, userKey, key_len);
}

int cyassl_wrapper_aes_encrypt(aes_t *aes, const u8 *plain,
			       u8 *cipher, u32 sz)
{
	return aes_drv_encrypt(NULL, aes, plain, cipher, sz);
}

int cyassl_wrapper_aes_ccm_encrypt(aes_t *aes, char *cipher,
				   const char *plain, uint32_t sz,
				   const char *nonce, uint32_t nonceSz,
				   char *authTag, uint32_t authTagSz,
				   const char *authIn, uint32_t authInSz)
{
	return aes_drv_ccm_encrypt(NULL, aes, plain, cipher, sz,
				   nonce, nonceSz, authTag, authTagSz,
				   authIn, authInSz);
}

int cyassl_wrapper_aes_decrypt(aes_t *aes, const u8 *cipher,
			       u8 *plain, u32 sz)
{
	return aes_drv_decrypt(NULL, aes, cipher, plain, sz);
}

int cyassl_wrapper_aes_ccm_decrypt(aes_t *aes, char *plain,
				   const char *cipher, uint32_t sz,
				   const char *nonce, uint32_t nonceSz,
				   const char *authTag, uint32_t authTagSz,
				   const char *authIn, uint32_t authInSz)
{
	return aes_drv_ccm_decrypt(NULL, aes, cipher, plain, sz,
				   nonce, nonceSz, authTag, authTagSz,
				   authIn, authInSz);
}

/**
 * hmac_md5_vector - HMAC-MD5 over data vector (RFC 2104)
 * @key: Key for HMAC operations
 * @key_len: Length of the key in bytes
 * @num_elem: Number of elements in the data vector
 * @addr: Pointers to the data areas
 * @len: Lengths of the data blocks
 * @mac: Buffer for the hash (16 bytes)
 */
void
hmac_md5_vector(const u8 * key, size_t key_len, size_t num_elem,
                const u8 * addr[], const size_t * len, u8 * mac, size_t maclen)
{
    u8 k_pad[64];               /* padding - key XORd with ipad/opad */
    u8 tk[16];
    const u8 *_addr[6];
    size_t i, _len[6];

    if (num_elem > 5) {
        /* 
         * Fixed limit on the number of fragments to avoid having to
         * allocate memory (which could fail).
         */
        return;
    }

    /* if key is longer than 64 bytes reset it to key = MD5(key) */
    if (key_len > 64) {
        mrvl_md5_vector(1, &key, &key_len, tk, maclen);
        key = tk;
        key_len = 16;
    }

    /* the HMAC_MD5 transform looks like: MD5(K XOR opad, MD5(K XOR ipad,
       text)) where K is an n byte key ipad is the byte 0x36 repeated 64 times
       opad is the byte 0x5c repeated 64 times and text is the data being
       protected */

    /* start out by storing key in ipad */
    memset(k_pad, 0, sizeof(k_pad));
    memcpy(k_pad, key, key_len);

    /* XOR key with ipad values */
    for (i = 0; i < 64; i++)
        k_pad[i] ^= 0x36;

    /* perform inner MD5 */
    _addr[0] = k_pad;
    _len[0] = 64;
    for (i = 0; i < num_elem; i++) {
	_addr[i + 1] = addr[i];
        _len[i + 1] = len[i];
    }
    mrvl_md5_vector(1 + num_elem, _addr, _len, mac, maclen);

    memset(k_pad, 0, sizeof(k_pad));
    memcpy(k_pad, key, key_len);
    /* XOR key with opad values */
    for (i = 0; i < 64; i++)
        k_pad[i] ^= 0x5c;

    /* perform outer MD5 */
    _addr[0] = k_pad;
    _len[0] = 64;
    _addr[1] = mac;
    _len[1] = MD5_MAC_LEN;
    mrvl_md5_vector(2, _addr, _len, mac, maclen);
}

/**
 * hmac_sha1_vector - HMAC-SHA1 over data vector (RFC 2104)
 * @key: Key for HMAC operations
 * @key_len: Length of the key in bytes
 * @num_elem: Number of elements in the data vector
 * @addr: Pointers to the data areas
 * @len: Lengths of the data blocks
 * @mac: Buffer for the hash (20 bytes)
 */
int hmac_sha1_vector(const u8 *key, size_t key_len, size_t num_elem,
		const u8 *addr[], const size_t *len, u8 *mac, size_t maclen)
{
	unsigned char k_pad[64];    /* padding - key XORd with ipad/opad */
	unsigned char tk[20];
	const u8 *_addr[6];
	size_t _len[6], i;

	if (num_elem > 5) {
		/*
		 * Fixed limit on the number of fragments to avoid having to
		 * allocate memory (which could fail).
		 */
		return -1;
	}

	/* if key is longer than 64 bytes reset it to key = SHA1(key) */
	if (key_len > 64) {
		mrvl_sha1_vector(1, &key, &key_len, tk, maclen);
		key = tk;
		key_len = 20;
	}

	/* the HMAC_SHA1 transform looks like: SHA1(K XOR opad, SHA1(K XOR ipad,
	   text)) where K is an n byte key ipad is the byte 0x36 repeated 64
	   times
	   opad is the byte 0x5c repeated 64 times and text is the data being
	   protected */

	/* start out by storing key in ipad */
	memset(k_pad, 0, sizeof(k_pad));
	memcpy(k_pad, key, key_len);
	/* XOR key with ipad values */
	for (i = 0; i < 64; i++)
		k_pad[i] ^= 0x36;

	/* perform inner SHA1 */
	_addr[0] = k_pad;
	_len[0] = 64;
	for (i = 0; i < num_elem; i++) {
		_addr[i + 1] = addr[i];
		_len[i + 1] = len[i];
	}
	mrvl_sha1_vector(1 + num_elem, _addr, _len, mac, maclen);

	memset(k_pad, 0, sizeof(k_pad));
	memcpy(k_pad, key, key_len);
	/* XOR key with opad values */
	for (i = 0; i < 64; i++)
		k_pad[i] ^= 0x5c;

	/* perform outer SHA1 */
	_addr[0] = k_pad;
	_len[0] = 64;
	_addr[1] = mac;
	_len[1] = SHA1_MAC_LEN;
	mrvl_sha1_vector(2, _addr, _len, mac, maclen);

	return 0;
}

/**
 * hmac_md5 - HMAC-MD5 over data buffer (RFC 2104)
 * @key: Key for HMAC operations
 * @key_len: Length of the key in bytes
 * @data: Pointers to the data area
 * @data_len: Length of the data area
 * @mac: Buffer for the hash (16 bytes)
 */
void
hmac_md5(const u8 * key, size_t key_len, const u8 * data, size_t data_len,
         u8 * mac, size_t maclen)
{
    hmac_md5_vector(key, key_len, 1, &data, &data_len, mac, maclen);
}

/**
 * hmac_sha1 - HMAC-SHA1 over data buffer (RFC 2104)
 * @key: Key for HMAC operations
 * @key_len: Length of the key in bytes
 * @data: Pointers to the data area
 * @data_len: Length of the data area
 * @mac: Buffer for the hash (20 bytes)
 */
int hmac_sha1(const u8 *key, size_t key_len, const u8 *data, size_t data_len,
		u8 *mac, size_t maclen)
{
	return hmac_sha1_vector(key, key_len, 1, &data, &data_len, mac, maclen);
}

void mrvl_hmac_md5(const u8 *key, size_t key_len, const u8 *data,
		size_t data_len, u8 *mac, size_t maclen)
{
	hmac_md5(key, key_len, data, data_len, mac, maclen);
}

/**
 * tls_prf - Pseudo-Random Function for TLS (TLS-PRF, RFC 2246)
 * @secret: Key for PRF
 * @secret_len: Length of the key in bytes
 * @label: A unique label for each purpose of the PRF
 * @seed: Seed value to bind into the key
 * @seed_len: Length of the seed
 * @out: Buffer for the generated pseudo-random key
 * @outlen: Number of bytes of key to generate
 * Returns: 0 on success, -1 on failure.
 *
 * This function is used to derive new, cryptographically separate keys from a
 * given key in TLS. This PRF is defined in RFC 2246, Chapter 5.
 */
int mrvl_tls_prf(const u8 *secret, size_t secret_len, const char *label,
		const u8 *seed, size_t seed_len, u8 *out, size_t outlen)
{
	size_t L_S1, L_S2, i;
	const u8 *S1, *S2;
	u8 A_MD5[MD5_MAC_LEN], A_SHA1[SHA1_MAC_LEN];
	u8 P_MD5[MD5_MAC_LEN], P_SHA1[SHA1_MAC_LEN];
	int MD5_pos, SHA1_pos;
	const u8 *MD5_addr[3];
	size_t MD5_len[3];
	const unsigned char *SHA1_addr[3];
	size_t SHA1_len[3];

	if (secret_len & 1)
		return -1;

	MD5_addr[0] = A_MD5;
	MD5_len[0] = MD5_MAC_LEN;
	MD5_addr[1] = (unsigned char *) label;
	MD5_len[1] = strlen(label);
	MD5_addr[2] = seed;
	MD5_len[2] = seed_len;

	SHA1_addr[0] = A_SHA1;
	SHA1_len[0] = SHA1_MAC_LEN;
	SHA1_addr[1] = (unsigned char *) label;
	SHA1_len[1] = strlen(label);
	SHA1_addr[2] = seed;
	SHA1_len[2] = seed_len;

	/* RFC 2246, Chapter 5 A(0) = seed, A(i) = HMAC(secret, A(i-1)) P_hash =
	   HMAC(secret, A(1) + seed) + HMAC(secret, A(2) + seed) + .. PRF =
	   P_MD5(S1, label + seed) XOR P_SHA-1(S2, label + seed) */

	L_S1 = L_S2 = (secret_len + 1) / 2;
	S1 = secret;
	S2 = secret + L_S1;

	hmac_md5_vector(S1, L_S1, 2, &MD5_addr[1], &MD5_len[1], A_MD5, MD5_MAC_LEN);
	hmac_sha1_vector(S2, L_S2, 2, &SHA1_addr[1], &SHA1_len[1], A_SHA1, SHA1_MAC_LEN);

	MD5_pos = MD5_MAC_LEN;
	SHA1_pos = SHA1_MAC_LEN;
	for (i = 0; i < outlen; i++) {
		if (MD5_pos == MD5_MAC_LEN) {
			hmac_md5_vector(S1, L_S1, 3, MD5_addr, MD5_len, P_MD5, MD5_MAC_LEN);
			MD5_pos = 0;
			hmac_md5(S1, L_S1, A_MD5, MD5_MAC_LEN, A_MD5, MD5_MAC_LEN);
		}
		if (SHA1_pos == SHA1_MAC_LEN) {
			hmac_sha1_vector(S2, L_S2, 3, SHA1_addr, SHA1_len,
						P_SHA1, SHA1_MAC_LEN);
			SHA1_pos = 0;
			hmac_sha1(S2, L_S2, A_SHA1, SHA1_MAC_LEN, A_SHA1, SHA1_MAC_LEN);
		}

		out[i] = P_MD5[MD5_pos] ^ P_SHA1[SHA1_pos];

		MD5_pos++;
		SHA1_pos++;
	}

	return 0;
}

/**
 * sha1_prf - SHA1-based Pseudo-Random Function (PRF) (IEEE 802.11i, 8.5.1.1)
 * @key: Key for PRF
 * @key_len: Length of the key in bytes
 * @label: A unique label for each purpose of the PRF
 * @data: Extra data to bind into the key
 * @data_len: Length of the data
 * @buf: Buffer for the generated pseudo-random key
 * @buf_len: Number of bytes of key to generate
 * Returns: 0 on success, -1 of failure
 *
 * This function is used to derive new, cryptographically separate keys from a
 * given key (e.g., PMK in IEEE 802.11i).
 */
int mrvl_sha1_prf(const u8 *key, size_t key_len, const char *label,
	     const u8 *data, size_t data_len, u8 *buf, size_t buf_len)
{
	u8 counter = 0;
	size_t pos, plen;
	u8 hash[SHA1_MAC_LEN];
	size_t label_len = strlen(label) + 1;
	const unsigned char *addr[3];
	size_t len[3];

	addr[0] = (u8 *) label;
	len[0] = label_len;
	addr[1] = data;
	len[1] = data_len;
	addr[2] = &counter;
	len[2] = 1;

	pos = 0;
	while (pos < buf_len) {
		plen = buf_len - pos;
		if (plen >= SHA1_MAC_LEN) {
			if (hmac_sha1_vector(key, key_len, 3, addr, len,
					     &buf[pos], SHA1_MAC_LEN))
				return -1;
			pos += SHA1_MAC_LEN;
		} else {
			if (hmac_sha1_vector(key, key_len, 3, addr, len,
					     hash, SHA1_MAC_LEN))
				return -1;
			memcpy(&buf[pos], hash, plen);
			break;
		}
		counter++;
	}

	return 0;
}
