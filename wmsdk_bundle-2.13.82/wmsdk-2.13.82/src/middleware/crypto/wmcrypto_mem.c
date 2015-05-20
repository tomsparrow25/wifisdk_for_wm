/*
 *  Copyright 2008-2013, Marvell International Ltd.
 *  All Rights Reserved.
 */

/* crypto_mem.c: Marvell crypto  memory routines
 *
 */

#include <stdio.h>
#include <string.h>
#include <wmstdio.h>
#include <wm_os.h>

#include <wmcrypto.h>
#include "wmcrypto_mem.h"

void *crypto_mem_malloc(size_t size)
{
	if (size == 0)
		return NULL;

	void *buffer_ptr = os_mem_alloc(size);
	if (!buffer_ptr) {
		crypto_e("Failed to allocate mem: Size: %d",
			       size);
		return NULL;
	}

	return buffer_ptr;
}

void crypto_mem_free(void *buffer)
{
	os_mem_free(buffer);
}

void *crypto_mem_calloc(size_t nmemb, size_t size)
{
	return crypto_mem_malloc(nmemb * size);
}

void *crypto_mem_realloc(void *ptr, size_t size)
{
	void *new_ptr = os_mem_realloc(ptr, size);
	if (!new_ptr) {
		crypto_e("Crypto: Failed to realloc: %d", size);
		return NULL;
	}

	return new_ptr;
}
