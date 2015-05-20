/*
 * Copyright (C) 2008-2012, Marvell International Ltd.
 * All Rights Reserved.
 */
#ifdef __ICCARM__

#pragma location = "_bss_start"
unsigned long *_bss;
#pragma location = "_bss_end"
unsigned long *_ebss;

#pragma location = "_iobufs_start"
unsigned long *_iobufs_start;
#pragma location = "_iobufs_end"
unsigned long *_iobufs_end;

#pragma location = "_nvram_begin"
unsigned long *_nvram_begin;
#pragma location = "_nvram_end"
unsigned long *_nvram_end;

/* For gcc this comes from libc */
int errno;
#endif
