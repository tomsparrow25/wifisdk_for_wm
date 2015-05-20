/*
 * Copyright (C) 2011-2013, Marvell International Ltd.
 * All Rights Reserved.
 */

/* Standard includes. */
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <boot2.h>

/* Driver includes. */
#include <mc200_uart.h>
#include <mc200_qspi0.h>
#include <mc200_flash.h>
#include <mc200_pmu.h>
#include <board.h>

void setup_stack(void) __attribute__((naked));

uint32_t *nvram_addr = (uint32_t *)NVRAM_ADDR;

static void system_init(void)
{
	/* Initialize flash power domain */
	PMU_PowerOnVDDIO(PMU_VDDIO_FL);

	/* Initialize boot override switch/gpio */
	board_gpio_power_on();
}

int main(void)
{
	system_init();
	writel(SYS_INIT, nvram_addr);
	QSPI0_Init_CLK();
	writel(readel(nvram_addr) | FLASH_INIT, nvram_addr);
	dbg_printf("\r\nboot2: version %s\r\n", BOOT2_VERSION);
	dbg_printf("boot2: Hardware setup done...\r\n");
	boot2_main();
	/* boot2_main never returns */
	return 0;
}

void initialize_bss(void)
{
	unsigned long *pulSrc, *pulDest;

	/* Zero fill the bss segment. */
	for (pulDest = &_bss; pulDest < &_ebss;)
		*pulDest++ = 0;

	/* Call the application's entry point. */
	main();
}

void setup_stack(void)
{
	/* Setup stack pointer */
	asm("ldr r3, =_estack");
	asm("mov sp, r3");
	initialize_bss();
}
