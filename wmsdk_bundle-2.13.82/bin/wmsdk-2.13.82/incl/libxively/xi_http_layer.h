#ifndef __HTTP_LAYER_H__
#define __HTTP_LAYER_H__

// local
#include "xi_layer.h"
#include "xi_http_layer_input.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \file   http_layer.h
 * \author Olgierd Humenczuk
 * \brief  File that containes the declaration of the http layer functions
 */

/**
 * \brief   see the layer_interface for details
 */
layer_state_t http_layer_data_ready(
      layer_connectivity_t* context
    , const void* data
    , const layer_hint_t hint );

/**
 * \brief  see the layer_interface for details
 */
layer_state_t http_layer_on_data_ready(
      layer_connectivity_t* context
    , const void* data
    , const layer_hint_t hint );

/**
 * \brief  see the layer_interface for details
 */
layer_state_t http_layer_close(
    layer_connectivity_t* context );

/**
 * \brief  see the layer_interface for details
 */
layer_state_t http_layer_on_close(
    layer_connectivity_t* context );


/**
 * \brief connect_to_endpoint
 * \param layer
 * \return
 */
layer_t* init_http_layer(
      layer_t* layer );

const void* http_layer_data_generator_datastream_get(
          const void* input
        , short* state );

#ifdef __cplusplus
}
#endif

#endif // __HTTP_LAYER_H__
