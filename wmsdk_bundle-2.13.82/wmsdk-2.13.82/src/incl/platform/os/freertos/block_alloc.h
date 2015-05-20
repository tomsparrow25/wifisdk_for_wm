/*! \file block_alloc.h
 *  \brief Slab/Block Allocator
 *
 * Copyright 2008-2013, Marvell International Ltd.
 * All Rights Reserved.
 *
 * \section intro Introduction
 *
 * There are some packages which perform a large number of fixed-size allacations.
 * But, the maximum number of allocations that are active at any given point
 * in time is very less.
 * It would be desirable to have a slab allocator which can reduce the
 * fragmentation that may potentially be caused by the use of
 * such large number of allocations.
 * The slab/block allocator aims at allocating fixed size blocks as per the
 * requirement of the user, subject to the total size specified by the user
 * when initializing the pool of that specific block size.
 *
 * Example: If a user wishes to create a pool with restricted access and
 * having 10 blocks of size say 20 bytes,
 * then he can first initialize the pool.
 * Note that the input buffer needs some extra space to store the metadata.
 *
 * For that he can use:
 *
 * struct pool_info *pool_ptr;
 *
 * char buffer[20 * 10 + 20];
 *
 * char *block_ptr;
 *
 *
 * 1. Initialize Pool:
 *
 * pool_ptr = ba_pool_create(20, buffer, sizeof(buffer), 1);
 *
 * Further, he may allocate or deallocate blocks using the pool pointer.
 *
 * 2. Allocation
 *
 * block_ptr = ba_block_allocate(pool_ptr);
 *
 * 3. Deallocation:
 *
 * ba_block_release(pool_ptr, block_ptr);
 *
 * Further, if a user wishes to delete the memory pool, he may do so by:
 *
 * 4. Delete pool
 *
 * ba_pool_delete(pool_ptr);
 *
 * Note that a pool can be deleted only if all the blocks in it are free.
 */


#ifndef BLOCK_ALLOC_H_
#define BLOCK_ALLOC_H_

#include <wmstdio.h>
#include <string.h>
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"
#include "timers.h"

/** The maximum number of blocks that can be allocated for a given pool. */
#define MAX_BLOCKS (sizeof(unsigned long) * 8)

#define OS_WAIT_FOREVER portMAX_DELAY

struct pool_info {

	/** head points to the start of the block for that particular size. */
	void *head;
	/** bitmap is used to know the status of each block starting from
	 *  block 0. (corresponding to LSB) */
	unsigned long bitmap;
	/** block_count indicates the number of blocks that are available
	 *  for allocation. (or are free)*/
	unsigned char block_count;
	/** block_size indicates the fixed size of each block in the pool. */
	unsigned long block_size;
	/** mutex to restrict access to critical section. */
	/* As of now xSemaphoreHandle is used instead of os_mutex_t to avoid recursive dependency problem. */
	/* In future if this is to be used for threadx xSemaphoreHandle should be changed to os_mutex_t. */
	xSemaphoreHandle mutex;
};

/** Create a memory pool of specified block size.
 *
 *  This function creates and initializes a pool with blocks of specified size.
 *  \param block_size The required block size for each block.
 *  \param buf A pointer to the caller allocated buffer. This buffer needs
 *  to be 4 byte aligned.
 *   All allocations are satisfied from this pool.
 *  \param pool_size Total size of the buffer.
 *   Note that the first few bytes of the pool are used by the block allocator to
 *   maintain its metadata. Hence, if your pool size if 100 and the block size
 *   is 20, only 4 successive allocations can be made from this pool.
 *   This should be taken into account by the users.
 *  \param mutex_flag A flag to enable (set to 1) or disable( set to 0) mutex lock for the pool.
 *   If the pool is being accessed in the context of a single thread, you may disable mutex_flag,
 *   thus saving some time.
 *
 *  \return pointer to the created pool if successful.
 *  \return NULL if unsuccessful.
 */
struct pool_info *ba_pool_create(unsigned long block_size, void *buf, unsigned long pool_size, unsigned char mutex_flag);

/** Delete already initialized memory pool.
 *
 *  This function deletes an initialized memory pool.
 *  \param b A pointer of structure pool_info to be deleted.
 *
 *  \return 0 if successful.
 *  \return -1 if unsuccessful.
 */
int ba_pool_delete(struct pool_info *b);

/** Allocate a block from the memory pool.
 *
 *  This function allocates a block of the specified size.
 *  \param b An already initialized pointer of structure pool_info corresponding to the required size.
 *
 *  \return the pointer if successful.
 *  \return NULL if unsuccessful.
 */
void *ba_block_allocate(struct pool_info *b);

/** Free a block from the memory pool.
 *
 *  This function frees a block of the specified size.
 *  \param b An already initialized pointer of structure pool_info whose block is to be freed.
 *  \param alloc The actual pointer to be deallocated.
 *
 *  \return 0 if successful.
 *  \return -1 if unsuccessful.
 */
int ba_block_release(struct pool_info *b, void *alloc);

/** Get information about already initialized memory pool.
 *
 * This function gives details about already initialized memory pool.
 * \param b An already initialized pointer of structure pool_info whose info is required.
 * \param bitmap Pointer to destination for the current bitmap value of the pool.
 * \param available_blocks Pointer to destination for the number of available blocks in the pool.
 * \param block_size Pointer to destination for the size of each block in the pool.
 *
 * \return 0 if successful.
 * \return -1 if unsuccessful.
 */
int ba_block_pool_info_get(struct pool_info *b, unsigned long *bitmap, unsigned long *available_blocks, unsigned long *block_size);

/** Register the CLI commands for Block/Slab Allocator. */
int ba_cli_init(void);
#endif
