#ifndef __LAYER_API_H__
#define __LAYER_API_H__

#include "xi_layer.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifdef XI_DEBUG_LAYER_API
#define LAYER_LOCAL_INSTANCE( layer_name_instance, layer_interface, layer_type_id, user_data )\
    layer_t layer_name_instance                     = { layer_interface, { 0, 0, 0 }, layer_type_id, ( void* ) user_data, { 0, 0, 0, 0 }, { 0, 0, 0, 0, 0, 0 } };\
    INIT_INSTANCE( layer_name_instance )
#define INIT_INSTANCE( instance )\
    instance.layer_connection.self                  = &instance;\
    instance.debug_info.debug_line_init             = __LINE__;\
    instance.debug_info.debug_file_init             = __FILE__;
#else
#define LAYER_LOCAL_INSTANCE( layer_name_instance, layer_interface, layer_type_id, user_data )\
    layer_t   layer_name_instance                   = { layer_interface, { 0, 0, 0 }, layer_type_id, ( void* ) user_data, { 0, 0, 0, 0 } };\
    INIT_INSTANCE( layer_name_instance )
#define INIT_INSTANCE( instance )\
    instance.layer_connection.self                  = &instance;
#endif

#ifdef XI_DEBUG_LAYER_API
#define CONNECT_LAYERS( lp_i, ln_i )\
    ln_i->layer_connection.prev             = lp_i;\
    lp_i->layer_connection.next             = ln_i;\
    lp_i->debug_info.debug_line_connect     = __LINE__;\
    lp_i->debug_info.debug_file_connect     = __FILE__;\
    ln_i->debug_info.debug_line_connect     = __LINE__;\
    ln_i->debug_info.debug_file_connect     = __FILE__;
#else
#define CONNECT_LAYERS( lp_i, ln_i )\
    ln_i->layer_connection.prev  = lp_i;\
    lp_i->layer_connection.next  = ln_i
#endif

#ifdef XI_DEBUG_LAYER_API
#define DISCONNECT_LAYERS( lp_i, ln_i )\
    ln_i->layer_connection.prev             = 0;\
    lp_i->layer_connection.next             = 0;\
    lp_i->debug_info.debug_line_connect     = __LINE__;\
    lp_i->debug_info.debug_file_connect     = __FILE__;\
    ln_i->debug_info.debug_line_connect     = __LINE__;\
    ln_i->debug_info.debug_file_connect     = __FILE__;
#else
#define DISCONNECT_LAYERS( lp_i, ln_i )\
    ln_i->layer_connection.prev  = 0;\
    lp_i->layer_connection.next  = 0
#endif


#define LAYER_GET_CONTEXT_PTR( instance )\
    &instance->layer_connection

#ifdef XI_DEBUG_LAYER_API
#define SET_DEBUG_INFO_ON( layer, context )\
    context->layer_connection.layer->debug_info.debug_line_last_call = __LINE__;\
    context->layer_connection.layer->debug_info.debug_file_last_call = __FILE__;
#endif

#ifdef XI_DEBUG_LAYER_API
#define CALL_ON( layer, target, context )\
    context->layer_connection.layer->layer_functions->target( &context->layer_connection.layer->layer_connection );\
    SET_DEBUG_INFO_ON( layer, context );
#define CALL_ON2( layer, target, context, data, impulse )\
    context->layer_connection.layer->layer_functions->target( &context->layer_connection.layer->layer_connection, data, impulse );\
    SET_DEBUG_INFO_ON( layer, context )
#else
#define CALL_ON( layer, target, context )\
    context->layer_connection.layer->layer_functions->target( &context->layer_connection.layer->layer_connection );
#define CALL_ON2( layer, target, context, data, impulse )\
    context->layer_connection.layer->layer_functions->target( &context->layer_connection.layer->layer_connection, data, impulse )
#endif

// ON_DEMAND
#define CALL_ON_SELF_DATA_READY( context, data, impulse )\
    CALL_ON2( self, data_ready, context, data, impulse )

#define CALL_ON_NEXT_DATA_READY( context, data, impulse )\
    CALL_ON2( next, data_ready, context, data, impulse )

#define CALL_ON_PREV_DATA_READY( context, data, impulse )\
    CALL_ON2( prev, data_ready, context, data, impulse )

// ON_DATA_READY
#define CALL_ON_SELF_ON_DATA_READY( context, data, impulse )\
    CALL_ON2( self, on_data_ready, context, data, impulse )

#define CALL_ON_NEXT_ON_DATA_READY( context, data, impulse )\
    CALL_ON2( next, on_data_ready, context, data, impulse )

#define CALL_ON_PREV_ON_DATA_READY( context, data, impulse )\
    CALL_ON2( prev, on_data_ready, context, data, impulse )

// CLOSE
#define CALL_ON_SELF_CLOSE( context )\
    CALL_ON( self, close, context )

#define CALL_ON_NEXT_CLOSE( context )\
    CALL_ON( next, close, context )

#define CALL_ON_PREV_CLOSE( context )\
    CALL_ON( prev, close, context )

// ON_CLOSE
#define CALL_ON_SELF_ON_CLOSE( context )\
    CALL_ON( self, on_close, context )

#define CALL_ON_NEXT_ON_CLOSE( context )\
    CALL_ON( next, on_close, context )

#define CALL_ON_PREV_ON_CLOSE( context )\
    CALL_ON( prev, on_close, context )

typedef enum
{
      FUNCTION_ID_ON_DATA_READY = 0
    , FUNCTION_ID_DATA_READY
    , FUNCTION_ID_ON_CLOSE
    , FUNCTION_ID_CLOSE
} layer_api_function_id_t;

#ifdef __cplusplus
}
#endif

#endif // __LAYER_API_H__
