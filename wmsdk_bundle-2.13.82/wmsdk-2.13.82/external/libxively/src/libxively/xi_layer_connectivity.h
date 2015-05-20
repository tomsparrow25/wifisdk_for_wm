#ifndef __LAYER_CONNECTIVITY_H__
#define __LAYER_CONNECTIVITY_H__

#ifdef __cplusplus
extern "C" {
#endif

// forward declaration
struct layer;

/**
 * \brief   allows to seek for the next call context
 */
typedef struct layer_connectivity
{
    struct layer* self;
    struct layer* next;
    struct layer* prev;
} layer_connectivity_t;

#ifdef __cplusplus
}
#endif

#endif // __LAYER_CONNECTIVITY_H__
