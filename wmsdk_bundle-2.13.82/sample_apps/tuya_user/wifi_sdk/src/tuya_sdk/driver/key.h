/***********************************************************
*  File: key.h 
*  Author: nzy
*  Date: 20150522
***********************************************************/
#ifndef _KEY_H
    #define _KEY_H

    #include "../common/com_def.h"

#ifdef __cplusplus
	extern "C" {
#endif

#ifdef  __KEY_GLOBALS
    #define __KEY_EXT
#else
    #define __KEY_EXT extern
#endif

/***********************************************************
*************************micro define***********************
***********************************************************/
typedef enum {
    NORMAL_KEY = 0,
    SEQ_KEY,
    LONG_KEY,
}PUSH_KEY_TYPE_E;

typedef enum {
    KEY_DOWN = 0,
    KEY_DOWN_CONFIRM,
    KEY_DOWNNING, 
    KEY_UP_CONFIRM,
    KEY_UPING,
    KEY_FINISH,
}KEY_STAT_E;

typedef VOID(* KEY_CALLBACK)(INT gpio_no,PUSH_KEY_TYPE_E type,INT cnt);

// (cnt >= 2) ==> SEQ_KEY
// time < long_key_time && (cnt = 1) ==> NORMAL_KEY
// time >= long_key_time && (cnt = 1) ==> LONG_KEY
typedef struct {
    // user define
    INT gpio_no;
    INT long_key_time; // ms
    KEY_CALLBACK call_back;

    // run variable
    KEY_STAT_E status;
    INT down_time; // ms
    INT up_time;
    INT seq_key_cnt;
}KEY_ENTITY_S;

#define TIMER_SPACE_MAX 100 // ms
/***********************************************************
*************************variable define********************
***********************************************************/


/***********************************************************
*************************function define********************
***********************************************************/
/***********************************************************
*  Function: key_init
*  Input: 
*  Output: 
*  Return: OPERATE_RET
***********************************************************/
__KEY_EXT \
OPERATE_RET key_init(IN KEY_ENTITY_S *p_tbl,\
                     IN CONST INT cnt,
                     IN CONST INT timer_space);
                            
#ifdef __cplusplus
}
#endif
#endif

