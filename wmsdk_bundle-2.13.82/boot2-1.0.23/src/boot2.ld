/*
 * Copyright (C) 2013, Marvell International Ltd.
 * All Rights Reserved.
 */

/* Entry point */
ENTRY(setup_stack)

/* Highest address of the user mode stack */
_estack = 0x20020000;    /* end of 128K RAM1 */

/* Generate a link error if heap and stack don't fit into RAM */
_min_stack_size = 0x800; /* required amount of stack */

MEMORY
{
	SRAM0 (rwx)  : ORIGIN = 0x0015F000, LENGTH = 4K
	SRAM1 (rwx)  : ORIGIN = 0x2001F000, LENGTH = 4K
}

SECTIONS
{
	.text :
	{
		*(.text.setup_stack)
		*(.text .text.* .gnu.linkonce.t.*)
		*(.rodata .rodata.* .gnu.linkonce.r.*)
		. = ALIGN(4);
		_etext = .;
	} > SRAM0

	.data :
	{
		_data = .;
		*(.data)
		*(.data.*)
		_edata = .;
	} > SRAM0

	.bss :
	{
		. = ALIGN(4);
		_bss = .;
		*(.bss)
		*(.bss.*)
		. = ALIGN(4);
		_ebss = .;
	} > SRAM1

	/* Check for enough space for stack */
	.main_stack :
	{
		. = ALIGN(4);
		. = . + _min_stack_size;
		. = ALIGN(4);
	} > SRAM1
}
