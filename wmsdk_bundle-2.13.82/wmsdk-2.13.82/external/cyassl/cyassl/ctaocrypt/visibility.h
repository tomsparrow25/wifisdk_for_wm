/* visibility.h
 *
 * Copyright (C) 2006-2013 wolfSSL Inc.  All rights reserved.
 *
 * This file is part of CyaSSL.
 *
 * Contact licensing@yassl.com with any questions or comments.
 *
 * http://www.yassl.com
 */


/* Visibility control macros */


#ifndef CTAO_CRYPT_VISIBILITY_H
#define CTAO_CRYPT_VISIBILITY_H


/* CYASSL_API is used for the public API symbols.
        It either imports or exports (or does nothing for static builds)

   CYASSL_LOCAL is used for non-API symbols (private).
*/

#if defined(BUILDING_CYASSL)
    #if defined(HAVE_VISIBILITY) && HAVE_VISIBILITY
        #define CYASSL_API   __attribute__ ((visibility("default")))
        #define CYASSL_LOCAL __attribute__ ((visibility("hidden")))
    #elif defined(__SUNPRO_C) && (__SUNPRO_C >= 0x550)
        #define CYASSL_API   __global  
        #define CYASSL_LOCAL __hidden
    #elif defined(_MSC_VER)
        #ifdef CYASSL_DLL
            #define CYASSL_API extern __declspec(dllexport)
        #else
            #define CYASSL_API
        #endif
        #define CYASSL_LOCAL
    #else
        #define CYASSL_API
        #define CYASSL_LOCAL
    #endif /* HAVE_VISIBILITY */
#else /* BUILDING_CYASSL */
    #if defined(_MSC_VER)
        #ifdef CYASSL_DLL
            #define CYASSL_API extern __declspec(dllimport)
        #else
            #define CYASSL_API
        #endif
        #define CYASSL_LOCAL
    #else
        #define CYASSL_API
        #define CYASSL_LOCAL
    #endif
#endif /* BUILDING_CYASSL */


#endif /* CTAO_CRYPT_VISIBILITY_H */

