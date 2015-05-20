/*
 *  Copyright 2008-2013, Marvell International Ltd.
 *  All Rights Reserved.
 */
/** httpd_handle_file.c: Functions to server files from FTFS
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <wm_os.h>

#include <httpd.h>
#include <http_parse.h>
#include <http-strings.h>
#include <wmstats.h>
#include <wmtime.h>
#include <ftfs.h>

#include "httpd_priv.h"

/*
 * Some notes about HTTP GET caching.
 *
 * There are two main HTTP response headers that can be used to control
 * caching behavior viz. "Expires" and "Cache-Control". Either of them can
 * be used.
 * Cacheable responses should also include a validator. Either an "ETag" OR
 * a "Last-Modified header". ETag is an opaque string token that the server
 * associates with the resource.
 */
#define MAX_ETAG_STR_LEN 8
/* All chars in the header except the main Etag value */
#define MAX_ETAG_HDR_LEN (MAX_ETAG_STR_LEN + 12)
#define NO_CACHE_HEADERS FALSE
#define SEND_CACHE_HEADERS TRUE
#define UNUSED_PARAM 0


static const char *ssi_include_file_str_p = "<!--#include file=";
static const char *ssi_include_virtual_str_p = "<!--#include virtual=";
static const char *closing_html_comment_str_p = "-->";

typedef enum ssi_directive_ {
	SSI_NONE = 0,
	SSI_INCLUDE_FILE,
	SSI_INCLUDE_VIRTUAL
} ssi_directive_t;

/* Return the type of SSI that this line contains. Every line within a
 * shtml file could potentialy be an SSI directive. An SSI directive
 * can either be a 'file' directive or a 'virtual' directive.
 *
 * The functions returns:
 *         SSI_NONE (cmd and args are invalid)
 *         SSI_INCLUDE_VIRTUAL (cmd is func name or NULL if none found,
 *                              args is arg list, maybe NULL)
 *         SSI_INCLUDE_FILE (cmd is the filename or NULL if none found,
 *                           args is invalid)
 */
static int httpd_is_script_directive(char *data_p, int len,
				     char **cmd, char **args)
{
	char *cptr = NULL;
	int ssi_include_file_strlen;
	int ssi_include_virtual_strlen;
	int closing_html_comment_strlen;
	int ssi = SSI_NONE;

	ssi_include_file_strlen = strlen(ssi_include_file_str_p);
	ssi_include_virtual_strlen = strlen(ssi_include_virtual_str_p);
	closing_html_comment_strlen = strlen(closing_html_comment_str_p);

	while (len) {
		if (strncmp(data_p, ssi_include_file_str_p,
			    ssi_include_file_strlen) == 0) {
			/* This is a file directive. */
			ssi = SSI_INCLUDE_FILE;
			cptr = data_p + ssi_include_file_strlen;
			len -= ssi_include_file_strlen;
			break;
		} else if (strncmp(data_p, ssi_include_virtual_str_p,
				   ssi_include_virtual_strlen) == 0) {
			/* This is a virtual directive. */
			ssi = SSI_INCLUDE_VIRTUAL;
			cptr = data_p + ssi_include_virtual_strlen;
			len -= ssi_include_virtual_strlen;
			break;
		}
		++data_p;
		--len;
	}
	/* Now if this is an ssi directive, ssi contains the directive
	 * type, while cptr points to the location after the
	 * directive */
	if (ssi == SSI_NONE)
		return ssi;

	*cmd = NULL;
	*args = NULL;

	/* Skip opening quote if any */
	if (*cptr == ISO_quot) {
		++cptr;
		--len;
	}

	/* Is the function/filename empty? */
	if (len == 0 || *cptr == ISO_quot || *cptr == '\n' ||
	    strncmp(cptr, closing_html_comment_str_p,
		    closing_html_comment_strlen) == 0) {
		return ssi;
	}

	/* Skip blank spaces */
	while (len && *cptr == ISO_space) {
		++cptr;
		--len;
	}

	if (len == 0 || *cptr == ISO_quot || *cptr == '\n' ||
	    strncmp(cptr, closing_html_comment_str_p,
		    closing_html_comment_strlen) == 0) {
		return ssi;
	}

	/* Okay.  Finally, we're pointing to the beginning of
	   something */
	*cmd = cptr;

	/* Skip until end of file or function name string */
	while (len && *cptr != ISO_quot && *cptr != '\n' && *cptr != ' ' &&
	       strncmp(cptr, closing_html_comment_str_p,
		       closing_html_comment_strlen) != 0) {
		++cptr;
		--len;
	}

	if (len == 0 || *cptr == '\n') {
		/* Got to the end before we found " or -->.  Abort. */
		*cmd = NULL;
		return ssi;
	}

	if (ssi == SSI_INCLUDE_FILE) {
		/* No need to proceed with arguments, just the cmd
		 * should be valid for the file */
		*cptr = 0;
		return ssi;
	}

	/* parse the arguments */

	/* Skip blank spaces, null terminating as we go */
	while (len && *cptr == ISO_space) {
		*cptr++ = 0;
		len--;
	}
	/* empty arg list? */
	if (len == 0 || *cptr == ISO_quot || *cptr == '\n' ||
	    strncmp(cptr, closing_html_comment_str_p,
		    closing_html_comment_strlen) == 0) {
		*cptr = 0;
		return ssi;
	}

	*args = cptr;

	/* Skip until end of argument string */
	while (len && *cptr != ISO_quot && *cptr != '\n' &&
	       strncmp(cptr, closing_html_comment_str_p,
		       closing_html_comment_strlen) != 0) {
		++cptr;
		--len;
	}

	/* null terminate the args */
	if (len == 0 || *cptr == '\n') {
		/* Got to the end before we found " or -->.  Abort. */
		*cmd = NULL;
		*args = NULL;
		return ssi;
	}
	*cptr = 0;
	return ssi;
}

/* Send the requested file, that has already been opened to the client
 * over the connection conn. We close the file and then return.
 */

#define SND_BUF_LEN 0x40
static int httpd_send_file(struct fs *fs, int conn,
			   const sys_file_t *sys_file_p)
{
	char data[SND_BUF_LEN];
	int len;
	int total = 0;
	int err = WM_SUCCESS;

	/* Chunk begin */
	err = httpd_send_chunk_begin(conn,
		     htsys_get_file_size(fs, sys_file_p));
	if (err != WM_SUCCESS)
		return err;

	/* Send the file */
	while (TRUE) {
		len = htsys_read(fs, sys_file_p,
				 (unsigned char *)data, SND_BUF_LEN);
		if (len <= 0) {
			if (len < 0)
				httpd_d("read failed\r\n");
			break;
		}
		err = httpd_send(conn, data, len);
		if (err != WM_SUCCESS) {
			httpd_d("Failed to send file (sent %d bytes)\r\n",
				total);
			break;
		}
		total += len;
	}

	htsys_close(fs, sys_file_p);

	if (err != WM_SUCCESS) {
		return err;
	}

	/* Chunk end */
	err = httpd_send_crlf(conn);

	return err;
}


/* Save stack space by not allocating argspace on the stack.  User may wish to
 * make this pretty big.
 */
static char script_args[HTTPD_MAX_SSI_ARGS + 1];
/* +1 for null termination */

/* If a script file (shtml), script_file_p is found, send that file to
 * the client over the socket, conn. Process any SSI directives within
 * the file.
 */
static int handle_script(struct fs *fs, httpd_request_t *req, int conn,
		  sys_file_t *script_file_p, char *data_p)
{
	char *cmd, *args;
	int len, err;
	int script_cmd;
	httpd_ssifunction func;
	SYS_FILE_DECL;

	err = WM_SUCCESS;

	/* proccess each line of the script */
	while ((len = htsys_getln(fs, script_file_p, data_p,
				  HTTPD_MAX_MESSAGE + 1)) != 0) {
		if (len < 0) {
			httpd_d("getln failed\r\n");
			err = -WM_FAIL;
			break;
		}

		/* Check if we should start executing a script. */
		script_cmd = httpd_is_script_directive(data_p, len,
						       &cmd, &args);
		if (script_cmd == SSI_INCLUDE_FILE) {
			/* Include another file and send to client */
			if (cmd == NULL)
				continue;

			httpd_d("Opening %s from script", cmd);
			err = htsys_file_open(fs, cmd, sys_filep);
			if (err != WM_SUCCESS) {
				httpd_d("open failed\r\n");
				break;
			}

			httpd_d("sending file %s (%d bytes).\r\n", cmd,
			      htsys_get_file_size(fs, sys_filep));
			err = httpd_send_file(fs, conn, sys_filep);
			if (err != WM_SUCCESS) {
				break;
			}
		} else if (script_cmd == SSI_INCLUDE_VIRTUAL) {
			/* find the function and run it */
			/* it should always at least return a nullfunction */
			if (cmd == NULL)
				continue;
			httpd_d("Invoking SSI %s from script", cmd);
			func = httpd_ssi(cmd);
			/* Pass args in their own buffer so we can use data_p as
			 * scratch */
			if (args == NULL) {
				err = func(req, NULL, conn, data_p);
			} else {
				strncpy(script_args, args, HTTPD_MAX_SSI_ARGS);
				script_args[HTTPD_MAX_SSI_ARGS] = 0;
				err = func(req, script_args, conn, data_p);
			}
		} else {
			/* send embedded html to client */
			err = httpd_send_chunk_begin(conn, len);
			if (err != WM_SUCCESS)
				break;

			err = httpd_send(conn, data_p, len);
			if (err != WM_SUCCESS)
				break;

			err = httpd_send_crlf(conn);
			if (err != WM_SUCCESS)
				break;

		}
	}
	return err;
}

static const char *httpd_get_etag_hdr(unsigned etag_val, int *len)
{
	static char http_etag_header[MAX_ETAG_HDR_LEN];
	*len = sprintf(http_etag_header, "ETag: \"%x\"\r\n", etag_val);
	return http_etag_header;
}

/* Send the required HTTP headers on the connection conn
 */
static int httpd_send_hdr_from_str(int conn, const char *statushdr,
				   const char *file_type, const char *encoding,
				   bool send_cache_headers, unsigned etag_val)
{
	int err;

	/* Send the HTTP status header, 200/404, etc.*/
	err = httpd_send(conn, statushdr, strlen(statushdr));
	if (err != WM_SUCCESS) {
		return err;
	}

	/* Set enconding to gz if so */
	if (strncmp(encoding, http_gz, sizeof(http_gz) - 1) == 0) {
		err = httpd_send(conn, http_content_encoding_gz,
				 sizeof(http_content_encoding_gz) - 1);
		if (err != WM_SUCCESS)
			return err;
	}

	if (send_cache_headers &&
	    strncmp(http_shtml, file_type, sizeof(http_shtml) - 1)) {
		err = httpd_send(conn, http_cache_control,
				 sizeof(http_cache_control) - 1);
		if (err != WM_SUCCESS)
			return err;

		int len;
		const char *etag_hdr = httpd_get_etag_hdr(etag_val, &len);
		err = httpd_send(conn, etag_hdr, len);
		if (err != WM_SUCCESS)
			return err;
	}
	if (file_type == NULL) {
		err = httpd_send(conn, http_content_type_binary,
				 sizeof(http_content_type_binary) - 1);
	} else if (strncmp(http_manifest, file_type,
			   sizeof(http_manifest) - 1) == 0) {
		err = httpd_send(conn,
				 http_content_type_text_cache_manifest,
				 sizeof(http_content_type_text_cache_manifest)
				 - 1);
	} else if (strncmp(http_html, file_type,
			   sizeof(http_html) - 1) == 0) {
		err = httpd_send(conn, http_content_type_html,
				 sizeof(http_content_type_html) - 1);
	} else if (strncmp(http_shtml, file_type,
			   sizeof(http_shtml) - 1) == 0) {
		err = httpd_send(conn, http_content_type_html_nocache,
				 sizeof(http_content_type_html_nocache) - 1);
	} else if (strncmp(http_css, file_type,
			   sizeof(http_css) - 1) == 0) {
		err = httpd_send(conn, http_content_type_css,
				 sizeof(http_content_type_css) - 1);
	} else if (strncmp(http_js, file_type,
			   sizeof(http_js) - 1) == 0) {
		err = httpd_send(conn, http_content_type_js,
				 sizeof(http_content_type_js) - 1);
	} else if (strncmp(http_png, file_type,
			   sizeof(http_png) - 1) == 0) {
		err = httpd_send(conn, http_content_type_png,
				 sizeof(http_content_type_png) - 1);
	} else if (strncmp(http_gif, file_type,
			   sizeof(http_gif) - 1) == 0) {
		err = httpd_send(conn, http_content_type_gif,
				 sizeof(http_content_type_gif) - 1);
	} else if (strncmp(http_jpg, file_type,
			   sizeof(http_jpg) - 1) == 0) {
		err = httpd_send(conn, http_content_type_jpg,
				 sizeof(http_content_type_jpg) - 1);
	} else {
		err = httpd_send(conn, http_content_type_plain,
				 sizeof(http_content_type_plain) - 1);
	}

	return err;
}

#define CONTENT_TYPE_PLAIN ".txt"
#define FILE_NOT_FOUND "404 File Not Found"
int httpd_handle_file(httpd_request_t *req_p, struct fs *fs)
{
	const char *anchor = "/";
	char tmp[HTTPD_MAX_URI_LENGTH+1];
	const char *encoding = NULL;
	char *msg_in, *ptr, *type = NULL;
	int err, msg_in_len = HTTPD_MAX_MESSAGE - 1, conn = req_p->sock;
	int ret;

	msg_in = os_mem_alloc(HTTPD_MAX_MESSAGE + 2);
	if (!msg_in) {
		httpd_e("Failed to allocate memory for file handling");
		/* Check what needs to be returned */
		return -WM_FAIL;
	}
	memset(msg_in, 0, HTTPD_MAX_MESSAGE + 2);

	SYS_FILE_DECL;

	if (!fs) {
		/* The filesystem wasn't mounted properly */
		httpd_send_hdr_from_str(conn, http_header_404,
					CONTENT_TYPE_PLAIN, encoding,
					NO_CACHE_HEADERS, UNUSED_PARAM);
		httpd_send_default_headers(conn,
				req_p->wsgi->hdr_fields);
		httpd_send_crlf(conn);
		httpd_send(conn, FILE_NOT_FOUND, sizeof(FILE_NOT_FOUND));
		ret = WM_SUCCESS;
		goto out;
	}
	struct ftfs_super *f_super = fs_to_ftfs_sb(fs);

	/* Ensure that the anchor is the first part of the filename */
	ptr = strstr(req_p->filename, anchor);
	if (ptr == NULL ||
	    ptr != req_p->filename) {
		httpd_d("No anchor in filename\r\n");
		ret = httpd_send_error(conn, HTTP_500);
		goto out;
	}

	/* Zap the anchor from the filename */
	ptr = req_p->filename + strlen(anchor);
	err = 0;
	/* anchors are only across directory boundaries */
	if (*ptr != '/')
		req_p->filename[err++] = '/';

	while (*ptr && (*ptr != '?')) {
		req_p->filename[err] = *ptr;
		ptr++;
		err++;
	}
	req_p->filename[err] = '\0';

	/* "/" implies a request for index.html */
	if (strncmp(req_p->filename, "/", 2) == 0) {
		strncpy(req_p->filename, http_index_html,
			sizeof(http_index_html));
	}

	/* Find if .gz version exists for file, if it is then serve that */
	type = strrchr(req_p->filename, ISO_period);
	if (type) {
		strncpy(tmp, req_p->filename, sizeof(tmp));
		strncat(tmp, ".gz", 3);
		httpd_d("Look for gz file if it exists: %s\r\n", tmp);
		if (htsys_file_open(fs, tmp, sys_filep) == WM_SUCCESS) {
			/* Gzipped version exists, serve that */
			httpd_d("Serve gzipped file\r\n");
			strncpy(req_p->filename, tmp, HTTPD_MAX_URI_LENGTH + 1);
			encoding = ".gz";
			htsys_close(fs, sys_filep);
		}
	}

	ret = httpd_parse_hdr_tags(req_p, conn, msg_in, msg_in_len);
	if (ret < 0) {
		httpd_d("Parsing headers failed \r\n");
		ret = httpd_send_error(conn, HTTP_500);
		goto out;
	}

	/* It is not a WSGI, check to see if the file exists */
	if (htsys_file_open(fs, req_p->filename, sys_filep) == -WM_FAIL) {
		httpd_w("file not found: %s\r\n", req_p->filename);

		ret = httpd_send_hdr_from_str(conn, http_header_404,
					      type, encoding, NO_CACHE_HEADERS,
					      UNUSED_PARAM);
		httpd_send_default_headers(conn,
				req_p->wsgi->hdr_fields);
		httpd_send_crlf(conn);
		if (ret != WM_SUCCESS)
			goto out;

		ret = htsys_file_open(fs, http_404_html, sys_filep);
		if (ret == WM_SUCCESS) {
			ret = httpd_send_file(fs, conn, sys_filep);
			goto out;
		} else
			httpd_w("No local 404 file.  Sending empty 404"
			      " response.\r\n");
	} else {
		/* Ok, the file exists, is it a script html or just html */
		g_wm_stats.wm_hd_file++;
		if (req_p->if_none_match &&
		    (f_super->fs_crc32 == req_p->etag_val)) {
			/* We do not need the file handle now */
			htsys_close(fs, sys_filep);

			/* Send Not Modified header */

			/* Send header prologue */
			ret = httpd_send(conn, http_header_304_prologue,
					 strlen(http_header_304_prologue));
			if (ret != WM_SUCCESS)
				goto out;
			httpd_send_header(conn, "Server", "Marvell-WM");
			httpd_send_header(conn, "Connection", "Close");

			/* Send ETag */
			int len;
			const char *h = httpd_get_etag_hdr(req_p->etag_val,
							   &len);
			ret = httpd_send(conn, h, len);
			if (ret != WM_SUCCESS)
				goto out;
			/* Close the header */
			ret = httpd_send_crlf(conn);
			goto out;
		} else  {
			ret = httpd_send_hdr_from_str(conn, http_header_200,
						      type, encoding,
						      SEND_CACHE_HEADERS,
						      f_super->fs_crc32);
			httpd_send_default_headers(conn,
					req_p->wsgi->hdr_fields);
			httpd_send_crlf(conn);

			if (ret != WM_SUCCESS)
				goto out;
			ptr = strchr(req_p->filename, ISO_period);
			if (ptr != NULL && strncmp(ptr, http_shtml,
						   sizeof(http_shtml)
						   - 1) == 0) {
				httpd_d("Handling script: %s", req_p->filename);
				ret = handle_script(fs, req_p, conn,
						    sys_filep, msg_in);
				htsys_close(fs, sys_filep);
				if (ret != WM_SUCCESS) {
					httpd_d("Script failed\r\n");
					goto out;
				}
			} else {
				ret = httpd_send_file(fs, conn, sys_filep);
				if (ret != WM_SUCCESS)
					goto out;
			}
		}
	}
	ret = httpd_send_last_chunk(conn);
out:
	os_mem_free(msg_in);
	return ret;
}
