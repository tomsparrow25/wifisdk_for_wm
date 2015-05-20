#ifndef __SSL_COMMON_H__
#define __SSL_COMMON_H__

#include <wm_net.h>
#include <openssl/ssl.h>
#include <wmlog.h>

#define UNUSED_PARAM 0

int ssl_tcp_accept(int *sockfd, int *clientfd,
		   const char *server, uint16_t port);
void SetDH(SSL *ssl);
void tcp_set_nonblocking(int *sockfd);
int ssl_tcp_connect(int *sockfd, const char *ip, uint16_t port);
void showPeer(SSL *ssl);
void close_tcp_sockets(int *sock_fds, int count);
void close_ssl_connections(SSL **ssl, int count);
void free_ssl_connections(SSL **ssl, int count);

#ifdef USE_WINDOWS_API
#define CloseSocket(s) closesocket(s)
#define StartTCP() { WSADATA wsd; WSAStartup(0x0002, &wsd); }
#else
#define CloseSocket(s) net_close(s)
#define StartTCP()
#endif


#define tls_e(...)				\
	wmlog_e("tls", ##__VA_ARGS__)
#define tls_w(...)				\
	wmlog_w("tls", ##__VA_ARGS__)

#ifdef CONFIG_TLS_DEBUG
#define tls_d(...)				\
	wmlog("tls", ##__VA_ARGS__)
#else
#define tls_d(...)
#endif /* ! CONFIG_TLS_DEBUG */

#endif  /* __SSL_COMMON_H__ */
