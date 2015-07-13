/***********************************************************
*  File: mqtt_client.c 
*  Author: nzy
*  Date: 20150526
***********************************************************/
#define __MQTT_CLIENT_GLOBALS
#include <wm_net.h>
#include "mqtt_client.h"
#include "sysdata_adapter.h"
#include "cJSON.h"

/***********************************************************
*************************micro define***********************
***********************************************************/
#define MQTT_ALIVE_TIME_S 30
#define RESP_TIMEOUT 5 // sec
#define PRE_TOPIC "smart/gw/"
#define PRE_TOPIC_LEN 9

// mqtt 状态机执行
typedef enum {
    MQTT_GET_SERVER_IP = 0,
    MQTT_SOCK_CONN,
    MQTT_CONNECT,
    MQ_CONN_RESP,
    MQTT_SUBSCRIBE,
    MQ_SUB_RESP,
    MQTT_REC_MSG,
}MQTT_STATUS_MACHINE;

typedef enum {
   MQTT_PARSE_CONNECT = 0,
   MQTT_PARSE_SUBSCRIBE,
   //MQTT_PARSE_MSG_PUBLISH,
}MQTT_PARSE_TYPE;

#define MQTT_HEAD_SIZE 4
#define MQ_RECV_BUF 1024

typedef struct {
    mqtt_broker_handle_t mq;
    CHAR topic[PRE_TOPIC_LEN+GW_ID_LEN+1]; // use gwid
    CHAR serv_ip[16];
    USHORT serv_port;
    MQ_CALLBACK callback;
    MUTEX_HANDLE mutex;
    THREAD thread;
    TIMER_ID alive_timer; // for alive
    TIMER_ID resp_timer; // for respond timeout
    INT is_start;
    MQTT_STATUS_MACHINE status;
    BYTE recv_buf[MQ_RECV_BUF];
    USHORT recv_out;
    USHORT remain_len;    
    BYTE topic_msg_buf[MQ_RECV_BUF];
}MQ_CNTL_S;

static os_thread_stack_define(mq_stack, 2048);

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
    if(NULL == socket_info) {
        PR_DEBUG("err");
        return -1;
    }

    int fd;
    fd = *((int *)socket_info);

    return send(fd,buf,count,0);
}

STATIC INT mq_recv_block(BYTE *buf,INT count)
{
    if(NULL == mq_cntl.mq.socket_info) {
        return -1;
    }

    int fd;
    fd = *((int *)mq_cntl.mq.socket_info);

    return recv(fd,buf,count,0);
}

VOID mq_close(VOID)
{
    os_mutex_get(&mq_cntl.mutex, OS_WAIT_FOREVER);
    if(mq_cntl.mq.socket_info) {
        int fd;
        fd = *((int *)mq_cntl.mq.socket_info);
        close(fd);
        mq_cntl.mq.socket_info = NULL;
    }
    os_mutex_put(&mq_cntl.mutex);
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
    if(setsockopt(fd,SOL_SOCKET,SO_REUSEADDR,(const char*)&flag,sizeof(int)) < 0) {
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

    // set block 
	int flags = fcntl(fd, F_GETFL, 0);
	if(fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0) {
        close(fd);
        return OPRT_SET_SOCK_ERR;
    }
    
    return OPRT_OK;
}

static void mq_alive_timer_cb(os_timer_arg_t arg)
{
    INT ret;

    ret = mqtt_ping(&mq_cntl.mq);
    if(ret != 1) {
        mq_close();
        return;
    }

    os_timer_change(&mq_cntl.resp_timer,os_msec_to_ticks((mq_cntl.mq.alive-5)*1000),0);
    os_timer_activate(&mq_cntl.resp_timer);
}

static void mq_resp_timer_cb(os_timer_arg_t arg)
{
    mq_close();
}

#ifdef MQ_DOMAIN_NAME
STATIC BOOL domain2ip(IN CONST CHAR *domain,OUT CHAR *ip)
{
	struct hostent* h;

	if((h = gethostbyname(domain))==NULL) {
		return TRUE;
	}	

	sprintf(ip,"%s",inet_ntoa(*((struct in_addr *)h->h_addr)));
	return FALSE;
}
#endif

static void mq_ctrl_task(os_thread_arg_t arg)
{
	OPERATE_RET op_ret;
    USHORT msg_id;
    INT ret;

	while (1) {
        //  the network is not ready
        if(get_wf_gw_status() != STAT_STA_CONN || \
           get_gw_status() < STAT_WORK) {
            os_thread_sleep(os_msec_to_ticks(3000));
            continue;
        }

        // PR_DEBUG("mq_cntl.status:%d",mq_cntl.status);
        switch(mq_cntl.status) {
            case MQTT_GET_SERVER_IP: {
                // check the alive is close
                if(WM_SUCCESS == os_timer_is_active(&mq_cntl.alive_timer)) {
                    os_timer_deactivate(&mq_cntl.alive_timer);
                }

                CHAR ip[16];
                #ifdef MQ_DOMAIN_NAME
                CHAR *mq_url = MQ_DOMAIN_ADDR;
                CHAR *mq_url_bak = MQ_DOMAIN_ADDR1;

                GW_CNTL_S *gw_cntl = get_gw_cntl();
                if(gw_cntl->active.mq_url[0]) {
                    mq_url = gw_cntl->active.mq_url;
                }else {
                    if(gw_cntl->active.mq_url_bak[0]) {
                        mq_url = gw_cntl->active.mq_url_bak;
                    }
                }

                if(gw_cntl->active.mq_url_bak[0]) {
                    mq_url_bak = gw_cntl->active.mq_url_bak;
                }else {
                    if(gw_cntl->active.mq_url[0]) {
                        mq_url_bak = gw_cntl->active.mq_url;
                    }
                }

                STATIC INT who_fir = 1;
                if(who_fir) {
                    if(0 != domain2ip(mq_url,ip)) {
                        if(0 != domain2ip(mq_url_bak,ip)) {
                            os_thread_sleep(os_msec_to_ticks(3000));
                            continue;
                        }
                    }
                }else {
                    if(0 != domain2ip(mq_url_bak,ip)) {
                        if(0 != domain2ip(mq_url,ip)) {
                            os_thread_sleep(os_msec_to_ticks(3000));
                            continue;
                        }
                    }
                }
                who_fir = !who_fir;
                PR_DEBUG("who_fir:%d ip:%s",who_fir,ip);
                #else
                strcpy(ip,MQ_DOMAIN_ADDR);
                #endif

                set_mq_serv_info(ip,MQ_DOMAIN_PORT);
                mq_cntl.status = MQTT_SOCK_CONN;
            }
            break;

            case MQTT_SOCK_CONN: {
                op_ret = mq_client_sock_conn();
                if(OPRT_OK != op_ret) {
                    PR_ERR("op_ret:%d.",op_ret);
                    os_thread_sleep(os_msec_to_ticks(3000));
                    mq_cntl.status = MQTT_GET_SERVER_IP;
                    continue;
                }
                mq_cntl.status = MQTT_CONNECT;
            }
            break;

            case MQTT_CONNECT: 
            case MQTT_SUBSCRIBE: {
                if(MQTT_CONNECT == mq_cntl.status) {
                    ret = mqtt_connect(&mq_cntl.mq);
                    mq_cntl.status = MQ_CONN_RESP;
                }else {
                    ret = mqtt_subscribe(&mq_cntl.mq,mq_cntl.topic, &msg_id);
                    mq_cntl.status = MQ_SUB_RESP;
                }
                if(1 != ret) {
                    PR_ERR("ret:%d.",ret);
                    mq_close();
                    os_thread_sleep(os_msec_to_ticks(3000));
                    mq_cntl.status = MQTT_GET_SERVER_IP;
                    continue;
                }

                os_timer_change(&mq_cntl.resp_timer,os_msec_to_ticks(RESP_TIMEOUT*1000),0);
                os_timer_activate(&mq_cntl.resp_timer);
            }
            break;

            case MQ_CONN_RESP: 
            case MQ_SUB_RESP: {
                ret = mq_recv_block(mq_cntl.recv_buf,MQ_RECV_BUF);
                if(ret <= 0) {
                    if(EWOULDBLOCK == errno) {
                        os_thread_sleep(os_msec_to_ticks(10));
                        continue;
                    }
                    PR_ERR("ret:%d.",ret);
                    goto MQ_EXE_ERR;
                }

                BYTE msg_ack_type;
                if(MQ_CONN_RESP == mq_cntl.status) {
                    msg_ack_type = MQTT_MSG_CONNACK;
                }else {
                    msg_ack_type = MQTT_MSG_SUBACK;
                }

                if(msg_ack_type != MQTTParseMessageType(&mq_cntl.recv_buf[0])) {
                    PR_ERR("msg_ack_type:%d.",msg_ack_type);
                    goto MQ_EXE_ERR;
                }                

                if(MQ_CONN_RESP == mq_cntl.status) {
                    if(mq_cntl.recv_buf[3] != 0) {
                        PR_ERR("recv_buf[3]:%d",mq_cntl.recv_buf[3]);
                        goto MQ_EXE_ERR;
                    }

                    mq_cntl.status = MQTT_SUBSCRIBE;
                }else {
                    if(msg_id != mqtt_parse_msg_id(mq_cntl.recv_buf)) {
                        PR_ERR("msg_id:%d",msg_id);
                        goto MQ_EXE_ERR;
                    }

                    mq_cntl.status = MQTT_REC_MSG;
                    os_timer_activate(&mq_cntl.alive_timer);
                }

                if(WM_SUCCESS == os_timer_is_active(&mq_cntl.resp_timer)) {
                    os_timer_deactivate(&mq_cntl.resp_timer);
                }
                break;

                MQ_EXE_ERR:
                if(WM_SUCCESS == os_timer_is_active(mq_cntl.resp_timer)) {
                    os_timer_deactivate(&mq_cntl.resp_timer);
                }
                mq_close();
                os_thread_sleep(os_msec_to_ticks(3000));
                mq_cntl.status = MQTT_GET_SERVER_IP;
                continue;
            }
            break;

            case MQTT_REC_MSG: {
                // PR_DEBUG("mqtt connect success");                
                // 剩余数据COPY
                if(mq_cntl.recv_out && mq_cntl.remain_len) {
                    memcpy(mq_cntl.recv_buf,\
                           mq_cntl.recv_buf+mq_cntl.recv_out,\
                           mq_cntl.remain_len);
                }
                mq_cntl.recv_out = 0;

                // recv start
                ret = mq_recv_block(mq_cntl.recv_buf+mq_cntl.remain_len,\
                                    MQ_RECV_BUF-mq_cntl.remain_len);
                if(ret <= 0) {                    
                    if(EWOULDBLOCK == errno) {
                        os_thread_sleep(os_msec_to_ticks(10));
                        continue;
                    }
                    
                    PR_ERR("ret:%d",ret);
                    goto MQ_EXE_ERR;
                }
                
                // receive data success
                mq_cntl.remain_len += ret;

                //PR_DEBUG("ret:%d",ret);
                //PR_DEBUG("remain_len:%d",mq_cntl.remain_len);

                #define MQ_LEAST_DATA 2 
                while(mq_cntl.remain_len >= MQ_LEAST_DATA) {
                    INT head_size = 0;
                    INT remain_len = 0;
                    INT msg_len = 0;
                    INT multiplier = 1;
                    BYTE digit;

                    INT i;
                    for(i = 0;i < 4;i++) { // 可变长度最大4字节
                        digit = *(mq_cntl.recv_buf+mq_cntl.recv_out+1+i); // 首个字节标明消息类型
                        remain_len += (digit & 127)*multiplier;
                        multiplier *= 128;

                        head_size++;
                        if(0 == (digit & 0x80)) {
                            break;
                        }
                    }
                    msg_len = 1+head_size+remain_len;

                    if(msg_len > MQ_RECV_BUF) { 
                        goto MQ_RECV_ERR;
                    }
                    if(mq_cntl.remain_len < msg_len) { // 接收数据长度不够继续接收
                        break;
                    }

                    INT last_buf_out = mq_cntl.recv_out;
                    mq_cntl.recv_out += msg_len;
                    if(mq_cntl.remain_len >= msg_len) {
                        mq_cntl.remain_len -= msg_len;
                    }else {
                        mq_cntl.remain_len = 0;
                    }

                    // 判断消息类型
                    BYTE mq_msg_type;
                    mq_msg_type = MQTTParseMessageType(&(mq_cntl.recv_buf[last_buf_out]));            
                    if(mq_msg_type != MQTT_MSG_PUBLISH) { // 丢弃
                        if(mq_msg_type == MQTT_MSG_PINGRESP) { // receive ping resp
                            //PR_DEBUG("stop resp timer");
                            // stop resp timeout
                            os_timer_deactivate(&mq_cntl.resp_timer);
                        }
                        continue;
                    }

                    INT len = 0;
                    len = mqtt_parse_pub_topic(&mq_cntl.recv_buf[last_buf_out],mq_cntl.topic_msg_buf);
                    if(len == 0) {
                        continue;
                    }

                    mq_cntl.topic_msg_buf[len] = 0;
                    if(0 != strcasecmp(mq_cntl.topic,(CHAR *)mq_cntl.topic_msg_buf)) {
                        continue;
                    }

                    len = mqtt_parse_publish_msg((&mq_cntl.recv_buf[last_buf_out]), \
                                                  mq_cntl.topic_msg_buf);
                    if(0 == len) {
                        continue;
                    }
                    mq_cntl.topic_msg_buf[len] = 0; // for printf

                    #if 0
                    // parse protocol
                    cJSON *root = NULL;
                    root = cJSON_Parse((CHAR *)mq_cntl.topic_msg_buf);
                    if(NULL == root) {
                        
                        goto JSON_PROC_ERR;
                    }

                    cJSON *json = NULL;
                    // protocol
                    json = cJSON_GetObjectItem(root,"protocol");
                    if(NULL == json) {
                        goto JSON_PROC_ERR;
                    }
                    INT mq_pro;
                    mq_pro = json->valueint;

                    // data
                    json = cJSON_GetObjectItem(root,"data");
                    if(NULL == json) {
                        goto JSON_PROC_ERR;
                    }
                    if(PRO_CMD == mq_pro) {
                        BYTE *buf = (BYTE *)cJSON_PrintUnformatted(json);;
                        if(mq_cntl.callback) {
                            mq_cntl.callback(buf,strlen((CHAR *)buf)+1);
                        }
                        Free(buf);
                    }else if(PRO_ADD_USER == mq_pro) {
                        
                    }else if(PRO_DEL_USER == mq_pro) {
                        
                    }else if(PRO_FW_UG_CFM == mq_pro) {
                        
                    }else {
                        goto JSON_PROC_ERR;
                    }

                    cJSON_Delete(root);
                    continue;

                JSON_PROC_ERR:
                    PR_ERR("%s",mq_cntl.topic_msg_buf);
                    cJSON_Delete(root);
                    continue;
                #else
                    if(mq_cntl.callback) {
                        mq_cntl.callback(mq_cntl.topic_msg_buf,len-1);
                    }
                #endif
                }
                break;

                MQ_RECV_ERR:
                mq_close();
                os_timer_deactivate(&mq_cntl.alive_timer);
                mq_cntl.status = MQTT_GET_SERVER_IP;
                break;
            }
            break;
        }
    }
}

/***********************************************************
*  Function: mq_client_start
*  Input: 
*  Output: 
*  Return: none
***********************************************************/
OPERATE_RET mq_client_start(IN CONST CHAR *topic,IN CONST MQ_CALLBACK callback)
{
    if(NULL == topic || \
       NULL == callback) {
        return OPRT_INVALID_PARM;
    }

    if(mq_cntl.is_start) {
        return OPRT_OK;
    }

    snprintf(mq_cntl.topic,sizeof(mq_cntl.topic),"%s%s",PRE_TOPIC,topic);
    mq_cntl.callback = callback;

    int ret;
    ret = os_mutex_create(&mq_cntl.mutex, "mq_mutex", 1);
    if(ret != WM_SUCCESS) {
        return OPRT_CR_MUTEX_ERR;
    }

    ret = os_timer_create(&mq_cntl.alive_timer, "alive_timer", \
                          os_msec_to_ticks(mq_cntl.mq.alive*1000),\
		                  mq_alive_timer_cb, NULL,\
		                  OS_TIMER_PERIODIC, OS_TIMER_NO_ACTIVATE);
    if(ret != WM_SUCCESS) {
        return OPRT_CR_TIMER_ERR;
    }

    ret = os_timer_create(&mq_cntl.resp_timer, "resp_timer", \
                          os_msec_to_ticks(RESP_TIMEOUT*1000),\
		                  mq_resp_timer_cb, NULL,\
		                  OS_TIMER_ONE_SHOT, OS_TIMER_NO_ACTIVATE);
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


