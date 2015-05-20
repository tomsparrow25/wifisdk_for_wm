/* 
 *  Copyright 2008-2013, Marvell International Ltd.
 *  All Rights Reserved.
 */

/** psm_partition.c: Managing the partitions of psm
 */
#include <stdio.h>
#include <stdlib.h>

#include <wmstdio.h>
#include <psm.h>
#include <crc32.h>

typedef struct mod_partition_mapping_cache {
	char module_name[MODULE_NAME_SIZE];
	int partition_id;
} mod_partition_mapping_cache_t;
#define	MOD_PART_CACHE_SIZE	8
static mod_partition_mapping_cache_t mapping_cache[MOD_PART_CACHE_SIZE];
static int mapping_cache_index = 0;

void psm_clear_mapping_cache()
{
	int i;
	for (i = 0; i < MOD_PART_CACHE_SIZE; i++)
		mapping_cache[i].partition_id = 0;
}

static int mapping_cache_lookup(const char *module_name)
{
	int i;
	for (i = 0; i < MOD_PART_CACHE_SIZE; i++) {
		if (mapping_cache[i].partition_id == 0)
			return -1;
		else if (!strcmp(mapping_cache[i].module_name, module_name))
			return mapping_cache[i].partition_id;
	}
	return -WM_FAIL;
}

static void mapping_cache_add(const char *module_name, int partition)
{
	strcpy(mapping_cache[mapping_cache_index].module_name, module_name);
	mapping_cache[mapping_cache_index].partition_id = partition;
	mapping_cache_index = (mapping_cache_index + 1) % MOD_PART_CACHE_SIZE;
}

static void psm_crc_calculate(int partition_id, char *buffer)
{
	psm_metadata_t *metadata = (psm_metadata_t *) buffer;
	psm_var_block_t *var_block = (psm_var_block_t *) buffer;

	if (partition_id == 0)
		metadata->crc = crc32(metadata->vars, METADATA_ENV_SIZE, 0);
	else
		var_block->crc = crc32(var_block->vars, ENV_SIZE, 0);

}

static int psm_crc_verify(int partition_id, char *buffer)
{
	psm_metadata_t *metadata = (psm_metadata_t *) buffer;
	psm_var_block_t *var_block = (psm_var_block_t *) buffer;
	unsigned int crc;

	if (partition_id == 0) {
		crc = crc32(metadata->vars, METADATA_ENV_SIZE, 0);
		if (crc != metadata->crc) {
			psm_e("CRC mismatch while "
				"reading partition %d. Expected: %x, Got: %x",
				partition_id, metadata->crc, crc);
			return -WM_E_PSM_METADATA_CRC;
		}
	} else {
		crc = crc32(var_block->vars, ENV_SIZE, 0);
		if (crc != var_block->crc) {
			psm_e("CRC mismatch while "
				"reading partition %d. Expected: %x, Got: %x",
				partition_id, var_block->crc, crc);
			return -WM_E_CRC;
		}
	}

	return WM_SUCCESS;

}

int psm_read_partition(short partition_id,
		       psm_partition_cache_t *partition_cache)
{
	unsigned long int base_address;
	int ret;
	if (partition_id == partition_cache->partition_num)
		return 0;
	base_address = psm_fl.fl_start + (partition_id) * PARTITION_SIZE;
	ret = psm_spi_read(base_address, partition_cache->buffer,
		PARTITION_SIZE);
	if (ret != WM_SUCCESS)
		return -WM_FAIL;
	ret = psm_crc_verify(partition_id, partition_cache->buffer);
	if (ret) {
		/* Incorrect CRC */
		partition_cache->partition_num = -1;
	} else {
		partition_cache->partition_num = partition_id;
	}
	return ret;
}

int psm_write_partition(psm_partition_cache_t *partition_cache)
{
	unsigned long int base_address;
	if (partition_cache->partition_num < 0 ||
	    partition_cache->partition_num >= psm_fl.fl_size / PARTITION_SIZE)
		return -WM_E_INVAL;
	base_address = psm_fl.fl_start + (partition_cache->partition_num)
			* PARTITION_SIZE;
	psm_crc_calculate(partition_cache->partition_num,
			  partition_cache->buffer);
	return psm_spi_write(base_address, partition_cache->buffer,
		PARTITION_SIZE);
}

/* Return which partition the module is stored on */
int __psm_get_partition_id(const char *module_name)
{
	psm_metadata_t *metadata;
	char partition_key[PARTITION_KEY_SIZE], value[32];
	int ret, partition;

	partition = mapping_cache_lookup(module_name);
	if (partition > 0)
		return partition;

	ret = psm_read_partition(0, &gpsm_partition_cache);
	if (ret)
		return ret;

	metadata = (psm_metadata_t *) gpsm_partition_cache.buffer;

	ret = util_search_var(metadata->vars, METADATA_ENV_SIZE,
			module_name, partition_key, PARTITION_KEY_SIZE);
	if (ret == WM_SUCCESS) {
		ret = util_search_var(metadata->vars, METADATA_ENV_SIZE,
				partition_key, value, 32);
		if (ret != WM_SUCCESS)
			return ret;
		partition = atoi(value);
		mapping_cache_add(module_name, partition);
		return partition;
	} else
		return ret;

}

int psm_get_partition_id(const char *module_name)
{
	int ret;
	ret = psm_get_mutex();
	if (ret == WM_SUCCESS)
		ret = __psm_get_partition_id(module_name);
	psm_put_mutex();
	return ret;
}

/* Assign a partiton to a module. Create the partition if necessary */
int psm_assign_partition(psm_metadata_t *metadata, char *partition_key)
{
	char value[4], *val_ptr = value;
	int intval;

	util_search_var(metadata->vars, METADATA_ENV_SIZE, "last_used", val_ptr,
			4);
	intval = atoi(val_ptr);
	intval++;
	if (intval > (psm_fl.fl_size / PARTITION_SIZE - 1))
		return -WM_E_NOSPC;
	snprintf(val_ptr, 4, "%d", intval);
	util_set_var(metadata->vars, METADATA_ENV_SIZE, "last_used", val_ptr);
	return intval;
}
