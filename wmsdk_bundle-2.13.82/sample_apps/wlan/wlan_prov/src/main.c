/*
 *  Copyright (C) 2008-2013, Marvell International Ltd.
 *  All Rights Reserved.
 */
/*
 * Simple Provisioning Application
 *
 * Summary:
 *
 * This application uses the Application Framework to perform network
 * provisioning of the device. Network provisioning can be performed by either
 * using a Webapp based wizard over the micro-AP interface or using the WPS
 * (WiFi Protect Setup) protocol.
 *
 * Description:
 *
 * When the application framework is started, it starts up the WLAN
 * sub-system and initializes the network stack. The app receives the event
 * when the WLAN subsystem has been started and initialized.
 *
 * The mDNS/DNS-SD service and the Web Server is started.
 *
 * If network configuration is not found, the app starts a micro-AP network
 * along with a DHCP server. This will create a WLAN network and also creates
 * a IP network interface associated with that WLAN network. The DHCP server
 * allows devices to associate and connect to this network and establish an
 * IP level connection.
 *
 * When the network is initialized and ready, app will receive a
 * micro-AP started event.
 *
 * Micro-AP Started:
 *
 * The Provisioning service is then started. This service allows users to
 * perform network provisioning using one of the following methods:
 *
 * 1. Webapp-based Provisioning: Users can connect to the micro-AP network and
 * access the Webapp that is served by the Web Server. The Webapp has a wizard
 * that takes the users through the network provisioning steps
 *
 * 2. WPS Provisioning: Users press a button on the Access Point (AP) and a
 * button on this device. The WPS protocol is initiated which takes care of
 * transferring the network credentials to the device.
 *
 * Once network provisioning is successful, the network credentials are recorded
 * in the PSM (Persistent Storage Manager). From next bootup on wards, once the
 * WLAN subsystem is initialized, the application knows that the network
 * credentials are already present, and the application directly starts the
 * station interface.
 *
 */
#include <wm_os.h>
#include <app_framework.h>
#include <wmtime.h>
#include <partition.h>
#include <appln_cb.h>
#include <appln_dbg.h>
#include <cli.h>
#include <psm.h>
#include <wmstdio.h>
#include <wmsysinfo.h>
#include <wm_net.h>
#include <mdns_helper.h>
#include <wps_helper.h>
#include <reset_prov_helper.h>
#include <httpd.h>
#include <ftfs.h>
#include <led_indicator.h>
#include <board.h>
#include <dhcp-server.h>
#include <mdev_gpio.h>

/*-----------------------Global declarations----------------------*/

appln_config_t appln_cfg = {
	.ssid = "nzy_test",
	.passphrase = "12345678",
	.hostname = "provdemo",
};

int ftfs_api_version = 100;
char *ftfs_part_name = "ftfs";

static uint8_t first_time = 1;
struct fs *fs;

/*-----------------------Global declarations----------------------*/
static int provisioned;
static uint8_t mdns_announced;

/* This function must initialize the variables required (network name,
 * passphrase, etc.) It should also register all the event handlers that are of
 * interest to the application.
 */
int appln_config_init()
{
	/* Initialize service name for mdns */
	snprintf(appln_cfg.servname, MAX_SRVNAME_LEN, "provdemo");
	appln_cfg.wps_pb_gpio = board_button_1();
	/* Initialize reset to provisioning push button settings */
	appln_cfg.reset_prov_pb_gpio = board_button_2();
	return 0;
}

/* This function is defined for handling critical error.
 * For this application, we just stall and do nothing when
 * a critical error occurs.
 *
 */
void appln_critical_error_handler(void *data)
{
	while (1)
		;
	/* do nothing -- stall */
}

/*
 * Handler invoked on WLAN_INIT_DONE event.
 *
 * When WLAN is started, the application framework looks to
 * see whether a home network information is configured
 * and stored in PSM (persistent storage module).
 *
 * The data field returns whether a home network is provisioned
 * or not, which is used to determine what network interfaces
 * to start (station, micro-ap, or both).
 *
 * If provisioned, the station interface of the device is
 * connected to the configured network.
 *
 * Else, Micro-AP network is configured.
 *
 * (If desired, the Micro-AP network can also be started
 * along with the station interface.)
 *
 * We also start all the services which don't need to be
 * restarted between provisioned and non-provisioned mode
 * or between connected and disconnected state.
 *
 * Accordingly:
 *	-- Start mDNS and advertize services
 *	-- Start HTTP Server
 *	-- Register WSGI handlers for HTTP server
 */
void event_wlan_init_done(void *data)
{
	int ret;
	/* We receive provisioning status in data */
	provisioned = (int)data;

	dbg("AF_EVT_WLAN_INIT_DONE provisioned=%d", provisioned);

	if (provisioned) {
		app_sta_start();
	} else {
		app_uap_start_with_dhcp(appln_cfg.ssid, appln_cfg.passphrase);
	}

	if (provisioned)
		hp_configure_reset_prov_pushbutton();

	/*
	 * Start mDNS and advertize our hostname using mDNS
	 */
	dbg("Starting mdns");
	app_mdns_start(appln_cfg.hostname);

	/*
	 * Start http server and enable webapp in the
	 * FTFS partition on flash
	 */
	ret = app_httpd_with_fs_start(ftfs_api_version, ftfs_part_name, &fs);
	if (ret != WM_SUCCESS)
		dbg("Failed to start HTTPD");

	/*
	 * Initialize CLI Commands for some of the modules:
	 *
	 * -- psm:  allows user to check data in psm partitions
	 * -- ftfs: allows user to see contents of ftfs
	 * -- wlan: allows user to explore basic wlan functions
	 */

	ret = psm_cli_init();
	if (ret != WM_SUCCESS)
		dbg("Error: psm_cli_init failed");
	ret = ftfs_cli_init(fs);
	if (ret != WM_SUCCESS)
		dbg("Error: ftfs_cli_init failed");
	ret = wlan_cli_init();
	if (ret != WM_SUCCESS)
		dbg("Error: wlan_cli_init failed");

	if (!provisioned) {
		/* Start Slow Blink */
		led_slow_blink(board_led_2());
	}

}

/*
 * Event: Micro-AP Started
 *
 * If we are not provisioned, then start provisioning on
 * the Micro-AP network.
 *
 * Also, enable WPS.
 *
 * Since Micro-AP interface is UP, announce mDNS service
 * on the Micro-AP interface.
 */
void event_uap_started(void *data)
{
	void *iface_handle = net_get_uap_handle();

	if (!provisioned) {
		dbg("Starting provisioning");
		hp_configure_wps_pushbutton();
		app_provisioning_start(PROVISIONING_WLANNW | PROVISIONING_WPS);
	}
	dbg("mdns uap up event");
	hp_mdns_announce(iface_handle, UP);
}

void event_normal_reset_prov(void *data)
{
	led_on(board_led_2());
	first_time = 1;

	/* Start Slow Blink */
	led_slow_blink(board_led_2());

	/* Reset to provisioning */
	provisioned = 0;
	mdns_announced = 0;
	hp_unconfigure_reset_prov_pushbutton();
	if (is_uap_started() == false) {
		app_uap_start_with_dhcp(appln_cfg.ssid, appln_cfg.passphrase);
	} else {
		hp_configure_wps_pushbutton();
		app_provisioning_start(PROVISIONING_WLANNW | PROVISIONING_WPS);
	}
}

void event_prov_done(void *data)
{
	hp_configure_reset_prov_pushbutton();
	hp_unconfigure_wps_pushbutton();
	app_provisioning_stop();
	dbg("Provisioning successful");
}

void event_prov_client_done(void *data)
{
	app_uap_stop();
	dhcp_server_stop();
}

/*
 * Event: PROV_WPS_SSID_SELECT_REQ
 *
 * An SSID with active WPS session is found and WPS negotiation will
 * be started with this AP.
 *
 * Since WPS take a lot of memory resources (on the heap), we
 * temporarily stop http server (and, the Micro-AP provisioning
 * along with it).
 *
 * The HTTP server will be restarted when WPS session is over.
 */
void event_prov_wps_ssid_select_req(void *data)
{
	int ret;

	ret = app_httpd_stop();
	if (ret != WM_SUCCESS) {
		dbg("Error stopping HTTP server");
	}
}

/*
 * Event: PROV_WPS_SUCCESSFUL
 *
 * WPS session completed successfully.
 *
 * Restart the HTTP server that was stopped when WPS session attempt
 * began.
 */
void event_prov_wps_successful(void *data)
{
	int ret;

	ret = app_httpd_with_fs_start(ftfs_api_version, ftfs_part_name, &fs);
	if (ret != WM_SUCCESS) {
		dbg("Error starting HTTP server");
	}

	return;
}

/*
 * Event: PROV_WPS_UNSUCCESSFUL
 *
 * WPS session completed unsuccessfully.
 *
 * Restart the HTTP server that was stopped when WPS session attempt
 * began.
 */
void event_prov_wps_unsuccessful(void *data)
{
	int ret;

	ret = app_httpd_with_fs_start(ftfs_api_version, ftfs_part_name, &fs);
	if (ret != WM_SUCCESS) {
		dbg("Error starting HTTP server");
	}
	return;
}

void event_normal_connecting(void *data)
{
	net_dhcp_hostname_set(appln_cfg.hostname);
	dbg("Connecting to Home Network");
	/* Start Fast Blink */
	led_fast_blink(board_led_2());
}

/* Event: AF_EVT_NORMAL_CONNECTED
 *
 * Station interface connected to home AP.
 *
 * Network dependent services can be started here. Note that these
 * services need to be stopped on disconnection and
 * reset-to-provisioning event.
 */
void event_normal_connected(void *data)
{
	void *iface_handle;
	char ip[16];

	led_off(board_led_2());

	app_network_ip_get(ip);
	dbg("Connected to provisioned network with ip address =%s", ip);

	iface_handle = net_get_mlan_handle();
	if (!mdns_announced) {
		hp_mdns_announce(iface_handle, UP);
		mdns_announced = 1;
	} else {
		hp_mdns_down_up(iface_handle);
	}
}

/* Event handler for AF_EVT_NORMAL_DISCONNECTED - Station interface
 * disconnected.
 * Stop the network services which need not be running in disconnected mode.
 */
void event_normal_user_disconnect(void *data)
{
	led_on(board_led_2());
	/* restart the micro-ap interface */
}

void event_normal_link_lost(void *data)
{
	/* is this automatically retried ?? */
	event_normal_user_disconnect(data);
}

void event_normal_dhcp_renew(void *data)
{
	void *iface_handle = net_get_mlan_handle();
	hp_mdns_announce(iface_handle, REANNOUNCE);
}

/* This is the main event handler for this project. The application framework
 * calls this function in response to the various events in the system.
 */
int common_event_handler(int event, void *data)
{
	switch (event) {
	case AF_EVT_WLAN_INIT_DONE:
		event_wlan_init_done(data);
		break;
	case AF_EVT_NORMAL_CONNECTING:
		event_normal_connecting(data);
		break;
	case AF_EVT_NORMAL_CONNECTED:
		event_normal_connected(data);
		break;
	case AF_EVT_NORMAL_LINK_LOST:
		event_normal_link_lost(data);
		break;
	case AF_EVT_NORMAL_USER_DISCONNECT:
		event_normal_user_disconnect(data);
		break;
	case AF_EVT_NORMAL_DHCP_RENEW:
		event_normal_dhcp_renew(data);
		break;
	case AF_EVT_PROV_DONE:
		event_prov_done(data);
		break;
	case AF_EVT_PROV_WPS_SSID_SELECT_REQ:
		event_prov_wps_ssid_select_req(data);
		break;
	case AF_EVT_PROV_WPS_SUCCESSFUL:
		event_prov_wps_successful(data);
		break;
	case AF_EVT_PROV_WPS_UNSUCCESSFUL:
		event_prov_wps_unsuccessful(data);
		break;
	case AF_EVT_NORMAL_RESET_PROV:
		event_normal_reset_prov(data);
		break;
	case AF_EVT_UAP_STARTED:
		event_uap_started(data);
		break;
	case AF_EVT_PROV_CLIENT_DONE:
		event_prov_client_done(data);
		break;
	default:
		break;
	}

	return 0;
}

static void modules_init()
{
	int ret;

	/*
	 * Initialize wmstdio prints
	 */
	ret = wmstdio_init(UART0_ID, 0);
	if (ret != WM_SUCCESS) {
		dbg("Error: wmstdio_init failed");
		appln_critical_error_handler((void *) -WM_FAIL);
	}

	/*
	 * Initialize CLI Commands
	 */
	ret = cli_init();
	if (ret != WM_SUCCESS) {
		dbg("Error: cli_init failed");
		appln_critical_error_handler((void *) -WM_FAIL);
	}
	/* Initialize time subsystem.
	 *
	 * Initializes time to 1/1/1970 epoch 0.
	 */
	ret = wmtime_init();
	if (ret != WM_SUCCESS) {
		dbg("Error: wmtime_init failed");
		appln_critical_error_handler((void *) -WM_FAIL);
	}

	/*
	 * Register Time CLI Commands
	 */
	ret = wmtime_cli_init();
	if (ret != WM_SUCCESS) {
		dbg("Error: wmtime_cli_init failed");
		appln_critical_error_handler((void *) -WM_FAIL);
	}

	/*
	 * Register Power Management CLI Commands
	 */
	ret = pm_cli_init();
	if (ret != WM_SUCCESS) {
		dbg("Error: pm_cli_init failed");
		appln_critical_error_handler((void *) -WM_FAIL);
	}

	ret = gpio_drv_init();
	if (ret != WM_SUCCESS) {
		dbg("Error: gpio_drv_init failed");
		appln_critical_error_handler((void *) -WM_FAIL);
	}

	return;
}

int main()
{    
	modules_init();

	dbg("Build Time: " __DATE__ " " __TIME__ "");

	appln_config_init();

	/* Start the application framework */
	if (app_framework_start(common_event_handler) != WM_SUCCESS) {
		dbg("Failed to start application framework");
		appln_critical_error_handler((void *) -WM_FAIL);
	}
	return 0;
}
