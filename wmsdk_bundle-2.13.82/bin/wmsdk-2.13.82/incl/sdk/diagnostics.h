/*
 * Copyright 2008-2013 Marvell International Ltd.
 * All Rights Reserved.
 */

#ifndef __DIAGNOSTICS_H__
#define __DIAGNOSTICS_H__

#include <json.h>
#include <wmstats.h>
#include <wmerrno.h>

int diagnostics_read_stats(struct json_str *jptr);
int diagnostics_write_stats();
int diagnostics_read_stats_psm(struct json_str *jptr);
void diagnostics_set_reboot_reason(wm_reboot_reason_t reason);
#endif
