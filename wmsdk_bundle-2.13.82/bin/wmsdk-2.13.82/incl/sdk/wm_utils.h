/*
 * Copyright 2008-2013, Marvell International Ltd.
 * All Rights Reserved.
 */

#ifndef _UTIL_H_
#define _UTIL_H_

#include <wmtypes.h>
#include <stddef.h>
/**
 * container_of - cast a member of a structure out to the containing structure
 * @ptr:	the pointer to the member.
 * @type:	the type of the container struct this is embedded in.
 * @member:	the name of the member within the struct.
 *
 */
#define container_of(ptr, type, member) ({			\
	const typeof( ((type *)0)->member ) *__ptr = (ptr);	\
	(type *)( (char *)__ptr - offsetof(type, member) );})

#ifdef __GNUC__

#define WARN_UNUSED_RET __attribute__ ((warn_unused_result))
#define WEAK __attribute__ ((weak))

#ifndef PACK_START
#define PACK_START
#endif
#ifndef PACK_END
#define PACK_END __attribute__((packed))
#endif
#define NORETURN __attribute__ ((noreturn))

#else /* __GNUC__ */

#define WARN_UNUSED_RET
#define WEAK __weak

#define PACK_START __packed
#define PACK_END
#define NORETURN

#endif /* __GNUC__ */

NORETURN void wmpanic(void);

/**
 * Convert a given hex string to a equivalent binary representation.
 *
 * E.g. If your input string of 4 bytes is {'F', 'F', 'F', 'F'} the output
 * string will be of 2 bytes {255, 255} or to put the same in other way
 * {0xFF, 0xFF}
 *
 * Note that hex2bin is not the same as strtoul as the latter will properly
 * return the integer in the correct machine binary format viz. little
 * endian. hex2bin however does only in-place like replacement of two ASCII
 * characters to one binary number taking 1 byte in memory.
 */
unsigned int hex2bin(const uint8_t *ibuf, uint8_t *obuf,
		     unsigned max_olen);
void bin2hex(uint8_t *src, char *dest, unsigned int src_len,
		unsigned int dest_len);
unsigned int sys_get_epoch();
void sys_set_epoch(unsigned int epoch_val);

/* Get random sequence in buf upto size bytes */
typedef uint32_t (*random_hdlr_t)(void);
int random_register_handler(random_hdlr_t func);
int random_unregister_handler(random_hdlr_t func);
void get_random_sequence(unsigned char *buf, unsigned int size);

void dump_hex(const void *data, unsigned len);
void dump_hex_ascii(const void *data, unsigned len);
#endif
