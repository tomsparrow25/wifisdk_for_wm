/***********************************************************
*  File: tuya_ws_db.h 
*  Author: nzy
*  Date: 20150601
***********************************************************/
#ifndef _TUYA_WS_DB_H
    #define _TUYA_WS_DB_H

    #include <wm_os.h>
    #include "com_def.h"
    #include "com_struct.h"
    #include "sys_adapter.h"
    #include "mem_pool.h"
    #include "error_code.h"

#ifdef __cplusplus
	extern "C" {
#endif

#ifdef  __TUYA_WS_DB_GLOBALS
    #define __TUYA_WS_DB_EXT
#else
    #define __TUYA_WS_DB_EXT extern
#endif

/***********************************************************
*************************micro define***********************
***********************************************************/
// 产品信息 生产时写入
typedef struct {
    CHAR prod_idx[PROD_IDX_LEN+1];
    CHAR mac[12+1];
}PROD_IF_REC_S;

/***********************************************************
*************************variable define********************
***********************************************************/


/***********************************************************
*************************function define********************
***********************************************************/
/***********************************************************
*  Function: ws_db_init
*  Input: 
*  Output: 
*  Return: OPERATE_RET
***********************************************************/
__TUYA_WS_DB_EXT \
OPERATE_RET ws_db_init(VOID);

/***********************************************************
*  Function: ws_db_set_prod_if
*  Input: 
*  Output: 
*  Return: OPERATE_RET
***********************************************************/
__TUYA_WS_DB_EXT \
OPERATE_RET ws_db_set_prod_if(IN CONST PROD_IF_REC_S *prod_if);

/***********************************************************
*  Function: ws_db_get_prod_if
*  Input: 
*  Output: 
*  Return: OPERATE_RET
***********************************************************/
__TUYA_WS_DB_EXT \
OPERATE_RET ws_db_get_prod_if(OUT PROD_IF_REC_S *prod_if);

/***********************************************************
*  Function: ws_db_set_gw_actv
*  Input: 
*  Output: 
*  Return: OPERATE_RET
***********************************************************/
__TUYA_WS_DB_EXT \
OPERATE_RET ws_db_set_gw_actv(IN CONST GW_ACTV_IF_S *gw_actv);

/***********************************************************
*  Function: ws_db_get_gw_actv
*  Input: 
*  Output: 
*  Return: OPERATE_RET
***********************************************************/
__TUYA_WS_DB_EXT \
OPERATE_RET ws_db_get_gw_actv(OUT GW_ACTV_IF_S *gw_actv);

/***********************************************************
*  Function: ws_db_set_dev_if
*  Input: 
*  Output: 
*  Return: OPERATE_RET
***********************************************************/
__TUYA_WS_DB_EXT \
OPERATE_RET ws_db_set_dev_if(IN CONST DEV_DESC_IF_S *dev_if);

/***********************************************************
*  Function: ws_db_get_dev_if
*  Input: 
*  Output: 
*  Return: OPERATE_RET
***********************************************************/
__TUYA_WS_DB_EXT \
OPERATE_RET ws_db_get_dev_if(OUT DEV_DESC_IF_S *dev_if);

/***********************************************************
*  Function: ws_db_reset
*  Input: 
*  Output: 
*  Return: OPERATE_RET
*  Note: only reset gw reset info and device bind info
***********************************************************/
__TUYA_WS_DB_EXT \
VOID ws_db_reset(VOID);

#ifdef __cplusplus
}
#endif
#endif

