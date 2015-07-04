/***********************************************************
*  File: com_struct.h 
*  Author: nzy
*  Date: 20150522
***********************************************************/
#ifndef _COM_STRUCT_H
    #define _COM_STRUCT_H

    #include "com_def.h"
    #include "sys_adapter.h"
#ifdef __cplusplus
	extern "C" {
#endif

#ifdef  __COM_STRUCT_GLOBALS
    #define __COM_STRUCT_EXT
#else
    #define __COM_STRUCT_EXT extern
#endif

/***********************************************************
*************************micro define***********************
***********************************************************/
// gw information define
#define PROD_IDX_LEN 8 // prodect index len
#define GW_ID_LEN 40 // gw id len
#define DEV_ID_LEN 40 // dev id len
#define NAME_LEN 20 // name len
#define UID_LEN 20 // user id len
#define UID_ACL_LMT 5 // user acl limit
#define SW_VER_LEN 16 // sw ver len
#define SEC_KEY_LEN 16 // securt key len
#define ACTIVE_KEY_LEN 32 // active key len
#define SCHEMA_ID_LEN 10 
#define UI_ID_LEN 10
#define ETAG_LEN 10
#define HTTP_URL_LMT 128
#define MQ_URL_LMT 128

// gateway access ability
typedef INT GW_ABI;
#define GW_NO_ABI 0 // virtual gateway
#define GW_ZB_ABI (1<<0) // zigbee
#define GW_BLE_ABI (1<<1) // ble
#define GW_RF433_ABI (1<<2) // RF 315/433
#define GW_DEF_ABI GW_NO_ABI

// device ability
typedef enum {
    DEV_SINGLE = 0, // ��ָWIFI��Ʒ�豸
    DEV_ZB, // zigbee
    DEV_BLE,
    DEV_RF433,
    DEV_WIFI, // ͨ��ʵ��GW�����WIFI�豸
}DEV_ABI_E;

// dp type
typedef enum {
    T_OBJ = 0,
    T_RAW,
    T_FILE,
}DP_TYPE_E;

// dp mode
typedef enum {
    M_RW = 0, // read write
    M_WR, // only write
    M_RO, // only read
}DP_MODE_E;

// dp schema type
typedef enum {
    PROP_BOOL = 0,
    PROP_VALUE,
    PROP_STR,
    PROP_ENUM
}DP_PROP_TP_E;

typedef struct {
    CHAR id[GW_ID_LEN+1];
    CHAR name[NAME_LEN+4+1+1]; // add '-xxxx'
    CHAR sw_ver[SW_VER_LEN+1];
    GW_ABI ability;
    BOOL sync;
}GW_DESC_IF_S;

typedef struct {
    CHAR id[DEV_ID_LEN+1];
    CHAR name[NAME_LEN+1];
    CHAR sw_ver[SW_VER_LEN+1];
    CHAR schema_id[SCHEMA_ID_LEN+1];
    CHAR ui_id[UI_ID_LEN+1];
    DEV_ABI_E ability;
    BOOL bind;
    BOOL sync;
}DEV_DESC_IF_S;

// dp prop 
typedef struct {
    UINT min;
    UINT max;
    USHORT step;
    USHORT scale; // ����value��DP��10��ָ��
    UINT value;
}DP_PROP_VAL_S;

typedef struct {
    INT cnt;
    CHAR **pp_enum;
    INT value;
}DP_PROP_ENUM_S;

typedef struct {
    INT max_len;
    CHAR *value;
}DP_PROP_STR_S;

typedef struct {
    BOOL value;
}DP_BOOL_S;

typedef union {
    DP_PROP_VAL_S prop_value;
    DP_PROP_ENUM_S prop_enum;
    DP_PROP_STR_S prop_str;
    DP_BOOL_S prop_bool;
}DP_PROP_VALUE_U;

typedef enum {
    TRIG_PULSE = 0,
    TRIG_DIRECT
}DP_TRIG_T_E;

/*
��ʶĳDP״̬�Ƿ�Ϊ�����ϱ�
DP SCHEMA�м�¼ֵΪbool��
*/
typedef enum {
    PSV_FALSE = FALSE,
    PSV_TRUE,
    PSV_F_ONCE, // �������ϱ��ɹ�һ�κ�������Ϊ�����ϱ�������������ȷ��
}DP_PSV_E;

typedef struct {
    BYTE dp_id;
    DP_MODE_E mode;
    DP_PSV_E passive;
    DP_TYPE_E type;
    DP_PROP_TP_E prop_tp; // type == objʱ��Ч
    DP_TRIG_T_E trig_t; // ������������
}DP_DESC_IF_S;

typedef enum {
    INVALID = 0, // ������Ч
    VALID_LC, // ������Ч����
    VALID_CLOUD, // ������Ч����������һ��
}DP_PV_STAT_E;

typedef struct {
    DP_DESC_IF_S dp_desc;
    DP_PROP_VALUE_U prop;
    DP_PV_STAT_E pv_stat; 
    //BOOL upload; // ָʾ�����Ƿ��ϴ�
}DP_CNTL_S;

typedef struct dev_cntl_n_s {
    struct dev_cntl_n_s *next;
    DEV_DESC_IF_S dev_if;
    BOOL online;
    BOOL preprocess; // ָʾ���豸�Ƿ�Ԥ����
    BYTE dp_num;
    DP_CNTL_S dp[0];
}DEV_CNTL_N_S;

typedef struct {
    CHAR token[SEC_KEY_LEN+1];
    CHAR key[SEC_KEY_LEN+1];
    CHAR http_url[HTTP_URL_LMT+1];
    CHAR mq_url[MQ_URL_LMT+1];
    CHAR mq_url_bak[MQ_URL_LMT+1];
    INT uid_cnt;
    CHAR uid_acl[UID_ACL_LMT][UID_LEN+1];
}GW_ACTV_IF_S;

typedef enum {
    UN_INIT = 0, // δ��ʼ��������������Ϣδд��
    UN_ACTIVE, // δ����
    ACTIVE_RD, // �������̬
    STAT_WORK, // ��������̬
}GW_STAT_E;

typedef enum {
    STAT_UNPROVISION = 0,
    STAT_AP_STA_UNCONN,
    STAT_AP_STA_CONN,
    STAT_STA_UNCONN,
    STAT_STA_CONN,
}GW_WIFI_STAT_E;

typedef struct {
    GW_DESC_IF_S gw;
    GW_ACTV_IF_S active;
    GW_STAT_E stat;
    GW_WIFI_STAT_E wf_stat;
    INT dev_num;
    DEV_CNTL_N_S *dev;
}GW_CNTL_S;

/***********************************************************
*************************variable define********************
***********************************************************/


/***********************************************************
*************************function define********************
***********************************************************/
/***********************************************************
*  Function: 
*  Input: 
*  Output: 
*  Return: 
***********************************************************/

#ifdef __cplusplus
}
#endif
#endif

