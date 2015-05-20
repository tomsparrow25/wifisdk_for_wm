/*
 *  Copyright 2008-2013, Marvell International Ltd.
 *  All Rights Reserved.
 */

#ifndef _APP_CTRL_H_
#define _APP_CTRL_H_

#include <app_framework.h>

/** These flags are required for maintaining diagnostics statistics.
 * These are set by the app_reboot function and are used from the
 * app_main thread to store diagnostics statistics to psm and trigger
 * a reboot.
 */
extern wm_reboot_reason_t g_reboot_reason;
extern uint8_t g_reboot_flag;

/** Init function for application controller */
int app_ctrl_init(void);

int app_ctrl_notify_event(app_ctrl_event_t event, void *data);
int app_ctrl_notify_event_wait(app_ctrl_event_t event, void *data);
int app_ctrl_event_to_prj(app_ctrl_event_t event, void *data);
int app_ctrl_getmode();
WEAK void app_handle_link_loss();
void app_handle_chan_switch();

int app_add_sm(int (*sm_func_ptr)(app_ctrl_event_t event, void *data));
int app_delete_sm(int (*sm_func_ptr)(app_ctrl_event_t event, void *data));

struct connection_state {
	unsigned short auth_failed;
	unsigned short conn_success;
	unsigned short conn_failed;
	unsigned short dhcp_failed;
#define CONN_STATE_NOTCONNECTED 0
#define CONN_STATE_CONNECTING   1
#define CONN_STATE_CONNECTED    2
	unsigned char state;
};
extern struct connection_state conn_state;

extern app_event_handler_t g_ev_hdlr;
extern int app_reconnect_attempts;
#endif
