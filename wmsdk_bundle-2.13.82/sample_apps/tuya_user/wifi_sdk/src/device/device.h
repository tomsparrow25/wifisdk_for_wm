/***********************************************************
*  File: device.h 
*  Author: nzy
*  Date: 20150605
***********************************************************/
#ifndef _DEVICE_H
    #define _DEVICE_H

    #include "com_def.h"
    #include "appln_dbg.h"
    #include "error_code.h"
    #include "sysdata_adapter.h"
    #include "tuya_ws_db.h"
    #include "mem_pool.h"
    #include "mqtt_client.h"

#ifdef __cplusplus
	extern "C" {
#endif

#ifdef  __DEVICE_GLOBALS
    #define __DEVICE_EXT
#else
    #define __DEVICE_EXT extern
#endif

/***********************************************************
*************************micro define***********************
***********************************************************/
// device information define
#define SW_VER "1.0"
#define SW_TAG "xxxx"
#define DEF_NAME "switch"
#define SCHEMA_ID "100000"
#define UI_ID "100000"
#define DEF_DEV_ABI DEV_SINGLE
/***********************************************************
*************************variable define********************
***********************************************************/


/***********************************************************
*************************function define********************
***********************************************************/
/***********************************************************
*  Function: device_init
*  Input: 
*  Output: 
*  Return: 
***********************************************************/
__DEVICE_EXT \
OPERATE_RET device_init(VOID);


#ifdef __cplusplus
}
#endif
#endif

