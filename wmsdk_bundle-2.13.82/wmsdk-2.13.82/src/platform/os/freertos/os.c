/*
 * Copyright 2008-2013, Marvell International Ltd.
 * All Rights Reserved.
 */

#include <inttypes.h>
#include <wmstdio.h>
#include <arch/boot_flags.h>
#include <stdio.h>
#include <wm_os.h>
#include <board.h>

#include <mdev_gpt.h>

WEAK int main();
WEAK int user_app_init();

os_mutex_t g_sys_lock;

/* Some prerequisites before calling application's entry point
 */
void app_start(void *pvParameters)
{
	int ret;

	/* Create a global mutex here, this is done to avoid inter dependency
	 * between modules where they can't create or initialize mutex.
	 */
	ret = os_mutex_create(&g_sys_lock, "sys_lock", OS_MUTEX_INHERIT);
	if (ret != WM_SUCCESS)
		os_thread_self_complete(NULL);

	/* Initialize user main function */
	main();
	/* user_app_init() is kept only for backward compatibility reasons */
	user_app_init();

	os_thread_delete(NULL);
}

#define mainTEST_TASK_PRIORITY		(tskIDLE_PRIORITY)
#define mainTEST_DELAY			(400 / portTICK_RATE_MS)

#ifdef CONFIG_ENABLE_FREERTOS_RUNTIME_STATS_SUPPORT

static void runtime_stats_gpt_overflow_cb()
{
	vTaskResetRunTimeStats();
}

#if defined(CONFIG_RUNTIME_STATS_USE_GPT0)
#define RUNTIME_STATS_GPTID GPT0_ID
#define RUNTIME_STATS_GPT_CLKID CLK_GPT0
#elif defined(CONFIG_RUNTIME_STATS_USE_GPT1)
#define RUNTIME_STATS_GPTID GPT1_ID
#define RUNTIME_STATS_GPT_CLKID CLK_GPT1
#elif defined(CONFIG_RUNTIME_STATS_USE_GPT2)
#define RUNTIME_STATS_GPTID GPT2_ID
#define RUNTIME_STATS_GPT_CLKID CLK_GPT2
#elif defined(CONFIG_RUNTIME_STATS_USE_GPT3)
#define RUNTIME_STATS_GPTID GPT3_ID
#define RUNTIME_STATS_GPT_CLKID CLK_GPT3
#endif

/*
 * This function is called by FreeRTOS if configGENERATE_RUN_TIME_STATS
 * macro is defined as 1.
 */
void portWMSDK_CONFIGURE_TIMER_FOR_RUN_TIME_STATS()
{
	mdev_t *gpt_dev;

	gpt_drv_init(RUNTIME_STATS_GPTID);
	gpt_dev = gpt_drv_open(RUNTIME_STATS_GPTID);
	if (!gpt_dev)
		return;

	/* Reduce GPT counter clock from 50 MHz to 1 MHz */
	CLK_ModuleClkDivider(RUNTIME_STATS_GPT_CLKID, 50);
	gpt_drv_setcb(gpt_dev, runtime_stats_gpt_overflow_cb);

	/* Divide by 50 as gpt_drv_set() will divide given param by 50 */
	gpt_drv_set(gpt_dev, 0xFFFFFFFF/50);
	gpt_drv_start(gpt_dev);

	/* GPT clock now should have started at 1 uS period */
}

/*
 * This function is called by FreeRTOS if configGENERATE_RUN_TIME_STATS
 * macro is defined as 1.
 */
unsigned long portWMSDK_GET_RUN_TIME_COUNTER_VALUE()
{
	return GPT_GetCounterVal(RUNTIME_STATS_GPTID);
}
#endif /* CONFIG_ENABLE_FREERTOS_RUNTIME_STATS_SUPPORT */

int os_init()
{
	int ret;

	ret = xTaskCreate(app_start, (const signed char * const)"app_start",
				configMINIMAL_STACK_SIZE + 500,
				NULL, tskIDLE_PRIORITY + 3, NULL);
	vTaskStartScheduler();

	return ret == pdPASS ? WM_SUCCESS : -WM_FAIL;
}

int os_thread_create(os_thread_t *thandle, const char *name,
		     void (*main_func)(os_thread_arg_t arg),
		     void *arg, os_thread_stack_t *stack, int prio)
{
	int ret;

	ret = xTaskCreate(main_func, (const signed char *)name,
			  stack->size, arg, prio, thandle);

	os_dprintf(" Thread Create: ret %d thandle %p"
		   " stacksize = %d\r\n", ret, *thandle, stack->size);
	return ret == pdPASS ? WM_SUCCESS : -WM_FAIL;

}

int os_timer_create(os_timer_t *timer_t, const char *name, os_timer_tick ticks,
		    void (*call_back)(os_timer_arg_t), void *cb_arg,
		    os_timer_reload_t reload,
		    os_timer_activate_t activate)
{
	int auto_reload = (reload == OS_TIMER_ONE_SHOT) ? pdFALSE : pdTRUE;

	*timer_t = xTimerCreate((const signed char *)name, ticks,
				      auto_reload, cb_arg, call_back);
	if (*timer_t == NULL)
		return -WM_FAIL;

	if (activate == OS_TIMER_AUTO_ACTIVATE)
		xTimerStart(*timer_t, 0);

	return WM_SUCCESS;
}

void vApplicationStackOverflowHook(xTaskHandle *pxTask, signed
				   portCHAR * pcTaskName)
{
	ll_printf("Panic! stack overflow happened with task %s\r\n",
		  pcTaskName);
	while (1)
		;
}

void vApplicationMallocFailedHook(void)
{
	ll_printf("Panic! heap overflow happened - halt.\r\n");
	while (1)
		;
}

/* the OS timer register is loaded with CNTMAX */
#define CNTMAX		((board_cpu_freq() / configTICK_RATE_HZ) - 1UL)
#define CPU_CLOCK_TICKSPERUSEC	(board_cpu_freq()/1000000U)
#define USECSPERTICK		(1000000U/configTICK_RATE_HZ)

/* returns time in micro-secs since time began */
uint32_t os_get_timestamp()
{
	uint32_t nticks = xTaskGetTickCount();
	uint32_t counter = SysTick->VAL;

	/*
	 * If this is called after SysTick counter
	 * expired but before SysTick Handler got a
	 * chance to run, then set the return value
	 * to the start of next tick.
	 */
	if (SCB->ICSR & SCB_ICSR_PENDSTSET_Msk) {
		nticks++;
		counter = CNTMAX;
	}

	return ((CNTMAX - counter)/CPU_CLOCK_TICKSPERUSEC)
				+ nticks*USECSPERTICK;
}

int os_queue_create(os_queue_t *qhandle, const char *name, int msgsize, os_queue_pool_t *poolname)
{

	/** The size of the pool divided by the max. message size gives the
	 * max. number of items in the queue. */
	os_dprintf(" Queue Create: name = %s poolsize = %d msgsize = %d\r\n" \
				, name, poolname->size, msgsize);
	*qhandle = xQueueCreate(poolname->size/msgsize, msgsize);
	os_dprintf(" Queue Create: handle %p\r\n", *qhandle);
	if (*qhandle) {
		sem_debug_add((const xQueueHandle)*qhandle,
			      name, 0);
		return WM_SUCCESS;
	}
	return -WM_FAIL;
}

void (*g_os_tick_hooks[MAX_CUSTOM_HOOKS])(void) = { NULL };
void (*g_os_idle_hooks[MAX_CUSTOM_HOOKS])(void) = { NULL };

/** The FreeRTOS Tick hook function. */
void vApplicationTickHook( void )
{
	int i;

	for (i = 0; i < MAX_CUSTOM_HOOKS; i++) {
		if (g_os_tick_hooks[i])
			g_os_tick_hooks[i]();
	}
}

void vApplicationIdleHook(void)
{
	int i;
	for (i = 0; i < MAX_CUSTOM_HOOKS; i++) {
		if (g_os_idle_hooks[i])
			g_os_idle_hooks[i]();
	}
}

/* Freertos handles this internally? */
void os_thread_stackmark(char *name)
{
	/* Nothing to-do */
}

/* Freertos does no size check on this buffer and hence kept higher than
 * minimal size that would be required */
#define MAX_TASK_INFO_BUF 1024

void os_dump_threadinfo(char *name)
{
	uint8_t status;
	uint32_t stack_remaining, tcbnumber, priority, pc, lr;
	char tname[25], *substr;
	char *task_info_buf = NULL;
	task_info_buf = (char *) os_mem_alloc(MAX_TASK_INFO_BUF);
	if (task_info_buf == NULL)
		return;
	vTaskList((signed char *) task_info_buf);
	substr = strtok(task_info_buf, "\r\n");
	while (substr) {
		char *tabtab = strstr(substr, "\t\t");
		if (!tabtab) {
			wmprintf("Task list info corrupted."
					"Cannot parse\n\r");
			break;
		}
		/* NULL terminate the task name */
		*tabtab = 0;
		strncpy(tname, substr, sizeof(tname));
		/* advance to thread status */
		substr = tabtab + 2;
		sscanf((const char *)substr,
		       "%c\t%u\t%u\t%u\t%x\t%x\r\n", &status,
		       &priority, &stack_remaining, &tcbnumber, &lr, &pc);
		if (strncmp(tname, name, strlen(name)) == 0 || name == NULL)  {
			wmprintf("%s:\r\n", tname);
			const char *status_str;
			switch (status) {
			case 'R':
				status_str = "ready";
				break;
			case 'B':
				status_str = "blocked";
				break;
			case 'S':
				status_str = "suspended";
				break;
			case 'D':
				status_str = "deleted";
				break;
			default:
				status_str = "unknown";
				break;
			}
			wmprintf("\tstate = %s\r\n", status_str);
			wmprintf("\tpriority = %u\r\n", priority);
			wmprintf("\tstack remaining = %d bytes\r\n",
				 stack_remaining);
			wmprintf("\ttcbnumber = %d\r\n", tcbnumber);
			wmprintf("\tpc = 0x%x, lr = 0x%x\r\n", pc, lr);
			if (name != NULL && task_info_buf) {
				os_mem_free(task_info_buf);
				return;
			}
		}

		substr = strtok(NULL, "\r\n");
	}

	if (name)
                /* If we got this far and didn't find the thread, say so. */
		wmprintf("ERROR: No thread named '%s'\r\n", name);
	if (task_info_buf)
		os_mem_free(task_info_buf);
}

typedef struct event_wait_t {
	/* parameter passed in the event get call */
	unsigned thread_mask;
	/* The 'get' thread will wait on this sem */
	os_semaphore_t sem;
	struct event_wait_t *next;
	struct event_wait_t *prev;
} event_wait_t;

typedef struct event_group_t {
	/* Main event flags will be stored here */
	unsigned flags;
	/* This flag is used to indicate deletion
	* of event group */
	bool delete_group;
	/* to protect this structure and the waiting list */
	os_mutex_t mutex;
	event_wait_t *list;
} event_group_t;

static inline void os_event_flags_remove_node(event_wait_t *node,
		event_group_t *grp_ptr)
{
	if (node->prev)
		node->prev->next = node->next;
	if (node->next)
		node->next->prev = node->prev;
	/* If only one node is present */
	if (!node->next && !node->prev)
		grp_ptr->list = NULL;
	os_mem_free(node);
}

int os_event_flags_create(event_group_handle_t *hnd)
{
	int ret;
	event_group_t *eG = os_mem_alloc(sizeof(event_group_t));
	if (!eG) {
		os_dprintf("ERROR:Mem allocation\r\n");
		return -WM_FAIL;
	}
	memset(eG, 0x00, sizeof(event_group_t));
	ret = os_mutex_create(&eG->mutex, "event-flag-mutex", OS_MUTEX_INHERIT);
	if (ret != WM_SUCCESS) {
		os_mem_free(eG);
		return -WM_FAIL;
	}
	*hnd = (event_group_handle_t) eG;
	return WM_SUCCESS;
}

int os_event_flags_get(event_group_handle_t hnd, unsigned requested_flags,
		       flag_rtrv_option_t option, unsigned *actual_flags_ptr,
		       unsigned wait_option)
{
	bool wait_done = 0;
	unsigned status;
	int ret;
	*actual_flags_ptr = 0;
	event_wait_t *tmp = NULL, *node = NULL;
	if (hnd == 0) {
		os_dprintf("ERROR:Invalid event flag handle\r\n");
		return -WM_FAIL;
	}
	if (requested_flags == 0) {
		os_dprintf("ERROR:Requested flag is zero\r\n");
		return -WM_FAIL;
	}
	if (actual_flags_ptr == NULL) {
		os_dprintf("ERROR:Flags pointer is NULL\r\n");
		return -WM_FAIL;
	}
	event_group_t *eG = (event_group_t *) hnd;

check_again:
	os_mutex_get(&eG->mutex, OS_WAIT_FOREVER);

	if ((option == EF_AND) || (option == EF_AND_CLEAR)) {
		if ((eG->flags & requested_flags) == requested_flags)
			status = eG->flags;
		else
			status = 0;
	} else if ((option == EF_OR) || (option == EF_OR_CLEAR)) {
		status = (requested_flags & eG->flags);
	} else {
		os_dprintf("ERROR:Invalid event flag get option\r\n");
		os_mutex_put(&eG->mutex);
		return -WM_FAIL;
	}
	/* Check flags */
	if (status) {
		*actual_flags_ptr = status;

		/* Clear the requested flags from main flag */
		if ((option == EF_AND_CLEAR) || (option == EF_OR_CLEAR))
			eG->flags &= ~status;

		if (wait_done) {
			/*Delete the created semaphore */
			os_semaphore_delete(&tmp->sem);
			/* Remove ourselves from the list */
			os_event_flags_remove_node(tmp, eG);
		}
		os_mutex_put(&eG->mutex);
		return WM_SUCCESS;
	} else {
		if (wait_option) {
			if (wait_done == 0) {
				/* Add to link list */
				/* Prepare a node to add in the link list */
				node = os_mem_alloc(sizeof(event_wait_t));
				if (!node) {
					os_dprintf("ERROR:memory alloc\r\n");
					os_mutex_put(&eG->mutex);
					return -WM_FAIL;
				}
				memset(node, 0x00, sizeof(event_wait_t));
				/* Set the requested flag in the node */
				node->thread_mask = requested_flags;
				/* Create a semaophore */
				ret = os_semaphore_create(&node->sem,
					"wait_thread");
				if (ret) {
					os_dprintf
					("ERROR:In creating semaphore\r\n");
					os_mem_free(node);
					os_mutex_put(&eG->mutex);
					return -WM_FAIL;
				}
				/* If there is no node present */
				if (eG->list == NULL) {
					eG->list = node;
					tmp = eG->list;
				} else {
					tmp = eG->list;
					/* Move to last node */
					while (tmp->next != NULL) {
						os_dprintf("waiting \r\n");
						tmp = tmp->next;
					}
					tmp->next = node;
					node->prev = tmp;
					tmp = tmp->next;
				}
				/* Take semaphore first time */
				ret = os_semaphore_get(&tmp->sem,
					OS_WAIT_FOREVER);
				if (ret != WM_SUCCESS) {
					os_dprintf
					("ERROR:1st sem get error\r\n");
					os_mutex_put(&eG->mutex);
					/*Delete the created semaphore */
					os_semaphore_delete(&tmp->sem);
					/* Remove ourselves from the list */
					os_event_flags_remove_node(tmp, eG);
					return -WM_FAIL;
				}
			}
			os_mutex_put(&eG->mutex);
			/* Second time get is performed for work-around purpose
			as in current implementation of semaphore 1st request
			is always satisfied */
			ret = os_semaphore_get(&tmp->sem,
				os_msec_to_ticks(wait_option));
			if (ret != WM_SUCCESS) {
				os_mutex_get(&eG->mutex, OS_WAIT_FOREVER);
				/*Delete the created semaphore */
				os_semaphore_delete(&tmp->sem);
				/* Remove ourselves from the list */
				os_event_flags_remove_node(tmp, eG);
				os_mutex_put(&eG->mutex);
				return EF_NO_EVENTS;
			}

			/* We have woken up */
			/* If the event group deletion has been requested */
			if (eG->delete_group) {
				os_mutex_get(&eG->mutex, OS_WAIT_FOREVER);
				/*Delete the created semaphore */
				os_semaphore_delete(&tmp->sem);
				/* Remove ourselves from the list */
				os_event_flags_remove_node(tmp, eG);
				os_mutex_put(&eG->mutex);
				return -WM_FAIL;
			}
			wait_done = 1;
			goto check_again;
		} else {
			os_mutex_put(&eG->mutex);
			return EF_NO_EVENTS;
		}
	}
}

int os_event_flags_set(event_group_handle_t hnd, unsigned flags_to_set,
		       flag_rtrv_option_t option)
{
	event_wait_t *tmp = NULL;

	if (hnd == 0) {
		os_dprintf("ERROR:Invalid event flag handle\r\n");
		return -WM_FAIL;
	}
	if (flags_to_set == 0) {
		os_dprintf("ERROR:Flags to be set is zero\r\n");
		return -WM_FAIL;
	}

	event_group_t *eG = (event_group_t *) hnd;

	os_mutex_get(&eG->mutex, OS_WAIT_FOREVER);

	/* Set flags according to the set_option */
	if (option == EF_OR)
		eG->flags |= flags_to_set;
	else if (option == EF_AND)
		eG->flags &= flags_to_set;
	else {
		os_dprintf("ERROR:Invalid flag set option\r\n");
		os_mutex_put(&eG->mutex);
		return -WM_FAIL;
	}

	if (eG->list) {
		tmp = eG->list;
		if (tmp->next == NULL) {
			if (tmp->thread_mask & eG->flags)
				os_semaphore_put(&tmp->sem);
		} else {
			while (tmp != NULL) {
				if (tmp->thread_mask & eG->flags)
					os_semaphore_put(&tmp->sem);
				tmp = tmp->next;
			}
		}
	}
	os_mutex_put(&eG->mutex);
	return WM_SUCCESS;
}

int os_event_flags_delete(event_group_handle_t *hnd)
{
	int i, max_attempt = 3;
	event_wait_t *tmp = NULL;

	if (*hnd == 0) {
		os_dprintf("ERROR:Invalid event flag handle\r\n");
		return -WM_FAIL;
	}
	event_group_t *eG = (event_group_t *)*hnd;

	os_mutex_get(&eG->mutex, OS_WAIT_FOREVER);

	/* Set the flag to delete the group */
	eG->delete_group = 1;

	if (eG->list) {
		tmp = eG->list;
		if (tmp->next == NULL)
			os_semaphore_put(&tmp->sem);
		else {
			while (tmp != NULL) {
				os_semaphore_put(&tmp->sem);
				tmp = tmp->next;
			}
		}
	}
	os_mutex_put(&eG->mutex);

	/* If still list is not empty then wait for 3 seconds */
	for (i = 0; i < max_attempt; i++) {
		os_mutex_get(&eG->mutex, OS_WAIT_FOREVER);
		if (eG->list) {
			os_mutex_put(&eG->mutex);
			os_thread_sleep(os_msec_to_ticks(1000));
		} else {
			os_mutex_put(&eG->mutex);
			break;
		}
	}

	os_mutex_get(&eG->mutex, OS_WAIT_FOREVER);
	if (eG->list) {
		os_mutex_put(&eG->mutex);
		return -WM_FAIL;
	} else
		os_mutex_put(&eG->mutex);

	/* Delete the event group */
	os_mem_free(eG);
	*hnd = 0;
	return WM_SUCCESS;
}

void _os_delay(int cnt)
{
	/*
	 * Approx number of instructions in 1 ms.
	 * This divisor is arrived at experimentaly.
	 * So not accurate.
	 */
	volatile unsigned long i = board_cpu_freq() / 2000;

	/* Delay loop has 5 instructions.
	 * This might vary with toolchain/optimizations.
	 */
	i /= 5;

	/* Execution from Flash is slower.
	 * The divisor is arrived at experimentaly.
	 * So not accurate.
	 */
	if (((unsigned int)_os_delay & 0x1f000000) == 0x1f000000)
		i /= 700;

	i *= cnt;

	while (i--)
		;
}

/** Disables all interrupts at NVIC level */
inline void os_disable_all_interrupts()
{
	taskDISABLE_INTERRUPTS();
}

/** Enable all interrupts at NVIC lebel */
inline void os_enable_all_interrupts()
{
	taskENABLE_INTERRUPTS();
}

int os_rwlock_create(os_rw_lock_t *plock,
		     const char *mutex_name,
		     const char *lock_name)
{
	return os_rwlock_create_with_cb(plock,
				     mutex_name,
				     lock_name,
				     NULL);
}
int os_rwlock_create_with_cb(os_rw_lock_t *plock,
		     const char *mutex_name,
		     const char *lock_name,
		     cb_fn r_fn)
{
	int ret = WM_SUCCESS;
	ret = os_mutex_create(&(plock->reader_mutex),
			      mutex_name,
			      OS_MUTEX_INHERIT);
	if (ret == -WM_FAIL) {
		return -WM_FAIL;
	}
	ret = os_semaphore_create(&(plock->rw_lock),
				  lock_name);
	if (ret == -WM_FAIL) {
		return -WM_FAIL;
	}
	plock->reader_count = 0;
	plock->reader_cb = r_fn;
	return ret;
}

int os_rwlock_read_lock(os_rw_lock_t *lock,
			unsigned int wait_time)
{
	int ret = WM_SUCCESS;
	ret = os_mutex_get(&(lock->reader_mutex), OS_WAIT_FOREVER);
	if (ret == -WM_FAIL) {
		return ret;
	}
	lock->reader_count++;
	if (lock->reader_count == 1) {
		if (lock->reader_cb) {
			ret = lock->reader_cb(lock, wait_time);
			if (ret == -WM_FAIL) {
				lock->reader_count--;
				os_mutex_put(&(lock->reader_mutex));
				return ret;
			}
		} else {
			/* If  1 it is the first reader and
			 * if writer is not active, reader will get access
			 * else reader will block.
			 */
			ret = os_semaphore_get(&(lock->rw_lock),
					   wait_time);
			if (ret == -WM_FAIL) {
				lock->reader_count--;
				os_mutex_put(&(lock->reader_mutex));
				return ret;
			}
		}
	}
	os_mutex_put(&(lock->reader_mutex));
	return ret;
}

int os_rwlock_read_unlock(os_rw_lock_t *lock)
{
	int ret = os_mutex_get(&(lock->reader_mutex), OS_WAIT_FOREVER);
	if (ret == -WM_FAIL) {
		return ret;
	}
	lock->reader_count--;
	if (lock->reader_count == 0) {
		/* This is last reader so
		 * give a chance to writer now
		 */
		os_semaphore_put(&(lock->rw_lock));
	}
	os_mutex_put(&(lock->reader_mutex));
	return ret;
}

int os_rwlock_write_lock(os_rw_lock_t *lock,
			 unsigned int wait_time)
{
	int ret = os_mutex_get(&(lock->reader_mutex),
			       wait_time);
	if (ret == -WM_FAIL) {
		return ret;
	}
	ret = os_semaphore_get(&(lock->rw_lock),
				   wait_time);
	os_mutex_put(&(lock->reader_mutex));
	return ret;
}

void os_rwlock_write_unlock(os_rw_lock_t *lock)
{
	os_semaphore_put(&(lock->rw_lock));
}

void os_rw_lock_delele(os_rw_lock_t *lock)
{
	lock->reader_cb = NULL;
	os_semaphore_delete(&(lock->rw_lock));
	os_mutex_delete(&(lock->reader_mutex));
	lock->reader_count = 0;
}

