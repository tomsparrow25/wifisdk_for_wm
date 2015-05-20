/*
 * Copyright 2008-2013, Marvell International Ltd.
 * All Rights Reserved.
 */

#include <stdlib.h>
#include <crc32.h>
#include <wm_os.h>
#include <wm_utils.h>

#define MAX_ENTROPY_HDLRS 4
static random_hdlr_t entropy_hdlrs[MAX_ENTROPY_HDLRS];

int random_register_handler(random_hdlr_t func)
{
	int i;

	for (i = 0; i < MAX_ENTROPY_HDLRS; i++) {
		if (entropy_hdlrs[i] != NULL)
			continue;

		entropy_hdlrs[i] = func;
		return WM_SUCCESS;
	}

	return -WM_E_NOSPC;
}

int random_unregister_handler(random_hdlr_t func)
{
	int i;

	for (i = 0; i < MAX_ENTROPY_HDLRS; i++) {
		if (entropy_hdlrs[i] != func)
			continue;

		entropy_hdlrs[i] = NULL;
		return WM_SUCCESS;
	}

	return -WM_E_INVAL;
}

void get_random_sequence(unsigned char *buf, unsigned int size)
{
	int i, epoch;
	unsigned int feed_data = 0, curr_time;

	curr_time = os_ticks_get();
	epoch = sys_get_epoch();

	feed_data = curr_time ^ epoch;

	for (i = 0; i < MAX_ENTROPY_HDLRS; i++) {
		if (entropy_hdlrs[i]) {
			feed_data ^= (*entropy_hdlrs[i])();
		}
	}

	/* In the beginning, the MSBs are mostly the same, hence XOR with all
	 * the bytes in the feed_data to get greater randomness.
	 */
	for (i = 0; i < 4; i++) {
		feed_data ^= ((curr_time ^ epoch)  << (i * 8));
	}

	srand(feed_data);

	for (i = 0; i < size; i++)
		buf[i] = rand() & 0xff;
}
