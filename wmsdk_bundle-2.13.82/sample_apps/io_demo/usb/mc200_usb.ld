/*
 * Copyright (C) 2008-2012, Marvell International Ltd.
 * All Rights Reserved.
 */

/* Entry Point */
ENTRY(Reset_IRQHandler)

/* Start address of main stack pointer
 * Note: Stack grows towards lower addresses.
 */
_estack = 0x20020000;    /* end of 128K SRAM1 */

/* Heap size in bytes */
_heap_size = (72 * 1024);

/* Generate a link error if stack don't fit into SRAM.
 * Total stack size requirement will depend on the number of concurrent
 * threads in an application and the maximum stack required by each
 * thread.
 */
_min_stack_size = 0x800; /* required minimum amount of stack */

MEMORY
{
	SRAM0 (rwx)  : ORIGIN = 0x00100000, LENGTH = 384K
	SRAM1 (rwx)  : ORIGIN = 0x20000000, LENGTH = 128K
	NVRAM (rw)   : ORIGIN = 0x480C0000, LENGTH = 4K
}

SECTIONS
{
	.text :
	{
		. = ALIGN(256);
		KEEP(*(.isr_vector))
		. = ALIGN(4);

		*(.text.Reset_IRQHandler)
		*(.text .text.* .gnu.linkonce.t.*)
		*(.rodata .rodata.* .gnu.linkonce.r.*)

		. = ALIGN(4);
		_etext = .;
	} > SRAM0

	.bss (NOLOAD):
	{
		_bss = .;
		*(.bss)
		*(.bss.*)
		*(COMMON)
		_ebss = .;
	} > SRAM0

	.data :
	{
		_data = .;
		*(.data)
		*(.data.*)
		_edata = .;
	} > SRAM1

	/*
	 * NOTE: Please do not move the below section ".iobufs" to SRAM0.
	 * Some of the peripherals (e.g. USB, SDIO) with their own DMA engines
	 * have a requirement that the data buffers for DMA need to be in the
	 * "Data" memory (SRAM1). The peripherals that use internal DMA engine
	 * of MC200 (e.g. UART) can use data buffers from SRAM0 or SRAM1.
	 */
	.iobufs (NOLOAD):
	{
		. = ALIGN(4);
		_iobufs_start = .;
		*(.iobufs.page_aligned)
		*(.iobufs)
		*(.iobufs.*)
		_iobufs_end = .;
	} > SRAM1

	.heapdata (NOLOAD):
	{
		. = ALIGN(4);
		_heap_start = .;
		. += _heap_size;
		. = ALIGN(4);
		_heap_end = .;
	} > SRAM1

	/* Check for enough space for stack */
	._main_stack :
	{
	        . = ALIGN(4);
	        . = . + _min_stack_size;
	        . = ALIGN(4);
	} > SRAM1

	.nvram (NOLOAD):
	{
		/* BootROM uses first few bytes of retention ram */
		_nvram_start = .;
		. = . + 64;
		. = ALIGN(4);
		/* Zero initialized on bootup */
		_nvram_begin = .;
		*(.nvram)
		*(.nvram.*)
		_nvram_end = .;
		/* Un-initialized nvram section */
		. = ALIGN(4);
		*(.nvram_uninit)
		*(.nvram_uninit.*)
	} > NVRAM
}
