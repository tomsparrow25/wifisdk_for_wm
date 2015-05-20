#ifndef __LAYER_CONNECTION_H__
#define __LAYER_CONNECTION_H__

/**
 * \file    layer_connection.h
 * \author  Olgierd Humenczuk
 * \brief   containes datastructures and functions required via the connection functionality
 *          of the layer system so that the layers connection can be pre - defined
 */

// local
#include "xi_macros.h"
#include "xi_debug.h"
#include "xi_layer_api.h"
#include "xi_layer.h"
#include "xi_layer_type.h"
#include "xi_layer_factory.h"

#ifdef __cplusplus
extern "C" {
#endif

#define SIZE_SUFFIX _SIZE

/**
 * \brief describes the connected layers endpoints, so that it's possible to refer to the
 *          each of the endpoint separately
 */
typedef struct
{
    layer_t* bottom;
    layer_t* top;
} layer_chain_t;

/**
 * \brief macro that creates the layer connection scheme
 */
#define DEFINE_CONNECTION_SCHEME( name, args ) \
    static layer_type_id_t name[]       = { args }; \
    static size_t name##SIZE_SUFFIX     = sizeof( name ) / sizeof( layer_type_id_t )

/**
 * \brief helper to extract the defined size of the connection scheme
 *        may be helpfull within the for loop
 */
#define CONNECTION_SCHEME_LENGTH( name ) name##SIZE_SUFFIX

/**
 * \brief   connector
 * \param   connections
 * \return  pointer to the first connected layer
 */
static inline layer_chain_t connect_layers( layer_t* layers[], const size_t length )
{
    size_t i = 1;
    XI_UNUSED( layers );
    assert( length >= 2 && "you have to connect at least two layers to each other" );

    // connecto the layers in a chain
    for( i = 1; i < length; ++i )
    {
        CONNECT_LAYERS( layers[ i - 1 ], layers[ i ] );
    }

    layer_chain_t ret;

    ret.bottom  = layers[ 0 ];
    ret.top     = layers[ length -1 ];

    return ret;
}

/**
 * \brief create_and_connect_layers
 * \param layers_ids
 * \param user_datas
 * \return
 */
static inline layer_chain_t create_and_connect_layers(
          const layer_type_id_t layers_ids[]
        , void* user_datas[]
        , const size_t length )
{
    size_t i = 0;
    layer_t* layers[ length ];
    memset( layers, 0, sizeof( layers ) );

    for( i = 0; i < length; ++i )
    {
        layers[ i ] = alloc_create_layer( layers_ids[ i ], user_datas[ i ] );
    }

    return connect_layers( layers, length );
}

static inline void destroy_and_disconnect_layers( layer_chain_t* chain, const size_t length )
{
    size_t j = 0;
    layer_t* layers[ length ];
    memset( layers, 0, sizeof( layers ) );

    assert( chain != 0 && "layer chain must not be 0!" );
    assert( chain->bottom->layer_connection.next != 0 && "layer chain must have at least 2 elements!" );

    layer_t* prev   = chain->bottom;
    layer_t* tmp    = prev->layer_connection.next;

    unsigned char i = 0;
    layers[ i ]     = prev;

    // disconnect layers
    while( tmp )
    {
        DISCONNECT_LAYERS( prev, tmp );

        prev          = tmp;
        tmp           = tmp->layer_connection.next;

        layers[ ++i ] = prev;
    }

    // free and delete
    for( j = 0; j < length; ++j )
    {
        free_destroy_layer( layers[ j ] );
    }
}

#ifdef __cplusplus
}
#endif

#endif // __LAYER_CONNECTION_H__
