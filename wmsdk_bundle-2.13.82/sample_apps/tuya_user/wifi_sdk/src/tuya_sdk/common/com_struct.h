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
#define GW_VER "0.0"
#define GW_DEF_NAME "virtual gw"

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

// gateway access ability
typedef INT GW_ABI;
#define GW_NO_ABI 0 // virtual gateway
#define GW_ZB_ABI (1<<0) // zigbee
#define GW_BLE_ABI (1<<1) // ble
#define GW_RF433_ABI (1<<2) // RF 315/433

// device ability
typedef enum {
    DEV_SINGLE = 0, // 特指WIFI单品设备
    DEV_ZB, // zigbee
    DEV_BLE,
    DEV_RF433,
    DEV_WIFI, // 通过实体GW接入的WIFI设备
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
    CHAR name[NAME_LEN+1];
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
    USHORT scale; // 描述value型DP的10的指数
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
}DP_PROP_U;

typedef enum {
    TRIG_PULSE = 0,
    TRIG_DIRECT
}DP_TRIG_T_E;

typedef struct {
    BYTE dp_id;
    DP_MODE_E mode;
    DP_TYPE_E type;
    DP_PROP_TP_E prop_tp; // type == obj时有效
    DP_TRIG_T_E trig_t; // 联动触发类型
}DP_DESC_IF_S;

typedef enum {
    UNVALID = 0, // 数据无效
    VALID_LC, // 本地有效数据
    VALID_CLOUD, // 本地有效数据与服务端一致
}DP_VALUE_PROP_E;

typedef struct {
    DP_DESC_IF_S dp_desc;
    DP_PROP_U prop;
    DP_VALUE_PROP_E value_prop; 
    //BOOL upload; // 指示数据是否上传
}DP_CNTL_S;

typedef struct dev_cntl_n_s {
    struct dev_cntl_n_s *next;
    DEV_DESC_IF_S dev_if;
    BOOL online;
    INT dp_num;
    DP_CNTL_S dp[0];
}DEV_CNTL_N_S;

typedef struct {
    CHAR token[SEC_KEY_LEN+1];
    CHAR key[SEC_KEY_LEN+1];
    INT uid_cnt;
    CHAR uid_acl[UID_ACL_LMT][UID_LEN+1];
}GW_ACTV_IF_S;

typedef enum {
    UN_INIT = 0, // 未初始化，比如生产信息未写入
    UN_ACTIVE, // 未激活
    ACTIVE_RD, // 激活就绪态
    STAT_UG, // 升级态
    STAT_WORK, // 正常工作态
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

