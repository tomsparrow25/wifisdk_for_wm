/***********************************************************
*  File: app_agent.c 
*  Author: nzy
*  Date: 20150618
***********************************************************/
#define __APP_AGENT_GLOBALS
#include "app_agent.h"
#include "crc32.h"
#include <wmtime.h>

/***********************************************************
*************************micro define***********************
***********************************************************/
#define SERV_PORT_UDP 6666
#define SERV_PORT_TCP 6668
#define SERV_MAX_CONN 5

#pragma pack(1)
// lan protocol app head
typedef struct
{
    UINT head; // 0x55aa
    UINT fr_num;
    UINT fr_type;
    UINT len;
    BYTE data[0];
}LAN_PRO_HEAD_APP_S;

typedef struct {
    UINT crc;
    UINT tail; // 0xaa55
}LAN_PRO_TAIL_S;

// lan protocol gateway head 
typedef struct 
{
    UINT head; // 0x55aa
    UINT fr_num;
    UINT fr_type;
    UINT len;
    UINT ret_code;
    BYTE data[0];
}LAN_PRO_HEAD_GW_S;
#pragma pack()

#define UDP_T_ITRV 3 // s
#define CLIENT_LMT 5
#define RECV_BUF_LMT 1024
typedef struct {
    INT serv_fd;
    // fd_set readfds;
    INT c_fd_cnt;
    INT c_fd[CLIENT_LMT];
    time_t c_time[CLIENT_LMT];
    UINT fr_num;

    INT upd_fd;
    TIMER_ID udp_tr; // udp brocast timer
    MUTEX_HANDLE mutex;
    struct sockaddr_in udp_addr;
    // INT bc_ap_mode; // 1:ap 2:ap wf cfg ok 3:ap wf cfg failed

    TIMER_ID c_tm_tr; // client socket timeout timer
    BYTE recv_buf[RECV_BUF_LMT];

    INT is_start;
    THREAD thread;
}LAN_PRO_CNTL_S;

typedef enum {
    TSS_INIT = 0,
    TSS_CR_SERV,
    TSS_SELECT,
}TCP_SERV_STAT_E;
/***********************************************************
*************************variable define********************
***********************************************************/
STATIC LAN_PRO_CNTL_S lan_pro_cntl;
static os_thread_stack_define(lpc_stack, 1024);

/***********************************************************
*************************function define********************
***********************************************************/
static void udp_timer_cb(os_timer_arg_t arg);
static void lpc_task_cb(os_thread_arg_t arg);
STATIC CHAR *get_gw_cur_ip(GW_WIFI_STAT_E wf_stat);
STATIC BYTE *mlp_gw_send_da(IN CONST UINT fr_num,IN CONST UINT fr_type,\
                            IN CONST UINT ret_code,IN CONST BYTE *data,\
                            IN CONST UINT len,OUT UINT *s_len);
static int setup_tcp_serv_socket(int port);
static OPERATE_RET ag_select(int max_sock, const fd_set *readfds,
			                 fd_set *active_readfds, int timeout_secs,\
			                 int *actv_cnt);
STATIC VOID lpc_close_socket(IN CONST INT socket);
INT *get_lpc_sockets(VOID);
INT get_lpc_socket_num(VOID); 
STATIC VOID lpc_add_socket(IN CONST INT socket,IN CONST time_t time);
STATIC VOID up_socket_time(IN CONST INT socket,IN CONST time_t time);
STATIC VOID check_socket_tm(IN CONST time_t time);
extern OPERATE_RET smart_frame_recv_appmsg(IN CONST INT socket,\
                                           IN CONST UINT frame_num,\
                                           IN CONST UINT frame_type,\
                                           IN CONST CHAR *data,\
                                           IN CONST UINT len);


/***********************************************************
*  Function: lan_pro_cntl_init
*  Input: 
*  Output: 
*  Return: none
***********************************************************/
OPERATE_RET lan_pro_cntl_init(VOID)
{
    if(lan_pro_cntl.is_start) {
        return OPRT_OK;
    }

    memset(&lan_pro_cntl,0,sizeof(lan_pro_cntl));

    int ret;
    // create timer
    ret = os_timer_create(&lan_pro_cntl.udp_tr, "udp_timer", \
                          os_msec_to_ticks(UDP_T_ITRV*1000),\
                          udp_timer_cb, NULL,\
                          OS_TIMER_PERIODIC, OS_TIMER_AUTO_ACTIVATE);
    if(ret != WM_SUCCESS) {
        return OPRT_CR_TIMER_ERR;
    }

    // mutex
    ret = os_mutex_create(&lan_pro_cntl.mutex, "lpc_mutex", 1);
    if(ret != WM_SUCCESS) {
        return OPRT_CR_MUTEX_ERR;
    }

    ret = os_thread_create(&lan_pro_cntl.thread,"lpc_task",\
                           lpc_task_cb,0, &lpc_stack, OS_PRIO_2);
    if(ret != WM_SUCCESS) {
        return OPRT_CR_THREAD_ERR;
    }

    lan_pro_cntl.serv_fd = -1;
    lan_pro_cntl.upd_fd = -1;
    lan_pro_cntl.is_start = 1;

    return OPRT_OK;
}

STATIC CHAR *get_gw_cur_ip(GW_WIFI_STAT_E wf_stat)
{
    if(wf_stat == STAT_UNPROVISION || \
       wf_stat == STAT_STA_UNCONN) {
        return NULL;
    }

    int ret;
    struct wlan_network nw;
    
    if(STAT_STA_CONN == wf_stat) {
        if(WLAN_ERROR_NONE != (ret = wlan_get_current_network(&nw))) {
            PR_ERR("wlan get cur nw err:%d",ret);
            return NULL;
        }
    }else {
        if(WLAN_ERROR_NONE != (ret = wlan_get_current_uap_network(&nw))) {
            PR_ERR("wlan get cur uap nw err:%d",ret);
            return NULL;
        }
    }

    return inet_ntoa(nw.address.ip);
}

static void udp_timer_cb(os_timer_arg_t arg)
{
    GW_WIFI_STAT_E wf_stat = get_wf_gw_status();
    if(STAT_UNPROVISION == wf_stat || \
       STAT_STA_UNCONN == wf_stat) {
        return;
    }

    memset(&lan_pro_cntl.udp_addr,0,sizeof(lan_pro_cntl.udp_addr));        
    lan_pro_cntl.udp_addr.sin_family = AF_INET;
    if(wf_stat == STAT_STA_CONN) {
        lan_pro_cntl.udp_addr.sin_addr.s_addr = inet_addr("255.255.255.255");
    }else {
        // PR_DEBUG("ap mode");
        lan_pro_cntl.udp_addr.sin_addr.s_addr = inet_addr("192.168.10.255");
    }
    lan_pro_cntl.udp_addr.sin_port = htons(SERV_PORT_UDP); 

    // create upd fd
    if(lan_pro_cntl.upd_fd < 0) {
        lan_pro_cntl.upd_fd = socket(AF_INET, SOCK_DGRAM, 0);  
        if(lan_pro_cntl.upd_fd < 0) {
            PR_ERR("create socket err");
            return;
        }

        UINT so_broadcast = 1;
        setsockopt(lan_pro_cntl.upd_fd,SOL_SOCKET,SO_BROADCAST,(char*)&so_broadcast,SIZEOF(so_broadcast));
    }

    CHAR *ip = get_gw_cur_ip(wf_stat);
    if(NULL == ip) {
        PR_ERR("get gw ip error");
        return;
    }

    GW_CNTL_S *gw_cntl = get_gw_cntl();
    // make data
    INT ability = gw_cntl->gw.ability;
    INT mode = 0;
    if(wf_stat == STAT_STA_CONN) {
        mode = 0;
    }else {
        mode = 1;
    }
    GW_STAT_E gw_stat = get_gw_status();
    INT active = 0;
    if(gw_stat <= UN_ACTIVE) {
        active = 0;
    }else if(gw_stat == ACTIVE_RD) {
        active = 1;
    }else {
        active = 2;
    }

    cJSON *root = NULL;
    root=cJSON_CreateObject();
    if(NULL == root) {
        PR_ERR("cr json error");
        return;
    }

    cJSON_AddStringToObject(root,"ip",ip);
    cJSON_AddStringToObject(root,"gwId",gw_cntl->gw.id);
    cJSON_AddNumberToObject(root,"active",active);
    cJSON_AddNumberToObject(root,"ability",ability);
    cJSON_AddNumberToObject(root,"mode",mode);

    CHAR *buf = cJSON_PrintUnformatted(root);
    cJSON_Delete(root),root = NULL;

    UINT s_len;
    BYTE *send_buf = mlp_gw_send_da(0,0,0,(BYTE *)buf,strlen(buf),&s_len);
    Free(buf);
    if(NULL == send_buf) {
        PR_ERR("mlp_gw_send_da error");
        return;
    }

    int ret = sendto(lan_pro_cntl.upd_fd,send_buf,s_len,0,\
                     (struct sockaddr*)&lan_pro_cntl.udp_addr, SIZEOF(lan_pro_cntl.udp_addr));
    Free(send_buf);
    if(ret < 0)
    {
        PR_ERR("sendto Failed,ret:%d,errno:%d",ret,errno);
        close(lan_pro_cntl.upd_fd),lan_pro_cntl.upd_fd = -1;
        return;
    }
}

static int setup_tcp_serv_socket(int port)
{
	int one = 1;
	int status, addr_len, sockfd;
	struct sockaddr_in addr_listen;

	/* create listening TCP socket */
	sockfd = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (sockfd < 0) {
		PR_ERR("Socket creation failed: Port:%d errno:%d",
			   port, errno);
		return -1;
	}

	setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR,
		       (char *)&one, sizeof(one));
	addr_listen.sin_family = AF_INET;
	addr_listen.sin_port = htons(port);
	addr_listen.sin_addr.s_addr = htonl(INADDR_ANY);
	addr_len = sizeof(struct sockaddr_in);

	/* bind insocket */
	status = bind(sockfd, (struct sockaddr *)&addr_listen,addr_len);
	if (status < 0) {
		PR_ERR("Failed to bind socket on port:%d errno:%d",port,errno);
        close(sockfd);
		return -1;
	}

	status = listen(sockfd, SERV_MAX_CONN);
	if (status < 0) {
		PR_ERR("Failed to listen on port:%d,errno:%d.", port, errno);
        close(sockfd);
		return -1;
	}

	return sockfd;
}

static OPERATE_RET ag_select(int max_sock, const fd_set *readfds,\
			                 fd_set *active_readfds, int timeout_secs,\
			                 int *actv_cnt)
{
	int activefds_cnt;
	struct timeval timeout;

	fd_set local_readfds;
	if (timeout_secs >= 0)
		timeout.tv_sec = timeout_secs;
	timeout.tv_usec = 0;

	memcpy(&local_readfds, readfds, sizeof(fd_set));
	activefds_cnt = select(max_sock + 1, &local_readfds,
				           NULL, NULL,timeout_secs >= 0 ? &timeout : NULL);
	if (activefds_cnt < 0) {
		PR_ERR("Select failed: %d,errno:%d", errno);
		return OPRT_SELECT_ERR;
	}

	if (activefds_cnt) {
		/* Update users copy of fd_set only if he wants */
		if (active_readfds)
			memcpy(active_readfds, &local_readfds,
			       sizeof(fd_set));
        *actv_cnt = activefds_cnt;
		return OPRT_OK;
	}

	return OPRT_SELECT_TM;
}

static void lpc_task_cb(os_thread_arg_t arg)
{
    PR_DEBUG("lpc task start");

    TCP_SERV_STAT_E status = TSS_INIT;
    INT max_sockfd = -1;

    while(1) {
        // PR_DEBUG("status:%d",status);
        switch(status) {
            case TSS_INIT: {
                GW_WIFI_STAT_E wf_stat = get_wf_gw_status();
                if(STAT_UNPROVISION == wf_stat || \
                   STAT_STA_UNCONN == wf_stat) {
                    os_thread_sleep(os_msec_to_ticks(1500));
                    continue;
                }

                status = TSS_CR_SERV;
            }
            break;

            case TSS_CR_SERV: {
                lan_pro_cntl.serv_fd = setup_tcp_serv_socket(SERV_PORT_TCP);
                if(lan_pro_cntl.serv_fd < 0) {
                    PR_ERR("create server socket err");
                    os_thread_sleep(os_msec_to_ticks(1500));
                    status = TSS_INIT;
                    continue;
                }

                status = TSS_SELECT;
                max_sockfd = lan_pro_cntl.serv_fd;
            }
            break;

            case TSS_SELECT: {
                INT actv_cnt = 0;
                OPERATE_RET op_ret = OPRT_OK;
                fd_set active_readfds;
                fd_set readfds;
                INT i;

                FD_ZERO(&readfds);
                FD_ZERO(&active_readfds);
                
                // set readfd
                FD_SET(lan_pro_cntl.serv_fd,&readfds);
                for(i = 0;i < lan_pro_cntl.c_fd_cnt;i++) {
                    FD_SET(lan_pro_cntl.c_fd[i],&readfds);
                }

                op_ret = ag_select(max_sockfd, &readfds,\
                                   &active_readfds, -1,&actv_cnt);
                if(op_ret != OPRT_OK) {
                    PR_ERR("ag_select op_ret:%d errno:%d",op_ret,errno);
                    goto TSS_SELECT_ERR;
                }

                time_t time = wmtime_time_get_posix();
                check_socket_tm(time);
                
                #if 0
                PR_DEBUG("actv_cnt:%d",actv_cnt);
                #endif
                
                if(FD_ISSET(lan_pro_cntl.serv_fd,&active_readfds)) { // 
                    actv_cnt--;

                    INT cfd;
                    struct sockaddr_in addr_from;
                    unsigned long addr_from_len = sizeof(addr_from);
                    cfd = accept(lan_pro_cntl.serv_fd, (struct sockaddr *)&addr_from,\
                                 &addr_from_len);
                    if (cfd <= 0) {
                        PR_ERR("net_accept client socket failed %d (errno: %d).",
                                cfd, errno);
                        break;
                    }
                    
                    // out of limit
                    if(get_lpc_socket_num() >= CLIENT_LMT) {
                        PR_ERR("out of client limit");
                        close(cfd);
                        break;
                    }

                    if(cfd > max_sockfd) {
                        max_sockfd = cfd;
                    }

                    // set no block 
                    int flags = fcntl(cfd, F_GETFL, 0);
                    fcntl(cfd, F_SETFL, flags | O_NONBLOCK);

                    // add socket
                    lpc_add_socket(cfd,time);
                    PR_DEBUG("client nums:%d cfd:%d ip:%s",\
                             get_lpc_socket_num(),cfd,inet_ntoa(addr_from.sin_addr));
                }
                
                if(actv_cnt) {
                    for(i = 0;i < lan_pro_cntl.c_fd_cnt;i++) {
                        if(FD_ISSET(lan_pro_cntl.c_fd[i],&active_readfds)) {
                            // update time
                            up_socket_time(lan_pro_cntl.c_fd[i],time);

                            // recv data
                            int recv_data;
                            recv_data = read(lan_pro_cntl.c_fd[i],lan_pro_cntl.recv_buf,RECV_BUF_LMT);
                            if(recv_data <= 0) {
                                PR_DEBUG("close socket");
                                lpc_close_socket(lan_pro_cntl.c_fd[i]);
                                break;
                            }

                            #if 0
                            INT j;
                            PR_DEBUG("recv_data len:%d",recv_data);
                            for(j = 0;j < recv_data;j++) {
                                PR_DEBUG_RAW("%02x ",lan_pro_cntl.recv_buf[j]);
                            }
                            PR_DEBUG_RAW("\r\n");
                            #endif

                            int mini_size = (sizeof(LAN_PRO_TAIL_S) + sizeof(LAN_PRO_HEAD_APP_S));
                            // recv data process
                            if(recv_data < mini_size) {
                                PR_ERR("not enough data");
                                break;
                            }
                            
                            UINT offset = 0;
                            while((recv_data-offset) >= mini_size) {
                                LAN_PRO_HEAD_APP_S *head = (LAN_PRO_HEAD_APP_S *)(lan_pro_cntl.recv_buf + offset);
                                if(0x55aa != DWORD_SWAP(head->head)) {
                                    offset += 1;
                                    continue;
                                }
                                
                                UINT frame_len = sizeof(LAN_PRO_HEAD_APP_S) + DWORD_SWAP(head->len);
                                if(frame_len > (recv_data-offset)) { // recv data not enough
                                    PR_ERR("frame len:%d",frame_len);
                                    PR_ERR("remain len:%d",(recv_data-offset));
                                    break;
                                }
                                
                                UINT crc = crc32(head,(frame_len-sizeof(LAN_PRO_TAIL_S)),0);
                                offset += (frame_len- sizeof(LAN_PRO_TAIL_S));
                                LAN_PRO_TAIL_S *tail = (LAN_PRO_TAIL_S *)(lan_pro_cntl.recv_buf+offset);
                                #if 0
                                if(crc != DWORD_SWAP(tail->crc) || \
                                   0xaa55 != DWORD_SWAP(tail->tail)) {
                                    PR_ERR("crc :0x%x",crc);
                                    PR_ERR("recv crc :0x%x",DWORD_SWAP(tail->crc));
                                    PR_ERR("recv tail:0x%x",DWORD_SWAP(tail->tail));
                                    offset += 1;
                                    continue;
                                }
                                #else
                                crc = crc; // avoid warnning
                                if(0xaa55 != DWORD_SWAP(tail->tail)) {
                                    PR_ERR("recv tail:0x%x",DWORD_SWAP(tail->tail));
                                    offset += 1;
                                    continue;
                                }
                                #endif
                                offset += sizeof(LAN_PRO_TAIL_S);

                                OPERATE_RET op_ret;
                                // heart beat
                                if(DWORD_SWAP(head->fr_type) == FRM_TP_HB) {
                                    op_ret = mlp_gw_tcp_send(lan_pro_cntl.c_fd[i],0,\
                                                             FRM_TP_HB,0,NULL,0);
                                    if(OPRT_OK != op_ret) {
                                        PR_ERR("mlp_gw_tcp_send op_ret:%d",op_ret);
                                    }
                                    continue;
                                }

                                op_ret = smart_frame_recv_appmsg(lan_pro_cntl.c_fd[i],DWORD_SWAP(head->fr_num),\
                                                                 DWORD_SWAP(head->fr_type),(CHAR *)head->data,\
                                                                 (DWORD_SWAP(head->len)-sizeof(LAN_PRO_TAIL_S)));
                                if(OPRT_OK != op_ret) {
                                    PR_ERR("op_ret:%d",op_ret);
                                }
                            }
                        }
                    }
                }
                break;

                TSS_SELECT_ERR:
                os_thread_sleep(os_msec_to_ticks(1500));
                lpc_close_all_socket();
                status = TSS_INIT;
                continue;
            }
            break;
        }
    }
}

VOID lpc_close_all_socket(VOID)
{
    INT i;

    os_mutex_get(&lan_pro_cntl.mutex, OS_WAIT_FOREVER);
    if(lan_pro_cntl.serv_fd != -1) {
        close(lan_pro_cntl.serv_fd),lan_pro_cntl.serv_fd = -1;
    }

    for(i = 0; i < lan_pro_cntl.c_fd_cnt;i++) {
        if(lan_pro_cntl.c_fd[i] != -1) {
            close(lan_pro_cntl.c_fd[i]),lan_pro_cntl.c_fd[i] = -1;
        }
    }
    lan_pro_cntl.c_fd_cnt = 0;
    os_mutex_put(&lan_pro_cntl.mutex);
}

STATIC VOID lpc_close_socket(IN CONST INT socket)
{
    if(0 == lan_pro_cntl.c_fd_cnt || \
       socket < 0) {
        return;
    }

    INT i;
    os_mutex_get(&lan_pro_cntl.mutex, OS_WAIT_FOREVER);
    for(i = 0;i < lan_pro_cntl.c_fd_cnt;i++) {
        if(socket == lan_pro_cntl.c_fd[i]) {
            break;
        }
    }

    // not found
    if(i >= lan_pro_cntl.c_fd_cnt) {
        os_mutex_put(&lan_pro_cntl.mutex);
        return;
    }

    if(lan_pro_cntl.c_fd[i] != -1) {
        close(lan_pro_cntl.c_fd[i]),lan_pro_cntl.c_fd[i] = -1;
    }

    INT j = 0;
    // move ahead
    for(j = i+1;j < lan_pro_cntl.c_fd_cnt;j++) {
        lan_pro_cntl.c_fd[j-1] = lan_pro_cntl.c_fd[j];
        lan_pro_cntl.c_time[j-1] = lan_pro_cntl.c_time[j];
    }
    lan_pro_cntl.c_fd_cnt--;
    os_mutex_put(&lan_pro_cntl.mutex);
}

STATIC VOID lpc_add_socket(IN CONST INT socket,IN CONST time_t time)
{
    if((lan_pro_cntl.c_fd_cnt >= CLIENT_LMT)|| \
       socket < 0) {
        return;
    }

    os_mutex_get(&lan_pro_cntl.mutex, OS_WAIT_FOREVER);
    lan_pro_cntl.c_fd[lan_pro_cntl.c_fd_cnt] = socket;
    lan_pro_cntl.c_time[lan_pro_cntl.c_fd_cnt] = time;
    lan_pro_cntl.c_fd_cnt++;
    os_mutex_put(&lan_pro_cntl.mutex);
}

STATIC VOID up_socket_time(IN CONST INT socket,IN CONST time_t time)
{
    if((lan_pro_cntl.c_fd_cnt >= CLIENT_LMT)|| \
       socket < 0) {
        return;
    }

    INT i;
    os_mutex_get(&lan_pro_cntl.mutex, OS_WAIT_FOREVER);
    for(i = 0;i < lan_pro_cntl.c_fd_cnt;i++) {
        if(socket == lan_pro_cntl.c_fd[i]) {
            break;
        }
    }

    // not found
    if(i >= lan_pro_cntl.c_fd_cnt) {
        os_mutex_put(&lan_pro_cntl.mutex);
        return;
    }

    lan_pro_cntl.c_time[i] = time;
    os_mutex_put(&lan_pro_cntl.mutex);
}

STATIC VOID check_socket_tm(IN CONST time_t time)
{
    INT close_fd[CLIENT_LMT];
    INT cnt = 0;
    INT i;
    os_mutex_get(&lan_pro_cntl.mutex, OS_WAIT_FOREVER);
    for(i = 0;i < lan_pro_cntl.c_fd_cnt;i++) {
        if((time - lan_pro_cntl.c_time[i]) >= 2592000) { // 1 month sencond
            continue;
        }else if(time - lan_pro_cntl.c_time[i] >= 30) {
            close_fd[cnt++] = lan_pro_cntl.c_fd[i];
        }
    }
    os_mutex_put(&lan_pro_cntl.mutex);

    if(0 == cnt) {
        return;
    }

    for(i = 0;i < cnt;i++) {
        PR_DEBUG("time out close socket:%d",close_fd[i]);
        lpc_close_socket(close_fd[i]);
    }
}


INT get_lpc_socket_num(VOID) 
{
    INT num;

    os_mutex_get(&lan_pro_cntl.mutex, OS_WAIT_FOREVER);
    num = lan_pro_cntl.c_fd_cnt;
    os_mutex_put(&lan_pro_cntl.mutex);

    return num;
}

INT *get_lpc_sockets(VOID)
{
    return lan_pro_cntl.c_fd;
}

/***********************************************************
*  Function: make lan protocol gateway send data,need Free(data)
*  Input: 
*  Output: 
*  Return: none
***********************************************************/
STATIC BYTE *mlp_gw_send_da(IN CONST UINT fr_num,IN CONST UINT fr_type,\
                            IN CONST UINT ret_code,IN CONST BYTE *data,\
                            IN CONST UINT len,OUT UINT *s_len)
{
    UINT send_da_len = sizeof(LAN_PRO_HEAD_GW_S) + len + sizeof(LAN_PRO_TAIL_S);
    BYTE *send_da = Malloc(send_da_len);
    if(send_da == NULL) {
        return NULL;
    }

    LAN_PRO_HEAD_GW_S *head_gw = (LAN_PRO_HEAD_GW_S *)send_da;
    head_gw->head = DWORD_SWAP(0x55aa);
    head_gw->fr_num = DWORD_SWAP(fr_num);
    head_gw->fr_type = DWORD_SWAP(fr_type);
    head_gw->len = DWORD_SWAP(len+sizeof(LAN_PRO_TAIL_S)+sizeof(ret_code));
    head_gw->ret_code = DWORD_SWAP(ret_code);
    memcpy(head_gw->data,data,len);

    UINT crc = 0;
    crc = crc32(send_da,(send_da_len-sizeof(LAN_PRO_TAIL_S)),0);

    LAN_PRO_TAIL_S *pro_tail = (LAN_PRO_TAIL_S *)(send_da + sizeof(LAN_PRO_HEAD_GW_S) + len);
    pro_tail->crc = DWORD_SWAP(crc);
    pro_tail->tail = DWORD_SWAP(0xaa55);
    *s_len = send_da_len;
    
    return send_da;
}

/***********************************************************
*  Function: mlp_gw_tcp_send
*  Input: 
*  Output: 
*  Return: OPERATE_RET
***********************************************************/
OPERATE_RET mlp_gw_tcp_send(INT socket,IN CONST UINT fr_num,\
                            IN CONST UINT fr_type,IN CONST UINT ret_code,\
                            IN CONST BYTE *data,IN CONST UINT len)
{
    BYTE *send_buf;
    UINT send_len;

    send_buf = mlp_gw_send_da(fr_num,fr_type,ret_code,data,len,&send_len);
    if(NULL == send_buf) {
        return OPRT_COM_ERROR;
    }

    int ret = send(socket,send_buf,send_len,0);
    Free(send_buf);
    if(ret <= 0 || \
       ret != send_len) {
       lpc_close_socket(socket);
       return OPRT_SEND_ERR;
    }

    return OPRT_OK;
}

