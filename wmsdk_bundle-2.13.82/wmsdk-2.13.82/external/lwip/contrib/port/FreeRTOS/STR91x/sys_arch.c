/*
 * Copyright (c) 2001-2003 Swedish Institute of Computer Science.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT
 * SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT
 * OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY
 * OF SUCH DAMAGE.
 *
 * This file is part of the lwIP TCP/IP stack.
 *
 * Author: Adam Dunkels <adam@sics.se>
 * Author: Stefano Oliveri <software@stf12.net>
 *
 * This file was modified by Stefano Oliveri to implement the system emulation layer for STR91x.
 *
 */

/* lwIP includes. */
#include "lwip/debug.h"
#include "lwip/def.h"
#include "lwip/sys.h"
#include "lwip/mem.h"
#include "lwip/stats.h"

//#define SYS_ARCH_DEBUG
#ifdef SYS_ARCH_DEBUG
#include <wmstdio.h>
#define DBG(...)        wmprintf("sys arch: " __VA_ARGS__)
#else
#define DBG(...)
#endif /* SYS_ARCH_DEBUG */

#define SYS_MAX_Q 20
static xQueueHandle sys_mbox_table[SYS_MAX_Q];
#define SYS_MAX_SEM 20
static xQueueHandle sys_sem_table[SYS_MAX_SEM];

/* This is the number of threads that can be started with sys_thread_new() */
#define SYS_THREAD_MAX 4

static u16_t s_nextthread = 0;


/*-----------------------------------------------------------------------------------*/
//  Creates an empty mailbox.
err_t sys_mbox_new(sys_mbox_t *mbox, int size)
{
	int i;
	err_t ret = ERR_OK;
	DBG("Enter: %s mbox %p\n\r",__FUNCTION__,*mbox);
	*mbox = xQueueCreate( archMESG_QUEUE_LENGTH, sizeof( void * ) );

#if SYS_STATS
      ++lwip_stats.sys.mbox.used;
      if (lwip_stats.sys.mbox.max < lwip_stats.sys.mbox.used) {
         lwip_stats.sys.mbox.max = lwip_stats.sys.mbox.used;
	  }
#endif /* SYS_STATS */
	vTaskSuspendAll();
	for(i=0;i<SYS_MAX_Q;i++) {
		if(sys_mbox_table[i] == NULL) {
			sys_mbox_table[i] = *mbox;
			break;
		}
	}
	xTaskResumeAll();	
	if(i == SYS_MAX_Q) {
		/* If we reach here *mbox was never added to the table, we can safely delete it */
		vQueueDelete( *mbox );
		*mbox = NULL;
		DBG("Error: sys_mbox_new failed, please increase the SYS_MAX_Q value \n\r");
		ret = ERR_MEM;
	}

	DBG("Exit: %s mbox %p\n\r",__FUNCTION__,*mbox);
	return ret;
}

/*-----------------------------------------------------------------------------------*/
/*
  Deallocates a mailbox. If there are messages still present in the
  mailbox when the mailbox is deallocated, it is an indication of a
  programming error in lwIP and the developer should be notified.
*/
void sys_mbox_free(sys_mbox_t *mbox)
{
	int i;
	DBG("Enter: %s mbox %p \n\r",__FUNCTION__,*mbox);
	if( *mbox == NULL )
	{
		DBG("Exit: %s: Invalid mbox\n\r",__FUNCTION__);
		return;
	}

	if( uxQueueMessagesWaiting( *mbox ) )
	{
		/* Line for breakpoint.  Should never break here! */
		portNOP();
#if SYS_STATS
	    lwip_stats.sys.mbox.err++;
#endif /* SYS_STATS */

		// TODO notify the user of failure.
	}


#if SYS_STATS
     --lwip_stats.sys.mbox.used;
#endif /* SYS_STATS */
	vTaskSuspendAll();
	for(i=0;i<SYS_MAX_Q;i++)
		if(*mbox == sys_mbox_table[i])
			sys_mbox_table[i] = NULL;
	xTaskResumeAll();
	/* Delete after marking the mbox in the table as invalid */
	vQueueDelete( *mbox );
	*mbox = NULL;

	DBG("Exit: %s\n\r",__FUNCTION__);
}

/*-----------------------------------------------------------------------------------*/
//   Posts the "msg" to the mailbox.
void sys_mbox_post(sys_mbox_t *mbox, void *data)
{
	if( *mbox == NULL )
	{
		DBG("Exit: %s: Invalid mbox\n\r",__FUNCTION__);
		return;
	}

	while ( xQueueSendToBack(*mbox, &data, portMAX_DELAY ) != pdTRUE );
}


/*-----------------------------------------------------------------------------------*/
//   Try to post the "msg" to the mailbox.
err_t sys_mbox_trypost(sys_mbox_t *mbox, void *msg)
{
   err_t result = ERR_VAL;

   DBG("Enter: %s\n\r",__FUNCTION__);
   if( *mbox == NULL )
   {
       DBG("Exit: %s: Invalid mbox\n\r",__FUNCTION__);
       return result;
   }

   if ( xQueueSend( *mbox, &msg, 0 ) == pdPASS )
   {
      result = ERR_OK;
   }
   else {
      // could not post, queue must be full
      result = ERR_MEM;

#if SYS_STATS
      lwip_stats.sys.mbox.err++;
#endif /* SYS_STATS */

   }

   DBG("Exit: %s, result %d\n\r",__FUNCTION__,result);
   return result;
}

/*-----------------------------------------------------------------------------------*/
/*
  Blocks the thread until a message arrives in the mailbox, but does
  not block the thread longer than "timeout" milliseconds (similar to
  the sys_arch_sem_wait() function). The "msg" argument is a result
  parameter that is set by the function (i.e., by doing "*msg =
  ptr"). The "msg" parameter maybe NULL to indicate that the message
  should be dropped.

  The return values are the same as for the sys_arch_sem_wait() function:
  Number of milliseconds spent waiting or SYS_ARCH_TIMEOUT if there was a
  timeout.

  Note that a function with a similar name, sys_mbox_fetch(), is
  implemented by lwIP.
*/
u32_t sys_arch_mbox_fetch(sys_mbox_t *mbox, void **msg, u32_t timeout)
{
	void *dummyptr;
	portTickType StartTime, EndTime, Elapsed;

	if( *mbox == NULL )
	{
		DBG("Exit: %s: Invalid mbox\n\r",__FUNCTION__);
		return SYS_ARCH_TIMEOUT;
	}

	StartTime = xTaskGetTickCount();

	if ( msg == NULL )
	{
		msg = &dummyptr;
	}

	if ( timeout != 0 )
	{
		if ( pdTRUE == xQueueReceive( *mbox, &(*msg), timeout / portTICK_RATE_MS ) )
		{
			EndTime = xTaskGetTickCount();
			Elapsed = (EndTime - StartTime) * portTICK_RATE_MS;

			return ( Elapsed );
		}
		else // timed out blocking for message
		{
			*msg = NULL;

			return SYS_ARCH_TIMEOUT;
		}
	}
	else // block forever for a message.
	{
		while( pdTRUE != xQueueReceive( *mbox, &(*msg), portMAX_DELAY ) ); // time is arbitrary
		EndTime = xTaskGetTickCount();
		Elapsed = (EndTime - StartTime) * portTICK_RATE_MS;

		return ( Elapsed ); // return time blocked TODO test
	}
}

/*-----------------------------------------------------------------------------------*/
/*
  Similar to sys_arch_mbox_fetch, but if message is not ready immediately, we'll
  return with SYS_MBOX_EMPTY.  On success, 0 is returned.
*/
u32_t sys_arch_mbox_tryfetch(sys_mbox_t *mbox, void **msg)
{
	void *dummyptr;
	if( *mbox == NULL )
	{
		DBG("Exit: %s: Invalid mbox\n\r",__FUNCTION__);
		return SYS_MBOX_EMPTY;
	}

	if ( msg == NULL )
	{
		msg = &dummyptr;
	}

	if ( pdTRUE == xQueueReceive( *mbox, &(*msg), 0 ) )
	{
		return ERR_OK;
	}
	else
	{
		return SYS_MBOX_EMPTY;
	}
}

int sys_mbox_valid(sys_mbox_t *mbox)
{
	int i,ret=0;
	DBG("Enter: %s mbox %p\n\r",__FUNCTION__,*mbox);
	if( *mbox == NULL )
	{
		DBG("Exit: %s: Invalid mbox\n\r",__FUNCTION__);
		return ret;
	}
	vTaskSuspendAll();
	for(i=0;i<SYS_MAX_Q;i++) {
		if(sys_mbox_table[i] == *mbox) {
			ret = 1;
		}
	}
	xTaskResumeAll();	

	DBG("Exit: %s ret %d\n\r",__FUNCTION__,ret);
	return ret;
}
void sys_mbox_set_invalid(sys_mbox_t *mbox)
{
	int i;
	DBG("Enter: %s mbox %p\n\r",__FUNCTION__,*mbox);
	if( *mbox == NULL )
	{
		DBG("Exit: %s: Invalid mbox\n\r",__FUNCTION__);
		return;
	}
	vTaskSuspendAll();
	for(i=0;i<SYS_MAX_Q;i++) {
		if(sys_mbox_table[i] == *mbox) {
			sys_mbox_table[i] = NULL;
			break;
		}
	}
	xTaskResumeAll();	
	*mbox = NULL;

	DBG("Exit: %s\n\r",__FUNCTION__);
}
/*-----------------------------------------------------------------------------------*/
//  Creates and returns a new semaphore. The "count" argument specifies
//  the initial state of the semaphore.
err_t sys_sem_new(sys_sem_t *sem, u8_t count)
{
	int i;
	DBG("Enter: %s\n\r",__FUNCTION__);
	err_t ret = ERR_OK;
	vSemaphoreCreateBinary( *sem );

	if( *sem == NULL )
	{

#if SYS_STATS
      ++lwip_stats.sys.sem.err;
#endif /* SYS_STATS */

		return ERR_MEM;	// TODO need assert
	}

	if(count == 0)	// Means it can't be taken
	{
		xSemaphoreTake(*sem,1);
	}

	vTaskSuspendAll();
	for(i=0;i<SYS_MAX_SEM;i++) {
		if(sys_sem_table[i] == NULL) {
			sys_sem_table[i] = *sem;
			break;
		}
	}
	xTaskResumeAll();	
	if(i == SYS_MAX_SEM) {
		vQueueDelete( *sem );
		*sem = NULL;
		DBG("Error: sys_sem_new failed, please increase the SYS_MAX_SEM value \n\r");
		ret = ERR_MEM;
	}

#if SYS_STATS
	++lwip_stats.sys.sem.used;
 	if (lwip_stats.sys.sem.max < lwip_stats.sys.sem.used) {
		lwip_stats.sys.sem.max = lwip_stats.sys.sem.used;
	}
#endif /* SYS_STATS */

	DBG("Exit: %s sem %p\n\r",__FUNCTION__,*sem);
	return ret;
}

/*-----------------------------------------------------------------------------------*/
/*
  Blocks the thread while waiting for the semaphore to be
  signaled. If the "timeout" argument is non-zero, the thread should
  only be blocked for the specified time (measured in
  milliseconds).

  If the timeout argument is non-zero, the return value is the number of
  milliseconds spent waiting for the semaphore to be signaled. If the
  semaphore wasn't signaled within the specified time, the return value is
  SYS_ARCH_TIMEOUT. If the thread didn't have to wait for the semaphore
  (i.e., it was already signaled), the function may return zero.

  Notice that lwIP implements a function with a similar name,
  sys_sem_wait(), that uses the sys_arch_sem_wait() function.
*/
u32_t sys_arch_sem_wait(sys_sem_t *sem, u32_t timeout)
{
	DBG("Enter: %s\n\r",__FUNCTION__);
	portTickType StartTime, EndTime, Elapsed;

	StartTime = xTaskGetTickCount();

	if(	timeout != 0)
	{
		if( xSemaphoreTake( *sem, timeout / portTICK_RATE_MS ) == pdTRUE )
		{
			EndTime = xTaskGetTickCount();
			Elapsed = (EndTime - StartTime) * portTICK_RATE_MS;

			DBG("Exit: %s\n\r",__FUNCTION__);
			return (Elapsed); // return time blocked TODO test
		}
		else
		{
			DBG("Exit: %s\n\r",__FUNCTION__);
			return SYS_ARCH_TIMEOUT;
		}
	}
	else // must block without a timeout
	{
		while( xSemaphoreTake( *sem, portMAX_DELAY ) != pdTRUE );
		EndTime = xTaskGetTickCount();
		Elapsed = (EndTime - StartTime) * portTICK_RATE_MS;

		DBG("Exit: %s\n\r",__FUNCTION__);
		return ( Elapsed ); // return time blocked

	}
}

/*-----------------------------------------------------------------------------------*/
// Signals a semaphore
void sys_sem_signal(sys_sem_t *sem)
{
	xSemaphoreGive( *sem );
}

/*-----------------------------------------------------------------------------------*/
// Deallocates a semaphore
void sys_sem_free(sys_sem_t *sem)
{
	int i;
	DBG("Enter: %s sem %p\n\r",__FUNCTION__,*sem);
#if SYS_STATS
      --lwip_stats.sys.sem.used;
#endif /* SYS_STATS */

	vTaskSuspendAll();
	for(i=0;i<SYS_MAX_SEM;i++)
		if(*sem == sys_sem_table[i])
			sys_sem_table[i] = NULL;
	xTaskResumeAll();
	/* Delete after marking the sem in the table as invalid */
	vQueueDelete( *sem );
	*sem = NULL;

	DBG("Exit: %s\n\r",__FUNCTION__);
}

void sys_sem_set_invalid(sys_sem_t *sem)
{
	int i;
	DBG("Enter: %s sem %p\n\r",__FUNCTION__,*sem);
	if( *sem == NULL )
	{
		DBG("Exit: %s: Invalid sem\n\r",__FUNCTION__);
		return;
	}
	vTaskSuspendAll();
	for(i=0;i<SYS_MAX_SEM;i++) {
		if(sys_sem_table[i] == *sem) {
			sys_sem_table[i] = NULL;
		}
	}
	xTaskResumeAll();
	*sem = NULL;

	DBG("Exit: %s\n\r",__FUNCTION__);
}

int sys_sem_valid(sys_sem_t *sem)
{
	int i,ret = 0;
	DBG("Enter: %s sem %p\n\r",__FUNCTION__,*sem);

	if( *sem == NULL )
	{
		DBG("Exit: %s: Invalid sem\n\r",__FUNCTION__);
		return ret;
	}
	vTaskSuspendAll();
	for(i=0;i<SYS_MAX_SEM;i++) {
		if(sys_sem_table[i] == *sem) {
			ret = 1;
		}
	}
	xTaskResumeAll();	

	DBG("Exit: %s\n\r",__FUNCTION__);
	return ret;
}
/*-----------------------------------------------------------------------------------*/
// Initialize sys arch
void sys_init(void)
{
	int i;

	DBG("Enter: %s\n\r",__FUNCTION__);
	for(i = 0; i < SYS_MAX_Q; i++) {
		sys_mbox_table[i] = NULL;
	}
	for(i = 0; i < SYS_MAX_SEM; i++) {
		sys_sem_table[i] = NULL;
	}
	DBG("Exit: %s\n\r",__FUNCTION__);
}

/*-----------------------------------------------------------------------------------*/
/*-----------------------------------------------------------------------------------*/
// TODO
/*-----------------------------------------------------------------------------------*/
/*
  Starts a new thread with priority "prio" that will begin its execution in the
  function "thread()". The "arg" argument will be passed as an argument to the
  thread() function. The id of the new thread is returned. Both the id and
  the priority are system dependent.
*/
sys_thread_t sys_thread_new(const char *name, lwip_thread_fn thread, void *arg, int stacksize, int prio)
{
xTaskHandle CreatedTask;
DBG("Enter: %s",__FUNCTION__);
int result;

   if ( s_nextthread < SYS_THREAD_MAX )
   {
      result = xTaskCreate( thread, ( signed portCHAR * ) name, stacksize, arg, prio, &CreatedTask );

	   if(result == pdPASS)
	   {
		DBG("Exit: %s",__FUNCTION__);
		   return CreatedTask;
	   }
	   else
	   {
		DBG("Exit: %s",__FUNCTION__);
		   return NULL;
	   }
   }
   else
   {
	DBG("Exit: %s",__FUNCTION__);
      return NULL;
   }
}

/*
  This optional function does a "fast" critical region protection and returns
  the previous protection level. This function is only called during very short
  critical regions. An embedded system which supports ISR-based drivers might
  want to implement this function by disabling interrupts. Task-based systems
  might want to implement this by using a mutex or disabling tasking. This
  function should support recursive calls from the same task or interrupt. In
  other words, sys_arch_protect() could be called while already protected. In
  that case the return value indicates that it is already protected.

  sys_arch_protect() is only required if your port is supporting an operating
  system.
*/
sys_prot_t sys_arch_protect(void)
{
	vPortEnterCritical();
	return 1;
}

/*
  This optional function does a "fast" set of critical region protection to the
  value specified by pval. See the documentation for sys_arch_protect() for
  more information. This function is only required if your port is supporting
  an operating system.
*/
void sys_arch_unprotect(sys_prot_t pval)
{
	( void ) pval;
	vPortExitCritical();
}

unsigned long sys_arch_get_os_ticks()
{
	return  xTaskGetTickCount();
}

