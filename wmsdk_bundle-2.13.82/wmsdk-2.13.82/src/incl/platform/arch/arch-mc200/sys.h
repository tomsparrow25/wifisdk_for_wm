/*! \file sys.h
 *  \brief Architecture specific system include file
 *
 * Copyright (C) 2008-2012, Marvell International Ltd.
 * All Rights Reserved.
 */

#ifndef _WM_BOARD_SYS_H_
#define _WM_BOARD_SYS_H_

#include <stdint.h>


/** Initialize WLAN
 *
 * This function is responsible for initializing only wireless.
 *
 * \return Returns 0 on success, error otherwise.
 * */

int wm_wlan_init();


/** Initialize SDK core components plus WLAN
 *
 * This function is responsible for initializing core SDK components like
 * uart, console, flash and wireless.
 *
 * \return Returns 0 on success, error otherwise.
 * */
int wm_core_and_wlan_init(void);


#endif /* ! _WM_BOARD_SYS_H_ */
