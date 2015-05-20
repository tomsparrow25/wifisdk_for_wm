/*
 * Copyright 2008-2013, Marvell International Ltd.
 * All Rights Reserved.
 */

#include <string.h>
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"
#include "timers.h"
#include "block_alloc.h"
#include "wm_os.h"

#define ffs __builtin_ffs

/* Uncomment/Comment these two flags to turn ON/OFF debug and extended debug messages respectively */

/* #define BLOCK_ALLOC_DEBUG */
/* #define EXTENDED_BLOCK_ALLOC_DEBUG */

#ifdef BLOCK_ALLOC_DEBUG
#include <wmstdio.h>
#define BA_DBG(...)        wmprintf("\nblock_alloc: " __VA_ARGS__)
#else
#define BA_DBG(...)
#endif /* BLOCK_ALLOC_DEBUG */


#ifdef EXTENDED_BLOCK_ALLOC_DEBUG
#include <wmstdio.h>
#define EXT_BA_DBG(...)        wmprintf("\next_block_alloc: " __VA_ARGS__)
#else
#define EXT_BA_DBG(...)
#endif /* EXTENDED_BLOCK_ALLOC_DEBUG */

/* This function returns the free block number if successful. It returns -1 on error */
static int ba_get_free_block(const struct pool_info *b)
{

	int block_num = 0;
	unsigned long bitmap = b->bitmap;

	if (b->block_count == 0)
		return -1;

	block_num = ffs(~(bitmap));
	if (block_num == 0)
		return -1;
	else
		return block_num - 1;
}

/* This function creates the pool for a particular block size */
struct pool_info *ba_pool_create(unsigned long block_size, void *buf, unsigned long pool_size, unsigned char mutex_flag)
{
	int ret, position, block_count;
	struct pool_info *b;
	if (buf == NULL) {
		EXT_BA_DBG("Error.. Input pointer is NULL.\r\n");
		return NULL;
	}

	/* Ensure that the buffer given by user is 4 byte aligned */
	if ((unsigned)buf & 0x3) {
		EXT_BA_DBG("Error:Input pointer needs to be word aligned.\r\n");
		return NULL;
	}

	if (pool_size % block_size) {
		EXT_BA_DBG("Error.. For a block allocator pool size(%d) should be a multiple of block_size(%d) \
			\r\nCannot create the pool.\r\n", pool_size, block_size);
		return NULL;
	}

	block_count = (pool_size / block_size);
	if (block_size >= sizeof(struct pool_info)) {
			position = 1;
	} else {
		position = sizeof(struct pool_info) / block_size;
		if (sizeof(struct pool_info) % block_size) {
			position += 1;
		}
	}
	if (block_count < position + 1) {
		EXT_BA_DBG("Error.. The pool size (currently %d) should atleast be %d times the block_size %d.\r\n", pool_size, position + 1, block_size);
		return NULL;
	}
	b = (struct pool_info *) buf;
	EXT_BA_DBG("Address of b is: %p.\r\n", b);

	if (mutex_flag != 0) {
		ret = os_mutex_create(&b->mutex, "block-alloc", OS_MUTEX_INHERIT);
		if (ret) {
			EXT_BA_DBG("Cannot create the mutex for block allocator.\r\n");
			return NULL;
		}
		EXT_BA_DBG("Created the mutex for block allocator.\r\n");
	} else
		b->mutex = NULL;

	b->head = (char *)b + (block_size * position);
	b->bitmap = 0;
	b->block_size = block_size;
	b->block_count = block_count - position;
	return b;
}

/* This function destroys the pool (if created) */
int ba_pool_delete(struct pool_info *b)
{

	int ret;
	if (b == NULL) {
		EXT_BA_DBG("Error.. Input pointer is NULL.\r\n");
		return -1;
	}
		/* Delete the pool only if all its blocks are unallocated or free */
	if (b->bitmap == 0) {
		if (b->mutex != NULL) {
			ret = os_mutex_get(&b->mutex, OS_WAIT_FOREVER);
			if (ret) {
				EXT_BA_DBG("Failed to get the lock for block allocator.\r\n");
				return -1;
			}
			ret = os_mutex_delete(&b->mutex);
			if (!ret) {
				EXT_BA_DBG("Deleting the mutex for block allocator.\r\n");
			}
		}
		EXT_BA_DBG("Freeing the pool having blocks each of size %d from address %p.\r\n", b->block_size, b->head);
		return 0;
	} else {
		EXT_BA_DBG("Error.. Trying to free a pool which has blocks allocated.\r\n");
		EXT_BA_DBG("Free all the blocks and then try to delete the pool.\r\n");
		return -1;
	}
}

/* This function is used to allocate a free block */
void *ba_block_allocate(struct pool_info *b)
{

	int block_num, ret;
	void *ptr;
	if (b == NULL) {
		BA_DBG("Error.. Input pointer is NULL or not initialized.\r\n");
		return NULL;
	}

	if (b->mutex != NULL) {
		ret = os_mutex_get(&b->mutex, OS_WAIT_FOREVER);

		if (ret) {
			EXT_BA_DBG("Failed to get the lock for block allocator.\r\n");
			return NULL;
		}
		EXT_BA_DBG("Got the lock for block allocator.\r\n");
	}

	/* Check if atleast one free block is available. If not return error. */
	if (b->block_count > 0) {
		/* Get the position of the first free block */
		block_num = ba_get_free_block(b);
		if (block_num == -1) {
			BA_DBG("No free block.. Returning.\r\n");
			if (b->mutex != NULL) {
				os_mutex_put(&b->mutex);
				EXT_BA_DBG("Releasing the lock for block allocator.\r\n");
			}
			return NULL;
		}
		EXT_BA_DBG("Block number to be allocated is: %d.\r\n", block_num);
		ptr = b->head + (b->block_size * block_num);

		/* Update the bitmap by setting the corresponding bit to 1 (indicating allocated) */
		b->bitmap |= (0x01 << block_num);
		EXT_BA_DBG("Updated bitmap is:%ld.\r\n", b->bitmap);
		BA_DBG("Pointer is:%p.\r\n", ptr);

		/* Decrement the count of free blocks by 1 */
		b->block_count--;
		if (b->mutex != NULL) {
			os_mutex_put(&b->mutex);
			EXT_BA_DBG("Releasing the lock for block allocator.\r\n");
		}
		return ptr;
	} else {
		BA_DBG("No free block.. Returning.\r\n");
		if (b->mutex != NULL) {
			os_mutex_put(&b->mutex);
			EXT_BA_DBG("Releasing the lock for block allocator.\r\n");
		}
		return NULL;
	}
}

/* This function is used to free an allocated block */
int ba_block_release(struct pool_info *b, void *alloc)
{
	int ret;
	unsigned int diff;

	if (b == NULL || alloc == NULL) {
		BA_DBG("Error.. Input pointer is NULL or not initialized.\r\n");
		return -1;
	}

	diff = (alloc - (void *)b->head);

	if (diff % b->block_size) {
		BA_DBG("Error.. Invalid address. Cannot free the memory.\r\n");
		return -1;
	} else {
		diff /= b->block_size;
	}

	if (diff >= MAX_BLOCKS) {
		EXT_BA_DBG("Error.. Invalid address. Cannot free the memory.\r\n");
		return -1;
	}
	if (b->mutex != NULL) {
		ret = os_mutex_get(&b->mutex, OS_WAIT_FOREVER);
		if (ret) {
			EXT_BA_DBG("Failed to get the lock for block allocator.\r\n");
			return -1;
		}
		EXT_BA_DBG("Got the lock for block allocator.\r\n");
	}

	EXT_BA_DBG("Block number to be freed is: %d.\r\n", diff);

	/* Check if the specified bit is allocated (1) else return error */
	if (!(b->bitmap & (0x01 << diff))) {
		BA_DBG("Error.. Trying to free a location that is already free.\r\n");
		if (b->mutex != NULL) {
			os_mutex_put(&b->mutex);
			EXT_BA_DBG("Releasing the lock for block allocator.\r\n");
		}
		return -1;
	}

	/* Set the specified bit to 0 indicating that now it is free */
	b->bitmap &= ~(0x01 << diff);

	/* Increment the count of free blocks by 1 */
	b->block_count++;
	EXT_BA_DBG("Updated bitmap is: %ld.\r\n", b->bitmap);
	if (b->mutex != NULL) {
		os_mutex_put(&b->mutex);
		EXT_BA_DBG("Releasing the lock for block allocator.\r\n");
	}
	BA_DBG("Freed the requested block successfully.\r\n");
	return 0;
}

int ba_block_pool_info_get(struct pool_info *b, unsigned long *bitmap, unsigned long *available_blocks, unsigned long *block_size)
{
	if (b == NULL) {
		BA_DBG("Error.. Input pointer is NULL.\r\n");
		return -1;
	}
	*bitmap = b->bitmap;
	*available_blocks = b->block_count;
	*block_size = b->block_size;
	return 0;
}
