#ifndef PMK_CACHE_ROM_H__
#define PMK_CACHE_ROM_H__

#include "wltypes.h"
#include "IEEE_types.h"

#define PSK_PASS_PHRASE_LEN_MAX 64
#define PMK_LEN_MAX  32
#define MAX_PMK_SIZE 32

typedef struct
{
    union 
    {
        IEEEtypes_MacAddr_t Bssid;
        char                Ssid[32];
    } key;
    UINT8 PMK[MAX_PMK_SIZE];            /* PMK / PSK */
    UINT8 length;
    SINT8 replacementRank;
} pmkElement_t;


/*!
** \brief      Finds a PMK matching a given BSSID
** \param      pBssid pointer to the desired BSSID
** \return     pointer to the matching PMK.
**             NULL, if no matching PMK entry is found
*/
extern UINT8 * pmkCacheFindPMK(IEEEtypes_MacAddr_t * pBssid);

extern BOOLEAN (*pmkCacheFindPSKElement_hook)(UINT8 * pSsid,
                                              UINT8 ssidLen,
                                              pmkElement_t ** ptr_val);
extern pmkElement_t * pmkCacheFindPSKElement(UINT8 * pSsid,
                                             UINT8 ssidLen);

/*!
** \brief      adds a new PMK entry to PMK cache.
** \param      pBssid pointer to Bssid for which to add the PMK
** \param      pPMK pointer to PMK data
*/
extern BOOLEAN (*pmkCacheAddPMK_hook)(IEEEtypes_MacAddr_t * pBssid,
                                      UINT8 *pPMK);
extern void pmkCacheAddPMK(IEEEtypes_MacAddr_t * pBssid,
                           UINT8 *pPMK);

/*!
** \brief      Adds a new PSK to PMK cache. 
** \param      pSsid pointer to desired SSID for which to add the PSK entry.
** \param      ssidLen length of the SSID string.
** \param      pPSK pointer to PSK to store.
*/
extern BOOLEAN (*pmkCacheAddPSK_hook)(UINT8 * pSsid, 
                                      UINT8 ssidLen, 
                                      UINT8 * pPSK);
extern void pmkCacheAddPSK(UINT8 * pSsid, 
                           UINT8 ssidLen, 
                           UINT8 * pPSK);

/*!
** \brief      Delete a particular PMK entry from PMK cache.
** \param      pBssid pointer to BSSID that needs to be deleted
*/
extern void pmkCacheDeletePMK(IEEEtypes_MacAddr_t * pBssid);

/*!
** \brief      delete a particular PSK entry from PMK cache
** \param      Ssid pointer to SSID that needs to be deleted
** \param      ssidLen length of the string pointed to by Ssid
*/
extern void pmkCacheDeletePSK(UINT8 *ssid, UINT8 ssidLen);

extern BOOLEAN (*pmkCacheGeneratePSK_hook)(UINT8 * pSsid,
                                           UINT8   ssidLen,
                                           char  * pPassphrase,
                                           UINT8 * pPSK);
extern void pmkCacheGeneratePSK(UINT8 * pSsid,
                                UINT8   ssidLen,
                                char  * pPassphrase,
                                UINT8 * pPSK);

extern BOOLEAN (*pmkCacheNewElement_hook)(pmkElement_t ** ptr_val);
extern pmkElement_t * pmkCacheNewElement(void);

extern BOOLEAN (*pmkCacheFindPMKElement_hook)(IEEEtypes_MacAddr_t * pBssid,
                                              pmkElement_t ** ptr_val);
extern pmkElement_t * pmkCacheFindPMKElement(IEEEtypes_MacAddr_t * pBssid);

extern void pmkCacheUpdateReplacementRank(pmkElement_t * pPMKElement);

extern SINT8  replacementRankMax;

/* ROM linkages */
extern SINT32 ramHook_MAX_PMK_CACHE_ENTRIES;
extern pmkElement_t * ramHook_pmkCache;
extern char         * ramHook_PSKPassPhrase;

extern void (*ramHook_hal_SetCpuMaxSpeed)(void);
extern void (*ramHook_hal_RestoreCpuSpeed)(void);

#endif
