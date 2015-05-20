/** @file  dh.h 
  * @brief This file contains definition for Diffie-Hellman Key
  * 
  * Copyright (C) 2003-2008, Marvell International Ltd.
  * All Rights Reserved
  */

#ifndef _DH_H
#define _DH_H

#include "type_def.h"
#include "encrypt.h"

int setup_dh_agreement(u8 *, u32, u8 *, u32, DH_PG_PARAMS *);
int compute_dh_agreed_key(u8 *, u8 *, u32, u8 *, u32, DH_PG_PARAMS *);

#endif /* _DH_H */
