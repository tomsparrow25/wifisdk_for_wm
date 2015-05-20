/*
 * Copyright 2008-2013, Marvell International Ltd.
 * All Rights Reserved.
 */

/*
 * Simple Json Parser
 */

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#ifdef JSON_LINUX_TEST
#include "../../incl/sdk/json.h"
#include "../../incl/sdk/wmerrno.h"
#else
#include <wmstdio.h>
#include <json.h>
#include <wmerrno.h>
#endif

int fmt = 0;

#ifdef JSON_LINUX_TEST
#define wmprintf printf
#endif

#include <wmlog.h>

#define json_e(...)				\
	wmlog_e("json", ##__VA_ARGS__)
#define json_w(...)				\
	wmlog_w("json", ##__VA_ARGS__)

#ifdef CONFIG_JSON_DEBUG
#define json_d(...)				\
	wmlog("json", ##__VA_ARGS__)
#else
#define json_d(...)
#endif /* ! CONFIG_JSON_DEBUG */


// Utility to jump whitespace and cr/lf
static const char *skip(const char *in)
{
	while (in && (unsigned char)*in <= 32)
		in++;
	return in;
}

static const char *rev_skip(const char *in)
{
	while (in && (unsigned char)*in <= 32)
		in--;
	return in;
}

const char *verify_json_start(const char *buff)
{
	buff = skip(buff);
	if (*buff != '{') {
		json_e("Invalid JSON document");
		return NULL;
	} else {
		return ++buff;
	}
}

static int verify_buffer_limit(struct json_str *jptr)
{
	/*
	 * Check for buffer overflow condition here, and then copy remaining
	 * data using snprintf. This makes sure there is no mem corruption in
	 * json set operations.
	 */
	if (jptr->free_ptr >= (jptr->len - 1)) {
		json_e("buffer maximum limit reached");
		return -1;
	} else
		return WM_SUCCESS;
}

#ifdef CONFIG_JSON_FLOAT
static void print_float(float val, short precision, char *str_val,
			int len_str_val)
{
	int val_int, val_frac;
	int scale = 1;
	short pre_tmp = precision;
	char sign[2] = "";

	while (pre_tmp--)
		scale *= 10;

	if (val < 0) {
		val = -val;
		sign[0] = '-';
		sign[1] = '\0';
	}

	val_int = (int)val;
	val_frac = (int)((val - val_int) * scale);

	snprintf(str_val, len_str_val, "%s%d.%.*d", sign, val_int, precision,
		 val_frac);
}
#else
static void print_float(float val, short precision, char *str_val,
			int len_str_val)
{
	snprintf(str_val, len_str_val, "\"unsupported\"");
}
#endif

// Parse the input text to generate a number, and populate the result into item.
static const char *parse_number(max_num_t *gbl_int, const char *num)
{
	double n = 0, sign = 1, scale = 1;

	// Could use sscanf for this?
	if (*num == '-')
		sign = -1, num++;	// Has sign?
	if (*num == '0')
		num++;		// is zero
	if (*num >= '1' && *num <= '9')
		do
			n = (n * 10.0) + (*num++ - '0');
		while (*num >= '0' && *num <= '9');	// Number?
	if (*num == '.') {
		num++;
		do
			n = (n * 10.0) + (*num++ - '0'), scale /= 10;
		while (*num >= '0' && *num <= '9');
	}			// Fractional part?

	n = sign * n * scale;	// number = +/- number.fraction * 10^+/- exponent
	*gbl_int = (max_num_t)n;
	return num;
}

// Parse the input text into an unescaped cstring, and populate item.
static const char firstByteMark[7] =
    { 0x00, 0x00, 0xC0, 0xE0, 0xF0, 0xF8, 0xFC };

static const char *parse_string(char *gbl_str, const char *str, int op_len)
{
	char out[MAX_JSON_VAL_LEN + 1];
	const char *ptr = str + 1;
	char *ptr2;
	int len = 0, max_size = 0;
	unsigned uc;
	if (*str != '\"') {
		return 0;	// not a string!
	}
	while (*ptr != '\"' && (unsigned char)*ptr > 31 && ++len)
		if (*ptr++ == '\\')
			ptr++;	// Skip escaped quotes.

	ptr = str + 1;
	ptr2 = out;
	max_size =
	    op_len > (sizeof(out) - 1) ? (sizeof(out) - 1) : (op_len - 1);
	while (*ptr != '\"' && (unsigned char)*ptr > 31
	       && ptr2 < (out + max_size)) {
		if (*ptr != '\\')
			*ptr2++ = *ptr++;
		else {
			ptr++;
			switch (*ptr) {
			case 'b':
				*ptr2++ = '\b';
				break;
			case 'f':
				*ptr2++ = '\f';
				break;
			case 'n':
				*ptr2++ = '\n';
				break;
			case 'r':
				*ptr2++ = '\r';
				break;
			case 't':
				*ptr2++ = '\t';
				break;
			case 'u':	// transcode utf16 to utf8.
				sscanf(ptr + 1, "%4x", &uc);	// get the unicode char.
				len = 3;
				if (uc < 0x80)
					len = 1;
				else if (uc < 0x800)
					len = 2;
				ptr2 += len;

				switch (len) {
				case 3:
					*--ptr2 = ((uc | 0x80) & 0xBF);
					uc >>= 6;
				case 2:
					*--ptr2 = ((uc | 0x80) & 0xBF);
					uc >>= 6;
				case 1:
					*--ptr2 = (uc | firstByteMark[len]);
				}
				ptr2 += len;
				ptr += 4;
				break;
			default:
				*ptr2++ = *ptr;
				break;
			}
			ptr++;
		}
	}
	*ptr2 = 0;
	if (*ptr == '\"')
		ptr++;

	strncpy(gbl_str, out, op_len);
	return ptr;
}

static const char *parse_value(const char *buff, char *val,
			       int len, max_num_t *num)
{
	if (!buff)
		return NULL;

	if (!strncmp(buff, "null", 4) && val) {
		strncpy(val, "null", len);
		return buff + 4;
	}
	if (!strncmp(buff, "false", 5) && val) {
		strncpy(val, "false", len);
		return buff + 5;
	}
	if (!strncmp(buff, "true", 4) && val) {
		strncpy(val, "true", len);
		return buff + 4;
	}
	if (*buff == '\"' && val) {
		return parse_string(val, buff, len);
	}
	if ((*buff == '-' || (*buff >= '0' && *buff <= '9')) && num) {
		return parse_number(num, buff);
	}

	return NULL;
}

static const char *skip_json_member(const char *buff)
{
	int tmp;
	while (*buff != ',' && *buff != '}') {
		if (*buff == '{') {
			tmp = 1;
			while (tmp) {
				buff++;
				if (*buff == '}')
					tmp--;
				if (*buff == '{')
					tmp++;
			}
		} else if (*buff == '\"') {
			/* Skip string as they may have ',' or '}' */
			while (*++buff != '\"') ;
		}
		buff++;
	}
	return buff;
}

/* ### To-Do: Add support for push/pop of multiple levels of object.
 * Now release_composite_object just brings level to topmost object rather it
 * should just pop current object and point to next upper object.
 */

int json_get_composite_object(struct json_object *obj, const char *name)
{
	char member[MAX_JSON_STR_LEN + 1];
	char val_str[MAX_JSON_VAL_LEN + 1];
	int size;
	max_num_t val_int;
	const char *start;
	const char *buff = obj->str;

	start = buff;
	size = strlen(buff);
	/* Point to first object */
	obj->current_obj = 0;

	buff = verify_json_start(buff);
	if (buff == NULL)
		return -WM_E_JSON_INVSTART;

	while ((buff - start - 1) < size) {
		buff = skip(parse_string(member, skip(buff), sizeof(member)));
		if (!buff)
			goto err;
		if (*buff != ':')
			goto err;

		buff = skip(buff + 1);
		if (!strcmp(member, name) && *buff == '{') {
			/* Match Found for object */
			obj->current_obj = buff - start;
			return WM_SUCCESS;
		} else if (*buff == '{') {
			buff++;
		} else {
			buff =
			    skip(parse_value
				 (buff, val_str, sizeof(val_str), &val_int));
		}

		if (*buff == '}')
			buff++;
		if (*buff == ',')
			buff++;
	}
	return -WM_E_JSON_CO_MNF;
err:
	return -WM_E_JSON_FAIL;
}

/* #### To-Do: Make sure get_array_object finds array object from composite
 * object as well which include normal member:val pair.
 */

int json_get_array_object(struct json_object *obj, const char *name)
{
	char member[MAX_JSON_STR_LEN + 1];
	int size;
	const char *start;
	const char *buff = &obj->str[obj->current_obj];
	char val_str[MAX_JSON_VAL_LEN + 1];
	max_num_t val_int;

	start = buff;
	size = strlen(buff);

	buff = verify_json_start(buff);
	if (buff == NULL)
		return -WM_E_JSON_FAIL;

	while ((buff - start - 1) < size) {
		buff = skip(parse_string(member, skip(buff), sizeof(member)));
		if (!buff)
			goto err;
		if (*buff != ':')
			goto err;

		buff = skip(buff + 1);
		if (!strcmp(member, name) && *buff == '[') {
			/* Match Found for array */
			obj->array_obj = buff - (obj->str);
			return WM_SUCCESS;
		} else if (*buff == '[') {
			/* Skip all data memebers */
			while (*buff++ != ']') ;
		} else {
			buff =
			    skip(parse_value
				 (buff, val_str, sizeof(val_str), &val_int));
		}

		if (*buff == ']')
			buff++;
		if (*buff == ',')
			buff++;
	}
	return -WM_E_JSON_ARR_MNF;
err:
	return -WM_E_JSON_FAIL;
}

int json_get_array_val(struct json_object *obj, int *i_val, max_num_t *f_val)
{
	max_num_t tmp;
	/* Start getting values from current array pointer */
	const char *buff = &obj->str[obj->array_obj];
	buff = skip(buff);
	if (*buff == ']') {
		json_d("Empty array or no more elements");
		return -WM_E_JSON_EARR;
	}
	buff = skip(parse_value(skip(buff + 1), NULL, 0, &tmp));
	if (buff) {
		if (i_val == NULL)
			*f_val = tmp;
		else
			*i_val = (int)tmp;
		obj->array_obj = (unsigned int)(buff - (obj->str));
	} else {
		json_w("Failed to parse value");
		return -WM_E_JSON_INVOBJ;
	}

	return WM_SUCCESS;
}

int json_release_composite_object(struct json_object *obj)
{
	if (!obj->str)
		return -WM_E_JSON_FAIL;
	/* Set current_obj to start of data string */
	obj->current_obj = 0;
	return WM_SUCCESS;
}

int json_release_array_object(struct json_object *obj)
{
	if (!obj->str)
		return -WM_E_JSON_FAIL;
	/* Just pop out latest array */
	obj->array_obj = 0;
	return WM_SUCCESS;
}

int json_object_init(struct json_object *obj, const char *buff)
{
	obj->str = buff;
	/* Set current_obj to start of data string */
	obj->current_obj = 0;
	obj->array_obj = 0;
	return WM_SUCCESS;
}

int json_get_val_str(struct json_object *obj, const char *name,
		     char *val, int maxlen)
{
	char member[MAX_JSON_STR_LEN + 1];
	char val_str[MAX_JSON_VAL_LEN + 1];
	const char *buff;

	buff = &obj->str[obj->current_obj];
	buff = verify_json_start(buff);
	if (buff == NULL)
		return -WM_E_JSON_INVSTART;

	while (1) {
		buff = skip(parse_string(member, skip(buff), sizeof(member)));
		if (!buff)
			goto err;
		if (strcmp(member, name))
			/* No match skip till char ',' */
			buff = skip_json_member(buff);
		else {
			/* Match Found */
			if (*buff != ':')
				goto err;	// fail!
			buff =
			    skip(parse_value
				 (skip(buff + 1), val_str, sizeof(val_str),
				  NULL));
			if (!buff) {
				json_e("String not found");
				/* Return null string */
				val[0] = '\0';
				return -WM_E_JSON_STR_NF;
			}

			if (strnlen(val_str, maxlen) == maxlen)
				return -WM_E_JSON_BUFF_TOO_SMALL;
			strcpy(val, val_str);
			return WM_SUCCESS;
		}

		if (!buff)
			goto err;
		if (*buff == ',')
			buff++;

		if (*buff == '}')
			break;
	}
	/* Return null string */
	val[0] = '\0';
	return -WM_E_JSON_STR_NF;
err:
	json_e("Improper Json Document");
	/* Return null string */
	val[0] = '\0';
	return -WM_E_JSON_FAIL;
}

int json_get_val_number(struct json_object *obj, const char *name,
			max_num_t *val_int)
{
	char member[MAX_JSON_STR_LEN + 1];
	const char *buff;

	buff = &obj->str[obj->current_obj];
	buff = verify_json_start(buff);
	if (buff == NULL)
		return -WM_E_JSON_INVSTART;

	while (1) {
		buff = skip(parse_string(member, skip(buff), sizeof(member)));
		if (!buff)
			goto err;
		if (strcmp(member, name))
			/* No match skip till char ',' */
			buff = skip_json_member(buff);
		else {
			/* Match Found */
			if (*buff != ':')
				goto err;	// fail!
			buff =
			    skip(parse_value(skip(buff + 1), NULL, 0, val_int));
			if (!buff) {
				json_e("No Integer found");
				return -WM_E_JSON_NUM_NF;
			}
			return WM_SUCCESS;
		}

		if (!buff)
			goto err;
		if (*buff == ',')
			buff++;

		if (*buff == '}')
			break;
	}
	return -WM_E_JSON_NUM_NF;
err:
	json_e("Improper Json Document");
	return -WM_E_JSON_INVDOC;
}

int json_get_val_int(struct json_object *obj, const char *name, int *val_int)
{
	int ret;
	max_num_t val;
	ret = json_get_val_number(obj, name, &val);
	if (ret == WM_SUCCESS)
		*val_int = (int)val;
	return ret;
}

#ifdef CONFIG_JSON_FLOAT
int json_get_val_float(struct json_object *obj, const char *name,
		       float *val_int)
{
	return json_get_val_number(obj, name, val_int);
}
#else
int json_get_val_float(struct json_object *obj, const char *name,
		       float *val_int)
{
	return -WM_FAIL;
}
#endif

void json_str_init(struct json_str *jptr, char *buff, int len, int formatting)
{
	jptr->buff = buff;
	memset(jptr->buff, 0, len);
	jptr->free_ptr = 0;
	jptr->len = len;
	fmt = formatting;
}

int json_push_object(struct json_str *jptr, const char *name)
{
	char *buff;

	if (verify_buffer_limit(jptr) < 0)
		return -WM_E_JSON_OBUF;

	/* From last skip cr/lf */
	buff = (char *)rev_skip(&jptr->buff[jptr->free_ptr - 1]);
	if (*buff != '{')	/* Element in object */
		jptr->buff[jptr->free_ptr++] = ',';

	if (fmt)
		snprintf(&jptr->buff[jptr->free_ptr],
			 jptr->len - jptr->free_ptr, "\n  \"%s\": {\n", name);
	else
		snprintf(&jptr->buff[jptr->free_ptr],
			 jptr->len - jptr->free_ptr, "\"%s\":{", name);

	jptr->free_ptr = strlen(jptr->buff);
	return WM_SUCCESS;
}

int json_push_array_object(struct json_str *jptr, const char *name)
{
	char *buff;

	if (verify_buffer_limit(jptr) < 0)
		return -WM_E_JSON_OBUF;

	/* From last skip cr/lf */
	buff = (char *)rev_skip(&jptr->buff[jptr->free_ptr - 1]);
	if (*buff != '{')	/* Element in object */
		jptr->buff[jptr->free_ptr++] = ',';

	if (fmt)
		snprintf(&jptr->buff[jptr->free_ptr],
			 jptr->len - jptr->free_ptr, "\n  \"%s\": [\n", name);
	else
		snprintf(&jptr->buff[jptr->free_ptr],
			 jptr->len - jptr->free_ptr, "\"%s\":[", name);

	jptr->free_ptr = strlen(jptr->buff);
	return WM_SUCCESS;
}

int json_start_object(struct json_str *jptr)
{
	char *buff;

	if (verify_buffer_limit(jptr) < 0)
		return -WM_E_JSON_OBUF;

	if (jptr->free_ptr) {
		/* This should be first call after json_str_init so free_ptr
		 * should be 0 but if it is not then we add ',' before
		 * starting object as there could have been earlier object
		 * already present as case in array.
		 */
		/* From last skip cr/lf */
		buff = (char *)rev_skip(&jptr->buff[jptr->free_ptr - 1]);

		if (*buff == '}')
			jptr->buff[jptr->free_ptr++] = ',';
	}
	jptr->buff[jptr->free_ptr++] = '{';
	return WM_SUCCESS;
}

int json_close_object(struct json_str *jptr)
{
	if (verify_buffer_limit(jptr) < 0)
		return -WM_E_JSON_OBUF;

	if (fmt)
		jptr->buff[jptr->free_ptr++] = '\n';
	jptr->buff[jptr->free_ptr++] = '}';

	return WM_SUCCESS;
}

int json_pop_array_object(struct json_str *jptr)
{
	if (verify_buffer_limit(jptr) < 0)
		return -WM_E_JSON_OBUF;

	if (fmt)
		jptr->buff[jptr->free_ptr++] = '\n';
	jptr->buff[jptr->free_ptr++] = ']';

	return WM_SUCCESS;
}

int json_start_array(struct json_str *jptr)
{
	char *buff;
	if (verify_buffer_limit(jptr) < 0)
		return -WM_E_JSON_OBUF;

	/* From last skip cr/lf */
	buff = (char *)rev_skip(&jptr->buff[jptr->free_ptr - 1]);

	if (*buff == ']')
		jptr->buff[jptr->free_ptr++] = ',';

	jptr->buff[jptr->free_ptr++] = '[';
	return WM_SUCCESS;
}

int json_close_array(struct json_str *jptr)
{
	if (verify_buffer_limit(jptr) < 0)
		return -WM_E_JSON_OBUF;

	jptr->buff[jptr->free_ptr++] = ']';
	return WM_SUCCESS;
}

int json_set_array_value(struct json_str *jptr, char *str, int value, float val,
			 json_data_types data)
{
	char *buff;

	if (!verify_json_start(jptr->buff))
		return WM_E_JSON_INVSTART;

	if (verify_buffer_limit(jptr) < 0)
		return -WM_E_JSON_OBUF;

	/* From last skip cr/lf */
	buff = (char *)rev_skip(&jptr->buff[jptr->free_ptr - 1]);

	if (*buff != '[')	/* Element in object */
		jptr->buff[jptr->free_ptr++] = ',';

	switch (data) {
	case JSON_VAL_STR:
		snprintf(&jptr->buff[jptr->free_ptr],
			 jptr->len - jptr->free_ptr, "\"%s\"", str);
		break;
	case JSON_VAL_INT:
		snprintf(&jptr->buff[jptr->free_ptr],
			 jptr->len - jptr->free_ptr, "%d", value);
		break;
	case JSON_VAL_FLOAT:
		print_float(val, 2, &jptr->buff[jptr->free_ptr],
			    jptr->len - jptr->free_ptr);
		break;
	default:
		json_e("Invalid case in array set");
	}

	jptr->free_ptr = strlen(jptr->buff);
	return WM_SUCCESS;
}

int json_set_object_value(struct json_str *jptr, const char *name,
			  const char *str, int64_t value, float val,
			  short precision, json_data_types data)
{
	char *buff;

	if (!verify_json_start(jptr->buff))
		return -WM_E_JSON_INVSTART;

	if (verify_buffer_limit(jptr) < 0)
		return -WM_E_JSON_OBUF;

	/* From last skip cr/lf */
	buff = (char *)rev_skip(&jptr->buff[jptr->free_ptr - 1]);

	if (*buff != '{')	/* Element in object */
		jptr->buff[jptr->free_ptr++] = ',';

	switch (data) {
	case JSON_VAL_STR:
		if (fmt)
			snprintf(&jptr->buff[jptr->free_ptr],
				 jptr->len - jptr->free_ptr,
				 "\n  \"%s\":\t\"%s\"", name, str);
		else
			snprintf(&jptr->buff[jptr->free_ptr],
				 jptr->len - jptr->free_ptr, "\"%s\":\"%s\"",
				 name, str);
		break;

	case JSON_VAL_INT:
		if (fmt)
			snprintf(&jptr->buff[jptr->free_ptr],
				 jptr->len - jptr->free_ptr, "\n  \"%s\":\t%d",
				 name, value);
		else
			snprintf(&jptr->buff[jptr->free_ptr],
				 jptr->len - jptr->free_ptr, "\"%s\":%d", name,
				 value);
		break;

	case JSON_VAL_UINT:
		if (fmt)
			snprintf(&jptr->buff[jptr->free_ptr],
				 jptr->len - jptr->free_ptr, "\n  \"%s\":\t%u",
				 name, value);
		else
			snprintf(&jptr->buff[jptr->free_ptr],
				 jptr->len - jptr->free_ptr, "\"%s\":%u", name,
				 value);
		break;

	case JSON_VAL_UINT_64:
		if (fmt)
			snprintf(&jptr->buff[jptr->free_ptr],
				jptr->len - jptr->free_ptr, "\n  \"%s\":\t%llu",
				 name, value);
		else
			snprintf(&jptr->buff[jptr->free_ptr],
				jptr->len - jptr->free_ptr, "\"%s\":%llu", name,
				 value);
		break;

	case JSON_VAL_FLOAT:
		if (fmt) {
			snprintf(&jptr->buff[jptr->free_ptr],
				 jptr->len - jptr->free_ptr, "\n  \"%s\":\t",
				 name);
			jptr->free_ptr = strlen(jptr->buff);
			print_float(val, precision, &jptr->buff[jptr->free_ptr],
				    jptr->len - jptr->free_ptr);
		} else {
			snprintf(&jptr->buff[jptr->free_ptr],
				 jptr->len - jptr->free_ptr, "\"%s\":", name);
			jptr->free_ptr = strlen(jptr->buff);
			print_float(val, precision, &jptr->buff[jptr->free_ptr],
				    jptr->len - jptr->free_ptr);
		}
		break;
	default:
		json_e("Invalid case in object set");
	}

	jptr->free_ptr = strlen(jptr->buff);
	return WM_SUCCESS;
}
