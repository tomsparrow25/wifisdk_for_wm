/*
 *  Copyright 2008-2013, Marvell International Ltd.
 *  All Rights Reserved.
 */

/*
 * mdev_acomp.c: mdev driver for ACOMP
 */
#include <mc200.h>
#include <mc200_gpio.h>
#include <mc200_pinmux.h>
#include <mdev_acomp.h>
#include <wm_os.h>

#define MC200_ACOMP_NUM 2

/* acomp mdev and synchronization handles */
static mdev_t mdev_acomp[MC200_ACOMP_NUM];
static os_mutex_t acomp_mutex[MC200_ACOMP_NUM];

/* acomp device specific names */
static const char *mdev_acomp_name[MC200_ACOMP_NUM] = {
	"MDEV_ACOMP0",
	"MDEV_ACOMP1",
};

mdev_t *acomp_drv_open(ACOMP_ID_Type acomp_id, ACOMP_PosChannel_Type pos,
					 ACOMP_NegChannel_Type neg)
{
	int ret;
	mdev_t *mdev_p = mdev_get_handle(mdev_acomp_name[acomp_id]);

	if (mdev_p == NULL) {
		ACOMP_LOG("driver open called before register (%s)\r\n",
			  mdev_acomp_name[acomp_id]);
		return NULL;
	}

	/* Make sure acomp<n> is available for use */
	ret = os_mutex_get(&acomp_mutex[mdev_p->port_id], OS_WAIT_FOREVER);
	if (ret == -WM_FAIL) {
		ACOMP_LOG("failed to get mutex\r\n");
		return NULL;
	}

	/* Configure pinmux for external pins */
	if (pos <= ACOMP_POS_CH_GPIO0)
		GPIO_PinMuxFun((ACOMP_POS_CH_GPIO0 - pos), PINMUX_FUNCTION_6);
	if (neg <= ACOMP_NEG_CH_GPIO0)
		GPIO_PinMuxFun((ACOMP_NEG_CH_GPIO0 - neg), PINMUX_FUNCTION_6);

	ACOMP_ChannelConfig(mdev_p->port_id, pos, neg);
	ACOMP_Enable(mdev_p->port_id);

	return mdev_p;
}

int acomp_drv_result(mdev_t *mdev_p)
{
	return ACOMP_GetResult(mdev_p->port_id);
}

int acomp_drv_close(mdev_t *dev)
{
	/* Mark acomp<n> as free for use */
	return os_mutex_put(&acomp_mutex[dev->port_id]);
}

static void acomp_drv_mdev_init(uint8_t acomp_num)
{
	mdev_t *mdev_p;

	mdev_p = &mdev_acomp[acomp_num];

	mdev_p->name = mdev_acomp_name[acomp_num];

	mdev_p->port_id = acomp_num;
}

int acomp_drv_init(ACOMP_ID_Type id)
{
	int ret;

	if (mdev_get_handle(mdev_acomp_name[id]) != NULL)
		return WM_SUCCESS;

	ACOMP_CFG_Type acompConfigSet = {ACOMP_HYSTER_0MV, ACOMP_HYSTER_0MV,
				LOGIC_LO, ACOMP_PWMODE_1,
				ACOMP_WARMTIME_16CLK, ACOMP_PIN_OUT_SYN};

	ret = os_mutex_create(&acomp_mutex[id], mdev_acomp_name[id],
				      OS_MUTEX_INHERIT);
	if (ret == -WM_FAIL) {
		ACOMP_LOG("Failed to create mutex\r\n");
		return -WM_FAIL;
	}

	/* Initialize the acomp mdev structure */
	acomp_drv_mdev_init(id);

	/* Initialize the ACOMP */
	ACOMP_Init(id, &acompConfigSet);

	/* Register the acomp device driver for this port */
	mdev_register(&mdev_acomp[id]);

	return WM_SUCCESS;
}
