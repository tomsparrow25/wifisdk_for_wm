/*
 *  Copyright (C) 2008-2013, Marvell International Ltd.
 *  All Rights Reserved.
 */

/* Simple WLAN Application using WPA2 Enterprise connection
 * for Station interface.
 *
 * Summary:
 *
 * This application connects to a WPA2 Enterprise based WLAN network
 * using pre-defined certificates, key and network parameters.
 *
 * Description:
 *
 * The application is written using Application Framework that
 * simplifies development of WLAN networking applications
 *
 * WLAN Initialization:
 *
 * When the application framework is started, it starts up the WLAN
 * sub-system and initializes the network stack. The app receive the event
 * when the WLAN subsystem has been started and initialized.
 *
 * Since the WPA2 Enterprise association is happening with EAP-TLS, the required
 * network configuration parameters like the certificates/keys, network SSID
 * etc. are pre-configured in the device. In this application these certificates
 * are stored in the header files cacert.h, client-cert.h and client-key.h.
 *
 * These parameters are used to start the station interface.
 *
 * Station Connected:
 *
 * When the station is connected, network services like the Web Server and the
 * mDNS/DNS-SD service are started.
 *
 * Note that the WPA2 Enterprise connection establishment requires quite a lot
 * of heap memory. Hence it is safer to shutdown all the other services while
 * connection establishment is in progress.
 *
 * LED indications:
 * Following are the various LED indications to indicate the connection states
 * of WPA2 Enterprise connection
 * --------------------------------------------------------
 * Blue LED			Red LED			Connection state
 * --------------------------------------------------------
 * Fast blinking	Off				Connecting
 * Slow blinking	Off				DHCP failure
 * Off				Fast blinking	AUTH failure
 * Off				Slow blinking	Unreachable AP
 * On				On				Link Lost
 * Off				Off				Connected
 * --------------------------------------------------------
 */
#include <wm_os.h>
#include <app_framework.h>
#include <wmtime.h>
#include <partition.h>
#include <appln_cb.h>
#include <appln_dbg.h>
#include <cli.h>
#include <wmstdio.h>
#include <wm_net.h>
#include <mdns_helper.h>
#include <led_indicator.h>
#include <board.h>
#include <wmtime.h>
#include <psm.h>
#include <wm-tls.h>
#include <wpa2_network.h>
#include <power_mgr_helper.h>

#define MDNS_SRVNAME	"wpa2ent"
/*-----------------------Global declarations----------------------*/

/* Application Configuration
 * SSID and Passphrase are not required since there is no uAP mode
 */
appln_config_t appln_cfg = {
	.ssid = "",
	.passphrase = "",
	.hostname = "wpa2ent",
	.wps_pb_gpio = -1,
	.reset_prov_pb_gpio = -1
};

/* Default values for time.
 * This is required so that the default time after bootup is closer to the
 * actual current time. This time is used to validate WPA2 certificates.
 */
#define DEF_HR		12
#define DEF_MIN		0
#define DEF_DATE	1
#define DEF_MONTH	10
#define DEF_YEAR	2013
int appln_set_time()
{
	struct tm tm_set, tm_get;
	bzero(&tm_set, sizeof(tm_set));
	tm_set.tm_min = DEF_MIN;
	tm_set.tm_hour = DEF_HR;
	tm_set.tm_mday = DEF_DATE;
	tm_set.tm_mon = DEF_MONTH;
/* 1900 subtracted since gmtime uses offset from year 1900 */
	tm_set.tm_year = DEF_YEAR - 1900;
	wmtime_time_set(&tm_set);
	wmtime_time_get(&tm_get);
	dbg("Current Time set to: %d-%d-%d %d:%d", tm_get.tm_mday,
		tm_get.tm_mon, tm_get.tm_year + 1900,
		tm_get.tm_hour, tm_get.tm_min);
	return 0;
}

/* This function is defined for handling critical error.
 * For this application, we just stall and do nothing when a
 * critical error occurs.
 */
void appln_critical_error_handler(void *data)
{
	while (1)
		;
}

/* This function must initialize the variables required.
 * It should also do other application specific initializations
 * as required.
 */
int appln_config_init()
{
	/* Initialize service name for mDNS */
	snprintf(appln_cfg.servname, MAX_SRVNAME_LEN, MDNS_SRVNAME);
	/* A dummy time is first set so that the WPA2-Enterprise certificates
	 * are valid. The certificates will be invalid for the default time of
	 * 1-Jan-1970
	 */
	appln_set_time();
	hp_pm_init();
	return 0;
}

static uint8_t services_started;

/* Start various services required by the application.
 */
int start_app_services()
{
	int ret;
	dbg("Starting services");
	if (app_httpd_start() != WM_SUCCESS) {
		dbg("Failed to start HTTPD");
	}
	app_mdns_start(appln_cfg.hostname);
	/* Initialize CLI Commands for some of the modules:
	 *
	 * -- psm:  allows user to check data in psm partitions
	 * -- wlan: allows user to explore basic wlan functions
	 *
	 *  CLI is started only after successful connection because the memory
	 *  requirement during WPA2 Enterprise connection phase is high, and
	 *  keeping the CLI ON results in failure.
	 */
	ret = cli_init();
	if (ret != WM_SUCCESS) {
		dbg("CLI init failed.");
	} else {
		ret = psm_cli_init();
		if (ret != WM_SUCCESS) {
			dbg("Error: psm_cli_init failed");
			appln_critical_error_handler((void *) -WM_FAIL);
		}

		ret = wlan_cli_init();
		if (ret != WM_SUCCESS) {
			dbg("Error: wlan_cli_init failed");
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

		ret = pm_mc200_cli_init();
		if (ret != WM_SUCCESS) {
			dbg("Error: pm_mc200_cli_init failed");
			appln_critical_error_handler((void *) -WM_FAIL);
		}
	}
	services_started = 1;
	return WM_SUCCESS;
}

/* Stop services when they are not required.
 */
int stop_app_services()
{
	int ret;
	if (!services_started)
		return -WM_FAIL;
	dbg("Stopping the services");

	dbg("Stopping HTTPD");
	app_httpd_stop();
	dbg("Stopping mdns");
	app_mdns_stop();
	dbg("Stopping CLI");
	ret = cli_stop();
	if (ret != WM_SUCCESS) {
		dbg("CLI stop failed.");
	}
	services_started = 0;
	return WM_SUCCESS;
}

/*-----------------------Local declarations----------------------*/
static uint8_t mdns_announced;

/*
 * Event: INIT_DONE
 *
 * The application framework is initialized.
 *
 * The data field has information passed by boot2 bootloader
 * when it loaded the application.
 *
 * If application is loaded by JTAG, the data field will have stale values
 * and can be ignored.
 *
 */
static void event_init_done(void *data)
{
#if APPCONFIG_DEBUG_ENABLE
	struct app_init_state *state;
	state = (struct app_init_state *)data;
#endif /* APPCONFIG_DEBUG_ENABLE */

	dbg("Event: INIT_DONE");
	dbg("Factory reset bit status: %d", state->factory_reset);
	dbg("Booting from backup firmware status: %d", state->backup_fw);
	dbg("Previous reboot cause: %u", state->rst_cause);
}

/*
 * Handler invoked on WLAN_INIT_DONE event.
 *
 * When WLAN is started, the application framework looks to
 * see whether the network information is configured
 * and stored in PSM (persistent storage module).
 *
 * The data field returns whether a network is provisioned
 * or not, which is used to determine what network interfaces
 * to start (station, micro-ap, or both).
 *
 * For the WPA2 Enterprise application, this value is ignored
 * since the network details are provided by another mechanism
 * and the station interface is started.
 *
 * No other services (including the CLI) have been started here because
 * the WPA2 Enterprise connection phase can fail if it does not get
 * enough free memory.
 */
static void event_wlan_init_done(void *data)
{
	struct wlan_network nw;
	dbg("Event: WLAN_INIT_DONE");
	/* Populate the WLAN Network structure with details of the network
	 * to be conected to. This also includes pointers to the various
	 * certificates required by the WPA2 Enterprise Connection.
	 */
	get_wpa2_nw(&nw);
	app_sta_start_by_network(&nw);
}

/*
 * Event: CONNECTING
 *
 * Attempting to connect to the Network
 *
 * Note: The control can come here:
 *
 *   1. After boot -- if already provisioned.
 *   2. After provisioning
 *   3. After link loss
 *
 * This is just a transient state as we will either get
 * CONNECTED or have a CONNECTION/AUTH Failure.
 *
 */
static void event_normal_connecting(void *data)
{

	dbg("Connecting to Network");
	/* Stop blinking of Red led */
	led_off(board_led_2());
	/* Start Fast Blinking of White LED */
	led_fast_blink(board_led_1());
}

/* Event: AF_EVT_NORMAL_CONNECTED
 *
 * Station interface connected to configured network.
 *
 * Network dependent services can be started here. Note that these
 * services need to be stopped on disconnection and
 * reset-to-provisioning event.
 */
static void event_normal_connected(void *data)
{
	void *iface_handle;
	char ip[16];

	start_app_services();
	/* Turn off LED */
	led_off(board_led_1());

	app_network_ip_get(ip);
	dbg("Connected to Network with IP address = %s", ip);

	iface_handle = net_get_sta_handle();
	if (!mdns_announced) {
		hp_mdns_announce(iface_handle, UP);
		mdns_announced = 1;
	} else {
		hp_mdns_down_up(iface_handle);
	}
	hp_pm_wifi_ps_enable();
}

/*
 * Event: CONNECT_FAILED
 *
 * Attempted to connect to the AP, but the connection did
 * not succeed.
 *
 * This typically indicates:
 *
 * -- The access point could not be found.
 * -- Authentication failed.
 * -- No valid IP address from the AP.
 *
 */
static void event_connect_failed(void *data)
{
	dbg("Application Error: Connection Failed");
	stop_app_services();
	/* Stop blinking of white led */
	led_off(board_led_1());
	if (*(app_conn_failure_reason_t *)data == AUTH_FAILED)
		led_fast_blink(board_led_2());
	if (*(app_conn_failure_reason_t *)data == NETWORK_NOT_FOUND)
		led_slow_blink(board_led_2());
	if (*(app_conn_failure_reason_t *)data == DHCP_FAILED)
		led_slow_blink(board_led_1());
	/* A small delay so that the user can see the LED indication */
	os_thread_sleep(os_msec_to_ticks(1000));
}

/*
 * Event: USER_DISCONNECT
 *
 * This means that the application has explicitly requested a network
 * disconnect,
 *
 */
static void event_normal_user_disconnect(void *data)
{
	dbg("User disconnect");
}

/*
 * Event: LINK LOSS
 *
 * Connection to the AP is lost.
 *
 * The App Framework will attempt to reconnect. There is
 * no need to do anything here.
 * However, the services should be stopped so that more memory will
 * be available for the wlan connection manager.
 *
 * Some LED indication will also be shown.
 *
 */
static void event_normal_link_lost(void *data)
{
	dbg("Link Lost");
	stop_app_services();
	/* Keep both LEDs on for 1 sec */
	led_on(board_led_1());
	led_on(board_led_2());
	/* A small delay so that the user can see the LED indication */
	os_thread_sleep(os_msec_to_ticks(1000));
}

/*
 * Event: DHCP RENEW
 *
 * This event is received when the IP Address is renewed by the DHCP Server.
 */
static void event_normal_dhcp_renew(void *data)
{
	void *iface_handle = net_get_mlan_handle();
	hp_mdns_announce(iface_handle, REANNOUNCE);
}

void ps_state_to_desc(char *ps_state_desc, int ps_state)
{
	switch (ps_state) {
	case WLAN_IEEE:
		strcpy(ps_state_desc, "IEEE PS");
		break;
	case WLAN_DEEP_SLEEP:
		strcpy(ps_state_desc, "Deep sleep");
		break;
	case WLAN_PDN:
		strcpy(ps_state_desc, "Power down");
		break;
	case WLAN_ACTIVE:
		strcpy(ps_state_desc, "WLAN Active");
		break;
	default:
		strcpy(ps_state_desc, "Unknown");
		break;
	}
}

/*
 * Event: PS ENTER
 *
 * Application framework event to indicate the user that WIFI
 * has entered Power save mode.
 */

static void event_ps_enter(void *data)
{
	int ps_state = (int) data;
	char ps_state_desc[32];
	if (ps_state == WLAN_PDN) {
		dbg("NOTE: Due to un-availability of "
		    "enough dynamic memory for ");
		dbg("de-compression of WLAN Firmware, "
		    "exit from PDn will not\r\nwork with wm_demo.");
		dbg("Instead of wm_demo, pm_mc200_wifi_demo"
		    " application demonstrates\r\nthe seamless"
		    " exit from PDn functionality today.");
		dbg("This will be fixed in subsequent "
		    "software release.\r\n");
	}
	ps_state_to_desc(ps_state_desc, ps_state);
	dbg("Power save enter : %s", ps_state_desc);
}

/*
 * Event: PS EXIT
 *
 * Application framework event to indicate the user that WIFI
 * has exited Power save mode.
 */

static void event_ps_exit(void *data)
{
	int ps_state = (int) data;
	char ps_state_desc[32];
	ps_state_to_desc(ps_state_desc, ps_state);
	dbg("Power save exit : %s", ps_state_desc);
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
	case AF_EVT_NORMAL_USER_DISCONNECT:
		event_normal_user_disconnect(data);
		break;
	case AF_EVT_NORMAL_DHCP_RENEW:
		event_normal_dhcp_renew(data);
		break;
	case AF_EVT_PS_ENTER:
		event_ps_enter(data);
		break;
	case AF_EVT_PS_EXIT:
		event_ps_exit(data);
		break;
	default:
		break;
	}

	return 0;
}

/* Initialize the various modules that will be used by this application
 */
static void modules_init()
{
	int ret;

#ifdef CONFIG_ENABLE_TLS
	tls_lib_init();
#endif /* CONFIG_ENABLE_TLS */

	ret = wmstdio_init(UART0_ID, 0);
	if (ret != WM_SUCCESS) {
		dbg("Error: wmstdio_init failed.");
		appln_critical_error_handler((void *) -WM_FAIL);
	}
	/* Initialize time subsystem.
	 *
	 * Initializes time to 1/1/1970 epoch 0.
	 */
	ret = wmtime_init();
	if (ret != WM_SUCCESS) {
		dbg("Error: wmtime_init failed.");
		appln_critical_error_handler((void *) -WM_FAIL);
	}
	ret = part_init();
	if (ret != WM_SUCCESS) {
		dbg("Error: part_init failed.");
		appln_critical_error_handler((void *) -WM_FAIL);
	}
	/*
	 * Initialize Power Management Subsystem
	 */
	ret = pm_init();
	if (ret != WM_SUCCESS) {
		dbg("Error: pm_init failed");
		appln_critical_error_handler((void *) -WM_FAIL);
	}
	return;
}

/* The main function (entry point) for the application
 */
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
