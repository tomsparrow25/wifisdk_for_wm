#ifndef __XI_STATED_SSCANF_H__
#define __XI_STATED_SSCANF_H__

#include "xi_stated_sscanf_state.h"
#include "xi_common.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief xi_stated_sscanf
 * @param s
 * @param pattern
 * @param source
 * @param variables
 * @return
 */
signed char xi_stated_sscanf(
          xi_stated_sscanf_state_t* s
        , const const_data_descriptor_t* pattern
        , const_data_descriptor_t* source
        , void** variables );

#ifdef __cplusplus
}
#endif

#endif // __XI_STATED_SSCANF_H__
