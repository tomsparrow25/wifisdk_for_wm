/***********************************************************
*  File: error_code.h 
*  Author: nzy
*  Date: 20150522
***********************************************************/
#ifndef _ERROR_CODE_H
    #define _ERROR_CODE_H

    #include "com_def.h"
#ifdef __cplusplus
	extern "C" {
#endif

#ifdef  __ERROR_CODE_GLOBALS
    #define __ERROR_CODE_EXT
#else
    #define __ERROR_CODE_EXT extern
#endif

/***********************************************************
*************************micro define***********************
***********************************************************/
#define OPRT_EC_STARAT OPRT_USER_START
#define OPRT_SOCK_ERR INCREMENT(OPRT_EC_STARAT,0)
#define OPRT_SET_SOCK_ERR INCREMENT(OPRT_EC_STARAT,1)
#define OPRT_SOCK_CONN_ERR INCREMENT(OPRT_EC_STARAT,2)
#define OPRT_CR_MUTEX_ERR INCREMENT(OPRT_EC_STARAT,3)
#define OPRT_CR_TIMER_ERR INCREMENT(OPRT_EC_STARAT,4)
#define OPRT_CR_THREAD_ERR INCREMENT(OPRT_EC_STARAT,5)

 
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

