/*
 *  Copyright (C) 2008-2013, Marvell International Ltd.
 *  All Rights Reserved.
 */

#ifndef __APPLN_DBG_H__
#define __APPLN_DBG_H__

#include <wmlog.h>

#if APPCONFIG_DEBUG_ENABLE
#define dbg(_fmt_, ...)				\
	wmprintf("[appln] "_fmt_"\n\r", ##__VA_ARGS__)
#define PR_DEBUG(_fmt_, ...) \
        wmprintf("[dbg]%s:%d "_fmt_"\n\r", __FILE__,__LINE__,##__VA_ARGS__)
#define PR_DEBUG_RAW(_fmt_, ...) \
        wmprintf(_fmt_,##__VA_ARGS__)
#else
#define dbg(...)
#define PR_DEBUG(...)
#define PR_DEBUG_RAW(_fmt_, ...)
#endif /* APPCONFIG_DEBUG_ENABLE */

#define PR_ERR(_fmt_, ...) \
        wmprintf("[err]%s:%d "_fmt_"\n\r", __FILE__,__LINE__,##__VA_ARGS__)

#endif /* ! __APPLN_DBG_H__ */
