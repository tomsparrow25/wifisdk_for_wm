/*
 *  Copyright 2008-2013, Marvell International Ltd.
 *  All Rights Reserved.
 */

/*
 * mdev_rtc.c: mdev driver for RTC
 */

#include <wmstdio.h>
#include <wm_os.h>
#include <mdev_rtc.h>
#include <mdev_gpt.h>
#include <mc200_clock.h>
#include <mc200_pinmux.h>
#include <mc200_gpio.h>
#include <mc200_pmu.h>
#include <board.h>
#include <arch/boot_flags.h>
#include <pwrmgr.h>

static mdev_t MDEV_rtc;
static const char *MDEV_NAME_rtc = "MDEV_RTC";

inline void rtc_xtal32k_ps_cb(power_save_event_t event, void *data)
{
	if (board_32k_xtal() || board_32k_osc()) {
		GPIO_PinMuxFun(GPIO_18, GPIO18_OSC32K_IN);
		GPIO_PinMuxFun(GPIO_19, GPIO19_OSC32K_OUT);
	}
}

inline void rtc_drv_set_cb(void *user_cb)
{
	NVIC_ClearPendingIRQ(RTC_IRQn);
	install_int_callback(INT_RTC, RTC_INT_CNT_UPP, user_cb);
	if (user_cb != NULL)
		RTC_IntMask(UNMASK);
	else
		RTC_IntMask(MASK);
}

inline void rtc_drv_set(mdev_t *dev, uint32_t cnt_upp)
{
	uint32_t val;
	struct bootrom_info *info = (struct bootrom_info *)&_nvram_start;
	if (info->version >= BOOTROM_A0_VERSION)
		val = cnt_upp;
	else
		/* Divider for Z1 does not work, and hence RTC is functioning
		 * at 32KHz, take that into consideration.
		 */
		val = (cnt_upp * 32768) / 1000;
	RTC_IntClr();
	RTC_SetCounterUppVal(val);
}

inline void rtc_drv_reset(mdev_t *dev)
{
	RTC_CounterReset();
}

inline unsigned int rtc_drv_get(mdev_t *dev)
{
	return RTC_GetCounterVal();
}

inline unsigned int rtc_drv_get_uppval(mdev_t *dev)
{
	return RTC_GetCounterUppVal();
}

inline void rtc_drv_stop(mdev_t *dev)
{
	RTC_Stop();
}

inline void rtc_drv_start(mdev_t *dev)
{
	RTC_Start();
}

inline int rtc_drv_close(mdev_t *dev)
{
	return 0;
}

mdev_t *rtc_drv_open(const char *name)
{
	mdev_t *mdev_p = mdev_get_handle(name);

	if (mdev_p == NULL) {
		RTC_LOG("driver open called without registering device"
							" (%s)\n\r", name);
		return NULL;
	}

	return mdev_p;
}

static void rtc_drv_rc32k_calib()
{
	mdev_t *gpt_dev;

	gpt_drv_init(GPT3_ID);
	gpt_dev = gpt_drv_open(GPT3_ID);

	/* Set GPT3 at ~ 32.768KHz and use it for calibration */
	PMU_PowerOnVDDIO(PMU_VDDIO_D0);
	GPIO_PinMuxFun(GPIO_18, GPIO18_GPT3_CH0);
	CLK_GPTInternalClkSrc(CLK_GPT_ID_3, CLK_MAINXTAL);
	CLK_ModuleClkDivider(CLK_GPT3, 0);

	gpt_drv_set(gpt_dev, 0x51EB851); /* 0xffffffff / 50 */
	gpt_drv_pwm(gpt_dev, GPT_CH_0, 488, 488);
	gpt_drv_start(gpt_dev);

	CLK_Xtal32KEnable(CLK_OSC_EXTERN);
	while (CLK_GetClkStatus(CLK_XTAL32K) == RESET)
		;

	CLK_RC32KEnable();
	while (CLK_GetClkStatus(CLK_RC32K) == RESET)
		;

	CLK_RC32KCalibration(CLK_AUTO_CAL, 0);

	CLK_Xtal32KDisable();
	gpt_drv_stop(gpt_dev);
	gpt_drv_close(gpt_dev);
}

int rtc_drv_init(void)
{
	if (mdev_get_handle(MDEV_NAME_rtc) != NULL)
		return WM_SUCCESS;

	MDEV_rtc.name = MDEV_NAME_rtc;

	if (board_32k_xtal() || board_32k_osc()) {
		/* Enable XTAL32K */
		GPIO_PinMuxFun(GPIO_18, GPIO18_OSC32K_IN);
		GPIO_PinMuxFun(GPIO_19, GPIO19_OSC32K_OUT);

		/* Register a callback with  power manager of MC200
		 * this callback will be called on exit of PM3 mode
		 * of MC200
		 */
		pm_register_cb(ACTION_EXIT_PM3, rtc_xtal32k_ps_cb, NULL);
		if (board_32k_xtal())
			CLK_Xtal32KEnable(CLK_OSC_INTERN);
		else
			CLK_Xtal32KEnable(CLK_OSC_EXTERN);
		while (CLK_GetClkStatus(CLK_XTAL32K) == RESET)
			;
		CLK_RTCClkSrc(CLK_XTAL32K);
	} else
	if (board_main_xtal() > 0) {
		/* Calibrate and use RC32K */
		CLK_RTCClkSrc(CLK_RC32K);
		rtc_drv_rc32k_calib();
	} else {
		/* Use uncalibrated RC32K */
		CLK_RTCClkSrc(CLK_RC32K);
		CLK_RC32KEnable();
	}
	CLK_ModuleClkEnable(CLK_RTC);

	/* Configure if not already running */
	if (!RTC_GetCntStatus()) {
		RTC_Config_Type rtc_cfg;
		struct bootrom_info *info =
			(struct bootrom_info *)&_nvram_start;

		rtc_cfg.CntValUpdateMode = RTC_CNT_VAL_UPDATE_AUTO;
		if (info->version >= BOOTROM_A0_VERSION)
			/* Set the clock to 1024 Hz */
			rtc_cfg.clockDivider = 5;
		else
			/* Keep default clock setting, 32KHz */
			rtc_cfg.clockDivider = 0;
		rtc_cfg.uppVal = 0xffffffff;
		while (!RTC->CNT_EN.BF.STS_RESETN)
			;
		RTC_Init(&rtc_cfg);
	}

	/* This needs to be higher than kernel priority
	 * so that RTC ISR is executed before the
	 * scheduler
	 */
	NVIC_SetPriority(RTC_IRQn, 0x8);
	NVIC_EnableIRQ(RTC_IRQn);

	mdev_register(&MDEV_rtc);

	return WM_SUCCESS;
}
