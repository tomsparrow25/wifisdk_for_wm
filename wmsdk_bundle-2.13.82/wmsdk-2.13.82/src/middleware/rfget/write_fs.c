/*
 * Copyright (C) 2008-2012, Marvell International Ltd.
 * All Rights Reserved.
 */

/** write_fs.c: Write filesystem images to flash
 */
#include <string.h>

#include <wmstdio.h>
#include <rfget.h>
#include <ftfs.h>
#include <wm_os.h>
#include <flash.h>
#include <crc32.h>
#include <partition.h>

#include "firmware.h"


#ifndef CONFIG_SOFTWARE_CRC32
#include <mdev_crc.h>
#else

#endif

#define FS_WRITE_BUFSIZE 256

static unsigned int recv_crc;
static FT_HEADER ftfs_header;

int read_ftfs_headers(void *priv, data_fetch data_fetch_cb)
{
	int ret;
	char magic[] = FT_MAGIC;

	/* Read the FTFS headers */
	ret = data_fetch_cb(priv, (char *)&ftfs_header, sizeof(ftfs_header));
	if (ret < sizeof(ftfs_header)) {
		rf_e("Cannot read FTFS header");
		return -WM_E_RFGET_FRD;
	}

	/* Incorrect magic in the image */
	if (memcmp((void *)ftfs_header.magic, magic, sizeof(magic))) {
		rf_e("Not a valid filesystem image");
		return -WM_E_RFGET_INV_IMG;
	}

	/* Save CRC */
	recv_crc = ftfs_header.crc;

	/* Check if the version is supported */
	if (memcmp((void *)ftfs_header.version, FT_VERSION,
		   sizeof(FT_VERSION) - 1)) {
		rf_e("Version %s not supported. "
			      "Current version is %s",
			      ftfs_header.version, FT_VERSION);
		return -WM_E_RFGET_INV_IMG;
	}

	return WM_SUCCESS;
}

int write_ftfs_image(data_fetch data_fetch_cb, void *priv, size_t fslen,
	struct partition_entry *f1)
{
	unsigned char writebuf[FS_WRITE_BUFSIZE];
	int ret, data_written, data_read, flash_addr;
	unsigned int read_length;
	uint32_t calculated_crc = 0;
	update_desc_t ud;

	read_length = sizeof(writebuf);

	if (fslen == 0) {
		rf_e("Invalid file length");
		return -WM_E_RFGET_INVLEN;
	}

	if (fslen > f1->size) {
		rf_e("Update FS too large");
		purge_stream_bytes(data_fetch_cb, priv, fslen,
				   writebuf, FS_WRITE_BUFSIZE);
		return -WM_E_RFGET_INVLEN;
	}

	/* Read the verify FTFS headers */
	ret = read_ftfs_headers(priv, data_fetch_cb);
	if (ret != WM_SUCCESS) {
		rf_e("FTFS header read failed");
		return ret;
	}

	/* If we reached here, the magic number is sane and the version is
	 * supported. Go ahead and burn the image.
	 */
	ret = rfget_update_begin(&ud, f1);
	if (ret != WM_SUCCESS) {
		rf_e("FTFS header write failed");
		return ret;
	}

	/* Write the FTFS header */
	ret = rfget_update_data(&ud, (char *)&ftfs_header, sizeof(ftfs_header));
	if (ret < 0) {
		return ret;
	}
	data_written = sizeof(ftfs_header);

	/* Write the FTFS data and calculate the CRC */
	while (data_written < fslen) {
		if ((fslen - data_written) < sizeof(writebuf))
			read_length = fslen - data_written;
		ret = data_fetch_cb(priv, writebuf, read_length);
		if (ret <= 0)
			break;

#ifdef CONFIG_SOFTWARE_CRC32
		calculated_crc = crc32(writebuf, ret, calculated_crc);
#endif
		if (rfget_update_data(&ud, (char *)writebuf, ret) != 0) {
			rf_e("Failed to write FTFS ");
			return -1;
		}
		data_written += ret;
	}

#if !defined(CONFIG_SOFTWARE_CRC32)
	/*
	 * FTFS checksum verification operation is not done above (in case of
	 * hardware engine) because,
	 * 1. It is not guaranteed to receive word aligned data from network.
	 * 2. With network data fetch callback, critical section would last
	 *    longer, not very good thing to have.
	 */
	read_length = sizeof(writebuf);
	flash_addr = f1->start;
	flash_addr += sizeof(ftfs_header);
	data_read = 0;
	data_read += sizeof(ftfs_header);

	mdev_t *crc_drv = crc_drv_open(MDEV_CRC_ENGINE_0, CRC32_IEEE);
	if (crc_drv == NULL) {
		rf_e("Unable to open CRC driver");
		return -WM_FAIL;
	}
	crc_drv_stream_setlen(crc_drv, fslen - sizeof(ftfs_header));
	while (data_read < fslen) {
		if ((fslen - data_read) < sizeof(writebuf))
			read_length = fslen - data_read;

		ret = flash_drv_read(ud.dev, writebuf, read_length, flash_addr);
		if (ret != WM_SUCCESS) {
			rf_e("Failed to read from flash %d", ret);
			break;
		}

		crc_drv_stream_feed(crc_drv, writebuf, read_length);

		flash_addr += read_length;
		data_read += read_length;
	}
	crc_drv_stream_get_result(crc_drv, &calculated_crc);
	crc_drv_close(crc_drv);
#endif


	if (memcmp(&recv_crc, &calculated_crc, sizeof(recv_crc)) == 0) {
		rfget_update_complete(&ud);
	}

	return WM_SUCCESS;
}
