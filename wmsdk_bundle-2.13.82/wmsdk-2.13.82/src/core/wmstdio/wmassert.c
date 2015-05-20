/*
 *  Copyright 2008-2013, Marvell International Ltd.
 *  All Rights Reserved.
 */

/** wmassert.c: Assert Functions
 */
#include <string.h>

#include <wm_os.h>
#include <wmstdio.h>
#include <wmassert.h>

void _wm_assert(const char *filename, int lineno, 
	    const char* fail_cond)
{
	wmprintf("\n\n\r*************** PANIC *************\n\r");
	wmprintf("Filename  : %s ( %d )\n\r", filename, lineno);
	wmprintf("Condition : %s\n\r", fail_cond);
	wmprintf("***********************************\n\r");
	os_enter_critical_section();
	for( ;; );
}
