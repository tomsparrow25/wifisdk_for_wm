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
#define SW_VER "1.0.0"
#define DEF_NAME "油汀"
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

