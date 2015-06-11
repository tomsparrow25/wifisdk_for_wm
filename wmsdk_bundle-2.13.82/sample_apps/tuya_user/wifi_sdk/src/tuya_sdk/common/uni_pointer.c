/***********************************************************
*  File: uni_pointer.c
*  Author: nzy
*  Date: 120427
***********************************************************/
#define _UNI_POINTER_GLOBAL
#include "uni_pointer.h"

/***********************************************************
*************************micro define***********************
***********************************************************/

/***********************************************************
*************************variable define********************
***********************************************************/

/***********************************************************
*************************function define********************
***********************************************************/
/***********************************************************
*  Function: __list_add 前后两节点之间插入一个新的节点
*  Input: pNew->新加入的节点
*         pPrev->前节点
*         pNext->后节点
*  Output: none
*  Return: none
*  Date: 120427
***********************************************************/
STATIC VOID __list_add(IN CONST P_LIST_HEAD pNew, IN CONST P_LIST_HEAD pPrev,\
                       IN CONST P_LIST_HEAD pNext)
{
    pNext->prev = pNew;
    pNew->next = pNext;
    pNew->prev = pPrev;
    pPrev->next = pNew;
}

/***********************************************************
*  Function: __list_del 将前后两节点之间的节点移除
*  Input: pPrev->前节点
*         pNext->后节点
*  Output: none
*  Return: none
*  Date: 120427
***********************************************************/
STATIC VOID __list_del(IN CONST P_LIST_HEAD pPrev, IN CONST P_LIST_HEAD pNext)
{
    pNext->prev = pPrev;
    pPrev->next = pNext;
}

/***********************************************************
*  Function: list_empty 判断该链表是否为空
*  Input: pHead
*  Output: none
*  Return: none
*  Date: 120427
***********************************************************/
INT list_empty(IN CONST P_LIST_HEAD pHead)
{
    return pHead->next == pHead;
}

/***********************************************************
*  Function: list_add 插入一个新的节点
*  Input: pNew->新节点
*         pHead->插入点
*  Output: none
*  Return: none
*  Date: 120427
***********************************************************/
VOID list_add(IN CONST P_LIST_HEAD pNew, IN CONST P_LIST_HEAD pHead)
{
    __list_add(pNew, pHead, pHead->next);
}

/***********************************************************
*  Function: list_add_tail 反向插入一个新的节点
*  Input: pNew->新节点
*         pHead->插入点
*  Output: none
*  Return: none
*  Date: 120427
***********************************************************/
VOID list_add_tail(IN CONST P_LIST_HEAD pNew, IN CONST P_LIST_HEAD pHead)
{
    __list_add(pNew, pHead->prev, pHead);
}

/***********************************************************
*  Function: list_splice 接合两条链表
*  Input: pList->被接合链
*         pHead->接合链
*  Output: none
*  Return: none
*  Date: 120427
***********************************************************/
VOID list_splice(IN CONST P_LIST_HEAD pList, IN CONST P_LIST_HEAD pHead)
{
    P_LIST_HEAD pFirst = pList->next;

    if (pFirst != pList) // 该链表不为空
    {
        P_LIST_HEAD pLast = pList->prev;
        P_LIST_HEAD pAt = pHead->next; // list接合处的节点

        pFirst->prev = pHead;
        pHead->next = pFirst;
        pLast->next = pAt;
        pAt->prev = pLast;
    }
}

/***********************************************************
*  Function: list_del 链表中删除节点
*  Input: pEntry->待删除节点
*  Output: none
*  Return: none
*  Date: 120427
***********************************************************/
VOID list_del(IN CONST P_LIST_HEAD pEntry)
{
    __list_del(pEntry->prev, pEntry->next);
}

/***********************************************************
*  Function: list_del 链表中删除某节点并初始化该节点
*  Input: pEntry->待删除节点
*  Output: none
*  Return: none
*  Date: 120427
***********************************************************/
VOID list_del_init(IN CONST P_LIST_HEAD pEntry)
{
    __list_del(pEntry->prev, pEntry->next);
    INIT_LIST_HEAD(pEntry);
}

