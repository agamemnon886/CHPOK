/*
 * Copyright (C) 2026 Kirill Brazhnikov <agamemnon886@gmail.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Modified by Kirill Brazhnikov, 2026-11-02: MISRA C:2025: deviations are
 * documented where a guideline can't be followed.
 *
 * Modified by Kirill Brazhnikov, 2026-11-03: MISRA C:2025: deviations are
 * documented where a guideline can't be followed.
 *
 * Modified by Kirill Brazhnikov, 2026-11-08: Doxygen headers for the file
 * and its functions.
 *
 * Modified by Kirill Brazhnikov, 2026-11-14: the enum types are named with _e:
 * pok_ret_e (was pok_ret_t), pok_syscall_id_e (was pok_syscall_id_t).
 *
 * Modified by Kirill Brazhnikov, 2026-11-15: the comments name kernel/csps (it
 * was kernel/arch).
 */

/**
 * @file
 * Syscalls of libpok on arm64.
 */

#include <arch/syscall.h>
#include <types.h>

/**
 * @funcname `lja_do_syscall`
 *
 * @description
 * Performs a syscall. The convention (see kernel/csps/arm64/syscalls.c) is svc
 * \#0 with the syscall ID in x0 and the address of the arguments in x1; the
 * result is returned in x0, other registers are preserved.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `syscall_id` | `pok_syscall_id_e` | [in] The syscall ID. |
 * | `args` | `pok_syscall_args_t *` | [in] The arguments. |
 *
 * @return The result.
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `x0` | `uint64_t` | Register x0: the ID, then the result |
 * | `x1` | `uint64_t` | Register x1: the address of the arguments |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Set `x0` to `(uint64_t)syscall_id`.
 * 2. Set `x1` to `(uint64_t)(uintptr_t)args`.
 * 3. Execute the assembly code `svc #0`.
 * 4. Return `(pok_ret_e)x0`.
 *
 * @calls
 * None.
 */
/*
 * MISRA C:2025 Dir 1.2 and Dir 4.3 deviation: the system call instruction
 * is GNU inline assembly, isolated in this function. Rule 11.4: the address
 * of the arguments is passed in a register, as an integer.
 */
pok_ret_e lja_do_syscall (pok_syscall_id_e syscall_id, pok_syscall_args_t* args)
{
   register uint64_t x0 asm("x0") = (uint64_t)syscall_id;
   register uint64_t x1 asm("x1") = (uint64_t)(uintptr_t)args;

   // The kernel reads arguments and may write to user memory.
   asm volatile ("svc #0"
                 : "+r" (x0)
                 : "r" (x1)
                 : "memory");

   return (pok_ret_e)x0;
}
