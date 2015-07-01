/***********************************************************
*  File: mem_pool.c
*  Author: nzy
*  Date: 20130106
***********************************************************/
#define _MEM_POOL_GLOBAL
#include "com_def.h"
#include "mem_pool.h"
#include "sys_adapter.h"

/***********************************************************
*************************micro define***********************
***********************************************************/
// 内存分区管理节点
typedef struct {
    LIST_HEAD listHead;     // 链表节点
    MEM_PARTITION memPartition;
}MEM_PARTITION_NODE,*P_MEM_PARTITION_NODE;

// 内存池管理结构
typedef struct {
    INT nodeNum;
    LIST_HEAD listHead;     // 内存池链表首址
    #if MEM_POOL_MUTLI_THREAD
    MUTEX_HANDLE mutexHandle;
    #endif
}MEM_POOL_MANAGE;

// 内存池建立表
typedef struct{
    DWORD  memBlkSize; // 块大小
    DWORD  memNBlks; // 块数量
}MEM_POOL_SETUP_TBL;


// for portable
#undef malloc
#define malloc(x) os_mem_alloc(x)

#undef calloc 
#define calloc os_mem_calloc(x)

#undef free
#define free(x) os_mem_free(x)
/***********************************************************
*************************variable define********************
***********************************************************/
STATIC MEM_POOL_MANAGE *pSysMemPoolMag = NULL;

// 用户自定义系统线程池规格
STATIC MEM_POOL_SETUP_TBL memPoolTbl[] = {
    {32,64},    // 2k
    {64,32},    // 2k
    {128,16},   // 2k
    {256,4},    // 1k
    {512,4},    // 2K
    {1024,4},   // 4K
    // {2048,2},   // 4k
};

#define MEM_PARTITION_NUM CNTSOF(memPoolTbl)
/***********************************************************
*************************function define********************
***********************************************************/


/***********************************************************
*  Function: SysMemoryPoolManageInit 系统内存池管理初始化
*  Input: none
*  Output: none
*  Return: oprt->结果集
*  Date: 20130106
***********************************************************/
STATIC OPERATE_RET SysMemoryPoolManageInit(VOID)
{
    if(pSysMemPoolMag)
        return OPRT_OK;

    pSysMemPoolMag = (MEM_POOL_MANAGE *)malloc(sizeof(MEM_POOL_MANAGE));
    if(!pSysMemPoolMag)
        return OPRT_MALLOC_FAILED;
    memset((PBYTE)pSysMemPoolMag,0,SIZEOF(MEM_POOL_MANAGE));

    INIT_LIST_HEAD(&(pSysMemPoolMag->listHead)); // 初始化模块管理链
    pSysMemPoolMag->nodeNum = 0;

    #if MEM_POOL_MUTLI_THREAD
    OPERATE_RET opRet;
    INT ret;
    
    ret = os_mutex_create(&(pSysMemPoolMag->mutexHandle), "sys_mempool", 1);
    if(ret != WM_SUCCESS) {
        opRet = OPRT_CR_MUTEX_ERR;
        if(pSysMemPoolMag)
            free(pSysMemPoolMag);

        pSysMemPoolMag = NULL;
        return opRet;
    }
    #endif

    return OPRT_OK;
}

/***********************************************************
*  Function: MemPartitionCreate 创建内存分区
*  Input: nblks:内存块数 blksize:块尺寸
*  Output: ppMemPartition:内存分区存储缓冲,注意:如ppMemPartition为NULL，
*  则生成的内存分区归系统管理，否则由创建者自己管理
*  Return: oprt->结果集
*  Date: 20130106
***********************************************************/
OPERATE_RET MemPartitionCreate(IN CONST DWORD nblks, \
                               IN CONST DWORD blksize,\
                               OUT MEM_PARTITION **ppMemPartition)
{
    MEM_PARTITION_NODE *pMemPartNode;
    MEM_PARTITION *pMemPartition;
    OPERATE_RET opRet;
    BYTE *pblk;
    void **ppLink;
    DWORD i;

    // 内存块数必须大于2，尺寸必须能容纳单个指针容量
    if (nblks < 2 || \
        blksize < sizeof(void *) || \
        (!pSysMemPoolMag && !ppMemPartition)) { // 系统内存池管理不存在
        return OPRT_INVALID_PARM;
    }

    pMemPartNode = NULL;
    pMemPartition = NULL;
    if(!ppMemPartition){ // 分区系统托管
        // NEW_LIST_NODE(MEM_PARTITION_NODE, pMemPartNode);
        pMemPartNode = (MEM_PARTITION_NODE *)malloc(SIZEOF(MEM_PARTITION_NODE));
        if(!pMemPartNode){
            opRet = OPRT_MALLOC_FAILED;
            goto ERR_RET;
        }
        pMemPartition = &(pMemPartNode->memPartition);
    }else
    {
        pMemPartition = (MEM_PARTITION *)malloc(SIZEOF(MEM_PARTITION));
        if(!pMemPartition){
            opRet = OPRT_MALLOC_FAILED;
            goto ERR_RET;
        }
    }

    pMemPartition->pMemAddr = malloc(nblks*blksize); // 内存分区首址
    if(!pMemPartition->pMemAddr){
        opRet = OPRT_MALLOC_FAILED;
        goto ERR_RET;
    }

    if(!ppMemPartition){ // 分区系统托管
        #if MEM_POOL_MUTLI_THREAD
        os_mutex_get(&pSysMemPoolMag->mutexHandle, OS_WAIT_FOREVER);
        #endif

        // 入链
        list_add_tail(&(pMemPartNode->listHead),&(pSysMemPoolMag->listHead));
        pSysMemPoolMag->nodeNum++;
    }

    // 内存分区初始化
    ppLink = (void **)pMemPartition->pMemAddr;
    pblk  = (BYTE *)pMemPartition->pMemAddr + blksize;
    for (i = 0; i < (nblks - 1); i++) {
        *ppLink = (void *)pblk;
        ppLink  = (void **)pblk;
        pblk   = pblk + blksize;
    }
    *ppLink = (void *)0; // Last memory block points to NULL

    pMemPartition->pMemAddrEnd = (BYTE *)pMemPartition->pMemAddr+(nblks*blksize)-1;
    pMemPartition->pMemFreeList = pMemPartition->pMemAddr;
    pMemPartition->memNFree = nblks;
    pMemPartition->memNBlks = nblks; 
    pMemPartition->memBlkSize = blksize;

    if(!ppMemPartition){ // 分区系统托管
        #if MEM_POOL_MUTLI_THREAD
        os_mutex_put(&pSysMemPoolMag->mutexHandle);
        #endif
    }
    
    if(ppMemPartition){
        *ppMemPartition = pMemPartition;
    }

    return OPRT_OK;

ERR_RET:
    if(pMemPartition->pMemAddr)
        free(pMemPartition->pMemAddr); // 释放数据区
    
    if(!ppMemPartition){ // 分区系统托管
        if(pMemPartNode)
            free(pMemPartNode);
    }
    else{
        if(pMemPartition)
            free(pMemPartition);
    }

    return opRet;
}

/***********************************************************
*  Function: MemBlockGet 获取内存块
*  Input: pMemPart:内存分区
*  Output: pRet:结果集
*  Return: 内存块地址
*  Date: 20130106
***********************************************************/
VOID *MemBlockGet(INOUT MEM_PARTITION *pMemPart,\
                  OUT OPERATE_RET *pOprtRet)
{
    void  *pblk;
    
    if(!pMemPart || !pOprtRet){
        *pOprtRet = OPRT_INVALID_PARM;
        return NULL;
    }

    // 无空闲链表
    if(!pMemPart->pMemFreeList){
        *pOprtRet = OPRT_MEM_PARTITION_EMPTY;
        return NULL;
    }
    //if(!pMemPart->memNFree)
    //{
    //    *pOprtRet = OPRT_MEM_PARTITION_EMPTY;
    //    return NULL;
    //}
    
    // 临界区 IN
    pblk = pMemPart->pMemFreeList;
    pMemPart->pMemFreeList = *(void **)pblk;
    pMemPart->memNFree--;
    // 临界区 OUT

    *pOprtRet = OPRT_OK;
    return (pblk);
}

/***********************************************************
*  Function: MemBlockPut 归还内存块
*  Input: pMemPart:内存分区
*  Output: pRet:结果集
*  Return: 内存块地址
*  Date: 20130106
***********************************************************/
OPERATE_RET MemBlockPut(INOUT MEM_PARTITION *pMemPart,\
                        IN CONST VOID *pblk)
{
    if(!pMemPart || !pblk)
        return OPRT_INVALID_PARM;
    
    // 临界区 IN
    if (pMemPart->memNFree >= pMemPart->memNBlks) {
        // 临界区 OUT
        return OPRT_MEM_PARTITION_FULL;
    }
    
    *(void **)pblk = pMemPart->pMemFreeList;
    pMemPart->pMemFreeList = (VOID *)pblk;
    pMemPart->memNFree++;
    // 临界区 OUT

    return OPRT_OK;
}

/***********************************************************
*  Function: MemPartitionDelete 内存分区删除
*  Input: pMemPart:内存分区
*  Output: none
*  Return: oprt->结果集
*  Date: 20130106
***********************************************************/
OPERATE_RET MemPartitionDelete(IN MEM_PARTITION *pMemPart)
{
    MEM_PARTITION_NODE *pMemPartNode;
    P_LIST_HEAD pPos;

    if (!pMemPart){
        return OPRT_INVALID_PARM;
    };

    if(pSysMemPoolMag)
    {
        #if MEM_POOL_MUTLI_THREAD
        os_mutex_get(&pSysMemPoolMag->mutexHandle, OS_WAIT_FOREVER);
        #endif

        // 遍历链
        list_for_each(pPos, &(pSysMemPoolMag->listHead)){// 系统管理内存分区
            pMemPartNode = list_entry(pPos, MEM_PARTITION_NODE, listHead);
            if(&(pMemPartNode->memPartition) == pMemPart) // 找到
            {
                DeleteNode(pMemPartNode,listHead); // 删除节点
                pSysMemPoolMag->nodeNum--;
                if(pMemPartNode->memPartition.pMemAddr)
                    free(pMemPartNode->memPartition.pMemAddr); // 释放数据内存
                if(pMemPartNode)
                    free(pMemPartNode);
                #if MEM_POOL_MUTLI_THREAD
                os_mutex_put(&pSysMemPoolMag->mutexHandle);
                #endif

                return OPRT_OK;
            }
        }
        #if MEM_POOL_MUTLI_THREAD
        os_mutex_put(&pSysMemPoolMag->mutexHandle);
        #endif
    }

    // 非系统管理的内存分区
    if(pMemPart->pMemAddr)
        free(pMemPart->pMemAddr);
    if(pMemPart)
        free(pMemPart);
        
    return OPRT_OK;
}

/***********************************************************
*  Function: SysMemoryPoolSetup 建立系统内存池
*  Input: none
*  Output: none
*  Return: oprt->结果集
*  Date: 20130106
***********************************************************/
OPERATE_RET SysMemoryPoolSetup(VOID)
{
    OPERATE_RET opRet;
    INT i;
    
    if(!pSysMemPoolMag){
        if(OPRT_OK != (opRet = SysMemoryPoolManageInit()))
            return opRet;
    }

    for(i = 0; i < MEM_PARTITION_NUM;i++){
        opRet = MemPartitionCreate(memPoolTbl[i].memNBlks, \
                                   memPoolTbl[i].memBlkSize,\
                                   NULL);
        if(OPRT_OK != opRet)
        {
            SysMemoryPoolDelete();
            return opRet;
        }
    }

    return OPRT_OK;
}

/***********************************************************
*  Function: SysMemoryPoolDelete 删除系统内存池
*  Input: none
*  Output: none
*  Return: oprt->结果集
*  Date: 20130107
***********************************************************/
VOID SysMemoryPoolDelete(VOID)
{
    MEM_PARTITION_NODE *pMemPartNode;

    #if MEM_POOL_MUTLI_THREAD
    os_mutex_get(&pSysMemPoolMag->mutexHandle, OS_WAIT_FOREVER);
    #endif
    
    // 反向遍历删除
    while(!list_empty(&(pSysMemPoolMag->listHead))) {
		pMemPartNode = list_entry(pSysMemPoolMag->listHead.prev, MEM_PARTITION_NODE,listHead);
		DeleteNode(pMemPartNode,listHead); // 删除节点
		pSysMemPoolMag->nodeNum--;
        if(pMemPartNode->memPartition.pMemAddr)
            free(pMemPartNode->memPartition.pMemAddr); // 释放数据内存
        if(pMemPartNode)
            free(pMemPartNode);
    }
    #if MEM_POOL_MUTLI_THREAD
    os_mutex_put(&pSysMemPoolMag->mutexHandle);
    os_mutex_delete(&pSysMemPoolMag->mutexHandle);
    #endif
    
    if(pSysMemPoolMag)
        free(pSysMemPoolMag);

    pSysMemPoolMag = NULL;
}

/***********************************************************
*  Function: MallocFromSysMemPool 由系统内存池中申请内存
*  Input: reqSize:需要分配的内存 
*  Output: pRet:返回值结果集
*  Return: 申请的内存地址
*  Date: 20130107
***********************************************************/
VOID *MallocFromSysMemPool(IN CONST DWORD reqSize,OPERATE_RET *pRet)
{
    MEM_PARTITION_NODE *pMemPartNode;
    P_LIST_HEAD pPos;
    VOID *pMalloc;
    OPERATE_RET opRet;

    if(!pSysMemPoolMag){
        *pRet = OPRT_INVALID_PARM;
        return NULL;
    }
    
    #if MEM_POOL_MUTLI_THREAD
    if(!pSysMemPoolMag->mutexHandle) {
        *pRet = OPRT_INVALID_PARM;
        return NULL;
    }

    os_mutex_get(&pSysMemPoolMag->mutexHandle, OS_WAIT_FOREVER);
    #endif
    
    // 遍历链
    list_for_each(pPos, &(pSysMemPoolMag->listHead)){
        pMemPartNode = list_entry(pPos, MEM_PARTITION_NODE, listHead);
        if(pMemPartNode->memPartition.memBlkSize >= reqSize){ // 找到符合申请需求的内存块
            pMalloc = MemBlockGet(&pMemPartNode->memPartition,&opRet);
            if(!pMalloc && \
               OPRT_MEM_PARTITION_EMPTY == opRet){ // 无可用内存块
                continue;
            }else if(!pMalloc){
                os_mutex_put(&pSysMemPoolMag->mutexHandle);
                *pRet = opRet;
                return NULL;
            }

            *pRet = OPRT_OK;
            #if MEM_POOL_MUTLI_THREAD
            os_mutex_put(&pSysMemPoolMag->mutexHandle);
            #endif
            return pMalloc;
        }
    }
    #if MEM_POOL_MUTLI_THREAD
    os_mutex_put(&pSysMemPoolMag->mutexHandle);
    #endif
    
    *pRet = OPRT_MEM_PARTITION_NOT_FOUND;
    return NULL;
}

/***********************************************************
*  Function: FreeFromSysMemPool 归还内存快至内存池
*  Input: pReqMem:需要归还的内存 
*  Output: none
*  Return: opRt:返回值结果集
*  Date: 20130107
***********************************************************/
OPERATE_RET FreeFromSysMemPool(IN PVOID pReqMem)
{
    MEM_PARTITION_NODE *pMemPartNode;
    P_LIST_HEAD pPos;
    OPERATE_RET opRet;

    if(!pSysMemPoolMag){
        return OPRT_INVALID_PARM;
    }

    #if MEM_POOL_MUTLI_THREAD
    os_mutex_get(&pSysMemPoolMag->mutexHandle, OS_WAIT_FOREVER);
    #endif
    // 遍历链
    list_for_each(pPos, &(pSysMemPoolMag->listHead)){
        pMemPartNode = list_entry(pPos, MEM_PARTITION_NODE, listHead);
        if(pReqMem >= pMemPartNode->memPartition.pMemAddr && \
           pReqMem <= pMemPartNode->memPartition.pMemAddrEnd){ // 找到所属分区地址
            opRet = MemBlockPut(&pMemPartNode->memPartition,pReqMem);
            #if MEM_POOL_MUTLI_THREAD
            os_mutex_put(&pSysMemPoolMag->mutexHandle);
            #endif
            return opRet;
        }
    }
    #if MEM_POOL_MUTLI_THREAD
    os_mutex_put(&pSysMemPoolMag->mutexHandle);
    #endif

    return OPRT_MEM_PARTITION_NOT_FOUND;
}

/***********************************************************
*  Function: ShowSysMemPoolInfo
*  Input: none 
*  Output: none
*  Return: opRt:返回值结果集
*  Date: 20130107
***********************************************************/
#if SHOW_MEM_POOL_DEBUG
VOID ShowSysMemPoolInfo(VOID)
{
    MEM_PARTITION_NODE *pMemPartNode;
    P_LIST_HEAD pPos;
    INT index = 0;

    if(!pSysMemPoolMag){
        return;
    }

    #if MEM_POOL_MUTLI_THREAD
    os_mutex_get(&pSysMemPoolMag->mutexHandle, OS_WAIT_FOREVER);
    #endif
    // 遍历链
    list_for_each(pPos, &(pSysMemPoolMag->listHead)){
        pMemPartNode = list_entry(pPos, MEM_PARTITION_NODE, listHead);

        PR_DEBUG_RAW("mem partition index:%d\r\n",index);
        index++;
        //PR_DEBUG_RAW("begin:%p\r\n",pMemPartNode->memPartition.pMemAddr);
        //PR_DEBUG_RAW("end:%p\r\n",pMemPartNode->memPartition.pMemAddrEnd);
        //PR_DEBUG_RAW("pMemFreeList:%p\r\n",pMemPartNode->memPartition.pMemFreeList);
        PR_DEBUG_RAW("memBlkSize:%d\r\n",pMemPartNode->memPartition.memBlkSize);
        PR_DEBUG_RAW("memNBlks:%d\r\n",pMemPartNode->memPartition.memNBlks);
        PR_DEBUG_RAW("memNFree:%d\r\n",pMemPartNode->memPartition.memNFree);
        PR_DEBUG_RAW("***********************************");
    }
    #if MEM_POOL_MUTLI_THREAD
    os_mutex_put(&pSysMemPoolMag->mutexHandle);
    #endif
}
#endif

/***********************************************************
*  Function: Malloc 内存申请
*  Input: reqSize->申请的内存大小
*  Output: none
*  Return: 失败NULL 成功返回内存地址
*  Date: 120427
***********************************************************/
VOID *Malloc(IN CONST DWORD reqSize)
{
    PVOID pMalloc;
    OPERATE_RET opRet;

    pMalloc = MallocFromSysMemPool(reqSize,&opRet);
    if(OPRT_OK != opRet){
        pMalloc = malloc(reqSize);
    }

#if SYS_MEM_DEBUG
    STATIC INT cnt = 0;

    if(OPRT_OK != opRet)
        PR_DEBUG("cnt:%d malloc:%p\r\n",cnt,pMalloc);
    else
        PR_DEBUG("cnt:%d malloc from mempool:%p\r\n",cnt,pMalloc);
    cnt++;
#endif    
    return pMalloc;
}

/***********************************************************
*  Function: Free 安全内存释放
*  Input: ppReqMem
*  Output: none
*  Return: none
*  Date: 120427
***********************************************************/
VOID Free(IN PVOID pReqMem)
{
    OPERATE_RET opRet;

    opRet = OPRT_OK;
    if(pReqMem){
        opRet = FreeFromSysMemPool(pReqMem);
        if(OPRT_OK != opRet){
            free(pReqMem);
        }
    }else
        return;
    
#if SYS_MEM_DEBUG
    STATIC INT cnt = 0;

    if(OPRT_OK != opRet)
        PR_DEBUG("cnt:%d free:%p\r\n",cnt,pReqMem);
    else
        PR_DEBUG("cnt:%d free from mempool:%p\r\n",cnt,pReqMem);
    cnt++;
#endif
}


