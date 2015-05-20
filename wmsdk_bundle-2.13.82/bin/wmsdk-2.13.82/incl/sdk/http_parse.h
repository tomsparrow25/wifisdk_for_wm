/*! \file http_parse.h
 *  \brief Common HTTP functions
 *
 *  Copyright 2008-2013, Marvell International Ltd.
 *  All Rights Reserved.
 */
#ifndef __HTTP_PARSE_H__
#define __HTTP_PARSE_H__

#include <httpd.h>


/** Parse tag/value form elements present in HTTP POST body
 *
 * Given a tag this function will retrieve its value from the buffer and return
 * it to the caller.
 * \param[in] inbuf Pointer to NULL-terminated buffer that holds POST data
 * \param[in] tag The tag to look for
 * \param[out] val Buffer where the value will be copied to
 * \param[in] val_len The length of the val buffer
 *
 *
 * \return WM_SUCCESS when a valid tag is found, error otherwise
 */
int httpd_get_tag_from_post_data(char *inbuf, const char *tag,
			  char *val, unsigned val_len);

/** Parse tag/value form elements present in HTTP GET URL
 *
 * Given a tag this function will retrieve its value from the HTTP URL and
 * return it to the caller.
 * \param[in] inbuf Pointer to NULL-terminated buffer that holds POST data
 * \param[in] tag The tag to look for
 * \param[out] val Buffer where the value will be copied to
 * \param[in] val_len The length of the val buffer
 *
 * \return WM_SUCCESS when a valid tag is found, error otherwise
 */
int httpd_get_tag_from_url(httpd_request_t *req_p,
			const char *tag,
			char *val, unsigned val_len);

int htsys_getln_soc(int sd, char *data_p, int buflen);

#endif /* __HTTP_PARSE_H__ */
