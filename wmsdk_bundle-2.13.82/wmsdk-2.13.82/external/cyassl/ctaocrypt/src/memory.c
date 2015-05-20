/* memory.c 
 *
 * Copyright (C) 2006-2013 wolfSSL Inc.  All rights reserved.
 *
 * This file is part of CyaSSL.
 *
 * Contact licensing@yassl.com with any questions or comments.
 *
 * http://www.yassl.com
 */


#ifdef HAVE_CONFIG_H
    #include <config.h>
#endif

#include <cyassl/ctaocrypt/settings.h>

#ifdef USE_CYASSL_MEMORY

#include <cyassl/ctaocrypt/memory.h>
#include <cyassl/ctaocrypt/error.h>

#ifdef CYASSL_MALLOC_CHECK
    #include <stdio.h>
#endif

/* Set these to default values initially. */
static CyaSSL_Malloc_cb  malloc_function = 0;
static CyaSSL_Free_cb    free_function = 0;
static CyaSSL_Realloc_cb realloc_function = 0;

int CyaSSL_SetAllocators(CyaSSL_Malloc_cb  mf,
                         CyaSSL_Free_cb    ff,
                         CyaSSL_Realloc_cb rf)
{
    int res = 0;

    if (mf)
        malloc_function = mf;
	else
        res = BAD_FUNC_ARG;

    if (ff)
        free_function = ff;
    else
        res = BAD_FUNC_ARG;

    if (rf)
        realloc_function = rf;
    else
        res = BAD_FUNC_ARG;

    return res;
}


void* CyaSSL_Malloc(size_t size)
{
    void* res = 0;

    if (malloc_function)
        res = malloc_function(size);
#if 0
    else
        res = malloc(size);
#endif /* 0 */

    #ifdef CYASSL_MALLOC_CHECK
        if (res == NULL)
            printf("CyaSSL_malloc failed\n");
    #endif
				
    return res;
}

void CyaSSL_Free(void *ptr)
{
    if (free_function)
        free_function(ptr);
#if 0
    else
        free(ptr);
#endif /* 0 */
}

void* CyaSSL_Realloc(void *ptr, size_t size)
{
    void* res = 0;

    if (realloc_function)
        res = realloc_function(ptr, size);
#if 0
    else
        res = realloc(ptr, size);
#endif /* 0 */

    return res;
}

#endif /* USE_CYASSL_MEMORY */
