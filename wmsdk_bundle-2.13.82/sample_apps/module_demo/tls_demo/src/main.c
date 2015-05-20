/*
 *  Copyright (C) 2008-2013, Marvell International Ltd.
 *  All Rights Reserved.
 */

/*
 * HTTP TLS Demo
 *
 * Summary:
 *
 * This application provides a command 'tls-http-client' that can be used to
 * open a connection with an https (HTTP over TLS) website. The command connects
 * to the website, downloads the data and dumps the same on the console.
 *
 * Description:
 *
 * The application is written using Application Framework that
 * simplifies development of WLAN networking applications.
 *
 *
 * WLAN Initialization:
 *
 * When the application framework is started, it starts up the WLAN
 * sub-system and initializes the network stack. We receive the event
 * when the WLAN subsystem has been started and initialized.
 *
 * If a provisioned network is found in PSM, we connect to that network. If not,
 * we connect to a pre-defined network with the following settings:
 *
 * SSID : marvell
 * passphrase: marvellwm
 * security: WPA2
 *
 * TLS Transaction :
 *
 * Once the connection is established, a HTTP-TLS session may be initiated by
 * typing the tls-http-client command on the CLI. Since a TLS transaction takes
 * a much larger stack space than is available in the CLI thread, a new thread
 * is spawned to perform the TLS transaction.
 *
 * The thread starts a HTTP session, reads data and prints it all on the
 * console.
 *
 */
#include <wm_os.h>
#include <app_framework.h>
#include <wmtime.h>
#include <partition.h>
#include <appln_cb.h>
#include <appln_dbg.h>
#include <cli.h>
#include <wmstdio.h>
#include <wmsysinfo.h>
#include <wm_net.h>
#include <mdns_helper.h>
#include <wps_helper.h>
#include <reset_prov_helper.h>
#include <httpd.h>
#include <led_indicator.h>
#include <board.h>
#include <dhcp-server.h>
#include <wmtime.h>
#include <psm.h>
#include <ftfs.h>
#include <mdev_gpio.h>

#include "tls-demo.h"

/*
 * Event: INIT_DONE
 *
 * The application framework is initialized.
 *
 * The data field has information passed by boot2 bootloader
 * when it loads the application.
 *
 */
static void event_init_done(void *data)
{
	struct app_init_state *state;

	state = (struct app_init_state *)data;

	dbg("Event: INIT_DONE");
	dbg("Factory reset bit status: %d", state->factory_reset);
	dbg("Booting from backup firmware status: %d", state->backup_fw);
	dbg("Previous reboot cause: %u", state->rst_cause);
}

/*
 * Handler invoked on WLAN_INIT_DONE event.
 *
 */
static void event_wlan_init_done(void *data)
{
	int ret;
	struct wlan_network network;
	int provisioned = (int)data;

	if (provisioned)
		app_sta_start();
	else {
#define DEF_NW_NAME          "station"
#define DEF_NW_SSID          "marvell"
#define DEF_NW_PASSPHRASE    "marvellwm"
#define DEF_NW_SECURITY      WLAN_SECURITY_WPA2

		memset(&network, 0, sizeof(network));
		strncpy(network.name, DEF_NW_NAME, sizeof(network.name));
		strncpy(network.ssid, DEF_NW_SSID, sizeof(network.ssid));
		strncpy(network.security.psk, DEF_NW_PASSPHRASE,
			sizeof(network.security.psk));
		network.security.psk_len = strlen(network.security.psk);
		network.security.type = DEF_NW_SECURITY;
		network.address.addr_type = ADDR_TYPE_DHCP;
		network.type = WLAN_BSS_TYPE_STA;
		network.role = WLAN_BSS_ROLE_STA;

		app_sta_start_by_network(&network);
	}
	/*
	 * Initialize CLI Commands for some of the modules:
	 *
	 * -- wlan: allows user to explore basic wlan functions
	 */
	ret = wlan_cli_init();
	if (ret != WM_SUCCESS)
		dbg("Error: wlan_cli_init failed");

	ret = psm_cli_init();
	if (ret != WM_SUCCESS)
		dbg("Error: wlan_cli_init failed");

	tls_demo_init();
}


/*
 * Event: CONNECTING
 *
 * We are attempting to connect to the Home Network
 *
 * This is just a transient state as we will either get
 * CONNECTED or have a CONNECTION/AUTH Failure.
 *
 */
static void event_normal_connecting(void *data)
{
	dbg("Connecting to Home Network");
}

/* Event: AF_EVT_NORMAL_CONNECTED
 *
 * Station interface connected to home AP.
 *
 * Network dependent services can be started here. Note that these
 * services need to be stopped on disconnection and
 * reset-to-provisioning event.
 */
static void event_normal_connected(void *data)
{
	char ip[16];

	app_network_ip_get(ip);
	dbg("Connected to Home Network with IP address =%s", ip);

}

/*
 * Event: CONNECT_FAILED
 *
 * We attempted to connect to the Home AP, but the connection did
 * not succeed.
 *
 * This typically indicates:
 *
 * -- The access point could not be found.
 * -- We did not get a valid IP address from the AP
 *
 */
static void event_connect_failed()
{
	dbg("Application Error: Connection Failed");
}


/*
 * Event: LINK LOSS
 *
 * We lost connection to the AP.
 *
 * The App Framework will attempt to reconnect. We dont
 * need to do anything here.
 */
static void event_normal_link_lost(void *data)
{
	dbg("Link Lost");
}


/* This is the main event handler for this project. The application framework
 * calls this function in response to the various events in the system.
 */
int common_event_handler(int event, void *data)
{
	switch (event) {
	case AF_EVT_INIT_DONE:
		event_init_done(data);
		break;
	case AF_EVT_WLAN_INIT_DONE:
		event_wlan_init_done(data);
		break;
	case AF_EVT_NORMAL_CONNECTING:
		event_normal_connecting(data);
		break;
	case AF_EVT_NORMAL_CONNECTED:
		event_normal_connected(data);
		break;
	case AF_EVT_NORMAL_CONNECT_FAILED:
		event_connect_failed(data);
		break;
	case AF_EVT_NORMAL_LINK_LOST:
		event_normal_link_lost(data);
		break;
	default:
		break;
	}

	return 0;
}

static void modules_init()
{
	int ret;

	ret = wmstdio_init(UART0_ID, 0);
	if (ret != WM_SUCCESS) {
		while (1)
			;
		/* do nothing -- stall */
	}

	ret = cli_init();
	if (ret != WM_SUCCESS) {
		dbg("Error: cli_init failed");
		while (1)
			;
		/* do nothing -- stall */
	}
	/* Initialize time subsystem.
	 *
	 * Initializes time to 1/1/1970 epoch 0.
	 */
	ret = wmtime_init();
	if (ret != WM_SUCCESS) {
		dbg("Error: wmtime_init failed");
		while (1)
			;
		/* do nothing -- stall */
	}

	/*
	 * Register Time CLI Commands
	 */
	ret = wmtime_cli_init();
	if (ret != WM_SUCCESS) {
		dbg("Error: wmtime_cli_init failed");
		while (1)
			;
		/* do nothing -- stall */
	}

	return;
}

int main()
{
	modules_init();

	dbg("Build Time: " __DATE__ " " __TIME__ "");

	/* Start the application framework */
	if (app_framework_start(common_event_handler) != WM_SUCCESS) {
		dbg("Failed to start application framework");
		while (1)
			;
		/* do nothing -- stall */
	}
	return 0;
}
