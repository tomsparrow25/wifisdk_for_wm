#ifndef __XI_COROUTINE_H__
#define __XI_COROUTINE_H__

#ifdef __cplusplus
extern "C" {
#endif

// COROUTINE API
#define BEGIN_CORO( state )\
    switch( state )\
    { \
        default:

#define YIELD( state, ret )\
    state = __LINE__; return ret; case __LINE__:

#define EXIT( state, ret )\
    state = 1; return ret;

#define RESTART( state, ret )\
    state = 0; return ret;

#define END_CORO()\
    };

#ifdef __cplusplus
}
#endif


#endif // __XI_COROUTINE_H__
