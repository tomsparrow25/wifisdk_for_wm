/* 
 *  Copyright 2008-2013, Marvell International Ltd.
 *  All Rights Reserved.
 */

/** psm.c: The psm API
 */

#include <stdio.h>
#include <string.h>

#include <wm_os.h>
#include <psm.h>
#include <arch/boot_flags.h>
#include <wmerrno.h>
#include <mdev.h>
#include <flash.h>
#include <crc32.h>
#include <wmassert.h>
#include <wmerrno.h>

static mdev_t * fl_dev;
psm_partition_cache_t gpsm_partition_cache;
os_mutex_t psm_mutex;
int reset_to_factory;

flash_desc_t psm_fl;

void psm_clear_mapping_cache();

int psm_get_single(const char *module, const char *variable,
		   char *value, unsigned max_len)
{
	ASSERT(module != NULL);
	ASSERT(variable != NULL);
	ASSERT(value != NULL);
	ASSERT(max_len != 0);

	psm_handle_t handle;
	int status = psm_open(&handle, module);
	if (status != WM_SUCCESS) {
		psm_e("Open failed. Unable to read variable %s. "
			"Status: %d", variable, status);
		return -WM_FAIL;
	}

	status = psm_get(&handle, variable, value, max_len);
	psm_close(&handle);
	return status;
}

int psm_set_single(const char *module, const char *variable, const char *value)
{
	ASSERT(module != NULL);
	ASSERT(variable != NULL);
	ASSERT(value != NULL);

	psm_handle_t handle;
	int status = psm_open(&handle, module);
	if (status != WM_SUCCESS) {
		psm_e("Open failed. Unable to write variable %s. "
			"Status: %d", variable, status);
		return -WM_FAIL;
	}

	status = psm_set(&handle, variable, value);
	if (status != WM_SUCCESS) {
		psm_w("Unable to write variable %s. Value: %s"
			"Status: %d.", variable, value, status);
	}

	psm_close(&handle);
	return status;
}

static void __psm_clear_buffer(void *source, unsigned long int len)
{
	memset(source, 0xff, len);
}

/* full_name is assumed to be of FULL_VAR_NAME_SIZE */
static int __psm_get_full_varname(psm_handle_t *handle, const char *short_name,
				  char full_name[])
{
	if (snprintf(full_name, FULL_VAR_NAME_SIZE,
		PRODUCT_NAME ".%s.%s", handle->psmh_mod_name,
		short_name) > (FULL_VAR_NAME_SIZE - 1))
		return -WM_E_INVAL;
	return WM_SUCCESS;
}

int psm_get_free_space(psm_handle_t *handle)
{
	int length = 0;

	if (handle == NULL)
		return -WM_E_INVAL;

	while (length < (handle->psmh_env.len - 1)) {
		if (handle->psmh_env.name[length] == '\0' &&
		    handle->psmh_env.name[length + 1] == '\0')
			return (handle->psmh_env.len - length - 2);
		length++;
	}

	/* Shouldn't reach here */
	return -WM_E_IO;
}

int psm_set(psm_handle_t *handle, const char *variable, const char *value)
{
	char full_name[FULL_VAR_NAME_SIZE];
	int ret;

	if (handle == NULL || variable == NULL || value == NULL)
		return -WM_E_INVAL;

	ret = __psm_get_full_varname(handle, variable, full_name);
	if (ret == WM_SUCCESS) {
		ret = util_set_var(handle->psmh_env.name, handle->psmh_env.len,
				 full_name, value);
		handle->psmh_dirty = ret ? 0 : 1;
		if (!ret)
			return ret;
	} else
		return ret;
	return WM_SUCCESS;
}

int psm_get(psm_handle_t *handle, const char *variable, char *value, int val_len)
{
	char full_name[FULL_VAR_NAME_SIZE];
	int ret;

	if (handle == NULL || variable == NULL)
		return -WM_E_INVAL;

	ret = __psm_get_full_varname(handle, variable, full_name);
	if (ret == WM_SUCCESS)
		ret = util_search_var(handle->psmh_env.name,
			handle->psmh_env.len, full_name, value, val_len);
	return ret;
}

int psm_register_module(const char *module_name, const char *partition_key,
			short flags)
{
	psm_metadata_t *metadata;
	int ret, partition_id;
	char module[MODULE_NAME_SIZE], partition[PARTITION_KEY_SIZE];
	char value[PARTITION_KEY_SIZE], *val_ptr = value;

	ret = psm_get_mutex();
	if (ret != WM_SUCCESS)
		return ret;

	/* Truncate to required size */
	snprintf(module, MODULE_NAME_SIZE, "%s", module_name);
	snprintf(partition, PARTITION_KEY_SIZE, "%s", partition_key);

	ret = psm_read_partition(0, &gpsm_partition_cache);
	if (ret)
		goto error_path;

	metadata = (psm_metadata_t *) gpsm_partition_cache.buffer;

	/* Check whether the module already exists */
	ret = util_search_var(metadata->vars, METADATA_ENV_SIZE, module,
		val_ptr, PARTITION_KEY_SIZE);
	if (ret == 0) {
		ret = -WM_E_EXIST;
		goto error_path;
	}

	/* Module doesn't exist. check whether partition exists. */
	/* Truncate the partition_key */
	ret = util_search_var(metadata->vars, METADATA_ENV_SIZE, partition,
			    val_ptr, 32);
	if (ret == -WM_E_INVAL) {	/* Partition doesn't exist. */
		if (flags & PSM_CREAT) {
			partition_id = psm_assign_partition(metadata,
						partition);
			if (partition_id <= 0) {
				ret = partition_id;	/* return error */
				goto error_path;
			}
			snprintf(val_ptr, 32, "%d", partition_id);
			util_set_var(metadata->vars, METADATA_ENV_SIZE,
				     partition, val_ptr);
		} else {
			ret = -WM_E_NOENT;
			goto error_path;
		}
	} else if (ret != 0)
		goto error_path;

	/* At this point, the partition is created or already exists and the module doesn't exist. Create it */
	util_set_var(metadata->vars, METADATA_ENV_SIZE, module, partition);
	psm_write_partition(&gpsm_partition_cache);
	ret = WM_SUCCESS;

error_path:
	psm_put_mutex();
	return ret;
}

int psm_open(psm_handle_t *handle, const char *module_name)
{
	short partition_id;
	int ret;
	psm_var_block_t *psm_vars;

	ret = psm_get_mutex();
	if (ret != 0)
		return ret;

	if (module_name)
		snprintf(handle->psmh_mod_name, MODULE_NAME_SIZE, "%s",
			 module_name);
	handle->psmh_dirty = 0;

	partition_id = __psm_get_partition_id(module_name);
	if (partition_id > 0) {
		handle->psmh_partition_id = partition_id;
		ret = psm_read_partition(handle->psmh_partition_id,
			&gpsm_partition_cache);	/* CRC check will be internal */
		if (ret) {
			psm_put_mutex();
			return ret;
		}
		psm_vars = (psm_var_block_t *) gpsm_partition_cache.buffer;
		handle->psmh_env.name = psm_vars->vars;
		handle->psmh_env.len = ENV_SIZE;
	} else {
		/* Return error */
		psm_put_mutex();
		return partition_id;
	}
	return WM_SUCCESS;
}

int psm_commit(psm_handle_t *handle)
{
	int ret;
	if (!handle->psmh_dirty)
		return WM_SUCCESS;

	/* Write to flash */
	ret = psm_write_partition(&gpsm_partition_cache);
	handle->psmh_dirty = 0;
	return ret;
}

void psm_close(psm_handle_t *handle)
{
	psm_commit(handle);
	psm_put_mutex();
}

static void psm_var_block_init(psm_partition_cache_t *partition_cache)
{
	psm_var_block_t *psm_var_block =
		(psm_var_block_t *) partition_cache->buffer;
	__psm_clear_buffer(psm_var_block, PARTITION_SIZE);
	psm_var_block->crc = 0x0000;
	psm_var_block->vars[0] = '\0';
	psm_var_block->vars[1] = '\0';
}

static int __psm_init_flash_partition(short partition_id)
{
	gpsm_partition_cache.partition_num = partition_id;
	psm_var_block_init(&gpsm_partition_cache);
	return psm_write_partition(&gpsm_partition_cache);
}

static int __psm_init_flash()
{
	psm_metadata_t *psm_metadata = (psm_metadata_t *)
					gpsm_partition_cache.buffer;
	/* First partition shall be written out of loop; hence -1 */
	int npart = (psm_fl.fl_size / PARTITION_SIZE) - 1;

	/* Write all partitions except partition 0 */
	psm_var_block_init(&gpsm_partition_cache);
	while (npart > 0) {
		gpsm_partition_cache.partition_num = npart;
		psm_write_partition(&gpsm_partition_cache);
		npart--;
	}

	/* Write partition 0 i.e. metadata partition */
	gpsm_partition_cache.partition_num = 0;
	__psm_clear_buffer(psm_metadata, PARTITION_SIZE);
	memcpy(&psm_metadata->signature, PSM_SIGNATURE,
	       sizeof(PSM_SIGNATURE) - 1);
	psm_metadata->crc = 0x00;
	psm_metadata->version = PSM_PROGRAM_VERSION;
	psm_metadata->vars[0] = '\0';
	psm_metadata->vars[1] = '\0';
	util_set_var(psm_metadata->vars, sizeof(psm_metadata->vars),
		     "last_used", "0");
	psm_write_partition(&gpsm_partition_cache);

	return WM_SUCCESS;

}

int psm_erase_partition(short partition_id)
{
	int ret;

	ret = psm_get_mutex();
	if (ret != 0) {
		psm_e("psm_get_mutex failed with : %d", ret);
		return -WM_FAIL;
	}

	if (partition_id >= psm_fl.fl_size / PARTITION_SIZE ||
		partition_id <= 0) {
		psm_e("incorrect partition id for psm erase\r\n"
			"    partition_id > 0 and < %d ",
			psm_fl.fl_size / PARTITION_SIZE);
		ret = -WM_FAIL;
	} else
		ret = __psm_init_flash_partition(partition_id);

	psm_put_mutex();
	return ret;
}

/* Called in case of flash errors or new flash or Reset to Factory */
int psm_erase_and_init()
{
	int ret;
	ret = flash_drv_erase(fl_dev, psm_fl.fl_start, psm_fl.fl_size);
	if (ret != WM_SUCCESS)
		return -WM_FAIL;
	/* Flush cache since we are clearing the mappings in the backend */
	psm_clear_mapping_cache();
	return __psm_init_flash();
}

int psm_spi_write(uint32_t address, char *buffer, uint32_t length)
{
	int ret;

	if (address < (psm_fl.fl_start) ||
	    (address + length) > (psm_fl.fl_start + psm_fl.fl_size))
		return -WM_FAIL;

	ret = flash_drv_erase(fl_dev, address, length);
	if (ret != WM_SUCCESS)
		return -WM_FAIL;

	return flash_drv_write(fl_dev, (uint8_t *)buffer, length, address);
}

int psm_spi_read(uint32_t address, char *buffer, uint32_t length)
{
	if (address < (psm_fl.fl_start) ||
	    (address + length) > (psm_fl.fl_start + psm_fl.fl_size))
		return -WM_FAIL;

	return flash_drv_read(fl_dev, (uint8_t *)buffer, length, address);
}

static int psm_check_version_compatibility(psm_metadata_t *psm_metadata)
{
	/* The return value of this function determines whether the psm
	 * partition needs to be reinitialized or not. Use this function to
	 * clear psm partition in case of an upgrade that needs restructuring
	 * of psm variables.
	 */
	if (PSM_PROGRAM_VERSION == psm_metadata->version)
		return WM_SUCCESS;
	else if (PSM_PROGRAM_VERSION < psm_metadata->version)
		return -WM_FAIL;

	/* If a program number bump needs a psm erase, just add a condition
	 * here similar to this one:
	 * if (PSM_PROGRAM_VERSION == 5) return 1;
	 */
	return WM_SUCCESS;
}

int psm_actual_init()
{
	psm_metadata_t *psm_metadata;
	int ret;

	psm_d("init");

	/* Create the psm-mutex */
	ret = os_mutex_create(&psm_mutex, "psm-mutex", OS_MUTEX_INHERIT);
	if (ret != WM_SUCCESS) {
		psm_e("psm_mutex_create failed with: %d", ret);
		return ret;
	}

	if (boot_factory_reset()) {
		reset_to_factory = 1;
		psm_w("Reset to Factory action (wait..)");
		return psm_erase_and_init();
	}

	/* Check whether signature is valid */
	ret = psm_read_partition(0, &gpsm_partition_cache);
	psm_metadata = (psm_metadata_t *) gpsm_partition_cache.buffer;

	if (ret && psm_metadata->crc == 0xffffffff) {
		psm_w("CRC mismatch "
			 "because psm block is empty");
		psm_w("First boot? Formatting..");
	} else if (ret)
		psm_w("Attempting to recover from CRC error. "
			"Formatting..");

	if (ret ||
	    memcmp(&psm_metadata->signature, PSM_SIGNATURE,
		   sizeof(PSM_SIGNATURE) - 1)) {
		psm_d("erase and init (wait..)");
		return psm_erase_and_init();
	}

	ret = psm_check_version_compatibility(psm_metadata);
	if (ret < 0) {
		psm_e("The psm variables have been "
			"created by a newer version of the program.");
		psm_e("Version on flash: %d, "
			"Version of the program loaded: %d",
		     psm_metadata->version, PSM_PROGRAM_VERSION);
		return -WM_FAIL;
	} else if (ret > 0) {
		psm_d("erase and init (wait..)");
		return psm_erase_and_init();
	}

	return WM_SUCCESS;
}

int psm_init(flash_desc_t *fl)
{
	int ret;
	static char init_done;

	if (init_done)
		return WM_SUCCESS;

	psm_fl = *fl;

	/* Initialise internal/external flash memory */
	flash_drv_init();

	/* CRC driver init is required by the psm */
	crc32_init();

	/* Open the flash */
	fl_dev = flash_drv_open(psm_fl.fl_dev);

	if (fl_dev == NULL) {
		psm_e("Flash driver init is required before open");
		return -WM_FAIL;
	}

	/* Initialise the cache to NULL */
	gpsm_partition_cache.partition_num = -1;

	/* Create the psm-mutex and verify flash layout */
	ret = psm_actual_init();
	if (ret == WM_SUCCESS)
		init_done = 1;
	return ret;
}
