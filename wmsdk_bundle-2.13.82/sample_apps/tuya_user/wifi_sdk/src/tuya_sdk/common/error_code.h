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
#define OPRT_HTTP_AD_HD_ERROR INCREMENT(OPRT_EC_STARAT,12)
#define OPRT_HTTP_GET_RESP_ERROR INCREMENT(OPRT_EC_STARAT,13)
#define OPRT_HTTP_AES_INIT_ERR INCREMENT(OPRT_EC_STARAT,14)
#define OPRT_HTTP_AES_OPEN_ERR INCREMENT(OPRT_EC_STARAT,15)
#define OPRT_HTTP_AES_SET_KEY_ERR INCREMENT(OPRT_EC_STARAT,16)
#define OPRT_HTTP_AES_ENCRYPT_ERR INCREMENT(OPRT_EC_STARAT,17)
#define OPRT_TY_WS_PART_ERR INCREMENT(OPRT_EC_STARAT,18)
#define OPRT_CR_CJSON_ERR INCREMENT(OPRT_EC_STARAT,19)
#define OPRT_PSM_SET_ERROR INCREMENT(OPRT_EC_STARAT,20)
#define OPRT_PSM_GET_ERROR INCREMENT(OPRT_EC_STARAT,21)
#define OPRT_CJSON_PARSE_ERR INCREMENT(OPRT_EC_STARAT,22)
#define OPRT_CJSON_GET_ERR INCREMENT(OPRT_EC_STARAT,23)
#define OPRT_CR_HTTP_URL_H_ERR INCREMENT(OPRT_EC_STARAT,24)
#define OPRT_HTTPS_HANDLE_FAIL INCREMENT(OPRT_EC_STARAT,25)
#define OPRT_HTTPS_RESP_UNVALID INCREMENT(OPRT_EC_STARAT,26)
#define OPRT_MEM_PARTITION_EMPTY INCREMENT(OPRT_EC_STARAT,27)
#define OPRT_MEM_PARTITION_FULL INCREMENT(OPRT_EC_STARAT,28)
#define OPRT_MEM_PARTITION_NOT_FOUND INCREMENT(OPRT_EC_STARAT,29)
#define OPRT_CR_QUE_ERR INCREMENT(OPRT_EC_STARAT,30)
#define OPRT_SND_QUE_ERR INCREMENT(OPRT_EC_STARAT,31)
#define OPRT_NOT_FOUND_DEV INCREMENT(OPRT_EC_STARAT,32)
#define OPRT_NOT_FOUND_DEV_DP INCREMENT(OPRT_EC_STARAT,33)
#define OPRT_DP_ATTR_ILLEGAL INCREMENT(OPRT_EC_STARAT,34)
#define OPRT_DP_TYPE_PROP_ILLEGAL INCREMENT(OPRT_EC_STARAT,35) // dp type property illegal
#define OPRT_DP_REPORT_CLOUD_ERR INCREMENT(OPRT_EC_STARAT,36) 
#define OPRT_NO_NEED_SET_PRODINFO INCREMENT(OPRT_EC_STARAT,37)

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

