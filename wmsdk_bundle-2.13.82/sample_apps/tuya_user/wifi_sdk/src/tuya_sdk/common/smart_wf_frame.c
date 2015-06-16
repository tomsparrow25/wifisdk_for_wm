/***********************************************************
*  File: smart_wf_frame.c
*  Author: nzy
*  Date: 20150611
***********************************************************/
#define __SMART_WF_FRAME_GLOBALS
#include "smart_wf_frame.h"
#include "mqtt_client.h"
#include "sys_adapter.h"
#include "sysdata_adapter.h"
#include "base64.h"
#include "error_code.h"
#include <wm_os.h>

/***********************************************************
*************************micro define***********************
***********************************************************/
typedef struct {
    os_queue_t msg_que;
    MUTEX_HANDLE mutex;
    SMART_FRAME_CB cb;
    THREAD thread;
    INT is_start;
}SMT_FRM_CNTL_S;

#define SMT_FRM_MAX_EVENTS 10

// lan protocol
#define FRM_TP_CFG_WF 1
#define FRM_TP_ACTV 2
#define FRM_TP_BIND_DEV 3
#define FRM_TP_UNBIND_DEV 4
#define FRM_TP_CMD 5
#define FRM_TP_STAT_REPORT 6
#define FRM_TP_HB 7
#define FRM_QUERY_STAT 8

// mqtt protocol
#define PRO_CMD 5
#define PRO_ADD_USER 6
#define PRO_DEL_USER 7
#define PRO_FW_UG_CFM 10

/***********************************************************
*************************variable define********************
***********************************************************/
SMT_FRM_CNTL_S smt_frm_cntl;
static os_queue_pool_define(smt_frm_queue_data,SMT_FRM_MAX_EVENTS * sizeof(MESSAGE));
static os_thread_stack_define(sf_stack, 512);

/***********************************************************
*************************function define********************
***********************************************************/
static void sf_ctrl_task(os_thread_arg_t arg);
STATIC VOID mq_callback(BYTE *data,UINT len);
STATIC VOID sf_mlcfa_proc(IN CONST SF_MLCFA_FR_S *fr);
extern int lan_set_net_work(char *ssid,char *passphrase);

INT lan_msg_tcp_send(INT iSockfd, UINT frame_num, UINT frame_type, \
                             UINT ret_code,UINT len, CHAR *data)
{
    return 0;
}

VOID gw_lan_respond(IN CONST INT fd,IN CONST UINT fr_num,IN CONST UINT fr_tp,\
                    IN CONST UINT ret_code,IN CONST CHAR *data,IN CONST UINT len)
{
    INT ret; 

    ret = lan_msg_tcp_send(fd, fr_num, fr_tp,ret_code,len, (CHAR *)data);
    if(ret != 0) {
        PR_ERR("lan_msg_tcp_send error:%d",ret);
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
    mq_client_init(gw_cntl->gw.id,"8888888","123456",0);

    op_ret = mq_client_start(gw_cntl->gw.id,mq_callback);
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
                PR_DEBUG("len:%s",msg->len);
                PR_DEBUG("data:%s",msg->data);
            }
            break;

            case SF_MSG_LAN_AP_WF_CFG_RESP: {
                
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
    }

    op_ret = gw_lc_bind_device(&dev_if,dp_schemas);
    if(OPRT_OK != op_ret) {
        return op_ret;
    }

    check_and_update_dev_desc_if(dev_if.id,NULL,def_dev_if->sw_ver,\
                                 def_dev_if->schema_id,def_dev_if->ui_id);

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
    if(PRO_CMD == mq_pro) {
        BYTE *buf = (BYTE *)cJSON_PrintUnformatted(json);
        if(smt_frm_cntl.cb) {
            os_mutex_get(&smt_frm_cntl.mutex, OS_WAIT_FOREVER);
            smt_frm_cntl.cb(MQ_CMD,buf,strlen((CHAR *)buf));
            os_mutex_put(&smt_frm_cntl.mutex);
        }
        Free(buf);
    }else if(PRO_ADD_USER == mq_pro) {
        
    }else if(PRO_DEL_USER == mq_pro) {
        
    }else if(PRO_FW_UG_CFM == mq_pro) {
        
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


#define FRM_TP_CFG_WF 1
#define FRM_TP_ACTV 2
#define FRM_TP_BIND_DEV 3
#define FRM_TP_UNBIND_DEV 4
#define FRM_TP_CMD 5
#define FRM_TP_STAT_REPORT 6
#define FRM_TP_HB 7
#define FRM_QUERY_STAT 8

STATIC VOID sf_mlcfa_proc(IN CONST SF_MLCFA_FR_S *fr)
{
    if(NULL == fr) {
        return;
    }

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

            int ret = 0;
            ret = lan_set_net_work(cJSON_GetObjectItem(root,"ssid")->valuestring,\
                                   cJSON_GetObjectItem(root,"passwd")->valuestring);
            if(WM_SUCCESS != ret) {
                goto FRM_TP_CFG_ERR;
            }

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
                ws_db_set_gw_actv(&gw_cntl->active);
                
                start_active_gateway();
                cJSON_Delete(root);
                
                gw_lan_respond(fr->socket, fr->frame_num, fr->frame_type,\
                               0,NULL,0);
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
            root = cJSON_Parse(fr->data);
            if(NULL == root) {
                gw_lan_respond(fr->socket, fr->frame_num, fr->frame_type,\
                               1,"data format error",strlen("data format error"));
                goto FRM_TP_CMD_ERR;
            }

            if(NULL == cJSON_GetObjectItem(root,"devid") || \
               NULL == cJSON_GetObjectItem(root,"data") || \
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
            if(smt_frm_cntl.cb) {
                os_mutex_get(&smt_frm_cntl.mutex, OS_WAIT_FOREVER);
                smt_frm_cntl.cb(LAN_CMD,(BYTE *)out,strlen(out));
                os_mutex_put(&smt_frm_cntl.mutex);
            }

            gw_lan_respond(fr->socket, fr->frame_num, fr->frame_type,\
                           0,NULL,0);
            Free(out);
            cJSON_Delete(root);
            break;
            
            FRM_TP_CMD_ERR:
            cJSON_Delete(root);
            break;
        }
        break;
        
        case FRM_QUERY_STAT: {
            
        }
        break;

        default: {
            PR_ERR("unsupport frame type:%d",fr->frame_type);
        }
        break;
    }
}

/***********************************************************
*  Function: sf_mk_dp_rept_data
*  Input: data->the format is :{"1":1,"2":true} or {"1":"base64"}
*  Output: 
*  Return: 
*  Note:
***********************************************************/
STATIC OPERATE_RET __sf_mk_dp_rept_data(IN CONST CHAR *id,IN CONST CHAR *data,OUT CHAR **pp_out)
{
    if(id == NULL || data == NULL || NULL == pp_out) {
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
    cJSON_AddStringToObject(root,"devId",id);
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
*  Function: __sf_dev_lc_obj_dp_up 更新本地device obj dp的数据值
*  Input: data->the format is :{"1":1,"2":true}
*  Output: 
*  Return: 
*  Note:
***********************************************************/
STATIC OPERATE_RET __sf_dev_lc_obj_dp_up(IN CONST CHAR *id,IN CONST CHAR *data)
{
    if(id == NULL || data == NULL) {
        return OPRT_INVALID_PARM;
    }

    DEV_CNTL_N_S *dev_cntl = get_dev_cntl(id);
    if(NULL == dev_cntl) {
        return OPRT_NOT_FOUND_DEV;
    }

    OPERATE_RET op_ret;
    cJSON *data_json = cJSON_Parse(data);
    if(NULL == data_json) {
        op_ret = OPRT_CJSON_PARSE_ERR;
        goto ERR_EXIT;
    }

    cJSON *nxt = data_json->next;
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
                if(nxt->type != cJSON_False || \
                   nxt->type != cJSON_True) {
                    op_ret = OPRT_DP_TYPE_PROP_ILLEGAL;
                    goto ERR_EXIT;
                }
                dp_cntl->prop.prop_bool.value = nxt->type;
            }
            break;

            case PROP_VALUE: {
                if(nxt->type != cJSON_Number || \
                   nxt->valueint > dp_cntl->prop.prop_value.max || \
                   nxt->valueint < dp_cntl->prop.prop_value.min) {
                    op_ret = OPRT_DP_TYPE_PROP_ILLEGAL;
                    goto ERR_EXIT;
                }
                dp_cntl->prop.prop_value.value = nxt->valueint;
            }
            break;

            case PROP_STR: {
                if(nxt->type != cJSON_String || \
                   strlen(nxt->valuestring) > dp_cntl->prop.prop_str.max_len) {
                    op_ret = OPRT_DP_TYPE_PROP_ILLEGAL;
                    goto ERR_EXIT;
                }

                strcpy(dp_cntl->prop.prop_str.value,nxt->valuestring);
            }
            break;

            case PROP_ENUM: {
                if(nxt->type != cJSON_Array) {
                    op_ret = OPRT_DP_TYPE_PROP_ILLEGAL;
                    goto ERR_EXIT;
                }

                INT j = 0;
                for(j = 0; j < dp_cntl->prop.prop_enum.cnt;j++) {
                    if(!strcmp(dp_cntl->prop.prop_enum.pp_enum[i],nxt->valuestring)) {
                        dp_cntl->prop.prop_enum.value = j;
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

        dp_cntl->pv_stat = VALID_LC;
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
*  Input: data->the format is :{"1":1,"2":true}
*  Output: 
*  Return: 
*  Note:
***********************************************************/
STATIC OPERATE_RET __sf_dev_obj_dp_set_vc(IN CONST CHAR *id,IN CONST CHAR *data)
{
    DEV_CNTL_N_S *dev_cntl = get_dev_cntl(id);
    
    cJSON *data_json = cJSON_Parse(data);
    cJSON *nxt = data_json->next;
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

        dp_cntl->pv_stat = VALID_LC;
        nxt = nxt->next;
    }

    cJSON_Delete(data_json);
    return OPRT_OK;
}

/***********************************************************
*  Function: sf_obj_dp_report ,obj dp report for user
*  Input: data->the format is :{"1":1,"2":true}
*  Output: 
*  Return: 
*  Note:
***********************************************************/
OPERATE_RET sf_obj_dp_report(IN CONST CHAR *id,IN CONST CHAR *data) 
{
    if(id == NULL || data == NULL) {
        return OPRT_INVALID_PARM;
    }

    CHAR *out = NULL;
    OPERATE_RET op_ret;
    op_ret = __sf_mk_dp_rept_data(id,data,&out);
    if(op_ret != OPRT_OK) {
        return op_ret;
    }

    // lan dp report
    op_ret = smart_frame_send_msg(SF_MSG_LAN_STAT_REPORT,out,strlen(out));
    if(op_ret != OPRT_OK) {
        Free(out);
        return op_ret;
    }

    op_ret = __sf_dev_lc_obj_dp_up(id,data);
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
    __sf_dev_obj_dp_set_vc(id,data);
    Free(out);
    return OPRT_OK;
}

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
    if(id == NULL || data == NULL) {
        return OPRT_INVALID_PARM;
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
    op_ret = __sf_mk_dp_rept_data(id,out,&tmp_out);
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



















