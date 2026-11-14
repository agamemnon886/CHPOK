/*
 *                               POK header
 * 
 * The following file is a part of the POK project. Any modification should
 * made according to the POK licence. You CANNOT use this file or a part of
 * this file is this part of a file for your own project
 *
 * For more information on the POK licence, please see our LICENCE FILE
 *
 * Please follow the coding guidelines described in doc/CODING_GUIDELINES
 *
 *                                      Copyright (c) 2007-2009 POK team 
 *
 * Created by julien on Fri Dec 11 16:32:31 2009 
 *
 * Copyright (C) 2026 Kirill Brazhnikov <agamemnon886@gmail.com>
 * Modified by Kirill Brazhnikov, 2026-11-09: Doxygen headers for the file
 * and its functions.
 * Modified by Kirill Brazhnikov, 2026-11-14: The function headers no longer
 * have a step for the declaration of a local variable; the @locals table
 * describes it, and an initialized one is set in a step.
 * The modifications are distributed under the POK licence
 * (BSD-2-Clause, see POK-LICENSE).
 */

/* crypto/bf/bf_enc.c */
/* Copyright (C) 1995-1998 Eric Young (eay@cryptsoft.com)
 * All rights reserved.
 *
 * This package is an SSL implementation written
 * by Eric Young (eay@cryptsoft.com).
 * The implementation was written so as to conform with Netscapes SSL.
 * 
 * This library is free for commercial and non-commercial use as long as
 * the following conditions are aheared to.  The following conditions
 * apply to all code found in this distribution, be it the RC4, RSA,
 * lhash, DES, etc., code; not just the SSL code.  The SSL documentation
 * included with this distribution is covered by the same copyright terms
 * except that the holder is Tim Hudson (tjh@cryptsoft.com).
 * 
 * Copyright remains Eric Young's, and as such any Copyright notices in
 * the code are not to be removed.
 * If this package is used in a product, Eric Young should be given attribution
 * as the author of the parts of the library used.
 * This can be in the form of a textual message at program startup or
 * in documentation (online or textual) provided with the package.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *    "This product includes cryptographic software written by
 *     Eric Young (eay@cryptsoft.com)"
 *    The word 'cryptographic' can be left out if the rouines from the library
 *    being used are not cryptographic related :-).
 * 4. If you include any Windows specific code (or a derivative thereof) from 
 *    the apps directory (application code) you must include an acknowledgement:
 *    "This product includes software written by Tim Hudson (tjh@cryptsoft.com)"
 * 
 * THIS SOFTWARE IS PROVIDED BY ERIC YOUNG ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 * 
 * The licence and distribution terms for any publically available version or
 * derivative of this code cannot be changed.  i.e. this code cannot simply be
 * copied and put under another distribution licence
 * [including the GNU Public Licence.]
 */

/**
 * @file
 * Blowfish encryption of libpok's protocols (from OpenSSL, by Eric
 * Young): one block, CBC and ECB modes.
 */


#include <config.h>

#ifdef POK_NEEDS_PROTOCOLS_BLOWFISH

#include "blowfish.h"
#include "bf_locl.h"

/* Blowfish as implemented from 'Blowfish: Springer-Verlag paper'
 * (From LECTURE NOTES IN COMPUTER SCIENCE 809, FAST SOFTWARE ENCRYPTION,
 * CAMBRIDGE SECURITY WORKSHOP, CAMBRIDGE, U.K., DECEMBER 9-11, 1993)
 */

#if (BF_ROUNDS != 16) && (BF_ROUNDS != 20)
#error If you set BF_ROUNDS to some value other than 16 or 20, you will have \
to modify the code.
#endif

/**
 * @funcname `BF_encrypt`
 *
 * @description
 * Encrypts one 64-bit block (two 32-bit halves) with Blowfish: 16 rounds with
 * the key's P-array and S-boxes.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `data` | `BF_LONG *` | [in,out] The block, encrypted in place. |
 * | `key` | `const BF_KEY *` | [in] The key schedule. |
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `k` | `uint32_t *` | The P-array |
 * | `l` | `uint32_t` | The left half |
 * | `r` | `uint32_t` | The right half |
 * | `t` | `uint32_t` | A temporary of a round |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Set `l` to `data[0]`.
 * 2. Set `r` to `data[1]`.
 * 3. Set `k` to `(BF_LONG*)key`.
 * 4. Toggle the bits `k[0]` in `l`.
 * 5. Use the macro `BF_ENC()` with the arguments `r`, `l`, `k` and `1`.
 * 6. Use the macro `BF_ENC()` with the arguments `l`, `r`, `k` and `2`.
 * 7. Use the macro `BF_ENC()` with the arguments `r`, `l`, `k` and `3`.
 * 8. Use the macro `BF_ENC()` with the arguments `l`, `r`, `k` and `4`.
 * 9. Use the macro `BF_ENC()` with the arguments `r`, `l`, `k` and `5`.
 * 10. Use the macro `BF_ENC()` with the arguments `l`, `r`, `k` and `6`.
 * 11. Use the macro `BF_ENC()` with the arguments `r`, `l`, `k` and `7`.
 * 12. Use the macro `BF_ENC()` with the arguments `l`, `r`, `k` and `8`.
 * 13. Use the macro `BF_ENC()` with the arguments `r`, `l`, `k` and `9`.
 * 14. Use the macro `BF_ENC()` with the arguments `l`, `r`, `k` and `10`.
 * 15. Use the macro `BF_ENC()` with the arguments `r`, `l`, `k` and `11`.
 * 16. Use the macro `BF_ENC()` with the arguments `l`, `r`, `k` and `12`.
 * 17. Use the macro `BF_ENC()` with the arguments `r`, `l`, `k` and `13`.
 * 18. Use the macro `BF_ENC()` with the arguments `l`, `r`, `k` and `14`.
 * 19. Use the macro `BF_ENC()` with the arguments `r`, `l`, `k` and `15`.
 * 20. Use the macro `BF_ENC()` with the arguments `l`, `r`, `k` and `16`.
 * 21. Toggle the bits `k[BF_ROUNDS+1]` in `r`.
 * 22. Set `data[1]` to `l&0xffffffffL`.
 * 23. Set `data[0]` to `r&0xffffffffL`.
 *
 * @calls
 * None.
 */
void BF_encrypt(BF_LONG *data, const BF_KEY *key)
	{
#ifndef BF_PTR2
	register BF_LONG l,r;
	register const BF_LONG *p,*s;

	p=key->P;
	s= &(key->S[0]);
	l=data[0];
	r=data[1];

	l^=p[0];
	BF_ENC(r,l,s,p[ 1]);
	BF_ENC(l,r,s,p[ 2]);
	BF_ENC(r,l,s,p[ 3]);
	BF_ENC(l,r,s,p[ 4]);
	BF_ENC(r,l,s,p[ 5]);
	BF_ENC(l,r,s,p[ 6]);
	BF_ENC(r,l,s,p[ 7]);
	BF_ENC(l,r,s,p[ 8]);
	BF_ENC(r,l,s,p[ 9]);
	BF_ENC(l,r,s,p[10]);
	BF_ENC(r,l,s,p[11]);
	BF_ENC(l,r,s,p[12]);
	BF_ENC(r,l,s,p[13]);
	BF_ENC(l,r,s,p[14]);
	BF_ENC(r,l,s,p[15]);
	BF_ENC(l,r,s,p[16]);
#if BF_ROUNDS == 20
	BF_ENC(r,l,s,p[17]);
	BF_ENC(l,r,s,p[18]);
	BF_ENC(r,l,s,p[19]);
	BF_ENC(l,r,s,p[20]);
#endif
	r^=p[BF_ROUNDS+1];

	data[1]=l&0xffffffffL;
	data[0]=r&0xffffffffL;
#else
	register BF_LONG l,r,t,*k;

	l=data[0];
	r=data[1];
	k=(BF_LONG*)key;

	l^=k[0];
	BF_ENC(r,l,k, 1);
	BF_ENC(l,r,k, 2);
	BF_ENC(r,l,k, 3);
	BF_ENC(l,r,k, 4);
	BF_ENC(r,l,k, 5);
	BF_ENC(l,r,k, 6);
	BF_ENC(r,l,k, 7);
	BF_ENC(l,r,k, 8);
	BF_ENC(r,l,k, 9);
	BF_ENC(l,r,k,10);
	BF_ENC(r,l,k,11);
	BF_ENC(l,r,k,12);
	BF_ENC(r,l,k,13);
	BF_ENC(l,r,k,14);
	BF_ENC(r,l,k,15);
	BF_ENC(l,r,k,16);
#if BF_ROUNDS == 20
	BF_ENC(r,l,k,17);
	BF_ENC(l,r,k,18);
	BF_ENC(r,l,k,19);
	BF_ENC(l,r,k,20);
#endif
	r^=k[BF_ROUNDS+1];

	data[1]=l&0xffffffffL;
	data[0]=r&0xffffffffL;
#endif
	}

#ifndef BF_DEFAULT_OPTIONS

/**
 * @funcname `BF_decrypt`
 *
 * @description
 * Decrypts one 64-bit block with Blowfish (the rounds in reverse order).
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `data` | `BF_LONG *` | [in,out] The block, decrypted in place. |
 * | `key` | `const BF_KEY *` | [in] The key schedule. |
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `k` | `uint32_t *` | The P-array |
 * | `l` | `uint32_t` | The left half |
 * | `r` | `uint32_t` | The right half |
 * | `t` | `uint32_t` | A temporary of a round |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Set `l` to `data[0]`.
 * 2. Set `r` to `data[1]`.
 * 3. Set `k` to `(BF_LONG *)key`.
 * 4. Toggle the bits `k[BF_ROUNDS+1]` in `l`.
 * 5. Use the macro `BF_ENC()` with the arguments `r`, `l`, `k` and `16`.
 * 6. Use the macro `BF_ENC()` with the arguments `l`, `r`, `k` and `15`.
 * 7. Use the macro `BF_ENC()` with the arguments `r`, `l`, `k` and `14`.
 * 8. Use the macro `BF_ENC()` with the arguments `l`, `r`, `k` and `13`.
 * 9. Use the macro `BF_ENC()` with the arguments `r`, `l`, `k` and `12`.
 * 10. Use the macro `BF_ENC()` with the arguments `l`, `r`, `k` and `11`.
 * 11. Use the macro `BF_ENC()` with the arguments `r`, `l`, `k` and `10`.
 * 12. Use the macro `BF_ENC()` with the arguments `l`, `r`, `k` and `9`.
 * 13. Use the macro `BF_ENC()` with the arguments `r`, `l`, `k` and `8`.
 * 14. Use the macro `BF_ENC()` with the arguments `l`, `r`, `k` and `7`.
 * 15. Use the macro `BF_ENC()` with the arguments `r`, `l`, `k` and `6`.
 * 16. Use the macro `BF_ENC()` with the arguments `l`, `r`, `k` and `5`.
 * 17. Use the macro `BF_ENC()` with the arguments `r`, `l`, `k` and `4`.
 * 18. Use the macro `BF_ENC()` with the arguments `l`, `r`, `k` and `3`.
 * 19. Use the macro `BF_ENC()` with the arguments `r`, `l`, `k` and `2`.
 * 20. Use the macro `BF_ENC()` with the arguments `l`, `r`, `k` and `1`.
 * 21. Toggle the bits `k[0]` in `r`.
 * 22. Set `data[1]` to `l&0xffffffffL`.
 * 23. Set `data[0]` to `r&0xffffffffL`.
 *
 * @calls
 * None.
 */
void BF_decrypt(BF_LONG *data, const BF_KEY *key)
	{
#ifndef BF_PTR2
	register BF_LONG l,r;
	register const BF_LONG *p,*s;

	p=key->P;
	s= &(key->S[0]);
	l=data[0];
	r=data[1];

	l^=p[BF_ROUNDS+1];
#if BF_ROUNDS == 20
	BF_ENC(r,l,s,p[20]);
	BF_ENC(l,r,s,p[19]);
	BF_ENC(r,l,s,p[18]);
	BF_ENC(l,r,s,p[17]);
#endif
	BF_ENC(r,l,s,p[16]);
	BF_ENC(l,r,s,p[15]);
	BF_ENC(r,l,s,p[14]);
	BF_ENC(l,r,s,p[13]);
	BF_ENC(r,l,s,p[12]);
	BF_ENC(l,r,s,p[11]);
	BF_ENC(r,l,s,p[10]);
	BF_ENC(l,r,s,p[ 9]);
	BF_ENC(r,l,s,p[ 8]);
	BF_ENC(l,r,s,p[ 7]);
	BF_ENC(r,l,s,p[ 6]);
	BF_ENC(l,r,s,p[ 5]);
	BF_ENC(r,l,s,p[ 4]);
	BF_ENC(l,r,s,p[ 3]);
	BF_ENC(r,l,s,p[ 2]);
	BF_ENC(l,r,s,p[ 1]);
	r^=p[0];

	data[1]=l&0xffffffffL;
	data[0]=r&0xffffffffL;
#else
	register BF_LONG l,r,t,*k;

	l=data[0];
	r=data[1];
	k=(BF_LONG *)key;

	l^=k[BF_ROUNDS+1];
#if BF_ROUNDS == 20
	BF_ENC(r,l,k,20);
	BF_ENC(l,r,k,19);
	BF_ENC(r,l,k,18);
	BF_ENC(l,r,k,17);
#endif
	BF_ENC(r,l,k,16);
	BF_ENC(l,r,k,15);
	BF_ENC(r,l,k,14);
	BF_ENC(l,r,k,13);
	BF_ENC(r,l,k,12);
	BF_ENC(l,r,k,11);
	BF_ENC(r,l,k,10);
	BF_ENC(l,r,k, 9);
	BF_ENC(r,l,k, 8);
	BF_ENC(l,r,k, 7);
	BF_ENC(r,l,k, 6);
	BF_ENC(l,r,k, 5);
	BF_ENC(r,l,k, 4);
	BF_ENC(l,r,k, 3);
	BF_ENC(r,l,k, 2);
	BF_ENC(l,r,k, 1);
	r^=k[0];

	data[1]=l&0xffffffffL;
	data[0]=r&0xffffffffL;
#endif
	}

/**
 * @funcname `BF_cbc_encrypt`
 *
 * @description
 * Encrypts or decrypts data with Blowfish in CBC mode; a last partial block is
 * padded.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `in` | `const unsigned char *` | [in] The input. |
 * | `out` | `unsigned char *` | [out] The output. |
 * | `length` | `long` | [in] The length of the input. |
 * | `schedule` | `const BF_KEY *` | [in] The key schedule. |
 * | `ivec` | `unsigned char *` | [in] The initialization vector (updated). |
 * | `encrypt` | `int` | [in] BF_ENCRYPT or BF_DECRYPT. |
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `l` | `long` | The length left |
 * | `tin` | `uint32_t[2]` | The block |
 * | `tin0` | `uint32_t` | The first half of the input block |
 * | `tin1` | `uint32_t` | Its second half |
 * | `tout0` | `uint32_t` | The first half of the output block |
 * | `tout1` | `uint32_t` | Its second half |
 * | `xor0` | `uint32_t` | The first half of the vector |
 * | `xor1` | `uint32_t` | Its second half |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Set `l` to `length`.
 * 2. If `encrypt` is true:
 *    1. Use the macro `n2l()` with the arguments `ivec` and `tout0`.
 *    2. Use the macro `n2l()` with the arguments `ivec` and `tout1`.
 *    3. Subtract `8` from `ivec`.
 *    4. Subtract `8` from `l`; then while `l` is greater than or equal to `0`,
 *       repeat (after each pass: subtract `8` from `l`):
 *       1. Use the macro `n2l()` with the arguments `in` and `tin0`.
 *       2. Use the macro `n2l()` with the arguments `in` and `tin1`.
 *       3. Toggle the bits `tout0` in `tin0`.
 *       4. Toggle the bits `tout1` in `tin1`.
 *       5. Set `tin[0]` to `tin0`.
 *       6. Set `tin[1]` to `tin1`.
 *       7. Call `BF_encrypt()` with the arguments `tin` and `schedule`.
 *       8. Set `tout0` to `tin[0]`.
 *       9. Set `tout1` to `tin[1]`.
 *       10. Use the macro `l2n()` with the arguments `tout0` and `out`.
 *       11. Use the macro `l2n()` with the arguments `tout1` and `out`.
 *    5. If `l` is not equal to `-8`:
 *       1. Use the macro `n2ln()` with the arguments `in`, `tin0`, `tin1` and
 *          `l+8`.
 *       2. Use the macro `n2ln()` with the arguments `in`, `tin0`, `tin1` and
 *          `l+8`.
 *       3. Use the macro `n2ln()` with the arguments `in`, `tin0`, `tin1` and
 *          `l+8`.
 *       4. Do nothing.
 *       5. Toggle the bits `tout0` in `tin0`.
 *       6. Toggle the bits `tout1` in `tin1`.
 *       7. Set `tin[0]` to `tin0`.
 *       8. Set `tin[1]` to `tin1`.
 *       9. Call `BF_encrypt()` with the arguments `tin` and `schedule`.
 *       10. Set `tout0` to `tin[0]`.
 *       11. Set `tout1` to `tin[1]`.
 *       12. Use the macro `l2n()` with the arguments `tout0` and `out`.
 *       13. Use the macro `l2n()` with the arguments `tout1` and `out`.
 *    6. Use the macro `l2n()` with the arguments `tout0` and `ivec`.
 *    7. Use the macro `l2n()` with the arguments `tout1` and `ivec`.
 * 3. Otherwise:
 *    1. Use the macro `n2l()` with the arguments `ivec` and `xor0`.
 *    2. Use the macro `n2l()` with the arguments `ivec` and `xor1`.
 *    3. Subtract `8` from `ivec`.
 *    4. Subtract `8` from `l`; then while `l` is greater than or equal to `0`,
 *       repeat (after each pass: subtract `8` from `l`):
 *       1. Use the macro `n2l()` with the arguments `in` and `tin0`.
 *       2. Use the macro `n2l()` with the arguments `in` and `tin1`.
 *       3. Set `tin[0]` to `tin0`.
 *       4. Set `tin[1]` to `tin1`.
 *       5. Call `BF_decrypt()` with the arguments `tin` and `schedule`.
 *       6. Set `tout0` to `tin[0]^xor0`.
 *       7. Set `tout1` to `tin[1]^xor1`.
 *       8. Use the macro `l2n()` with the arguments `tout0` and `out`.
 *       9. Use the macro `l2n()` with the arguments `tout1` and `out`.
 *       10. Set `xor0` to `tin0`.
 *       11. Set `xor1` to `tin1`.
 *    5. If `l` is not equal to `-8`:
 *       1. Use the macro `n2l()` with the arguments `in` and `tin0`.
 *       2. Use the macro `n2l()` with the arguments `in` and `tin1`.
 *       3. Set `tin[0]` to `tin0`.
 *       4. Set `tin[1]` to `tin1`.
 *       5. Call `BF_decrypt()` with the arguments `tin` and `schedule`.
 *       6. Set `tout0` to `tin[0]^xor0`.
 *       7. Set `tout1` to `tin[1]^xor1`.
 *       8. Use the macro `l2nn()` with the arguments `tout0`, `tout1`, `out`
 *          and `l+8`.
 *       9. Use the macro `l2nn()` with the arguments `tout0`, `tout1`, `out`
 *          and `l+8`.
 *       10. Do nothing.
 *       11. Set `xor0` to `tin0`.
 *       12. Set `xor1` to `tin1`.
 *    6. Use the macro `l2n()` with the arguments `xor0` and `ivec`.
 *    7. Use the macro `l2n()` with the arguments `xor1` and `ivec`.
 * 4. Set `tin0` to `tin1=tout0=tout1=xor0=xor1=0`.
 * 5. Set `tin[0]` to `tin[1]=0`.
 *
 * @calls
 * - `BF_encrypt()`
 * - `BF_decrypt()`
 */
void BF_cbc_encrypt(const unsigned char *in, unsigned char *out, long length,
	     const BF_KEY *schedule, unsigned char *ivec, int encrypt)
	{
	register BF_LONG tin0,tin1;
	register BF_LONG tout0,tout1,xor0,xor1;
	register long l=length;
	BF_LONG tin[2];

	if (encrypt)
		{
		n2l(ivec,tout0);
		n2l(ivec,tout1);
		ivec-=8;
		for (l-=8; l>=0; l-=8)
			{
			n2l(in,tin0);
			n2l(in,tin1);
			tin0^=tout0;
			tin1^=tout1;
			tin[0]=tin0;
			tin[1]=tin1;
			BF_encrypt(tin,schedule);
			tout0=tin[0];
			tout1=tin[1];
			l2n(tout0,out);
			l2n(tout1,out);
			}
		if (l != -8)
			{
			n2ln(in,tin0,tin1,l+8);
			tin0^=tout0;
			tin1^=tout1;
			tin[0]=tin0;
			tin[1]=tin1;
			BF_encrypt(tin,schedule);
			tout0=tin[0];
			tout1=tin[1];
			l2n(tout0,out);
			l2n(tout1,out);
			}
		l2n(tout0,ivec);
		l2n(tout1,ivec);
		}
	else
		{
		n2l(ivec,xor0);
		n2l(ivec,xor1);
		ivec-=8;
		for (l-=8; l>=0; l-=8)
			{
			n2l(in,tin0);
			n2l(in,tin1);
			tin[0]=tin0;
			tin[1]=tin1;
			BF_decrypt(tin,schedule);
			tout0=tin[0]^xor0;
			tout1=tin[1]^xor1;
			l2n(tout0,out);
			l2n(tout1,out);
			xor0=tin0;
			xor1=tin1;
			}
		if (l != -8)
			{
			n2l(in,tin0);
			n2l(in,tin1);
			tin[0]=tin0;
			tin[1]=tin1;
			BF_decrypt(tin,schedule);
			tout0=tin[0]^xor0;
			tout1=tin[1]^xor1;
			l2nn(tout0,tout1,out,l+8);
			xor0=tin0;
			xor1=tin1;
			}
		l2n(xor0,ivec);
		l2n(xor1,ivec);
		}
	tin0=tin1=tout0=tout1=xor0=xor1=0;
	tin[0]=tin[1]=0;
	}


/**
 * @funcname `BF_ecb_encrypt`
 *
 * @description
 * Encrypts or decrypts one 8-byte block with Blowfish in ECB mode.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `in` | `const unsigned char *` | [in] The input block. |
 * | `out` | `unsigned char *` | [out] The output block. |
 * | `key` | `const BF_KEY *` | [in] The key schedule. |
 * | `encrypt` | `int` | [in] BF_ENCRYPT or BF_DECRYPT. |
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `d` | `uint32_t[2]` | The block |
 * | `l` | `uint32_t` | A half of the block |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Use the macro `n2l()` with the arguments `in` and `l`.
 * 2. Set `d[0]` to `l`.
 * 3. Use the macro `n2l()` with the arguments `in` and `l`.
 * 4. Set `d[1]` to `l`.
 * 5. If `encrypt` is true:
 *    1. Call `BF_encrypt()` with the arguments `d` and `key`.
 * 6. Otherwise:
 *    1. Call `BF_decrypt()` with the arguments `d` and `key`.
 * 7. Set `l` to `d[0]`.
 * 8. Use the macro `l2n()` with the arguments `l` and `out`.
 * 9. Set `l` to `d[1]`.
 * 10. Use the macro `l2n()` with the arguments `l` and `out`.
 * 11. Set `l` to `d[0]=d[1]=0`.
 *
 * @calls
 * - `BF_encrypt()`
 * - `BF_decrypt()`
 */
void BF_ecb_encrypt(const unsigned char *in, unsigned char *out,
      const BF_KEY *key, int encrypt)
{
   BF_LONG l,d[2];

   n2l(in,l); d[0]=l;
   n2l(in,l); d[1]=l;
   if (encrypt)
      BF_encrypt(d,key);
   else
      BF_decrypt(d,key);
   l=d[0]; l2n(l,out);
   l=d[1]; l2n(l,out);
   l=d[0]=d[1]=0;
}


#endif

#endif /* POK_NEEDS_PROTOCOLS */
