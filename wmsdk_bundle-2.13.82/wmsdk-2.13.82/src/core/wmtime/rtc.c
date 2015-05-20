/*
 * Copyright (C) 2008-2013, Marvell International Ltd.
 * All Rights Reserved.
 */
#include <wmtime.h>
#include <rtc.h>
#include <arch/boot_flags.h>

/*
 * Abstraction layer calls to switch between hardware or software rtc
 * depending on chip revision ID of 88MC200 -- currently BootROM version
 * number.
 */
void rtc_init(void)
{
#ifdef CONFIG_HW_RTC
	struct bootrom_info *info = (struct bootrom_info *)&_nvram_start;
	if (info->version >= BOOTROM_A0_VERSION)
		hwrtc_init();
	else
#endif
		swrtc_init();
}

int rtc_time_set(time_t time)
{
#ifdef CONFIG_HW_RTC
	struct bootrom_info *info = (struct bootrom_info *)&_nvram_start;
	if (info->version >= BOOTROM_A0_VERSION)
		return hwrtc_time_set(time);
	else
#endif
		return swrtc_time_set(time);
}

time_t rtc_time_get(void)
{
#ifdef CONFIG_HW_RTC
	struct bootrom_info *info = (struct bootrom_info *)&_nvram_start;
	if (info->version >= BOOTROM_A0_VERSION)
		return hwrtc_time_get();
	else
#endif
		return swrtc_time_get();
}
