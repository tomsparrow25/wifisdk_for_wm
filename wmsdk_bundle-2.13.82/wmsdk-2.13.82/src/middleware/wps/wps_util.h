/** @file wps_util.h
 *  @brief This file contains definitions for debugging print functions.
 *  
 *  Copyright (C) 2003-2010, Marvell International Ltd.
 *  All Rights Reserved
 */

#ifndef _WPS_UTIL_H_
#define _WPS_UTIL_H_

#include "wps_msg.h"

/** MAC to string */
#define MAC2STR(a) (a)[0], (a)[1], (a)[2], (a)[3], (a)[4], (a)[5]
/** MAC string */
#define MACSTR "%02x:%02x:%02x:%02x:%02x:%02x"

#ifndef BIT
#define BIT(x) (1 << (x))
#endif

#define WPA_GET_BE16(a) ((u16) (((a)[0] << 8) | (a)[1]))
#define WPA_PUT_BE16(a, val)			\
	do {					\
		(a)[0] = ((u16) (val)) >> 8;	\
		(a)[1] = ((u16) (val)) & 0xff;	\
	} while (0)

#define WPA_GET_LE16(a) ((u16) (((a)[1] << 8) | (a)[0]))
#define WPA_PUT_LE16(a, val)			\
	do {					\
		(a)[1] = ((u16) (val)) >> 8;	\
		(a)[0] = ((u16) (val)) & 0xff;	\
	} while (0)

#define WPA_GET_BE24(a) ((((u32) (a)[0]) << 16) | (((u32) (a)[1]) << 8) | \
			 ((u32) (a)[2]))
#define WPA_PUT_BE24(a, val)					\
	do {							\
		(a)[0] = (u8) ((((u32) (val)) >> 16) & 0xff);	\
		(a)[1] = (u8) ((((u32) (val)) >> 8) & 0xff);	\
		(a)[2] = (u8) (((u32) (val)) & 0xff);		\
	} while (0)

#define WPA_GET_BE32(a) ((((u32) (a)[0]) << 24) | (((u32) (a)[1]) << 16) | \
			 (((u32) (a)[2]) << 8) | ((u32) (a)[3]))
#define WPA_PUT_BE32(a, val)					\
	do {							\
		(a)[0] = (u8) ((((u32) (val)) >> 24) & 0xff);	\
		(a)[1] = (u8) ((((u32) (val)) >> 16) & 0xff);	\
		(a)[2] = (u8) ((((u32) (val)) >> 8) & 0xff);	\
		(a)[3] = (u8) (((u32) (val)) & 0xff);		\
	} while (0)

#define WPA_GET_LE32(a) ((((u32) (a)[3]) << 24) | (((u32) (a)[2]) << 16) | \
			 (((u32) (a)[1]) << 8) | ((u32) (a)[0]))
#define WPA_PUT_LE32(a, val)					\
	do {							\
		(a)[3] = (u8) ((((u32) (val)) >> 24) & 0xff);	\
		(a)[2] = (u8) ((((u32) (val)) >> 16) & 0xff);	\
		(a)[1] = (u8) ((((u32) (val)) >> 8) & 0xff);	\
		(a)[0] = (u8) (((u32) (val)) & 0xff);		\
	} while (0)

#define WPA_GET_BE64(a) ((((u64) (a)[0]) << 56) | (((u64) (a)[1]) << 48) | \
			 (((u64) (a)[2]) << 40) | (((u64) (a)[3]) << 32) | \
			 (((u64) (a)[4]) << 24) | (((u64) (a)[5]) << 16) | \
			 (((u64) (a)[6]) << 8) | ((u64) (a)[7]))
#define WPA_PUT_BE64(a, val)				\
	do {						\
		(a)[0] = (u8) (((u64) (val)) >> 56);	\
		(a)[1] = (u8) (((u64) (val)) >> 48);	\
		(a)[2] = (u8) (((u64) (val)) >> 40);	\
		(a)[3] = (u8) (((u64) (val)) >> 32);	\
		(a)[4] = (u8) (((u64) (val)) >> 24);	\
		(a)[5] = (u8) (((u64) (val)) >> 16);	\
		(a)[6] = (u8) (((u64) (val)) >> 8);	\
		(a)[7] = (u8) (((u64) (val)) & 0xff);	\

/* 
 * Debugging function - conditional printf and hex dump.
 * Driver wrappers can use these for debugging purposes.
 */
/** Debug Message : None */
#define MSG_NONE            0x00000000
/** Debug Message : All */
#define DEBUG_ALL           0xFFFFFFFF

/** Debug Message : Message Dump */
#define MSG_MSGDUMP         0x00000001
/** Debug Message : Info */
#define MSG_INFO            0x00000002
/** Debug Message : Warning */
#define MSG_WARNING         0x00000004
/** Debug Message : Error */
#define MSG_ERROR           0x00000008

/** Debug Message : Init */
#define DEBUG_INIT          0x00000010
/** Debug Message : EAPOL */
#define DEBUG_EAPOL         0x00000020
/** Debug Message : WLAN */
#define DEBUG_WLAN          0x00000040
/** Debug Message : Call Flow  */
#define DEBUG_CALL_FLOW     0x00000080

/** Debug Message : WPS Message */
#define DEBUG_WPS_MSG       0x00000100
/** Debug Message : WPS State */
#define DEBUG_WPS_STATE     0x00000200
/** Debug Message : Post Run */
#define DEBUG_POST_RUN      0x00000400
/** Debug Message : Walk Time */
#define DEBUG_WALK_TIME     0x00000800
/** Debug Message : Event */
#define DEBUG_EVENT         0x00001000
/** Debug Message : Vendor TLV */
#define DEBUG_VENDOR_TLV    0x00002000
/** Debug Message : WFD Discovery */
#define DEBUG_WFD_DISCOVERY  0x00008000
/** Debug Message : Input */
#define DEBUG_INPUT          0x00010000

/** Default Debug Message */
#define DEFAULT_MSG     (MSG_NONE   \
|   MSG_INFO    \
|   MSG_WARNING \
|   MSG_ERROR   \
|   DEBUG_ALL	\
|   MSG_NONE)

#ifdef STDOUT_DEBUG

/** 
 *  @brief Timestamp debug print function
 *
 *  @return         None
*/
void wps_debug_print_timestamp(void);

/** 
 *  @brief Debug print function
 *         Note: New line '\n' is added to the end of the text when printing to stdout.
 *
 *  @param level    Debugging flag
 *  @param fmt      Printf format string, followed by optional arguments
 *  @return         None
 */
void wps_printf(int level, char *fmt, ...)
    __attribute__ ((format(printf, 2, 3)));

/** 
 *  @brief Debug buffer dump function
 *         Note: New line '\n' is added to the end of the text when printing to stdout.
 *
 *  @param level    Debugging flag
 *  @param title    Title of for the message
 *  @param buf      Data buffer to be dumped
 *  @param len      Length of the buf
 *  @return         None
 */
void wps_hexdump(int level, const char *title, const unsigned char *buf,
		 size_t len);
/** ENTER print */
#define ENTER()         wps_printf(DEBUG_CALL_FLOW, "Enter: %s, %s:%i", __FUNCTION__, \
                            __FILE__, __LINE__)
/** LEAVE print */
#define LEAVE()         wps_printf(DEBUG_CALL_FLOW, "Leave: %s, %s:%i", __FUNCTION__, \
                            __FILE__, __LINE__)

#else /* STDOUT_DEBUG */

/** WPS debug print timestamp */
#define wps_debug_print_timestamp() do { } while (0)
/** WPS printf */
#define wps_printf(args...) do { } while (0)
/** WPS hexdump */
#define wps_hexdump(args...) do { } while (0)

/** ENTER print */
#define ENTER()
/** LEAVE print */
#define LEAVE()

#endif /* STDOUT_DEBUG */

#endif /* _WPS_UTIL_H_ */
