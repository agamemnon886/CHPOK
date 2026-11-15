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
 *
 * @llr
 *
 * @design
 * 1. Execute the assembly code `mov %%%dr6, %0`.
 * 2. Return `value`.
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
 *
 * @llr
 *
 * @design
 * 1. Execute the assembly code `mov %0, %%%dr6`.
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
 *
 * @llr
 *
 * @design
 * 1. Execute the assembly code `mov %0, %%%dr7`.
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
 *
 * @llr
 *
 * @design
 * 1. Depending on `index`:
 *    1. Case `0U`:
 *       1. Execute the assembly code `mov %0, %%%dr0`.
 *       2. Leave the switch.
 *    2. Case `1U`:
 *       1. Execute the assembly code `mov %0, %%%dr1`.
 *       2. Leave the switch.
 *    3. Case `2U`:
 *       1. Execute the assembly code `mov %0, %%%dr2`.
 *       2. Leave the switch.
 *    4. Case `3U`:
 *       1. Execute the assembly code `mov %0, %%%dr3`.
 *       2. Leave the switch.
 *    5. Otherwise (default):
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
 *
 * @llr
 *
 * @design
 * 1. Set `i` to `0U`; then while `i` is less than `size`, repeat (after each
 *    pass: increment `i`):
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
 *
 * @llr
 *
 * @design
 * 1. Set `value` to `0U`.
 * 2. Set `i` to `0U`; then while `i` is less than `size`, repeat (after each
 *    pass: increment `i`):
 *    1. Set the bits `(uint64_t)in[offset + i] << (8U * i)` in `value`.
 * 3. Return `value`.
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
 *
 * @llr
 *
 * @design
 * 1. If `regno` is less than or equal to `REG_RIP`:
 *    1. Set `size` to `8U`.
 * 2. Otherwise, if `regno` is less than `REG_ST0`:
 *    1. Set `size` to `4U`.
 * 3. Otherwise, if `regno` is less than `REG_FCTRL`:
 *    1. Set `size` to `ST_SIZE`.
 * 4. Otherwise, if `regno` is less than `REG_XMM0`:
 *    1. Set `size` to `4U`.
 * 5. Otherwise, if `regno` is less than `REG_MXCSR`:
 *    1. Set `size` to `16U`.
 * 6. Otherwise, if `regno` is equal to `REG_MXCSR`:
 *    1. Set `size` to `4U`.
 * 7. Otherwise:
 *    1. Set `size` to `0U`.
 * 8. Return `size`.
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
 *
 * @llr
 *
 * @design
 * 1. Depending on `regno`:
 *    1. Case `0U`:
 *       1. Set `value` to `frame->rax`.
 *       2. Leave the switch.
 *    2. Case `1U`:
 *       1. Set `value` to `frame->rbx`.
 *       2. Leave the switch.
 *    3. Case `2U`:
 *       1. Set `value` to `frame->rcx`.
 *       2. Leave the switch.
 *    4. Case `3U`:
 *       1. Set `value` to `frame->rdx`.
 *       2. Leave the switch.
 *    5. Case `4U`:
 *       1. Set `value` to `frame->rsi`.
 *       2. Leave the switch.
 *    6. Case `5U`:
 *       1. Set `value` to `frame->rdi`.
 *       2. Leave the switch.
 *    7. Case `6U`:
 *       1. Set `value` to `frame->rbp`.
 *       2. Leave the switch.
 *    8. Case `7U`:
 *       1. Set `value` to `frame->rsp`.
 *       2. Leave the switch.
 *    9. Case `8U`:
 *       1. Set `value` to `frame->r8`.
 *       2. Leave the switch.
 *    10. Case `9U`:
 *        1. Set `value` to `frame->r9`.
 *        2. Leave the switch.
 *    11. Case `10U`:
 *        1. Set `value` to `frame->r10`.
 *        2. Leave the switch.
 *    12. Case `11U`:
 *        1. Set `value` to `frame->r11`.
 *        2. Leave the switch.
 *    13. Case `12U`:
 *        1. Set `value` to `frame->r12`.
 *        2. Leave the switch.
 *    14. Case `13U`:
 *        1. Set `value` to `frame->r13`.
 *        2. Leave the switch.
 *    15. Case `14U`:
 *        1. Set `value` to `frame->r14`.
 *        2. Leave the switch.
 *    16. Case `15U`:
 *        1. Set `value` to `frame->r15`.
 *        2. Leave the switch.
 *    17. Case `16U`:
 *        1. Set `value` to `frame->rip`.
 *        2. Leave the switch.
 *    18. Case `17U`:
 *        1. Set `value` to `frame->rflags`.
 *        2. Leave the switch.
 *    19. Case `18U`:
 *        1. Set `value` to `frame->cs`.
 *        2. Leave the switch.
 *    20. Case `19U`:
 *        1. Set `value` to `frame->ss`.
 *        2. Leave the switch.
 *    21. Otherwise (default):
 *        1. Set `value` to `0U`.
 *        2. Leave the switch.
 * 2. Return `value`.
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
 *
 * @llr
 *
 * @design
 * 1. Depending on `regno`:
 *    1. Case `0U`:
 *       1. Set `frame->rax` to `value`.
 *       2. Leave the switch.
 *    2. Case `1U`:
 *       1. Set `frame->rbx` to `value`.
 *       2. Leave the switch.
 *    3. Case `2U`:
 *       1. Set `frame->rcx` to `value`.
 *       2. Leave the switch.
 *    4. Case `3U`:
 *       1. Set `frame->rdx` to `value`.
 *       2. Leave the switch.
 *    5. Case `4U`:
 *       1. Set `frame->rsi` to `value`.
 *       2. Leave the switch.
 *    6. Case `5U`:
 *       1. Set `frame->rdi` to `value`.
 *       2. Leave the switch.
 *    7. Case `6U`:
 *       1. Set `frame->rbp` to `value`.
 *       2. Leave the switch.
 *    8. Case `7U`:
 *       1. Set `frame->rsp` to `value`.
 *       2. Leave the switch.
 *    9. Case `8U`:
 *       1. Set `frame->r8` to `value`.
 *       2. Leave the switch.
 *    10. Case `9U`:
 *        1. Set `frame->r9` to `value`.
 *        2. Leave the switch.
 *    11. Case `10U`:
 *        1. Set `frame->r10` to `value`.
 *        2. Leave the switch.
 *    12. Case `11U`:
 *        1. Set `frame->r11` to `value`.
 *        2. Leave the switch.
 *    13. Case `12U`:
 *        1. Set `frame->r12` to `value`.
 *        2. Leave the switch.
 *    14. Case `13U`:
 *        1. Set `frame->r13` to `value`.
 *        2. Leave the switch.
 *    15. Case `14U`:
 *        1. Set `frame->r14` to `value`.
 *        2. Leave the switch.
 *    16. Case `15U`:
 *        1. Set `frame->r15` to `value`.
 *        2. Leave the switch.
 *    17. Case `16U`:
 *        1. Set `frame->rip` to `value`.
 *        2. Leave the switch.
 *    18. Case `17U`:
 *        1. Set `frame->rflags` to
 *           `(frame->rflags & ~(uint64_t)RFLAGS_GDB_WRITABLE) | (value &`
 *           `RFLAGS_GDB_WRITABLE)`.
 *        2. Leave the switch.
 *    19. Otherwise (default):
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
 *
 * @llr
 *
 * @design
 * 1. Set `offset` to `FX_ST0 + (st * FX_STRIDE)`.
 * 2. Set `exponent` to
 *    `(((uint32_t)fx[offset + 9U] & 0x7FU) << 8) | (uint32_t)fx[offset + 8U]`.
 * 3. Set `integer` to `((uint32_t)fx[offset + 7U] & 0x80U) != 0U`.
 * 4. Set `fraction_zero` to `((uint32_t)fx[offset + 7U] & 0x7FU) == 0U`.
 * 5. Set `i` to `0U`; then while `i` is less than `7U`, repeat (after each
 *    pass: increment `i`):
 *    1. If `fx[offset + i]` is not equal to `0U`:
 *       1. Set `fraction_zero` to `FALSE`.
 * 6. If `exponent` is equal to `0x7FFFU`:
 *    1. Set `tag` to `FP_TAG_SPECIAL`.
 * 7. Otherwise, if `exponent` is equal to `0U`:
 *    1. Set `tag` to `FP_TAG_ZERO` if (`fraction_zero` is true and `integer` is
 *       false), otherwise `FP_TAG_SPECIAL`.
 * 8. Otherwise:
 *    1. Set `tag` to `FP_TAG_VALID` if `integer` is true, otherwise
 *       `FP_TAG_SPECIAL`.
 * 9. Return `tag`.
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
 *
 * @llr
 *
 * @design
 * 1. Set `top` to `((uint32_t)fx[FX_FSW + 1U] >> 3) & 7U`.
 * 2. Set `abridged` to `fx[FX_FTW]`.
 * 3. Set `full` to `0U`.
 * 4. Set `phys` to `0U`; then while `phys` is less than `8U`, repeat (after
 *    each pass: increment `phys`):
 *    1. Set `tag` to `FP_TAG_EMPTY`.
 *    2. If `(abridged & (1U << phys))` is not equal to `0U`:
 *       1. Call `fp_tag()` with the arguments `fx` and
 *          `(phys + 8U - top) % 8U`, and store the result in `tag`.
 *    3. Set the bits `tag << (2U * phys)` in `full`.
 * 5. Return `full`.
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
 *
 * @llr
 *
 * @design
 * 1. Set `abridged` to `0U`.
 * 2. Set `phys` to `0U`; then while `phys` is less than `8U`, repeat (after
 *    each pass: increment `phys`):
 *    1. Set `tag` to `full >> (2U * phys)`.
 *    2. If `(tag & 3U)` is not equal to `FP_TAG_EMPTY`:
 *       1. Set the bits `1U << phys` in `abridged`.
 * 3. Return `(uint8_t)abridged`.
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
 *
 * @llr
 *
 * @design
 * 1. Set `i` to `0U`; then while `i` is less than `size`, repeat (after each
 *    pass: increment `i`):
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
 *
 * @llr
 *
 * @design
 * 1. Set `fx` to `fp->data`.
 * 2. If `regno` is less than `REG_FCTRL`:
 *    1. Call `copy_bytes()` with the arguments `value`, `0U`, `fx`,
 *       `FX_ST0 + ((regno - REG_ST0) * FX_STRIDE)` and `ST_SIZE`.
 * 3. Otherwise, if `regno` is greater than or equal to `REG_XMM0` and `regno`
 *    is less than `REG_MXCSR`:
 *    1. Call `copy_bytes()` with the arguments `value`, `0U`, `fx`,
 *       `FX_XMM0 + ((regno - REG_XMM0) * FX_STRIDE)` and `FX_STRIDE`.
 * 4. Otherwise:
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
 *
 * @llr
 *
 * @design
 * 1. Set `fx` to `fp->data`.
 * 2. If `regno` is less than `REG_FCTRL`:
 *    1. Call `copy_bytes()` with the arguments `fx`,
 *       `FX_ST0 + ((regno - REG_ST0) * FX_STRIDE)`, `value`, `0U` and
 *       `ST_SIZE`.
 * 3. Otherwise, if `regno` is greater than or equal to `REG_XMM0` and `regno`
 *    is less than `REG_MXCSR`:
 *    1. Call `copy_bytes()` with the arguments `fx`,
 *       `FX_XMM0 + ((regno - REG_XMM0) * FX_STRIDE)`, `value`, `0U` and
 *       `FX_STRIDE`.
 * 4. Otherwise:
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
 *
 * @llr
 *
 * @design
 * 1. Set `available` to `FALSE`.
 * 2. Call `ja_gdb_register_size()` with the argument `regno`, and store the
 *    result in `size`.
 * 3. If `regno` is less than or equal to `REG_SS`:
 *    1. If `ctx` is not NULL:
 *       1. Call `put_le()` with the arguments `frame_register_get(ctx, regno)`,
 *          `size` and `value`.
 *       2. Set `available` to `TRUE`.
 * 4. Otherwise, if `regno` is less than `REG_ST0`:
 *    1. Do nothing.
 * 5. Otherwise, if `fp` is not NULL and `size` is not equal to `0U`:
 *    1. Call `fp_register_get()` with the arguments `fp`, `regno` and `value`.
 *    2. Set `available` to `TRUE`.
 * 6. Otherwise:
 *    1. Do nothing.
 * 7. Return `available`.
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
 *
 * @llr
 *
 * @design
 * 1. Set `written` to `FALSE`.
 * 2. Call `ja_gdb_register_size()` with the argument `regno`, and store the
 *    result in `size`.
 * 3. If `regno` is less than or equal to `REG_EFLAGS`:
 *    1. If `ctx` is not NULL:
 *       1. Call `frame_register_set()` with the arguments `ctx`, `regno` and
 *          `get_le(value, 0U, size)`.
 *       2. Set `written` to `TRUE`.
 * 4. Otherwise, if `regno` is less than `REG_ST0`:
 *    1. Do nothing.
 * 5. Otherwise, if `fp` is not NULL and `size` is not equal to `0U`:
 *    1. Call `fp_register_set()` with the arguments `fp`, `regno` and `value`.
 *    2. Set `written` to `TRUE`.
 * 6. Otherwise:
 *    1. Do nothing.
 * 7. Return `written`.
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
 *
 * @llr
 *
 * @design
 * 1. Return `ctx->rip`.
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
 *
 * @llr
 *
 * @design
 * 1. Set `ctx->rip` to `pc`.
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
 *
 * @llr
 *
 * @design
 * 1. Set `space_id` to `ja_space_get_current()` if
 *    `ja_gdb_context_is_user(ctx)` is true, otherwise `0U`.
 * 2. Call `ja_addr_to_gdb_ro()` with the arguments `ctx->rip`, `2U` and
 *    `space_id`, and store the result in `code`.
 * 3. Set `changes` to `TRUE`.
 * 4. If `code` is not NULL:
 *    1. Set `opcode` to `code[0]`.
 *    2. If (`opcode` is greater than or equal to `0x40U` and `opcode` is less
 *       than or equal to `0x4FU`) or `opcode` is equal to `0x66U`:
 *       1. Set `opcode` to `code[1]`.
 *    3. Set `changes` to
 *       `(opcode == 0xFAU) || (opcode == 0xFBU) || (opcode == 0x9DU) ||`
 *       `(opcode == 0xCFU)`.
 * 5. Return `changes`.
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
 *
 * @llr
 *
 * @design
 * 1. If `enable` is true:
 *    1. Set the bits `RFLAGS_TF` in `ctx->rflags`.
 *    2. Set `step_enable_interrupts` to `FALSE`.
 *    3. If `(ctx->rflags & RFLAGS_IF)` is not equal to `0U` and
 *       `insn_changes_if(ctx)` is false:
 *       1. Keep only the bits `~(uint64_t)RFLAGS_IF` of `ctx->rflags`.
 *       2. Set `step_enable_interrupts` to `!ja_gdb_context_is_user(ctx)`.
 * 2. Otherwise:
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
 *
 * @llr
 *
 * @design
 * 1. Return `(ctx->cs & 3U) == 3U`.
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
 *
 * @llr
 *
 * @design
 * 1. Mark `addr` as not used.
 * 2. Mark `size` as not used.
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
 *
 * @llr
 *
 * @design
 * 1. If `type` is equal to `JA_GDB_HW_EXEC`:
 *    1. Set `supported` to `(len == 1U)`.
 * 2. Otherwise:
 *    1. Set `supported` to
 *       `((len == 1U) || (len == 2U) || (len == 4U) || (len == 8U)) && ((addr`
 *       `% len) == 0U)`.
 * 3. Return `supported`.
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
 *
 * @llr
 *
 * @design
 * 1. Return
 *    `slot->used && (slot->type == type) && (slot->addr == addr) && (slot->len`
 *    `== len) && (slot->space_id == space_id)`.
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
 *
 * @llr
 *
 * @design
 * 1. Set `inserted` to `FALSE`.
 * 2. If `hw_area_supported(type, addr, len)` is true:
 *    1. Set `i` to `0U`; then while `i` is less than `JA_GDB_HW_SLOTS`, repeat
 *       (after each pass: increment `i`):
 *       1. If `hw_slot_is(&hw_slots[i], type, addr, len, space_id)` is true:
 *          1. Set `inserted` to `TRUE`.
 *    2. Set `i` to `0U`; then while `i` is less than `JA_GDB_HW_SLOTS` and
 *       `inserted` is false, repeat (after each pass: increment `i`):
 *       1. If `hw_slots[i].used` is false:
 *          1. Set `hw_slots[i].used` to `TRUE`.
 *          2. Set `hw_slots[i].type` to `type`.
 *          3. Set `hw_slots[i].addr` to `addr`.
 *          4. Set `hw_slots[i].len` to `len`.
 *          5. Set `hw_slots[i].space_id` to `space_id`.
 *          6. Set `inserted` to `TRUE`.
 * 3. Return `inserted`.
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
 *
 * @llr
 *
 * @design
 * 1. Set `removed` to `FALSE`.
 * 2. Set `i` to `0U`; then while `i` is less than `JA_GDB_HW_SLOTS`, repeat
 *    (after each pass: increment `i`):
 *    1. If `hw_slot_is(&hw_slots[i], type, addr, len, space_id)` is true:
 *       1. Set `hw_slots[i].used` to `FALSE`.
 *       2. Set `removed` to `TRUE`.
 * 3. Return `removed`.
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
 *
 * @llr
 *
 * @design
 * 1. Set `i` to `0U`; then while `i` is less than `JA_GDB_HW_SLOTS`, repeat
 *    (after each pass: increment `i`):
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
 *
 * @llr
 *
 * @design
 * 1. Call `cpu_write_dr7()` with the argument `0U`.
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
 *
 * @llr
 *
 * @design
 * 1. Depending on `slot->type`:
 *    1. Case `JA_GDB_HW_EXEC`:
 *       1. Set `rw` to `0U`.
 *       2. Leave the switch.
 *    2. Case `JA_GDB_HW_WRITE`:
 *       1. Set `rw` to `1U`.
 *       2. Leave the switch.
 *    3. Otherwise (default):
 *       1. Set `rw` to `3U`.
 *       2. Leave the switch.
 * 2. Depending on `slot->len`:
 *    1. Case `2U`:
 *       1. Set `len` to `1U`.
 *       2. Leave the switch.
 *    2. Case `4U`:
 *       1. Set `len` to `3U`.
 *       2. Leave the switch.
 *    3. Case `8U`:
 *       1. Set `len` to `2U`.
 *       2. Leave the switch.
 *    4. Otherwise (default):
 *       1. Set `len` to `0U`.
 *       2. Leave the switch.
 * 3. Set `enable_shift` to `2U * index`.
 * 4. Set `rw_shift` to `16U + (4U * index)`.
 * 5. Set `len_shift` to `18U + (4U * index)`.
 * 6. Set `enable` to `(uint64_t)1U << enable_shift`.
 * 7. Return `enable | (rw << rw_shift) | (len << len_shift)`.
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
 *
 * @llr
 *
 * @design
 * 1. Set `dr7` to `0U`.
 * 2. Set `i` to `0U`; then while `i` is less than `JA_GDB_HW_SLOTS`, repeat
 *    (after each pass: increment `i`):
 *    1. If `hw_slots[i].used` is true:
 *       1. Call `cpu_write_dr_addr()` with the arguments `i` and
 *          `hw_slots[i].addr`.
 *       2. Set the bits `hw_dr7_bits(&hw_slots[i], i)` in `dr7`.
 * 3. If `dr7` is not equal to `0U`:
 *    1. Set the bits `DR7_LE` in `dr7`.
 * 4. Call `cpu_write_dr7()` with the argument `dr7`.
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
 *
 * @llr
 *
 * @design
 * 1. Depending on `type`:
 *    1. Case `JA_GDB_HW_EXEC`:
 *       1. Set `reason` to `JET_GDB_STOP_HW_BREAKPOINT`.
 *       2. Leave the switch.
 *    2. Case `JA_GDB_HW_WRITE`:
 *       1. Set `reason` to `JET_GDB_STOP_WATCH`.
 *       2. Leave the switch.
 *    3. Case `JA_GDB_HW_READ`:
 *       1. Set `reason` to `JET_GDB_STOP_RWATCH`.
 *       2. Leave the switch.
 *    4. Otherwise (default):
 *       1. Set `reason` to `JET_GDB_STOP_AWATCH`.
 *       2. Leave the switch.
 * 2. Return `reason`.
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
 *
 * @llr
 *
 * @design
 * 1. Return `(uintptr_t)&pok_trap`.
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
 *
 * @llr
 *
 * @design
 * 1. Return `ja_space_get_current()` if `from_user` is true, otherwise `0U`.
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
 *
 * @llr
 *
 * @design
 * 1. Set `pc` to `frame->rip - JA_GDB_BREAKPOINT_SIZE`.
 * 2. If `from_user` is false and `pc` is equal to `pok_trap_addr()`:
 *    1. Call `jet_gdb_trap()` with the arguments `frame`,
 *       `JET_GDB_STOP_REQUEST` and `pc`, and store the result in `handled`.
 * 3. Otherwise, if `jet_gdb_is_breakpoint(trap_space(from_user), pc)` is true:
 *    1. Set `frame->rip` to `pc`.
 *    2. Call `jet_gdb_trap()` with the arguments `frame`,
 *       `JET_GDB_STOP_SW_BREAKPOINT` and `pc`, and store the result in
 *       `handled`.
 *    3. If `handled` is false:
 *       1. Set `frame->rip` to `pc + JA_GDB_BREAKPOINT_SIZE`.
 * 4. Otherwise:
 *    1. Call `jet_gdb_trap()` with the arguments `frame`, `JET_GDB_STOP_TRAP`
 *       and `pc`, and store the result in `handled`.
 * 5. Return `handled`.
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
 *
 * @llr
 *
 * @design
 * 1. Call `cpu_read_dr6()`, and store the result in `dr6`.
 * 2. Set `handled` to `FALSE`.
 * 3. Set `hit` to `FALSE`.
 * 4. Call `cpu_write_dr6()` with the argument `DR6_CLEAR`.
 * 5. Set `i` to `0U`; then while `i` is less than `JA_GDB_HW_SLOTS` and `hit`
 *    is false, repeat (after each pass: increment `i`):
 *    1. Set `slot` to the address of `hw_slots[i]`.
 *    2. If `(dr6 & ((uint64_t)1U << i))` is not equal to `0U` and `slot->used`
 *       is true:
 *       1. If `slot->type` is equal to `JA_GDB_HW_EXEC`:
 *          1. Set the bits `RFLAGS_RF` in `frame->rflags`.
 *       2. If `slot->space_id` is equal to `0U` or (`from_user` is true and
 *          `slot->space_id` is equal to `trap_space(from_user)`):
 *          1. Set `hit` to `TRUE`.
 *          2. Call `jet_gdb_trap()` with the arguments `frame`,
 *             `hw_stop_reason(slot->type)` and `slot->addr`, and store the
 *             result in `handled`.
 *       3. Otherwise:
 *          1. Set `handled` to `TRUE`.
 * 6. If `hit` is false and `(dr6 & DR6_BS)` is not equal to `0U`:
 *    1. Keep only the bits `~(uint64_t)RFLAGS_TF` of `frame->rflags`.
 *    2. If `from_user` is true or `step_enable_interrupts` is true:
 *       1. Set the bits `RFLAGS_IF` in `frame->rflags`.
 *    3. Set `step_enable_interrupts` to `FALSE`.
 *    4. Call `jet_gdb_trap()` with the arguments `frame`, `JET_GDB_STOP_STEP`
 *       and `frame->rip`, and store the result in `handled`.
 * 7. Return `handled`.
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
 *
 * @llr
 *
 * @design
 * 1. If `vector` is equal to `EXCEPTION_BREAKPOINT`:
 *    1. Call `breakpoint_trap()` with the arguments `frame` and `from_user`,
 *       and store the result in `handled`.
 * 2. Otherwise:
 *    1. Call `debug_trap()` with the arguments `frame` and `from_user`, and
 *       store the result in `handled`.
 * 3. Return `handled`.
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
