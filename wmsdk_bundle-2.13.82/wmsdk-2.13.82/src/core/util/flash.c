/*
 *  Copyright 2008-2013, Marvell International Ltd.
 *  All Rights Reserved.
 */

#include <wmtypes.h>
#include <wm_os.h>
#include <wmstdio.h>
#include <flash.h>
#include <mdev_iflash.h>
#ifdef CONFIG_XFLASH_DRIVER
#include <peripherals/xflash.h>
#endif
#ifdef CONFIG_SPI_FLASH_DRIVER
#include <peripherals/spi_flash.h>
#endif

int flash_drv_get_id(mdev_t *dev, uint64_t *id)
{
	flash_cfg *fl_cfg = (flash_cfg *) dev->private_data;

	if (id == NULL)
		return -WM_FAIL;

	switch (fl_cfg->fl_dev) {
	case FL_INT:
		return iflash_drv_get_id(dev, id);
#ifdef CONFIG_XFLASH_DRIVER
	case FL_EXT:
		/* This function is only for internal flash.
		 * So, as of now, return error if mdev device of external
		 * flash is passed.
		 */
		return -WM_FAIL;
#endif
#ifdef CONFIG_SPI_FLASH_DRIVER
	case FL_SPI_EXT:
		return spi_flash_drv_read_id(dev, (uint8_t *)id);
#endif
	default:
		FLASH_LOG("invalid flash device type: %d", fl_cfg->fl_dev);
		return -WM_FAIL;
	}
}

int flash_drv_eraseall(mdev_t *dev)
{
	flash_cfg *fl_cfg = (flash_cfg *) dev->private_data;

	switch (fl_cfg->fl_dev) {
	case FL_INT:
		return iflash_drv_eraseall(dev);
#ifdef CONFIG_XFLASH_DRIVER
	case FL_EXT:
		return xflash_drv_eraseall(dev);
#endif
#ifdef CONFIG_SPI_FLASH_DRIVER
	case FL_SPI_EXT:
		return spi_flash_drv_eraseall(dev);
#endif
	default:
		FLASH_LOG("invalid flash device type: %d", fl_cfg->fl_dev);
		return -WM_FAIL;
	}
}

int flash_drv_erase(mdev_t *dev, unsigned long start, unsigned long size)
{
	flash_cfg *fl_cfg = (flash_cfg *) dev->private_data;

	switch (fl_cfg->fl_dev) {
	case FL_INT:
		return iflash_drv_erase(dev, start, size);
#ifdef CONFIG_XFLASH_DRIVER
	case FL_EXT:
		return xflash_drv_erase(dev, start, size);
#endif
#ifdef CONFIG_SPI_FLASH_DRIVER
	case FL_SPI_EXT:
		return spi_flash_drv_erase(dev, start, size);
#endif
	default:
		FLASH_LOG("invalid flash device type: %d", fl_cfg->fl_dev);
		return -WM_FAIL;
	}
}

int flash_drv_read(mdev_t *dev, uint8_t *buf,
		   uint32_t len, uint32_t addr)
{
	flash_cfg *fl_cfg = (flash_cfg *) dev->private_data;

	switch (fl_cfg->fl_dev) {
	case FL_INT:
		return iflash_drv_read(dev, buf, len, addr);
#ifdef CONFIG_XFLASH_DRIVER
	case FL_EXT:
		return xflash_drv_read(dev, buf, len, addr);
#endif
#ifdef CONFIG_SPI_FLASH_DRIVER
	case FL_SPI_EXT:
		return spi_flash_drv_read(dev, buf, len, addr);
#endif
	default:
		FLASH_LOG("invalid flash device type: %d", fl_cfg->fl_dev);
		return -WM_FAIL;
	}
}

int flash_drv_write(mdev_t *dev, uint8_t *buf,
		    uint32_t len, uint32_t addr)
{
	flash_cfg *fl_cfg = (flash_cfg *) dev->private_data;

	switch (fl_cfg->fl_dev) {
	case FL_INT:
		return iflash_drv_write(dev, buf, len, addr);
#ifdef CONFIG_XFLASH_DRIVER
	case FL_EXT:
		return xflash_drv_write(dev, buf, len, addr);
#endif
#ifdef CONFIG_SPI_FLASH_DRIVER
	case FL_SPI_EXT:
		return spi_flash_drv_write(dev, buf, len, addr);
#endif
	default:
		FLASH_LOG("invalid flash device type: %d", fl_cfg->fl_dev);
		return -WM_FAIL;
	}
}

int flash_drv_close(mdev_t *dev)
{
	flash_cfg *fl_cfg = (flash_cfg *) dev->private_data;

	switch (fl_cfg->fl_dev) {
	case FL_INT:
		return iflash_drv_close(dev);
#ifdef CONFIG_XFLASH_DRIVER
	case FL_EXT:
		return xflash_drv_close(dev);
#endif
#ifdef CONFIG_SPI_FLASH_DRIVER
	case FL_SPI_EXT:
		return spi_flash_drv_close(dev);
#endif
	default:
		FLASH_LOG("invalid flash device type: %d", fl_cfg->fl_dev);
		return -WM_FAIL;
	}
}

mdev_t *flash_drv_open(int fl_dev)
{
	mdev_t *mdev_p;

	switch (fl_dev) {
	case FL_INT:
		mdev_p =  iflash_drv_open("iflash", 0);
		break;
#ifdef CONFIG_XFLASH_DRIVER
	case FL_EXT:
		mdev_p = xflash_drv_open("xflash", 0);
		break;
#endif
#ifdef CONFIG_SPI_FLASH_DRIVER
	case FL_SPI_EXT:
		mdev_p = spi_flash_drv_open("spi_flash", 0);
		break;
#endif
	default:
		FLASH_LOG("invalid flash device type: %d", fl_dev);
		mdev_p = NULL;
		break;
	}

	if (mdev_p == NULL)
		FLASH_LOG("driver open called without registering device");

	return mdev_p;
}

void flash_drv_init(void)
{
	iflash_drv_init();
#ifdef CONFIG_SPI_FLASH_DRIVER
	spi_flash_drv_init();
#endif
#ifdef CONFIG_XFLASH_DRIVER
	xflash_drv_init();
#endif
}
