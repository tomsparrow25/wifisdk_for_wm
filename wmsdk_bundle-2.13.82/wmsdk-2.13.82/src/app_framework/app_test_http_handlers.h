/*
 *  Copyright 2008-2013, Marvell International Ltd.
 *  All Rights Reserved.
 */

#ifndef APP_TEST_HTTP_HANDLERS_H_
#define APP_TEST_HTTP_HANDLERS_H_

static inline void app_test_http_unregister_handlers()
{
	/* Nothing to unregister */
}

#ifdef CONFIG_AUTO_TEST_BUILD
void app_test_http_register_handlers();
#else
static inline void app_test_http_register_handlers()
{
}
#endif

#endif
