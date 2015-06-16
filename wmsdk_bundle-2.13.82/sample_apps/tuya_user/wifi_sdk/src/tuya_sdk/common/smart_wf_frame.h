/***********************************************************
*  File: smart_wf_frame.h 
*  Author: nzy
*  Date: 20150611
***********************************************************/
#ifndef _SMART_WF_FRAME_H
    #define _SMART_WF_FRAME_H

    #include "com_def.h"
    #include "com_struct.h"
    #include "appln_dbg.h"
    #include "mem_pool.h"
    #include "tuya_httpc.h"
    #include "cJSON.h"

#ifdef __cplusplus
	extern "C" {
#endif

#ifdef  __SMART_WF_FRAME_GLOBALS
    #define __SMART_WF_FRAME_EXT
#else
    #define __SMART_WF_FRAME_EXT extern
#endif

/***********************************************************
*************************micro define***********************
***********************************************************/
typedef enum {
    LAN_CMD = 0, // command come from lan
    MQ_CMD,      // command come from mqtt
}SMART_CMD_E;

// data format: {"1":100,"2":200}
typedef VOID (*SMART_FRAME_CB)(SMART_CMD_E cmd,BYTE *data,UINT len);

// smart frame process message
#define SF_MSG_LAN_CMD_FROM_APP 0 // 局域网控制命令
#define SF_MSG_LAN_STAT_REPORT 1 // 局域网数据状态主动上报
#define SF_MSG_LAN_AP_WF_CFG_RESP 2 // 局域网wifi配置响应

// msg frame 
typedef struct {
    INT socket;
    UINT frame_num;
    UINT frame_type;
    UINT len;
    CHAR data[0];
}SF_MLCFA_FR_S;

/***********************************************************
*************************variable define********************
***********************************************************/

/***********************************************************
*************************function define********************
***********************************************************/
/***********************************************************
*  Function: smart_frame_init
*  Input: 
*  Output: 
*  Return: 
*  Note:
***********************************************************/
__SMART_WF_FRAME_EXT \
OPERATE_RET smart_frame_init(IN CONST SMART_FRAME_CB cb);

/***********************************************************
*  Function: single_wf_device_init
*  Input: 
*  Output: 
*  Return: 
*  Note: if can not read dev_if in the flash ,so use the def_dev_if
*        def_dev_if->id 由内部产生
***********************************************************/
__SMART_WF_FRAME_EXT \
OPERATE_RET single_wf_device_init(INOUT DEV_DESC_IF_S *def_dev_if,\
                                  IN CONST CHAR *dp_schemas);

/***********************************************************
*  Function: sf_obj_dp_report ,obj dp report for user
*  Input: data->the format is :{"1":1,"2":true}
*  Output: 
*  Return: 
*  Note:
***********************************************************/
__SMART_WF_FRAME_EXT \
OPERATE_RET sf_obj_dp_report(IN CONST CHAR *id,IN CONST CHAR *data);

/***********************************************************
*  Function: sf_raw_dp_report ,raw dp report for user
*  Input: data->raw 
*  Output: 
*  Return: 
*  Note:
***********************************************************/
__SMART_WF_FRAME_EXT \
OPERATE_RET sf_raw_dp_report(IN CONST CHAR *id,IN CONST BYTE dpid,\
                             IN CONST BYTE *data, IN CONST UINT len);

/***********************************************************
*  Function: smart_frame_send_msg
*  Input: 
*  Output: 
*  Return: 
*  Note:
***********************************************************/
__SMART_WF_FRAME_EXT \
OPERATE_RET smart_frame_send_msg(IN CONST UINT msgid,\
                                 IN CONST VOID *data,\
                                 IN CONST UINT len);


#ifdef __cplusplus
}
#endif
#endif

