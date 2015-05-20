/*
 * Copyright 2008-2012, Marvell International Ltd.
 * All Rights Reserved.
 * Firmware partition, segment, and command block definitions
 */

#ifndef __FIRMWARE_H
#define __FIRMWARE_H

#include <stdint.h>
#include <firmware_structure.h>


int verify_firmware(int fl_dev, uint32_t partition_offset,
			struct partition_entry *p, uint32_t firmware_size);

#endif
