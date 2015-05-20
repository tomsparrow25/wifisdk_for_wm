/*
 * Big number math
 *
 * Copyright (c) 2006, Jouni Malinen <j@w1.fi>
 *
 * Copyright 2008-2013, Marvell International Ltd. and its affiliates
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
 *   Marvell  05/21/09: add Marvell file header
 */

#ifndef BIGNUM_H
#define BIGNUM_H

struct bignum;

struct bignum *bignum_init(void);
void bignum_deinit(struct bignum *n);
size_t bignum_get_unsigned_bin_len(struct bignum *n);
int bignum_get_unsigned_bin(const struct bignum *n, u8 * buf, size_t * len);
int bignum_set_unsigned_bin(struct bignum *n, const u8 * buf, size_t len);
int bignum_cmp(const struct bignum *a, const struct bignum *b);
int bignum_cmp_d(const struct bignum *a, unsigned long b);
int bignum_add(const struct bignum *a, const struct bignum *b,
               struct bignum *c);
int bignum_sub(const struct bignum *a, const struct bignum *b,
               struct bignum *c);
int bignum_mul(const struct bignum *a, const struct bignum *b,
               struct bignum *c);
int bignum_mulmod(const struct bignum *a, const struct bignum *b,
                  const struct bignum *c, struct bignum *d);
int bignum_exptmod(const struct bignum *a, const struct bignum *b,
                   const struct bignum *c, struct bignum *d);

#endif /* BIGNUM_H */
