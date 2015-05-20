#if !defined (WL_MACROS_H__)
#define WL_MACROS_H__
/*
 *                Copyright 2003, Marvell Semiconductor, Inc.
 * This code contains confidential information of Marvell Semiconductor, Inc.
 * No rights are granted herein under any patent, mask work right or copyright
 * of Marvell or any third party.
 * Marvell reserves the right at its sole discretion to request that this code
 * be immediately returned to Marvell. This code is provided "as is".
 * Marvell makes no warranties, express, implied or otherwise, regarding its
 * accuracy, completeness or performance.
 */

/*!
 * \file  wl_macros.h
 * \brief Common macros are defined here
 *
 *  Must include "wltypes.h" before this file 
 */

#define MACRO_START          do {
#define MACRO_END            } while (0)

#define WL_REGS8(x)     (*(volatile unsigned char *)(x))
#define WL_REGS16(x)    (*(volatile unsigned short *)(x))
#define WL_REGS32(x)    (*(volatile unsigned long *)(x))

#define WL_READ_REGS8(reg,val)      ((val) = WL_REGS8(reg))
#define WL_READ_REGS16(reg,val)     ((val) = WL_REGS16(reg))
#define WL_READ_REGS32(reg,val)     ((val) = WL_REGS32(reg))
#define WL_READ_BYTE(reg,val)       ((val) = WL_REGS8(reg))
#define WL_READ_HWORD(reg,val)      ((val) = WL_REGS16(reg)) /*half word; */
/*16bits */
#define WL_READ_WORD(reg,val)       ((val) = WL_REGS32(reg)) /*32 bits */
#define WL_WRITE_REGS8(reg,val)     (WL_REGS8(reg) = (val))
#define WL_WRITE_REGS16(reg,val)    (WL_REGS16(reg) = (val))
#define WL_WRITE_REGS32(reg,val)    (WL_REGS32(reg) = (val))
#define WL_WRITE_BYTE(reg,val)      (WL_REGS8(reg) = (val))
#define WL_WRITE_HWORD(reg,val)     (WL_REGS16(reg) = (val))  /*half word; */
/*16bits */
#define WL_WRITE_WORD(reg,val)      (WL_REGS32(reg) = (val))  /*32 bits */
#define WL_REGS8_SETBITS(reg, val)  (WL_REGS8(reg) |= (UINT8)(val))
#define WL_REGS16_SETBITS(reg, val) (WL_REGS16(reg) |= (UINT16)(val))
#define WL_REGS32_SETBITS(reg, val) (WL_REGS32(reg) |= (val))

#define WL_REGS8_CLRBITS(reg, val)  (WL_REGS8(reg) = \
                                     (UINT8)(WL_REGS8(reg)&~(val)))

#define WL_REGS16_CLRBITS(reg, val) (WL_REGS16(reg) = \
                                     (UINT16)(WL_REGS16(reg)&~(val)))

#define WL_REGS32_CLRBITS(reg, val) (WL_REGS32(reg) = \
                                     (WL_REGS32(reg)&~(val)))

#define WL_WRITE_CHUNK(dst, src, length) (memcpy((void*) (dst), \
                                          (void*) (src), (length)))
/*!
 * Bitmask macros
 */
#define WL_BITMASK(nbits)           ((0x1 << nbits) - 1)


/*!
 * Macro to put the WLAN SoC into sleep mode
 */
#define WL_GO_TO_SLEEP   asm volatile ("MCR p15, 0, r3, c7, c0, 4;")

/*!
 * BE vs. LE macros
 */
#ifdef BE /* Big Endian */
#define SHORT_SWAP(X) (X)
#define WORD_SWAP(X) (X)
#define LONG_SWAP(X) ((l64)(X))
#else    /* Little Endian */

#define SHORT_SWAP(X) ((X <<8 ) | (X >> 8))     //!< swap bytes in a 16 bit short

#define WORD_SWAP(X) (((X)&0xff)<<24)+      \
                    (((X)&0xff00)<<8)+      \
                    (((X)&0xff0000)>>8)+    \
                    (((X)&0xff000000)>>24)      //!< swap bytes in a 32 bit word

#define LONG_SWAP(X) ( (l64) (((X)&0xffULL)<<56)+               \
                            (((X)&0xff00ULL)<<40)+              \
                            (((X)&0xff0000ULL)<<24)+            \
                            (((X)&0xff000000ULL)<<8)+           \
                            (((X)&0xff00000000ULL)>>8)+         \
                            (((X)&0xff0000000000ULL)>>24)+      \
                            (((X)&0xff000000000000ULL)>>40)+    \
                            (((X)&0xff00000000000000ULL)>>56))      //!< swap bytes in a 64 bit long
#endif

/*!
 * Alignment macros
 */
#define ALIGN4(x)       (((x) + 3) & ~3)
#define ALIGN4BYTE(x)   (x=ALIGN4(x))
#define ROUNDUP4US(x)   (ALIGN4(x))



#ifndef min
#define min(a,b)            (((a) < (b)) ? (a) : (b))
#endif
#ifndef max
#define max(a,b)            (((a) > (b)) ? (a) : (b))
#endif

#ifndef MIN
#define MIN(a,b)            (((a) < (b)) ? (a) : (b))
#endif
#ifndef MAX
#define MAX(a,b)            (((a) > (b)) ? (a) : (b))
#endif

#ifndef NELEMENTS
#define NELEMENTS(x) (sizeof(x)/sizeof(x[0]))
#endif

#define HWORD_LOW_BYTE(x)   ((x) & 0xFF)
#define HWORD_HIGH_BYTE(x)  (((x) >> 8) & 0xFF)

#define htons(x) (UINT16)SHORT_SWAP(x)
#define htonl(x) (UINT32)WORD_SWAP(x)

#define ntohs(x) (UINT16)SHORT_SWAP(x)
#define ntohl(x) (UINT32)WORD_SWAP(x)

#define CEIL_aByb(a, b) ((a + b - 1) / b)

#endif /* _WL_MACROS_H_ */
