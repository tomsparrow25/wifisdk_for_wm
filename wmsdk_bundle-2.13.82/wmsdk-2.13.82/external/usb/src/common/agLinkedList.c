/***********************************************************
* (c) Copyright 2008-2014 Marvell International Ltd.
*
*               Marvell Confidential
* ==========================================================
*/

/******************************************************************************
 *
 * Description:  This implements a linked list data structure.
 *
 *****************************************************************************/
#include <stdint.h>
#include <incl/agLinkedList.h>
#include <incl/cpu_interrupt.h>

/*
 ** agAddTail
 *
 *  PARAMETERS:
        LinkHead    This is the list to be added to
        NewMember   This is the item to add to the list
 *
 *  DESCRIPTION:
        Add NewMember to the end of the list pointed to by LinkHead
 *
 *  RETURNS:
 *
 */

void agAddTail(LINK_HEADER * LinkHead, LINK_MEMBER * NewMember)
{
	CPU_INTER_SAVE;

	CPU_INTER_DISABLE;

	if (LinkHead->LinkHead == NULL) {
		// the list is empty, init the head pointer also.
		LinkHead->LinkHead = NewMember;
		LinkHead->LinkTail = NewMember;
		NewMember->NextLink = NULL;
		CPU_INTER_ENABLE;

		return;
	}
	LinkHead->LinkTail->NextLink = NewMember;
	LinkHead->LinkTail = NewMember;
	NewMember->NextLink = NULL;
	CPU_INTER_ENABLE;
}

/*
 ** Function Name: agAddHead
 *
 * PARAMETERS: LinkHead  the headpointer to add
               NewMember The link member to add to the head of LinkHead
 *
 * DESCRIPTION:  This adds a new member to the head of a linked list.
 *
 * RETURNS: Nothing
 *
 */
void agAddHead(LINK_HEADER * LinkHead, LINK_MEMBER * NewMember)
{
	CPU_INTER_SAVE;

	CPU_INTER_DISABLE;

	//if list is empty, NewMember will be the tail
	//else tail stays the same
	if (LinkHead->LinkHead == NULL) {
		LinkHead->LinkTail = NewMember;
	}
	NewMember->NextLink = LinkHead->LinkHead;
	LinkHead->LinkHead = NewMember;
	CPU_INTER_ENABLE;
}

/*
 ** agDelHead
 *
 *  PARAMETERS:
            LinkHead    Pointer to the list to remove the member.
 *
 *  DESCRIPTION:
            This removes the first member on a linked list.
 *
 *  RETURNS:
            A pointer to the linked member that was removed.
 *
 */

LINK_MEMBER *agDelHead(LINK_HEADER * LinkHead)
{
	LINK_MEMBER *Temp;

	CPU_INTER_SAVE;

	CPU_INTER_DISABLE;
	if (LinkHead->LinkHead == NULL) {
		CPU_INTER_ENABLE;

		return (NULL);
	}
	Temp = LinkHead->LinkHead;
	LinkHead->LinkHead = LinkHead->LinkHead->NextLink;
	if (LinkHead->LinkHead == NULL)
		LinkHead->LinkTail = NULL;
	CPU_INTER_ENABLE;
	return Temp;
}

/*
** agDelItem
*
*  PARAMETERS: Head:   The start of the linked list
               TempLink    The link to remove from the list.
*
*  DESCRIPTION:   Given a linked list, remove the item that is send in the 2nd parameter
*
*  RETURNS:
*
*/

uint32_t agDelItem(LINK_HEADER * Head, LINK_MEMBER * TempLink)
{
	LINK_MEMBER *Temp, *Last;

	Temp = agGetHead(Head);
	Last = Temp;
	while (Temp != NULL) {
		if (Temp == TempLink) {
			// We found the item, now remove it.
			//
			if (Temp == Head->LinkHead) {
				agDelHead(Head);
				return (1);
			} else if (Temp == Head->LinkTail) {
				agDelTail(Head);
				return (1);
			} else {
				Last->NextLink = Temp->NextLink;
				return (1);
			}
			return (0);
		}
		Last = Temp;
		Temp = Temp->NextLink;
	}
	return (0);
}

/*
** agDelItemDirect
*
*  PARAMETERS: Head:   The start of the linked list
               TempLink    The link to remove from the list.
               PrevLink     The link preceeding TempLink.
*
*  DESCRIPTION:   Given a linked list, remove the item that is send in the 2nd parameter. Uses the PrevLink param
*                            to efficiently reform the list.
*
*  RETURNS:
*
*/
uint32_t agDelItemDirect(LINK_HEADER * Head, LINK_MEMBER * TempLink,
		       LINK_MEMBER * PrevLink)
{
	if (NULL != TempLink) {
		// Remove the item
		if (TempLink == Head->LinkHead) {
			agDelHead(Head);
			return (1);
		} else if (TempLink == Head->LinkTail) {
			agDelTail(Head);
			return (1);
		} else if ((NULL != PrevLink) &&
			   (PrevLink->NextLink == TempLink)) {
			PrevLink->NextLink = TempLink->NextLink;
			return (1);
		}
	}
	return (0);
}

/*
 ** Function Name: agDelTail
 *
 * PARAMETERS:  LinkHead  The head pointer of the linked list to work on.
 *
 * DESCRIPTION:  Remove the item on the end of the list and return it.
 *
 * RETURNS:  A pointer to the item removed.
 *
 */

LINK_MEMBER *agDelTail(LINK_HEADER * LinkHead)
{
	LINK_MEMBER *Temp;

	CPU_INTER_SAVE;

	CPU_INTER_DISABLE;

	if (LinkHead->LinkHead == NULL) {
		Temp = NULL;
	} else if (LinkHead->LinkHead == LinkHead->LinkTail) {
		Temp = LinkHead->LinkHead;
		LinkHead->LinkHead = NULL;
		LinkHead->LinkTail = NULL;
	} else {
		LINK_MEMBER *newTail;
		newTail = LinkHead->LinkHead;
		while (newTail->NextLink != LinkHead->LinkTail) {
			newTail = newTail->NextLink;
		}

		Temp = newTail->NextLink;
		newTail->NextLink = NULL;
		LinkHead->LinkTail = newTail;
	}
	CPU_INTER_ENABLE;
	return Temp;
}

/*
 ** Function Name: agGetHead
 *
 * PARAMETERS:  LinkHead  The pointer to the linked list we are interested in.
 *
 * DESCRIPTION:  Get the head of a linked list and return it.
 *
 * RETURNS:  The pointer to the linked list member at the head of the list.
 *
 */
LINK_MEMBER *agGetHead(const LINK_HEADER * LinkHead)
{
	return LinkHead->LinkHead;
}

/*
 ** Function Name: agGetTail
 *
 * PARAMETERS: LinkHead  The pointer to the linked list we are interested in.
 *
 * DESCRIPTION:  Get the tail of a linked list and return it.
 *
 * RETURNS: The pointer to the linked list member at the tail of the list.
 *
 */
LINK_MEMBER *agGetTail(const LINK_HEADER * LinkHead)
{
	return LinkHead->LinkTail;
}

/*
 ** Function Name: agLinkInit
 *
 * PARAMETERS:  LinkHead  The linked list to init.
 *
 * DESCRIPTION:  Initialize the linked list header.
 *
 * RETURNS: nothing.
 *
 */
void agLinkInit(LINK_HEADER * LinkHead)
{
	LinkHead->LinkHead = NULL;
	LinkHead->LinkTail = NULL;
}
