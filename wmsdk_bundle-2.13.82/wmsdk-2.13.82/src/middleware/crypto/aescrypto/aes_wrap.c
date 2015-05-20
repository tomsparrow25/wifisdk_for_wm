/*
 * AES-based functions
 *
 * - AES Key Wrap Algorithm (128-bit KEK) (RFC3394)
 * - One-Key CBC MAC (OMAC1) hash with AES-128
 * - AES-128 CTR mode encryption
 * - AES-128 EAX mode encryption/decryption
 * - AES-128 CBC
 *
 * Copyright (C) 2003-2005, Jouni Malinen <jkmaline@cc.hut.fi>
 *
 * Copyright (C) 2006-2007, Marvell International Ltd. and its affiliates
 * All rights reserved.
 *
 * 1. Redistributions in binary form must reproduce the above copyright 
 * notice, this list of conditions and the following disclaimer in the 
 * documentation and/or other materials provided with the distribution.
 * 2. Neither the name of Jouni Malinen, Marvell nor the names of its 
 * contributors may be used to endorse or promote products derived from 
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS 
 * ''AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT 
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR 
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT 
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, 
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT 
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, 
 * DATA, OR PROFITS; OR BUSINESS  INTERRUPTION) HOWEVER CAUSED AND ON ANY 
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT 
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE 
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

/*
 * Change log:
 *   Marvell  09/28/07: add Marvell file header
 */

#include "includes.h"
#include "common.h"
#include "aes_wrap.h"
#include "crypto.h"

#ifdef INTERNAL_AES
#include "aes.c"
#endif /* INTERNAL_AES */

/**
 * aes_wrap - Wrap keys with AES Key Wrap Algorithm (128-bit KEK) (RFC3394)
 * @kek: Key encryption key (KEK)
 * @n: Length of the wrapped key in 64-bit units; e.g., 2 = 128-bit = 16 bytes
 * @plain: Plaintext key to be wrapped, n * 64 bit
 * @cipher: Wrapped key, (n + 1) * 64 bit
 * Returns: 0 on success, -1 on failure
 */
	int
aes_wrap(const u8 *kek, int n, const u8 *plain, u8 *cipher)
{
	u8 *a, *r, b[16];
	int i, j;
	void *ctx;

	a = cipher;
	r = cipher + 8;

	/* 1) Initialize variables. */
	memset(a, 0xa6, 8);
	memcpy(r, plain, 8 * n);

	ctx = aes_encrypt_init(kek, 16);
	if (ctx == NULL)
		return -1;

	/* 2) Calculate intermediate values. For j = 0 to 5 For i=1 to n
	   B = AES(K,
	   A | R[i]) A = MSB(64, B) ^ t where t = (n*j)+i R[i] = LSB(64, B) */
	for (j = 0; j <= 5; j++) {
		r = cipher + 8;
		for (i = 1; i <= n; i++) {
			memcpy(b, a, 8);
			memcpy(b + 8, r, 8);
			aes_encrypt(ctx, b, b);
			memcpy(a, b, 8);
			a[7] ^= n * j + i;
			memcpy(r, b + 8, 8);
			r += 8;
		}
	}
	aes_encrypt_deinit(ctx);

	/* 3) Output the results. These are already in @cipher
	   due to the location
	   of temporary variables. */

	return 0;
}

/**
 * aes_unwrap - Unwrap key with AES Key Wrap Algorithm (128-bit KEK) (RFC3394)
 * @kek: Key encryption key (KEK)
 * @n: Length of the wrapped key in 64-bit units; e.g., 2 = 128-bit = 16 bytes
 * @cipher: Wrapped key to be unwrapped, (n + 1) * 64 bit
 * @plain: Plaintext key, n * 64 bit
 * Returns: 0 on success, -1 on failure (e.g., integrity verification failed)
 */
int
aes_unwrap(const u8 * kek, int n, const u8 * cipher, u8 * plain)
{
    u8 a[8], *r, b[16];
    int i, j;
    void *ctx;

    /* 1) Initialize variables. */
    memcpy(a, cipher, 8);
    r = plain;
    memcpy(r, cipher + 8, 8 * n);

    ctx = aes_decrypt_init(kek, 16);
    if (ctx == NULL)
        return -1;

    /* 2) Compute intermediate values. For j = 5 to 0 For i = n to 1 B =
       AES-1(K, (A ^ t) | R[i]) where t = n*j+i A = MSB(64, B) R[i] = LSB(64,
       B) */
    for (j = 5; j >= 0; j--) {
        r = plain + (n - 1) * 8;
        for (i = n; i >= 1; i--) {
            memcpy(b, a, 8);
            b[7] ^= n * j + i;

            memcpy(b + 8, r, 8);
            aes_decrypt(ctx, b, b);
            memcpy(a, b, 8);
            memcpy(r, b + 8, 8);
            r -= 8;
        }
    }
    aes_decrypt_deinit(ctx);

    /* 3) Output results. These are already in @plain due to the location of
       temporary variables. Just verify that the IV matches with the expected
       value. */
    for (i = 0; i < 8; i++) {
        if (a[i] != 0xa6)
            return -1;
    }

    return 0;
}
