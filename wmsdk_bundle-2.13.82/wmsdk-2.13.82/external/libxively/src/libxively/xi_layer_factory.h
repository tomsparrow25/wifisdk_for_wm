#ifndef __LAYER_FACTORY_H__
#define __LAYER_FACTORY_H__

/**
 *\file     layer_factory.h
 *\author   Olgierd Humenczuk
 *\brief    containes the layer factory implementation thanks to the policies that has been
 *          included within alloc and create pair of methods that can be fairly simple implementation
 */

#include "xi_layer.h"
#include "xi_layer_interface.h"
#include "xi_layer_factory_interface.h"

#ifdef __cplusplus
extern "C" {
#endif

extern const layer_factory_interface_t FACTORY_ENTRIES[ ];
extern const layer_type_t LAYER_TYPES[];

/**
 * \brief   create_layer
 * \param   layer_type
 * \return  pointer to the newly allocated layer instance
 */
static inline layer_t* alloc_layer( const layer_type_id_t layer_type_id )
{
    const layer_type_t* layer_type = &LAYER_TYPES[ layer_type_id ];
    return FACTORY_ENTRIES[ layer_type_id ].alloc( layer_type );
}

static inline void free_layer( layer_t* layer )
{
    const layer_type_t* layer_type = &LAYER_TYPES[ layer->layer_type_id ];
    return FACTORY_ENTRIES[ layer->layer_type_id ].free( layer_type, layer );
}

/**
 * \brief create_layer
 * \param layer
 * \param init_data
 * \return pointer to the newly created layer
 */
static inline layer_t* create_layer( layer_t* layer, void* user_data )
{
    // PRECONDITION
    assert( layer != 0 );

    return FACTORY_ENTRIES[ layer->layer_type_id ].placement_create( layer, user_data );
}

/**
 * \brief destroy_layer
 * \param layer
 * \return
 */
static inline void destroy_layer( layer_t* layer )
{
    // PRECONDITION
    assert( layer != 0 );

    FACTORY_ENTRIES[ layer->layer_type_id ].placement_delete( layer );
}

/**
 * \brief alloc_create_layer
 * \param layer_type_id
 * \param user_data
 * \return
 */
static inline layer_t* alloc_create_layer( const layer_type_id_t layer_type_id, void* user_data )
{
    layer_t* ret = alloc_layer( layer_type_id );

    XI_CHECK_MEMORY( ret );

    return create_layer( ret, user_data );

err_handling:
    return 0;
}

/**
 * \brief free_destroy_layer
 * \param layer
 */
static inline void free_destroy_layer( layer_t* layer )
{
    destroy_layer( layer );
    free_layer( layer );
}


#ifdef __cplusplus
}
#endif

#endif // __LAYER_FACTORY_H__
