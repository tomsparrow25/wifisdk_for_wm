/*
 *  Copyright 2008-2013, Marvell International Ltd.
 *  All Rights Reserved.
 */

#ifndef RTC_H
#define RTC_H

#include <wmtime.h>

extern void rtc_init(void);
extern int rtc_time_set(time_t time);
extern time_t rtc_time_get(void);

#ifdef CONFIG_HW_RTC

extern void hwrtc_init(void);
extern int hwrtc_time_set(time_t time);
extern time_t hwrtc_time_get(void);
extern void hwrtc_time_update(void);
#endif /* CONFIG_HW_RTC */

extern void swrtc_init(void);
extern int swrtc_time_set(time_t time);
extern time_t swrtc_time_get(void);

/** Time stored in POSIX format (Seconds since epoch) */
extern time_t cur_posix_time;

/** Absolute number of ticks */
extern unsigned int abs_tick_count;
#endif
