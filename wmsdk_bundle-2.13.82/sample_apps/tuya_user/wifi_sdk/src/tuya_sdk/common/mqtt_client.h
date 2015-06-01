/***********************************************************
*  File: mqtt_client.h 
*  Author: nzy
*  Date: 20150526
***********************************************************/
#ifndef _MQTT_CLIENT_H
    #define _MQTT_CLIENT_H

    #include "com_def.h"
    #include "libemqtt.h"
    #include "sys_adapter.h"
    #include "error_code.h"
    #include "com_struct.h"

#ifdef __cplusplus
	extern "C" {
#endif

#ifdef  __MQTT_CLIENT_GLOBALS
    #define __MQTT_CLIENT_EXT
#else
    #define __MQTT_CLIENT_EXT extern
#endif

/***********************************************************
*************************micro define***********************
***********************************************************/
typedef VOID (*MQ_CALLBACK)(BYTE *data,UINT len);

#define MQ_DOMAIN_NAME
#ifdef MQ_DOMAIN_NAME // ”Ú√˚
#define MQ_DOMAIN_ADDR "tuya.mqdefault"
#define MQ_DOMAIN_PORT 8888
#else
#define MQ_DOMAIN_ADDR "192.168.0.1"
#define MQ_DOMAIN_PORT 8888
#endif

/***********************************************************
*************************variable define********************
***********************************************************/


/***********************************************************
*************************function define********************
***********************************************************/
/***********************************************************
*  Function: mqtt_client_init
*  Input: alive second
*  Output: 
*  Return: none
***********************************************************/
__MQTT_CLIENT_EXT \
VOID mq_client_init(IN CONST CHAR *clientid,\
                    IN CONST CHAR *username,\
                    IN CONST CHAR *password,\
                    IN CONST INT alive);

/***********************************************************
*  Function: mq_client_start
*  Input: 
*  Output: 
*  Return: none
***********************************************************/
__MQTT_CLIENT_EXT \
OPERATE_RET mq_client_start(IN CONST CHAR *topic,IN CONST MQ_CALLBACK callback);



#ifdef __cplusplus
}
#endif
#endif

