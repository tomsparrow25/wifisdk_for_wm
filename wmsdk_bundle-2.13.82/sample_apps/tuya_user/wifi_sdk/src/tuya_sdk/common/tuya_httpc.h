/***********************************************************
*  File: tuya_httpc.h 
*  Author: nzy
*  Date: 20150527
***********************************************************/
#ifndef _TUYA_HTTPC_H
    #define _TUYA_HTTPC_H

    #include "com_def.h"
    #include "sys_adapter.h"
    #include "error_code.h"
    #include "com_struct.h"
    #include <wm_os.h>
    #include <httpc.h>

#ifdef __cplusplus
	extern "C" {
#endif

#ifdef  __TUYA_HTTPC_GLOBALS
    #define __TUYA_HTTPC_EXT
#else
    #define __TUYA_HTTPC_EXT extern
#endif

/***********************************************************
*************************micro define***********************
***********************************************************/
#define TY_SMART_DOMAIN "http://192.168.0.19/gw.json"

// gw interface
#define TI_GW_ACTIVE "s.gw.active" // gw active
#define TI_GW_RESET "s.gw.reset" // gw reset
#define TI_GW_HB "s.gw.heartbeat" // gw heart beat
//#define TI_GW_QU_UG "s.gw.is.upgrade" // query gw whether need upgrade
//#define TI_GET_GW_UGP "s.gw.firmware.url" // get gw upg url
#define TI_GW_INFO_UP "s.gw.update" // update gw base info

#define TI_BIND_DEV "s.gw.dev.bind" // dev bind
#define TI_DEV_INFO_UP "s.gw.dev.update" // dev info upgrade
#define TI_UNBIND_DEV "s.gw.dev.unbind" // dev unbind
#define TI_DEV_REP_STAT "s.gw.dev.online.report" // dev report status
#define TI_DEV_DP_REP "s.gw.dev.dp.report" // dev dp report

#define TI_GET_GW_ACL "s.gw.group.get" // get gw access list

/*
** is_end: 指示传输是否结束
** offset: 累积数据偏移
** data: 当次数据内容
** len: 当次数据长度
** note: non-chunked content is not null
**       chunked content or len is null or 0
*/
typedef VOID (*HTTPC_CB)(IN CONST BOOL is_end,\
                         IN CONST UINT offset,\
                         IN CONST BYTE *data,\
                         IN CONST UINT len);

/***********************************************************
*************************variable define********************
***********************************************************/

/***********************************************************
*************************function define********************
***********************************************************/
/***********************************************************
*  Function: httpc_aes_init
*  Input: 
*  Output: 
*  Return: OPERATE_RET
***********************************************************/
__TUYA_HTTPC_EXT \
OPERATE_RET httpc_aes_init(VOID);

/***********************************************************
*  Function: httpc_aes_init
*  Input: 
*  Output: 
*  Return: OPERATE_RET
***********************************************************/
__TUYA_HTTPC_EXT \
OPERATE_RET httpc_aes_set(IN CONST BYTE *key,IN CONST BYTE *iv);

__TUYA_HTTPC_EXT VOID tuya_http_test(VOID);


#ifdef __cplusplus
}
#endif
#endif

