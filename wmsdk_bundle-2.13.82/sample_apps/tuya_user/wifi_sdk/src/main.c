/*
 *  Copyright (C) 2008-2013, Marvell International Ltd.
 *  All Rights Reserved.
 */

/* WLAN Application using micro-AP and station
 *
 * Summary:
 *
 * This application showcases the end-to-end functionality of the SDK,
 * from configuring the device for the first time (provisioning),
 * to connection with the configured APs, to advertising mdns services,
 * to providing HTTP/HTML interfaces for interaction with the device,
 * to communicating with the cloud server.
 * This application also showcases the use of advanced features like
 * overlays, power management.
 *
 * The typical scenario in which you would use wm_demo, is to copy it
 * as your project and make modifications to the same.
 *
 * Description:
 *
 * The application is written using Application Framework that
 * simplifies development of WLAN networking applications.
 *
 * WLAN Initialization:
 *
 * When the application framework is started, it starts up the WLAN
 * sub-system and initializes the network stack. The app receives the event
 * when the WLAN subsystem has been started and initialized.
 *
 * If the device is not provisioned, Application framework sends an event
 * to the app to start a micro-AP network along with a DHCP server. This
 * will create a WLAN network and also creates a IP network interface
 * associated with that WLAN network. The DHCP server allows devices
 * to associate and connect to this network and establish an IP level
 * connection.
 *
 * When the network is initialized and ready, app will receive a
 * Micro-AP started event.
 *
 * Micro-AP Started:
 *
 * The micro-AP network is now up and ready.
 *
 * The app starts the web-server which will then listen to incoming http
 * connections (e.g, from a browser).
 *
 * By default, device can be accessed at URI http://192.168.10.1
 * Through the Web UI, user can provision the device to an Access Point.
 * Once the device is provisioned successfully to the configured Access
 * Point, app will receive a CONNECTED event.
 *
 * CONNECTED:
 *
 * Now the station interface of the device is up and app takes the
 * Micro-AP interface down. App enables MC200 + WiFi power save mode
 *
 * Cloud support:
 * By default, cloud support is not active. User can activate it by
 * executing following commands on the prompt of the device:
 * psm-set cloud enabled 1
 * psm-set cloud url "http://<ip-address>/cloud"
 * After the device reboot, cloud will get activated.
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
#include <wm_net.h>
#include <reset_prov_helper.h>
#include <power_mgr_helper.h>
#include <httpd.h>
//#include <wmcloud.h>
#include <led_indicator.h>
#include <board.h>
#include <dhcp-server.h>
#include <wmtime.h>
#include <psm.h>
#include <ftfs.h>
#include <rfget.h>
#include <diagnostics.h>
#include <mdev_gpio.h>
#include <healthmon.h>
#include "app_psm.h" // include NETWORK_MOD_NAME
#include <board.h>
#include <mc200_gpio.h>
#include <mc200_pinmux.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <provisioning.h>
#include <wm_net.h>
#include <json.h>
#include "provisioning_int.h"
#include "app_network_config.h"
#include "device.h"

/*-----------------------define declarations----------------------*/
#define UAP_DOWN_TIMEOUT (10 * 1000)

// include by NETWORK_MOD_NAME
#define VAR_UAP_SSID		"uap_ssid"
//#define VAR_PROV_KEY        "prov_key"
#define WF_CFG_MODE   "wf_cfg_mode" // include by NETWORK_MOD_NAME
#define SMART_CFG_SUCCESS  "smart_cfg_success" // include by NETWORK_MOD_NAME
typedef enum {
    SMART_CFG = 0,
    AP_CFG
}WF_CFG_MODE_E;

typedef struct wi_cfg_cntl_s {
    WF_CFG_MODE_E wf_cfg_mode;
    int is_smart_cfg_ok;
}WF_CFG_CNTL_S;

/*-----------------------function declarations----------------------*/

/*-----------------------Global declarations----------------------*/
static char g_uap_ssid[IEEEtypes_SSID_SIZE + 1];
appln_config_t appln_cfg = {
	.passphrase = "888888888",
	.wps_pb_gpio = -1,
	.reset_prov_pb_gpio = -1
};

int ftfs_api_version = 100;
char *ftfs_part_name = "ftfs";

struct fs *fs;

static os_timer_t uap_down_timer;
static WF_CFG_CNTL_S wf_cfg_cntl;

/*-----------------------function declarations----------------------*/
static int set_smart_cfg_ok(const int is_ok);
static int set_wf_cfg_mode(const WF_CFG_MODE_E cfg);
static int select_cfg_mode_for_next(void);
static void set_smart_cfg(void);
static int tuya_user_init(void);

// lan net config
int lan_set_net_work(char *ssid,char *passphrase)
{
    int provisioning_mode = 0;
    int err = -1;

    if(NULL == ssid) {
       err = -WM_E_PERM;
       goto ERR_OUT;
    }

    provisioning_mode = prov_get_state();
    if(PROVISIONING_STATE_SET_CONFIG == provisioning_mode) {
        err = -WM_FAIL;
        goto ERR_OUT;
    }

    static struct wlan_network lan_curr_net;
    memset(&lan_curr_net, 0, sizeof(struct wlan_network));

    strncpy(lan_curr_net.ssid, ssid, IEEEtypes_SSID_SIZE); // copy ssid
    lan_curr_net.channel = 0;
    if(NULL == passphrase) {
        lan_curr_net.security.type = WLAN_SECURITY_NONE;
    }else {
        lan_curr_net.security.type = WLAN_SECURITY_WILDCARD;

        int len = strlen(passphrase);
        strncpy(lan_curr_net.security.psk, passphrase, len+1);
        lan_curr_net.security.psk_len = len;
    }
    lan_curr_net.address.addr_type = ADDR_TYPE_DHCP;
    
    err = prov_set_network(PROVISIONING_WLANNW, &lan_curr_net);

ERR_OUT:
    return err;
}



/** Provisioning done timer call back function
 * Once the provisioning is done, we wait for provisioning client to send
 * AF_EVT_PROV_CLIENT_DONE which stops uap and dhcp server. But if any case
 * client doesn't send AF_EVT_PROV_CLIENT_DONE event, then we wait for
 * 60seconds(timer) to shutdown UAP.
 */
static void uap_down_timer_cb(os_timer_arg_t arg)
{
	if (is_uap_started()) {
		//hp_mdns_deannounce(net_get_uap_handle());
		app_uap_stop();
	}
}

/* This function initializes the SSID with the PSM variable network.uap_ssid
 * If the variable is not found, the default value is used.
 * To change the ssid, please set the network.uap_ssid variable
 * from the console.
 */
void appln_init_ssid()
{
    #if 0
	if (psm_get_single(NETWORK_MOD_NAME, VAR_UAP_SSID, g_uap_ssid,
			sizeof(g_uap_ssid)) == WM_SUCCESS) {
		dbg("Using %s as the uAP SSID", g_uap_ssid);
		appln_cfg.ssid = g_uap_ssid;
		// appln_cfg.hostname = g_uap_ssid;
	} else {
			uint8_t my_mac[6];

			memset(g_uap_ssid, 0, sizeof(g_uap_ssid));
			wlan_get_mac_address(my_mac);
			/* Provisioning SSID */
			snprintf(g_uap_ssid, sizeof(g_uap_ssid),
				 "TuyaSmart-%02X%02X", my_mac[4], my_mac[5]);
			dbg("Using %s as the uAP SSID", g_uap_ssid);
			appln_cfg.ssid = g_uap_ssid;
			// appln_cfg.hostname = g_uap_ssid;

            // store UAP_SSID 
            psm_set_single(NETWORK_MOD_NAME, VAR_UAP_SSID, appln_cfg.ssid);
	}
    #else

    uint8_t my_mac[6];
    
    memset(g_uap_ssid, 0, sizeof(g_uap_ssid));
    wlan_get_mac_address(my_mac);
    /* Provisioning SSID */
    snprintf(g_uap_ssid, sizeof(g_uap_ssid),
         "TuyaSmart-%02X%02X", my_mac[4], my_mac[5]);
    dbg("Using %s as the uAP SSID", g_uap_ssid);
    appln_cfg.ssid = g_uap_ssid;
    // appln_cfg.hostname = g_uap_ssid;
    #endif
}

#if 0
#define KEY_LEN 16
uint8_t prov_key[KEY_LEN + 1]; /* One extra length to store \0" */

int wmdemo_get_prov_key(uint8_t *prov_key)
{

	if (psm_get_single(NETWORK_MOD_NAME, VAR_PROV_KEY,
			   (char *)prov_key,
			   KEY_LEN + 1) == WM_SUCCESS) {
		if (strlen((char *)prov_key) == KEY_LEN) {
			dbg("Using key from psm %s", prov_key);
			prov_ezconn_set_device_key(prov_key, KEY_LEN);
			return KEY_LEN;
		} else {
			dbg("Found incorrect prov_key. Starting provisioning"
			    " without key");
			dbg("You can set 16byte key using below command and "
			    "reboot the board");
			dbg("psm-set network prov_key <16byte key>");
			memset(prov_key, 0 , KEY_LEN);
			return 0;
		}
	} else {
		dbg("No prov_key found. Starting provisioning without key");
		return 0;
	}
}
#endif

/*
 * A simple HTTP Web-Service Handler
 *
 * Returns the string "Hello World" when a GET on http://<IP>/hello
 * is done.
 */
// delete by nzy 20150606
#if 0
char *hello_world_string = "Hello World\n";

int hello_handler(httpd_request_t *req)
{
	char *content = hello_world_string;
	httpd_send_response(req, HTTP_RES_200,
			    content, strlen(content),
			    HTTP_CONTENT_PLAIN_TEXT_STR);
	return WM_SUCCESS;
}

static int wm_demo_get_ui_link(httpd_request_t *req)
{
	return cloud_get_ui_link(req);
}

struct httpd_wsgi_call wm_demo_http_handlers[] = {
	{"/hello", HTTPD_DEFAULT_HDR_FLAGS, 0,
	hello_handler, NULL, NULL, NULL},
	{"/cloud_ui", HTTPD_DEFAULT_HDR_FLAGS | HTTPD_HDR_ADD_PRAGMA_NO_CACHE,
	0, wm_demo_get_ui_link, NULL, NULL, NULL},
};

static int wm_demo_handlers_no =
	sizeof(wm_demo_http_handlers) / sizeof(struct httpd_wsgi_call);

#endif

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
 * Register Web-Service handlers
 *
 */
// delete by nzy 20150606
#if 0
int register_httpd_handlers()
{
	return httpd_register_wsgi_handlers(wm_demo_http_handlers,
		wm_demo_handlers_no);
}
#endif

/* This function must initialize the variables required (network name,
 * passphrase, etc.) It should also register all the event handlers that are of
 * interest to the application.
 */
int appln_config_init()
{
    // do not user mdns ,so delete it
    #if 0
	/* Initialize service name for mdns */
	snprintf(appln_cfg.servname, MAX_SRVNAME_LEN, "wm_demo");
    appln_cfg.wps_pb_gpio = board_button_1();
    #endif

    #if 0
	/* Initialize reset to provisioning push button settings */
	appln_cfg.reset_prov_pb_gpio = board_button_2();
    #endif

    /* Initialize power management */
	hp_pm_init();
	return 0;
}

/*-----------------------Local declarations----------------------*/
static int provisioned;
#if 0
static uint8_t mdns_announced;
#endif

/* This function stops various services when
 * device gets disconnected or reset to provisioning is done.
 */
	// delete by nzy 20150606
#if 0
static void stop_services()
{
	wm_demo_cloud_stop();
	led_off(board_led_1());
}
#endif

/* This function starts various services when
 * device get connected to a network.
 */

// delete by nzy 20150606
#if 0
static void start_services()
{
	dbg("Start Cloud");
	wm_demo_cloud_start();
}
#endif
/*
 * Event: INIT_DONE
 *
 * The application framework is initialized.
 *
 * The data field has information passed by boot2 bootloader
 * when it loaded the application.
 *
 * ?? What happens if app is loaded via jtag
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

	int err = os_timer_create(&uap_down_timer,
				  "uap-down-timer",
				  os_msec_to_ticks(UAP_DOWN_TIMEOUT),
				  &uap_down_timer_cb,
				  NULL,
				  OS_TIMER_ONE_SHOT,
				  OS_TIMER_NO_ACTIVATE);
	if (err != WM_SUCCESS) {
		dbg("Unable to start uap down timer");
	}
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
 *      -- Start mDNS and advertize services
 *	-- Start HTTP Server
 *	-- Register WSGI handlers for HTTP server
 */
static void event_wlan_init_done(void *data)
{
	int ret;
	/* We receive provisioning status in data */
	provisioned = (int)data;

	dbg("Event: WLAN_INIT_DONE provisioned=%d", provisioned);
    
	/* Initialize ssid to be used for uAP mode */
	appln_init_ssid();

    // todo:tuya user init
    tuya_user_init();

	if (provisioned) {
        set_wf_gw_status(STAT_STA_UNCONN);
		app_sta_start();
        #if 0
		/* Load  CLOUD overlay in memory */
		wm_demo_load_cloud_overlay();
        #endif        
	} else {
        #if 0
        #ifndef APPCONFIG_PROV_EZCONNECT
		app_uap_start_with_dhcp(appln_cfg.ssid, appln_cfg.passphrase);
		/* Load WPS overlay in memory */
		wm_demo_load_wps_overlay();
        #else
        #if 0
		int keylen = wmdemo_get_prov_key(prov_key);
		app_ezconnect_provisioning_start(prov_key, keylen);
        #else
        app_ezconnect_provisioning_start(NULL, 0);
        #endif
        #endif
        #else
        if(SMART_CFG == wf_cfg_cntl.wf_cfg_mode){
            set_wf_gw_status(STAT_UNPROVISION);
            app_ezconnect_provisioning_start(NULL, 0);
        }else {
            app_uap_start_with_dhcp(appln_cfg.ssid, appln_cfg.passphrase);
        }
        #endif
	}

#if 0
	if (provisioned)
		hp_configure_reset_prov_pushbutton();
#endif

#if APPCONFIG_MDNS_ENABLE
	/*
	 * Start mDNS and advertize our hostname using mDNS
	 */
	dbg("Starting mdns");
	app_mdns_start(appln_cfg.hostname);
#endif /* APPCONFIG_MDNS_ENABLE */

	/*
	 * Start http server and enable webapp in the
	 * FTFS partition on flash
	 */
	ret = app_httpd_with_fs_start(ftfs_api_version, ftfs_part_name, &fs);
	if (ret != WM_SUCCESS)
		dbg("Error: Failed to start HTTPD");

	/*
	 * Register /hello http handler
	 */
	// delete by nzy 20150606
	#if 0
	register_httpd_handlers();
    #endif

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
	ret = rfget_cli_init();
	if (ret != WM_SUCCESS)
		dbg("Error: rfget_cli_init failed");
	ret = wlan_cli_init();
	if (ret != WM_SUCCESS)
		dbg("Error: wlan_cli_init failed");

	if (!provisioned) {
		/* Start Slow Blink */
		//led_slow_blink(board_led_2());
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
static void event_uap_started(void *data)
{
	dbg("Event: Micro-AP Started");
	if (!provisioned) {
		dbg("Starting provisioning");
        #if APPCONFIG_WPS_ENABLE
		hp_configure_wps_pushbutton();
		wm_demo_wps_cli_init();
		app_provisioning_start(PROVISIONING_WLANNW |
				       PROVISIONING_WPS);
        #else
		app_provisioning_start(PROVISIONING_WLANNW);
        #endif /* APPCONFIG_WPS_ENABLE */

	}
}

/*
 * Event: PROV_DONE
 *
 * Provisioning is complete. We can stop the provisioning
 * service.
 *
 * Stop WPS.
 *
 * Enable Reset to Prov Button.
 */
static void event_prov_done(void *data)
{
    #if 0
	hp_configure_reset_prov_pushbutton();
    #endif

    #if 0
    #ifndef APPCONFIG_PROV_EZCONNECT
    #if APPCONFIG_WPS_ENABLE
	hp_unconfigure_wps_pushbutton();
    #endif /* APPCONFIG_WPS_ENABLE */
	wm_demo_wps_cli_deinit();
	app_provisioning_stop();
    #else
	app_ezconnect_provisioning_stop();
    #endif /* APPCONFIG_PROV_EZCONNECT */
    #else
    if(SMART_CFG == wf_cfg_cntl.wf_cfg_mode){
        app_ezconnect_provisioning_stop();
    }else {
        #if APPCONFIG_WPS_ENABLE
        hp_unconfigure_wps_pushbutton();
        #endif /* APPCONFIG_WPS_ENABLE */
        // delete by nzy 20150606
        #if 0
        wm_demo_wps_cli_deinit();
        #endif
        app_provisioning_stop();
    }
    #endif

	dbg("Provisioning successful");
}

/* Event: PROV_CLIENT_DONE
 *
 * Provisioning Client has terminated session.
 *
 * We can now safely stop the Micro-AP network.
 *
 * Note: It is possible to keep the Micro-AP network alive even
 * when the provisioning client is done.
 */
static void event_prov_client_done(void *data)
{
    #if 0
    #ifndef APPCONFIG_PROV_EZCONNECT
	int ret;

	//hp_mdns_deannounce(net_get_uap_handle());
	ret = app_uap_stop();
	if (ret != WM_SUCCESS)
		dbg("Error: Failed to Stop Micro-AP");
    #endif
    #else
    if(AP_CFG == wf_cfg_cntl.wf_cfg_mode) {
        int ret;
        ret = app_uap_stop();
	    if (ret != WM_SUCCESS) {
		    dbg("Error: Failed to Stop Micro-AP");
        }
    }
    #endif
}

/*
 * Event UAP_STOPPED
 *
 * Normally, we will get here when provisioning is complete,
 * and the Micro-AP network is brought down.
 *
 * If we are connected to an AP, we can enable IEEE Power Save
 * mode here.
 */
static void event_uap_stopped(void *data)
{
	dbg("Event: Micro-AP Stopped");
	hp_pm_wifi_ps_enable();
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
static void event_prov_wps_ssid_select_req(void *data)
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
static void event_prov_wps_successful(void *data)
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
static void event_prov_wps_unsuccessful(void *data)
{
	int ret;

	ret = app_httpd_with_fs_start(ftfs_api_version, ftfs_part_name, &fs);
	if (ret != WM_SUCCESS) {
		dbg("Error starting HTTP server");
	}
	return;
}

/*
 * Event: CONNECTING
 *
 * We are attempting to connect to the Home Network
 *
 * Note: We can come here:
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
	net_dhcp_hostname_set(appln_cfg.hostname);
	dbg("Connecting to Home Network");
	/* Start Fast Blink */
	//led_fast_blink(board_led_2());
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
	//void *iface_handle;
	char ip[16];

	//led_off(board_led_2());

	//led_on(board_led_1());

	app_network_ip_get(ip);
	dbg("Connected to Home Network with IP address = %s", ip);

    #if 0
	iface_handle = net_get_sta_handle();
	if (!mdns_announced) {
		hp_mdns_announce(iface_handle, UP);
		mdns_announced = 1;
	} else {
		hp_mdns_down_up(iface_handle);
	}
    #endif

    //  delete by nzy 20150606
    #if 0
	/* Load CLOUD overlay in memory */
	wm_demo_load_cloud_overlay();
	start_services();
    #endif

    if(is_uap_started()) {
        set_wf_gw_status(STAT_AP_STA_CONN);
    }else {
        set_wf_gw_status(STAT_STA_CONN);
    }

	/*
	 * If micro AP interface is up
	 * queue a timer which will take
	 * micro AP interface down.
	 */
	if (is_uap_started()) {
        int ret;
		ret = os_timer_activate(&uap_down_timer);
        PR_DEBUG("os_timer_activate ret:%d",ret);
		return;
	}

	hp_pm_wifi_ps_enable();
}

/*
 * Event: CONNECT_FAILED
 *
 * We attempted to connect to the Home AP, but the connection did
 * not succeed.
 *
 * This typically indicates:
 *
 * -- Authentication failed.
 * -- The access point could not be found.
 * -- We did not get a valid IP address from the AP
 *
 */
static void event_connect_failed(void *data)
{
	char failure_reason[32];

	if (*(app_conn_failure_reason_t *)data == AUTH_FAILED) {
		strcpy(failure_reason, "Authentication failure");        
    }else if (*(app_conn_failure_reason_t *)data == NETWORK_NOT_FOUND) {
		strcpy(failure_reason, "Network not found");
    }else if (*(app_conn_failure_reason_t *)data == DHCP_FAILED) {
		strcpy(failure_reason, "DHCP failure");
    }

	os_thread_sleep(os_msec_to_ticks(2000));
	dbg("Application Error: Connection Failed: %s", failure_reason);
	//led_off(board_led_1());
}

/*
 * Event: USER_DISCONNECT
 *
 * This means that the application has explicitly requested a network
 * disconnect
 *
 */
static void event_normal_user_disconnect(void *data)
{
	//led_off(board_led_1());
	dbg("User disconnect");
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

static void event_normal_pre_reset_prov(void *data)
{
	//hp_mdns_deannounce(net_get_mlan_handle());
}

static void event_normal_dhcp_renew(void *data)
{
	//void *iface_handle = net_get_mlan_handle();
	//hp_mdns_announce(iface_handle, REANNOUNCE);
}

static void event_normal_reset_prov(void *data)
{
	//led_on(board_led_2());
	/* Start Slow Blink */
	//led_slow_blink(board_led_2());

    // delete by nzy 20150606
    #if 0
	/* Stop services like cloud */
	stop_services();
    #endif

	/* Cancel the UAP down timer timer */
	os_timer_deactivate(&uap_down_timer);

	hp_pm_wifi_ps_disable();

    // delete by nzy 20150606
    #if 0
	/* Load WPS overlay in memory */
	wm_demo_load_wps_overlay();
    #endif

	/* Reset to provisioning */
	provisioned = 0;

    #if 0
	//mdns_announced = 0;
	hp_unconfigure_reset_prov_pushbutton();
    #ifndef APPCONFIG_PROV_EZCONNECT
	if (is_uap_started() == false) {
		app_uap_start_with_dhcp(appln_cfg.ssid, appln_cfg.passphrase);
	} else {
        #ifdef APPCONFIG_WPS_ENABLE
		hp_configure_wps_pushbutton();
		wm_demo_wps_cli_init();
		app_provisioning_start(PROVISIONING_WLANNW |
				       PROVISIONING_WPS);
        #else
		app_provisioning_start(PROVISIONING_WLANNW);
        #endif /* APPCONFIG_WPS_ENABLE */
	}
    #else
    app_ezconnect_provisioning_start(NULL, 0);
    #endif
    #else
    
    if(SMART_CFG == wf_cfg_cntl.wf_cfg_mode){
        set_wf_gw_status(STAT_UNPROVISION);
        app_ezconnect_provisioning_start(NULL, 0);
    }else {
        if (is_uap_started() == false) {
                app_uap_start_with_dhcp(appln_cfg.ssid, appln_cfg.passphrase);
        } else {
            #ifdef APPCONFIG_WPS_ENABLE
            hp_configure_wps_pushbutton();
            wm_demo_wps_cli_init();
            app_provisioning_start(PROVISIONING_WLANNW |
                                   PROVISIONING_WPS);
            #else
            app_provisioning_start(PROVISIONING_WLANNW);
            #endif /* APPCONFIG_WPS_ENABLE */
        }

    }
    #endif
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
    // add by nzy 20150610
    if(AF_EVT_NORMAL_CONNECTING == event || \
       AF_EVT_NORMAL_LINK_LOST == event || \
       AF_EVT_NORMAL_CONNECT_FAILED == event || \
       AF_EVT_NORMAL_USER_DISCONNECT == event) {
        if(is_uap_started()) {
            set_wf_gw_status(STAT_AP_STA_UNCONN);
        }else {
            set_wf_gw_status(STAT_STA_UNCONN);
        }
    }

	switch (event) {
	case AF_EVT_INIT_DONE:
		event_init_done(data);
		break;
	case AF_EVT_WLAN_INIT_DONE:
		event_wlan_init_done(data);
		break;
	case AF_EVT_NORMAL_CONNECTING:
        // add by nzy 20150526
        set_smart_cfg();
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
	case AF_EVT_PROV_WPS_SSID_SELECT_REQ:
		event_prov_wps_ssid_select_req(data);
		break;
	case AF_EVT_PROV_WPS_SUCCESSFUL:
		event_prov_wps_successful(data);
		break;
	case AF_EVT_PROV_WPS_UNSUCCESSFUL:
		event_prov_wps_unsuccessful(data);
		break;
	case AF_EVT_NORMAL_PRE_RESET_PROV:
		event_normal_pre_reset_prov(data);
		break;
	case AF_EVT_NORMAL_RESET_PROV:
		event_normal_reset_prov(data);
		break;
	case AF_EVT_UAP_STARTED:
        set_wf_gw_status(STAT_AP_STA_UNCONN);
		event_uap_started(data);
		break;
	case AF_EVT_UAP_STOPPED:
        set_wf_gw_status(STAT_STA_CONN);
		event_uap_stopped(data);
		break;
	case AF_EVT_PROV_DONE:
		event_prov_done(data);
		break;
	case AF_EVT_PROV_CLIENT_DONE:
		event_prov_client_done(data);
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

	ret = healthmon_init();
	if (ret != WM_SUCCESS) {
		dbg("Error: healthmon_init failed");
		appln_critical_error_handler((void *) -WM_FAIL);
	}

	ret = healthmon_start();
	if (ret != WM_SUCCESS) {
		dbg("Error: healthmon_start failed");
		appln_critical_error_handler((void *) -WM_FAIL);
	}

	/* Set the final_about_to_die handler of the healthmon */
	healthmon_set_final_about_to_die_handler
		((void (*)())diagnostics_write_stats);

	app_sys_register_diag_handler();

	app_sys_register_upgrade_handler();
	return;
}

static void heart_beat_test(int argc, char **argv)
{        
    OPERATE_RET op_ret;
    op_ret = httpc_gw_hearat();
    if(OPRT_OK != op_ret) {
        PR_ERR("op_ret:%d",op_ret);
        return;
    }
}

static void dev_wifi_reset(int argc, char **argv)
{
    int ret;
    ret = select_cfg_mode_for_next();
    if(WM_SUCCESS != ret) {
        PR_DEBUG("ret:%d.",ret);
    }else {
        app_network_set_nw_state(0);
    }

    pm_reboot_soc();
}

static void mk_prod_info(int argc, char **argv)
{
    if(argc != 3) {
        PR_ERR("invalid param");
        return;
    }

    if((strlen(argv[1]) > PROD_IDX_LEN) || \
       (strlen(argv[2]) != 12)) {
        PR_ERR("invalid param");
        return;
    }
    
    PR_DEBUG("prod_idx:%s",argv[1]);
    PR_DEBUG("mac:%s",argv[2]);

    set_gw_prodinfo(argv[1],argv[2]);
}

static void show_memory(int argc, char **argv)
{
    #if 0
    STATIC INT size = 32;
    CHAR *test = Malloc(size);
    if(NULL == test) {
        PR_DEBUG("size:%d",size);
        return;
    }
    size *= 2;
    #endif
    ShowSysMemPoolInfo();
}

static void dev_fac_restore(int argc, char **argv)
{
    set_gw_data_fac_reset();
}

static struct cli_command test_cmds[] = {
    {"mk-prod-info","mk-prod-info <prodect info index> <mac addr>",mk_prod_info},
    {"dev-wf-reset","",dev_wifi_reset},
    {"dev-fac-restore","",dev_fac_restore},
    {"heart-beat","",heart_beat_test},
    {"show-mem-info","",show_memory},
};

static void tuya_wf_cfg_init(void)
{
    // initiate for wifi config
    memset(&wf_cfg_cntl,0,sizeof(WF_CFG_CNTL_S));
    int ret;
    char tmp_buf[10];
    ret = psm_get_single(NETWORK_MOD_NAME, WF_CFG_MODE, tmp_buf,
			             sizeof(tmp_buf));
    if(WM_SUCCESS == ret) {
        wf_cfg_cntl.wf_cfg_mode = atoi(tmp_buf);
    }else {
        psm_set_single(NETWORK_MOD_NAME, WF_CFG_MODE,"0");
    }

    ret = psm_get_single(NETWORK_MOD_NAME, SMART_CFG_SUCCESS, tmp_buf,sizeof(tmp_buf));
    if(WM_SUCCESS == ret) {
        wf_cfg_cntl.is_smart_cfg_ok = atoi(tmp_buf);
    }else {
        psm_set_single(NETWORK_MOD_NAME, SMART_CFG_SUCCESS,"0");
    }
}

static int tuya_user_init(void)
{
    tuya_wf_cfg_init();

    OPERATE_RET op_ret;
    op_ret = device_init();
    if(OPRT_OK != op_ret) {
        PR_ERR("op_ret:%d",op_ret);
        return 1;
    }

#if 1
    int i;
    for(i = 0;i < CNTSOF(test_cmds);i++) {
        cli_register_command(&test_cmds[i]);
    }
#endif

    return 0;
}

unsigned char asc2hex(char asccode)
{
    unsigned char ret;

    if('0'<=asccode && asccode<='9')
        ret=asccode-'0';
    else if('a'<=asccode && asccode<='f')
        ret=asccode-'a'+10;
    else if('A'<=asccode && asccode<='F')
        ret=asccode-'A'+10;
    else
        ret=0;

    return ret;
}

void ascs2hex(unsigned char *hex,unsigned char *ascs,int srclen)
{
    unsigned char l4,h4;
    int i,lenstr;
    lenstr = srclen;
    
    if(lenstr%2) {
        lenstr -= (lenstr%2);
    }
    
    if(lenstr==0){
        return;
    }
    
    for(i=0; i < lenstr; i+=2) {
        h4=asc2hex(ascs[i]);
        l4=asc2hex(ascs[i+1]);
        hex[i/2]=(h4<<4)+l4;
    }
}

static void tuya_mac_init()
{
    // set mac addr
    OPERATE_RET op_ret;
    op_ret = ws_db_init();
    if(OPRT_OK != op_ret) {
        PR_ERR("ws_db_init error:%d",op_ret);
        return;
    }

    PROD_IF_REC_S prod_if;
    op_ret = ws_db_get_prod_if(&prod_if);
    if(OPRT_OK == op_ret) {
        BYTE mac[6];
        ascs2hex(mac,(BYTE *)prod_if.mac,strlen(prod_if.mac));
        wlan_set_mac_addr(mac);
    }
}

static int set_smart_cfg_ok(const int is_ok)
{
    wf_cfg_cntl.is_smart_cfg_ok = is_ok;
    if(is_ok) {
        return psm_set_single(NETWORK_MOD_NAME, SMART_CFG_SUCCESS,"1");
    }else {
        return psm_set_single(NETWORK_MOD_NAME, SMART_CFG_SUCCESS,"0");
    }
}

static int set_wf_cfg_mode(const WF_CFG_MODE_E cfg)
{
    wf_cfg_cntl.wf_cfg_mode = cfg;
    if(AP_CFG == cfg) {
        return psm_set_single(NETWORK_MOD_NAME, WF_CFG_MODE,"1");
    }else {
        return psm_set_single(NETWORK_MOD_NAME, WF_CFG_MODE,"0");
    }
}

static void set_smart_cfg(void)
{
    if(SMART_CFG == wf_cfg_cntl.wf_cfg_mode) {
        if(0 == wf_cfg_cntl.is_smart_cfg_ok) {
            set_smart_cfg_ok(1);
            dbg("%s:%d save smart cfg ok.",__FILE__,__LINE__);
            return;
        }
    }

    dbg("%s:%d nothing to do.",__FILE__,__LINE__);
}
    
static int select_cfg_mode_for_next(void)
{
    int ret;
    if(SMART_CFG == wf_cfg_cntl.wf_cfg_mode && \
       wf_cfg_cntl.is_smart_cfg_ok) {
        ret = set_smart_cfg_ok(0);
        if(ret != WM_SUCCESS) {
            return ret;
        }
        PR_DEBUG("set next smartconfig mode.");
    }else if(SMART_CFG == wf_cfg_cntl.wf_cfg_mode) {
        ret = set_wf_cfg_mode(AP_CFG);
        if(ret != WM_SUCCESS) {
            return ret;
        }
        PR_DEBUG("set next ap mode.");
    }else { // AP_CFG
        ret = set_wf_cfg_mode(SMART_CFG);
        if(ret != WM_SUCCESS) {
            return ret;
        }
        PR_DEBUG("set next smartconfig mode.");
        if(wf_cfg_cntl.is_smart_cfg_ok) {
            set_smart_cfg_ok(0);
        }
    }

    app_network_set_nw_state(0);
    pm_reboot_soc();

    return WM_SUCCESS;
}

void auto_select_wf_cfg(void)
{
    int ret;
    
    ret = select_cfg_mode_for_next();
    if(WM_SUCCESS != ret) {
        PR_ERR("select_cfg_mode_for_next error:%d",ret);
    }
}

void select_smart_cfg_wf(void)
{
    set_smart_cfg_ok(0);
    set_wf_cfg_mode(SMART_CFG);
    app_network_set_nw_state(0);
    pm_reboot_soc();

}

void select_ap_cfg_wf(void)
{
    set_smart_cfg_ok(0);
    set_wf_cfg_mode(AP_CFG);
    app_network_set_nw_state(0);
    pm_reboot_soc();
}

void single_dev_reset_factory(void)
{
    set_smart_cfg_ok(0);
    set_wf_cfg_mode(SMART_CFG);
    app_network_set_nw_state(0);
    set_gw_data_fac_reset();
}

int main()
{
	modules_init();

	dbg("Build Time: " __DATE__ " " __TIME__ "");

	appln_config_init();
    tuya_mac_init();

	/* Start the application framework */
	if (app_framework_start(common_event_handler) != WM_SUCCESS) {
		dbg("Failed to start application framework");
		appln_critical_error_handler((void *) -WM_FAIL);
	}

	return 0;
}
