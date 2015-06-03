/***********************************************************
*  File: sys_adapter.h 
*  Author: nzy
*  Date: 20150526
***********************************************************/
#ifndef _SYS_ADAPTER_H
    #define _SYS_ADAPTER_H

    #include <wm_os.h>
    #include "com_def.h"

#ifdef __cplusplus
	extern "C" {
#endif

#ifdef  __SYS_ADAPTER_GLOBALS
    #define __SYS_ADAPTER_EXT
#else
    #define __SYS_ADAPTER_EXT extern
#endif

/***********************************************************
*************************micro define***********************
***********************************************************/
// for portable
typedef os_mutex_t MUTEX_HANDLE;
typedef os_thread_t THREAD;
typedef os_timer_t TIMER_ID;

// for portable
#undef malloc
#define malloc(x) os_mem_alloc(x)

#undef calloc 
#define calloc os_mem_calloc(x)

#undef free
#define free(x) os_mem_free(x)

/***********************************************************
*************************variable define********************
***********************************************************/


/***********************************************************
*************************function define********************
***********************************************************/


#ifdef __cplusplus
}
#endif
#endif

