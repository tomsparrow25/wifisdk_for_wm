/*
 *  Copyright (C) 2012, Marvell International Ltd.
 *  All Rights Reserved.
 */

#ifndef PERF_DEMO_MC2K_DRV_H_
#define PERF_DEMO_MC2K_DRV_H_

#ifndef CONFIG_WPS2
static inline int perf_demo_wps_pbc_init(void)
{
	return 0;
}

static inline void perf_demo_wps_pbc_clean(void)
{
}
#else
int perf_demo_wps_pbc_init(void);
void perf_demo_wps_pbc_clean(void);
#endif

static inline void perf_demo_init_backlight(void)
{
}
void perf_demo_set_led(unsigned int pin, unsigned int state);
int perf_demo_get_button_status(unsigned int pin);

#endif /* PERF_DEMO_MC2K_DRV_H_ */
