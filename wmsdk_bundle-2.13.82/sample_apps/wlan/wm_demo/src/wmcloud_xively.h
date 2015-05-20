/*
 * Copyright (C) 2008-2013, Marvell International Ltd.
 * All Rights Reserved.
 */

#ifndef _WMCLOUD_XIVELY_H_
#define _WMCLOUD_XIVELY_H_

#include <wmcloud.h>
#include <libxively/xively.h>
#include <libxively/xi_err.h>

#define XIVELY_SERIAL_NO_MAX 13
#define CLOUD_FEED_ID_LEN 20
#define CLOUD_API_KEY_LEN 60

#define VAR_CLOUD_INTERVAL "interval"   /* cloud.interval */
#define VAR_CLOUD_FEED_ID  "feed_id"    /* cloud.feed_id */
#define VAR_CLOUD_API_KEY  "api_key"    /* cloud.api_key */

/** Structure for Xively cloud parameters */
struct xively_cloud {
	unsigned probe_interval_secs;
	int feed_id;
	char api_key[CLOUD_API_KEY_LEN];
	char serial_no[XIVELY_SERIAL_NO_MAX];
	char *product_secret;
	unsigned int product_secret_len;
	int (*cloud_handler) ();
};


/* Cloud's Exported Functions */

/** Start communication with Xively Cloud
 *
 *  This function starts the communication with Xively Cloud
 *  \param[in] dev_class The type of device like power_switch
 *  \param[in] serial_no The serial number of the device that is pre registered
 *  with Xively Cloud. This is used as the data in HMAC SHA1 algorithm
 *  \param[in] product_secret The product secret obtained from Xively Cloud. It
 *  is used as the key in the HMAC SHA1 algorithm
 *  \param[in] cloud_handler The function pointer to a function that will get
 *  called during every cloud post
 *  \return WM_SUCCESS if successful
 *  \return -WM_FAIL in case of an error
 *
 *  \note Details for Xively Provisioning can be found at
 *  https://xively.com/dev/docs/api/product_management/provisioning/
 *
 */
int xively_cloud_start(const char *dev_class, char *serial_no,
		char *product_secret, unsigned int product_secret_len,
		int (*cloud_handler)(xi_context_t *xi));

/** Stop communication with Xively Cloud
 *
 *  This function is used to terminate the communication with Xively Cloud
 *  \return WM_SUCCESS if successful
 *  \return -WM_FAIL in case of an error
 */
int xively_cloud_stop();

#endif
