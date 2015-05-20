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

#ifndef AES_WRAP_H
#define AES_WRAP_H

int aes_wrap(const u8 * kek, int n, const u8 * plain, u8 * cipher);
int aes_unwrap(const u8 * kek, int n, const u8 * cipher, u8 * plain);
int omac1_aes_128(const u8 * key, const u8 * data, size_t data_len, u8 * mac);
int aes_128_encrypt_block(const u8 * key, const u8 * in, u8 * out);
int aes_128_ctr_encrypt(const u8 * key, const u8 * nonce,
                        u8 * data, size_t data_len);
int aes_128_eax_encrypt(const u8 * key, const u8 * nonce, size_t nonce_len,
                        const u8 * hdr, size_t hdr_len,
                        u8 * data, size_t data_len, u8 * tag);
int aes_128_eax_decrypt(const u8 * key, const u8 * nonce, size_t nonce_len,
                        const u8 * hdr, size_t hdr_len,
                        u8 * data, size_t data_len, const u8 * tag);
int aes_128_cbc_encrypt(const u8 * key, const u8 * iv, u8 * data,
                        size_t data_len);
int aes_128_cbc_decrypt(const u8 * key, const u8 * iv, u8 * data,
                        size_t data_len);

#endif /* AES_WRAP_H */
