/***********************************************************
*  File: device.c 
*  Author: nzy
*  Date: 20150605
***********************************************************/
#define __DEVICE_GLOBALS
#include "device.h"
#include <mdev_uart.h>
#include "com_struct.h"
#include "com_def.h"
#include "mem_pool.h"
#include <wm_os.h>
#include <wm_net.h>
#include <wlan.h>
#include "json.h"
#include "mqtt_client.h"
#include <wmtime.h>
#include <mdev_ssp.h>
#include <mdev_i2c.h>
#include "app_agent.h"

/***********************************************************
*************************micro define***********************
***********************************************************/
#define YT_QUERY 1
#define YT_FRAME_MIN_SIZE 22
#define YT_FRAME_SIZE 12

typedef struct
{
    BYTE SOI;        /*固定为0x7e*/
    BYTE CMD;       /*1字节*/
    BYTE PARAM1;    /*PARAM字段*/
    BYTE PARAM2;
    BYTE EOI;        /*固定为0x5a*/
}struSendMsg;

typedef struct
{
    BYTE SOI;        /*固定为0x7e*/
    BYTE ACK;       /*1字节*/
    BYTE PARAM1;    /*PARAM字段*/
    BYTE PARAM2;
    BYTE PARAM3;
    BYTE PARAM4;
    BYTE PARAM5;
    BYTE PARAM6;
    BYTE PARAM7;
    BYTE PARAM8;
    BYTE PARAM9;
    BYTE EOI;        /*固定为0x5a*/
}struRecvMsg;

typedef struct
{
    struSendMsg pstruMsg;
    struRecvMsg pstruRecMsg;
    BYTE cmd;
    mdev_t *uart_dev;
    os_timer_t yt_timer;
    MUTEX_HANDLE mutex; 
}YT_MSG;

/***********************************************************
*************************function define********************
***********************************************************/
STATIC os_thread_stack_define(yt_stack, 2048);
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
CONST CHAR dp_schemas[] = "[ {\"id\": 1, \"code\": \"switch\", \"name\": \"开关\", \"mode\": \"rw\", \"type\": \"obj\", \"property\": {\"type\": \"bool\" }},{\"id\": 2, \"code\": \"temperature\", \"name\": \"温度\", \"mode\": \"rw\", \"type\": \"obj\", \"property\": {\"type\": \"value\", \"unit\": \"℃\", \"min\": 5, \"max\": 37, \"step\": 1 }}, { \"id\": 3, \"code\": \"gear\", \"name\": \"档位\", \"mode\": \"rw\", \"type\": \"obj\", \"property\": { \"type\": \"enum\", \"range\": [ \"1\", \"2\", \"3\" ] }}, {\"id\": 4, \"code\": \"lock\", \"name\": \"童锁\", \"mode\": \"rw\", \"type\": \"obj\", \"property\": {\"type\": \"bool\" }}, {\"id\": 5, \"code\": \"eco\", \"name\": \"eco节能模式\", \"mode\": \"rw\", \"type\": \"obj\", \"property\": {\"type\": \"bool\" }},{ \"id\": 6, \"code\": \"appoint\", \"name\": \"预约\", \"mode\": \"rw\", \"type\": \"obj\", \"property\": { \"type\": \"enum\", \"range\": [ \"0\", \"30\", \"60\", \"120\", \"180\", \"240\", \"300\", \"360\", \"420\", \"480\", \"540\"],\"unit\":\"m\" }}]";

// KEY
KEY_ENTITY_S key_tbl[] = {
    {WF_RESET_KEY,5000,key_process,0,0,0,0},
};

YT_MSG yt_msg; 

/***********************************************************
*************************function define********************
***********************************************************/
/*把一个字节拆分成两字节*/
void byte_change(uint temp, char* hight, char* low)
{
    *hight = (temp & 0xf0) >> 4;
    *low = temp & 0x0f;
}

/*把两个字节合并成一个字节*/
void byte_combine(uint hight, uint low, char* temp)
{
    *temp = (hight << 4) | low;
}

STATIC INT yt_query_msg_proc_upload(CHAR* yt_buf)
{
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

    if(yt_buf[4] == 0) {/*定时时间*/
        snprintf(dpid1,10,"%d",yt_buf[5]);
    }else {
        snprintf(dpid1,10,"%d",(yt_buf[4]*60));
    }

    if(yt_buf[3] == 0) { /*节能模式*/
        Value = gw_cntl->dev->dp[2].prop.prop_enum.value;
        snprintf(dpid,10,"%d",atoi(gw_cntl->dev->dp[2].prop.prop_enum.pp_enum[Value]));
        yt_buf[3] = 1;
    }else {
        yt_buf[3] = 0;
    }
    cJSON_AddBoolToObject(root_test,"1",yt_buf[2]);
    cJSON_AddNumberToObject(root_test,"2",yt_buf[6]&0x7f);
    cJSON_AddStringToObject(root_test,"3",dpid);
    cJSON_AddBoolToObject(root_test,"4",yt_buf[8]&0x01);
    cJSON_AddBoolToObject(root_test,"5",yt_buf[3]);
    cJSON_AddStringToObject(root_test,"6",dpid1);

    CHAR *out;
    out=cJSON_PrintUnformatted(root_test);
    cJSON_Delete(root_test);
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
    return WM_SUCCESS;
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

VOID yt_msg_combine(CHAR* buf, CHAR* yt_buf)
{
    yt_buf[0] = buf[0];
    yt_buf[11] = buf[21];
    byte_combine(buf[1], buf[2], &yt_buf[1]); //ack
    byte_combine(buf[3], buf[4], &yt_buf[2]);  //param1
    byte_combine(buf[5], buf[6], &yt_buf[3]);
    byte_combine(buf[7], buf[8], &yt_buf[4]);
    byte_combine(buf[9], buf[10], &yt_buf[5]);
    byte_combine(buf[11], buf[12], &yt_buf[6]);
    byte_combine(buf[13], buf[14], &yt_buf[7]);
    byte_combine(buf[15], buf[16], &yt_buf[8]);
}

INT yt_msg_query(BYTE cmd, BYTE param1, BYTE param2, CHAR* buf)
{
    INT rCount = 0;
    INT i;
    INT count;

    CHAR* SendBuf = (CHAR*)Malloc(11);
    memset(SendBuf, 0, 11);
    SendBuf[0] = 0x7e;
    SendBuf[9] = 0x5a;
    byte_change(cmd, &SendBuf[1], &SendBuf[2]);
    byte_change(param1, &SendBuf[3], &SendBuf[4]);
    byte_change(param2, &SendBuf[5], &SendBuf[6]);

    uart_drv_write(yt_msg.uart_dev, SendBuf, 10);   
    Free(SendBuf);
    for(i = 0; i < 5; i++){
        count = 0;
        os_thread_sleep(os_msec_to_ticks(100));
        count = uart_drv_read(yt_msg.uart_dev, buf + rCount, 25);
        if(count == 0 && i != 0){
            break;
        }
        rCount += count;
    }
    return rCount;
}

INT yt_data_analyse(BYTE cmd, BYTE param1, BYTE param2, CHAR* buf)
{
    INT rCount = 0;
    INT offset = 0;
    INT frame_num = 0;
    CHAR* RecvBuf = (CHAR*)Malloc(126);
    memset(RecvBuf, 0, 126);

    rCount = yt_msg_query(cmd, param1, param2, RecvBuf);
    if(rCount < YT_FRAME_MIN_SIZE){
        Free(RecvBuf);
        return 0;
    }

    while((rCount - offset) >= YT_FRAME_MIN_SIZE){
        /*判断头 0x7e*/
        if(RecvBuf[offset] != 0x7e)
        {
            offset += 1;
            continue;
        }
        INT i = 0;
        /*判断数据是否在0x00和0x0f之间*/
        for (i = (offset + 1); i < (YT_FRAME_MIN_SIZE - 1); i++)
        {
            if((RecvBuf[i] < 0x00) || (RecvBuf[i] > 0x0f))
            {
                offset += i;
                continue;
            }
        }
        /*判断尾部 0x5a*/
        if(RecvBuf[offset + YT_FRAME_MIN_SIZE - 1] != 0x5a)
        {
            offset += (YT_FRAME_MIN_SIZE - 1);
            continue;
        }
        /*-------------收到一帧完整数据----------*/
        yt_msg_combine(RecvBuf + offset, buf + frame_num*YT_FRAME_SIZE);
        offset += YT_FRAME_MIN_SIZE;
        frame_num++;
    }
    Free(RecvBuf);
    return frame_num;
}

/*查询设备状态*/
STATIC VOID yt_query_timer_cb(os_timer_arg_t arg)
{
    INT frame_num = 0;
    CHAR* RecvBuf = (CHAR*)Malloc(YT_FRAME_SIZE*5 + 1);
    memset(RecvBuf, 0, YT_FRAME_SIZE*5 + 1);

    os_mutex_get(&yt_msg.mutex, OS_WAIT_FOREVER);
    frame_num = yt_data_analyse(0, 0, 0, RecvBuf);
    os_mutex_put(&yt_msg.mutex);
    if(frame_num == 0){
        Free(RecvBuf);
        return;
    }
    INT ret;
    ret = yt_query_msg_proc_upload(RecvBuf + (frame_num - 1)*YT_FRAME_SIZE);
    if(ret != WM_SUCCESS)
    {
        Free(RecvBuf);
        return;
    }
    Free(RecvBuf);
}
 

INT yt_msg_cmd_type_value(cJSON *root, BYTE *param1, BYTE *param2)
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
        case cJSON_Number:*param1 = root->valueint;break;
        case cJSON_String:*param1 = atoi(root->valuestring);break;
        default:break;
    }
    if(yt_msg.cmd == 5){
        if(*param1 == 0){
            INT Value = 0;
            Value = dev_cntl->dp[2].prop.prop_enum.value;
            *param1 = atoi(dev_cntl->dp[2].prop.prop_enum.pp_enum[Value]);
        }else{
            *param1 = 0;
        }
    }else if(yt_msg.cmd == 6){
        if((*param1/60) < 1){
            *param2 = *param1;
            *param1 = 0;
        }else{
            *param1 = *param1/60;
        }
    }
    return WM_SUCCESS;
}

STATIC INT yt_msg_proc(CHAR* buf)
{
    INT compare = 0;
    struRecvMsg *pstruRecMsg = (struRecvMsg*)buf;   
    if(yt_msg.pstruMsg.CMD != pstruRecMsg->ACK){
        PR_DEBUG("cmd != ack");
        return WM_FAIL;
    }
    switch(yt_msg.cmd)
    {
        case 1:
            if(yt_msg.pstruMsg.PARAM1 != pstruRecMsg->PARAM1){
                return WM_FAIL;
            }
        break;

        case 2:
            compare = (pstruRecMsg->PARAM5)&0x7f;
            if(compare != yt_msg.pstruMsg.PARAM1){
                return WM_FAIL;
            }
        break;

        case 3:
            if(pstruRecMsg->PARAM2 != yt_msg.pstruMsg.PARAM1){
                return WM_FAIL;
            }
        break;

        case 4:
            compare = (pstruRecMsg->PARAM7)&0x01;
            if(compare != yt_msg.pstruMsg.PARAM1){
                return WM_FAIL;
            }
        break;

        case 5:
            if(pstruRecMsg->PARAM2 != yt_msg.pstruMsg.PARAM1){
                return WM_FAIL;
            }
        break;

        case 6:
            if((pstruRecMsg->PARAM3 != yt_msg.pstruMsg.PARAM1) || (pstruRecMsg->PARAM4 != yt_msg.pstruMsg.PARAM2)){
                return WM_FAIL;
            }
        break;

        default:return WM_FAIL;break;
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
    while(nxt) {
        yt_msg.cmd = atoi(nxt->string);
        yt_msg.pstruMsg.CMD = yt_id_cmd(yt_msg.cmd);
        if(yt_msg.pstruMsg.CMD == 0x00){
            //cJSON_Delete(nxt);
            return; 
        } 
        if(yt_msg_cmd_type_value(nxt, &yt_msg.pstruMsg.PARAM1, &yt_msg.pstruMsg.PARAM2) != WM_SUCCESS){
            //cJSON_Delete(nxt);
            return;
        }
        nxt = nxt->next;
    }
    //cJSON_Delete(nxt);
    INT frame_num = 0;
    CHAR* RecvBuf = (CHAR*)Malloc(YT_FRAME_SIZE*5 + 1);
    memset(RecvBuf, 0, YT_FRAME_SIZE*5 + 1);
    os_mutex_get(&yt_msg.mutex, OS_WAIT_FOREVER);
    frame_num = yt_data_analyse(yt_msg.pstruMsg.CMD, yt_msg.pstruMsg.PARAM1, yt_msg.pstruMsg.PARAM2, RecvBuf);
    os_mutex_put(&yt_msg.mutex);
    if(frame_num == 0){
        Free(RecvBuf);
        return;
    }
    INT ret;
    INT offset = 0;
    while(frame_num){
        ret = yt_msg_proc(RecvBuf + offset);
        if(ret != WM_SUCCESS)
        {
            Free(RecvBuf);
            return;
        }
        ret = yt_query_msg_proc_upload(RecvBuf + offset);
        if(ret != WM_SUCCESS)
        {
            Free(RecvBuf);
            return;
        }
        offset += YT_FRAME_SIZE;
        frame_num--;
    }
    Free(RecvBuf);
#endif   
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

    ret = os_mutex_create(&yt_msg.mutex, "yt_mutex", 1);
    if(ret != WM_SUCCESS) {
        return OPRT_CR_MUTEX_ERR;
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
            auto_select_wf_cfg();
        }else if(SEQ_KEY == type && cnt >= 5) { // data restore factory
            
        }
    }
}

STATIC OPERATE_RET device_differ_init(VOID)
{
    OPERATE_RET op_ret;
    
    // key process init
    op_ret = key_init(key_tbl,(CONST INT)CNTSOF(key_tbl),30);
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
                led_blink(WF_DIR_LEN, 500, 500);
            }
            break;
            
            case STAT_AP_STA_UNCONN:
            case STAT_AP_STA_CONN: {
                led_blink(WF_DIR_LEN, 1000, 1000);
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













