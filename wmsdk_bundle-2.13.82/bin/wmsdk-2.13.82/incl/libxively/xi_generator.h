#ifndef __XI_GENERATOR_H__
#define __XI_GENERATOR_H__

#include <string.h>

#include "xi_coroutine.h"
#include "xi_common.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \brief  this is the generator abstract interface definition
 *
 * \param   input the input data, the generator will know how to interpret that
 * \param   curr_state returned through the pointer, this is where generator will return and save it's state
 *              so that the communication can be done throught that value
 * \warning do not change the curr_state from outside, unless you know what youre doing!
 * \return  abstract value that interpreted by caller of generator
 */
typedef const void* ( xi_generator_t )( const void* input, short* curr_state );

// there can be only one descriptor cause it's not needed after single use
extern const_data_descriptor_t __xi_tmp_desc;

// holds the len only and it's used to make difference between normal functions
// and generator once
#define ENABLE_GENERATOR() \
    unsigned char __xi_len = 0; \
    static short __xi_gen_sub_state = 0; \
    const const_data_descriptor_t* __xi_gen_data = 0; \
    ( void )( __xi_len ); \
    ( void )( __xi_gen_sub_state ); \
    ( void )( __xi_gen_data );

// couple of macros
#define gen_ptr_text( state, ptr_text ) \
{ \
    __xi_len = strlen( ptr_text ); \
    __xi_tmp_desc.data_ptr = ptr_text; \
    __xi_tmp_desc.data_size = __xi_len; \
    __xi_tmp_desc.real_size = __xi_len; \
    YIELD( state, ( void* ) &__xi_tmp_desc ); \
}

#define gen_ptr_text_and_exit( state, ptr_text ) \
{ \
    __xi_len = strlen( ptr_text ); \
    __xi_tmp_desc.data_ptr  = ptr_text; \
    __xi_tmp_desc.data_size = __xi_len; \
    __xi_tmp_desc.real_size = __xi_len; \
    EXIT( state, ( void* ) &__xi_tmp_desc ); \
}

// couple of macros
#define gen_static_text( state, text ) \
{ \
    static const char* const tmp_str = text; \
    __xi_tmp_desc.data_ptr  = tmp_str; \
    __xi_tmp_desc.real_size = __xi_tmp_desc.data_size = sizeof( text ) - 1; \
    YIELD( state, ( void* ) &__xi_tmp_desc ); \
}

#define call_sub_gen( state, input, sub_gen ) \
{ \
    __xi_gen_sub_state = 0; \
    while( __xi_gen_sub_state != 1 ) \
    { \
        YIELD( state, sub_gen( input, &__xi_gen_sub_state) ); \
    } \
}

#define call_sub_gen_and_exit( state, input, sub_gen ) \
{ \
    __xi_gen_sub_state = 0; \
    while( __xi_gen_sub_state != 1 ) \
    { \
        __xi_gen_data = sub_gen( input, &__xi_gen_sub_state ); \
        if( __xi_gen_sub_state != 1 ) \
        { \
            YIELD( state, __xi_gen_data ); \
        } \
        else \
        { \
            EXIT( state, __xi_gen_data ); \
        } \
    } \
}

#define gen_static_text_and_exit( state, text ) \
{ \
    static const char* const tmp_str = text; \
    __xi_tmp_desc.data_ptr = tmp_str; \
    __xi_tmp_desc.real_size = __xi_tmp_desc.data_size = sizeof( text ) - 1; \
    EXIT( state, ( void* ) &__xi_tmp_desc ); \
}

#ifdef __cplusplus
}
#endif

#endif // __XI_GENERATOR_H__
