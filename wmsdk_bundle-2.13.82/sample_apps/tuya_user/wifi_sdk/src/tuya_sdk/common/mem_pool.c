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
// �ڴ��������ڵ�
typedef struct {
    LIST_HEAD listHead;     // ����ڵ�
    MEM_PARTITION memPartition;
}MEM_PARTITION_NODE,*P_MEM_PARTITION_NODE;

// �ڴ�ع���ṹ
typedef struct {
    INT nodeNum;
    LIST_HEAD listHead;     // �ڴ��������ַ
    #if MEM_POOL_MUTLI_THREAD
    MUTEX_HANDLE mutexHandle;
    #endif
}MEM_POOL_MANAGE;

// �ڴ�ؽ�����
typedef struct{
    DWORD  memBlkSize; // ���С
    DWORD  memNBlks; // ������
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

// �û��Զ���ϵͳ�̳߳ع��
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
*  Function: SysMemoryPoolManageInit ϵͳ�ڴ�ع����ʼ��
*  Input: none
*  Output: none
*  Return: oprt->�����
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

    INIT_LIST_HEAD(&(pSysMemPoolMag->listHead)); // ��ʼ��ģ�������
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
*  Function: MemPartitionCreate �����ڴ����
*  Input: nblks:�ڴ���� blksize:��ߴ�
*  Output: ppMemPartition:�ڴ�����洢����,ע��:��ppMemPartitionΪNULL��
*  �����ɵ��ڴ������ϵͳ���������ɴ������Լ�����
*  Return: oprt->�����
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

    // �ڴ�����������2���ߴ���������ɵ���ָ������
    if (nblks < 2 || \
        blksize < sizeof(void *) || \
        (!pSysMemPoolMag && !ppMemPartition)) { // ϵͳ�ڴ�ع�������
        return OPRT_INVALID_PARM;
    }

    pMemPartNode = NULL;
    pMemPartition = NULL;
    if(!ppMemPartition){ // ����ϵͳ�й�
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

    pMemPartition->pMemAddr = malloc(nblks*blksize); // �ڴ������ַ
    if(!pMemPartition->pMemAddr){
        opRet = OPRT_MALLOC_FAILED;
        goto ERR_RET;
    }

    if(!ppMemPartition){ // ����ϵͳ�й�
        #if MEM_POOL_MUTLI_THREAD
        os_mutex_get(&pSysMemPoolMag->mutexHandle, OS_WAIT_FOREVER);
        #endif

        // ����
        list_add_tail(&(pMemPartNode->listHead),&(pSysMemPoolMag->listHead));
        pSysMemPoolMag->nodeNum++;
    }

    // �ڴ������ʼ��
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

    if(!ppMemPartition){ // ����ϵͳ�й�
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
        free(pMemPartition->pMemAddr); // �ͷ�������
    
    if(!ppMemPartition){ // ����ϵͳ�й�
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
*  Function: MemBlockGet ��ȡ�ڴ��
*  Input: pMemPart:�ڴ����
*  Output: pRet:�����
*  Return: �ڴ���ַ
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

    // �޿�������
    if(!pMemPart->pMemFreeList){
        *pOprtRet = OPRT_MEM_PARTITION_EMPTY;
        return NULL;
    }
    //if(!pMemPart->memNFree)
    //{
    //    *pOprtRet = OPRT_MEM_PARTITION_EMPTY;
    //    return NULL;
    //}
    
    // �ٽ��� IN
    pblk = pMemPart->pMemFreeList;
    pMemPart->pMemFreeList = *(void **)pblk;
    pMemPart->memNFree--;
    // �ٽ��� OUT

    *pOprtRet = OPRT_OK;
    return (pblk);
}

/***********************************************************
*  Function: MemBlockPut �黹�ڴ��
*  Input: pMemPart:�ڴ����
*  Output: pRet:�����
*  Return: �ڴ���ַ
*  Date: 20130106
***********************************************************/
OPERATE_RET MemBlockPut(INOUT MEM_PARTITION *pMemPart,\
                        IN CONST VOID *pblk)
{
    if(!pMemPart || !pblk)
        return OPRT_INVALID_PARM;
    
    // �ٽ��� IN
    if (pMemPart->memNFree >= pMemPart->memNBlks) {
        // �ٽ��� OUT
        return OPRT_MEM_PARTITION_FULL;
    }
    
    *(void **)pblk = pMemPart->pMemFreeList;
    pMemPart->pMemFreeList = (VOID *)pblk;
    pMemPart->memNFree++;
    // �ٽ��� OUT

    return OPRT_OK;
}

/***********************************************************
*  Function: MemPartitionDelete �ڴ����ɾ��
*  Input: pMemPart:�ڴ����
*  Output: none
*  Return: oprt->�����
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

        // ������
        list_for_each(pPos, &(pSysMemPoolMag->listHead)){// ϵͳ�����ڴ����
            pMemPartNode = list_entry(pPos, MEM_PARTITION_NODE, listHead);
            if(&(pMemPartNode->memPartition) == pMemPart) // �ҵ�
            {
                DeleteNode(pMemPartNode,listHead); // ɾ���ڵ�
                pSysMemPoolMag->nodeNum--;
                if(pMemPartNode->memPartition.pMemAddr)
                    free(pMemPartNode->memPartition.pMemAddr); // �ͷ������ڴ�
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

    // ��ϵͳ������ڴ����
    if(pMemPart->pMemAddr)
        free(pMemPart->pMemAddr);
    if(pMemPart)
        free(pMemPart);
        
    return OPRT_OK;
}

/***********************************************************
*  Function: SysMemoryPoolSetup ����ϵͳ�ڴ��
*  Input: none
*  Output: none
*  Return: oprt->�����
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
*  Function: SysMemoryPoolDelete ɾ��ϵͳ�ڴ��
*  Input: none
*  Output: none
*  Return: oprt->�����
*  Date: 20130107
***********************************************************/
VOID SysMemoryPoolDelete(VOID)
{
    MEM_PARTITION_NODE *pMemPartNode;

    #if MEM_POOL_MUTLI_THREAD
    os_mutex_get(&pSysMemPoolMag->mutexHandle, OS_WAIT_FOREVER);
    #endif
    
    // �������ɾ��
    while(!list_empty(&(pSysMemPoolMag->listHead))) {
		pMemPartNode = list_entry(pSysMemPoolMag->listHead.prev, MEM_PARTITION_NODE,listHead);
		DeleteNode(pMemPartNode,listHead); // ɾ���ڵ�
		pSysMemPoolMag->nodeNum--;
        if(pMemPartNode->memPartition.pMemAddr)
            free(pMemPartNode->memPartition.pMemAddr); // �ͷ������ڴ�
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
*  Function: MallocFromSysMemPool ��ϵͳ�ڴ���������ڴ�
*  Input: reqSize:��Ҫ������ڴ� 
*  Output: pRet:����ֵ�����
*  Return: ������ڴ��ַ
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
    
    // ������
    list_for_each(pPos, &(pSysMemPoolMag->listHead)){
        pMemPartNode = list_entry(pPos, MEM_PARTITION_NODE, listHead);
        if(pMemPartNode->memPartition.memBlkSize >= reqSize){ // �ҵ���������������ڴ��
            pMalloc = MemBlockGet(&pMemPartNode->memPartition,&opRet);
            if(!pMalloc && \
               OPRT_MEM_PARTITION_EMPTY == opRet){ // �޿����ڴ��
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
*  Function: FreeFromSysMemPool �黹�ڴ�����ڴ��
*  Input: pReqMem:��Ҫ�黹���ڴ� 
*  Output: none
*  Return: opRt:����ֵ�����
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
    // ������
    list_for_each(pPos, &(pSysMemPoolMag->listHead)){
        pMemPartNode = list_entry(pPos, MEM_PARTITION_NODE, listHead);
        if(pReqMem >= pMemPartNode->memPartition.pMemAddr && \
           pReqMem <= pMemPartNode->memPartition.pMemAddrEnd){ // �ҵ�����������ַ
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
*  Return: opRt:����ֵ�����
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
    // ������
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
*  Function: Malloc �ڴ�����
*  Input: reqSize->������ڴ��С
*  Output: none
*  Return: ʧ��NULL �ɹ������ڴ��ַ
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
*  Function: Free ��ȫ�ڴ��ͷ�
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


