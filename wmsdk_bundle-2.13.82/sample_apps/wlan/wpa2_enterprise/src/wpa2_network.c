/*
 * Copyright (C) 2008-2013, Marvell International Ltd.
 * All Rights Reserved.
 */
#include <wm_os.h>
#include <arch/sys.h>
#include <wm-tls.h>
#include <partition.h>
#include <appln_dbg.h>
#include <wlan.h>
#include <wm_net.h>

/* Certificates are provided from following header files.
 * Please modify these with your own certificates.
 */
#include "cacert.h"
#include "client-cert.h"
#include "client-key.h"
#include "nw-params.h"

char *nw_name = WPA2_NW_NAME;
char *nw_identity = WPA2_NW_IDENTITY;
char *nw_ssid = WPA2_NW_SSID;

/* Populate the WLAN Network structure with the details of the
 * network to connect to.
 */
int get_wpa2_nw(struct wlan_network *wpa2_network)
{
	memset(wpa2_network, 0x00, sizeof(struct wlan_network));

	/* Set profile name */
	strcpy(wpa2_network->name, (const char *)nw_name);
	/* Set Client Identity */
	strcpy(wpa2_network->identity, (const char *)nw_identity);
	/* Set SSID to desired WPA2 Enterprise network */
	strcpy(wpa2_network->ssid, (const char *)nw_ssid);
	/** Set SSID specific network search */
	wpa2_network->ssid_specific = 1;
	/* Set network mode infra */
	wpa2_network->type = WLAN_BSS_TYPE_STA;
	/* Set network mode infra */
	wpa2_network->role = WLAN_BSS_ROLE_STA;
	/* Set security to WPA2 Enterprise */
	wpa2_network->security.type = WLAN_SECURITY_EAP_TLS;
	/* Specify CA certificate */
	wpa2_network->security.ca_cert = ca_cert;
	/* Specify CA certificate size */
	wpa2_network->security.ca_cert_size = ca_cert_size;
	/* Specify Client certificate */
	wpa2_network->security.client_cert = client_cert;
	/* Specify Cleint certificate size */
	wpa2_network->security.client_cert_size = client_cert_size;
	/* Specify Client key */
	wpa2_network->security.client_key = client_key;
	/* Specify Client key size */
	wpa2_network->security.client_key_size = client_key_size;
	/* Specify address type as DHCP */
	wpa2_network->address.addr_type = ADDR_TYPE_DHCP;
	return 0;
}
