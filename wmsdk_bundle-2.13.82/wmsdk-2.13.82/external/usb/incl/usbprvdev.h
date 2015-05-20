/*
 *
 * ============================================================================
 * Portions Copyright (c) 2008-2010   Marvell International, Ltd. All Rights Reserved.
 *
 *                         Marvell Confidential
 * ============================================================================
 */

#ifndef __usbprv_h__
#define __usbprv_h__ 1
/*******************************************************************************
** File          : $HeadURL$
** Author        : $Author: rasbury $
** Project       : HSCTRL
** Instances     :
** Creation date :
********************************************************************************
********************************************************************************
** ChipIdea Microelectronica - IPCS
** TECMAIA, Rua Eng. Frederico Ulrich, n 2650
** 4470-920 MOREIRA MAIA
** Portugal
** Tel: +351 229471010
** Fax: +351 229471011
** e_mail: chipidea.com
********************************************************************************
** ISO 9001:2000 - Certified Company
** (C) 2005 Copyright Chipidea(R)
** Chipidea(R) - Microelectronica, S.A. reserves the right to make changes to
** the information contained herein without notice. No liability shall be
** incurred as a result of its use or application.
********************************************************************************
** Modification history:
** $Date: 2007/08/01 $
** $Revision: #5 $
*******************************************************************************
*** Comments:
***   This file contains the internal USB specific type definitions
***
**************************************************************************
**END*********************************************************/
#include <stdlib.h>
#include <string.h>
#include <wm_os.h>

#ifdef __cplusplus
extern "C" {
#endif

/* haitao 03/14/12 */
#define USB_device_lock()       _disable_device_interrupts()
#define USB_device_unlock()     _enable_device_interrupts()

static inline void *usb_mem_alloc_aligned(size_t size, size_t alignment)
{
	void *ptr1, *ptr2;
	ptr1 = (void *) pvPortMalloc(size + (alignment - 1) + sizeof(void *));
	if (ptr1 != NULL) {
		ptr2 = (void *) (((uint32_t)ptr1 + sizeof(void *)
			+ (alignment - 1)) & ~(alignment - 1));
		/* Store the actual malloc pointer
		which will be used in freeing the memory */
		*((void **)ptr2-1) = ptr1;
		return ptr2;
	}
	return NULL;
}

static inline void *usb_mem_remalloc_aligned(uint8_t **srcPtr, uint32_t size)
{
	void *p;
	uint32_t *ptmp, **stmp, i;
	stmp = (uint32_t **)(srcPtr);
	p = usb_mem_alloc_aligned(size, 4);
	if (p == NULL) {
		wmprintf("Failed to allocate memory\r\n");
		return NULL;
	}
	ptmp = (uint32_t *)p;
	for (i = 0; i < ((size / 4) - 1); i++)
		*(ptmp++) = (uint32_t)(*(stmp++));

	os_mem_free(*((void **)srcPtr-1));
	return p;
}

static inline void usb_memfree_and_null(void *p)
{
	os_mem_free(p);
	p = NULL;
}

static inline void *usb_mem_calloc(size_t nmemb, size_t size)
{
	if ((nmemb == 0) || (size == 0))
		return NULL;

	return os_mem_alloc(nmemb*size);
}


#define USB_memalloc(n)                  os_mem_alloc(n)
#define USB_memcalloc(n, m)                  usb_mem_calloc(n, m)
#define USB_memfree_and_null(n)          usb_memfree_and_null(n)
#define USB_memalloc_aligned(n, a)       usb_mem_alloc_aligned(n, a)
#define USB_memfree(ptr)                 os_mem_free(ptr)
#define mem_remalloc(ptr, a)		     usb_mem_remalloc_aligned(ptr, a)

#define USB_memzero(ptr, n)                   memset(ptr, 0, n)
#define USB_memcopy(src, dst, n)               memcpy(dst, src, n)

#ifdef ENABLE_DCACHE
#define USB_dcache_invalidate_mlines(p, n)    cpu_dcache_invalidate_region(p, n)
#define USB_dcache_flush_mlines(p, n)    	 cpu_dcache_writeback_region(p, n)
#else
#define USB_dcache_invalidate_mlines(p, n)
#define USB_dcache_flush_mlines(p, n)
#endif

#define USB_install_isr(vector, isr, data)   ((void)0)

#ifdef __cplusplus
}
#endif
#endif
/* EOF */
