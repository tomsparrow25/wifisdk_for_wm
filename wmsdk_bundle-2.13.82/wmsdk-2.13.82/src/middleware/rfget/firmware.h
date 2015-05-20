/*
 * Firmware partition, segment, and command block definitions
 */

#ifndef __FIRMWARE_H
#define __FIRMWARE_H

#include <wmtypes.h>
#include <firmware_structure.h>

int update_firmware(data_fetch data_fetch_cb, void *priv,
		    size_t firmware_length, struct partition_entry *passive);
int update_wifi_firmware(data_fetch data_fetch_cb, void *priv,
		    size_t firmware_length, struct partition_entry *passive);
int write_ftfs_image(data_fetch data_fetch_cb, void *priv, size_t fslen,
		     struct partition_entry *passive);
int verify_load_firmware(uint32_t start, int size);

#endif
