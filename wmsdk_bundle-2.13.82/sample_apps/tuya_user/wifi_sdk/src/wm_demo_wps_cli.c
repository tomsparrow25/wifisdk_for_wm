/*
 *  Copyright (C) 2013, Marvell International Ltd.
 *  All Rights Reserved.
 */

#include <wmstdio.h>
#include <wm_os.h>
#include <app_framework.h>
#include <cli.h>
#include <wm_demo_wps_cli.h>
#include <appln_dbg.h>

#ifdef CONFIG_WPS2
static void wm_demo_simulate_wps_pb(int argc, char **argv)
{
	dbg("WPS pushbutton press");
	if (app_prov_wps_session_start(NULL) == WM_SUCCESS) {
		dbg("WPS session start successful");
	}
	return;
}

static void wm_demo_simulate_wps_pin(int argc, char **argv)
{
	int ret = -WM_FAIL;

	if (argc < 2) {
		dbg("Usage: %s <PIN>", argv[0]);
		dbg("The PIN can be 4 or 8 digits in length.");
		dbg("The PIN can be static or dynamically "
				"generated on AP or WM device.");
		dbg("Example PINs: 12345670, 44151645, 1234");
		return;
	}

	if (strcmp((char *)argv[1], "00000000") == 0 ||
		strcmp((char *)argv[1], "0000") == 0) {
		dbg("WPS pushbutton press");
		ret = app_prov_wps_session_start(NULL);
	} else {
		dbg("WPS PIN provided");
		ret = app_prov_wps_session_start((char *)argv[1]);
	}

	if (ret == WM_SUCCESS) {
		dbg("WPS session start successful");
	}
	return;
}

static struct cli_command wps_cmds[] = {
	{"wps-pb", NULL, wm_demo_simulate_wps_pb},
	{"wps-pin", "<pin>", wm_demo_simulate_wps_pin},
};

void wm_demo_wps_cli_init(void)
{
	int i;

	for (i = 0; i < sizeof(wps_cmds) / sizeof(struct cli_command); i++)
		if (cli_register_command(&wps_cmds[i]))
			dbg("Command register error");

}

void wm_demo_wps_cli_deinit(void)
{
	int i;

	for (i = 0; i < sizeof(wps_cmds) / sizeof(struct cli_command); i++)
		if (cli_unregister_command(&wps_cmds[i]))
			dbg("Command unregister error");

}

#else

void wm_demo_wps_cli_init(void)
{
}

void wm_demo_wps_cli_deinit(void)
{
}

#endif /* CONFIG_WPS2 */
