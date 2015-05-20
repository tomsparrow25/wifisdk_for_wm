/*
 * Copyright 2008-2013, Marvell International Ltd.
 * All Rights Reserved.
 */

/* Test Program for block_alloc */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <cli.h>
#include <cli_utils.h>
#include <wm_os.h>


void ba_test(int argc, char **argv)
{
	int ret;
	char *name = NULL, str[100], str1[100];
	char buf1[75], buf2[20], buf3[25];
	os_block_pool_t ptr1, ptr2;
	void *alloc1, *alloc2, *alloc3, *alloc4;

	/* Try to create invalid pool */
	wmprintf("\nTry to create invalid pool:\r\n");
	if (!(os_block_pool_create(&ptr2, name, 5, buf2, sizeof(buf2), 0))) {
		wmprintf("\nInitialized the blocks of requested size %d from address %p for ptr2.\r\n", ptr2->block_size, ptr2->head);
		goto ERROR;
	}

	/* Create pool with mutex */
	wmprintf("\nA pool with mutex:\r\n");
	if (!(os_block_pool_create(&ptr1, name, 25, buf1, sizeof(buf1), 1)))
		wmprintf("\nInitialized the blocks of requested size %d from address %p for ptr1.\r\n", ptr1->block_size, ptr1->head);
	else {
		wmprintf("\nCannot initialize the pool");
		goto ERROR;
	}

	ret = os_block_pool_info_get_str(&ptr1, str, sizeof(str));
	if (ret)
		goto ERROR;
	else
		wmprintf("\nPool info is:%s", str);

	ret = os_block_alloc(&ptr1, &alloc1, 0);
	if (ret)
		goto ERROR;

	ret = os_block_free(&ptr1, alloc1);
	if (ret)
		goto ERROR;

	ret = os_block_alloc(&ptr1, &alloc1, 0);
	if (ret)
		goto ERROR;

	ret = os_block_alloc(&ptr1, &alloc2, 0);
	if (ret)
		goto ERROR;

	/* Allcoation should fail as max limit exceeded */
	ret = os_block_alloc(&ptr1, &alloc3, 0);
	if (ret != -WM_FAIL)
		/* As this is a negative test case */
		goto ERROR;

	ret = os_block_pool_info_get_str(&ptr1, str, sizeof(str));
	if (ret)
		goto ERROR;
	else
		wmprintf("\nPool info is:%s", str);

	ret = os_block_free(&ptr1, alloc1);
	if (ret)
		goto ERROR;

	ret = os_block_free(&ptr1, alloc2);
	if (ret)
		goto ERROR;

	ret = os_block_pool_delete(&ptr1);
	if (ret)
		goto ERROR;

	/* Create pool without mutex */
	wmprintf("\nA pool without mutex:\r\n");
	if (!(os_block_pool_create(&ptr2, name, 5, buf3, sizeof(buf3), 0)))
		wmprintf("\nInitialized the blocks of requested size %d from address %p for ptr2\r\n", ptr2->block_size, ptr2->head);
	else {
		wmprintf("\nCannot initialize the pool");
		goto ERROR;
	}

	ret = os_block_pool_info_get_str(&ptr2, str1, sizeof(str1));
	if  (ret)
		goto ERROR;
	else
		wmprintf("\nPool info is:%s", str1);

	ret = os_block_alloc(&ptr2, &alloc4, 0);
	if (ret)
		goto ERROR;

	ret = os_block_pool_delete(&ptr2);
	if (ret != -WM_FAIL)
		/* As this is a negative test case */
		goto ERROR;

	ret = os_block_free(&ptr2, alloc4);
	if (ret)
		goto ERROR;

	ret = os_block_pool_delete(&ptr2);
	if (ret)
		goto ERROR;

	goto SUCCESS;
ERROR:
	wmprintf("Error");
	return;
SUCCESS:
	wmprintf("Success");

}


struct cli_command ba_command = {
	"slab-alloc-test", "Runs Block Alloc Tests", ba_test
};

int ba_cli_init(void)
{
	if (cli_register_command(&ba_command))
		return 1;
	return 0;
}

