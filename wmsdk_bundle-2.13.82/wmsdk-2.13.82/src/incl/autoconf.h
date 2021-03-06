/*
 * Automatically generated C config: don't edit
 */ 

/*
 * System Type
 */
#define CONFIG_CPU_MC200 1
#undef CONFIG_WiFi_878x
#define CONFIG_WiFi_8801 1
#define CONFIG_OS_FREERTOS 1
#define CONFIG_LWIP_STACK 1

/*
 * Application Framework
 */
#define CONFIG_APP_FRM_PROV_WPS 1

/*
 * Modules
 */

/*
 * WLAN
 */

/*
 * Wifi driver
 */
#define CONFIG_HOST_SUPP 1
#undef CONFIG_EXT_SCAN
#define CONFIG_WMM 1
#define CONFIG_11N 1
#define CONFIG_11D 1
#define CONFIG_WLAN_FW_HEARTBEAT 1
#define CONFIG_5GHz_SUPPORT 1
#define CONFIG_WLAN_FAST_PATH 1
#define CONFIG_MAX_AP_ENTRIES 20
#define CONFIG_WLAN_KNOWN_NETWORKS 5

/*
 * uAP configuration
 */
#define CONFIG_WIFI_UAP_WORKAROUND_STICKY_TIM 1

/*
 * Wifi extra debug options
 */
#undef CONFIG_WIFI_EXTRA_DEBUG
#undef CONFIG_WIFI_UAP_DEBUG
#undef CONFIG_WIFI_EVENTS_DEBUG
#undef CONFIG_WIFI_CMD_RESP_DEBUG
#undef CONFIG_WIFI_SCAN_DEBUG
#undef CONFIG_WIFI_IO_DEBUG
#undef CONFIG_WIFI_IO_DUMP
#undef CONFIG_WIFI_MEM_DEBUG
#undef CONFIG_WIFI_AMPDU_DEBUG
#undef CONFIG_WIFI_TIMER_DEBUG
#undef CONFIG_P2P
#undef CONFIG_WPS2

/*
 * WPA2 Enterprise Support
 */
#undef CONFIG_WPA2_ENTP
#undef CONFIG_HOSTAPD_RADIUS
#undef CONFIG_FREE_RADIUS
#define CONFIG_MLAN_WMSDK 1

/*
 * MDNS
 */
#undef CONFIG_MDNS_QUERY

/*
 * TLS
 */
#undef CONFIG_ENABLE_TLS
#undef CONFIG_CYASSL

/*
 * HTTP Server
 */
#define CONFIG_ENABLE_HTTP 1
#undef CONFIG_ENABLE_HTTPS

/*
 * JSON
 */
#undef CONFIG_JSON_FLOAT

/*
 * XZ Decompression
 */
#define CONFIG_XZ_DECOMPRESSION 1

/*
 * WiFi Firmware Upgrade Support
 */
#undef CONFIG_WIFI_FW_UPGRADE

/*
 * Peripheral Drivers
 */
#undef CONFIG_MRVL_DRV_TUNL_DRIVER
#undef CONFIG_LCD_DRIVER
#undef CONFIG_XFLASH_DRIVER
#undef CONFIG_SPI_FLASH_DRIVER
#undef CONFIG_USB_DRIVER
#undef CONFIG_USB_DRIVER_HOST

/*
 * Miscellaneous
 */
#undef CONFIG_UART_RS485
#undef CONFIG_UART_LARGE_RCV_BUF
#undef CONFIG_SW_WATCHDOG
#define CONFIG_HW_RTC 1

/*
 * Development and Debugging
 */
#define CONFIG_ENABLE_ERROR_LOGS 1
#define CONFIG_ENABLE_WARNING_LOGS 1
#define CONFIG_DEBUG_BUILD 1
#undef CONFIG_ENABLE_FREERTOS_RUNTIME_STATS_SUPPORT
#undef CONFIG_RUNTIME_STATS_USE_GPT0
#undef CONFIG_RUNTIME_STATS_USE_GPT1
#undef CONFIG_RUNTIME_STATS_USE_GPT2
#undef CONFIG_RUNTIME_STATS_USE_GPT3
#undef CONFIG_ENABLE_ASSERTS
#undef CONFIG_DEBUG_OUTPUT
#undef CONFIG_WLCMGR_DEBUG
#undef CONFIG_HEALTHMON_DEBUG
#undef CONFIG_HTTPD_DEBUG
#undef CONFIG_TLS_DEBUG
#undef CONFIG_WIFI_DEBUG
#undef CONFIG_PWR_DEBUG
#undef CONFIG_WAKELOCK_DEBUG
#undef CONFIG_WPS_DEBUG
#undef CONFIG_P2P_DEBUG
#undef CONFIG_DHCP_SERVER_DEBUG
#undef CONFIG_PROVISIONING_DEBUG
#undef CONFIG_SUPPLICANT_DEBUG
#undef CONFIG_HTTPC_DEBUG
#undef CONFIG_CRC_DEBUG
#undef CONFIG_RFGET_DEBUG
#undef CONFIG_CRYPTO_DEBUG
#undef CONFIG_JSON_DEBUG
#undef CONFIG_FLASH_DEBUG
#undef CONFIG_MDNS_DEBUG
#undef CONFIG_MDNS_CHECK_ARGS
#undef CONFIG_APP_DEBUG
#undef CONFIG_APP_FRAME_INTERNAL_DEBUG
#undef CONFIG_LL_DEBUG
#undef CONFIG_OS_DEBUG
#undef CONFIG_HEAP_DEBUG
#undef CONFIG_PSM_DEBUG
#undef CONFIG_FTFS_DEBUG
#undef CONFIG_SEMAPHORE_DEBUG
#undef CONFIG_ENABLE_TESTS
#undef CONFIG_HTTPD_TESTS
#undef CONFIG_JSON_TESTS
#undef CONFIG_CLI_TESTS
#undef CONFIG_SYSINFO_TESTS
#undef CONFIG_MDNS_TESTS
#undef CONFIG_EZXML_TESTS
#undef CONFIG_BA_TESTS
#undef CONFIG_MDEV_TESTS
#undef CONFIG_HEAP_TESTS
#undef CONFIG_AUTO_TEST_BUILD
