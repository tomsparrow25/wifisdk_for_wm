
#ifndef _SUPPLICANT_H
#define _SUPPLICANT_H

#include "keyMgmtCommon.h"
#include "IEEE_types.h"
#include "bufferMgmtLib.h"

extern UINT8 ProcessEAPoLPkt(BufferDesc_t *bufDesc, 
                             IEEEtypes_MacAddr_t* sa,
                             IEEEtypes_MacAddr_t* da);

#ifdef HOST_SLEEP_MODE
extern BOOLEAN Is_EAPOL_inSleep( BufferDesc_t *bufDesc, RxPD_t *rxPd_p);
#endif

#endif
