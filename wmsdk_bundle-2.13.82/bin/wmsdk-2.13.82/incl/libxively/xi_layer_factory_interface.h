#ifndef __LAYER_FACTORY_INTERFACE_H__
#define __LAYER_FACTORY_INTERFACE_H__

#include "xi_layer.h"
#include "xi_macros.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \author  Olgierd Humenczuk
 * \file    layer_factory_interface.h
 */

/**
 * \struct  layer_factory_interface_t
 * \brief   The layer factory interface
 */
typedef struct
{
    /** \brief describes the type to create, mainly for sainity checks and nice error reporting */
    layer_type_id_t                  type_id_to_create;

    /** \brief enables the placement initialization which is separeted from the allocation / deallocation */
    layer_t* ( *placement_create ) ( layer_t* layer, void* user_data );

    /** \brief placement delete same as create but with oposite effect, may be used to clean and|or deallocate memory */
    layer_t* ( *placement_delete ) ( layer_t* layer );

    /** \brief strict layer allocation, may implement different strategies for allocation of the memory required for layer */
    layer_t* ( *alloc )            ( const layer_type_t* type );

    /** \brief strict deallocation of layer's memory */
    void ( *free )                 ( const layer_type_t* type
                                   , layer_t* layer );
} layer_factory_interface_t;

/**
 * \brief   placement_pass_create the simpliest version of the layer initialization function that does simply nothing
 * \param   layer
 * \return  the pointer to the initialized layer
 */
inline static layer_t* placement_layer_pass_create( layer_t* layer, void* user_data )
{
    layer->user_data = user_data;
    return layer;
}

/**
 * \brief   placement_pass_delete the simpliest version of the layer delete function that does simply nothing
 * \param   layer
 * \return  the pointer to the initialized layer
 */
inline static layer_t* placement_layer_pass_delete( layer_t* layer )
{
    return layer;
}

#ifdef __cplusplus
}
#endif

#endif // __LAYER_FACTORY_INTERFACE_H__
