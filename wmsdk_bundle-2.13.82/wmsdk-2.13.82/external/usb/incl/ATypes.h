/*
 *
 * ============================================================================
 * Copyright (c) 2007-2012  Marvell International, Ltd. All Rights Reserved.
 *
 *                         Marvell Confidential
 * ============================================================================
 */
/**
 *
 * \file ATypes.h
 *
 * \brief Global type declarations.
 *
 */

#ifndef _ATYPES_H
#define _ATYPES_H

#include "error_types.h"

#ifdef __cplusplus
extern "C" {
#endif

#if !defined(TX_PORT) && !defined(TX_PORT_H)
typedef char CHAR;
typedef unsigned int UINT;
#endif

//typedef short INT16;
typedef unsigned short UINT16;
typedef unsigned long UINT32;
typedef long SINT32;
typedef short SINT16;
typedef unsigned char UINT8;
typedef signed char SINT8;
typedef unsigned short WCHAR;
typedef float FLOAT;
typedef double DOUBLE;
typedef void * DEVICE_HANDLE;
typedef unsigned long long UINT64;


#undef FALSE
#undef TRUE
#undef BOOL
// --------------- Boolean declarations --------------- BEGIN ---------------
typedef enum
{
    FALSE=0,
    TRUE=1
}_BOOL;

#define BOOL _BOOL

// --------------- Boolean declarations --------------- END ---------------

/* General defines */
#ifndef NULL
#define NULL 0
#endif

#ifndef MIN
#define MIN(a, b) ( (a) > (b) ? (b) : (a) )
#endif

#ifndef MAX
#define MAX(a, b) ( (a) > (b) ? (a) : (b) )
#endif

/** Use the following for defining thread priorities */
typedef enum
{
    THR_PRI_HIGHEST     = 0,
    THR_PRI_IMAGE       = 13,
    THR_ENG_PRIORITY    = 14,
    THR_PRI_NORMAL      = 15,
    THR_PRI_LOW         = 20,
    THR_PRI_WIRELESS    = 25,
    THR_PRI_NVRAM       = 29,
    THR_PRI_LOWEST      = 31
} tThrPriority;

#define DEFAULT_TIME_SLICE 2


#ifdef __cplusplus
}
#endif

#endif // _ATYPES_H
