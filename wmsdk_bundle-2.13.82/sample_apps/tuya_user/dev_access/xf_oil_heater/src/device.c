/***********************************************************
*  File: device.c 
*  Author: nzy
*  Date: 20150605
***********************************************************/
#define __DEVICE_GLOBALS
#include "device.h"
#include <mc200_gpio.h>
#include <led_indicator.h>
#include <mdev_uart.h>
#include "mem_pool.h"
#include <wmtime.h>
#include "sysdata_adapter.h"
#include "tuya_ws_db.h"
#include "mem_pool.h"
#include "smart_wf_frame.h"
#include "key.h"

/***********************************************************
*************************micro define***********************
***********************************************************/
#define YT_QUERY 1
#define YT_FRAME_MIN_SIZE 22
#define YT_FRAME_SIZE 12
#define YT_RECV_BUF_MAX 1024
#define YT_UART_SEND_MAX 3

typedef struct
{
    UCHAR SOI;        /*固定为0x7e*/
    UCHAR CMD;       /*1字节*/
    UCHAR PARAM1;    /*PARAM字段*/
    UCHAR PARAM2;
    UCHAR EOI;        /*固定为0x5a*/
}struSendMsg;

typedef struct
{
    UCHAR SOI;        /*固定为0x7e*/
    UCHAR ACK;       /*1字节*/
    UCHAR PARAM1;    /*PARAM字段*/
    UCHAR PARAM2;
    UCHAR PARAM3;
    UCHAR PARAM4;
    UCHAR PARAM5;
    UCHAR PARAM6;
    UCHAR PARAM7;
    UCHAR PARAM8;
    UCHAR PARAM9;
    UCHAR EOI;        /*固定为0x5a*/
}struRecvMsg;

typedef enum {
    YT_INIT = 0,    
    YT_RECV,
}YT_RECV_STAT_E;

typedef struct
{
    struSendMsg pstruMsg;
    struRecvMsg pstruRecMsg;
   // YT_RECV_STAT_E status;
    UCHAR cmd;
    UINT i;
    BOOL Flag1;
    BOOL Flag2;
    BOOL Flag3;
    mdev_t *uart_dev;
    os_timer_t yt_timer;
    os_timer_t yt_send_timer;
    UCHAR uart_recvbuf[YT_RECV_BUF_MAX];
    THREAD thread;
    MUTEX_HANDLE mutex; 
}YT_MSG;

STATIC os_thread_stack_define(yt_stack, 1024);
/***********************************************************
*************************function define********************
***********************************************************/
mdev_t *uart_drv_open(UART_ID_Type port_id, uint32_t baud);
uint32_t uart_drv_read(mdev_t *dev, uint8_t *buf, uint32_t num);
uint32_t uart_drv_write(mdev_t *dev, const uint8_t *buf, uint32_t num);
int uart_drv_set_opts(UART_ID_Type uart_id, uint32_t parity, uint32_t stopbits, flow_control_t flow_control);

STATIC VOID key_process(INT gpio_no,PUSH_KEY_TYPE_E type,INT cnt);
STATIC void wfl_timer_cb(os_timer_arg_t arg);
STATIC OPERATE_RET device_differ_init(VOID);
extern void auto_select_wf_cfg(void);
extern void select_smart_cfg_wf(void);
extern void select_ap_cfg_wf(void);
extern void single_dev_reset_factory(void);

/***********************************************************
*************************variable define********************
***********************************************************/
//CONST CHAR dp_schemas[] = "[ {\"id\": 1, \"code\": \"switch\", \"name\": \"????", \"mode\": \"rw\", \"type\": \"obj\", \"property\": {\"type\": \"bool\" }},{\"id\": 2, \"code\": \"temperature\", \"name\": \"???", \"mode\": \"rw\", \"type\": \"obj\", \"property\": {\"type\": \"value\", \"unit\": \"??", \"min\": 5, \"max\": 37, \"step\": 1 }}, { \"id\": 3, \"code\": \"gear\", \"name\": \"??λ\", \"mode\": \"rw\", \"type\": \"obj\", \"property\": { \"type\": \"enum\", \"range\": [ \"1\", \"2\", \"3\" ] }}, {\"id\": 4, \"code\": \"lock\", \"name\": \"ͯ?\", \"mode\": \"rw\", \"type\": \"obj\", \"property\": {\"type\": \"bool\" }}, {\"id\": 5, \"code\": \"eco\", \"name\": \"eco????ʽ\", \"mode\": \"rw\", \"type\": \"obj\", \"property\": {\"type\": \"bool\" }},{ \"id\": 6, \"code\": \"appoint\", \"name\": \"ԤԼ\", \"mode\": \"rw\", \"type\": \"obj\", \"property\": { \"type\": \"value\", \"unit\": \"m\", \"min\": 0, \"max\": 540, \"step\": 1, \"range\": [ \"0\", \"30\", \"60\", \"120\", \"180\", \"240\", \"300\", \"360\", \"420\", \"480\", \"540\"],\"unit\":\"m\" }}]";
CONST CHAR dp_schemas[] = "[ {\"id\": 1, \"code\": \"switch\", \"name\": \"ø™πÿ\", \"mode\": \"rw\", \"type\": \"obj\", \"property\": {\"type\": \"bool\" }},{\"id\": 2, \"code\": \"temperature\", \"name\": \"Œ¬∂»\", \"mode\": \"rw\", \"type\": \"obj\", \"property\": {\"type\": \"value\", \"unit\": \"°Ê\", \"min\": 5, \"max\": 37, \"step\": 1 }}, { \"id\": 3, \"code\": \"gear\", \"name\": \"µµŒª\", \"mode\": \"rw\", \"type\": \"obj\", \"property\": { \"type\": \"enum\", \"range\": [ \"1\", \"2\", \"3\" ] }}, {\"id\": 4, \"code\": \"lock\", \"name\": \"ÕØÀ¯\", \"mode\": \"rw\", \"type\": \"obj\", \"property\": {\"type\": \"bool\" }}, {\"id\": 5, \"code\": \"eco\", \"name\": \"ecoΩ⁄ƒ‹ƒ£ Ω\", \"mode\": \"rw\", \"type\": \"obj\", \"property\": {\"type\": \"bool\" }},{ \"id\": 6, \"code\": \"appoint\", \"name\": \"‘§‘º\", \"mode\": \"rw\", \"type\": \"obj\", \"property\": { \"type\": \"value\", \"unit\": \"m\", \"min\": 0, \"max\": 540, \"step\": 1, \"range\": [ \"0\", \"30\", \"60\", \"120\", \"180\", \"240\", \"300\", \"360\", \"420\", \"480\", \"540\"],\"unit\":\"m\" }}]";
// KEY
KEY_ENTITY_S key_tbl[] = {
    {WF_RESET_KEY,5000,key_process,0,0,0,0},
};

YT_MSG yt_msg; 

/***********************************************************
*************************function define********************
***********************************************************/
/*把一个字节拆分成两字节*/
void UCHAR_change(uint temp, UCHAR* hight, UCHAR* low)
{
    *hight = (temp & 0xf0) >> 4;
    *low = temp & 0x0f;
}

/*把两个字节合并成一个字节*/
void UCHAR_combine(uint hight, uint low, UCHAR* temp)
{
    *temp = (hight << 4) | low;
}

INT yt_uart_init(VOID)
{
    INT ret;
    ret = uart_drv_init(UART1_ID, UART_8BIT);
    if(ret != WM_SUCCESS)
    {
        PR_DEBUG("uart_drv_init failed");
        return WM_FAIL;
    }

    ret = uart_drv_set_opts(UART1_ID, UART_PARITY_NONE, UART_STOPBITS_1, FLOW_CONTROL_NONE);
    if(ret != WM_SUCCESS)
    {
        PR_DEBUG("uart_drv_set_opts failed");
        return WM_FAIL;
    }

    yt_msg.uart_dev = uart_drv_open(UART1_ID, 9600);
    if(yt_msg.uart_dev == NULL)
    {
        PR_DEBUG("uart_drv_open failed");
        return WM_FAIL;
    }
    return WM_SUCCESS;
}

INT yt_close_uart(VOID)
{
    INT ret;
    ret = uart_drv_close(yt_msg.uart_dev);
    if(ret != WM_SUCCESS)
    {
        return WM_FAIL;
    }
    uart_drv_deinit(UART1_ID);
    return WM_SUCCESS;
}

INT yt_id_cmd(INT id)
{
    INT cmd;
    switch(id)
    {
        case 1: cmd = 0x01;break;
        case 2: cmd = 0x04;break;
        case 3: 
        case 5:
            cmd = 0x02;break;
        case 4:cmd = 0x05;break;
        case 6:cmd = 0x03;break;
        default:cmd = 0x00;break;
    }
    return cmd;
}

VOID yt_msg_combine(UCHAR* buf, UCHAR* yt_buf)
{
    yt_buf[0] = buf[0];
    yt_buf[11] = buf[21];
    UCHAR_combine(buf[1], buf[2], &yt_buf[1]); //ack
    UCHAR_combine(buf[3], buf[4], &yt_buf[2]);  //param1
    UCHAR_combine(buf[5], buf[6], &yt_buf[3]);
    UCHAR_combine(buf[7], buf[8], &yt_buf[4]);
    UCHAR_combine(buf[9], buf[10], &yt_buf[5]);
    UCHAR_combine(buf[11], buf[12], &yt_buf[6]);
    UCHAR_combine(buf[13], buf[14], &yt_buf[7]);
    UCHAR_combine(buf[15], buf[16], &yt_buf[8]);
}

INT yt_msg_query_send(UCHAR cmd, UCHAR param1, UCHAR param2)
{
    UCHAR* SendBuf = (UCHAR*)Malloc(11);
    memset(SendBuf, 0, 11);
    SendBuf[0] = 0x7e;
    SendBuf[9] = 0x5a;
    UCHAR_change(cmd, &SendBuf[1], &SendBuf[2]);
    UCHAR_change(param1, &SendBuf[3], &SendBuf[4]);
    UCHAR_change(param2, &SendBuf[5], &SendBuf[6]);
    uart_drv_write(yt_msg.uart_dev, SendBuf, 10);   
    Free(SendBuf);
    return WM_SUCCESS;
}

STATIC VOID yt_query_send_timer_cb(os_timer_arg_t arg)
{
    if(yt_msg.i == 0) {
        yt_msg.i ++;
        return;
    }
    yt_msg.i++;
    yt_msg.Flag1 = FALSE;
}


/*查询设备状态*/
STATIC VOID yt_query_timer_cb(os_timer_arg_t arg)
{
    GW_WIFI_STAT_E wf_stat = get_wf_gw_status();
    if(STAT_UNPROVISION == wf_stat || \
       STAT_STA_UNCONN == wf_stat) {
        return;
    }
    if(yt_msg.Flag1) {
        return;
    }
    yt_msg.Flag2 = TRUE;
    os_mutex_get(&yt_msg.mutex, OS_WAIT_FOREVER);
    yt_msg_query_send(0, 0, 0);
    os_mutex_put(&yt_msg.mutex);
}
 

INT yt_msg_cmd_type_value(cJSON *root, UCHAR *param1, UCHAR *param2)
{
    *param1 = 0;
    *param2 = 0;
    DEV_CNTL_N_S *dev_cntl = get_single_wf_dev();
    if(NULL == dev_cntl) {
        return WM_FAIL;
    }
    switch(root->type)
    {
        case cJSON_False:*param1 = 0x00;break;
        case cJSON_True:*param1 = 0x01;break;
        case cJSON_Number: {
            if(yt_msg.cmd == 6) {
                if((root->valueint)/60 < 1) {
                    *param2 = root->valueint; 
                    *param1 = 0;
                }else {
                    *param1 = root->valueint/60;
                }
            }else {
                *param1 = root->valueint;
            }
        }
        break;
        case cJSON_String:{
            *param1 = atoi(root->valuestring);      
        }
        break;
        default:break;
    }
    if(yt_msg.cmd == 5) {
        if(*param1 == 0){
            INT Value = 0;
            Value = dev_cntl->dp[2].prop.prop_enum.value;
            *param1 = atoi(dev_cntl->dp[2].prop.prop_enum.pp_enum[Value]);
        }else{
            *param1 = 0;
        }
    }
    return WM_SUCCESS;
}

/***********************************************************
*************************function define********************
***********************************************************/
VOID device_cb(SMART_CMD_E cmd,cJSON *root)
{
#if 1 //husai_test    
    PR_DEBUG("cmd:%d",cmd); 
    cJSON *nxt = root->child;
    yt_msg.Flag1 = TRUE;
    yt_msg.Flag3 = FALSE;
    UINT count = 0;
    while(nxt) {
        if(count > 40) {
            yt_msg.Flag1 = FALSE;
            return;
        } 
        if(yt_msg.Flag2) {   
            count++;
            os_thread_sleep(os_msec_to_ticks(10));
            continue;
        }
        yt_msg.cmd = atoi(nxt->string);
        yt_msg.pstruMsg.CMD = yt_id_cmd(yt_msg.cmd);
        if(yt_msg_cmd_type_value(nxt, &yt_msg.pstruMsg.PARAM1, &yt_msg.pstruMsg.PARAM2) != WM_SUCCESS){
            return;
        }
        os_mutex_get(&yt_msg.mutex, OS_WAIT_FOREVER);
        yt_msg_query_send(yt_msg.pstruMsg.CMD, yt_msg.pstruMsg.PARAM1, yt_msg.pstruMsg.PARAM2);
        os_mutex_put(&yt_msg.mutex);
        yt_msg.i = 0;
        os_timer_activate(&yt_msg.yt_send_timer);
        nxt = nxt->next;
    }
#else
    GW_CNTL_S *gw_cntl = get_gw_cntl();
    UCHAR *out;
    out=cJSON_PrintUnformatted(root);
    if(NULL == out) {
        PR_ERR("cJSON_PrintUnformatted err:");
        return WM_FAIL;
    }
    OPERATE_RET op_ret;
    op_ret = sf_obj_dp_report(gw_cntl->dev->dev_if.id,\
                              out);
    if(op_ret == OPRT_NW_INVALID){
        Free(out);
        return WM_FAIL;
    }else if(OPRT_OK != op_ret) {
        PR_ERR("sf_obj_dp_report op_ret:%d",op_ret);
        Free(out);
        return WM_FAIL;
    }
    Free(out);
#endif
}

#if 1 
STATIC INT yt_msg_upload_proc(UCHAR* yt_buf)
{
    if(os_timer_is_active(&yt_msg.yt_send_timer) != WM_SUCCESS) {
        os_timer_deactivate(&yt_msg.yt_send_timer);
    }

    INT Value = 0;
    GW_CNTL_S *gw_cntl = get_gw_cntl();

    cJSON *root_test = NULL;
    root_test = cJSON_CreateObject();
    if(NULL == root_test) {
        return WM_FAIL;
    }
    CHAR dpid[10];
    CHAR dpid1[10];
    snprintf(dpid,10,"%d",yt_buf[3]); 
    snprintf(dpid1,10,"%d",yt_buf[5] + yt_buf[4]*60);

    if(yt_buf[3] == 0) { /*节能模式*/
        Value = gw_cntl->dev->dp[2].prop.prop_enum.value;
        snprintf(dpid,10,"%d",atoi(gw_cntl->dev->dp[2].prop.prop_enum.pp_enum[Value]));
        yt_buf[3] = 1;
    }else {
        yt_buf[3] = 0;
    }
    
    if(yt_buf[2] == 2){
        cJSON_AddBoolToObject(root_test,"1",0);
    }else{
        cJSON_AddBoolToObject(root_test,"1",yt_buf[2]);     
    }
    cJSON_AddNumberToObject(root_test,"2",yt_buf[6]&0x7f);
    cJSON_AddStringToObject(root_test,"3",dpid);
    cJSON_AddBoolToObject(root_test,"4",yt_buf[8]&0x01);
    cJSON_AddBoolToObject(root_test,"5",yt_buf[3]);
    cJSON_AddNumberToObject(root_test,"6",atoi(dpid1));

    CHAR *out;
    out=cJSON_PrintUnformatted(root_test);
    cJSON_Delete(root_test);
    if(NULL == out) {
        PR_ERR("cJSON_PrintUnformatted err:");
        return WM_FAIL;
    }
    PR_DEBUG("out[%s]", out);
    OPERATE_RET op_ret;
    op_ret = sf_obj_dp_report(gw_cntl->dev->dev_if.id,\
                              out);
    if(op_ret == OPRT_NW_INVALID){
        Free(out);
        return WM_FAIL;
    }else if(OPRT_OK != op_ret) {
        PR_ERR("sf_obj_dp_report op_ret:%d",op_ret);
        Free(out);
        return WM_FAIL;
    }
    Free(out);
    if(yt_msg.Flag2) {
        yt_msg.Flag2 = FALSE;
    }
    if(yt_msg.Flag1) {
        yt_msg.Flag1 = FALSE;
    }  
    return WM_SUCCESS;
}

/*消息处理*/
STATIC INT yt_msg_proc(UCHAR* data, UINT len)
{
    UINT offset = 0;
    INT ret = 0;
    while((len - offset) >= YT_FRAME_MIN_SIZE){
        /*判断头 0x7e*/
        if(data[offset] != 0x7e){
            offset += 1;
            continue;
        }
        INT i = 0;
        /*判断数据是否在0x00和0x0f之间*/
        for (i = (offset + 1); i < (YT_FRAME_MIN_SIZE - 1); i++)
        {
            if((data[i] < 0x00) || (data[i] > 0x0f))
            {
                offset += i;
                continue;
            }
        }
        /*判断尾部 0x5a*/
        if(data[offset + YT_FRAME_MIN_SIZE - 1] != 0x5a)
        {
            offset += (YT_FRAME_MIN_SIZE - 1);
            continue;
        }
        /*接收到完整数据*/
        UCHAR *buf = (UCHAR*)Malloc(YT_FRAME_SIZE + 1);
        memset(buf, 0, YT_FRAME_SIZE + 1);
        yt_msg_combine(data + offset, buf);
        ret = yt_msg_upload_proc(buf);
        Free(buf);
        if(ret != WM_SUCCESS){
            return WM_FAIL;
        }
        offset += YT_FRAME_MIN_SIZE;
    }
    return WM_SUCCESS;
}

/*串口接收*/
STATIC INT yt_msg_recv()
{
    UINT count = 0;
    UINT rCount = 0;
    INT ret;
    memset(yt_msg.uart_recvbuf, 0, 1024);
    while(1){
        count = uart_drv_read(yt_msg.uart_dev, yt_msg.uart_recvbuf + rCount, YT_FRAME_MIN_SIZE);
        rCount += count;
        if(count != 0 && rCount < YT_FRAME_MIN_SIZE){
            os_thread_sleep(os_msec_to_ticks(10));
            continue;
        }
        if(rCount < YT_FRAME_MIN_SIZE){
            os_thread_sleep(os_msec_to_ticks(10));
            continue;
        }
        /*消息处理*/
        if(yt_msg.Flag1 && yt_msg.Flag2 && (yt_msg.uart_recvbuf[1] == 0)) {
            yt_msg.Flag3 = TRUE;
            yt_msg.Flag2 = FALSE;
            return WM_SUCCESS;
        }
        ret = yt_msg_proc(yt_msg.uart_recvbuf, rCount);
        if(ret != WM_SUCCESS){
            return WM_FAIL;
        } 
        rCount = 0;
        memset(yt_msg.uart_recvbuf, 0, 1024); 
    }
    return WM_SUCCESS;
}

/*串口接收线程*/
STATIC VOID yt_task_cb(os_thread_arg_t arg)
{
    YT_RECV_STAT_E status = YT_INIT;

    while(1) {
        switch(status) {
            case YT_INIT: {
                GW_WIFI_STAT_E wf_stat = get_wf_gw_status();
                if(STAT_UNPROVISION == wf_stat || \
                   STAT_STA_UNCONN == wf_stat) {
                    os_thread_sleep(os_msec_to_ticks(1500));
                    continue;
                }

                status = YT_RECV;
            }
            break;

            case YT_RECV: {
                INT ret;
                ret = yt_msg_recv();
                if(ret != WM_SUCCESS) {
                    status = YT_INIT;
                }
            }
            break;

            default: status = YT_INIT; break;
        }
    }
}
#endif

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

    INT ret;
    ret = yt_uart_init();
    if(ret != WM_SUCCESS)
    {
        PR_DEBUG("yt_uart_init failed");
        yt_close_uart();
        return OPRT_COM_ERROR;
    }

    ret = os_timer_create(&yt_msg.yt_timer, "yt_timer", \
                          os_msec_to_ticks(YT_QUERY*1000),\
                          yt_query_timer_cb, NULL,\
                          OS_TIMER_PERIODIC, OS_TIMER_AUTO_ACTIVATE);
    if(ret != WM_SUCCESS) {
        return OPRT_CR_TIMER_ERR;
    }

    ret = os_timer_create(&yt_msg.yt_send_timer, "yt_send_timer", \
                          os_msec_to_ticks(3000),\
                          yt_query_send_timer_cb, NULL,\
                          OS_TIMER_PERIODIC, OS_TIMER_NO_ACTIVATE);
    if(ret != WM_SUCCESS) {
        return OPRT_CR_TIMER_ERR;
    }


    ret = os_mutex_create(&yt_msg.mutex, "yt_mutex", 1);
    if(ret != WM_SUCCESS) {
        return OPRT_CR_MUTEX_ERR;
    }

    ret = os_thread_create(&yt_msg.thread,"yt_task",\
                           yt_task_cb,0, &yt_stack, OS_PRIO_2);
    if(ret != WM_SUCCESS) {
        return OPRT_CR_THREAD_ERR;
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













