/* 
 *  Copyright 2008-2013, Marvell International Ltd.
 *  All Rights Reserved.
 */

#ifndef __TEST_UTILS_H__
#define __TEST_UTILS_H__

#ifndef LOG
#error "You must define the printf-like LOG macro or function prior to including test_utils.h"
#else

#define FAIL_UNLESS(condition, ...) \
	{ \
		if (!(condition)) { \
			LOG("FAIL: %s: %d: ", __FILE__, __LINE__); \
			LOG(__VA_ARGS__); \
			LOG("\r\n"); \
		} else { \
			LOG("PASS: %s: %d\r\n", __FILE__, __LINE__); \
		} \
	}

#endif
#endif
