#ifndef KEYMGMTSTAHOSTTYPES_ROM_H__
#define KEYMGMTSTAHOSTTYPES_ROM_H__

#include "wltypes.h"

typedef struct
{
    UINT8  Key[TK_SIZE];
    UINT8  RxMICKey[8];
    UINT8  TxMICKey[8];
    UINT32 TxIV32;
    UINT16 TxIV16;
    UINT16 KeyIndex;
} KeyData_t;

#endif
