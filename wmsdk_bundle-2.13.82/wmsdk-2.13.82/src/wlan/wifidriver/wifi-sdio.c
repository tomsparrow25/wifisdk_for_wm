/* 
 *  Copyright 2008-2013, Marvell International Ltd.
 *  All Rights Reserved.
 */

#include <mlan_wmsdk.h>

#include <flash.h>
#include <mdev_sdio.h>

#if defined(CONFIG_XZ_DECOMPRESSION)
#include <xz.h>
#include <decompress.h>
#endif /* CONFIG_XZ_DECOMPRESSION */

/* Additional WMSDK header files */
#include <wmstdio.h>
#include <wmassert.h>
#include <wmerrno.h>
#include <wm_os.h>
#include <pwrmgr.h>
#include <wm_utils.h>

#include "wifi-sdio.h"
#include "wifi-internal.h"

/* fixme: sizeof(HostCmd_DS_COMMAND) is 1132 bytes. So have kept this at
   the current size.
*/
#define WIFI_FW_CMDBUF_SIZE 1200


/* Buffer pointers to point to command and, command response buffer */
static uint8_t cmd_buf[WIFI_FW_CMDBUF_SIZE];


nvram_backup_t backup_s __attribute__((section(".nvram")));

int g_txrx_flag;

int mlan_subsys_init(void);

uint8_t *wlanfw;

uint32_t txportno __attribute__((section(".nvram")));

os_semaphore_t g_wakeup_sem;

t_u32 last_resp_rcvd, last_cmd_sent;

os_mutex_t txrx_sem;
os_semaphore_t datasem;

/*
 * Used to authorize the SDIO interrupt handler to accept the incoming
 * packet from the SDIO interface. If this flag is set a semaphore is
 * signalled.
 */
extern int g_txrx_flag;
extern bool cal_data_valid;
extern bool mac_addr_valid;

void handle_data_packet(t_u8 *rcvdata, t_u16 datalen);

static struct {
	/* Where the cmdresp/event should be dispached depends on its value */
	/* int special; */
	/* Default queue where the cmdresp/events will be sent */
	xQueueHandle *event_queue;
	/* Special queue on which only cmdresp will be sent based on
	 * value in special */
	xQueueHandle *special_queue;
} bus;


/* remove this after mlan integration complete */
enum {
	MLAN_CARD_NOT_DETECTED = 3,
	MLAN_STATUS_FW_DNLD_FAILED,
	MLAN_STATUS_FW_NOT_DETECTED = 5,
	MLAN_STATUS_FW_NOT_READY,
	MLAN_CARD_CMD_TIMEOUT
};


/* fixme: This structure is not present in mlan and can be removed later */
typedef MLAN_PACK_START struct
{
	t_u16 size;
	t_u16 pkttype;    
	HostCmd_DS_COMMAND hostcmd;
} MLAN_PACK_END SDIOPkt;

extern SDIOPkt *sdiopkt;

uint8_t inbuf[SDIO_INBUF_LEN] \
		__attribute__ ((section(".iobufs"), aligned(4)));
uint8_t outbuf[SDIO_OUTBUF_LEN] \
		__attribute__ ((section(".iobufs"), aligned(4)));
SDIOPkt *sdiopkt = (SDIOPkt *)outbuf;

void wrapper_wlan_cmd_11n_cfg(void *hostcmd);
int wrapper_wlan_cmd_11d_enable(void *cmd);
void wrapper_wifi_ret_mib(void *resp);

uint8_t dev_mac_addr[MLAN_MAC_ADDR_LENGTH];
uint8_t dev_fw_ver_ext[MLAN_MAX_VER_STR_LEN];

int wifi_get_device_mac_addr(mac_addr_t *mac_addr)
{
	memcpy(mac_addr->mac, dev_mac_addr, MLAN_MAC_ADDR_LENGTH);
	return WM_SUCCESS;
}

int wifi_get_device_firmware_version_ext(wifi_fw_version_ext_t *fw_ver_ext)
{
	memcpy(fw_ver_ext->version_str, dev_fw_ver_ext, MLAN_MAX_VER_STR_LEN);
	return WM_SUCCESS;
}

/* Initializes the driver struct */
static int wlan_init_struct()
{
	if (txrx_sem == NULL) {
		int status = os_mutex_create(&txrx_sem, "txrx-mutex",
					     OS_MUTEX_INHERIT);
		if (status != WM_SUCCESS) {
			wifi_e("Mutex create failed");
			return status;
		}
	}
	return WM_SUCCESS;
}


static void sd878x_enter_pm4()
{
	/* save the amc adr*/
	backup_s.sta_mac_addr1 = 0;
	backup_s.sta_mac_addr2 = 0;
	backup_s.sta_mac_addr1 = dev_mac_addr[0] << 24|
		dev_mac_addr[1] << 16|
		dev_mac_addr[2] << 8|
		dev_mac_addr[3];
	backup_s.sta_mac_addr2 = dev_mac_addr[4] << 24|
		dev_mac_addr[5] << 16;
	backup_s.ioport = mlan_adap->ioport;
	backup_s.mp_end_port = mlan_adap->mp_end_port;
	backup_s.curr_rd_port = mlan_adap->curr_rd_port;
	backup_s.curr_wr_port = mlan_adap->curr_wr_port;
}

static void sd878x_exit_pm4()
{
	mlan_adap->ioport = backup_s.ioport;
	mlan_adap->mp_end_port = backup_s.mp_end_port ;
	mlan_adap->curr_rd_port = backup_s.curr_rd_port ;
	mlan_adap->curr_wr_port = backup_s.curr_wr_port;
	/* restore the mac addrr*/
	dev_mac_addr[0] = backup_s.sta_mac_addr1 >> 24;
	dev_mac_addr[1] = (backup_s.sta_mac_addr1 >> 16) & 0xff;
	dev_mac_addr[2] = (backup_s.sta_mac_addr1 >> 8) & 0xff;
	dev_mac_addr[3] = (backup_s.sta_mac_addr1) & 0xff;
	dev_mac_addr[4] = backup_s.sta_mac_addr2 >> 24;
	dev_mac_addr[5] = (backup_s.sta_mac_addr2 >> 16) & 0xff;
}


/*
 * fixme: mlan_sta_tx.c can be used directly here. This functionality is
 * already present there.
 */
/*         SDIO               TxPD             PAYLOAD                          |         4         |       22       |       payload        |	*/

/* we return the offset of the payload from the beginning of the buffer */
int process_pkt_hdrs(void *pbuf, t_u32 payloadlen, t_u8 interface)
{
	mlan_private *pmpriv = (mlan_private *) mlan_adap->priv[0];
	SDIOPkt *sdiohdr = (SDIOPkt *) pbuf;
	TxPD *ptxpd = (TxPD *) (pbuf + INTF_HEADER_LEN);

	ptxpd->bss_type = interface;
	ptxpd->bss_num = GET_BSS_NUM(pmpriv);
	ptxpd->tx_pkt_offset = 0x16;	/* we'll just make this constant */
	ptxpd->tx_pkt_length = payloadlen - ptxpd->tx_pkt_offset - INTF_HEADER_LEN;
	ptxpd->tx_pkt_type = 0;
	ptxpd->tx_control = 0;
	ptxpd->priority = 0;
	ptxpd->flags = 0;
	ptxpd->pkt_delay_2ms = 0;

	sdiohdr->size = payloadlen + ptxpd->tx_pkt_offset + INTF_HEADER_LEN;

	return (ptxpd->tx_pkt_offset + INTF_HEADER_LEN);
}

static int wlan_card_fw_status(mdev_t *sdio_drv, t_u16 *dat)
{
	t_u32 resp;

	sdio_drv_creg_read(sdio_drv, CARD_FW_STATUS0_REG, 1, &resp);
	*dat = resp & 0xff;
	sdio_drv_creg_read(sdio_drv, CARD_FW_STATUS1_REG, 1, &resp);
	*dat |= (resp & 0xff) << 8;

	return true;
}

static bool wlan_card_ready_wait(mdev_t *sdio_drv, t_u32 poll)
{
	t_u16 dat;
	int i;

	for (i = 0; i < poll; i++) {
		wlan_card_fw_status(sdio_drv, &dat);
		if (dat == FIRMWARE_READY) {
			wifi_io_d("Firmware Ready");
			return true;
		}
		os_thread_sleep(os_msec_to_ticks(1));
	}
	return false;
}

static t_u16 wlan_card_read_f1_base_regs(mdev_t *sdio_drv)
{
	t_u16 reg;
	t_u32 resp;

	sdio_drv_creg_read(sdio_drv, READ_BASE_0_REG, 1, &resp);
	reg = resp & 0xff;
	sdio_drv_creg_read(sdio_drv, READ_BASE_1_REG, 1, &resp);
	reg |= (resp & 0xff) << 8;

	return reg;
}

/**
 *  @brief This function reads the CARD_TO_HOST_EVENT_REG and
 *  checks if input bits are set
 *  @param bits		bits to check status against
 *  @return		true if bits are set
 *                      SDIO_POLLING_STATUS_TIMEOUT if bits
 *                      aren't set
 */
static int wlan_card_status(mdev_t *sdio_drv, t_u8 bits)
{
	t_u32 resp;
	t_u32 tries;

	for (tries = 0; tries < MAX_POLL_TRIES; tries++) {
		sdio_drv_creg_read(sdio_drv, CARD_TO_HOST_EVENT_REG,
				   1, &resp);
		if ((resp & bits) == bits) {
			return true;
		}
		os_thread_sleep(os_msec_to_ticks(1));
	}
	return false;
}

mlan_status wlan_download_normal_fw(mdev_t *sdio_drv, mdev_t *fl_dev,
				    t_u8 *wlanfw, t_u32 firmwarelen,
				    t_u32 ioport)
{
	t_u32 tx_blocks = 0, txlen = 0, buflen = 0;
	t_u16 len = 0;
	t_u32 offset = 0;
	t_u32 tries = 0;
	t_u32 resp;

	memset(outbuf, 0, SDIO_OUTBUF_LEN);

	do {
		if (offset >= firmwarelen) {
			break;
		}

		/* Read CARD_STATUS_REG (0X30) FN =1 */
		for (tries = 0; tries < MAX_POLL_TRIES; tries++) {
			if (wlan_card_status(sdio_drv,
				DN_LD_CARD_RDY | CARD_IO_READY) == true) {
				len = wlan_card_read_f1_base_regs(sdio_drv);
			} else {
				wifi_io_e("Error in wlan_card_status()");
				break;
			}

			if (len)
				break;
		}

		if (!len) {
			wifi_io_e("Card timeout %s:%d", __func__, __LINE__);
			break;
		} else if (len > WLAN_UPLD_SIZE) {
			wifi_io_e("FW Download Failure. Invalid len");
			return MLAN_STATUS_FW_DNLD_FAILED;
		}

		txlen = len;

		/* Set blocksize to transfer - checking for last block */
		if (firmwarelen && (firmwarelen - offset) < txlen) {
			txlen = firmwarelen - offset;
		}

		/*
		 * fixme: check and use calculate_sdio_write_params() for
		 * this.
		 */
		if (txlen > 512) {
			tx_blocks =
			    (txlen + MLAN_SDIO_BLOCK_SIZE_FW_DNLD -
			     1) / MLAN_SDIO_BLOCK_SIZE_FW_DNLD;
			buflen = MLAN_SDIO_BLOCK_SIZE_FW_DNLD;
		} else {
			tx_blocks =
			    (txlen + MLAN_SDIO_BLOCK_SIZE_FW_DNLD -
			     1) / MLAN_SDIO_BLOCK_SIZE_FW_DNLD;
			buflen = tx_blocks * MLAN_SDIO_BLOCK_SIZE_FW_DNLD;

			tx_blocks = 1;
		}
		flash_drv_read(fl_dev, outbuf, txlen,
			       (t_u32) (wlanfw + offset));

		sdio_drv_write(sdio_drv, ioport, 1, tx_blocks, buflen,
						(t_u8 *) outbuf, &resp);
		offset += txlen;
		len = 0;
	} while (1);

	return MLAN_STATUS_SUCCESS;
}

#if defined(CONFIG_XZ_DECOMPRESSION)
mlan_status wlan_download_decomp_fw(mdev_t *sdio_drv, mdev_t *fl_dev,
				    t_u8 *wlanfw, t_u32 firmwarelen,
				    t_u32 ioport)
{
	t_u32 tx_blocks = 0, txlen = 0, buflen = 0;
	t_u16 len = 0;
	t_u32 offset = 0;
	t_u32 tries = 0;
	t_u32 resp;

	memset(outbuf, 0, SDIO_OUTBUF_LEN);

	#define SBUF_SIZE 2048
	int ret;
	struct xz_buf stream;
	t_u32 retlen, readlen = 0;
	t_u8 *sbuf = (t_u8 *)os_mem_alloc(SBUF_SIZE);
	if (sbuf == NULL) {
		wifi_io_e("Allocation failed");
		return MLAN_STATUS_FAILURE;
	}

	xz_uncompress_init(&stream, sbuf, outbuf);

	do {
		/* Read CARD_STATUS_REG (0X30) FN =1 */
		for (tries = 0; tries < MAX_POLL_TRIES; tries++) {
			if (wlan_card_status(sdio_drv,
				DN_LD_CARD_RDY | CARD_IO_READY) == true) {
				len = wlan_card_read_f1_base_regs(sdio_drv);
			} else {
				wifi_io_e("Error in wlan_card_status()");
				break;
			}

			if (len)
				break;
		}

		if (!len) {
			wifi_io_e("Card timeout %s:%d", __func__, __LINE__);
			break;
		} else if (len > WLAN_UPLD_SIZE) {
			wifi_io_e("FW Download Failure. Invalid len");
			xz_uncompress_end();
			os_mem_free(sbuf);
			return MLAN_STATUS_FW_DNLD_FAILED;
		}

		txlen = len;

		do {
			if (stream.in_pos == stream.in_size) {
				readlen = MIN(SBUF_SIZE, firmwarelen);
				flash_drv_read(fl_dev, sbuf, readlen,
						(t_u32)(wlanfw + offset));
				offset += readlen;
				firmwarelen -= readlen;
			}
			ret = xz_uncompress_stream(&stream, sbuf,
						   readlen, outbuf,
					txlen, &retlen);

			if (ret == XZ_STREAM_END) {
				txlen = retlen;
				break;
			} else if (ret !=	XZ_OK) {
				wifi_io_e("Decompression failed:%d", ret);
				break;
			}
		} while (retlen == 0);

		/*
		 * fixme: check and use calculate_sdio_write_params() for
		 * this.
		 */
		if (txlen > 512) {
			tx_blocks =
			    (txlen + MLAN_SDIO_BLOCK_SIZE_FW_DNLD -
			     1) / MLAN_SDIO_BLOCK_SIZE_FW_DNLD;
			buflen = MLAN_SDIO_BLOCK_SIZE_FW_DNLD;
		} else {
			tx_blocks =
			    (txlen + MLAN_SDIO_BLOCK_SIZE_FW_DNLD -
			     1) / MLAN_SDIO_BLOCK_SIZE_FW_DNLD;
			buflen = tx_blocks * MLAN_SDIO_BLOCK_SIZE_FW_DNLD;

			tx_blocks = 1;
		}

		sdio_drv_write(sdio_drv, ioport, 1, tx_blocks, buflen,
						(t_u8 *) outbuf, &resp);

		if (ret == XZ_STREAM_END) {
			wifi_io_d("Decompression successful");
			break;
		} else if (ret != XZ_OK) {
			wifi_io_e("Exit:%d", ret);
			break;
		}
		len = 0;
	} while (1);

	xz_uncompress_end();
	os_mem_free(sbuf);

	if (ret == XZ_OK || ret == XZ_STREAM_END)
		return MLAN_STATUS_SUCCESS;
	else
		return MLAN_STATUS_FAILURE;
}
#endif /* CONFIG_XZ_DECOMPRESSION */

/*
 * FW dnld blocksize set 0x110 to 0 and 0x111 to 0x01 => 0x100 => 256
 * Note this only applies to the blockmode we use 256 bytes
 * as block because MLAN_SDIO_BLOCK_SIZE = 256
 */
static int wlan_set_fw_dnld_size(mdev_t *sdio_drv)
{
	t_u32 resp;

	int rv = sdio_drv_creg_write(sdio_drv, FN1_BLOCK_SIZE_0, 0, 0, &resp);
	if (rv == false)
		return -WM_FAIL;

	rv = sdio_drv_creg_write(sdio_drv, FN1_BLOCK_SIZE_1, 0, 1, &resp);
	if (rv == false)
		return -WM_FAIL;

	return WM_SUCCESS;
}

/*
 * Download firmware to the card through SDIO.
 * The firmware is stored in Flash.
 */
static mlan_status wlan_download_fw(mdev_t *sdio_drv, flash_desc_t *fl)
{
	t_u32 firmwarelen;
	wlanfw_hdr_type wlanfwhdr;
	mdev_t *fl_dev;
	int ret;

	/* set fw download block size */
	ret = wlan_set_fw_dnld_size(sdio_drv);
	if (ret != WM_SUCCESS)
		return ret;

	fl_dev = flash_drv_open(fl->fl_dev);
	if (fl_dev == NULL) {
		wifi_io_e("Flash driver init is required before open");
		return MLAN_STATUS_FW_NOT_DETECTED;
	}

	wlanfw = (t_u8 *)fl->fl_start;

	wifi_io_d("Start copying wlan firmware over sdio from 0x%x",
		     (t_u32) wlanfw);

	flash_drv_read(fl_dev, (t_u8 *) &wlanfwhdr, sizeof(wlanfwhdr),
		       (t_u32) wlanfw);

	if (wlanfwhdr.magic_number != WLAN_MAGIC_NUM) {
		wifi_io_e("WLAN FW not detected in Flash.");
		return MLAN_STATUS_FW_NOT_DETECTED;
	}

	wifi_io_d("Valid WLAN FW found in %s flash",
			fl->fl_dev ? "external" : "internal");

	/* skip the wlanhdr and move wlanfw to beginning of the firmware */
	wlanfw += sizeof(wlanfwhdr);
	firmwarelen = wlanfwhdr.len;

#if defined(CONFIG_XZ_DECOMPRESSION)
	t_u8 buffer[6];
	flash_drv_read(fl_dev, buffer, sizeof(buffer), (t_u32) wlanfw);

	/* See if image is XZ compressed or not */
	if (verify_xz_header(buffer) == WM_SUCCESS) {
		wifi_io_d("XZ Compressed image found, start decompression,"
				" len: %d", firmwarelen);
		ret = wlan_download_decomp_fw(sdio_drv, fl_dev, wlanfw,
					      firmwarelen, mlan_adap->ioport);
	} else
#endif /* CONFIG_XZ_DECOMPRESSION */
	{
		wifi_io_d("Un-compressed image found, start download,"
				" len: %d", firmwarelen);
		ret = wlan_download_normal_fw(sdio_drv, fl_dev, wlanfw,
					      firmwarelen, mlan_adap->ioport);
	}

	flash_drv_close(fl_dev);

	if (ret != MLAN_STATUS_SUCCESS)
		return ret;

	if (wlan_card_ready_wait(sdio_drv, 300) != true) {
		wifi_io_e("SDIO - FW Ready Registers not set");
		return MLAN_STATUS_FW_NOT_READY;
	} else {
		wifi_io_d("WLAN FW download Successful");
		return MLAN_STATUS_SUCCESS;
	}
}

int bus_register_event_queue(xQueueHandle *event_queue)
{
	if (bus.event_queue)
		return -WM_FAIL;

	bus.event_queue = event_queue;

	return WM_SUCCESS;
}

void bus_deregister_event_queue()
{
	if (bus.event_queue)
		bus.event_queue = NULL;
}

#ifdef CONFIG_P2P
int bus_register_special_queue(xQueueHandle *special_queue)
{
	if (bus.special_queue)
		return -WM_FAIL;
	bus.special_queue = special_queue;
	return WM_SUCCESS;
}

void bus_deregister_special_queue()
{
	if (bus.special_queue)
		bus.special_queue = NULL;
}
#endif

void wifi_get_mac_address_from_cmdresp(void *resp, t_u8 *mac_addr);
void wifi_get_firmware_ver_ext_from_cmdresp(void *resp, t_u8 *fw_ver_ext);
mlan_status wlan_handle_cmd_resp_packet(t_u8 *pmbuf)
{
	HostCmd_DS_GEN *cmdresp;
	t_u32 cmdtype;

	cmdresp = (HostCmd_DS_GEN *)
	    (pmbuf + INTF_HEADER_LEN);	/* size + pkttype=4 */
	cmdtype = cmdresp->command & HostCmd_CMD_ID_MASK;

	last_resp_rcvd = cmdtype;

	if ((cmdresp->command & 0xff00) != 0x8000) {
		wifi_io_e("cmdresp->command = (0x%x)", cmdresp->command);
	}

	if ((cmdresp->command & 0xff) != last_cmd_sent) {
		wifi_io_e
		    ("cmdresp->command = (0x%x) last_cmd_sent"
			"=(0x%lx)", cmdresp->command, last_cmd_sent);
	}

	if (cmdresp->result) {
		wifi_io_e("cmdresp->result = (0x%x)", cmdresp->result);
	}

	wifi_io_d("Resp : (0x%lx)", cmdtype);
	switch (cmdtype) {
	case HostCmd_CMD_MAC_CONTROL:
	case HostCmd_CMD_FUNC_INIT:
	case HostCmd_CMD_CFG_DATA:
		break;
	case HostCmd_CMD_802_11_MAC_ADDRESS:
		wifi_get_mac_address_from_cmdresp(cmdresp, dev_mac_addr);
		break;
	case HostCmd_CMD_GET_HW_SPEC:
		wlan_ret_get_hw_spec
			((mlan_private *) mlan_adap->priv[0],
			 (HostCmd_DS_COMMAND *) cmdresp, NULL);
		break;
	case HostCmd_CMD_VERSION_EXT:
		wifi_get_firmware_ver_ext_from_cmdresp(cmdresp, dev_fw_ver_ext);
		break;
#ifdef CONFIG_11N
	case HostCmd_CMD_11N_CFG:
	case HostCmd_CMD_AMSDU_AGGR_CTRL:
		break;
#endif
	case HostCmd_CMD_FUNC_SHUTDOWN:
		break;
	case HostCmd_CMD_802_11_SNMP_MIB:
		wrapper_wifi_ret_mib(cmdresp);
		break;
	default:
		wifi_io_d("Unimplemented Resp : (0x%lx)", cmdtype);
		dump_hex(cmdresp, cmdresp->size);
		break;
	}

	return MLAN_STATUS_SUCCESS;
}

/*
 * Accepts event and command packets. Redirects them to queues if
 * registered. If queues are not registered (as is the case during
 * initialization then the packet is given to lower layer cmd/event
 * handling part.
 */
static mlan_status wlan_decode_rx_packet(t_u8 *pmbuf, t_u32 upld_typ)
{
	if (upld_typ == MLAN_TYPE_DATA)
		return MLAN_STATUS_FAILURE;

	if (upld_typ == MLAN_TYPE_CMD)
		wifi_io_d("  --- Rx: Cmd Response ---");
	else
		wifi_io_d(" --- Rx: EVENT Response ---");

	t_u8 *cmdBuf;
	SDIOPkt *sdiopkt = (SDIOPkt *)pmbuf;
	int ret;
	struct bus_message msg;


	if (bus.special_queue != NULL && upld_typ == MLAN_TYPE_CMD) {
		msg.data = os_mem_alloc(sdiopkt->size);
		if (!msg.data) {
			wifi_io_e("Buffer allocation failed");
			return MLAN_STATUS_FAILURE;
		}

		msg.event = upld_typ;
		cmdBuf = pmbuf;
		cmdBuf = cmdBuf + 4;
		memcpy(msg.data, cmdBuf, sdiopkt->size);

		ret = xQueueSendToBack(*(bus.special_queue), &msg, 0);

		if (ret != pdTRUE) {
			wifi_io_e("xQueueSendToBack Failed");
		}
	} else if (bus.event_queue) {
		if (upld_typ == MLAN_TYPE_CMD)
			msg.data = wifi_mem_malloc_cmdrespbuf(sdiopkt->size);
		else
			msg.data = wifi_malloc_eventbuf(sdiopkt->size);

		if (!msg.data) {
			wifi_io_e("[fail] Buffer alloc: T: %d S: %d",
				  upld_typ, sdiopkt->size);
			return MLAN_STATUS_FAILURE;
		}

		msg.event = upld_typ;
		memcpy(msg.data, pmbuf, sdiopkt->size);

		ret = xQueueSendToBack(*(bus.event_queue), &msg, 10);

		if (ret != pdTRUE) {
			wifi_io_e("xQueueSendToBack Failed");
		}
	} else {
		/* No queues registered yet. Use local handling */
		wlan_handle_cmd_resp_packet(pmbuf);
	}

	return MLAN_STATUS_SUCCESS;
}

static mlan_status wlan_sdio_init_ioport(mdev_t *sdio_drv)
{
	t_u32 resp;
	t_u8 data;

	/* Read the PORT regs for IOPORT address */
	sdio_drv_creg_read(sdio_drv, IO_PORT_0_REG, 1, &resp);
	mlan_adap->ioport = (resp & 0xff);

	sdio_drv_creg_read(sdio_drv, IO_PORT_1_REG, 1, &resp);
	mlan_adap->ioport |= ((resp & 0xff) << 8);

	sdio_drv_creg_read(sdio_drv, IO_PORT_2_REG, 1, &resp);
	mlan_adap->ioport |= ((resp & 0xff) << 16);

	wifi_io_d("IOPORT : (0x%lx)", mlan_adap->ioport);

	/* Set Host interrupt reset to read to clear */
	sdio_drv_creg_read(sdio_drv, HOST_INT_RSR_REG, 1, &resp);
	data = (resp & 0xff) | HOST_INT_RSR_MASK;
	sdio_drv_creg_write(sdio_drv, HOST_INT_RSR_REG, 1, data, &resp);

	/* Dnld/Upld ready set to auto reset */
	sdio_drv_creg_read(sdio_drv, CARD_MISC_CFG_REG, 1, &resp);
	data = (resp & 0xff) | AUTO_RE_ENABLE_INT;
	sdio_drv_creg_write(sdio_drv, CARD_MISC_CFG_REG, 1, data, &resp);

	return true;
}

t_u32 get_ioport()
{
	return mlan_adap->ioport;
}

static t_u8 *wlan_read_rcv_packet(t_u32 port, t_u32 rxlen, t_u32 *type)
{
	t_u32 rx_blocks = 0, blksize = 0;
	t_u32 resp;

	if (rxlen > SDIO_INBUF_LEN) {
		/* Call abort or panic kind of function that will put
		the thread in hang state. */
		while (1) {
			wifi_io_e("PANIC:Received packet is bigger than"
					 " inbuf. rxlen:%ld", rxlen);
			vTaskDelay((3000) / portTICK_RATE_MS);
		}
	}

	if (rxlen > 512) {
		rx_blocks = rxlen / MLAN_SDIO_BLOCK_SIZE;

		if (rxlen % MLAN_SDIO_BLOCK_SIZE)
			rx_blocks++;

		blksize = MLAN_SDIO_BLOCK_SIZE;
	} else {
		rx_blocks = 1;

		if (rxlen > MLAN_SDIO_BLOCK_SIZE) {
			blksize = 512;
		} else {
			blksize = MLAN_SDIO_BLOCK_SIZE;
		}
	}

	/* addr = 0 fn = 1 */
	sdio_drv_read(NULL, mlan_adap->ioport + port, 1, rx_blocks, blksize,
		      inbuf, &resp);

	SDIOPkt *insdiopkt = (SDIOPkt *) inbuf;
	*type = insdiopkt->pkttype;

#ifdef CONFIG_WIFI_IO_DUMP
	wmprintf("wlan_read_rcv_packet: DUMP:");
	dump_hex((t_u8 *) insdiopkt, insdiopkt->size);
#endif /* CONFIG_WIFI_IO_DUMP */

	return inbuf;
}


static t_u32 wlan_get_next_seq_num()
{
	static t_u32 seqnum;
	seqnum++;
	return seqnum;
}


#ifdef CONFIG_11D
static void wlan_enable_11d()
{
	t_u32 tx_blocks = 1, buflen = MLAN_SDIO_BLOCK_SIZE;
	t_u32 resp;
	memset(outbuf, 0, SDIO_OUTBUF_LEN);
	wrapper_wlan_cmd_11d_enable(&sdiopkt->hostcmd);
	/* sdiopkt = outbuf */
	sdiopkt->hostcmd.seq_num = wlan_get_next_seq_num();
	sdiopkt->pkttype = MLAN_TYPE_CMD;
	last_cmd_sent = HostCmd_CMD_802_11_SNMP_MIB;

	sdio_drv_write(NULL, mlan_adap->ioport, 1, tx_blocks, buflen,
						(t_u8 *) outbuf, &resp);
}
#endif /* CONFIG_11D */

void wifi_prepare_set_cal_data_cmd(void *cmd, int seq_number);
static int wlan_set_cal_data()
{
	t_u32 tx_blocks = 4, buflen = MLAN_SDIO_BLOCK_SIZE;
	t_u32 resp;

	memset(outbuf, 0, SDIO_OUTBUF_LEN);

	/* sdiopkt = outbuf */
	wifi_prepare_set_cal_data_cmd(&sdiopkt->hostcmd,
				      wlan_get_next_seq_num());

	sdiopkt->pkttype = MLAN_TYPE_CMD;
	sdiopkt->size = sdiopkt->hostcmd.size + INTF_HEADER_LEN;

	last_cmd_sent = HostCmd_CMD_CFG_DATA;

	/* send CMD53 to write the command to get mac address */
	sdio_drv_write(NULL, mlan_adap->ioport, 1, tx_blocks, buflen,
						(t_u8 *) outbuf, &resp);

	return true;
}

void wifi_prepare_get_mac_addr_cmd(void *cmd, int seq_number);
void wifi_prepare_get_hw_spec_cmd(HostCmd_DS_COMMAND *cmd,
				  int seq_number);


static int wlan_get_hw_spec()
{
	uint32_t tx_blocks = 1, buflen = MLAN_SDIO_BLOCK_SIZE;
	uint32_t resp;
	memset(outbuf, 0, buflen);
	/* sdiopkt = outbuf */
	wifi_prepare_get_hw_spec_cmd(&sdiopkt->hostcmd,
				     wlan_get_next_seq_num());

	sdiopkt->pkttype = MLAN_TYPE_CMD;
	sdiopkt->size = sdiopkt->hostcmd.size + INTF_HEADER_LEN;

	last_cmd_sent = HostCmd_CMD_GET_HW_SPEC;
	sdio_drv_write(NULL, mlan_adap->ioport, 1, tx_blocks, buflen,
		       (t_u8 *) outbuf, &resp);
	return true;
}

static int wlan_get_mac_addr()
{
	t_u32 tx_blocks = 1, buflen = MLAN_SDIO_BLOCK_SIZE;
	t_u32 resp;

	memset(outbuf, 0, buflen);

	/* sdiopkt = outbuf */
	wifi_prepare_get_mac_addr_cmd(&sdiopkt->hostcmd,
				      wlan_get_next_seq_num());

	sdiopkt->pkttype = MLAN_TYPE_CMD;
	sdiopkt->size = sdiopkt->hostcmd.size + INTF_HEADER_LEN;

	last_cmd_sent = HostCmd_CMD_802_11_MAC_ADDRESS;

	/* send CMD53 to write the command to get mac address */
	sdio_drv_write(NULL, mlan_adap->ioport, 1, tx_blocks, buflen,
						(t_u8 *) outbuf, &resp);

	return true;
}

void wifi_prepare_get_fw_ver_ext_cmd(void *cmd, int seq_number);
static int wlan_get_fw_ver_ext()
{
	t_u32 tx_blocks = 1, buflen = MLAN_SDIO_BLOCK_SIZE;
	t_u32 resp;

	memset(outbuf, 0, buflen);

	/* sdiopkt = outbuf */
	wifi_prepare_get_fw_ver_ext_cmd(&sdiopkt->hostcmd,
				      wlan_get_next_seq_num());

	sdiopkt->pkttype = MLAN_TYPE_CMD;
	sdiopkt->size = sdiopkt->hostcmd.size + INTF_HEADER_LEN;

	last_cmd_sent = HostCmd_CMD_VERSION_EXT;

	/* send CMD53 to write the command to get mac address */
	sdio_drv_write(NULL, mlan_adap->ioport, 1, tx_blocks, buflen,
						(t_u8 *) outbuf, &resp);

	return true;
}

void wifi_prepare_set_mac_addr_cmd(void *cmd, int seq_number);
static int _wlan_set_mac_addr()
{
	t_u32 tx_blocks = 1, buflen = MLAN_SDIO_BLOCK_SIZE;
	t_u32 resp;

	memset(outbuf, 0, buflen);

	/* sdiopkt = outbuf */
	wifi_prepare_set_mac_addr_cmd(&sdiopkt->hostcmd,
				      wlan_get_next_seq_num());

	sdiopkt->pkttype = MLAN_TYPE_CMD;
	sdiopkt->size = sdiopkt->hostcmd.size + INTF_HEADER_LEN;

	last_cmd_sent = HostCmd_CMD_802_11_MAC_ADDRESS;

	/* send CMD53 to write the command to get mac address */
	sdio_drv_write(NULL, mlan_adap->ioport, 1, tx_blocks, buflen,
						(t_u8 *) outbuf, &resp);

	return true;
}

#ifdef CONFIG_11N
static int wlan_set_11n_cfg()
{
	t_u32 tx_blocks = 1, buflen = MLAN_SDIO_BLOCK_SIZE;
	t_u32 resp;

	memset(outbuf, 0, SDIO_OUTBUF_LEN);
	wrapper_wlan_cmd_11n_cfg(&sdiopkt->hostcmd);
	/* sdiopkt = outbuf */
	sdiopkt->hostcmd.seq_num = wlan_get_next_seq_num();
	sdiopkt->pkttype = MLAN_TYPE_CMD;
	last_cmd_sent = HostCmd_CMD_11N_CFG;

	sdio_drv_write(NULL, mlan_adap->ioport, 1, tx_blocks, buflen,
						(t_u8 *) outbuf, &resp);
	return true;
}

#ifdef CONFIG_ENABLE_AMSDU_RX
void wifi_prepare_enable_amsdu_cmd(void *cmd, int seq_number);
static int wlan_enable_amsdu()
{
	t_u32 tx_blocks = 1, buflen = MLAN_SDIO_BLOCK_SIZE;
	t_u32 resp;

	memset(outbuf, 0, buflen);

	/* sdiopkt = outbuf */
	wifi_prepare_enable_amsdu_cmd(&sdiopkt->hostcmd,
				      wlan_get_next_seq_num());

	sdiopkt->pkttype = MLAN_TYPE_CMD;
	sdiopkt->size = sdiopkt->hostcmd.size + INTF_HEADER_LEN;

	last_cmd_sent = HostCmd_CMD_AMSDU_AGGR_CTRL;

	sdio_drv_write(NULL, mlan_adap->ioport, 1, tx_blocks, buflen,
						(t_u8 *) outbuf, &resp);

	return true;
}
#endif /* CONFIG_ENABLE_AMSDU_RX */
#endif /* CONFIG_11N */

static int wlan_cmd_shutdown()
{
	t_u32 tx_blocks = 1, buflen = MLAN_SDIO_BLOCK_SIZE;
	t_u32 resp;

	memset(outbuf, 0, buflen);

	/* sdiopkt = outbuf */
	sdiopkt->hostcmd.command = HostCmd_CMD_FUNC_SHUTDOWN;
	sdiopkt->hostcmd.size = S_DS_GEN;
	sdiopkt->hostcmd.seq_num = wlan_get_next_seq_num();
	sdiopkt->hostcmd.result = 0;

	sdiopkt->pkttype = MLAN_TYPE_CMD;
	sdiopkt->size = sdiopkt->hostcmd.size + INTF_HEADER_LEN;

	last_cmd_sent = HostCmd_CMD_FUNC_SHUTDOWN;

	sdio_drv_write(NULL, mlan_adap->ioport, 1, tx_blocks, buflen,
						(t_u8 *) outbuf, &resp);

	return true;
}

void wlan_prepare_mac_control_cmd(void *cmd, int seq_number);
static int wlan_set_mac_ctrl()
{
	t_u32 tx_blocks = 1, buflen = MLAN_SDIO_BLOCK_SIZE;
	t_u32 resp;

	memset(outbuf, 0, buflen);

	/* sdiopkt = outbuf */
	wlan_prepare_mac_control_cmd(&sdiopkt->hostcmd,
				     wlan_get_next_seq_num());

	sdiopkt->pkttype = MLAN_TYPE_CMD;
	sdiopkt->size = sdiopkt->hostcmd.size + INTF_HEADER_LEN;

	last_cmd_sent = HostCmd_CMD_MAC_CONTROL;

	/* send CMD53 to write the command to set mac control */
	sdio_drv_write(NULL, mlan_adap->ioport, 1, tx_blocks, buflen,
						(t_u8 *) outbuf, &resp);

	return true;
}

static int wlan_cmd_init()
{
	t_u32 tx_blocks = 1, buflen = MLAN_SDIO_BLOCK_SIZE;
	t_u32 resp;

	memset(outbuf, 0, buflen);

	/* sdiopkt = outbuf */
	sdiopkt->hostcmd.command = HostCmd_CMD_FUNC_INIT;
	sdiopkt->hostcmd.size = S_DS_GEN;
	sdiopkt->hostcmd.seq_num = wlan_get_next_seq_num();
	sdiopkt->hostcmd.result = 0;

	sdiopkt->pkttype = MLAN_TYPE_CMD;
	sdiopkt->size = sdiopkt->hostcmd.size + INTF_HEADER_LEN;

	last_cmd_sent = HostCmd_CMD_FUNC_INIT;

	sdio_drv_write(NULL, mlan_adap->ioport, 1, tx_blocks, buflen,
						(t_u8 *) outbuf, &resp);

	return true;
}

mlan_status wlan_process_int_status(mlan_adapter *pmadapter);
/* Setup the firmware with commands */
static void wlan_fw_init_cfg()
{
	wifi_io_d("FWCMD : INIT (0xa9)");

	wlan_cmd_init();

	while (last_resp_rcvd != HostCmd_CMD_FUNC_INIT) {
		os_thread_sleep(os_msec_to_ticks(10));
		wlan_process_int_status(mlan_adap);
	}

	if (cal_data_valid) {

		wifi_io_d("CMD : SET_CAL_DATA (0x8f)");

		wlan_set_cal_data();

		while (last_resp_rcvd != HostCmd_CMD_CFG_DATA) {
			os_thread_sleep(os_msec_to_ticks(10));
			wlan_process_int_status(mlan_adap);
		}
	}

	if (mac_addr_valid) {
		wifi_io_d("CMD : SET_MAC_ADDR (0x4d)");

		_wlan_set_mac_addr();

		while (last_resp_rcvd != HostCmd_CMD_802_11_MAC_ADDRESS) {
			os_thread_sleep(os_msec_to_ticks(10));
			wlan_process_int_status(mlan_adap);
		}
	}

	wifi_io_d("CMD : GET_HW_SPEC (0x03");
	wlan_get_hw_spec();
	while (last_resp_rcvd != HostCmd_CMD_GET_HW_SPEC) {
		os_thread_sleep(os_msec_to_ticks(10));
		wlan_process_int_status(mlan_adap);
	}

	wifi_io_d("CMD : GET_MAC_ADDR (0x4d)");

	wlan_get_mac_addr();

	while (last_resp_rcvd != HostCmd_CMD_802_11_MAC_ADDRESS) {
		os_thread_sleep(os_msec_to_ticks(10));
		wlan_process_int_status(mlan_adap);
	}

	wifi_io_d("CMD : GET_FW_VER_EXT (0x97)");

	wlan_get_fw_ver_ext();

	while (last_resp_rcvd != HostCmd_CMD_VERSION_EXT) {
		os_thread_sleep(os_msec_to_ticks(10));
		wlan_process_int_status(mlan_adap);
	}

	wifi_io_d("CMD : MAC_CTRL (0x28)");

	wlan_set_mac_ctrl();

	while (last_resp_rcvd != HostCmd_CMD_MAC_CONTROL) {
		os_thread_sleep(os_msec_to_ticks(10));
		wlan_process_int_status(mlan_adap);
	}


#ifdef CONFIG_11N
	wlan_set_11n_cfg();

	while (last_resp_rcvd != HostCmd_CMD_11N_CFG) {
		os_thread_sleep(os_msec_to_ticks(1));
		wlan_process_int_status(mlan_adap);
	}

#ifdef CONFIG_ENABLE_AMSDU_RX
	wlan_enable_amsdu();

	while (last_resp_rcvd != HostCmd_CMD_AMSDU_AGGR_CTRL) {
		os_thread_sleep(os_msec_to_ticks(1));
		wlan_process_int_status(mlan_adap);
	}
#endif /* CONFIG_ENABLE_AMSDU_RX */
#endif /* CONFIG_11N */

#ifdef CONFIG_11D
	wlan_enable_11d();

	while (last_resp_rcvd != HostCmd_CMD_802_11_SNMP_MIB) {
		os_thread_sleep(os_msec_to_ticks(10));
		wlan_process_int_status(mlan_adap);
	}
#endif /* CONFIG_11D */

	wifi_io_d("CMD : SNMP_MIB (0x16)");

	return;
}

#ifdef CONFIG_P2P
mlan_status wlan_send_gen_sdio_cmd(t_u8 *buf, t_u32 buflen)
{
	SDIOPkt *sdio = (SDIOPkt *) outbuf;
	t_u32 resp;

	memset(outbuf, 0, 512);

	os_mutex_get(&txrx_sem, OS_WAIT_FOREVER);
	memcpy(outbuf, buf, buflen);
	sdio->pkttype = MLAN_TYPE_CMD;
	sdio->hostcmd.seq_num = (0x01) << 13;
	sdio->size = sdio->hostcmd.size + INTF_HEADER_LEN;

	sdio_drv_write(NULL, mlan_adap->ioport, 1, 1, buflen,
				(t_u8 *) outbuf, &resp);

	last_cmd_sent = sdio->hostcmd.command;
	os_mutex_put(&txrx_sem);

	return 0;
}
#endif

int wlan_send_sdio_cmd(t_u8 *buf, t_u32 buflen)
{
	SDIOPkt *sdio = (SDIOPkt *) outbuf;
	t_u32 resp;

	os_mutex_get(&txrx_sem, OS_WAIT_FOREVER);
	memcpy(outbuf, buf, buflen);
	sdio->pkttype = MLAN_TYPE_CMD;
	sdio->size = sdio->hostcmd.size + INTF_HEADER_LEN;

#ifdef CONFIG_WIFI_IO_DUMP
	wmprintf("OUT_CMD");
	dump_hex(outbuf, sdio->size);
#endif /* CONFIG_WIFI_IO_DUMP */
	sdio_drv_write(NULL, mlan_adap->ioport, 1, 1, buflen,
				(t_u8 *) outbuf, &resp);

	last_cmd_sent = sdio->hostcmd.command;
	os_mutex_put(&txrx_sem);

	return WM_SUCCESS;
}

int wifi_send_cmdbuffer(t_u32 len)
{
	return wlan_send_sdio_cmd(cmd_buf, len);
}

static void calculate_sdio_write_params(t_u32 txlen,
					t_u32 *tx_blocks, t_u32 *buflen)
{
	*tx_blocks = 1;
	*buflen = MLAN_SDIO_BLOCK_SIZE;

	if (txlen > 512) {
		*tx_blocks =
		    (txlen + MLAN_SDIO_BLOCK_SIZE_FW_DNLD -
		     1) / MLAN_SDIO_BLOCK_SIZE_FW_DNLD;
		/* this is really blksize */
		*buflen = MLAN_SDIO_BLOCK_SIZE_FW_DNLD;
	} else {
		*tx_blocks =
		    (txlen + MLAN_SDIO_BLOCK_SIZE_FW_DNLD -
		     1) / MLAN_SDIO_BLOCK_SIZE_FW_DNLD;
		*buflen = *tx_blocks * MLAN_SDIO_BLOCK_SIZE_FW_DNLD;

		*tx_blocks = 1;	/* tx_blocks of size 512 */
	}

}

t_u16 get_mp_end_port(void);
mlan_status wlan_xmit_pkt(t_u32 txlen, t_u8 interface)
{
	t_u32 tx_blocks = 0, buflen = 0;
	t_u32 ret, resp;

	calculate_sdio_write_params(txlen, &tx_blocks, &buflen);
#ifdef CONFIG_WIFI_IO_DEBUG
	wmprintf("%s: txportno = %d mlan_adap->mp_wr_bitmap: %p\n\r",
			 __func__, txportno, mlan_adap->mp_wr_bitmap);
#endif /* CONFIG_WIFI_IO_DEBUG */
	/* Check if the port is available */
	if (!((1 << txportno) & mlan_adap->mp_wr_bitmap)) {
		/*
		 * fixme: This condition is triggered in legacy as well as
		 * this new code. Check this out later.
		 */
#ifdef CONFIG_WIFI_IO_DEBUG
		wifi_io_e("txportno out of sync txportno "
			  "= (%ld) mp_wr_bitmap = (0x%x)",
			  txportno, mlan_adap->mp_wr_bitmap);
#endif /* CONFIG_WIFI_IO_DEBUG */
		return MLAN_STATUS_RESOURCE;
	} else {
		/* Mark the port number we will use */
		mlan_adap->mp_wr_bitmap &= ~(1 << txportno);
	}
	process_pkt_hdrs((t_u8 *) outbuf, txlen, interface);
	/* send CMD53 */
	ret = sdio_drv_write(NULL, mlan_adap->ioport + txportno, 1, tx_blocks,
					buflen, (t_u8 *) outbuf, &resp);
	txportno++;
	if (txportno == mlan_adap->mp_end_port)
		txportno = 1;

	if (ret == false) {
		wifi_io_e("sdio_drv_write failed (%d)", ret);
		return MLAN_STATUS_FAILURE;
	}
	return MLAN_STATUS_SUCCESS;
}

/*
 * This function gets interrupt status.
 */
t_void wlan_interrupt(mlan_adapter *pmadapter)
{
	/* Read SDIO multiple port group registers */
	t_u32 resp = 0;
	t_u8 *mp_regs = pmadapter->mp_regs;

	/* Read the registers in DMA aligned buffer */
	sdio_drv_read(NULL, 0, 1, 1, MAX_MP_REGS, mp_regs, &resp);

	t_u32 sdio_ireg = mp_regs[HOST_INT_STATUS_REG];

	if (sdio_ireg) {
		/*
		 * DN_LD_HOST_INT_STATUS and/or UP_LD_HOST_INT_STATUS
		 * Clear the interrupt status register
		 */
		pmadapter->sdio_ireg |= sdio_ireg;
	}

#ifdef CONFIG_WIFI_IO_DEBUG
	t_u32 rd_bitmap, wr_bitmap;

	rd_bitmap = ((t_u16) mp_regs[RD_BITMAP_U]) << 8;
	rd_bitmap |= (t_u16) mp_regs[RD_BITMAP_L];
	wmprintf("INT : rd_bitmap=0x%04lx\n\r", rd_bitmap);

	wr_bitmap = ((t_u16) mp_regs[WR_BITMAP_U]) << 8;
	wr_bitmap |= (t_u16) mp_regs[WR_BITMAP_L];

	wmprintf("INT : wr_bitmap=0x%04lx\n\r", wr_bitmap);
	wmprintf("INT : sdio_ireg = (0x%x)\r\n", sdio_ireg);
#endif /* CONFIG_WIFI_IO_DEBUG */
}

/* returns port number from rd_bitmap. if ctrl port, then it clears
 * the bit and does nothing else
 * if data port then increments curr_port value also */
mlan_status wlan_get_rd_port(mlan_adapter *pmadapter, t_u8 *pport)
{
	t_u16 rd_bitmap = pmadapter->mp_rd_bitmap;

	wifi_io_d("wlan_get_rd_port: mp_rd_bitmap=0x%x"
		  " curr_rd_bitmap=0x%x", pmadapter->mp_rd_bitmap,
		  pmadapter->curr_rd_port);

	if (!(rd_bitmap & (CTRL_PORT_MASK | DATA_PORT_MASK)))
		return MLAN_STATUS_FAILURE;

	if (pmadapter->mp_rd_bitmap & CTRL_PORT_MASK) {
		pmadapter->mp_rd_bitmap &= (t_u16) (~CTRL_PORT_MASK);
		*pport = CTRL_PORT;

		wifi_io_d("wlan_get_rd_port: port=%d mp_rd_bitmap=0x%04x",
			  *pport, pmadapter->mp_rd_bitmap);
	} else {
		/* Data */
		if (pmadapter->mp_rd_bitmap &
		    (1 << pmadapter->curr_rd_port)) {
			pmadapter->mp_rd_bitmap &=
				(t_u16) (~(1 << pmadapter->curr_rd_port));
			*pport = pmadapter->curr_rd_port;

			if (++pmadapter->curr_rd_port == MAX_PORT)
				pmadapter->curr_rd_port = 1;
		} else {
			wifi_io_e("wlan_get_rd_port : Returning FAILURE");
			return MLAN_STATUS_FAILURE;
		}

		wifi_io_d("port=%d mp_rd_bitmap=0x%04x -> 0x%04x\n", *pport,
			  rd_bitmap, pmadapter->mp_rd_bitmap);
	}

	return MLAN_STATUS_SUCCESS;
}

/*
 * Assumes that pmadapter->mp_rd_bitmap contains latest values
 */
static mlan_status _handle_sdio_packet_read(mlan_adapter *pmadapter,
				     void **packet, t_u16 *datalen,
				     t_u32 *pkt_type)
{
	t_u8 port;
	mlan_status ret = wlan_get_rd_port(pmadapter, &port);

	/* nothing to read */
	if (ret != MLAN_STATUS_SUCCESS)
		return ret;

	t_u32 len_reg_l = RD_LEN_P0_L + (port << 1);
	t_u32 len_reg_u = RD_LEN_P0_U + (port << 1);

	t_u16 rx_len = ((t_u16) pmadapter->mp_regs[len_reg_u]) << 8;
	*datalen = rx_len |= (t_u16) pmadapter->mp_regs[len_reg_l];

	*packet = wlan_read_rcv_packet(port, rx_len, pkt_type);

	return MLAN_STATUS_SUCCESS;
}

/*
 * This function keeps on looping till all the packets are read
 */
static void handle_sdio_packet_read(mlan_adapter *pmadapter)
{
	mlan_status ret = MLAN_STATUS_SUCCESS;
	t_u16 datalen = 0;

	pmadapter->mp_rd_bitmap =
		((t_u16) pmadapter->mp_regs[RD_BITMAP_U]) << 8;
	pmadapter->mp_rd_bitmap |=
		(t_u16) pmadapter->mp_regs[RD_BITMAP_L];

	while (1) {
		t_u32 pkt_type;
		void *packet = NULL;
		ret = _handle_sdio_packet_read(pmadapter, &packet,
					       &datalen, &pkt_type);
		if (ret != MLAN_STATUS_SUCCESS) {
			/* nothing to read. break out of while loop */
			break;
		}

		if (pkt_type == MLAN_TYPE_DATA) {
			handle_data_packet(packet, datalen);
		} else {
			/* non-data packets such as events
			   and command responses are
			   handled here */
			wlan_decode_rx_packet(packet, pkt_type);
		}
	}
}

/*
 * This is supposed to be called in thread context.
 */
mlan_status wlan_process_int_status(mlan_adapter *pmadapter)
{
	mlan_status ret = MLAN_STATUS_SUCCESS;

	/* Get the interrupt status */
	wlan_interrupt(pmadapter);

	t_u8 sdio_ireg = pmadapter->sdio_ireg;
	pmadapter->sdio_ireg = 0;

	if (!sdio_ireg)
		goto done;


	/*
	 * Below two statement look like they are present for the purpose
	 * of unconditional initializing of mp_wr_bitmap which will be used
	 * during packet xmit. proper mlan code does not do this most
	 * probably because they have used wlan_get_wr_port_data() to
	 * decide on the write port which we have not done. Check this out
	 * later.
	 */
	pmadapter->mp_wr_bitmap =
		((t_u16) pmadapter->mp_regs[WR_BITMAP_U]) << 8;
	pmadapter->mp_wr_bitmap |=
		(t_u16) pmadapter->mp_regs[WR_BITMAP_L];

	/*
	 * DN_LD_HOST_INT_STATUS interrupt happens when the txmit sdio
	 * ports are freed This is usually when we write to port most
	 * significant port.
	 */
	if (sdio_ireg & DN_LD_HOST_INT_STATUS) {
		pmadapter->mp_wr_bitmap =
			((t_u16) pmadapter->mp_regs[WR_BITMAP_U]) << 8;
		pmadapter->mp_wr_bitmap |=
			(t_u16) pmadapter->mp_regs[WR_BITMAP_L];

	}

	/*
	 * As firmware will not generate download ready interrupt if the
	 * port updated is command port only, cmd_sent should be done for
	 * any SDIO interrupt.
	*/

	if (pmadapter->cmd_sent == true) {
		/*
		 * Check if firmware has attach buffer at command port and
		 * update just that in wr_bit_map.
		 */
		pmadapter->mp_wr_bitmap |=
			(t_u16) pmadapter->mp_regs[WR_BITMAP_L] &
			CTRL_PORT_MASK;

		if (pmadapter->mp_wr_bitmap & CTRL_PORT_MASK)
			pmadapter->cmd_sent = false;
	}

	if (sdio_ireg & UP_LD_HOST_INT_STATUS) {
		/* This means there is data to be read */
		handle_sdio_packet_read(pmadapter);
	}

	ret = MLAN_STATUS_SUCCESS;

done:
	return ret;
}

/**
 * Interrupt callback handler registered with the SDIO driver.
 */
static void handle_cdint(int error)
{
	long xHigherPriorityTaskWoken = pdFALSE;

	/* Wake up LWIP thread. */
	if (datasem && !error && g_txrx_flag) {
		g_txrx_flag = 0;
		xSemaphoreGiveFromISR(datasem, &xHigherPriorityTaskWoken);
		portEND_SWITCHING_ISR(xHigherPriorityTaskWoken);
	}
}

int wifi_raw_packet_recv(t_u8 **data, t_u32 *pkt_type)
{
	if (!data)
		return -WM_FAIL;

	if (datasem == NULL) {
		int rv = os_semaphore_create(&datasem, "data-mutex");
		if (rv != WM_SUCCESS) {
			wifi_io_e("Unable to create semaphore");
			return -WM_FAIL;
		}

		/* Initial get */
		os_semaphore_get(&datasem, 0);
	}

	int sta = os_enter_critical_section();
	/* Allow interrupt handler to deliver us a packet */
	g_txrx_flag = 1;
	SDIOC_IntMask(SDIOC_INT_CDINT, UNMASK);
	SDIOC_IntSigMask(SDIOC_INT_CDINT, UNMASK);
	os_exit_critical_section(sta);

	/* Wait till we  a packet from SDIO */
	os_semaphore_get(&datasem, OS_WAIT_FOREVER);

	/* Get the interrupt status */
	wlan_interrupt(mlan_adap);

	t_u8 sdio_ireg = mlan_adap->sdio_ireg;
	mlan_adap->sdio_ireg = 0;

	if (!(sdio_ireg & UP_LD_HOST_INT_STATUS))
		return -WM_FAIL;

	mlan_status ret = MLAN_STATUS_SUCCESS;
	t_u16 datalen = 0;

	mlan_adap->mp_rd_bitmap =
		((t_u16) mlan_adap->mp_regs[RD_BITMAP_U]) << 8;
	mlan_adap->mp_rd_bitmap |=
			(t_u16) mlan_adap->mp_regs[RD_BITMAP_L];

	void *packet;
	while (1) {
		ret = _handle_sdio_packet_read(mlan_adap, &packet,
					       &datalen, pkt_type);
		if (ret == MLAN_STATUS_SUCCESS) {
			break;
		}
	}

	*data = packet;
	return WM_SUCCESS;
}

int wifi_raw_packet_send(const t_u8 *packet, t_u32 length)
{
	if (!packet || !length)
		return -WM_E_INVAL;

	if (length > SDIO_INBUF_LEN) {
		wifi_io_e("Insufficient buffer");
		return -WM_FAIL;
	}

	t_u32 tx_blocks = 0, buflen = 0;
	calculate_sdio_write_params(length, &tx_blocks, &buflen);

	memcpy(outbuf, packet, length);

	uint32_t resp;
	sdio_drv_write(NULL, mlan_adap->ioport, 1, tx_blocks, buflen,
		       outbuf, &resp);
	return WM_SUCCESS;
}

static int sd878x_reinit()
{
	int ret = sdio_drv_reinit(&handle_cdint);
	if (ret != WM_SUCCESS)
		wifi_io_e("SDIO reinit failed");

	return ret;
}

static void sd878x_ps_cb(power_save_event_t event, void *data)
{

	switch (event) {
	case ACTION_EXIT_PM3:
		sd878x_reinit();
		break;
	case ACTION_ENTER_PM4:
		sd878x_enter_pm4();
		break;
	default:
		break;
	}
}

mlan_status sd878x_init(enum wlan_type type, flash_desc_t *fl)
{
	mdev_t *sdio_drv;
	uint32_t resp;
	mlan_status mlanstatus = MLAN_STATUS_SUCCESS;

	/* This semaphore is used for synchronization
	 * between Data path and CMD path when  IEEE
	 * Power save is enabled
	 */
	int rv = os_semaphore_create(&g_wakeup_sem,
				     "sdio_wakeupn-synch-sem");
	if (rv != WM_SUCCESS)
		return rv;

	/* Initialise internal/external flash memory */
	flash_drv_init();

	/* initializes the driver struct */
	int sdiostatus = wlan_init_struct();
	if (sdiostatus != WM_SUCCESS) {
		wifi_io_e("Init failed. Cannot create init struct");
		return MLAN_STATUS_FAILURE;
	}

	/*
	 * Register a callback with power manager of MC200
	 * This callback will be called on entry /exit
	 * of low power mode of MC200 based on first paramter
	 * passed to the call.
	 */
	pm_register_cb(ACTION_EXIT_PM3 | ACTION_ENTER_PM4,
		       sd878x_ps_cb, NULL);

	if ((backup_s.wifi_state == WIFI_INACTIVE) ||
	     (backup_s.wifi_state == WIFI_DEEP_SLEEP)) {
		wifi_io_d("Waking from PM4, calling sdio_drv_reinit()");
		wifi_io_d("txportno %x", txportno);
		sdio_drv_reinit(&handle_cdint);
		sdio_drv = sdio_drv_open("MDEV_SDIO");
		if (!sdio_drv) {
			wifi_io_e("SDIO driver open failed during re-init");
			return MLAN_STATUS_FAILURE;
		}

		mlan_subsys_init();
		sd878x_exit_pm4();
		return WM_SUCCESS;
	}

	txportno = 0;
	/* Initialize SDIO driver */
	rv = sdio_drv_init(&handle_cdint);
	if (rv  != WM_SUCCESS) {
		wifi_io_e("SDIO driver init failed.");
		return MLAN_STATUS_FAILURE;
	}

	sdio_drv = sdio_drv_open("MDEV_SDIO");
	if (!sdio_drv) {
		wifi_io_e("SDIO driver open failed.");
		return MLAN_STATUS_FAILURE;
	}

	/* Initialize the mlan subsystem before initializing 878x driver */
	mlan_subsys_init();

	/* this sets intmask on card and makes interrupts repeatable */
	sdiostatus = wlan_sdio_init_ioport(sdio_drv);

	if (sdiostatus != true) {
		wifi_io_e("SDIO - Failed to read IOPORT");
		return MLAN_STATUS_FAILURE;
	}

	mlanstatus = wlan_download_fw(sdio_drv, fl);

	if (mlanstatus != MLAN_STATUS_SUCCESS)
		return mlanstatus;

	sdio_drv_creg_write(sdio_drv, HOST_INT_MASK_REG, 1, 0x3, &resp);

	/* If we're running a Manufacturing image, start the tasks.
	   If not, initialize and setup the firmware */
	switch (type) {
	case WLAN_TYPE_NORMAL:
		wlan_fw_init_cfg();
		break;
	case WLAN_TYPE_WIFI_CALIB:
		g_txrx_flag = 1;
		break;
	case WLAN_TYPE_FCC_CERTIFICATION:
		break;
	default:
		wifi_io_e("Enter a valid input to sd878x_init");
		return MLAN_STATUS_FAILURE;
	}

	txportno = 1;

	return mlanstatus;
}

void sd878x_deinit(void)
{
	cal_data_valid = false;
	mac_addr_valid = false;
	wlan_cmd_shutdown();
	sdio_drv_deinit();
}

HostCmd_DS_COMMAND *wifi_get_command_buffer()
{
	/* First 4 bytes reserved for SDIO pkt header */
	return (HostCmd_DS_COMMAND *)(cmd_buf + INTF_HEADER_LEN);
}
