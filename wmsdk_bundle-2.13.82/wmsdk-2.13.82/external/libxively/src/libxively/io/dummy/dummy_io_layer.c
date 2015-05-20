// Copyright (c) 2003-2013, LogMeIn, Inc. All rights reserved.
// This is part of Xively C library, it is under the BSD 3-Clause license.

/**
 * \file    dummy_io_layer.c
 * \author  Olgierd Humenczuk
 * \brief   Implements DUMMY _io layer_ abstraction interface [see layer.h]
 */
// c
#include <stdio.h>
#include <string.h>
#include <stdint.h>

// local
#include "dummy_io_layer.h"
#include "dummy_data.h"
#include "xi_allocator.h"
#include "xi_err.h"
#include "xi_macros.h"
#include "xi_debug.h"

#include "xi_layer_api.h"
#include "xi_common.h"

layer_state_t dummy_io_layer_data_ready(
      layer_connectivity_t* context
    , const void* data
    , const layer_hint_t hint )
{
    XI_UNUSED( context );
    XI_UNUSED( data );
    XI_UNUSED( hint );

    return LAYER_STATE_OK;
}

layer_state_t dummy_io_layer_on_data_ready(
      layer_connectivity_t* context
    , const void* data
    , const layer_hint_t hint )
{
    XI_UNUSED( context );
    XI_UNUSED( data );
    XI_UNUSED( hint );

    return LAYER_STATE_OK;
}

layer_state_t dummy_io_layer_close( layer_connectivity_t* context )
{
    XI_UNUSED( context );

    return LAYER_STATE_OK;
}

layer_state_t dummy_io_layer_on_close( layer_connectivity_t* context )
{
    XI_UNUSED( context );

    return LAYER_STATE_OK;
}

layer_t* connect_to_endpoint(
      layer_t* layer
    , const char* address
    , const int port )
{
    XI_UNUSED( layer );
    XI_UNUSED( address );
    XI_UNUSED( port );

    return layer;
}
