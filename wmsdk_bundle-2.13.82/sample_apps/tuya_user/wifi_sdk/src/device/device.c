/***********************************************************
*  File: device.c 
*  Author: nzy
*  Date: 20150605
***********************************************************/
#define __DEVICE_GLOBALS
#include "device.h"


/***********************************************************
*************************micro define***********************
***********************************************************/

 
/***********************************************************
*************************variable define********************
***********************************************************/
CONST CHAR dp_schemas[] = {
"["
    "{"
        "\"id\": \"1\","
        "\"code\": \"temperature\","
        "\"name\": \"温度\","
        "\"mode\": \"rw\","
        "\"type\": \"obj\","
        "\"property\": {"
            "\"type\": \"value\","
            "\"unit\": \"℃\","
            "\"min\": 5,"
            "\"max\": 37,"
            "\"step\": 1"
        "}"
    "},"
    "{"
        "\"id\": \"2\","
        "\"code\": \"gear\","
        "\"name\": \"档位\","
        "\"mode\": \"rw\","
        "\"type\": \"obj\","
        "\"property\": {"
            "\"type\": \"enum\","
            "\"range\": ["
                "\"1\","
                "\"2\","
                "\"3\""
            "]"
        "}"
    "},"    
    "{"
        "\"id\": \"3\","
        "\"code\": \"lock\","
        "\"name\": \"童锁\","
        "\"mode\": \"rw\","
        "\"type\": \"obj\","
        "\"property\": {"
            "\"type\": \"bool\""
        "}"
    "},"
    "{"
        "\"id\": \"4\","
        "\"code\": \"eco\","
        "\"name\": \"节能模式\","
        "\"mode\": \"rw\","
        "\"type\": \"obj\","
        "\"property\": {"\
            "\"type\": \"bool\""
        "}"
    "}"
"]"};


/***********************************************************
*************************function define********************
***********************************************************/
STATIC VOID dev_cmd_cb(BYTE *data,UINT len);

/***********************************************************
*  Function: device_init
*  Input: 
*  Output: 
*  Return: 
***********************************************************/
OPERATE_RET device_init(VOID)
{
    DEV_DESC_IF_S def_dev_if;

    strcpy(def_dev_if.name,DEF_NAME);
    strcpy(def_dev_if.schema_id,SCHEMA_ID);
    strcpy(def_dev_if.ui_id,UI_ID);
    strcpy(def_dev_if.sw_ver,SW_VER);
    def_dev_if.ability = DEV_SINGLE;

    OPERATE_RET op_ret = single_wf_device_init(&def_dev_if,dp_schemas);
    if(op_ret != OPRT_OK) {
        return op_ret;
    }
    
    GW_CNTL_S *gw_cntl = get_gw_cntl();
    mq_client_init(gw_cntl->gw.id,"8888888","123456",0);

    op_ret = mq_client_start(gw_cntl->gw.id,dev_cmd_cb);
    if(op_ret != OPRT_OK) {
        return op_ret;
    }

    return op_ret;
}

STATIC VOID dev_cmd_cb(BYTE *data,UINT len)
{
    PR_DEBUG("data:%s",data);
}














