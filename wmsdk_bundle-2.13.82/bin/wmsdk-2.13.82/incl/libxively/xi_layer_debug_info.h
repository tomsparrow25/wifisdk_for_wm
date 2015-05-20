#ifndef __LAYER_DEBUG_INFO_H__
#define __LAYER_DEBUG_INFO_H__

/**
 * \file    layer_debug_info.h
 * \author  Olgierd Humenczuk
 * \brief   the debug info of the layer system used to remember the file and line of each operation
 *          so that it's easier to track where the layer has been created and by whom
 */

#ifdef __cplusplus
extern "C" {
#endif

#ifdef XI_DEBUG_LAYER_API
/**
 * \struct  layer_debug_info
 * \brief   The structure holds information related to the layer debugging. Here the original place of initialization
 *          or connection is being stored so it's easier to determine what layer is it.
 */
typedef struct layer_debug_info
{
    int                     debug_line_init;
    const char*             debug_file_init;

    int                     debug_line_connect;
    const char*             debug_file_connect;

    int                     debug_line_last_call;
    const char*             debug_file_last_call;
} layer_debug_info_t;
#endif

#ifdef __cplusplus
}
#endif

#endif // __LAYER_DEBUG_INFO_H__
