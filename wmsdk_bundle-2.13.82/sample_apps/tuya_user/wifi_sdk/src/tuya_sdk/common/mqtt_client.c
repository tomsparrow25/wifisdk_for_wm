/***********************************************************
*  File: mqtt_client.c 
*  Author: nzy
*  Date: 20150526
***********************************************************/
#define __MQTT_CLIENT_GLOBALS
#include <wm_net.h>
#include "mqtt_client.h"
#include "com_struct.h"
#include "sys_adapter.h"

/***********************************************************
*************************micro define***********************
***********************************************************/
#define MQTT_ALIVE_TIME_S 30

// mqtt ×´Ì¬»úÖ´ÐÐ
typedef enum {
    MQTT_GET_SERVER_IP = 0,
    MQTT_INIT,
    MQTT_CONNECT,
    MQTT_SUBSCRIBE,
    MQTT_REC_MSG,
}MQTT_STATUS_MACHINE;

typedef enum {
   MQTT_PARSE_CONNECT = 0,
   MQTT_PARSE_SUBSCRIBE,
   //MQTT_PARSE_MSG_PUBLISH,
}MQTT_PARSE_TYPE;

#define MQTT_HEAD_SIZE      4

typedef struct {
    mqtt_broker_handle_t mq;
    CHAR topic[GW_ID_LEN+1]; // use gwid
    CHAR serv_ip[16];
    USHORT serv_port;
    MQ_CALLBACK *callback;
    MUTEX_HANDLE mutex;
    THREAD thread;
    TIMER_ID alive_timer; // for alive 
    INT is_start;
}MQ_CNTL_S;

static os_thread_stack_define(mq_stack, 512);

/***********************************************************
*************************variable define********************
***********************************************************/
STATIC MQ_CNTL_S mq_cntl;

/***********************************************************
*************************function define********************
***********************************************************/
/***********************************************************
*  Function: mqtt_client_init
*  Input: alive second
*  Output: 
*  Return: none
***********************************************************/
VOID mq_client_init(IN CONST CHAR *clientid,\
                    IN CONST CHAR *username,\
                    IN CONST CHAR *password,\
                    IN CONST INT alive)
{
    memset(&mq_cntl,0,sizeof(mq_cntl));
    mqtt_init(&mq_cntl.mq,clientid);
    mqtt_init_auth(&mq_cntl.mq,username,password);

    INT tmp_alive = alive;
    if(0 == tmp_alive) {
        tmp_alive = MQTT_ALIVE_TIME_S;
    }
    mqtt_set_alive(&mq_cntl.mq,tmp_alive);
}

/***********************************************************
*  Function: set_mq_serv_info
*  Input: 
*  Output: 
*  Return: OPERATE_RET
***********************************************************/
STATIC OPERATE_RET set_mq_serv_info(IN CONST CHAR *ip,\
                                    IN USHORT port)
{
    if(NULL == ip) {
        return OPRT_INVALID_PARM;
    }

    strcpy(mq_cntl.serv_ip,ip);
    mq_cntl.serv_port = port;

    return OPRT_OK;
}

static int mq_send(void* socket_info, const void* buf, unsigned int count)
{
    int fd;
    fd = *((int *)socket_info);

    return send(fd,buf,count,0);
}

static VOID mq_close(VOID)
{
    int fd;
    fd = *((int *)mq_cntl.mq.socket_info);
    
    close(fd);
    mq_cntl.mq.socket_info = NULL;
}

/***********************************************************
*  Function: mq_client_sock_conn
*  Input: 
*  Output: 
*  Return: none
***********************************************************/
STATIC OPERATE_RET mq_client_sock_conn()
{
    int fd;
    fd = socket(AF_INET, SOCK_STREAM, 0);
    if(fd == -1) {
        return OPRT_SOCK_ERR;
    }

    int flag = 1;
    // reuse socket port
    if(setsockopt(fd,SOL_SOCKET,SO_REUSEADDR,(const char*)&flag,sizeof(int))) {
        close(fd);
        return OPRT_SET_SOCK_ERR;
    }

    // Disable Nagle Algorithm
    if (setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, (char*)&flag, sizeof(flag)) < 0)
    {
        close(fd);
        return OPRT_SET_SOCK_ERR;
    }

    struct sockaddr_in socket_address;
    // Create the stuff we need to connect
    socket_address.sin_family = AF_INET;
    socket_address.sin_port = htons(mq_cntl.serv_port);
    socket_address.sin_addr.s_addr = inet_addr(mq_cntl.serv_ip);

    // Connect the socket
    if((connect(fd, (struct sockaddr*)&socket_address, sizeof(socket_address))) < 0)
    {
        close(fd);
        return OPRT_SOCK_CONN_ERR;
    }

    // MQTT stuffs
    mq_cntl.mq.socket_info = (void*)&fd;
    mq_cntl.mq.send = mq_send;

    return OPRT_OK;
}

static void mq_alive_timer_cb(os_timer_arg_t arg)
{
    
}

static void mq_ctrl_task(os_thread_arg_t arg)
{
	int ret;

	while (1) {
        

        
	}
}

/***********************************************************
*  Function: mq_client_start
*  Input: 
*  Output: 
*  Return: none
***********************************************************/
OPERATE_RET mq_client_start(IN CONST CHAR *topic,MQ_CALLBACK *callback)
{
    if(NULL == topic || \
       NULL == callback) {
        return OPRT_INVALID_PARM;
    }

    if(mq_cntl.is_start) {
        return OPRT_OK;
    }

    strcpy(mq_cntl.topic,topic);
    mq_cntl.callback = callback;

    int ret;
    ret = os_mutex_create(&mq_cntl.mutex, "mq_mutex", 1);
    if(ret != WM_SUCCESS) {
        return OPRT_CR_MUTEX_ERR;
    }

    ret = os_timer_create(&mq_cntl.alive_timer, "alive_timer", \
                          os_msec_to_ticks(mq_cntl.mq.alive*1000),
		                  mq_alive_timer_cb, NULL,\
		                  OS_TIMER_PERIODIC, OS_TIMER_NO_ACTIVATE);
    if(ret != WM_SUCCESS) {
        return OPRT_CR_TIMER_ERR;
    }

    ret = os_thread_create(&mq_cntl.thread,"mq_cntl_task",\
			               mq_ctrl_task,0, &mq_stack, OS_PRIO_2);
    if(ret != WM_SUCCESS) {
        return OPRT_CR_THREAD_ERR;
    }
    mq_cntl.is_start = 1;

    return OPRT_OK;
}


