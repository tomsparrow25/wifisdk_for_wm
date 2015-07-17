/***********************************************************
*  File: tuya_httpc.h 
*  Author: nzy
*  Date: 20150527
***********************************************************/
#ifndef _TUYA_HTTPC_H
    #define _TUYA_HTTPC_H

    #include "com_def.h"
    #include "sys_adapter.h"
    #include "mem_pool.h"
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
#if 0
#define TY_SMART_DOMAIN "http://192.168.0.19:7002/atop/gw.json"
#else
#define TY_SMART_DOMAIN "http://121.40.64.43:8080/atop-web-1.0.1/gw.json"
#endif

// gw interface
#define TI_GW_ACTIVE "s.gw.active" // gw active
#define TI_GW_RESET "s.gw.reset" // gw reset
#define TI_GW_HB "s.gw.heartbeat" // gw heart beat
#define TI_GW_INFO_UP "s.gw.update" // update gw base info

#define TI_BIND_DEV "s.gw.dev.bind" // dev bind
#define TI_DEV_INFO_UP "s.gw.dev.update" // dev info upgrade
#define TI_UNBIND_DEV "s.gw.dev.unbind" // dev unbind
#define TI_DEV_REP_STAT "s.gw.dev.online.report" // dev report status
#define TI_DEV_DP_REP "s.gw.dev.dp.report" // dev dp report

#define TI_GET_GW_ACL "s.gw.group.get" // get gw access list

// firmware upgrade by gw
#define TI_FW_UG_INFO "s.gw.upgrade" // get gw/dev firmware info
#define TI_FW_STAT "s.gw.upgrade.updatestatus"

#define TI_GW_DEV_ACTIVE "s.gw.dev.active" // gateway device active

/*
** is_end: 指示传输是否结束
** offset: 累积数据偏移
** data: 当次数据内容
** len: 当次数据长度
** note: non-chunked content is not null
**       chunked content or len is null or 0
*/
typedef OPERATE_RET (*HTTPC_CB)(IN CONST BOOL is_end,\
                                IN CONST UINT offset,\
                                IN CONST BYTE *data,\
                                IN CONST UINT len);

// fw upgrade status
typedef enum {
    UG_IDLE = 0,
    UG_RD,
    UPGRADING,
    UG_FIN,
    UG_EXECPTION,
}FW_UG_STAT_E;

// firmware upgrade
#define FW_URL_LEN 255
#define FW_MD5_LEN 32

typedef struct {
    CHAR fw_url[FW_URL_LEN+1];
    CHAR fw_md5[FW_MD5_LEN+1];
    CHAR sw_ver[SW_VER_LEN+1];
    BOOL auto_ug;
}FW_UG_S;
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

/***********************************************************
*  Function: httpc_gw_active
*  Input: 
*  Output: 
*  Return: OPERATE_RET
***********************************************************/
__TUYA_HTTPC_EXT \
OPERATE_RET httpc_gw_active();

/***********************************************************
*  Function: httpc_gw_reset
*  Input: 
*  Output: 
*  Return: OPERATE_RET
***********************************************************/
__TUYA_HTTPC_EXT \
OPERATE_RET httpc_gw_reset(VOID);

/***********************************************************
*  Function: httpc_gw_update
*  Input: 
*  Output: 
*  Return: OPERATE_RET
***********************************************************/
__TUYA_HTTPC_EXT \
OPERATE_RET httpc_gw_update(IN CONST CHAR *name,IN CONST CHAR *sw_ver);

/***********************************************************
*  Function: httpc_gw_hearat
*  Input: 
*  Output: 
*  Return: OPERATE_RET
***********************************************************/
__TUYA_HTTPC_EXT \
OPERATE_RET httpc_gw_hearat(VOID);

/***********************************************************
*  Function: httpc_dev_bind
*  Input: 
*  Output: 
*  Return: OPERATE_RET
***********************************************************/
__TUYA_HTTPC_EXT \
OPERATE_RET httpc_dev_bind(IN CONST DEV_DESC_IF_S *dev_if);

/***********************************************************
*  Function: httpc_dev_unbind
*  Input: 
*  Output: 
*  Return: OPERATE_RET
***********************************************************/
__TUYA_HTTPC_EXT \
OPERATE_RET httpc_dev_unbind(IN CONST CHAR *id);

/***********************************************************
*  Function: httpc_dev_update
*  Input: 
*  Output: 
*  Return: OPERATE_RET
***********************************************************/
__TUYA_HTTPC_EXT \
OPERATE_RET httpc_dev_update(IN CONST DEV_DESC_IF_S *dev_if);

/***********************************************************
*  Function: httpc_dev_stat_report
*  Input: 
*  Output: 
*  Return: OPERATE_RET
***********************************************************/
__TUYA_HTTPC_EXT \
OPERATE_RET httpc_dev_stat_report(IN CONST CHAR *id,IN CONST BOOL online);

/***********************************************************
*  Function: httpc_device_dp_report
*  Input: data->format is:{"devid":"xx","dps":{"1",1}}
*  Output: 
*  Return: OPERATE_RET
***********************************************************/
__TUYA_HTTPC_EXT \
OPERATE_RET httpc_device_dp_report(IN CONST DP_TYPE_E type,\
                                   IN CONST CHAR *data);

#if 0
/***********************************************************
*  Function: httpc_dev_raw_dp_report
*  Input: 
*  Output: 
*  Return: OPERATE_RET
***********************************************************/
__TUYA_HTTPC_EXT \
OPERATE_RET httpc_dev_raw_dp_report(IN CONST CHAR *id,IN CONST BYTE dpid,\
                                    IN CONST BYTE *data, IN CONST UINT len);


/***********************************************************
*  Function: httpc_dev_obj_dp_report
*  Input: 
*  Output: 
*  Return: OPERATE_RET
***********************************************************/
__TUYA_HTTPC_EXT \
OPERATE_RET httpc_dev_obj_dp_report(IN CONST CHAR *id,IN CONST CHAR *data);
#endif

/***********************************************************
*  Function: httpc_get_fw_ug_info
*  Input: etag
*  Output: p_fw_ug
*  Return: OPERATE_RET
***********************************************************/
__TUYA_HTTPC_EXT \
OPERATE_RET httpc_get_fw_ug_info(IN CONST CHAR *etag,OUT FW_UG_S *p_fw_ug);

/***********************************************************
*  Function: httpc_up_fw_ug_stat
*  Input: etag devid stat
*  Output: 
*  Return: OPERATE_RET
***********************************************************/
__TUYA_HTTPC_EXT \
OPERATE_RET httpc_up_fw_ug_stat(IN CONST CHAR *devid,\
                                IN CONST FW_UG_STAT_E stat);

/***********************************************************
*  Function: httpc_upgrade_fw
*  Input: url_str
*  Output: 
*  Return: OPERATE_RET
***********************************************************/
__TUYA_HTTPC_EXT \
OPERATE_RET httpc_upgrade_fw(CONST CHAR *url_str);

/***********************************************************
*  Function: httpc_gw_dev_active 
*  Input: dev_if
*  Output: 
*  Return: OPERATE_RET
*  说明:用于单品设备绑定+实体网关附带虚拟设备绑定
***********************************************************/
__TUYA_HTTPC_EXT \
OPERATE_RET httpc_gw_dev_active(IN CONST DEV_DESC_IF_S *dev_if);

#ifdef __cplusplus
}
#endif
#endif

