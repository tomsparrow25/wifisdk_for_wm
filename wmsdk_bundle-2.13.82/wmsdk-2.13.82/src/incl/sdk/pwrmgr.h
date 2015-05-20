/*  Copyright 2008-2013, Marvell International Ltd.
 *  All Rights Reserved.
 */

/*! \file pwrmgr.h
 *  \brief Power Manager
 *
 * The processor-specific part of the power manager provides functions like
 * pm_mc200_state() and pm_mc200_cfg() that can put the processor in various
 * power-save modes as described in  @link pm_mc200.h MC200 Power
 * Management @endlink.
 *
 * The power manager provides a functionality to register callbacks on entry or
 * exit from  power modes @ref power_state_t. Applications or drivers can
 * register a power save callback using the function pm_register_cb()
 * or deregister using pm_deregister_cb().
 */


#ifndef _PWRMGR_H_
#define _PWRMGR_H_

#include <wmtypes.h>
#include <wm_os.h>
#include <arch/pm_mc200.h>
#include <wmlog.h>

#define pm_e(...)				\
	wmlog_e("pm", ##__VA_ARGS__)

#ifdef CONFIG_PWR_DEBUG
#define pm_d(...)				\
	wmlog("pm", ##__VA_ARGS__)
#else
#define pm_d(...)
#endif

/** Power save event enumerations */
typedef enum {
	/** On entering PM2  mode */
	ACTION_ENTER_PM2 = 1,
	/** On exiting PM2 mode */
	ACTION_EXIT_PM2 = 2,
	/** On entering PM3  mode */
	ACTION_ENTER_PM3 = 4,
	/** On exiting  PM3 mode */
	ACTION_EXIT_PM3 = 8,
	/** On entering PM4 mode */
	ACTION_ENTER_PM4 = 16
} power_save_event_t;

/** Function pointer for callback function called on low power entry and exit */
typedef void (*power_save_state_change_cb_fn) (power_save_event_t event,
					       void *data);

/** Register Power Save Callback
 *
 *   This function registers a callback handler with the power manager. Based on
 *   the 'action' specified, this callback will be invoked on the corresponding
 *   power-save enter or exit events of the processor.
 *
 * @note The callbacks are called with interrupts disabled.
 *
 *  @param[in] action Logical OR of set of power-save events
 *             @ref power_save_event_t for which callback should be invoked.
 *  @param[in] cb_fn pointer to the callback function
 *  @param[in] data Pointer which can be a pointer to any
 *                         object or can be NULL.
 *  @return handle to be used in other APIs on success,
 *  @return -WM_E_INVAL  if incorrect parameter
 *  @return -WM_E_NOMEM if no space left.
 */
int pm_register_cb(unsigned char action, power_save_state_change_cb_fn cb_fn,
		   void *data);

/** De-register Power Save Callback
 *
 *  This function de-registers the power save callback handlers from the power
 *  manager.
 *
 *  @param[in] handle handle returned in pm_register_cb()
 *  @return WM_SUCCESS on success
 *  @return -WM_FAIL otherwise.
 */

int pm_deregister_cb(int handle);

/** Modify Power Save Callback
 *
 *  Change the power-save events for which the callback will be invoked.
 *  This function allows the application to change the events without
 *  changing  callback function.

 *  @param[in] handle handle returned in pm_register_cb()
 *  @param[in] action logical OR of new set of power-save
 *             events @ref power_save_event_t
 *  @return WM_SUCCESS on success
 *  @return -WM_FAIL otherwise.
 */
int pm_change_event(int handle, unsigned char action);

/*
 * This function is called from platform specific power manager code.
 *
 * It should be called with interrupts disabled.
 *
 * @param[in] action Power save event for which
 *                        callbacks will be invoked
 *                        @ref power_save_event_t
 */
void pm_invoke_ps_callback(power_save_event_t action);

/** Initialize the Power Manager module
 * This functions initializes the Power Manager module on boot up.
 * It registers an idle hook with OS which will allow processor to
 * toggle between Active (PM0) and Idle (PM1) mode.
 * Please see @ref power_state_t for details of power modes.
 * @return WM_SUCCESS on success
 * @return -WM_FAIL otherwise.
 */
int pm_init(void);

/** Check init status of the Power Manager module
 * This functions check the init status of Power Manager module.
 *
 * @return true if Power Manager module init is done.
 * @return false if not.
 */
bool is_pm_init_done();

/** Do a reboot of the chip
 *
 * This function forces a reboot of the chip, as if it were power-cycled.
 *
 * @note This function will not return as it will reboot the hardware.
 */
void pm_reboot_soc();

void arch_pm_init(void);
void arch_reboot();

/*
 *  Wake lock is used to control entry of core in low power mode
 */
/** Acquire WakeLock
 *
 * When the power manager framework is enabled in an application using a call to
 * pm_mc200_cfg(), the system opportunistically enters power save modes on
 * idle-time detection. Wakelocks are used to prevent the system from entering
 * such power save modes for certain sections of the code. The system will not
 * automatically enter the MC200 power save states if wakelock is being
 * held.
 *
 * Typically wakelocks have to be held only if any process/thread is waiting for
 * an external event or completion of asynchronous data transmission.
 * Systems continue to work properly for all other cases without acquiring
 * the wakelocks.
 *
 * @param[in] id_str This is a logical wakelock ID string used for debugging.
 * Logically correlated get/put should use the same string. This string is used
 * internally to track the individual logical wakelock counts.
 * @note This function should NOT be called in an interrupt context.
 * @return WM_SUCCESS on success
 * @return -WM_FAIL otherwise.
 */
int wakelock_get(const char *id_str);

/** Release WakeLock
 *
 * Use this function to release a wakelock previously acquired using
 * wakelock_get().The system enters MC200 power save states, only when all the
 * acquired wakelocks have been released.
 *
 * @note The number of wakelock_get() and wakelock_put() requests must exactly
 * match to ensure that the system can enter power save states.
 *
 * @param[in] id_str Logical ID string used for wakelock debugging. Please
 *               read the documentation for @ref wakelock_get.
 * @note This function should NOT be called in an interrupt context.
 * @return WM_SUCCESS  on success
 * @return -WM_FAIL otherwise.
 */
int wakelock_put(const char *id_str);

/** Check if WakeLock is held
 *
 * @return 0 if no one is holding a wake lock.
 * @return Non-zero otherwise.
 */
int wakelock_isheld();


/** Register Power-Mode Command Line Interface commands
 * @return  0  on success
 * @return  1  on failure
 * @note  This function is available to the application and can
 *        be directly called if the pm module is initialised.
 */
int pm_cli_init(void);

#endif /* _PWRMGR_H_*/
