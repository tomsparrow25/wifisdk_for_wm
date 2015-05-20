/*
 *  Copyright (C) 2013, Marvell International Ltd.
 *  All Rights Reserved.
 */

#include <wmstdio.h>
#include <string.h>
#include <wmlog.h>
#include <wm_os.h>
#include <cli.h>
#include <ctype.h>

static sem_dbg_info_t semdbg[MAX_SEM_INFO_BUF];
static int semcnt;
static os_mutex_t sem_mutex;

void sem_debug_add(const xSemaphoreHandle handle, const char *name,
		   bool is_semaphore)
{
	int temp, i;
	if (sem_mutex) {
		temp = os_mutex_get(&sem_mutex, OS_WAIT_FOREVER);
		if (temp == -WM_FAIL) {
			wmprintf("[sem-dbg] Failed to get sem-mutex\r\n");
			return;
		}
	}
	for (i = 0;  i < MAX_SEM_INFO_BUF; i++) {
		if (semdbg[i].x_queue == 0) {
			if (name && strlen(name)) {
				semdbg[i].q_name =
					os_mem_alloc(strlen(name)+1);
				if (semdbg[i].q_name == NULL)
					break;
				strcpy(semdbg[i].q_name, name);
			}
			semdbg[i].x_queue = handle;
			semdbg[i].is_semaphore = is_semaphore;
			semcnt++;
			break;
		}
	}
	if (sem_mutex)
		os_mutex_put(&sem_mutex);
	return;
}

void sem_debug_delete(const xSemaphoreHandle handle)
{
	int temp, i;
	if (sem_mutex) {
		temp = os_mutex_get(&sem_mutex, OS_WAIT_FOREVER);
		if (temp == -WM_FAIL) {
			wmprintf("[sem-dbg] Failed to get sem-mutex\r\n");
			return;
		}
	}
	for (i = 0; i < MAX_SEM_INFO_BUF; i++) {
		if (semdbg[i].x_queue == handle) {
			semdbg[i].x_queue  = 0;
			os_mem_free(semdbg[i].q_name);
			semdbg[i].q_name = NULL;
			semcnt--;
			break;
		}
	}
	if (sem_mutex)
		os_mutex_put(&sem_mutex);
	return;
}

#define MAX_WAITER_BUF 512

static void os_dump_seminfo()
{
	int i = 0, temp;
	const unsigned int *px_queue = NULL;
	unsigned int *list;
	char *pc_write_buffer = os_mem_alloc(MAX_WAITER_BUF);
	if (pc_write_buffer == NULL)
		return;

	if (sem_mutex) {
		temp = os_mutex_get(&sem_mutex, OS_WAIT_FOREVER);
		if (temp == -WM_FAIL) {
			wmprintf("[sem-dbg] Failed to get sem-mutex\r\n");
			os_mem_free(pc_write_buffer);
			return;
		}
	}

	if (semcnt > 0) {
		wmprintf("%-32s%-8s%-50s%s", "Name", "Count", "Waiters",
			 "Type\r\n");
	}

	for (i = 0; i < MAX_SEM_INFO_BUF; i++) {
		if (semdbg[i].x_queue == 0)
			continue;
		if (semdbg[i].q_name && isascii(semdbg[i].q_name[0])) {
			wmprintf("%-32s", semdbg[i].q_name);
		} else {
			wmprintf("%-32s", "-");
		}
		px_queue = semdbg[i].x_queue;
		wmprintf("%-8d",
			 os_semaphore_getcount(&semdbg[i].x_queue));
		vGetWaiterListFromHandle(px_queue, &list);
		vGetTaskNamesInList((signed char *) pc_write_buffer,
				    MAX_WAITER_BUF - 1,
				    (const unsigned int *)list);
		pc_write_buffer[MAX_WAITER_BUF - 1] = '\0';
		wmprintf("%-50s ", pc_write_buffer);
		(semdbg[i].is_semaphore == 0) ?
			wmprintf("q") :
			wmprintf("s");
		wmprintf("\r\n");
	}
	if (sem_mutex)
		os_mutex_put(&sem_mutex);
	os_mem_free(pc_write_buffer);
}

static void semaphore_info(int argc, char **argv)
{
	os_dump_seminfo();
	return;
}

static struct cli_command cli[] = {
	{"sem-dbg", "", semaphore_info},
};


int seminfo_init(void)
{
	int ret;
	ret = os_mutex_create(&sem_mutex, "sem-mutex",
			OS_MUTEX_INHERIT);
	if (ret == -WM_FAIL) {
		wmprintf("[sem-dbg] Failed to create sem-mutex\r\n");
		return -WM_FAIL;
	}
	if (cli_register_commands
	(&cli[0], sizeof(cli) / sizeof(struct cli_command)))
		return 1;
	return 0;
}
