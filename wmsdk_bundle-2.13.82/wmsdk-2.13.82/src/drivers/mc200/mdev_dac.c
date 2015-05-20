/*
 *  Copyright 2008-2013, Marvell International Ltd.
 *  All Rights Reserved.
 */

/*
 * mdev_dac.c: mdev driver for DAC
 */
#include <mc200.h>
#include <mc200_gpio.h>
#include <mc200_pinmux.h>
#include <mdev_dac.h>
#include <wm_os.h>

/* dac mdev and synchronization handles */
static mdev_t mdev_dac;
static os_mutex_t dac_mutex;

/* dac device specific names */
static const char *mdev_dac_name = {MDEV_DAC};

static DAC_ChannelID_Type dacdev_data;

mdev_t *dac_drv_open(const char *name, DAC_ChannelID_Type channel)
{
	int ret;
	DAC_ChannelConfig_Type dacConfigSet;
	mdev_t *mdev_p = mdev_get_handle(name);

	if (mdev_p == NULL) {
		DAC_LOG("driver open called before register (%s)\r\n", name);
		return NULL;
	}

	/* Make sure dac<n> is available for use */
	ret = os_mutex_get(&dac_mutex, OS_WAIT_FOREVER);
	if (ret == -WM_FAIL) {
		DAC_LOG("failed to get mutex\r\n");
		return NULL;
	}

	/* Configure pinmux for external pins */
	switch (channel) {
	case DAC_CH_A:
		GPIO_PinMuxFun(GPIO_4, PINMUX_FUNCTION_6);
		break;

	case DAC_CH_B:
		GPIO_PinMuxFun(GPIO_11, PINMUX_FUNCTION_6);
		break;
	}

	dacConfigSet.waveMode = DAC_WAVE_NORMAL;
	dacConfigSet.outMode = DAC_OUTPUT_PAD;
	dacConfigSet.timingMode = DAC_NON_TIMING_CORRELATED;

	DAC_ChannelConfig(channel, &dacConfigSet);
	DAC_ChannelEnable(channel);

	mdev_p->private_data = channel;

	return mdev_p;
}

void dac_drv_output(mdev_t *mdev_p, int val)
{
	return DAC_SetChannelData(mdev_p->private_data, (val & 0x3ff));
}

int dac_drv_close(mdev_t *mdev_p)
{
	DAC_ChannelDisable(mdev_p->private_data);
	DAC_ChannelReset(mdev_p->private_data);

	/* Mark dac<n> as free for use */
	return os_mutex_put(&dac_mutex);
}

static void dac_drv_mdev_init()
{
	mdev_t *mdev_p;

	mdev_p = &mdev_dac;
	mdev_p->name = mdev_dac_name;
	mdev_p->private_data = (uint32_t) &dacdev_data;
}

int dac_drv_init()
{
	int ret;

	if (mdev_get_handle(mdev_dac_name) != NULL)
		return WM_SUCCESS;

	ret = os_mutex_create(&dac_mutex, mdev_dac_name,
				      OS_MUTEX_INHERIT);
	if (ret == -WM_FAIL) {
		DAC_LOG("Failed to create mutex\r\n");
		return -WM_FAIL;
	}

	/* Initialize the dac mdev structure */
	dac_drv_mdev_init();

	/* Register the dac device driver for this port */
	mdev_register(&mdev_dac);

	return WM_SUCCESS;
}
