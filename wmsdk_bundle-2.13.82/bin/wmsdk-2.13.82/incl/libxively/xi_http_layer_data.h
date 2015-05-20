#ifndef __HTTP_LAYER_DATA_H__
#define __HTTP_LAYER_DATA_H__

/**
 *\file     http_layer_data.h
 *\author   Olgierd Humenczuk
 *\brief    containes the structure related to the internal layer data that shall be used over the lifecycle of each instance
 *          to process the queries and parse the responses.
 */

#include "xi_stated_sscanf_state.h"
#include "xively.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 *\brief    shall contain the data needed via parsing and constructing queries
 */
typedef struct
{
    char                        parser_state;          //!< represents the state of the http layer parser parsing header, state, payload etc, etc.
    unsigned char               last_char_marker;      //!< the marker that it's used to point to the last char in the buffer
    xi_stated_sscanf_state_t    xi_stated_sscanf_state;
    short                       counter;               //!< multiple purposes counter
    short                       content_length;        //!< size of the payload
    xi_response_t*              response;
} http_layer_data_t;

#ifdef __cplusplus
}
#endif

#endif // __HTTP_LAYER_DATA_H__
