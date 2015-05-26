/***********************************************************
*  File: key.c 
*  Author: nzy
*  Date: 20150522
***********************************************************/
#define __KEY_GLOBALS
#include "key.h"
#include <wm_os.h>

/***********************************************************
*************************micro define***********************
***********************************************************/
#if 0
typedef struct {
    KEY_STAT_E status;
    INT down_time; // ms
    INT up_time;
    INT seq_key_cnt;
}KEY_ENTITY_VAL_S;
#endif

typedef struct {
    KEY_ENTITY_S *p_tbl;
    INT tbl_cnt;
    INT timer_space;
}KEY_MANAGE_S;

#define SEQ_KEY_DETECT_TIME 400 // ms
/***********************************************************
*************************variable define********************
***********************************************************/
STATIC KEY_MANAGE_S key_mag;

/***********************************************************
*************************function define********************
***********************************************************/
STATIC VOID key_handle(VOID);

// key adapter
#include <mc200_gpio.h>
#include <mc200_pinmux.h>

STATIC VOID __set_gpio_in(INT gpio_no)
{
	GPIO_PinMuxFun(gpio_no, PINMUX_FUNCTION_0);
	GPIO_SetPinDir(gpio_no, GPIO_INPUT);
}

STATIC INT __read_gpio_level(INT gpio_no)
{
    return GPIO_ReadPinLevel(gpio_no);
}

static void key_timer_cb(os_timer_arg_t arg)
{
    key_handle();
}

STATIC OPERATE_RET cr_key_timer(INT timer_space)
{
    STATIC os_timer_t key_timer;
    int err = os_timer_create(&key_timer,
				  "key-timer",
				  os_msec_to_ticks(timer_space),
				  &key_timer_cb,
				  NULL,
				  OS_TIMER_PERIODIC,
				  OS_TIMER_AUTO_ACTIVATE);
	if (err != WM_SUCCESS) {
		return OPRT_COM_ERROR;
	}

    return OPRT_OK;
}

/***********************************************************
*  Function: key_init
*  Input: 
*  Output: 
*  Return: OPERATE_RET
***********************************************************/
OPERATE_RET key_init(IN KEY_ENTITY_S *p_tbl,\
                     IN CONST INT cnt,
                     IN CONST INT timer_space)
{
    if(NULL == p_tbl || \
       0 == cnt || \
       timer_space > TIMER_SPACE_MAX) {
        return OPRT_INVALID_PARM;
    }

    key_mag.p_tbl = p_tbl;
    key_mag.tbl_cnt = cnt;
    key_mag.timer_space = timer_space;

    // init
    INT i;
    for(i = 0;i < cnt;i++) {
        __set_gpio_in(key_mag.p_tbl[i].gpio_no);
        key_mag.p_tbl[i].status = 0;
        key_mag.p_tbl[i].down_time = 0;
        key_mag.p_tbl[i].up_time = 0;
        key_mag.p_tbl[i].seq_key_cnt = 0;
    }

    OPERATE_RET op_ret;
    op_ret = cr_key_timer(timer_space);
    if(OPRT_OK != op_ret) {
        return op_ret;
    }

    return OPRT_OK;
}

/***********************************************************
*  Function: key_handle 
*  Input: 
*  Output: 
*  Return: VOID
*  NOTE: put this handle in the timer_space timer
***********************************************************/
STATIC VOID key_handle(VOID)
{
    INT i;
    INT gpio_no;

    for(i = 0;i < key_mag.tbl_cnt;i++) {
        gpio_no = key_mag.p_tbl[i].gpio_no;
        
        switch(key_mag.p_tbl[i].status) {
            case KEY_DOWN: {
                if(0 == __read_gpio_level(gpio_no)) {
                    key_mag.p_tbl[i].status = KEY_DOWN_CONFIRM;
                }
                key_mag.p_tbl[i].down_time = 0;
                key_mag.p_tbl[i].up_time = 0;
                key_mag.p_tbl[i].seq_key_cnt = 0;
            }
            break;
            
            case KEY_DOWN_CONFIRM: {
                if(0 == __read_gpio_level(gpio_no)) {
                    key_mag.p_tbl[i].status = KEY_DOWNNING;
                }else {
                    key_mag.p_tbl[i].status = KEY_DOWN;
                    key_mag.p_tbl[i].down_time = 0;
                }
            }
            break;
            
            case KEY_DOWNNING: {
                if(0 == __read_gpio_level(gpio_no)) {
                    if(0 == key_mag.p_tbl[i].down_time) {
                        key_mag.p_tbl[i].down_time = (key_mag.timer_space)*2;
                    }else {
                        key_mag.p_tbl[i].down_time += (key_mag.timer_space);
                    }
                }else {
                    key_mag.p_tbl[i].status = KEY_UP_CONFIRM;
                    key_mag.p_tbl[i].up_time = 0;
                }
            }
            break;
            
            case KEY_UP_CONFIRM: {
                if(__read_gpio_level(gpio_no)) {
                    key_mag.p_tbl[i].status = KEY_UPING;
                }else {
                    key_mag.p_tbl[i].down_time += (key_mag.timer_space)*2;
                    key_mag.p_tbl[i].status = KEY_DOWNNING;
                }
            }
            break;

            case KEY_UPING: {
                if(__read_gpio_level(gpio_no)) {
                    if(0 == key_mag.p_tbl[i].up_time) {
                        key_mag.p_tbl[i].up_time = (key_mag.timer_space)*2;
                    }else {
                        key_mag.p_tbl[i].up_time += key_mag.timer_space;
                    }
                    
                    if(key_mag.p_tbl[i].up_time >= SEQ_KEY_DETECT_TIME) {
                        key_mag.p_tbl[i].status = KEY_FINISH;
                    }
                }else { // is seq key?
                    if(key_mag.p_tbl[i].up_time >= SEQ_KEY_DETECT_TIME) {
                        key_mag.p_tbl[i].status = KEY_FINISH;
                    }else {
                        key_mag.p_tbl[i].status = KEY_DOWN_CONFIRM;
                        key_mag.p_tbl[i].up_time = 0;
                        key_mag.p_tbl[i].seq_key_cnt++;
                    }
                }

                if(KEY_FINISH == key_mag.p_tbl[i].status) {
                    if(key_mag.p_tbl[i].seq_key_cnt) {
                        key_mag.p_tbl[i].seq_key_cnt++;
                    }
                }
            }
            break;

            case KEY_FINISH: {
                PUSH_KEY_TYPE_E type;
                
                if(key_mag.p_tbl[i].seq_key_cnt < 1) {
                    if(key_mag.p_tbl[i].down_time >= key_mag.p_tbl[i].long_key_time) {
                        type = LONG_KEY;
                    }else {
                        type = NORMAL_KEY;
                    }
                }else {
                    type = SEQ_KEY;
                }

                key_mag.p_tbl[i].call_back(key_mag.p_tbl[i].gpio_no,type,key_mag.p_tbl[i].seq_key_cnt);
                key_mag.p_tbl[i].status = KEY_DOWN;
            }
            break;

            default:
                break;
        }
    }
}





