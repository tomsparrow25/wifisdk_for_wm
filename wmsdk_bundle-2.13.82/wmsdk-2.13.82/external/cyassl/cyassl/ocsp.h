/* ocsp.h
 *
 * Copyright (C) 2006-2013 wolfSSL Inc.  All rights reserved.
 *
 * This file is part of CyaSSL.
 *
 * Contact licensing@yassl.com with any questions or comments.
 *
 * http://www.yassl.com
 */



/* CyaSSL OCSP API */

#ifndef CYASSL_OCSP_H
#define CYASSL_OCSP_H

#ifdef HAVE_OCSP

#include <cyassl/ssl.h>
#include <cyassl/ctaocrypt/asn.h>

#ifdef __cplusplus
    extern "C" {
#endif

typedef struct CYASSL_OCSP CYASSL_OCSP;

CYASSL_LOCAL int  CyaSSL_OCSP_Init(CYASSL_OCSP*);
CYASSL_LOCAL void CyaSSL_OCSP_Cleanup(CYASSL_OCSP*);

CYASSL_LOCAL int  CyaSSL_OCSP_set_override_url(CYASSL_OCSP*, const char*);
CYASSL_LOCAL int  CyaSSL_OCSP_Lookup_Cert(CYASSL_OCSP*, DecodedCert*);


#ifdef __cplusplus
    }  /* extern "C" */
#endif


#endif /* HAVE_OCSP */
#endif /* CYASSL_OCSP_H */


