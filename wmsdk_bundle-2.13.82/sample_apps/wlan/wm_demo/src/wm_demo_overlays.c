/*
 *  Copyright (C) 2008-2013, Marvell International Ltd.
 *  All Rights Reserved.
 */

#include <wmstdio.h>
#include <overlays.h>

/* first overlay part is WPS */
#define OVERLAY_PART_WPS   0
/* second overlay part is cloud */
#define OVERLAY_PART_CLOUD 1

extern struct overlay ovl;


void wm_demo_load_cloud_overlay()
{
	overlay_load(&ovl, OVERLAY_PART_CLOUD);
}

void wm_demo_load_wps_overlay()
{
	overlay_load(&ovl, OVERLAY_PART_WPS);
}
