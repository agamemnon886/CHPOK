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
 */

/*
 * x86_64 control registers, the EFER MSR and the large page size: what
 * the start-up code of the kernel (kernel/csps/x86_64) and of the
 * hypervisor (hypervisor/csps/x86_64) needs to enter long mode. The
 * header is read by assembly code too. What only one of them uses is in
 * its own files.
 *
 * The definitions were in kernel/csps/x86_64/entry.S and space.h, and in
 * hypervisor/csps/x86_64/entry.S and include/arch/x86.h, each with its
 * own copy.
 */

#ifndef JET_CPU_REGS_H
#define JET_CPU_REGS_H

/*
 * MISRA C:2025 Rule 20.10 deviation: X86_U() pastes the U suffix to a
 * constant for C code; the assembler, which reads this header too, doesn't
 * accept the suffix.
 */
#ifdef __ASSEMBLER__
/**
 * @macroname `X86_U`
 *
 * @description
 * An unsigned constant that both C code and assembly code can use: the
 * constant as it is in assembly code, and with the U suffix in C code.
 *
 * @params
 * | Name | Description |
 * |------|-------------|
 * | `v` | The constant, without a suffix. |
 */
#define X86_U(v)                v
#else
/**
 * @macroname `X86_U`
 *
 * @description
 * An unsigned constant that both C code and assembly code can use: the
 * constant as it is in assembly code, and with the U suffix in C code.
 *
 * @params
 * | Name | Description |
 * |------|-------------|
 * | `v` | The constant, without a suffix. |
 */
#define X86_U(v)                v##U
#endif

/**
 * @macroname `CR0_PE`
 *
 * @description
 * The bit of CR0 that enables protected mode.
 */
#define CR0_PE                  X86_U(0x00000001)

/**
 * @macroname `CR0_WP`
 *
 * @description
 * The bit of CR0 that makes read-only pages read-only for supervisor
 * code too.
 */
#define CR0_WP                  X86_U(0x00010000)

/**
 * @macroname `CR0_PG`
 *
 * @description
 * The bit of CR0 that enables paging.
 */
#define CR0_PG                  X86_U(0x80000000)

/**
 * @macroname `CR4_PAE`
 *
 * @description
 * The bit of CR4 that enables physical address extension (needed for
 * long mode).
 */
#define CR4_PAE                 X86_U(0x00000020)

/**
 * @macroname `CR4_PGE`
 *
 * @description
 * The bit of CR4 that enables global pages.
 */
#define CR4_PGE                 X86_U(0x00000080)

/**
 * @macroname `MSR_EFER`
 *
 * @description
 * The number of the extended feature enable register MSR.
 */
#define MSR_EFER                X86_U(0xC0000080)

/**
 * @macroname `EFER_LME`
 *
 * @description
 * The bit of EFER that enables long mode.
 */
#define EFER_LME                X86_U(0x00000100)

/**
 * @macroname `EFER_NXE`
 *
 * @description
 * The bit of EFER that enables the no-execute bit of the page
 * tables.
 */
#define EFER_NXE                X86_U(0x00000800)

/**
 * @macroname `LARGE_PAGE_SIZE`
 *
 * @description
 * The size of a large page (a page directory entry with the page
 * size bit), in bytes: 2 MiB.
 */
#define LARGE_PAGE_SIZE         X86_U(0x200000)

#endif /* JET_CPU_REGS_H */
