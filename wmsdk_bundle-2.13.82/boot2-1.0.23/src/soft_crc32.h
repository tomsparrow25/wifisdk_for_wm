/*
 *  Copyright (C) 2011-2013, Marvell International Ltd.
 *  All Rights Reserved.
 */

#ifndef __SOFT_CRC32_H
#define __SOFT_CRC32_H

#include <stdint.h>

uint32_t soft_crc32(const void *__data, int data_size, uint32_t crc);
void soft_crc32_init();

#endif
