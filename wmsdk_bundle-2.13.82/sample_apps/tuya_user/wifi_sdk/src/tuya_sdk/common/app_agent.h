/***********************************************************
*  File: app_agent.h 
*  Author: nzy
*  Date: 20150618
***********************************************************/
#ifndef _APP_AGENT_H
    #define _APP_AGENT_H

    #include "com_def.h"
    #include "appln_dbg.h"
    #include "smart_wf_frame.h"

#ifdef __cplusplus
	extern "C" {
#endif

#ifdef  __APP_AGENT_GLOBALS
    #define __APP_AGENT_EXT
#else
    #define __APP_AGENT_EXT extern
#endif

/***********************************************************
*************************micro define***********************
***********************************************************/
// lan protocol
#define FRM_TP_CFG_WF 1
#define FRM_TP_ACTV 2
#define FRM_TP_BIND_DEV 3
#define FRM_TP_UNBIND_DEV 6
#define FRM_TP_CMD 7
#define FRM_TP_STAT_REPORT 8
#define FRM_TP_HB 9
#define FRM_QUERY_STAT 0x0a
#define FRM_SSID_QUERY 0x0b

/***********************************************************
*************************variable define********************
***********************************************************/


/***********************************************************
*************************function define********************
***********************************************************/
/***********************************************************
*  Function: lan_pro_cntl_init
*  Input: 
*  Output: 
*  Return: none
***********************************************************/
__APP_AGENT_EXT \
OPERATE_RET lan_pro_cntl_init(VOID);

/***********************************************************
*  Function: mlp_gw_tcp_send
*  Input: 
*  Output: 
*  Return: OPERATE_RET
***********************************************************/
__APP_AGENT_EXT \
OPERATE_RET mlp_gw_tcp_send(INT socket,IN CONST UINT fr_num,\
                            IN CONST UINT fr_type,IN CONST UINT ret_code,\
                            IN CONST BYTE *data,IN CONST UINT len);

__APP_AGENT_EXT \
INT *get_lpc_sockets(VOID);

__APP_AGENT_EXT \
INT get_lpc_socket_num(VOID);

#ifdef __cplusplus
}
#endif
#endif

