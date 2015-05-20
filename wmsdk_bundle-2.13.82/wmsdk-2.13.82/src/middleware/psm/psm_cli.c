/*
 *  Copyright 2008-2013, Marvell International Ltd.
 *  All Rights Reserved.
 */

/** psm_cli.c: The psm CLI
 */
#include <string.h>
#include <stdlib.h>

#include <cli.h>
#include <cli_utils.h>
#include <wmstdio.h>

#include <psm.h>

void psm_cli_register_module(int argc, char **argv)
{
	int ret;

	if (argc != 3) {
		wmprintf("[psm] Usage: %s <module> <partition-key>\r\n",
			       argv[0]);
		wmprintf("[psm] Error: invalid number of arguments\r\n");
		return;
	}
	ret = psm_register_module(argv[1], argv[2], PSM_CREAT);
	if (ret != 0)
		psm_e("Registering module failed: %d", ret);
	return;
}

void psm_cli_get(int argc, char **argv)
{
	psm_handle_t handle;
	int ret;
	char value[32];

	if (argc != 3) {
		wmprintf("[psm] Usage: %s <module> <variable>\r\n", argv[0]);
		wmprintf("[psm] Error: invalid number of arguments\r\n");
		return;
	}
	ret = psm_open(&handle, argv[1]);
	if (ret != 0) {
		psm_e("psm_open failed with: %d "
			"(Is the module name registered?)", ret);
		return;
	}
	psm_get(&handle, argv[2], value, sizeof(value));
	wmprintf("[psm] Value: %s\r\n", value);
	psm_close(&handle);

}

void psm_cli_set(int argc, char **argv)
{
	psm_handle_t handle;
	int ret;

	if (argc != 4) {
		wmprintf("[psm] Usage: %s <module> <variable> <value>\r\n",
			       argv[0]);
		wmprintf("[psm] Error: invalid number of arguments\r\n");
		return;
	}
	if ((ret = psm_open(&handle, argv[1])) != 0) {
		psm_e("psm_open failed with: %d "
			"(Is the module name registered?)", ret);
		return;
	}
	psm_set(&handle, argv[2], argv[3]);
	psm_close(&handle);
}

void psm_erase(int argc, char **argv)
{
	int ret;

	if (argc == 2) {
		short part = atoi(argv[1]);
		psm_erase_partition(part);
		return;
	}

	/* psm_erase_and_init is not protected by mutices. So, do that ourselves */
	ret = psm_get_mutex();
	if (ret != 0) {
		psm_e("psm_get_mutex failed with : %d", ret);
		return;
	}

	ret = psm_erase_and_init();
	if (ret != 0)
		psm_e("psm_erase_and_init failed with : %d", ret);
	psm_put_mutex();
}

/* Ideally this should get the partition key: CLI only */
void psm_dump(int argc, char **argv)
{
	int part, ret;
	psm_metadata_t *h0;
	psm_var_block_t *hn;
	char *env_start, *name;

	if (argc != 2) {
		wmprintf("[psm] Usage: %s <partition_no>\r\n", argv[0]);
		wmprintf("[psm] Error: invalid number of arguments\r\n");
		return;
	}

	part = atoi(argv[1]);
	if (part >= psm_fl.fl_size / PARTITION_SIZE || part < 0) {
		wmprintf("[psm] Usage: %s <partition_no>\r\n", argv[0]);
		wmprintf("[psm] Error: the partition no. is invalid\n");
		return;
	}

	ret = psm_get_mutex();
	if (ret != 0) {
		psm_e("lock on the "
			"global psm buffer failed: %d", ret);
		return;
	}

	if (psm_read_partition(part, &gpsm_partition_cache)) {
		/* CRC error. Error message is printed by the library itself,
		 * so no handling here */
		psm_put_mutex();
		return;
	}
	if (part == 0) {
		h0 = (psm_metadata_t *) gpsm_partition_cache.buffer;
		env_start = (char *)h0->vars;
	} else {
		hn = (psm_var_block_t *) gpsm_partition_cache.buffer;
		env_start = (char *)hn->vars;
	}

	for (name = env_start; *name; name += strlen(name) + 1)
		wmprintf("[psm] %s\r\n", name);

	psm_put_mutex();
}

void psm_cli_get_free_space(int argc, char **argv)
{
	int ret;
	psm_handle_t handle;

	if (argc != 2) {
		wmprintf("[psm] Usage: %s <module_name>\r\n", argv[0]);
		wmprintf("[psm] Error: invalid number of arguments\r\n");
		return;
	}

	if ((ret = psm_open(&handle, argv[1])) != 0) {
		psm_e("psm_open failed with: %d "
			"(Is the module name registered?)", ret);
		return;
	}
	ret = psm_get_free_space(&handle);
	if (ret >= 0)
		wmprintf("[psm] Free space available: %d bytes", ret);
	else
		wmprintf("[psm] Error: psm_get_free_space failed with: %d\r\n",
			       ret);
	psm_close(&handle);
}

struct cli_command psm_commands[] = {
	{"psm-register", "<module> <partition-key>", psm_cli_register_module},
	{"psm-get", "<module> <variable>", psm_cli_get},
	{"psm-set", "<module> <variable> <value>", psm_cli_set},
	{"psm-erase", NULL, psm_erase},
	{"psm-dump", "<partition_no>", psm_dump},
	{"psm-get-free-space", "<module>", psm_cli_get_free_space},
};

int psm_cli_init(void)
{
	int i;

	for (i = 0; i < sizeof(psm_commands) / sizeof(struct cli_command); i++)
		if (cli_register_command(&psm_commands[i]))
			return 1;
	return 0;
}
