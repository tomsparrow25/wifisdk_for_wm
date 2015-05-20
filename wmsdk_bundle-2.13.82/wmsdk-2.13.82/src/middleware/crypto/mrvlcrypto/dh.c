/** @file  dh.c 
  * @brief This file contains function for Diffie-Hellman Key
  *          which utilize the functions of modular exponentiation
  * 
  * Copyright (C) 2003-2008, Marvell International Ltd.
  * All Rights Reserved
  */

#include "includes.h"
#include "common.h"
#include "crypto.h"
#include "dh.h"

/********************************************************
        Local Functions
********************************************************/
static int generate_random_bytes(u8 *block, u32 block_len)
{
	u32 available = 0;
	u32 ut;

	ENTER();

	ut = os_ticks_get();
	srand(ut);

	while (block_len > available)
		block[available++] = rand() % 0x100;

	LEAVE();
	return 0;
}

/********************************************************
        Global Functions
********************************************************/
/** 
 *  @brief  Sets up Diffie-Hellman key agreement.
 *
 *  @param public_key       Pointer to public key generated
 *  @param public_len       Length of public key
 *  @param private_key      Pointer to private key generated randomly
 *  @param private_len      Length of private key
 *  @param dh_params        Parameters for DH algorithm
 *  @return                 0 on success, -1 on failure
 */
int
setup_dh_agreement(u8 *public_key,
		   u32 public_len,
		   u8 *private_key, u32 private_len, DH_PG_PARAMS *dh_params)
{
	size_t generate_len;

	ENTER();

	if (generate_random_bytes(private_key, private_len) != 0) {
		LEAVE();
		return -1;
	}

	generate_len = dh_params->primeLen;
	if (crypto_mod_exp(dh_params->generator, dh_params->generatorLen,
			   private_key, private_len,
			   dh_params->prime, dh_params->primeLen,
			   public_key, &generate_len) < 0) {
		crypto_d("setup_dh_agreement: crypto_mod_exp failed");
		LEAVE();
		return -WM_FAIL;
	}

	LEAVE();
	return WM_SUCCESS;
}

/** 
 *  @brief  Computes agreed shared key from the public value,
 *          private value, and Diffie-Hellman parameters.
 *
 *  @param shared_key       Pointer to agreed shared key generated
 *  @param public_key       Pointer to public key
 *  @param public_len       Length of public key
 *  @param private_key      Pointer to private key
 *  @param private_len      Length of private key
 *  @param dh_params        Parameters for DH algorithm
 *  @return                 0 on success, -1 on failure
 */
int
compute_dh_agreed_key(u8 * shared_key,
		      u8 *public_key,
		      u32 public_len,
		      u8 *private_key,
		      u32 private_len, DH_PG_PARAMS *dh_params)
{
	size_t shared_len;

	ENTER();

	if (shared_key == NULL || public_key == NULL || private_key == NULL) {
		LEAVE();
		return -1;
	}

	shared_len = dh_params->primeLen;
	memset(shared_key, 0, 192);

	if (crypto_mod_exp(public_key, public_len,
			   private_key, private_len,
			   dh_params->prime, dh_params->primeLen,
			   shared_key, &shared_len) < 0) {
		crypto_d("compute_dh_agreed_key: crypto_mod_exp failed");
		LEAVE();
		return -WM_FAIL;
	}

	LEAVE();
	return shared_len;
}
