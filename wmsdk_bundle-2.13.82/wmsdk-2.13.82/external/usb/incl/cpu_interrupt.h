/*
 * ============================================================================
 * Copyright (c) 2004-2014  Marvell Semiconductor, Inc. All Rights Reserved.
 *
 *                         Marvell Confidential
 * ============================================================================
 */
 /**
  * \file cpu_interrupt.h
  *
  * \brief This is the cpu interrupt
  *
  */

#ifndef CPU_INTERRUPT_H
#define CPU_INTERRUPT_H

#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

	unsigned int _cpu_get_int(void) __attribute__ ((naked));

	unsigned int _cpu_get_int(void) {
		__asm volatile
		 ("MRS r0, PRIMASK\n" "BX  lr\n");
	} void _cpu_disable_int(void) {
		__asm volatile
		 ("NOP\n" "CPSID i\n" "BX lr\n");
	}

	void _cpu_enable_int(unsigned int posture) {
		__asm volatile
		 ("MSR PRIMASK, r0\n" "BX lr\n");
	}

#define CPU_INTER_SAVE      unsigned int cpu_interrupt_save;
#define CPU_INTER_DISABLE   { cpu_interrupt_save = _cpu_get_int(); _cpu_disable_int(); };
#define CPU_INTER_ENABLE    { _cpu_enable_int(cpu_interrupt_save); };

#ifdef __cplusplus
}
#endif

#endif
