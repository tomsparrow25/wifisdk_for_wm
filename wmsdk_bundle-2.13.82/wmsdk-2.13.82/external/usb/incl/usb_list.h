/*
 * ============================================================================
 * Copyright (c) 2004-2010   Marvell Semiconductor, Inc. All Rights Reserved.
 *
 *                         Marvell Confidential
 * ============================================================================
 */
 /**
  * \file list.h
  *
  * \brief This is the interface header file for list.c
  *
  */

#ifndef USB_LIST_H
#define USB_LIST_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#undef FALSE
#define FALSE 0
#undef TRUE
#define TRUE 1


/**
 * This macro used to be written as:
 * \code
 * #define CONTAINING_RECORD(address, type, field) ((type *)((char *)(address)
 * - offsetof((type), (field))))
 * \endcode
 * But, the arm compiler could not deal with having the offsetof macro in the
 * CONTAINING_RECORD macro so it was rewritten as below which just directly
 * includes the offsetof code.
 *
 * \warning Address must be an address not a function returning a pointer.
 * NULL address returns NULL record.
 */
#define CONTAINING_RECORD(address, type, field) ((address) ? ((type*)( ((char*)(address)) - ((unsigned int)((char *)&(((type *)0)->field) - (char *)0)))) : 0 )

	typedef struct ATLISTENTRY_s ATLISTENTRY;
/** This struct serves as both the list head and the list nodes */
	struct ATLISTENTRY_s {
		ATLISTENTRY *nextEntry;	///< Next node on list
		ATLISTENTRY *prevEntry;	///< Previous node on list
	};
///< Initialize a node.
	void ATInitNode(ATLISTENTRY * pNode);

///< Initialize a list.
	void ATInitList(ATLISTENTRY * pListHead);

///< Return true if list is empty.
//RD :bool ATIsListEmpty(ATLISTENTRY* pListHead);
	int ATIsListEmpty(ATLISTENTRY * pListHead);

///< Insert as the first element of a list.
	void ATInsertHeadList(ATLISTENTRY * pListHead, ATLISTENTRY * pNode);

///< Insert as the last element of a list.
	void ATInsertTailList(ATLISTENTRY * pListHead, ATLISTENTRY * pNode);

///< Remove and return the first element of the list.
	ATLISTENTRY *ATRemoveHeadList(ATLISTENTRY * pListHead);

///< Remove and return the last element of a list.
	ATLISTENTRY *ATRemoveTailList(ATLISTENTRY * pListHead);

///< This will remove the element from a list, any list it happens to be on.
	void ATRemoveEntryList(ATLISTENTRY * pNode);

///< Return pointer to first element on list.
	ATLISTENTRY *ATListHead(ATLISTENTRY * pListHead);
#define ATFirstListEntry ATListHead

///< Return pointer to the element following this one.
	ATLISTENTRY *ATListNext(ATLISTENTRY * pListHead, ATLISTENTRY * pNode);
#define ATNextListEntry ATListNext

///< Return pointer to the element before this one.
	ATLISTENTRY *ATListPrev(ATLISTENTRY * pListHead, ATLISTENTRY * pNode);
#define ATPrevListEntry ATListPrev

///< Return pointer to last element in list.
	ATLISTENTRY *ATListTail(ATLISTENTRY * pListHead);
#define ATLastListEntry ATListTail

///< Insert element after this one.
	void ATListInsertAfter(ATLISTENTRY * after, ATLISTENTRY * pNode);

///< Insert element before this one.
	void ATListInsertBefore(ATLISTENTRY * before, ATLISTENTRY * pNode);

///< Run a for loop to count the number of elements in list.
	uint32_t ATNumListNodes(ATLISTENTRY * pListHead);

#ifdef __cplusplus
}
#endif
#endif
