/*
 *  Copyright (C) 2008-2012, Marvell International Ltd.
 *  All Rights Reserved.
 */

/* pm_mc200_wifi_demo_app.c
 * Entry point into the application specific code.
 */

/* System includes */
#include <wmstdio.h>
#include <string.h>
#include <wmtime.h>
#include <wm_os.h>
#include <arch/sys.h>
#include <rfget.h>
#include <wmsysinfo.h>
#include <diagnostics.h>
#include <arch/pm_mc200.h>
#include <cli.h>
#include <stdlib.h>
#include <psm.h>

/* Generic Application includes */
#include <app_framework.h>
/* Application specific includes */
#include <pm_mc200_wifi_demo_app.h>

extern void pm_pm0_in_idle();
static void pm1_only()
{
	os_disable_all_interrupts();
	PMU_SetSleepMode(PMU_PM1);
	__asm volatile ("wfi");
}
char pm_mc200_wifi_demo_hostname[30];
char pm_mc200_wifi_demo_ssid[30];

static struct cli_command pm0_pm1_cmds[] = {
	{"pm0-enter",
	"<NONE>",
	 pm_pm0_in_idle},
	{ "pm1-enter",
	   "<NONE>",
	   pm1_only},
};

static void pm_mc200_wifi_demo_configure_defaults()
{
	uint8_t my_mac[6];

	wlan_get_mac_address(my_mac);
	/* Provisioning SSID */
	snprintf(pm_mc200_wifi_demo_ssid, sizeof(pm_mc200_wifi_demo_ssid),
		 "wmdemo-%02X%02X", my_mac[4], my_mac[5]);
	/* Default hostname */
	snprintf(pm_mc200_wifi_demo_hostname,
		 sizeof(pm_mc200_wifi_demo_hostname),
		 "wmdemo-%02X%02X", my_mac[4], my_mac[5]);
}

int main()
{
	int ret;
	/* Initialize WM core modules */
	ret = wm_core_and_wlan_init();

	if (ret) {
		wmprintf("Error initializing WLAN subsystem. Reason: %d\r\n",
				ret);
		/* System core init failed, just wait forever here */
		while (1)
			;
	}
	ret = psm_cli_init();
	if (ret) {
		wmprintf("Error registering PSM commands. Reason: %d\r\n",
				ret);
		/* System core init failed, just wait forever here */
		while (1)
			;
	}
	wlan_cli_init();
	pm_mc200_cli_init();
	wmprintf("[pm_mc200_wifi_demo] Build Time: "__DATE__" "__TIME__"\r\n");
	sysinfo_init();
	pm_mc200_wifi_demo_configure_defaults();
	/* Disable sending null packets */
	wlan_configure_null_pkt_interval(-1);
	int i;
	for (i = 0; i < sizeof(pm0_pm1_cmds) / sizeof(struct cli_command); i++)
		cli_register_command(&pm0_pm1_cmds[i]);
	/* Start the application framework */
	if (app_framework_start(pm_mc200_wifi_demo_app_event_handler)
	    != WM_SUCCESS) {
		wmprintf("Failed to start application framework\r\n");
		while (1)
			;
	}
	return 0;
}
