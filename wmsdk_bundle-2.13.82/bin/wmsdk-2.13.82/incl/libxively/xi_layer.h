#ifndef __LAYER_H__
#define __LAYER_H__

/**
 * \file    layer.h
 * \author  Olgierd Humenczuk
 * \brief   the layer type definition used across the whole layer system
 */

#ifdef __cplusplus
extern "C" {
#endif

// local
#include "xi_layer_connectivity.h"
#include "xi_layer_debug_info.h"
#include "xi_layer_interface.h"
#include "xi_layer_type.h"

/**
 * \brief The layer struct that makes the access to the generated types possible
 */
typedef struct layer
{
    const layer_interface_t*    layer_functions;
    layer_connectivity_t        layer_connection;
    layer_type_id_t             layer_type_id;
    void*                       user_data;
    short                       layer_states[ 4 ];
#ifdef XI_DEBUG_LAYER_API
    layer_debug_info_t          debug_info;
#endif
} layer_t;


#ifdef __cplusplus
}
#endif

#endif // __LAYER_H__
