#ifndef __FACTORY_CONF_H__
#define __FACTORY_CONF_H__

/**
 *\file     layer_factory_conf.h
 *\author   Olgierd Humenczuk
 *\brief    containes macros that will be used to create the configuration of the layer factory
 *          policies
 */

#ifdef __cplusplus
extern "C" {
#endif

#include "xi_layer_factory_interface.h"

#define BEGIN_FACTORY_CONF() \
const layer_factory_interface_t FACTORY_ENTRIES[] = {

#define FACTORY_ENTRY( ttc, pc, pd, a, f ) \
    { ttc, pc, pd, a, f }

#define END_FACTORY_CONF() };

#ifdef __cplusplus
}
#endif

#endif // __FACTORY_CONF_H__
