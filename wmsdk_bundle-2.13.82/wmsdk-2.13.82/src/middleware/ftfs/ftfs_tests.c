/* 
 *  Copyright 2008-2013, Marvell International Ltd.
 *  All Rights Reserved.
 */

/* FTFS Test Functions for the Marvell Test Framework (MTF) */

#include <ftfs.h>
#include <wmstdio.h>
#include <cli.h>
#include <cli_utils.h>

#define BUF_SIZE	16

static struct fs *fs;
static struct ftfs_super *fsdesc;

static void ftfs_ls(int argc, char **argv)
{
	struct ft_entry entry;
	/* start past the 8-byte magic number */
	uint32_t addr = fsdesc->active_addr + sizeof(FT_HEADER);

	while (1) {
		flash_drv_read(fsdesc->dev, (uint8_t *)&entry,
			       sizeof(entry), addr);

		if (entry.name[0] == '\0')	/* reached end of table */
			return;

		wmprintf("\"%s\" (%d bytes)\r\n", entry.name,
			       entry.length);
		addr += sizeof(entry);
	}
}

#define USAGE_ftfs_cat "[-u] <filename>\r\n -u - for unix-style files\r\n"
static void ftfs_cat(int argc, char **argv)
{
	char buf[BUF_SIZE + 1];
	unsigned int len;
	char *name;
	file *f;
	int c;
	bool unix_le = false;

	if (argc < 2) {
		wmprintf("Usage: %s " USAGE_ftfs_cat, argv[0]);
		wmprintf("Error: invalid number of arguments\r\n");
		return;
	}
	name = argv[1];

	cli_optind = 1;
	while ((c = cli_getopt(argc, argv, "u")) != -1) {
		switch (c) {
		case 'u':
			unix_le = true;
			break;
		default:
			wmprintf("Usage: %s " USAGE_ftfs_cat, argv[0]);
			wmprintf("Error: unexpected option: %c\r\n", c);
			return;
		}
	}

	if (cli_optind >= argc) {
		wmprintf("Usage: %s " USAGE_ftfs_cat, argv[0]);
		wmprintf("Error: No file specified.\r\n");
		return;
	}
	name = argv[cli_optind];

	f = fs->fopen(fs, name, "r");
	if (!f) {
		wmprintf("Error: failed to open \"%s\"\r\n", name);
		return;
	}

	fs->fseek(f, 0, SEEK_END);
	wmprintf("opened \"%s\", length: %d bytes\r\n", name,
		       fs->ftell(f));
	fs->fseek(f, 0, SEEK_SET);

	wmprintf("\"");

	while ((len = fs->fread(buf, BUF_SIZE, 1, f)) > 0) {
		buf[len] = '\0';
		if (unix_le) {
			char *char_ptr = buf;
			// Have to look for \n and insert \r before them.
			while (*char_ptr) {
				if (*char_ptr == '\n')
					wmprintf("\r");
				wmprintf("%c", *char_ptr);
				++char_ptr;
			}
		} else {
			wmprintf("%s", buf);
		}
	}

	wmprintf("\"\r\n");

	fs->fclose(f);
}

static void ftfs_hexdump(int argc, char **argv)
{
	char buf[BUF_SIZE];
	char *name;
	unsigned int len;
	int i, r = 0;

	file *f;

	if (argc != 2) {
		wmprintf("Usage: %s <filename>", argv[0]);
		wmprintf("Error: invalid number of arguments\r\n");
		return;
	}
	name = argv[1];

	f = fs->fopen(fs, name, "r");
	if (!f) {
		wmprintf("Error: failed to open \"%s\"\r\n", name);
		return;
	}

	fs->fseek(f, 0, SEEK_END);
	wmprintf("opened \"%s\", length: %d bytes\r\n", name,
		       fs->ftell(f));
	fs->fseek(f, 0, SEEK_SET);

	while ((len = fs->fread(buf, BUF_SIZE, 1, f)) > 0) {
		wmprintf("\r\n%04X :: ", r * BUF_SIZE);
		for (i = 0; i < len; i++)
			wmprintf("%02X ", buf[i]);
		wmprintf("\r\n");
		r++;
	}
	fs->fclose(f);
}

static struct cli_command tests[] = {
	{"ftfs-ls", NULL, ftfs_ls},
	{"ftfs-cat", "[-u] <filename>", ftfs_cat},
	{"ftfs-hexdump", "<filename>", ftfs_hexdump},
};

int ftfs_cli_init(struct fs *ftfs)
{
	int i;

	if (!ftfs)
		return -WM_E_FTFS_INIT_FAIL;
	fs = ftfs;
	fsdesc = (struct ftfs_super *)fs;

	for (i = 0; i < sizeof(tests) / sizeof(struct cli_command); i++)
		if (cli_register_command(&tests[i]))
			return -WM_E_FTFS_INIT_FAIL;

	return WM_SUCCESS;
}
