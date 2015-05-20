/** @file wps_util.c
 *  @brief This file contains functions for debugging print.
 *  
 *  Copyright (C) 2003-2010, Marvell International Ltd.
 *  All Rights Reserved
 */

#ifdef STDOUT_DEBUG

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdarg.h>
#include <ctype.h>
#include "wmstdio.h"

#include "wps_def.h"
#include "wps_util.h"

/********************************************************
        Local Variables
********************************************************/
static int wps_debug_level = DEFAULT_MSG;	/* defined in wps_util.h */

/********************************************************
        Global Variables
********************************************************/

/********************************************************
        Local Functions
********************************************************/

/********************************************************
        Global Functions
********************************************************/

/** 
 *  @brief Debug print function
 *         Note: New line '\n' is added to the end of the text when printing to stdout.
 *
 *  @param level    Debugging flag
 *  @param fmt      Printf format string, followed by optional arguments
 *  @return         None
 */
void wps_printf(int level, char *fmt, ...)
{
	va_list ap;
	char buffer[130];

	va_start(ap, fmt);

	if (level & wps_debug_level) {
		vsprintf(buffer, fmt, ap);
		wmprintf("[wps] %s \r\n", buffer);
	}
	va_end(ap);
}

/** 
 *  @brief Debug buffer dump function
 *         Note: New line '\n' is added to the end of the text when printing to stdout.
 *
 *  @param level    Debugging flag
 *  @param title    Title of for the message
 *  @param buf      Data buffer to be dumped
 *  @param len      Length of the buf
 *  @return         None
 */
void
wps_hexdump(int level, const char *title, const unsigned char *buf, size_t len)
{
	int i, j;
	unsigned char *offset;

	if (!(level & wps_debug_level))
		return;

	offset = (unsigned char *)buf;
	wmprintf("[wps] %s - hexdump(len=%lu):\r\n", title,
		       (unsigned long)len);

	for (i = 0; i < len / 16; i++) {
		for (j = 0; j < 16; j++)
			wmprintf("%02x  ", offset[j]);
		wmprintf("\r\n");
		offset += 16;
	}
	i = len % 16;
	for (j = 0; j < i; j++)
		wmprintf("%02x  ", offset[j]);
	wmprintf("\r\n");
}

#endif /* STDOUT_DEBUG */
