/*! \file psm.h
 * \brief Persistent Storage Manager
 * 
 * Persistent Storage Manager (PSM) is meant to store configuration variables
 * in a NAME=VALUE format on the flash. Variables can be grouped together into modules.
 * For instance, a module named "network" can store all network related variables:
 * ip_addr, gateway, etc. 
 * These variables will be stored as :
 * WMC.network.ip_addr=192.168.1.1
 *
 * The available flash space is divided into fixed size partitions for faster
 * operation. At any given time one partition can be loaded into memory.
 * The user can choose which module lives on which partition. This decision is 
 * conveyed in the psm_register_module() call, using the \a partition_key parameter.
 *
 * \note A psm transaction starts with psm_open() and ends with psm_close(). A mutex is 
 * taken in psm_open() and released in psm_close(). Make sure that there are no 
 * time-consuming operations between psm_open() and psm_close(), otherwise, other threads
 * will block on psm operations.
 *
 * \note When a variable is changed, usually, the change is made only in the in-memory
 * buffer. The actual write to flash happens in psm_close(). You can use
 * psm_commit() to sync this buffer to flash manually.
 */
/*  Copyright 2008-2013, Marvell International Ltd.
 *  All Rights Reserved.
 */

/* -*- linux-c -*- */
/* psm -- Persistent Storage Manager */
#ifndef _PSM_H_
#define _PSM_H_

#include <wm_os.h>
#include <wmtypes.h>
#include <arch/psm.h>
#include <flash.h>
#include <wmlog.h>

#define psm_e(...)				\
	wmlog_e("psm", ##__VA_ARGS__)
#define psm_w(...)				\
	wmlog_w("psm", ##__VA_ARGS__)

#ifdef CONFIG_PSM_DEBUG
#define psm_d(...)				\
	wmlog("psm", ##__VA_ARGS__)
#else
#define psm_d(...)
#endif

/** PSM Error Codes **/
enum wm_psm_errno {
	WM_E_PSM_ERRNO_BASE = MOD_ERROR_START(MOD_PSM),
	/* Incorrect CRC of PSM Metadata */
	WM_E_PSM_METADATA_CRC,
};

/** The psm in-memory buffer of one partition */
typedef struct psm_partition_cache {
	/** Holds partition data */
	char buffer[PARTITION_SIZE];
	/** Partition number */
	int partition_num;
} psm_partition_cache_t;
extern psm_partition_cache_t gpsm_partition_cache;
extern flash_desc_t psm_fl;

/** The signature that recognizes that a flash block belongs to psm */
#define PSM_SIGNATURE "PSM1"
#define PRODUCT_NAME "WMC"
/** Used to handle psm variable structure reorganisations in case of 
 * upgrades
 */
#define PSM_PROGRAM_VERSION 1

/** Flag to ask psm to create the partition if it doesn't exist.
 * Used with psm_register_module()
 */
#define PSM_CREAT 1

/* Note the product_name + module_name + var_name max sizes add up to MORE than 64. 
 * But thats okay since they are just MAX sizes 
 */

/** The maximum size of the fully qualified variable name including the
 * trailing \\0. Fully qualified variable name is:
 * \<product_name\>.\<module_name\>.\<variable_name\>
 */
#define FULL_VAR_NAME_SIZE 64
/** Maximum size of the variable name including the trailing \\0 */
#define VAR_NAME_SIZE      32
/** Maximum size of the module name including the trailing \\0 */
#define MODULE_NAME_SIZE   32
/** Maximum size of the partition name including the trailing \\0 */
#define PARTITION_KEY_SIZE 12

/** mutex for protecting critical sections in psm */
extern os_mutex_t psm_mutex;

typedef struct qstr {
	unsigned char *name;
	unsigned long int len;
} qstr_t;

/** The psm handle 
 * 
 * This structure is returned on performing a psm_open(). It keeps track of the
 * current  state of the psm
 */
typedef struct psm_handle {
        /** The module name */
	char   psmh_mod_name[32];
        /** Structure to store the env */
	qstr_t psmh_env;
        /** Is the in-memory buffer dirty? */
	short  psmh_dirty;
        /** That partition that we have read into */
	short  psmh_partition_id;
        /** Other psm flags */
	short  psmh_flags;
} psm_handle_t;

/** The psm metadata format
 * 
 * This is the psm metadata that is stored on the first partition of the psm flash
 * sector
 */
typedef struct psm_metadata {
	/** Signature of metadata */
	unsigned int signature;
	/** CRC of metadata  */
	unsigned int crc;
	/** version of metadata */
	unsigned int version;
	#define METADATA_ENV_SIZE (PARTITION_SIZE - 3*sizeof(unsigned int))
	/** Holds contents of metadata blocks */
	unsigned char vars[METADATA_ENV_SIZE];
} psm_metadata_t;

/** The psm variable header
 *
 * This is the psm header that is stored on all subsequent partitions (apart
 * from the first) of the psm flash sector
 */
typedef struct psm_var_block {
	/** CRC of variable header of PSM  */
	unsigned int crc;
	#define ENV_SIZE (PARTITION_SIZE - sizeof(unsigned int))
	/** Holds environment variables of the PSM block */
	unsigned char vars[ENV_SIZE];
} psm_var_block_t;

/** Register a psm module
 * 
 * This function registers a module name with the Persistent Storage
 * Manager. The name is associated with a partition key. 
 *
 * \note All variables belonging to this module name will be stored in the
 * partition identified by partition key. Multiple modules registered with the
 * same partition key will be stored in the same partition.
 *
 * \note module_name is truncated to \ref MODULE_NAME_SIZE and partition_key is
 * truncated to \ref PARTITION_KEY_SIZE.
 * 
 * \param module_name The name of the module being registered.
 * \param partition_key A string value, two same string values
 * identify the same partition.
 * \param flags Typically \ref PSM_CREAT to register the module name,
 * if it doesn't already exist
 *
 * \return 0 Sucessfully registered the module name
 * \return EEXIST The module name has already been registered
 * \return ENOENT The module name hasn't been registered and the
 * \ref PSM_CREAT was not set.
 */
int psm_register_module(const char *module_name, 
			const char *partition_key, short flags);

/** PSM open handle
 * 
 * This function informs the psm to open a handle for a specified module
 * name. The partition that the module belongs to is also loaded into the
 * memory. 
 *
 * \note The module name should already be registered with psm.
 * \note The psm handle that is returned is associated with the module name that
 * was opened.
 * \note Only one partition can be loaded into memory at a given time. Consider
 * this point when accessing variables belonging to multiple partitions
 * simultaneously.
 *
 * \param handle The psm handle that is returned in response to the open. This
 * is an out parameter.
 * \param module_name The name of the module being opened
 *
 * \return 0 if the successful. Non-zero otherwise.
 */
int psm_open(psm_handle_t *handle, const char *module_name);

/** PSM close handle
 * 
 * This function closes a handle that was opened with \ref psm_open.
 *
 * \note This function typically flushes out any modifications made to the
 * in-memory  psm buffer to flash.
 *
 * \note module_name is truncated to \ref MODULE_NAME_SIZE
 * 
 * \param handle The psm handle that should be closed.
 *
 */
void psm_close(psm_handle_t *handle);

/** Set psm variable
 * 
 * This function sets the value of a variable in psm.
 *
 * \note The Persistent Storage Manager builds a variable namespace based on the
 * module name and the variable name. Typically the variables are stored as:
 * WMC.\<module_name\>.\<variable_name\>. Thus two modules with
 * same variable names refer to distinct variables.
 * 
 * \note The modifications made by psm_set are not immediately flushed to flash
 * but are reflected in the in-memory psm buffer. These are flushed to the flash
 * in response to a \ref psm_commit or \ref psm_close.
 *
 * \note The variable name is truncated to \ref VAR_NAME_SIZE.
 *
 * \note The maximum size of value that can be stored is 
 * ~(PARTITION_SIZE - length(fully qualified variable name ) - 3 (for 2 \\0s and '='))
 * This maximum size keeps on reducing as new variables are written to that partition. 
 *
 * \param handle An open psm handle that is associated with a module name.
 * \param variable The name of the variable to modify
 * \param value The value that should be assigned to this variable.
 *
 * \return 0 if the successful. Non-zero otherwise.
 */
int psm_set(psm_handle_t *handle, const char *variable, const char *value);

/** Get psm variable
 *
 * This functions retrieves the value of a variable from psm.
 * 
 * \note The variable name is truncated to \ref VAR_NAME_SIZE.
 *
 * \param handle An open psm handle that is associated with a module name.
 * \param variable The name of the variable to read
 * \param value Pointer to a buffer that will hold the value of the
 * variable. This is an out parameter.
 * \param value_len Length of the buffer that is passed above.
 *
 * \return 0 if the successful. Non-zero otherwise.
 */
int psm_get(psm_handle_t *handle, const char *variable, 
	    char *value, int value_len);

/** Commit psm modifications
 * 
 * This functions writes any modifications made to the in-memory psm buffer to
 * flash.
 *
 * \param handle An open psm handle that is associated with a module name.
 *
 * \return -WM_E_INVAL if invalid parameters passed.
 * \return WM_SUCCESS if operation success.
 */
int psm_commit(psm_handle_t *handle);

/** Get the amount of free space available for the current module
 *
 * \param handle An open psm handle that is associated with a module name.
 *
 * \return 0 amount of free space, if successful. Negative otherwise.
 */
int psm_get_free_space(psm_handle_t *handle);

/** Erase and format a psm flash block
 *
 * This function erases a psm flash block and formats it with the various
 * psm headers.
 *
 * \return -WM_FAIL in case of erase failure.
 * \return WM_SUCCESS if operation successful.
 */
int psm_erase_and_init();

/** Erase and format a specific psm partition
 *
 * This function erases a psm partition specified by the user.
 *
 * \param partition_id The partition number to be erased.
 *
 * \return 0 if successful. Negative otherwise.
 */
int psm_erase_partition(short partition_id);

/** Get the partition id from the module name
 *
 * This function retrieves psm partition id from the module name.
 *
 * \param module_name The module name whose partition id is to be fetched.
 *
 * \return partition id if successful. Negative otherwise.
 */
int psm_get_partition_id(const char *module_name);

/** Initialize the Persistent Storage Manager
 * 
 * This function initializes the Persistent Storage Manager module.
 *
 * \param[in] fl The flash descriptor for PSM
 *
 * \return -WM_FAIL if operations fails.
 * \return WM_SUCCESS if operation is successful.
 */ 
int psm_init(flash_desc_t *fl);

/** Initialize the Persistent Storage Manager CLI
 * 
 * This function initializes the command line interface for the Persistent
 * Storage Manager module. 
 * \note This function can only be called if the psm module is initialized
 *
 * \return 1 if failed to init psm cli.
 * \return 0 if init successful.
 */ 
int psm_cli_init(void);
/***** Utility functions *****/
int util_search_var(unsigned char *env, int size, const char *variable, 
		    char *value, int val_len);
int util_del_var(unsigned char *environment, int size, 
		 const char *variable, int *delete_len);
int util_set_var(unsigned char *environment, int size, 
		 const char *variable, const char *value);

/*** Functions NOT exported *****/
int psm_read_partition(short partition_id, psm_partition_cache_t *);
int psm_write_partition(psm_partition_cache_t *);
int psm_assign_partition(psm_metadata_t *metadata, char *partition_key);
int __psm_get_partition_id(const char *module_name);

int psm_get_mutex();
int psm_put_mutex();

/*** The following are SPI IO functions that every board should map to its appropriate drivers */
int psm_spi_write(uint32_t address, char *buffer, uint32_t length);
int psm_spi_read(uint32_t address, char *buffer, uint32_t length);
int psm_spi_erase_all();
void psm_spi_init(void);

/**
 * Read a single variable value from the PSM
 *
 * This function is a helper function to read a variable value in a single
 * API instead of following the open, get, close sequence.
 *
 * \param[in] module The PSM module
 * \param[in] variable Name of the variable whose value is to be retrieved.
 * \param[in, out] value Pointer to allocated buffer. The value will be
 * stored here.
 * \param[in] max_len The length of the input buffer. If this is less than
 * count required by the variable value then error will be returned.
 *
 * \return -WM_FAIL if operation failed.
 * \return WM_SUCCESS if operation successful.
 */
int psm_get_single(const char *module, const char *variable,
		   char *value, unsigned max_len);

/**
 * Write a single variable value to the PSM
 *
 * This function is a helper function to write a variable value in a single API
 * instead of following the open, set, close sequence. Please ensure that this
 * function is only used in situations when only one variable is to be set. If
 * multiple variables are to be set one after another then please perform a
 * psm_open, psm_set, psm_set, ..., psm_close sequence, as it a more efficient
 * and causes less wear of the flash.
 *
 * \param[in] module The PSM module
 * \param[in] variable Name of the variable whose value is to be set.
 * \param[in, out] value Pointer to buffer having the value.
 *
 * \return -WM_FAIL if operation failed.
 * \return WM_SUCCESS if operation successful.
 */
int psm_set_single(const char *module, const char *variable, const char *value);

extern int reset_to_factory;
#endif /* !  _PSM_H_ */
