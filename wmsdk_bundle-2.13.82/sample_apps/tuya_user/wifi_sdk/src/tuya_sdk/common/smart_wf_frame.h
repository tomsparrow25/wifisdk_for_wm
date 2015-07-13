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
    #include "sys_adapter.h"
    #include "sysdata_adapter.h"

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
typedef VOID (*SMART_FRAME_CB)(SMART_CMD_E cmd,cJSON *root);

typedef enum {
    CFG_SUCCESS = 0,
    CFG_NW_NO_FIND,
    CFG_AUTH_FAIL,
}LAN_WF_CFG_RESULT_E;

typedef enum {
    SMART_CFG = 0,
    AP_CFG
}WF_CFG_MODE_E;
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
*  Function: get_single_wf_dev
*  Input:  
*  Output: 
*  Return: 
*  Note: DEV_CNTL_N_S *
***********************************************************/
__SMART_WF_FRAME_EXT \
DEV_CNTL_N_S *get_single_wf_dev(VOID);

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
*  Function: get_fw_ug_stat
*  Input: none
*  Output: 
*  Return: FW_UG_STAT_E
*  Note: none
***********************************************************/
__SMART_WF_FRAME_EXT \
FW_UG_STAT_E get_fw_ug_stat(VOID);

/***********************************************************
*  Function: sf_fw_ug_msg_infm
*  Input: none
*  Output: 
*  Return: none
*  Note: none
***********************************************************/
__SMART_WF_FRAME_EXT \
OPERATE_RET sf_fw_ug_msg_infm(VOID);

/***********************************************************
*  Function: tuya_get_wf_cfg_mode
*  Input: none
*  Output: 
*  Return: none
*  Note: none
***********************************************************/
__SMART_WF_FRAME_EXT \
WF_CFG_MODE_E tuya_get_wf_cfg_mode(VOID);

/***********************************************************
*  Function: single_dev_reset_factory
*  Input: none
*  Output: 
*  Return: none
*  Note: none
***********************************************************/
__SMART_WF_FRAME_EXT \
void single_dev_reset_factory(void);

/***********************************************************
*  Function: select_ap_cfg_wf
*  Input: none
*  Output: 
*  Return: none
*  Note: none
***********************************************************/
__SMART_WF_FRAME_EXT \
void select_ap_cfg_wf(void);

/***********************************************************
*  Function: select_smart_cfg_wf
*  Input: none
*  Output: 
*  Return: none
*  Note: none
***********************************************************/
__SMART_WF_FRAME_EXT \
void select_smart_cfg_wf(void);

/***********************************************************
*  Function: auto_select_wf_cfg
*  Input: none
*  Output: 
*  Return: none
*  Note: none
***********************************************************/
__SMART_WF_FRAME_EXT \
void auto_select_wf_cfg(void);

/***********************************************************
*  Function: set_smart_cfg
*  Input: none
*  Output: 
*  Return: none
*  Note: none
***********************************************************/
__SMART_WF_FRAME_EXT \
void set_smart_cfg(void);

/***********************************************************
*  Function: select_cfg_mode_for_next
*  Input: none
*  Output: 
*  Return: none
*  Note: 
***********************************************************/
__SMART_WF_FRAME_EXT \
int select_cfg_mode_for_next(void);

#ifdef __cplusplus
}
#endif
#endif

