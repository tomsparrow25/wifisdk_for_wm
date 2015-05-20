/*! \file json.h
 * \brief The JSON Module
 *
 * The JSON module is used to a) generate JSON strings or b) to parse JSON
 * strings.
 *
 * \section json_usage Usage
 *
 * <h2>JSON Parser</h2>
 * The JSON data can be viewed as a tree of key-value pairs. Composite objects
 * indicate special nodes within the tree that anchor other subtrees. 
 * 
 * So, for the JSON data that looks like the following:
 * \code
 * { "name" : "John Travolta",
 *   "roll" : 123,
 *   "address" : { "house" : "Atlas",
 *                 "street" : "New Avenue",
 *                 "pin" : 39065,
 *               }
 * }
 * \endcode
 *
 * can be thought of as a tree with a root node that has 3 children "name",
 * "roll" and "address". The node at "address" further has 3 children nodes
 * "house", "street" and "pin".
 * 
 * Using this JSON module you can parse the above elements with the following C
 * code: 
 *
 * \code
 *   // Intialise the JSON object 
 * json_object_init(&json_obj, buf_ptr);
 *
 * json_get_val_str(&json_obj, "name", buf_name, sizeof(buf_name)); 
 * // buf_name will now contain "John Travolta".
 *
 * json_get_val_str(&json_obj, "house", buf_house, sizeof(buf_house));
 * // This call will fail since "house" is not within the current search depth.
 *
 * json_get_composite_object(&json_obj, "address");
 * // Sets the search depth to search within the "address" composite object
 *
 * json_get_val_str(&json_obj, "house", buf_house, sizeof(buf_house));
 * // This will now succeed and return the value "Atlas"
 *
 * json_get_val_str(&json_obj, "name", buf_name, sizeof(buf_name)); 
 * // This will fail since we are currently searching restrictively in the
 * // "address subtree only
 *
 * json_release_composite_object(&json_obj);
 * // This sets the search depth up by one level
 *
 * json_get_val_str(&json_obj, "name", buf_name, sizeof(buf_name)); 
 * // This call now succeeds since we are at the appropriate search depth.
 *
 * \endcode
 *
 * <h2>JSON Generator</h2>
 * The JSON Generator can create various JSON strings. Here is an example:
 *
 * \code
 * json_str_int(&jstr, buff, sizeof(buff), 0);
 * // This initializes the json generator to generate string in the buffer buff
 *
 * json_start_object(&jstr);
 * json_set_val_str(&jstr, "name", "John Travolta");
 * json_set_val_int(&jstr, "roll", 123);
 * json_push_object(&jstr, "address");
 * json_set_val_str(&jstr, "house", "Atlas");
 * json_set_val_str(&jstr, "street", "New Avenue");
 * json_set_val_int(&jstr, "ping", 39065);
 * json_pop_object(&jstr);
 * json_close_object(&jstr);
 *
 * \endcode 
 * will create the above mentioned JSON string with no formatting.
 *
 * Similarly arrays can be created as:
 * \code
 * json_start_object(&jstr);
 * json_push_array_object(&jstr, "roll");
 * json_set_array_int(&jstr, 10);
 * json_set_array_int(&jstr, 20);
 * json_set_array_int(&jstr, 30);
 * json_pop_array_object(&jstr);
 * json_close_object(&jstr);
 * \endcode
 * will create the JSON string
 *
 * \code
 * { "roll": [ [ 10, 20, 30] ] } 
 * \endcode
 */

/*
 * Copyright 2008-2013, Marvell International Ltd.
 * All Rights Reserved.
 */

/* -*- linux-c -*- */
#ifndef _JSON_H_
#define _JSON_H_
#include <stdint.h>
#ifdef JSON_LINUX_TEST
#include "./wmerrno.h"
#else
#include <wmerrno.h>
#endif
/** Json Error Codes */
enum wm_json_errno {
	WM_E_JSON_ERRNO_BASE = MOD_ERROR_START(MOD_JSON),
	/** Json buffer overflow */
	WM_E_JSON_OBUF,
	/** Generic json error*/
	WM_E_JSON_FAIL,
	/** Array is empty*/
	WM_E_JSON_EARR,
	/** Invalid json object*/
	WM_E_JSON_INVOBJ,
	/** Invalid json document*/
	WM_E_JSON_INVDOC,
	/** Invalid json start*/
	WM_E_JSON_INVSTART,
	/** Json composite Member not found*/
	WM_E_JSON_CO_MNF,
	/** Json array member not found*/
	WM_E_JSON_ARR_MNF,
	/** Json string not found*/
	WM_E_JSON_STR_NF,
	/** Json number not found*/
	WM_E_JSON_NUM_NF,
	/** Json given buffer not sufficient */
	WM_E_JSON_BUFF_TOO_SMALL
};

#define MAX_JSON_STR_LEN  64	/* Maximum object or member name length */
#define MAX_JSON_VAL_LEN  128	/* Maximum value name length */

#ifdef CONFIG_JSON_FLOAT
/** The maximum size of a number */
typedef float max_num_t;
#else
/** The maximum size of a number */
typedef int max_num_t;
#endif

struct json_str {
	char *buff;
	int len;
	int free_ptr;		/* Where the \0 currently is, start writing from this
				 * offset */
};

/*
 * JSON objects indices
 */
struct json_object {
	const char *str;
	int current_obj;
	int array_obj;
};

typedef enum {
	JSON_VAL_STR,
	JSON_VAL_INT,
	JSON_VAL_UINT,
	JSON_VAL_UINT_64,
	JSON_VAL_FLOAT,
} json_data_types;

/** Initialize JSON parser
 *
 * This function initializes the JSON parser to start parsing of a json
 * string. The JSON tree may also have subtrees anchored by composite
 * objects. This function initializes the search depth to 0 (outermost
 * tree). Any JSON query function operates only at a given depth.
 * 
 * \param[out] obj The JSON object used as a handle for other search calls
 * \param[in] name A pointer to the string containing JSON text
 *
 * \return WM_SUCCESS. The function does not return any other error code at
 * present.
 */
int json_object_init(struct json_object *obj, const char *name);

/** Search for a composite object
 *
 * This function searches for a composite object with a given name in the JSON
 * string. If the composite object is found the search depth is set to point to
 * the subtree anchored at this composite object. Thus any calls after this will
 * search in this subtree.
 *
 *
 * \note Currently json_get_composite_object() may return composite objects at
 * any depth, and not only at the current depth. This problem will be fixed in a
 * future release.
 * 
 * \param[in,out] obj The JSON object handle returned by json_object_init()
 * \param[in] name The name of the composite object that should be searched
 *
 * \return WM_SUCCESS if success. Negative error codes in case of error.
 * \return -WM_E_JSON_INVSTART if the current buffer location to process
 * does not contain the JSON start character '{'.
 * \return -WM_E_JSON_CO_MNF if JSON composite member not found.
 * \return -WM_E_JSON_FAIL if generic json error.
 */
int json_get_composite_object(struct json_object *obj, const char *name);

/** Release a composite object
 *
 * This function decrements the search depth by 1. This function is the reverse
 * of the json_get_composite_object() function mentioned above. This sets the
 * JSON search depth to the last but one search depth.
 *
 *
 * \param[in,out] obj The JSON object handler returned by json_object_init()
 *
 * \return -WM_E_JSON_FAIL if given JSON object is invalid
 * \return WM_SUCCESS if operation successful.
 */
int json_release_composite_object(struct json_object *obj);

int json_get_array_object(struct json_object *obj, const char *name);
int json_get_array_val(struct json_object *obj, int *i_val, max_num_t *f_val);
int json_release_array_object(struct json_object *obj);

#define json_get_array_int(obj, val) \
			json_get_array_val(obj, val, NULL)

#define json_get_array_float(obj, val) \
			json_get_array_val(obj, NULL, val)

/** Find JSON key and return string
 *
 * This function searches for a JSON key with a given name and if it is a string
 * returns the corresponding value. 
 * 
 * \note JSON Query functions operate only at the current search depth. They do
 * not recursively search subtress anchored at the current depth.
 *
 * \param[in] obj The JSON object handler returned by json_object_init()
 * \param[in] name The name of the JSON key to lookup
 * \param[out] val A pointer to buffer where the string value will be stored
 * \param[in] maxlen The length of the buffer at pointer val
 *
 * \return 0 Success
 * \return -1 Failure (The key was not found)
 *
 */
int json_get_val_str(struct json_object *obj, const char *name, char *val,
		     int maxlen);

/** Find JSON key and return integer value
 *
 * This function searches for a JSON key with a given name and if it is an integer
 * returns the corresponding value. 
 * 
 * \note JSON Query functions operate only at the current search depth. They do
 * not recursively search subtress anchored at the current depth.
 *
 * \param[in] obj The JSON object handler returned by json_object_init()
 * \param[in] name The name of the JSON key to lookup
 * \param[out] val A pointer to an integer variable in which the value will be
 * returned.
 *
 * \return 0 Success
 * \return -1 Failure (The key was not found)
 *
 */
int json_get_val_int(struct json_object *obj, const char *name, int *val);

/** Find JSON key and return a floating point value
 *
 * This function searches for a JSON key with a given name and if it is a
 * floating point returns the corresponding value. 
 * 
 * \note JSON Query functions operate only at the current search depth. They do
 * not recursively search subtress anchored at the current depth.
 *
 * \param[in] obj The JSON object handler returned by json_object_init()
 * \param[in] name The name of the JSON key to lookup
 * \param[out] val A pointer to an float variable in which the value will be
 * returned.
 *
 * \return 0 Success
 * \return -1 Failure (The key was not found)
 *
 */
int json_get_val_float(struct json_object *obj, const char *name, float *val);

/** Initialize the JSON generator
 *
 * This function initializes the JSON generator.
 * 
 * \param[out] jptr A pointer to the structure json_str which will be used as a
 * handle for the rest of the json_str operations.
 * \param[out] buff A pointer to a buffer that will be used to store the
 * generated JSON data.
 * \param[in] len The length of the buffer pointed to by buff
 * \param[in] formatting A flag that indicates whether pretty formatting should
 * be enabled while generating JSON text.
 */
void json_str_init(struct json_str *jptr, char *buff, int len, int formatting);

/** Start a new JSON object
 *
 * This function starts a new JSON object
 *
 * \param[in] jptr Pointer to json_str object.
 *
 * \return -WM_E_JSON_OBUF if buffer available is insufficient or a buffer
 * overflow has already occured.
 * \return WM_SUCCESS if operation successful.
 */
int json_start_object(struct json_str *jptr);

/** Start a new composite object
 *
 * This function starts a new composite object which may have its own set of
 * key-value pairs. Any json_set() calls performed after this will create
 * key-value pairs within this composite object.
 *
 * \param[in] jptr A pointer to the json_str handle returned by the
 * json_str_init() call.
 * \param[in] name The name of the composite object that should be created.
 *
 * \return -WM_E_JSON_OBUF if buffer available is insufficient or a buffer
 * overflow has already occurred.
 * \return WM_SUCCESS if operation successful.
 */
int json_push_object(struct json_str *jptr, const char *name);

/** Close a composite object
 *
 * This closes a composite object. Any json_set() calls after this will create
 * key-value pairs at the same depth as the composite object that was closed.
 */
#define json_pop_object(jptr)  json_close_object((jptr))

/** Start a JSON array object.
 *
 * This function creates an element that has an array as its value.
 * 
 * \param[in] jptr A pointer to the json_str handle returned by the
 * json_str_init() call.
 * \param[in] name The name of the array object
 *
 * \return -WM_E_JSON_OBUF if buffer available is insufficient or a buffer
 * overflow has already occurred.
 * \return WM_SUCCESS if operation successful.
 */
int json_push_array_object(struct json_str *jptr, const char *name);

/** Close a JSON array object.
 *
 * This closes a previously pushed JSON object.
 *
 * \param[in] jptr Pointer to json_str object.
 *
 * \return -WM_E_JSON_OBUF if buffer available is insufficient or a buffer
 * overflow has already occurred.
 * \return WM_SUCCESS if operation successful.
 */
int json_pop_array_object(struct json_str *jptr);

/** Close JSON object
 * 
 * This function closes a JSON object that was started earlier.
 *
 * \param[in] jptr Pointer to json_str object.
 *
 * \return -WM_E_JSON_OBUF if buffer available is insufficient or a buffer
 * overflow has already occurred.
 * \return WM_SUCCESS if operation successful.
 */
int json_close_object(struct json_str *jptr);

int json_set_object_value(struct json_str *jptr, const char *name,
			  const char *str, int64_t value, float val,
			  short precision, json_data_types data);

/** Create a key with a string value.
 * 
 * This function adds a key-value pair to the JSON text with string as the
 * value.
 *
 * \param[in] jptr A pointer to the json_str handle returned by the
 * json_str_init() call.
 * \param[in] name The name of the key
 * \param[in] val The string value of the key
 */
#define json_set_val_str(jptr, name, val) \
		json_set_object_value(jptr, name, val, 0, 0.0, 0, JSON_VAL_STR)

/** Create a key with an integer value.
 * 
 * This function adds a key-value pair to the JSON text with an integer as the
 * value.
 *
 * \param[in] jptr A pointer to the json_str handle returned by the
 * json_str_init() call.
 * \param[in] name The name of the key
 * \param[in] val The integer value of the key
 */
#define json_set_val_int(jptr, name, val) \
		json_set_object_value(jptr, name, NULL, val, 0.0, 0, JSON_VAL_INT)

/** Create a key with an unsigned integer value.
 *
 * This function adds a key-value pair to the JSON text with an unsigned integer
 * as the value.
 *
 * \param[in] jptr A pointer to the json_str handle returned by the
 * json_str_init() call.
 * \param[in] name The name of the key
 * \param[in] val The unsigned integer value of the key
 */
#define json_set_val_uint(jptr, name, val) \
	json_set_object_value(jptr, name, NULL, val, 0.0, 0, JSON_VAL_UINT)

/** Create a key with an unsigned 64 bit integer value.
 *
 * This function adds a key-value pair to the JSON text with an unsigned 64 bit
 * integer as the value.
 *
 * \param[in] jptr A pointer to the json_str handle returned by the
 * json_str_init() call.
 * \param[in] name The name of the key
 * \param[in] val The unsigned 64 bit integer value of the key
 */
#define json_set_val_uint_64(jptr, name, val) \
	json_set_object_value(jptr, name, NULL, val, 0.0, 0, JSON_VAL_UINT_64)

/** Create a key with a float value.
 * 
 * This function adds a key-value pair to the JSON text with a float as the
 * value.
 *
 * \param[in] jptr A pointer to the json_str handle returned by the
 * json_str_init() call.
 * \param[in] name The name of the key
 * \param[in] val The float value of the key
 */
#define json_set_val_float(jptr, name, val) \
		json_set_object_value(jptr, name, NULL, 0, val, 2, JSON_VAL_FLOAT)

/** Create a key with a float value and decimal precision.
 * 
 * This function adds a key-value pair to the JSON text with a float as the
 * value.
 *
 * \param jptr A pointer to the json_str handle returned by the json_str_init()
 * call.
 * \param[in] name The name of the key
 * \param[in] val The float value of the key
 * \param[in] precision The number of precision digits for float
 */
#define json_set_val_float_precision(jptr, name, val, precision) \
		json_set_object_value(jptr, name, NULL, 0, val, precision, JSON_VAL_FLOAT)

/** Start a JSON array
 *
 * This function starts a JSON array. This is different than the
 * json_push_arraj_object(), in that json_push_array_object() creates an
 * key-value pair with an array as the value, while this simply starts an array
 * object. 
 *
 * \param[in] jptr Pointer to json_str object.
 *
 * \return -WM_E_JSON_OBUF if buffer available is insufficient or a buffer
 * overflow has already occurred.
 * \return WM_SUCCESS if operation successful.
 */
int json_start_array(struct json_str *jptr);

/** Close a JSON array
 *
 * This function closes a JSON array started by json_start_array().
 *
 * \param[in] jptr Pointer to json_str object.
 *
 * \return -WM_E_JSON_OBUF if buffer available is insufficient or a buffer
 * overflow has already occurred.
 * \return WM_SUCCESS if operation successful.
 */
int json_close_array(struct json_str *jptr);

int json_cli_init(void);

int json_set_array_value(struct json_str *jptr, char *str, int value, float val,
			 json_data_types data);

/** Create a string array element
 *
 * This function creates a string array element in a previously started JSON
 * array. 
 *
 * \param[in] jptr  A pointer to the json_str handle returned by the
 * json_str_init() call.
 * \param[in] val A pointer to the string value of the array element.
 */
#define json_set_array_str(jptr, val)  \
		json_set_array_value(jptr, val, 0, 0.0, JSON_VAL_STR)

/** Create an integer array element
 *
 * This function creates an integer array element in a previously started JSON
 * array. 
 *
 * \param[in] jptr  A pointer to the json_str handle returned by the
 * json_str_init() call.
 * \param[in] val The integer value of the array element.
 */
#define json_set_array_int(jptr, val) \
		json_set_array_value(jptr, NULL, val, 0.0, JSON_VAL_INT)

/** Create a float array element
 *
 * This function creates a float array element in a previously started JSON
 * array. 
 *
 * \param[in] jptr  A pointer to the json_str handle returned by the
 * json_str_init() call.
 * \param[in] val A pointer to the float value of the array element.
 */
#define json_set_array_float(jptr, val) \
		json_set_array_value(jptr, NULL, 0, val, JSON_VAL_FLOAT)

#endif /* ! _JSON_H_ */
