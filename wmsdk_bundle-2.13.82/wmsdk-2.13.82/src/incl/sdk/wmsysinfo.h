/*! \file wmsysinfo.h
 *  \brief System Info Utilities
 *
 *  Copyright 2008-2013, Marvell International Ltd.
 *  All Rights Reserved.
 */

#ifndef _WMSYSINFO_H_
#define _WMSYSINFO_H_

#include <string.h>

/** Initialize System Information Utility
 *
 * This function registers system info CLI commands like 'sysinfo', 'memdump'
 * and 'memwrite' that can be used to query information about the various
 * susbsystems.
 *
 * \return WM_SUCCESS on success, error otherwise
 */
int sysinfo_init(void);

#endif
