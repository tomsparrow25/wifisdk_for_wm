// Copyright (c) 2003-2013, LogMeIn, Inc. All rights reserved.
// This is part of Xively C library, it is under the BSD 3-Clause license.

/**
 * \file    mbed_comm.h
 * \author  Olgierd Humenczuk
 * \brief   Implements mbed _communication layer_ functions [see comm_layer.h and mbed_comm.cpp]
 */

#ifndef __MBED_COMM_H__
#define __MBED_COMM_H__

// local
#include "xi_layer.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \file   mbed_io_layer.h
 * \author Olgierd Humenczuk
 * \brief  File that contains the declaration of the mbed io layer functions
 */

/**
 * \brief  see the layer_interface for details
 */
layer_state_t mbed_io_layer_data_ready(
      layer_connectivity_t* context
    , const void* data
    , const layer_hint_t hint );

/**
 * \brief  see the layer_interface for details
 */
layer_state_t mbed_io_layer_on_data_ready(
      layer_connectivity_t* context
    , const void* data
    , const layer_hint_t hint );

/**
 * \brief  see the layer_interface for details
 */
layer_state_t mbed_io_layer_close(
    layer_connectivity_t* context );

/**
 * \brief  see the layer_interface for details
 */
layer_state_t mbed_io_layer_on_close(
    layer_connectivity_t* context );


/**
 * \brief connect_to_endpoint
 * \param layer
 * \param init_data
 * \return
 */
layer_t* connect_to_endpoint(
      layer_t* layer
    , const char* address
    , const int port );

#ifdef __cplusplus
}
#endif

#endif // __MBED_COMM_H__
