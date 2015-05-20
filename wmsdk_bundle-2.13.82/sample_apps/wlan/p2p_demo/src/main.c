/*
 *  Copyright (C) 2008-2013, Marvell International Ltd.
 *  All Rights Reserved.
 */

/* Simple WLAN Application using P2P connection
 *
 * Summary:
 *
 * This application starts up as a P2P enabled node and can connect to
 * other such nodes. Other nodes can also scan for and connect to this
 * application.
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
 *
 * P2P Start:
 *
 * After the WLAN subsystem has been started, the application startes the P2P
 * module with the desired role (Auto Group Owner/Undecided). Once this is
 * done, the device is discoverable from other WiFi Direct enabled devices.
 * For the Undecided role, the actual role i.e. client or Group Owner (GO)
 * is decided after negotiation with other P2P node.
 *
 *
 * P2P Sessions:
 *
 * In the undefined role, a connection can be established either by selcting
 * this device from the scan list of a smartphone or by pressing the push
 * button on the device. The role (client/GO)
 *
 * In the Auto GO mode, the push button needs to be first pressed and then
 * a connection needs to be initiated from the smartphone.
 *
 *
 * Other services:
 *
 * In the Auto_GO or GO role, the application will also get UAP related events.
 * The application can then decide what action needs to be taken, like starting
 * various services(like HTTPD, mDNS, etc.)
 *
 * In the client role, the various station related events will be received and
 * the application can then decide the actions to be taken.
 *
 *
 * --------------------------------------------------------
 */
#include <wm_os.h>
#include <app_framework.h>
#include <wmtime.h>
#include <partition.h>
#include <appln_dbg.h>
#include <cli.h>
#include <wmstdio.h>
#include <cli_utils.h>
#include <wmsysinfo.h>
#include <wm_net.h>
#include <board.h>
#include <wmtime.h>
#include <psm.h>
#include <wm-tls.h>
#include <power_mgr_helper.h>
#include <mdev_gpio.h>
#include <push_button.h>

/*-----------------------Global declarations----------------------*/

int p2p_device_started;
int p2p_session_on;
int num_scan_results;
struct p2p_scan_result *p2p_scan_results;
static int p2p_timeout;

int start_scan_thread();

int ftfs_api_version = 100;
char *ftfs_part_name = "ftfs";

struct fs *fs;

/* Thread handle */
static os_thread_t app_thread;
/* Buffer to be used as stack */
static os_thread_stack_define(app_stack, 512);

static void p2p_demo_simulate_p2p_disconnect(int argc, char **argv)
{
	app_p2p_disconnect();
}

static void p2p_demo_simulate_p2p_stop(int argc, char **argv)
{
	app_p2p_stop();
}
static struct cli_command p2p_cmds[] = {
	{"p2p-disconnect", NULL, p2p_demo_simulate_p2p_disconnect},
	{"p2p-stop", NULL, p2p_demo_simulate_p2p_stop},
};
static void p2p_pb_callback(void)
{
	dbg("P2P pushbutton pressed");
	app_p2p_session_start(NULL, p2p_scan_results);
}

void hp_configure_p2p_pushbutton()
{
	push_button_set_cb(board_button_1(),
			   p2p_pb_callback);
}

void hp_unconfigure_p2p_pushbutton()
{
	push_button_set_cb(board_button_1(),
			   NULL);
}

int app_start_services()
{
	int ret;
	ret = app_httpd_with_fs_start(ftfs_api_version, ftfs_part_name, &fs);
	if (ret != WM_SUCCESS) {
		dbg("Error starting HTTP server");
	} else {
		dbg("Started HTTP");
	}
	return ret;
}

int app_stop_services()
{
	dbg("Stopping HTTPD");
	app_httpd_stop();
	return WM_SUCCESS;
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
	/* Initialize power management */
	hp_pm_init();
	/* Nothing to do here */
	return WM_SUCCESS;
}

/*-----------------------Local declarations----------------------*/

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
	int i;
	dbg("Event: WLAN_INIT_DONE");
	for (i = 0; i < sizeof(p2p_cmds) / sizeof(struct cli_command); i++)
		if (cli_register_command(&p2p_cmds[i]))
			wmprintf("Command register error");
	/* Configure a pushbutton to initiate a P2P connection when pressed */
	hp_configure_p2p_pushbutton();

	/* Start P2P */
	app_p2p_start(ROLE_UNDECIDED);
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
	char ip[16];

	app_p2p_ip_get(ip);
	dbg("Connected to Network with IP address = %s", ip);
	app_start_services();
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
	dbg("Connect Failed");
	os_thread_sleep(os_msec_to_ticks(1000));
	app_stop_services();
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
	app_stop_services();
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
	/* A small delay so that the user can see the LED indication */
	os_thread_sleep(os_msec_to_ticks(1000));
	app_stop_services();
}

static void event_uap_started(void *data)
{
	char ip[16];
	dbg("uAP Started");
	app_p2p_ip_get(ip);
	dbg("Started with IP address = %s", ip);
	app_start_services();
}

static void event_uap_stopped(void *data)
{
	dbg("uAP Stopped");
	app_stop_services();
}

static void ps_state_to_desc(char *ps_state_desc, int ps_state)
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
	if (ps_state == WLAN_DEEP_SLEEP) {
		dbg("Enter into PM4: %d milliseconds", p2p_timeout);
		pm_mc200_state(PM4, p2p_timeout);
	}
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
	app_p2p_role_data_t *app_p2p_role_data = NULL;
	char name[33];
	bzero(name, sizeof(name));
	switch (event) {
	case AF_EVT_INIT_DONE:
		event_init_done(data);
		break;
	case AF_EVT_WLAN_INIT_DONE:
		event_wlan_init_done(data);
		break;
	case AF_EVT_NORMAL_CONNECTED:
		event_normal_connected(data);
		break;
	case AF_EVT_NORMAL_CONNECT_FAILED:
		p2p_session_on = 0;
		hp_pm_reset_p2p_timeout_timer();
		event_connect_failed(data);
		break;
	case AF_EVT_NORMAL_LINK_LOST:
		p2p_session_on = 0;
		hp_pm_reset_p2p_timeout_timer();
		event_normal_link_lost(data);
		break;
	case AF_EVT_NORMAL_USER_DISCONNECT:
		p2p_session_on = 0;
		hp_pm_reset_p2p_timeout_timer();
		event_normal_user_disconnect(data);
		break;
	case AF_EVT_P2P_STARTED:
		app_p2p_role_data = data;
		if (app_p2p_role_data->data != NULL)
			strncpy(name, (char *)(app_p2p_role_data->data),
				sizeof(name));
		if (app_p2p_role_data->role == ROLE_AUTO_GO)
			dbg("P2P Auto GO Started: %s", name);
		else {
			dbg("P2P Device Started: %s", name);
			start_scan_thread();
			p2p_device_started = 1;
			hp_pm_reset_p2p_timeout_timer();
		}
		break;
	case AF_EVT_P2P_SESSION_STARTED:
		dbg("P2P Session started");
		p2p_session_on = 1;
		hp_pm_stop_p2p_timeout_timer();
		break;
	case AF_EVT_P2P_ROLE_NEGOTIATED:
		app_p2p_role_data = data;
		if (app_p2p_role_data->role == ROLE_GO)
			dbg("GO Role");
		else
			dbg("Client Role");
		break;
	case AF_EVT_P2P_FAILED:
		dbg("P2P Failed");
		p2p_device_started = 0;
		p2p_session_on = 0;
		hp_pm_reset_p2p_timeout_timer();
		break;
	case AF_EVT_P2P_FINISHED:
		dbg("P2P finished");
		p2p_device_started = 0;
		p2p_session_on = 0;
		dbg("Start Deep Sleep mode");
		wlan_deepsleepps_on();
		break;
	case AF_EVT_UAP_STARTED:
		event_uap_started(data);
		break;
	case AF_EVT_UAP_STOPPED:
		event_uap_stopped(data);
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

	ret = wmstdio_init(UART0_ID, 0);
	if (ret != WM_SUCCESS) {
		dbg("Error: wmstdio_init failed");
		appln_critical_error_handler((void *) -WM_FAIL);
	}

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
	 * Initialize Power Management Subsystem
	 */
	ret = pm_init();
	if (ret != WM_SUCCESS) {
		dbg("Error: pm_init failed");
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

	ret = gpio_drv_init();
	if (ret != WM_SUCCESS) {
		dbg("Error: gpio_drv_init failed");
		appln_critical_error_handler((void *) -WM_FAIL);
	}

	return;
}

static void p2p_demo_main(os_thread_arg_t data)
{
#ifdef PRINT_SCAN_RESULTS
	int i, j, len;
#endif

	dbg("P2P Demo : Main Thread Started");
	while (1) {
		if (p2p_device_started && !p2p_session_on) {
			dbg("Fetching scan results");
			p2p_scan_results = p2p_scan(&num_scan_results);
			if (num_scan_results) {
				wmprintf("");
				dbg("P2P Scan Result : %d "
						"entries in List",
						num_scan_results);
#ifdef PRINT_SCAN_RESULTS
				dbg
					("---------------------------------"
					 "---------------------------------"
					 "-------------------------");
				dbg
					("  # |    Intf Address   |       "
					"Device Name	|  Op mode |   "
					"Dev Address     |  SSID |");
				dbg
					("---------------------------------"
					 "---------------------------------"
					 "-------------------------");

				for (j = 0; j < num_scan_results;
						j++) {
					/* Index Number */
					wmprintf("\t %02d | ", j);

					/* Device ID */
					print_mac((const char *)
						p2p_scan_results[j].device_id);
					wmprintf("|");
					/** Device Name printing in smart way */
					len =
						((MAX_DEVICE_NAME_LEN - 6) -
						 strlen((const char *)
					p2p_scan_results[j].device_name)) / 2;
					for (i = 0; i < len; i++)
						wmprintf(" ");
					for (i = 0; i < strlen((const char *)
					p2p_scan_results[j].device_name); i++)
						wmprintf("%c",
					p2p_scan_results[j].device_name[i]);
					for (i = 0; i <
						((MAX_DEVICE_NAME_LEN - 6) -
						strlen((const char *)
							p2p_scan_results[j].
						device_name) - len); i++)
						wmprintf(" ");
					wmprintf("|");

					/* Group Capability */
					if (!(p2p_scan_results[j].
						group_capability &
							P2P_GROUP_OWNER_MASK))
						wmprintf("  Device  |");
					else
						wmprintf("    GO    |");

					if (memcmp(p2p_scan_results[j].
							device_address,
						"\x00\x00\x00\x00\x00\x00", 6)
						) {
						wmprintf(" ");
						print_mac((const char *)
							p2p_scan_results[j].
								device_address);
						wmprintf("|");
					}
					if (p2p_scan_results[j].group_capability
						 & P2P_GROUP_OWNER_MASK)
						wmprintf("  %s",
							p2p_scan_results
							[j].go_ssid);
					wmprintf("");
				}
				wmprintf("\r\n");
#endif
			} else
				dbg("P2P device not "
						"found");
			if (hp_pm_is_p2p_timeout_occurred()) {
				hp_unconfigure_p2p_pushbutton();
				app_p2p_stop();
			}
		} else {
			dbg("Not fetching scan results");
		}
		/* Sleep 5 seconds */
		os_thread_sleep(os_msec_to_ticks(5000));
	}
}

int start_scan_thread()
{
	return os_thread_create(&app_thread,    /* thread handle */
				"p2p_demo_thread",/* thread name */
				p2p_demo_main,  /* entry function */
				0,	    /* argument */
				&app_stack,     /* stack */
				OS_PRIO_2);     /* priority - medium low */
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
