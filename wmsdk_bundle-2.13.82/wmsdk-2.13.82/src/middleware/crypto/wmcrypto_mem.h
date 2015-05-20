/*
 *  Copyright 2008-2013, Marvell International Ltd.
 *  All Rights Reserved.
 */

/** Malloc memory
 */
void *crypto_mem_malloc(size_t size);

/** Free previously allocated memory
 */
void crypto_mem_free(void *ptr);

/** Calloc memory
 */
void *crypto_mem_calloc(size_t nmemb, size_t size);

/** Realloc memory
 */
void *crypto_mem_realloc(void *ptr, size_t size);
