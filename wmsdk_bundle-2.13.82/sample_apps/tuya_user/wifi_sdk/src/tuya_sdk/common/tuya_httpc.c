/***********************************************************
*  File: tuya_httpc.c
*  Author: nzy
*  Date: 20150527
***********************************************************/
#define __TUYA_HTTPC_GLOBALS
#include "tuya_httpc.h"
#include "md5.h"
#include <mdev_aes.h>

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

    HTTP_URL_H_S *hu_h = malloc(sizeof(HTTP_URL_H_S) + len + \
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
    if(hu_h) {
        free(hu_h);
    }
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
STATIC OPERATE_RET make_full_url(INOUT HTTP_URL_H_S *hu_h)
{
    if(NULL == hu_h || \
       NULL == hu_h->param_h || \
       (hu_h->param_h && (0 == hu_h->param_h->cnt))) {
        return OPRT_INVALID_PARM;
    }

    HTTP_PARAM_H_S *param_h = hu_h->param_h;
    INT i,j,k;
    CHAR *tmp_str;

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

    // make md5 sign 
    CHAR sign[32+1];
    MD5_CTX md5;
    MD5Init(&md5);
    unsigned char decrypt[16];

    for(i = 0;i < param_h->cnt;i++) {
        MD5Update(&md5,(BYTE *)param_h->pos[i],strlen(param_h->pos[i]));
    }
    MD5Final(&md5,decrypt);

    INT offset = 0;
    for(i = 0;i < 16;i++) {
        sprintf(&sign[offset],"%02x",decrypt[i]);
        offset += 2;
    }
    sign[offset] = 0;

    // to sort string
    INT len = hu_h->param_in - (hu_h->buf+hu_h->head_size);
    tmp_str = (CHAR *)malloc(len+1); // +1 to save 0
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

    free(tmp_str);

    // set sign to url
    OPERATE_RET op_ret;
    op_ret = fill_url_param(hu_h,"sign",sign);
    if(OPRT_OK != op_ret) {
        return op_ret;
    }

    return op_ret;
}

STATIC OPERATE_RET __httpc_com_hanle(IN CONST http_req_t *req,IN CONST CHAR *url,IN CONST HTTPC_CB callback)
{
    if(NULL == req || \
       NULL == callback || \
       NULL == url) {
        return OPRT_OK;
    }

    http_session_t hnd;
    int ret = http_open_session(&hnd, url, 0, NULL, 0);
    if (ret != 0) {
        PR_ERR("Open session failed: %s (%d)", url, ret);
        return OPRT_HTTP_OS_ERROR;
    }

    ret = http_prepare_req(hnd, req,
                  STANDARD_HDR_FLAGS |
                  HDR_ADD_CONN_KEEP_ALIVE);
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
    #define DEF_BUF_SIZE 512
    BYTE *buf;
    buf = (BYTE *)malloc(512);
    if(buf == NULL) {
        http_close_session(&hnd);
        return OPRT_MALLOC_FAILED;
    }

    UINT offset = 0;
    while (1) {
        ret = http_read_content(hnd, buf, DEF_BUF_SIZE);
        if (ret < 0) {
            free(buf);
            http_close_session(&hnd);
            return OPRT_HTTP_RD_ERROR;
        }else if(0 == ret){ // ½áÊø
            if(resp->chunked) {
                callback(TRUE,offset,NULL,0);
            }
            break;
        }else {
            BOOL is_end = FALSE;
            if(!resp->chunked) {
                if((offset+ret) >= resp->content_length) {
                    is_end = TRUE;
                }
            }
            callback(is_end,offset,buf,ret);
            offset += ret;
        }
    }

    free(buf);
    http_close_session(&hnd);

    return OPRT_OK;
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
    op_ret = __httpc_com_hanle(&req,url,callback);

    return op_ret;
}

/***********************************************************
*  Function: http_client_post
*  Input: url callback content len
*  Output: hu_h
*  Return: OPERATE_RET
***********************************************************/
STATIC OPERATE_RET http_client_post(IN CONST CHAR *url,\
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
    op_ret = __httpc_com_hanle(&req,url,callback);

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
                         httpc_aes.iv, AES_ENCRYPTION, HW_AES_MODE_CBC);
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

OPERATE_RET httpc_gw_active()
{
    
}




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



