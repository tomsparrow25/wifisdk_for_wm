/*
 *  Copyright 2008-2013, Marvell International Ltd.
 *  All Rights Reserved.
 */

/* httpd_sys.c: System abstraction layer for the HTTP server. The abstraction
 * provides independence from the details of the filesystem used. The WMSDK
 * currently support the FTFS filesystem type.
 */

#include <wm_net.h>
#include <wmassert.h>
#include <httpd.h>
#include <http-strings.h>

#include "httpd_priv.h"

/* Retrieve the file size of the file */
unsigned int htsys_get_file_size(struct fs *fs, const sys_file_t *fd_p)
{
	unsigned int len;
	long offset = fs->ftell(fd_p->f);
	fs->fseek(fd_p->f, 0, SEEK_END);
	len = fs->ftell(fd_p->f);
	fs->fseek(fd_p->f, offset, SEEK_SET);
	return len;
}

/* Open a file */
int htsys_file_open(struct fs *fs, const char *filename, sys_file_t *fd_p)
{
	ASSERT(fs != NULL);
	ASSERT(filename != NULL);
	ASSERT(fd_p != NULL);

	if (fs)
		fd_p->f = fs->fopen(fs, filename, "r");
	else
		fd_p->f = NULL;

	return fd_p->f == NULL ? -WM_FAIL : WM_SUCCESS;
}

/* Read data from a file into the buffer */
int htsys_read(struct fs *fs, const sys_file_t *sys_file_p,
	       unsigned char *datap, int len)
{
	return fs->fread(datap, len, 1, sys_file_p->f);
}

/* Close the file */
void htsys_close(struct fs *fs, const sys_file_t *sys_file_p)
{
	fs->fclose(sys_file_p->f);
}

/* Note: This getln will return an entire line, including '\n'
   If line termination was DOS-style (\r\n), the \r is removed
 */
int htsys_getln(struct fs *fs, const sys_file_t *fd_p,
		char *data_p, int buflen)
{
	int len = 0;
	char *c_p;
	int result;

	ASSERT(data_p != NULL);
	c_p = data_p;

	while ((result = fs->fread(c_p, 1, 1, fd_p->f)) != 0) {
		if (result < 0) {
			*c_p = 0;
			httpd_e("Read failed len is %d", len);
			return -WM_FAIL;
		}

		if ((result == 0) || (*c_p == 0))
			break;

		len += result;

		if (*c_p == ISO_nl) {
			if (*(c_p - 1) == ISO_cr) {
				--c_p;
				*c_p = ISO_nl;
				len--;
			}
			c_p++;
			break;
		}

		if (len == buflen)
			break;

		c_p++;
	}

	*c_p = 0;
	return len;
}

int htsys_getln_soc(int sd, char *data_p, int buflen)
{
	int len = 0;
	char *c_p;
	int result;
	int serr;

	ASSERT(data_p != NULL);
	c_p = data_p;

	/* Read one byte at a time */
	while ((result = httpd_recv(sd, c_p, 1, 0)) != 0) {
		/* error on recv */
		if (result == -1) {
			*c_p = 0;
			serr = net_get_sock_error(sd);
			if ((serr == -WM_E_NOMEM) || (serr == -WM_E_AGAIN))
				continue;
			httpd_e("recv failed len: %d, err: 0x%x", len, serr);
			return -WM_FAIL;
		}

		/* If new line... */
		if ((*c_p == ISO_nl) || (*c_p == ISO_cr)) {
			result = httpd_recv(sd, c_p, 1, 0);
			if ((*c_p != ISO_nl) && (*c_p != ISO_cr)) {
				httpd_e("should get double CR LF: %d, %d",
				      (int)*c_p, result);
			}
			break;
		}
		len++;
		c_p++;

		/* give up here since we'll at least need 3 more chars to
		 * finish off the line */
		if (len >= buflen - 1) {
			httpd_e("buf full: recv didn't read complete line.");
			break;
		}
	}

	*c_p = 0;
	return len;
}
