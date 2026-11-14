/*
 * Institute for System Programming of the Russian Academy of Sciences
 * Copyright (C) 2016 ISPRAS
 * Copyright (C) 2026 Kirill Brazhnikov <agamemnon886@gmail.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation, Version 3.
 *
 * This program is distributed in the hope # that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * See the GNU General Public License version 3 for more details.
 *
 * This file also incorporates work covered by POK License.
 * Copyright (c) 2007-2009 POK team
 *
 * Modified by Kirill Brazhnikov, 2026-10-31: ported from
 * libpok/arch/x86/include/arch/syscall.h to x86_64 (64-bit).
 *
 * Modified by Kirill Brazhnikov, 2026-11-02: MISRA C:2025: the header
 * guard isn't a reserved identifier; U suffixes for unsigned constants;
 * macro parameters are parenthesized.
 *
 * Modified by Kirill Brazhnikov, 2026-11-03: long lines are wrapped at 80
 * columns.
 *
 * Modified by Kirill Brazhnikov, 2026-11-13: the function declarations are
 * hidden from Doxygen (\cond): only the definitions of the functions are
 * documented.
 *
 * Modified by Kirill Brazhnikov, 2026-11-14: the enum types are named with _e:
 * pok_ret_e (was pok_ret_t), pok_syscall_id_e (was pok_syscall_id_t).
 */

#ifndef LIBPOK_X86_64_SYSCALL_H
#define LIBPOK_X86_64_SYSCALL_H

#include <uapi/syscall_types.h>
#include <types.h>

/**
 * @macroname `LIBJET_ARCH_DECLARE_SYSCALL`
 *
 * @description
 * Tells libpok/include/asp/syscall.h that the architecture defines the
 * lja_syscall0() ... lja_syscall5() macros itself (the same on x86_64 and
 * arm64).
 */
#define LIBJET_ARCH_DECLARE_SYSCALL 1

/* Must match the kernel's definition (kernel/include/core/syscall.h). */
/**
 * @typename `pok_syscall_args_t`
 *
 * @description
 * The arguments of a system call, passed to the kernel by address
 * (lja_do_syscall()). It must match the kernel's definition
 * (kernel/include/core/syscall.h). The same type on x86_64 and arm64
 * (libpok/arch/`<arch>`/include/arch/syscall.h).
 */
typedef struct
{
	 /**
	  * x86_64: The number of arguments used (0 to 5).
	  */
	 uint32_t             nargs;
	 uintptr_t            arg1; /**< x86_64: The first argument. */
	 uintptr_t            arg2; /**< x86_64: The second argument. */
	 uintptr_t            arg3; /**< x86_64: The third argument. */
	 uintptr_t            arg4; /**< x86_64: The fourth argument. */
	 uintptr_t            arg5; /**< x86_64: The fifth argument. */
} pok_syscall_args_t;

/*
 * To reduce the number of functions and improve code coverage, we define
 * only one function to perform the syscall, the other are just maccro.
 */
/** \cond */
pok_ret_e lja_do_syscall (pok_syscall_id_e syscall_id,
    pok_syscall_args_t* args);
/** \endcond */

/**
 * @macroname `lja_syscall0`
 *
 * @description
 * Makes the system call `sid` with 0 arguments: calls lja_do_syscall() with a
 * pok_syscall_args_t made of them (the unused ones 0). Evaluates to the result
 * of the call (pok_ret_e).
 *
 * @params
 * | Name | Description |
 * |------|-------------|
 * | `sid` | The system call (pok_syscall_id_e). |
 */
#define lja_syscall0(sid) \
			 lja_do_syscall((sid), \
	     &((pok_syscall_args_t){0U,0U,0U,0U,0U,0U}))

/**
 * @macroname `lja_syscall1`
 *
 * @description
 * Makes the system call `sid` with 1 argument: calls lja_do_syscall() with a
 * pok_syscall_args_t made of them (the unused ones 0). Evaluates to the result
 * of the call (pok_ret_e).
 *
 * @params
 * | Name | Description |
 * |------|-------------|
 * | `sid` | The system call (pok_syscall_id_e). |
 * | `arg1` | Its argument 1 (uintptr_t). |
 */
#define lja_syscall1(sid,arg1) \
			 lja_do_syscall((sid), \
	     &((pok_syscall_args_t){1U,(arg1),0U,0U,0U,0U}))

/**
 * @macroname `lja_syscall2`
 *
 * @description
 * Makes the system call `sid` with 2 arguments: calls lja_do_syscall() with a
 * pok_syscall_args_t made of them (the unused ones 0). Evaluates to the result
 * of the call (pok_ret_e).
 *
 * @params
 * | Name | Description |
 * |------|-------------|
 * | `sid` | The system call (pok_syscall_id_e). |
 * | `arg1` | Its argument 1 (uintptr_t). |
 * | `arg2` | Its argument 2 (uintptr_t). |
 */
#define lja_syscall2(sid,arg1,arg2) \
			 lja_do_syscall((sid), \
	     &((pok_syscall_args_t){2U,(arg1),(arg2),0U,0U,0U}))

/**
 * @macroname `lja_syscall3`
 *
 * @description
 * Makes the system call `sid` with 3 arguments: calls lja_do_syscall() with a
 * pok_syscall_args_t made of them (the unused ones 0). Evaluates to the result
 * of the call (pok_ret_e).
 *
 * @params
 * | Name | Description |
 * |------|-------------|
 * | `sid` | The system call (pok_syscall_id_e). |
 * | `arg1` | Its argument 1 (uintptr_t). |
 * | `arg2` | Its argument 2 (uintptr_t). |
 * | `arg3` | Its argument 3 (uintptr_t). |
 */
#define lja_syscall3(sid,arg1,arg2,arg3) \
			 lja_do_syscall((sid), \
	     &((pok_syscall_args_t){3U,(arg1),(arg2),(arg3),0U,0U}))

/**
 * @macroname `lja_syscall4`
 *
 * @description
 * Makes the system call `sid` with 4 arguments: calls lja_do_syscall() with a
 * pok_syscall_args_t made of them (the unused ones 0). Evaluates to the result
 * of the call (pok_ret_e).
 *
 * @params
 * | Name | Description |
 * |------|-------------|
 * | `sid` | The system call (pok_syscall_id_e). |
 * | `arg1` | Its argument 1 (uintptr_t). |
 * | `arg2` | Its argument 2 (uintptr_t). |
 * | `arg3` | Its argument 3 (uintptr_t). |
 * | `arg4` | Its argument 4 (uintptr_t). |
 */
#define lja_syscall4(sid,arg1,arg2,arg3,arg4) \
			 lja_do_syscall((sid), \
	     &((pok_syscall_args_t){4U,(arg1),(arg2),(arg3),(arg4),0U}))

/**
 * @macroname `lja_syscall5`
 *
 * @description
 * Makes the system call `sid` with 5 arguments: calls lja_do_syscall() with a
 * pok_syscall_args_t made of them (the unused ones 0). Evaluates to the result
 * of the call (pok_ret_e).
 *
 * @params
 * | Name | Description |
 * |------|-------------|
 * | `sid` | The system call (pok_syscall_id_e). |
 * | `arg1` | Its argument 1 (uintptr_t). |
 * | `arg2` | Its argument 2 (uintptr_t). |
 * | `arg3` | Its argument 3 (uintptr_t). |
 * | `arg4` | Its argument 4 (uintptr_t). |
 * | `arg5` | Its argument 5 (uintptr_t). |
 */
#define lja_syscall5(sid,arg1,arg2,arg3,arg4,arg5) \
			 lja_do_syscall((sid), \
	     &((pok_syscall_args_t){5U,(arg1),(arg2),(arg3),(arg4),(arg5)}))

#endif /* LIBPOK_X86_64_SYSCALL_H */
