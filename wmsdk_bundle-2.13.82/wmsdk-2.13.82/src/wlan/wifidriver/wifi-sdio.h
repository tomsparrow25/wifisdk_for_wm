#ifndef __WIFI_SDIO_H__
#define __WIFI_SDIO_H__

/* fixme: remove this as soon as there is no dependancy */
#define INCLUDE_FROM_MLAN

#include <flash.h>
#include <wmlog.h>
#include <wifi.h>

#define wifi_io_e(...)				\
	wmlog_e("wifi_io", ##__VA_ARGS__)
#define wifi_io_w(...)				\
	wmlog_w("wifi_io", ##__VA_ARGS__)

#ifdef CONFIG_WIFI_IO_DEBUG
#define wifi_io_d(...)				\
	wmlog("wifi_io", ##__VA_ARGS__)
#else
#define wifi_io_d(...)
#endif /* ! CONFIG_WIFI_IO_DEBUG */

#define WLAN_MAGIC_NUM	(('W' << 0)|('L' << 8)|('F' << 16)|('W' << 24))

#define  SDIO_OUTBUF_LEN	2048

/* fixme: Force enable this flag. This could be made user configurable if
   we have the capability of dropping large sized packets */
#define CONFIG_ENABLE_AMSDU_RX


#ifdef CONFIG_ENABLE_AMSDU_RX
#define SDIO_INBUF_LEN	(2048 * 2)
#else /* ! CONFIG_ENABLE_AMSDU_RX */
#define  SDIO_INBUF_LEN	2048
#endif /* CONFIG_ENABLE_AMSDU_RX */

#if (SDIO_INBUF_LEN % MLAN_SDIO_BLOCK_SIZE)
#error "Please keep buffer length aligned to SDIO block size"
#endif /* Sanity check */

#if (SDIO_OUTBUF_LEN % MLAN_SDIO_BLOCK_SIZE)
#error "Please keep buffer length aligned to SDIO block size"
#endif /* Sanity check */

extern uint8_t outbuf[SDIO_OUTBUF_LEN];

typedef struct wlanfw_hdr {
	t_u32 magic_number;
	t_u32 len;
} wlanfw_hdr_type;

/** Card Control Registers : Function 1 Block size 0 */
#define FN1_BLOCK_SIZE_0 0x110
/** Card Control Registers : Function 1 Block size 1 */
#define FN1_BLOCK_SIZE_1 0x111

/* Duplicated in wlan.c. keep in sync till we can be included directly */
typedef struct __nvram_backup_struct {
	t_u32 ioport;
	t_u32 curr_wr_port;
	t_u32 curr_rd_port;
	t_u32 mp_end_port;
	t_u32 bss_num;
	t_u32 sta_mac_addr1;
	t_u32 sta_mac_addr2;
	t_u32 wifi_state;
} nvram_backup_t;

mlan_status sd878x_init(enum wlan_type type, flash_desc_t *fl);


/*
 * @internal
 *
 *
 */
int wlan_send_sdio_cmd(t_u8 *buf, t_u32 buflen);

/*
 * @internal
 *
 *
 */
int wifi_send_cmdbuffer(t_u32 len);


/*
 * @internal
 *
 *
 */
HostCmd_DS_COMMAND *wifi_get_command_buffer(void);

#endif /* __WIFI_SDIO_H__ */
