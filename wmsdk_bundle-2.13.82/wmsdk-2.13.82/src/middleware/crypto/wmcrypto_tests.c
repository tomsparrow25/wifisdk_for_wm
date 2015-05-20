/*
 * Copyright 2008-2013, Marvell International Ltd.
 * All Rights Reserved.
 */

#include <cli.h>
#include <cli_utils.h>
#include <stdlib.h>
#include <wm_os.h>
#include <wmstdio.h>
#include <cyassl/ctaocrypt/aes.h>




#define CRYPTD(...)				\
	do {					\
		wmprintf("CRYPT-TEST: ");	\
		wmprintf(__VA_ARGS__);	\
		wmprintf("\n\r");		\
	} while (0)

#define CRYPTD_ERROR(...)			\
	do {					\
		wmprintf("CRYPT-TEST: Error: ");	\
		wmprintf(__VA_ARGS__);		\
		wmprintf("\n\r");			\
	} while (0);

#define CRYPTD_WARN(...)			\
	do {						\
		wmprintf("CRYPT-TEST: Warn:  ");	\
		wmprintf(__VA_ARGS__);		\
		wmprintf("\n\r");			\
	} while (0);

int crypto_test_cli_init(void);

unsigned char const key[] = {
	0xc2, 0x86, 0x69, 0x6d,
	0x88, 0x7c, 0x9a, 0xa0,
	0x61, 0x1b, 0xbb, 0x3e,
	0x20, 0x25, 0xa4, 0x5a
};

unsigned char const initial_vector[] = {
	0x56, 0x2e, 0x17, 0x99,
	0x6d, 0x09, 0x3d, 0x28,
	0xdd, 0xb3, 0xba, 0x69,
	0x5a, 0x2e, 0x6f, 0x58
};

#define AES_BENCHMARK_BLOCK_SIZE 1024
#define AES_BENCHMARK_ITERATION 300

static void aes_benchmark()
{
	unsigned char *plain_text, *cipher_text;
	int i;

	plain_text = os_mem_alloc(AES_BENCHMARK_BLOCK_SIZE);
	if (!plain_text) {
		CRYPTD_ERROR("Memory allocation failed");
		return;
	}

	cipher_text = os_mem_alloc(AES_BENCHMARK_BLOCK_SIZE);
	if (!cipher_text) {
		CRYPTD_ERROR("Memory allocation failed");
		return;
	}

	unsigned start_us, end_us;
	Aes enc, dec;

	start_us = os_get_timestamp();
	AesSetKey(&enc, key, 16, initial_vector,
		  AES_ENCRYPTION);

	for (i = 0; i < AES_BENCHMARK_ITERATION; i++)
		AesCbcEncrypt(&enc, cipher_text,
			      plain_text, AES_BENCHMARK_BLOCK_SIZE);

	end_us = os_get_timestamp();
	CRYPTD("AES encryption %d Kbytes took %d microseconds",
	       (AES_BENCHMARK_BLOCK_SIZE * AES_BENCHMARK_ITERATION/1024),
	       end_us - start_us);

	start_us = os_get_timestamp();
	AesSetKey(&dec, key, 16, initial_vector,
		  AES_DECRYPTION);

	for (i = 0; i < AES_BENCHMARK_ITERATION; i++)
		AesCbcDecrypt(&dec, plain_text,
			      cipher_text, AES_BENCHMARK_BLOCK_SIZE);

	end_us = os_get_timestamp();
	CRYPTD("AES decryption %d Kbytes took %d microseconds",
	       (AES_BENCHMARK_BLOCK_SIZE * AES_BENCHMARK_ITERATION/1024),
	       end_us - start_us);

	os_mem_free(plain_text);
	os_mem_free(cipher_text);
}


static void test_aes()
{
	CRYPTD("AES benchmark:");
	aes_benchmark();
}

static void crypto_test(int argc, char **argv)
{
	test_aes();
}

static struct cli_command crypto_test_commands[] = {
	{"crypto-test", NULL, crypto_test},
};

int crypto_test_cli_init()
{
	int i;

	for (i = 0; i < sizeof(crypto_test_commands) /
		     sizeof(struct cli_command);
	     i++)
		if (cli_register_command(&crypto_test_commands[i]))
			return 1;
	return 0;
}
