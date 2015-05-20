#ifndef AP_MLME_H_
#define AP_MLME_H_

#include "wltypes.h"
#include "IEEE_types.h"
#include "w81connmgr.h"
#include "macMgmtMain.h"
//#include "mlmeApi.h"

extern void InitializeAp(cm_ConnectionInfo_t *connPtr);
  
extern void ap_setpsk(cm_ConnectionInfo_t *connPtr, CHAR * ssid, CHAR *passphrase);

extern void ap_resetConfiguration(cm_ConnectionInfo_t *connPtr);

extern void ap_initConnection(cm_ConnectionInfo_t *connPtrCurr,
                              cm_ConnectionInfo_t *connPtrNew,
                              IEEEtypes_MacAddr_t *peerAddr);
                       
extern void ap_UpdateStaTrafficIndication(cm_ConnectionInfo_t *connPtr,
                                           UINT16 Aid, BOOLEAN Set);



extern void apMlmeResetStaAge(cm_ConnectionInfo_t *connPtr);

extern void apMlmeUpdateErpProtection(void *connPtr);
extern void apMlmeResetErpProtection(cm_ConnectionInfo_t *connPtr);
extern void apMlmeUpdateBarkerPreamble(cm_ConnectionInfo_t *connPtr);
extern void apMlmeResetBarkerPreamble(cm_ConnectionInfo_t *connPtr);
extern void apMlmeUpdateSlotSettings(cm_ConnectionInfo_t *connPtr);
extern void apMlmeResetSlotSettings(cm_ConnectionInfo_t *connPtr);

extern BOOLEAN apMlmeIsOFDMRatePresent(IEEEtypes_DataRate_t *pRates,
                                       IEEEtypes_Len_t len);
extern BOOLEAN apMlmeIsCCKRatePresent(IEEEtypes_DataRate_t *pRates,
                                      IEEEtypes_Len_t len);
extern void apMlmeUpdateHTProtection(void *connPtr);
extern void apMlmeResetAllActiveStaAge(void *connPtr);

#if 0
extern void apMlmeResetHTInfo(apInfo_t *pApInfo);
extern BOOLEAN apMlmeIsApActive(cm_ConnectionInfo_t *connPtr);
extern void ap_initAllApConnections(UINT8 conType);

extern void ap_reset_OpRateSet_11a(IEEEtypes_DataRate_t *pOpRateSet);
#ifdef DOT11AC
extern void apMlmeUpdateVhtOp(cm_ConnectionInfo_t *connPtr, 
                              BssConfig_t *pBssConfig);
#endif
#endif // DOT11AC


#endif

