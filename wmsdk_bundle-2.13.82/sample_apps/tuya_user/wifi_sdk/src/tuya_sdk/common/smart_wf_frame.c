/***********************************************************
*  File: smart_wf_frame.c
*  Author: nzy
*  Date: 20150611
***********************************************************/
#define __SMART_WF_FRAME_GLOBALS
#include "smart_wf_frame.h"
#include "mqtt_client.h"
#include "base64.h"
#include "error_code.h"
#include <wm_os.h>
#include "app_agent.h"
#include "md5.h"
#include "device.h"

/***********************************************************
*************************micro define***********************
***********************************************************/
// smart frame process message
#define SF_MSG_LAN_CMD_FROM_APP 0 // 局域网控制命令
#define SF_MSG_LAN_STAT_REPORT 1 // 局域网数据状态主动上报
#define SF_MSG_UG_FM 2 // 通知固件升级

// msg frame 
typedef struct {
    INT socket;
    UINT frame_num;
    UINT frame_type;
    UINT len;
    CHAR data[0];
}SF_MLCFA_FR_S;

typedef enum {
    SF_DA_IDLE = 0,
    SF_DA_WF_CFG_STAT,
}SF_DATA_STAT_E;

typedef struct {
    os_queue_t msg_que;
    MUTEX_HANDLE mutex;
    SMART_FRAME_CB cb;
    THREAD thread;
    INT is_start;
    
    #define HB_TIMER_INTERVAL (4*60) // s
    TIMER_ID hb_timer;

    // firmware upgrade
    TIMER_ID fw_ug_t;
    FW_UG_STAT_E stat;
    FW_UG_S fw_ug;
}SMT_FRM_CNTL_S;

#define SMT_FRM_MAX_EVENTS 10

// mqtt protocol
#define PRO_CMD 5
#define PRO_ADD_USER 6
#define PRO_DEL_USER 7
#define PRO_FW_UG_CFM 10

#define UG_TIME_VAL (24*3600)
/***********************************************************
*************************variable define********************
***********************************************************/
SMT_FRM_CNTL_S smt_frm_cntl;
static os_queue_pool_define(smt_frm_queue_data,SMT_FRM_MAX_EVENTS * sizeof(MESSAGE));
static os_thread_stack_define(sf_stack, 1024);
static void fw_ug_timer_cb(os_timer_arg_t arg);
STATIC OPERATE_RET sw_ver_change(IN CONST CHAR *ver,OUT UINT *change);
VOID set_fw_ug_stat(IN CONST FW_UG_STAT_E stat);

/***********************************************************
*************************function define********************
***********************************************************/
static void sf_ctrl_task(os_thread_arg_t arg);
STATIC VOID mq_callback(BYTE *data,UINT len);
STATIC VOID sf_mlcfa_proc(IN CONST SF_MLCFA_FR_S *fr);
extern int lan_set_net_work(char *ssid,char *passphrase);
static void hb_timer_cb(os_timer_arg_t arg);
STATIC CHAR *mk_json_obj_data(IN CONST DEV_CNTL_N_S *dev_cntl,IN CONST BOOL vlc_da);
STATIC OPERATE_RET __sf_com_mk_obj_dp_rept_data(IN CONST DEV_CNTL_N_S *dev_cntl,OUT CHAR **pp_out);
STATIC OPERATE_RET sf_obj_dp_qry_dpid(IN CONST CHAR *id,IN CONST BYTE *dpid,IN CONST BYTE num);
STATIC void smt_frm_cmd_prep(IN CONST SMART_CMD_E cmd,IN CONST BYTE *data,IN CONST UINT len);

VOID gw_lan_respond(IN CONST INT fd,IN CONST UINT fr_num,IN CONST UINT fr_tp,\
                    IN CONST UINT ret_code,IN CONST CHAR *data,IN CONST UINT len)
{
    OPERATE_RET op_ret;

    op_ret = mlp_gw_tcp_send(fd,fr_num,fr_tp,ret_code,(BYTE *)data,len);
    if(op_ret != 0) {
        PR_ERR("lan_msg_tcp_send error:%d,errno:%d",op_ret,errno);
        return;
    }
}

/***********************************************************
*  Function: smart_frame_init
*  Input: 
*  Output: 
*  Return: 
*  Note:
***********************************************************/
OPERATE_RET smart_frame_init(IN CONST SMART_FRAME_CB cb)
{
    if(smt_frm_cntl.is_start) {
        return OPRT_OK;
    }

    // memory pool init
    OPERATE_RET op_ret;
    op_ret = SysMemoryPoolSetup();
    if(OPRT_OK != op_ret) {
        return op_ret;
    }

    // gw_cntl init
    op_ret = gw_cntl_init();
    if(OPRT_OK != op_ret) {
        return op_ret;
    }

    // mqtt init
    GW_CNTL_S *gw_cntl = get_gw_cntl();
    #if 0
    // make mqtt passwd
    CHAR mq_passwd[16+1];
    unsigned char decrypt[16];
    MD5_CTX md5;
    MD5Init(&md5);
    MD5Update(&md5,(BYTE *)gw_cntl->gw.id,strlen(gw_cntl->gw.id));
    MD5Final(&md5,decrypt);
    INT offset = 0;
    INT i;
    for(i = 0;i < 8;i++) {
        sprintf(&mq_passwd[offset],"%02x",decrypt[4+i]);
        offset += 2;
    }
    mq_passwd[offset] = 0;
    mq_client_init(gw_cntl->gw.id,gw_cntl->gw.id,mq_passwd,0);
    #else
    mq_client_init(gw_cntl->gw.id,"8888888","123456",0);
    #endif

    // mqtt starat
    op_ret = mq_client_start(gw_cntl->gw.id,mq_callback);
    if(op_ret != OPRT_OK) {
        return op_ret;
    }

    // lan agent init
    op_ret = lan_pro_cntl_init();
    if(op_ret != OPRT_OK) {
        return op_ret;
    }
    
    // smart frame init
    if(NULL == cb) {
        return OPRT_INVALID_PARM;
    }
    memset(&smt_frm_cntl,0,sizeof(smt_frm_cntl));

    int ret = 0;
    ret = os_queue_create(&smt_frm_cntl.msg_que, "sf_que", \
                          SIZEOF(P_MESSAGE),&smt_frm_queue_data);
    if(WM_SUCCESS != ret) {
        return OPRT_CR_QUE_ERR;
    }

    ret = os_mutex_create(&smt_frm_cntl.mutex, "sf_mutex", 1);
    if(ret != WM_SUCCESS) {
        return OPRT_CR_MUTEX_ERR;
    }

    ret = os_timer_create(&smt_frm_cntl.hb_timer, "hb_timer", \
                          os_msec_to_ticks(10*1000),\
                          hb_timer_cb, NULL,\
                          OS_TIMER_PERIODIC, OS_TIMER_AUTO_ACTIVATE);
    if(ret != WM_SUCCESS) {
        return OPRT_CR_TIMER_ERR;
    }

    ret = os_timer_create(&smt_frm_cntl.fw_ug_t, "fw_ug_timer", \
                          os_msec_to_ticks(10*1000),\
                          fw_ug_timer_cb, NULL,\
                          OS_TIMER_PERIODIC, OS_TIMER_AUTO_ACTIVATE);
    if(ret != WM_SUCCESS) {
        return OPRT_CR_TIMER_ERR;
    }

    ret = os_thread_create(&smt_frm_cntl.thread,"sf_task",\
			               sf_ctrl_task,0, &sf_stack, OS_PRIO_3);
    if(ret != WM_SUCCESS) {
        return OPRT_CR_THREAD_ERR;
    }

    smt_frm_cntl.cb = cb;
    smt_frm_cntl.is_start = 1;

    return OPRT_OK;
}

static void sf_ctrl_task(os_thread_arg_t arg)
{
    int ret;
    P_MESSAGE msg;

    PR_DEBUG("%s start",__FUNCTION__);
    while(1) {
        ret = os_queue_recv(&smt_frm_cntl.msg_que, &msg,OS_WAIT_FOREVER);
        if (ret != WM_SUCCESS) {
            PR_ERR("os_queue_recv ret:%d", ret);
            continue;
        }

        // Todo msg process
        switch(msg->msg_id) {
            case SF_MSG_LAN_CMD_FROM_APP: {
                sf_mlcfa_proc((SF_MLCFA_FR_S *)msg->data);                
            }
            break;

            case SF_MSG_LAN_STAT_REPORT: {
                #if 0
                PR_DEBUG("len:%d",msg->len);
                PR_DEBUG("data:%s",msg->data);
                #else
                PR_DEBUG("len:%d",msg->len);
                PR_DEBUG("data:%s",msg->data);
                
                INT num = get_lpc_socket_num();
                INT *socket = get_lpc_sockets();
                INT i;
                for(i = 0;i < num;i++) {
                    gw_lan_respond(socket[i],0,FRM_TP_STAT_REPORT,0,(CHAR *)msg->data,msg->len);
                }
                #endif
            }
            break;

            case SF_MSG_UG_FM: {
                PR_DEBUG("now,we'll go to upgrade firmware");
                OPERATE_RET op_ret;
                
                op_ret = httpc_up_fw_ug_stat(get_single_wf_dev()->dev_if.id,UPGRADING);
                if(OPRT_OK != op_ret) {
                    PR_DEBUG("httpc_up_fw_ug_stat error:%d",op_ret);
                    break;
                }
                set_fw_ug_stat(UPGRADING);
                
                op_ret = httpc_upgrade_fw(smt_frm_cntl.fw_ug.fw_url);
                if(OPRT_OK != op_ret) {
                    PR_ERR("httpc_upgrade_fw err:%d",op_ret);

                    op_ret = httpc_up_fw_ug_stat(get_single_wf_dev()->dev_if.id,UG_EXECPTION);
                    if(OPRT_OK != op_ret) {
                        PR_DEBUG("httpc_up_fw_ug_stat error:%d",op_ret);
                    }
                    set_fw_ug_stat(UG_EXECPTION);
                    break;
                }

                op_ret = httpc_up_fw_ug_stat(get_single_wf_dev()->dev_if.id,UG_FIN);
                if(OPRT_OK != op_ret) {
                    PR_DEBUG("httpc_up_fw_ug_stat error:%d",op_ret);
                }
                set_fw_ug_stat(UG_FIN);

                pm_reboot_soc();
            }
            break;
        }

        Free(msg);
    }
}

/***********************************************************
*  Function: single_wf_device_init
*  Input: 
*  Output: 
*  Return: 
*  Note: if can not read dev_if in the flash ,so use the def_dev_if
*        def_dev_if->id 由内部产生
***********************************************************/
OPERATE_RET single_wf_device_init(INOUT DEV_DESC_IF_S *def_dev_if,\
                                  IN CONST CHAR *dp_schemas)
{
    if(NULL == def_dev_if || \
       NULL == dp_schemas) {
        return OPRT_INVALID_PARM;
    }

    if(UN_INIT == get_gw_status()) {
        return OPRT_OK;
    }

    OPERATE_RET op_ret;
    GW_CNTL_S *gw_cntl = get_gw_cntl();

    // device data restore
    DEV_DESC_IF_S dev_if;
    memset(&dev_if,0,sizeof(dev_if));
    ws_db_get_dev_if(&dev_if);
    if(0 == dev_if.id[0] || \
       0 == dev_if.name[0] || \
       0 == dev_if.sw_ver[0] || \
       0 == dev_if.schema_id[0] || \
       0 == dev_if.ui_id[0]) {
        PR_DEBUG("set defult info");
        
        memcpy(&dev_if,def_dev_if,sizeof(dev_if));
        strcpy(dev_if.id,gw_cntl->gw.id);
        dev_if.bind = FALSE;
        dev_if.sync = FALSE;

        op_ret = ws_db_set_dev_if(&dev_if);
        if(op_ret != OPRT_OK) {
            PR_ERR("ws_db_set_dev_if error op_ret:%d",op_ret);
        }
    }else {
        if((strlen(gw_cntl->gw.id) != strlen(dev_if.id)) || \
           (strcasecmp(gw_cntl->gw.id,dev_if.id))) {
            strcpy(dev_if.id,gw_cntl->gw.id);
            dev_if.bind = FALSE;
            dev_if.sync = FALSE;
            op_ret = ws_db_set_dev_if(&dev_if);
            if(op_ret != OPRT_OK) {
                PR_ERR("ws_db_set_dev_if error op_ret:%d",op_ret);
            }
        }
    }

    op_ret = gw_lc_bind_device(&dev_if,dp_schemas);
    if(OPRT_OK != op_ret) {
        return op_ret;
    }

    if(get_gw_status() > ACTIVE_RD) {
        check_and_update_dev_desc_if(dev_if.id,NULL,def_dev_if->sw_ver,\
                                     def_dev_if->schema_id,def_dev_if->ui_id);
    }

// for debug
#if 0
    PR_DEBUG("dev_num:%d",gw_cntl.dev_num);
    PR_DEBUG("id:%s",gw_cntl.dev->dev_if.id);
    PR_DEBUG("name:%s",gw_cntl.dev->dev_if.name);
    PR_DEBUG("schema_id:%s",gw_cntl.dev->dev_if.schema_id);
    PR_DEBUG("ui_id:%s",gw_cntl.dev->dev_if.ui_id);
    PR_DEBUG("sw_ver:%s",gw_cntl.dev->dev_if.sw_ver);
    PR_DEBUG("ability:%d",gw_cntl.dev->dev_if.ability);
    PR_DEBUG("dp_num:%d",gw_cntl.dev->dp_num);

    PR_DEBUG();
    INT i;
    DP_CNTL_S *dp;
    for(i = 0;i < gw_cntl.dev->dp_num;i++) {
        dp = &gw_cntl.dev->dp[i];
        PR_DEBUG("dp_id:%d",dp->dp_desc.dp_id);
        PR_DEBUG("mode:%d",dp->dp_desc.mode);
        PR_DEBUG("type:%d",dp->dp_desc.type);
        PR_DEBUG("trig_t:%d",dp->dp_desc.trig_t);
        if(dp->dp_desc.type != T_OBJ) {
            continue;
        }
        PR_DEBUG("prop_tp:%d",dp->dp_desc.prop_tp);
        if(dp->dp_desc.prop_tp == PROP_BOOL) {
            
        }else if(dp->dp_desc.prop_tp == PROP_VALUE) {
            PR_DEBUG("max:%d",dp->prop.prop_value.max);
            PR_DEBUG("min:%d",dp->prop.prop_value.min);
            PR_DEBUG("step:%d",dp->prop.prop_value.step);
            PR_DEBUG("scale:%d",dp->prop.prop_value.scale);
        }else if(dp->dp_desc.prop_tp == PROP_STR) {
            PR_DEBUG("max_len:%d",dp->prop.prop_str.max_len);
        }else if(dp->dp_desc.prop_tp == PROP_ENUM) {
            INT j;
            for(j = 0;j < dp->prop.prop_enum.cnt;j++) {
                PR_DEBUG("%s",dp->prop.prop_enum.pp_enum[j]);
            }
        }
        PR_DEBUG();
    }
#endif

    return OPRT_OK;
}

STATIC VOID mq_callback(BYTE *data,UINT len)
{
    // parse protocol
    cJSON *root = NULL;
    root = cJSON_Parse((CHAR *)data);
    if(NULL == root) {
        goto JSON_PROC_ERR;
    }

    cJSON *json = NULL;
    // protocol
    json = cJSON_GetObjectItem(root,"protocol");
    if(NULL == json) {
        goto JSON_PROC_ERR;
    }
    INT mq_pro;
    mq_pro = json->valueint;

    // data
    json = cJSON_GetObjectItem(root,"data");
    if(NULL == json) {
        goto JSON_PROC_ERR;
    }
    GW_CNTL_S *gw_cntl = get_gw_cntl();
    if((NULL == cJSON_GetObjectItem(json,"gwId")) || \
       strcasecmp(cJSON_GetObjectItem(json,"gwId")->valuestring,\
                  gw_cntl->gw.id)) {
        PR_ERR("gwid:%s",gw_cntl->gw.id);
        goto JSON_PROC_ERR;
    }

    if(PRO_CMD == mq_pro) {
        BYTE *buf = (BYTE *)cJSON_PrintUnformatted(json);
        smt_frm_cmd_prep(MQ_CMD,buf,strlen((CHAR *)buf));
        Free(buf);
    }else if(PRO_ADD_USER == mq_pro || \
             PRO_DEL_USER == mq_pro) {
        if(gw_cntl->active.uid_cnt >= UID_ACL_LMT && \
           PRO_ADD_USER == mq_pro) {
            PR_ERR("the number of user are out of limit");
            goto JSON_PROC_ERR;
        }

        // uid
        if(NULL == cJSON_GetObjectItem(json,"uid")) {
            PR_ERR("no uid");
            goto JSON_PROC_ERR;
        }

        INT i;
        for(i = 0;i < gw_cntl->active.uid_cnt;i++) {
            if(!strcasecmp(cJSON_GetObjectItem(json,"uid")->valuestring,\
                           gw_cntl->active.uid_acl[i])) {
                break;
            }
        }

        if(PRO_ADD_USER == mq_pro) {
            if(i < gw_cntl->active.uid_cnt) {
                PR_ERR("already add this uid");
                goto JSON_PROC_ERR;
            }
            strcpy(gw_cntl->active.uid_acl[gw_cntl->active.uid_cnt++],\
                   cJSON_GetObjectItem(json,"uid")->valuestring);
        }else {
            if(i >= gw_cntl->active.uid_cnt) {
                goto JSON_PROC_ERR;
            }

            for(;i < gw_cntl->active.uid_cnt-1;i++) {
                strcpy(gw_cntl->active.uid_acl[i],gw_cntl->active.uid_acl[i+1]);
            }
            gw_cntl->active.uid_cnt--;
        }

        ws_db_set_gw_actv(&gw_cntl->active);
    }else if(PRO_FW_UG_CFM == mq_pro) {
        if((NULL == cJSON_GetObjectItem(json,"devId") || \
           strcasecmp(cJSON_GetObjectItem(json,"devId")->valuestring,\
           get_single_wf_dev()->dev_if.id))) {
            goto JSON_PROC_ERR;
        }

        OPERATE_RET op_ret;
        op_ret = sf_fw_ug_msg_infm();
        if(OPRT_OK != op_ret) {
            PR_ERR("sf_fw_ug_msg_infm:%d",op_ret);
        }
    }else {
        goto JSON_PROC_ERR;
    }

    cJSON_Delete(root);
    return;

JSON_PROC_ERR:
    PR_ERR("%s",data);
    cJSON_Delete(root);
}

/***********************************************************
*  Function: smart_frame_send_msg
*  Input: 
*  Output: 
*  Return: 
*  Note:
***********************************************************/
OPERATE_RET smart_frame_send_msg(IN CONST UINT msgid,\
                                 IN CONST VOID *data,\
                                 IN CONST UINT len)
{
    P_MESSAGE msg = Malloc(sizeof(MESSAGE)+ len+1);
    if(!msg) {
        return OPRT_MALLOC_FAILED;
    }
    memset(msg,0,sizeof(MESSAGE)+ len+1);
    
    msg->msg_id = msgid;
    msg->len = len;
    if(data && len) {
        memcpy(msg->data,data,len);
    }

    int ret;
    ret = os_queue_send(&smt_frm_cntl.msg_que, &msg,OS_WAIT_FOREVER);
    if(WM_SUCCESS != ret) {
        Free(msg);
        return OPRT_SND_QUE_ERR;
    }

    return OPRT_OK;
}

STATIC VOID sf_mlcfa_proc(IN CONST SF_MLCFA_FR_S *fr)
{
    if(NULL == fr) {
        return;
    }

    STATIC CHAR ssid[32+1];
    STATIC CHAR passwd[32+1];

    switch(fr->frame_type) {
        case FRM_TP_CFG_WF: {
            GW_WIFI_STAT_E  gw_wf_stat = get_wf_gw_status();
            if(gw_wf_stat == STAT_UNPROVISION || \
               gw_wf_stat == STAT_STA_UNCONN) {
                PR_ERR("the gw have not lan network");
                return;
            }

            cJSON *root = NULL;
            root = cJSON_Parse(fr->data);
            if(NULL == root) {
                goto FRM_TP_CFG_ERR;
            }

            #if 0
            PR_DEBUG("ssid:%s",cJSON_GetObjectItem(root,"ssid")->valuestring);
            PR_DEBUG("passwd:%s",cJSON_GetObjectItem(root,"passwd")->valuestring);
            
            int ret = 0;
            ret = lan_set_net_work(cJSON_GetObjectItem(root,"ssid")->valuestring,\
                                   cJSON_GetObjectItem(root,"passwd")->valuestring);
            if(WM_SUCCESS != ret) {
                goto FRM_TP_CFG_ERR;
            }
            #else
            if(cJSON_GetObjectItem(root,"ssid") == NULL) {
                goto FRM_TP_CFG_ERR;
            }
            
            memset(ssid,0,sizeof(ssid));
            memset(passwd,0,sizeof(passwd));
            strcpy(ssid,cJSON_GetObjectItem(root,"ssid")->valuestring);
            strcpy(passwd,cJSON_GetObjectItem(root,"passwd")->valuestring);

            GW_STAT_E gw_stat = get_gw_status();
            if(gw_stat >= ACTIVE_RD) {
                int ret;
                if(passwd[0] == 0) {
                    ret = lan_set_net_work(ssid,NULL);
                }else {
                    PR_DEBUG("passwd:%s",passwd);
                    ret = lan_set_net_work(ssid,passwd);
                }
                if(ret != WM_SUCCESS) {
                    PR_ERR("ret %d",ret);
                }
            }
            #endif

            gw_lan_respond(fr->socket, fr->frame_num, fr->frame_type,0,NULL,0);
            cJSON_Delete(root);
            break;

            FRM_TP_CFG_ERR:
            PR_ERR("recv:%s",fr->data);
            cJSON_Delete(root);
            gw_lan_respond(fr->socket, fr->frame_num, fr->frame_type,\
                           1,"set wifi cfg failed",strlen("set wifi cfg failed"));
            break;
        }
        break;

        case FRM_TP_ACTV: {
            GW_STAT_E gw_stat = get_gw_status();
            if(UN_INIT == gw_stat) { // 返回错误提示，未写入生产信息
                gw_lan_respond(fr->socket, fr->frame_num, fr->frame_type,\
                               1,"the gateway is not initalize",strlen("the gateway is not initalize"));

            }else if(UN_ACTIVE == gw_stat) {
                cJSON *root = NULL;
                root = cJSON_Parse(fr->data);
                if(NULL == root) {
                    goto FRM_TP_ACTV_ERR;
                }

                // get token
                GW_CNTL_S *gw_cntl = get_gw_cntl();
                cJSON *json = cJSON_GetObjectItem(root,"token");
                if(NULL == json) {
                    goto FRM_TP_ACTV_ERR;
                }
                strcpy(gw_cntl->active.token,json->valuestring);

                // get uid
                json = cJSON_GetObjectItem(root,"uid");
                if(NULL == json) {
                    goto FRM_TP_ACTV_ERR;
                }
                gw_cntl->active.uid_cnt = 1;
                strcpy(gw_cntl->active.uid_acl[0],json->valuestring);
                gw_cntl->active.key[0] = 0;

                // add by nzy 20150704 process http mqtt url
                if((json = cJSON_GetObjectItem(root,"httpUrl"))) {
                    snprintf(gw_cntl->active.http_url,\
                             sizeof(gw_cntl->active.http_url),\
                             "%s",json->valuestring);
                }else {
                    gw_cntl->active.http_url[0] = 0;
                }

                if((json = cJSON_GetObjectItem(root,"mqUrl"))) {
                    snprintf(gw_cntl->active.mq_url,\
                             sizeof(gw_cntl->active.mq_url),\
                             "%s",json->valuestring);
                }else {
                    gw_cntl->active.mq_url[0] = 0;
                }
                
                if((json = cJSON_GetObjectItem(root,"mqUrlBak"))) {
                    snprintf(gw_cntl->active.mq_url_bak,\
                             sizeof(gw_cntl->active.mq_url_bak),\
                             "%s",json->valuestring);
                }else {
                    gw_cntl->active.mq_url_bak[0] = 0;
                }

                ws_db_set_gw_actv(&gw_cntl->active);
                start_active_gateway();
                cJSON_Delete(root);

                gw_lan_respond(fr->socket, fr->frame_num, fr->frame_type,\
                               0,NULL,0);

                GW_WIFI_STAT_E wf_stat = get_wf_gw_status();
                if((STAT_AP_STA_UNCONN == wf_stat || 
                    STAT_AP_STA_CONN == wf_stat)) {
                    INT ret;                    
                    
                    PR_DEBUG("ssid:%s",ssid);
                    if(passwd[0] == 0) {
                        ret = lan_set_net_work(ssid,NULL);
                    }else {
                        PR_DEBUG("passwd:%s",passwd);
                        ret = lan_set_net_work(ssid,passwd);
                    }
                    if(ret != WM_SUCCESS) {
                        PR_ERR("ret %d",ret);
                    }
                }
                break;

                FRM_TP_ACTV_ERR:
                PR_ERR("recv:%s",fr->data);
                cJSON_Delete(root);
                gw_lan_respond(fr->socket, fr->frame_num, fr->frame_type,\
                               1,"data format error",strlen("data format error"));
                break;
            }else { // respond ok
                gw_lan_respond(fr->socket, fr->frame_num, fr->frame_type,\
                               0,NULL,0);
            } 
        }
        break;

        case FRM_TP_BIND_DEV: {
            
        }
        break;

        case FRM_TP_UNBIND_DEV: {
            
        }
        break;

        case FRM_TP_CMD: {
            cJSON *root = NULL;

            if(UPGRADING == get_fw_ug_stat()) {
                gw_lan_respond(fr->socket, fr->frame_num, fr->frame_type,\
                               1,"device is upgrading...",strlen("device is upgrading..."));
                goto FRM_TP_CMD_ERR;
            }
            
            root = cJSON_Parse(fr->data);
            if(NULL == root) {
                gw_lan_respond(fr->socket, fr->frame_num, fr->frame_type,\
                               1,"data format error",strlen("data format error"));
                goto FRM_TP_CMD_ERR;
            }
            
            if(NULL == cJSON_GetObjectItem(root,"devId") || \
               NULL == cJSON_GetObjectItem(root,"dps") || \
               NULL == cJSON_GetObjectItem(root,"uid")) {
                gw_lan_respond(fr->socket, fr->frame_num, fr->frame_type,\
                               1,"data format error",strlen("data format error"));
                goto FRM_TP_CMD_ERR;
            }

            INT i;
            GW_CNTL_S *gw_cntl = get_gw_cntl();
            for(i = 0;i < gw_cntl->active.uid_cnt;i++) {
                if(0 == strcasecmp(gw_cntl->active.uid_acl[i],cJSON_GetObjectItem(root,"uid")->valuestring)) {
                    break;
                }
            }
            if(i >= gw_cntl->active.uid_cnt) {
                gw_lan_respond(fr->socket, fr->frame_num, fr->frame_type,\
                               1,"no permission",strlen("no permission"));
                goto FRM_TP_CMD_ERR;
            }

            cJSON_DeleteItemFromObject(root,"uid");
            
            CHAR *out;
            out=cJSON_PrintUnformatted(root);
            smt_frm_cmd_prep(LAN_CMD,(BYTE *)out,strlen(out));

            gw_lan_respond(fr->socket, fr->frame_num, fr->frame_type,\
                           0,NULL,0);
            Free(out);
            cJSON_Delete(root);
            break;
            
            FRM_TP_CMD_ERR:
            cJSON_Delete(root);
            PR_ERR("recv:%s",fr->data);
            break;
        }
        break;
        
        case FRM_QUERY_STAT: {
            cJSON *root = NULL;
            root = cJSON_Parse(fr->data);
            if(NULL == root) {
                gw_lan_respond(fr->socket, fr->frame_num, fr->frame_type,\
                               1,"data format error",strlen("data format error"));
                goto FRM_QRY_STAT_ERR;
            }
            
            if(NULL == cJSON_GetObjectItem(root,"gwId") || \
               NULL == cJSON_GetObjectItem(root,"devId")) {
                gw_lan_respond(fr->socket, fr->frame_num, fr->frame_type,\
                               1,"data format error",strlen("data format error"));
                goto FRM_QRY_STAT_ERR;
            }

            GW_CNTL_S *gw_cntl = get_gw_cntl();
            if(strcasecmp(gw_cntl->gw.id,\
                          cJSON_GetObjectItem(root,"devId")->valuestring)) {
                gw_lan_respond(fr->socket, fr->frame_num, fr->frame_type,\
                               1,"gw id invalid",strlen("gw id invalid"));
                goto FRM_QRY_STAT_ERR;
            }

            DEV_CNTL_N_S *dev_cntl = get_dev_cntl(cJSON_GetObjectItem(root,"devId")->valuestring);
            if(NULL == dev_cntl) {
                gw_lan_respond(fr->socket, fr->frame_num, fr->frame_type,\
                               1,"devid not found",strlen("devid not found"));
                goto FRM_QRY_STAT_ERR;
            }

            CHAR *data = mk_json_obj_data(dev_cntl,FALSE);
            if(NULL == data) {
                PR_ERR("mk_json_obj_data error");
                goto FRM_QRY_STAT_ERR;
            }

            gw_lan_respond(fr->socket, fr->frame_num, fr->frame_type,\
                           0,data,strlen(data));

            Free(data);
            cJSON_Delete(root);
            break;
            
            FRM_QRY_STAT_ERR:
            PR_ERR("recv:%s",fr->data);
            cJSON_Delete(root);
            break;
        }
        break;

        default: {
            PR_ERR("unsupport frame type:%d",fr->frame_type);
        }
        break;
    }
}

/***********************************************************
*  Function: mk_json_obj_data->make json object dp data from device
*  Input: vlc_da->本地有效OBJ数据OR全部OBJ数据
*  Output: 
*  Return: data format:{"devid":"xx","dps":{"1",1}}
*  Note:
***********************************************************/
STATIC CHAR *mk_json_obj_data(IN CONST DEV_CNTL_N_S *dev_cntl,IN CONST BOOL vlc_da)
{
    if(NULL == dev_cntl) {
        return NULL;
    }
    
    cJSON *cjson = cJSON_CreateObject();
    if(NULL == cjson) {
        return NULL;
    }

    INT i;
    BOOL data_valid = FALSE;
    for(i = 0;i < dev_cntl->dp_num;i++) {
        if(dev_cntl->dp[i].dp_desc.type != T_OBJ || \
           dev_cntl->dp[i].dp_desc.mode == M_WR) {
            continue;
        }

        if(dev_cntl->dp[i].pv_stat == INVALID || \
           (dev_cntl->dp[i].pv_stat == VALID_CLOUD && TRUE == vlc_da)) {
            continue;
        }

        data_valid = TRUE;

        CHAR dpid[10];
        snprintf(dpid,10,"%d",dev_cntl->dp[i].dp_desc.dp_id);
        
        switch(dev_cntl->dp[i].dp_desc.prop_tp) {
            case PROP_BOOL: {
                cJSON_AddBoolToObject(cjson,dpid,dev_cntl->dp[i].prop.prop_bool.value);
            }
            break;

            case PROP_VALUE: {
                cJSON_AddNumberToObject(cjson,dpid,dev_cntl->dp[i].prop.prop_value.value);
            }
            break;
            
            case PROP_STR: {
                cJSON_AddStringToObject(cjson,dpid,dev_cntl->dp[i].prop.prop_str.value);
            }
            break;
            
            case PROP_ENUM: {
                INT value = 0;
                value = dev_cntl->dp[i].prop.prop_enum.value;
                cJSON_AddStringToObject(cjson,dpid,dev_cntl->dp[i].prop.prop_enum.pp_enum[value]);
            }
            break;
        }
    }

    if(FALSE == data_valid) {
        cJSON_Delete(cjson);
        return NULL;
    }
    
    cJSON *root = cJSON_CreateObject();
    if(NULL == root) {
        cJSON_Delete(cjson);
        return NULL;
    }
    cJSON_AddStringToObject(root,"devId",dev_cntl->dev_if.id);
    cJSON_AddItemToObject(root, "dps", cjson);

    CHAR *out = cJSON_PrintUnformatted(root);
    cJSON_Delete(root),root = NULL;

    return out;
}

/***********************************************************
*  Function: sf_mk_dp_rept_data
*  Input: data->the format is :{"1":1,"2":true} or {"1":"base64"}
*  Output: 
*  Return: 
*  Note:
***********************************************************/
STATIC OPERATE_RET __sf_mk_dp_rept_data(IN CONST DEV_CNTL_N_S *dev_cntl,IN CONST CHAR *data,OUT CHAR **pp_out)
{
    if(dev_cntl == NULL || data == NULL || NULL == pp_out) {
        return OPRT_INVALID_PARM;
    }

    OPERATE_RET op_ret = OPRT_OK;
    cJSON *data_json = cJSON_Parse(data);
    if(NULL == data_json) {
        op_ret = OPRT_CJSON_PARSE_ERR;
        goto ERR_EXIT;
    }

    // make content
    cJSON *root=cJSON_CreateObject();
    if(NULL == root) {
        op_ret = OPRT_CR_CJSON_ERR;
        goto ERR_EXIT;
    }
    cJSON_AddStringToObject(root,"devId",dev_cntl->dev_if.id);
    cJSON_AddItemToObject(root, "dps", data_json);

    *pp_out = cJSON_PrintUnformatted(root);
    cJSON_Delete(root),data_json = NULL,root = NULL;
    if(NULL == *pp_out) {
        op_ret = OPRT_MALLOC_FAILED;
        goto ERR_EXIT;
    }
    
    return op_ret;
    
ERR_EXIT:
    cJSON_Delete(data_json);
    return op_ret;
}


/***********************************************************
*  Function: __sf_mk_obj_dp_rept_data
*  Input: 
*  Output: 
*  Return: 
*  Note:
***********************************************************/
STATIC OPERATE_RET __sf_com_mk_obj_dp_rept_data(IN CONST DEV_CNTL_N_S *dev_cntl,OUT CHAR **pp_out)
{
    if(NULL == dev_cntl || \
       NULL == pp_out) {
         return OPRT_INVALID_PARM;
    }

    cJSON *cjson = cJSON_CreateObject();
    if(NULL == cjson) {
        return OPRT_CR_CJSON_ERR;
    }

    BOOL nd_rept = FALSE;
    INT i;
    for(i = 0;i < dev_cntl->dp_num;i++) {        
        if(dev_cntl->dp[i].dp_desc.type != T_OBJ || \
           dev_cntl->dp[i].dp_desc.mode == M_WR || \
           dev_cntl->dp[i].dp_desc.passive == PSV_TRUE) {
            continue;
        }

        if(dev_cntl->dp[i].pv_stat == INVALID) {
            continue;
        }
        
        // already upload and trigger direct
        if((dev_cntl->dp[i].pv_stat == VALID_CLOUD) && \
           (dev_cntl->dp[i].dp_desc.trig_t != TRIG_DIRECT)) {
            continue;
        }
        
        CHAR dpid[10];
        snprintf(dpid,10,"%d",dev_cntl->dp[i].dp_desc.dp_id);
        switch(dev_cntl->dp[i].dp_desc.prop_tp) {
            case PROP_BOOL: {
                cJSON_AddBoolToObject(cjson,dpid,dev_cntl->dp[i].prop.prop_bool.value);
            }
            break;
        
            case PROP_VALUE: {
                cJSON_AddNumberToObject(cjson,dpid,dev_cntl->dp[i].prop.prop_value.value);
            }
            break;
            
            case PROP_STR: {
                cJSON_AddStringToObject(cjson,dpid,dev_cntl->dp[i].prop.prop_str.value);
            }
            break;
            
            case PROP_ENUM: {
                INT value = 0;
                value = dev_cntl->dp[i].prop.prop_enum.value;
                cJSON_AddStringToObject(cjson,dpid,dev_cntl->dp[i].prop.prop_enum.pp_enum[value]);
            }
            break;
        }

        nd_rept = TRUE;
    }

    // do not need report data
    if(FALSE == nd_rept) {
        cJSON_Delete(cjson);
        *pp_out = NULL;
        return OPRT_OK;
    }
    
    // make content
    cJSON *root=cJSON_CreateObject();
    if(NULL == root) {
        cJSON_Delete(cjson);
        return OPRT_CR_CJSON_ERR;
    }
    cJSON_AddStringToObject(root,"devId",dev_cntl->dev_if.id);
    cJSON_AddItemToObject(root, "dps",cjson);

    *pp_out = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if(NULL == *pp_out) {
        return OPRT_MALLOC_FAILED;
    }

    return OPRT_OK;
}


/***********************************************************
*  Function: __sf_dev_lc_obj_dp_up 更新本地device obj dp的数据值
*  Input: data->the format is :{"1":1,"2":true}/{"devId":"xx","dps":{"1",1}}
*  Output: 
*  Return: 
*  Note:
***********************************************************/
STATIC OPERATE_RET __sf_dev_lc_obj_dp_up(INOUT DEV_CNTL_N_S *dev_cntl,IN CONST CHAR *data)
{
    if(data == NULL || NULL == dev_cntl) {
        return OPRT_INVALID_PARM;
    }

    OPERATE_RET op_ret;
    cJSON *nxt = NULL;
    cJSON *data_json = cJSON_Parse(data);
    if(NULL == data_json) {
        op_ret = OPRT_CJSON_PARSE_ERR;
        goto ERR_EXIT;
    }

    if(cJSON_GetObjectItem(data_json,"devId") && cJSON_GetObjectItem(data_json,"dps")) {
        nxt = cJSON_GetObjectItem(data_json,"dps")->child;
    }else {
        nxt = data_json->child;
    }

    while(nxt) {
        INT id = atoi(nxt->string);
        INT i;
        DP_CNTL_S *dp_cntl =  NULL;
        for(i = 0; i < dev_cntl->dp_num;i++) {
            if(dev_cntl->dp[i].dp_desc.dp_id == id) {
                dp_cntl = &dev_cntl->dp[i];
                break;
            }
        }

        if(NULL == dp_cntl) {
            op_ret = OPRT_NOT_FOUND_DEV_DP;
            goto ERR_EXIT;
        }

        // find dp
        if(dp_cntl->dp_desc.mode == M_WR || \
           dp_cntl->dp_desc.type != T_OBJ) {
            nxt = nxt->next;
            continue;
        }

        switch(dp_cntl->dp_desc.prop_tp) {
            case PROP_BOOL: {
                if(nxt->type != cJSON_False && \
                   nxt->type != cJSON_True) {
                    op_ret = OPRT_DP_TYPE_PROP_ILLEGAL;
                    goto ERR_EXIT;
                }
                
                if(INVALID == dp_cntl->pv_stat) {
                    dp_cntl->prop.prop_bool.value = nxt->type;
                    dp_cntl->pv_stat = VALID_LC;
                }else {
                    if(dp_cntl->prop.prop_bool.value != nxt->type) {
                        dp_cntl->prop.prop_bool.value = nxt->type;
                        dp_cntl->pv_stat = VALID_LC;
                    }
                }
            }
            break;

            case PROP_VALUE: {
                if(nxt->type != cJSON_Number || \
                   nxt->valueint > dp_cntl->prop.prop_value.max || \
                   nxt->valueint < dp_cntl->prop.prop_value.min) {
                    op_ret = OPRT_DP_TYPE_PROP_ILLEGAL;
                    goto ERR_EXIT;
                }

                if(INVALID == dp_cntl->pv_stat) {
                    dp_cntl->prop.prop_value.value = nxt->valueint;
                    dp_cntl->pv_stat = VALID_LC;
                }else {
                    if(dp_cntl->prop.prop_value.value != nxt->valueint) {
                        dp_cntl->prop.prop_value.value = nxt->valueint;
                        dp_cntl->pv_stat = VALID_LC;
                    }
                }
            }
            break;

            case PROP_STR: {
                if(nxt->type != cJSON_String || \
                   strlen(nxt->valuestring) > dp_cntl->prop.prop_str.max_len) {
                    op_ret = OPRT_DP_TYPE_PROP_ILLEGAL;
                    goto ERR_EXIT;
                }

                if(INVALID == dp_cntl->pv_stat) {
                    strcpy(dp_cntl->prop.prop_str.value,nxt->valuestring);
                    dp_cntl->pv_stat = VALID_LC;
                }else {
                    if((strlen(dp_cntl->prop.prop_str.value) != strlen(nxt->valuestring)) || \
                        strcmp(dp_cntl->prop.prop_str.value,nxt->valuestring)) {
                        strcpy(dp_cntl->prop.prop_str.value,nxt->valuestring);
                        dp_cntl->pv_stat = VALID_LC;
                    }
                }
            }
            break;

            case PROP_ENUM: {
                if(nxt->type != cJSON_String) {
                    op_ret = OPRT_DP_TYPE_PROP_ILLEGAL;
                    goto ERR_EXIT;
                }

                INT j = 0;
                for(j = 0; j < dp_cntl->prop.prop_enum.cnt;j++) {
                    if(!strcmp(dp_cntl->prop.prop_enum.pp_enum[j],nxt->valuestring)) {
                        
                        if(INVALID == dp_cntl->pv_stat) {
                            dp_cntl->pv_stat = VALID_LC;
                            dp_cntl->prop.prop_enum.value = j;
                        }else {
                            if(dp_cntl->prop.prop_enum.value != j) {
                                dp_cntl->pv_stat = VALID_LC;
                                dp_cntl->prop.prop_enum.value = j;
                            }
                        }
                        break;
                    }
                }
                if(j >= dp_cntl->prop.prop_enum.cnt) {
                    op_ret = OPRT_DP_TYPE_PROP_ILLEGAL;
                    goto ERR_EXIT;
                }
            }
            break;
        }

        nxt = nxt->next;
    }

    cJSON_Delete(data_json);
    return OPRT_OK;
    
ERR_EXIT:
    cJSON_Delete(data_json);
    return op_ret;
}


/***********************************************************
*  Function: __sf_dev_obj_dp_set_vc 本地存储DP值成功上传至云端
*  Input: data->the format is :{"1":1,"2":true}/{"devId":"xx","dps":{"1",1}}
*  Output: 
*  Return: 
*  Note:
***********************************************************/
STATIC OPERATE_RET __sf_dev_obj_dp_set_vc(INOUT DEV_CNTL_N_S *dev_cntl,IN CONST CHAR *data)
{
    cJSON *nxt = NULL;
    cJSON *data_json = cJSON_Parse(data);
    if(cJSON_GetObjectItem(data_json,"devId") && cJSON_GetObjectItem(data_json,"dps")) {
        nxt = cJSON_GetObjectItem(data_json,"dps")->child;
    }else {
        nxt = data_json->child;
    }
    
    while(nxt) {
        INT id = atoi(nxt->string);
        INT i;
        DP_CNTL_S *dp_cntl =  NULL;
        for(i = 0; i < dev_cntl->dp_num;i++) {
            if(dev_cntl->dp[i].dp_desc.dp_id == id) {
                dp_cntl = &dev_cntl->dp[i];
                break;
            }
        }

        // find dp
        if(dp_cntl->dp_desc.mode == M_WR || \
           dp_cntl->dp_desc.type != T_OBJ) {
            nxt = nxt->next;
            continue;
        }

        dp_cntl->pv_stat = VALID_CLOUD;
        if(PSV_F_ONCE == dp_cntl->dp_desc.passive) {
            dp_cntl->dp_desc.passive = PSV_TRUE;
        }
        nxt = nxt->next;
    }

    cJSON_Delete(data_json);
    return OPRT_OK;
}

#if 0
/***********************************************************
*  Function: sf_simp_dp_report
*  Input: data->format is:{"devid":"xx","dps":{"1",1}}
*  Output: 
*  Return: 
*  Note:
***********************************************************/
OPERATE_RET sf_simp_dp_report(IN CONST CHAR *id,IN CONST CHAR *data) 
{
    if(STAT_STA_CONN != get_wf_gw_status()) {
        return OPRT_NW_INVALID;
    }

    if(data == NULL) {
        return OPRT_INVALID_PARM;
    }

    OPERATE_RET op_ret;

    // smart dp report    
    op_ret = httpc_device_dp_report(T_OBJ,data);
    if(op_ret != OPRT_OK) {
        return OPRT_DP_REPORT_CLOUD_ERR;
    }

    // update dp value
    __sf_dev_obj_dp_set_vc(id,data);

    return OPRT_OK;
}
#endif

/***********************************************************
*  Function: sf_obj_dp_report ,obj dp report for user
*  Input: data->the format is :{"1":1,"2":true}/{"devid":"xx","dps":{"1",1}}
*  Output: 
*  Return: 
*  Note:
***********************************************************/
OPERATE_RET sf_obj_dp_report(IN CONST CHAR *id,IN CONST CHAR *data) 
{
    if(STAT_STA_CONN != get_wf_gw_status()) {
        return OPRT_NW_INVALID;
    }

    if(id == NULL || data == NULL) {
        return OPRT_INVALID_PARM;
    }

    DEV_CNTL_N_S *dev_cntl = get_dev_cntl(id);
    if(NULL == dev_cntl) {
        return OPRT_NOT_FOUND_DEV;
    }

    if(dev_cntl->dev_if.bind == FALSE) {
        return OPRT_DEV_NOT_BIND;
    }

    // update local data
    OPERATE_RET op_ret;
    op_ret = __sf_dev_lc_obj_dp_up(dev_cntl,data);
    if(op_ret != OPRT_OK) {
        return op_ret;
    }

    CHAR *out = NULL;
    op_ret = __sf_com_mk_obj_dp_rept_data(dev_cntl,&out);
    if(op_ret != OPRT_OK) {
        return op_ret;
    }

    // PR_DEBUG("out:%s",out);
    if(NULL == out) {
        return OPRT_OK;
    }

    // lan dp report
    op_ret = smart_frame_send_msg(SF_MSG_LAN_STAT_REPORT,out,strlen(out));
    if(op_ret != OPRT_OK) {
        Free(out);
        return op_ret;
    }

    // smart dp report
    op_ret = httpc_device_dp_report(T_OBJ,out);
    if(op_ret != OPRT_OK) {
        Free(out);
        return OPRT_DP_REPORT_CLOUD_ERR;
    }

    // update dp value
    __sf_dev_obj_dp_set_vc(dev_cntl,out);
    Free(out);
    return OPRT_OK;
}

/***********************************************************
*  Function: sf_obj_dp_query ,obj dp query to device
*  Input: dpid:dpid buf num:dpid num
*  Output: 
*  Return: 
*  Note:
***********************************************************/
STATIC OPERATE_RET sf_obj_dp_qry_dpid(IN CONST CHAR *id,IN CONST BYTE *dpid,IN CONST BYTE num)
{
    if(NULL == id || \
       0 == num) {
        return OPRT_INVALID_PARM;
    }

    DEV_CNTL_N_S *dev_cntl = get_dev_cntl(id);
    if(NULL == dev_cntl) {
        return OPRT_NOT_FOUND_DEV;
    }

    OPERATE_RET op_ret;

    cJSON *qr_data = cJSON_CreateObject();
    if(NULL == qr_data) {
        return OPRT_CR_CJSON_ERR;
    }

    INT i;
    for(i = 0;i < num;i++) {
        INT id = dpid[i];
        
        INT j;
        DP_CNTL_S *dp_cntl =  NULL;
        for(j = 0; j < dev_cntl->dp_num;j++) {
            if(dev_cntl->dp[j].dp_desc.dp_id == id) {
                dp_cntl = &dev_cntl->dp[j];
                break;
            }
        }

        // find dp
        if(dp_cntl->dp_desc.mode == M_WR || \
           dp_cntl->dp_desc.type != T_OBJ) {
            continue;
        }

        CHAR dpid[10];
        snprintf(dpid,10,"%d",id);

        switch(dp_cntl->dp_desc.prop_tp) {
            case PROP_BOOL: {
                cJSON_AddBoolToObject(qr_data,dpid,dp_cntl->prop.prop_bool.value);
            }
            break;
        
            case PROP_VALUE: {
                cJSON_AddNumberToObject(qr_data,dpid,dp_cntl->prop.prop_value.value);
            }
            break;
        
            case PROP_STR: {
                cJSON_AddStringToObject(qr_data,dpid,dp_cntl->prop.prop_str.value);
            }
            break;
        
            case PROP_ENUM: {
                INT cnt = dp_cntl->prop.prop_enum.value;
                cJSON_AddStringToObject(qr_data,dpid,dp_cntl->prop.prop_enum.pp_enum[cnt]);
            }
            break;
        }
    }

    cJSON *qr_root = cJSON_CreateObject();
    if(NULL == qr_root) {
        cJSON_Delete(qr_data);
        return OPRT_CR_CJSON_ERR;
    }
    cJSON_AddStringToObject(qr_root,"devId",id);
    cJSON_AddItemToObject(qr_root, "dps", qr_data);

    CHAR *out;
    out=cJSON_PrintUnformatted(qr_root);
    cJSON_Delete(qr_root);
    if(NULL == out) {
        return OPRT_MALLOC_FAILED;
    }

    // lan dp report
    op_ret = smart_frame_send_msg(SF_MSG_LAN_STAT_REPORT,out,strlen(out));
    if(op_ret != OPRT_OK) {
        Free(out);
        return op_ret;
    }

    // smart dp report
    op_ret = httpc_device_dp_report(T_OBJ,out);
    if(op_ret != OPRT_OK) {
        Free(out);
        return OPRT_DP_REPORT_CLOUD_ERR;
    }

    Free(out);
    return OPRT_OK;
}

#if 0
/***********************************************************
*  Function: sf_obj_dp_query ,obj dp query to device
*  Input: data->the format is :{"1":null,"2":null}/{"devid":"xx","dps":{"1",null}}
*  Output: 
*  Return: 
*  Note:
***********************************************************/
OPERATE_RET sf_obj_dp_query(IN CONST CHAR *id,IN CONST CHAR *data)
{
    if(NULL == id) {
        return OPRT_INVALID_PARM;
    }

    DEV_CNTL_N_S *dev_cntl = get_dev_cntl(id);
    if(NULL == dev_cntl) {
        return OPRT_NOT_FOUND_DEV;
    }

    OPERATE_RET op_ret;
    cJSON *nxt = NULL;
    cJSON *data_json = cJSON_Parse(data);
    if(NULL == data_json) {
        op_ret = OPRT_CJSON_PARSE_ERR;
        goto ERR_EXIT;
    }

    if(cJSON_GetObjectItem(data_json,"devId") && cJSON_GetObjectItem(data_json,"dps")) {
        nxt = cJSON_GetObjectItem(data_json,"dps")->child;
    }else {
        nxt = data_json->child;
    }

    cJSON *qr_data = cJSON_CreateObject();
    if(NULL == qr_data) {
        op_ret = OPRT_CR_CJSON_ERR;
        goto ERR_EXIT;
    }

    BOOL query = FALSE;
    while(nxt) {
        if(nxt->type != cJSON_NULL) {
            continue;
        }

        INT id = atoi(nxt->string);
        INT i;
        DP_CNTL_S *dp_cntl =  NULL;
        for(i = 0; i < dev_cntl->dp_num;i++) {
            if(dev_cntl->dp[i].dp_desc.dp_id == id) {
                dp_cntl = &dev_cntl->dp[i];
                break;
            }
        }

        // find dp
        if(dp_cntl->dp_desc.mode == M_WR || \
           dp_cntl->dp_desc.type != T_OBJ) {
            nxt = nxt->next;
            continue;
        }

        query = TRUE;
        switch(dp_cntl->dp_desc.prop_tp) {
            case PROP_BOOL: {
                cJSON_AddBoolToObject(qr_data,nxt->string,dp_cntl->prop.prop_bool.value);
            }
            break;
        
            case PROP_VALUE: {
                cJSON_AddNumberToObject(qr_data,nxt->string,dp_cntl->prop.prop_value.value);
            }
            break;
        
            case PROP_STR: {
                cJSON_AddStringToObject(qr_data,nxt->string,dp_cntl->prop.prop_str.value);
            }
            break;
        
            case PROP_ENUM: {
                INT cnt = dp_cntl->prop.prop_enum.value;
                cJSON_AddStringToObject(qr_data,nxt->string,dp_cntl->prop.prop_enum.pp_enum[cnt]);
            }
            break;
        }
        
        nxt = nxt->next;
    }
    cJSON_Delete(data_json);

    if(FALSE == query) {
        cJSON_Delete(qr_data);
        return OPRT_OK;
    }

    cJSON *qr_root = cJSON_CreateObject();
    if(NULL == qr_root) {
        cJSON_Delete(qr_data);
        return OPRT_CR_CJSON_ERR;
    }
    cJSON_AddStringToObject(qr_root,"devId",id);
    cJSON_AddItemToObject(qr_root, "dps", qr_data);

    CHAR *out;
    out=cJSON_PrintUnformatted(qr_root);
    cJSON_Delete(qr_root);
    if(NULL == out) {
        return OPRT_MALLOC_FAILED;
    }

    // lan dp report
    op_ret = smart_frame_send_msg(SF_MSG_LAN_STAT_REPORT,out,strlen(out));
    if(op_ret != OPRT_OK) {
        Free(out);
        return op_ret;
    }

    // smart dp report
    op_ret = httpc_device_dp_report(T_OBJ,out);
    if(op_ret != OPRT_OK) {
        Free(out);
        return OPRT_DP_REPORT_CLOUD_ERR;
    }

    Free(out);
    return OPRT_OK;
    
ERR_EXIT:
    cJSON_Delete(data_json);
    return op_ret;
}
#endif

/***********************************************************
*  Function: sf_raw_dp_report ,raw dp report for user
*  Input: data->raw 
*  Output: 
*  Return: 
*  Note:
***********************************************************/
OPERATE_RET sf_raw_dp_report(IN CONST CHAR *id,IN CONST BYTE dpid,\
                             IN CONST BYTE *data, IN CONST UINT len)
{
    if(STAT_STA_CONN != get_wf_gw_status()) {
        return OPRT_NW_INVALID;
    }

    if(id == NULL || data == NULL) {
        return OPRT_INVALID_PARM;
    }

    DEV_CNTL_N_S *dev_cntl = get_dev_cntl(id);
    if(NULL == dev_cntl) {
        return OPRT_NOT_FOUND_DEV;
    }

    if(dev_cntl->dev_if.bind == FALSE) {
        return OPRT_DEV_NOT_BIND;
    }

    CHAR *base64_buf = Malloc(2*len +1);
    if(NULL == base64_buf) {
        return OPRT_MALLOC_FAILED;
    }

    base64_encode( data, base64_buf, len );

    // make data content
    cJSON *root=cJSON_CreateObject();
    if(NULL == root) {
        Free(base64_buf);
        return OPRT_CR_CJSON_ERR;
    }

    CHAR dpid_buf[10];
    snprintf(dpid_buf,10,"%d",dpid);
    cJSON_AddStringToObject(root,dpid_buf,base64_buf);

    CHAR *out=cJSON_PrintUnformatted(root);
    if(out == NULL) {
        Free(base64_buf);
        return OPRT_CJSON_GET_ERR;
    }

    Free(base64_buf),base64_buf = NULL;
    cJSON_Delete(root),root = NULL;

    CHAR *tmp_out = NULL;
    OPERATE_RET op_ret;
    op_ret = __sf_mk_dp_rept_data(dev_cntl,out,&tmp_out);
    if(op_ret != OPRT_OK) {
        Free(out),out = NULL;
        return op_ret;
    }
    Free(out),out = NULL;

    // lan dp report
    op_ret = smart_frame_send_msg(SF_MSG_LAN_STAT_REPORT,tmp_out,strlen(tmp_out));
    if(op_ret != OPRT_OK) {
        Free(tmp_out);
        return op_ret;
    }

    // smart dp report    
    op_ret = httpc_device_dp_report(T_OBJ,tmp_out);
    if(op_ret != OPRT_OK) {
        Free(tmp_out);
        return OPRT_DP_REPORT_CLOUD_ERR;
    }

    Free(tmp_out);
    return OPRT_OK;
}

static void hb_timer_cb(os_timer_arg_t arg)
{
    if(STAT_STA_CONN != get_wf_gw_status() || \
       get_gw_status() < STAT_WORK) {
        return;
    }

    PR_DEBUG("heart beat");
    
    OPERATE_RET op_ret;
    op_ret = httpc_gw_hearat();
    if(op_ret != OPRT_OK) {
        PR_ERR("httpc_gw_hearat error:%d",op_ret);
    }

    STATIC INT is_hb = 0;
    if(0 == is_hb) {
        if(WM_SUCCESS == os_timer_change(&smt_frm_cntl.hb_timer,\
                                         os_msec_to_ticks(HB_TIMER_INTERVAL*1000),0)) {
            is_hb = 1;
        }
    }
}

static void fw_ug_timer_cb(os_timer_arg_t arg)
{
    if(STAT_STA_CONN != get_wf_gw_status() || \
       (get_gw_status() < STAT_WORK) || \
       (get_fw_ug_stat() == UPGRADING)) {
        return;
    }

    PR_DEBUG("firmware upgrade start...");
    
    OPERATE_RET op_ret;
    op_ret = httpc_get_fw_ug_info(DEV_ETAG,&smt_frm_cntl.fw_ug);
    if(OPRT_OK != op_ret) {
        if(op_ret == OPRT_FW_NOT_EXIST) {
            os_timer_change(&smt_frm_cntl.fw_ug_t,os_msec_to_ticks(UG_TIME_VAL*1000),0);
            PR_DEBUG("the firmware is not exist");
            return;
        }
        
        PR_ERR("get fw ug info error:%d",op_ret);
        goto ERR_RET;
    }

    PR_DEBUG("fw_url:%s",smt_frm_cntl.fw_ug.fw_url);
    PR_DEBUG("fw_md5:%s",smt_frm_cntl.fw_ug.fw_md5);
    PR_DEBUG("serv_sw_ver:%s",smt_frm_cntl.fw_ug.sw_ver);
    PR_DEBUG("sw_ver:%s",get_single_wf_dev()->dev_if.sw_ver);
    PR_DEBUG("auto_ug:%d",smt_frm_cntl.fw_ug.auto_ug);

    UINT sw_ver = 0;
    UINT serv_sw_ver = 0;
    sw_ver_change(get_single_wf_dev()->dev_if.sw_ver,&sw_ver);
    sw_ver_change(smt_frm_cntl.fw_ug.sw_ver,&serv_sw_ver);
    
    if(serv_sw_ver > sw_ver) {
        if(smt_frm_cntl.fw_ug.auto_ug) { // auto 
            op_ret = sf_fw_ug_msg_infm();
            if(op_ret != OPRT_OK) {
                PR_ERR("sf_fw_ug_msg_infm error:%d",op_ret);
                goto ERR_RET;
            }
        }else {
            op_ret = httpc_up_fw_ug_stat(get_single_wf_dev()->dev_if.id,UG_RD);
            if(OPRT_OK != op_ret) {
                PR_ERR("up_fw_ug_stat error:%d",op_ret);
                goto ERR_RET;
            }
            set_fw_ug_stat(UG_RD);
        }
    }else {
        PR_DEBUG("do't need to upgrade firmware");
    }

    os_timer_change(&smt_frm_cntl.fw_ug_t,os_msec_to_ticks(UG_TIME_VAL*1000),0);
    return;

ERR_RET:
    os_timer_change(&smt_frm_cntl.fw_ug_t,os_msec_to_ticks(10*1000),0);
}

/***********************************************************
*  Function: smart_frame_recv_appmsg
*  Input:  
*  Output: 
*  Return: 
*  Note:
***********************************************************/
OPERATE_RET smart_frame_recv_appmsg(IN CONST INT socket,\
                                    IN CONST UINT frame_num,\
                                    IN CONST UINT frame_type,\
                                    IN CONST CHAR *data,\
                                    IN CONST UINT len)
{
    P_MESSAGE msg = Malloc(sizeof(MESSAGE)+sizeof(SF_MLCFA_FR_S)+len+1);
    if(!msg) {
        return OPRT_MALLOC_FAILED;
    }
    memset(msg,0,sizeof(MESSAGE)+sizeof(SF_MLCFA_FR_S)+len+1);

    msg->msg_id = SF_MSG_LAN_CMD_FROM_APP;
    msg->len = sizeof(SF_MLCFA_FR_S)+len;

    SF_MLCFA_FR_S *fr = (SF_MLCFA_FR_S *)((CHAR *)msg+sizeof(MESSAGE));
    fr->socket = socket;
    fr->frame_num = frame_num;
    fr->frame_type = frame_type;
    fr->len = len;
    if(data && len) {
        memcpy(fr->data,data,len);
    }

    int ret;
    ret = os_queue_send(&smt_frm_cntl.msg_que, &msg,OS_WAIT_FOREVER);
    if(WM_SUCCESS != ret) {
        Free(msg);
        return OPRT_SND_QUE_ERR;
    }

    return OPRT_OK;
}

// data command preprocessing
STATIC void smt_frm_cmd_prep(IN CONST SMART_CMD_E cmd,IN CONST BYTE *data,IN CONST UINT len)
{
    cJSON *root = NULL;
    root = cJSON_Parse((CHAR *)data);
    if(NULL == root) {
        PR_ERR("data format err:%s",data);
        goto ERR_RET;
    }

    // data valid verify
    if(NULL == cJSON_GetObjectItem(root,"devId") || \
       NULL == cJSON_GetObjectItem(root,"dps")) {
        goto ERR_RET;
    }

    DEV_CNTL_N_S *dev_cntl = get_dev_cntl(cJSON_GetObjectItem(root,"devId")->valuestring);
    if(NULL == dev_cntl) {
        PR_ERR("not found device");
        goto ERR_RET;
    }

    // find null
    BYTE *dpid = (BYTE *)Malloc(255+1);
    if(NULL == dpid) {
        PR_ERR("malloc error");
        goto ERR_RET;
    }

    BYTE num = 0;
    cJSON *nxt = NULL;
    do {
        cJSON *nxt = cJSON_GetObjectItem(root,"dps")->child;
        for(;nxt != NULL;nxt = nxt->next) {
            if(nxt->type == cJSON_NULL) {
                dpid[num++] = atoi(nxt->string);
                cJSON_DeleteItemFromObject(cJSON_GetObjectItem(root,"dps"),nxt->string);
                break;
            }
        }
    }while(nxt);
    
    OPERATE_RET op_ret;
    if(num) {
        op_ret = sf_obj_dp_qry_dpid(cJSON_GetObjectItem(root,"devId")->valuestring,dpid,num);
        if(OPRT_OK != op_ret) {
            PR_DEBUG("sf_obj_dp_qry_dpid error:%d",op_ret);
            Free(dpid);
            goto ERR_RET;
        }
    }
    Free(dpid);

    nxt = cJSON_GetObjectItem(root,"dps")->child;
    if(nxt) {
        if(dev_cntl->preprocess) {
            // preprocessing
            for(;nxt != NULL;nxt = nxt->next) {
                INT id = atoi(nxt->string);
                INT i;
                DP_CNTL_S *dp_cntl =  NULL;
                for(i = 0; i < dev_cntl->dp_num;i++) {
                    if(dev_cntl->dp[i].dp_desc.dp_id == id) {
                        dp_cntl = &dev_cntl->dp[i];
                        break;
                    }
                }

                if(dp_cntl->dp_desc.passive == PSV_TRUE) {
                    dp_cntl->dp_desc.passive = PSV_F_ONCE;
                }
            }
        }

        if(smt_frm_cntl.cb) {
            os_mutex_get(&smt_frm_cntl.mutex, OS_WAIT_FOREVER);
            smt_frm_cntl.cb(cmd,cJSON_GetObjectItem(root,"dps"));
            os_mutex_put(&smt_frm_cntl.mutex);
        }
    }
    
    cJSON_Delete(root);
    return;
    
ERR_RET:
    cJSON_Delete(root);
    PR_DEBUG("cmd:%d",cmd);
    PR_DEBUG("data:%s",data);
    return;
}


/***********************************************************
*  Function: get_single_wf_dev
*  Input:  
*  Output: 
*  Return: 
*  Note: DEV_CNTL_N_S *
***********************************************************/
DEV_CNTL_N_S *get_single_wf_dev(VOID)
{
    return get_gw_cntl()->dev;
}

/***********************************************************
*  Function: set_fw_ug_stat
*  Input: stat
*  Output: 
*  Return: 
*  Note: none
***********************************************************/
VOID set_fw_ug_stat(IN CONST FW_UG_STAT_E stat)
{
    os_mutex_get(&smt_frm_cntl.mutex, OS_WAIT_FOREVER);
    smt_frm_cntl.stat = stat;
    os_mutex_put(&smt_frm_cntl.mutex);
}

/***********************************************************
*  Function: get_fw_ug_stat
*  Input: none
*  Output: 
*  Return: FW_UG_STAT_E
*  Note: none
***********************************************************/
FW_UG_STAT_E get_fw_ug_stat(VOID)
{
    FW_UG_STAT_E stat;

    os_mutex_get(&smt_frm_cntl.mutex, OS_WAIT_FOREVER);
    stat = smt_frm_cntl.stat;
    os_mutex_put(&smt_frm_cntl.mutex);

    return stat;
}

/***********************************************************
*  Function: sf_fw_ug_msg_infm
*  Input: none
*  Output: 
*  Return: none
*  Note: none
***********************************************************/
OPERATE_RET sf_fw_ug_msg_infm(VOID)
{
    return smart_frame_send_msg(SF_MSG_UG_FM,NULL,0);
}

/***********************************************************
*  Function: sw_ver_change
*  Input: ver->x.x.x
*  Output: 
*  Return: none
*  Note: none
***********************************************************/
STATIC OPERATE_RET sw_ver_change(IN CONST CHAR *ver,OUT UINT *change)
{
    if(NULL == ver || NULL == change) {
        return OPRT_INVALID_PARM;
    }

    INT num = 0;
    BYTE buf[3];
    
    num = sscanf(ver,"%d.%d.%d",&buf[0],&buf[1],&buf[2]);
    if(0 == num) {
        PR_ERR("version format error:%s",ver);
        return OPRT_VER_FMT_ERR;
    }

    UINT tmp = 0;
    tmp = buf[0];
    tmp = (tmp << 8) | buf[1];
    tmp = (tmp << 8) | buf[2];
    *change = tmp;

    return OPRT_OK;
}
