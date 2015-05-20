/*
 * ============================================================================
 * Copyright (c) 2004-2014   Marvell Semiconductor, Inc. All Rights Reserved.
 *
 *                         Marvell Confidential
 * ============================================================================
 */
/**
 * \file list.c
 *
 * \brief Implementation of a doubly linked list using "containing record" macro to
 * extract nodes..
 *
 * This file contains a simple circular link list implementation.  Linked
 * list are created using a ATLISTENTRY node declared inside the structure
 * to be linked.  When a node is removed from a list the original struct is
 * recovered using the CONTAINING_RECORD macro.
 *
 * NOTE that these routines are not protected.  If you want to use the
 * lists between threads, etc you must provide your own access synchronization.
 *
 *
 * As a usage example, given the following:
 *
 * ATLISTENTRY MyList;
 * typedef struct _MyData
 * {
 *     int x;
 *     int y;
 *     ATLISTENTRY ListNode;
 * }MyData;
 *
 * MyData Data1;
 * MyData Data2;
 *
 * We could use the list as follows:
 * main()
 * {
 *     MyData* pData;
 *     ATLISTENTRY* pNode;
 *
 *     ATInitList(&MyList);
 *     ATInsertTailList(&MyList, &Data1.ListNode);
 *     ATInsertTailList(&MyList, &Data2.ListNode);
 *
 *     pNode = ATRemoveHeadList(&MyList);
 *     pData = CONTAINING_RECORD(pNode, MyData, ListNode);
 * }
 *
 **/
#include <stdio.h>
#include <incl/usb_list.h>

void ATInitNode(ATLISTENTRY * pNode)
{
	pNode->nextEntry = pNode->prevEntry = 0;
}

/*
    Name:
      ATInitList
    Description:
        This routine initializes a linked list.  The list MUST be
        initialized with this rouine before being used.
    Input:
        pListHead - The address of the head node in the list.
*/
void ATInitList(ATLISTENTRY * pListHead)
{
	pListHead->nextEntry = pListHead->prevEntry = pListHead;
}

/*
    Name:
      ATIsListEmpty
    Description:
        This routine determines whether or not a linked list is empty.
    Input:
        pListHead - The address of the main list node.
    Output:
        TRUE if list is empty; FALSE else
*/
//RD : bool ATIsListEmpty(ATLISTENTRY* pListHead)
int ATIsListEmpty(ATLISTENTRY * pListHead)
{
	return ((pListHead->nextEntry == pListHead) ? TRUE : FALSE);
}

/*
    Name:
      ATInsertHeadList
    Description:
        This routine inserts a node on the head of a list.
    Input:
        pListHead - The address of the main list node.
        pNode - The address of the node to be added.
*/
void ATInsertHeadList(ATLISTENTRY * pListHead, ATLISTENTRY * pNode)
{
	if (pNode) {
		pNode->nextEntry = pListHead->nextEntry;
		pListHead->nextEntry = pNode;
		pNode->nextEntry->prevEntry = pNode;
		pNode->prevEntry = pListHead;
	}
}

/*
    Name:
      ATInsertTailList
    Description:
        This routine inserts a node on the tail of a list.
    Input:
        pListHead - The address of the main list node.
        pNode - The address of the node to be added.
*/
void ATInsertTailList(ATLISTENTRY * pListHead, ATLISTENTRY * pNode)
{
	if (pNode) {
		pNode->nextEntry = pListHead;
		pNode->prevEntry = pListHead->prevEntry;
		pListHead->prevEntry = pNode;
		pNode->prevEntry->nextEntry = pNode;
	}
}

/*
    Name:
      ATRemoveHeadList
    Description:
        This routine removes and returns the node at the
        head of the list.
    Input:
        pListHead - The address of the main list node.
    Output:
        Returns a pointer to the node at the head of the list
        or NULL if the list is empty.
*/
ATLISTENTRY *ATRemoveHeadList(ATLISTENTRY * pListHead)
{
	ATLISTENTRY *pNode;

	// check for empty list
	if (pListHead->nextEntry == pListHead) {
		return NULL;
	}

	pNode = pListHead->nextEntry;
	pNode->nextEntry->prevEntry = pListHead;
	pListHead->nextEntry = pNode->nextEntry;
	pNode->nextEntry = 0;
	pNode->prevEntry = 0;	// not in a list any longer.
	return pNode;
}

/*
    Name:
      ATRemoveHeadList
    Description:
        This routine removes and returns the node at the
        tail of the list.
    Input:
        pListHead - The address of the main list node.
    Output:
        Returns a pointer to the node at the tail of the list
        or NULL if the list is empty.
*/
ATLISTENTRY *ATRemoveTailList(ATLISTENTRY * pListHead)
{
	ATLISTENTRY *pNode;

	if (pListHead->nextEntry == pListHead) {
		return NULL;
	}

	pNode = pListHead->prevEntry;
	pNode->prevEntry->nextEntry = pListHead;
	pListHead->prevEntry = pNode->prevEntry;
	pNode->nextEntry = 0;
	pNode->prevEntry = 0;	// not in a list any longer.
	return pNode;
}

/*
    Name:
      ATRemoveEntryList
    Description:
        This routine removes a node from whatever list it is on.
        Note that this routine will do BAD THINGS if called on
        a node that is not actually on a list.
    Input:
        pListHead - The address of the node to be removed.
*/
void ATRemoveEntryList(ATLISTENTRY * pNode)
{
	if (pNode && pNode->nextEntry && pNode->prevEntry) {
		pNode->nextEntry->prevEntry = pNode->prevEntry;
		pNode->prevEntry->nextEntry = pNode->nextEntry;
		pNode->nextEntry = 0;
		pNode->prevEntry = 0;	// not in a list any longer.
	}
}

ATLISTENTRY *ATListHead(ATLISTENTRY * head)
{
	ATLISTENTRY *node;
	node = ((void *)head->nextEntry == (void *)head) ? NULL : head->nextEntry;	// cast base derived compare.
	return node;
}

ATLISTENTRY *ATListTail(ATLISTENTRY * head)
{
	ATLISTENTRY *node;
	node = ((void *)head->prevEntry == (void *)head) ? NULL : head->prevEntry;	// cast base derived compare.
	return node;
}

ATLISTENTRY *ATListNext(ATLISTENTRY * head, ATLISTENTRY * pNode)
{
	if (pNode && (pNode->nextEntry != head))
		return pNode->nextEntry;

	return 0;		// next == head
}

ATLISTENTRY *ATListPrev(ATLISTENTRY * head, ATLISTENTRY * pNode)
{
	if (pNode && (pNode->prevEntry != head))
		return pNode->prevEntry;

	return 0;		// prev == head
}

void ATListInsertAfter(ATLISTENTRY * after, ATLISTENTRY * pNode)
{
	if (after && pNode) {
		pNode->nextEntry = after->nextEntry;
		after->nextEntry->prevEntry = pNode;
		after->nextEntry = pNode;
		pNode->prevEntry = after;
	}
}

void ATListInsertBefore(ATLISTENTRY * before, ATLISTENTRY * pNode)
{
	if (before && pNode) {
		pNode->prevEntry = before->prevEntry;
		before->prevEntry->nextEntry = pNode;
		before->prevEntry = pNode;
		pNode->nextEntry = before;
	}
}

uint32_t ATNumListNodes(ATLISTENTRY * pListHead)
{
	uint32_t nodes = 0;
	ATLISTENTRY *pNode = pListHead;

	while (pNode->nextEntry != pListHead) {
		pNode = pNode->nextEntry;
		nodes++;
	}

	return nodes;
}
