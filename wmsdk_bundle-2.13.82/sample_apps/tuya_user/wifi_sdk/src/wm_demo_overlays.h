/*
 *  Copyright (C) 2008-2013, Marvell International Ltd.
 *  All Rights Reserved.
 */


#ifndef __WM_DEMO_OVERLAYS_H__
#define __WM_DEMO_OVERLAYS_H__

#if APPCONFIG_OVERLAY_ENABLE
void wm_demo_load_cloud_overlay();
void wm_demo_load_wps_overlay();

#else

static inline void wm_demo_load_cloud_overlay() {}
static inline void wm_demo_load_wps_overlay() {}

#endif   /* APPCONFIG_OVERLAY_ENABLE */

#endif   /* __WM_DEMO_OVERLAYS_H__ */

