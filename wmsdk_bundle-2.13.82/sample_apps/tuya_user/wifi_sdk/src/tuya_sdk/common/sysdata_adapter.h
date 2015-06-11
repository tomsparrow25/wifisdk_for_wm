/***********************************************************
*  File: sysdata_adapter.h 
*  Author: nzy
*  Date: 20150601
***********************************************************/
#ifndef _SYSDATA_ADAPTER_H
    #define _SYSDATA_ADAPTER_H

    #include <wm_os.h>
    #include "com_def.h"
    #include "tuya_ws_db.h"

#ifdef __cplusplus
	extern "C" {
#endif

#ifdef  __SYSDATA_ADAPTER_GLOBALS
    #define __SYSDATA_ADAPTER_EXT
#else
    #define __SYSDATA_ADAPTER_EXT extern
#endif

/***********************************************************
*************************micro define***********************
***********************************************************/

/***********************************************************
*************************variable define********************
***********************************************************/

/***********************************************************
*************************function define********************
***********************************************************/
/***********************************************************
*  Function: set_gw_status
*  Input: 
*  Output: 
*  Return: 
***********************************************************/
__SYSDATA_ADAPTER_EXT \
VOID set_gw_status(IN CONST GW_STAT_E stat);

/***********************************************************
*  Function: get_gw_status
*  Input: 
*  Output: 
*  Return: GW_STAT_E
***********************************************************/
__SYSDATA_ADAPTER_EXT \
GW_STAT_E get_gw_status(VOID);

/***********************************************************
*  Function: get_gw_cntl
*  Input: 
*  Output: 
*  Return: GW_CNTL_S
***********************************************************/
__SYSDATA_ADAPTER_EXT \
GW_CNTL_S *get_gw_cntl(VOID);

/***********************************************************
*  Function: set_wf_gw_status
*  Input: 
*  Output: 
*  Return: 
***********************************************************/
__SYSDATA_ADAPTER_EXT \
VOID set_wf_gw_status(IN CONST GW_WIFI_STAT_E wf_stat);

/***********************************************************
*  Function: get_wf_gw_status
*  Input: 
*  Output: 
*  Return: GW_WIFI_STAT_E
***********************************************************/
__SYSDATA_ADAPTER_EXT \
GW_WIFI_STAT_E get_wf_gw_status(VOID);

/***********************************************************
*  Function: gw_cntl_init
*  Input: 
*  Output: 
*  Return: 
***********************************************************/
__SYSDATA_ADAPTER_EXT \
OPERATE_RET gw_cntl_init(VOID);

/***********************************************************
*  Function: gw_lc_bind_device
*  Input: 
*  Output: 
*  Return: 
***********************************************************/
__SYSDATA_ADAPTER_EXT \
OPERATE_RET gw_lc_bind_device(IN CONST DEV_DESC_IF_S *dev_if,\
                              IN CONST CHAR *sch_json_arr);

/***********************************************************
*  Function: get_dev_cntl
*  Input: 
*  Output: 
*  Return: 
*  Note: 
***********************************************************/
__SYSDATA_ADAPTER_EXT \
DEV_CNTL_N_S *get_dev_cntl(IN CONST CHAR *id);

/***********************************************************
*  Function: check_dev_need_update
*  Input: 
*  Output: 
*  Return: 
*  Note: user need to fill the content in the function before 
         call it
***********************************************************/
__SYSDATA_ADAPTER_EXT \
VOID check_dev_need_update(IN CONST DEV_CNTL_N_S *dev_cntl);

/***********************************************************
*  Function: check_all_dev_if_update
*  Input: 
*  Output: 
*  Return: 
***********************************************************/
__SYSDATA_ADAPTER_EXT \
VOID check_all_dev_if_update(VOID);

/***********************************************************
*  Function: check_and_update_dev_desc_if
*  Input: 
*  Output: 
*  Return: 
***********************************************************/
__SYSDATA_ADAPTER_EXT \
VOID check_and_update_dev_desc_if(IN CONST CHAR *id,\
                                  IN CONST CHAR *name,\
                                  IN CONST CHAR *sw_ver,\
                                  IN CONST CHAR *schema_id,\
                                  IN CONST CHAR *ui_id);

/***********************************************************
*  Function: update_dev_ol_status
*  Input: 
*  Output: 
*  Return: 
***********************************************************/
__SYSDATA_ADAPTER_EXT \
VOID update_dev_ol_status(IN CONST CHAR *id,IN CONST BOOL online);

/***********************************************************
*  Function: ws_db_reset
*  Input: 
*  Output: 
*  Return: OPERATE_RET
*  Note: only reset gw reset info and device bind info
***********************************************************/
__SYSDATA_ADAPTER_EXT \
VOID ws_db_reset(VOID);

/***********************************************************
*  Function: single_wf_device_init
*  Input: 
*  Output: 
*  Return: 
*  Note: if can not read dev_if in the flash ,so use the def_dev_if
*        def_dev_if->id 由内部产生
***********************************************************/
__SYSDATA_ADAPTER_EXT \
OPERATE_RET single_wf_device_init(INOUT DEV_DESC_IF_S *def_dev_if,\
                                  IN CONST CHAR *dp_schemas);

/***********************************************************
*  Function: start_active_gateway
*  Input: 
*  Output: 
*  Return: 
***********************************************************/
__SYSDATA_ADAPTER_EXT \
VOID start_active_gateway(VOID);

#ifdef __cplusplus
}
#endif
#endif

