/*
 *  Copyright 2008-2013, Marvell International Ltd.
 *  All Rights Reserved.
 */

#ifndef _APP_DEB_H_
#define _APP_DEB_H_

#include <wmlog.h>

#define app_l(...)				\
	wmlog("af", ##__VA_ARGS__)

#define app_e(...)				\
	wmlog_e("af", ##__VA_ARGS__)
#define app_w(...)				\
	wmlog_w("af", ##__VA_ARGS__)

#ifdef CONFIG_APP_DEBUG
#define app_d(...)				\
	wmlog("af", ##__VA_ARGS__)
#else
#define app_d(...)
#endif /* ! CONFIG_APP_DEBUG */

#endif /* _APP_DEB_H_ */
