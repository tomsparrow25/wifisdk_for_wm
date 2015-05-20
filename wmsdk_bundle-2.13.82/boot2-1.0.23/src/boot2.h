/*
 * Copyright (C) 2008-2013 Marvell International Ltd.
 * All Rights Reserved.
 */

#ifndef _BOOT2_H_
#define _BOOT2_H_

#include <stdint.h>

/* Nvram address word to store debugging information */
#define NVRAM_ADDR 0x480C0020

/* Boot2 progress status */
#define SYS_INIT (1<<0)
#define FLASH_INIT (1<<1)
#define PRIMARY_FIRMWARE (1<<2)

/* Boot2 error status */
#define BAD_PART_TABLE_CRC (1<<16)
#define BAD_PART_ENTRY_CRC (1<<17)
#define BAD_FIRMWARE_CRC (1<<18)
#define BAD_PART_TABLE_MAGIC (1<<19)
#define BAD_FIRMWARE_SIG (1<<20)
#define BAD_FIRMWARE_IMG (1<<21)

/* Boot2 success or failure status */
#define BOOT_SUCCESS (0x80000000)
#define BOOT_FAILED (0x00800000)

extern unsigned long _bss;
extern unsigned long _ebss;
extern unsigned long _estack;

#ifdef	DEBUG
#define dbg_printf	printf
#else
#define	dbg_printf(...)
#endif

extern unsigned long *nvram_addr;

static inline void writel(uint32_t data, void *addr)
{
	*((volatile uint32_t *)addr) = data;
}

static inline uint32_t readel(void *addr)
{
	return *((volatile uint32_t *)addr);
}

extern int boot_override_btn_pressed(void);
#endif
