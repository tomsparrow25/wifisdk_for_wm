#include <ctype.h>

#include <wmstdio.h>

#include <lwip/sockets.h>
#include <wm_net.h>
#include <lwip/inet.h>
#include <lwip/netdb.h>

#include "tls_common.h"

#define USE_ANY_ADDR

void close_tcp_sockets(int *sock_fds, int count)
{
	int i;
	for (i = 0; i < count; i++) {
		CloseSocket(sock_fds[i]);
		sock_fds[i] = -1;
	}
}

void close_ssl_connections(SSL **ssl, int count)
{
	int i;
	for (i = 0; i < count; i++)
		if (ssl[i])
			SSL_shutdown(ssl[i]);

	free_ssl_connections(ssl, count);
}

void free_ssl_connections(SSL **ssl, int count)
{
	int i;
	for (i = 0; i < count; i++) {
		if (ssl[i])
			SSL_free(ssl[i]);
		ssl[i] = NULL;
	}
}

void tcp_set_nonblocking(int *sockfd)
{
#ifdef NON_BLOCKING
	int flags = fcntl(*sockfd, F_GETFL, 0);
	fcntl(*sockfd, F_SETFL, flags | O_NONBLOCK);
#endif
}

static int tcp_socket(int *sockfd, struct sockaddr_in *addr,
		      const char *peer, uint16_t port)
{
	if (peer != INADDR_ANY)
		tls_d("Creating socket: %s:%d", peer, port);
	else
		tls_d("Creating socket: INADDR_ANY:%d", port);

	const char *host = peer;

	/* peer could be in human readable form */
	if ((peer != INADDR_ANY) && peer != INADDR_ANY && isalpha(peer[0])) {
		struct hostent *entry = gethostbyname(peer);

		if (entry) {
			struct sockaddr_in tmp;
			memset(&tmp, 0, sizeof(struct sockaddr_in));
			memcpy(&tmp.sin_addr.s_addr, entry->h_addr_list[0],
			       entry->h_length);
			host = inet_ntoa(tmp.sin_addr);
		} else {
			tls_e("SSL: no entry for host %s found\n\r",
				       peer);
			return -WM_FAIL;
		}
	}

	*sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (*sockfd < 0) {
		tls_e("SSL: Socket creation failed");
		return *sockfd;
	}

	memset(addr, 0, sizeof(struct sockaddr_in));

#ifndef TEST_IPV6
	addr->sin_family = AF_INET;
	addr->sin_port = htons(port);
	if (host == INADDR_ANY)
		addr->sin_addr.s_addr = INADDR_ANY;
	else
		addr->sin_addr.s_addr = inet_addr(host);
#else
	addr->sin6_family = AF_INET;
	addr->sin6_port = htons(port);
	addr->sin6_addr = in6addr_loopback;
#endif

#ifdef SO_NOSIGPIPE
	{
		int on = 1;
		socklen_t len = sizeof(on);
		int res =
			setsockopt(*sockfd, SOL_SOCKET, SO_NOSIGPIPE, &on, len);
		if (res < 0)
			err_sys("setsockopt SO_NOSIGPIPE failed\n");
	}
#endif

#if defined(TCP_NODELAY) && !defined(CYASSL_DTLS)
	{
		int on = 1;
		socklen_t len = sizeof(on);
		int res =
			setsockopt(*sockfd, IPPROTO_TCP, TCP_NODELAY, &on, len);
		if (res < 0) {
			tls_e("setsockopt TCP_NODELAY failed");
			return res;
		}
	}
#endif
	return WM_SUCCESS;
}

int ssl_tcp_connect(int *sockfd, const char *ip, uint16_t port)
{
	struct sockaddr_in addr;
	int status = tcp_socket(sockfd, &addr, ip, port);
	if (status != 0) {
		tls_e("Socket creation for %s:%d failed",
			       ip, port);
		return status;
	}

	tls_d("Connecting .. %s : %d", ip, port);
	if (connect(*sockfd, (const struct sockaddr *)&addr,
		    sizeof(addr)) != 0) {
		tls_e("tcp connect failed");
		return -WM_FAIL;
	}
	return WM_SUCCESS;
}

void showPeer(SSL *ssl)
{
#ifdef OPENSSL_EXTRA

	SSL_CIPHER *cipher;
	X509 *peer = SSL_get_peer_certificate(ssl);
	if (peer) {
		char *issuer =
			X509_NAME_oneline(X509_get_issuer_name(peer), 0, 0);
		char *subject =
			X509_NAME_oneline(X509_get_subject_name(peer), 0, 0);
		byte serial[32];
		int ret;
		int sz = sizeof(serial);

		printf("peer's cert info:\n issuer : %s\n subject: %s\n",
		       issuer, subject);
		ret = CyaSSL_X509_get_serial_number(peer, serial, &sz);
		if (ret == 0) {
			int i;
			int strLen;
			char serialMsg[80];

			/* testsuite has multiple threads writing to
			   stdout, get output message ready to write once */
			strLen = sprintf(serialMsg, " serial number");
			for (i = 0; i < sz; i++)
				sprintf(serialMsg + strLen + (i * 3), ":%02x ",
					serial[i]);
			printf("%s\n", serialMsg);
		}

		XFREE(subject, 0, DYNAMIC_TYPE_OPENSSL);
		XFREE(issuer, 0, DYNAMIC_TYPE_OPENSSL);
	} else
		printf("peer has no cert!\n");
	printf("SSL version is %s\n", SSL_get_version(ssl));

	cipher = SSL_get_current_cipher(ssl);
	printf("SSL cipher suite is %s\n", SSL_CIPHER_get_name(cipher));
#endif

#ifdef SESSION_CERTS
	{
		X509_CHAIN *chain = CyaSSL_get_peer_chain(ssl);
		int count = CyaSSL_get_chain_count(chain);
		int i;

		for (i = 0; i < count; i++) {
			int length;
			unsigned char buffer[3072];

			CyaSSL_get_chain_cert_pem(chain, i, buffer,
						  sizeof(buffer), &length);
			buffer[length] = 0;
			printf("cert %d has length %d data = %s\n", i, length,
			       buffer);
		}
	}
#endif

}

#if 0
void ssl_udp_accept(int *sockfd, int *clientfd, func_args *args)
{
	SOCKADDR_IN_T addr;

	tcp_socket(sockfd, &addr, yasslIP, yasslPort);

#ifndef USE_WINDOWS_API
	{
		int on = 1;
		socklen_t len = sizeof(on);
		setsockopt(*sockfd, SOL_SOCKET, SO_REUSEADDR, &on, len);
	}
#endif

	if (bind(*sockfd, (const struct sockaddr *)&addr, sizeof(addr)) != 0)
		err_sys("tcp bind failed");

#if defined(_POSIX_THREADS) && defined(NO_MAIN_DRIVER)
	/* signal ready to accept data */
	{
		tcp_ready *ready = args->signal;
		pthread_mutex_lock(&ready->mutex);
		ready->ready = 1;
		pthread_cond_signal(&ready->cond);
		pthread_mutex_unlock(&ready->mutex);
	}
#endif

	*clientfd = udp_read_connect(*sockfd);
}
#endif /* 0 */

int tcp_listen(int *sockfd, const char *server, uint16_t port)
{
	struct sockaddr_in addr;
	int ret;

	/* don't use INADDR_ANY by default, firewall may block, make user switch
	   on */
#ifdef USE_ANY_ADDR
	ret = tcp_socket(sockfd, &addr, INADDR_ANY, port);
#else
	ret = tcp_socket(sockfd, &addr, server, port);
#endif
	if (ret != 0) {
		tls_e("Socket could be created: %d", ret);
		return ret;
	}

	int on = 1;
	socklen_t len = sizeof(on);
	ret = setsockopt(*sockfd, SOL_SOCKET, SO_REUSEADDR, &on, len);
	if (ret) {
		tls_e("setsockopt failed: %d", ret);
		CloseSocket(*sockfd);
		return ret;
	}

	ret = bind(*sockfd, (const struct sockaddr *)&addr, sizeof(addr));
	if (ret) {
		tls_e("tcp bind failed");
		CloseSocket(*sockfd);
		return ret;
	}
#ifndef CYASSL_DTLS
	ret = listen(*sockfd, 5);
	if (ret) {
		tls_e("tcp listen failed: %d", ret);
		return ret;
	}
#endif
	return WM_SUCCESS;
}

int ssl_tcp_accept(int *sockfd, int *clientfd, const char *server,
		   uint16_t port)
{
	struct sockaddr_in client;
	socklen_t client_len = sizeof(client);

	tcp_listen(sockfd, server, port);

	tls_d("Calling accept on main fd  ...");
	*clientfd = accept(*sockfd, (struct sockaddr *)&client, &client_len);
	if (*clientfd == -1) {
		tls_e("tcp accept failed");
		return *clientfd;
	}

	return WM_SUCCESS;
}


void SetDH(SSL *ssl)
{
	/* dh1024 p */
	static unsigned char p[] = {
		0xE6, 0x96, 0x9D, 0x3D, 0x49, 0x5B, 0xE3, 0x2C, 0x7C, 0xF1,
		0x80, 0xC3, 0xBD, 0xD4, 0x79, 0x8E, 0x91, 0xB7, 0x81, 0x82,
		0x51, 0xBB, 0x05, 0x5E, 0x2A, 0x20, 0x64, 0x90, 0x4A, 0x79,
		0xA7, 0x70, 0xFA, 0x15, 0xA2, 0x59, 0xCB, 0xD5, 0x23, 0xA6,
		0xA6, 0xEF, 0x09, 0xC4, 0x30, 0x48, 0xD5, 0xA2, 0x2F, 0x97,
		0x1F, 0x3C, 0x20, 0x12, 0x9B, 0x48, 0x00, 0x0E, 0x6E, 0xDD,
		0x06, 0x1C, 0xBC, 0x05, 0x3E, 0x37, 0x1D, 0x79, 0x4E, 0x53,
		0x27, 0xDF, 0x61, 0x1E, 0xBB, 0xBE, 0x1B, 0xAC, 0x9B, 0x5C,
		0x60, 0x44, 0xCF, 0x02, 0x3D, 0x76, 0xE0, 0x5E, 0xEA, 0x9B,
		0xAD, 0x99, 0x1B, 0x13, 0xA6, 0x3C, 0x97, 0x4E, 0x9E, 0xF1,
		0x83, 0x9E, 0xB5, 0xDB, 0x12, 0x51, 0x36, 0xF7, 0x26, 0x2E,
		0x56, 0xA8, 0x87, 0x15, 0x38, 0xDF, 0xD8, 0x23, 0xC6, 0x50,
		0x50, 0x85, 0xE2, 0x1F, 0x0D, 0xD5, 0xC8, 0x6B,
	};

	/* dh1024 g */
	static unsigned char g[] = {
		0x02,
	};

	CyaSSL_SetTmpDH(ssl, p, sizeof(p), g, sizeof(g));
}
