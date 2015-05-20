#ifndef __LAYER_HELPERS_H__
#define __LAYER_HELPERS_H__

#include "xi_common.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \brief layer_sender little helper to encapsulate
 *        error handling over sending data between layers
 */
static inline layer_state_t layer_sender(
          layer_connectivity_t* context
        , const char* const data
        , const layer_hint_t hint )
{
    const int len = strlen( (data) );
    const const_data_descriptor_t tmp_data = { (data), len, len, 0 };
    return CALL_ON_PREV_DATA_READY( context->self, ( const void* ) &tmp_data, hint );
}

/**
 * \brief wrapper around the layer_sender just some syntactic sugar on top
 */
#define send_through( context, data, hint ) \
    { \
        layer_state_t ret = LAYER_STATE_OK; \
        if( ( ret = layer_sender( context, data, hint ) ) != LAYER_STATE_OK ) { return ret; } \
    }

#ifdef __cplusplus
}
#endif

#endif // __LAYER_HELPERS_H__
