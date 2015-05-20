#ifndef __POSIX_IO_LAYER_H__
#define __POSIX_IO_LAYER_H__

// local
#include "xi_layer.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \file   posix_io_layer.h
 * \author Olgierd Humenczuk
 * \brief  File that contains the declaration of the posix io layer functions
 */

/**
 * \brief  see the layer_interface for details
 */
layer_state_t posix_io_layer_data_ready(
      layer_connectivity_t* context
    , const void* data
    , const layer_hint_t hint );

/**
 * \brief  see the layer_interface for details
 */
layer_state_t posix_io_layer_on_data_ready(
      layer_connectivity_t* context
    , const void* data
    , const layer_hint_t hint );

/**
 * \brief  see the layer_interface for details
 */
layer_state_t posix_io_layer_close(
    layer_connectivity_t* context );

/**
 * \brief  see the layer_interface for details
 */
layer_state_t posix_io_layer_on_close(
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

#endif
