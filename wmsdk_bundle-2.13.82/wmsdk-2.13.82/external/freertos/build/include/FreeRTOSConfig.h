/*
 * Copyright (C) 2011-2012, Marvell International Ltd.
 * All Rights Reserved.
 */

#ifndef FREERTOS_CONFIG_H
#define FREERTOS_CONFIG_H

/*-----------------------------------------------------------
 * Application specific definitions.
 *
 * These definitions should be adjusted for your particular hardware and
 * application requirements.
 *
 * THESE PARAMETERS ARE DESCRIBED WITHIN THE 'CONFIGURATION' SECTION OF THE
 * FreeRTOS API DOCUMENTATION AVAILABLE ON THE FreeRTOS.org WEB SITE. 
 *
 * See http://www.freertos.org/a00110.html.
 *----------------------------------------------------------*/

#define configUSE_PREEMPTION		1
#define configUSE_IDLE_HOOK		1
#define configUSE_TICK_HOOK		1
/* #define configCPU_CLOCK_HZ		( ( unsigned long ) 200000000 ) */
#define configTICK_RATE_HZ		( ( portTickType ) 1000 )
#define configMAX_PRIORITIES		( ( unsigned portBASE_TYPE ) 5 )
#define configMINIMAL_STACK_SIZE	( ( unsigned short ) 256 )
/* #define configTOTAL_HEAP_SIZE		( ( size_t ) ( 72 * 1024 ) ) */
#define configMAX_TASK_NAME_LEN		( 16 )
#define configUSE_TRACE_FACILITY	1
#define configUSE_16_BIT_TICKS		0
#define configIDLE_SHOULD_YIELD		1
#define configUSE_MUTEXES		1
#define configCHECK_FOR_STACK_OVERFLOW	2
#define configUSE_MALLOC_FAILED_HOOK	0
#define configUSE_RECURSIVE_MUTEXES	1
#define configUSE_COUNTING_SEMAPHORES	1

/* Enable this if run time statistics are to be enabled. The support
   functions are already added in WMSDK */
#define configGENERATE_RUN_TIME_STATS   0

#if ( configGENERATE_RUN_TIME_STATS == 1 )
#define portCONFIGURE_TIMER_FOR_RUN_TIME_STATS		\
	portWMSDK_CONFIGURE_TIMER_FOR_RUN_TIME_STATS
#define portGET_RUN_TIME_COUNTER_VALUE		\
	portWMSDK_GET_RUN_TIME_COUNTER_VALUE
#endif /* configGENERATE_RUN_TIME_STATS */

#define configUSE_CO_ROUTINES 		0
#define configMAX_CO_ROUTINE_PRIORITIES ( 2 )

/*  Software timer definitions. */
#define configUSE_TIMERS		1
#define configTIMER_TASK_PRIORITY	( 4 )
#define configTIMER_QUEUE_LENGTH	5
#define configTIMER_TASK_STACK_DEPTH	( configMINIMAL_STACK_SIZE*2 )

/* Set the following definitions to 1 to include the API function, or zero
to exclude the API function. */

#define INCLUDE_vTaskPrioritySet	1
#define INCLUDE_uxTaskPriorityGet	1
#define INCLUDE_vTaskDelete		1
#define INCLUDE_vTaskCleanUpResources	0
#define INCLUDE_vTaskSuspend		1
#define INCLUDE_vTaskDelayUntil		1
#define INCLUDE_vTaskDelay		1
#define INCLUDE_uxTaskGetStackHighWaterMark	1

#define configKERNEL_INTERRUPT_PRIORITY 	0xf0
#define configMAX_SYSCALL_INTERRUPT_PRIORITY 	0xa0 /* equivalent to 0xa0, or priority 5. */

#define xPortPendSVHandler	PendSV_IRQHandler
#define xPortSysTickHandler	SysTick_IRQHandler
#define vPortSVCHandler		SVC_IRQHandler

#endif /* FREERTOS_CONFIG_H */
