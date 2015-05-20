/*
 *  Copyright (C) 2008-2012, Marvell International Ltd.
 *  All Rights Reserved.
 */

/* Advanced Event Handlers for pm_mc200_wifi_demo
 * This displays various status messages on the  console .
 */

#include <wmstdio.h>
#include <app_framework.h>
#include <board.h>
#include <wm_os.h>
#include <wm_net.h>
#include <ttcp.h>
#include "pm_mc200_wifi_demo_app.h"

int pm_mc200_wifi_demo_app_event_handler(int event, void *data)
{
	char ip[16];
	int i;
	PM_MC200_WIFI_DEMOAPP_LOG
		("pm_mc200_wifi_demo app: Received event %d\r\n", event);

	switch (event) {
	case AF_EVT_WLAN_INIT_DONE:
		i = (int) data;
		if (i == APP_NETWORK_NOT_PROVISIONED) {
			/* Include indicators if any */
			wmprintf("\r\n Please provision the device"
				 "uisng PSM CLI \r\n");
			wmprintf("psm-set network ssid  <network name>\r\n");
			wmprintf("psm-set network security <security type>"
				 "\r\n");
			wmprintf("security_type : 0 : open ,3 :wpa ,4 : wpa2"
				 "\r\n");
			wmprintf("psm-set network lan DYNAMIC\r\n");
			wmprintf("psm-set network configured 1\r\n");
			wmprintf(" If  security type is 3 (wpa) or 4 (wpa2)"
				 "\r\n");
			wmprintf("psm-set network passphrase <passphrase>\r\n");
		} else
			app_sta_start();
		break;
	case AF_EVT_NORMAL_CONNECTING:
		/* Connecting attempt is in progress */
		net_dhcp_hostname_set(pm_mc200_wifi_demo_hostname);
		break;
	case AF_EVT_NORMAL_CONNECTED:
		/* We have successfully connected to the network. Note that
		 * we can get here after normal bootup or after successful
		 * provisioning.
		 */
		app_network_ip_get(ip);
		wmprintf("\r\n Connected with IP:%s\r\n", ip);
		ttcp_init();
		break;
	case AF_EVT_NORMAL_CONNECT_FAILED:
		/* One connection attempt to the network has failed. Note that
		 * we can get here after normal bootup or after an unsuccessful
		 * provisioning.
		 */
		break;
	case AF_EVT_NORMAL_LINK_LOST:
		/* We were connected to the network, but the link was lost
		 * intermittently.
		 */
	case AF_EVT_NORMAL_USER_DISCONNECT:
		break;
	case AF_EVT_PS_ENTER:
		wmprintf("Power save enter\r\n");
		break;
	case AF_EVT_PS_EXIT:
		wmprintf("Power save exit\r\n");
		break;
	default:
		PM_MC200_WIFI_DEMOAPP_DBG
			("pm_mc200_wifi_demo app: Not handling event %d\r\n",
				event);
		break;
	}
	return 0;
}
