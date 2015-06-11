/***********************************************************
*  File: tuya_ws_db.c
*  Author: nzy
*  Date: 20150601
***********************************************************/
#define __TUYA_WS_DB_GLOBALS
#include "tuya_ws_db.h"
#include <psm.h>
#include <partition.h>
#include "cJSON.h"


/***********************************************************
*************************micro define***********************
***********************************************************/
#define TUYA_WS_PARTITION "ty_ws_part" // partition
#define TUYA_WS_MODULE "ty_ws_mod" // module

#define PROD_IF_REC_KEY "prod_if_rec_key"
#define GW_ACTIVE_KEY "gw_active_key"
#define DEV_IF_REC_KEY "dev_if_rec_key"

/***********************************************************
*************************variable define********************
***********************************************************/

/***********************************************************
*************************function define********************
***********************************************************/
/***********************************************************
*  Function: ws_db_init
*  Input: 
*  Output: 
*  Return: OPERATE_RET
***********************************************************/
OPERATE_RET ws_db_init(VOID) 
{
	int ret;
	struct partition_entry *p;
	flash_desc_t fl;

	ret = part_init();
	if (ret != WM_SUCCESS) {
		return OPRT_TY_WS_PART_ERR;
	}
	p = part_get_layout_by_id(FC_COMP_PSM, NULL);
	if (!p) {
		return OPRT_COM_ERROR;
	}

	part_to_flash_desc(p, &fl);
	ret = psm_init(&fl);
	if (ret) {
		return OPRT_COM_ERROR;
	}

	ret = psm_register_module(TUYA_WS_MODULE,
					TUYA_WS_PARTITION, PSM_CREAT);
	if (ret != WM_SUCCESS && ret != -WM_E_EXIST) {
		return OPRT_COM_ERROR;
	}

    return OPRT_OK;
}

/***********************************************************
*  Function: ws_db_set_prod_if
*  Input: 
*  Output: 
*  Return: OPERATE_RET
***********************************************************/
OPERATE_RET ws_db_set_prod_if(IN CONST PROD_IF_REC_S *prod_if)
{
    if(NULL == prod_if) {
        return OPRT_INVALID_PARM;
    }

    cJSON *root;	
    root=cJSON_CreateObject();
    if(NULL == root) {
        return OPRT_CR_CJSON_ERR;
    }

    CONST CHAR *pp_tmpstr[] = {prod_if->mac,prod_if->prod_idx};
    CHAR *pp_str[] = {"mac","prod_idx"};
    INT i;
    
    for(i = 0;i < CNTSOF(pp_str);i++) {
        if(0 == pp_tmpstr[i][0]) {
            cJSON_AddNullToObject(root,pp_str[i]);
        }else {
            cJSON_AddStringToObject(root,pp_str[i],pp_tmpstr[i]);
        }
    }

    CHAR *out;
    out=cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    PR_DEBUG_RAW("%s\r\n",out);

    int ret;
    ret = psm_set_single(TUYA_WS_MODULE, PROD_IF_REC_KEY, out);
    if(WM_SUCCESS != ret) {
        Free(out);
        return OPRT_PSM_SET_ERROR;
    }

    Free(out);
    return OPRT_OK;
}

/***********************************************************
*  Function: ws_db_get_prod_if
*  Input: 
*  Output: 
*  Return: OPERATE_RET
***********************************************************/
OPERATE_RET ws_db_get_prod_if(OUT PROD_IF_REC_S *prod_if)
{
    if(NULL == prod_if) {
        return OPRT_INVALID_PARM;
    }

    CHAR *buf = (CHAR *)Malloc(512);
    if(NULL == buf) {
        return OPRT_MALLOC_FAILED;
    }

    int ret;
    ret = psm_get_single(TUYA_WS_MODULE, PROD_IF_REC_KEY,buf,512);
    if(WM_SUCCESS != ret) {
        return OPRT_PSM_GET_ERROR;
    }

    PR_DEBUG_RAW("%s\r\n",buf);

    cJSON *root = NULL;
    root = cJSON_Parse(buf);
    if(NULL == root) {
        return OPRT_CJSON_PARSE_ERR;
    }
    Free(buf);

    CHAR *pp_tmpstr[] = {prod_if->mac,prod_if->prod_idx};
    CHAR *pp_str[] = {"mac","prod_idx"};
    cJSON *json;
    INT i;

    for(i = 0; i < CNTSOF(pp_str);i++) {
        json = cJSON_GetObjectItem(root,pp_str[i]);
        if(NULL == json) {
            cJSON_Delete(root);
            return OPRT_CJSON_GET_ERR;
        }

        if(json->type == cJSON_NULL) {
            pp_tmpstr[i][0] = 0;
        }else {
            strcpy(pp_tmpstr[i],json->valuestring);
        }
    }

    cJSON_Delete(root);
    return OPRT_OK;
}


/***********************************************************
*  Function: ws_db_set_gw_actv
*  Input: 
*  Output: 
*  Return: OPERATE_RET
***********************************************************/
OPERATE_RET ws_db_set_gw_actv(IN CONST GW_ACTV_IF_S *gw_actv)
{
    if(NULL == gw_actv) {
        return OPRT_INVALID_PARM;
    }

    cJSON *root = NULL;    
    OPERATE_RET op_ret = OPRT_OK;
    
    root=cJSON_CreateObject();
    if(NULL == root) {
        return OPRT_CR_CJSON_ERR;
    }

    if(!gw_actv->token[0]) {
        cJSON_AddNullToObject(root,"token");
    }else {
        cJSON_AddStringToObject(root,"token",gw_actv->token);
    }

    if(!gw_actv->key[0]) {
        cJSON_AddNullToObject(root,"key");
    }else {
        cJSON_AddStringToObject(root,"key",gw_actv->key);
    }

    if(0 == gw_actv->uid_cnt) {
        cJSON_AddNullToObject(root,"uid_acl");
    }else {
        cJSON *array = NULL;
        array = cJSON_CreateArray();
        if(NULL == array) {
            op_ret = OPRT_CR_CJSON_ERR;
            goto ERR_EXIT;
        }

        INT i;
        for(i = 0;i < gw_actv->uid_cnt;i++) {
            cJSON_AddItemToArray(array, cJSON_CreateString(gw_actv->uid_acl[i]));
        }
        cJSON_AddItemToObject(root,"uid_acl",array);
    }

    CHAR *out;
    out=cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    PR_DEBUG_RAW("%s\r\n",out);

    int ret;
    ret = psm_set_single(TUYA_WS_MODULE, GW_ACTIVE_KEY, out);
    if(WM_SUCCESS != ret) {
        Free(out);
        op_ret = OPRT_PSM_SET_ERROR;
        goto ERR_EXIT;
    }

    Free(out);
    return OPRT_OK;

ERR_EXIT:
    if(root) {
        cJSON_Delete(root);
    }

    return op_ret;
}

/***********************************************************
*  Function: ws_db_get_gw_actv
*  Input: 
*  Output: 
*  Return: OPERATE_RET
***********************************************************/
OPERATE_RET ws_db_get_gw_actv(OUT GW_ACTV_IF_S *gw_actv)
{
    if(NULL == gw_actv) {
        return OPRT_INVALID_PARM;
    }

    CHAR *buf = (CHAR *)Malloc(512);
    if(NULL == buf) {
        return OPRT_MALLOC_FAILED;
    }

    int ret;
    ret = psm_get_single(TUYA_WS_MODULE, GW_ACTIVE_KEY,buf,512);
    if(WM_SUCCESS != ret) {
        return OPRT_PSM_GET_ERROR;
    }

    PR_DEBUG_RAW("%s\r\n",buf);

    cJSON *root = NULL;
    root = cJSON_Parse(buf);
    if(NULL == root) {
        return OPRT_CJSON_PARSE_ERR;
    }
    Free(buf);

    CHAR *pp_str[] = {"token","key","uid_acl"};
    cJSON *json;
    INT i;

    for(i = 0; i < CNTSOF(pp_str);i++) {
        json = cJSON_GetObjectItem(root,pp_str[i]);
        if(NULL == json) {
            cJSON_Delete(root);
            return OPRT_CJSON_GET_ERR;
        }

        if(i == 0) {
            if(json->type != cJSON_NULL) {
                strcpy(gw_actv->token,json->valuestring);
            }else {
                gw_actv->token[0] = 0;
            }
        }
        else if(i == 1) {
            if(json->type != cJSON_NULL) {
                strcpy(gw_actv->key,json->valuestring);
            }else {
                gw_actv->key[0] = 0;
            }
        }else {
            if(json->type == cJSON_NULL) {
                gw_actv->uid_cnt = 0;
                break;
            }

            cJSON *tmp_json;
            gw_actv->uid_cnt = cJSON_GetArraySize(json);
            INT j;
            for(j = 0;j < gw_actv->uid_cnt;j++) {
                tmp_json = cJSON_GetArrayItem(json,j);
                if(NULL == tmp_json) {
                    cJSON_Delete(root);
                    return OPRT_CJSON_GET_ERR;
                }
                strcpy(gw_actv->uid_acl[j],tmp_json->valuestring);
            }
        }
    }

    cJSON_Delete(root);
    return OPRT_OK;
}

/***********************************************************
*  Function: ws_db_set_dev_if
*  Input: 
*  Output: 
*  Return: OPERATE_RET
***********************************************************/
OPERATE_RET ws_db_set_dev_if(IN CONST DEV_DESC_IF_S *dev_if)
{
    if(NULL == dev_if) {
        return OPRT_INVALID_PARM;
    }

    cJSON *root = NULL;
    OPERATE_RET op_ret = OPRT_OK;
    
    root=cJSON_CreateObject();
    if(NULL == root) {
        return OPRT_CR_CJSON_ERR;
    }

    CONST CHAR *pp_dec_str[] = {dev_if->id,dev_if->name,dev_if->sw_ver,dev_if->schema_id,dev_if->ui_id};
    CHAR *pp_str[] = {"id","name","sw_ver","schema_id","ui_id","ability","bind","sync"};
    INT i;
    for(i = 0; i < CNTSOF(pp_str);i++) {
        if(i < (CNTSOF(pp_str)-3)) {
            if(0 == pp_dec_str[i][0]) {
                cJSON_AddNullToObject(root,pp_str[i]);
            }else {
                cJSON_AddStringToObject(root,pp_str[i],pp_dec_str[i]);
            }
        }else if(i == (CNTSOF(pp_str)-3)){
            cJSON_AddNumberToObject(root,pp_str[i],dev_if->ability);
        }else if(i == (CNTSOF(pp_str)-2)){
            cJSON_AddBoolToObject(root,pp_str[i],dev_if->bind);
        }else {
            cJSON_AddBoolToObject(root,pp_str[i],dev_if->sync);
        }
    }

    CHAR *out;
    out=cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    PR_DEBUG_RAW("%s\r\n",out);

    INT ret;
    ret = psm_set_single(TUYA_WS_MODULE, DEV_IF_REC_KEY, out);
    if(WM_SUCCESS != ret) {
        Free(out);
        op_ret = OPRT_PSM_SET_ERROR;
        goto ERR_EXIT;
    }

    Free(out);
    return OPRT_OK;

ERR_EXIT:
    if(root) {
        cJSON_Delete(root);
    }

    return op_ret;
}

/***********************************************************
*  Function: ws_db_get_dev_if
*  Input: 
*  Output: 
*  Return: OPERATE_RET
***********************************************************/
OPERATE_RET ws_db_get_dev_if(OUT DEV_DESC_IF_S *dev_if)
{
    if(NULL == dev_if) {
        return OPRT_INVALID_PARM;
    }

    CHAR *buf = (CHAR *)Malloc(512);
    if(NULL == buf) {
        return OPRT_MALLOC_FAILED;
    }

    int ret;
    ret = psm_get_single(TUYA_WS_MODULE, DEV_IF_REC_KEY,buf,512);
    if(WM_SUCCESS != ret) {
        return OPRT_PSM_GET_ERROR;
    }

    PR_DEBUG_RAW("%s\r\n",buf);

    cJSON *root = NULL;
    root = cJSON_Parse(buf);
    if(NULL == root) {
        return OPRT_CJSON_PARSE_ERR;
    }
    Free(buf);

    CHAR *pp_dec_str[] = {dev_if->id,dev_if->name,dev_if->sw_ver,dev_if->schema_id,dev_if->ui_id};
    CHAR *pp_str[] = {"id","name","sw_ver","schema_id","ui_id","ability","bind","sync"};

    cJSON *json;
    INT i;
    for(i = 0; i < CNTSOF(pp_str);i++) {
        json = cJSON_GetObjectItem(root,pp_str[i]);
        if(NULL == json) {
            cJSON_Delete(root);
            return OPRT_CJSON_GET_ERR;
        }

        if(i < (CNTSOF(pp_str)-3)) {
            if(json->type == cJSON_NULL) {
                pp_dec_str[i][0] = 0; 
            }else {
                strcpy(pp_dec_str[i],json->valuestring);
            }
        }else if(i == (CNTSOF(pp_str)-3)) {
            dev_if->ability = json->valueint;
        }else if(i == (CNTSOF(pp_str)-2)) {
            dev_if->bind = json->type;
        }else {
            dev_if->sync = json->type;
        }
    }

    cJSON_Delete(root);
    return OPRT_OK;
}

/***********************************************************
*  Function: ws_db_reset
*  Input: 
*  Output: 
*  Return: OPERATE_RET
*  Note: only reset gw reset info and device bind info
***********************************************************/
VOID ws_db_reset(VOID)
{
    GW_ACTV_IF_S gw_actv;
    
    memset(&gw_actv,0,sizeof(gw_actv));
    ws_db_set_gw_actv(&gw_actv);

    DEV_DESC_IF_S dev_if;
    memset(&dev_if,0,sizeof(dev_if));
    ws_db_set_dev_if(&dev_if);
}


