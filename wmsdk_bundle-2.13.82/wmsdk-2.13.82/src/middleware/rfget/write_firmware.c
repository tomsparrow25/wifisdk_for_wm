/*
 * Copyright 2008-2013, Marvell International Ltd.
 * All Rights Reserved.
 */

/** write_firmware.c: Write firmware images to flash
 */

#include <string.h>

#include <wmstdio.h>
#include <rfget.h>
#include <crc32.h>
#include <wm_os.h>
#include <flash.h>
#include <partition.h>

#include "firmware.h"

/*
 * For efficiently receiving data from network and writing it to internal
 * flash it is better to keep write buffer size equal to MTU in TCP.
 * (1500 - TCP/IP headers = 1460)
 */
#define FIRMWARE_UPDATE_BUFF_SIZE 256

/*
 * Description	: Update the firmware
 * Input	: void
 * Output	: WM_SUCCESS  => ok
 * 		: -1 => Fail
 */
int update_firmware(data_fetch data_fetch_cb, void *priv,
		    size_t firmware_length, struct partition_entry *f)
{
	uint32_t filesize, read_length;
	int32_t size_read;
	int error;
	update_desc_t ud;
	struct img_hdr ih;
	unsigned char buf[FIRMWARE_UPDATE_BUFF_SIZE];

	rf_d(" Updating firmware at address 0x%0x"
		       , f->start);
	rf_d("@ address 0x%x len: %d", f->start, f->size);

	if (firmware_length == 0) {
		rf_e("Firmware binary zero length");
		return -WM_E_RFGET_INVLEN;
	}

	/* Verify firmware length first before proceeding further */
	if (firmware_length > f->size) {
		rf_e("Firmware binary too large %d > %d",
			      firmware_length, f->size);
		purge_stream_bytes(data_fetch_cb, priv, firmware_length,
				   buf, FIRMWARE_UPDATE_BUFF_SIZE);
		return -WM_E_RFGET_INVLEN;
	}

	/*
	 * Before erasing anything in flash, let's get the first
	 * firmware data buffer in order to validate it.
	 */
	size_read = data_fetch_cb(priv, buf, FIRMWARE_UPDATE_BUFF_SIZE);
	if (size_read <= 16) {
		rf_e("Failed to get firmware data for initial validation");
		error = -WM_FAIL;
		goto out;
	}
	memcpy(&ih, buf, sizeof(ih));

	if (ih.magic_str != FW_MAGIC_STR || ih.magic_sig != FW_MAGIC_SIG) {
		rf_e("File data is not a firmware file");
		error = -WM_E_RFGET_INV_IMG;
		purge_stream_bytes(data_fetch_cb, priv,
				   firmware_length - size_read,
				   buf, FIRMWARE_UPDATE_BUFF_SIZE);
		goto out;
	}

	error = rfget_update_begin(&ud, f);
	if (error) {
		rf_e("Failed to erase firmware area");
		purge_stream_bytes(data_fetch_cb, priv,
				   firmware_length - size_read,
				   buf, FIRMWARE_UPDATE_BUFF_SIZE);
		goto out;
	}

	filesize = 0;
	read_length = FIRMWARE_UPDATE_BUFF_SIZE;
	while (filesize < firmware_length) {
		if (filesize != 0) {
			/* we got the data the first time around */
			if ((firmware_length - filesize) < sizeof(buf))
				read_length = firmware_length - filesize;
			size_read = data_fetch_cb(priv, buf, read_length);
		}
		if (size_read <= 0)
			break;
		error = rfget_update_data(&ud, (char *)buf, size_read);
		if (error) {
			rf_e("Failed to write firmware data");
			goto out;
		}
		filesize += size_read;
	}

	rf_d("Validating firmware start ... filesize = %d", filesize);
	/* Then validate firmware data in flash */
	error = verify_load_firmware(f->start, filesize);
	if (error) {
		rf_e("Firmware verification failed with code %d",
			      error);
		goto out;
	}

	rf_d("Validating firmware done");
	rfget_update_complete(&ud);

	return WM_SUCCESS;
out:
	return error;
}
