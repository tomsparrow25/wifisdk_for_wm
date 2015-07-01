/***********************************************************
*  File: mem_pool.h
*  Author: nzy
*  Date: 20130106
***********************************************************/
#ifndef _MEM_POOL_H
#define _MEM_POOL_H

#include "uni_pointer.h"
#include "error_code.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifdef _MEM_POOL_GLOBAL
    #define _MEM_POOL_EXT 
#else
    #define _MEM_POOL_EXT extern
#endif

/***********************************************************
*************************micro define***********************
***********************************************************/
#define SYS_MEM_DEBUG 0 // ϵͳ�ڴ����
#define SHOW_MEM_POOL_DEBUG 1 // �ڴ��ʹ�������ʾ����֧��
#define MEM_POOL_MUTLI_THREAD 1 // �Ƿ�֧�ֶ��̴߳���

// �ڴ����
typedef struct {            // MEMORY CONTROL BLOCK                                         
    VOID   *pMemAddr;       // Pointer to beginning of memory partition    
    VOID   *pMemAddrEnd;    // Pointer to ending of memory partition
    VOID   *pMemFreeList;   // Pointer to list of free memory blocks                        
    DWORD  memBlkSize;      // Size (in bytes) of each block of memory                      
    DWORD  memNBlks;        // Total number of blocks in this partition                     
    DWORD  memNFree;        // Number of memory blocks remaining in this partition          
}MEM_PARTITION;

/***********************************************************
*************************variable define********************
***********************************************************/

/***********************************************************
*************************function define********************
***********************************************************/
/***********************************************************
*  Function: SysMemoryPoolSetup ����ϵͳ�ڴ��
*  Input: none
*  Output: none
*  Return: oprt->�����
*  Date: 20130106
***********************************************************/
_MEM_POOL_EXT \
OPERATE_RET SysMemoryPoolSetup(VOID);

/***********************************************************
*  Function: MemPartitionCreate �����ڴ����
*  Input: nblks:�ڴ���� blksize:��ߴ�
*  Output: ppMemPartition:�ڴ�����洢����,ע��:��ppMemPartitionΪNULL��
*  �����ɵ��ڴ������ϵͳ���������ɴ������Լ�����
*  Return: oprt->�����
*  Date: 20130106
***********************************************************/
_MEM_POOL_EXT \
OPERATE_RET MemPartitionCreate(IN CONST DWORD nblks, \
                               IN CONST DWORD blksize,\
                               OUT MEM_PARTITION **ppMemPartition);

/***********************************************************
*  Function: MemBlockGet ��ȡ�ڴ��
*  Input: pMemPart:�ڴ����
*  Output: pRet:�����
*  Return: �ڴ���ַ
*  Date: 20130106
***********************************************************/
_MEM_POOL_EXT \
VOID *MemBlockGet(INOUT MEM_PARTITION *pMemPart,\
                  OUT OPERATE_RET *pOprtRet);

/***********************************************************
*  Function: MemBlockPut �黹�ڴ��
*  Input: pMemPart:�ڴ����
*  Output: pRet:�����
*  Return: �ڴ���ַ
*  Date: 20130106
***********************************************************/
_MEM_POOL_EXT \
OPERATE_RET MemBlockPut(INOUT MEM_PARTITION *pMemPart,\
                        IN CONST VOID *pblk);

/***********************************************************
*  Function: MemPartitionDelete �ڴ����ɾ��
*  Input: pMemPart:�ڴ����
*  Output: none
*  Return: oprt->�����
*  Date: 20130106
***********************************************************/
_MEM_POOL_EXT \
OPERATE_RET MemPartitionDelete(IN MEM_PARTITION *pMemPart);

/***********************************************************
*  Function: SysMemoryPoolDelete ɾ��ϵͳ�ڴ��
*  Input: none
*  Output: none
*  Return: oprt->�����
*  Date: 20130107
***********************************************************/
_MEM_POOL_EXT \
VOID SysMemoryPoolDelete(VOID);

/***********************************************************
*  Function: MallocFromSysMemPool ��ϵͳ�ڴ���������ڴ�
*  Input: reqSize:��Ҫ������ڴ� 
*  Output: pRet:����ֵ�����
*  Return: ������ڴ��ַ
*  Date: 20130107
***********************************************************/
_MEM_POOL_EXT \
VOID *MallocFromSysMemPool(IN CONST DWORD reqSize,OPERATE_RET *pRet);

/***********************************************************
*  Function: FreeFromSysMemPool �黹�ڴ�����ڴ��
*  Input: pReqMem:��Ҫ�黹���ڴ� 
*  Output: none
*  Return: opRt:����ֵ�����
*  Date: 20130107
***********************************************************/
_MEM_POOL_EXT \
OPERATE_RET FreeFromSysMemPool(IN PVOID pReqMem);

/***********************************************************
*  Function: ShowSysMemPoolInfo
*  Input: none 
*  Output: none
*  Return: opRt:����ֵ�����
*  Date: 20130107
***********************************************************/
#if SHOW_MEM_POOL_DEBUG
_MEM_POOL_EXT \
VOID ShowSysMemPoolInfo(VOID);
#endif

/***********************************************************
*  Function: Malloc �ڴ�����
*  Input: reqSize->������ڴ��С
*  Output: none
*  Return: ʧ��NULL �ɹ������ڴ��ַ
*  Date: 120427
***********************************************************/
_MEM_POOL_EXT \
VOID *Malloc(IN CONST DWORD reqSize);

/***********************************************************
*  Function: Free ��ȫ�ڴ��ͷ�
*  Input: ppReqMem
*  Output: none
*  Return: none
*  Date: 120427
***********************************************************/
_MEM_POOL_EXT \
VOID Free(IN PVOID pReqMem);


#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif



