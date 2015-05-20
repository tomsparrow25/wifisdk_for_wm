/*
 * SHA256 hash implementation and interface functions
 *
 * Copyright (C) 2003-2006, Jouni Malinen <jkmaline@cc.hut.fi>
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

#ifndef SHA256_H
#define SHA256_H

#define SHA256_MAC_LEN 32

void hmac_sha256_vector(const u8 * key, size_t key_len, size_t num_elem,
                        const u8 * addr[], const size_t * len, u8 * mac);
void hmac_sha256(const u8 * key, size_t key_len, const u8 * data,
                 size_t data_len, u8 * mac);
void sha256_prf(const u8 * key, size_t key_len, const char *label,
                const u8 * data, size_t data_len, u8 * buf, size_t buf_len);

#endif /* SHA256_H */
