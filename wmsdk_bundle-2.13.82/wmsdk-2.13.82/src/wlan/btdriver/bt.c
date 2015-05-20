/*
 *  Copyright 2008-2013, Marvell International Ltd.
 *  All Rights Reserved.
 */

#include <mc200.h>
#include <mc200_sdio.h>
#include <mdev.h>
#include <mdev_sdio.h>
#include <wmstdio.h>
#include <wm_os.h>
#include <wm_utils.h>
#include <wmlog.h>

#include "bt_sdio.h"

#define bt_log(...)  wmlog("bt_drv", ##__VA_ARGS__)

/* Uncomment this to print
 * all incoming and outgoing
 * packets to console.
 *
#define PKT_DEBUG */

/** Sub Command: Module Bring Up Request */
#define MODULE_BRINGUP_REQ		0xF1
/** Sub Command: Module Shut Down Request */
#define MODULE_SHUTDOWN_REQ		0xF2
/** Vendor OGF */
#define VENDOR_OGF			0x3F
/** OGF for reset */
#define RESET_OGF			0x03
/** Marvell vendor packet */
#define MRVL_VENDOR_PKT			0xFE
/** Bluetooth command : Reset */
#define BT_CMD_RESET			0x03
/** Bluetooth command : Get FW Version */
#define BT_CMD_GET_FW_VERSION		0x0F
/** Bluetooth command : Sleep mode */
#define BT_CMD_AUTO_SLEEP_MODE		0x23
/** Bluetooth command : Host Sleep configuration */
#define BT_CMD_HOST_SLEEP_CONFIG	0x59
/** Bluetooth command : Host Sleep enable */
#define BT_CMD_HOST_SLEEP_ENABLE	0x5A
/** Bluetooth command : Module Configuration request */
#define BT_CMD_MODULE_CFG_REQ		0x5B
/** Bluetooth command : SDIO pull up down configuration request */
#define BT_CMD_SDIO_PULL_CFG_REQ	0x69

/* HCI data types */
#define HCI_COMMAND_PKT		0x01
#define HCI_ACLDATA_PKT		0x02
#define HCI_SCODATA_PKT		0x03
#define HCI_EVENT_PKT		0x04
#define HCI_VENDOR_PKT		0xff

struct BT_CMD {
	/** fw hdr */
	uint8_t hdr[4];
	/** OCF OGF */
	uint16_t ocf_ogf;
	/** Length */
	uint8_t length;
	/** Data */
	uint8_t data[32];
} __attribute__ ((packed));

struct BT_EVENT {
	/** Event Counter */
	uint8_t EC;
	/** Length */
	uint8_t length;
	/** Data */
	uint8_t data[8];
} __attribute__ ((packed));


static mdev_t mdev_bt;
static const char mdev_bt_name[] = {"MDEV_BT"};
os_thread_t bt_bh_thread;
static os_thread_stack_define(bt_bh_stack, 1024);
static os_semaphore_t bt_bh_sem;
static mdev_t *sdio_dev;
static uint8_t inbuf[SD_BLOCK_SIZE] \
		__attribute__ ((section(".iobufs"), aligned(4)));
static uint8_t outbuf[SD_BLOCK_SIZE] \
		__attribute__ ((section(".iobufs"), aligned(4)));
static int32_t bt_func = 2;
static int32_t bt_ioport;
static int32_t bt_unit;

static void (*recv_cb)(uint8_t pkt_type, uint8_t *data, uint32_t size);

static void sd_card_to_host()
{
	uint32_t resp;
	uint32_t bt_length;
	uint8_t type;

	sdio_drv_creg_read(sdio_dev, CARD_RX_LEN_REG, bt_func, &resp);
	bt_length = (resp & 0xff) << bt_unit;

	sdio_drv_read(sdio_dev, bt_ioport, bt_func, 1, SD_BLOCK_SIZE, inbuf,
									&resp);

#ifdef PKT_DEBUG
	bt_log("Packet Recv:");
	dump_hex((uint8_t *) inbuf, bt_length);
#endif

	bt_length = inbuf[0];
	bt_length |= (uint16_t) inbuf[1] << 8;
	type = inbuf[3];

	switch (type) {
	case HCI_ACLDATA_PKT:
	case HCI_SCODATA_PKT:
	case HCI_EVENT_PKT:
	case HCI_VENDOR_PKT:
		if (recv_cb)
			recv_cb(type, &inbuf[4], bt_length - 4);
		break;
	case MRVL_VENDOR_PKT:
		break;
	default:
		bt_log("Unknown PKT type:%d\n", type);
		dump_hex((uint8_t *) inbuf, bt_length);
		break;
	}
}

static void bt_interrupt(os_thread_arg_t data)
{
	uint32_t resp = 0;
	uint8_t *mp_regs = inbuf;
	uint32_t sdio_ireg;

	while (1) {
		SDIOC_IntMask(SDIOC_INT_CDINT, UNMASK);
		SDIOC_IntSigMask(SDIOC_INT_CDINT, UNMASK);
		os_semaphore_get(&bt_bh_sem, OS_WAIT_FOREVER);

		/* Read the registers in DMA aligned buffer */
		sdio_drv_read(NULL, 0, bt_func, 1, 64, mp_regs,	&resp);

		sdio_ireg = mp_regs[HOST_INTSTATUS_REG];

		if (sdio_ireg & UP_LD_HOST_INT_STATUS)
			sd_card_to_host();
	}
}

static void handle_cdint(int error)
{
	long xHigherPriorityTaskWoken = pdFALSE;

	/* Wake up bottom half */
	xSemaphoreGiveFromISR(bt_bh_sem, &xHigherPriorityTaskWoken);
	portEND_SWITCHING_ISR(xHigherPriorityTaskWoken);
}

static int bt_card_fw_status(uint16_t *dat)
{
	uint32_t resp;

	sdio_drv_creg_read(sdio_dev, CARD_FW_STATUS0_REG, bt_func, &resp);
	*dat = resp & 0xff;
	sdio_drv_creg_read(sdio_dev, CARD_FW_STATUS1_REG, bt_func, &resp);
	*dat |= (resp & 0xff) << 8;

	return true;
}

static bool bt_card_ready_wait(uint32_t poll)
{
	uint16_t dat;
	int32_t i;

	for (i = 0; i < poll; i++) {
		bt_card_fw_status(&dat);
		if (dat == FIRMWARE_READY) {
			bt_log("Firmware Ready");
			return true;
		}
		os_thread_sleep(os_msec_to_ticks(1));
	}
	return false;
}

static bool bt_enable_func(uint32_t poll)
{
	uint32_t resp;
	uint32_t data;
	int32_t i;

	/* I/O Enable */
	sdio_drv_creg_read(sdio_dev, 0x2, 0, &resp);
	data = resp | (1 << bt_func);
	sdio_drv_creg_write(sdio_dev, 0x2, 0, data, &resp);

	/* Wait for I/O Ready */
	for (i = 0; i < poll; i++) {
		sdio_drv_creg_read(sdio_dev, 0x3, 0, &resp);
		if (resp & (1 << bt_func)) {

			/* Interrupt enable */
			sdio_drv_creg_read(sdio_dev, 0x4, 0, &resp);
			data = resp | (1 << bt_func) | 1;
			sdio_drv_creg_write(sdio_dev, 0x4, 0, data, &resp);
			return true;
		}
		os_thread_sleep(os_msec_to_ticks(1));
	}

	return false;
}

static void bt_send_module_cfg_cmd(int subcmd)
{
	struct BT_CMD *pCmd = (struct BT_CMD *)outbuf;
	uint32_t size = sizeof(struct BT_CMD);
	uint32_t resp;

	memset(outbuf, 0, sizeof(struct BT_CMD));

	pCmd->ocf_ogf = (VENDOR_OGF << 10) | BT_CMD_MODULE_CFG_REQ;
	pCmd->length = 1;
	pCmd->data[0] = subcmd;
	pCmd->hdr[0] = (size & 0x0000ff);
	pCmd->hdr[1] = (size & 0x00ff00) >> 8;
	pCmd->hdr[2] = (size & 0xff0000) >> 16;
	pCmd->hdr[3] = MRVL_VENDOR_PKT;
	if (sdio_drv_write(sdio_dev, bt_ioport, bt_func, 1, SD_BLOCK_SIZE,
						outbuf, &resp) == false)
		bt_log("%s failed", __func__);
#ifdef PKT_DEBUG
	else {
		bt_log("Cmd Sent: CFG");
		dump_hex((const void *)outbuf, size);
	}
#endif
}

static void bt_fw_init(void)
{
	uint32_t resp;
	uint32_t data;

	sdio_drv_creg_read(sdio_dev, CARD_REVISION_REG, bt_func, &resp);
	bt_log("revision=%#x", resp & 0xff);

	/* Read this once to avoid interrupt later on */
	sdio_drv_creg_read(sdio_dev, HOST_INTSTATUS_REG, bt_func, &resp);

	/* Read the PORT regs for IOPORT address */
	sdio_drv_creg_read(sdio_dev, IO_PORT_0_REG, bt_func, &resp);
	bt_ioport = (resp & 0xff);

	sdio_drv_creg_read(sdio_dev, IO_PORT_1_REG, bt_func, &resp);
	bt_ioport |= ((resp & 0xff) << 8);

	sdio_drv_creg_read(sdio_dev, IO_PORT_2_REG, bt_func, &resp);
	bt_ioport |= ((resp & 0xff) << 16);

	bt_log("IOPORT : (0x%lx)", bt_ioport);

#define SDIO_INT_MASK       0x3F
	/* Set Host interrupt reset to read to clear */
	sdio_drv_creg_read(sdio_dev, HOST_INT_RSR_REG, bt_func, &resp);
	data = (resp & 0xff) | SDIO_INT_MASK;
	sdio_drv_creg_write(sdio_dev, HOST_INT_RSR_REG, bt_func, data, &resp);

	/* Dnld/Upld ready set to auto reset */
	sdio_drv_creg_read(sdio_dev, CARD_MISC_CFG_REG, bt_func, &resp);
	data = (resp & 0xff) | AUTO_RE_ENABLE_INT;
	sdio_drv_creg_write(sdio_dev, CARD_MISC_CFG_REG, bt_func, data, &resp);

	sdio_drv_creg_write(sdio_dev, HOST_INT_MASK_REG, bt_func, HIM_ENABLE,
									&resp);

	sdio_drv_creg_read(sdio_dev, CARD_RX_UNIT_REG, bt_func, &resp);
	bt_unit = (resp & 0xff);

	bt_send_module_cfg_cmd(MODULE_BRINGUP_REQ);
}

int32_t bt_drv_send(uint8_t pkt_type, uint8_t *data, uint32_t size)
{
	uint32_t resp;

	memcpy(outbuf + 4, data, size);
	size += 4;

	outbuf[0] = (size & 0x0000ff);
	outbuf[1] = (size & 0x00ff00) >> 8;
	outbuf[2] = (size & 0xff0000) >> 16;
	outbuf[3] = pkt_type;
	if (sdio_drv_write(sdio_dev, bt_ioport, bt_func, 1, SD_BLOCK_SIZE,
						outbuf, &resp) == false) {
		bt_log("%s failed", __func__);
		return -WM_FAIL;
	}
#ifdef PKT_DEBUG
	else {
		bt_log("Cmd Sent:");
		dump_hex((const void *)outbuf, size);
	}
#endif
	return WM_SUCCESS;
}

int32_t bt_drv_set_cb(void (*cb_func)(uint8_t pkt_type, uint8_t *data,
							uint32_t size))
{
	recv_cb = cb_func;
	return WM_SUCCESS;
}

int32_t bt_drv_init(void)
{
	if (mdev_get_handle(mdev_bt_name) != NULL)
		return WM_SUCCESS;

	sdio_dev = sdio_drv_open("MDEV_SDIO");
	if (!sdio_dev) {
		bt_log("SDIO driver open failed.");
		return -WM_FAIL;
	}

	if (!bt_enable_func(300)) {
		bt_log("Func %d could not be enabled", bt_func);
		return -WM_FAIL;
	}

	if (os_semaphore_create(&bt_bh_sem, "bt-bh-sem") != WM_SUCCESS) {
		bt_log("Unable to create semaphore");
		return -WM_FAIL;
	}
	os_semaphore_get(&bt_bh_sem, OS_NO_WAIT);

	if (os_thread_create(&bt_bh_thread, "bt_bh", bt_interrupt, 0,
				&bt_bh_stack, OS_PRIO_2) != WM_SUCCESS) {
		bt_log("Failed to create bt_bh_thread");
		return -WM_FAIL;
	}

	sdio_drv_set_cb1(&handle_cdint);

	bt_fw_init();

	if (!bt_card_ready_wait(300)) {
		bt_log("Firmware not detected");
		return -WM_FAIL;
	}

	/* Register the device driver */
	mdev_register(&mdev_bt);

	bt_log("BT driver initialized");

	return WM_SUCCESS;
}
