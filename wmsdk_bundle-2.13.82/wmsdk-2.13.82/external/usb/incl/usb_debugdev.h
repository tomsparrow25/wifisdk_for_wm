/*
 *
 * ============================================================================
 * Portions Copyright (c) 2008-2014   Marvell International, Ltd. All Rights Reserved.
 *
 *                         Marvell Confidential
 * ============================================================================
 */
/**
 *
 * \brief
 *
 */

#ifndef __host_debug_h__
#define __host_debug_h__
/*******************************************************************************
** File          : $HeadURL$
** Author        : $Author: prolig $
** Project       : HSCTRL
** Instances     :
** Creation date :
********************************************************************************
********************************************************************************
** ChipIdea Microelectronica - IPCS
** TECMAIA, Rua Eng. Frederico Ulrich, n 2650
** 4470-920 MOREIRA MAIA
** Portugal
** Tel: +351 229471010
** Fax: +351 229471011
** e_mail: chipidea.com
********************************************************************************
** ISO 9001:2000 - Certified Company
** (C) 2005 Copyright Chipidea(R)
** Chipidea(R) - Microelectronica, S.A. reserves the right to make changes to
** the information contained herein without notice. No liability shall be
** incurred as a result of its use or application.
********************************************************************************
** Modification history:
** $Date: 2007/04/13 $
** $Revision: #3 $
*******************************************************************************
*** Comments:
***   This file contains definitions for debugging the software stack
***
**************************************************************************
**END*********************************************************/
#include "typesdev.h"

#ifdef __cplusplus
extern "C" {
#endif

#define USB_DEBUG(x)
/*--------------------------------------------------------------**
** Debug macros, assume _DBUG_ is defined during development    **
**    (perhaps in the make file), and undefined for production. **
**--------------------------------------------------------------*/
#ifndef _DBUG_
#define DEBUG_FLUSH
#define DEBUG_PRINT(X)
#define DEBUG_PRINT2(X,Y)
#else
#define DEBUG_FLUSH        fflush(stdout);
#define DEBUG_PRINT(X)     printf(X);
#define DEBUG_PRINT2(X,Y)  printf(X,Y);
#endif

/*--------------------------------------------------------------**
** ASSERT macros, assume _ASSERT_ is defined during development **
**    (perhaps in the make file), and undefined for production. **
** This macro will check for pointer values and other validations
** wherever appropriate.
**--------------------------------------------------------------*/
// The ASSERT macro is defined in "lassert.h"
#if 0
#ifndef _ASSERT_
#define ASSERT(X,Y)
#else
#define ASSERT(X,Y) if(Y) { printf(X); exit(1);}
#endif
#endif

/************************************************************
The following array is used to make a run time trace route
inside the USB stack.
*************************************************************/
#define _DEBUG_INFO_TRACE_LEVEL_
#ifdef _DEBUG_INFO_TRACE_LEVEL_

#define TRACE_ARRAY_SIZE 1000
#define MAX_STRING_SIZE  50

/*
#ifndef __TRACE_VARIABLES_DEFINED__
uint_16 DEBUG_TRACE_ARRAY_COUNTER;
char   DEBUG_TRACE_ARRAY[TRACE_ARRAY_SIZE][MAX_STRING_SIZE];
boolean DEBUG_TRACE_ENABLED;
#else
uint_16 DEBUG_TRACE_ARRAY_COUNTER;
char   DEBUG_TRACE_ARRAY[TRACE_ARRAY_SIZE][MAX_STRING_SIZE];
boolean DEBUG_TRACE_ENABLED;
#endif */

//#define _DEVICE_DEBUG_
#define DEBUG_LOG_TRACE(x) DPRINTF("%s\n", x)

/*
#define DEBUG_LOG_TRACE(x) \
{ \
  if(DEBUG_TRACE_ENABLED) \
 { \
	 USB_memcopy(x,DEBUG_TRACE_ARRAY[DEBUG_TRACE_ARRAY_COUNTER],MAX_STRING_SIZE);\
	 DEBUG_TRACE_ARRAY_COUNTER++;\
	 if(DEBUG_TRACE_ARRAY_COUNTER >= TRACE_ARRAY_SIZE) \
	 {DEBUG_TRACE_ARRAY_COUNTER = 0;}\
 }\
} */

#define START_DEBUG_TRACE
/*\
{ \
	DEBUG_TRACE_ARRAY_COUNTER =0;\
	DEBUG_TRACE_ENABLED = TRUE;\
} */

#define STOP_DEBUG_TRACE
/* \
{ \
	DEBUG_TRACE_ENABLED = FALSE;\
} */

/*if trace switch is not enabled define debug log trace to empty*/
#else
#define DEBUG_LOG_TRACE(x)
#define START_DEBUG_TRACE
#define STOP_DEBUG_TRACE
#endif

/************************************************************
The following are global data structures that can be used
to copy data from stack on run time. This structure can
be analyzed at run time to see the state of various other
data structures in the memory.
*************************************************************/

#ifdef _DEBUG_INFO_DATA_LEVEL_

	typedef struct debug_data {
	} DEBUG_DATA_STRUCT, _PTR_ DEBUG_DATA_STRUCT_PTR;

#endif

/**************************************************************
	The following lines assign numbers to each of the routines
	in the stack. These numbers can be used to generate a trace
	on sequenece of routines entered.
**************************************************************/

#ifdef __cplusplus
}
#endif
#endif
/* EOF */
