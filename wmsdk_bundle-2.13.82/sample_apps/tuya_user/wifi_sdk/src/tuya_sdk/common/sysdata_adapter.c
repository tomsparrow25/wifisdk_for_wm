/***********************************************************
*  File: sysdata_adapter.c
*  Author: nzy
*  Date: 20150601
***********************************************************/
#define __SYSDATA_ADAPTER_GLOBALS
#include "sysdata_adapter.h"
#include "cJSON.h"
#include "tuya_httpc.h"


/***********************************************************
*************************micro define***********************
***********************************************************/

/***********************************************************
*************************variable define********************
***********************************************************/
STATIC GW_CNTL_S gw_cntl;
STATIC MUTEX_HANDLE gw_mutex;
STATIC TIMER_ID dev_ul_timer;
STATIC TIMER_ID gw_actv_timer;

/***********************************************************
*************************function define********************
***********************************************************/
static void gw_actv_timer_cb(os_timer_arg_t arg);

/***********************************************************
*  Function: set_gw_status
*  Input: 
*  Output: 
*  Return: 
***********************************************************/
VOID set_gw_status(IN CONST GW_STAT_E stat)
{
    os_mutex_get(&gw_mutex, OS_WAIT_FOREVER);
    gw_cntl.stat = stat;
    os_mutex_put(&gw_mutex);
}

/***********************************************************
*  Function: get_gw_status
*  Input: 
*  Output: 
*  Return: GW_STAT_E
***********************************************************/
GW_STAT_E get_gw_status(VOID)
{
    GW_STAT_E stat;
    os_mutex_get(&gw_mutex, OS_WAIT_FOREVER);
    stat = gw_cntl.stat;
    os_mutex_put(&gw_mutex);

    return stat;
}

/***********************************************************
*  Function: set_wf_gw_status
*  Input: 
*  Output: 
*  Return: 
***********************************************************/
VOID set_wf_gw_status(IN CONST GW_WIFI_STAT_E wf_stat)
{
    os_mutex_get(&gw_mutex, OS_WAIT_FOREVER);
    gw_cntl.wf_stat = wf_stat;
    os_mutex_put(&gw_mutex);
}

/***********************************************************
*  Function: get_wf_gw_status
*  Input: 
*  Output: 
*  Return: GW_WIFI_STAT_E
***********************************************************/
GW_WIFI_STAT_E get_wf_gw_status(VOID)
{
    GW_WIFI_STAT_E wf_stat;

    os_mutex_get(&gw_mutex, OS_WAIT_FOREVER);
    wf_stat = gw_cntl.wf_stat;
    os_mutex_put(&gw_mutex);

    return wf_stat;
}


/***********************************************************
*  Function: get_gw_cntl
*  Input: 
*  Output: 
*  Return: GW_CNTL_S
***********************************************************/
GW_CNTL_S *get_gw_cntl(VOID)
{
    return &gw_cntl;
}

/***********************************************************
*  Function: gw_cntl_init
*  Input: 
*  Output: 
*  Return: 
***********************************************************/
OPERATE_RET gw_cntl_init(VOID)
{
    OPERATE_RET op_ret;

    op_ret = httpc_aes_init();
    if(OPRT_OK != op_ret) {
        PR_ERR("op_ret:%d",op_ret);
        return op_ret;        
    }

    op_ret = ws_db_init();
    if(OPRT_OK != op_ret) {
        return op_ret;
    }

    memset(&gw_cntl,0,sizeof(gw_cntl));
    memset(&gw_mutex,0,sizeof(gw_mutex));

    // create mutex
    int ret;
    ret = os_mutex_create(&gw_mutex, "gw_mutex", 1);
    if(ret != WM_SUCCESS) {
        return OPRT_CR_MUTEX_ERR;
    }

    // create gw active timer
    ret = os_timer_create(&dev_ul_timer,
				  "gw_actv_timer",
				  os_msec_to_ticks(500),
				  &gw_actv_timer_cb,
				  NULL,
				  OS_TIMER_PERIODIC,
				  OS_TIMER_NO_ACTIVATE);
	if (ret != WM_SUCCESS) {
		return OPRT_COM_ERROR;
	}

    PROD_IF_REC_S prod_if;    
#if 0
    memset(&prod_if,0,sizeof(prod_if));
    ws_db_get_prod_if(&prod_if);
    if(!prod_if.mac[0] || !prod_if.prod_idx[0]) { // un init
        set_gw_status(UN_INIT);
        return OPRT_OK;
    }
#else
    strcpy(prod_if.mac,"112233445566");
    strcpy(prod_if.prod_idx,"0da02001");
#endif

    op_ret = ws_db_get_gw_actv(&gw_cntl.active);
    if(OPRT_OK != op_ret) {
        memset(&gw_cntl.active,0,sizeof(gw_cntl.active));
        ws_db_set_gw_actv(&gw_cntl.active);
        set_gw_status(UN_ACTIVE);
    }else {
        if(!gw_cntl.active.key[0] || !gw_cntl.active.uid_cnt) { // un active
            if(!gw_cntl.active.token[0]) {
                set_gw_status(UN_ACTIVE);
            }else {
                start_active_gateway();
            }
        }else {
            set_gw_status(STAT_WORK);
        }
    }

    strcpy(gw_cntl.gw.sw_ver,GW_VER);
    strcpy(gw_cntl.gw.name,GW_DEF_NAME);
    snprintf(gw_cntl.gw.id,sizeof(gw_cntl.gw.id),"%s%s",prod_if.prod_idx,prod_if.mac);
    gw_cntl.gw.ability = GW_NO_ABI;

    return OPRT_OK;
}

/***********************************************************
*  Function: gw_lc_bind_device
*  Input: 
*  Output: 
*  Return: 
***********************************************************/
OPERATE_RET gw_lc_bind_device(IN CONST DEV_DESC_IF_S *dev_if,\
                              IN CONST CHAR *sch_json_arr)
{
    if(NULL == dev_if || \
       NULL == sch_json_arr) {
        return OPRT_INVALID_PARM;
    }

    if(get_dev_cntl(dev_if->id)) {
        return OPRT_OK;
    }

    cJSON *root = NULL;
    OPERATE_RET op_ret = OPRT_OK;
    root = cJSON_Parse(sch_json_arr);
    if(NULL == root) {
        return OPRT_CJSON_PARSE_ERR;
    }
    
    INT dp_num;
    DEV_CNTL_N_S *dev_cntl = NULL;
    dp_num = cJSON_GetArraySize(root);
    if(0 == dp_num) {
        op_ret = OPRT_INVALID_PARM;
        goto ERR_EXIT;
    }

    dev_cntl = (DEV_CNTL_N_S *)Malloc(sizeof(DEV_CNTL_N_S)+dp_num*sizeof(DP_CNTL_S));
    if(NULL == dev_cntl) {
        op_ret = OPRT_MALLOC_FAILED;
        goto ERR_EXIT;
    }
    memset(dev_cntl,0,sizeof(DEV_CNTL_N_S)+dp_num*sizeof(DP_CNTL_S));

    memcpy(&dev_cntl->dev_if,dev_if,sizeof(DEV_DESC_IF_S));
    dev_cntl->dp_num = dp_num;

    // dp schema parse
    INT i;
    DP_DESC_IF_S *dp_desc;
    DP_PROP_U *prop;
    cJSON *cjson;
    cJSON *next;
    
    for(i = 0;i < dev_cntl->dp_num;i++) {        
        dp_desc = &(dev_cntl->dp[i].dp_desc);
        prop = &(dev_cntl->dp[i].prop);

        cjson = cJSON_GetArrayItem(root,i);
        if(NULL == cjson) {
            op_ret = OPRT_CJSON_GET_ERR;
            goto ERR_EXIT;
        }

        // id
        next = cJSON_GetObjectItem(cjson,"id");
        if(NULL == next) {
            PR_ERR("get id null");
            op_ret = OPRT_CJSON_GET_ERR;
            goto ERR_EXIT;
        }
        dp_desc->dp_id = atoi(next->valuestring);

        // mode
        next = cJSON_GetObjectItem(cjson,"mode");
        if(NULL == next) {
            PR_ERR("get mode null");
            op_ret = OPRT_CJSON_GET_ERR;
            goto ERR_EXIT;
        }
        if(!strcmp(next->valuestring,"rw")) {
            dp_desc->mode = M_RW;
        }else if(!strcmp(next->valuestring,"ro")) {
            dp_desc->mode = M_RO;
        }else {
            dp_desc->mode = M_WR;
        }

        // trigger
        next = cJSON_GetObjectItem(cjson,"trigger");
        if(NULL == next) {
            dp_desc->trig_t = TRIG_PULSE;
        }else {
            if(!strcmp(next->valuestring,"pulse")) {
                dp_desc->trig_t = TRIG_PULSE;
            }else {
                dp_desc->trig_t = TRIG_DIRECT;
            }
        }

        // type
        next = cJSON_GetObjectItem(cjson,"type");
        if(NULL == next) {
            PR_ERR("get type null");
            op_ret = OPRT_CJSON_GET_ERR;
            goto ERR_EXIT;
        }
        if(!strcmp(next->valuestring,"obj")) {
            dp_desc->type = T_OBJ;
        }else if(!strcmp(next->valuestring,"raw")) {
            dp_desc->type = T_RAW;
            continue;
        }else {
            dp_desc->type = T_FILE;
            continue;
        }

        // property
        next = cJSON_GetObjectItem(cjson,"property");
        if(NULL == next) {
            PR_ERR("get property null");
            op_ret = OPRT_CJSON_GET_ERR;
            goto ERR_EXIT;
        }
        cJSON *child;
        child = cJSON_GetObjectItem(next,"type");
        if(NULL == next) {
            PR_ERR("get type null");
            op_ret = OPRT_CJSON_GET_ERR;
            goto ERR_EXIT;
        }
        if(!strcmp(child->valuestring,"bool")) {
            dp_desc->prop_tp = PROP_BOOL;
        }else if(!strcmp(child->valuestring,"value")) {
            dp_desc->prop_tp = PROP_VALUE;

            CHAR *str[] = {"max","min","step","scale"};
            INT i;
            for(i = 0; i < CNTSOF(str);i++) {
                child = cJSON_GetObjectItem(next,str[i]);
                if(NULL == child && (i != CNTSOF(str)-1)) {
                    PR_ERR("get property null");
                    op_ret = OPRT_CJSON_GET_ERR;
                    goto ERR_EXIT;
                }else if(NULL == child && (i == CNTSOF(str)-1)) {
                    prop->prop_value.scale = 0;
                }else {
                    switch(i) {
                        case 0: prop->prop_value.max = child->valueint; break;
                        case 1: prop->prop_value.min = child->valueint; break;
                        case 2: prop->prop_value.step = child->valueint; break;
                        case 3: prop->prop_value.scale = child->valueint; break;
                    }
                }
            }
        }else if(!strcmp(child->valuestring,"string")) {
            dp_desc->prop_tp = PROP_STR;
            child = cJSON_GetObjectItem(next,"maxlen");
            if(NULL == child) {
                PR_ERR("get maxlen null");
                op_ret = OPRT_CJSON_GET_ERR;
                goto ERR_EXIT;
            }
            prop->prop_str.max_len = child->valueint;
            prop->prop_str.value = Malloc(prop->prop_str.max_len+1);
            if(NULL == prop->prop_str.value) {
                PR_ERR("malloc error");
                op_ret = OPRT_MALLOC_FAILED;
                goto ERR_EXIT;
            }
        }else {
            dp_desc->prop_tp = PROP_ENUM;
            child = cJSON_GetObjectItem(next,"range");
            if(NULL == child) {
                PR_ERR("get range null");
                op_ret = OPRT_CJSON_GET_ERR;
                goto ERR_EXIT;
            }
            
            INT i,num;
            num = cJSON_GetArraySize(child);
            if(num == 0) {
                PR_ERR("get array size error");
                op_ret = OPRT_CJSON_GET_ERR;
                goto ERR_EXIT;
            }
            prop->prop_enum.pp_enum = Malloc(num*sizeof(CHAR *));
            if(NULL == prop->prop_enum.pp_enum) {
                PR_ERR("malloc error");
                op_ret = OPRT_MALLOC_FAILED;
                goto ERR_EXIT;
            }

            prop->prop_enum.cnt = num;
            for(i = 0;i < num;i++) {
                cJSON *c_child = cJSON_GetArrayItem(child,i);
                if(NULL == c_child) {
                    PR_ERR("get array null");
                    op_ret = OPRT_CJSON_GET_ERR;
                    goto ERR_EXIT;
                }

                prop->prop_enum.pp_enum[i] = cJSON_strdup(c_child->valuestring);
                if(NULL == prop->prop_enum.pp_enum[i]) {
                    PR_ERR("malloc error");
                    op_ret = OPRT_MALLOC_FAILED;
                    goto ERR_EXIT;
                }
            }
        }
    }

    os_mutex_get(&gw_mutex, OS_WAIT_FOREVER);
    if(NULL == gw_cntl.dev) {
        gw_cntl.dev = dev_cntl;
    }else {
        DEV_CNTL_N_S *tmp_dev_cntl = gw_cntl.dev;
        while(tmp_dev_cntl->next) {
            tmp_dev_cntl = tmp_dev_cntl->next;
        }
        tmp_dev_cntl->next = dev_cntl;
    }
    gw_cntl.dev_num++;
    os_mutex_put(&gw_mutex);

    if(root) {
        cJSON_Delete(root);
    }

    return OPRT_OK;
        
ERR_EXIT:
    if(dev_cntl) {
        Free(dev_cntl);
    }

    if(root) {
        cJSON_Delete(root);
    }

    return op_ret;
}

OPERATE_RET gw_lc_unbind_device(IN CONST CHAR *id)
{
    if(NULL == id) {
        return OPRT_INVALID_PARM;
    }


    return OPRT_OK;
}

static void gw_actv_timer_cb(os_timer_arg_t arg)
{
    PR_DEBUG("now,we'll go to active gateway");
}

static void dev_ul_timer_cb(os_timer_arg_t arg)
{
    GW_WIFI_STAT_E wf_stat;
    wf_stat = get_wf_gw_status();

    if(wf_stat != STAT_STA_CONN) {
        PR_DEBUG("we can not update info,because the wifi state is :%d",wf_stat);
        return;
    }
    else {
        PR_DEBUG("now,we'll go to update device info");
    }

    // TODO 1 http device bind 
    //      2 device info update
    GW_CNTL_S *gw_cntl = get_gw_cntl();
    DEV_CNTL_N_S *dev_cntl = NULL;
    BOOL close_timer = TRUE;
    OPERATE_RET op_ret = OPRT_OK;

    for(dev_cntl = gw_cntl->dev;dev_cntl;dev_cntl = dev_cntl->next) {        
        // device bind
        if(FALSE == dev_cntl->dev_if.bind) {
            PR_DEBUG("bind");
            op_ret = httpc_dev_bind(&dev_cntl->dev_if);
            if(OPRT_OK != op_ret) {
                close_timer = FALSE;
                continue;
            }

            os_mutex_get(&gw_mutex, OS_WAIT_FOREVER);
            dev_cntl->dev_if.bind = TRUE;
            dev_cntl->dev_if.sync = FALSE;
            os_mutex_put(&gw_mutex);

            op_ret = ws_db_set_dev_if(&dev_cntl->dev_if);
            if(OPRT_OK != op_ret) {
                PR_ERR("ws_db_set_dev_if error,op_ret:%d",op_ret);
            }
        }

        // device info update
        if(TRUE == dev_cntl->dev_if.sync && \
           TRUE == dev_cntl->dev_if.bind) {
            PR_DEBUG("update");
            op_ret = httpc_dev_update(&dev_cntl->dev_if);
            if(OPRT_OK != op_ret) {
                close_timer = FALSE;
                continue;
            }

            os_mutex_get(&gw_mutex, OS_WAIT_FOREVER);
            dev_cntl->dev_if.sync = FALSE;
            os_mutex_put(&gw_mutex);

            op_ret = ws_db_set_dev_if(&dev_cntl->dev_if);
            if(OPRT_OK != op_ret) {
                PR_ERR("ws_db_set_dev_if error,op_ret:%d",op_ret);
            }
        }
    }

    if(TRUE == close_timer) {
        PR_DEBUG("close timer");
        os_timer_deactivate(&dev_ul_timer);
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

    OPERATE_RET op_ret;
    op_ret = gw_cntl_init();
    if(OPRT_OK != op_ret) {
        return op_ret;
    }

    if(UN_INIT == get_gw_status()) {
        return OPRT_OK;
    }

    // create dev upload timer
    int ret = os_timer_create(&dev_ul_timer,
				  "dev_ul_timer",
				  os_msec_to_ticks(600),
				  &dev_ul_timer_cb,
				  NULL,
				  OS_TIMER_PERIODIC,
				  OS_TIMER_NO_ACTIVATE);
	if (ret != WM_SUCCESS) {
		return OPRT_COM_ERROR;
	}

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
        strcpy(dev_if.id,gw_cntl.gw.id);
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

/***********************************************************
*  Function: get_dev_cntl
*  Input: 
*  Output: 
*  Return: 
*  Note: 
***********************************************************/
DEV_CNTL_N_S *get_dev_cntl(IN CONST CHAR *id)
{
    if(NULL == id) {
        return NULL;
    }

    DEV_CNTL_N_S *list = gw_cntl.dev;

    while(list) {
        if((strlen(list->dev_if.id) == strlen(id)) && 
           !strcasecmp(list->dev_if.id,id)) {
            break;
        }

        list = list->next;
    }

    return list;
}

/***********************************************************
*  Function: check_dev_need_update
*  Input: 
*  Output: 
*  Return: 
*  Note: user need to fill the content in the function before 
         call it
***********************************************************/
VOID check_dev_need_update(IN CONST DEV_CNTL_N_S *dev_cntl)
{
    if(NULL == dev_cntl) {
        PR_ERR("invalid param");
        return;
    }

    if((get_gw_status() > UN_ACTIVE) && \
        ((dev_cntl->dev_if.sync && dev_cntl->dev_if.bind) || \
        (!dev_cntl->dev_if.bind))) { // 启动信息更新操作
        
        if(os_timer_is_active(&dev_ul_timer) != WM_SUCCESS) {
            int ret = os_timer_activate(&dev_ul_timer);
            if(WM_SUCCESS != ret) {
                PR_ERR("activeate timer error");
            }
        }
    }
}

/***********************************************************
*  Function: check_and_update_dev_desc_if
*  Input: 
*  Output: 
*  Return: 
***********************************************************/
VOID check_and_update_dev_desc_if(IN CONST CHAR *id,\
                                  IN CONST CHAR *name,\
                                  IN CONST CHAR *sw_ver,\
                                  IN CONST CHAR *schema_id,\
                                  IN CONST CHAR *ui_id)
{
    if(id == NULL) {
        return;
    }

    DEV_CNTL_N_S *dev_cntl = get_dev_cntl(id);
    if(NULL == dev_cntl) {
        PR_ERR("do not get dev cntl");
        return;
    }

    CONST CHAR *str[] = {name,sw_ver,schema_id,ui_id};
    CHAR *oldstr[] = {dev_cntl->dev_if.name,dev_cntl->dev_if.sw_ver,\
                      dev_cntl->dev_if.schema_id,dev_cntl->dev_if.ui_id};
    INT i;

    os_mutex_get(&gw_mutex, OS_WAIT_FOREVER);
    for(i = 0;i < CNTSOF(str);i++) {
        if(NULL == str[i]) {
            continue;
        }
        
        if(strcmp(str[i],oldstr[i]) || \
           (strlen(str[i]) != strlen(oldstr[i]))) {
            dev_cntl->dev_if.sync = TRUE;
            strcpy(oldstr[i],str[i]);
        }
    }
    os_mutex_put(&gw_mutex);

    check_dev_need_update(dev_cntl);
}

/***********************************************************
*  Function: check_all_dev_if_update
*  Input: 
*  Output: 
*  Return: 
***********************************************************/
VOID check_all_dev_if_update(VOID)
{
    DEV_CNTL_N_S *dev_cntl = NULL;
    GW_CNTL_S *gw_cntl = get_gw_cntl();
    
    for(dev_cntl = gw_cntl->dev;dev_cntl;dev_cntl = dev_cntl->next) {
        check_dev_need_update(dev_cntl);
    }
}

#if 0
/***********************************************************
*  Function: update_dev_ol_status
*  Input: 
*  Output: 
*  Return: 
***********************************************************/
VOID update_dev_ol_status(IN CONST CHAR *id,IN CONST BOOL online)
{
    if(id == NULL) {
        return;
    }

    DEV_CNTL_N_S *dev_cntl = get_dev_cntl(id);
    if(NULL == dev_cntl) {
        PR_ERR("do not get dev cntl");
        return;
    }

    if((get_gw_status() <= UN_ACTIVE) || \
        FALSE == dev_cntl->dev_if.bind) {
        return;
    }

    os_mutex_get(&gw_mutex, OS_WAIT_FOREVER);
    dev_cntl->online = online;
    os_mutex_put(&gw_mutex);

    check_dev_need_update(dev_cntl);
}
#endif

/***********************************************************
*  Function: start_active_gateway
*  Input: 
*  Output: 
*  Return: 
***********************************************************/
VOID start_active_gateway(VOID)
{
    set_gw_status(ACTIVE_RD);

    if(os_timer_is_active(&gw_actv_timer) != WM_SUCCESS) {
        os_timer_activate(&gw_actv_timer);
    }
}



