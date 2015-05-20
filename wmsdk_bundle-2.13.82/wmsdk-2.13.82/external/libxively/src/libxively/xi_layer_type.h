#ifndef __LAYER_TYPE_H__
#define __LAYER_TYPE_H__

/**
 * \file    layer_type.h
 * \brief   containes definition of the layer type
 *
 */

#include "xi_layer_interface.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \brief   layer_type_id_t
 */
typedef unsigned char layer_type_id_t;

/**
 * \brief   layer type definition
 */
typedef struct
{
    layer_type_id_t     layer_type_id;
    layer_interface_t   layer_interface;
} layer_type_t;


#define LAYER_LOCAL_TYPE( type_name, layer_type_id, data_ready, on_read, on_close, close )\
    static layer_type_t type_name = { layer_type_id, { data_ready, on_read, on_close, close } }

#ifdef __cplusplus
}
#endif

#endif
