/*
 * Copyright 2008-2013, Marvell International Ltd.
 * All Rights Reserved.
 */

#include <stdlib.h>
#include <wmstdio.h>
#include <wm_os.h>
#include <cli.h>
#include <pwrmgr.h>
#include <mc200.h>
#include <arch/pm_mc200.h>
#include <mc200_clock.h>
#include <mc200_pmu.h>
#include <mc200_flash.h>
#include <mc200_xflash.h>
#include <board.h>
#include <wmtime.h>
#include <wlan.h>
#include <rtc.h>
#include <mdev.h>
#include <mdev_rtc.h>
#include <arch/boot_flags.h>

/* These are the bits [8 :0] of SYS_CTRL->REV_ID
 * It si used to differentiate between chip versions
 * pf  88 MC200
 */
#define REV_ID_A1 0x41

#define RTC_COUNT_ONE_MSEC 32
/* all 3 wakeup sources RTC , GPIO0 and GPIO1 */
#define WAKEUP_SRC_MASK  0x7

typedef struct {
	bool enabled;
	power_state_t mode;
	unsigned int threshold;
	unsigned int latency;
	bool io_d0_on;
	bool io_d1_on;
	bool io_d2_on;
} pm_state_t;

static pm_state_t pm_state;


typedef struct {
	uint32_t ISER[2];
	volatile uint8_t  IP[64];
	volatile uint32_t ICSR;
	volatile uint32_t VTOR;
	volatile uint32_t AIRCR;
	volatile uint32_t SCR;
	volatile uint32_t CCR;
	volatile uint8_t  SHP[12];
	volatile uint32_t SHCSR;
	volatile uint32_t CFSR;
	volatile uint32_t HFSR;
	volatile uint32_t DFSR;
	volatile uint32_t MMFAR;
	volatile uint32_t BFAR;
	volatile uint32_t AFSR;
	volatile uint32_t CPACR;
	/* Sys tick registers  */
	volatile uint32_t SYSTICK_CTRL;
	volatile uint32_t SYSTICK_LOAD;
}  pm3_saved_sys_regs;

static pm3_saved_sys_regs sys_regs ;
static mdev_t *rtc_dev;
static int wakeup_reason;

uint32_t *nvram_saved_sp_addr  __attribute__((section(".nvram")));
#ifdef CONFIG_HW_RTC
extern uint8_t tcalib;
#endif

#ifndef CONFIG_HW_RTC
/*  This function is called on exit from low power modes
 *  due to RTC expiry.
 *  This updates OS global tick count  and also posix time
 *  counter on basis of the time duration for which system
 *  was in low power mode.
 *  @param [in] : time_duration in milliseconds.
 */
static void update_system_time_on_rtc_wakeup(uint32_t time_dur)
{
	time_t  curr_time;
	/* Wakeup was due to RTC expiry
	 * Simply update values using time duration for
	 * which RTC was configured
	 */
	/* Update sys ticks*/
	os_update_tick_count(os_msec_to_ticks(time_dur));
	curr_time = wmtime_time_get_posix();
	curr_time += (time_dur / 1000);

	/* Update posix time in seconds*/
	wmtime_time_set_posix(curr_time);
}

/*  This function is called on exit from low power modes
 *  due to GPIO toggle.
 *  This updates OS global tick count  and also posix time
 *  counter on basis of the time duration for which system
 *  was in low power mode.
 */
static void update_system_time_on_gpio_wakeup()
{
	time_t  curr_time;
	/* Wakeup is by GPIO toggle
	 * read RTC counter value and convert it to
	 * ticks and seconds and update accordingly
	 */
	uint32_t rtc_count = rtc_drv_get(rtc_dev);

	/* Get time in milliseconds from RTC tick count  */
	rtc_count = rtc_count;

	/* Update sys ticks*/
	os_update_tick_count(os_msec_to_ticks(rtc_count));
	curr_time = wmtime_time_get_posix();
	curr_time += (rtc_count / 1000);

	/* Update posix time in seconds*/
	wmtime_time_set_posix(curr_time);
}
#endif

static void update_time(uint32_t time_dur)
{
#ifdef CONFIG_HW_RTC
	if (wakeup_reason & ERTC)
		os_update_tick_count(rtc_drv_get_uppval(rtc_dev) + 1);
	else
		os_update_tick_count(rtc_drv_get(rtc_dev) + tcalib);
#else

	if (wakeup_reason & ERTC) {
		update_system_time_on_rtc_wakeup(time_dur);
	} else {
		update_system_time_on_gpio_wakeup();
	}
	rtc_drv_stop(rtc_dev);
	rtc_drv_set_cb(NULL);
#endif
}

static void switch_off_io_domains()
{
	/* Turn OFF different power domains. */
	PMU_PowerOffVDDIO(PMU_VDDIO_SDIO);
	PMU_PowerDeepOffVDDIO(PMU_VDDIO_AON);
	if (pm_state.mode == PM2) {
		if (!pm_state.io_d0_on)
			PMU_PowerOffVDDIO(PMU_VDDIO_D0);
		if (!pm_state.io_d1_on)
			PMU_PowerOffVDDIO(PMU_VDDIO_D1);
		if (!pm_state.io_d2_on)
			PMU_PowerOffVDDIO(PMU_VDDIO_D2);
	} else {
		PMU_PowerOffVDDIO(PMU_VDDIO_D0);
		PMU_PowerOffVDDIO(PMU_VDDIO_D1);
		PMU_PowerOffVDDIO(PMU_VDDIO_D2);
	}
}

static void switch_on_io_domains()
{
	/* Turn ON different power domains. */
	PMU_PowerOnVDDIO(PMU_VDDIO_SDIO);
	PMU_PowerOnVDDIO(PMU_VDDIO_D0);
	PMU_PowerOnVDDIO(PMU_VDDIO_D1);
	PMU_PowerOnVDDIO(PMU_VDDIO_D2);
	PMU_PowerOnVDDIO(PMU_VDDIO_AON);
}

/*  This function saves SCB, NVIC and Sys Tick registers
 *  before entering low power state of PM3*/
static void save_system_context()
{
	volatile int cnt = 0;
	/* Save Interrupt Enable registers of NVIC */
	/* Since MC200 defines only 64 interrupts
	 * Save only NVIC ISER [0]  NVIC ISER [1]
	 */
	for (cnt = 0 ; cnt < 2; cnt++)
		sys_regs.ISER[cnt] = NVIC->ISER[cnt];

	/* Save Interrupt Priority Registers of NVIC */
	for (cnt = 0 ; cnt < 64; cnt++)
		sys_regs.IP[cnt] = NVIC->IP[cnt];

	/* Save SCB configuration */
	sys_regs.ICSR = SCB->ICSR;
	sys_regs.VTOR = SCB->VTOR;
	sys_regs.AIRCR = SCB->AIRCR;
	sys_regs.SCR  =  SCB->SCR;
	sys_regs.CCR = SCB->CCR;
	for (cnt = 0 ; cnt < 12; cnt++)
		sys_regs.SHP[cnt] = SCB->SHP[cnt] ;

	sys_regs.SHCSR = SCB->SHCSR;
	sys_regs.CFSR = SCB->CFSR;
	sys_regs.HFSR = SCB->HFSR;
	sys_regs.DFSR = SCB->DFSR;
	sys_regs.MMFAR = SCB->MMFAR ;
	sys_regs.BFAR = SCB->BFAR;
	sys_regs.AFSR = SCB->AFSR;
	sys_regs.CPACR = SCB->CPACR ;

	/* Save sys tick registers */
	sys_regs.SYSTICK_CTRL = SysTick->CTRL;
	sys_regs.SYSTICK_LOAD = SysTick->LOAD;
}

static void restore_system_context()
{
	int cnt = 0;
	/* On exit from PM3 NVIc registers are not retained.
	 * A copy of ISER registers was saved in NVRAM beforee
	 * entering PM3.Using these saved values enable the interrupts.
	 */
	for (cnt = 0 ; cnt < 2 ; cnt++) {
		if (sys_regs.ISER[cnt]) {
			uint32_t temp_reg = sys_regs.ISER[cnt];
			int  i = 0;
			while (temp_reg) {
				if (temp_reg & 0x1)
					NVIC_EnableIRQ(i + cnt * 32);
				i++;
				temp_reg = temp_reg >> 1;
			}
		}
	}
	/* Restore NVIC interrupt priority registers  */
	for (cnt = 0 ; cnt < 64; cnt++)
		NVIC->IP[cnt] = sys_regs.IP[cnt];

	/* Restore SCB configuration  */
	SCB->ICSR = sys_regs.ICSR ;
	SCB->VTOR = sys_regs.VTOR;
	for (cnt = 0 ; cnt < 12; cnt++)
		SCB->SHP[cnt] =  sys_regs.SHP[cnt];
	SCB->AIRCR = sys_regs.AIRCR;
	SCB->SCR = sys_regs.SCR;
	SCB->CCR = sys_regs.CCR;

	SCB->SHCSR = sys_regs.SHCSR;
	SCB->CFSR = sys_regs.CFSR;
	SCB->HFSR = sys_regs.HFSR;
	SCB->DFSR = sys_regs.DFSR;
	SCB->MMFAR = sys_regs.MMFAR;
	SCB->BFAR = sys_regs.BFAR;
	SCB->AFSR = sys_regs.AFSR;
	SCB->CPACR = sys_regs.CPACR;

	/* Sys tick restore */
	SysTick->CTRL = sys_regs.SYSTICK_CTRL;
	SysTick->LOAD = sys_regs.SYSTICK_LOAD;
}

void asm_mc200_pm3()
{
	/* Address: 0x480C0008 is the address in NVRAM which holds address
	 * where control returns after exit from PM3*/
	__asm volatile
	(
		"b l2\n"
		"save_sp_addr : .word  nvram_saved_sp_addr\n"
		"nvram_ret_addr_ptr : .word 0x480C0008\n"

	/* All general purpose registers and special registers
	*  are saved by pushing them on current thread's stack
	*  (psp is being used as sp) and finally SP is saved in NVRAM location*/
		"l2:\n"
		"push  { r1}\n"
		"mrs r1,  msp\n"
		"push  { r1 }\n"
		"mrs   r1,  basepri\n"
		"push  { r1 }\n"
		"mrs r1,  primask\n"
		"push  { r1 }\n"
		"mrs r1,  faultmask\n"
		"push  { r1 }\n"
		"mrs r1,  control\n"
		"push  { r1 }\n"
		"push  {r0-r12}\n"
		"push {lr}\n"
		"ldr   r0 , save_sp_addr\n"
		"str sp , [r0]\n"
		"ldr  r0 , nvram_ret_addr_ptr\n"
		"mov r1 , pc\n"
		"add r1 ,r1 , #24\n"
		"str  r1 , [r0]\n"
	 );

	/*
	 * Execute WFI to generate a state change
	 * and system is in an unresponsive state
	 * press wakeup key to get it out of standby
	 * If time_to_standby is set to valid value
	 * RTC is programmed and RTC generates
	 * a wakeup signal.
	 */
	__asm volatile ("wfi");

	__asm volatile ("nop");
	__asm volatile ("nop");
	__asm volatile ("nop");
	__asm volatile ("nop");
	__asm volatile ("nop");
	__asm volatile ("nop");
	__asm volatile ("nop");
	__asm volatile ("nop");
	__asm volatile ("nop");

	/* When system exits PM3 all registers need to be
	 * restored as they are lost.
	 * After exit from PM3, control register is 0
	 * This indicates that msp is being used as sp
	 * psp is populated with saved_sp_addr value.
	 * When control register is popped, system starts using
	 * psp as sp and thread stack is now accessible.
	 */
	__asm volatile
	(
		" ldr  r0, save_sp_addr\n"
		" ldr sp , [r0]\n"
		" pop {lr}\n"
		" ldr r1 , [r0]\n"
		" msr psp , r1\n"
		" pop { r0-r12}\n"
		" mov r1, sp\n"
		" add r1 ,r1, #4\n"
		" msr psp , r1\n"
		" pop {r1}\n"
		" msr   control , r1\n"
		" pop {r1}\n"
		" msr  faultmask  , r1\n"
		" pop {r1}\n"
		" msr primask   , r1\n"
		" pop {r1}\n"
		" msr  basepri  , r1\n"
		" pop {r1}\n"
		" msr   msp , r1\n"
		" pop {r1}\n"
	 );
}

static void pm3_post_low_power_operations(uint32_t time_dur)
{
	/* Restore to idle state */
	PMU_SetSleepMode(PMU_PM1);
	switch_on_io_domains();

	/* Take PMU clock back to 32Mhz */
	RC32M->CLK.BF.SEL_32M = 1;
	CLK_ModuleClkDivider(CLK_PMU, 1);

	CLK_Init();
	restore_system_context();
	wakeup_reason |= NVIC->ISPR[0] & WAKEUP_SRC_MASK;
	update_time(time_dur);
	pm_invoke_ps_callback(ACTION_EXIT_PM3);
	XFLASH_PowerDown(DISABLE);
}

void extpin0_cb(void)
{
	wakeup_reason |= EGPIO0;
}

void extpin1_cb(void)
{
	wakeup_reason |= EGPIO1;
}

#ifndef CONFIG_HW_RTC
void dummy_rtc_cb()
{
	/* Do nothing */
}
#endif

static void configure_enable_rtc(uint32_t low_pwr_duration)
{
	struct bootrom_info *info = (struct bootrom_info *)&_nvram_start;
	if (low_pwr_duration == 0) {
		if (info->version >= BOOTROM_A0_VERSION)
			low_pwr_duration = 0xffffffff;
		else
			low_pwr_duration = 0xffffffff / RTC_COUNT_ONE_MSEC;
	}
	_os_delay(1);
#ifdef CONFIG_HW_RTC
	hwrtc_time_update();
	rtc_drv_set(rtc_dev, low_pwr_duration);
	rtc_drv_reset(rtc_dev);
#else
	rtc_drv_set(rtc_dev, low_pwr_duration);
	rtc_drv_reset(rtc_dev);
	rtc_drv_set_cb(dummy_rtc_cb);
	rtc_drv_start(rtc_dev);
#endif
}

/* Prepare to go to low power
 *  Change clock source to RC32M
*   Switch off PLLs, XTAL
*  Set Deep sleep bit in SRC register
*  Initiate state change
*/
static void prepare_for_low_power(uint32_t low_pwr_duration,
				  power_state_t state)
{
	/* Turn off Systick to avoid interrupt
	*  when entering low power state
	*/
	sys_regs.SYSTICK_CTRL = SysTick->CTRL;
	sys_regs.SYSTICK_LOAD = SysTick->LOAD;
	SysTick->CTRL = 0;
	SysTick->LOAD = 0;
	configure_enable_rtc(low_pwr_duration);

	wakeup_reason = 0;

	switch (state) {
	case PM2:
		pm_invoke_ps_callback(ACTION_ENTER_PM2);
		break;
	case PM3:
		pm_invoke_ps_callback(ACTION_ENTER_PM3);
		break;
	case PM4:
		pm_invoke_ps_callback(ACTION_ENTER_PM4);
		break;
	default:
		break;
	}

       /* Send power down instruction to flash */
	XFLASH_PowerDown(ENABLE);

	/* Switch clock source to RC 32Mhz */
	CLK_SystemClkSrc(CLK_RC32M);

	/* Take PMU clock down to 1Mhz */
	RC32M->CLK.BF.SEL_32M = 0;
	CLK_ModuleClkDivider(CLK_PMU, 0xf);

	/* Disable xtal */
	CLK_MainXtalDisable();

	/* Disable Audio PLL */
	CLK_AupllDisable();

	/* Disable System PLL */
	CLK_SfllDisable();

	switch_off_io_domains();
}

/*
  Restore settings back to normal after low power mode exit
 */
static void pm2_post_low_power_operations(uint32_t time_dur)
{
	/* Restore to idle state */
	PMU_SetSleepMode(PMU_PM1);

	/* Switch on all IO domains which were switched off
	 * while entering PM2 standby mode
	 */
	switch_on_io_domains();

	/* Take PMU clock back to 32Mhz */
	RC32M->CLK.BF.SEL_32M = 1;
	CLK_ModuleClkDivider(CLK_PMU, 1);
	CLK_Init();
	wakeup_reason |= NVIC->ISPR[0] & WAKEUP_SRC_MASK;
	update_time(time_dur);
	pm_invoke_ps_callback(ACTION_EXIT_PM2);
	XFLASH_PowerDown(DISABLE);

	/* restore Systick back to saved values */
	SysTick->CTRL = sys_regs.SYSTICK_CTRL;
	SysTick->LOAD = sys_regs.SYSTICK_LOAD;

}

/* This function puts system in standby state  */
void  mc200_pm2(uint32_t time_to_standby)
{
	/* Prepare for low power change  */
	prepare_for_low_power(time_to_standby, PM2);

	PMU_SetSleepMode(PMU_PM2);

	/*
	  This is needed to match up timing difference between
	  APB and AHB which causes malfunction.
	  PWR_MODE is written through APB bus and  then  SCR is written
	  through AHB bus followed by instruction WFI.
	  Due to the APB bus delay, there is possibility that CM3
	  enters sleep mode but PWR_MODE are not set to PM2 (or PM3 or PM4)
	  in time, which will result in failure to wakeup.
	*/
	while (PMU_PM2 != PMU_GetSleepMode())
		;
	/*
	 * Execute WFI to generate a state change
	 * and system is in an unresponsive state
	 * press wakeup key to get it out of standby
	 * If time_to_standby is set to valid value
	 * RTC is programmed and RTC generates
	 * a wakeup signal
	*/
	__asm volatile ("wfi");
	pm2_post_low_power_operations(time_to_standby);
}

/* This function puts system in sleep state */
void  mc200_pm3(uint32_t time_to_sleep)
{
	/* Save registers of NVIC and SCB  */
	save_system_context();

	/* Prepare for low power */
	prepare_for_low_power(time_to_sleep, PM3);

	/* Set the deepsleep bit
	   change PMU state to shutdown PM3 */
	PMU_SetSleepMode(PMU_PM3);

	/*
	  This is needed to match up timing difference between
	  APB and AHB which causes malfunction.
	  PWR_MODE is written through APB bus and  then  SCR is written
	  through AHB bus followed by instruction WFI.
	  Due to the APB bus delay, there is possibility that CM3
	  enters sleep mode but PWR_MODE are not set to PM2 (or PM3 or PM4)
	  in time, which will result in failure to wakeup.
	*/
	while (PMU_PM3 != PMU_GetSleepMode())
		;
	asm_mc200_pm3();
	pm3_post_low_power_operations(time_to_sleep);
}

/* This function puts system in shut down state */
static void mc200_pm4(uint32_t time_to_wakeup)
{
	/* Prepare for low power */
	prepare_for_low_power(time_to_wakeup, PM4);

	/* Set the deepsleep bit
	   change PMU state to shutdown PM4 */
	PMU_SetSleepMode(PMU_PM4);
	/*
	  This is needed to match up timing difference between
	  APB and AHB which causes malfunction.
	  PWR_MODE is written through APB bus and  then  SCR is written
	  through AHB bus followed by instruction WFI.
	  Due to the APB bus delay, there is possibility that CM3
	  enters sleep mode but PWR_MODE are not set to PM2 (or PM3 or PM4)
	  in time, which will result in failure to wakeup.
	*/

	while (PMU_PM4 != PMU_GetSleepMode())
		;
	/*
	 * Execute WFI to  generate a state change
	 * and system is in an unresponsive state
	 * press wakeup key to get it out of standby
	 */
	__asm volatile ("wfi");
}

static void print_cli_usage()
{
	wmprintf("Usage :-\r\n pm-mc200-state  power_state");
	wmprintf("  time-duration \r\n");
	wmprintf("power_state :\r\n2 -> PM2\r\n");
	wmprintf("3 -> PM3\r\n");
	wmprintf("4 -> PM4\r\n");
}

/*
   Command handler for power management
    */
static void cmd_pm_mc200_state(int argc, char *argv[])
{
	uint32_t time_dur = 0;
	uint32_t state = 0;
	if (argc < 2) {
		print_cli_usage();
		return;
	}
	state = atoi(argv[1]);
	if (state > PM4) {
		print_cli_usage();
		return;
	}
	if (argc == 3) {
		time_dur = atoi(argv[2]);
	}
	if (time_dur == 0) {
		wmprintf("No Timeout specified\r\n");
		wmprintf("Wakeup will occur by Wakeup key press\r\n ");
		wmprintf("PM0->PM%d \t", state);
	} else {
		wmprintf("Timeout specified :%d millisec\r\n", time_dur);
		wmprintf("The system can be brought out of standby by an RTC "
			 "timeout or a wakeup key press\r\n");
		wmprintf(" PM0->PM%d\t", state);
		wmprintf(" For %d millisec\n\r ", time_dur);
	}
	pm_mc200_state(state, time_dur);
	wmprintf("\n\r PM%d->PM0", state);
	if (wakeup_reason & ERTC) {
		wmprintf("\t (Wakeup by Timeout %d millisec)\n\r",
			 time_dur);
	} else {
		wmprintf("\t (Wakeup by GPIO toggle)\n\r");
	}
}
static void print_usage()
{
	wmprintf(" Usage : pm-mc200-cfg <enable> <mode> <threshold>\r\n");
	wmprintf(" <enabled> : true to enable \r\n"
		 "             false to disable \r\n");
	wmprintf(" <mode>    : 88MC200 power mode\r\n"
		 "             2 : PM2\r\n"
		 "             3 : PM3\r\n");
	wmprintf(" <threshold> System will not go to power save state\r\n"
		 "             if it is going to wake up in less than\r\n");
	wmprintf("             [threshold] ticks.\r\n");
}

static void cmd_pm_mc200_cfg(int argc, char *argv[])
{
	if (argc < 3) {
		print_usage();
		return;
	}
	int mode = atoi(argv[1]);
	if (mode > PM4 || mode < PM0) {
		print_usage();
		return;
	}
	int enabled = atoi(argv[1]);
	if (enabled != true || enabled != false) {
		print_usage();
		return;
	}
	pm_mc200_cfg(atoi(argv[1]),  atoi(argv[2]), atoi(argv[3]));
}

static struct cli_command commands[] = {
	{"pm-mc200-cfg", "<enable> <mode> <threshold>", cmd_pm_mc200_cfg},
	{"pm-mc200-state", "<state> [timeout in milliseconds]",
	 cmd_pm_mc200_state},
};

int pm_mc200_cli_init()
{
	int i;

	for (i = 0; i < sizeof(commands) / sizeof(struct cli_command); i++)
		if (cli_register_command(&commands[i]))
			return -WM_FAIL;
	return WM_SUCCESS;
}

void pm_mc200_io_cfg(bool io_d0_on, bool io_d1_on, bool io_d2_on)
{
	pm_state.io_d2_on = io_d2_on;
	pm_state.io_d1_on = io_d1_on;
	pm_state.io_d0_on = io_d0_on;
}


void pm_mc200_cfg(bool enabled, power_state_t mode,
		  unsigned int threshold)
{
	if (mode > PM4 || mode < PM0) {
		return;
	}

	if (!is_pm_init_done()) {
		pm_e("Power Manager Module is not initialized.");
		return;
	}

	pm_state.enabled = enabled;
	pm_state.mode = mode;
	pm_state.threshold = os_msec_to_ticks(threshold);
	pm_state.latency = os_msec_to_ticks(10);
}

int pm_mc200_state(power_state_t state, uint32_t time_dur)
{
	if (state > PM4 || state < PM2) {
		return -WM_FAIL;
	}
	os_disable_all_interrupts();
	__disable_irq();
	switch (state) {
	case PM2:
		mc200_pm2(time_dur);
		break;
	case PM3:
		mc200_pm3(time_dur);
		break;
	case PM4:
		mc200_pm4(time_dur);
		break;
	default:
		break;
	}
	__enable_irq();
	os_enable_all_interrupts();
	return wakeup_reason;
}

int pm_wakeup_source()
{
	return wakeup_reason;
}

bool pm_mc200_is_enabled()
{
	return pm_state.enabled;
}

int pm_mc200_get_mode()
{
	return pm_state.mode;
}
/* Function called when system executes idle thread */
static void pm_idle()
{
	os_disable_all_interrupts();
	__disable_irq();
	if (pm_state.enabled && !wakelock_isheld() &&
		(os_ticks_to_unblock() > (pm_state.threshold +
				pm_state.latency))) {
		switch (pm_state.mode) {
		case PM2:
			mc200_pm2(os_ticks_to_unblock()
					- pm_state.latency);
			break;
		case PM3:
			mc200_pm3(os_ticks_to_unblock()
					- pm_state.latency);
			break;
		default:
			break;
		}
		os_enable_all_interrupts();
	} else if (pm_state.mode != PM0) {
		os_enable_all_interrupts();
		PMU_SetSleepMode(PMU_PM1);
		__asm volatile ("wfi");
	} else
		/* enable interrupts for PM0 */
		os_enable_all_interrupts();

	__enable_irq();
}

static void wakeup_latency_configuration()
{
	/* Registers in PMU to be configured.
	 * PMIP_BRNDET_VFL  [19:18]   for VFL ramp up speed
	 * PMIP_BRNDET_AV18 [19:18]   for AV18 ramp up speed
	 * PMIP_CHP_CTRL0   [15:14]   for V12 ramp up speed
	 */

	/* configure Power delay control register */
	PMU_PowerReadyDelayConfig_Type pwr_ready_delay_cfg;

	/* PMU_VFL_READY_DELAY_SHORT = 204.8us */
	pwr_ready_delay_cfg.vflDelay = PMU_VFL_READY_DELAY_SHORT;

	/* PMU_V18_READY_DELAY_SHORT = 51.2us */
	pwr_ready_delay_cfg.v18Delay = PMU_V18_READY_DELAY_SHORT;

	/* PMU_V12_READY_DELAY_SHORT =  51.2us */
	pwr_ready_delay_cfg.v12Delay = PMU_V12_READY_DELAY_SHORT;

	PMU_ConfigPowerReadyDelayTime(&pwr_ready_delay_cfg);


	/* Configure power ramp up control register
	 * PMIP_BRNDET_V12  [1:0]  for V12 LDO ramp rate selection.
	 * PMIP_BRNDET_AV18 [3:2]  for VAv18 LDO ramp rate selection.
	 */

	PMU_PowerRampRateConfig_Type pwr_ramp_rate_cfg;

	pwr_ramp_rate_cfg.v12ramp = PMU_V12_RAMP_RATE_FAST;

	pwr_ramp_rate_cfg.v18ramp = PMU_V18_RAMP_RATE_FAST;
	PMU_ConfigPowerRampUpTime(&pwr_ramp_rate_cfg);
}


void arch_pm_init(void)
{
	memset(&pm_state, 0, sizeof(pm_state));
	/*
	 * Set this to 1 so that WFI will be executed when
	 * core is in idle thread context. If it is set to 0
	 * by some application core will always be in PM0 mode..
	 */
	pm_state.mode = 1;
	os_setup_idle_function(pm_idle);
	rtc_dev = rtc_drv_open("MDEV_RTC");

	if (board_wakeup0_functional() &&
	    !board_wakeup0_wifi()) {
		install_int_callback(INT_EXTPIN0,
				     0, extpin0_cb);
		NVIC_EnableIRQ(ExtPin0_IRQn);
		NVIC_SetPriority(ExtPin0_IRQn, 8);
	}
	if (board_wakeup1_functional() &&
	    !board_wakeup1_wifi()) {
		install_int_callback(INT_EXTPIN1,
				     0, extpin1_cb);
		NVIC_EnableIRQ(ExtPin1_IRQn);
		NVIC_SetPriority(ExtPin0_IRQn, 8);
	}
	wakeup_latency_configuration();

	/* Check whether the chip is A1
	 * if so configure power save settings
	 * for following pins
	 * XTAL32K_IN
	 * XTAL32K_OUT
	 * TDO
	 * GPIO 26
	 */
	volatile uint32_t revid = SYS_CTRL->REV_ID.WORDVAL;

	if (revid & REV_ID_A1) {
		/* Configure pad pull up or pull down
		 * to reduce current leakage*/
		PMU_ConfigPowerSavePadMode
			(PMU_PAD_XTAL32K_IN, PMU_PAD_MODE_POWER_SAVING);
		PMU_ConfigPowerSavePadMode
			(PMU_PAD_XTAL32K_OUT, PMU_PAD_MODE_POWER_SAVING);
		PMU_ConfigPowerSavePadMode
			(PMU_PAD_TDO, PMU_PAD_MODE_POWER_SAVING);

		PMU_ConfigWakeupPadMode
			(PMU_GPIO26_INT, PMU_WAKEUP_PAD_PULLUP);

	}
}

void arch_reboot()
{
	PMU_SetSleepMode(PMU_PM1);
	CLK_SystemClkSrc(CLK_RC32M);
	NVIC_SystemReset();
}

/* This API added to skip WFI in idle thread.
 *  It keeps MC200 in PM0.
 *  Used in current measurement demo pm_mc200_demo.
 */
void pm_pm0_in_idle()
{
	pm_state.mode = PM0;
}

