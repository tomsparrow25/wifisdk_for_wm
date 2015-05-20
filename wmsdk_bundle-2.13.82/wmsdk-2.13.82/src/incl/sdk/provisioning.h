/*
 * Copyright 2008-2013, Marvell International Ltd.
 * All Rights Reserved.
 *
 */

/*! \file provisioning.h
 * \brief Provisioning Support
 * \section intro Introduction
 * Provisioning process lets user configure the WMSDK enabled device to connect
 * to his home network first time. Provisioning library in WMSDK provides
 * implementation of three provisioning methods to the application developer.
 * These methods are:
 * \li micro-AP network based provisioning
 * \li WPS based provisioning
 * \li EZConnect (Sniffer-based) provisioning
 *
 * Of the above methods, the provisioning library allows co-existence of
 * micro-AP and WPS provisioning methods.
 *
 *
 * \subsection wlannw_prov WLAN network (micro-AP) based provisioning
 *
 * In this method, a network is hosted by WMSDK enabled device. Clients can
 * associate with this network and use the provided HTTP APIs to configure the
 * home network properties on to the device. These HTTP APIs let users scan the
 * environment to find available access points, choose the access point of the
 * interest, provide security passphrase for the access point and finally push
 * the settings to the device to complete provisioning. These HTTP APIs are
 * listed in the Marvell System API guide.
 *
 * For microAP based provisioning, a secure network can be hosted on the device
 * thus ensuring that the data transferred over the air during the provisioning
 * process is encrypted.
 *
 *
 * \subsection wps_prov WPS based provisioning
 * WiFi Protected Setup is a standard that attempts to allow easy yet secure
 * association of WiFi clients to the access point. Provisioning library allows
 * developers to extend WPS association methods to the end-user. It supports
 * both a) PIN and b) Push-button methods. These are described below.
 * \li PIN Method: In this method a 8-digit numerical PIN should be used on both
 * device and access point. This PIN can be static or dynamically generated on
 * the device. Depending on the capabilities of the device the PIN can be
 * printed or displayed on the device screen. When the same PIN is entered
 * within interval of 2 minutes on the access point and WPS session is started,
 * association succeeds.
 * \li Push-button Method: In this method a physical or virtual pushbutton on
 * the access point as well as on the device is pressed within interval of 2
 * minutes, the association succeeds.
 *
 * @cond uml_diag
 *
 * @startuml{secure_provisioning.png}
 * actor Customer
 * participant WM
 * participant "Service Client" as SP
 * note left of SP: iPOD App or Laptop
 * Customer->WM: First Power on
 * WM->WM: Start micro-AP network
 * note left: IP: 192.168.10.1\nSSID: "wmdemo-AABB"
 * Customer->SP: Start Client
 * SP->SP: Scan Wireless Networks
 * SP->WM: Connect to micro-AP network
 * SP->WM: Get list of available networks
 * activate WM
 * WM->WM: Scan Wireless Networks in vicinity
 * WM->SP: Returns list of networks
 * deactivate WM
 *
 * SP->SP: Display the list of networks
 * Customer->SP: Select the home wireless network
 * SP->WM: HTTP POST /sys/network
 *
 *
 * WM->WM: Connect to the configured network
 * @enduml
 * @endcond
 *
 * \subsection ezconnect_prov EZConnect provisioning
 *
 * In this method, the device acts as a sniffer and captures all the packets on
 * the air looking for a certain signature. A smart phone application (Android /
 * iOS) is then used to transmit the target network credentials in a certain
 * format. The target network credentials can be encrypted using a pre-shared
 * key. The device sniffs these packets off the air, decrypts the data and
 * establishes connection with the target network.
 */

#ifndef __PROVISIONING_H__
#define __PROVISIONING_H__

#include <wlan.h>

/** Provisioning Scan Policies
 *
 * The micro-AP provisioning process performs a periodic scan. The scan results
 * are available through the /sys/scan HTTP API. The following scan policies can
 * be used for this scanning.
 */
enum prov_scan_policy {
	 /** Periodic Scan. This is the default scan policy. The periodicity can
	  * be configured using the prov_set_scan_config() function or through
	  * the /sys/scan-config HTTP API.
	  */
	 PROV_PERIODIC_SCAN,
	 /** On-Demand Scan. In this policy a network scan is performed on
	  * provisioning start-up. Further calls are only triggered on
	  * subsequent requests to the /sys/scan HTTP API.
	  */
	 PROV_ON_DEMAND_SCAN,
};
/** @cond */
enum wm_prov_errno {
	WM_E_PROV_ERRNO_BASE = MOD_ERROR_START(MOD_PROV),
	WM_E_PROV_INVALID_CONF,
	WM_E_PROV_INVALID_STATE,
	WM_E_PROV_SESSION_ATTEMPT,
	WM_E_PROV_PIN_CHECKSUM_ERR,
};
/** @endcond */

/** This enum enlists the events that are delivered to the application by
  provisioning module through the provisioning_event_handler callback
  registered in struct provisioning_config. Note that all the events may not
  be delivered in the context of provisioning thread. */
enum prov_event {
	/** This event is delivered immediately after when provisioning thread
	  begins its operation i.e. when prov_start is called */
	PROV_START,
	/** This event is received along with struct wlan_network data. This
	  event is delivered to application in case of successful POST to
	  /sys/network or in case of successful WPS session attempt.
	  Application can use received wlan_network structure to connect as
	  well as to store in PSM. */
	PROV_NETWORK_SET_CB,
	/** This event is currently unused
	*/
	PROV_ABORTED, /* unused */
	/** This event is sent to the application on succeful POST on
	  /sys/scan-cfg. This is sent along with struct prov_scan_config data.
	  Provisioning module already modifies the current scan configuration
	  as per provided values. Application can choose to write this data
	  into PSM for future use.
	*/
	PROV_SCAN_CFG_SET_CB,
	/** This event is generated by provisioning library when
	  \li PIN is registered with provisioning thread and found scan results
	  contain atleast one SSID with on-going WPS session with PIN mode
	  active
	  \li Pushbutton is registered with provisioning thread and found scan
	  results contain atleast one SSID with on-going WPS session with
	  Pushbutton mode active.

	  Along with this event an array of struct wlan_scan_results is sent.
	  The number of array elements can be found by dividing "len" by sizeof
	  struct wlan_scan_results. The application is expected to return index
	  of chosen SSID with which WPS session should be attempted after going
	  through the provided scan list. The application should return -1, if
	  it doesn't intend to attempt session with any SSID found in the list.
	*/
	PROV_WPS_SSID_SELECT_REQ,
	/** This event is generated when WPS session attempt is started by the
	  provisioning thread. Application can use this to provide visual
	  indicator to end-user.
	*/
	PROV_WPS_SESSION_STARTED,
	/** This event is generated when the PIN times out. The timeout interval
	  is 2 minutes. Application may choose to re-send the PIN on timeout.
	*/
	PROV_WPS_PIN_TIMEOUT,
	/** This event is generated when the Pushbutton event times out. The
	  timeout interval is 2 minutes.
	*/
	PROV_WPS_PBC_TIMEOUT,
	/** This event is generated when WPS session succeeds. This event is
	  preceded by PROV_SET_NETWORK_CB event that delivers the home network
	  security parameters to the application.
	*/
	PROV_WPS_SESSION_SUCCESSFUL,
	/** This event is generated when the WPS session can not succeed due
	  to some condition like PIN values at both ends did not match.
	*/
	PROV_WPS_SESSION_UNSUCCESSFUL,
};


/** This enum enlists the states in which provisioning thread can be.
  Application may want to do some validation depending on the state before
  sending any request to provisioning thread. */
enum provisioning_state {
	/** This state indicates the provisioning thread is still initializing
	  provisioning network and/or WPS thread. Application should not pass
	  events like PIN or Pushbutton registration in this state as they will
	  be ignored.
	*/
	PROVISIONING_STATE_INITIALIZING = 0,
	/** This state indicates that the device is in provisioning state ready
	  to get network settings applied.
	*/
	PROVISIONING_STATE_ACTIVE,
	/** This state indicates either
	 * \li POST on /sys/network has arrived
	 * 	or
	 * \li WPS session attempt is in progress
	 *
	 * This state protects from simultaneous attempts to configure the
	 * device
	 */
	PROVISIONING_STATE_SET_CONFIG,
	/** This state indicates that provisioning is successful
	*/
	PROVISIONING_STATE_DONE,
};

/** This structure is used to represent scan configuration */
struct prov_scan_config {
	/** Wireless scan parameters as defined at struct wifi_scan_params_t */
	struct wifi_scan_params_t wifi_scan_params;
	/** Interval between consecutive scans represented in seconds. This
	  value should not be less than 2 seconds and should not exceed 60
	  seconds. */
	int scan_interval;
};

/** WLAN network (uAP) based provisioning mode */
#define PROVISIONING_WLANNW		(unsigned int)(1<<0)
/** WPS provisioning mode */
#define PROVISIONING_WPS                (unsigned int)(1<<1)
/** Marvell EZConnect Provisioning mode */
#define PROVISIONING_EZCONNECT		(unsigned int)(1<<2)

/** This structure is used to configure provisioning module with application
  specific settings. */
struct provisioning_config {
	/** Provisioning mode; could be \ref PROVISIONING_WLANNW and/or \ref
	 * PROVISIONING_WPS or \ref PROVISIONING_EZCONNECT. Note that while \ref
	 * PROVISIONING_WLANNW and \ref PROVISIONING_WPS can be simultaneously
	 * used, \ref PROVISIONING_EZCONNECT can only be used by itself. */
	unsigned int prov_mode;
	/** Callback function pointer that will be invoked for any events to be
	 * given to the application. These events are listed in enum
	 * prov_event. arg is a generic pointer that may be valid when data is
	 * to be received along with event. In such case len contains length of
	 * data passed along.  */
	int (*provisioning_event_handler) (enum prov_event event, void *arg,
					   int len);
};

/* Provisioning APIs */

/** Starts provisioning with provided configuration
 * \param pc Pointer to provisioning_config structure that contains application
 *        specific provisioning configuration.
 *
 * \return WM_SUCCESS if the call is successful
 *	   -WM_FAIL in case of error
 *
 * \note The application should not create this structure instance through local
 * variable. Provisioning thread needs to access this throughout its lifetime.
 *
 */
int prov_start(struct provisioning_config *pc);

/** Stops provisioning thread */
void prov_finish(void);

/* EZconnect Provisioning APIs */

/** Start EZConnect Provisioning
 *
 * This function starts the EZConnect provisioning process.
 *
 * \param pc Pointer to provisioning_config structure that contains application
 *        specific provisioning configuration.
 * \param prov_key Pointer to provisioning key string for encryption. Pass
 *        prov_key as NULL to start ezconnect provisioning without encryption.
 * \param prov_key_len length of provisioning key.
 *
 * \return WM_SUCCESS if the call is successful
 *	   -WM_FAIL in case of error
 *
 * \note The application should not create this structure instance through local
 * variable. Provisioning thread needs to access this throughout its lifetime.
 *
 */
int prov_ezconnect_start(struct provisioning_config *pc, uint8_t *prov_key,
			 int prov_key_len);

/** Stops ezconnect provisioning thread */
void prov_ezconnect_finish(void);

/** Returns currently configured scan configuration
 * \param scan_cfg Current configuration is copied into the structure pointed
 *        by this pointer
 * \return WM_SUCCESS if the call is successful
 *	   -WM_FAIL in case of error
 */
int prov_get_scan_config(struct prov_scan_config *scan_cfg);

/** Sets scan configuration
 * \param scan_cfg Pointer to structure that holds valid scan configuration
 * \return WM_SUCCESS if the call is successful
 *	   non-zero error-code in case of error
 *
 * \note Call to this function generates application callback with event
 * PROV_SCAN_CFG_SET_CB.
 */
int prov_set_scan_config(struct prov_scan_config *scan_cfg);

/** Sets scan configuration without application callback
 * \param scan_cfg Pointer to structure that holds valid scan configuration
 * \return WM_SUCCESS if the call is successful
 *	   non-zero error-code in case of error
 *
 * \note Call to this function does not generate application callback with event
 * PROV_SCAN_CFG_SET_CB. If application itself wants to apply scan configuration
 * settings (e.g. at initialization), this call should be used.
 */
int prov_set_scan_config_no_cb(struct prov_scan_config *scan_cfg);

#ifdef CONFIG_WPS2
/** Give WPS pushbutton press event to provisioning module
 * \return WM_SUCCESS if call is successful
 *	   -WM_E_PROV_SESSION_ATTEMPT if WPS session is already in progress
 * 	   -WM_E_PROV_INVALID_STATE if provisioning module is not in active
 *        state
 */
int prov_wps_pushbutton_press(void);

/** Register WPS PIN with provisioning module
 * \param pin PIN to be registered
 * \return WM_SUCCESS if call is successful
 * 	   -WM_E_PROV_PIN_CHECKSUM_ERR in case of PIN checksum invalid
 *	   -WM_E_PROV_SESSION_ATTEMPT if WPS session is already in progress
 * 	   -WM_E_PROV_INVALID_STATE if provisioning module is not in active
 *        state
 */
int prov_wps_provide_pin(unsigned int pin);
#endif

/** Verify scan interval value
 * \param scan_interval Scan interval in seconds
 * \return WM_SUCCESS if interval is valid
 *	   -WM_FAIL if interval is invalid
 */
int verify_scan_interval_value(int scan_interval);

/* Provisioning web handlers APIs */
/** Register provisioning HTTP API handlers
 * \return WM_SUCCESS if handlers registered successfully
 *	   -WM_FAIL if handlers registration fails
 */
int register_provisioning_web_handlers(void);

/** Unregister provisioning HTTP API handlers */
void unregister_provisioning_web_handlers(void);

/** This function lets user use /sys/network handler in "provisioned" mode
 * by letting application register a callback handler that is called with
 * received wlan_network structure that gives control directly to the
 * application bypassing provisioning module.
 * \param cb_fn Function pointer that should be called in case of POST on
 *              /sys/network
 * \return WM_SUCCESS if handler registered successfully
 * 	   -WM_FAIL if handler registration fails
 */
int set_network_cb_register(int (*cb_fn) (struct wlan_network *));

/* This function is internal to the provisioning module */
int prov_ezconn_set_device_key(uint8_t *key, int len);

/* This function is internal to the provisioning module */
void prov_ezconn_unset_device_key(void);

/** Set Provisioning Scan Policy
 *
 * The micro-AP provisioning process performs a periodic scan. The scan results
 * are available through the /sys/scan HTTP API. This API can be used to modify
 * the provisioning scan policy.
 *
 * The default policy is to scan periodically. The periodicity can be configured
 * using the prov_set_scan_config() function or through the /sys/scan-config
 * HTTP API.
 *
 * \param scan_policy can be either of either of \ref PROV_PERIODIC_SCAN,
 * \ref PROV_ON_DEMAND_SCAN or \ref PROV_CUSTOMIZED_SCAN
 * \return -WM_FAIL in case of the scan policy cannot be set.
 * WM_SUCCESS if scan policy is set successfully.
 */
int prov_set_scan_policy(enum prov_scan_policy scan_policy);
#endif
