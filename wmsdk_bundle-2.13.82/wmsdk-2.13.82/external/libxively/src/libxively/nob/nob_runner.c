#include "nob_runner.h"
#include "xi_layer_api.h"
#include "xi_coroutine.h"


layer_state_t process_xively_nob_step( xi_context_t* xi )
{
    // PRECONDITION
    assert( xi != 0 );

    static int16_t state                = 0;
    static layer_state_t layer_state    = LAYER_STATE_OK;

    BEGIN_CORO( state )

    // write data to the endpoint
    do
    {
        layer_state = CALL_ON_SELF_DATA_READY(
                      xi->layer_chain.top
                    , xi->input, LAYER_HINT_NONE );

        if( layer_state == LAYER_STATE_NOT_READY )
        {
            YIELD( state, layer_state );
        }

    } while( layer_state != LAYER_STATE_NOT_READY );

    if( layer_state == LAYER_STATE_ERROR )
    {
        EXIT( state, layer_state);
    }

    // now read the data from the endpoint
    do
    {
        layer_state = CALL_ON_SELF_ON_DATA_READY(
                      xi->layer_chain.bottom
                    , 0, LAYER_HINT_NONE );

        if( layer_state == LAYER_STATE_NOT_READY )
        {
            YIELD( state, layer_state );
        }

    } while( layer_state != LAYER_STATE_NOT_READY );

    EXIT( state, layer_state );

    END_CORO()

    state = 0;
    return 0;
}
