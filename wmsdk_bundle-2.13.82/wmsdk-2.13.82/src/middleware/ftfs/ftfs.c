/* 
 *  Copyright 2008-2013, Marvell International Ltd.
 *  All Rights Reserved.
 */

#include <string.h>

#include <wmstdio.h>
#include <ftfs.h>

static char magic[8] = FT_MAGIC;

int ft_fclose(file *f)
{
	unsigned int i;
	struct ftfs_super *sb = f_to_ftfs_sb(f);

	if (!f)
		return -WM_E_FTFS_INVALID_FILE;

	for (i = 0; i < sizeof(sb->fds) / sizeof(sb->fds[0]); i++) {
		if (&sb->fds[i] == (FT_FILE *) f) {
			sb->fds_mask &= ~(1 << i);
			return WM_SUCCESS;
		}
	}

	return -WM_E_FTFS_INVALID_FILE;
}

file *ft_fopen(struct fs *fs, const char *path, const char *mode)
{
	unsigned int i;
	FT_FILE *f = NULL;
	const char *name;
	struct ftfs_super *sb = (struct ftfs_super *)fs;

	struct ft_entry entry;
	uint32_t addr = sb->active_addr + sizeof(FT_HEADER);

	if (!path)
		return NULL;

	if (path[0] == '/')
		name = &path[1];
	else
		name = &path[0];

	do {
		flash_drv_read(sb->dev, (uint8_t *)&entry,
				sizeof(entry), addr);

		if (entry.name[0] == '\0')	/* reached end of table */
			return NULL;	/* file not found */

		addr += sizeof(entry);
	} while (strncmp(entry.name, name, 24));

	for (i = 0; i < sizeof(sb->fds) / sizeof(sb->fds[0]);
	     i++) {
		if (!(sb->fds_mask & 1 << i)) {
			f = &sb->fds[i];
			f->offset = entry.offset;
			f->length = entry.length;
			f->fp = 0;
			f->sb = sb;
			sb->fds_mask |= 1 << i;
			break;
		}
	}
	if (i == sizeof(sb->fds) / sizeof(sb->fds[0]))
		return NULL;
	return (file *) f;
}

size_t ft_fread(void *ptr, size_t size, size_t nmemb, file *f)
{
	FT_FILE *stream = (FT_FILE *) f;
	int i;
	size_t len = 0;
	uint32_t addr = f_to_ftfs_sb(f)->active_addr + sizeof(FT_HEADER) +
		stream->offset + stream->fp;
	char *b = (char *)ptr;
	mdev_t *fl_dev = f_to_ftfs_sb(f)->dev;

	if (!stream)
		return (size_t) - 1;

	if (stream->fp >= stream->length)
		return 0;

	for (i = 0; i < nmemb; i++) {
		if (stream->fp + size >= stream->length)
			size = stream->length - stream->fp;
		flash_drv_read(fl_dev, (uint8_t *)(b + len), size, addr);
		len += size;
		stream->fp += size;
		addr += size;
		if (stream->fp >= stream->length)
			break;
	};

	return len;
}

size_t ft_fwrite(const void *ptr, size_t size, size_t nmemb, file *f)
{
	return (size_t) - 1;
}

long ft_ftell(file *f)
{
	FT_FILE *stream = (FT_FILE *) f;
	if (!stream)
		return 0;

	return (long)stream->fp;
}

int ft_fseek(file *f, long offset, int whence)
{
	FT_FILE *stream = (FT_FILE *) f;

	if (!stream)
		return -WM_E_FTFS_SEEK_FAIL;

	switch (whence) {
	case SEEK_SET:
		if (offset < 0 || offset >= stream->length)
			return -WM_E_FTFS_SEEK_FAIL;
		stream->fp = offset;
		break;

	case SEEK_CUR:
		if (offset + (long)stream->fp < 0 ||
		    offset + stream->fp >= stream->length)
			return -WM_E_FTFS_SEEK_FAIL;
		stream->fp += offset;
		break;

	case SEEK_END:
		if (offset > 0 || offset + (long)stream->length < 0)
			return -WM_E_FTFS_SEEK_FAIL;
		stream->fp = offset + stream->length;
		break;

	default:
		return -WM_E_FTFS_SEEK_FAIL;
	}

	return WM_SUCCESS;
}

int ft_is_valid_magic(uint8_t *test_magic)
{
	if (memcmp(test_magic, magic, sizeof(magic)) == 0)
		return 1;
	return 0;
}


struct fs *ftfs_init(struct ftfs_super *sb, flash_desc_t *fd)
{
	FT_HEADER sec;
	uint32_t start_addr = fd->fl_start;
	mdev_t *fl_dev = flash_drv_open(fd->fl_dev);

	if (fl_dev == NULL)
		return NULL;

	if (ft_read_header(&sec, start_addr, fl_dev) != WM_SUCCESS)
		return NULL;

	if (!ft_is_valid_magic(sec.magic))
		return NULL;

	memset(sb, 0, sizeof(*sb));
	sb->fs.fopen = ft_fopen;
	sb->fs.fclose = ft_fclose;
	sb->fs.fread = ft_fread;
	sb->fs.fwrite = ft_fwrite;
	sb->fs.ftell = ft_ftell;
	sb->fs.fseek = ft_fseek;


	memset(sb->fds, 0, sizeof(sb->fds));
	sb->fds_mask = 0;

	sb->dev = fl_dev;
	sb->active_addr = start_addr;
	if (sb->active_addr < 0)
		return 0;

	/* Check CRC on each init? */
	sb->fs_crc32 = sec.crc;

	return (struct fs *)sb;
}

bool ft_is_content_valid(struct flash_desc *f1, int be_ver)
{
	FT_HEADER sec1;
	mdev_t *fl_dev;
	bool ret = false;

	flash_drv_init();

	fl_dev = flash_drv_open(f1->fl_dev);
	if (fl_dev == NULL) {
		ftfs_e("Couldn't open flash driver\r\n");
		goto ret;
	}

	if (ft_read_header(&sec1, f1->fl_start, fl_dev) != WM_SUCCESS)
		goto ret;

	ftfs_d("part: be_ver: %d\r\n", sec1.backend_version);

	if (ft_is_valid_magic(sec1.magic) && sec1.backend_version == be_ver) {
		ret = true;
	}

ret:
	flash_drv_close(fl_dev);

	return ret;
}
