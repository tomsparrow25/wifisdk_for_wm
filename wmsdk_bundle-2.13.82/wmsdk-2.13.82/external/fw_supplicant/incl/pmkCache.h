#ifndef PMK_CACHE_H__
#define PMK_CACHE_H__

#include "wltypes.h"
#include "IEEE_types.h"
#include "pmkCache_rom.h"

extern char PSKPassPhrase[];

/*!
** \brief      If a matching SSID entry is present in the PMK Cache, returns a 
**             pointer to the PSK.  If no entry is found in the cache, a
**             new PSK entry will be generated if a PassPhrase is set.
** \param      pSsid pointer to string containing desired SSID.
** \param      ssidLen length of the SSID string *pSsid.
** \exception  Does not handle the case when multiple matching SSID entries are
**             found. Returns the first match.
** \return     pointer to PSK with matching SSID entry from PMK cache,
**             NULL if no matching entry found.
*/
extern UINT8 *pmkCacheFindPSK(UINT8 * pSsid, UINT8 ssidLen);

/*!
** \brief      Flushes all entries in PMK cache
*/
extern void pmkCacheFlush(void);

extern void pmkCacheGetPassphrase(char * pPassphrase);

extern void pmkCacheSetPassphrase(char * pPassphrase);
extern void pmkCacheInit(void);
extern void pmkCacheRomInit(void);

#endif
