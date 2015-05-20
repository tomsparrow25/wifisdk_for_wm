#ifndef __HTTP_LAYER_INPUT_H__
#define __HTTP_LAYER_INPUT_H__

/**
 *\file     http_layer_input.h
 *\author   Olgierd Humenczuk
 *\brief    File contains declaration of the http layer data structure which is used
 *          within the between layer communication process.
 */


#include "xively.h"
#include "xi_layer.h"
#include "xi_generator.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \enum describes the xi http layer api input function types
 */
typedef enum
{
      HTTP_LAYER_INPUT_DATASTREAM_GET = 0
    , HTTP_LAYER_INPUT_DATASTREAM_UPDATE
    , HTTP_LAYER_INPUT_DATASTREAM_CREATE
    , HTTP_LAYER_INPUT_DATASTREAM_DELETE
    , HTTP_LAYER_INPUT_DATAPOINT_DELETE
    , HTTP_LAYER_INPUT_DATAPOINT_DELETE_RANGE
    , HTTP_LAYER_INPUT_FEED_UPDATE
    , HTTP_LAYER_INPUT_FEED_GET
    , HTTP_LAYER_INPUT_FEED_GET_ALL
} xi_query_type_t;


/**
 *\brief    Contains the data related to creation the transport for any payload
 *          that we shall send over network.
 *
 *\note     Version of that structure is specialized for communication with RESTful xively API
 *          so it is not fully functional generic implementation of the HTTP protocol.
 */
typedef struct
{
    xi_query_type_t         query_type;             //!< pass the information about the type of the query that is constructed
    xi_context_t*           xi_context;             //!< the pointer to the context of the xi library
    xi_generator_t*         payload_generator;      //!< the pointer to the payload generator, used via the http layer to construct the payload

    union http_union_data_t
    {
        struct xi_get_datastream_t
        {
            const char*     datastream;
            xi_datapoint_t* value;
        } xi_get_datastream;

        struct xi_update_datastream_t
        {
            const char*     datastream;
            xi_datapoint_t* value;
        } xi_update_datastream;

        struct xi_create_datastream_t
        {
            const char*         datastream;
            xi_datapoint_t*     value;
        } xi_create_datastream;

        struct xi_delete_datastream_t
        {
            const char*         datastream;
        } xi_delete_datastream;

        struct xi_delete_datapoint_t
        {
            const char*         datastream;
            xi_datapoint_t*     value;
        } xi_delete_datapoint;

        struct xi_delete_datapoint_range_t
        {
            const char*         datastream;
            xi_timestamp_t*     value_start;
            xi_timestamp_t*     value_end;
        } xi_delete_datapoint_range;

        struct xi_get_feed_t
        {
            xi_feed_t*      feed;
        } xi_get_feed;

        struct xi_update_feed_t
        {
            xi_feed_t*      feed;
        } xi_update_feed;
    } http_union_data;                              //!< the union suppose to contain different combinations of data used in queries

} http_layer_input_t;

#ifdef __cplusplus
}
#endif

#endif // __HTTP_LAYER_INPUT_H__
