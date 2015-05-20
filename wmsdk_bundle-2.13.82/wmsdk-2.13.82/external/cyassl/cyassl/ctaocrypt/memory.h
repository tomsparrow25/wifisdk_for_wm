/* memory.h
 *
 * Copyright (C) 2006-2013 wolfSSL Inc.  All rights reserved.
 *
 * This file is part of CyaSSL.
 *
 * Contact licensing@yassl.com with any questions or comments.
 *
 * http://www.yassl.com
 */


/* submitted by eof */


#ifndef CYASSL_MEMORY_H
#define CYASSL_MEMORY_H

#include <stdlib.h>

#ifdef __cplusplus
    extern "C" {
#endif


typedef void *(*CyaSSL_Malloc_cb)(size_t size);
typedef void (*CyaSSL_Free_cb)(void *ptr);
typedef void *(*CyaSSL_Realloc_cb)(void *ptr, size_t size);


/* Public set function */
CYASSL_API int CyaSSL_SetAllocators(CyaSSL_Malloc_cb  malloc_function,
                                    CyaSSL_Free_cb    free_function,
                                    CyaSSL_Realloc_cb realloc_function);

/* Public in case user app wants to use XMALLOC/XFREE */
CYASSL_API void* CyaSSL_Malloc(size_t size);
CYASSL_API void  CyaSSL_Free(void *ptr);
CYASSL_API void* CyaSSL_Realloc(void *ptr, size_t size);


#ifdef __cplusplus
}
#endif

#endif /* CYASSL_MEMORY_H */
