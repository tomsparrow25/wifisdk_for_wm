
#ifndef _KEYMGMTAPTYPES_H_
#define _KEYMGMTAPTYPES_H_

#include "wltypes.h"
#include "IEEE_types.h"
//#include "timer.h"
#include "keyMgmtStaHostTypes.h"
#include "keyMgmtStaTypes.h"
#if 0
#include "microTimer_rom.h"
#endif
#include "keyMgmtApTypes_rom.h"

typedef enum
{
    STA_ASSO_EVT,
    MSGRECVD_EVT,
    KEYMGMTTIMEOUT_EVT,
    GRPKEYTIMEOUT_EVT,
    UPDATEKEYS_EVT
} keyMgmt_HskEvent_e;

/* Fields till keyMgmtState are being accessed in rom code and
  * should be kept intact. Fields after keyMgmtState can be changed
  * safely.
  */
typedef struct
{
    apKeyMgmtInfoStaRom_t rom;
    UINT8  numHskTries;
    UINT32 counterLo;       
    UINT32 counterHi;
    os_timer_t HskTimer;
    UINT8 EAPOL_MIC_Key[EAPOL_MIC_KEY_SIZE];
    UINT8 EAPOL_Encr_Key[EAPOL_ENCR_KEY_SIZE];
    UINT8 EAPOLProtoVersion;
    UINT8 rsvd[3];
} apKeyMgmtInfoSta_t;

/*  Convert an Ascii character to a hex nibble
    e.g. Input is 'b' : Output will be 0xb
         Input is 'E' : Output will be 0xE
         Input is '8' : Output will be 0x8
    Assumption is that input is a-f or A-F or 0-9 
*/
#define ASCII2HEX(Asc) (((Asc) >= 'a') ? (Asc - 'a' + 0xA)\
    : ( (Asc) >= 'A' ? ( (Asc) - 'A' + 0xA ) : ((Asc) - '0') ))

#define ETH_P_EAPOL 0x8E88

#endif //_KEYMGMTAPTYPES_H_
