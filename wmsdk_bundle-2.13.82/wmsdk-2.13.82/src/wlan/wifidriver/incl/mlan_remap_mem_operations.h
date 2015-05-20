/*
 * File added for wmsdk. Not present in original mlan release.
 * 
 * Purpose: The mlan release source files contain non-standard (libc)
 * prototypes for memcpy, memmove, memcmp, etc. This causes problems when
 * mlan header files are included by the remaining code. We work around
 * this problem by remapping these operations to standard functions.
 *
 * IMPORTANT: Ensure that this file is included in every mlan source file
 * which used mem* operations. ENSURE that this is the last file included
 * in the include header list.
 */


#ifdef memset
#undef memset
#endif
/** Memset routine */
#define memset(adapter, s, c, len)		\
	memset(s, c, len)

#ifdef memmove
#undef memmove
#endif
/** Memmove routine */
#define memmove(adapter, dest, src, len)	\
	memmove(dest, src, len)

#ifdef memcpy
#undef memcpy
#endif
/** Memcpy routine */
#define memcpy(adapter, to, from, len)		\
	memcpy(to, from, len)

#ifdef memcmp
#undef memcmp
#endif
/** Memcmp routine */
#define memcmp(adapter, s1, s2, len)		\
	memcmp(s1, s2, len)
