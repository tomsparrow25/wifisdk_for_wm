/** @file  includes.h 
  * @brief This file contains definition for include files
  * 
  * Copyright (C) 2003-2008, Marvell International Ltd.
  * All Rights Reserved  
  */

#ifndef INCLUDES_H
#define INCLUDES_H

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdarg.h>
#include <string.h>
#include <ctype.h>
#include <sys/types.h>
#include <wm_os.h>

#include "../wmcrypto_mem.h"
#include "type_def.h"
#include "common.h"
#include "wmstdio.h"

#ifdef DBG_PRINT_FUNC
/** ENTER */
#define ENTER()		wmprintf("Enter: %s, %s:%i\r\n", __FUNCTION__, __FILE__, __LINE__)
/** LEAVE */
#define LEAVE()		wmprintf("Leave: %s, %s:%i\r\n", __FUNCTION__, __FILE__, __LINE__)
#else
/** ENTER */
#define ENTER()
/** LEAVE */
#define LEAVE()
#endif

#endif /* INCLUDES_H */
