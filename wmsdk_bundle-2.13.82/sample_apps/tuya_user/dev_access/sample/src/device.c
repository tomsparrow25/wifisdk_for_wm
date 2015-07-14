/***********************************************************
*  File: device.c 
*  Author: nzy
*  Date: 20150605
***********************************************************/
#define __DEVICE_GLOBALS
#include "device.h"
#include <mc200_gpio.h>
#include <led_indicator.h>
#include "mem_pool.h"
#include "smart_wf_frame.h"
#include "key.h"

/***********************************************************
*************************micro define***********************
***********************************************************/

/***********************************************************
*************************function define********************
***********************************************************/
STATIC OPERATE_RET device_differ_init(VOID);
STATIC VOID key_process(INT gpio_no,PUSH_KEY_TYPE_E type,INT cnt);
static void wfl_timer_cb(os_timer_arg_t arg);


/***********************************************************
*************************variable define********************
***********************************************************/
CONST CHAR dp_schemas[] = "[ {\"id\": 1, \"code\": \"switch\", \"name\": \"开关\", \"mode\": \"rw\", \"type\": \"obj\", \"property\": {\"type\": \"bool\" }},{\"id\": 2, \"code\": \"temperature\", \"name\": \"温度\", \"mode\": \"rw\", \"type\": \"obj\", \"property\": {\"type\": \"value\", \"unit\": \"℃\", \"min\": 5, \"max\": 37, \"step\": 1 }}, { \"id\": 3, \"code\": \"gear\", \"name\": \"档位\", \"mode\": \"rw\", \"type\": \"obj\", \"property\": { \"type\": \"enum\", \"range\": [ \"1\", \"2\", \"3\" ] }}, {\"id\": 4, \"code\": \"lock\", \"name\": \"童锁\", \"mode\": \"rw\", \"type\": \"obj\", \"property\": {\"type\": \"bool\" }}, {\"id\": 5, \"code\": \"eco\", \"name\": \"eco节能模式\", \"mode\": \"rw\", \"type\": \"obj\", \"property\": {\"type\": \"bool\" }},{ \"id\": 6, \"code\": \"appoint\", \"name\": \"预约\", \"mode\": \"rw\", \"type\": \"obj\", \"property\": { \"type\": \"value\", \"unit\": \"m\", \"min\": 0, \"max\": 540, \"step\": 1, \"range\": [ \"0\", \"30\", \"60\", \"120\", \"180\", \"240\", \"300\", \"360\", \"420\", \"480\", \"540\"],\"unit\":\"m\" }}]";

// KEY
KEY_ENTITY_S key_tbl[] = {
    {WF_RESET_KEY,5000,key_process,0,0,0,0},
};

/***********************************************************
*************************function define********************
***********************************************************/
VOID device_cb(SMART_CMD_E cmd,cJSON *root)
{
    CHAR *buf = cJSON_PrintUnformatted(root);
    if(NULL == buf) {
        PR_ERR("malloc error");
        return;
    }

    OPERATE_RET op_ret = sf_obj_dp_report(get_single_wf_dev()->dev_if.id,buf);
    if(OPRT_OK != op_ret) {
        PR_ERR("sf_obj_dp_report err:%d",op_ret);
        PR_DEBUG_RAW("%s\r\n",buf);
        Free(buf);
        return;
    }
    Free(buf);
}

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

    OPERATE_RET op_ret;
    op_ret = smart_frame_init(device_cb);
    if(op_ret != OPRT_OK) {
        return op_ret;
    }

    op_ret = single_wf_device_init(&def_dev_if,dp_schemas);
    if(op_ret != OPRT_OK) {
        return op_ret;
    }

    op_ret = device_differ_init();
    if(op_ret != OPRT_OK) {
        return op_ret;
    }

    return op_ret;
}

STATIC VOID key_process(INT gpio_no,PUSH_KEY_TYPE_E type,INT cnt)
{
    PR_DEBUG("gpio_no: %d",gpio_no);
    PR_DEBUG("type: %d",type);
    PR_DEBUG("cnt: %d",cnt);

    if(WF_RESET_KEY == gpio_no) {
        if(LONG_KEY == type) {
            single_dev_reset_factory();
        }else if(SEQ_KEY == type && cnt >= 4) { // data restore factory            
            auto_select_wf_cfg();
        }
    }
}

STATIC OPERATE_RET device_differ_init(VOID)
{
    OPERATE_RET op_ret;
    
    // key process init
    op_ret = key_init(key_tbl,(CONST INT)CNTSOF(key_tbl),25);
    if(OPRT_OK  != op_ret) {
        return op_ret;
    }

    STATIC TIMER_ID wfl_timer;
    // wf direct light timer init
    int ret;
    // create timer
    ret = os_timer_create(&wfl_timer, "wfl_timer", \
                          os_msec_to_ticks(300),\
                          wfl_timer_cb, NULL,\
                          OS_TIMER_PERIODIC, OS_TIMER_AUTO_ACTIVATE);
    if(ret != WM_SUCCESS) {
        return OPRT_CR_TIMER_ERR;
    }

    return OPRT_OK;
}

static void wfl_timer_cb(os_timer_arg_t arg)
{
    STATIC UINT last_wf_stat = 0xffffffff;
    GW_WIFI_STAT_E wf_stat = get_wf_gw_status();
    
    if(last_wf_stat != wf_stat) {
        PR_DEBUG("wf_stat:%d",wf_stat);
        switch(wf_stat) {
            case STAT_UNPROVISION: {
                led_blink(WF_DIR_LEN, 250, 250);
            }
            break;
            
            case STAT_AP_STA_UNCONN:
            case STAT_AP_STA_CONN: {
                led_blink(WF_DIR_LEN, 1500, 1500);
            }
            break;
            
            case STAT_STA_UNCONN: {
                led_off(WF_DIR_LEN);
            }
            break;
            
            case STAT_STA_CONN: {
                led_on(WF_DIR_LEN);
            }
            break;
        }

        last_wf_stat = wf_stat;
    }
}













