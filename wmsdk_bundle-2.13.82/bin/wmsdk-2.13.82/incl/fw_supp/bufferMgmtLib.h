/******************** (c) Marvell Semiconductor, Inc., 2001 *******************
 *
 * Purpose:
 *
 *
 *
 * Public Procedures:
 *
 *
 * Notes:
 *
 *
 *****************************************************************************/

#ifndef BUFFERMGMTLIB_H__
#define BUFFERMGMTLIB_H__

#include "wltypes.h"
//#include "osa.h"

/** @defgroup Global Buffer management library
 *  @{
 */

/**
*** @brief Flag value to be used for non blocking buffer allocation call
**/
#define BML_NO_WAIT                    0
/**
*** @brief Flag value to be used for blocking buffer allocation call
**/
#define BML_WAIT_FOREVER               0xFFFFFFFFUL

/**
*** @brief Wildcard state definition for buffers
**/
#define BML_STATE_WILDCARD             0xFF

#if 0
/**
*** @brief Config structure for block pools in a single pool id
**/
typedef struct 
{
#ifdef BUFMGT_POOL_NAME
    char name[20];
#endif
    OSPoolRef osaPoolRef;
    uint16 pool_BlockSize;
    uint8 block_Count;
} pool_Block_Info;

/**
*** @brief Config structure for a single Pool ID
**/
typedef struct
{
#ifdef BUFMGT_POOL_NAME
    char name[20];
#endif
    uint8 num_BlockPools;
    uint16 num_TotalBufs;
    uint32 *track_BufPtr;
    pool_Block_Info *pool_BlockInfo;
} pool_Config_t;


#endif
typedef void (*BufferReturnNotify_t)(void);

/**
*** @brief Buffer descriptor structure
**/
typedef struct BufferDesc
{
#if 0
    struct BufferDesc *NextBuffer; /* This pointer is used by sw queues
                                   ** (e.g. sdio) for chaining buffers.
                                   ** The queue structure uses a type cast.
                                   ** Should not be removed
                                   */
#endif
    union
    {
        // This field is used for time stamping to calculate latency for the
        // receive and transmitted packets, as they go in and out of controller
        // TBD: This is used for BT and a new field in the union will be added
        // once cleanup of Interface field is completed for BT.
        uint32 Interface;
        struct cm_ConnectionInfo *connPtr;
    }intf;
    uint16 DataLen;
    void *Buffer;
#if 0
    uint16 Type;
    uint16 Offset;
    uint16 rsvd;  /* Reserved field. Currently being used for inserting packet 
                  ** number in the insertTxPacketNum function. Can be used in
                  ** future for other purposes also.
                  */
    uint8  control;
    uint8  isCB   : 1;
    uint8  BDSize : 7;
    uint8  RefCount;
    uint8  BufIndex;
    uint32 PoolConfig;
    union 
    {
        // This field is used for time stamping to calculate latency for the 
        // receive and transmitted packets, as they go in and out of controller
        // TBD: This is used for BT and a new field in the union will be added
        // once cleanup of Interface field is completed for BT.
        uint32 Interface;
        struct cm_ConnectionInfo *connPtr;
    }intf;
#endif
#if 0
#if !defined(W8790)
    BufferReturnNotify_t (*freeCallback)(struct BufferDesc *);
#endif // #if !defined(W8790)
#endif
} BufferDesc_t;

#if 0
typedef BufferReturnNotify_t (*BufferFreeCallback_t)(BufferDesc_t*);


typedef UINT8 (*PFPoolCreate_t)(OSPoolRef * poolRef,
                                UINT32  poolType,
                                UINT8 * poolBase,
                                UINT32  poolSize,
                                UINT32  partitionSize,
                                UINT8   waitingMode);

typedef UINT8 (*PFPoolDelete_t)(OSPoolRef * poolRef);
#endif
/**
 * @brief Amount of memory reserved for ThreadX block functions
 */
#define BML_THRDX_OVERHEAD  (sizeof(uint32))

/**
 * @brief Returns the current memory location pointed by the descriptor
 */
#define BML_DATA_PTR(x) \
    (((BufferDesc_t *) (x))->Buffer)

/**
 * @brief Sets the memory offset of the descriptor
 */
#define BML_SET_OFFSET(x, y) \
    ((((BufferDesc_t *) (x))->Offset) = y)

/**
 * @brief Gets the current offset of the descriptor
 */
#define BML_GET_OFFSET(x) \
    (((BufferDesc_t *) (x))->Offset)

/**
 * @brief Add the size of the common buffer descriptor
 */
#define BML_ADD_DESC(x)  ((x) + sizeof(BufferDesc_t))

 /**
 * @brief Sets the memory offset of the descriptor
 */
#define BML_INCR_OFFSET(x, y) \
    ((((BufferDesc_t *) (x))->Offset) += (y))

#if defined(BUILD_NO_OSA)
#define BML_POOL_REF  ((TX_BLOCK_POOL *)(pBI->osaPoolRef))
#else
#define BML_POOL_REF  (&(((OS_Mem *)(pBI->osaPoolRef))->u.fPoolRef))
#endif

#if 0
/******************************************************************************
 *
 * Name: init_BufferPool
 *
 * Description:
 *   This routine is called to initialize the buffer pool
 *
 * Conditions For Use:
 *   During initilization before allocation any buffers from the pool.
 *
 * Arguments:
 *   Arg 1 : PoolConfig   - Pool Configuration of the pool
 *   Arg 2 : track_BufPtr - Pointer to track buffer memory. The memory region
 *                          pointed by track_BufPtr must have enough memory 
 *                          to hold MAX_BLOCK_PER_POOL pointers
 *   Arg3  : start_addr   - The address of the pool memory
 *
 * Return Value:
 *   Status - FAIL    = Failure to initialize the pool
 *            SUCCESS = Successful initialization of the pool
 *
 * Notes:
 *   None.
 *
 *
 *****************************************************************************/
extern Status_e init_BufferPool(pool_Config_t *poolConfig,
                                pool_Block_Info *pBI,
                                uint32 *track_BufPtr,
                                void *start_addr,
                                PFPoolCreate_t poolCreate);

extern Status_e DeletePool(pool_Config_t *poolConfig,
                           PFPoolDelete_t poolDelete);

/******************************************************************************
 *
 * Name: bml_AllocBuffer
 *
 * Description:
 *   This routine is called to get a buffer from the buffer management library.
 *
 * Conditions For Use:
 *   After successfful initialization of the buffer management library.
 *
 * Arguments:
 *   Arg 1 : PoolConfig - Pool configuration of the pool to get the block from.
 *   Arg 2 : BufferSize - Total size of the buffer needed(Includes the buffer 
 *                        descriptor)
 *   Arg 3 : Flags      - wait_option(As defined in the ThreadX memory pool 
 *                        management
 *
 * Return Value:
 *   Non zero - Pointer to the allocated buffer.
 *   NULL     - Buffer couldn't be allocated
 *
 * Notes:
 *   None.
 *
 *
 *****************************************************************************/
extern uint32 bml_AllocBuffer(pool_Config_t * poolConfig,
                              uint16 BufferSize, uint32 Flag);

/**
 * Name: bml_FreeBuffer
 * - Description:
 *   - This routine is called to free a buffer to the block pool
 *   .
 * - Conditions For Use:
 *   - The buffer MUST be allocated using the buffer management library
 *     before getting freed
 *   .
 * - Arguments:
 *   - Arg1 : Buffer Handle - The pointer to the buffer descriptor
 *   .
 * - Return Value:
 *   - SUCCESS - Pointer successfully freed
 *   - FAIL    - Failed to free the pointer
 *   .
 * - Notes:
 *   - None
 *   .
 * . 
 *****************************************************************************/
extern Status_e bml_FreeBuffer(uint32 BufferHandle);
/**
 * Name: bml_GetAllocBufCnt
 * - Description:
 *   - This routine is called to free an allocated buffer in the pool
 *   .
 * - Conditions For Use:
 *   - Can only be called after initializing buffer management pool & library
 *   .
 * - Arguments:
 *   - Arg1 : PoolConfig - Pool configuration of the pool whose buffers are 
 *                         to be checked
 *   .
 * - Return Value:
 *   - retVal - Number of allocated buffers in the pool
 *   .
 * - Notes:
 *   - None
 *   .
 * .
 */
extern uint32 bml_GetAllocBufCnt(pool_Config_t *poolConfig);
/**
 *
 * Name: bml_GetTotalBufCnt
 * - Description:
 *   - This routine is called to get the number of total buffers in the pool
 *   .
 * - Conditions For Use:
 *   - Can only be called after initializing buffer management pool & library 
 *   .
 * - Arguments:
 *   - Arg1 : PoolConfig - Pool configuration of the pool
 *   - Arg2 : numBlockPool - Block pool index within the pool
 *   .
 * - Return Value:
 *   - retVal - Number of total buffers in the pool
 *   .
 * - Notes:
 *   - None
 *   .
 * .
 */
extern uint32 bml_GetTotalBufCnt(pool_Config_t * poolConfig);

/**
 *
 * Name: bml_FreeAllPoolBufs
 * - Description:
 *   - This routine is called to free all allocated buffers
 *   .
 * - Conditions For Use:
 *   - Can only be called after initializing buffer management pool & library
 *   .
 * - Arguments:
 *   - Arg1 : PoolConfig - Pool configuration of the pool
 *   - Arg2 : state      - State of the buffers to be freed
 *   .
 * - Return Value:
 *   - None
 *   .
 * - Notes:
 *   - None
 *   .
 * .
 */
extern void bml_FreeAllPoolBufs(pool_Config_t * poolConfig, uint8 state);
/**
 *
 * Name: bml_MatchAllocState
 * - Description:
 *   - This routine is called to check if there is an allocated buffer with the
 *   specified state
 *   .
 * - Conditions For Use:
 *   - Can only be called after initializing buffer management pool & library
 *   .
 * - Arguments:
 *   - Arg1 : PoolConfig - Pool configuration of the pool
 *   - Arg2 : state      - State of the buffers to match
 *   .
 * - Return Value:
 *   - retVal : Non-Zero  - If match is found and the matched count
 *              FALSE - If no match is found
 *   .
 * - Notes:
 *   - None
 *   .
 * .
 */
extern uint32 bml_MatchAllocState(pool_Config_t * poolConfig, uint8 state);

extern PFPoolCreate_t pfBufferMgmtPoolCreate;

// Dummy function to be defined in RAM if patching is required
extern BOOLEAN (*bml_AllocBuffer_Hook)(pool_Config_t *poolConfig,
                                       uint16        BufferSize,
                                       uint32        Flag, 
                                       uint32        *ptr_val);

// Dummy function to be defined in RAM if patching is required
extern BOOLEAN (*bml_FreeBuffer_Hook)(uint32 BufferHandle, Status_e *peStatus);

/* @} */
#endif
#endif // _BUFFERMGMTLIB_H_
