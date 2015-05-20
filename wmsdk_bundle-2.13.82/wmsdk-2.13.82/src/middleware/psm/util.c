/* 
 *  Copyright 2008-2013, Marvell International Ltd.
 *  All Rights Reserved.
 */

/* -*- linux-c -*- */
/* util.c: Utility functions to be used by the backends and others */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <psm.h>

/* Variables are stored as name=value in the buffer. Once variable
 * definition is separated from the other by using a '\0'. The and of
 * the environment is indicated by an additional '\0'. Thus two
 * consecutive '\0' indicate the end of the environment.
 */

/*
 * s1 is either a simple 'name', or a 'name=value' pair.
 * s2 is a 'name=value' pair.
 * If the names match, return the value of s2, else NULL.
 */
static char *util_envmatch(const char *search, char *s2)
{
	int len = strlen(search);
	const char *s1 = search;

	while (*s1 == *s2++ && len--)
		if (*s1++ == '=')
			return s2;
	if (*s1 == '\0' && *(s2 - 1) == '=')
		return s2;
	return NULL;
}

/* Modified version of uboot/tools/env/ */
int util_search_var(unsigned char *environment, int size, const char *variable,
		    char *value, int val_len)
{
	unsigned char *env, *nxt = NULL;
	for (env = environment; *env; env = nxt + 1) {
		unsigned char *val;
		/* nxt is the pointer to the next variable definition. */
		for (nxt = env; *nxt; ++nxt) {
			if (nxt >= &environment[size]) {
				/* This should never happen, else we have
				already read beyond the bounds. That is because
				the env is terminated by 2 \0 */
				*value = '\0';
				return -WM_E_IO;
			}
		}
		val = (unsigned char *)util_envmatch(variable, (char *)env);
		if (!val)
			continue;
		if (strlen((const char *)val) >= val_len) {
			psm_e("Error: Given buffer is less then required. "
				 "Will not copy");
			return -WM_FAIL;
		}

		snprintf(value, val_len, "%s", val);
		return WM_SUCCESS;
	}
	*value = '\0';
	return -WM_E_INVAL;
}

/* Delete a variable from the environment, if present */
/* Return 0, if a variable is deleted, else 1 */
int util_del_var(unsigned char *environment, int size, 
		 const char *variable, int *delete_len)
{
	char *oldval = NULL;
	unsigned char *env, *nxt = NULL;

	/* Search if this variable already exists. This is the same
	 * code as that of search_var. Although we additionally use
	 * the "env" pointer from the code later on. 
	 */

	for (env = environment; *env; env = nxt + 1) {
		for (nxt = env; *nxt; ++nxt) {
			if (nxt >= &environment[size]) {
				/* This should never happen, else we have
				already read beyond the bounds. That is because
				the env is terminated by 2 \0 */
				return 1;
			}
		}
		oldval = util_envmatch(variable, (char *)env);
		if (oldval)
			break;
	}

	if (oldval) {
		/* currently, 'nxt' points to the \0, nxt +1 is the real next entry */
		*delete_len = (nxt + 1) - env;
		/* If this is the end of the environment, update the env variable. */
		if (*++nxt == '\0') {
			*env = '\0';
		} else {
			/* else move all the variable definitions up, till we reach the end of the environment */
			for (;;) {
				*env = *nxt++;
				if ((*env == '\0') && (*nxt == '\0'))
					break;
				++env;
			}
		}
		*++env = '\0';
		return 0;
	}

	return 1;
}

/* Set or modify a variable in the environment */
int util_set_var(unsigned char *environment, int size, const char *variable,
		 const char *value)
{
	unsigned char *env;
	unsigned int len;
	int delete_len = 0;
	int variable_len, value_len;

	variable_len = strlen(variable);
	value_len = strlen(value);

	/* Go to the end of the environment */
	for (env = environment; *env || *(env + 1); ++env)
		;
	/* If the first byte itself is '\0', i.e. we haven't initiated
	 * the buffer at all, do not increment.
	 */
	if (env > environment)
		++env;

	/*        variable    =     value     \0\0 */
	len = variable_len + 1 + value_len + 2;
	if (len > &environment[size] - env)
		return -WM_E_NOSPC;

	/* Delete the variable if it already exists */
	/* The delete should ideally happen before the calculation for space is
	 * done. Thus the deleted space is also accounted for. But we do not do
	 * that. Because, what will happen, if we delete the variable, but still
	 * there is no space available for the new value of the variable. We'll
	 * end up in a situation where a "set" has acted like "delete". To avoid
	 * this, we take a conservative approach. */

	util_del_var(environment, size, variable, &delete_len);
	/* No error returns after this point, if the deletion is successul */

	/* Do not calculate the new end again, just use the delete_len */
	env -= delete_len;

	memcpy(env, variable, variable_len);
	env += variable_len;

	*env++ = '=';

	memcpy(env, value, value_len);
	env += value_len;

	*env++ = '\0';
	*env = '\0';
	return 0;
}
