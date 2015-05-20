/*
 *  Copyright (C) 2008-2013, Marvell International Ltd.
 *  All Rights Reserved.
 */

#ifndef MDNS_HELPER_H_
#define MDNS_HELPER_H_

/* mDNS */
#ifdef APPCONFIG_MDNS_ENABLE
void hp_mdns_announce(void *iface, int state);
void hp_mdns_deannounce(void *iface);
void hp_mdns_down_up(void *iface);
#else
static inline void hp_mdns_down_up(void *iface)
{}

static inline void hp_mdns_announce(void *iface, int state)
{}
#endif   /* APPCONFIG_MDNS_ENABLE */

#endif /* ! _HELPERS_H_ */
