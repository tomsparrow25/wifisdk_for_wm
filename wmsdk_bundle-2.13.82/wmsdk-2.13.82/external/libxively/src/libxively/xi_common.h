#ifndef __COMMON_H__
#define __COMMON_H__

#include <stdlib.h>
#include <inttypes.h>

#include "xi_debug.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \brief the structure that describes the buffer for storing/passing
 * within the program the structure can be easly manipulated using the 
 * module functions
 */
typedef struct
{
    char*   data_buffer;
    size_t  buffer_capacity;    //!< the maximum capacity of the buffer
    size_t  buffer_size;        //!< the actual used size of the buffer 
} data_buffer_t;

/**
 * \brief the structure that describes the simple char* with the size so it's easier to
 * pass this as a paremeter
 */
typedef struct
{
    char*           data_ptr;
    unsigned short  data_size;
    unsigned short  real_size;
    unsigned short  curr_pos;
} data_descriptor_t;

typedef struct
{    
    const char*     data_ptr;
    unsigned short  data_size;
    unsigned short  real_size;
    unsigned short  curr_pos;
} const_data_descriptor_t;

#ifdef __cplusplus
}
#endif


#endif
