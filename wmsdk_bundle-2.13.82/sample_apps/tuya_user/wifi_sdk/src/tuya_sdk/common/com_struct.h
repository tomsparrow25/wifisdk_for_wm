/***********************************************************
*  File: com_struct.h 
*  Author: nzy
*  Date: 20150522
***********************************************************/
#ifndef _COM_STRUCT_H
    #define _COM_STRUCT_H

    #include "com_def.h"
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
#define PROD_IDX_LEN 8 // prodect index len
#define GW_ID_LEN 40 // gw id len
#define DEV_ID_LEN 40 // dev id len
#define NAME_LEN 20 // name len
#define UID_LEN 20 // user id len
#define SW_VER_LEN 16 // sw ver len
#define SEC_KEY_LEN 32 // securt key len
#define ACTIVE_KEY_LEN 32 // active key len

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

typedef enum {
    OFFLINE = 0,
    ONLINE,
}LINE_STATUS_E;

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
    SCH_BOOL = 0,
    SCH_TYPE,
    SCH_STR,
    SCH_ENUM
}DP_SCH_TP_E;

typedef struct {
    CHAR id[GW_ID_LEN+1];
    CHAR name[NAME_LEN+1];
    CHAR sw_ver[SW_VER_LEN+1];
    GW_ABI ability;
}GW_DESC_IF_S;

typedef struct {
    CHAR id[DEV_ID_LEN+1];
    CHAR name[NAME_LEN+1];
    CHAR sw_ver[SW_VER_LEN+1];
    DEV_ABI_E ability;
    UINT schema_id;
    UINT ui_id;
}DEV_DESC_IF_S;

typedef struct {
    BYTE dp_id;
    DP_MODE_E mode;
    DP_TYPE_E type;
    DP_SCH_TP_E sch_tp; // type == obj时有效
}DP_DESC_IF_S;

#define DEF_DP_DATA_SIZE 32
typedef struct {
    DP_DESC_IF_S dp_desc;
    INT len;
    CHAR *dp_data;
}DP_CNTL_S;

typedef struct dev_cntl_n_s {
    struct dev_cntl_n_s *next;
    DEV_DESC_IF_S dev_desc;
    LINE_STATUS_E status;
    INT du_num;
    DP_CNTL_S dp[0];
}DEV_CNTL_N_S;

typedef struct {
    GW_DESC_IF_S gw;
    INT dev_num;
    DEV_CNTL_N_S *dev;
}GW_CNTL_S;

// want to save in the flash
// mac addr 
// prodect index
// dev sw_ver
// UINT schema_id
// UINT ui_id;
// dev name
// 
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

