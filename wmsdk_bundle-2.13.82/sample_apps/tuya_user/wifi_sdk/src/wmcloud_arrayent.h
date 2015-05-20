/*
 * Copyright (C) 2008-2014, Marvell International Ltd.
 * All Rights Reserved.
 */

#ifndef _WMCLOUD_ARRAYENT_H_
#define _WMCLOUD_ARRAYENT_H_

#include <wmcloud.h>
#include <arrayent/aca.h>


#define VAR_CLOUD_INTERVAL "interval"   /* cloud.interval */


#define LOAD_BALANCER_PORT  80

#define AES_KEY_LEN 129
#define PRODUCT_ID_LEN  6
#define DEVICE_NAME_LEN 11
#define DEVICE_PWD_LEN 4
#define MAX_URL_LEN 200


/** Structure for arrayent cloud parameters */
struct arrayent_cloud {
	unsigned probe_interval_secs;
	uint16_t product_id;
	char product_aes[AES_KEY_LEN];
	char device_name[DEVICE_NAME_LEN];
	char device_password[DEVICE_PWD_LEN];
	char device_aes[AES_KEY_LEN];
	char load_balancer_one[MAX_URL_LEN];
	char load_balancer_two[MAX_URL_LEN];
	char load_balancer_three[MAX_URL_LEN];
	int (*cloud_handler) ();
};



/* Cloud's Exported Functions */

/** Start communication with Arrayent Cloud
 *
 *  This function starts the communication with Arrayent Cloud
 *  \param[in] dev_class The type of device like power_switch
 *  \param[in] pcloud Pointer to arrayent configuration structure using which
 *  the cloud will be started
 *  \return WM_SUCCESS if successful
 *  \return -WM_FAIL in case of an error
 *
 */
int arrayent_cloud_start(const char *dev_class,
			 struct arrayent_cloud *pcloud);

/** Stop communication with Arrayent Cloud
 *
 *  This function is used to terminate the communication with Arrayent Cloud
 *  \return WM_SUCCESS if successful
 *  \return -WM_FAIL in case of an error
 */
int arrayent_cloud_stop();

#endif /* _WMCLOUD_ARRAYENT_H_ */
