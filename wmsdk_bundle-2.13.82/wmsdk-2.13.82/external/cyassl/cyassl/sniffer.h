/* sniffer.h
 *
 * Copyright (C) 2006-2013 wolfSSL Inc.  All rights reserved.
 *
 * This file is part of CyaSSL.
 *
 * Contact licensing@yassl.com with any questions or comments.
 *
 * http://www.yassl.com
 */



#ifndef CYASSL_SNIFFER_H
#define CYASSL_SNIFFER_H

#include <cyassl/ctaocrypt/settings.h>

#ifdef _WIN32
    #ifdef SSL_SNIFFER_EXPORTS
        #define SSL_SNIFFER_API __declspec(dllexport)
    #else
        #define SSL_SNIFFER_API __declspec(dllimport)
    #endif
#else
    #define SSL_SNIFFER_API
#endif /* _WIN32 */


#ifdef __cplusplus
    extern "C" {
#endif


CYASSL_API 
SSL_SNIFFER_API int ssl_SetPrivateKey(const char* address, int port,
                                      const char* keyFile, int keyType,
                                      const char* password, char* error);

CYASSL_API 
SSL_SNIFFER_API int ssl_DecodePacket(const unsigned char* packet, int length,
                                     unsigned char* data, char* error);

CYASSL_API 
SSL_SNIFFER_API int ssl_Trace(const char* traceFile, char* error);
        
        
CYASSL_API void ssl_InitSniffer(void);
        
CYASSL_API void ssl_FreeSniffer(void);

        
/* ssl_SetPrivateKey keyTypes */
enum {
    FILETYPE_PEM = 1,
    FILETYPE_DER = 2,
};


#ifdef __cplusplus
    }  /* extern "C" */
#endif

#endif /* CyaSSL_SNIFFER_H */

