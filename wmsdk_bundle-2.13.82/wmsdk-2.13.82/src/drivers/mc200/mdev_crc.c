/*
 *  Copyright 2008-2013, Marvell International Ltd.
 *  All Rights Reserved.
 */

/*
 * mdev_crc.c: mdev driver for CRC
 */
#include <wm_os.h>
#include <mdev_crc.h>
#include <wmlog.h>

static os_mutex_t crc_mutex;

#ifdef CONFIG_CRC_DEBUG
#define CRCD(...)				\
	do {					\
		wmprintf("CRC: ");	\
		wmprintf(__VA_ARGS__);	\
		wmprintf("\n\r");		\
	} while (0);
#else
#define CRCD(...)
#endif /* CONFIG_CRC_DEBUG */

#define CRCD_E(...)				\
	wmlog_e("CRC", ##__VA_ARGS__)

#define CRCD_W(...)					\
	wmlog_w("CRC", ##__VA_ARGS__)

#define CRC_LOCK \
	os_mutex_get(&crc_mutex, OS_WAIT_FOREVER)

#define CRC_UNLOCK \
	os_mutex_put(&crc_mutex)

#define NUM_MC200_CRC_ENGINES 1

/* The device objects */
static mdev_t mdev_crc[NUM_MC200_CRC_ENGINES];
/* Device name */
static const char *mdev_crc_name[NUM_MC200_CRC_ENGINES] = {
	MDEV_CRC_ENGINE_0
};

typedef enum {
	CRC_SESSION_INIT_DONE,
	CRC_SESSION_OPEN_DONE,
	CRC_SESSION_STREAM_LEN_SET,
	CRC_SESSION_FIRST_FEED_DONE,
} crc_session_t;

mdev_t *crc_drv_open(const char *name, crc_mode_t mode)
{
	mdev_t *dev = mdev_get_handle(name);

	if (dev == NULL) {
		CRCD_E("driver open called without registering device"
		       " (%s)\n\r", name);
		return NULL;
	}

	if (dev->port_id != 0) {
		CRCD_E("Support for more that one engine not available");
		return NULL;
	}

	crc_session_t state = (crc_session_t)dev->private_data;
	if (state != CRC_SESSION_INIT_DONE) {
		CRCD_E("CRC session error in open: %d", state);
		return NULL;
	}

	CRC_Mode_Type mode_hw;
	switch (mode) {
	case CRC16_CCITT:
		mode_hw = CRC_16_CCITT;
		break;
	case CRC16_IBM:
		mode_hw = CRC_16_IBM;
		break;
	case CRC16_T10_DIF:
		mode_hw = CRC_16_T10_DIF;
		break;
	case CRC32_IEEE:
		mode_hw = CRC_32_IEEE;
		break;
	default:
		CRCD_E("CRC mode invalid");
		return NULL;
	}

	/* Lock until the user calls a close on this device. We need this
	   because CRC hardware does not support parallel access */
	CRC_LOCK;
	CRC_Enable();

	CRC_SetMode(mode_hw);
	state = CRC_SESSION_OPEN_DONE;
	dev->private_data = (uint32_t)state;
	return dev;
}

int crc_drv_block_calc(mdev_t *dev, const uint8_t *buf, uint32_t len,
		       uint32_t *result)
{
	if (!dev || !buf || !len || !result) {
		CRCD_E("Invalid parameters");
		return -WM_FAIL;
	}

	crc_session_t state = (crc_session_t)dev->private_data;
	if (state != CRC_SESSION_OPEN_DONE) {
		CRCD_E("CRC session error in block calc: %d", state);
		return -WM_FAIL;
	}

	*result = CRC_Calculate(buf, len);
	return WM_SUCCESS;
}

int crc_drv_stream_setlen(mdev_t *dev, uint32_t len)
{
	if (!dev || !len) {
		CRCD_E("Invalid parameters");
		return -WM_FAIL;
	}

	crc_session_t state = (crc_session_t)dev->private_data;
	if (state != CRC_SESSION_OPEN_DONE) {
		CRCD_E("CRC session error in set len: %d", state);
		return -WM_FAIL;
	}

	CRC_SetStreamLen(len);
	state = CRC_SESSION_STREAM_LEN_SET;
	dev->private_data = (uint32_t)state;
	return WM_SUCCESS;
}

int crc_drv_stream_feed(mdev_t *dev, const uint8_t *buf, uint32_t len)
{
	if (!dev || !buf || !len) {
		CRCD_E("Invalid parameters");
		return -WM_FAIL;
	}

	crc_session_t state = (crc_session_t)dev->private_data;
	switch (state) {
	case CRC_SESSION_STREAM_LEN_SET:
		CRC_Stream_Feed(buf, len);
		state = CRC_SESSION_FIRST_FEED_DONE;
		dev->private_data = (uint32_t)state;
		break;
	case CRC_SESSION_FIRST_FEED_DONE:
		if (len % 4) {
			CRCD_E("Misaligned sizes allowed in first call only %s",
			       __func__);
			return -WM_FAIL;
		}
		CRC_Stream_Feed(buf, len);
		break;
	default:
		CRCD_E("CRC session error in stream feed: %d", state);
		return -WM_FAIL;
	}

	return WM_SUCCESS;
}

int crc_drv_stream_get_result(mdev_t *dev, uint32_t *result)
{
	if (!dev || !result) {
		CRCD_E("Invalid parameters");
		return -WM_FAIL;
	}

	crc_session_t state = (crc_session_t)dev->private_data;
	if (state != CRC_SESSION_FIRST_FEED_DONE) {
		CRCD_E("CRC session error in get result: %d", state);
		return -WM_FAIL;
	}

	*result = CRC_GetResult();

	state = CRC_SESSION_OPEN_DONE;
	dev->private_data = (uint32_t)state;
	return WM_SUCCESS;
}

void crc_drv_close(mdev_t *dev)
{
	if (!dev) {
		CRCD_E("Invalid parameters");
		return;
	}

	CRC_Disable();

	crc_session_t state = (crc_session_t)dev->private_data;
	state = CRC_SESSION_INIT_DONE;
	dev->private_data = (uint32_t)state;
	CRC_UNLOCK;
}

static void crc_drv_mdev_init(unsigned engine_no)
{
	mdev_t *dev;
	crc_session_t state;

	dev = &mdev_crc[engine_no];

	dev->name = mdev_crc_name[engine_no];
	dev->port_id = engine_no;

	state = CRC_SESSION_INIT_DONE;
	dev->private_data = (uint32_t)state;
}

int crc_drv_init(void)
{
	int status, i;

	if (mdev_get_handle(mdev_crc_name[0]) != NULL)
		return WM_SUCCESS;

	status = os_mutex_create(&crc_mutex, "crcm", OS_MUTEX_INHERIT);
	if (status != WM_SUCCESS) {
		CRCD_E("mutex init failed");
		return -WM_FAIL;
	}

	for (i = 0; i < NUM_MC200_CRC_ENGINES; i++) {
		/* Initialize the crc engine's mdev structure */
		crc_drv_mdev_init(i);

		/* Register the I2C device driver for this port */
		mdev_register(&mdev_crc[i]);
	}

	return WM_SUCCESS;
}
