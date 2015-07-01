/***********************************************************
*  File: device.h 
*  Author: nzy
*  Date: 20150605
***********************************************************/
#ifndef _DEVICE_H
    #define _DEVICE_H

    #include <mc200_gpio.h>
    #include <led_indicator.h>
    #include "com_def.h"
    #include "appln_dbg.h"
    #include "error_code.h"
    #include "sysdata_adapter.h"
    #include "tuya_ws_db.h"
    #include "mem_pool.h"
    #include "smart_wf_frame.h"
    #include "../tuya_sdk/driver/key.h"

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
#define SW_VER GW_VER
#define DEF_NAME GW_DEF_NAME
#define SCHEMA_ID "a"
#define UI_ID "a"
#define DEF_DEV_ABI DEV_SINGLE
#define DEV_ETAG "a"

// reset key define
#define WF_RESET_KEY GPIO_10
//#define WF_RESET_KEY GPIO_27


// wifi direct led
#define WF_DIR_LEN GPIO_11
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

