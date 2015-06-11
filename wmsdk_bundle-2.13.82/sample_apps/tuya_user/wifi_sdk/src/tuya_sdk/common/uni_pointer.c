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
*  Function: __list_add ǰ�����ڵ�֮�����һ���µĽڵ�
*  Input: pNew->�¼���Ľڵ�
*         pPrev->ǰ�ڵ�
*         pNext->��ڵ�
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
*  Function: __list_del ��ǰ�����ڵ�֮��Ľڵ��Ƴ�
*  Input: pPrev->ǰ�ڵ�
*         pNext->��ڵ�
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
*  Function: list_empty �жϸ������Ƿ�Ϊ��
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
*  Function: list_add ����һ���µĽڵ�
*  Input: pNew->�½ڵ�
*         pHead->�����
*  Output: none
*  Return: none
*  Date: 120427
***********************************************************/
VOID list_add(IN CONST P_LIST_HEAD pNew, IN CONST P_LIST_HEAD pHead)
{
    __list_add(pNew, pHead, pHead->next);
}

/***********************************************************
*  Function: list_add_tail �������һ���µĽڵ�
*  Input: pNew->�½ڵ�
*         pHead->�����
*  Output: none
*  Return: none
*  Date: 120427
***********************************************************/
VOID list_add_tail(IN CONST P_LIST_HEAD pNew, IN CONST P_LIST_HEAD pHead)
{
    __list_add(pNew, pHead->prev, pHead);
}

/***********************************************************
*  Function: list_splice �Ӻ���������
*  Input: pList->���Ӻ���
*         pHead->�Ӻ���
*  Output: none
*  Return: none
*  Date: 120427
***********************************************************/
VOID list_splice(IN CONST P_LIST_HEAD pList, IN CONST P_LIST_HEAD pHead)
{
    P_LIST_HEAD pFirst = pList->next;

    if (pFirst != pList) // ������Ϊ��
    {
        P_LIST_HEAD pLast = pList->prev;
        P_LIST_HEAD pAt = pHead->next; // list�Ӻϴ��Ľڵ�

        pFirst->prev = pHead;
        pHead->next = pFirst;
        pLast->next = pAt;
        pAt->prev = pLast;
    }
}

/***********************************************************
*  Function: list_del ������ɾ���ڵ�
*  Input: pEntry->��ɾ���ڵ�
*  Output: none
*  Return: none
*  Date: 120427
***********************************************************/
VOID list_del(IN CONST P_LIST_HEAD pEntry)
{
    __list_del(pEntry->prev, pEntry->next);
}

/***********************************************************
*  Function: list_del ������ɾ��ĳ�ڵ㲢��ʼ���ýڵ�
*  Input: pEntry->��ɾ���ڵ�
*  Output: none
*  Return: none
*  Date: 120427
***********************************************************/
VOID list_del_init(IN CONST P_LIST_HEAD pEntry)
{
    __list_del(pEntry->prev, pEntry->next);
    INIT_LIST_HEAD(pEntry);
}

