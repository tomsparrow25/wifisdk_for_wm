/***********************************************************
*  File: error_code.h 
*  Author: nzy
*  Date: 20150522
***********************************************************/
#ifndef _ERROR_CODE_H
    #define _ERROR_CODE_H

    #include "com_def.h"
    #include "appln_dbg.h"

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
#define OPRT_EC_STARAT OPRT_USER_START // 4
#define OPRT_SOCK_ERR INCREMENT(OPRT_EC_STARAT,0)
#define OPRT_SET_SOCK_ERR INCREMENT(OPRT_EC_STARAT,1)
#define OPRT_SOCK_CONN_ERR INCREMENT(OPRT_EC_STARAT,2)
#define OPRT_CR_MUTEX_ERR INCREMENT(OPRT_EC_STARAT,3)
#define OPRT_CR_TIMER_ERR INCREMENT(OPRT_EC_STARAT,4)
#define OPRT_CR_THREAD_ERR INCREMENT(OPRT_EC_STARAT,5)
#define OPRT_BUF_NOT_ENOUGH INCREMENT(OPRT_EC_STARAT,6)
#define OPRT_URL_PARAM_OUT_LIMIT INCREMENT(OPRT_EC_STARAT,7)
#define OPRT_HTTP_OS_ERROR INCREMENT(OPRT_EC_STARAT,8)
#define OPRT_HTTP_PR_REQ_ERROR INCREMENT(OPRT_EC_STARAT,9)
#define OPRT_HTTP_SD_REQ_ERROR INCREMENT(OPRT_EC_STARAT,10)
#define OPRT_HTTP_RD_ERROR INCREMENT(OPRT_EC_STARAT,11)
#define OPRT_HTTP_GET_RESP_ERROR INCREMENT(OPRT_EC_STARAT,12)
#define OPRT_HTTP_AES_INIT_ERR INCREMENT(OPRT_EC_STARAT,13)
#define OPRT_HTTP_AES_OPEN_ERR INCREMENT(OPRT_EC_STARAT,14)
#define OPRT_HTTP_AES_SET_KEY_ERR INCREMENT(OPRT_EC_STARAT,15)
#define OPRT_HTTP_AES_ENCRYPT_ERR INCREMENT(OPRT_EC_STARAT,16)



 
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

