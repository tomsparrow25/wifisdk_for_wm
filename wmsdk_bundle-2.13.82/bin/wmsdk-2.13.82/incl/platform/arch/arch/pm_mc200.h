/*
 * Copyright 2008-2013, Marvell International Ltd.
 * All Rights Reserved.
 */

/*! \file pm_mc200.h
 *  \brief 88MC200 power management module
 *
 * allows application to put 88MC200 in various low power modes.
 * 88MC200 power modes in brief are :
 *
 * - PM0 (Active Mode): This is the full power state of MC200.Instruction
 *                      execution takes place only in PM0.
 *
 * - PM1 (Idle Mode)  : In this mode, the Cortex M3 core function clocks are
 *                      stopped until the occurrence of any interrupt.
 *                      This consumes lower power than PM0.
 *                      On exit from this mode, execution resume from
 *                      the next instruction in SRAM.
 *
 * - PM2 (Standby Mode):In this mode, the Cortex M3, most of the peripherals &
 *                      SRAM arrays are in low-power mode.
 *                      The PMU and RTC are operational.
 *                      A wakeup can happen by timeout (RTC based)
 *                      or by asserting the WAKEUP 0/1 lines.
 *                      This consumes much lower power than PM1.
 *                      On exit from this mode execution resumes from
 *                      the next instruction in SRAM.
 *
 * - PM3 (Sleep Mode) : This mode further aggressively conserves power.
 *                      Only 192 KB (160 KB in SRAM0  and 32 KB in SRAM1)
 *                      out of 512 KB of SRAM is alive. All peripherals
 *                      are turned off and register config is lost.
 *                      Applications should restore the peripheral config
 *                      after exit from PM3. This consumes lower power
 *                      than in PM2. A wakeup can happen by timeout (RTC based)
 *                      or by asserting the WAKEUP 0/1 lines.
 *                      On exit from this mode execution resumes from
 *                      the next instruction in SRAM.
 *
 * - PM4(Shutoff Mode) : This simulates a shutdown condition.
 *                      A wakeup can happen by timeout (RTC based) or by
 *                      asserting the WAKEUP 0/1 lines.
 *                      This is the lowest power state of 88MC200.
 *                      On wakeup execution begins from bootrom as
 *                      if a fresh bootup has occurred.
 *

 *  The power management module provides two APIs :
 *
 * - a power manager framework API, pm_mc200_cfg(), that allows applications to
 * define a power management policy. Once defined, the system will automatically
 * put the 88MC200 into the configured power save modes on detection of idle
 * activity.
 *
 * - a raw API, pm_mc200_state(), which puts the 88MC200 into the
 * configured power save states immediately.
 *
 * @section pm_table 88MC200 Power Modes relationship Table.
 *
 * Cortex M3 state
 *
 | Run (C0)  | Idle (C1) | Standby (C2)  | Off (C3) |
 | ----:---- | --------- | ------------- | -------- |
 | HCLK On   | HCLK On   | HCLK Off      | Power removed |
 | FCLK On   | FCLK Off   | FCLK Off     | Power removed |
 *
 * SRAM Modes
 *
 | Run (M0)  | Standby (M2) | Off (M3)  |
 | ----:---- | --------- | ------------- |
 | On        | Low power    | Power off  |
 *
 * Power Modes with  Core , SRAM , Peripherals\n
 *

|             | PM0 | PM1 | PM2 | PM3 | PM4 |
 | ----:----- | --- | --- | --- | --- | --- |
 |  Cortex M3 | C0  | C1  | C2  | C3  |C3 |
 |   SRAM     | M0  | M0  | M2  | M2  |M3 |
 |   Peripherals | On  | On  | Retained  | Off  |Off |
 *
 *
 * @section pm_usage Usage
 *  88MC200 power management can be used by applications as follows:
 * -# Call @ref pm_init(). This initializes the power management module. The
 *  most basic power-management scheme is enabled at this time, which puts the
 *  88MC200 in PM1 mode on idle.
 * -# Register callback for low power entry and exit using @ref
 *  pm_register_cb. This will ensure that the callback gets called everytime
 *  before entering the power save mode or exiting from the power save
 *  mode. Applications can perform any state save-restore operations in this
 *  callback.
 * -# Set power save configuration @ref pm_mc200_cfg(). This call will ensure
 *  that the system continues to enter the configured power save mode as and
 *  when it detects inactivity. The power manager framework makes sure that the
 *  system comes up just in time to service any software timers, or threads
 *  waiting on timed sleep.
 * -# If an application/driver expects to receive events from an external source
 *  (apart from RTC or wakeup0/1 lines), then it may have to ensure that the
 *  system does not enter power save modes in this duration. This control can be
 *  exerted on the power manager framework by using wakelocks. Acquiring a
 *  wakelock using wakelock_get() prevents the power manager framework from
 *  putting the system into low power modes. The wakelock can be released using
 *  wakelock_put().
 *
 * @cond uml_diag
 *
 * 88MC200 state transiotions are as shown below:
 *
 * @startuml{states.jpg}
 *
 * [*] --> BootUp : On reset key press or reboot
 * PM0 : Active
 * PM1 : Idle
 * PM2 : Standby
 * PM3 : Sleep
 * PM4 : Shutoff
 * BootUp --> PM0
 * PM1 --> PM0 : Any interupt
 * PM0 --> PM1 : Wait for interrupt (WFI)
 * PM2 --> PM0 : Wakeup 0/1 or timeout
 * PM0 --> PM2 : PMU_State PM2 + WFI
 * PM3 --> PM0 : Wakeup 0/1 or timeout
 * PM0 --> PM3 : PMU_State PM3 + WFI
 * PM4 --> BootUp : Wakeup 0/1 or timeout
 * PM0 --> PM4 : PMU_State PM4 + WFI
 *
 * @enduml
 * @endcond
 *
 */


#ifndef _PM_MC200_H_
#define _PM_MC200_H_

#include "mc200_pmu.h"

extern void init_uart();
extern void init_clk();

/**
 *  Indicates the wakeup source
 *  It can be any external GPIO pin or RTC.
 */
enum wakeup_cause {
	/** Wakeup source is EXT GPIO 0 */
	EGPIO0 = 1,
	/** Wakeup Source is EXT GPIO 1 */
	EGPIO1 = 1<<1,
	/** Wakeup source is RTC */
	ERTC   = 1<<2
};

/** Power States of MC200 */
typedef enum {

	/** (Active Mode): This is the full power state of MC200.
	 *  Instruction execution takes place only in PM0.
	*/
	PM0,
	/** (Idle Mode): In this mode Cortex M3 core function
	 *  clocks are stopped until the occurrence of any interrupt.
	 *  This consumes lower power than PM0. */
	PM1,

	/** (Standby Mode):In this mode, the Cortex M3,
	 *  most of the peripherals & SRAM arrays are in
	 *  low-power mode.The PMU and RTC are operational.
	 *  A wakeup can happen by timeout (RTC based) or by asserting the
	 *  WAKEUP 0/1 lines.This consumes much lower power than PM1.
	 */
	PM2,

	/**(Sleep Mode): This mode further aggressively conserves power.
	 * Only 192 KB (160 KB in SRAM0  and 32 KB in SRAM1)
	 * out of 512 KB of SRAM is alive. All peripherals
	 * are turned off and register config is lost.
	 * Application should restore the peripheral config
	 * after exit form PM3. This consumes lower power
	 * than in PM2. A wakeup can happen by timeout (RTC based)
	 * or by asserting the WAKEUP 0/1 lines.
	 */
	PM3,

	/** (Shutoff Mode): This simulates a shutdown condition.
	 * A wakeup can happen by timeout (RTC based) or by
	 * asserting the WAKEUP 0/1 lines.
	 * This is the lowest power state of 88MC200.
	 * On wakeup execution begins from bootrom as
	 * if a fresh bootup has occurred.
	 */
	PM4
} power_state_t;


/** Turn on individual IO domains
 *
 * 88MC200 has 6 configurable IO Voltage domains.
 *  -# D0   :  GPIO 0 to GPIO 17
 *  -# Aon  : GPIO 18 to GPIO 27 (Always On)
 *  -# D1   : GPIO 28 to GPIO 50
 *  -# SDIO : GPIO 51 to GPIO 56 (SDIO interface)
 *  -# D2   : GPIO 59 to GPIO 79
 *  -# FL   : Internal Flash
 *
 *  Each domain can be in 3 possible states
 *  - ON : Full power on.
 *  - Deep OFF : Low power state.
 *               Latency to go back to ON state is less.
 *  - OFF : Completely off.
 *          Latency to go back to ON state is higher than Deep Off.
 *
 *  When 88MC200 enters @ref PM2, @ref PM3, @ref PM4 using
 *  power management APIs all IO voltage domains except
 *  Aon are turned off. Aon is kept in deep off. These APIs assist in
 *  controlling the IO domains in PM0.
 *
 *  @param [in] domain IO domain to be turned on
 *  @return none
 */
#define pmu_drv_poweron_vddio(domain)  PMU_PowerOnVDDIO(domain)

/** Turn off individual IO domains
 *  @param [in] domain IO domain to be turned off
 *  @return none
 *  @note For details of IO domains please see
 *        pmu_drv_poweron_vddio()
 */
#define pmu_drv_poweroff_vddio(domain)  PMU_PowerOffVDDIO(domain)

/** Deep off individual IO domains
 *  @param [in] domain IO domain to be set to deep off
 *  @return none
 *  @note For details of IO domains please see
 *        pmu_drv_poweron_vddio()
 */
#define pmu_drv_powerdeepoff_vddio(domain)  PMU_PowerDeepOffVDDIO(domain)


/** Configure Power Mgr Framework's MC200 Power Save
 *
 * This function is used to configure the power manager framework's MC200 Power
 * Save. When enabled, the power manager framework will detect if the system is
 * idle and put the MC200 processor in power save mode. Before entering power
 * save mode, the power manager framework will detect the next scheduled wakeup
 * for any process/timer and configure the sleep duration such that the
 * processor wakes up just in time to service that process. Once enabled the
 * power manager framework will continue to opportunistically put the system
 * into the configured low power mode.
 *
 * A threshold parameter is also supported. The power manager framework will
 * make the MC200 enter power save mode, only if the next scheduled thread
 * wakeup is greater than the configured threshold.
 *
 * For example, say,
 * -# the threshold is configured to 10 milliseconds
 * -# the PM2 exit latency is 3 milliseconds
 * Now if the system is idle, (that is there is no running thread in the
 * system) at time t0, the power management framework will put the 88MC200 into
 * power save mode, only if the next thread is scheduled to wakeup and execute
 * at t0 + 10 + 3 milliseconds. If a thread is scheduled to wakeup and execute
 * before this time, then the 88MC200 doesn't enter power save at time t0.
 *
 * @param[in] enabled true to enable, false to disable power manager framework
 * @param[in] mode 88MC200 power mode @ref power_state_t.
 * @param[in] threshold System will not go to power save state
 *                        if it is going to wake up in less than [threshold]
 *                        ticks.
 * @note This API should be used only for @ref PM2 and @ref PM3.
 *       The application should call this function only once
 *       to configure and enable power save scheme. Power manager
 *       framework takes care of opportunistically entering and
 *       exiting power save.
 * @note  Applications can register callbacks for low power entry and exit using
 *        @ref pm_register_cb. This will ensure that the callback gets called
 *        everytime before entering the power save mode or exiting from the
 *        power save mode. Applications can perform any state save-restore
 *        operations in this callback.
 */
void pm_mc200_cfg(bool enabled, power_state_t mode,
	 unsigned int threshold);


/** Enter low power state of MC200
 *
 * This function puts the core into the requested low power mode.
 * @param[in] state  the desired power mode of MC200  \ref power_state_t
 * @param[in] time_dur duration (in milliseconds) for which device
 *            should be in requested low power mode. The special value 0
 *            indicates maximum possible timeout that can be configured using
 *            RTC.
 *  @return   @ref wakeup_cause
 *
 *  @note The device wakes up automatically after the pre-configured
 *  time interval has expired or when the external gpio pin is asserted.
 *  For details please refer to the 88MC200 data sheet.
 *  MC200 by default keeps on toggling between PM0 and PM1.
 */
int pm_mc200_state(power_state_t state, uint32_t time_dur);



/** This function is used to get last source of wake up.
 *
 *  @return @ref wakeup_cause
 */
int pm_wakeup_source();

/** This function is used to check enable status of power manager framework.
 *
 *  @return  current power manager enable status,
 *  @return  true if  enabled
 *  @return  false if disabled
 */
bool pm_mc200_is_enabled();

/** Return target power save mode
 *
 * This function returns the configured power save mode that the 88MC200 will
 * enter on detection of inactivity.
 *
 *  @return @ref power_state_t
 */

int pm_mc200_get_mode();


/** Register 88MC200 Power Manager CLI
 *  @return  WM_SUCCESS on success
 *  @return  -WM_FAIL on failure
 *  @note  This function can be called after the power manager is initialized
*/
int pm_mc200_cli_init();

/** Manage IO Domains on/off in PM2
 *  This function allows application to determine which
 *  GPIO domain (D0, D1 , D2) are to be kept on or off
 *  when MC200 enters PM2
 *  @note This API should be used if  application
 *        wants to keep IO domains on in  PM2
 *        else by default they will be turned off while entering
 *        PM2 and turned on after exiting PM2.
 *  @param [in] io_d0_on true indicates D0 domain is kept on in PM2
 *                       false indicates D0 domain is turned off in PM2
 *  @param [in] io_d1_on true indicates D1 domain is kept on in PM2
 *                       false indicates D1 domain is turned off in PM2
 *  @param [in] io_d2_on true indicates D2 domain is kept on in PM2
 *                       false indicates D2 domain is turned off in PM2
 */
void pm_mc200_io_cfg(bool io_d0_on, bool io_d1_on, bool io_d2_on);



#endif   /* _PM_MC200_H_ */
