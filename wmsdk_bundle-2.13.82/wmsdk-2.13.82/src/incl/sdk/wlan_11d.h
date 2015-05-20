/*! \file wlan_11d.h
   \brief WLAN module 11d API


*/

#ifndef __WLAN_11D_H__
#define __WLAN_11D_H__

#include <wifi.h>

/**
 * @internal
 *
 * The information in the below arrays of sub bands is manually created
 * taking reference from the tables in mlan_cfp.c
 */

#ifdef CONFIG_11D
/**
   Sets the domain parameters for the uAP.

   @note Please ensure to call this API before wlan_start(). Ideally this API
   should be called on receiving \ref AF_EVT_WLAN_INIT_DONE before uAP is
   started or connection attempt is made using STA interface.

   To use this API you will need to fill up the structure
   \ref wifi_domain_param_t with correct parameters.

   The below section lists all the arrays that can be passed individually
   or in combination to the API \ref wlan_uap_set_domain_params(). These are
   the sub band sets to be part of the Country Info IE in the uAP beacon.
   One of them is to be selected according to your region. Please have a look
   at the example given in the documentation below for reference.

   Supported Country Codes:
   "US" : USA,
   "CA" : Canada,
   "SG" : Singapore,
   "EU" : Europe,
   "AU" : Australia,
   "KR" : Republic of Korea,
   "CN" : China,
   "FR" : France,
   "JP" : Japan

   \code

Region: US(US) or Canada(CA) or Singapore(SG) 2.4 GHz
wifi_sub_band_set_t subband_US_CA_SG_2_4_GHz[] = {
	{1, 11, 30}
};

Region: US(US) or Singapore(SG) 5 GHz
wifi_sub_band_set_t subband_US_SG_5GHz[] = {
	{36, 1, 20},
	{40, 1, 20},
	{44, 1, 20},
	{48, 1, 20},
	{52, 1, 20},
	{56, 1, 20},
	{60, 1, 20},
	{64, 1, 20},
	{100, 1, 20},
	{104, 1, 20},
	{108, 1, 20},
	{112, 1, 20},
	{116, 1, 20},
	{120, 1, 20},
	{124, 1, 20},
	{128, 1, 20},
	{132, 1, 20},
	{136, 1, 20},
	{140, 1, 20},
	{149, 1, 20},
	{153, 1, 20},
	{157, 1, 20},
	{161, 1, 20},
	{165, 1, 20}
};

Region: Canada(CA) 5 GHz
wifi_sub_band_set_t subband_CA_5GHz[] = {
	{36, 1, 20},
	{40, 1, 20},
	{44, 1, 20},
	{48, 1, 20},
	{52, 1, 20},
	{56, 1, 20},
	{60, 1, 20},
	{64, 1, 20},
	{100, 1, 20},
	{104, 1, 20},
	{108, 1, 20},
	{112, 1, 20},
	{116, 1, 20},
	{132, 1, 20},
	{136, 1, 20},
	{140, 1, 20},
	{149, 1, 20},
	{153, 1, 20},
	{157, 1, 20},
	{161, 1, 20},
	{165, 1, 20}
};

Region: Europe(EU), Australia(AU), Republic of Korea(KR),
China(CN) 2.4 GHz
wifi_sub_band_set_t subband_EU_AU_KR_CN_2_4GHz[] = {
	{1, 13, 20}
};

Region: Europe/ETSI(EU), Australia(AU), Republic of Korea(KR) 5 GHz
wifi_sub_band_set_t subband_EU_AU_KR_5GHz[] = {
	{36, 1, 20},
	{40, 1, 20},
	{44, 1, 20},
	{48, 1, 20},
	{52, 1, 20},
	{56, 1, 20},
	{60, 1, 20},
	{64, 1, 20},
	{100, 1, 20},
	{104, 1, 20},
	{108, 1, 20},
	{112, 1, 20},
	{116, 1, 20},
	{120, 1, 20},
	{124, 1, 20},
	{128, 1, 20},
	{132, 1, 20},
	{136, 1, 20},
	{140, 1, 20}
};

Region: China(CN) 5 GHz
wifi_sub_band_set_t subband_CN_5GHz[] = {
	{149, 1, 33},
	{153, 1, 33},
	{157, 1, 33},
	{161, 1, 33},
	{165, 1, 33},
};

Region: France(FR) 2.4 GHz
wifi_sub_band_set_t subband_FR_2_4GHz[] = {
	{1, 9, 20},
	{10, 4, 10}
};

Region: France(FR) 5 GHz
wifi_sub_band_set_t subband_FR_5GHz[] = {
	{36, 1, 20},
	{40, 1, 20},
	{44, 1, 20},
	{48, 1, 20},
	{52, 1, 20},
	{56, 1, 20},
	{60, 1, 20},
	{64, 1, 20},
	{100, 1, 20},
	{104, 1, 20},
	{108, 1, 20},
	{112, 1, 20},
	{116, 1, 20},
	{120, 1, 20},
	{124, 1, 20},
	{128, 1, 20},
	{132, 1, 20},
	{136, 1, 20},
	{140, 1, 20},
	{149, 1, 20},
	{153, 1, 20},
	{157, 1, 20},
	{161, 1, 20},
	{165, 1, 20}
};

Region: Japan(JP) 2.4 Ghz
wifi_sub_band_set_t subband_JP_2_4GHz[] = {
	{1, 14, 20}
};

Region: Japan(JP) 5 GHz
wifi_sub_band_set_t subband_JP_5_GHz[] = {
	{8, 1, 23},
	{12, 1, 23},
	{16, 1, 23},
	{36, 1, 23},
	{40, 1, 23},
	{44, 1, 23},
	{48, 1, 23},
	{52, 1, 23},
	{56, 1, 23},
	{60, 1, 23},
	{64, 1, 23},
	{100, 1, 23},
	{104, 1, 23},
	{108, 1, 23},
	{112, 1, 23},
	{116, 1, 23},
	{120, 1, 23},
	{124, 1, 23},
	{128, 1, 23},
	{132, 1, 23},
	{136, 1, 23},
	{140, 1, 23}
};

	// We will be using the KR 2.4 and 5 GHz bands for this example

	int nr_sb = (sizeof(subband_EU_AU_KR_CN_2_4GHz)
		     + sizeof(subband_EU_AU_KR_5GHz))
		     / sizeof(wifi_sub_band_set_t);

	// We already have space for first sub band info entry in
	// wifi_domain_param_t
	wifi_domain_param_t *dp =
		os_mem_alloc(sizeof(wifi_domain_param_t) +
			     (sizeof(wifi_sub_band_set_t) * (nr_sb - 1)));

	// COUNTRY_CODE_LEN is 3. Add extra ' ' as country code is 2 characters
	memcpy(dp->country_code, "KR ", COUNTRY_CODE_LEN);

	dp->no_of_sub_band = nr_sb;
	memcpy(&dp->sub_band[0], &subband_EU_AU_KR_CN_2_4GHz[0],
	       1 * sizeof(wifi_sub_band_set_t));
	memcpy(&dp->sub_band[1], &subband_EU_AU_KR_5GHz,
	       (nr_sb - 1) * sizeof(wifi_sub_band_set_t));

	wlan_uap_set_domain_params(dp);
	os_mem_free(dp);
\endcode

\param[in] dp The wifi domain parameters

\return -WM_E_INVAL if invalid parameters were passed.
\return -WM_E_NOMEM if memory allocation failed.
\return WM_SUCCESS if operation was successful.
 */
static inline int wlan_uap_set_domain_params(wifi_domain_param_t *dp)
{
	return wifi_uap_set_domain_params(dp);
}

/**
  Sets the domain parameters for the Station interface.

  @note Please ensure to call this API before wlan_start(). Ideally this API
  should be called on receiving \ref AF_EVT_WLAN_INIT_DONE before
  connection attempt is made using STA interface.

  @note Refer \ref wlan_uap_set_domain_params API documentation for
  \ref wifi_sub_band_set_t structures for different regions.

\code
// We will be using the KR 2.4 and 5 GHz bands for this example

int nr_sb = (sizeof(subband_EU_AU_KR_CN_2_4GHz)
+ sizeof(subband_EU_AU_KR_5GHz))
/ sizeof(wifi_sub_band_set_t);

// We already have space for first sub band info entry in
// wifi_domain_param_t
wifi_domain_param_t *dp =
os_mem_alloc(sizeof(wifi_domain_param_t) +
(sizeof(wifi_sub_band_set_t) * (nr_sb - 1)));

// COUNTRY_CODE_LEN is 3. Add extra ' ' as country code is 2 characters
memcpy(dp->country_code, "KR ", COUNTRY_CODE_LEN);

dp->no_of_sub_band = nr_sb;
memcpy(&dp->sub_band[0], &subband_EU_AU_KR_CN_2_4GHz[0],
1 * sizeof(wifi_sub_band_set_t));
memcpy(&dp->sub_band[1], &subband_EU_AU_KR_5GHz,
(nr_sb - 1) * sizeof(wifi_sub_band_set_t));

wlan_set_domain_params(dp);
os_mem_free(dp);
\endcode

\param[in] dp The wifi domain parameters

\return -WM_E_INVAL if invalid parameters were passed.
\return WM_SUCCESS if operation was successful.
*/
static inline int wlan_set_domain_params(wifi_domain_param_t *dp)
{
	return wifi_set_domain_params(dp);
}

/**
 Enable/Disable 11D support in WLAN firmware.

\param[in] enable Enable/Disable 11D support.

\return -WM_FAIL if operation was failed.
\return WM_SUCCESS if operation was successful.
 */
static inline int wlan_set_11d(bool enable)
{
	return wifi_set_11d(enable);
}

/**
 Set 11D region code.

\param[in] region_code 11D region code to set.

\return -WM_FAIL if operation was failed.
\return WM_SUCCESS if operation was successful.
 */
static inline int wlan_set_region_code(uint32_t region_code)
{
	return wifi_set_region_code(region_code);
}
#endif /* CONFIG_11D */

#endif /* __WLAN_11D_H__ */
