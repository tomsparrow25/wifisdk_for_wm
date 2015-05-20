#ifndef __MLAN_WMSDK_H__
#define __MLAN_WMSDK_H__
#include <string.h>
#include <wmstdio.h>

#define INCLUDE_FROM_MLAN

/* wmsdk: This is an assumed constant based on practical observations. The
   default value was 256. We need to optimize mem. usage. So we are
   reducing this value. We have put checks in place to avoid buffer
   overflows.  */
#define MLAN_WMSDK_MAX_WPA_IE_LEN 32

#define STA_SUPPORT
#define UAP_SUPPORT

#define KEY_MATERIAL_WEP
#ifdef CONFIG_WiFi_8801
#define KEY_PARAM_SET_V2
#endif

#define WPA
#define HOST_AUTHENTICATOR

#ifdef CONFIG_EXT_SCAN
#define EXT_SCAN_SUPPORT
#endif

#include "mlan.h"
#include "mlan_join.h"
#include "mlan_util.h"
#include "mlan_fw.h"
#include "mlan_main.h"
#include "mlan_wmm.h"
#include "mlan_11n.h"
#include "mlan_11h.h"
#include "mlan_decl.h"
#include "mlan_11n_aggr.h"
#include "mlan_sdio.h"
#include "mlan_11n_rxreorder.h"
#include "mlan_meas.h"
#include "mlan_ioctl.h"
#include "mlan_uap.h"
#include <wifi-debug.h>
#include "wifi-internal.h"

/* #define CONFIG_WIFI_DEBUG */

#ifdef CONFIG_WIFI_DEBUG
/* #define DEBUG_11N_ASSOC */
/* #define DEBUG_11N_AGGR */
/* #define DEBUG_11N_REORDERING */
/* #define DEBUG_MLAN */
/* #define DEBUG_DEVELOPMENT */
/* #define DUMP_PACKET_MAC */
#endif /* CONFIG_WIFI_DEBUG */

#ifdef ENTER
#undef ENTER
#define ENTER(...)
#endif /* ENTER */

#ifdef EXIT
#undef EXIT
#define EXIT(...)
#endif /* EXIT */

#ifdef PRINTM
#undef PRINTM
#ifdef DEBUG_MLAN
#define PRINTM(level, ...) \
	do {					\
		wmprintf("[11n] "__VA_ARGS__);	\
		wmprintf("\n\r");		\
	} while (0)
#else
#define PRINTM(...)
#endif /* PRINTM */
#endif /* DEBUG_MLAN */


#define DOT11N_CFG_ENABLE_RIFS 0x08
#define DOT11N_CFG_ENABLE_GREENFIELD_XMIT (1 << 4)
#define DOT11N_CFG_ENABLE_SHORT_GI_20MHZ (1 << 5)
#define DOT11N_CFG_ENABLE_SHORT_GI_40MHZ (1 << 6)

#define CLOSEST_DTIM_TO_LISTEN_INTERVAL  65534

#define SDIO_DMA_ALIGNMENT 4

/* Following is allocated in mlan_register */
extern mlan_adapter *mlan_adap;

void dump_mac_addr(const char *msg, unsigned char *addr);
void dump_htcap_info(const MrvlIETypes_HTCap_t *htcap);
void dump_ht_info(const MrvlIETypes_HTInfo_t *htinfo);


mlan_status wifi_prepare_and_send_cmd(IN mlan_private * pmpriv,
				      IN t_u16 cmd_no,
				      IN t_u16 cmd_action,
				      IN t_u32 cmd_oid,
				      IN t_void * pioctl_buf,
				      IN t_void * pdata_buf,
				      int bss_type, void *priv);
int wifi_uap_prepare_and_send_cmd(mlan_private *pmpriv,
			 t_u16 cmd_no,
			 t_u16 cmd_action,
			 t_u32 cmd_oid,
			 t_void *pioctl_buf, t_void *pdata_buf,
				  int bss_type, void *priv);

bool wmsdk_is_11N_enabled(void);

/**
 * Abort the split scan if it is in progress.
 *
 * After this call returns this scan function will abort the current split
 * scan and return back to the caller. The scan list may be incomplete at
 * this moment. There are no other side effects on the scan function apart
 * from this. The next call to scan function should proceed as normal.
 */
void wlan_abort_split_scan(void);

void wlan_scan_process_results(IN mlan_private * pmpriv);
#endif /* __MLAN_WMSDK_H__ */
