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
 * Modified by Kirill Brazhnikov, 2026-11-13: the macros NUM_WEAK_KEY and
 * HPERM_OP are moved here from libpok/protocols/des/set_key.c.
 * The modifications are distributed under the POK licence
 * (BSD-2-Clause, see POK-LICENSE).
 */

/* crypto/des/des_locl.h */
/* Copyright (C) 1995-1997 Eric Young (eay@cryptsoft.com)
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

#include <config.h>

#ifdef POK_NEEDS_PROTOCOLS_DES

#ifndef HEADER_DES_LOCL_H
#define HEADER_DES_LOCL_H

/*
#include <openssl/e_os2.h>
*/


/**
 * @macroname `DES_LONG`
 *
 * @description
 * The type of the 32-bit words of DES.
 */
#define DES_LONG unsigned long
#if defined(OPENSSL_SYS_VMS_NODECC)

/**
 * @macroname `OPENSSL_EXPORT`
 *
 * @description
 * The storage class of an exported global of OpenSSL (extern here).
 */
# define OPENSSL_EXPORT globalref

/**
 * @macroname `OPENSSL_IMPORT`
 *
 * @description
 * The storage class of an imported global of OpenSSL (extern here).
 */
# define OPENSSL_IMPORT globalref

/**
 * @macroname `OPENSSL_GLOBAL`
 *
 * @description
 * The storage class of the definition of a global of OpenSSL (nothing here).
 */
# define OPENSSL_GLOBAL globaldef
#elif defined(OPENSSL_SYS_WINDOWS) && defined(OPENSSL_OPT_WINDLL)

/**
 * @macroname `OPENSSL_EXPORT`
 *
 * @description
 * The storage class of an exported global of OpenSSL (extern here).
 */
# define OPENSSL_EXPORT extern __declspec(dllexport)

/**
 * @macroname `OPENSSL_IMPORT`
 *
 * @description
 * The storage class of an imported global of OpenSSL (extern here).
 */
# define OPENSSL_IMPORT extern __declspec(dllimport)

/**
 * @macroname `OPENSSL_GLOBAL`
 *
 * @description
 * The storage class of the definition of a global of OpenSSL (nothing here).
 */
# define OPENSSL_GLOBAL
#else

/**
 * @macroname `OPENSSL_EXPORT`
 *
 * @description
 * The storage class of an exported global of OpenSSL (extern here).
 */
# define OPENSSL_EXPORT extern

/**
 * @macroname `OPENSSL_IMPORT`
 *
 * @description
 * The storage class of an imported global of OpenSSL (extern here).
 */
# define OPENSSL_IMPORT extern

/**
 * @macroname `OPENSSL_GLOBAL`
 *
 * @description
 * The storage class of the definition of a global of OpenSSL (nothing here).
 */
# define OPENSSL_GLOBAL
#endif

/**
 * @macroname `OPENSSL_EXTERN`
 *
 * @description
 * The storage class of a global of OpenSSL used by the library (OPENSSL_IMPORT,
 * or OPENSSL_EXPORT when building the shared library).
 */
#define OPENSSL_EXTERN OPENSSL_IMPORT


#ifdef OPENSSL_EXPORT_VAR_AS_FUNCTION

/**
 * @macroname `OPENSSL_IMPLEMENT_GLOBAL`
 *
 * @description
 * Defines the global `name` of type `type` of OpenSSL (as _shadow_name, or
 * behind a function that returns its address).
 *
 * @params
 * | Name | Description |
 * |------|-------------|
 * | `type` | Its type. |
 * | `name` | Its name. |
 */
# define OPENSSL_IMPLEMENT_GLOBAL(type,name)                         \
        extern type _hide_##name;                                    \
        type *_shadow_##name(void) { return &_hide_##name; }         \
        static type _hide_##name

/**
 * @macroname `OPENSSL_DECLARE_GLOBAL`
 *
 * @description
 * Declares the global `name` of type `type` of OpenSSL.
 *
 * @params
 * | Name | Description |
 * |------|-------------|
 * | `type` | Its type. |
 * | `name` | Its name. |
 */
# define OPENSSL_DECLARE_GLOBAL(type,name) type *_shadow_##name(void)

/**
 * @macroname `OPENSSL_GLOBAL_REF`
 *
 * @description
 * The global `name` of OpenSSL (an lvalue).
 *
 * @params
 * | Name | Description |
 * |------|-------------|
 * | `name` | Its name. |
 */
# define OPENSSL_GLOBAL_REF(name) (*(_shadow_##name()))
#else

/**
 * @macroname `OPENSSL_IMPLEMENT_GLOBAL`
 *
 * @description
 * Defines the global `name` of type `type` of OpenSSL (as _shadow_name, or
 * behind a function that returns its address).
 *
 * @params
 * | Name | Description |
 * |------|-------------|
 * | `type` | Its type. |
 * | `name` | Its name. |
 */
# define OPENSSL_IMPLEMENT_GLOBAL(type,name) OPENSSL_GLOBAL type _shadow_##name

/**
 * @macroname `OPENSSL_DECLARE_GLOBAL`
 *
 * @description
 * Declares the global `name` of type `type` of OpenSSL.
 *
 * @params
 * | Name | Description |
 * |------|-------------|
 * | `type` | Its type. |
 * | `name` | Its name. |
 */
# define OPENSSL_DECLARE_GLOBAL(type,name) OPENSSL_EXPORT type _shadow_##name

/**
 * @macroname `OPENSSL_GLOBAL_REF`
 *
 * @description
 * The global `name` of OpenSSL (an lvalue).
 *
 * @params
 * | Name | Description |
 * |------|-------------|
 * | `name` | Its name. |
 */
# define OPENSSL_GLOBAL_REF(name) _shadow_##name
#endif


#if defined(OPENSSL_SYS_WIN32) || defined(OPENSSL_SYS_WIN16)
#ifndef OPENSSL_SYS_MSDOS

/**
 * @macroname `OPENSSL_SYS_MSDOS`
 *
 * @description
 * The system is MS-DOS or Windows (not here).
 */
#define OPENSSL_SYS_MSDOS
#endif
#endif

#include <stdio.h>
#include <stdlib.h>

#ifndef OPENSSL_SYS_MSDOS
#if !defined(OPENSSL_SYS_VMS) || defined(__DECC)
#ifdef OPENSSL_UNISTD
# include OPENSSL_UNISTD
#else
# include <libc/stdlib.h>
# include <libc/stdio.h>
#include <libc/string.h>
#endif
//#include <libm.h>
#endif
#endif
#include "des.h"

#ifdef OPENSSL_SYS_MSDOS		/* Visual C++ 2.1 (Windows NT/95) */
#include <stdlib.h>
#include <errno.h>
#include <time.h>
#include <io.h>
#endif

#if defined(__STDC__) || defined(OPENSSL_SYS_VMS) || defined(M_XENIX) || defined(OPENSSL_SYS_MSDOS)
#include <string.h>
#endif

#ifdef OPENSSL_BUILD_SHLIBCRYPTO
# undef OPENSSL_EXTERN

/**
 * @macroname `OPENSSL_EXTERN`
 *
 * @description
 * The storage class of a global of OpenSSL used by the library (OPENSSL_IMPORT,
 * or OPENSSL_EXPORT when building the shared library).
 */
# define OPENSSL_EXTERN OPENSSL_EXPORT
#endif

/**
 * @macroname `ITERATIONS`
 *
 * @description
 * The number of rounds of DES.
 */
#define ITERATIONS 16

/**
 * @macroname `HALF_ITERATIONS`
 *
 * @description
 * Half the number of rounds of DES.
 */
#define HALF_ITERATIONS 8

/**
 * @macroname `MAXWRITE`
 *
 * @description
 * The largest message of DES_enc_read() and DES_enc_write().
 */
#define MAXWRITE	(1024*16)

/**
 * @macroname `BSIZE`
 *
 * @description
 * The size of the buffer of DES_enc_read()/DES_enc_write().
 */
#define BSIZE		(MAXWRITE+4)

/**
 * @macroname `c2l`
 *
 * @description
 * Reads 4 bytes at `c`, least significant first, into `l`, and advances `c`.
 *
 * @params
 * | Name | Description |
 * |------|-------------|
 * | `c` | The pointer to the bytes (an lvalue: unsigned char *). |
 * | `l` | Receives the value. |
 */
#define c2l(c,l)	(l =((DES_LONG)(*((c)++)))    , \
			 l|=((DES_LONG)(*((c)++)))<< 8L, \
			 l|=((DES_LONG)(*((c)++)))<<16L, \
			 l|=((DES_LONG)(*((c)++)))<<24L)

/**
 * @macroname `c2ln`
 *
 * @description
 * Reads `n` (1 to 8) bytes at `c`, least significant first, into `l1` and `l2`;
 * `c` is not advanced (unlike c2l()).
 *
 * @params
 * | Name | Description |
 * |------|-------------|
 * | `c` | The pointer to the bytes (an lvalue: unsigned char *). |
 * | `l1` | Receives the first word. |
 * | `l2` | Receives the second word. |
 * | `n` | The number of bytes. |
 */
#define c2ln(c,l1,l2,n)	{ \
			c+=n; \
			l1=l2=0; \
			switch (n) { \
			case 8: l2 =((DES_LONG)(*(--(c))))<<24L; \
			case 7: l2|=((DES_LONG)(*(--(c))))<<16L; \
			case 6: l2|=((DES_LONG)(*(--(c))))<< 8L; \
			case 5: l2|=((DES_LONG)(*(--(c))));     \
			case 4: l1 =((DES_LONG)(*(--(c))))<<24L; \
			case 3: l1|=((DES_LONG)(*(--(c))))<<16L; \
			case 2: l1|=((DES_LONG)(*(--(c))))<< 8L; \
			case 1: l1|=((DES_LONG)(*(--(c))));     \
				} \
			}

/**
 * @macroname `l2c`
 *
 * @description
 * Writes `l` to 4 bytes at `c`, least significant first, and advances `c`.
 *
 * @params
 * | Name | Description |
 * |------|-------------|
 * | `l` | The value. |
 * | `c` | The pointer to the bytes (an lvalue: unsigned char *). |
 */
#define l2c(l,c)	(*((c)++)=(unsigned char)(((l)     )&0xff), \
			 *((c)++)=(unsigned char)(((l)>> 8L)&0xff), \
			 *((c)++)=(unsigned char)(((l)>>16L)&0xff), \
			 *((c)++)=(unsigned char)(((l)>>24L)&0xff))

/**
 * @macroname `HDRSIZE`
 *
 * @description
 * The size of the length header of DES_enc_read()/DES_enc_write().
 */
#define HDRSIZE 4

/**
 * @macroname `n2l`
 *
 * @description
 * Reads 4 bytes at `c`, most significant first (network order), into `l`, and
 * advances `c`: a replacement for ntohl(), which doesn't fit machines with
 * 8-byte longs.
 *
 * @params
 * | Name | Description |
 * |------|-------------|
 * | `c` | The pointer to the bytes (an lvalue: unsigned char *). |
 * | `l` | Receives the value. |
 */
#define n2l(c,l)	(l =((DES_LONG)(*((c)++)))<<24L, \
			 l|=((DES_LONG)(*((c)++)))<<16L, \
			 l|=((DES_LONG)(*((c)++)))<< 8L, \
			 l|=((DES_LONG)(*((c)++))))

/**
 * @macroname `l2n`
 *
 * @description
 * Writes `l` to 4 bytes at `c`, most significant first (network order), and
 * advances `c`: a replacement for htonl().
 *
 * @params
 * | Name | Description |
 * |------|-------------|
 * | `l` | The value. |
 * | `c` | The pointer to the bytes (an lvalue: unsigned char *). |
 */
#define l2n(l,c)	(*((c)++)=(unsigned char)(((l)>>24L)&0xff), \
			 *((c)++)=(unsigned char)(((l)>>16L)&0xff), \
			 *((c)++)=(unsigned char)(((l)>> 8L)&0xff), \
			 *((c)++)=(unsigned char)(((l)     )&0xff))

/**
 * @macroname `l2cn`
 *
 * @description
 * Writes `n` (1 to 8) bytes of `l1` and `l2` at `c`, least significant first;
 * `c` is not advanced (unlike l2c()).
 *
 * @params
 * | Name | Description |
 * |------|-------------|
 * | `l1` | The first word. |
 * | `l2` | The second word. |
 * | `c` | The pointer to the bytes (an lvalue: unsigned char *). |
 * | `n` | The number of bytes. |
 */
#define l2cn(l1,l2,c,n)	{ \
			c+=n; \
			switch (n) { \
			case 8: *(--(c))=(unsigned char)(((l2)>>24L)&0xff); \
			case 7: *(--(c))=(unsigned char)(((l2)>>16L)&0xff); \
			case 6: *(--(c))=(unsigned char)(((l2)>> 8L)&0xff); \
			case 5: *(--(c))=(unsigned char)(((l2)     )&0xff); \
			case 4: *(--(c))=(unsigned char)(((l1)>>24L)&0xff); \
			case 3: *(--(c))=(unsigned char)(((l1)>>16L)&0xff); \
			case 2: *(--(c))=(unsigned char)(((l1)>> 8L)&0xff); \
			case 1: *(--(c))=(unsigned char)(((l1)     )&0xff); \
				} \
			}

#if (defined(OPENSSL_SYS_WIN32) && defined(_MSC_VER)) || defined(__ICC)

/**
 * @macroname `ROTATE`
 *
 * @description
 * Rotates the 32-bit word `a` right by `n` bits (an instruction where the
 * compiler allows it).
 *
 * @params
 * | Name | Description |
 * |------|-------------|
 * | `a` | The word. |
 * | `n` | The number of bits. |
 */
#define	ROTATE(a,n)	(_lrotr(a,n))
#elif defined(__GNUC__) && __GNUC__>=2 && !defined(__STRICT_ANSI__) && !defined(OPENSSL_NO_ASM) && !defined(OPENSSL_NO_INLINE_ASM) && !defined(PEDANTIC)
# if defined(__i386) || defined(__i386__) || defined(__x86_64) || defined(__x86_64__)

/**
 * @macroname `ROTATE`
 *
 * @description
 * Rotates the 32-bit word `a` right by `n` bits (an instruction where the
 * compiler allows it).
 *
 * @params
 * | Name | Description |
 * |------|-------------|
 * | `a` | The word. |
 * | `n` | The number of bits. |
 */
#  define ROTATE(a,n)	({ register unsigned int ret;	\
				asm ("rorl %1,%0"	\
					: "=r"(ret)	\
					: "I"(n),"0"(a)	\
					: "cc");	\
			   ret;				\
			})
# endif
#endif
#ifndef ROTATE

/**
 * @macroname `ROTATE`
 *
 * @description
 * Rotates the 32-bit word `a` right by `n` bits (an instruction where the
 * compiler allows it).
 *
 * @params
 * | Name | Description |
 * |------|-------------|
 * | `a` | The word. |
 * | `n` | The number of bits. |
 */
#define	ROTATE(a,n)	(((a)>>(n))+((a)<<(32-(n))))
#endif

/* Don't worry about the LOAD_DATA() stuff, that is used by
 * fcrypt() to add it's little bit to the front */

#ifdef DES_FCRYPT

/**
 * @macroname `LOAD_DATA_tmp`
 *
 * @description
 * Loads the two subkey words of the round `S` mixed with `R` into `u` and `t`
 * (LOAD_DATA() with a temporary of its own).
 *
 * @params
 * | Name | Description |
 * |------|-------------|
 * | `R` | The right half. |
 * | `S` | The index of the round's subkeys. |
 * | `u` | Receives the first word. |
 * | `t` | Receives the second word. |
 * | `E0` | The salt mask (fcrypt()). |
 * | `E1` | The salt mask (fcrypt()). |
 */
#define LOAD_DATA_tmp(R,S,u,t,E0,E1) \
	{ DES_LONG tmp; LOAD_DATA(R,S,u,t,E0,E1,tmp); }

/**
 * @macroname `LOAD_DATA`
 *
 * @description
 * Loads the two subkey words of the round `S` mixed with `R` into `u` and `t`
 * (fcrypt() adds its salt with `E0`, `E1` and `tmp`).
 *
 * @params
 * | Name | Description |
 * |------|-------------|
 * | `R` | The right half. |
 * | `S` | The index of the round's subkeys. |
 * | `u` | Receives the first word. |
 * | `t` | Receives the second word. |
 * | `E0` | The salt mask (fcrypt()). |
 * | `E1` | The salt mask (fcrypt()). |
 * | `tmp` | A temporary. |
 */
#define LOAD_DATA(R,S,u,t,E0,E1,tmp) \
	t=R^(R>>16L); \
	u=t&E0; t&=E1; \
	tmp=(u<<16); u^=R^s[S  ]; u^=tmp; \
	tmp=(t<<16); t^=R^s[S+1]; t^=tmp
#else

/**
 * @macroname `LOAD_DATA_tmp`
 *
 * @description
 * Loads the two subkey words of the round `S` mixed with `R` into `u` and `t`
 * (LOAD_DATA() with a temporary of its own).
 *
 * @params
 * | Name | Description |
 * |------|-------------|
 * | `a` | The right half. |
 * | `b` | The index of the round's subkeys. |
 * | `c` | Receives the first word. |
 * | `d` | Receives the second word. |
 * | `e` | The salt mask (not used without fcrypt()). |
 * | `f` | The salt mask (not used without fcrypt()). |
 */
#define LOAD_DATA_tmp(a,b,c,d,e,f) LOAD_DATA(a,b,c,d,e,f,g)

/**
 * @macroname `LOAD_DATA`
 *
 * @description
 * Loads the two subkey words of the round `S` mixed with `R` into `u` and `t`
 * (fcrypt() adds its salt with `E0`, `E1` and `tmp`).
 *
 * @params
 * | Name | Description |
 * |------|-------------|
 * | `R` | The right half. |
 * | `S` | The index of the round's subkeys. |
 * | `u` | Receives the first word. |
 * | `t` | Receives the second word. |
 * | `E0` | The salt mask (fcrypt()). |
 * | `E1` | The salt mask (fcrypt()). |
 * | `tmp` | A temporary. |
 */
#define LOAD_DATA(R,S,u,t,E0,E1,tmp) \
	u=R^s[S  ]; \
	t=R^s[S+1]
#endif

/* The changes to this macro may help or hinder, depending on the
 * compiler and the architecture.  gcc2 always seems to do well :-).
 * Inspired by Dana How <how@isl.stanford.edu>
 * DO NOT use the alternative version on machines with 8 byte longs.
 * It does not seem to work on the Alpha, even when DES_LONG is 4
 * bytes, probably an issue of accessing non-word aligned objects :-( */
#ifdef DES_PTR

/* It recently occurred to me that 0^0^0^0^0^0^0 == 0, so there
 * is no reason to not xor all the sub items together.  This potentially
 * saves a register since things can be xored directly into L */

#if defined(DES_RISC1) || defined(DES_RISC2)
#ifdef DES_RISC1

/**
 * @macroname `D_ENCRYPT`
 *
 * @description
 * One round of DES: xors into `LL` the S-box output of the right half `R` with
 * the subkeys of the round `S`.
 *
 * @params
 * | Name | Description |
 * |------|-------------|
 * | `LL` | The left half, changed. |
 * | `R` | The right half. |
 * | `S` | The index of the round's subkeys. |
 */
#define D_ENCRYPT(LL,R,S) { \
	unsigned int u1,u2,u3; \
	LOAD_DATA(R,S,u,t,E0,E1,u1); \
	u2=(int)u>>8L; \
	u1=(int)u&0xfc; \
	u2&=0xfc; \
	t=ROTATE(t,4); \
	u>>=16L; \
	LL^= *(const DES_LONG *)(des_SP      +u1); \
	LL^= *(const DES_LONG *)(des_SP+0x200+u2); \
	u3=(int)(u>>8L); \
	u1=(int)u&0xfc; \
	u3&=0xfc; \
	LL^= *(const DES_LONG *)(des_SP+0x400+u1); \
	LL^= *(const DES_LONG *)(des_SP+0x600+u3); \
	u2=(int)t>>8L; \
	u1=(int)t&0xfc; \
	u2&=0xfc; \
	t>>=16L; \
	LL^= *(const DES_LONG *)(des_SP+0x100+u1); \
	LL^= *(const DES_LONG *)(des_SP+0x300+u2); \
	u3=(int)t>>8L; \
	u1=(int)t&0xfc; \
	u3&=0xfc; \
	LL^= *(const DES_LONG *)(des_SP+0x500+u1); \
	LL^= *(const DES_LONG *)(des_SP+0x700+u3); }
#endif
#ifdef DES_RISC2

/**
 * @macroname `D_ENCRYPT`
 *
 * @description
 * One round of DES: xors into `LL` the S-box output of the right half `R` with
 * the subkeys of the round `S`.
 *
 * @params
 * | Name | Description |
 * |------|-------------|
 * | `LL` | The left half, changed. |
 * | `R` | The right half. |
 * | `S` | The index of the round's subkeys. |
 */
#define D_ENCRYPT(LL,R,S) { \
	unsigned int u1,u2,s1,s2; \
	LOAD_DATA(R,S,u,t,E0,E1,u1); \
	u2=(int)u>>8L; \
	u1=(int)u&0xfc; \
	u2&=0xfc; \
	t=ROTATE(t,4); \
	LL^= *(const DES_LONG *)(des_SP      +u1); \
	LL^= *(const DES_LONG *)(des_SP+0x200+u2); \
	s1=(int)(u>>16L); \
	s2=(int)(u>>24L); \
	s1&=0xfc; \
	s2&=0xfc; \
	LL^= *(const DES_LONG *)(des_SP+0x400+s1); \
	LL^= *(const DES_LONG *)(des_SP+0x600+s2); \
	u2=(int)t>>8L; \
	u1=(int)t&0xfc; \
	u2&=0xfc; \
	LL^= *(const DES_LONG *)(des_SP+0x100+u1); \
	LL^= *(const DES_LONG *)(des_SP+0x300+u2); \
	s1=(int)(t>>16L); \
	s2=(int)(t>>24L); \
	s1&=0xfc; \
	s2&=0xfc; \
	LL^= *(const DES_LONG *)(des_SP+0x500+s1); \
	LL^= *(const DES_LONG *)(des_SP+0x700+s2); }
#endif
#else

/**
 * @macroname `D_ENCRYPT`
 *
 * @description
 * One round of DES: xors into `LL` the S-box output of the right half `R` with
 * the subkeys of the round `S`.
 *
 * @params
 * | Name | Description |
 * |------|-------------|
 * | `LL` | The left half, changed. |
 * | `R` | The right half. |
 * | `S` | The index of the round's subkeys. |
 */
#define D_ENCRYPT(LL,R,S) { \
	LOAD_DATA_tmp(R,S,u,t,E0,E1); \
	t=ROTATE(t,4); \
	LL^= \
	*(const DES_LONG *)(des_SP      +((u     )&0xfc))^ \
	*(const DES_LONG *)(des_SP+0x200+((u>> 8L)&0xfc))^ \
	*(const DES_LONG *)(des_SP+0x400+((u>>16L)&0xfc))^ \
	*(const DES_LONG *)(des_SP+0x600+((u>>24L)&0xfc))^ \
	*(const DES_LONG *)(des_SP+0x100+((t     )&0xfc))^ \
	*(const DES_LONG *)(des_SP+0x300+((t>> 8L)&0xfc))^ \
	*(const DES_LONG *)(des_SP+0x500+((t>>16L)&0xfc))^ \
	*(const DES_LONG *)(des_SP+0x700+((t>>24L)&0xfc)); }
#endif

#else /* original version */

#if defined(DES_RISC1) || defined(DES_RISC2)
#ifdef DES_RISC1

/**
 * @macroname `D_ENCRYPT`
 *
 * @description
 * One round of DES: xors into `LL` the S-box output of the right half `R` with
 * the subkeys of the round `S`.
 *
 * @params
 * | Name | Description |
 * |------|-------------|
 * | `LL` | The left half, changed. |
 * | `R` | The right half. |
 * | `S` | The index of the round's subkeys. |
 */
#define D_ENCRYPT(LL,R,S) {\
	unsigned int u1,u2,u3; \
	LOAD_DATA(R,S,u,t,E0,E1,u1); \
	u>>=2L; \
	t=ROTATE(t,6); \
	u2=(int)u>>8L; \
	u1=(int)u&0x3f; \
	u2&=0x3f; \
	u>>=16L; \
	LL^=DES_SPtrans[0][u1]; \
	LL^=DES_SPtrans[2][u2]; \
	u3=(int)u>>8L; \
	u1=(int)u&0x3f; \
	u3&=0x3f; \
	LL^=DES_SPtrans[4][u1]; \
	LL^=DES_SPtrans[6][u3]; \
	u2=(int)t>>8L; \
	u1=(int)t&0x3f; \
	u2&=0x3f; \
	t>>=16L; \
	LL^=DES_SPtrans[1][u1]; \
	LL^=DES_SPtrans[3][u2]; \
	u3=(int)t>>8L; \
	u1=(int)t&0x3f; \
	u3&=0x3f; \
	LL^=DES_SPtrans[5][u1]; \
	LL^=DES_SPtrans[7][u3]; }
#endif
#ifdef DES_RISC2

/**
 * @macroname `D_ENCRYPT`
 *
 * @description
 * One round of DES: xors into `LL` the S-box output of the right half `R` with
 * the subkeys of the round `S`.
 *
 * @params
 * | Name | Description |
 * |------|-------------|
 * | `LL` | The left half, changed. |
 * | `R` | The right half. |
 * | `S` | The index of the round's subkeys. |
 */
#define D_ENCRYPT(LL,R,S) {\
	unsigned int u1,u2,s1,s2; \
	LOAD_DATA(R,S,u,t,E0,E1,u1); \
	u>>=2L; \
	t=ROTATE(t,6); \
	u2=(int)u>>8L; \
	u1=(int)u&0x3f; \
	u2&=0x3f; \
	LL^=DES_SPtrans[0][u1]; \
	LL^=DES_SPtrans[2][u2]; \
	s1=(int)u>>16L; \
	s2=(int)u>>24L; \
	s1&=0x3f; \
	s2&=0x3f; \
	LL^=DES_SPtrans[4][s1]; \
	LL^=DES_SPtrans[6][s2]; \
	u2=(int)t>>8L; \
	u1=(int)t&0x3f; \
	u2&=0x3f; \
	LL^=DES_SPtrans[1][u1]; \
	LL^=DES_SPtrans[3][u2]; \
	s1=(int)t>>16; \
	s2=(int)t>>24L; \
	s1&=0x3f; \
	s2&=0x3f; \
	LL^=DES_SPtrans[5][s1]; \
	LL^=DES_SPtrans[7][s2]; }
#endif

#else

/**
 * @macroname `D_ENCRYPT`
 *
 * @description
 * One round of DES: xors into `LL` the S-box output of the right half `R` with
 * the subkeys of the round `S`.
 *
 * @params
 * | Name | Description |
 * |------|-------------|
 * | `LL` | The left half, changed. |
 * | `R` | The right half. |
 * | `S` | The index of the round's subkeys. |
 */
#define D_ENCRYPT(LL,R,S) {\
	LOAD_DATA_tmp(R,S,u,t,E0,E1); \
	t=ROTATE(t,4); \
	LL^=\
		DES_SPtrans[0][(u>> 2L)&0x3f]^ \
		DES_SPtrans[2][(u>>10L)&0x3f]^ \
		DES_SPtrans[4][(u>>18L)&0x3f]^ \
		DES_SPtrans[6][(u>>26L)&0x3f]^ \
		DES_SPtrans[1][(t>> 2L)&0x3f]^ \
		DES_SPtrans[3][(t>>10L)&0x3f]^ \
		DES_SPtrans[5][(t>>18L)&0x3f]^ \
		DES_SPtrans[7][(t>>26L)&0x3f]; }
#endif
#endif

	/* IP and FP
	 * The problem is more of a geometric problem that random bit fiddling.
	 0  1  2  3  4  5  6  7      62 54 46 38 30 22 14  6
	 8  9 10 11 12 13 14 15      60 52 44 36 28 20 12  4
	16 17 18 19 20 21 22 23      58 50 42 34 26 18 10  2
	24 25 26 27 28 29 30 31  to  56 48 40 32 24 16  8  0

	32 33 34 35 36 37 38 39      63 55 47 39 31 23 15  7
	40 41 42 43 44 45 46 47      61 53 45 37 29 21 13  5
	48 49 50 51 52 53 54 55      59 51 43 35 27 19 11  3
	56 57 58 59 60 61 62 63      57 49 41 33 25 17  9  1

	The output has been subject to swaps of the form
	0 1 -> 3 1 but the odd and even bits have been put into
	2 3    2 0
	different words.  The main trick is to remember that
	t=((l>>size)^r)&(mask);
	r^=t;
	l^=(t<<size);
	can be used to swap and move bits between words.

	So l =  0  1  2  3  r = 16 17 18 19
	        4  5  6  7      20 21 22 23
	        8  9 10 11      24 25 26 27
	       12 13 14 15      28 29 30 31
	becomes (for size == 2 and mask == 0x3333)
	   t =   2^16  3^17 -- --   l =  0  1 16 17  r =  2  3 18 19
		 6^20  7^21 -- --        4  5 20 21       6  7 22 23
		10^24 11^25 -- --        8  9 24 25      10 11 24 25
		14^28 15^29 -- --       12 13 28 29      14 15 28 29

	Thanks for hints from Richard Outerbridge - he told me IP&FP
	could be done in 15 xor, 10 shifts and 5 ands.
	When I finally started to think of the problem in 2D
	I first got ~42 operations without xors.  When I remembered
	how to use xors :-) I got it to its final state.
	*/

/**
 * @macroname `PERM_OP`
 *
 * @description
 * Swaps the bits of `b` selected by the mask `m` with those of `a` `n` places
 * away, using `t` (a step of the initial and final permutations, see the
 * comment above).
 *
 * @params
 * | Name | Description |
 * |------|-------------|
 * | `a` | The first word. |
 * | `b` | The second word. |
 * | `t` | A temporary. |
 * | `n` | The distance. |
 * | `m` | The mask. |
 */
#define PERM_OP(a,b,t,n,m) ((t)=((((a)>>(n))^(b))&(m)),\
	(b)^=(t),\
	(a)^=((t)<<(n)))

/**
 * @macroname `IP`
 *
 * @description
 * The initial permutation of DES on the halves `l` and `r`.
 *
 * @params
 * | Name | Description |
 * |------|-------------|
 * | `l` | The left half. |
 * | `r` | The right half. |
 */
#define IP(l,r) \
	{ \
	register DES_LONG tt; \
	PERM_OP(r,l,tt, 4,0x0f0f0f0fL); \
	PERM_OP(l,r,tt,16,0x0000ffffL); \
	PERM_OP(r,l,tt, 2,0x33333333L); \
	PERM_OP(l,r,tt, 8,0x00ff00ffL); \
	PERM_OP(r,l,tt, 1,0x55555555L); \
	}

/**
 * @macroname `FP`
 *
 * @description
 * The final permutation of DES on the halves `l` and `r`.
 *
 * @params
 * | Name | Description |
 * |------|-------------|
 * | `l` | The left half. |
 * | `r` | The right half. |
 */
#define FP(l,r) \
	{ \
	register DES_LONG tt; \
	PERM_OP(l,r,tt, 1,0x55555555L); \
	PERM_OP(r,l,tt, 8,0x00ff00ffL); \
	PERM_OP(l,r,tt, 2,0x33333333L); \
	PERM_OP(r,l,tt,16,0x0000ffffL); \
	PERM_OP(l,r,tt, 4,0x0f0f0f0fL); \
	}

extern const DES_LONG DES_SPtrans[8][64];

/**
 * @macroname `NUM_WEAK_KEY`
 *
 * @description
 * The number of weak and semi-weak keys of DES (the array weak_keys of
 * set_key.c).
 */
#define NUM_WEAK_KEY	16

/**
 * @macroname `HPERM_OP`
 *
 * @description
 * A half permutation of the key schedule of DES (DES_set_key_unchecked()):
 * swaps the bits of `a` selected by the mask `m` with those `16 - n` places
 * away, using `t`.
 *
 * @params
 * | Name | Description |
 * |------|-------------|
 * | `a` | The value permuted. |
 * | `t` | A temporary. |
 * | `n` | The shift. |
 * | `m` | The mask. |
 */
#define HPERM_OP(a,t,n,m) ((t)=((((a)<<(16-(n)))^(a))&(m)),\
	(a)=(a)^(t)^(t>>(16-(n))))

#endif

#endif /* POK_NEEDS_ ... */
