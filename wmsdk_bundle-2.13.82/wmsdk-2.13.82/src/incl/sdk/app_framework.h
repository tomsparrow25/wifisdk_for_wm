/*! \file app_framework.h
 *  \brief The Application Framework
 *
 * The application framework (AF) provides a core state machine that is commonly
 * required by applications. It manages two wireless network interfaces,
 * micro-AP and station. The station interface can either be in the unconfigured
 * or the configured mode. The AF is so designed that it enables a wide variety
 * of use cases and pulls in only the code for the used APIs.
 *
 * \section af_features Application Framework Features
 *
 * The AF provides wrappers for other modules that provide an easy to use
 * interface. The AF provides the following features:
 *
 * a) Micro-AP Mode: State machine for starting and stopping the Micro-AP
 *    interface.
 *
 * b) Station Mode: State machine for starting and stopping the station
 *    interface. Typically the station interface requires information about the
 *    wireless network to connect to. This information includes parameters like
 *    Network name, security, passphrase among other things. The process of
 *    configuring this information in the device is called provisioning. AF
 *    allows a variety of ways to provision the device. Once the network
 *    information is configured, AF maintains this information in \link psm.h
 *    Persistent Storage Manager (PSM) \endlink so that the information is
 *    maintained across power down events.
 *
 * c) Marvell Provisioning Service: AF provides wrappers for Marvell
 *    Provisioning service. This service allows two ways for network
 *    provisioning:
 *
 *    -# Micro-AP based Provisioning: Users can connect to the Micro-AP
 *    network. The provisioning services provides HTTP handlers that can be used
 *    to perform network provisioning. Applications can use a webapp-based
 *    wizard to guide the user through the provisioning process.
 *    -# WPS based Provisioning: Users press a button on the Access Point (AP)
 *    and a button on this device. The WPS protocol is initiated which takes
 *    care of transferring the network credentials to the device.
 *
 *    The Marvell Provisioning Service is optional. Applications may choose to
 *    use their own mechanism of provisioning.
 *
 * d) HTTP Service: AF provides convenience wrappers for the HTTP Web
 *    Service. These wrappers implement the following commonly required
 *    functionality of the HTTP Web service:
 *
 *    -# Filesystem: A method to optionally attach a filesystem at the '/'
 *       location of the Web Server. This is useful when the Web Service has to
 *       service files like HTML/JS/JPEG.
 *    -# /sys/ Handlers: A default set of HTTP handlers is available at the
 *       '/sys/' location of the Web Server. These handlers provide basic
 *       information/configuration of the device
 *
 *  e) mDNS/DNS-SD Service: AF provides convenience wrappers for the mDNS/DNS-SD
 *     service.
 *
 *
 * \section af_usage Usage
 *
 * -# Applications can use the AF by calling the \ref app_framework_start()
 * function. A callback is registered with the call the
 * app_framework_start(). The AF invokes this callback handler on occurrence of
 * various events in the system. The set of events are documented in the \ref
 * app_ctrl_event_t data type. No other functions of the AF should be called
 * before the AF delivers the AF_EVT_INIT_DONE event to the callback handler.
 * -# Applications can control the Micro-AP interface by making calls to the
 * app_uap_start_with_dhcp() and app_uap_stop() functions.
 * -# Applications can control the station interface by making calls to the
 * app_sta_start(), app_sta_start_by_network() and app_sta_stop()
 * functions. Once the station interface is started, the AF checks for events
 * like link-loss and deauth that cause a network disconnection. The AF
 * automatically reinitiates connection attempts on occurrence of these events.
 * -# Applications can use Marvell Provisioning service by calling
 * app_provisioning_start() and app_provisioning_stop() functions. Applications
 * also have an option to use their own provisioning mechanism. If applications
 * do so, they should inform the application framework of the configured network
 * using the app_configure_network() function. This will inform the AF of the
 * network configuration so that the information can be stored in PSM for future
 * use. Applications can request the AF to forget the network configuration by
 * using the app_reset_configured_network() call.
 * -# Applications can control the Web Server using the app_httpd_start(),
 * app_httpd_with_fs_start() and app_httpd_stop() functions. Applications can
 * control what HTTP handlers to expose by using the functions
 * app_sys_register_upgrade_handler(), app_sys_register_diag_handler() and
 * app_sys_register_perf_handler().
 * -# Applications can control the mDNS/DNS-SD service using the
 * app_mdns_start() and app_mdns_stop() calls.
 *
 * @cond uml_diag
 * \section af_sm State Machine
 *
 * The application framework state diagram is as shown below. The Yellow boxes
 * indicate the various states. The top half is the name of the state, and the
 * bottom half consists of any code that is executed on entering that
 * state. Labels on the transitions indicate when that particular transition is
 * taken.
 *
 * @startuml{prov_normal.jpg}
 *
 * [*] --> AF_INIT_SYSTEM
 * AF_INIT_SYSTEM: wlan_start()
 * AF_INIT_SYSTEM --> microAP : WLAN_REASON_SUCCESS
 * AF_INIT_SYSTEM --> Station : WLAN_REASON_SUCCESS
 * state microAP {
 * [*] --> AF_UAP_INIT
 * AF_UAP_INIT --> AF_UAP_STARTING : app_uap_start
 * AF_UAP_STARTING: wlan_start_network
 * AF_UAP_STARTING --> AF_UAP_UP : WLAN_REASON_UAP_SUCCESS
 * AF_UAP_UP --> AF_UAP_INIT : app_uap_stop();
 * AF_UAP_UP --> Station : app_configure_network();
 * }
 *
 *
 * state Station {
 *   state AF_NORMAL_INIT
 *   state AF_NORMAL_CONNECTING
 *   state AF_NORMAL_CONNECTED
 *   state AF_NORMAL_DISCONNECTED
 *   state AF_NORMAL_NW_CHANGED
 *    [*] --> AF_UNCONFIGURED : station_configured = 0
 *    [*] --> AF_CONFIGURED : station_configured = 1
 *   AF_UNCONFIGURED --> AF_NORMAL_INIT : app_configure_network() OR \n app_sta_start_by_network();
 *   AF_CONFIGURED --> AF_NORMAL_INIT : app_sta_start()
 *   AF_NORMAL_INIT --> AF_NORMAL_CONNECTING
 *   AF_NORMAL_CONNECTING: load_nw();\nwlan_connect();
 *   AF_NORMAL_CONNECTING --> AF_NORMAL_CONNECTED : WLAN_REASON_SUCCESS
 *   AF_NORMAL_CONNECTING --> AF_NORMAL_CONNECTING : WLAN_REASON_AUTH_FAILED  \n WLAN_REASON_NW_NOT_FOUND \n WLAN_REASON_ADDR_FAILED
 *   AF_NORMAL_CONNECTED --> AF_NORMAL_CONNECTING : WLAN_REASON_LINK_LOSS
 *   AF_NORMAL_CONNECTED --> AF_NORMAL_NW_CHANGED : POST /sys/network
 *   AF_NORMAL_CONNECTED --> AF_UNCONFIGURED : Reset to Prov.
 *   AF_NORMAL_NW_CHANGED : wlan_disconnect(); \n unload_nw();
 *   AF_NORMAL_NW_CHANGED --> AF_NORMAL_CONNECTING
 *   AF_NORMAL_CONNECTING --> AF_NORMAL_DISCONNECTED : app_sta_stop();
 *   AF_NORMAL_CONNECTED --> AF_NORMAL_DISCONNECTED : app_sta_stop();
 *   AF_NORMAL_DISCONNECTED --> AF_NORMAL_INIT : app_sta_start();
 * }
 *
 *
 * @enduml
 *
 * There is an independent P2P state machine as well, as shown below.
 *
 * @startuml{p2p_states.jpg}
 *
 * [*] --> AF_STATE_P2P_INIT
 * AF_STATE_P2P_INIT --> AF_STATE_P2P_INIT : app_p2p_start()
 * AF_STATE_P2P_INIT --> AF_STATE_P2P_DEVICE_STARTED : APP_EVT_P2P_STARTED(ROLE_UNDECIDED)
 * AF_STATE_P2P_INIT --> AF_STATE_P2P_GO_UP : APP_EVT_P2P_STARTED(ROLE_AUTO_GO)
 * AF_STATE_P2P_GO_UP --> AF_STATE_P2P_GO_UP : AF_EVT_UAP_STARTED \n /dhcp_server_start()
 * AF_STATE_P2P_GO_UP --> AF_STATE_P2P_GO_UP : app_p2p_session_start()
 * AF_STATE_P2P_GO_UP --> AF_STATE_P2P_INIT : APP_EVT_P2P_FINISHED
 * AF_STATE_P2P_DEVICE_STARTED --> AF_STATE_P2P_DEVICE_STARTED : app_p2p_session_start()
 * AF_STATE_P2P_DEVICE_STARTED --> AF_STATE_P2P_SESSION_STARTED : AF_EVT_P2P_SESSION_STARTED
 * AF_STATE_P2P_DEVICE_STARTED --> AF_STATE_P2P_INIT : APP_EVT_P2P_FINISHED
 * AF_STATE_P2P_SESSION_STARTED --> AF_STATE_P2P_SESSION_COMPLETED : APP_EVT_P2P_SESSION_SUCCESSFUL
 * AF_STATE_P2P_SESSION_STARTED --> AF_STATE_P2P_DEVICE_STARTED : APP_EVT_P2P_SESSION_FAILED
 * AF_STATE_P2P_SESSION_STARTED --> AF_STATE_P2P_INIT : APP_EVT_P2P_FINISHED
 * AF_STATE_P2P_SESSION_STARTED --> AF_STATE_P2P_GO_UP : AF_EVT_P2P_ROLE_NEGOTIATED(ROLE_GO)
 * AF_STATE_P2P_SESSION_COMPLETED --> AF_STATE_P2P_CONNECTED : APP_EVT_NORMAL_CONNECTED
 * AF_STATE_P2P_SESSION_COMPLETED --> AF_STATE_P2P_INIT : AF_EVT_P2P_FINISHED
 * AF_STATE_P2P_SESSION_COMPLETED --> AF_STATE_P2P_DEVICE_STARTED : AF_EVT_NORMAL_CONNECT_FAILED \n AF_EVT_NORMAL_LINK_LOST
 * AF_STATE_P2P_CONNECTED --> AF_STATE_P2P_DEVICE_STARTED : AF_EVT_NORMAL_CONNECT_FAILED \n AF_EVT_NORMAL_USER_DISCONNECT \n AF_EVT_NORMAL_LINK_LOST
 * AF_STATE_P2P_CONNECTED --> AF_STATE_P2P_INIT : AF_EVT_P2P_FINISHED
 * AF_STATE_P2P_SESSION_COMPLETED : app_connect_to_p2p_go()
 *
 * @enduml
 *
 * @endcond
 */
/*  Copyright 2008-2013, Marvell International Ltd.
 *  All Rights Reserved.
 */

#ifndef _APP_FRAMEWORK_H_
#define _APP_FRAMEWORK_H_

#include <wmstats.h>
#include <fs.h>
#include <wlan.h>
#include <mdns.h>
#include <json.h>
#include <provisioning.h>
#include <wmerrno.h>

#ifdef CONFIG_P2P
#include <p2p.h>
#endif

/** Application Framework Error Codes
 */
enum wm_af_errno {
	WM_E_AF_ERRNO_BASE = MOD_ERROR_START(MOD_AF),
	/** Error while reading network configuration data */
	WM_E_AF_NW_RD,
	/** Error while writing network configuration data */
	WM_E_AF_NW_WR,
	/** Error in updating reboot timer settings */
	WM_E_AF_REB_TMR_WR,
	/** Network add failed */
	WM_E_AF_NW_ADD,
	/** Network remove failed */
	WM_E_AF_NW_DEL,
	/** Network connect failed */
	WM_E_AF_NW_CONN,
	/** Failed to start wlcmgr */
	WM_E_AF_WLCMGR_FAIL,
	/** PSM Initialization failed */
	WM_E_AF_PSM_INIT,
	/** Watchdog Initialization failed */
	WM_E_AF_WD_INIT,
	/* This should be returned by application
	 * to skip default action taken by app framework*/
	WM_E_SKIP_DEF_ACTION,
};

/** App-framework failure reason codes
 */
typedef enum {
	/** In case AP-is down or network unavailable */
	NETWORK_NOT_FOUND,
	/** In case of authentication failure */
	AUTH_FAILED,
	/** In case of failure in assigning IP address */
	DHCP_FAILED,
#ifdef CONFIG_P2P
	/** In case of a P2P failure after starting a session */
	P2P_CONNECT_FAILED,
#endif
} app_conn_failure_reason_t;

/** Application Framework Event Handler
 *
 * This is the event handler that applications register with the application
 * framework for handling any events. This function is executed in the context
 * of the application framework's thread.
 */
typedef	int (*app_event_handler_t)(int event, void *data);


/** Start Application Framework
 *
 * This is the main function that starts the application framework
 *
 * If you wish to use the application framework, call this from the
 * application's main() function. This function will:
 * -# Start the application framework thread
 * -# Initialize the \link wlan.h Wireless Connection Manager \endlink
 * -# Initialize the \link psm.h Persistent State Manager (PSM) \endlink
 * -# Read the network configuration state from the psm
 * -# Informs the application's callback handler with events \ref
 * AF_EVT_INIT_DONE followed by \ref AF_EVT_WLAN_INIT_DONE
 *
 * Any functions related to the WiFi like app_sta_start() or
 * app_uap_start_with_dhcp() should be called only after the \ref
 * AF_EVT_WLAN_INIT_DONE event. Any other application framework functions should
 * be called only after the \ref AF_EVT_INIT_DONE event.
 *
 * \param[in] event_handler the application's event handling function \ref
 * app_event_handler_t
 *
 * \return WM_SUCCESS on success
 * \return error code otherwise
 */
int app_framework_start(app_event_handler_t event_handler);

/** Events processed by application controller.
 *
 * These are the events that are processed by the application controller. The
 * application is notified of these events so that it can show appropriate
 * indicators to the user or take appropriate actions.
 *
 */
typedef enum
{
	/* These are application framework's internal states that should be
	 * ignored by the application. */
	AF_EVT_INTERNAL_STATE_ENTER,
	AF_EVT_INTERNAL_WLAN_INIT_FAIL,
	AF_EVT_INTERNAL_PROV_REQUESTED,
	AF_EVT_INTERNAL_PROV_NW_SET,
	AF_EVT_INTERNAL_UAP_REQUESTED,
	AF_EVT_INTERNAL_UAP_STOP,
	AF_EVT_INTERNAL_STA_REQUESTED,
#ifdef CONFIG_P2P
	AF_EVT_INTERNAL_P2P_REQUESTED,
	AF_EVT_INTERNAL_P2P_SESSION_REQUESTED,
	AF_EVT_INTERNAL_P2P_SESSION_SUCCESSFUL,
	AF_EVT_INTERNAL_P2P_AP_SESSION_SUCCESSFUL,
	AF_EVT_INTERNAL_P2P_SESSION_FAILED,
#endif
	/** The WM is up and all basic modules have been initialized, perform
	 * any notification-cleanups/inits if required. The structure
	 * app_init_state is passed as data along with this event.
	 */
	AF_EVT_INIT_DONE,
	/** The WLAN subsystem has been initialized. The data that is passed
	 * along with this is either \ref APP_NETWORK_NOT_PROVISIONED or
	 * APP_NETWORK_PROVISIONED. Applications can decide whether to start the
	 * micro-AP or station interface using app_uap_start_with_dhcp() or
	 * app_sta_start() calls respectively.
	 */
	AF_EVT_WLAN_INIT_DONE,
	/** This event is provided so that the application can be notified when
	 * something critical happens in this system. The application can decide
	 * anyway to recover from this critical situation. The data field
	 * indicates the various critical error conditions If application wants
	 * to allow app_framework to skip any default action that the
	 * application framework takes, then it should return
	 * -WM_E_SKIP_DEF_ACTION.
	 */
	AF_EVT_CRITICAL,
	/** We are now starting in normal mode */
	AF_EVT_NORMAL_INIT,
	/** Attempting to connect in normal mode */
	AF_EVT_NORMAL_CONNECTING,
	/** We have successfully connected to the network. Note that
	 * we can get here after normal bootup or after successful
	 * provisioning.
	 */
	AF_EVT_NORMAL_CONNECTED,
	/** One connection attempt to the network has failed. Note that we can
	 * get here after normal bootup or after an unsuccessful provisioning. A
	 * reference to \ref app_conn_failure_reason_t object gets propagated
	 * which specifies the failure reason.
	 *
	 * \ref app_event_handler_t function should return WM_SUCCESS if it
	 * wishes to attempt reconnection upon receiving this event.  Returning
	 * non-zero value shall cause the application framework to stop
	 * reconnection attempts and it moves the station into disconnected
	 * state. In this state app_sta_start() (or its variants) can be used to
	 * attempt reconnection.
	 */
	AF_EVT_NORMAL_CONNECT_FAILED,
	/** We were connected to the network, but the link was lost
	 * intermittently.
	 */
	AF_EVT_NORMAL_LINK_LOST,
	/** We were connected to the network, but the channel switch
	 * announcement is received */
	AF_EVT_NORMAL_CHAN_SWITCH,
	/** We were connected to the WPS network, the WPS module asked to
	 * be disconnected. This event is delivered regardless of whether WPS is
	 * successful or not.
	 */
	AF_EVT_NORMAL_WPS_DISCONNECT,
	/** We were connected to the network, the user (application) asked to
	 * be disconnected.
	 */
	AF_EVT_NORMAL_USER_DISCONNECT,
	/** Network settings have been modified and updated. Attempt to connect
	 * to that network now.
	 */
	AF_EVT_NORMAL_ADVANCED_SETTINGS,
	/** DHCP lease was expired and hence the DHCP address was renewed. */
	AF_EVT_NORMAL_DHCP_RENEW,
	/** Provisioning is complete, attempting to connect to
	 * network. Include some progress indicators while the
	 * connection attempt either succeeds or fails.
	 */
	AF_EVT_PROV_DONE,
	/** An SSID with an active WPS session is found. WPS negotiation will
	 * now be started with this AP.
	 */
	AF_EVT_PROV_WPS_SSID_SELECT_REQ,
	/** The WPS session started.
	 */
	AF_EVT_PROV_WPS_START,
	/** The WPS session failed due to timeout
	 */
	AF_EVT_PROV_WPS_REQ_TIMEOUT,
	/** The WPS session has completed successfully
	 */
	AF_EVT_PROV_WPS_SUCCESSFUL,
	/** The WPS session has completed unsuccessfully
	 */
	AF_EVT_PROV_WPS_UNSUCCESSFUL,
	/** This indicates to the user the application framework has received
	* the reset-to-provisioning request in a valid state. The station
	* interface is about to be taken down.
	*/
	AF_EVT_NORMAL_PRE_RESET_PROV,
	/** This indicates to the user the application framework has received
	* the reset-to-provisioning request and the station interface has been
	* taken down.
	*/
	AF_EVT_NORMAL_RESET_PROV,
	/** This indicates to the user that the microAP interface has been
	 * started.
	 */
	AF_EVT_UAP_STARTED,
	/** This indicates to the user that the microAP interface has been
	 * stopped.
	 */
	AF_EVT_UAP_STOPPED,
	/** This indicates to the user that WIFI has entered
	*   Power save mode.
	*/
	AF_EVT_PS_ENTER,
	/** This indicates to the user that WIFI has exited
	*   Power save mode.
	*/
	AF_EVT_PS_EXIT,
	/** This event is sent when an indication is received from the http
	 *  provisioning client that it has concluded that the provisioning
	 * of the device is successful.
	 */
	AF_EVT_PROV_CLIENT_DONE,
#ifdef CONFIG_P2P
	/** This event indicates that the P2P module has started. The data
	 * field is of type \ref app_p2p_role_data_t and tells if the
	 * device has started in the Auto GO role or undecided role.
	 * If the role is undecided, it will be decided when a session is
	 * initiated and the negotiation has completed. The role in that
	 * case is notified in the data of the AF_EVT_P2P_ROLE_NEGOTIATED
	 * event.
	 * The data field also gives the name of the device.
	 */
	AF_EVT_P2P_STARTED,
	/** This indicates that the P2P session has started
	 */
	AF_EVT_P2P_SESSION_STARTED,
	/** This indicates that the P2P device's role has been negotiated.
	 * It can be either client or GO (Group Owner). This information is
	 * available in the data field which is of type
	 * \ref app_p2p_role_data_t.
	 * The status of the connection is indicated by other events like
	 * AF_EVT_NORMAL_CONNECTED, AF_EVT_NORMAL_CONNECT_FAILED, etc.
	 */
	AF_EVT_P2P_ROLE_NEGOTIATED,
	/** This indicates that the P2P thread failed to start. */
	AF_EVT_P2P_FAILED,
	/** This indicates that the P2P module has been shutdown successfully.
	*/
	AF_EVT_P2P_FINISHED,
#endif
	/* This should be the last event in this enum */
	APP_CTRL_EVT_MAX,
} app_ctrl_event_t;

/** The critical data that is sent along with the APP_CTRL_EVT_CRITICAL
 * event. */
typedef struct app_crit_data {
	/** The error code */
	int cd_error;
} app_crit_data_t;

/** This structure is passed along with the \ref AF_EVT_INIT_DONE event to
 * indicate the initial state.
 */
struct app_init_state {
	/** Set to 1 if we are booting up after a factory reset */
	char factory_reset;
	/** Set to 1 if we are booting from backup firmware */
	char backup_fw;
	/** Previous reset cause */
	uint32_t rst_cause;
};

/** Station configuration not found */
#define APP_NETWORK_NOT_PROVISIONED     0
/** Station configuration found */
#define APP_NETWORK_PROVISIONED         1
/** Station configuration is invalid */
#define APP_NETWORK_CONF_INVALID        3

#define DEF_PROV_DURATION		180*1000 /* In milli-seconds */
#define DEF_DEVICE_NAME                 "wm_device"
#define COMMON_PARTITION		"common_part"

/** App Framework Start micro-AP Network with DHCP Server
 *
 * This provides the facility to start the micro-AP network.
 *
 * This starts a default micro-AP network with WPA2 security and start a DHCP
 * Server on this interface. The IP Address of the micro-AP interface is
 * 192.168.10.1/255.255.255.0 This also starts the DHCP service on that network.
 *
 * If NULL is passed as the passphrase, an Open, unsecured micro-AP network is
 * started.
 *
 * For a finer grained control on the network configuration use
 * app_uap_start_by_network() instead.
 *
 * \note This function should not be called before the \ref
 * AF_EVT_WLAN_INIT_DONE event is received by the application.
 *
 * \param[in] ssid the micro-AP SSID
 * \param[in] wpa2_passphrase the WPA2 passphrase of the micro-AP.
 *				Pass NULL for an Open network.
 *
 *
 * \return WM_SUCCESS on success
 * \return error code otherwise
 */
int app_uap_start_with_dhcp(char *ssid, char *wpa2_passphrase);

/** App Framework Start micro-AP Network
 *
 * This provides the facility to start the micro-AP network.
 *
 * This starts a default micro-AP network with WPA2 security, and the IP Address
 * of the micro-AP interface is 192.168.10.1/255.255.255.0. Please note that the
 * application should start the DHCP server on its own by calling the
 * dhcp_server_start() function.
 *
 * If NULL is passed as the passphrase, an Open, unsecured micro-AP network is
 * started
 *
 * For a finer grained control, like using a different security settings, or
 * different IP Address range, or a particular channel, use
 * app_uap_start_by_network() instead.
 *
 * \note This function should not be called before the \ref
 * AF_EVT_WLAN_INIT_DONE event is received by the application.
 *
 * \param[in] ssid the micro-AP SSID
 * \param[in] wpa2_passphrase the WPA2 passphrase of the micro-AP.
 *				Pass NULL for an Open network.
 *
 * \return WM_SUCCESS on success
 * \return error code otherwise
 */
int app_uap_start(char *ssid, char *wpa2_passphrase);

/** App Framework Start micro-AP Network
 *
 * This function is similar to the app_uap_start() function, but is more
 * configurable since it accepts the entire struct wlan_network as a
 * parameter.
 *
 * \note This function should not be called before the \ref
 * AF_EVT_WLAN_INIT_DONE event is received by the application.
 *
 * \param[in] net the micro-AP network details in struct wlan_network format
 *
 * \return WM_SUCCESS on success
 * \return error code otherwise
 */
int app_uap_start_by_network(struct wlan_network *net);

/** App Framework Stop micro-AP Network
 *
 * This stops the micro-AP network that was started using the app_uap_start()
 * function (or its variants). If app_uap_start_with_dhcp() was used to start
 * the micro-AP network, then this call also stops the DHCP service.
 *
 * \return WM_SUCCESS on success
 * \return error code otherwise
 *
 */
int app_uap_stop();

/** App Framework Start Station Interface
 *
 * Connect the station interface to the configured network. A configured network
 * should exist in the psm for this call to function properly. If Marvell
 * provisioning is performed then the network configuration is written to psm
 * when provisioning is complete. If Marvell provisioning is not used, the
 * application should have informed the application framework about the
 * configured network using the app_configure_network().
 *
 * \note This function should not be called before the \ref
 * AF_EVT_WLAN_INIT_DONE event is received by the application.
 *
 *
 * \return WM_SUCCESS on success
 * \return error code otherwise
 */
int app_sta_start();

/** App Framework Start Station Interface with given Network
 *
 * Connect the station interface to the network provided.  This network is not
 * stored to PSM by the application framework.
 *
 * \note This function should not be called before the \ref
 * AF_EVT_WLAN_INIT_DONE event is received by the application.
 *
 * \param[in] net network details in the form struct wlan_network, to be used as
 * the target network to connect to
 *
 * \return WM_SUCCESS on success
 * \return error code otherwise
 */
int app_sta_start_by_network(struct wlan_network *net);

/** App Framework Stop Station Interface
 *
 * This stops the station interface that was started using the app_sta_start()
 * function (or its variants). After this, the application framework goes back
 * to the disconnected state.
 *
 * \return WM_SUCCESS on success
 * \return error code otherwise
 */
int app_sta_stop();

/** App Framework Configure Network
 *
 * Configure a network to connect the station interface to, and connect to this
 * configured network.
 *
 * If Marvell provisioning (using app_provisioning_* calls) is being
 * used, then this function is not required. The provisioning module takes care
 * of configuring the network, that the user has selected, within the
 * application framework.
 *
 * If any other mechanism is being used for querying the network configuration
 * information from the user, then this call should be used to inform the
 * application framework of the configured network.
 *
 * The application framework makes a note of this configuration in the psm. This
 * is the WiFi network that app_sta_start() initiates connections to, even
 * across reboots.
 *
 * \note This function should not be called before the \ref
 * AF_EVT_WLAN_INIT_DONE event is received by the application.
 *
 * \param[in] net the network to connect the station interface to in the struct
 * wlan_network format. The memory location pointed to by this pointer should be
 * available until the network connection is successfully established.
 *
 * \return WM_SUCCESS on success
 * \return error code otherwise
 */
int app_configure_network(struct wlan_network *net);

/** App Framework Reset Configured Network
 *
 * This resets the configured network such that the application framework from
 * now onwards comes up in the provisioning/unconfigured mode.
 *
 * \return WM_SUCCESS on success
 * \return error code otherwise
 */
int app_reset_configured_network();

/** App Framework Start Marvell Provisioning Service
 *
 * Start the network provisioning process. If the network provisioning is
 * desired, the application should first start the micro-AP before calling
 * app_provisioning_start().
 *
 * This will start the provisioning thread that does periodic WiFi scans for
 * the networks in the vicinity. Also if WPS provisioning is desired, its state
 * machine is initialized.
 *
 * \param[in] prov_mode The modes of provisioning that should be started. This
 * could be either \ref PROVISIONING_WLANNW or \ref PROVISIONING_WPS or
 * \ref PROVISIONING_WLANNW | \ref PROVISIONING_WPS
 *
 * \return WM_SUCCESS on success
 * \return error code otherwise
 */
int app_provisioning_start(char prov_mode);

/** App Framework Start Marvell EZConnect Provisioning Service
 *
 * Start the network EZConnect provisioning process. If the EZConnect
 * provisioning is desired, then there is no need to start micro-AP. If a
 * prov_key is specified then the network credentials are encrypted using this
 * key thus ensuring the network credentials are transmitted securely over the
 * air.
 *
 * \note This provisioning is supported only on SD8782 and SD8801 WiFi
 * chipsets.
 *
 * A Smart Phone app should be used to send network configuration information to
 * this device.
 *
 * \param[in] prov_key Pointer to provisioning key string for encryption. Pass
 *        prov_key as NULL to start ezconnect provisioning without encryption.
 * \param[in] prov_key_len length of provisioning key.
 *
 * \return WM_SUCCESS on success
 * \return error code otherwise
 */
int app_ezconnect_provisioning_start(uint8_t *prov_key, int prov_key_len);

/** App Framework Start WPS Session
 *
 * Instruct the application framework to start a WPS session.
 *
 * It is mandatory that app_provisioning_start() is called before this
 * function. This call will indicate the provisioning module to start a WPS
 * session with the specified pin or push-button.
 *
 * \param[in] pin  In case of push-button based WPS this should be NULL. While
 * for pin-based WPS, it should include a null-terminated string for the WPS
 * PIN.
 *
 * \return WM_SUCCESS on success
 * \return error code otherwise
 */
int app_prov_wps_session_start(char *pin);


/** App Framework Stop Provisioning
 *
 * This stops the provisioning service and clears any resources allocated to
 * it.
 *
 * On successful provisioning, this should be called in response to the \ref
 * AF_EVT_PROV_DONE event.
 *
 */
void app_provisioning_stop(void);

/** App Framework Stop EZConnect Provisioning
 *
 * This stops the ezconnect provisioning service and clears any resources
 * allocated to it.
 *
 * On successful provisioning, this should be called in response to the \ref
 * AF_EVT_PROV_DONE event.
 *
 */
void app_ezconnect_provisioning_stop(void);

/** App Framework Start HTTP Web Service with filesystem
 *
 * This starts the HTTPD server similar to app_httpd_start(). But in addition to
 * it, this also initializes the filesystem that is specified as \ref
 * FC_COMP_FTFS in the flash layout and mounts it at '/' of the webserver.
 *
 * \param[in] fs_ver The FTFS API version that should be mounted. When
 * active-passive (redundant) filesystems are used, this will mount the active
 * filesystem that matches the specified fs_ver.
 *
 * \param[in] part_name The FTFS partition name to mount.
 *
 * \param[out] fs A pointer to a pointer of fs structure which stores
 * filesystem handle of filesystem in use (struct fs).
 *
 * \return WM_SUCCESS on success
 * \return error code otherwise
 */
int app_httpd_with_fs_start(int fs_ver, const char *part_name, struct fs **fs);

/** App Framework Start HTTP Web Service
 *
 * Start the HTTP Web Service. This will start the HTTP server which will listen
 * for HTTP connections on port 80 (http) or port 443 (https) as configured in
 * the build configuration.
 *
 * \return WM_SUCCESS on success
 * \return error code otherwise
 */
int app_httpd_start();

/** App Framework Register Upgrade Services
 *
 * This call registers the HTTP upgrade handlers /sys/firmware, /sys/filesystem
 * and /sys/updater that allow upgrades on the system.
 *
 * \return WM_SUCCESS on success
 * \return error code otherwise
 */
int app_sys_register_upgrade_handler();

/** App Framework Register Diagnostics Services
 *
 * This call registers the HTTP diagnostics handlers available under /sys/diag/
 *
 * \return WM_SUCCESS on success
 * \return error code otherwise
 */
int app_sys_register_diag_handler();

/** App Framework Register Performance Measurement Services
 *
 * This call registers the HTTP performance measurement API /sys/perf. HTTP
 * Clients can use this API to get a quick sense of the performance of the
 * device. Please note that this provides only an approximation, and specific
 * performance measurement APIs should be used to get an accurate
 * characterization of the performance. A sample way to get a performance
 * assessment is:
 *
 * for TCP RX:
 * curl -F "filename=@/tmp/testfile" wmdemo-acca.local/sys/perf
 *
 * for TCP TX:
 * curl wmdemo-acca.local/sys/perf?size=10485760 -o /tmp/ignore.txt
 *
 * \return WM_SUCCESS on success
 * \return error code otherwise
 *
 */
int app_sys_register_perf_handler();

/** App Framework Stop HTTP Web Service
 *
 * Stop the HTTP Web Service
 *
 * \return WM_SUCCESS on success
 * \return error code otherwise
 *
 */
int app_httpd_stop();

/** App Framework Start mDNS/DNS-SD Service
 *
 * The mDNS/DNS-SD service is used to announce services over the
 * network. mDNS/DNS-SD also exposes a hostname that can be resolved using
 * mDNS. For further details about mDNS, please refer to \link mdns.h mDNS-SD
 * Responder/Querier \endlink.
 *
 * Services can be associated with a wireless network interface
 * app_mdns_add_service_iface() or app_mdns_remove_service_iface().
 *
 * \param[in] hostname mDNS hostname
 */

void app_mdns_start(char *hostname);

/** App Framework Stop mDNS/DNS-SD Service
 *
 */
void app_mdns_stop(void);

/** App Framework Refresh mDNS hostname
 *
 * This refreshes the hostname that is being used by the mDNS service.
 *
 * \param[in] hostname new mDNS hostname
 */
void app_mdns_refresh_hostname(char *hostname);

/** App Framework Associate mDNS/DNS-SD services with Interface
 *
 * mDNS/DNS-SD services can be announced on any of the micro-AP or station
 * interfaces of the device. This call is used to associate a set of services
 * with a network interface. The call app_mdns_iface_state_change() is later
 * used to actually control announcements of these services on the configured
 * interface.
 *
 * \param[in] config mDNS/DNS-SD service-interface configuration (struct
 * mdns_service_config)
 *
 */
void app_mdns_add_service_iface(struct mdns_service_config config);

/** App Framework Disassociate mDNS/DNS-SD services from Interface
 *
 * This clears any services that were associated with a network interface.
 *
 * \param[in] iface pointer to interface handler as retrieved using
 * net_get_uap_handle() or net_get_sta_handle()
 *
 */
void app_mdns_remove_service_iface(void *iface);

/** App Framework mDNS/DNS-SD Add single service instance
 *
 * This call adds a service to app framework mDNS layer and internally
 * announces(UP) the service. Currently there are MAX_MDNS_LST (5) services
 * supported for micro-AP or station interface each. For each new service,
 * previous services are removed(DOWN) and re-announced(UP) with new service.
 *
 * \note This call is used to add a single service. Please use the
 * app_mdns_add_service_iface() call to add more than one services.
 *
 * \param[in] srv pointer to global service structure (struct mdns_service)
 * \param[in] iface pointer to interface handler as retrieved using
 *  net_get_uap_handle() or net_get_sta_handle()
 * \return WM_SUCCESS on success
 * \return error code otherwise
 */
int app_mdns_add_service(struct mdns_service *srv, void *iface);

/** App Framework mDNS/DNS-SD Remove single service instance
 *
 * This call removes a service from app framework mDNS layer and internally
 * sends BYE packet(DOWN). For each removed service, previous services are
 * also removed(DOWN) and later re-announced(UP).
 *
 * \note This call is used to remove a single service. Please use the
 * app_mdns_remove_service_iface() call to remove more than one services.
 *
 * \param[in] srv pointer to global service structure (struct mdns_service)
 * \param[in] iface pointer to interface handler as retrieved using
 *  net_get_uap_handle() or net_get_sta_handle()
 * \return WM_SUCCESS on success
 * \return error code otherwise
 */
int app_mdns_remove_service(struct mdns_service *srv, void *iface);

/** App Framework mDNS/DNS-SD Service Announcement Control
 *
 * This controls the mDNS/DNS-SD service announcement on the network
 * interface. The state of the interface can be configured as \ref UP, \ref DOWN
 * or \ref REANNOUNCE. The mDNS/DNS-SD module refers to the value of the state
 * to appropriately announce, unannounce or reannounce the services.
 *
 * Generally, the application should use UP state to announce services on first
 * network interface up event. In events like link-lost or dhcp renew, network
 * interface is down for some time during which it gives a chance to other
 * devices to announce services with same name. Hence, after such events, device
 * should REANNOUNCE services to resolve service name conflicts if introduced by
 * other devices. Application can use DOWN state to send bye-bye packet on mDNS
 * group so that the mDNS clients can remove service entries from their cache.
 * In case the user changes hostname or service name, the application can use
 * DOWN state, followed by UP in which case previous service entry in mDNS
 * clients will be removed and new updated entry will be added.
 *
 * \param[in] iface pointer to interface handler as retrieved using
 * net_get_uap_handle() or net_get_sta_handle()
 * \param[in] state the state of the interface \ref UP, \ref DOWN or \ref
 * REANNOUNCE
 */
void app_mdns_iface_state_change(void *iface, int state);

/** Initialize Device name in psm
 *
 * This function sets a default device name in the PSM. The application
 * framework does not use the device name in anyway by itself. Applications can
 * retrieve and use this for their purposes.
 *
 * \param[in] dev_name A NULL-terminated string that stores the default device
 * name.
 *
 * \return WM_SUCCESS on success
 * \return error code otherwise
 */
int app_init_device_name(char *dev_name);

/** Get current device name
 *
 * This function retrieves the device name that is stored in the psm.
 *
 * \param[out] dev_name Pointer to a buffer to store the device name
 * \param[in] len The length of the buffer pointed to by dev_name above
 *
 * \return WM_SUCCESS on success
 * \return error code otherwise
 */
int app_get_device_name(char *dev_name, int len);

/** Set device name
 *
 * This function updates the device name stored in the PSM. The application
 * framework does not use the device name in anyway by itself. Applications can
 * retrieve and use this for their purposes.
 *
 * \param[in] dev_name A NULL-terminated string that stores the default device
 * name.
 *
 * \return WM_SUCCESS on success
 * \return error code otherwise
 *
 */
int app_set_device_name(char *dev_name);

/** App Framework GET UUID (Universal Unique Identifier)
 * This function is used to get the UUID of the system in a string format.
 *
 * \param[in] output A character buffer which will be populated with the UUID.
 * \param[in] len Length of the buffer.
 *
 */
void app_sys_get_uuid(char *output, int len);


/** Query the device's IP Address
 *
 * This function retrieves the device's IP Address in a NULL-terminated string
 * format.
 *
 * \param[out] ipaddr Pointer to a buffer to write the NULL-terminated IP
 * Address to. The buffer should be at least 16 bytes in size.
 *
 * \return WM_SUCCESS on success
 * \return error code otherwise
 */
int app_network_ip_get(char* ipaddr);


/** Soft reboot the System
 *
 * \param[in] reason The reason of type \ref wm_reboot_reason_t for the reboot.
 *
 * \return This function does not return
 */
int app_reboot(wm_reboot_reason_t reason);


/** Application Framework notify application event
 *
 * This function allows the applications to generate their own application
 * specific events. The event will pass through the app_framework's event
 * handling mechanism and then the appropriate application registered
 * event callback will be triggered.
 *
 * This avoids the need of creating one more thread context and enables
 * the application to do some things in the context of the app_framework's
 * thread.
 * This simplifies asynchronous event handling for the applications.
 *
 * The application needs to define its own events. However, care must be taken
 * that the events do not clash with the application framework's events.
 * So, it would be best to start the enumeration of the application specific
 * events from APP_CTRL_EVT_MAX + 1.
 *
 * Eg.
 * typedef enum
 * {
 *         APP_EVT_APP1 = APP_CTRL_EVT_MAX + 1,
 *         APP_EVT_APP2,
 * } app_event_t;
 *
 * \param[in] event The application specific event value (\ref app_ctrl_event_t)
 * \param[in] data Data if any that the event needs to pass (can be NULL)
 *
 * \return WM_SUCCESS on success
 * \return error code otherwise
 */
int app_ctrl_notify_application_event(app_ctrl_event_t event, void *data);

/** Upgrade the filesystem, firmware and/or wifi-firmware
 *
 * This function is used to upgrade filesystem, firmware and/or wifi-firmware
 * provided the input JSON contains appropriate values for elements fs_url,
 * fw_url, wififw_url.
 *
 * \param[in] obj JSON object containing the JSON data for upgrades
 * \param[out] fs_done Parameter indicating if filesystem upgrade was
 * successful
 * \param[out] fw_done Parameter indicating if firmware upgrade was successful
 * \param[out] wififw_done Parameter indicating if wifi firmware upgrade was
 * successful
 *
 * \return WM_SUCCESS on success
 * \return -WM_FAIL otherwise
 */
int app_sys_http_update_all(struct json_object *obj, short *fs_done,
		short *fw_done, short *wififw_done);

#ifdef CONFIG_P2P

/** Role of the P2P application
 */
typedef enum {
/** The P2P application has started, but the role is undecided.
 * The actual role will be decided after negotiation.
 */
	ROLE_UNDECIDED = 0,
/** The P2P application has started in the auto GO mode.
 * This means that the device will be the group owner (GO).
 */
	ROLE_AUTO_GO,
/** The P2P device has taken up the role of a client after the
 * negotiation.
 */
	ROLE_CLIENT,
/** The P2P device has taken up the role of a Group Owner (GO)
 * after the negotiation.
 */
	ROLE_GO,
} app_p2p_role_t;

/** This structure is passed along with the \ref AF_EVT_P2P_STARTED and the
 * \ref AF_EVT_P2P_ROLE_NEGOTIATED events to indicate the current role.
 * Refer \ref app_p2p_role_t for details.
 */

typedef struct app_p2p_role_data {
	/** The P2P node type (client/GO) */
	app_p2p_role_t role;
	/** Points to the name of the device */
	void *data;
} app_p2p_role_data_t;

/** Start P2P module
 *
 * This function starts the P2P module. If the operations succeeds, the
 * \ref AF_EVT_P2P_STARTED event is generated. Else, the \ref AF_EVT_P2P_FAILED
 * event is generated.
 *
 * \param[in] role can be \ref ROLE_AUTO_GO or \ref ROLE_UNDECIDED only
 *
 * \return WM_SUCCESS on success. Actual result is notified through the events
 * \ref AF_EVT_P2P_STARTED or \ref AF_EVT_P2P_FAILED
 */
int app_p2p_start(app_p2p_role_t role);

/** Stop the P2P Module
 *
 * This function stops the P2P module. The \ref AF_EVT_P2P_FINISHED event is
 * generated on success.
 *
 * \return WM_SUCCESS on success. Actual status is notified through the event
 * \ref AF_EVT_P2P_FINISHED
 */
int app_p2p_stop();

/** Start a P2P session
 *
 * This function initiates a connection with the node specified in argument.
 * In AUTO_GO mode, this function just means that the device is ready to
 * accept connections.
 *
 * \param[in] pin The pin to be used for the session. A NULL value means that it
 * is a push button session. Currently, only push button is supported.
 * \param[in] remote A pointer to struct p2p_scan_result. A session with the
 * node pointed to by this is pointer is started.
 *
 * \return WM_SUCCESS on success. Further results are notified by various P2P
 * events.
 */
int app_p2p_session_start(char *pin, struct p2p_scan_result *remote);

/** Disconnect from a P2P connection
 *
 * This function disconnects a connected P2P session.
 *
 * \return WM_SUCCESS on success.
 * \return -WM_FAIL on failure.
 */
int app_p2p_disconnect();

/** Get P2P scan results.
 *
 * This function returns a pointer to a list if P2P nodes. The P2P module
 * should be started prior to using this function.
 *
 * \param[out] no Number of P2P nodes in the scan list.
 *
 * \return A pointer to a struct p2p_scan_result array which holds a list
 * of scanned P2P nodes.
 */
struct p2p_scan_result *app_p2p_scan(int *no);

/** Query the P2P interface's IP address
 *
 * This function retrieves the P2P interface's IP Address in a
 * NULL-terminated string format.
 *
 * \param[out] ipaddr Pointer to a buffer to write the NULL-terminated IP
 * Address to. The buffer should be at least 16 bytes in size.
 * \return WM_SUCCESS on success
 * \return error code otherwise
 */
int app_p2p_ip_get(char *ipaddr);
#endif /* CONFIG_P2P */

/** Convert App Framework event id to event string
 *
 * This is useful for debugging.
 *
 * \param[in] event App Framework event ID
 *
 * \return String name corresponding to the App Framework event ID
 */
const char *app_ctrl_event_name(app_ctrl_event_t event);

#endif /* _APP_FRAMEWORK_H_ */
