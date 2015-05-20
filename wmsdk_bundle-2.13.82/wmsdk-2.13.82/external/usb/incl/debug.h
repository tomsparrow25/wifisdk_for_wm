/*
 * ============================================================================
 * (C) Copyright 2009-2010   Marvell International Ltd.  All Rights Reserved
 *
 *                         Marvell Confidential
 * ============================================================================
 */
#ifndef _DEBUG_H
#define _DEBUG_H


#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifdef _USB_DEBUG_
#define DPRINTF(fmt, args...)      wmprintf("%s %s %d: "fmt"\n",__FILE__, __func__, __LINE__, ##args )
#define dbg_printf(fmt, args...)   wmprintf("%s %s %d: "fmt"\n",__FILE__, __func__, __LINE__, ##args )
#define print_log(fmt, args...)   wmprintf("%s %s %d: "fmt"\n",__FILE__, __func__, __LINE__, ##args )
#else
#define DPRINTF(fmt, args...)
#define dbg_printf(fmt, args...)
#define print_log(fmt, args...) 
#endif

#ifdef __cplusplus
}
#endif

#endif /* End of _DEBUG_H */
