/* crl.h
 *
 * Copyright (C) 2006-2013 wolfSSL Inc.  All rights reserved.
 *
 * This file is part of CyaSSL.
 *
 * Contact licensing@yassl.com with any questions or comments.
 *
 * http://www.yassl.com
 */



#ifndef CYASSL_CRL_H
#define CYASSL_CRL_H


#ifdef HAVE_CRL

#include <cyassl/ssl.h>
#include <cyassl/ctaocrypt/asn.h>

#ifdef __cplusplus
    extern "C" {
#endif

typedef struct CYASSL_CRL CYASSL_CRL;

CYASSL_LOCAL int  InitCRL(CYASSL_CRL*, CYASSL_CERT_MANAGER*);
CYASSL_LOCAL void FreeCRL(CYASSL_CRL*, int dynamic);

CYASSL_LOCAL int  LoadCRL(CYASSL_CRL* crl, const char* path, int type, int mon);
CYASSL_LOCAL int  BufferLoadCRL(CYASSL_CRL*, const byte*, long, int);
CYASSL_LOCAL int  CheckCertCRL(CYASSL_CRL*, DecodedCert*);


#ifdef __cplusplus
    }  /* extern "C" */
#endif

#endif /* HAVE_CRL */
#endif /* CYASSL_CRL_H */
