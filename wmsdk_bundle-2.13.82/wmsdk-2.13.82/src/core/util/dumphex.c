/*
 * Copyright (C) 2008-2013, Marvell International Ltd.
 * All Rights Reserved.
 */
#include <ctype.h>
#include <wmstdio.h>

#define DUMP_WRAPAROUND 16
void dump_hex(const void *data, unsigned len)
{
	wmprintf("**** Dump @ 0x%x Len: %d ****\n\r", data, len);

	int i;
	const char *data8 = (const char *)data;
	for (i = 0; i < len;) {
		wmprintf("%02x ", data8[i++]);
		if (!(i % DUMP_WRAPAROUND))
			wmprintf("\n\r");
	}

	wmprintf("\n\r******** End Dump *******\n\r");
}

void dump_hex_ascii(const void *data, unsigned len)
{
	wmprintf("**** Dump @ 0x%x Len: %d ****\n\r", data, len);

	int i;
	const char *data8 = (const char *)data;
	for (i = 0; i < len;) {
		wmprintf("%02x", data8[i]);
		if (!iscntrl(data8[i]))
			wmprintf(" == %c  ", data8[i]);
		else
			wmprintf(" ==    ", data8[i]);

		i++;
		if (!(i % DUMP_WRAPAROUND / 2))
			wmprintf("\n\r");
	}

	wmprintf("\n\r******** End Dump *******\n\r");
}
