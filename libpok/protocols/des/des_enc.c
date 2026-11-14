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

/* crypto/des/des_enc.c */
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
 * DES encryption of libpok's protocols (from OpenSSL, by Eric Young): one
 * block, triple DES and triple DES in CBC mode.
 */


#include <config.h>

#ifdef POK_NEEDS_PROTOCOLS_DES

#include "des_locl.h"
#include "spr.h"

/**
 * @funcname `DES_encrypt1`
 *
 * @description
 * Encrypts or decrypts one 64-bit block with DES: the initial permutation, 16
 * rounds and the final permutation.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `data` | `DES_LONG *` | [in,out] The block (two halves), changed in place. |
 * | `ks` | `DES_key_schedule *` | [in] The key schedule. |
 * | `enc` | `int` | [in] Non-zero to encrypt, 0 to decrypt. |
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `DES_SPtrans` | `const unsigned long[8][64]` | The combined S-box and P-permutation tables |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `i` | `int` | The round |
 * | `l` | `unsigned long` | The left half |
 * | `r` | `unsigned long` | The right half |
 * | `s` | `unsigned long *` | The key schedule, as words |
 * | `t` | `unsigned long` | A temporary of a round |
 * | `u` | `unsigned long` | A temporary of a round |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Set `r` to `data[0]`.
 * 2. Set `l` to `data[1]`.
 * 3. Use the macro `IP()` with the arguments `r` and `l`.
 * 4. Use the macro `IP()` with the arguments `r` and `l`.
 * 5. Use the macro `IP()` with the arguments `r` and `l`.
 * 6. Use the macro `IP()` with the arguments `r` and `l`.
 * 7. Use the macro `IP()` with the arguments `r` and `l`.
 * 8. Do nothing.
 * 9. Set `r` to `ROTATE(r,29)&0xffffffffL`.
 * 10. Set `l` to `ROTATE(l,29)&0xffffffffL`.
 * 11. Set `s` to `ks->ks->deslong`.
 * 12. If `enc` is true:
 *     1. Set `i` to `0`; then while `i` is less than `32`, repeat (after each
 *        pass: add `8` to `i`):
 *        1. Use the macro `D_ENCRYPT()` with the arguments `l`, `r` and `i+0`.
 *        2. Use the macro `D_ENCRYPT()` with the arguments `l`, `r` and `i+0`.
 *        3. Use the macro `D_ENCRYPT()` with the arguments `l`, `r` and `i+0`.
 *        4. Use the macro `D_ENCRYPT()` with the arguments `l`, `r` and `i+0`.
 *        5. Do nothing.
 *        6. Use the macro `D_ENCRYPT()` with the arguments `r`, `l` and `i+2`.
 *        7. Use the macro `D_ENCRYPT()` with the arguments `r`, `l` and `i+2`.
 *        8. Use the macro `D_ENCRYPT()` with the arguments `r`, `l` and `i+2`.
 *        9. Use the macro `D_ENCRYPT()` with the arguments `r`, `l` and `i+2`.
 *        10. Do nothing.
 *        11. Use the macro `D_ENCRYPT()` with the arguments `l`, `r` and `i+4`.
 *        12. Use the macro `D_ENCRYPT()` with the arguments `l`, `r` and `i+4`.
 *        13. Use the macro `D_ENCRYPT()` with the arguments `l`, `r` and `i+4`.
 *        14. Use the macro `D_ENCRYPT()` with the arguments `l`, `r` and `i+4`.
 *        15. Do nothing.
 *        16. Use the macro `D_ENCRYPT()` with the arguments `r`, `l` and `i+6`.
 *        17. Use the macro `D_ENCRYPT()` with the arguments `r`, `l` and `i+6`.
 *        18. Use the macro `D_ENCRYPT()` with the arguments `r`, `l` and `i+6`.
 *        19. Use the macro `D_ENCRYPT()` with the arguments `r`, `l` and `i+6`.
 *        20. Do nothing.
 * 13. Otherwise:
 *     1. Set `i` to `30`; then while `i` is greater than `0`, repeat (after
 *        each pass: subtract `8` from `i`):
 *        1. Use the macro `D_ENCRYPT()` with the arguments `l`, `r` and `i-0`.
 *        2. Use the macro `D_ENCRYPT()` with the arguments `l`, `r` and `i-0`.
 *        3. Use the macro `D_ENCRYPT()` with the arguments `l`, `r` and `i-0`.
 *        4. Use the macro `D_ENCRYPT()` with the arguments `l`, `r` and `i-0`.
 *        5. Do nothing.
 *        6. Use the macro `D_ENCRYPT()` with the arguments `r`, `l` and `i-2`.
 *        7. Use the macro `D_ENCRYPT()` with the arguments `r`, `l` and `i-2`.
 *        8. Use the macro `D_ENCRYPT()` with the arguments `r`, `l` and `i-2`.
 *        9. Use the macro `D_ENCRYPT()` with the arguments `r`, `l` and `i-2`.
 *        10. Do nothing.
 *        11. Use the macro `D_ENCRYPT()` with the arguments `l`, `r` and `i-4`.
 *        12. Use the macro `D_ENCRYPT()` with the arguments `l`, `r` and `i-4`.
 *        13. Use the macro `D_ENCRYPT()` with the arguments `l`, `r` and `i-4`.
 *        14. Use the macro `D_ENCRYPT()` with the arguments `l`, `r` and `i-4`.
 *        15. Do nothing.
 *        16. Use the macro `D_ENCRYPT()` with the arguments `r`, `l` and `i-6`.
 *        17. Use the macro `D_ENCRYPT()` with the arguments `r`, `l` and `i-6`.
 *        18. Use the macro `D_ENCRYPT()` with the arguments `r`, `l` and `i-6`.
 *        19. Use the macro `D_ENCRYPT()` with the arguments `r`, `l` and `i-6`.
 *        20. Do nothing.
 * 14. Set `l` to `ROTATE(l,3)&0xffffffffL`.
 * 15. Set `r` to `ROTATE(r,3)&0xffffffffL`.
 * 16. Use the macro `FP()` with the arguments `r` and `l`.
 * 17. Use the macro `FP()` with the arguments `r` and `l`.
 * 18. Use the macro `FP()` with the arguments `r` and `l`.
 * 19. Use the macro `FP()` with the arguments `r` and `l`.
 * 20. Use the macro `FP()` with the arguments `r` and `l`.
 * 21. Do nothing.
 * 22. Set `data[0]` to `l`.
 * 23. Set `data[1]` to `r`.
 * 24. Set `l` to `r=t=u=0`.
 *
 * @calls
 * None.
 */
void DES_encrypt1(DES_LONG *data, DES_key_schedule *ks, int enc)
	{
	register DES_LONG l,r,t,u;
#ifdef DES_PTR
	register const unsigned char *des_SP=(const unsigned char *)DES_SPtrans;
#endif
#ifndef DES_UNROLL
	register int i;
#endif
	register DES_LONG *s;

	r=data[0];
	l=data[1];

	IP(r,l);
	/* Things have been modified so that the initial rotate is
	 * done outside the loop.  This required the
	 * DES_SPtrans values in sp.h to be rotated 1 bit to the right.
	 * One perl script later and things have a 5% speed up on a sparc2.
	 * Thanks to Richard Outerbridge <71755.204@CompuServe.COM>
	 * for pointing this out. */
	/* clear the top bits on machines with 8byte longs */
	/* shift left by 2 */
	r=ROTATE(r,29)&0xffffffffL;
	l=ROTATE(l,29)&0xffffffffL;

	s=ks->ks->deslong;
	/* I don't know if it is worth the effort of loop unrolling the
	 * inner loop */
	if (enc)
		{
#ifdef DES_UNROLL
		D_ENCRYPT(l,r, 0); /*  1 */
		D_ENCRYPT(r,l, 2); /*  2 */
		D_ENCRYPT(l,r, 4); /*  3 */
		D_ENCRYPT(r,l, 6); /*  4 */
		D_ENCRYPT(l,r, 8); /*  5 */
		D_ENCRYPT(r,l,10); /*  6 */
		D_ENCRYPT(l,r,12); /*  7 */
		D_ENCRYPT(r,l,14); /*  8 */
		D_ENCRYPT(l,r,16); /*  9 */
		D_ENCRYPT(r,l,18); /*  10 */
		D_ENCRYPT(l,r,20); /*  11 */
		D_ENCRYPT(r,l,22); /*  12 */
		D_ENCRYPT(l,r,24); /*  13 */
		D_ENCRYPT(r,l,26); /*  14 */
		D_ENCRYPT(l,r,28); /*  15 */
		D_ENCRYPT(r,l,30); /*  16 */
#else
		for (i=0; i<32; i+=8)
			{
			D_ENCRYPT(l,r,i+0); /*  1 */
			D_ENCRYPT(r,l,i+2); /*  2 */
			D_ENCRYPT(l,r,i+4); /*  3 */
			D_ENCRYPT(r,l,i+6); /*  4 */
			}
#endif
		}
	else
		{
#ifdef DES_UNROLL
		D_ENCRYPT(l,r,30); /* 16 */
		D_ENCRYPT(r,l,28); /* 15 */
		D_ENCRYPT(l,r,26); /* 14 */
		D_ENCRYPT(r,l,24); /* 13 */
		D_ENCRYPT(l,r,22); /* 12 */
		D_ENCRYPT(r,l,20); /* 11 */
		D_ENCRYPT(l,r,18); /* 10 */
		D_ENCRYPT(r,l,16); /*  9 */
		D_ENCRYPT(l,r,14); /*  8 */
		D_ENCRYPT(r,l,12); /*  7 */
		D_ENCRYPT(l,r,10); /*  6 */
		D_ENCRYPT(r,l, 8); /*  5 */
		D_ENCRYPT(l,r, 6); /*  4 */
		D_ENCRYPT(r,l, 4); /*  3 */
		D_ENCRYPT(l,r, 2); /*  2 */
		D_ENCRYPT(r,l, 0); /*  1 */
#else
		for (i=30; i>0; i-=8)
			{
			D_ENCRYPT(l,r,i-0); /* 16 */
			D_ENCRYPT(r,l,i-2); /* 15 */
			D_ENCRYPT(l,r,i-4); /* 14 */
			D_ENCRYPT(r,l,i-6); /* 13 */
			}
#endif
		}

	/* rotate and clear the top bits on machines with 8byte longs */
	l=ROTATE(l,3)&0xffffffffL;
	r=ROTATE(r,3)&0xffffffffL;

	FP(r,l);
	data[0]=l;
	data[1]=r;
	l=r=t=u=0;
	}

/**
 * @funcname `DES_encrypt2`
 *
 * @description
 * Encrypts or decrypts one 64-bit block with DES: the 16 rounds only, without
 * the initial and final permutations (for triple DES).
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `data` | `DES_LONG *` | [in,out] The block (two halves), changed in place. |
 * | `ks` | `DES_key_schedule *` | [in] The key schedule. |
 * | `enc` | `int` | [in] Non-zero to encrypt, 0 to decrypt. |
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `DES_SPtrans` | `const unsigned long[8][64]` | The combined S-box and P-permutation tables |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `i` | `int` | The round |
 * | `l` | `unsigned long` | The left half |
 * | `r` | `unsigned long` | The right half |
 * | `s` | `unsigned long *` | The key schedule, as words |
 * | `t` | `unsigned long` | A temporary of a round |
 * | `u` | `unsigned long` | A temporary of a round |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Set `r` to `data[0]`.
 * 2. Set `l` to `data[1]`.
 * 3. Set `r` to `ROTATE(r,29)&0xffffffffL`.
 * 4. Set `l` to `ROTATE(l,29)&0xffffffffL`.
 * 5. Set `s` to `ks->ks->deslong`.
 * 6. If `enc` is true:
 *    1. Set `i` to `0`; then while `i` is less than `32`, repeat (after each
 *       pass: add `8` to `i`):
 *       1. Use the macro `D_ENCRYPT()` with the arguments `l`, `r` and `i+0`.
 *       2. Use the macro `D_ENCRYPT()` with the arguments `l`, `r` and `i+0`.
 *       3. Use the macro `D_ENCRYPT()` with the arguments `l`, `r` and `i+0`.
 *       4. Use the macro `D_ENCRYPT()` with the arguments `l`, `r` and `i+0`.
 *       5. Do nothing.
 *       6. Use the macro `D_ENCRYPT()` with the arguments `r`, `l` and `i+2`.
 *       7. Use the macro `D_ENCRYPT()` with the arguments `r`, `l` and `i+2`.
 *       8. Use the macro `D_ENCRYPT()` with the arguments `r`, `l` and `i+2`.
 *       9. Use the macro `D_ENCRYPT()` with the arguments `r`, `l` and `i+2`.
 *       10. Do nothing.
 *       11. Use the macro `D_ENCRYPT()` with the arguments `l`, `r` and `i+4`.
 *       12. Use the macro `D_ENCRYPT()` with the arguments `l`, `r` and `i+4`.
 *       13. Use the macro `D_ENCRYPT()` with the arguments `l`, `r` and `i+4`.
 *       14. Use the macro `D_ENCRYPT()` with the arguments `l`, `r` and `i+4`.
 *       15. Do nothing.
 *       16. Use the macro `D_ENCRYPT()` with the arguments `r`, `l` and `i+6`.
 *       17. Use the macro `D_ENCRYPT()` with the arguments `r`, `l` and `i+6`.
 *       18. Use the macro `D_ENCRYPT()` with the arguments `r`, `l` and `i+6`.
 *       19. Use the macro `D_ENCRYPT()` with the arguments `r`, `l` and `i+6`.
 *       20. Do nothing.
 * 7. Otherwise:
 *    1. Set `i` to `30`; then while `i` is greater than `0`, repeat (after
 *       each pass: subtract `8` from `i`):
 *       1. Use the macro `D_ENCRYPT()` with the arguments `l`, `r` and `i-0`.
 *       2. Use the macro `D_ENCRYPT()` with the arguments `l`, `r` and `i-0`.
 *       3. Use the macro `D_ENCRYPT()` with the arguments `l`, `r` and `i-0`.
 *       4. Use the macro `D_ENCRYPT()` with the arguments `l`, `r` and `i-0`.
 *       5. Do nothing.
 *       6. Use the macro `D_ENCRYPT()` with the arguments `r`, `l` and `i-2`.
 *       7. Use the macro `D_ENCRYPT()` with the arguments `r`, `l` and `i-2`.
 *       8. Use the macro `D_ENCRYPT()` with the arguments `r`, `l` and `i-2`.
 *       9. Use the macro `D_ENCRYPT()` with the arguments `r`, `l` and `i-2`.
 *       10. Do nothing.
 *       11. Use the macro `D_ENCRYPT()` with the arguments `l`, `r` and `i-4`.
 *       12. Use the macro `D_ENCRYPT()` with the arguments `l`, `r` and `i-4`.
 *       13. Use the macro `D_ENCRYPT()` with the arguments `l`, `r` and `i-4`.
 *       14. Use the macro `D_ENCRYPT()` with the arguments `l`, `r` and `i-4`.
 *       15. Do nothing.
 *       16. Use the macro `D_ENCRYPT()` with the arguments `r`, `l` and `i-6`.
 *       17. Use the macro `D_ENCRYPT()` with the arguments `r`, `l` and `i-6`.
 *       18. Use the macro `D_ENCRYPT()` with the arguments `r`, `l` and `i-6`.
 *       19. Use the macro `D_ENCRYPT()` with the arguments `r`, `l` and `i-6`.
 *       20. Do nothing.
 * 8. Set `data[0]` to `ROTATE(l,3)&0xffffffffL`.
 * 9. Set `data[1]` to `ROTATE(r,3)&0xffffffffL`.
 * 10. Set `l` to `r=t=u=0`.
 *
 * @calls
 * None.
 */
void DES_encrypt2(DES_LONG *data, DES_key_schedule *ks, int enc)
	{
	register DES_LONG l,r,t,u;
#ifdef DES_PTR
	register const unsigned char *des_SP=(const unsigned char *)DES_SPtrans;
#endif
#ifndef DES_UNROLL
	register int i;
#endif
	register DES_LONG *s;

	r=data[0];
	l=data[1];

	/* Things have been modified so that the initial rotate is
	 * done outside the loop.  This required the
	 * DES_SPtrans values in sp.h to be rotated 1 bit to the right.
	 * One perl script later and things have a 5% speed up on a sparc2.
	 * Thanks to Richard Outerbridge <71755.204@CompuServe.COM>
	 * for pointing this out. */
	/* clear the top bits on machines with 8byte longs */
	r=ROTATE(r,29)&0xffffffffL;
	l=ROTATE(l,29)&0xffffffffL;

	s=ks->ks->deslong;
	/* I don't know if it is worth the effort of loop unrolling the
	 * inner loop */
	if (enc)
		{
#ifdef DES_UNROLL
		D_ENCRYPT(l,r, 0); /*  1 */
		D_ENCRYPT(r,l, 2); /*  2 */
		D_ENCRYPT(l,r, 4); /*  3 */
		D_ENCRYPT(r,l, 6); /*  4 */
		D_ENCRYPT(l,r, 8); /*  5 */
		D_ENCRYPT(r,l,10); /*  6 */
		D_ENCRYPT(l,r,12); /*  7 */
		D_ENCRYPT(r,l,14); /*  8 */
		D_ENCRYPT(l,r,16); /*  9 */
		D_ENCRYPT(r,l,18); /*  10 */
		D_ENCRYPT(l,r,20); /*  11 */
		D_ENCRYPT(r,l,22); /*  12 */
		D_ENCRYPT(l,r,24); /*  13 */
		D_ENCRYPT(r,l,26); /*  14 */
		D_ENCRYPT(l,r,28); /*  15 */
		D_ENCRYPT(r,l,30); /*  16 */
#else
		for (i=0; i<32; i+=8)
			{
			D_ENCRYPT(l,r,i+0); /*  1 */
			D_ENCRYPT(r,l,i+2); /*  2 */
			D_ENCRYPT(l,r,i+4); /*  3 */
			D_ENCRYPT(r,l,i+6); /*  4 */
			}
#endif
		}
	else
		{
#ifdef DES_UNROLL
		D_ENCRYPT(l,r,30); /* 16 */
		D_ENCRYPT(r,l,28); /* 15 */
		D_ENCRYPT(l,r,26); /* 14 */
		D_ENCRYPT(r,l,24); /* 13 */
		D_ENCRYPT(l,r,22); /* 12 */
		D_ENCRYPT(r,l,20); /* 11 */
		D_ENCRYPT(l,r,18); /* 10 */
		D_ENCRYPT(r,l,16); /*  9 */
		D_ENCRYPT(l,r,14); /*  8 */
		D_ENCRYPT(r,l,12); /*  7 */
		D_ENCRYPT(l,r,10); /*  6 */
		D_ENCRYPT(r,l, 8); /*  5 */
		D_ENCRYPT(l,r, 6); /*  4 */
		D_ENCRYPT(r,l, 4); /*  3 */
		D_ENCRYPT(l,r, 2); /*  2 */
		D_ENCRYPT(r,l, 0); /*  1 */
#else
		for (i=30; i>0; i-=8)
			{
			D_ENCRYPT(l,r,i-0); /* 16 */
			D_ENCRYPT(r,l,i-2); /* 15 */
			D_ENCRYPT(l,r,i-4); /* 14 */
			D_ENCRYPT(r,l,i-6); /* 13 */
			}
#endif
		}
	/* rotate and clear the top bits on machines with 8byte longs */
	data[0]=ROTATE(l,3)&0xffffffffL;
	data[1]=ROTATE(r,3)&0xffffffffL;
	l=r=t=u=0;
	}

/**
 * @funcname `DES_encrypt3`
 *
 * @description
 * Encrypts one 64-bit block with triple DES (encrypt, decrypt, encrypt with
 * three keys).
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `data` | `DES_LONG *` | [in,out] The block, changed in place. |
 * | `ks1` | `DES_key_schedule *` | [in] The first key schedule. |
 * | `ks2` | `DES_key_schedule *` | [in] The second key schedule. |
 * | `ks3` | `DES_key_schedule *` | [in] The third key schedule. |
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `l` | `unsigned long` | The left half |
 * | `r` | `unsigned long` | The right half |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Set `l` to `data[0]`.
 * 2. Set `r` to `data[1]`.
 * 3. Use the macro `IP()` with the arguments `l` and `r`.
 * 4. Use the macro `IP()` with the arguments `l` and `r`.
 * 5. Use the macro `IP()` with the arguments `l` and `r`.
 * 6. Use the macro `IP()` with the arguments `l` and `r`.
 * 7. Use the macro `IP()` with the arguments `l` and `r`.
 * 8. Do nothing.
 * 9. Set `data[0]` to `l`.
 * 10. Set `data[1]` to `r`.
 * 11. Call `DES_encrypt2()` with the arguments `(DES_LONG *)data`, `ks1` and
 *     `DES_ENCRYPT`.
 * 12. Call `DES_encrypt2()` with the arguments `(DES_LONG *)data`, `ks2` and
 *     `DES_DECRYPT`.
 * 13. Call `DES_encrypt2()` with the arguments `(DES_LONG *)data`, `ks3` and
 *     `DES_ENCRYPT`.
 * 14. Set `l` to `data[0]`.
 * 15. Set `r` to `data[1]`.
 * 16. Use the macro `FP()` with the arguments `r` and `l`.
 * 17. Use the macro `FP()` with the arguments `r` and `l`.
 * 18. Use the macro `FP()` with the arguments `r` and `l`.
 * 19. Use the macro `FP()` with the arguments `r` and `l`.
 * 20. Use the macro `FP()` with the arguments `r` and `l`.
 * 21. Do nothing.
 * 22. Set `data[0]` to `l`.
 * 23. Set `data[1]` to `r`.
 *
 * @calls
 * - `DES_encrypt2()`
 */
void DES_encrypt3(DES_LONG *data, DES_key_schedule *ks1,
		  DES_key_schedule *ks2, DES_key_schedule *ks3)
	{
	register DES_LONG l,r;

	l=data[0];
	r=data[1];
	IP(l,r);
	data[0]=l;
	data[1]=r;
	DES_encrypt2((DES_LONG *)data,ks1,DES_ENCRYPT);
	DES_encrypt2((DES_LONG *)data,ks2,DES_DECRYPT);
	DES_encrypt2((DES_LONG *)data,ks3,DES_ENCRYPT);
	l=data[0];
	r=data[1];
	FP(r,l);
	data[0]=l;
	data[1]=r;
	}

/**
 * @funcname `DES_decrypt3`
 *
 * @description
 * Decrypts one 64-bit block with triple DES (decrypt, encrypt, decrypt with
 * three keys).
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `data` | `DES_LONG *` | [in,out] The block, changed in place. |
 * | `ks1` | `DES_key_schedule *` | [in] The first key schedule. |
 * | `ks2` | `DES_key_schedule *` | [in] The second key schedule. |
 * | `ks3` | `DES_key_schedule *` | [in] The third key schedule. |
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `l` | `unsigned long` | The left half |
 * | `r` | `unsigned long` | The right half |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Set `l` to `data[0]`.
 * 2. Set `r` to `data[1]`.
 * 3. Use the macro `IP()` with the arguments `l` and `r`.
 * 4. Use the macro `IP()` with the arguments `l` and `r`.
 * 5. Use the macro `IP()` with the arguments `l` and `r`.
 * 6. Use the macro `IP()` with the arguments `l` and `r`.
 * 7. Use the macro `IP()` with the arguments `l` and `r`.
 * 8. Do nothing.
 * 9. Set `data[0]` to `l`.
 * 10. Set `data[1]` to `r`.
 * 11. Call `DES_encrypt2()` with the arguments `(DES_LONG *)data`, `ks3` and
 *     `DES_DECRYPT`.
 * 12. Call `DES_encrypt2()` with the arguments `(DES_LONG *)data`, `ks2` and
 *     `DES_ENCRYPT`.
 * 13. Call `DES_encrypt2()` with the arguments `(DES_LONG *)data`, `ks1` and
 *     `DES_DECRYPT`.
 * 14. Set `l` to `data[0]`.
 * 15. Set `r` to `data[1]`.
 * 16. Use the macro `FP()` with the arguments `r` and `l`.
 * 17. Use the macro `FP()` with the arguments `r` and `l`.
 * 18. Use the macro `FP()` with the arguments `r` and `l`.
 * 19. Use the macro `FP()` with the arguments `r` and `l`.
 * 20. Use the macro `FP()` with the arguments `r` and `l`.
 * 21. Do nothing.
 * 22. Set `data[0]` to `l`.
 * 23. Set `data[1]` to `r`.
 *
 * @calls
 * - `DES_encrypt2()`
 */
void DES_decrypt3(DES_LONG *data, DES_key_schedule *ks1,
		  DES_key_schedule *ks2, DES_key_schedule *ks3)
	{
	register DES_LONG l,r;

	l=data[0];
	r=data[1];
	IP(l,r);
	data[0]=l;
	data[1]=r;
	DES_encrypt2((DES_LONG *)data,ks3,DES_DECRYPT);
	DES_encrypt2((DES_LONG *)data,ks2,DES_ENCRYPT);
	DES_encrypt2((DES_LONG *)data,ks1,DES_DECRYPT);
	l=data[0];
	r=data[1];
	FP(r,l);
	data[0]=l;
	data[1]=r;
	}

#ifndef DES_DEFAULT_OPTIONS

#if !defined(OPENSSL_FIPS_DES_ASM)

#undef CBC_ENC_C__DONT_UPDATE_IV
#include "ncbc_enc.c" /* DES_ncbc_encrypt */

/**
 * @funcname `DES_ede3_cbc_encrypt`
 *
 * @description
 * Encrypts or decrypts data with triple DES in CBC mode; a last partial block
 * is padded.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `input` | `const unsigned char *` | [in] The input. |
 * | `output` | `unsigned char *` | [out] The output. |
 * | `length` | `long` | [in] The length of the input. |
 * | `ks1` | `DES_key_schedule *` | [in] The first key schedule. |
 * | `ks2` | `DES_key_schedule *` | [in] The second key schedule. |
 * | `ks3` | `DES_key_schedule *` | [in] The third key schedule. |
 * | `ivec` | `DES_cblock *` | [in,out] The initialization vector (updated). |
 * | `enc` | `int` | [in] Non-zero to encrypt, 0 to decrypt. |
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `in` | `const unsigned char *` | The input |
 * | `iv` | `unsigned char *` | The vector |
 * | `l` | `long` | The length left |
 * | `out` | `unsigned char *` | The output |
 * | `t0` | `unsigned long` | The first half of the next vector |
 * | `t1` | `unsigned long` | Its second half |
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
 * 2. Set `in` to `input`.
 * 3. Set `out` to `output`.
 * 4. Set `iv` to the address of `(*ivec)[0]`.
 * 5. If `enc` is true:
 *    1. Use the macro `c2l()` with the arguments `iv` and `tout0`.
 *    2. Use the macro `c2l()` with the arguments `iv` and `tout1`.
 *    3. Subtract `8` from `l`; then while `l` is greater than or equal to `0`,
 *       repeat (after each pass: subtract `8` from `l`):
 *       1. Use the macro `c2l()` with the arguments `in` and `tin0`.
 *       2. Use the macro `c2l()` with the arguments `in` and `tin1`.
 *       3. Toggle the bits `tout0` in `tin0`.
 *       4. Toggle the bits `tout1` in `tin1`.
 *       5. Set `tin[0]` to `tin0`.
 *       6. Set `tin[1]` to `tin1`.
 *       7. Call `DES_encrypt3()` with the arguments `(DES_LONG *)tin`, `ks1`,
 *          `ks2` and `ks3`.
 *       8. Set `tout0` to `tin[0]`.
 *       9. Set `tout1` to `tin[1]`.
 *       10. Use the macro `l2c()` with the arguments `tout0` and `out`.
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
 *       6. Toggle the bits `tout1` in `tin1`.
 *       7. Set `tin[0]` to `tin0`.
 *       8. Set `tin[1]` to `tin1`.
 *       9. Call `DES_encrypt3()` with the arguments `(DES_LONG *)tin`, `ks1`,
 *          `ks2` and `ks3`.
 *       10. Set `tout0` to `tin[0]`.
 *       11. Set `tout1` to `tin[1]`.
 *       12. Use the macro `l2c()` with the arguments `tout0` and `out`.
 *       13. Use the macro `l2c()` with the arguments `tout1` and `out`.
 *    5. Set `iv` to the address of `(*ivec)[0]`.
 *    6. Use the macro `l2c()` with the arguments `tout0` and `iv`.
 *    7. Use the macro `l2c()` with the arguments `tout1` and `iv`.
 * 6. Otherwise:
 *    1. Use the macro `c2l()` with the arguments `iv` and `xor0`.
 *    2. Use the macro `c2l()` with the arguments `iv` and `xor1`.
 *    3. Subtract `8` from `l`; then while `l` is greater than or equal to `0`,
 *       repeat (after each pass: subtract `8` from `l`):
 *       1. Use the macro `c2l()` with the arguments `in` and `tin0`.
 *       2. Use the macro `c2l()` with the arguments `in` and `tin1`.
 *       3. Set `t0` to `tin0`.
 *       4. Set `t1` to `tin1`.
 *       5. Set `tin[0]` to `tin0`.
 *       6. Set `tin[1]` to `tin1`.
 *       7. Call `DES_decrypt3()` with the arguments `(DES_LONG *)tin`, `ks1`,
 *          `ks2` and `ks3`.
 *       8. Set `tout0` to `tin[0]`.
 *       9. Set `tout1` to `tin[1]`.
 *       10. Toggle the bits `xor0` in `tout0`.
 *       11. Toggle the bits `xor1` in `tout1`.
 *       12. Use the macro `l2c()` with the arguments `tout0` and `out`.
 *       13. Use the macro `l2c()` with the arguments `tout1` and `out`.
 *       14. Set `xor0` to `t0`.
 *       15. Set `xor1` to `t1`.
 *    4. If `l` is not equal to `-8`:
 *       1. Use the macro `c2l()` with the arguments `in` and `tin0`.
 *       2. Use the macro `c2l()` with the arguments `in` and `tin1`.
 *       3. Set `t0` to `tin0`.
 *       4. Set `t1` to `tin1`.
 *       5. Set `tin[0]` to `tin0`.
 *       6. Set `tin[1]` to `tin1`.
 *       7. Call `DES_decrypt3()` with the arguments `(DES_LONG *)tin`, `ks1`,
 *          `ks2` and `ks3`.
 *       8. Set `tout0` to `tin[0]`.
 *       9. Set `tout1` to `tin[1]`.
 *       10. Toggle the bits `xor0` in `tout0`.
 *       11. Toggle the bits `xor1` in `tout1`.
 *       12. Use the macro `l2cn()` with the arguments `tout0`, `tout1`, `out`
 *           and `l+8`.
 *       13. Use the macro `l2cn()` with the arguments `tout0`, `tout1`, `out`
 *           and `l+8`.
 *       14. Do nothing.
 *       15. Set `xor0` to `t0`.
 *       16. Set `xor1` to `t1`.
 *    5. Set `iv` to the address of `(*ivec)[0]`.
 *    6. Use the macro `l2c()` with the arguments `xor0` and `iv`.
 *    7. Use the macro `l2c()` with the arguments `xor1` and `iv`.
 * 7. Set `tin0` to `tin1=tout0=tout1=xor0=xor1=0`.
 * 8. Set `tin[0]` to `tin[1]=0`.
 *
 * @calls
 * - `DES_encrypt3()`
 * - `DES_decrypt3()`
 */
void DES_ede3_cbc_encrypt(const unsigned char *input, unsigned char *output,
			  long length, DES_key_schedule *ks1,
			  DES_key_schedule *ks2, DES_key_schedule *ks3,
			  DES_cblock *ivec, int enc)
	{
	register DES_LONG tin0,tin1;
	register DES_LONG tout0,tout1,xor0,xor1;
	register const unsigned char *in;
	unsigned char *out;
	register long l=length;
	DES_LONG tin[2];
	unsigned char *iv;

	in=input;
	out=output;
	iv = &(*ivec)[0];

	if (enc)
		{
		c2l(iv,tout0);
		c2l(iv,tout1);
		for (l-=8; l>=0; l-=8)
			{
			c2l(in,tin0);
			c2l(in,tin1);
			tin0^=tout0;
			tin1^=tout1;

			tin[0]=tin0;
			tin[1]=tin1;
			DES_encrypt3((DES_LONG *)tin,ks1,ks2,ks3);
			tout0=tin[0];
			tout1=tin[1];

			l2c(tout0,out);
			l2c(tout1,out);
			}
		if (l != -8)
			{
			c2ln(in,tin0,tin1,l+8);
			tin0^=tout0;
			tin1^=tout1;

			tin[0]=tin0;
			tin[1]=tin1;
			DES_encrypt3((DES_LONG *)tin,ks1,ks2,ks3);
			tout0=tin[0];
			tout1=tin[1];

			l2c(tout0,out);
			l2c(tout1,out);
			}
		iv = &(*ivec)[0];
		l2c(tout0,iv);
		l2c(tout1,iv);
		}
	else
		{
		register DES_LONG t0,t1;

		c2l(iv,xor0);
		c2l(iv,xor1);
		for (l-=8; l>=0; l-=8)
			{
			c2l(in,tin0);
			c2l(in,tin1);

			t0=tin0;
			t1=tin1;

			tin[0]=tin0;
			tin[1]=tin1;
			DES_decrypt3((DES_LONG *)tin,ks1,ks2,ks3);
			tout0=tin[0];
			tout1=tin[1];

			tout0^=xor0;
			tout1^=xor1;
			l2c(tout0,out);
			l2c(tout1,out);
			xor0=t0;
			xor1=t1;
			}
		if (l != -8)
			{
			c2l(in,tin0);
			c2l(in,tin1);
			
			t0=tin0;
			t1=tin1;

			tin[0]=tin0;
			tin[1]=tin1;
			DES_decrypt3((DES_LONG *)tin,ks1,ks2,ks3);
			tout0=tin[0];
			tout1=tin[1];
		
			tout0^=xor0;
			tout1^=xor1;
			l2cn(tout0,tout1,out,l+8);
			xor0=t0;
			xor1=t1;
			}

		iv = &(*ivec)[0];
		l2c(xor0,iv);
		l2c(xor1,iv);
		}
	tin0=tin1=tout0=tout1=xor0=xor1=0;
	tin[0]=tin[1]=0;
	}

#endif

#endif /* DES_DEFAULT_OPTIONS */

#endif /* POK_NEEDS ... */
