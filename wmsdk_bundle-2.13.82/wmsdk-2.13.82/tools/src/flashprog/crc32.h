#ifndef __CRC32_H
#define __CRC32_H

#include <stdint.h>

uint32_t crc32(void *__data, int data_size, uint32_t crc);

#endif
