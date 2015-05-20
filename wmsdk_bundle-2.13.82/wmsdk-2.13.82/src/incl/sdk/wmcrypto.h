
/** @file wmcrypto.h
 *
 *  @brief Crypto Functions
 *
 *  This provides crypto wrapper functions that selectively call the Marvell or
 *  CyaSSL crypto functions based on the selected configuration.
 */
/*
 *  Copyright 2008-2013, Marvell International Ltd.
 *  All Rights Reserved
 */

#ifndef WMCRYPTO_H
#define WMCRYPTO_H

#include <stdint.h>
#ifdef CONFIG_CYASSL
#include <hmac.h>
#include <pwdbased.h>
#endif

#include <wmlog.h>

#define crypto_e(...)				\
	wmlog_e("crc", ##__VA_ARGS__)
#define crypto_w(...)				\
	wmlog_w("crc", ##__VA_ARGS__)

#ifdef CONFIG_CRYPTO_DEBUG
#define crypto_d(...)				\
	wmlog("crc", ##__VA_ARGS__)
#else
#define crypto_d(...)
#endif /* ! CONFIG_CRYPTO_DEBUG */

/** Unsigned char */
#define u8      uint8_t
/** Unsigned long integer */
#define u32     uint32_t

/** Digest size */
#define SHA256_DIGEST_SIZE (256 / 8)
/** Block size  */
#define SHA256_BLOCK_SIZE  (512 / 8)

#define SHA1_MAC_LEN 20
#define MD5_MAC_LEN 16

/**
 *  * Diffie-Hellman parameters.
 *   */
typedef struct {
	/** prime */
	unsigned char *prime;
	/** length of prime */
	unsigned int primeLen;
	/** generator */
	unsigned char *generator;
	/** length of generator */
	unsigned int generatorLen;
} DH_PG_PARAMS;

/**********************************************************
 *****     Diffie-Hellman Shared Key Generation       *****
 **********************************************************/
/**
 *  @brief  Sets up Diffie-Hellman key agreement.
 *
 *  @param public_key       Pointers to public key generated
 *  @param public_len       Length of public key
 *  @param private_key      Pointers to private key generated randomly
 *  @param private_len      Length of private key
 *  @param dh_params        Parameters for DH algorithm
 *
 *  @return                 0 on success, -1 on failure
 */
void *mrvl_dh_setup_key(u8 *public_key, u32 public_len,
			u8 *private_key, u32 private_len,
			DH_PG_PARAMS *dh_params);

/**
 *  @brief  Computes agreed shared key from the public value,
 *          private value, and Diffie-Hellman parameters.
 *
 *  @param dh		    DH key
 *  @param shared_key       Pointer to agreed shared key generated
 *  @param shared_len       Lenght of agreed shared key
 *  @param public_key       Pointer to public key generated
 *  @param public_len       Length of public key
 *  @param private_key      Pointer to private key generated randomly
 *  @param private_len      Length of private key
 *  @param dh_params        Parameters for DH algorithm
 *
 *  @return                 0 on success, -1 on failure
 */
int mrvl_dh_compute_key(void *dh, u8 *shared_key, u32 shared_len,
			u8 *public_key, u32 public_len,
			u8 *private_key, u32 private_len,
			DH_PG_PARAMS *dh_params);

/**
 * @brief  Free allocated DH key
 *
 * @param dh_context	   DH key
 *
 * @return		   None
 */
void mrvl_dh_free(void *dh_context);

/**
 *  @brief  SHA-1 hash for data vector
 *
 *  @param nmsg		    Number of elements in the data vector
 *  @param msg		    Pointers to the data areas
 *  @param msglen	    Lengths of the data blocks
 *  @param mac		    Buffer for the hash
 *  @param maclen	    Length of hash buffer
 *
 *  @return		    0 on success, -1 of failure
 */
u32 mrvl_sha1_vector(size_t nmsg, const u8 *msg[], const size_t msglen[],
		     u8 *mac, size_t maclen);

/**********************************************************
 ***   HMAC-SHA256
 **********************************************************/
/**
 *  @brief  SHA-256 hash for data vector
 *
 *  @param nmsg		    Number of elements in the data vector
 *  @param msg              Pointers to the data areas
 *  @param msglen           Lengths of the data blocks
 *  @param mac              Buffer for the hash
 *  @param maclen	    Length of hash buffer
 *
 *  @return                 0 on success, -1 on failure
 */
u32 mrvl_sha256_vector(size_t nmsg, const u8 *msg[], const size_t msglen[],
			u8 *mac, size_t maclen);

/**
 *  @brief  Wrapper function for SHA256 hash
 *
 *  @param num_elem	    Number of elements in the data vector
 *  @param addr             Pointers to the data areas
 *  @param len              Lengths of the data blocks
 *  @param mac		    Buffer for the hash
 *
 *  @return                 0 on success, -1 on failure
 */

void mrvl_sha256(size_t num_elem, const u8 *addr[], const size_t *len,
			u8 *mac);

/**
 *  @brief  Wrapper function for HMAC-SHA256 (RFC 2104)
 *
 *  @param key              Key for HMAC operations
 *  @param keylen          Length of the key in bytes
 *  @param msg		    Pointers to the data areas
 *  @param msglen	    Lengths of the data blocks
 *  @param mac              Buffer for the hash (32 bytes)
 *  @param maclen           Length of hash buffer
 *
 *  @return                 0 on success, -1 on failure
 */
u32 mrvl_hmac_sha256(const u8 *key, u32 keylen, u8 *msg, u32 msglen,
		     u8 *mac, u32 maclen);

/**
 *  @brief Key derivation function to generate Authentication
 *         Key and Key Wrap Key used in WPS registration protocol
 *
 *  @param key		    Input key to generate authentication key (KDK)
 *  @param key_len          Length of input key
 *  @param result           result buffer
 *  @param result_len       Length of result buffer
 *
 * @return		    0 on success, 1 otherwise
 */
int mrvl_kdf(u8 *key, u32 key_len, u8 *result, u32 result_len);

/**********************************************************
 *  ***   AES Key Wrap Key
 **********************************************************/
/**
 *  @brief  Wrap keys with AES Key Wrap Algorithm (128-bit KEK)
 *
 *  @param plain_txt	    Plaintext key to be wrapped
 *  @param txt_len          Length of the plain key in bytes (16 bytes)
 *  @param cip_txt          Wrapped key
 *  @param kek              Key encryption key (KEK)
 *  @param kek_len          Length of KEK in bytes (must be divisible by 16)
 *  @param iv               Encryption IV for CBC mode (16 bytes)
 *
 *  @return                 0 on success, -1 on failure
 */
int mrvl_aes_wrap(u8 *plain_txt, u32 txt_len, u8 *cip_txt,
		  u8 *kek, u32 kek_len, u8 *iv);

/**
 *  @brief  Unwrap key with AES Key Wrap Algorithm (128-bit KEK)
 *
 *  @param cip_txt          Wrapped key to be unwrapped
 *  @param txt_len          Length of the wrapped key in bytes (16 bytes)
 *  @param plain_txt        Plaintext key
 *  @param kek              Key encryption key (KEK)
 *  @param kek_len          Length of KEK in bytes (must be divisible by 16)
 *  @param iv               Encryption IV for CBC mode (16 bytes)
 *
 *  @return                 0 on success, -1 on failure
 */
int mrvl_aes_unwrap(u8 *cip_txt, u32 txt_len, u8 *plain_txt,
		    u8 *kek, u32 kek_len, u8 *iv);

/**********************************************************
 *  ***   Extended AES Key Wrap Key
 **********************************************************/
/**
 *  @brief  Wrap keys with Extended AES Key Wrap Algorithm (128-bit KEK)
 *
 *  @param plain_txt	    Plaintext key to be wrapped
 *  @param plain_len       Length of the plain key in bytes (16 bytes)
 *  @param cip_txt          Wrapped key
 *  @param kek              Key encryption key (KEK)
 *  @param kek_Len          Length of KEK in bytes (must be divisible by 16)
 *  @param iv               Encryption IV for CBC mode (16 bytes)
 *
 *  @return                 0 on success, -1 on failure
 */
int mrvl_aes_wrap_ext(u8 *plain_txt, u32 plain_len, u8 *cip_txt,
		      u8 *kek, u32 kek_Len, u8 *iv);

/**
 *  @brief  Unwrap key with Extended AES Key Wrap Algorithm (128-bit KEK)
 *
 *  @param cip_txt          Wrapped key to be unwrapped
 *  @param cip_Len          Length of the wrapped key in bytes (16 bytes)
 *  @param plain_txt        Plaintext key
 *  @param kek              Key encryption key (KEK)
 *  @param key_len          Length of KEK in bytes (must be divisible by 16)
 *  @param iv               Encryption IV for CBC mode (16 bytes)
 *
 *  @return                 0 on success, -1 on failure
 */
int mrvl_aes_unwrap_ext(u8 *cip_txt, u32 cip_Len, u8 *plain_txt,
			u8 *kek, u32 key_len, u8 *iv);

/**
 *  @brief  Wrapper function for HMAC-MD5
 *
 *  @param input            Pointer to input data
 *  @param len              Length of input data
 *  @param hash		    Pointer to hash
 *  @param hash_key	    Pointer to hash key
 *
 *  @return                 None
 */
void mrvl_crypto_hmac_md5(u8 *input, int len, u8 *hash, char *hash_key);

/**
 *  @brief  Wrapper function for MD5
 *
 *  @param input            Pointer to input data
 *  @param len              Length of input data
 *  @param hash	            Pointer to hash
 *  @param hlen		    Pointer to hash len
 *
 *  @return                 None
 */
void mrvl_crypto_md5(u8 *input, int len, u8 *hash, int hlen);

/**
 *  @brief  Wrapper function for PBKDF2
 *
 *  @param password         Pointer to password
 *  @param ssid             Pointer to ssid
 *  @param ssidlength	    Length of ssid
 *  @param iterations	    No if iterations
 *  @param count            Count
 *  @param output           Pointer to output data
 *
 *  @return                 None
 */
void mrvl_crypto_pass_to_key(char *password, unsigned char *ssid,
				int ssidlength, int iterations,
				int count, unsigned char *output);

int mrvl_sha1_prf(const u8 *key, size_t key_len, const char *label,
	     const u8 *data, size_t data_len, u8 *buf, size_t buf_len);

int aes_wrap(const u8 *kek, int n, const u8 *plain, u8 *cipher);

int aes_unwrap(const u8 *kek, int n, const u8 *cipher, u8 *plain);

int hmac_sha1(const u8 *key, size_t key_len, const u8 *data, size_t data_len,
		u8 *mac, size_t maclen);

int mrvl_tls_prf(const u8 *secret, size_t secret_len, const char *label,
		const u8 *seed, size_t seed_len, u8 *out, size_t outlen);

void mrvl_hmac_md5(const u8 *key, size_t key_len, const u8 *data,
			size_t data_len, u8 *mac, size_t maclen);
#endif
