/***********************************************************
*  File: uni_pointer.h
*  Author: nzy
*  Date: 120427
***********************************************************/
#ifndef _UNI_POINTER_H
#define _UNI_POINTER_H

#include "com_def.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifdef _UNI_POINTER_GLOBAL
    #define _UNI_POINTER_EXT 
#else
    #define _UNI_POINTER_EXT extern
#endif

/***********************************************************
*************************micro define***********************
***********************************************************/
typedef struct list_head 
{
    struct list_head *next, *prev;
}LIST_HEAD,*P_LIST_HEAD;

#define LIST_HEAD_INIT(name) { &(name), &(name) }

// ����LIST����̬��ʼ��һ���յ�ͨ��˫������
#define LIST_HEAD(name) \
LIST_HEAD name = LIST_HEAD_INIT(name)

// ��̬��ʼ��һ���յ�ͨ��˫������
#define INIT_LIST_HEAD(ptr) do { \
(ptr)->next = (ptr); (ptr)->prev = (ptr); \
} while (0)

// ��̬����һ������ͨ��˫������Ľṹ��
#define NEW_LIST_NODE(type, node) \
{\
    node = (type *)Malloc(sizeof(type));\
}

// �ͷ������е����нڵ㣬ʹ�������Ϊ������
#define FREE_LIST(type, p, list_name)\
{\
    type *posnode;\
    while(!list_empty(&(p)->list_name)) {\
    posnode = list_entry((&(p)->list_name)->next, type, list_name);\
    list_del((&(p)->list_name)->next);\
    Free(posnode);\
    }\
}

// ��ȡ�����е�һ���ڵ��ַ(�õ�ַָ�������ṹ)
#define GetFirstNode(type,p,list_name,pGetNode)\
{\
    pGetNode = NULL;\
    while(!list_empty(&(p)->list_name)){\
    pGetNode = list_entry((&(p)->list_name)->next, type, list_name);\
    break;\
    }\
}

// ������ɾ��ĳ�ڵ㣬���ͷŸýڵ����ڽṹռ�õ��ڴ�
#define DeleteNodeAndFree(pDelNode,list_name)\
{\
    list_del(&(pDelNode->list_name));\
    Free(pDelNode);\
}

// ��������ɾ��ĳ�ڵ�
#define DeleteNode(pDelNode,list_name)\
{\
    list_del(&(pDelNode->list_name));\
}

// ��ȡ������ͨ������ڵ�Ľṹ�����ַ
#define list_entry(ptr, type, member) \
((type *)((char *)(ptr)-(size_t)(&((type *)0)->member)))

// ��������
#define list_for_each(pos, head) \
for (pos = (head)->next; pos != (head); pos = pos->next)

/***********************************************************
*************************variable define********************
***********************************************************/

/***********************************************************
*************************function define********************
***********************************************************/
_UNI_POINTER_EXT \
INT list_empty(IN CONST P_LIST_HEAD pHead);

_UNI_POINTER_EXT \
VOID list_add(IN CONST P_LIST_HEAD pNew, IN CONST P_LIST_HEAD pHead);

_UNI_POINTER_EXT \
VOID list_add_tail(IN CONST P_LIST_HEAD pNew, IN CONST P_LIST_HEAD pHead);

_UNI_POINTER_EXT \
VOID list_splice(IN CONST P_LIST_HEAD pList, IN CONST P_LIST_HEAD pHead);

_UNI_POINTER_EXT \
VOID list_del(IN CONST P_LIST_HEAD pEntry);

_UNI_POINTER_EXT \
VOID list_del_init(IN CONST P_LIST_HEAD pEntry);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif
