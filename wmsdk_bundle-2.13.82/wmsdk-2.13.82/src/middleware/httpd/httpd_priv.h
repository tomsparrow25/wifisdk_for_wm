#ifndef __HTTPD_PRIV_H__
#define __HTTPD_PRIV_H__

#include <wmstdio.h>
#include <fs.h>
#include <httpd.h>

#include <wmlog.h>

#define httpd_e(...)				\
	wmlog_e("httpd", ##__VA_ARGS__)
#define httpd_w(...)				\
	wmlog_w("httpd", ##__VA_ARGS__)

#ifdef CONFIG_HTTPD_DEBUG
#define httpd_d(...)				\
	wmlog("httpd", ##__VA_ARGS__)
#else
#define httpd_d(...)
#endif /* ! CONFIG_HTTPD_DEBUG */

#if !defined(CONFIG_ENABLE_HTTPS) && !defined(CONFIG_ENABLE_HTTP)
#error Atleast one of HTTP and HTTPS needs to be enabled.
#endif  /* ! ENABLE_HTTPS && ! ENABLE_HTTP */


int httpd_test_setup(void);
int httpd_wsgi_init(void);

/** Set the httpd-wide error message
 *
 * Often, when something fails, a 500 Internal Server Error will be emitted.
 * This is especially true when the reason is something arbitrary, like the
 * maximum header line that the httpd can handle is exceeded.  When this
 * happens, a function can set the error string to be passed back to the user.
 * This facilitates debugging.  Note that the error message will also be
 * httpd_d'd, so no need to add extra lines of code for that.
 *
 * Note that this function is not re-entrant.
 *
 * Note that at most HTTPD_MAX_ERROR_STRING characters will be stored.
 *
 * Note: no need to have a \r\n on the end of the error message.
 */
#define HTTPD_MAX_ERROR_STRING 256
void httpd_set_error(const char *fmt, ...);

typedef struct sys_file_s {
	file *f;
} sys_file_t;

#define SYS_FILE_DECL sys_file_t sys_file, *sys_filep; sys_filep = &sys_file

int handle_message(char *msg_in, int msg_in_len, int conn);
int httpd_parse_hdr_main(const char *data_p, httpd_request_t *req_p);
int httpd_handle_message(int conn);

/* Various Defines */
#define TRUE 1
#define FALSE 0
#ifndef NULL
#define NULL 0
#endif

unsigned int htsys_get_file_size(struct fs *fs, const sys_file_t *fd_p);
int htsys_file_open(struct fs *fs, const char *filename, sys_file_t * fd_p);
int htsys_read(struct fs *fs, const sys_file_t *sys_file_p,
	       unsigned char *datap, int len);
void htsys_close(struct fs *fs, const sys_file_t *sys_file_p);
int htsys_getln(struct fs *fs, const sys_file_t *fd_p, char *data_p,
		int buflen);

int httpd_wsgi(httpd_request_t *req_p);

httpd_ssifunction httpd_ssi(char *);
int httpd_ssi_init(void);
int htsys_getln_soc(int sd, char *data_p, int buflen);

void httpd_parse_useragent(char *hdrline, httpd_useragent_t *agent);

int httpd_send_last_chunk(int conn);

enum {
	HTTP_404,
	HTTP_500,
	HTTP_505,
};

int httpd_send_error(int conn, int http_error);

bool httpd_is_https_active();
#endif				/* __HTTPD_PRIV_H__ */
