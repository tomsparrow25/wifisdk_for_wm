#ifndef __TLS_DEMO_H__
#define __TLS_DEMO_H__

#include <appln_dbg.h>

#define DEBUG_TLS_DEMO

#ifdef DEBUG_TLS_DEMO
#define TLSD(...)				\
	do {					\
		wmprintf("TLSD: ");	\
		wmprintf(__VA_ARGS__);	\
		wmprintf("\n\r");		\
	} while (0);
#else
#define TLSD(...)
#endif /* CONFIG_HTTPCLIENT_DEBUG */


#define TLS_DEMO_THREAD_STACK_SIZE 8192

int tls_demo_init(void);
void tls_httpc_client(char *url);

#endif /* __TLS_DEMO_H__ */
