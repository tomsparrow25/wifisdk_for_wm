/*
 *  Copyright 2008-2013, Marvell International Ltd.
 *  All Rights Reserved.
 */

#ifndef _MDEV_IFLASH_H_
#define _MDEV_IFLASH_H_

#include <mdev.h>
#include <mc200_flash.h>
#include <mc200_qspi0.h>
#include <wmlog.h>

#define IFLASH_LOG(...)  wmlog("iflash", ##__VA_ARGS__)
int iflash_drv_init(void);
mdev_t *iflash_drv_open(const char *name, uint32_t flags);
int iflash_drv_close(mdev_t *dev);
int iflash_drv_eraseall(mdev_t *dev);
int iflash_drv_erase(mdev_t *dev, unsigned long start, unsigned long size);
int iflash_drv_read(mdev_t *dev, uint8_t *buf, uint32_t len, uint32_t addr);
int iflash_drv_write(mdev_t *dev, uint8_t *buf, uint32_t len, uint32_t addr);
int iflash_drv_get_id(mdev_t *dev, uint64_t *id);
#endif /* _MDEV_IFLASH_H_ */
