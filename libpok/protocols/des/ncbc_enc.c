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
 * Created by julien on Tue Dec  8 15:53:28 2009 
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

/* crypto/des/ncbc_enc.c */
/*
 * #included by:
 *    cbc_enc.c  (DES_cbc_encrypt)
 *    des_enc.c  (DES_ncbc_encrypt)
 */
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
 * DES in CBC mode of libpok's protocols (from OpenSSL, by Eric Young).
 */


#include <config.h>

#ifdef POK_NEEDS_PROTOCOLS_DES

#include "des_locl.h"

#ifdef CBC_ENC_C__DONT_UPDATE_IV
void DES_cbc_encrypt(const unsigned char *in, unsigned char *out, long length,
		     DES_key_schedule *_schedule, DES_cblock *ivec, int enc)
#else
/**
 * @funcname `DES_ncbc_encrypt`
 *
 * @description
 * Encrypts or decrypts data with DES in CBC mode; a last partial block is
 * padded, and the vector is updated.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `in` | `const unsigned char *` | [in] The input. |
 * | `out` | `unsigned char *` | [out] The output. |
 * | `length` | `long` | [in] The length of the input. |
 * | `_schedule` | `DES_key_schedule *` | [in] The key schedule. |
 * | `ivec` | `DES_cblock *` | [in,out] The initialization vector (updated). |
 * | `enc` | `int` | [in] Non-zero to encrypt, 0 to decrypt. |
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `iv` | `unsigned char *` | The vector |
 * | `l` | `long` | The length left |
 * | `tin` | `unsigned long[2]` | The block |
 * | `tin0` | `unsigned long` | The first half of the input block |
 * | `tin1` | `unsigned long` | Its second half |
 * | `tout0` | `unsigned long` | The first half of the output block |
 * | `tout1` | `unsigned long` | Its second half |
 * | `xor0` | `unsigned long` | The first half of the vector |
 * | `xor1` | `unsigned long` | Its second half |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Set `l` to `length`.
 * 2. Set `iv` to the address of `(*ivec)[0]`.
 * 3. If `enc` is true:
 *    1. Use the macro `c2l()` with the arguments `iv` and `tout0`.
 *    2. Use the macro `c2l()` with the arguments `iv` and `tout1`.
 *    3. Subtract `8` from `l`; then while `l` is greater than or equal to `0`,
 *       repeat (after each pass: subtract `8` from `l`):
 *       1. Use the macro `c2l()` with the arguments `in` and `tin0`.
 *       2. Use the macro `c2l()` with the arguments `in` and `tin1`.
 *       3. Toggle the bits `tout0` in `tin0`.
 *       4. Set `tin[0]` to `tin0`.
 *       5. Toggle the bits `tout1` in `tin1`.
 *       6. Set `tin[1]` to `tin1`.
 *       7. Call `DES_encrypt1()` with the arguments `(DES_LONG *)tin`,
 *          `_schedule` and `DES_ENCRYPT`.
 *       8. Set `tout0` to `tin[0]`.
 *       9. Use the macro `l2c()` with the arguments `tout0` and `out`.
 *       10. Set `tout1` to `tin[1]`.
 *       11. Use the macro `l2c()` with the arguments `tout1` and `out`.
 *    4. If `l` is not equal to `-8`:
 *       1. Use the macro `c2ln()` with the arguments `in`, `tin0`, `tin1` and
 *          `l+8`.
 *       2. Use the macro `c2ln()` with the arguments `in`, `tin0`, `tin1` and
 *          `l+8`.
 *       3. Use the macro `c2ln()` with the arguments `in`, `tin0`, `tin1` and
 *          `l+8`.
 *       4. Do nothing.
 *       5. Toggle the bits `tout0` in `tin0`.
 *       6. Set `tin[0]` to `tin0`.
 *       7. Toggle the bits `tout1` in `tin1`.
 *       8. Set `tin[1]` to `tin1`.
 *       9. Call `DES_encrypt1()` with the arguments `(DES_LONG *)tin`,
 *          `_schedule` and `DES_ENCRYPT`.
 *       10. Set `tout0` to `tin[0]`.
 *       11. Use the macro `l2c()` with the arguments `tout0` and `out`.
 *       12. Set `tout1` to `tin[1]`.
 *       13. Use the macro `l2c()` with the arguments `tout1` and `out`.
 *    5. Set `iv` to the address of `(*ivec)[0]`.
 *    6. Use the macro `l2c()` with the arguments `tout0` and `iv`.
 *    7. Use the macro `l2c()` with the arguments `tout1` and `iv`.
 * 4. Otherwise:
 *    1. Use the macro `c2l()` with the arguments `iv` and `xor0`.
 *    2. Use the macro `c2l()` with the arguments `iv` and `xor1`.
 *    3. Subtract `8` from `l`; then while `l` is greater than or equal to `0`,
 *       repeat (after each pass: subtract `8` from `l`):
 *       1. Use the macro `c2l()` with the arguments `in` and `tin0`.
 *       2. Set `tin[0]` to `tin0`.
 *       3. Use the macro `c2l()` with the arguments `in` and `tin1`.
 *       4. Set `tin[1]` to `tin1`.
 *       5. Call `DES_encrypt1()` with the arguments `(DES_LONG *)tin`,
 *          `_schedule` and `DES_DECRYPT`.
 *       6. Set `tout0` to `tin[0]^xor0`.
 *       7. Set `tout1` to `tin[1]^xor1`.
 *       8. Use the macro `l2c()` with the arguments `tout0` and `out`.
 *       9. Use the macro `l2c()` with the arguments `tout1` and `out`.
 *       10. Set `xor0` to `tin0`.
 *       11. Set `xor1` to `tin1`.
 *    4. If `l` is not equal to `-8`:
 *       1. Use the macro `c2l()` with the arguments `in` and `tin0`.
 *       2. Set `tin[0]` to `tin0`.
 *       3. Use the macro `c2l()` with the arguments `in` and `tin1`.
 *       4. Set `tin[1]` to `tin1`.
 *       5. Call `DES_encrypt1()` with the arguments `(DES_LONG *)tin`,
 *          `_schedule` and `DES_DECRYPT`.
 *       6. Set `tout0` to `tin[0]^xor0`.
 *       7. Set `tout1` to `tin[1]^xor1`.
 *       8. Use the macro `l2cn()` with the arguments `tout0`, `tout1`, `out`
 *          and `l+8`.
 *       9. Use the macro `l2cn()` with the arguments `tout0`, `tout1`, `out`
 *          and `l+8`.
 *       10. Do nothing.
 *       11. Set `xor0` to `tin0`.
 *       12. Set `xor1` to `tin1`.
 *    5. Set `iv` to the address of `(*ivec)[0]`.
 *    6. Use the macro `l2c()` with the arguments `xor0` and `iv`.
 *    7. Use the macro `l2c()` with the arguments `xor1` and `iv`.
 * 5. Set `tin0` to `tin1=tout0=tout1=xor0=xor1=0`.
 * 6. Set `tin[0]` to `tin[1]=0`.
 *
 * @calls
 * - `DES_encrypt1()`
 */
void DES_ncbc_encrypt(const unsigned char *in, unsigned char *out, long length,
		     DES_key_schedule *_schedule, DES_cblock *ivec, int enc)
#endif
	{
	register DES_LONG tin0,tin1;
	register DES_LONG tout0,tout1,xor0,xor1;
	register long l=length;
	DES_LONG tin[2];
	unsigned char *iv;

	iv = &(*ivec)[0];

	if (enc)
		{
		c2l(iv,tout0);
		c2l(iv,tout1);
		for (l-=8; l>=0; l-=8)
			{
			c2l(in,tin0);
			c2l(in,tin1);
			tin0^=tout0; tin[0]=tin0;
			tin1^=tout1; tin[1]=tin1;
			DES_encrypt1((DES_LONG *)tin,_schedule,DES_ENCRYPT);
			tout0=tin[0]; l2c(tout0,out);
			tout1=tin[1]; l2c(tout1,out);
			}
		if (l != -8)
			{
			c2ln(in,tin0,tin1,l+8);
			tin0^=tout0; tin[0]=tin0;
			tin1^=tout1; tin[1]=tin1;
			DES_encrypt1((DES_LONG *)tin,_schedule,DES_ENCRYPT);
			tout0=tin[0]; l2c(tout0,out);
			tout1=tin[1]; l2c(tout1,out);
			}
#ifndef CBC_ENC_C__DONT_UPDATE_IV
		iv = &(*ivec)[0];
		l2c(tout0,iv);
		l2c(tout1,iv);
#endif
		}
	else
		{
		c2l(iv,xor0);
		c2l(iv,xor1);
		for (l-=8; l>=0; l-=8)
			{
			c2l(in,tin0); tin[0]=tin0;
			c2l(in,tin1); tin[1]=tin1;
			DES_encrypt1((DES_LONG *)tin,_schedule,DES_DECRYPT);
			tout0=tin[0]^xor0;
			tout1=tin[1]^xor1;
			l2c(tout0,out);
			l2c(tout1,out);
			xor0=tin0;
			xor1=tin1;
			}
		if (l != -8)
			{
			c2l(in,tin0); tin[0]=tin0;
			c2l(in,tin1); tin[1]=tin1;
			DES_encrypt1((DES_LONG *)tin,_schedule,DES_DECRYPT);
			tout0=tin[0]^xor0;
			tout1=tin[1]^xor1;
			l2cn(tout0,tout1,out,l+8);
#ifndef CBC_ENC_C__DONT_UPDATE_IV
			xor0=tin0;
			xor1=tin1;
#endif
			}
#ifndef CBC_ENC_C__DONT_UPDATE_IV 
		iv = &(*ivec)[0];
		l2c(xor0,iv);
		l2c(xor1,iv);
#endif
		}
	tin0=tin1=tout0=tout1=xor0=xor1=0;
	tin[0]=tin[1]=0;
	}


#endif /* POK_NEEDS_ ...*/
