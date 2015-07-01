/***********************************************************
*  File: tuya_httpc.c
*  Author: nzy
*  Date: 20150527
***********************************************************/
#define __TUYA_HTTPC_GLOBALS
#include "tuya_httpc.h"
#include "md5.h"
#include <mdev_aes.h>
#include <wmtime.h>
#include "cJSON.h"
#include "sysdata_adapter.h"
#include "base64.h"
#include "device.h"

/***********************************************************
*************************micro define***********************
***********************************************************/
typedef struct {
    BYTE total;
    BYTE cnt;
    CHAR *pos[0];
}HTTP_PARAM_H_S;

#define DEF_URL_LEN 1024
typedef struct {
    HTTP_PARAM_H_S *param_h; // param handle
    CHAR *param_in; // param insert pos
    USHORT head_size; // head_size == "url?" or "url"
    USHORT buf_len; // bufer len
    CHAR buf[0]; // "url?key=value" + "kev=value statistics"
}HTTP_URL_H_S;

// aes 
typedef struct {
    MUTEX_HANDLE mutex;
    mdev_t *aes_dev;
    BYTE key[16];
    BYTE iv[16]; // init vector
}AES_H_S;
/***********************************************************
*************************variable define********************
***********************************************************/
STATIC AES_H_S httpc_aes;

/***********************************************************
*************************function define********************
***********************************************************/
STATIC OPERATE_RET __httpc_common_cb(IN CONST BOOL is_end,\
                                     IN CONST UINT offset,\
                                     IN CONST BYTE *data,\
                                     IN CONST UINT len);

static int Add_Pkcs(char *p, int len)
{    
    char pkcs[16];
    int i, cz = 16-len%16;    
    memset(pkcs, 0, sizeof(pkcs));        
    for( i=0; i<cz; i++ ) {        
        pkcs[i]=cz;    
    }    
    memcpy(p + len, pkcs, cz);    
    return (len + cz);
}

void HexToStr(BYTE *pbDest, BYTE *pbSrc, int nLen)
{
    char	ddl,ddh;
    int i;

    for (i=0; i<nLen; i++) {
        ddh = 48 + pbSrc[i] / 16;
        ddl = 48 + pbSrc[i] % 16;
        if (ddh > 57) ddh = ddh + 7;
        if (ddl > 57) ddl = ddl + 7;
        pbDest[i*2] = ddh;
        pbDest[i*2+1] = ddl;
    }

    pbDest[nLen*2] = '\0';
}


/***********************************************************
*  Function: create_http_url_h
*  Input: buf_len: if len == 0 use DEF_URL_LEN
*         param_cnt
*  Output: 
*  Return: HTTP_URL_H_S *
***********************************************************/
STATIC HTTP_URL_H_S *create_http_url_h(IN CONST USHORT buf_len,\
                                       IN CONST USHORT param_cnt)
{
    USHORT len;

    if(0 == buf_len) {
        len = DEF_URL_LEN;    
    }else {
        len = buf_len;
    }

    HTTP_URL_H_S *hu_h = Malloc(sizeof(HTTP_URL_H_S) + len + \
                                sizeof(HTTP_PARAM_H_S) + sizeof(CHAR *)*param_cnt);
    if(NULL == hu_h) {
        return NULL;
    }

    hu_h->head_size = 0;
    hu_h->param_in = hu_h->buf;
    hu_h->buf_len = len;

    HTTP_PARAM_H_S *param_h = (HTTP_PARAM_H_S *)((BYTE *)hu_h + (sizeof(HTTP_URL_H_S) + len));
    param_h->cnt = 0;
    param_h->total = param_cnt;

    hu_h->param_h = param_h;
    
    return hu_h;
}

/***********************************************************
*  Function: del_http_url_h
*  Input: hu_h
*  Output: 
*  Return: HTTP_URL_H_S *
***********************************************************/
STATIC VOID del_http_url_h(IN HTTP_URL_H_S *hu_h)
{
    Free(hu_h);
}

/***********************************************************
*  Function: fill_url_head
*  Input: hu_h:http url handle 
*         url_h:url head
*  Output: hu_h
*  Return: OPERATE_RET
***********************************************************/
STATIC OPERATE_RET fill_url_head(INOUT HTTP_URL_H_S *hu_h,\
                                 IN CONST CHAR *url_h)
{
    if(NULL == hu_h || \
       NULL == url_h) {
        return OPRT_INVALID_PARM;
    }

    strcpy(hu_h->param_in,url_h);
    hu_h->head_size = strlen(url_h);
    hu_h->param_in += hu_h->head_size;

    return OPRT_OK;
}

/***********************************************************
*  Function: fill_url_param
*  Input: hu_h
*         key
*         value
*  Output: hu_h
*  Return: OPERATE_RET
***********************************************************/
STATIC OPERATE_RET fill_url_param(INOUT HTTP_URL_H_S *hu_h,\
                                  IN CONST CHAR *key,\
                                  IN CONST CHAR *value)
{
    if(NULL == hu_h || \
       NULL == key || \
       NULL == value) {
        return OPRT_INVALID_PARM;
    }

    if('?' != hu_h->buf[hu_h->head_size-1]) {
        strcpy(hu_h->param_in,"?");
        hu_h->param_in += strlen("?");
        hu_h->head_size += strlen("?");
    }

    INT ret;
    ret = snprintf(hu_h->param_in,((hu_h->buf+hu_h->buf_len)-hu_h->param_in),\
                   "%s=%s",key,value);
    if(ret != (strlen(hu_h->param_in))) {
        if(ret < 0) {
            return OPRT_COM_ERROR;
        }else {
            return OPRT_BUF_NOT_ENOUGH;
        }
    }

    HTTP_PARAM_H_S *param_h = hu_h->param_h;
    if(param_h->cnt >= param_h->total) {
        return OPRT_URL_PARAM_OUT_LIMIT;
    }

    param_h->pos[param_h->cnt++] = hu_h->param_in;
    hu_h->param_in += ret+1; // "key=value"

    return OPRT_OK;
}

/***********************************************************
*  Function: make_full_url
*  Input: hu_h
*  Output: hu_h
*  Return: OPERATE_RET
***********************************************************/
STATIC OPERATE_RET make_full_url(INOUT HTTP_URL_H_S *hu_h,IN CONST CHAR *key)
{
    if(NULL == hu_h || \
       NULL == hu_h->param_h || \
       (hu_h->param_h && (0 == hu_h->param_h->cnt))) {
        return OPRT_INVALID_PARM;
    }

    HTTP_PARAM_H_S *param_h = hu_h->param_h;
    INT i,j,k;
    CHAR *tmp_str;

    // use key to sort
    for(i = 0;i < param_h->cnt;i++) {
        CHAR *tmp;
        tmp = strstr(param_h->pos[i],"=");
        *tmp = 0;
    }

    // sort use choice method
    for(i = 0; i < (param_h->cnt-1);i++) {
        k = i;
        for(j = i+1;j < param_h->cnt;j++) {
            if(strcmp(param_h->pos[k],param_h->pos[j]) > 0) {
                tmp_str = param_h->pos[k];
                param_h->pos[k] = param_h->pos[j];
                param_h->pos[j] = tmp_str;
            }
        }
    }

    // data restore
    for(i = 0;i < param_h->cnt;i++) {
        *(param_h->pos[i] + strlen(param_h->pos[i])) = '=';
    }

    // make md5 sign 
    CHAR sign[32+1];
    MD5_CTX md5;
    MD5Init(&md5);
    unsigned char decrypt[16];

    for(i = 0;i < param_h->cnt;i++) {
        MD5Update(&md5,(BYTE *)param_h->pos[i],strlen(param_h->pos[i]));
        MD5Update(&md5,(BYTE *)"||",2);
    }
    MD5Update(&md5,(BYTE *)key,strlen(key));
    MD5Final(&md5,decrypt);

    INT offset = 0;
    for(i = 0;i < 16;i++) {
        sprintf(&sign[offset],"%02x",decrypt[i]);
        offset += 2;
    }
    sign[offset] = 0;

    // to sort string
    INT len = hu_h->param_in - (hu_h->buf+hu_h->head_size);
    tmp_str = (CHAR *)Malloc(len+1); // +1 to save 0
    if(tmp_str == NULL) {
        return OPRT_MALLOC_FAILED;
    }

    offset = 0;
    for(i = 0;i < param_h->cnt;i++) {
        strcpy(tmp_str+offset,param_h->pos[i]);
        offset += strlen(param_h->pos[i]);
        tmp_str[offset++] = '&';
    }
    tmp_str[offset] = 0;
 
    // copy to the huh handle
    strcpy((hu_h->buf+hu_h->head_size),tmp_str);

    Free(tmp_str);

    // set sign to url
    OPERATE_RET op_ret;
    op_ret = fill_url_param(hu_h,"sign",sign);
    if(OPRT_OK != op_ret) {
        return op_ret;
    }

    return op_ret;
}

STATIC OPERATE_RET __httpc_com_hanle(IN CONST http_req_t *req,\
                                     IN CONST CHAR *url,\
                                     IN CONST http_hdr_field_sel_t field_flags,\
                                     IN CONST HTTPC_CB callback)
{
    if(NULL == req || \
       NULL == callback || \
       NULL == url) {
        return OPRT_INVALID_PARM;
    }

    http_session_t hnd;
    int ret = http_open_session(&hnd, url, 0, NULL, 0);
    if (ret != 0) {
        PR_ERR("Open session failed: %s (%d)", url, ret);
        return OPRT_HTTP_OS_ERROR;
    }

    ret = http_prepare_req(hnd, req,field_flags);
    if (ret != 0) {
        http_close_session(&hnd);
        PR_ERR("Prepare request failed: %d", ret);
        return OPRT_HTTP_PR_REQ_ERROR;
    }

    ret = http_send_request(hnd, req);
    if (ret != 0) {
        http_close_session(&hnd);
        PR_ERR("Send request failed: %d", ret);
        return OPRT_HTTP_SD_REQ_ERROR;
    }

    http_resp_t *resp;
    ret = http_get_response_hdr(hnd, &resp);
    if (ret != 0) {
        http_close_session(&hnd);
        dbg("Get resp header failed: %d", ret);
        return OPRT_HTTP_GET_RESP_ERROR;
    }

    #undef DEF_BUF_SIZE
    #define DEF_BUF_SIZE 1024
    BYTE *buf;
    buf = (BYTE *)Malloc(DEF_BUF_SIZE);
    if(buf == NULL) {
        http_close_session(&hnd);
        return OPRT_MALLOC_FAILED;
    }
    memset(buf,0,DEF_BUF_SIZE);

    UINT offset = 0;
    OPERATE_RET op_ret = OPRT_OK;
    while (1) {
        ret = http_read_content(hnd, buf, DEF_BUF_SIZE);
        if (ret < 0) {
            Free(buf);
            http_close_session(&hnd);
            return OPRT_HTTP_RD_ERROR;
        }else if(0 == ret){ // ½áÊø
            if(resp->chunked) {
                op_ret = callback(TRUE,offset,NULL,0);
            }
            break;
        }else {
            BOOL is_end = FALSE;
            if(!resp->chunked) {
                if((offset+ret) >= resp->content_length) {
                    is_end = TRUE;
                }
            }
            op_ret = callback(is_end,offset,buf,ret);
            offset += ret;
        }

        if(op_ret != OPRT_OK) {
            break;
        }
    }

    Free(buf);
    http_close_session(&hnd);

    return op_ret;
}

/***********************************************************
*  Function: http_client_get
*  Input: url callback
*  Output: none
*  Return: OPERATE_RET
***********************************************************/
STATIC OPERATE_RET http_client_get(IN CONST CHAR *url,\
                                   IN CONST HTTPC_CB callback)
{
    if(NULL == url || \
       NULL == callback) {
        return OPRT_INVALID_PARM;
    }

    http_req_t req = {
		.type = HTTP_GET,
		.resource = url,
		.version = HTTP_VER_1_1,
	};

    OPERATE_RET op_ret;
    op_ret = __httpc_com_hanle(&req,url,STANDARD_HDR_FLAGS|HDR_ADD_CONN_KEEP_ALIVE,callback);

    return op_ret;
}

/***********************************************************
*  Function: http_client_post
*  Input: url callback content len
*  Output: hu_h
*  Return: OPERATE_RET
***********************************************************/
STATIC OPERATE_RET http_client_post(IN CONST CHAR *url,\
                                    IN CONST http_hdr_field_sel_t field_flags,\
                                    IN CONST HTTPC_CB callback,\
                                    IN CONST BYTE *data,\
                                    IN CONST UINT len)
{
    if(NULL == url || \
       NULL == callback) {
        return OPRT_INVALID_PARM;
    }

    http_req_t req = {
        .type = HTTP_POST,
        .resource = url,
        .version = HTTP_VER_1_1,
        .content = (CHAR *)data,
        .content_len = len,
    };
    
    OPERATE_RET op_ret;
    op_ret = __httpc_com_hanle(&req,url,field_flags,callback);

    return op_ret;
}

/***********************************************************
*  Function: httpc_aes_init
*  Input: 
*  Output: 
*  Return: OPERATE_RET
***********************************************************/
OPERATE_RET httpc_aes_init(VOID)
{
    memset(&httpc_aes,0,sizeof(httpc_aes));

    int ret;
    ret = aes_drv_init();
    if(WM_SUCCESS != ret) {
        return OPRT_HTTP_AES_INIT_ERR;
    }

    httpc_aes.aes_dev = aes_drv_open(MDEV_AES_0);
    if(NULL == httpc_aes.aes_dev) {
        return OPRT_HTTP_AES_OPEN_ERR;
    }

    ret = os_mutex_create(&httpc_aes.mutex, "httpc_aes_mutex", 1);
    if(ret != WM_SUCCESS) {
        return OPRT_CR_MUTEX_ERR;
    }

    return OPRT_OK;
}

/***********************************************************
*  Function: httpc_aes_init
*  Input: 
*  Output: 
*  Return: OPERATE_RET
***********************************************************/
OPERATE_RET httpc_aes_set(IN CONST BYTE *key,IN CONST BYTE *iv)
{
    if(NULL == key) {
        return OPRT_INVALID_PARM;
    }

    memcpy(httpc_aes.key,key,sizeof(httpc_aes.key));
    if(iv) {
        memcpy(httpc_aes.iv,iv,sizeof(httpc_aes.iv));
    }else {
        memset(httpc_aes.iv,0,sizeof(httpc_aes.iv));
    }

    return OPRT_OK;
}

/***********************************************************
*  Function: httpc_aes_encrypt
*  Input: 
*  Output: 
*  Return: OPERATE_RET
***********************************************************/
STATIC OPERATE_RET httpc_aes_encrypt(IN CONST BYTE *plain,UINT len,OUT BYTE *cipher)
{
    if(NULL == plain || \
       NULL == cipher || \
       0 == len) {
        return OPRT_INVALID_PARM;
    }

    INT ret;
    aes_t enc_aes;

    os_mutex_get(&httpc_aes.mutex, OS_WAIT_FOREVER);
    
    ret = aes_drv_setkey(httpc_aes.aes_dev,&enc_aes,\
                         httpc_aes.key, sizeof(httpc_aes.key),
                         httpc_aes.iv, AES_ENCRYPTION, HW_AES_MODE_ECB);
    if(WM_SUCCESS != ret) {
        os_mutex_put(&httpc_aes.mutex);
        return OPRT_HTTP_AES_SET_KEY_ERR;
    }

    ret = aes_drv_encrypt(httpc_aes.aes_dev, &enc_aes, \
                          plain, cipher, len);
    if(WM_SUCCESS != ret) {
        os_mutex_put(&httpc_aes.mutex);
        return OPRT_HTTP_AES_ENCRYPT_ERR;
    }
    
    os_mutex_put(&httpc_aes.mutex);

    return OPRT_OK;
}

STATIC OPERATE_RET httpc_data_aes_proc(IN CONST CHAR *in,IN CONST UINT len,\
                                       IN CONST CHAR *token,OUT CHAR **pp_out)
{
    if(NULL == in || \
       0 == len || \
       NULL == pp_out) {
        return OPRT_INVALID_PARM;
    }

    BYTE *buf = Malloc(len+16); // 16×Ö½Ú²¹Âë
    if(buf == NULL) {
        return OPRT_MALLOC_FAILED;
    }
    memcpy(buf,in,len);

    UINT tmplen;
    tmplen = Add_Pkcs((CHAR *)buf,len); // ²¹Âë

    httpc_aes_set((BYTE *)token,NULL);
    httpc_aes_encrypt(buf,tmplen,buf);

    CHAR *out = Malloc(2*tmplen + 10);
    if(NULL == out) {
        Free(buf);
        return OPRT_MALLOC_FAILED;
    }

    // prepare data=ase(data)
    strcpy(out,"data=");
    HexToStr((BYTE *)out+strlen("data="), (BYTE *)buf, tmplen);
    Free(buf);
    *pp_out = out;

    return OPRT_OK;
}

STATIC OPERATE_RET __httpc_com_json_resp_parse(IN CONST CHAR *if_name,IN CONST cJSON *root,\
                                               IN CONST BYTE *data,IN CONST UINT len,\
                                               OUT UINT *errcode)
{
    if(NULL == root || \
       NULL == if_name || \
       NULL == data) {
        return OPRT_INVALID_PARM;
    }

    cJSON *cjson;
    cjson = cJSON_GetObjectItem((cJSON *)root,"t");
    if(cjson) {
        wmtime_time_set_posix((time_t)cjson->valueint);
    }
    
    cjson = cJSON_GetObjectItem((cJSON *)root,"success");
    if(!cjson || (cjson && cjson->type != cJSON_True)) {
        PR_ERR("%s:%s",if_name,data);
        if(errcode) {
            *errcode = cJSON_GetObjectItem((cJSON *)root,"errorCode")->valueint;
        }
        return OPRT_HTTPS_HANDLE_FAIL;
    }else if(cjson && cjson->type == cJSON_True) {
        #if 0
        cjson = cJSON_GetObjectItem((cJSON *)root,"result");
        if(NULL == cjson) {
            PR_ERR("%s:%s",if_name,data);
            return OPRT_HTTPS_RESP_UNVALID;
        }
        #endif
    }

    return OPRT_OK;
}

STATIC OPERATE_RET __httpc_gw_active_cb(IN CONST BOOL is_end,\
                                        IN CONST UINT offset,\
                                        IN CONST BYTE *data,\
                                        IN CONST UINT len)
{
    if(NULL == data || \
       0 == len || \
       0 != offset || \
       is_end != TRUE) {
        return OPRT_INVALID_PARM;
    }

    OPERATE_RET op_ret;
    cJSON *root = NULL;
    root = cJSON_Parse((CHAR *)data);
    op_ret = __httpc_com_json_resp_parse("gw active",root,data,len,NULL);
    if(OPRT_OK != op_ret) {
        cJSON_Delete(root);
        PR_ERR("op_ret:%d",op_ret);
        return op_ret;
    }

    PR_DEBUG("%s",data);
    GW_CNTL_S *gw_cntl = get_gw_cntl();

    cJSON *child;
    child = cJSON_GetObjectItem((cJSON *)root,"result");
    if(NULL == child) {
        cJSON_Delete(root);
        return OPRT_CJSON_GET_ERR;
    }

    cJSON *cjson = cJSON_GetObjectItem(child,"secKey");
    if(NULL == cjson) {
        cJSON_Delete(root);
        return OPRT_CJSON_GET_ERR;
    }
    strcpy(gw_cntl->active.key,cjson->valuestring);

    cjson = cJSON_GetObjectItem(child,"uid");
    if(NULL == cjson) {
        cJSON_Delete(root);
        return OPRT_CJSON_GET_ERR;
    }
    gw_cntl->active.uid_cnt = 0;
    strcpy(gw_cntl->active.uid_acl[gw_cntl->active.uid_cnt++],cjson->valuestring);

    op_ret = ws_db_set_gw_actv(&gw_cntl->active);
    if(OPRT_OK != op_ret) {
        cJSON_Delete(root);
        return op_ret;
    }

    set_gw_status(STAT_WORK);
    cJSON_Delete(root);
    
    return OPRT_OK;
}

STATIC OPERATE_RET httpc_fill_com_param(INOUT HTTP_URL_H_S *hu_h,IN CONST CHAR *gwid,IN CONST CHAR *ifname)
{
    OPERATE_RET op_ret;
    op_ret = fill_url_head(hu_h,TY_SMART_DOMAIN);
    if(op_ret != OPRT_OK) {
        return op_ret;
    }

    fill_url_param(hu_h,"a",ifname);
    fill_url_param(hu_h,"gwId",gwid);

    // fill time
    CHAR time[15];
    snprintf(time,15,"%u",wmtime_time_get_posix());
    fill_url_param(hu_h,"t",time);

    return OPRT_OK;
}

/***********************************************************
*  Function: httpc_gw_active
*  Input: 
*  Output: 
*  Return: OPERATE_RET
***********************************************************/
OPERATE_RET httpc_gw_active()
{
    GW_CNTL_S *gw_cntl = get_gw_cntl();
    GW_DESC_IF_S *gw = &gw_cntl->gw;
    CHAR *out = NULL;
    HTTP_URL_H_S *hu_h = create_http_url_h(0,10);
    if(NULL == hu_h) {
        PR_ERR("create_http_url_h error");
        return OPRT_CR_HTTP_URL_H_ERR;
    }

    OPERATE_RET op_ret;
    op_ret = httpc_fill_com_param(hu_h,gw->id,TI_GW_ACTIVE);
    if(op_ret != OPRT_OK) {
        goto ERR_EXIT;
    }

    // fill uid
    cJSON *root=cJSON_CreateObject();
    if(NULL == root) {
        return OPRT_CR_CJSON_ERR;
    }
    cJSON_AddStringToObject(root,"uid",gw_cntl->active.uid_acl[0]);
    out=cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    root = NULL;
    if(NULL == out) {
        op_ret = OPRT_MALLOC_FAILED;
        goto ERR_EXIT;
    }

    fill_url_param(hu_h,"other",out);
    Free(out);
    out = NULL;

    // make content
    root=cJSON_CreateObject();
    if(NULL == root) {
        op_ret = OPRT_CR_CJSON_ERR;
        goto ERR_EXIT;
    }
    cJSON_AddStringToObject(root,"gwId",gw->id);
    cJSON_AddStringToObject(root,"name",gw->name);
    cJSON_AddStringToObject(root,"verSw",gw->sw_ver);
    cJSON_AddNumberToObject(root,"ability",gw->ability);
    cJSON_AddStringToObject(root,"etag",DEV_ETAG);
    out=cJSON_PrintUnformatted(root);
    cJSON_Delete(root),root = NULL;
    if(NULL == out) {
        op_ret = OPRT_MALLOC_FAILED;
        goto ERR_EXIT;
    }

    CHAR *buf;
    op_ret = httpc_data_aes_proc(out,strlen(out),gw_cntl->active.token,&buf);
    if(op_ret != OPRT_OK) {
        goto ERR_EXIT;
    }

    op_ret = make_full_url(hu_h,gw_cntl->active.token);
    if(op_ret != OPRT_OK) {
        Free(buf);
        goto ERR_EXIT;
    }

    op_ret = http_client_post(hu_h->buf,STANDARD_HDR_FLAGS|HDR_ADD_CONN_KEEP_ALIVE|HDR_ADD_CONTENT_TYPE_FORM_URLENCODE,\
                              __httpc_gw_active_cb,(BYTE *)buf,strlen(buf));
    Free(buf);
    Free(out);
    del_http_url_h(hu_h);

    return op_ret;

ERR_EXIT:
    if(out) {
        Free(out);
    }
    del_http_url_h(hu_h);

    return op_ret;
}

/***********************************************************
*  Function: httpc_gw_update
*  Input: 
*  Output: 
*  Return: OPERATE_RET
***********************************************************/
OPERATE_RET httpc_gw_update(IN CONST CHAR *name,IN CONST CHAR *sw_ver)
{
    if(NULL == name && NULL == sw_ver) {
        return OPRT_INVALID_PARM;
    }

    GW_CNTL_S *gw_cntl = get_gw_cntl();
    GW_DESC_IF_S *gw = &gw_cntl->gw;
    CHAR *out = NULL;
    HTTP_URL_H_S *hu_h = create_http_url_h(0,10);
    if(NULL == hu_h) {
        PR_ERR("create_http_url_h error");
        return OPRT_CR_HTTP_URL_H_ERR;
    }

    OPERATE_RET op_ret;
    op_ret = httpc_fill_com_param(hu_h,gw->id,TI_GW_INFO_UP);
    if(op_ret != OPRT_OK) {
        goto ERR_EXIT;
    }

    // fill uid
    cJSON *root=cJSON_CreateObject();
    if(NULL == root) {
        return OPRT_CR_CJSON_ERR;
    }
    cJSON_AddStringToObject(root,"uid",gw_cntl->active.uid_acl[0]);
    out=cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    root = NULL;
    if(NULL == out) {
        op_ret = OPRT_MALLOC_FAILED;
        goto ERR_EXIT;
    }

    fill_url_param(hu_h,"other",out);
    Free(out);
    out = NULL;

    // make content
    root=cJSON_CreateObject();
    if(NULL == root) {
        op_ret = OPRT_CR_CJSON_ERR;
        goto ERR_EXIT;
    }
    cJSON_AddStringToObject(root,"gwId",gw->id);
    if(NULL == name) {
        cJSON_AddStringToObject(root,"name",name);
    }
    if(NULL == sw_ver) {
        cJSON_AddStringToObject(root,"verSw",sw_ver);
    }

    out=cJSON_PrintUnformatted(root);
    cJSON_Delete(root),root = NULL;
    if(NULL == out) {
        op_ret = OPRT_MALLOC_FAILED;
        goto ERR_EXIT;
    }

    CHAR *buf;
    op_ret = httpc_data_aes_proc(out,strlen(out),gw_cntl->active.key,&buf);
    if(op_ret != OPRT_OK) {
        goto ERR_EXIT;
    }

    op_ret = make_full_url(hu_h,gw_cntl->active.key);
    if(op_ret != OPRT_OK) {
        Free(buf);
        goto ERR_EXIT;
    }

    op_ret = http_client_post(hu_h->buf,STANDARD_HDR_FLAGS|HDR_ADD_CONN_KEEP_ALIVE|HDR_ADD_CONTENT_TYPE_FORM_URLENCODE,\
                              __httpc_common_cb,(BYTE *)buf,strlen(buf));
    Free(buf);
    Free(out);
    del_http_url_h(hu_h);

    return op_ret;

ERR_EXIT:
    if(out) {
        Free(out);
    }
    del_http_url_h(hu_h);

    return op_ret;
}

/***********************************************************
*  Function: httpc_gw_reset
*  Input: 
*  Output: 
*  Return: OPERATE_RET
***********************************************************/
OPERATE_RET httpc_gw_reset(VOID)
{
    GW_CNTL_S *gw_cntl = get_gw_cntl();
    GW_DESC_IF_S *gw = &gw_cntl->gw;
    HTTP_URL_H_S *hu_h = create_http_url_h(0,10);
    if(NULL == hu_h) {
        PR_ERR("create_http_url_h error");
        return OPRT_CR_HTTP_URL_H_ERR;
    }

    OPERATE_RET op_ret;
    op_ret = httpc_fill_com_param(hu_h,gw->id,TI_GW_RESET);
    if(op_ret != OPRT_OK) {
        goto ERR_EXIT;
    }

    op_ret = make_full_url(hu_h,gw_cntl->active.key);
    if(op_ret != OPRT_OK) {
        goto ERR_EXIT;
    }

    op_ret = http_client_post(hu_h->buf,STANDARD_HDR_FLAGS|HDR_ADD_CONN_KEEP_ALIVE|HDR_ADD_CONTENT_TYPE_FORM_URLENCODE,\
                              __httpc_common_cb,NULL,0);
    del_http_url_h(hu_h);

    return op_ret;

ERR_EXIT:
    del_http_url_h(hu_h);

    return op_ret;
}

STATIC OPERATE_RET __httpc_gw_heart_cb(IN CONST BOOL is_end,\
                                       IN CONST UINT offset,\
                                       IN CONST BYTE *data,\
                                       IN CONST UINT len)
{
    if(NULL == data || \
       0 == len || \
       0 != offset || \
       is_end != TRUE) {
        return OPRT_INVALID_PARM;
    }

    cJSON *root = NULL;
    root = cJSON_Parse((CHAR *)data);

    cJSON *cjson;
    cjson = cJSON_GetObjectItem((cJSON *)root,"t");
    if(cjson) {
        wmtime_time_set_posix((time_t)cjson->valueint);
    }

    cJSON_Delete(root);
    return OPRT_OK;
}


/***********************************************************
*  Function: httpc_gw_hearat
*  Input: 
*  Output: 
*  Return: OPERATE_RET
***********************************************************/
OPERATE_RET httpc_gw_hearat(VOID)
{
    GW_CNTL_S *gw_cntl = get_gw_cntl();
    
    HTTP_URL_H_S *hu_h = create_http_url_h(0,10);
    if(NULL == hu_h) {
        PR_ERR("create_http_url_h error");
        return OPRT_CR_HTTP_URL_H_ERR;
    }

    OPERATE_RET op_ret;
    op_ret = httpc_fill_com_param(hu_h,gw_cntl->gw.id,TI_GW_HB);
    if(op_ret != OPRT_OK) {
        goto ERR_EXIT;
    }

    op_ret = make_full_url(hu_h,gw_cntl->active.key);
    if(op_ret != OPRT_OK) {
        goto ERR_EXIT;
    }

    op_ret = http_client_get(hu_h->buf,__httpc_gw_heart_cb);

    del_http_url_h(hu_h);
    return op_ret;

ERR_EXIT:
    del_http_url_h(hu_h);
    return op_ret;
}

STATIC OPERATE_RET __httpc_common_cb(IN CONST BOOL is_end,\
                                     IN CONST UINT offset,\
                                     IN CONST BYTE *data,\
                                     IN CONST UINT len)
{
    //PR_DEBUG("is_end:%d",is_end);
    //PR_DEBUG("offset:%d",offset);
    //PR_DEBUG("data:%s",data);
    //PR_DEBUG("len:%d",len);

    if(NULL == data || \
       0 == len || \
       0 != offset || \
       is_end != TRUE) {
        return OPRT_INVALID_PARM;
    }

    OPERATE_RET op_ret;
    cJSON *root = NULL;
    root = cJSON_Parse((CHAR *)data);
    op_ret = __httpc_com_json_resp_parse("common cb",root,data,len,NULL);
    cJSON_Delete(root);
    if(OPRT_OK != op_ret) {
        PR_ERR("op_ret:%d",op_ret);
        return op_ret;
    }

    return OPRT_OK;
}

/***********************************************************
*  Function: httpc_dev_bind
*  Input: 
*  Output: 
*  Return: OPERATE_RET
***********************************************************/
OPERATE_RET httpc_dev_bind(IN CONST DEV_DESC_IF_S *dev_if)
{
    if(NULL == dev_if || \
       0 == dev_if->id[0]) {
        return OPRT_INVALID_PARM;
    }

    GW_CNTL_S *gw_cntl = get_gw_cntl();

    CHAR *out = NULL;
    HTTP_URL_H_S *hu_h = create_http_url_h(0,10);
    if(NULL == hu_h) {
        PR_ERR("create_http_url_h error");
        return OPRT_CR_HTTP_URL_H_ERR;
    }

    OPERATE_RET op_ret;
    op_ret = httpc_fill_com_param(hu_h,gw_cntl->gw.id,TI_BIND_DEV);
    if(op_ret != OPRT_OK) {
        goto ERR_EXIT;
    }

    // make content
    cJSON *root=cJSON_CreateObject();
    if(NULL == root) {
        op_ret = OPRT_CR_CJSON_ERR;
        goto ERR_EXIT;
    }
    cJSON_AddStringToObject(root,"devId",dev_if->id);
    cJSON_AddStringToObject(root,"name",dev_if->name);
    cJSON_AddStringToObject(root,"verSw",dev_if->sw_ver);
    cJSON_AddStringToObject(root,"schemaId",dev_if->schema_id);
    cJSON_AddStringToObject(root,"uiId",dev_if->ui_id);
    cJSON_AddNumberToObject(root,"ability",dev_if->ability);
    cJSON_AddStringToObject(root,"etag",DEV_ETAG);

    out=cJSON_PrintUnformatted(root);
    cJSON_Delete(root),root = NULL;
    if(NULL == out) {
        op_ret = OPRT_MALLOC_FAILED;
        goto ERR_EXIT;
    }

    CHAR *buf;
    op_ret = httpc_data_aes_proc(out,strlen(out),gw_cntl->active.key,&buf);
    if(op_ret != OPRT_OK) {
        goto ERR_EXIT;
    }

    op_ret = make_full_url(hu_h,gw_cntl->active.key);
    if(op_ret != OPRT_OK) {
        Free(buf);
        goto ERR_EXIT;
    }

    op_ret = http_client_post(hu_h->buf,STANDARD_HDR_FLAGS|HDR_ADD_CONN_KEEP_ALIVE|HDR_ADD_CONTENT_TYPE_FORM_URLENCODE,\
                              __httpc_common_cb,(BYTE *)buf,strlen(buf));
    Free(buf);
    Free(out);
    del_http_url_h(hu_h);
    return op_ret;
    
ERR_EXIT:
    if(out) {
        Free(out);
    }
    del_http_url_h(hu_h);
    return op_ret;
}

/***********************************************************
*  Function: httpc_dev_unbind
*  Input: 
*  Output: 
*  Return: OPERATE_RET
***********************************************************/
OPERATE_RET httpc_dev_unbind(IN CONST CHAR *id)
{
    if(NULL == id) {
        return OPRT_INVALID_PARM;
    }

    GW_CNTL_S *gw_cntl = get_gw_cntl();
    DEV_CNTL_N_S *dev_cntl = get_dev_cntl(id);
    if(NULL == dev_cntl) {
        return OPRT_COM_ERROR;
    }

    CHAR *out = NULL;
    HTTP_URL_H_S *hu_h = create_http_url_h(0,10);
    if(NULL == hu_h) {
        PR_ERR("create_http_url_h error");
        return OPRT_CR_HTTP_URL_H_ERR;
    }

    OPERATE_RET op_ret;
    op_ret = httpc_fill_com_param(hu_h,gw_cntl->gw.id,TI_UNBIND_DEV);
    if(op_ret != OPRT_OK) {
        goto ERR_EXIT;
    }

    // make content
    cJSON *root=cJSON_CreateObject();
    if(NULL == root) {
        op_ret = OPRT_CR_CJSON_ERR;
        goto ERR_EXIT;
    }
    cJSON_AddStringToObject(root,"devId",dev_cntl->dev_if.id);
    out=cJSON_PrintUnformatted(root);
    cJSON_Delete(root),root = NULL;
    if(NULL == out) {
        op_ret = OPRT_MALLOC_FAILED;
        goto ERR_EXIT;
    }

    CHAR *buf;
    op_ret = httpc_data_aes_proc(out,strlen(out),gw_cntl->active.key,&buf);
    if(op_ret != OPRT_OK) {
        goto ERR_EXIT;
    }

    op_ret = make_full_url(hu_h,gw_cntl->active.key);
    if(op_ret != OPRT_OK) {
        Free(buf);
        goto ERR_EXIT;
    }

    op_ret = http_client_post(hu_h->buf,STANDARD_HDR_FLAGS|HDR_ADD_CONN_KEEP_ALIVE|HDR_ADD_CONTENT_TYPE_FORM_URLENCODE,\
                              __httpc_common_cb,(BYTE *)buf,strlen(buf));
    Free(buf);
    Free(out);
    del_http_url_h(hu_h);
    return op_ret;
    
ERR_EXIT:
    if(out) {
        Free(out);
    }
    del_http_url_h(hu_h);
    return op_ret;
}

/***********************************************************
*  Function: httpc_dev_update
*  Input: 
*  Output: 
*  Return: OPERATE_RET
***********************************************************/
OPERATE_RET httpc_dev_update(IN CONST DEV_DESC_IF_S *dev_if)
{
    if(NULL == dev_if || \
       0 == dev_if->id[0]) {
        return OPRT_INVALID_PARM;
    }

    GW_CNTL_S *gw_cntl = get_gw_cntl();

    CHAR *out = NULL;
    HTTP_URL_H_S *hu_h = create_http_url_h(0,10);
    if(NULL == hu_h) {
        PR_ERR("create_http_url_h error");
        return OPRT_CR_HTTP_URL_H_ERR;
    }

    OPERATE_RET op_ret;
    op_ret = httpc_fill_com_param(hu_h,gw_cntl->gw.id,TI_DEV_INFO_UP);
    if(op_ret != OPRT_OK) {
        goto ERR_EXIT;
    }

    // make content
    cJSON *root=cJSON_CreateObject();
    if(NULL == root) {
        op_ret = OPRT_CR_CJSON_ERR;
        goto ERR_EXIT;
    }
    cJSON_AddStringToObject(root,"devId",dev_if->id);
    if(dev_if->name[0]) {
        cJSON_AddStringToObject(root,"name",dev_if->name);
    }
    if(dev_if->sw_ver[0]) { 
        cJSON_AddStringToObject(root,"verSw",dev_if->sw_ver);
    }
    if(dev_if->schema_id[0]) {
        cJSON_AddStringToObject(root,"schemaId",dev_if->schema_id);
    }
    if(dev_if->ui_id[0]) {
        cJSON_AddStringToObject(root,"uiId",dev_if->ui_id);
    }

    out=cJSON_PrintUnformatted(root);
    cJSON_Delete(root),root = NULL;
    if(NULL == out) {
        op_ret = OPRT_MALLOC_FAILED;
        goto ERR_EXIT;
    }

    CHAR *buf;
    op_ret = httpc_data_aes_proc(out,strlen(out),gw_cntl->active.key,&buf);
    if(op_ret != OPRT_OK) {
        goto ERR_EXIT;
    }

    op_ret = make_full_url(hu_h,gw_cntl->active.key);
    if(op_ret != OPRT_OK) {
        Free(buf);
        goto ERR_EXIT;
    }

    op_ret = http_client_post(hu_h->buf,STANDARD_HDR_FLAGS|HDR_ADD_CONN_KEEP_ALIVE|HDR_ADD_CONTENT_TYPE_FORM_URLENCODE,\
                              __httpc_common_cb,(BYTE *)buf,strlen(buf));
    Free(buf);
    Free(out);
    del_http_url_h(hu_h);
    return op_ret;
    
ERR_EXIT:
    if(out) {
        Free(out);
    }
    del_http_url_h(hu_h);
    return op_ret;

}

/***********************************************************
*  Function: httpc_dev_stat_report
*  Input: 
*  Output: 
*  Return: OPERATE_RET
***********************************************************/
OPERATE_RET httpc_dev_stat_report(IN CONST CHAR *id,IN CONST BOOL online)
{
    GW_CNTL_S *gw_cntl = get_gw_cntl();

    CHAR *out = NULL;
    HTTP_URL_H_S *hu_h = create_http_url_h(0,10);
    if(NULL == hu_h) {
        PR_ERR("create_http_url_h error");
        return OPRT_CR_HTTP_URL_H_ERR;
    }

    OPERATE_RET op_ret;
    op_ret = httpc_fill_com_param(hu_h,gw_cntl->gw.id,TI_DEV_REP_STAT);
    if(op_ret != OPRT_OK) {
        goto ERR_EXIT;
    }

    // make content
    cJSON *root=cJSON_CreateObject();
    if(NULL == root) {
        op_ret = OPRT_CR_CJSON_ERR;
        goto ERR_EXIT;
    }
    cJSON_AddStringToObject(root,"devId",id);
    cJSON_AddBoolToObject(root,"online",online);

    out=cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    root = NULL;
    if(NULL == out) {
        op_ret = OPRT_MALLOC_FAILED;
        goto ERR_EXIT;
    }

    CHAR *buf;
    op_ret = httpc_data_aes_proc(out,strlen(out),gw_cntl->active.key,&buf);
    if(op_ret != OPRT_OK) {
        goto ERR_EXIT;
    }

    op_ret = make_full_url(hu_h,gw_cntl->active.key);
    if(op_ret != OPRT_OK) {
        Free(buf);
        goto ERR_EXIT;
    }

    op_ret = http_client_post(hu_h->buf,STANDARD_HDR_FLAGS|HDR_ADD_CONN_KEEP_ALIVE|HDR_ADD_CONTENT_TYPE_FORM_URLENCODE,\
                              __httpc_common_cb,(BYTE *)buf,strlen(buf));
    Free(buf);
    Free(out);
    del_http_url_h(hu_h);
    return op_ret;
    
ERR_EXIT:
    if(out) {
        Free(out);
    }
    del_http_url_h(hu_h);
    return op_ret;
}

#if 0
/***********************************************************
*  Function: httpc_dev_dp_report
*  Input: 
*  Output: 
*  Return: OPERATE_RET
***********************************************************/
STATIC OPERATE_RET httpc_dev_dp_report(IN CONST CHAR *id,IN CONST DP_TYPE_E type,\
                                       IN CONST CHAR *data)
{
    if(NULL == id || NULL == data) {
        return OPRT_INVALID_PARM;
    }

    if(T_FILE == type) {
        PR_ERR("do not support the file dp");
        return OPRT_INVALID_PARM;
    }

    GW_CNTL_S *gw_cntl = get_gw_cntl();
    cJSON *data_json = NULL;
    CHAR *out = NULL;
    HTTP_URL_H_S *hu_h = create_http_url_h(0,10);
    if(NULL == hu_h) {
        PR_ERR("create_http_url_h error");
        return OPRT_CR_HTTP_URL_H_ERR;
    }

    OPERATE_RET op_ret;
    op_ret = httpc_fill_com_param(hu_h,gw_cntl->gw.id,TI_DEV_DP_REP);
    if(op_ret != OPRT_OK) {
        goto ERR_EXIT;
    }

    data_json = cJSON_Parse(data);
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

    out=cJSON_PrintUnformatted(root);
    cJSON_Delete(root),data_json = NULL,root = NULL;
    if(NULL == out) {
        op_ret = OPRT_MALLOC_FAILED;
        goto ERR_EXIT;
    }

    CHAR *buf;
    op_ret = httpc_data_aes_proc(out,strlen(out),gw_cntl->active.key,&buf);
    if(op_ret != OPRT_OK) {
        goto ERR_EXIT;
    }

    op_ret = make_full_url(hu_h,gw_cntl->active.key);
    if(op_ret != OPRT_OK) {
        Free(buf);
        goto ERR_EXIT;
    }

    op_ret = http_client_post(hu_h->buf,STANDARD_HDR_FLAGS|HDR_ADD_CONN_KEEP_ALIVE|HDR_ADD_CONTENT_TYPE_FORM_URLENCODE,\
                              __httpc_common_cb,(BYTE *)buf,strlen(buf));
    Free(buf);
    Free(out);
    del_http_url_h(hu_h);
    return op_ret;

ERR_EXIT:
    cJSON_Delete(data_json);
    if(out) {
        Free(out);
    }
    del_http_url_h(hu_h);
    return op_ret;
}

/***********************************************************
*  Function: httpc_dev_raw_dp_report
*  Input: 
*  Output: 
*  Return: OPERATE_RET
***********************************************************/
OPERATE_RET httpc_dev_raw_dp_report(IN CONST CHAR *id,IN CONST BYTE dpid,\
                                    IN CONST BYTE *data, IN CONST UINT len)
{
    if(NULL == data || \
       NULL == id) {
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

    OPERATE_RET op_ret;
    op_ret = httpc_dev_dp_report(id,T_RAW,out);
    Free(out),out = NULL;

    return op_ret;
}

/***********************************************************
*  Function: httpc_dev_obj_dp_report
*  Input: 
*  Output: 
*  Return: OPERATE_RET
***********************************************************/
OPERATE_RET httpc_dev_obj_dp_report(IN CONST CHAR *id,IN CONST CHAR *data)
{
    return httpc_dev_dp_report(id,T_OBJ,data);
}
#endif

/***********************************************************
*  Function: httpc_device_dp_report
*  Input: data->format is:{"devid":"xx","dps":{"1",1}}
*  Output: 
*  Return: OPERATE_RET
***********************************************************/
OPERATE_RET httpc_device_dp_report(IN CONST DP_TYPE_E type,\
                                   IN CONST CHAR *data)
{
    if(NULL == data) {
        return OPRT_INVALID_PARM;
    }

    if(T_FILE == type) {
        PR_ERR("do not support the file dp");
        return OPRT_INVALID_PARM;
    }

    GW_CNTL_S *gw_cntl = get_gw_cntl();
    HTTP_URL_H_S *hu_h = create_http_url_h(0,10);
    if(NULL == hu_h) {
        PR_ERR("create_http_url_h error");
        return OPRT_CR_HTTP_URL_H_ERR;
    }

    OPERATE_RET op_ret;
    op_ret = httpc_fill_com_param(hu_h,gw_cntl->gw.id,TI_DEV_DP_REP);
    if(op_ret != OPRT_OK) {
        goto ERR_EXIT;
    }

    CHAR *buf;
    op_ret = httpc_data_aes_proc(data,strlen(data),gw_cntl->active.key,&buf);
    if(op_ret != OPRT_OK) {
        goto ERR_EXIT;
    }

    op_ret = make_full_url(hu_h,gw_cntl->active.key);
    if(op_ret != OPRT_OK) {
        Free(buf);
        goto ERR_EXIT;
    }

    op_ret = http_client_post(hu_h->buf,STANDARD_HDR_FLAGS|HDR_ADD_CONN_KEEP_ALIVE|HDR_ADD_CONTENT_TYPE_FORM_URLENCODE,\
                              __httpc_common_cb,(BYTE *)buf,strlen(buf));
    Free(buf);
    del_http_url_h(hu_h);
    return op_ret;

ERR_EXIT:
    del_http_url_h(hu_h);
    return op_ret;
}

#if 0
VOID tuya_http_test(VOID)
{
    HTTP_URL_H_S *hu_h = create_http_url_h(0,20);
    if(NULL == hu_h) {
        PR_ERR("create_http_url_h error");
        return;
    }

    OPERATE_RET op_ret;
    op_ret = fill_url_head(hu_h,TY_SMART_DOMAIN);
    if(op_ret != OPRT_OK) {
        PR_ERR("op_ret:%d",op_ret);
        return;
    }
    PR_DEBUG("url_buf:%s",hu_h->buf);

    op_ret = fill_url_param(hu_h,"test1","111");
    if(op_ret != OPRT_OK) {
        PR_ERR("op_ret:%d",op_ret);
        return;
    }
    PR_DEBUG("url_buf:%s",hu_h->buf);

    op_ret = fill_url_param(hu_h,"test5","555");
    if(op_ret != OPRT_OK) {
        PR_ERR("op_ret:%d",op_ret);
        return;
    }
    
    op_ret = fill_url_param(hu_h,"test2","222");
    if(op_ret != OPRT_OK) {
        PR_ERR("op_ret:%d",op_ret);
        return;
    }

    op_ret = make_full_url(hu_h);
    if(op_ret != OPRT_OK) {
        PR_ERR("op_ret:%d",op_ret);
        return;
    }    
    PR_DEBUG("url_buf:%s",hu_h->buf);

    del_http_url_h(hu_h);
}
#endif


