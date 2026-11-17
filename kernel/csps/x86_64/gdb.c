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
 * Modified by Kirill Brazhnikov, 2026-11-04: ported from
 * kernel/arch/x86/gdb.c (removed with the 32-bit x86 port) to x86_64:
 * GDB's amd64 registers, including x87 and SSE ones from the FXSAVE area,
 * are read and written one at a time; breakpoint and debug exceptions
 * are passed to the GDB server (as kernel/arch/x86/interrupt.c did);
 * single-stepping and hardware breakpoints and watchpoints use the trap
 * flag and the debug registers; interrupts stay disabled for the stepped
 * instruction, so that no other thread runs before the step is done.
 *
 * Modified by Kirill Brazhnikov, 2026-11-08: Doxygen headers for the file
 * and its functions.
 *
 * Modified by Kirill Brazhnikov, 2026-11-13: the structures and enums are used
 * by their typedef names: interrupt_frame, ja_gdb_hw_type_t, jet_fp_store_t,
 * jet_gdb_stop_reason_t; the macros and the type are moved to
 * kernel/arch/x86_64/include/arch/gdb.h (hw_slot_t, the same on arm64:
 * kernel/include/asp/gdb.h); struct hw_slot is the typedef hw_slot_t.
 *
 * Modified by Kirill Brazhnikov, 2026-11-14: the enum types are named with _e:
 * ja_gdb_hw_type_e (was ja_gdb_hw_type_t), jet_gdb_stop_reason_e (was
 * jet_gdb_stop_reason_t).
 *
 * Modified by Kirill Brazhnikov, 2026-11-15: moved from
 * kernel/arch/x86_64/gdb.c (the CPU support packages are in kernel/csps); the
 * comments name the new paths.
 *
 * Modified by Kirill Brazhnikov, 2026-11-16: high and low level requirements of
 * the functions, traced to the system requirements (docs/SRQs), and the steps
 * of their design linked to the LLRs.
 *
 * Modified by Kirill Brazhnikov, 2026-11-17: low level requirements of one
 * behaviour each; code that compiles to nothing (assertions, (void) x;,
 * empty branches) is left out of the design; requirements with more than one
 * "shall" are split into requirements of one "shall" each, and the high level
 * requirements trace to the system requirements split in the same way.
 */

/**
 * @file
 * Architecture part of the GDB server (kernel/debug/gdbserver.c) on x86_64:
 * GDB's registers (the interrupt frame and the FXSAVE area), single steps
 * (the trap flag), hardware breakpoints and watchpoints (the debug
 * registers) and the int3 and debug exceptions.
 */

#include <config.h>

#ifdef POK_NEEDS_GDB

#include <asp/gdb.h>
#include <asp/space.h>
#include <asp/uaccess.h>
#include <gdb.h>
#include <libc.h>

#include "interrupt.h"
#include "space.h"

/*
 * MISRA C:2025 Dir 1.2 and Dir 4.3 deviation (the cpu_*() functions):
 * the debug registers are accessed with GNU inline assembly, each access
 * isolated in a small function.
 */

/* The breakpoint instruction: int3. */
const uint8_t ja_gdb_breakpoint_insn[JA_GDB_BREAKPOINT_SIZE] = { 0xCCU };

/**
 * @funcname `cpu_read_dr6`
 *
 * @description
 * Reads DR6, the debug status register.
 *
 * @return DR6.
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `value` | `uint64_t` | DR6 |
 *
 * @hlr
 * - \hlrid{HLR_GDB_CPU_READ_DR6_01}: `cpu_read_dr6()` shall give the debug
 *   status register.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_DBG_014}
 *
 * @llr
 * - \llrid{LLR_GDB_CPU_READ_DR6_01}: `cpu_read_dr6()` shall return the value of
 *   DR6.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDB_CPU_READ_DR6_01}
 *
 * @design
 * 1. Execute the assembly code `mov %%%dr6, %0`.
 *    \llrref{LLR_GDB_CPU_READ_DR6_01}
 * 2. Return `value`. \llrref{LLR_GDB_CPU_READ_DR6_01}
 *
 * @calls
 * None.
 */
static uint64_t cpu_read_dr6(void)
{
    uint64_t value;

    asm volatile ("mov %%dr6, %0" : "=r" (value));
    return value;
}

/**
 * @funcname `cpu_write_dr6`
 *
 * @description
 * Writes DR6, the debug status register.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `value` | `uint64_t` | [in] The value. |
 *
 * @globals
 * None.
 *
 * @locals
 * None.
 *
 * @hlr
 * - \hlrid{HLR_GDB_CPU_WRITE_DR6_01}: `cpu_write_dr6()` shall set the debug
 *   status register.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_DBG_014}
 *
 * @llr
 * - \llrid{LLR_GDB_CPU_WRITE_DR6_01}: `cpu_write_dr6()` shall write `value` to
 *   DR6.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDB_CPU_WRITE_DR6_01}
 *
 * @design
 * 1. Execute the assembly code `mov %0, %%%dr6`.
 *    \llrref{LLR_GDB_CPU_WRITE_DR6_01}
 *
 * @calls
 * None.
 */
static void cpu_write_dr6(uint64_t value)
{
    asm volatile ("mov %0, %%dr6" : : "r" (value));
}

/**
 * @funcname `cpu_write_dr7`
 *
 * @description
 * Writes DR7, the debug control register.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `value` | `uint64_t` | [in] The value. |
 *
 * @globals
 * None.
 *
 * @locals
 * None.
 *
 * @hlr
 * - \hlrid{HLR_GDB_CPU_WRITE_DR7_01}: `cpu_write_dr7()` shall set the debug
 *   control register.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_DBG_005}
 *
 * @llr
 * - \llrid{LLR_GDB_CPU_WRITE_DR7_01}: `cpu_write_dr7()` shall write `value` to
 *   DR7.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDB_CPU_WRITE_DR7_01}
 *
 * @design
 * 1. Execute the assembly code `mov %0, %%%dr7`.
 *    \llrref{LLR_GDB_CPU_WRITE_DR7_01}
 *
 * @calls
 * None.
 */
static void cpu_write_dr7(uint64_t value)
{
    asm volatile ("mov %0, %%dr7" : : "r" (value));
}

/**
 * @funcname `cpu_write_dr_addr`
 *
 * @description
 * Writes a debug address register DR0-DR3.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `index` | `uint32_t` | [in] The register's number (0-3). |
 * | `value` | `uint64_t` | [in] The address. |
 *
 * @globals
 * None.
 *
 * @locals
 * None.
 *
 * @hlr
 * - \hlrid{HLR_GDB_CPU_WRITE_DR_ADDR_01}: `cpu_write_dr_addr()` shall set one
 *   of the four debug address registers.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_DBG_005}
 *
 * @llr
 * - \llrid{LLR_GDB_CPU_WRITE_DR_ADDR_01}: If `index` is 0, 1, 2 or 3,
 *   `cpu_write_dr_addr()` shall write `value` to DR0, DR1, DR2 or DR3, in this
 *   order.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDB_CPU_WRITE_DR_ADDR_01}
 * - \llrid{LLR_GDB_CPU_WRITE_DR_ADDR_02}: If `index` is greater than 3,
 *   `cpu_write_dr_addr()` shall write no register.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDB_CPU_WRITE_DR_ADDR_01}
 *
 * @design
 * 1. Depending on `index`: \llrref{LLR_GDB_CPU_WRITE_DR_ADDR_01},
 *    \llrref{LLR_GDB_CPU_WRITE_DR_ADDR_02}
 *    1. Case `0U`: \llrref{LLR_GDB_CPU_WRITE_DR_ADDR_01}
 *       1. Execute the assembly code `mov %0, %%%dr0`.
 *       2. Leave the switch.
 *    2. Case `1U`: \llrref{LLR_GDB_CPU_WRITE_DR_ADDR_01}
 *       1. Execute the assembly code `mov %0, %%%dr1`.
 *       2. Leave the switch.
 *    3. Case `2U`: \llrref{LLR_GDB_CPU_WRITE_DR_ADDR_01}
 *       1. Execute the assembly code `mov %0, %%%dr2`.
 *       2. Leave the switch.
 *    4. Case `3U`: \llrref{LLR_GDB_CPU_WRITE_DR_ADDR_01}
 *       1. Execute the assembly code `mov %0, %%%dr3`.
 *       2. Leave the switch.
 *    5. Otherwise (default): \llrref{LLR_GDB_CPU_WRITE_DR_ADDR_02}
 *       1. Leave the switch.
 *
 * @calls
 * None.
 */
static void cpu_write_dr_addr(uint32_t index, uint64_t value)
{
    switch (index)
    {
    case 0U:
        asm volatile ("mov %0, %%dr0" : : "r" (value));
        break;
    case 1U:
        asm volatile ("mov %0, %%dr1" : : "r" (value));
        break;
    case 2U:
        asm volatile ("mov %0, %%dr2" : : "r" (value));
        break;
    case 3U:
        asm volatile ("mov %0, %%dr3" : : "r" (value));
        break;
    default:
        // There are only 4 address registers.
        break;
    }
}

/**
 * @funcname `put_le`
 *
 * @description
 * Stores the low bytes of a value, little-endian.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `value` | `uint64_t` | [in] The value. |
 * | `size` | `size_t` | [in] The number of bytes. |
 * | `out` | `uint8_t *` | [out] Where they are stored. |
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `i` | `size_t` | The index of the byte |
 *
 * @hlr
 * - \hlrid{HLR_GDB_X86_64_PUT_LE_01}: `put_le()` shall store the low bytes of a
 *   value in little-endian order, the order of the registers in the protocol.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_DBG_004}
 *
 * @llr
 * - \llrid{LLR_GDB_X86_64_PUT_LE_01}: `put_le()` shall store the bits `8 * i +
 *   7` to `8 * i` of `value` at `out[i]`, for each `i` less than `size`.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDB_X86_64_PUT_LE_01}
 *
 * @design
 * 1. Set `i` to `0U`; then while `i` is less than `size`, repeat (after each
 *    pass: increment `i`): \llrref{LLR_GDB_X86_64_PUT_LE_01}
 *    1. Set `out[i]` to `(uint8_t)(value >> (8U * i))`.
 *
 * @calls
 * None.
 */
static void put_le(uint64_t value, size_t size, uint8_t* out)
{
    for (size_t i = 0U; i < size; i++)
    {
        out[i] = (uint8_t)(value >> (8U * i));
    }
}

/**
 * @funcname `get_le`
 *
 * @description
 * Loads bytes at an offset as a little-endian value.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `in` | `const uint8_t *` | [in] The bytes. |
 * | `offset` | `size_t` | [in] The offset. |
 * | `size` | `size_t` | [in] The number of bytes. |
 *
 * @return The value.
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `i` | `size_t` | The index of the byte |
 * | `value` | `uint64_t` | The value |
 *
 * @hlr
 * - \hlrid{HLR_GDB_X86_64_GET_LE_01}: `get_le()` shall give the value of bytes
 *   in little-endian order.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_DBG_004}
 *
 * @llr
 * - \llrid{LLR_GDB_X86_64_GET_LE_01}: `get_le()` shall return the value whose
 *   bits `8 * i + 7` to `8 * i` are `in[offset + i]`, for each `i` less than
 *   `size`, and whose other bits are 0.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDB_X86_64_GET_LE_01}
 *
 * @design
 * 1. Set `value` to `0U`. \llrref{LLR_GDB_X86_64_GET_LE_01}
 * 2. Set `i` to `0U`; then while `i` is less than `size`, repeat (after each
 *    pass: increment `i`): \llrref{LLR_GDB_X86_64_GET_LE_01}
 *    1. Set the bits `(uint64_t)in[offset + i] << (8U * i)` in `value`.
 * 3. Return `value`. \llrref{LLR_GDB_X86_64_GET_LE_01}
 *
 * @calls
 * None.
 */
static uint64_t get_le(const uint8_t* in, size_t offset, size_t size)
{
    uint64_t value = 0U;

    for (size_t i = 0U; i < size; i++)
    {
        value |= (uint64_t)in[offset + i] << (8U * i);
    }

    return value;
}

/**
 * @funcname `ja_gdb_register_size`
 *
 * @description
 * Returns the size of a GDB register: 8 for the integer registers and rip, 4
 * for eflags, the segment and x87 control registers and mxcsr, 10 for st0-st7,
 * 16 for xmm0-xmm15.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `regno` | `uint32_t` | [in] GDB's number of the register. |
 *
 * @return The size, or 0 for no such register.
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `size` | `size_t` | The size |
 *
 * @hlr
 * - \hlrid{HLR_GDB_X86_64_JA_GDB_REGISTER_SIZE_01}: `ja_gdb_register_size()`
 *   shall give the size of each register that the debugger knows: 8 bytes for
 *   the integer registers and RIP, 4 for EFLAGS, the segment registers, the x87
 *   control registers and MXCSR, 10 for ST0 to ST7, 16 for XMM0 to XMM15, and 0
 *   for no such register.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_DBG_004}
 *
 * @llr
 * - \llrid{LLR_GDB_X86_64_JA_GDB_REGISTER_SIZE_01}: If `regno` is not greater
 *   than `REG_RIP`, `ja_gdb_register_size()` shall return 8.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDB_X86_64_JA_GDB_REGISTER_SIZE_01}
 * - \llrid{LLR_GDB_X86_64_JA_GDB_REGISTER_SIZE_02}: If `regno` is greater than
 *   `REG_RIP` and less than `REG_ST0`, `ja_gdb_register_size()` shall return 4.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDB_X86_64_JA_GDB_REGISTER_SIZE_01}
 * - \llrid{LLR_GDB_X86_64_JA_GDB_REGISTER_SIZE_03}: If `regno` is not less than
 *   `REG_ST0` and is less than `REG_FCTRL`, `ja_gdb_register_size()` shall
 *   return `ST_SIZE`.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDB_X86_64_JA_GDB_REGISTER_SIZE_01}
 * - \llrid{LLR_GDB_X86_64_JA_GDB_REGISTER_SIZE_04}: If `regno` is not less than
 *   `REG_FCTRL` and is less than `REG_XMM0`, `ja_gdb_register_size()` shall
 *   return 4.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDB_X86_64_JA_GDB_REGISTER_SIZE_01}
 * - \llrid{LLR_GDB_X86_64_JA_GDB_REGISTER_SIZE_05}: If `regno` is not less than
 *   `REG_XMM0` and is less than `REG_MXCSR`, `ja_gdb_register_size()` shall
 *   return 16.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDB_X86_64_JA_GDB_REGISTER_SIZE_01}
 * - \llrid{LLR_GDB_X86_64_JA_GDB_REGISTER_SIZE_06}: If `regno` is `REG_MXCSR`,
 *   `ja_gdb_register_size()` shall return 4.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDB_X86_64_JA_GDB_REGISTER_SIZE_01}
 * - \llrid{LLR_GDB_X86_64_JA_GDB_REGISTER_SIZE_07}: If `regno` is greater than
 *   `REG_MXCSR`, `ja_gdb_register_size()` shall return 0.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDB_X86_64_JA_GDB_REGISTER_SIZE_01}
 *
 * @design
 * 1. If `regno` is less than or equal to `REG_RIP`:
 *    \llrref{LLR_GDB_X86_64_JA_GDB_REGISTER_SIZE_01}
 *    1. Set `size` to `8U`.
 * 2. Otherwise, if `regno` is less than `REG_ST0`:
 *    \llrref{LLR_GDB_X86_64_JA_GDB_REGISTER_SIZE_02}
 *    1. Set `size` to `4U`.
 * 3. Otherwise, if `regno` is less than `REG_FCTRL`:
 *    \llrref{LLR_GDB_X86_64_JA_GDB_REGISTER_SIZE_03}
 *    1. Set `size` to `ST_SIZE`.
 * 4. Otherwise, if `regno` is less than `REG_XMM0`:
 *    \llrref{LLR_GDB_X86_64_JA_GDB_REGISTER_SIZE_04}
 *    1. Set `size` to `4U`.
 * 5. Otherwise, if `regno` is less than `REG_MXCSR`:
 *    \llrref{LLR_GDB_X86_64_JA_GDB_REGISTER_SIZE_05}
 *    1. Set `size` to `16U`.
 * 6. Otherwise, if `regno` is equal to `REG_MXCSR`:
 *    \llrref{LLR_GDB_X86_64_JA_GDB_REGISTER_SIZE_06}
 *    1. Set `size` to `4U`.
 * 7. Otherwise: \llrref{LLR_GDB_X86_64_JA_GDB_REGISTER_SIZE_07}
 *    1. Set `size` to `0U`.
 * 8. Return `size`. \llrref{LLR_GDB_X86_64_JA_GDB_REGISTER_SIZE_01},
 *    \llrref{LLR_GDB_X86_64_JA_GDB_REGISTER_SIZE_02},
 *    \llrref{LLR_GDB_X86_64_JA_GDB_REGISTER_SIZE_03},
 *    \llrref{LLR_GDB_X86_64_JA_GDB_REGISTER_SIZE_04},
 *    \llrref{LLR_GDB_X86_64_JA_GDB_REGISTER_SIZE_05},
 *    \llrref{LLR_GDB_X86_64_JA_GDB_REGISTER_SIZE_06},
 *    \llrref{LLR_GDB_X86_64_JA_GDB_REGISTER_SIZE_07}
 *
 * @calls
 * None.
 */
size_t ja_gdb_register_size(uint32_t regno)
{
    size_t size;

    if (regno <= REG_RIP) {
        size = 8U;
    } else if (regno < REG_ST0) {
        size = 4U;
    } else if (regno < REG_FCTRL) {
        size = ST_SIZE;
    } else if (regno < REG_XMM0) {
        size = 4U;
    } else if (regno < REG_MXCSR) {
        size = 16U;
    } else if (regno == REG_MXCSR) {
        size = 4U;
    } else {
        size = 0U;
    }

    return size;
}

/**
 * @funcname `frame_register_get`
 *
 * @description
 * Returns an integer register (up to ss) of an interrupt frame.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `frame` | `const interrupt_frame *` | [in] The frame. |
 * | `regno` | `uint32_t` | [in] GDB's number of the register. |
 *
 * @return The value (0 for no such register).
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `value` | `uint64_t` | The value |
 *
 * @hlr
 * - \hlrid{HLR_GDB_X86_64_FRAME_REGISTER_GET_01}: `frame_register_get()` shall
 *   give an integer register, up to SS, from an interrupt frame.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_DBG_004}
 *
 * @llr
 * - \llrid{LLR_GDB_X86_64_FRAME_REGISTER_GET_01}: If `regno` is from 0 to 19,
 *   `frame_register_get()` shall return `rax`, `rbx`, `rcx`, `rdx`, `rsi`,
 *   `rdi`, `rbp`, `rsp`, `r8`, `r9`, `r10`, `r11`, `r12`, `r13`, `r14`, `r15`,
 *   `rip`, `rflags`, `cs` or `ss` of `frame`, in this order.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDB_X86_64_FRAME_REGISTER_GET_01}
 * - \llrid{LLR_GDB_X86_64_FRAME_REGISTER_GET_02}: If `regno` is greater than
 *   19, `frame_register_get()` shall return 0.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDB_X86_64_FRAME_REGISTER_GET_01}
 *
 * @design
 * 1. Depending on `regno`: \llrref{LLR_GDB_X86_64_FRAME_REGISTER_GET_01},
 *    \llrref{LLR_GDB_X86_64_FRAME_REGISTER_GET_02}
 *    1. Case `0U`: \llrref{LLR_GDB_X86_64_FRAME_REGISTER_GET_01}
 *       1. Set `value` to `frame->rax`.
 *       2. Leave the switch.
 *    2. Case `1U`: \llrref{LLR_GDB_X86_64_FRAME_REGISTER_GET_01}
 *       1. Set `value` to `frame->rbx`.
 *       2. Leave the switch.
 *    3. Case `2U`: \llrref{LLR_GDB_X86_64_FRAME_REGISTER_GET_01}
 *       1. Set `value` to `frame->rcx`.
 *       2. Leave the switch.
 *    4. Case `3U`: \llrref{LLR_GDB_X86_64_FRAME_REGISTER_GET_01}
 *       1. Set `value` to `frame->rdx`.
 *       2. Leave the switch.
 *    5. Case `4U`: \llrref{LLR_GDB_X86_64_FRAME_REGISTER_GET_01}
 *       1. Set `value` to `frame->rsi`.
 *       2. Leave the switch.
 *    6. Case `5U`: \llrref{LLR_GDB_X86_64_FRAME_REGISTER_GET_01}
 *       1. Set `value` to `frame->rdi`.
 *       2. Leave the switch.
 *    7. Case `6U`: \llrref{LLR_GDB_X86_64_FRAME_REGISTER_GET_01}
 *       1. Set `value` to `frame->rbp`.
 *       2. Leave the switch.
 *    8. Case `7U`: \llrref{LLR_GDB_X86_64_FRAME_REGISTER_GET_01}
 *       1. Set `value` to `frame->rsp`.
 *       2. Leave the switch.
 *    9. Case `8U`: \llrref{LLR_GDB_X86_64_FRAME_REGISTER_GET_01}
 *       1. Set `value` to `frame->r8`.
 *       2. Leave the switch.
 *    10. Case `9U`: \llrref{LLR_GDB_X86_64_FRAME_REGISTER_GET_01}
 *        1. Set `value` to `frame->r9`.
 *        2. Leave the switch.
 *    11. Case `10U`: \llrref{LLR_GDB_X86_64_FRAME_REGISTER_GET_01}
 *        1. Set `value` to `frame->r10`.
 *        2. Leave the switch.
 *    12. Case `11U`: \llrref{LLR_GDB_X86_64_FRAME_REGISTER_GET_01}
 *        1. Set `value` to `frame->r11`.
 *        2. Leave the switch.
 *    13. Case `12U`: \llrref{LLR_GDB_X86_64_FRAME_REGISTER_GET_01}
 *        1. Set `value` to `frame->r12`.
 *        2. Leave the switch.
 *    14. Case `13U`: \llrref{LLR_GDB_X86_64_FRAME_REGISTER_GET_01}
 *        1. Set `value` to `frame->r13`.
 *        2. Leave the switch.
 *    15. Case `14U`: \llrref{LLR_GDB_X86_64_FRAME_REGISTER_GET_01}
 *        1. Set `value` to `frame->r14`.
 *        2. Leave the switch.
 *    16. Case `15U`: \llrref{LLR_GDB_X86_64_FRAME_REGISTER_GET_01}
 *        1. Set `value` to `frame->r15`.
 *        2. Leave the switch.
 *    17. Case `16U`: \llrref{LLR_GDB_X86_64_FRAME_REGISTER_GET_01}
 *        1. Set `value` to `frame->rip`.
 *        2. Leave the switch.
 *    18. Case `17U`: \llrref{LLR_GDB_X86_64_FRAME_REGISTER_GET_01}
 *        1. Set `value` to `frame->rflags`.
 *        2. Leave the switch.
 *    19. Case `18U`: \llrref{LLR_GDB_X86_64_FRAME_REGISTER_GET_01}
 *        1. Set `value` to `frame->cs`.
 *        2. Leave the switch.
 *    20. Case `19U`: \llrref{LLR_GDB_X86_64_FRAME_REGISTER_GET_01}
 *        1. Set `value` to `frame->ss`.
 *        2. Leave the switch.
 *    21. Otherwise (default): \llrref{LLR_GDB_X86_64_FRAME_REGISTER_GET_02}
 *        1. Set `value` to `0U`.
 *        2. Leave the switch.
 * 2. Return `value`. \llrref{LLR_GDB_X86_64_FRAME_REGISTER_GET_01},
 *    \llrref{LLR_GDB_X86_64_FRAME_REGISTER_GET_02}
 *
 * @calls
 * None.
 */
static uint64_t frame_register_get(const interrupt_frame* frame,
    uint32_t regno)
{
    uint64_t value;

    switch (regno)
    {
    case 0U:  value = frame->rax; break;
    case 1U:  value = frame->rbx; break;
    case 2U:  value = frame->rcx; break;
    case 3U:  value = frame->rdx; break;
    case 4U:  value = frame->rsi; break;
    case 5U:  value = frame->rdi; break;
    case 6U:  value = frame->rbp; break;
    case 7U:  value = frame->rsp; break;
    case 8U:  value = frame->r8; break;
    case 9U:  value = frame->r9; break;
    case 10U: value = frame->r10; break;
    case 11U: value = frame->r11; break;
    case 12U: value = frame->r12; break;
    case 13U: value = frame->r13; break;
    case 14U: value = frame->r14; break;
    case 15U: value = frame->r15; break;
    case 16U: value = frame->rip; break;
    case 17U: value = frame->rflags; break;
    case 18U: value = frame->cs; break;
    case 19U: value = frame->ss; break;
    default:  value = 0U; break;
    }

    return value;
}

/**
 * @funcname `frame_register_set`
 *
 * @description
 * Sets an integer register (up to eflags) of an interrupt frame. Only the
 * status flags of eflags are changed; segment registers never are.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `frame` | `interrupt_frame *` | [out] The frame. |
 * | `regno` | `uint32_t` | [in] GDB's number of the register. |
 * | `value` | `uint64_t` | [in] The value. |
 *
 * @globals
 * None.
 *
 * @locals
 * None.
 *
 * @hlr
 * - \hlrid{HLR_GDB_X86_64_FRAME_REGISTER_SET_01}: `frame_register_set()` shall
 *   set an integer register, up to EFLAGS, in an interrupt frame, changing only
 *   the flags that the debugger may change and never a segment register.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_DBG_004}
 *
 * @llr
 * - \llrid{LLR_GDB_X86_64_FRAME_REGISTER_SET_01}: If `regno` is from 0 to 16,
 *   `frame_register_set()` shall set `rax`, `rbx`, `rcx`, `rdx`, `rsi`, `rdi`,
 *   `rbp`, `rsp`, `r8`, `r9`, `r10`, `r11`, `r12`, `r13`, `r14`, `r15` or `rip`
 *   of `frame`, in this order, to `value`.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDB_X86_64_FRAME_REGISTER_SET_01}
 * - \llrid{LLR_GDB_X86_64_FRAME_REGISTER_SET_02}: If `regno` is 17,
 *   `frame_register_set()` shall set the bits `RFLAGS_GDB_WRITABLE` of `rflags`
 *   of `frame` to those of `value`, and leave its other bits as they are.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDB_X86_64_FRAME_REGISTER_SET_01}
 * - \llrid{LLR_GDB_X86_64_FRAME_REGISTER_SET_03}: If `regno` is greater than
 *   17, `frame_register_set()` shall leave `frame` as it is.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDB_X86_64_FRAME_REGISTER_SET_01}
 *
 * @design
 * 1. Depending on `regno`: \llrref{LLR_GDB_X86_64_FRAME_REGISTER_SET_01},
 *    \llrref{LLR_GDB_X86_64_FRAME_REGISTER_SET_02},
 *    \llrref{LLR_GDB_X86_64_FRAME_REGISTER_SET_03}
 *    1. Case `0U`: \llrref{LLR_GDB_X86_64_FRAME_REGISTER_SET_01}
 *       1. Set `frame->rax` to `value`.
 *       2. Leave the switch.
 *    2. Case `1U`: \llrref{LLR_GDB_X86_64_FRAME_REGISTER_SET_01}
 *       1. Set `frame->rbx` to `value`.
 *       2. Leave the switch.
 *    3. Case `2U`: \llrref{LLR_GDB_X86_64_FRAME_REGISTER_SET_01}
 *       1. Set `frame->rcx` to `value`.
 *       2. Leave the switch.
 *    4. Case `3U`: \llrref{LLR_GDB_X86_64_FRAME_REGISTER_SET_01}
 *       1. Set `frame->rdx` to `value`.
 *       2. Leave the switch.
 *    5. Case `4U`: \llrref{LLR_GDB_X86_64_FRAME_REGISTER_SET_01}
 *       1. Set `frame->rsi` to `value`.
 *       2. Leave the switch.
 *    6. Case `5U`: \llrref{LLR_GDB_X86_64_FRAME_REGISTER_SET_01}
 *       1. Set `frame->rdi` to `value`.
 *       2. Leave the switch.
 *    7. Case `6U`: \llrref{LLR_GDB_X86_64_FRAME_REGISTER_SET_01}
 *       1. Set `frame->rbp` to `value`.
 *       2. Leave the switch.
 *    8. Case `7U`: \llrref{LLR_GDB_X86_64_FRAME_REGISTER_SET_01}
 *       1. Set `frame->rsp` to `value`.
 *       2. Leave the switch.
 *    9. Case `8U`: \llrref{LLR_GDB_X86_64_FRAME_REGISTER_SET_01}
 *       1. Set `frame->r8` to `value`.
 *       2. Leave the switch.
 *    10. Case `9U`: \llrref{LLR_GDB_X86_64_FRAME_REGISTER_SET_01}
 *        1. Set `frame->r9` to `value`.
 *        2. Leave the switch.
 *    11. Case `10U`: \llrref{LLR_GDB_X86_64_FRAME_REGISTER_SET_01}
 *        1. Set `frame->r10` to `value`.
 *        2. Leave the switch.
 *    12. Case `11U`: \llrref{LLR_GDB_X86_64_FRAME_REGISTER_SET_01}
 *        1. Set `frame->r11` to `value`.
 *        2. Leave the switch.
 *    13. Case `12U`: \llrref{LLR_GDB_X86_64_FRAME_REGISTER_SET_01}
 *        1. Set `frame->r12` to `value`.
 *        2. Leave the switch.
 *    14. Case `13U`: \llrref{LLR_GDB_X86_64_FRAME_REGISTER_SET_01}
 *        1. Set `frame->r13` to `value`.
 *        2. Leave the switch.
 *    15. Case `14U`: \llrref{LLR_GDB_X86_64_FRAME_REGISTER_SET_01}
 *        1. Set `frame->r14` to `value`.
 *        2. Leave the switch.
 *    16. Case `15U`: \llrref{LLR_GDB_X86_64_FRAME_REGISTER_SET_01}
 *        1. Set `frame->r15` to `value`.
 *        2. Leave the switch.
 *    17. Case `16U`: \llrref{LLR_GDB_X86_64_FRAME_REGISTER_SET_01}
 *        1. Set `frame->rip` to `value`.
 *        2. Leave the switch.
 *    18. Case `17U`: \llrref{LLR_GDB_X86_64_FRAME_REGISTER_SET_02}
 *        1. Set `frame->rflags` to
 *           `(frame->rflags & ~(uint64_t)RFLAGS_GDB_WRITABLE) | (value &`
 *           `RFLAGS_GDB_WRITABLE)`.
 *        2. Leave the switch.
 *    19. Otherwise (default): \llrref{LLR_GDB_X86_64_FRAME_REGISTER_SET_03}
 *        1. Leave the switch.
 *
 * @calls
 * None.
 */
static void frame_register_set(interrupt_frame* frame, uint32_t regno,
    uint64_t value)
{
    switch (regno)
    {
    case 0U:  frame->rax = value; break;
    case 1U:  frame->rbx = value; break;
    case 2U:  frame->rcx = value; break;
    case 3U:  frame->rdx = value; break;
    case 4U:  frame->rsi = value; break;
    case 5U:  frame->rdi = value; break;
    case 6U:  frame->rbp = value; break;
    case 7U:  frame->rsp = value; break;
    case 8U:  frame->r8 = value; break;
    case 9U:  frame->r9 = value; break;
    case 10U: frame->r10 = value; break;
    case 11U: frame->r11 = value; break;
    case 12U: frame->r12 = value; break;
    case 13U: frame->r13 = value; break;
    case 14U: frame->r14 = value; break;
    case 15U: frame->r15 = value; break;
    case 16U: frame->rip = value; break;
    case 17U:
        frame->rflags = (frame->rflags & ~(uint64_t)RFLAGS_GDB_WRITABLE)
            | (value & RFLAGS_GDB_WRITABLE);
        break;
    default:
        // Segment registers define the privilege level: never changed.
        break;
    }
}

/**
 * @funcname `fp_tag`
 *
 * @description
 * Returns the x87 tag of a register st<st> from its value in the FXSAVE area:
 * valid, zero or special.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `fx` | `const uint8_t *` | [in] The FXSAVE area. |
 * | `st` | `uint32_t` | [in] The register's number on the stack. |
 *
 * @return The tag.
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `exponent` | `uint32_t` | Its exponent |
 * | `fraction_zero` | `pok_bool_t` | Whether its fraction is zero |
 * | `i` | `uint32_t` | The index of a byte of the fraction |
 * | `integer` | `pok_bool_t` | Its integer bit |
 * | `offset` | `uint32_t` | The register's offset in the area |
 * | `tag` | `uint32_t` | The tag |
 *
 * @hlr
 * - \hlrid{HLR_GDB_FP_TAG_01}: `fp_tag()` shall give the x87 tag of a register
 *   from its value in the save area: valid, zero or special.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_DBG_004}
 *
 * @llr
 * - \llrid{LLR_GDB_FP_TAG_01}: If the exponent of the register `st` (the low 15
 *   bits of the bytes 8 and 9 of the 10 bytes at `FX_ST0 + st * FX_STRIDE` of
 *   `fx`) is `0x7FFF`, `fp_tag()` shall return `FP_TAG_SPECIAL`.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDB_FP_TAG_01}
 * - \llrid{LLR_GDB_FP_TAG_02}: If the exponent of the register is 0, its
 *   fraction is 0 and its integer bit (bit 7 of byte 7) is 0, `fp_tag()` shall
 *   return `FP_TAG_ZERO`.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDB_FP_TAG_01}
 * - \llrid{LLR_GDB_FP_TAG_03}: If the exponent of the register is 0, and its
 *   fraction is not 0 or its integer bit is 1, `fp_tag()` shall return
 *   `FP_TAG_SPECIAL`.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDB_FP_TAG_01}
 * - \llrid{LLR_GDB_FP_TAG_04}: If the exponent of the register is neither 0 nor
 *   `0x7FFF` and its integer bit is 1, `fp_tag()` shall return `FP_TAG_VALID`.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDB_FP_TAG_01}
 * - \llrid{LLR_GDB_FP_TAG_05}: If the exponent of the register is neither 0 nor
 *   `0x7FFF` and its integer bit is 0, `fp_tag()` shall return
 *   `FP_TAG_SPECIAL`.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDB_FP_TAG_01}
 * - \llrid{LLR_GDB_FP_TAG_06}: `fp_tag()` shall take the fraction of the
 *   register as 0 if and only if the low 7 bits of its byte 7 and all its bytes
 *   0 to 6 are 0.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDB_FP_TAG_01}
 *
 * @design
 * 1. Set `offset` to `FX_ST0 + (st * FX_STRIDE)`. \llrref{LLR_GDB_FP_TAG_01},
 *    \llrref{LLR_GDB_FP_TAG_02}, \llrref{LLR_GDB_FP_TAG_03},
 *    \llrref{LLR_GDB_FP_TAG_04}, \llrref{LLR_GDB_FP_TAG_05}
 * 2. Set `exponent` to
 *    `(((uint32_t)fx[offset + 9U] & 0x7FU) << 8) | (uint32_t)fx[offset + 8U]`.
 *    \llrref{LLR_GDB_FP_TAG_01}, \llrref{LLR_GDB_FP_TAG_02},
 *    \llrref{LLR_GDB_FP_TAG_03}, \llrref{LLR_GDB_FP_TAG_04},
 *    \llrref{LLR_GDB_FP_TAG_05}
 * 3. Set `integer` to `((uint32_t)fx[offset + 7U] & 0x80U) != 0U`.
 *    \llrref{LLR_GDB_FP_TAG_02}, \llrref{LLR_GDB_FP_TAG_03},
 *    \llrref{LLR_GDB_FP_TAG_04}, \llrref{LLR_GDB_FP_TAG_05}
 * 4. Set `fraction_zero` to `((uint32_t)fx[offset + 7U] & 0x7FU) == 0U`.
 *    \llrref{LLR_GDB_FP_TAG_06}
 * 5. Set `i` to `0U`; then while `i` is less than `7U`, repeat (after each
 *    pass: increment `i`): \llrref{LLR_GDB_FP_TAG_06}
 *    1. If `fx[offset + i]` is not equal to `0U`:
 *       1. Set `fraction_zero` to `FALSE`.
 * 6. If `exponent` is equal to `0x7FFFU`: \llrref{LLR_GDB_FP_TAG_01}
 *    1. Set `tag` to `FP_TAG_SPECIAL`.
 * 7. Otherwise, if `exponent` is equal to `0U`: \llrref{LLR_GDB_FP_TAG_02},
 *    \llrref{LLR_GDB_FP_TAG_03}
 *    1. Set `tag` to `FP_TAG_ZERO` if (`fraction_zero` is true and `integer` is
 *       false), otherwise `FP_TAG_SPECIAL`.
 * 8. Otherwise: \llrref{LLR_GDB_FP_TAG_04}, \llrref{LLR_GDB_FP_TAG_05}
 *    1. Set `tag` to `FP_TAG_VALID` if `integer` is true, otherwise
 *       `FP_TAG_SPECIAL`.
 * 9. Return `tag`. \llrref{LLR_GDB_FP_TAG_01}, \llrref{LLR_GDB_FP_TAG_02},
 *    \llrref{LLR_GDB_FP_TAG_03}, \llrref{LLR_GDB_FP_TAG_04},
 *    \llrref{LLR_GDB_FP_TAG_05}
 *
 * @calls
 * None.
 */
static uint32_t fp_tag(const uint8_t* fx, uint32_t st)
{
    uint32_t offset = FX_ST0 + (st * FX_STRIDE);
    uint32_t exponent = (((uint32_t)fx[offset + 9U] & 0x7FU) << 8)
        | (uint32_t)fx[offset + 8U];
    pok_bool_t integer = ((uint32_t)fx[offset + 7U] & 0x80U) != 0U;
    pok_bool_t fraction_zero = ((uint32_t)fx[offset + 7U] & 0x7FU) == 0U;
    uint32_t tag;

    for (uint32_t i = 0U; i < 7U; i++)
    {
        if (fx[offset + i] != 0U) {
            fraction_zero = FALSE;
        }
    }

    if (exponent == 0x7FFFU) {
        tag = FP_TAG_SPECIAL;
    } else if (exponent == 0U) {
        tag = (fraction_zero && !integer) ? FP_TAG_ZERO : FP_TAG_SPECIAL;
    } else {
        tag = integer ? FP_TAG_VALID : FP_TAG_SPECIAL;
    }

    return tag;
}

/**
 * @funcname `fp_full_tag`
 *
 * @description
 * Returns the full x87 tag word from the abridged one in the FXSAVE area (as
 * GDB does for its native targets). Tags are indexed by physical register; st0
 * is physical register TOP.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `fx` | `const uint8_t *` | [in] The FXSAVE area. |
 *
 * @return The full tag word.
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `abridged` | `uint32_t` | The abridged tag |
 * | `full` | `uint32_t` | The full tag word |
 * | `phys` | `uint32_t` | The physical register |
 * | `tag` | `uint32_t` | Its tag |
 * | `top` | `uint32_t` | TOP of the status word |
 *
 * @hlr
 * - \hlrid{HLR_GDB_FP_FULL_TAG_01}: `fp_full_tag()` shall give the full x87 tag
 *   word from the abridged one of the save area, as the debugger expects it.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_DBG_004}
 *
 * @llr
 * - \llrid{LLR_GDB_FP_FULL_TAG_01}: For each physical register `phys` from 0 to
 *   7 whose bit is set in the abridged tag `fx[FX_FTW]`, `fp_full_tag()` shall
 *   put in the bits `2 * phys + 1` and `2 * phys` of the result the result of
 *   `fp_tag()` for `fx` and the stack register `(phys + 8 - top) % 8`, where
 *   `top` is the bits 5 to 3 of `fx[FX_FSW + 1]`.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDB_FP_FULL_TAG_01}
 * - \llrid{LLR_GDB_FP_FULL_TAG_02}: For each physical register `phys` from 0 to
 *   7 whose bit is not set in the abridged tag `fx[FX_FTW]`, `fp_full_tag()`
 *   shall put `FP_TAG_EMPTY` in the bits `2 * phys + 1` and `2 * phys` of the
 *   result, without calling `fp_tag()`.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDB_FP_FULL_TAG_01}
 *
 * @design
 * 1. Set `top` to `((uint32_t)fx[FX_FSW + 1U] >> 3) & 7U`.
 *    \llrref{LLR_GDB_FP_FULL_TAG_01}
 * 2. Set `abridged` to `fx[FX_FTW]`. \llrref{LLR_GDB_FP_FULL_TAG_01},
 *    \llrref{LLR_GDB_FP_FULL_TAG_02}
 * 3. Set `full` to `0U`. \llrref{LLR_GDB_FP_FULL_TAG_01},
 *    \llrref{LLR_GDB_FP_FULL_TAG_02}
 * 4. Set `phys` to `0U`; then while `phys` is less than `8U`, repeat (after
 *    each pass: increment `phys`): \llrref{LLR_GDB_FP_FULL_TAG_01},
 *    \llrref{LLR_GDB_FP_FULL_TAG_02}
 *    1. Set `tag` to `FP_TAG_EMPTY`. \llrref{LLR_GDB_FP_FULL_TAG_02}
 *    2. If `(abridged & (1U << phys))` is not equal to `0U`:
 *       \llrref{LLR_GDB_FP_FULL_TAG_01}
 *       1. Call `fp_tag()` with the arguments `fx` and
 *          `(phys + 8U - top) % 8U`, and store the result in `tag`.
 *    3. Set the bits `tag << (2U * phys)` in `full`.
 *       \llrref{LLR_GDB_FP_FULL_TAG_01}, \llrref{LLR_GDB_FP_FULL_TAG_02}
 * 5. Return `full`. \llrref{LLR_GDB_FP_FULL_TAG_01},
 *    \llrref{LLR_GDB_FP_FULL_TAG_02}
 *
 * @calls
 * - `fp_tag()`
 */
static uint32_t fp_full_tag(const uint8_t* fx)
{
    uint32_t top = ((uint32_t)fx[FX_FSW + 1U] >> 3) & 7U;
    uint32_t abridged = fx[FX_FTW];
    uint32_t full = 0U;

    for (uint32_t phys = 0U; phys < 8U; phys++)
    {
        uint32_t tag = FP_TAG_EMPTY;

        if ((abridged & (1U << phys)) != 0U) {
            tag = fp_tag(fx, (phys + 8U - top) % 8U);
        }
        full |= tag << (2U * phys);
    }

    return full;
}

/**
 * @funcname `fp_abridged_tag`
 *
 * @description
 * Returns the abridged tag (1 bit per physical register, set if it isn't empty)
 * of a full tag word.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `full` | `uint32_t` | [in] The full tag word. |
 *
 * @return The abridged tag.
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `abridged` | `uint32_t` | The abridged tag |
 * | `phys` | `uint32_t` | The physical register |
 * | `tag` | `uint32_t` | Its tag |
 *
 * @hlr
 * - \hlrid{HLR_GDB_FP_ABRIDGED_TAG_01}: `fp_abridged_tag()` shall give the
 *   abridged tag of a full x87 tag word: one bit per physical register, set if
 *   it is not empty.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_DBG_004}
 *
 * @llr
 * - \llrid{LLR_GDB_FP_ABRIDGED_TAG_01}: `fp_abridged_tag()` shall return the
 *   byte whose bit `phys`, for each `phys` from 0 to 7, is 1 if the bits `2 *
 *   phys + 1` and `2 * phys` of `full` are not `FP_TAG_EMPTY`, and 0 if they
 *   are.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDB_FP_ABRIDGED_TAG_01}
 *
 * @design
 * 1. Set `abridged` to `0U`. \llrref{LLR_GDB_FP_ABRIDGED_TAG_01}
 * 2. Set `phys` to `0U`; then while `phys` is less than `8U`, repeat (after
 *    each pass: increment `phys`): \llrref{LLR_GDB_FP_ABRIDGED_TAG_01}
 *    1. Set `tag` to `full >> (2U * phys)`.
 *    2. If `(tag & 3U)` is not equal to `FP_TAG_EMPTY`:
 *       1. Set the bits `1U << phys` in `abridged`.
 * 3. Return `(uint8_t)abridged`. \llrref{LLR_GDB_FP_ABRIDGED_TAG_01}
 *
 * @calls
 * None.
 */
static uint8_t fp_abridged_tag(uint32_t full)
{
    uint32_t abridged = 0U;

    for (uint32_t phys = 0U; phys < 8U; phys++)
    {
        uint32_t tag = full >> (2U * phys);

        if ((tag & 3U) != FP_TAG_EMPTY) {
            abridged |= 1U << phys;
        }
    }

    return (uint8_t)abridged;
}

/**
 * @funcname `copy_bytes`
 *
 * @description
 * Copies bytes between offsets of two buffers.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `to` | `uint8_t *` | [out] The destination. |
 * | `to_offset` | `size_t` | [in] The offset in it. |
 * | `from` | `const uint8_t *` | [in] The source. |
 * | `from_offset` | `size_t` | [in] The offset in it. |
 * | `size` | `size_t` | [in] The number of bytes. |
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `i` | `size_t` | The index of the byte |
 *
 * @hlr
 * - \hlrid{HLR_GDB_COPY_BYTES_01}: `copy_bytes()` shall copy bytes between
 *   offsets of two buffers.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_DBG_004}
 *
 * @llr
 * - \llrid{LLR_GDB_COPY_BYTES_01}: `copy_bytes()` shall copy `size` bytes from
 *   `from` at the offset `from_offset` to `to` at the offset `to_offset`, from
 *   the first byte on.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDB_COPY_BYTES_01}
 *
 * @design
 * 1. Set `i` to `0U`; then while `i` is less than `size`, repeat (after each
 *    pass: increment `i`): \llrref{LLR_GDB_COPY_BYTES_01}
 *    1. Set `to[to_offset + i]` to `from[from_offset + i]`.
 *
 * @calls
 * None.
 */
static void copy_bytes(uint8_t* to, size_t to_offset, const uint8_t* from,
    size_t from_offset, size_t size)
{
    for (size_t i = 0U; i < size; i++)
    {
        to[to_offset + i] = from[from_offset + i];
    }
}

/**
 * @funcname `fp_register_get`
 *
 * @description
 * Reads a floating point register (st0 and above) from an FP store.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `fp` | `const jet_fp_store_t *` | [in] The FP store (FXSAVE area). |
 * | `regno` | `uint32_t` | [in] GDB's number of the register. |
 * | `value` | `uint8_t *` | [out] Where the value is stored. |
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `fx` | `const uint8_t *` | The FXSAVE area |
 * | `v` | `uint64_t` | The value of a control register |
 *
 * @hlr
 * - \hlrid{HLR_GDB_X86_64_FP_REGISTER_GET_01}: `fp_register_get()` shall give a
 *   floating point register, from ST0 on, from a store of the floating point
 *   state.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_DBG_004}
 *
 * @llr
 * - \llrid{LLR_GDB_X86_64_FP_REGISTER_GET_01}: If `regno` is less than
 *   `REG_FCTRL`, `fp_register_get()` shall call `copy_bytes()` once to copy
 *   `ST_SIZE` bytes from `data` of `fp` at the offset `FX_ST0 + (regno -
 *   REG_ST0) * FX_STRIDE` to `value`.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDB_X86_64_FP_REGISTER_GET_01}
 * - \llrid{LLR_GDB_X86_64_FP_REGISTER_GET_02}: If `regno` is not less than
 *   `REG_XMM0` and is less than `REG_MXCSR`, `fp_register_get()` shall call
 *   `copy_bytes()` once to copy `FX_STRIDE` bytes from `data` of `fp` at the
 *   offset `FX_XMM0 + (regno - REG_XMM0) * FX_STRIDE` to `value`.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDB_X86_64_FP_REGISTER_GET_01}
 * - \llrid{LLR_GDB_X86_64_FP_REGISTER_GET_03}: If `regno` is `REG_FCTRL`,
 *   `REG_FSTAT`, `REG_FISEG`, `REG_FIOFF`, `REG_FOSEG` or `REG_FOOFF`,
 *   `fp_register_get()` shall store at `value` with `put_le()`, as 4 bytes, the
 *   result of `get_le()` for the 2 bytes at `FX_FCW`, the 2 bytes at `FX_FSW`,
 *   the 4 bytes at `FX_FIP + 4`, the 4 bytes at `FX_FIP`, the 4 bytes at
 *   `FX_FDP + 4` or the 4 bytes at `FX_FDP` of `data` of `fp`, in this order.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDB_X86_64_FP_REGISTER_GET_01}
 * - \llrid{LLR_GDB_X86_64_FP_REGISTER_GET_04}: If `regno` is `REG_FTAG`,
 *   `fp_register_get()` shall store at `value` with `put_le()`, as 4 bytes, the
 *   result of `fp_full_tag()` for `data` of `fp`.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDB_X86_64_FP_REGISTER_GET_01}
 * - \llrid{LLR_GDB_X86_64_FP_REGISTER_GET_05}: If `regno` is `REG_FOP`,
 *   `fp_register_get()` shall store at `value` with `put_le()`, as 4 bytes, the
 *   low 11 bits of the result of `get_le()` for the 2 bytes at `FX_FOP` of
 *   `data` of `fp`.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDB_X86_64_FP_REGISTER_GET_01}
 * - \llrid{LLR_GDB_X86_64_FP_REGISTER_GET_06}: If `regno` is not less than
 *   `REG_FCTRL`, is not from `REG_XMM0` to below `REG_MXCSR`, and is none of
 *   `REG_FCTRL`, `REG_FSTAT`, `REG_FTAG`, `REG_FISEG`, `REG_FIOFF`,
 *   `REG_FOSEG`, `REG_FOOFF` and `REG_FOP`, `fp_register_get()` shall store at
 *   `value` with `put_le()`, as 4 bytes, the result of `get_le()` for the 4
 *   bytes at `FX_MXCSR` of `data` of `fp`.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDB_X86_64_FP_REGISTER_GET_01}
 *
 * @design
 * 1. Set `fx` to `fp->data`. \llrref{LLR_GDB_X86_64_FP_REGISTER_GET_01},
 *    \llrref{LLR_GDB_X86_64_FP_REGISTER_GET_02},
 *    \llrref{LLR_GDB_X86_64_FP_REGISTER_GET_03},
 *    \llrref{LLR_GDB_X86_64_FP_REGISTER_GET_04},
 *    \llrref{LLR_GDB_X86_64_FP_REGISTER_GET_05},
 *    \llrref{LLR_GDB_X86_64_FP_REGISTER_GET_06}
 * 2. If `regno` is less than `REG_FCTRL`:
 *    \llrref{LLR_GDB_X86_64_FP_REGISTER_GET_01}
 *    1. Call `copy_bytes()` with the arguments `value`, `0U`, `fx`,
 *       `FX_ST0 + ((regno - REG_ST0) * FX_STRIDE)` and `ST_SIZE`.
 * 3. Otherwise, if `regno` is greater than or equal to `REG_XMM0` and `regno`
 *    is less than `REG_MXCSR`: \llrref{LLR_GDB_X86_64_FP_REGISTER_GET_02}
 *    1. Call `copy_bytes()` with the arguments `value`, `0U`, `fx`,
 *       `FX_XMM0 + ((regno - REG_XMM0) * FX_STRIDE)` and `FX_STRIDE`.
 * 4. Otherwise: \llrref{LLR_GDB_X86_64_FP_REGISTER_GET_03},
 *    \llrref{LLR_GDB_X86_64_FP_REGISTER_GET_04},
 *    \llrref{LLR_GDB_X86_64_FP_REGISTER_GET_05},
 *    \llrref{LLR_GDB_X86_64_FP_REGISTER_GET_06}
 *    1. Depending on `regno`:
 *       1. Case `REG_FCTRL`:
 *          1. Call `get_le()` with the arguments `fx`, `FX_FCW` and `2U`, and
 *             store the result in `v`.
 *          2. Leave the switch.
 *       2. Case `REG_FSTAT`:
 *          1. Call `get_le()` with the arguments `fx`, `FX_FSW` and `2U`, and
 *             store the result in `v`.
 *          2. Leave the switch.
 *       3. Case `REG_FTAG`:
 *          1. Call `fp_full_tag()` with the argument `fx`, and store the result
 *             in `v`.
 *          2. Leave the switch.
 *       4. Case `REG_FISEG`:
 *          1. Call `get_le()` with the arguments `fx`, `FX_FIP + 4U` and `4U`,
 *             and store the result in `v`.
 *          2. Leave the switch.
 *       5. Case `REG_FIOFF`:
 *          1. Call `get_le()` with the arguments `fx`, `FX_FIP` and `4U`, and
 *             store the result in `v`.
 *          2. Leave the switch.
 *       6. Case `REG_FOSEG`:
 *          1. Call `get_le()` with the arguments `fx`, `FX_FDP + 4U` and `4U`,
 *             and store the result in `v`.
 *          2. Leave the switch.
 *       7. Case `REG_FOOFF`:
 *          1. Call `get_le()` with the arguments `fx`, `FX_FDP` and `4U`, and
 *             store the result in `v`.
 *          2. Leave the switch.
 *       8. Case `REG_FOP`:
 *          1. Set `v` to `get_le(fx, FX_FOP, 2U) & 0x7FFU`.
 *          2. Leave the switch.
 *       9. Otherwise (default):
 *          1. Call `get_le()` with the arguments `fx`, `FX_MXCSR` and `4U`, and
 *             store the result in `v`.
 *          2. Leave the switch.
 *    2. Call `put_le()` with the arguments `v`, `4U` and `value`.
 *
 * @calls
 * - `copy_bytes()`
 * - `get_le()`
 * - `fp_full_tag()`
 * - `put_le()`
 */
static void fp_register_get(const jet_fp_store_t* fp, uint32_t regno,
    uint8_t* value)
{
    const uint8_t* fx = fp->data;

    if (regno < REG_FCTRL)
    {
        copy_bytes(value, 0U, fx, FX_ST0 + ((regno - REG_ST0) * FX_STRIDE),
            ST_SIZE);
    }
    else if ((regno >= REG_XMM0) && (regno < REG_MXCSR))
    {
        copy_bytes(value, 0U, fx, FX_XMM0 + ((regno - REG_XMM0) * FX_STRIDE),
            FX_STRIDE);
    }
    else
    {
        uint64_t v;

        switch (regno)
        {
        case REG_FCTRL: v = get_le(fx, FX_FCW, 2U); break;
        case REG_FSTAT: v = get_le(fx, FX_FSW, 2U); break;
        case REG_FTAG:  v = fp_full_tag(fx); break;
        case REG_FISEG: v = get_le(fx, FX_FIP + 4U, 4U); break;
        case REG_FIOFF: v = get_le(fx, FX_FIP, 4U); break;
        case REG_FOSEG: v = get_le(fx, FX_FDP + 4U, 4U); break;
        case REG_FOOFF: v = get_le(fx, FX_FDP, 4U); break;
        case REG_FOP:   v = get_le(fx, FX_FOP, 2U) & 0x7FFU; break;
        default:        v = get_le(fx, FX_MXCSR, 4U); break;
        }

        put_le(v, 4U, value);
    }
}

/**
 * @funcname `fp_register_set`
 *
 * @description
 * Writes a floating point register (st0 and above) into an FP store; reserved
 * MXCSR bits are cleared (fxrstor would fault).
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `fp` | `jet_fp_store_t *` | [in] The FP store (FXSAVE area). |
 * | `regno` | `uint32_t` | [in] GDB's number of the register. |
 * | `value` | `const uint8_t *` | [in] The value. |
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `fx` | `uint8_t *` | The FXSAVE area |
 * | `mask` | `uint64_t` | The MXCSR bits that may be set |
 * | `v` | `uint64_t` | The value of a control register |
 *
 * @hlr
 * - \hlrid{HLR_GDB_X86_64_FP_REGISTER_SET_01}: `fp_register_set()` shall write
 *   a floating point register, from ST0 on, into a store of the floating point
 *   state, clearing the reserved bits of MXCSR.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_DBG_004}
 *
 * @llr
 * - \llrid{LLR_GDB_X86_64_FP_REGISTER_SET_01}: If `regno` is less than
 *   `REG_FCTRL`, `fp_register_set()` shall call `copy_bytes()` once to copy
 *   `ST_SIZE` bytes from `value` to `data` of `fp` at the offset `FX_ST0 +
 *   (regno - REG_ST0) * FX_STRIDE`.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDB_X86_64_FP_REGISTER_SET_01}
 * - \llrid{LLR_GDB_X86_64_FP_REGISTER_SET_02}: If `regno` is not less than
 *   `REG_XMM0` and is less than `REG_MXCSR`, `fp_register_set()` shall call
 *   `copy_bytes()` once to copy `FX_STRIDE` bytes from `value` to `data` of
 *   `fp` at the offset `FX_XMM0 + (regno - REG_XMM0) * FX_STRIDE`.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDB_X86_64_FP_REGISTER_SET_01}
 * - \llrid{LLR_GDB_X86_64_FP_REGISTER_SET_03}: If `regno` is `REG_FCTRL`,
 *   `REG_FSTAT`, `REG_FISEG`, `REG_FIOFF`, `REG_FOSEG` or `REG_FOOFF`,
 *   `fp_register_set()` shall store with `put_le()` the value of the 4 bytes at
 *   `value` (`get_le()`) as 2 bytes at `FX_FCW`, 2 bytes at `FX_FSW`, 4 bytes
 *   at `FX_FIP + 4`, 4 bytes at `FX_FIP`, 4 bytes at `FX_FDP + 4` or 4 bytes at
 *   `FX_FDP` of `data` of `fp`, in this order.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDB_X86_64_FP_REGISTER_SET_01}
 * - \llrid{LLR_GDB_X86_64_FP_REGISTER_SET_04}: If `regno` is `REG_FTAG`,
 *   `fp_register_set()` shall set the byte at `FX_FTW` of `data` of `fp` to the
 *   result of `fp_abridged_tag()` for the low 16 bits of the value of the 4
 *   bytes at `value`.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDB_X86_64_FP_REGISTER_SET_01}
 * - \llrid{LLR_GDB_X86_64_FP_REGISTER_SET_05}: If `regno` is `REG_FOP`,
 *   `fp_register_set()` shall store with `put_le()` the low 11 bits of the
 *   value of the 4 bytes at `value` as 2 bytes at `FX_FOP` of `data` of `fp`.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDB_X86_64_FP_REGISTER_SET_01}
 * - \llrid{LLR_GDB_X86_64_FP_REGISTER_SET_06}: If `regno` is not less than
 *   `REG_FCTRL`, is not from `REG_XMM0` to below `REG_MXCSR`, and is none of
 *   `REG_FCTRL`, `REG_FSTAT`, `REG_FTAG`, `REG_FISEG`, `REG_FIOFF`,
 *   `REG_FOSEG`, `REG_FOOFF` and `REG_FOP`, and the 4 bytes at `FX_MXCSR_MASK`
 *   of `data` of `fp` are not 0, `fp_register_set()` shall store with
 *   `put_le()` the value of the 4 bytes at `value`, ANDed with the value of
 *   those 4 bytes, as 4 bytes at `FX_MXCSR`.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDB_X86_64_FP_REGISTER_SET_01}
 * - \llrid{LLR_GDB_X86_64_FP_REGISTER_SET_07}: If `regno` is not less than
 *   `REG_FCTRL`, is not from `REG_XMM0` to below `REG_MXCSR`, and is none of
 *   `REG_FCTRL`, `REG_FSTAT`, `REG_FTAG`, `REG_FISEG`, `REG_FIOFF`,
 *   `REG_FOSEG`, `REG_FOOFF` and `REG_FOP`, and the 4 bytes at `FX_MXCSR_MASK`
 *   of `data` of `fp` are 0, `fp_register_set()` shall store with `put_le()`
 *   the value of the 4 bytes at `value`, ANDed with `MXCSR_MASK_DEFAULT`, as 4
 *   bytes at `FX_MXCSR`.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDB_X86_64_FP_REGISTER_SET_01}
 *
 * @design
 * 1. Set `fx` to `fp->data`. \llrref{LLR_GDB_X86_64_FP_REGISTER_SET_01},
 *    \llrref{LLR_GDB_X86_64_FP_REGISTER_SET_02},
 *    \llrref{LLR_GDB_X86_64_FP_REGISTER_SET_03},
 *    \llrref{LLR_GDB_X86_64_FP_REGISTER_SET_04},
 *    \llrref{LLR_GDB_X86_64_FP_REGISTER_SET_05},
 *    \llrref{LLR_GDB_X86_64_FP_REGISTER_SET_06},
 *    \llrref{LLR_GDB_X86_64_FP_REGISTER_SET_07}
 * 2. If `regno` is less than `REG_FCTRL`:
 *    \llrref{LLR_GDB_X86_64_FP_REGISTER_SET_01}
 *    1. Call `copy_bytes()` with the arguments `fx`,
 *       `FX_ST0 + ((regno - REG_ST0) * FX_STRIDE)`, `value`, `0U` and
 *       `ST_SIZE`.
 * 3. Otherwise, if `regno` is greater than or equal to `REG_XMM0` and `regno`
 *    is less than `REG_MXCSR`: \llrref{LLR_GDB_X86_64_FP_REGISTER_SET_02}
 *    1. Call `copy_bytes()` with the arguments `fx`,
 *       `FX_XMM0 + ((regno - REG_XMM0) * FX_STRIDE)`, `value`, `0U` and
 *       `FX_STRIDE`.
 * 4. Otherwise: \llrref{LLR_GDB_X86_64_FP_REGISTER_SET_03},
 *    \llrref{LLR_GDB_X86_64_FP_REGISTER_SET_04},
 *    \llrref{LLR_GDB_X86_64_FP_REGISTER_SET_05},
 *    \llrref{LLR_GDB_X86_64_FP_REGISTER_SET_06},
 *    \llrref{LLR_GDB_X86_64_FP_REGISTER_SET_07}
 *    1. Call `get_le()` with the arguments `value`, `0U` and `4U`, and store
 *       the result in `v`.
 *    2. Depending on `regno`:
 *       1. Case `REG_FCTRL`:
 *          1. Call `put_le()` with the arguments `v`, `2U` and the address of
 *             `fx[FX_FCW]`.
 *          2. Leave the switch.
 *       2. Case `REG_FSTAT`:
 *          1. Call `put_le()` with the arguments `v`, `2U` and the address of
 *             `fx[FX_FSW]`.
 *          2. Leave the switch.
 *       3. Case `REG_FTAG`:
 *          1. Call `fp_abridged_tag()` with the argument
 *             `(uint32_t)(v & 0xFFFFU)`, and store the result in `fx[FX_FTW]`.
 *          2. Leave the switch.
 *       4. Case `REG_FISEG`:
 *          1. Call `put_le()` with the arguments `v`, `4U` and the address of
 *             `fx[FX_FIP + 4U]`.
 *          2. Leave the switch.
 *       5. Case `REG_FIOFF`:
 *          1. Call `put_le()` with the arguments `v`, `4U` and the address of
 *             `fx[FX_FIP]`.
 *          2. Leave the switch.
 *       6. Case `REG_FOSEG`:
 *          1. Call `put_le()` with the arguments `v`, `4U` and the address of
 *             `fx[FX_FDP + 4U]`.
 *          2. Leave the switch.
 *       7. Case `REG_FOOFF`:
 *          1. Call `put_le()` with the arguments `v`, `4U` and the address of
 *             `fx[FX_FDP]`.
 *          2. Leave the switch.
 *       8. Case `REG_FOP`:
 *          1. Call `put_le()` with the arguments `v & 0x7FFU`, `2U` and the
 *             address of `fx[FX_FOP]`.
 *          2. Leave the switch.
 *       9. Otherwise (default):
 *          1. Call `get_le()` with the arguments `fx`, `FX_MXCSR_MASK` and
 *             `4U`, and store the result in `mask`.
 *          2. If `mask` is equal to `0U`:
 *             1. Set `mask` to `MXCSR_MASK_DEFAULT`.
 *          3. Call `put_le()` with the arguments `v & mask`, `4U` and the
 *             address of `fx[FX_MXCSR]`.
 *          4. Leave the switch.
 *
 * @calls
 * - `copy_bytes()`
 * - `get_le()`
 * - `put_le()`
 * - `fp_abridged_tag()`
 */
static void fp_register_set(jet_fp_store_t* fp, uint32_t regno,
    const uint8_t* value)
{
    uint8_t* fx = fp->data;

    if (regno < REG_FCTRL)
    {
        copy_bytes(fx, FX_ST0 + ((regno - REG_ST0) * FX_STRIDE), value, 0U,
            ST_SIZE);
    }
    else if ((regno >= REG_XMM0) && (regno < REG_MXCSR))
    {
        copy_bytes(fx, FX_XMM0 + ((regno - REG_XMM0) * FX_STRIDE), value, 0U,
            FX_STRIDE);
    }
    else
    {
        uint64_t v = get_le(value, 0U, 4U);
        uint64_t mask;

        switch (regno)
        {
        case REG_FCTRL: put_le(v, 2U, &fx[FX_FCW]); break;
        case REG_FSTAT: put_le(v, 2U, &fx[FX_FSW]); break;
        case REG_FTAG:
            fx[FX_FTW] = fp_abridged_tag((uint32_t)(v & 0xFFFFU));
            break;
        case REG_FISEG: put_le(v, 4U, &fx[FX_FIP + 4U]); break;
        case REG_FIOFF: put_le(v, 4U, &fx[FX_FIP]); break;
        case REG_FOSEG: put_le(v, 4U, &fx[FX_FDP + 4U]); break;
        case REG_FOOFF: put_le(v, 4U, &fx[FX_FDP]); break;
        case REG_FOP:   put_le(v & 0x7FFU, 2U, &fx[FX_FOP]); break;
        default:
            // Reserved MXCSR bits would make fxrstor fault.
            mask = get_le(fx, FX_MXCSR_MASK, 4U);
            if (mask == 0U) {
                mask = MXCSR_MASK_DEFAULT;
            }
            put_le(v & mask, 4U, &fx[FX_MXCSR]);
            break;
        }
    }
}

/**
 * @funcname `ja_gdb_register_get`
 *
 * @description
 * Reads a GDB register of a thread: from its interrupt frame (up to ss) or its
 * FP store; ds, es, fs and gs aren't saved.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `ctx` | `const interrupt_frame *` | [in] The thread's registers, or NULL. |
 * | `fp` | `const jet_fp_store_t *` | [in] The thread's FP store, or NULL. |
 * | `regno` | `uint32_t` | [in] GDB's number of the register. |
 * | `value` | `uint8_t *` | [out] Where the value is stored. |
 *
 * @return TRUE if the register is available.
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `available` | `pok_bool_t` | Whether it is available |
 * | `size` | `size_t` | Its size |
 *
 * @hlr
 * - \hlrid{HLR_GDB_X86_64_JA_GDB_REGISTER_GET_01}: `ja_gdb_register_get()`
 *   shall give a register of a thread to the debugger: from its interrupt frame
 *   up to SS, or from its floating point store, and tell whether it is
 *   available; DS, ES, FS and GS are not saved.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_DBG_004}
 *
 * @llr
 * - \llrid{LLR_GDB_X86_64_JA_GDB_REGISTER_GET_01}: If `regno` is not greater
 *   than `REG_SS` and `ctx` is not NULL, `ja_gdb_register_get()` shall store at
 *   `value` with `put_le()` the result of `frame_register_get()` for `ctx` and
 *   `regno`, in the number of bytes that `ja_gdb_register_size()` gives for
 *   `regno`, and return `TRUE`.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDB_X86_64_JA_GDB_REGISTER_GET_01}
 * - \llrid{LLR_GDB_X86_64_JA_GDB_REGISTER_GET_02}: If `regno` is not greater
 *   than `REG_SS` and `ctx` is NULL, `ja_gdb_register_get()` shall return
 *   `FALSE` without writing at `value`.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDB_X86_64_JA_GDB_REGISTER_GET_01}
 * - \llrid{LLR_GDB_X86_64_JA_GDB_REGISTER_GET_03}: If `regno` is greater than
 *   `REG_SS` and less than `REG_ST0`, `ja_gdb_register_get()` shall return
 *   `FALSE` without writing at `value`.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDB_X86_64_JA_GDB_REGISTER_GET_01}
 * - \llrid{LLR_GDB_X86_64_JA_GDB_REGISTER_GET_04}: If `regno` is not less than
 *   `REG_ST0`, `fp` is not NULL and the size that `ja_gdb_register_size()`
 *   gives for `regno` is not 0, `ja_gdb_register_get()` shall call
 *   `fp_register_get()` once with `fp`, `regno` and `value`, and return `TRUE`.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDB_X86_64_JA_GDB_REGISTER_GET_01}
 * - \llrid{LLR_GDB_X86_64_JA_GDB_REGISTER_GET_05}: If `regno` is not less than
 *   `REG_ST0`, and `fp` is NULL or the size that `ja_gdb_register_size()` gives
 *   for `regno` is 0, `ja_gdb_register_get()` shall return `FALSE` without
 *   calling `fp_register_get()`.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDB_X86_64_JA_GDB_REGISTER_GET_01}
 *
 * @design
 * 1. Set `available` to `FALSE`.
 *    \llrref{LLR_GDB_X86_64_JA_GDB_REGISTER_GET_02},
 *    \llrref{LLR_GDB_X86_64_JA_GDB_REGISTER_GET_03},
 *    \llrref{LLR_GDB_X86_64_JA_GDB_REGISTER_GET_05}
 * 2. Call `ja_gdb_register_size()` with the argument `regno`, and store the
 *    result in `size`. \llrref{LLR_GDB_X86_64_JA_GDB_REGISTER_GET_01},
 *    \llrref{LLR_GDB_X86_64_JA_GDB_REGISTER_GET_04},
 *    \llrref{LLR_GDB_X86_64_JA_GDB_REGISTER_GET_05}
 * 3. If `regno` is less than or equal to `REG_SS`:
 *    \llrref{LLR_GDB_X86_64_JA_GDB_REGISTER_GET_01},
 *    \llrref{LLR_GDB_X86_64_JA_GDB_REGISTER_GET_02}
 *    1. If `ctx` is not NULL:
 *       1. Call `put_le()` with the arguments `frame_register_get(ctx, regno)`,
 *          `size` and `value`. \llrref{LLR_GDB_X86_64_JA_GDB_REGISTER_GET_01}
 *       2. Set `available` to `TRUE`.
 *          \llrref{LLR_GDB_X86_64_JA_GDB_REGISTER_GET_01}
 * 4. Otherwise, if `regno` is less than `REG_ST0`:
 *    \llrref{LLR_GDB_X86_64_JA_GDB_REGISTER_GET_03}
 * 5. Otherwise, if `fp` is not NULL and `size` is not equal to `0U`:
 *    \llrref{LLR_GDB_X86_64_JA_GDB_REGISTER_GET_04},
 *    \llrref{LLR_GDB_X86_64_JA_GDB_REGISTER_GET_05}
 *    1. Call `fp_register_get()` with the arguments `fp`, `regno` and `value`.
 *       \llrref{LLR_GDB_X86_64_JA_GDB_REGISTER_GET_04}
 *    2. Set `available` to `TRUE`.
 *       \llrref{LLR_GDB_X86_64_JA_GDB_REGISTER_GET_04}
 * 6. Return `available`. \llrref{LLR_GDB_X86_64_JA_GDB_REGISTER_GET_01},
 *    \llrref{LLR_GDB_X86_64_JA_GDB_REGISTER_GET_02},
 *    \llrref{LLR_GDB_X86_64_JA_GDB_REGISTER_GET_03},
 *    \llrref{LLR_GDB_X86_64_JA_GDB_REGISTER_GET_04},
 *    \llrref{LLR_GDB_X86_64_JA_GDB_REGISTER_GET_05}
 *
 * @calls
 * - `ja_gdb_register_size()`
 * - `put_le()`
 * - `frame_register_get()`
 * - `fp_register_get()`
 */
pok_bool_t ja_gdb_register_get(const interrupt_frame* ctx,
    const jet_fp_store_t* fp, uint32_t regno, uint8_t* value)
{
    pok_bool_t available = FALSE;
    size_t size = ja_gdb_register_size(regno);

    if (regno <= REG_SS)
    {
        if (ctx != NULL)
        {
            put_le(frame_register_get(ctx, regno), size, value);
            available = TRUE;
        }
    }
    else if (regno < REG_ST0)
    {
        // ds, es, fs and gs aren't saved on entry to the kernel.
    }
    else if ((fp != NULL) && (size != 0U))
    {
        fp_register_get(fp, regno, value);
        available = TRUE;
    }
    else
    {
        // No floating point registers, or no such register.
    }

    return available;
}

/**
 * @funcname `ja_gdb_register_set`
 *
 * @description
 * Writes a GDB register of a thread: into its interrupt frame (up to eflags) or
 * its FP store; segment registers are never changed.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `ctx` | `interrupt_frame *` | [out] The thread's registers, or NULL. |
 * | `fp` | `jet_fp_store_t *` | [in] The thread's FP store, or NULL. |
 * | `regno` | `uint32_t` | [in] GDB's number of the register. |
 * | `value` | `const uint8_t *` | [in] The value. |
 *
 * @return TRUE if the register was written.
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `size` | `size_t` | Its size |
 * | `written` | `pok_bool_t` | Whether it was written |
 *
 * @hlr
 * - \hlrid{HLR_GDB_X86_64_JA_GDB_REGISTER_SET_01}: `ja_gdb_register_set()`
 *   shall write a register of a thread from the debugger: into its interrupt
 *   frame up to EFLAGS, or into its floating point store, never a segment
 *   register, and tell whether it did.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_DBG_004}
 *
 * @llr
 * - \llrid{LLR_GDB_X86_64_JA_GDB_REGISTER_SET_01}: If `regno` is not greater
 *   than `REG_EFLAGS` and `ctx` is not NULL, `ja_gdb_register_set()` shall call
 *   `frame_register_set()` once with `ctx`, `regno` and the value of the bytes
 *   at `value` (`get_le()`, in the number of bytes that
 *   `ja_gdb_register_size()` gives for `regno`), and return `TRUE`.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDB_X86_64_JA_GDB_REGISTER_SET_01}
 * - \llrid{LLR_GDB_X86_64_JA_GDB_REGISTER_SET_02}: If `regno` is not greater
 *   than `REG_EFLAGS` and `ctx` is NULL, `ja_gdb_register_set()` shall return
 *   `FALSE` without calling `frame_register_set()`.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDB_X86_64_JA_GDB_REGISTER_SET_01}
 * - \llrid{LLR_GDB_X86_64_JA_GDB_REGISTER_SET_03}: If `regno` is greater than
 *   `REG_EFLAGS` and less than `REG_ST0`, `ja_gdb_register_set()` shall return
 *   `FALSE` without writing a register.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDB_X86_64_JA_GDB_REGISTER_SET_01}
 * - \llrid{LLR_GDB_X86_64_JA_GDB_REGISTER_SET_04}: If `regno` is not less than
 *   `REG_ST0`, `fp` is not NULL and the size that `ja_gdb_register_size()`
 *   gives for `regno` is not 0, `ja_gdb_register_set()` shall call
 *   `fp_register_set()` once with `fp`, `regno` and `value`, and return `TRUE`.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDB_X86_64_JA_GDB_REGISTER_SET_01}
 * - \llrid{LLR_GDB_X86_64_JA_GDB_REGISTER_SET_05}: If `regno` is not less than
 *   `REG_ST0`, and `fp` is NULL or the size that `ja_gdb_register_size()` gives
 *   for `regno` is 0, `ja_gdb_register_set()` shall return `FALSE` without
 *   calling `fp_register_set()`.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDB_X86_64_JA_GDB_REGISTER_SET_01}
 *
 * @design
 * 1. Set `written` to `FALSE`. \llrref{LLR_GDB_X86_64_JA_GDB_REGISTER_SET_02},
 *    \llrref{LLR_GDB_X86_64_JA_GDB_REGISTER_SET_03},
 *    \llrref{LLR_GDB_X86_64_JA_GDB_REGISTER_SET_05}
 * 2. Call `ja_gdb_register_size()` with the argument `regno`, and store the
 *    result in `size`. \llrref{LLR_GDB_X86_64_JA_GDB_REGISTER_SET_01},
 *    \llrref{LLR_GDB_X86_64_JA_GDB_REGISTER_SET_04},
 *    \llrref{LLR_GDB_X86_64_JA_GDB_REGISTER_SET_05}
 * 3. If `regno` is less than or equal to `REG_EFLAGS`:
 *    \llrref{LLR_GDB_X86_64_JA_GDB_REGISTER_SET_01},
 *    \llrref{LLR_GDB_X86_64_JA_GDB_REGISTER_SET_02}
 *    1. If `ctx` is not NULL:
 *       1. Call `frame_register_set()` with the arguments `ctx`, `regno` and
 *          `get_le(value, 0U, size)`.
 *          \llrref{LLR_GDB_X86_64_JA_GDB_REGISTER_SET_01}
 *       2. Set `written` to `TRUE`.
 *          \llrref{LLR_GDB_X86_64_JA_GDB_REGISTER_SET_01}
 * 4. Otherwise, if `regno` is less than `REG_ST0`:
 *    \llrref{LLR_GDB_X86_64_JA_GDB_REGISTER_SET_03}
 * 5. Otherwise, if `fp` is not NULL and `size` is not equal to `0U`:
 *    \llrref{LLR_GDB_X86_64_JA_GDB_REGISTER_SET_04},
 *    \llrref{LLR_GDB_X86_64_JA_GDB_REGISTER_SET_05}
 *    1. Call `fp_register_set()` with the arguments `fp`, `regno` and `value`.
 *       \llrref{LLR_GDB_X86_64_JA_GDB_REGISTER_SET_04}
 *    2. Set `written` to `TRUE`. \llrref{LLR_GDB_X86_64_JA_GDB_REGISTER_SET_04}
 * 6. Return `written`. \llrref{LLR_GDB_X86_64_JA_GDB_REGISTER_SET_01},
 *    \llrref{LLR_GDB_X86_64_JA_GDB_REGISTER_SET_02},
 *    \llrref{LLR_GDB_X86_64_JA_GDB_REGISTER_SET_03},
 *    \llrref{LLR_GDB_X86_64_JA_GDB_REGISTER_SET_04},
 *    \llrref{LLR_GDB_X86_64_JA_GDB_REGISTER_SET_05}
 *
 * @calls
 * - `ja_gdb_register_size()`
 * - `frame_register_set()`
 * - `get_le()`
 * - `fp_register_set()`
 */
pok_bool_t ja_gdb_register_set(interrupt_frame* ctx,
    jet_fp_store_t* fp, uint32_t regno, const uint8_t* value)
{
    pok_bool_t written = FALSE;
    size_t size = ja_gdb_register_size(regno);

    if (regno <= REG_EFLAGS)
    {
        if (ctx != NULL)
        {
            frame_register_set(ctx, regno, get_le(value, 0U, size));
            written = TRUE;
        }
    }
    else if (regno < REG_ST0)
    {
        // Segment registers define the privilege level: never changed.
    }
    else if ((fp != NULL) && (size != 0U))
    {
        fp_register_set(fp, regno, value);
        written = TRUE;
    }
    else
    {
        // No floating point registers, or no such register.
    }

    return written;
}

/**
 * @funcname `ja_gdb_pc_get`
 *
 * @description
 * Returns the PC (rip) of a context.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `ctx` | `const interrupt_frame *` | [in] The context. |
 *
 * @return The PC.
 *
 * @globals
 * None.
 *
 * @locals
 * None.
 *
 * @hlr
 * - \hlrid{HLR_GDB_X86_64_JA_GDB_PC_GET_01}: `ja_gdb_pc_get()` shall give the
 *   program counter of a context.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_DBG_006}
 *
 * @llr
 * - \llrid{LLR_GDB_X86_64_JA_GDB_PC_GET_01}: `ja_gdb_pc_get()` shall return
 *   `rip` of `ctx`.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDB_X86_64_JA_GDB_PC_GET_01}
 *
 * @design
 * 1. Return `ctx->rip`. \llrref{LLR_GDB_X86_64_JA_GDB_PC_GET_01}
 *
 * @calls
 * None.
 */
uintptr_t ja_gdb_pc_get(const interrupt_frame* ctx)
{
    return ctx->rip;
}

/**
 * @funcname `ja_gdb_pc_set`
 *
 * @description
 * Sets the PC (rip) of a context.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `ctx` | `interrupt_frame *` | [out] The context. |
 * | `pc` | `uintptr_t` | [in] The PC. |
 *
 * @globals
 * None.
 *
 * @locals
 * None.
 *
 * @hlr
 * - \hlrid{HLR_GDB_X86_64_JA_GDB_PC_SET_01}: `ja_gdb_pc_set()` shall set the
 *   program counter of a context.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_DBG_006}
 *
 * @llr
 * - \llrid{LLR_GDB_X86_64_JA_GDB_PC_SET_01}: `ja_gdb_pc_set()` shall set `rip`
 *   of `ctx` to `pc`.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDB_X86_64_JA_GDB_PC_SET_01}
 *
 * @design
 * 1. Set `ctx->rip` to `pc`. \llrref{LLR_GDB_X86_64_JA_GDB_PC_SET_01}
 *
 * @calls
 * None.
 */
void ja_gdb_pc_set(interrupt_frame* ctx, uintptr_t pc)
{
    ctx->rip = pc;
}

/*
 * Whether interrupts have been disabled for a single step of kernel code
 * (they are enabled again when the step is done).
 */
static pok_bool_t step_enable_interrupts = FALSE;

/**
 * @funcname `insn_changes_if`
 *
 * @description
 * Tells whether the instruction at the PC of a context may change the interrupt
 * flag itself: cli, sti, popf or iret, maybe after a REX or operand-size
 * prefix. If it can't be read, assume it may.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `ctx` | `const interrupt_frame *` | [in] The context. |
 *
 * @return TRUE if it may.
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `changes` | `pok_bool_t` | Whether it may |
 * | `code` | `const uint8_t *` | The code, at its kernel address |
 * | `opcode` | `uint32_t` | The opcode |
 * | `space_id` | `jet_space_id` | The space of the code |
 *
 * @hlr
 * - \hlrid{HLR_GDB_INSN_CHANGES_IF_01}: `insn_changes_if()` shall tell whether
 *   the instruction at the program counter of a context may change the
 *   interrupt flag itself, assuming it may if the instruction cannot be read.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_DBG_006}
 *
 * @llr
 * - \llrid{LLR_GDB_INSN_CHANGES_IF_01}: `insn_changes_if()` shall call
 *   `ja_addr_to_gdb_ro()` once with `rip` of `ctx`, 2 and the space: the result
 *   of `ja_space_get_current()` if `ja_gdb_context_is_user()` returns true for
 *   `ctx`, and 0 if it returns false.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDB_INSN_CHANGES_IF_01}
 * - \llrid{LLR_GDB_INSN_CHANGES_IF_02}: If `ja_addr_to_gdb_ro()` returns NULL,
 *   `insn_changes_if()` shall return `TRUE`.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDB_INSN_CHANGES_IF_01}
 * - \llrid{LLR_GDB_INSN_CHANGES_IF_03}: If `ja_addr_to_gdb_ro()` returns an
 *   address and the first byte there is from `0x40` to `0x4F` or is `0x66`,
 *   `insn_changes_if()` shall take the second byte there as the opcode.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDB_INSN_CHANGES_IF_01}
 * - \llrid{LLR_GDB_INSN_CHANGES_IF_04}: If `ja_addr_to_gdb_ro()` returns an
 *   address and the first byte there is less than `0x40`, or is greater than
 *   `0x4F` and is not `0x66`, `insn_changes_if()` shall take the first byte
 *   there as the opcode.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDB_INSN_CHANGES_IF_01}
 * - \llrid{LLR_GDB_INSN_CHANGES_IF_05}: If the opcode is `0xFA`, `0xFB`, `0x9D`
 *   or `0xCF`, `insn_changes_if()` shall return `TRUE`.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDB_INSN_CHANGES_IF_01}
 * - \llrid{LLR_GDB_INSN_CHANGES_IF_06}: If the opcode is none of `0xFA`,
 *   `0xFB`, `0x9D` and `0xCF`, `insn_changes_if()` shall return `FALSE`.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDB_INSN_CHANGES_IF_01}
 *
 * @design
 * 1. Set `space_id` to `ja_space_get_current()` if
 *    `ja_gdb_context_is_user(ctx)` is true, otherwise `0U`.
 *    \llrref{LLR_GDB_INSN_CHANGES_IF_01}
 * 2. Call `ja_addr_to_gdb_ro()` with the arguments `ctx->rip`, `2U` and
 *    `space_id`, and store the result in `code`.
 *    \llrref{LLR_GDB_INSN_CHANGES_IF_01}
 * 3. Set `changes` to `TRUE`. \llrref{LLR_GDB_INSN_CHANGES_IF_02}
 * 4. If `code` is not NULL: \llrref{LLR_GDB_INSN_CHANGES_IF_03},
 *    \llrref{LLR_GDB_INSN_CHANGES_IF_04}, \llrref{LLR_GDB_INSN_CHANGES_IF_05},
 *    \llrref{LLR_GDB_INSN_CHANGES_IF_06}
 *    1. Set `opcode` to `code[0]`. \llrref{LLR_GDB_INSN_CHANGES_IF_03},
 *       \llrref{LLR_GDB_INSN_CHANGES_IF_04}
 *    2. If (`opcode` is greater than or equal to `0x40U` and `opcode` is less
 *       than or equal to `0x4FU`) or `opcode` is equal to `0x66U`:
 *       \llrref{LLR_GDB_INSN_CHANGES_IF_03},
 *       \llrref{LLR_GDB_INSN_CHANGES_IF_04}
 *       1. Set `opcode` to `code[1]`. \llrref{LLR_GDB_INSN_CHANGES_IF_03}
 *    3. Set `changes` to
 *       `(opcode == 0xFAU) || (opcode == 0xFBU) || (opcode == 0x9DU) ||`
 *       `(opcode == 0xCFU)`. \llrref{LLR_GDB_INSN_CHANGES_IF_05},
 *       \llrref{LLR_GDB_INSN_CHANGES_IF_06}
 * 5. Return `changes`. \llrref{LLR_GDB_INSN_CHANGES_IF_02},
 *    \llrref{LLR_GDB_INSN_CHANGES_IF_05}, \llrref{LLR_GDB_INSN_CHANGES_IF_06}
 *
 * @calls
 * - `ja_gdb_context_is_user()`
 * - `ja_space_get_current()`
 * - `ja_addr_to_gdb_ro()`
 */
/*
 * Whether the instruction at the PC of 'ctx' may change the interrupt flag
 * itself: cli, sti, popf or iret, maybe after a REX or operand-size
 * prefix. If it can't be read, assume it may.
 *
 * MISRA C:2025 Rule 11.5 deviation: ja_addr_to_gdb_ro() returns the
 * kernel address of the code as a pointer to void.
 */
static pok_bool_t insn_changes_if(const interrupt_frame* ctx)
{
    jet_space_id space_id = ja_gdb_context_is_user(ctx)
        ? ja_space_get_current() : 0U;
    const uint8_t* code = ja_addr_to_gdb_ro(ctx->rip, 2U, space_id);
    pok_bool_t changes = TRUE;

    if (code != NULL)
    {
        uint32_t opcode = code[0];

        if (((opcode >= 0x40U) && (opcode <= 0x4FU)) || (opcode == 0x66U)) {
            opcode = code[1];
        }
        changes = (opcode == 0xFAU) || (opcode == 0xFBU) || (opcode == 0x9DU)
            || (opcode == 0xCFU);
    }

    return changes;
}

/**
 * @funcname `ja_gdb_single_step`
 *
 * @description
 * Sets or clears the single step (trap flag) of a context. GDB steps one thread
 * and expects no other to run meanwhile: interrupts are disabled for the step
 * (unless the instruction changes the flag itself), so it is done before the
 * scheduler may switch.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `ctx` | `interrupt_frame *` | [in,out] The context. |
 * | `enable` | `pok_bool_t` | [in] TRUE to step, FALSE to stop stepping. |
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `step_enable_interrupts` | `pok_bool_t` | Whether interrupts were disabled for a step of kernel code |
 *
 * @locals
 * None.
 *
 * @hlr
 * - \hlrid{HLR_GDB_X86_64_JA_GDB_SINGLE_STEP_01}: `ja_gdb_single_step()` shall
 *   make a context run one instruction and stop, with interrupts disabled for
 *   the step so that no other thread runs, unless the instruction changes the
 *   interrupt flag itself.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_DBG_006}
 *
 * @llr
 * - \llrid{LLR_GDB_X86_64_JA_GDB_SINGLE_STEP_01}: If `enable` is true,
 *   `ja_gdb_single_step()` shall set `RFLAGS_TF` in `rflags` of `ctx`.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDB_X86_64_JA_GDB_SINGLE_STEP_01}
 * - \llrid{LLR_GDB_X86_64_JA_GDB_SINGLE_STEP_02}: If `enable` is true,
 *   `RFLAGS_IF` is set in `rflags` of `ctx` and `insn_changes_if()` returns
 *   false for `ctx`, `ja_gdb_single_step()` shall clear `RFLAGS_IF` in `rflags`
 *   of `ctx`, and set `step_enable_interrupts` to true if
 *   `ja_gdb_context_is_user()` returns false for `ctx`, and to false if it
 *   returns true.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDB_X86_64_JA_GDB_SINGLE_STEP_01}
 * - \llrid{LLR_GDB_X86_64_JA_GDB_SINGLE_STEP_03}: If `enable` is true, and
 *   `RFLAGS_IF` is not set in `rflags` of `ctx` or `insn_changes_if()` returns
 *   true for `ctx`, `ja_gdb_single_step()` shall leave `RFLAGS_IF` as it is and
 *   set `step_enable_interrupts` to `FALSE`.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDB_X86_64_JA_GDB_SINGLE_STEP_01}
 * - \llrid{LLR_GDB_X86_64_JA_GDB_SINGLE_STEP_04}: If `enable` is false,
 *   `ja_gdb_single_step()` shall clear `RFLAGS_TF` in `rflags` of `ctx` and
 *   leave `step_enable_interrupts` as it is.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDB_X86_64_JA_GDB_SINGLE_STEP_01}
 *
 * @design
 * 1. If `enable` is true: \llrref{LLR_GDB_X86_64_JA_GDB_SINGLE_STEP_01},
 *    \llrref{LLR_GDB_X86_64_JA_GDB_SINGLE_STEP_02},
 *    \llrref{LLR_GDB_X86_64_JA_GDB_SINGLE_STEP_03}
 *    1. Set the bits `RFLAGS_TF` in `ctx->rflags`.
 *       \llrref{LLR_GDB_X86_64_JA_GDB_SINGLE_STEP_01}
 *    2. Set `step_enable_interrupts` to `FALSE`.
 *       \llrref{LLR_GDB_X86_64_JA_GDB_SINGLE_STEP_02},
 *       \llrref{LLR_GDB_X86_64_JA_GDB_SINGLE_STEP_03}
 *    3. If `(ctx->rflags & RFLAGS_IF)` is not equal to `0U` and
 *       `insn_changes_if(ctx)` is false:
 *       \llrref{LLR_GDB_X86_64_JA_GDB_SINGLE_STEP_02},
 *       \llrref{LLR_GDB_X86_64_JA_GDB_SINGLE_STEP_03}
 *       1. Keep only the bits `~(uint64_t)RFLAGS_IF` of `ctx->rflags`.
 *          \llrref{LLR_GDB_X86_64_JA_GDB_SINGLE_STEP_02}
 *       2. Set `step_enable_interrupts` to `!ja_gdb_context_is_user(ctx)`.
 *          \llrref{LLR_GDB_X86_64_JA_GDB_SINGLE_STEP_02}
 * 2. Otherwise: \llrref{LLR_GDB_X86_64_JA_GDB_SINGLE_STEP_04}
 *    1. Keep only the bits `~(uint64_t)RFLAGS_TF` of `ctx->rflags`.
 *
 * @calls
 * - `insn_changes_if()`
 * - `ja_gdb_context_is_user()`
 */
void ja_gdb_single_step(interrupt_frame* ctx,
    pok_bool_t enable)
{
    if (enable)
    {
        ctx->rflags |= RFLAGS_TF;

        /*
         * GDB steps one thread and expects no other to run meanwhile:
         * with interrupts disabled (the timer interrupt is pending after a
         * stop), the step is done before the scheduler may switch.
         */
        step_enable_interrupts = FALSE;
        if (((ctx->rflags & RFLAGS_IF) != 0U) && !insn_changes_if(ctx))
        {
            ctx->rflags &= ~(uint64_t)RFLAGS_IF;
            step_enable_interrupts = !ja_gdb_context_is_user(ctx);
        }
    }
    else
    {
        ctx->rflags &= ~(uint64_t)RFLAGS_TF;
    }
}

/**
 * @funcname `ja_gdb_context_is_user`
 *
 * @description
 * Tells whether a context runs in user space (CPL 3).
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `ctx` | `const interrupt_frame *` | [in] The context. |
 *
 * @return TRUE for user space.
 *
 * @globals
 * None.
 *
 * @locals
 * None.
 *
 * @hlr
 * - \hlrid{HLR_GDB_X86_64_JA_GDB_CONTEXT_IS_USER_01}:
 *   `ja_gdb_context_is_user()` shall tell whether a context runs in user mode.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_DBG_002}
 *
 * @llr
 * - \llrid{LLR_GDB_X86_64_JA_GDB_CONTEXT_IS_USER_01}: If the low 2 bits of `cs`
 *   of `ctx` are 3, `ja_gdb_context_is_user()` shall return true.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDB_X86_64_JA_GDB_CONTEXT_IS_USER_01}
 * - \llrid{LLR_GDB_X86_64_JA_GDB_CONTEXT_IS_USER_02}: If the low 2 bits of `cs`
 *   of `ctx` are not 3, `ja_gdb_context_is_user()` shall return false.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDB_X86_64_JA_GDB_CONTEXT_IS_USER_01}
 *
 * @design
 * 1. Return `(ctx->cs & 3U) == 3U`.
 *    \llrref{LLR_GDB_X86_64_JA_GDB_CONTEXT_IS_USER_01},
 *    \llrref{LLR_GDB_X86_64_JA_GDB_CONTEXT_IS_USER_02}
 *
 * @calls
 * None.
 */
pok_bool_t ja_gdb_context_is_user(const interrupt_frame* ctx)
{
    return (ctx->cs & 3U) == 3U;
}

/**
 * @funcname `ja_gdb_sync_icache`
 *
 * @description
 * Makes code written by GDB visible to instruction fetch: nothing to do on x86
 * (instruction fetch sees stores, and iretq serializes the processor).
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `addr` | `void *` | [in] The code (not used). |
 * | `size` | `size_t` | [in] Its size (not used). |
 *
 * @globals
 * None.
 *
 * @locals
 * None.
 *
 * @hlr
 * - \hlrid{HLR_GDB_X86_64_JA_GDB_SYNC_ICACHE_01}: `ja_gdb_sync_icache()` shall
 *   make code that the debugger wrote visible to the instruction fetch, which
 *   needs nothing on x86_64.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_DBG_004}
 *
 * @llr
 * - \llrid{LLR_GDB_X86_64_JA_GDB_SYNC_ICACHE_01}: `ja_gdb_sync_icache()` shall
 *   return without reading or writing memory.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDB_X86_64_JA_GDB_SYNC_ICACHE_01}
 *
 * @design
 * 1. Do nothing. \llrref{LLR_GDB_X86_64_JA_GDB_SYNC_ICACHE_01}
 *
 * @calls
 * None.
 */
void ja_gdb_sync_icache(void* addr, size_t size)
{
    /*
     * Instruction fetch sees stores on x86, and the return from the
     * exception (iretq) serializes the processor.
     */
    (void) addr;
    (void) size;
}

static hw_slot_t hw_slots[JA_GDB_HW_SLOTS];

/**
 * @funcname `hw_area_supported`
 *
 * @description
 * Tells whether the debug registers can watch an area: 1 byte for an
 * instruction breakpoint, an aligned 1, 2, 4 or 8 bytes for a watchpoint.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `type` | `ja_gdb_hw_type_e` | [in] The type. |
 * | `addr` | `uintptr_t` | [in] The address. |
 * | `len` | `size_t` | [in] The length. |
 *
 * @return TRUE if supported.
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `supported` | `pok_bool_t` | Whether it is supported |
 *
 * @hlr
 * - \hlrid{HLR_GDB_X86_64_HW_AREA_SUPPORTED_01}: `hw_area_supported()` shall
 *   tell whether the debug registers can watch an area: 1 byte for an
 *   instruction breakpoint, and 1, 2, 4 or 8 aligned bytes for a watchpoint.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_DBG_005}
 *
 * @llr
 * - \llrid{LLR_GDB_X86_64_HW_AREA_SUPPORTED_01}: If `type` is `JA_GDB_HW_EXEC`
 *   and `len` is 1, `hw_area_supported()` shall return true.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDB_X86_64_HW_AREA_SUPPORTED_01}
 * - \llrid{LLR_GDB_X86_64_HW_AREA_SUPPORTED_02}: If `type` is `JA_GDB_HW_EXEC`
 *   and `len` is not 1, `hw_area_supported()` shall return false.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDB_X86_64_HW_AREA_SUPPORTED_01}
 * - \llrid{LLR_GDB_X86_64_HW_AREA_SUPPORTED_03}: If `type` is not
 *   `JA_GDB_HW_EXEC`, `len` is 1, 2, 4 or 8, and `addr` is a multiple of `len`,
 *   `hw_area_supported()` shall return true.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDB_X86_64_HW_AREA_SUPPORTED_01}
 * - \llrid{LLR_GDB_X86_64_HW_AREA_SUPPORTED_04}: If `type` is not
 *   `JA_GDB_HW_EXEC`, and `len` is none of 1, 2, 4 and 8 or `addr` is not a
 *   multiple of `len`, `hw_area_supported()` shall return false.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDB_X86_64_HW_AREA_SUPPORTED_01}
 *
 * @design
 * 1. If `type` is equal to `JA_GDB_HW_EXEC`:
 *    \llrref{LLR_GDB_X86_64_HW_AREA_SUPPORTED_01},
 *    \llrref{LLR_GDB_X86_64_HW_AREA_SUPPORTED_02}
 *    1. Set `supported` to `(len == 1U)`.
 * 2. Otherwise: \llrref{LLR_GDB_X86_64_HW_AREA_SUPPORTED_03},
 *    \llrref{LLR_GDB_X86_64_HW_AREA_SUPPORTED_04}
 *    1. Set `supported` to
 *       `((len == 1U) || (len == 2U) || (len == 4U) || (len == 8U)) && ((addr`
 *       `% len) == 0U)`.
 * 3. Return `supported`. \llrref{LLR_GDB_X86_64_HW_AREA_SUPPORTED_01},
 *    \llrref{LLR_GDB_X86_64_HW_AREA_SUPPORTED_02},
 *    \llrref{LLR_GDB_X86_64_HW_AREA_SUPPORTED_03},
 *    \llrref{LLR_GDB_X86_64_HW_AREA_SUPPORTED_04}
 *
 * @calls
 * None.
 */
static pok_bool_t hw_area_supported(ja_gdb_hw_type_e type,
    uintptr_t addr, size_t len)
{
    pok_bool_t supported;

    if (type == JA_GDB_HW_EXEC) {
        // GDB's breakpoint kind on x86 is 1 (the size of int3).
        supported = (len == 1U);
    } else {
        supported = ((len == 1U) || (len == 2U) || (len == 4U)
            || (len == 8U)) && ((addr % len) == 0U);
    }

    return supported;
}

/**
 * @funcname `hw_slot_is`
 *
 * @description
 * Tells whether a slot holds a hardware breakpoint or watchpoint.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `slot` | `const hw_slot_t *` | [in] The slot. |
 * | `type` | `ja_gdb_hw_type_e` | [in] The type. |
 * | `addr` | `uintptr_t` | [in] The address. |
 * | `len` | `size_t` | [in] The length. |
 * | `space_id` | `jet_space_id` | [in] The space. |
 *
 * @return TRUE if it does.
 *
 * @globals
 * None.
 *
 * @locals
 * None.
 *
 * @hlr
 * - \hlrid{HLR_GDB_X86_64_HW_SLOT_IS_01}: `hw_slot_is()` shall tell whether a
 *   slot holds a given hardware breakpoint or watchpoint.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_DBG_005}
 *
 * @llr
 * - \llrid{LLR_GDB_X86_64_HW_SLOT_IS_01}: If `used` of `slot` is true and its
 *   `type`, `addr`, `len` and `space_id` are equal to the arguments of the same
 *   names, `hw_slot_is()` shall return true.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDB_X86_64_HW_SLOT_IS_01}
 * - \llrid{LLR_GDB_X86_64_HW_SLOT_IS_02}: If `used` of `slot` is false, or its
 *   `type`, its `addr`, its `len` or its `space_id` differs from the argument
 *   of the same name, `hw_slot_is()` shall return false.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDB_X86_64_HW_SLOT_IS_01}
 *
 * @design
 * 1. Return
 *    `slot->used && (slot->type == type) && (slot->addr == addr) && (slot->len`
 *    `== len) && (slot->space_id == space_id)`.
 *    \llrref{LLR_GDB_X86_64_HW_SLOT_IS_01},
 *    \llrref{LLR_GDB_X86_64_HW_SLOT_IS_02}
 *
 * @calls
 * None.
 */
static pok_bool_t hw_slot_is(const hw_slot_t* slot,
    ja_gdb_hw_type_e type, uintptr_t addr, size_t len,
    jet_space_id space_id)
{
    return slot->used && (slot->type == type) && (slot->addr == addr)
        && (slot->len == len) && (slot->space_id == space_id);
}

/**
 * @funcname `ja_gdb_hw_insert`
 *
 * @description
 * Inserts a hardware breakpoint or watchpoint into a free slot (written to the
 * debug registers when the system resumes); an existing one is kept.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `type` | `ja_gdb_hw_type_e` | [in] The type. |
 * | `addr` | `uintptr_t` | [in] The address. |
 * | `len` | `size_t` | [in] The length. |
 * | `space_id` | `jet_space_id` | [in] The space. |
 *
 * @return TRUE if it is in place.
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `hw_slots` | `hw_slot_t[4]` | The hardware breakpoints and watchpoints |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `i` | `uint32_t` | The index of a slot |
 * | `inserted` | `pok_bool_t` | Whether it is in place |
 *
 * @hlr
 * - \hlrid{HLR_GDB_X86_64_JA_GDB_HW_INSERT_01}: `ja_gdb_hw_insert()` shall
 *   insert a hardware breakpoint or watchpoint that the debug registers support
 *   into a free slot, keep one that exists, and tell whether it is in place.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_DBG_005}
 *
 * @llr
 * - \llrid{LLR_GDB_X86_64_JA_GDB_HW_INSERT_01}: If `hw_area_supported()`
 *   returns false for `type`, `addr` and `len`, `ja_gdb_hw_insert()` shall
 *   return `FALSE` and leave `hw_slots` as it is.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDB_X86_64_JA_GDB_HW_INSERT_01}
 * - \llrid{LLR_GDB_X86_64_JA_GDB_HW_INSERT_02}: If `hw_area_supported()`
 *   returns true and `hw_slot_is()` returns true for one of the
 *   `JA_GDB_HW_SLOTS` elements of `hw_slots` and `type`, `addr`, `len` and
 *   `space_id`, `ja_gdb_hw_insert()` shall return `TRUE` and leave `hw_slots`
 *   as it is.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDB_X86_64_JA_GDB_HW_INSERT_01}
 * - \llrid{LLR_GDB_X86_64_JA_GDB_HW_INSERT_03}: If `hw_area_supported()`
 *   returns true, `hw_slot_is()` returns false for every element of `hw_slots`,
 *   and `used` of an element is false, `ja_gdb_hw_insert()` shall set, in the
 *   first such element, `used` to `TRUE` and `type`, `addr`, `len` and
 *   `space_id` to the arguments of the same names, and return `TRUE`.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDB_X86_64_JA_GDB_HW_INSERT_01}
 * - \llrid{LLR_GDB_X86_64_JA_GDB_HW_INSERT_04}: If `hw_area_supported()`
 *   returns true, `hw_slot_is()` returns false for every element of `hw_slots`,
 *   and `used` of every element is true, `ja_gdb_hw_insert()` shall return
 *   `FALSE` and leave `hw_slots` as it is.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDB_X86_64_JA_GDB_HW_INSERT_01}
 *
 * @design
 * 1. Set `inserted` to `FALSE`. \llrref{LLR_GDB_X86_64_JA_GDB_HW_INSERT_01},
 *    \llrref{LLR_GDB_X86_64_JA_GDB_HW_INSERT_02},
 *    \llrref{LLR_GDB_X86_64_JA_GDB_HW_INSERT_03},
 *    \llrref{LLR_GDB_X86_64_JA_GDB_HW_INSERT_04}
 * 2. If `hw_area_supported(type, addr, len)` is true:
 *    \llrref{LLR_GDB_X86_64_JA_GDB_HW_INSERT_01},
 *    \llrref{LLR_GDB_X86_64_JA_GDB_HW_INSERT_02},
 *    \llrref{LLR_GDB_X86_64_JA_GDB_HW_INSERT_03},
 *    \llrref{LLR_GDB_X86_64_JA_GDB_HW_INSERT_04}
 *    1. Set `i` to `0U`; then while `i` is less than `JA_GDB_HW_SLOTS`, repeat
 *       (after each pass: increment `i`):
 *       \llrref{LLR_GDB_X86_64_JA_GDB_HW_INSERT_02},
 *       \llrref{LLR_GDB_X86_64_JA_GDB_HW_INSERT_03},
 *       \llrref{LLR_GDB_X86_64_JA_GDB_HW_INSERT_04}
 *       1. If `hw_slot_is(&hw_slots[i], type, addr, len, space_id)` is true:
 *          \llrref{LLR_GDB_X86_64_JA_GDB_HW_INSERT_02}
 *          1. Set `inserted` to `TRUE`.
 *    2. Set `i` to `0U`; then while `i` is less than `JA_GDB_HW_SLOTS` and
 *       `inserted` is false, repeat (after each pass: increment `i`):
 *       \llrref{LLR_GDB_X86_64_JA_GDB_HW_INSERT_03},
 *       \llrref{LLR_GDB_X86_64_JA_GDB_HW_INSERT_04}
 *       1. If `hw_slots[i].used` is false:
 *          1. Set `hw_slots[i].used` to `TRUE`.
 *             \llrref{LLR_GDB_X86_64_JA_GDB_HW_INSERT_03}
 *          2. Set `hw_slots[i].type` to `type`.
 *             \llrref{LLR_GDB_X86_64_JA_GDB_HW_INSERT_03}
 *          3. Set `hw_slots[i].addr` to `addr`.
 *             \llrref{LLR_GDB_X86_64_JA_GDB_HW_INSERT_03}
 *          4. Set `hw_slots[i].len` to `len`.
 *             \llrref{LLR_GDB_X86_64_JA_GDB_HW_INSERT_03}
 *          5. Set `hw_slots[i].space_id` to `space_id`.
 *             \llrref{LLR_GDB_X86_64_JA_GDB_HW_INSERT_03}
 *          6. Set `inserted` to `TRUE`.
 *             \llrref{LLR_GDB_X86_64_JA_GDB_HW_INSERT_03}
 * 3. Return `inserted`. \llrref{LLR_GDB_X86_64_JA_GDB_HW_INSERT_01},
 *    \llrref{LLR_GDB_X86_64_JA_GDB_HW_INSERT_02},
 *    \llrref{LLR_GDB_X86_64_JA_GDB_HW_INSERT_03},
 *    \llrref{LLR_GDB_X86_64_JA_GDB_HW_INSERT_04}
 *
 * @calls
 * - `hw_area_supported()`
 * - `hw_slot_is()`
 */
pok_bool_t ja_gdb_hw_insert(ja_gdb_hw_type_e type, uintptr_t addr,
    size_t len, jet_space_id space_id)
{
    pok_bool_t inserted = FALSE;

    if (hw_area_supported(type, addr, len))
    {
        for (uint32_t i = 0U; i < JA_GDB_HW_SLOTS; i++)
        {
            if (hw_slot_is(&hw_slots[i], type, addr, len, space_id)) {
                inserted = TRUE; // Already there.
            }
        }

        for (uint32_t i = 0U; (i < JA_GDB_HW_SLOTS) && !inserted; i++)
        {
            if (!hw_slots[i].used)
            {
                hw_slots[i].used = TRUE;
                hw_slots[i].type = type;
                hw_slots[i].addr = addr;
                hw_slots[i].len = len;
                hw_slots[i].space_id = space_id;
                inserted = TRUE;
            }
        }
    }

    return inserted;
}

/**
 * @funcname `ja_gdb_hw_remove`
 *
 * @description
 * Removes a hardware breakpoint or watchpoint.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `type` | `ja_gdb_hw_type_e` | [in] The type. |
 * | `addr` | `uintptr_t` | [in] The address. |
 * | `len` | `size_t` | [in] The length. |
 * | `space_id` | `jet_space_id` | [in] The space. |
 *
 * @return TRUE if there was one.
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `hw_slots` | `hw_slot_t[4]` | The hardware breakpoints and watchpoints |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `i` | `uint32_t` | The index of a slot |
 * | `removed` | `pok_bool_t` | Whether it was removed |
 *
 * @hlr
 * - \hlrid{HLR_GDB_X86_64_JA_GDB_HW_REMOVE_01}: `ja_gdb_hw_remove()` shall
 *   remove a hardware breakpoint or watchpoint and tell whether there was one.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_DBG_005}
 *
 * @llr
 * - \llrid{LLR_GDB_X86_64_JA_GDB_HW_REMOVE_01}: For each of the
 *   `JA_GDB_HW_SLOTS` elements of `hw_slots` for which `hw_slot_is()` returns
 *   true with `type`, `addr`, `len` and `space_id`, `ja_gdb_hw_remove()` shall
 *   set `used` to `FALSE`.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDB_X86_64_JA_GDB_HW_REMOVE_01}
 * - \llrid{LLR_GDB_X86_64_JA_GDB_HW_REMOVE_02}: If `hw_slot_is()` returns true
 *   for at least one element of `hw_slots`, `ja_gdb_hw_remove()` shall return
 *   `TRUE`.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDB_X86_64_JA_GDB_HW_REMOVE_01}
 * - \llrid{LLR_GDB_X86_64_JA_GDB_HW_REMOVE_03}: If `hw_slot_is()` returns false
 *   for every element of `hw_slots`, `ja_gdb_hw_remove()` shall return `FALSE`
 *   and leave `hw_slots` as it is.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDB_X86_64_JA_GDB_HW_REMOVE_01}
 *
 * @design
 * 1. Set `removed` to `FALSE`. \llrref{LLR_GDB_X86_64_JA_GDB_HW_REMOVE_02},
 *    \llrref{LLR_GDB_X86_64_JA_GDB_HW_REMOVE_03}
 * 2. Set `i` to `0U`; then while `i` is less than `JA_GDB_HW_SLOTS`, repeat
 *    (after each pass: increment `i`):
 *    \llrref{LLR_GDB_X86_64_JA_GDB_HW_REMOVE_01},
 *    \llrref{LLR_GDB_X86_64_JA_GDB_HW_REMOVE_02},
 *    \llrref{LLR_GDB_X86_64_JA_GDB_HW_REMOVE_03}
 *    1. If `hw_slot_is(&hw_slots[i], type, addr, len, space_id)` is true:
 *       \llrref{LLR_GDB_X86_64_JA_GDB_HW_REMOVE_01},
 *       \llrref{LLR_GDB_X86_64_JA_GDB_HW_REMOVE_02}
 *       1. Set `hw_slots[i].used` to `FALSE`.
 *       2. Set `removed` to `TRUE`.
 * 3. Return `removed`. \llrref{LLR_GDB_X86_64_JA_GDB_HW_REMOVE_02},
 *    \llrref{LLR_GDB_X86_64_JA_GDB_HW_REMOVE_03}
 *
 * @calls
 * - `hw_slot_is()`
 */
pok_bool_t ja_gdb_hw_remove(ja_gdb_hw_type_e type, uintptr_t addr,
    size_t len, jet_space_id space_id)
{
    pok_bool_t removed = FALSE;

    for (uint32_t i = 0U; i < JA_GDB_HW_SLOTS; i++)
    {
        if (hw_slot_is(&hw_slots[i], type, addr, len, space_id))
        {
            hw_slots[i].used = FALSE;
            removed = TRUE;
        }
    }

    return removed;
}

/**
 * @funcname `ja_gdb_hw_remove_all`
 *
 * @description
 * Removes every hardware breakpoint and watchpoint.
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `hw_slots` | `hw_slot_t[4]` | The hardware breakpoints and watchpoints |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `i` | `uint32_t` | The index of a slot |
 *
 * @hlr
 * - \hlrid{HLR_GDB_X86_64_JA_GDB_HW_REMOVE_ALL_01}: `ja_gdb_hw_remove_all()`
 *   shall remove every hardware breakpoint and watchpoint.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_DBG_015}
 *
 * @llr
 * - \llrid{LLR_GDB_X86_64_JA_GDB_HW_REMOVE_ALL_01}: `ja_gdb_hw_remove_all()`
 *   shall set `used` of each of the `JA_GDB_HW_SLOTS` elements of `hw_slots` to
 *   `FALSE`.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDB_X86_64_JA_GDB_HW_REMOVE_ALL_01}
 *
 * @design
 * 1. Set `i` to `0U`; then while `i` is less than `JA_GDB_HW_SLOTS`, repeat
 *    (after each pass: increment `i`):
 *    \llrref{LLR_GDB_X86_64_JA_GDB_HW_REMOVE_ALL_01}
 *    1. Set `hw_slots[i].used` to `FALSE`.
 *
 * @calls
 * None.
 */
void ja_gdb_hw_remove_all(void)
{
    for (uint32_t i = 0U; i < JA_GDB_HW_SLOTS; i++)
    {
        hw_slots[i].used = FALSE;
    }
}

/**
 * @funcname `ja_gdb_hw_enter`
 *
 * @description
 * Disables the debug registers while the GDB server runs (it mustn't hit them).
 *
 * @globals
 * None.
 *
 * @locals
 * None.
 *
 * @hlr
 * - \hlrid{HLR_GDB_X86_64_JA_GDB_HW_ENTER_01}: `ja_gdb_hw_enter()` shall
 *   disable the debug registers while the GDB server runs.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_DBG_005}
 *
 * @llr
 * - \llrid{LLR_GDB_X86_64_JA_GDB_HW_ENTER_01}: `ja_gdb_hw_enter()` shall call
 *   `cpu_write_dr7()` once with 0.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDB_X86_64_JA_GDB_HW_ENTER_01}
 *
 * @design
 * 1. Call `cpu_write_dr7()` with the argument `0U`.
 *    \llrref{LLR_GDB_X86_64_JA_GDB_HW_ENTER_01}
 *
 * @calls
 * - `cpu_write_dr7()`
 */
void ja_gdb_hw_enter(void)
{
    // The GDB server itself mustn't hit them.
    cpu_write_dr7(0U);
}

/**
 * @funcname `hw_dr7_bits`
 *
 * @description
 * Returns the DR7 bits of a slot: its enable bit and its R/W and LEN fields
 * (reads are watched with writes: x86 can't watch reads only).
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `slot` | `const hw_slot_t *` | [in] The slot. |
 * | `index` | `uint32_t` | [in] The slot's debug address register. |
 *
 * @return The bits.
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `enable` | `uint64_t` | The enable bit |
 * | `enable_shift` | `uint32_t` | The position of the enable bit |
 * | `len` | `uint64_t` | The LEN field |
 * | `len_shift` | `uint32_t` | The position of the LEN field |
 * | `rw` | `uint64_t` | The R/W field |
 * | `rw_shift` | `uint32_t` | The position of the R/W field |
 *
 * @hlr
 * - \hlrid{HLR_GDB_HW_DR7_BITS_01}: `hw_dr7_bits()` shall give the bits of the
 *   debug control register for a slot: its enable bit and its access and length
 *   fields, a read watchpoint being watched with writes.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_DBG_005}
 *
 * @llr
 * - \llrid{LLR_GDB_HW_DR7_BITS_01}: If `type` of `slot` is `JA_GDB_HW_EXEC` or
 *   `JA_GDB_HW_WRITE`, `hw_dr7_bits()` shall take 0 or 1, in this order, as the
 *   R/W field.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDB_HW_DR7_BITS_01}
 * - \llrid{LLR_GDB_HW_DR7_BITS_02}: If `type` of `slot` is neither
 *   `JA_GDB_HW_EXEC` nor `JA_GDB_HW_WRITE`, `hw_dr7_bits()` shall take 3 as the
 *   R/W field.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDB_HW_DR7_BITS_01}
 * - \llrid{LLR_GDB_HW_DR7_BITS_03}: If `len` of `slot` is 2, 4 or 8,
 *   `hw_dr7_bits()` shall take 1, 3 or 2, in this order, as the LEN field.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDB_HW_DR7_BITS_01}
 * - \llrid{LLR_GDB_HW_DR7_BITS_04}: If `len` of `slot` is none of 2, 4 and 8,
 *   `hw_dr7_bits()` shall take 0 as the LEN field.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDB_HW_DR7_BITS_01}
 * - \llrid{LLR_GDB_HW_DR7_BITS_05}: `hw_dr7_bits()` shall return the value with
 *   the bit `2 * index` set, the R/W field at the bits from `16 + 4 * index`
 *   and the LEN field at the bits from `18 + 4 * index`.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDB_HW_DR7_BITS_01}
 *
 * @design
 * 1. Depending on `slot->type`: \llrref{LLR_GDB_HW_DR7_BITS_01},
 *    \llrref{LLR_GDB_HW_DR7_BITS_02}
 *    1. Case `JA_GDB_HW_EXEC`: \llrref{LLR_GDB_HW_DR7_BITS_01}
 *       1. Set `rw` to `0U`.
 *       2. Leave the switch.
 *    2. Case `JA_GDB_HW_WRITE`: \llrref{LLR_GDB_HW_DR7_BITS_01}
 *       1. Set `rw` to `1U`.
 *       2. Leave the switch.
 *    3. Otherwise (default): \llrref{LLR_GDB_HW_DR7_BITS_02}
 *       1. Set `rw` to `3U`.
 *       2. Leave the switch.
 * 2. Depending on `slot->len`: \llrref{LLR_GDB_HW_DR7_BITS_03},
 *    \llrref{LLR_GDB_HW_DR7_BITS_04}
 *    1. Case `2U`: \llrref{LLR_GDB_HW_DR7_BITS_03}
 *       1. Set `len` to `1U`.
 *       2. Leave the switch.
 *    2. Case `4U`: \llrref{LLR_GDB_HW_DR7_BITS_03}
 *       1. Set `len` to `3U`.
 *       2. Leave the switch.
 *    3. Case `8U`: \llrref{LLR_GDB_HW_DR7_BITS_03}
 *       1. Set `len` to `2U`.
 *       2. Leave the switch.
 *    4. Otherwise (default): \llrref{LLR_GDB_HW_DR7_BITS_04}
 *       1. Set `len` to `0U`.
 *       2. Leave the switch.
 * 3. Set `enable_shift` to `2U * index`. \llrref{LLR_GDB_HW_DR7_BITS_05}
 * 4. Set `rw_shift` to `16U + (4U * index)`. \llrref{LLR_GDB_HW_DR7_BITS_05}
 * 5. Set `len_shift` to `18U + (4U * index)`. \llrref{LLR_GDB_HW_DR7_BITS_05}
 * 6. Set `enable` to `(uint64_t)1U << enable_shift`.
 *    \llrref{LLR_GDB_HW_DR7_BITS_05}
 * 7. Return `enable | (rw << rw_shift) | (len << len_shift)`.
 *    \llrref{LLR_GDB_HW_DR7_BITS_05}
 *
 * @calls
 * None.
 */
static uint64_t hw_dr7_bits(const hw_slot_t* slot, uint32_t index)
{
    uint64_t rw;
    uint64_t len;

    switch (slot->type)
    {
    case JA_GDB_HW_EXEC:  rw = 0U; break;
    case JA_GDB_HW_WRITE: rw = 1U; break;
    // x86 can't watch reads only: reads are watched with writes.
    default:              rw = 3U; break;
    }

    switch (slot->len)
    {
    case 2U: len = 1U; break;
    case 4U: len = 3U; break;
    case 8U: len = 2U; break;
    default: len = 0U; break;
    }

    uint32_t enable_shift = 2U * index;
    uint32_t rw_shift = 16U + (4U * index);
    uint32_t len_shift = 18U + (4U * index);
    uint64_t enable = (uint64_t)1U << enable_shift;

    return enable | (rw << rw_shift) | (len << len_shift);
}

/**
 * @funcname `ja_gdb_hw_leave`
 *
 * @description
 * Writes the hardware breakpoints and watchpoints into the debug registers when
 * the system resumes.
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `hw_slots` | `hw_slot_t[4]` | The hardware breakpoints and watchpoints |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `dr7` | `uint64_t` | DR7 |
 * | `i` | `uint32_t` | The index of a slot |
 *
 * @hlr
 * - \hlrid{HLR_GDB_X86_64_JA_GDB_HW_LEAVE_01}: `ja_gdb_hw_leave()` shall write
 *   the hardware breakpoints and watchpoints into the debug registers when the
 *   module resumes.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_DBG_005}
 *
 * @llr
 * - \llrid{LLR_GDB_X86_64_JA_GDB_HW_LEAVE_01}: For each element `i` of
 *   `hw_slots` whose `used` is true, `ja_gdb_hw_leave()` shall call
 *   `cpu_write_dr_addr()` with `i` and its `addr`, and add the result of
 *   `hw_dr7_bits()` for the element and `i` to the value for DR7, which is 0 at
 *   first.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDB_X86_64_JA_GDB_HW_LEAVE_01}
 * - \llrid{LLR_GDB_X86_64_JA_GDB_HW_LEAVE_02}: If the value for DR7 is then not
 *   0, `ja_gdb_hw_leave()` shall call `cpu_write_dr7()` once with that value
 *   with `DR7_LE` set.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDB_X86_64_JA_GDB_HW_LEAVE_01}
 * - \llrid{LLR_GDB_X86_64_JA_GDB_HW_LEAVE_03}: If the value for DR7 is then 0,
 *   `ja_gdb_hw_leave()` shall call `cpu_write_dr7()` once with 0.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDB_X86_64_JA_GDB_HW_LEAVE_01}
 *
 * @design
 * 1. Set `dr7` to `0U`. \llrref{LLR_GDB_X86_64_JA_GDB_HW_LEAVE_01},
 *    \llrref{LLR_GDB_X86_64_JA_GDB_HW_LEAVE_02},
 *    \llrref{LLR_GDB_X86_64_JA_GDB_HW_LEAVE_03}
 * 2. Set `i` to `0U`; then while `i` is less than `JA_GDB_HW_SLOTS`, repeat
 *    (after each pass: increment `i`):
 *    \llrref{LLR_GDB_X86_64_JA_GDB_HW_LEAVE_01}
 *    1. If `hw_slots[i].used` is true:
 *       1. Call `cpu_write_dr_addr()` with the arguments `i` and
 *          `hw_slots[i].addr`.
 *       2. Set the bits `hw_dr7_bits(&hw_slots[i], i)` in `dr7`.
 * 3. If `dr7` is not equal to `0U`: \llrref{LLR_GDB_X86_64_JA_GDB_HW_LEAVE_02},
 *    \llrref{LLR_GDB_X86_64_JA_GDB_HW_LEAVE_03}
 *    1. Set the bits `DR7_LE` in `dr7`.
 *       \llrref{LLR_GDB_X86_64_JA_GDB_HW_LEAVE_02}
 * 4. Call `cpu_write_dr7()` with the argument `dr7`.
 *    \llrref{LLR_GDB_X86_64_JA_GDB_HW_LEAVE_02},
 *    \llrref{LLR_GDB_X86_64_JA_GDB_HW_LEAVE_03}
 *
 * @calls
 * - `cpu_write_dr_addr()`
 * - `hw_dr7_bits()`
 * - `cpu_write_dr7()`
 */
void ja_gdb_hw_leave(void)
{
    uint64_t dr7 = 0U;

    for (uint32_t i = 0U; i < JA_GDB_HW_SLOTS; i++)
    {
        if (hw_slots[i].used)
        {
            cpu_write_dr_addr(i, hw_slots[i].addr);
            dr7 |= hw_dr7_bits(&hw_slots[i], i);
        }
    }

    if (dr7 != 0U) {
        dr7 |= DR7_LE;
    }

    cpu_write_dr7(dr7);
}

/**
 * @funcname `hw_stop_reason`
 *
 * @description
 * Returns the stop reason of a hardware breakpoint or watchpoint type.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `type` | `ja_gdb_hw_type_e` | [in] The type. |
 *
 * @return The stop reason.
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `reason` | `jet_gdb_stop_reason_e` | The stop reason |
 *
 * @hlr
 * - \hlrid{HLR_GDB_X86_64_HW_STOP_REASON_01}: `hw_stop_reason()` shall give the
 *   stop reason of a type of hardware breakpoint or watchpoint.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_DBG_014}
 *
 * @llr
 * - \llrid{LLR_GDB_X86_64_HW_STOP_REASON_01}: If `type` is `JA_GDB_HW_EXEC`,
 *   `JA_GDB_HW_WRITE` or `JA_GDB_HW_READ`, `hw_stop_reason()` shall return
 *   `JET_GDB_STOP_HW_BREAKPOINT`, `JET_GDB_STOP_WATCH` or
 *   `JET_GDB_STOP_RWATCH`, in this order.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDB_X86_64_HW_STOP_REASON_01}
 * - \llrid{LLR_GDB_X86_64_HW_STOP_REASON_02}: If `type` is none of
 *   `JA_GDB_HW_EXEC`, `JA_GDB_HW_WRITE` and `JA_GDB_HW_READ`,
 *   `hw_stop_reason()` shall return `JET_GDB_STOP_AWATCH`.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDB_X86_64_HW_STOP_REASON_01}
 *
 * @design
 * 1. Depending on `type`: \llrref{LLR_GDB_X86_64_HW_STOP_REASON_01},
 *    \llrref{LLR_GDB_X86_64_HW_STOP_REASON_02}
 *    1. Case `JA_GDB_HW_EXEC`: \llrref{LLR_GDB_X86_64_HW_STOP_REASON_01}
 *       1. Set `reason` to `JET_GDB_STOP_HW_BREAKPOINT`.
 *       2. Leave the switch.
 *    2. Case `JA_GDB_HW_WRITE`: \llrref{LLR_GDB_X86_64_HW_STOP_REASON_01}
 *       1. Set `reason` to `JET_GDB_STOP_WATCH`.
 *       2. Leave the switch.
 *    3. Case `JA_GDB_HW_READ`: \llrref{LLR_GDB_X86_64_HW_STOP_REASON_01}
 *       1. Set `reason` to `JET_GDB_STOP_RWATCH`.
 *       2. Leave the switch.
 *    4. Otherwise (default): \llrref{LLR_GDB_X86_64_HW_STOP_REASON_02}
 *       1. Set `reason` to `JET_GDB_STOP_AWATCH`.
 *       2. Leave the switch.
 * 2. Return `reason`. \llrref{LLR_GDB_X86_64_HW_STOP_REASON_01},
 *    \llrref{LLR_GDB_X86_64_HW_STOP_REASON_02}
 *
 * @calls
 * None.
 */
static jet_gdb_stop_reason_e hw_stop_reason(ja_gdb_hw_type_e type)
{
    jet_gdb_stop_reason_e reason;

    switch (type)
    {
    case JA_GDB_HW_EXEC:  reason = JET_GDB_STOP_HW_BREAKPOINT; break;
    case JA_GDB_HW_WRITE: reason = JET_GDB_STOP_WATCH; break;
    case JA_GDB_HW_READ:  reason = JET_GDB_STOP_RWATCH; break;
    default:              reason = JET_GDB_STOP_AWATCH; break;
    }

    return reason;
}

/**
 * @funcname `pok_trap_addr`
 *
 * @description
 * Returns the address of pok_trap() (the int3 of a GDB request).
 *
 * @return The address.
 *
 * @globals
 * None.
 *
 * @locals
 * None.
 *
 * @hlr
 * - \hlrid{HLR_GDB_X86_64_POK_TRAP_ADDR_01}: `pok_trap_addr()` shall give the
 *   address of the trap by which the GDB partition asks to stop the module.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_DBG_006}
 *
 * @llr
 * - \llrid{LLR_GDB_X86_64_POK_TRAP_ADDR_01}: `pok_trap_addr()` shall return the
 *   address of `pok_trap()`.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDB_X86_64_POK_TRAP_ADDR_01}
 *
 * @design
 * 1. Return `(uintptr_t)&pok_trap`. \llrref{LLR_GDB_X86_64_POK_TRAP_ADDR_01}
 *
 * @calls
 * None.
 */
/*
 * MISRA C:2025 Rule 11.1 deviation: the address of pok_trap() is compared
 * with the address of the breakpoint as an integer.
 */
static uintptr_t pok_trap_addr(void)
{
    return (uintptr_t)&pok_trap;
}

/**
 * @funcname `trap_space`
 *
 * @description
 * Returns the space of the code that trapped.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `from_user` | `pok_bool_t` | [in] Whether it was user code. |
 *
 * @return The current space, or 0 for the kernel.
 *
 * @globals
 * None.
 *
 * @locals
 * None.
 *
 * @hlr
 * - \hlrid{HLR_GDB_X86_64_TRAP_SPACE_01}: `trap_space()` shall give the memory
 *   space of the code that trapped: the current space for user code and 0 for
 *   the kernel.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_DBG_014}
 *
 * @llr
 * - \llrid{LLR_GDB_X86_64_TRAP_SPACE_01}: If `from_user` is true,
 *   `trap_space()` shall return the result of `ja_space_get_current()`.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDB_X86_64_TRAP_SPACE_01}
 * - \llrid{LLR_GDB_X86_64_TRAP_SPACE_02}: If `from_user` is false,
 *   `trap_space()` shall return 0 without calling `ja_space_get_current()`.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDB_X86_64_TRAP_SPACE_01}
 *
 * @design
 * 1. Return `ja_space_get_current()` if `from_user` is true, otherwise `0U`.
 *    \llrref{LLR_GDB_X86_64_TRAP_SPACE_01},
 *    \llrref{LLR_GDB_X86_64_TRAP_SPACE_02}
 *
 * @calls
 * - `ja_space_get_current()`
 */
static jet_space_id trap_space(pok_bool_t from_user)
{
    return from_user ? ja_space_get_current() : 0U;
}

/**
 * @funcname `breakpoint_trap`
 *
 * @description
 * Handles the breakpoint exception (int3; rip is after the instruction): a
 * request of the GDB partition (pok_trap()), a GDB breakpoint (the PC is put
 * back at it) or another int3.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `frame` | `interrupt_frame *` | [in,out] The interrupt frame. |
 * | `from_user` | `pok_bool_t` | [in] Whether it came from user space. |
 *
 * @return TRUE if GDB handled it.
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `handled` | `pok_bool_t` | Whether GDB handled it |
 * | `pc` | `uintptr_t` | The address of the int3 |
 *
 * @hlr
 * - \hlrid{HLR_GDB_X86_64_BREAKPOINT_TRAP_01}: `breakpoint_trap()` shall handle
 *   the breakpoint exception: a request of the GDB partition, a software
 *   breakpoint of the debugger (the program counter going back to it), or
 *   another breakpoint instruction, and tell whether the debugger handled it.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_DBG_014}
 *   - \srqref{SRQ_JET_DBG_006}
 *
 * @llr
 * - \llrid{LLR_GDB_X86_64_BREAKPOINT_TRAP_01}: If `from_user` is false and the
 *   address of the trap (`rip` of `frame` minus `JA_GDB_BREAKPOINT_SIZE`) is
 *   the result of `pok_trap_addr()`, `breakpoint_trap()` shall return the
 *   result of `jet_gdb_trap()` for `frame`, `JET_GDB_STOP_REQUEST` and the
 *   address of the trap.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDB_X86_64_BREAKPOINT_TRAP_01}
 * - \llrid{LLR_GDB_X86_64_BREAKPOINT_TRAP_02}: If `from_user` is true or the
 *   address of the trap is not the result of `pok_trap_addr()`, and
 *   `jet_gdb_is_breakpoint()` returns true for the result of `trap_space()` for
 *   `from_user` and the address of the trap, `breakpoint_trap()` shall set
 *   `rip` of `frame` to the address of the trap and return the result of
 *   `jet_gdb_trap()` for `frame`, `JET_GDB_STOP_SW_BREAKPOINT` and the address
 *   of the trap.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDB_X86_64_BREAKPOINT_TRAP_01}
 * - \llrid{LLR_GDB_X86_64_BREAKPOINT_TRAP_03}: If and only if that
 *   `jet_gdb_trap()` for `JET_GDB_STOP_SW_BREAKPOINT` returns false,
 *   `breakpoint_trap()` shall set `rip` of `frame` back to the address of the
 *   trap plus `JA_GDB_BREAKPOINT_SIZE`.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDB_X86_64_BREAKPOINT_TRAP_01}
 * - \llrid{LLR_GDB_X86_64_BREAKPOINT_TRAP_04}: If `from_user` is true or the
 *   address of the trap is not the result of `pok_trap_addr()`, and
 *   `jet_gdb_is_breakpoint()` returns false for the result of `trap_space()`
 *   for `from_user` and the address of the trap, `breakpoint_trap()` shall
 *   return the result of `jet_gdb_trap()` for `frame`, `JET_GDB_STOP_TRAP` and
 *   the address of the trap, leaving `rip` of `frame` as it is.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDB_X86_64_BREAKPOINT_TRAP_01}
 *
 * @design
 * 1. Set `pc` to `frame->rip - JA_GDB_BREAKPOINT_SIZE`.
 *    \llrref{LLR_GDB_X86_64_BREAKPOINT_TRAP_01},
 *    \llrref{LLR_GDB_X86_64_BREAKPOINT_TRAP_02},
 *    \llrref{LLR_GDB_X86_64_BREAKPOINT_TRAP_03},
 *    \llrref{LLR_GDB_X86_64_BREAKPOINT_TRAP_04}
 * 2. If `from_user` is false and `pc` is equal to `pok_trap_addr()`:
 *    \llrref{LLR_GDB_X86_64_BREAKPOINT_TRAP_01}
 *    1. Call `jet_gdb_trap()` with the arguments `frame`,
 *       `JET_GDB_STOP_REQUEST` and `pc`, and store the result in `handled`.
 * 3. Otherwise, if `jet_gdb_is_breakpoint(trap_space(from_user), pc)` is true:
 *    \llrref{LLR_GDB_X86_64_BREAKPOINT_TRAP_02},
 *    \llrref{LLR_GDB_X86_64_BREAKPOINT_TRAP_03}
 *    1. Set `frame->rip` to `pc`.
 *    2. Call `jet_gdb_trap()` with the arguments `frame`,
 *       `JET_GDB_STOP_SW_BREAKPOINT` and `pc`, and store the result in
 *       `handled`.
 *    3. If `handled` is false: \llrref{LLR_GDB_X86_64_BREAKPOINT_TRAP_03}
 *       1. Set `frame->rip` to `pc + JA_GDB_BREAKPOINT_SIZE`.
 * 4. Otherwise: \llrref{LLR_GDB_X86_64_BREAKPOINT_TRAP_04}
 *    1. Call `jet_gdb_trap()` with the arguments `frame`, `JET_GDB_STOP_TRAP`
 *       and `pc`, and store the result in `handled`.
 * 5. Return `handled`. \llrref{LLR_GDB_X86_64_BREAKPOINT_TRAP_01},
 *    \llrref{LLR_GDB_X86_64_BREAKPOINT_TRAP_02},
 *    \llrref{LLR_GDB_X86_64_BREAKPOINT_TRAP_04}
 *
 * @calls
 * - `pok_trap_addr()`
 * - `jet_gdb_trap()`
 * - `jet_gdb_is_breakpoint()`
 * - `trap_space()`
 */
static pok_bool_t breakpoint_trap(interrupt_frame* frame,
    pok_bool_t from_user)
{
    uintptr_t pc = frame->rip - JA_GDB_BREAKPOINT_SIZE;
    pok_bool_t handled;

    if (!from_user && (pc == pok_trap_addr()))
    {
        handled = jet_gdb_trap(frame, JET_GDB_STOP_REQUEST, pc);
    }
    else if (jet_gdb_is_breakpoint(trap_space(from_user), pc))
    {
        // GDB expects the PC at the breakpoint it has inserted.
        frame->rip = pc;
        handled = jet_gdb_trap(frame, JET_GDB_STOP_SW_BREAKPOINT, pc);
        if (!handled) {
            frame->rip = pc + JA_GDB_BREAKPOINT_SIZE;
        }
    }
    else
    {
        handled = jet_gdb_trap(frame, JET_GDB_STOP_TRAP, pc);
    }

    return handled;
}

/**
 * @funcname `debug_trap`
 *
 * @description
 * Handles the debug exception: a hit of a debug register (in the space of its
 * slot, else it goes on) or the end of a single step, when interrupts are
 * enabled again as before the step (always for user code).
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `frame` | `interrupt_frame *` | [in,out] The interrupt frame. |
 * | `from_user` | `pok_bool_t` | [in] Whether it came from user space. |
 *
 * @return TRUE if GDB handled it.
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `hw_slots` | `hw_slot_t[4]` | The hardware breakpoints and watchpoints |
 * | `step_enable_interrupts` | `pok_bool_t` | Whether interrupts were disabled for a step of kernel code |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `dr6` | `uint64_t` | DR6 |
 * | `handled` | `pok_bool_t` | Whether GDB handled it |
 * | `hit` | `pok_bool_t` | Whether a debug register hit |
 * | `i` | `uint32_t` | The index of a slot |
 * | `slot` | `const hw_slot_t *` | The slot |
 *
 * @hlr
 * - \hlrid{HLR_GDB_DEBUG_TRAP_01}: `debug_trap()` shall handle the debug
 *   exception: a hit of a hardware breakpoint or watchpoint in its space, which
 *   stops the module (another space goes on), or the end of a single step,
 *   which restores the interrupt flag; and tell whether the debugger handled
 *   it.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_DBG_014}
 *   - \srqref{SRQ_JET_DBG_006}
 *
 * @llr
 * - \llrid{LLR_GDB_DEBUG_TRAP_01}: `debug_trap()` shall call `cpu_read_dr6()`
 *   once and then `cpu_write_dr6()` once with `DR6_CLEAR`.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDB_DEBUG_TRAP_01}
 * - \llrid{LLR_GDB_DEBUG_TRAP_02}: For the first element `i` of `hw_slots`
 *   whose bit `i` is set in the value of DR6, whose `used` is true, and whose
 *   `space_id` is 0, or is the result of `trap_space()` while `from_user` is
 *   true, `debug_trap()` shall return the result of `jet_gdb_trap()` for
 *   `frame`, the result of `hw_stop_reason()` for its `type` and its `addr`,
 *   without looking at the elements after it.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDB_DEBUG_TRAP_01}
 * - \llrid{LLR_GDB_DEBUG_TRAP_03}: For an element `i` of `hw_slots` whose bit
 *   `i` is set in the value of DR6 and whose `used` is true, while its
 *   `space_id` is not 0, and `from_user` is false or its `space_id` is not the
 *   result of `trap_space()`, `debug_trap()` shall take `TRUE` as the result
 *   and go on with the next element.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDB_DEBUG_TRAP_01}
 * - \llrid{LLR_GDB_DEBUG_TRAP_04}: If and only if `type` of an element whose
 *   bit is set in the value of DR6 and whose `used` is true is
 *   `JA_GDB_HW_EXEC`, `debug_trap()` shall set `RFLAGS_RF` in `rflags` of
 *   `frame`.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDB_DEBUG_TRAP_01}
 * - \llrid{LLR_GDB_DEBUG_TRAP_05}: For an element `i` of `hw_slots` whose bit
 *   `i` is not set in the value of DR6 or whose `used` is false, `debug_trap()`
 *   shall leave the result and `frame` as they are.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDB_DEBUG_TRAP_01}
 * - \llrid{LLR_GDB_DEBUG_TRAP_06}: If no element gave a call of
 *   `jet_gdb_trap()` and `DR6_BS` is set in the value of DR6, `debug_trap()`
 *   shall clear `RFLAGS_TF` in `rflags` of `frame`, set
 *   `step_enable_interrupts` to `FALSE`, and return the result of
 *   `jet_gdb_trap()` for `frame`, `JET_GDB_STOP_STEP` and `rip` of `frame`.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDB_DEBUG_TRAP_01}
 * - \llrid{LLR_GDB_DEBUG_TRAP_07}: In that case, if and only if `from_user` is
 *   true or `step_enable_interrupts` is true, `debug_trap()` shall set
 *   `RFLAGS_IF` in `rflags` of `frame` before it sets `step_enable_interrupts`
 *   to `FALSE`.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDB_DEBUG_TRAP_01}
 * - \llrid{LLR_GDB_DEBUG_TRAP_08}: If no element gave a call of
 *   `jet_gdb_trap()` and `DR6_BS` is not set in the value of DR6,
 *   `debug_trap()` shall return the result taken so far (`FALSE` if no used
 *   element had its bit set in the value of DR6), leaving
 *   `step_enable_interrupts` as it is.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDB_DEBUG_TRAP_01}
 *
 * @design
 * 1. Call `cpu_read_dr6()`, and store the result in `dr6`.
 *    \llrref{LLR_GDB_DEBUG_TRAP_01}
 * 2. Set `handled` to `FALSE`. \llrref{LLR_GDB_DEBUG_TRAP_08}
 * 3. Set `hit` to `FALSE`. \llrref{LLR_GDB_DEBUG_TRAP_02},
 *    \llrref{LLR_GDB_DEBUG_TRAP_06}
 * 4. Call `cpu_write_dr6()` with the argument `DR6_CLEAR`.
 *    \llrref{LLR_GDB_DEBUG_TRAP_01}
 * 5. Set `i` to `0U`; then while `i` is less than `JA_GDB_HW_SLOTS` and `hit`
 *    is false, repeat (after each pass: increment `i`):
 *    \llrref{LLR_GDB_DEBUG_TRAP_02}, \llrref{LLR_GDB_DEBUG_TRAP_03},
 *    \llrref{LLR_GDB_DEBUG_TRAP_04}, \llrref{LLR_GDB_DEBUG_TRAP_05}
 *    1. Set `slot` to the address of `hw_slots[i]`.
 *    2. If `(dr6 & ((uint64_t)1U << i))` is not equal to `0U` and `slot->used`
 *       is true:
 *       1. If `slot->type` is equal to `JA_GDB_HW_EXEC`:
 *          \llrref{LLR_GDB_DEBUG_TRAP_04}
 *          1. Set the bits `RFLAGS_RF` in `frame->rflags`.
 *       2. If `slot->space_id` is equal to `0U` or (`from_user` is true and
 *          `slot->space_id` is equal to `trap_space(from_user)`):
 *          \llrref{LLR_GDB_DEBUG_TRAP_02}
 *          1. Set `hit` to `TRUE`.
 *          2. Call `jet_gdb_trap()` with the arguments `frame`,
 *             `hw_stop_reason(slot->type)` and `slot->addr`, and store the
 *             result in `handled`.
 *       3. Otherwise: \llrref{LLR_GDB_DEBUG_TRAP_03}
 *          1. Set `handled` to `TRUE`.
 * 6. If `hit` is false and `(dr6 & DR6_BS)` is not equal to `0U`:
 *    \llrref{LLR_GDB_DEBUG_TRAP_06}, \llrref{LLR_GDB_DEBUG_TRAP_07},
 *    \llrref{LLR_GDB_DEBUG_TRAP_08}
 *    1. Keep only the bits `~(uint64_t)RFLAGS_TF` of `frame->rflags`.
 *       \llrref{LLR_GDB_DEBUG_TRAP_06}
 *    2. If `from_user` is true or `step_enable_interrupts` is true:
 *       \llrref{LLR_GDB_DEBUG_TRAP_07}
 *       1. Set the bits `RFLAGS_IF` in `frame->rflags`.
 *    3. Set `step_enable_interrupts` to `FALSE`. \llrref{LLR_GDB_DEBUG_TRAP_06}
 *    4. Call `jet_gdb_trap()` with the arguments `frame`, `JET_GDB_STOP_STEP`
 *       and `frame->rip`, and store the result in `handled`.
 *       \llrref{LLR_GDB_DEBUG_TRAP_06}
 * 7. Return `handled`. \llrref{LLR_GDB_DEBUG_TRAP_02},
 *    \llrref{LLR_GDB_DEBUG_TRAP_03}, \llrref{LLR_GDB_DEBUG_TRAP_06},
 *    \llrref{LLR_GDB_DEBUG_TRAP_08}
 *
 * @calls
 * - `cpu_read_dr6()`
 * - `cpu_write_dr6()`
 * - `trap_space()`
 * - `jet_gdb_trap()`
 * - `hw_stop_reason()`
 */
static pok_bool_t debug_trap(interrupt_frame* frame, pok_bool_t from_user)
{
    uint64_t dr6 = cpu_read_dr6();
    pok_bool_t handled = FALSE;
    pok_bool_t hit = FALSE;

    cpu_write_dr6(DR6_CLEAR);

    for (uint32_t i = 0U; (i < JA_GDB_HW_SLOTS) && !hit; i++)
    {
        const hw_slot_t* slot = &hw_slots[i];

        if (((dr6 & ((uint64_t)1U << i)) != 0U) && slot->used)
        {
            if (slot->type == JA_GDB_HW_EXEC) {
                // An instruction breakpoint is a fault: let it run.
                frame->rflags |= RFLAGS_RF;
            }

            // All partitions use the same virtual addresses.
            if ((slot->space_id == 0U)
                || (from_user && (slot->space_id == trap_space(from_user))))
            {
                hit = TRUE;
                handled = jet_gdb_trap(frame, hw_stop_reason(slot->type),
                    slot->addr);
            }
            else
            {
                handled = TRUE; // Another partition: go on.
            }
        }
    }

    if (!hit && ((dr6 & DR6_BS) != 0U))
    {
        frame->rflags &= ~(uint64_t)RFLAGS_TF;

        /*
         * Enable interrupts again after a step (ja_gdb_single_step()):
         * user code always runs with them enabled (a step which GDB has
         * given up may end later, in another stop), kernel code as before
         * the step.
         */
        if (from_user || step_enable_interrupts) {
            frame->rflags |= RFLAGS_IF;
        }
        step_enable_interrupts = FALSE;

        handled = jet_gdb_trap(frame, JET_GDB_STOP_STEP, frame->rip);
    }

    return handled;
}

/**
 * @funcname `ja_gdb_trap`
 *
 * @description
 * Handles a GDB exception (from ja_interrupt_handler()): the breakpoint or the
 * debug exception.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `frame` | `interrupt_frame *` | [in,out] The interrupt frame. |
 * | `vector` | `uint32_t` | [in] The vector. |
 * | `from_user` | `pok_bool_t` | [in] Whether it came from user space. |
 *
 * @return TRUE if GDB handled it.
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `handled` | `pok_bool_t` | Whether GDB handled it |
 *
 * @hlr
 * - \hlrid{HLR_GDB_X86_64_JA_GDB_TRAP_01}: `ja_gdb_trap()` shall handle an
 *   exception of the debugger and tell whether it did.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_DBG_014}
 *   - \srqref{SRQ_JET_DBG_006}
 *
 * @llr
 * - \llrid{LLR_GDB_X86_64_JA_GDB_TRAP_01}: If `vector` is
 *   `EXCEPTION_BREAKPOINT`, `ja_gdb_trap()` shall return the result of
 *   `breakpoint_trap()` for `frame` and `from_user`.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDB_X86_64_JA_GDB_TRAP_01}
 * - \llrid{LLR_GDB_X86_64_JA_GDB_TRAP_02}: If `vector` is not
 *   `EXCEPTION_BREAKPOINT`, `ja_gdb_trap()` shall return the result of
 *   `debug_trap()` for `frame` and `from_user`.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDB_X86_64_JA_GDB_TRAP_01}
 *
 * @design
 * 1. If `vector` is equal to `EXCEPTION_BREAKPOINT`:
 *    \llrref{LLR_GDB_X86_64_JA_GDB_TRAP_01}
 *    1. Call `breakpoint_trap()` with the arguments `frame` and `from_user`,
 *       and store the result in `handled`.
 * 2. Otherwise: \llrref{LLR_GDB_X86_64_JA_GDB_TRAP_02}
 *    1. Call `debug_trap()` with the arguments `frame` and `from_user`, and
 *       store the result in `handled`.
 * 3. Return `handled`. \llrref{LLR_GDB_X86_64_JA_GDB_TRAP_01},
 *    \llrref{LLR_GDB_X86_64_JA_GDB_TRAP_02}
 *
 * @calls
 * - `breakpoint_trap()`
 * - `debug_trap()`
 */
pok_bool_t ja_gdb_trap(interrupt_frame* frame, uint32_t vector,
    pok_bool_t from_user)
{
    pok_bool_t handled;

    if (vector == EXCEPTION_BREAKPOINT) {
        handled = breakpoint_trap(frame, from_user);
    } else {
        handled = debug_trap(frame, from_user);
    }

    return handled;
}

#endif /* POK_NEEDS_GDB */
