/*
 * ============================================================================
 * (C) Copyright 2009-2014   Marvell International Ltd.  All Rights Reserved
 *
 *                         Marvell Confidential
 * ============================================================================
 */
 /**
  * \brief define an io interface for accessing registers in a consistent manner
  * that will take care of endian issues.
  *
  * There are 2 parts to this.
  * -#  The first section gives a series of programs to convert from cpu
  *     endianess to big or little endianess.
  * -#  The last section is for read and writing to io addresses.  The first
  *     thing to do is to use ioremap to get the address of the io section
  *     The input is the address of the io block in our memory.  What this
  *     returns is the same thing.  This is added for other platforms that
  *     may require that we map from an io section to regular memory.  That
  *     address is assigned to a variable that is the type of a structure
  *     that defines the registers.  Then to use ioread32 for example do
  *     isread32(\&\<struct\>->register) the returned value is a 32 bit value
  *     that is byte swapped if needed.
  *
  */

#ifndef IO_H
#define IO_H
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef INLINE
#define INLINE __INLINE
#endif

#define SWAP32(x) ((((x) & 0xff)<<24) | (((x) & 0xff00) << 8) | (((x) & 0xff0000)>>8) | (((x)>>24) & 0xff))
#define SWAP16(x) ((((x) & 0xff)<< 8) | ((x) >> 8))

/**
 * \brief do endian conversions from cpu to data and back
 * \param[in] val the value to swap
 * \return the swapped value
 */
	extern INLINE uint32_t cpu_to_be32(uint32_t val) {
		return SWAP32(val);
	} extern INLINE uint32_t cpu_to_le32(uint32_t val) {
		return val;
	}

	extern INLINE uint32_t be32_to_cpu(uint32_t val) {
		return SWAP32(val);
	}

	extern INLINE uint32_t le32_to_cpu(uint32_t val) {
		return val;
	}

	extern INLINE uint16_t cpu_to_be16(uint16_t val) {
		return SWAP16(val);
	}

	extern INLINE uint16_t cpu_to_le16(uint16_t val) {
		return val;
	}

	extern INLINE uint16_t be16_to_cpu(uint16_t val) {
		return SWAP16(val);
	}

	extern INLINE uint16_t le16_to_cpu(uint16_t val) {
		return val;
	}

#ifdef __cplusplus
}
#endif

#endif
