/*! \file rfget.h
 *  \brief Remote Firmware Upgrade
 *
 * This module provides APIs for getting an upgrade image over the network
 * and updating on-flash firmware image. WMSDK supports redundant image
 * based firmware upgrades whereby there are two partitions in the flash to
 * hold the firmware; active and passive. Firmware upgrade process always
 * writes new image on passive partition and finally modifies partition
 * table to mark successful newer image write. This then results into boot
 * from new firmware image when device reboots.
 *
 * This module provides two methods for delivery of the image:
 * \li Server mode upgrade: In this method rfget module receives the
 * image data through an already opened socket connection with a peer. This
 * mode is agnostic to image delivery protocol.  This is covered by
 * \ref rfget_server_mode_update API.
 * \li Client mode upgrade: In this method wireless microcontroller device
 * acts as a HTTP client to receive the image from other web server using
 * specified URL. \ref rfget_client_mode_update API covers this functionality.
 *
 * In both the cases the image is written to flash using following
 * process:
 * -# Receive firmware image header and verify signature
 * -# Identify passive partition using partition table and erase it
 * -# Receive image data and write it as received in parts
 * -# Update partition table
 *
 * As of now these functions support the filesystem and firmware upgrades. If
 * an application specific component should be upgraded, the lower level API,
 * rfget_update_begin(), rfget_update_data() and rfget_update_complete() can be
 * used.
 *
 * Carrying out subsequent reboot is not handled by any of the APIs and is left
 * to the application.
 */

/*  Copyright 2008-2013, Marvell International Ltd.
 *  All Rights Reserved.
 */

#ifndef _RFGET_H_
#define _RFGET_H_

#include <wmtypes.h>
#include <wmerrno.h>
#include <wmstdio.h>
#include <mdev.h>
#include <flash.h>
#include <wmlog.h>
#include <ftfs.h>
#include <partition.h>

#define rf_e(...)				\
	wmlog_e("rf", ##__VA_ARGS__)
#define rf_w(...)				\
	wmlog_w("rf", ##__VA_ARGS__)

#ifdef CONFIG_RF_DEBUG
#define rf_d(...)				\
	wmlog("rf", ##__VA_ARGS__)
#else
#define rf_d(...)
#endif /* ! CONFIG_RF_DEBUG */

#define ftfs_l(...) wmlog("ftfs", ##__VA_ARGS__)
/*
 * Callback function pointer passed to the firmware/fs update
 * function. This is needed because the firmware/fs update file may be too 
 * big to fit in a buffer at a time. The update function needs to callback
 * rfget again and again to get next blocks of update file.
 */
typedef size_t(*data_fetch) (void *priv, void *buf, size_t len);

#define RFGET_PROTO_DEFAULT RFGET_HTTP_GET
/** RFGET Error Code */
enum wm_rfget_code {
	WM_RFGET_BASE = MOD_ERROR_START(MOD_RFGET),
	/** Protocol Not Supported */
	WM_E_RFGET_INVPRO,
	/** Failed to set 2msl time */
	WM_E_RFGET_F2MSL,
	/** Failed to send/recv http request */
	WM_E_RFGET_FHTTP_REQ,
	/** Failed to parse http header */
	WM_E_RFGET_FHTTP_PARSE,
	/** Invalid http status */
	WM_E_RFGET_INVHTTP_STATUS,
	/**  Error in rfget_sock_init */
	WM_E_RFGET_FSOCK,
	/** rfget length unknown or invalid*/
	WM_E_RFGET_INVLEN,
	/** Failed to init rfget*/
	WM_E_RFGET_FINIT,
	/** Firmware or ftfs Update failed */
	WM_E_RFGET_FUPDATE,
	/** Firmware or ftfs read fail */
	WM_E_RFGET_FRD,
	/** Firmware or ftfs write fail */
	WM_E_RFGET_FWR,
	/** Failed to load firmware */
	WM_E_RFGET_FFW_LOAD,
	/** Invalid file format */
	WM_E_RFGET_INV_IMG,
};


/**
 * Update a system image in server mode through http server.
 *
 * This function updates a system image. i.e. a firmware or filesystem
 * image. This function reads the file from the server side socket 'sockfd'
 *
 * @param[in] sockfd Server side socket file descriptor. This function will
 * read 'filelength' bytes from this socket and write it to appropriate
 * location depending on the 'type'
 * @param[in] filelength Number of bytes to be read from socket and written
 * to flash.
 * @param[in] passive the passive partition that should be upgraded.
 *
 * @return On success returns WM_SUCCESS. On error returns appropriate
 * error code.
 * \note In case of getting the file from HTTP server, this API should be
 * called only after stripping off HTTP header from received packets. The
 * sockfd should contain the data starting with image header.
 */
int rfget_server_mode_update(int sockfd, size_t filelength,
			     struct partition_entry *passive);

/**
 * Update a system image in client mode using http client.
 *
 * This function updates a system image. for e.g. a firmware
 * image. This function reads the resource given as a part of the URL.
 * 
 * @param[in] url_str URL to the image.
 * @param[in] passive the passive partition that should be upgraded.
 *
 * @return On success returns WM_SUCCESS. On error returns appropriate
 * error code.
 */
int rfget_client_mode_update(const char *url_str,
	struct partition_entry *passive);

/**
 * This function registers the CLI for the rfget application.
 *
 * @return On success returns WM_SUCCESS. On error returns appropriate
 * error code.
 */
int rfget_cli_init(void);
/**
 * This function tells whether system image update is in progress.
 *
 * @return If update is in progress returns TRUE else returns FALSE.
 */
bool rfget_is_update_in_progress(void);

/**
 * This function purges the stream bytes.
 *
 * @param[in] data_fetch_cb Function pointer callback for data retrieval.
 * @param[in] priv Pointer returned as a parameter to data_fetch_cb
 * @param[in] length length of data in bytes to purge from the stream.
 * @param[in] scratch_buf Pointer to buffer used internally by this function.
 * @param[in] scratch_buflen Length of the scratch_buf
 *
 */
void purge_stream_bytes(data_fetch data_fetch_cb, void *priv,
			size_t length, unsigned char *scratch_buf,
			const int scratch_buflen);

int rfget_init();

/** Initialize Upgradeable FTFS
 *
 * If you wish to use upgradeable FTFS please you should use this function. If
 * you have FTFS partitions that do not required upgradability please use
 * ftfs_init() instead.
 *
 * Given the backend API version this function will mount the appropriate FTFS
 * from the active-passive pair. The FTFS with the highest generation no. that
 * matches the be_ver is mounted.
 *
 * \param[out] sb pointer to a ftfs_super structure
 * \param[in] be_ver backend API version
 * \param[in] name name of the FTFS component in the layout
 * \param[out] passive a pointer to the passive component
 *
 * \return a pointer to struct fs. The pointer points to an entry within the
 * 'sb' parameter that is passed as an argument.
 * \return NULL in case of an error
 *
 */
struct fs *rfget_ftfs_init(struct ftfs_super *sb, int be_ver, const char *name,
			   struct partition_entry **passive);

/** Update Description Structure
 *
 * This is referenced when using the rfget_update_ APIs. The contents of this
 * structure are populated and used by the rfget_update_ APIs.
 */
typedef struct update_desc {
	/** Pointer to partition entry structure
	 *  this is the partition being updated
	 */
	struct partition_entry *pe;
	/** MDEV handle to flash driver */
	mdev_t *dev;
	/** Address in flash where update occurs */
	uint32_t addr;
	/** Remaining space in partition being updated */
	uint32_t remaining;
} update_desc_t;

/** Begin an update
 *
 * This function erases the associated flash sector for the partition entry that
 * is passed to it. The ud is used to maintain information across the various
 * calls to the rfget_update_ functions.
 *
 * \param[out] ud the update descriptor structure
 * \param[in] p the partition entry of the passive partition
 * \return WM_SUCCESS on success, error otherwise
 */
int rfget_update_begin(update_desc_t *ud, struct partition_entry *p);

/** Write data to partition
 *
 * This should be called after a call to the rfget_update_begin(). Successive
 * calls to this function write data to the flash.
 *
 * \param[in] ud the update descriptor
 * \param[in] data the data to be written
 * \param[in] datalen the length of the data to be written
 * \return WM_SUCCESS on success, error otherwise
 */
int rfget_update_data(update_desc_t *ud, const char *data, size_t datalen);

/** Finish update
 *
 * Mark this partition as the active partition and finish the update process.
 * \param[in] ud the update descriptor
 * \return WM_SUCCESS on success, error otherwise
 */
int rfget_update_complete(update_desc_t *ud);

/* Convenience function to get the passive firmware partition */
struct partition_entry *rfget_get_passive_firmware(void);
/* Convenience function to get the passive wifi firmware partition */
struct partition_entry *rfget_get_passive_wifi_firmware(void);


#endif
