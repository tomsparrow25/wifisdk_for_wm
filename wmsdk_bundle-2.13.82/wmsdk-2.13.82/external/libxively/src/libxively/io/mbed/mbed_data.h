// Copyright (c) 2003-2013, LogMeIn, Inc. All rights reserved.
// This is part of Xively C library, it is under the BSD 3-Clause license.

/**
 * \file    mbed_data.h
 * \author  Olgierd Humenczuk
 * \brief   Declares layer-specific data structure
 */

#ifndef __MBED_DATA_H__
#define __MBED_DATA_H__

#include "TCPSocketConnection.h"

typedef struct {
    TCPSocketConnection* socket_ptr;
} mbed_data_t;

#endif // __MBED_DATA_H__
