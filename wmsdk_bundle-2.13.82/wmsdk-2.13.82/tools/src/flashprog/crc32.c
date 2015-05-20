/*
 * Copyright 2008-2012, Marvell International Ltd.
 * All Rights Reserved.
 */

#include <inttypes.h>
#include "crc32.h"
#include "mc200_crc.h"

static int crc_init;

static void crc_set_mode()
{
	CRC_SetMode(CRC_32_IEEE);
}

uint32_t crc32(void *__data, int data_size, uint32_t crc)
{
	uint32_t result;

	if (!__data || !data_size)
		return 0;

	if (!crc_init) {
		crc_set_mode();
		crc_init = 1;
	}

	CRC_Disable();
	CRC_Enable();
	result = CRC_Calculate(__data, data_size);

	if (crc && result == crc)
		result = 0;

	return result;
}
