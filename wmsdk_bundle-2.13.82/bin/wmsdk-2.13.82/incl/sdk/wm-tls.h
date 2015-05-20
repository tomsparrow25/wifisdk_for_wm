/*! \file wm-tls.h
 *  \brief TLS API's
 *
 * The TLS module is a software components that provides an interface to a
 * selected TLS library. Currently this module interfaces with the CyaSSL
 * library. This provides functionality to work with secure TLS connections
 * for both client and server applications.
 *
 * A TLS client or TLS server application can be built using the APIs
 * present in this module. This module provides a thin layer on top of the
 * third party TLS library. The main goal of this module is to provide a
 * third party library independent API for the users. This protects the
 * other dependant modules from change if in case a different third party
 * library is used in the future.
 *
 * Most functionality of this module is centered in TLS session init
 * API. Other APIs are thin wrappers over the functions provided by the
 * third party library. One of the major tasks during a session init is to
 * set up all the certificates required for client and/or server
 * certificate validation.
 *
 *  Copyright 2008-2013, Marvell International Ltd.
 */

#ifndef __WM_TLS_H__
#define __WM_TLS_H__

#include <stdint.h>

/**
 * The tls_init_config_t paramter in the tls_session_init() takes a pointer
 * to the certificate buffer. The certificate buffer can either have a
 * single (or a chain of) X.509 PEM format certificate(s). If the server
 * uses a self-signed certificate, the same certificate needs to be present
 * in this buffer. If the server uses a CA signed certificate, you need to
 * have either of the following:
 * - the root CA certificate only
 * - the chain of certificates (entire or partial starting with the Root CA
 * certificate)
 * Note: The system time needs to be set correctly for successful
 * certificate validation.
 */

typedef int tls_handle_t;

typedef enum {
	/* TLS server mode */
	/* If this flag bit is zero client mode is assumed.*/
	TLS_SERVER_MODE = 0x01,
	TLS_CHECK_CLIENT_CERT = 0x02,

	/* TLS Client mode */
	TLS_CHECK_SERVER_CERT = 0x04,
	/* This will be needed if server mandates client certificate. If
	   this flag is enabled then client_cert and client_key from the
	   client structure in the union tls (from tls_init_config_t)
	   needs to be passed to tls_session_init() */
	TLS_USE_CLIENT_CERT = 0x08,
#ifdef CONFIG_WPA2_ENTP
	TLS_WPA2_ENTP = 0x10,
#endif
} tls_session_flags_t;

/*
 * Practical observation: It was observed that when the certificate is sent
 * its size send should be 1 less that sizeof(array in which it is
 * present.). Needs to be checked again.
 */
typedef struct {
	int flags;
	union {
		struct {
			/*
			 * Needed if the RADIUS server mandates verification of
			 * CA certificate. Otherwise set to NULL.*/
			const unsigned char *ca_cert;
			/* Size of CA_cert */
			int ca_cert_size;
			/* 
			 * Needed if the server mandates verification of
			 * client certificate. Otherwise set to NULL. In
			 * the former case please OR the flag
			 * TLS_USE_CLIENT_CERT to flags variable in
			 * tls_init_config_t passed to tls_session_init()
			 */
			const unsigned char *client_cert;
			/* Size of client_cert */
			int client_cert_size;
			/* Client private key */
			const unsigned char *client_key;
			/* Size of client key */
			int client_key_size;
		} client;
		struct {
			/* Mandatory. Will be sent to the client */
			const unsigned char *server_cert;
			/* Size of server_cert */
			int server_cert_size;
			/* 
			 * Server private key. Mandatory.
			 * For the perusal of the server 
			 */
			const unsigned char *server_key;
			/* Size of server_key */
			int server_key_size;
			/* 
			 * Needed if the server wants to verify client
			 * certificate. Otherwise set to NULL.
			 */
			const unsigned char *client_cert;
			/* Size of client_cert */
			int client_cert_size;
		}server;
	} tls;
} tls_init_config_t;

/**
 * Initialize the CTAO Crypto library
 *
 * \return -WM_FAIL if initialization failed.
 * \return WM_SUCCESS if operation successful.
 */
int ctaocrypt_lib_init(void);

/**
 * Initialize the TLS library
 *
 * @return Standard WMSDK return codes.
 */
int tls_lib_init(void);

/**
 * Start an TLS session on the given socket.
 *
 * @param[out] h Pointer to the handle,
 * @param[in] sockfd A socket descriptor on which 'connect' is called
 * before. This parameter is not to be given for TLS server init. The
 * client socket is to be 
 * @param[in] cfg TLS configuration request structure. To be allocated and
 * populated by the caller.
 *
 * @return Standard WMSDK return codes.
 */
int tls_session_init(tls_handle_t *h, int sockfd,
		     const tls_init_config_t *cfg);


int tls_server_set_clientfd(tls_handle_t h, int clientfd);

/**
 * Send data over an existing TLS connection.
 *
 * @param[in] h Handle returned from a previous call to TLS_session_init
 * @param[in] buf Buffer to send.
 * @param[in] len Length to write
 *
 * @return Amount data written to the network
 */
int tls_send(tls_handle_t h, const void *buf, int len);

/**
 * Receive data from an existing TLS connection.
 *
 * @param[in] h Handle returned from a previous call to TLS_session_init
 * @param[in] buf Buffer to receive data.
 * @param[in] max_len Max length of the buffer.
 *
 * @return Amount data read from the network
 */
int tls_recv(tls_handle_t h, void *buf, int max_len);

/**
 * Close the tls connection.
 *
 * This function will not close the socket. It will only terminate TLS
 * connection over it.
 *
 * @param[in,out] h Handle returned from a previous call to
 *                                  TLS_session_init. Will set it to NULL
 *                                  before returning.
 */
void tls_close(tls_handle_t *h);

/**
 * @internal
 */
int tls_session_key(tls_handle_t h, uint8_t *key, int len);

/**
 * @internal
 */
int get_session_key(uint8_t *key, int len);

/**
 * @internal
 */
void load_certs(const unsigned char *ca_cert,
		const unsigned char *client_cert,
		const unsigned char *client_key);

#endif  /* __WM_TLS_H__ */
