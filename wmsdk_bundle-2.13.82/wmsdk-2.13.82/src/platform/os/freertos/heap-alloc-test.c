/*
 * Copyright 2008-2013, Marvell International Ltd.
 * All Rights Reserved.
 */

#include <cli.h>
#include <stdlib.h>
#include <cli_utils.h>
#include <wmtypes.h>
#include <wmstdio.h>
#include <wm_os.h>

#define HTDEBUG(...)				\
	wmprintf("HEAP-TEST: ");		\
	wmprintf(__VA_ARGS__);		\
	wmprintf("\n\r")

#define HTDEBUG_ERROR(...)			\
	wmprintf("HEAP-TEST: Error: ");	\
	wmprintf(__VA_ARGS__);		\
	wmprintf("\n\r");

#define HTDEBUG_WARN(...)			\
	wmprintf("HEAP-TEST: Warn:  ");	\
	wmprintf(__VA_ARGS__);		\
	wmprintf("\n\r");

#ifndef CONFIG_OS_FREERTOS
#error This test uses certain special functionality present in FreeRTOS. Not supported for other OSes
#endif // CONFIG_OS_FREERTOS

typedef struct {
	void *addr;
	int size;
} alloc_info_t;

#define MAX_NUMBER_OF_ALLOCS 20
#define MIN_HEAP_REQUIRED_FOR_TEST				\
	(MAX_NUMBER_OF_ALLOCS * sizeof(alloc_info_t)) + 256
#define OVERHEAD_TEST_ALLOC_SIZE 128
#define MINIMUM_OVERHEAD_EXPECTED 12
#define TEST_2_ITERATIONS 2000

#define MIN_ALLOC_SIZE 1

int heaptest_cli_init(void);

/**
 * Find out overhead per allocated block.
 * 
 * @pre Caller is in a critical section
 */
static int run_test_1(int *space_for_overhead)
{
	int overhead;
	void *ptr;
	int remaining_heap = xPortGetFreeHeapSize();

	HTDEBUG("Calculating overhead ...");

	ptr = os_mem_alloc(OVERHEAD_TEST_ALLOC_SIZE);
	if (!ptr) {
		HTDEBUG_ERROR("Unable to allocate memory");
		return -WM_E_NOMEM;
	}

	overhead = remaining_heap - OVERHEAD_TEST_ALLOC_SIZE
	    - xPortGetFreeHeapSize();
	HTDEBUG("RH: %d FH: %d OV: %d",
		remaining_heap, xPortGetFreeHeapSize(), overhead);

	os_mem_free(ptr);
	if (overhead < MINIMUM_OVERHEAD_EXPECTED) {
		HTDEBUG_ERROR("Overhead less than expected. Min: %d",
			      MINIMUM_OVERHEAD_EXPECTED);
		vHeapSelfTest(1);
		return -WM_FAIL;
	}

	/* Verify that freeing returns back the same amount of memory */
	if (xPortGetFreeHeapSize() != remaining_heap) {
		HTDEBUG_ERROR("Heap size not as expected");
		return -WM_FAIL;
	}
	// rough calculation
	*space_for_overhead = 2 * overhead;
	return WM_SUCCESS;
}

static void dump_pattern(const char *mem, int size)
{
	int c = 10, i;
	for (i = 0; i < c; i++, mem++)
		wmprintf("%x: %x\n\r", mem, *mem);
}

static void fill_pattern(char *mem, int size)
{
	int data = ((unsigned)mem & 0x00000FF0) >> 4;
	//HTDEBUG("Pattern starts with: %x for address %x", data, mem);
	int i;
	for (i = 0; i < size; i++, data++)
		mem[i] = data;
}

static int check_pattern(const char *mem, int size)
{
	unsigned char data = (unsigned char)(((unsigned)mem & 0xFF0) >> 4);
	//HTDEBUG("Verify for address %x Pattern start: %x", mem, data);
	int i;
	for (i = 0; i < size; i++, data++) {
		//wmprintf("%d:%d %x == %x\n\r", size, i, mem[i], data);
		if (mem[i] != data) {
			HTDEBUG_ERROR("(%d) %x != %x", i, mem[i], data);
			dump_pattern(mem, size);
			return -WM_FAIL;
		}
	}

	//HTDEBUG("PATTERN CHECK SUCCESS");
	return WM_SUCCESS;
}

/**
 * Algorithm
 * 1. Allocate random sized blocks. 
 * 2. Put a pattern of data in each of them.
 * 3. Verify all the patterns again. 
 * 4. Free all the allcated blocks.
 * 5. Verify all allocated memory reclaimed.
 *
 * @note In this function we will not free the allocated memory even if
 * there is an error. This is because an error here implies that there is
 * problem with the allocator/some corruption has occured.
 */
static int run_test_2(alloc_info_t * alloc_info, int max_allocs,
		      int space_for_overhead)
{
	int iterations = TEST_2_ITERATIONS, i;
	int start_heap_size = xPortGetFreeHeapSize();

	HTDEBUG("Running test 2. Iterations: %d ", iterations);
	while (iterations--) {
		//wmprintf("ITER: %d\n\r", iterations);
		/* zero out alloc info array */
		memset(alloc_info, 0x00, max_allocs * sizeof(alloc_info_t));

		for (i = 0; i < max_allocs; i++) {
			int alloc_size;
			int max_alloc_size =
			    vPortBiggestFreeBlockSize() - space_for_overhead;
			if (max_alloc_size <= MIN_ALLOC_SIZE) {
				break;
			}

RETRY:
			alloc_size = rand() % max_alloc_size;
			if (alloc_size < MIN_ALLOC_SIZE)
				goto RETRY;

			//wmprintf("Allocate: %d bytes\n\r", alloc_size);
			alloc_info[i].addr = os_mem_alloc(alloc_size);
			if (!alloc_info[i].addr) {
				HTDEBUG_ERROR("Failed to allocate memory");
				return -WM_FAIL;
			}

			alloc_info[i].size = alloc_size;
			fill_pattern(alloc_info[i].addr, alloc_info[i].size);
			//dump_pattern(alloc_info[i].addr, alloc_info[i].size);
		}

		for (i = 0; i < max_allocs; i++) {
			if (alloc_info[i].addr) {
				//dump_pattern(alloc_info[i].addr, alloc_info[i].size);
				if (check_pattern
				    (alloc_info[i].addr,
				     alloc_info[i].size) != WM_SUCCESS) {
					HTDEBUG_ERROR("Pattern check failed");
					return -WM_FAIL;
				}
				//wmprintf("Freeing address: %x\n\r", alloc_info[i].addr);
				os_mem_free(alloc_info[i].addr);
			}
		}

		if (start_heap_size != xPortGetFreeHeapSize()) {
			HTDEBUG_ERROR("Heap size not same as earlier.");
			return -WM_FAIL;
		}

		if (vHeapSelfTest(0) == 1) {
			HTDEBUG_ERROR("Heap allocator self test fail.");
			return -WM_FAIL;
		}
	}
	return WM_SUCCESS;
}

static void heap_test(int argc, char **argv)
{
	int remaining_heap, istate, space_for_overhead, status;
	alloc_info_t *alloc_info;

#ifndef CONFIG_OS_FREERTOS
	HTDEBUG("Success");
	return;
#endif // CONFIG_OS_FREERTOS

	istate = os_enter_critical_section();

	remaining_heap = xPortGetFreeHeapSize();
	if (remaining_heap < MIN_HEAP_REQUIRED_FOR_TEST) {
		HTDEBUG_ERROR("Not enough heap remaining to test\n\rFAIL");
		os_exit_critical_section(istate);
		return;
	}

	/* Allocate space for pointers to store the allocations. */
	alloc_info = os_mem_alloc((sizeof(alloc_info_t) *
				   MAX_NUMBER_OF_ALLOCS));
	if (!alloc_info)
		goto malloc_fail;

	status = run_test_1(&space_for_overhead);
	if (status != WM_SUCCESS) {
		HTDEBUG("FAIL");
		return;
	}

	status =
	    run_test_2(alloc_info, MAX_NUMBER_OF_ALLOCS, space_for_overhead);
	if (status != WM_SUCCESS) {
		HTDEBUG("FAIL");
		return;
	}

	HTDEBUG("Success");
	os_mem_free(alloc_info);
	os_exit_critical_section(istate);
	return;

malloc_fail:
	os_exit_critical_section(istate);
	HTDEBUG("FAIL");
	return;
}

static struct cli_command heaptest_commands[] = {
	{"heap-test", NULL, heap_test},
};

int heaptest_cli_init(void)
{
	int i;

	for (i = 0; i < sizeof(heaptest_commands) / sizeof(struct cli_command);
	     i++)
		if (cli_register_command(&heaptest_commands[i]))
			return 1;
	return 0;
}
