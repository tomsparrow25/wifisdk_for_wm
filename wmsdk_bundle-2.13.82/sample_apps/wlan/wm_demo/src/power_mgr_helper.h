/*
 *  Copyright (C) 2008-2014, Marvell International Ltd.
 *  All Rights Reserved.
 */

#ifndef _POWER_MGR_HELPER_H_
#define _POWER_MGR_HELPER_H_

#ifdef APPCONFIG_PM_ENABLE
bool hp_pm_get_wake_status(void);
void hp_pm_reset_wake_status(void);
void hp_pm_set_wake_status(void);
int hp_pm_init(void);
void hp_pm_wifi_ps_enable();
void hp_pm_wifi_ps_disable();
#else
static inline bool hp_pm_get_wake_status(void)
{
	return true;
}
static inline void hp_pm_reset_wake_status(void) {}
static inline void hp_pm_set_wake_status(void) {}
static inline int hp_pm_init(void)
{
	return WM_SUCCESS;
}

static inline void hp_pm_wifi_ps_enable() {}
static inline void hp_pm_wifi_ps_disable() {}

#endif /* APPCONFIG_PM_ENABLE */


#endif /* ! _POWER_MGR_HELPER_H_ */
