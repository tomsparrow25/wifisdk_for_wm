/*
 * **********************************************************
 * (c) Copyright 2008-2012 Marvell International Ltd.
 *
 *               Marvell Confidential
 * ==========================================================
 */
/**
 * \file agLinkedList.h
 *
 * \brief Linked Lists
 *
 */

#ifndef AGLINKEDLIST_H
#define AGLINKEDLIST_H

#ifdef __cplusplus
extern "C" {
#endif

/** Link Member */
	typedef struct LINK_MEMBER1 {
		struct LINK_MEMBER1 *NextLink;
		uint32_t Param;
		uint32_t Param1;	/* Different parameters depends on application.*/
		void *Data;	/* Where the data is located. */
	} LINK_MEMBER;

/** Define a link Member */
	typedef struct {
		LINK_MEMBER *LinkHead;
		LINK_MEMBER *LinkTail;
	} LINK_HEADER;

	void agAddTail(LINK_HEADER *, LINK_MEMBER *);
	void agAddHead(LINK_HEADER *, LINK_MEMBER *);
/**
 * \brief returns the link removed from the list.
 */
	LINK_MEMBER *agDelHead(LINK_HEADER *);
/**
 * \brief returns the link removed from the tail.
 */
	LINK_MEMBER *agDelTail(LINK_HEADER *);
	LINK_MEMBER *agGetHead(const LINK_HEADER *);
	LINK_MEMBER *agGetTail(const LINK_HEADER *);
	uint32_t agDelItem(LINK_HEADER *, LINK_MEMBER *);
	uint32_t agDelItemDirect(LINK_HEADER *, LINK_MEMBER *, LINK_MEMBER *);

	void agLinkInit(LINK_HEADER *);

#ifdef __cplusplus
}
#endif
#endif
