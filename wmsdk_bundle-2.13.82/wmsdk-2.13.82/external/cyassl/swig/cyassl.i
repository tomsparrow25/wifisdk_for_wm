/* cyassl.i
 *
 * Copyright (C) 2006-2013 wolfSSL Inc.  All rights reserved.
 *
 * This file is part of CyaSSL.
 *
 * Contact licensing@yassl.com with any questions or comments.
 *
 * http://www.yassl.com
 */


%module cyassl
%{
    #include <cyassl/ssl.h>
    #include <cyassl/ctaocrypt/rsa.h>

    /* defn adds */
    char* CyaSSL_error_string(int err);
    int   CyaSSL_swig_connect(CYASSL*, const char* server, int port);
    RNG*  GetRng(void);
    RsaKey* GetRsaPrivateKey(const char* file);
    void    FillSignStr(unsigned char*, const char*, int);
%}


CYASSL_METHOD* CyaTLSv1_client_method(void);
CYASSL_CTX*    CyaSSL_CTX_new(CYASSL_METHOD*);
int            CyaSSL_CTX_load_verify_locations(CYASSL_CTX*, const char*, const char*);
CYASSL*        CyaSSL_new(CYASSL_CTX*);
int            CyaSSL_get_error(CYASSL*, int);
int            CyaSSL_write(CYASSL*, const char*, int);
int            CyaSSL_Debugging_ON(void);
int            CyaSSL_Init(void);
char*          CyaSSL_error_string(int);
int            CyaSSL_swig_connect(CYASSL*, const char* server, int port);

int         RsaSSL_Sign(const unsigned char* in, int inLen, unsigned char* out, int outLen, RsaKey* key, RNG* rng);

int         RsaSSL_Verify(const unsigned char* in, int inLen, unsigned char* out, int outLen, RsaKey* key);

RNG* GetRng(void);
RsaKey* GetRsaPrivateKey(const char* file);
void    FillSignStr(unsigned char*, const char*, int);

%include carrays.i
%include cdata.i
%array_class(unsigned char, byteArray);
int         CyaSSL_read(CYASSL*, unsigned char*, int);


#define    SSL_FAILURE      0
#define    SSL_SUCCESS      1

