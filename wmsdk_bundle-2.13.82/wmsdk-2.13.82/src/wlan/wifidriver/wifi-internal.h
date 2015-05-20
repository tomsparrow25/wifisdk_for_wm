#ifndef __WIFI_INTERNAL_H__
#define __WIFI_INTERNAL_H__

#include <mlan_wmsdk.h>

#include <wm_os.h>
#include <wifi_events.h>
#include <wifi-decl.h>

typedef struct mcast_filter {
	uint8_t mac_addr[MLAN_MAC_ADDR_LENGTH];
	struct mcast_filter *next;
} mcast_filter;

typedef struct {
	os_thread_t wm_wifi_main_thread;
	os_queue_t *wlc_mgr_event_queue;
#ifdef CONFIG_P2P
	os_queue_t *wfd_event_queue;
#endif
	os_mutex_t command_lock;
	os_semaphore_t command_resp_sem;
	os_mutex_t mcastf_mutex;

	unsigned last_sent_cmd_msec;

	/* Queue for events/data from low level interface driver */
	os_queue_t io_events;
	os_queue_pool_t io_events_queue_data;

	mcast_filter *start_list;

	/*
	 * Usage note:
	 * There are a number of API's (for e.g. wifi_get_antenna()) which
	 * return some data in the buffer passed by the caller. Most of the
	 * time this data needs to be retrived from the firmware. This
	 * retrival happens in a different thread context. Hence, we need
	 * to store the buffer pointer passed by the user at a shared
	 * location. This pointer to used for this purpose.
	 *
	 * Note to the developer: Please ensure to set this to NULL after
	 * use in the wifi driver thread context.
	 */
	void *cmd_resp_priv;
	/*
	 * In continuation with the description written for the
	 * cmd_resp_priv member above, the below member indicates the
	 * result of the retrieval operation from the firmware.
	 */
	int cmd_resp_status;

#ifdef CONFIG_11D
	/*
	 * This is updated when user calls the wifi_uap_set_domain_params()
	 * functions. This is used later during uAP startup. Since the uAP
	 * configuration needs to be done befor uAP is started we keep this
	 * cache. This is needed to enable 11d support in uAP.
	 */
	MrvlIEtypes_DomainParamSet_t *dp;
#endif /* CONFIG_11D */
} wm_wifi_t;

extern wm_wifi_t wm_wifi;

struct bus_message {
	uint16_t event;
	uint16_t reason;
	void *data;
};

/**
 * This function handles events received from the firmware.
 */
int wifi_handle_fw_event(struct bus_message *msg);

/**
 * This function is used to send events to the upper layer through the
 * message queue registered by the upper layer.
 */
void wifi_event_completion(int type, enum wifi_event_reason result,
				void *data);

/**
 * Use this function to know whether a split scan is in progress.
 */
bool is_split_scan_complete(void);

/**
 * Waits for Command processing to complete and waits for command response
 */
int wifi_wait_for_cmdresp(void *cmd_resp_priv);

/**
 * Register an event queue
 *
 * This queue is used to send events and command responses to the wifi
 * driver from the stack dispatcher thread.
 */
int bus_register_event_queue(xQueueHandle *event_queue);

/**
 * De-register the event queue.
 */
void bus_deregister_event_queue(void);

/**
 * Register a special queue for WPS
 */
int bus_register_special_queue(xQueueHandle *special_queue);

/**
 * Deregister special queue
 */
void bus_deregister_special_queue(void);

/*
 * @internal
 *
 *
 */
int wifi_get_command_lock(void);

/*
 * @internal
 *
 *
 */
int wifi_put_command_lock(void);

/*
 * Process the command reponse received from the firmware.
 *
 * Change the type of param below to HostCmd_DS_COMMAND after mlan
 * integration complete and then move it to header file.
 */
int wifi_process_cmd_response(HostCmd_DS_COMMAND *resp);

/*
 * @internal
 *
 *
 */
void *wifi_mem_malloc_cmdrespbuf(int size);

/*
 * @internal
 *
 *
 */
void *wifi_malloc_eventbuf(int size);
void wifi_free_eventbuf(void *buffer);

int wifi_mem_cleanup();
void wifi_uap_handle_cmd_resp(HostCmd_DS_COMMAND *resp);

mlan_status wrapper_moal_malloc(t_void * pmoal_handle
				__attribute__ ((unused)),
				t_u32 size, t_u32 flag
				__attribute__ ((unused)), t_u8 **ppbuf);
mlan_status wrapper_moal_mfree(t_void *pmoal_handle, t_u8 *pbuf);

#endif /* __WIFI_INTERNAL_H__ */
