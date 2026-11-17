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
 * Modified by Kirill Brazhnikov, 2026-10-31: ported from
 * kernel/arch/x86/uaccess.c to x86_64 (64-bit).
 *
 * Modified by Kirill Brazhnikov, 2026-10-31: built-in integer types (int,
 * unsigned, short, long, ...) are replaced with the fixed-width types
 * from <stdint.h>.
 *
 * Modified by Kirill Brazhnikov, 2026-11-02: MISRA C:2025: __user/__kuser
 * are renamed JET_USER/JET_KUSER; braces around all if/else/loop bodies;
 * essentially Boolean conditions; U suffixes for unsigned constants;
 * deviations are documented where a guideline can't be followed.
 *
 * Modified by Kirill Brazhnikov, 2026-11-03: MISRA C:2025: operands of
 * mixed precedence are parenthesized. Long lines are wrapped at 80 columns.
 *
 * Modified by Kirill Brazhnikov, 2026-11-04: ja_addr_to_gdb() and
 * ja_addr_to_gdb_ro() for the GDB server (ported from
 * kernel/arch/x86/uaccess.c): a user space gives access only to its own
 * memory, the kernel to the memory below the partitions.
 *
 * Modified by Kirill Brazhnikov, 2026-11-08: Doxygen headers for the file
 * and its functions.
 *
 * Modified by Kirill Brazhnikov, 2026-11-13: ja_x86_64_space_t is used by its
 * typedef name.
 *
 * Modified by Kirill Brazhnikov, 2026-11-15: moved from
 * kernel/arch/x86_64/uaccess.c (the CPU support packages are in kernel/csps).
 *
 * Modified by Kirill Brazhnikov, 2026-11-16: high and low level requirements of
 * the functions, traced to the system requirements (docs/SRQs), and the steps
 * of their design linked to the LLRs.
 *
 * Modified by Kirill Brazhnikov, 2026-11-17: low level requirements of one
 * behaviour each; code that compiles to nothing (assertions, (void) x;,
 * empty branches) is left out of the design; the high level requirements
 * trace to the system requirements split into requirements of one "shall"
 * each.
 */

/**
 * @file
 * Access to user memory on x86_64: checks of user addresses and their
 * translation to kernel addresses, also for GDB.
 */

#include <asp/uaccess.h>
#include <assert.h>
#include "space.h"

// TODO: Revisit (see also pok_space_* code in arch.h)

/**
 * @funcname `x86_64_check_access`
 *
 * @description
 * Checks that a user area is inside a space's code, data and heap, or inside
 * its stacks.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `addr` | `const void * JET_USER` | [in] The user address. |
 * | `size` | `size_t` | [in] The size (not 0). |
 * | `space_id` | `jet_space_id` | [in] The space (not 0). |
 *
 * @return TRUE if the area is accessible.
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `ja_spaces` | `ja_x86_64_space_t[]` | The memory spaces of the partitions |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `end` | `uintptr_t` | The end of the area |
 * | `space` | `ja_x86_64_space_t *` | The space |
 * | `start` | `uintptr_t` | The start of the area |
 *
 * @hlr
 * - \hlrid{HLR_UACCESS_X86_64_CHECK_ACCESS_01}: `x86_64_check_access()` shall
 *   tell whether an area of a user space lies in its code, data and heap, or in
 *   its stacks.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_MEM_015}
 *
 * @llr
 * - \llrid{LLR_UACCESS_X86_64_CHECK_ACCESS_01}: If `addr` plus `size` is less
 *   than `addr`, `x86_64_check_access()` shall return `FALSE`.
 *   <br>Traces to:
 *   - \hlrref{HLR_UACCESS_X86_64_CHECK_ACCESS_01}
 * - \llrid{LLR_UACCESS_X86_64_CHECK_ACCESS_02}: If `addr` plus `size` is not
 *   less than `addr` and is not greater than `POK_PARTITION_MEMORY_BASE` plus
 *   `heap_end` of `ja_spaces[space_id - 1]`, and `addr` is not less than
 *   `POK_PARTITION_MEMORY_BASE`, `x86_64_check_access()` shall return `TRUE`.
 *   <br>Traces to:
 *   - \hlrref{HLR_UACCESS_X86_64_CHECK_ACCESS_01}
 * - \llrid{LLR_UACCESS_X86_64_CHECK_ACCESS_03}: If `addr` plus `size` is not
 *   less than `addr` and is not greater than `POK_PARTITION_MEMORY_BASE` plus
 *   `heap_end` of the space, and `addr` is less than
 *   `POK_PARTITION_MEMORY_BASE`, `x86_64_check_access()` shall return `FALSE`.
 *   <br>Traces to:
 *   - \hlrref{HLR_UACCESS_X86_64_CHECK_ACCESS_01}
 * - \llrid{LLR_UACCESS_X86_64_CHECK_ACCESS_04}: If `addr` plus `size` is not
 *   less than `addr`, is greater than `POK_PARTITION_MEMORY_BASE` plus
 *   `heap_end` of the space and is not greater than `POK_PARTITION_MEMORY_BASE`
 *   plus `size_total` of the space, and `addr` is not less than
 *   `POK_PARTITION_MEMORY_BASE` plus `size_total` minus `size_stack_used` of
 *   the space, `x86_64_check_access()` shall return `TRUE`.
 *   <br>Traces to:
 *   - \hlrref{HLR_UACCESS_X86_64_CHECK_ACCESS_01}
 * - \llrid{LLR_UACCESS_X86_64_CHECK_ACCESS_05}: If `addr` plus `size` is not
 *   less than `addr`, is greater than `POK_PARTITION_MEMORY_BASE` plus
 *   `heap_end` of the space and is not greater than `POK_PARTITION_MEMORY_BASE`
 *   plus `size_total` of the space, and `addr` is less than
 *   `POK_PARTITION_MEMORY_BASE` plus `size_total` minus `size_stack_used` of
 *   the space, `x86_64_check_access()` shall return `FALSE`.
 *   <br>Traces to:
 *   - \hlrref{HLR_UACCESS_X86_64_CHECK_ACCESS_01}
 * - \llrid{LLR_UACCESS_X86_64_CHECK_ACCESS_06}: If `addr` plus `size` is not
 *   less than `addr`, is greater than `POK_PARTITION_MEMORY_BASE` plus
 *   `heap_end` of the space and is greater than `POK_PARTITION_MEMORY_BASE`
 *   plus `size_total` of the space, `x86_64_check_access()` shall return
 *   `FALSE`.
 *   <br>Traces to:
 *   - \hlrref{HLR_UACCESS_X86_64_CHECK_ACCESS_01}
 *
 * @design
 * 1. Set `start` to `(uintptr_t)addr`.
 *    \llrref{LLR_UACCESS_X86_64_CHECK_ACCESS_01},
 *    \llrref{LLR_UACCESS_X86_64_CHECK_ACCESS_02},
 *    \llrref{LLR_UACCESS_X86_64_CHECK_ACCESS_03},
 *    \llrref{LLR_UACCESS_X86_64_CHECK_ACCESS_04},
 *    \llrref{LLR_UACCESS_X86_64_CHECK_ACCESS_05},
 *    \llrref{LLR_UACCESS_X86_64_CHECK_ACCESS_06}
 * 2. Set `end` to `start + size`. \llrref{LLR_UACCESS_X86_64_CHECK_ACCESS_01},
 *    \llrref{LLR_UACCESS_X86_64_CHECK_ACCESS_02},
 *    \llrref{LLR_UACCESS_X86_64_CHECK_ACCESS_03},
 *    \llrref{LLR_UACCESS_X86_64_CHECK_ACCESS_04},
 *    \llrref{LLR_UACCESS_X86_64_CHECK_ACCESS_05},
 *    \llrref{LLR_UACCESS_X86_64_CHECK_ACCESS_06}
 * 3. Set `space` to the address of `ja_spaces[space_id - 1U]`.
 *    \llrref{LLR_UACCESS_X86_64_CHECK_ACCESS_02},
 *    \llrref{LLR_UACCESS_X86_64_CHECK_ACCESS_03},
 *    \llrref{LLR_UACCESS_X86_64_CHECK_ACCESS_04},
 *    \llrref{LLR_UACCESS_X86_64_CHECK_ACCESS_05},
 *    \llrref{LLR_UACCESS_X86_64_CHECK_ACCESS_06}
 * 4. If `end` is less than `start`: \llrref{LLR_UACCESS_X86_64_CHECK_ACCESS_01}
 *    1. Return `FALSE`.
 * 5. If `end` is less than or equal to
 *    `(space->heap_end + POK_PARTITION_MEMORY_BASE)`:
 *    \llrref{LLR_UACCESS_X86_64_CHECK_ACCESS_02},
 *    \llrref{LLR_UACCESS_X86_64_CHECK_ACCESS_03}
 *    1. Return `(start >= POK_PARTITION_MEMORY_BASE)`.
 * 6. Otherwise, if `end` is less than or equal to
 *    `(POK_PARTITION_MEMORY_BASE + space->size_total)`:
 *    \llrref{LLR_UACCESS_X86_64_CHECK_ACCESS_04},
 *    \llrref{LLR_UACCESS_X86_64_CHECK_ACCESS_05}
 *    1. Return `(start >= (POK_PARTITION_MEMORY_BASE + space->size_total -`
 *       `space->size_stack_used))`.
 * 7. Otherwise: \llrref{LLR_UACCESS_X86_64_CHECK_ACCESS_06}
 *    1. Return `FALSE`.
 *
 * @calls
 * None.
 */
/*
 * MISRA C:2025 Rule 11.6 deviation (this file): user-space addresses are
 * checked against the partition's memory as integers, and translated to
 * kernel addresses by integer arithmetic.
 */
static pok_bool_t x86_64_check_access(const void* JET_USER addr, size_t size,
    jet_space_id space_id)
{
    assert(space_id != 0U);
    assert(size != 0U);

    uintptr_t start = (uintptr_t)addr;
    uintptr_t end = start + size;

    ja_x86_64_space_t* space = &ja_spaces[space_id - 1U];

    /*
     * Currently, there are 2 segments accessible to user:
     * 1. [POK_PARTITION_MEMORY_BASE; POK_PARTITION_MEMORY_BASE + space->heap_end)
     *    code,data and heap
     * 2. [POK_PARTITION_MEMORY_BASE + space->size_total - space->size_stack_used; POK_PARTITION_MEMORY_BASE + space->size_total)
     *    stacks
     */

    if(end < start) {
        return FALSE; // Segments doesn't cross NULL.
    }

    if(end <= (space->heap_end + POK_PARTITION_MEMORY_BASE))
    {
        return (start >= POK_PARTITION_MEMORY_BASE);
    }
    else if(end <= (POK_PARTITION_MEMORY_BASE + space->size_total))
    {
        return (start >= (POK_PARTITION_MEMORY_BASE + space->size_total
                          - space->size_stack_used));
    }
    else
    {
        return FALSE;
    }
}

/**
 * @funcname `user_space_shift`
 *
 * @description
 * Returns the difference between a space's user addresses and their kernel
 * addresses (the kernel's identity mapping of RAM).
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `space_id` | `jet_space_id` | [in] The space (not 0). |
 *
 * @return The difference.
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `ja_spaces` | `ja_x86_64_space_t[]` | The memory spaces of the partitions |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `space` | `ja_x86_64_space_t *` | The space |
 *
 * @hlr
 * - \hlrid{HLR_UACCESS_USER_SPACE_SHIFT_01}: `user_space_shift()` shall give
 *   the difference between the user addresses of a space and the kernel
 *   addresses of its memory.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_MEM_006}
 *
 * @llr
 * - \llrid{LLR_UACCESS_USER_SPACE_SHIFT_01}: `user_space_shift()` shall return
 *   `POK_PARTITION_MEMORY_BASE` minus `phys_base` of `ja_spaces[space_id - 1]`.
 *   <br>Traces to:
 *   - \hlrref{HLR_UACCESS_USER_SPACE_SHIFT_01}
 *
 * @design
 * 1. Set `space` to the address of `ja_spaces[space_id - 1U]`.
 *    \llrref{LLR_UACCESS_USER_SPACE_SHIFT_01}
 * 2. Return `POK_PARTITION_MEMORY_BASE - space->phys_base`.
 *    \llrref{LLR_UACCESS_USER_SPACE_SHIFT_01}
 *
 * @calls
 * None.
 */
static uintptr_t user_space_shift(jet_space_id space_id)
{
    assert(space_id != 0U);
    ja_x86_64_space_t* space = &ja_spaces[space_id - 1U];

    return POK_PARTITION_MEMORY_BASE - space->phys_base;
}

/**
 * @funcname `ja_user_to_kernel_space`
 *
 * @description
 * Returns the kernel address of a user area, if it is accessible.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `addr` | `void * JET_USER` | [in] The user address. |
 * | `size` | `size_t` | [in] The size. |
 * | `space_id` | `jet_space_id` | [in] The space. |
 *
 * @return The kernel address, or NULL.
 *
 * @globals
 * None.
 *
 * @locals
 * None.
 *
 * @hlr
 * - \hlrid{HLR_UACCESS_X86_64_JA_USER_TO_KERNEL_SPACE_01}:
 *   `ja_user_to_kernel_space()` shall give the kernel address of an accessible
 *   area of a user space, or NULL.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_MEM_006}
 *   - \srqref{SRQ_JET_MEM_015}
 *
 * @llr
 * - \llrid{LLR_UACCESS_X86_64_JA_USER_TO_KERNEL_SPACE_01}: If
 *   `x86_64_check_access()` returns true for `addr`, `size` and `space_id`,
 *   `ja_user_to_kernel_space()` shall return `addr` minus the result of
 *   `user_space_shift()` for `space_id`.
 *   <br>Traces to:
 *   - \hlrref{HLR_UACCESS_X86_64_JA_USER_TO_KERNEL_SPACE_01}
 * - \llrid{LLR_UACCESS_X86_64_JA_USER_TO_KERNEL_SPACE_02}: If
 *   `x86_64_check_access()` returns false for `addr`, `size` and `space_id`,
 *   `ja_user_to_kernel_space()` shall return NULL without calling
 *   `user_space_shift()`.
 *   <br>Traces to:
 *   - \hlrref{HLR_UACCESS_X86_64_JA_USER_TO_KERNEL_SPACE_01}
 *
 * @design
 * 1. If `x86_64_check_access(addr, size, space_id)` is true:
 *    \llrref{LLR_UACCESS_X86_64_JA_USER_TO_KERNEL_SPACE_01}
 *    1. Return
 *       `(void* JET_KUSER)((uintptr_t)addr - user_space_shift(space_id))`.
 * 2. Otherwise: \llrref{LLR_UACCESS_X86_64_JA_USER_TO_KERNEL_SPACE_02}
 *    1. Return `NULL`.
 *
 * @calls
 * - `x86_64_check_access()`
 * - `user_space_shift()`
 */
void* JET_KUSER ja_user_to_kernel_space(void* JET_USER addr, size_t size,
    jet_space_id space_id)
{
    if(x86_64_check_access(addr, size, space_id)) {
       return (void* JET_KUSER)((uintptr_t)addr - user_space_shift(space_id));
    } else {
       return NULL;
    }
}

/**
 * @funcname `ja_user_to_kernel_ro_space`
 *
 * @description
 * Returns the kernel address of a user area for reading, if it is accessible.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `addr` | `const void * JET_USER` | [in] The user address. |
 * | `size` | `size_t` | [in] The size. |
 * | `space_id` | `jet_space_id` | [in] The space. |
 *
 * @return The kernel address, or NULL.
 *
 * @globals
 * None.
 *
 * @locals
 * None.
 *
 * @hlr
 * - \hlrid{HLR_UACCESS_X86_64_JA_USER_TO_KERNEL_RO_SPACE_01}:
 *   `ja_user_to_kernel_ro_space()` shall give the kernel address of an area of
 *   a user space that may be read, or NULL.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_MEM_006}
 *   - \srqref{SRQ_JET_MEM_015}
 *
 * @llr
 * - \llrid{LLR_UACCESS_X86_64_JA_USER_TO_KERNEL_RO_SPACE_01}: If
 *   `x86_64_check_access()` returns true for `addr`, `size` and `space_id`,
 *   `ja_user_to_kernel_ro_space()` shall return `addr` minus the result of
 *   `user_space_shift()` for `space_id`.
 *   <br>Traces to:
 *   - \hlrref{HLR_UACCESS_X86_64_JA_USER_TO_KERNEL_RO_SPACE_01}
 * - \llrid{LLR_UACCESS_X86_64_JA_USER_TO_KERNEL_RO_SPACE_02}: If
 *   `x86_64_check_access()` returns false for `addr`, `size` and `space_id`,
 *   `ja_user_to_kernel_ro_space()` shall return NULL without calling
 *   `user_space_shift()`.
 *   <br>Traces to:
 *   - \hlrref{HLR_UACCESS_X86_64_JA_USER_TO_KERNEL_RO_SPACE_01}
 *
 * @design
 * 1. If `x86_64_check_access(addr, size, space_id)` is true:
 *    \llrref{LLR_UACCESS_X86_64_JA_USER_TO_KERNEL_RO_SPACE_01}
 *    1. Return `(const void* JET_KUSER)((uintptr_t)addr -`
 *       `user_space_shift(space_id))`.
 * 2. Otherwise: \llrref{LLR_UACCESS_X86_64_JA_USER_TO_KERNEL_RO_SPACE_02}
 *    1. Return `NULL`.
 *
 * @calls
 * - `x86_64_check_access()`
 * - `user_space_shift()`
 */
const void* JET_KUSER ja_user_to_kernel_ro_space(const void* JET_USER addr,
    size_t size, jet_space_id space_id)
{
    if(x86_64_check_access(addr, size, space_id)) {
       return (const void* JET_KUSER)((uintptr_t)addr
                                      - user_space_shift(space_id));
    } else {
       return NULL;
    }
}

/**
 * @funcname `ja_check_access_exec`
 *
 * @description
 * Tells whether a user address is in a space's code and data (may be executed).
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `addr` | `void * JET_USER` | [in] The user address. |
 * | `space_id` | `jet_space_id` | [in] The space (not 0). |
 *
 * @return TRUE if it may be executed.
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `ja_spaces` | `ja_x86_64_space_t[]` | The memory spaces of the partitions |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `space` | `ja_x86_64_space_t *` | The space |
 * | `start` | `uintptr_t` | The address |
 *
 * @hlr
 * - \hlrid{HLR_UACCESS_X86_64_JA_CHECK_ACCESS_EXEC_01}:
 *   `ja_check_access_exec()` shall tell whether a user address lies in the code
 *   and data of a space, where code may be executed.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_MEM_007}
 *
 * @llr
 * - \llrid{LLR_UACCESS_X86_64_JA_CHECK_ACCESS_EXEC_01}: If `addr` is less than
 *   `POK_PARTITION_MEMORY_BASE` plus `size_normal` of `ja_spaces[space_id - 1]`
 *   and is not less than `POK_PARTITION_MEMORY_BASE`, `ja_check_access_exec()`
 *   shall return true.
 *   <br>Traces to:
 *   - \hlrref{HLR_UACCESS_X86_64_JA_CHECK_ACCESS_EXEC_01}
 * - \llrid{LLR_UACCESS_X86_64_JA_CHECK_ACCESS_EXEC_02}: If `addr` is not less
 *   than `POK_PARTITION_MEMORY_BASE` plus `size_normal` of
 *   `ja_spaces[space_id - 1]`, or is less than `POK_PARTITION_MEMORY_BASE`,
 *   `ja_check_access_exec()` shall return false.
 *   <br>Traces to:
 *   - \hlrref{HLR_UACCESS_X86_64_JA_CHECK_ACCESS_EXEC_01}
 *
 * @design
 * 1. Set `start` to `(uintptr_t)addr`.
 *    \llrref{LLR_UACCESS_X86_64_JA_CHECK_ACCESS_EXEC_01},
 *    \llrref{LLR_UACCESS_X86_64_JA_CHECK_ACCESS_EXEC_02}
 * 2. Set `space` to the address of `ja_spaces[space_id - 1U]`.
 *    \llrref{LLR_UACCESS_X86_64_JA_CHECK_ACCESS_EXEC_01},
 *    \llrref{LLR_UACCESS_X86_64_JA_CHECK_ACCESS_EXEC_02}
 * 3. Return
 *    `(start < (POK_PARTITION_MEMORY_BASE + space->size_normal)) && (start >=`
 *    `POK_PARTITION_MEMORY_BASE)`.
 *    \llrref{LLR_UACCESS_X86_64_JA_CHECK_ACCESS_EXEC_01},
 *    \llrref{LLR_UACCESS_X86_64_JA_CHECK_ACCESS_EXEC_02}
 *
 * @calls
 * None.
 */
pok_bool_t ja_check_access_exec(void* JET_USER addr, jet_space_id space_id)
{
    assert(space_id != 0U);

    uintptr_t start = (uintptr_t)addr;

    ja_x86_64_space_t* space = &ja_spaces[space_id - 1U];

    /*
     * Only single segment could be executed by user:
     *   [POK_PARTITION_MEMORY_BASE; POK_PARTITION_MEMORY_BASE + space->size_normal)
     *    code and data
     */

    return (start < (POK_PARTITION_MEMORY_BASE + space->size_normal))
        && (start >= POK_PARTITION_MEMORY_BASE);
}

#ifdef POK_NEEDS_GDB

/**
 * @funcname `x86_64_is_kernel_area`
 *
 * @description
 * Tells whether GDB may access the kernel memory [addr, addr + size): the
 * kernel image, its heap and stacks lie below the partitions' memory.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `addr` | `uintptr_t` | [in] The address. |
 * | `size` | `size_t` | [in] The size. |
 *
 * @return TRUE if it is.
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `end` | `uintptr_t` | The end of the area |
 *
 * @hlr
 * - \hlrid{HLR_UACCESS_X86_64_IS_KERNEL_AREA_01}: `x86_64_is_kernel_area()`
 *   shall tell whether the debugger may access an area of the kernel's memory:
 *   below the memory of the partitions.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_DBG_004}
 *
 * @llr
 * - \llrid{LLR_UACCESS_X86_64_IS_KERNEL_AREA_01}: If `size` is not 0, and
 *   `addr` plus `size` is greater than `addr` and is not greater than
 *   `POK_PARTITION_MEMORY_PHYS_START`, `x86_64_is_kernel_area()` shall return
 *   true.
 *   <br>Traces to:
 *   - \hlrref{HLR_UACCESS_X86_64_IS_KERNEL_AREA_01}
 * - \llrid{LLR_UACCESS_X86_64_IS_KERNEL_AREA_02}: If `size` is 0, or `addr`
 *   plus `size` is not greater than `addr`, or `addr` plus `size` is greater
 *   than `POK_PARTITION_MEMORY_PHYS_START`, `x86_64_is_kernel_area()` shall
 *   return false.
 *   <br>Traces to:
 *   - \hlrref{HLR_UACCESS_X86_64_IS_KERNEL_AREA_01}
 *
 * @design
 * 1. Set `end` to `addr + size`. \llrref{LLR_UACCESS_X86_64_IS_KERNEL_AREA_01},
 *    \llrref{LLR_UACCESS_X86_64_IS_KERNEL_AREA_02}
 * 2. Return `(size != 0U) && (end > addr) && (end <=`
 *    `POK_PARTITION_MEMORY_PHYS_START)`.
 *    \llrref{LLR_UACCESS_X86_64_IS_KERNEL_AREA_01},
 *    \llrref{LLR_UACCESS_X86_64_IS_KERNEL_AREA_02}
 *
 * @calls
 * None.
 */
static pok_bool_t x86_64_is_kernel_area(uintptr_t addr, size_t size)
{
    uintptr_t end = addr + size;

    return (size != 0U) && (end > addr)
        && (end <= POK_PARTITION_MEMORY_PHYS_START);
}

/**
 * @funcname `ja_addr_to_gdb`
 *
 * @description
 * Returns the kernel address of memory that GDB writes: kernel memory (space 0)
 * or a user area.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `addr` | `uintptr_t` | [in] The address. |
 * | `size` | `size_t` | [in] The size. |
 * | `space_id` | `jet_space_id` | [in] The space, or 0 for the kernel. |
 *
 * @return The kernel address, or NULL.
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `res` | `void *` | The kernel address |
 *
 * @hlr
 * - \hlrid{HLR_UACCESS_X86_64_JA_ADDR_TO_GDB_01}: `ja_addr_to_gdb()` shall give
 *   the kernel address of memory that the debugger writes: of the kernel
 *   (space 0) or of a user area, or NULL.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_DBG_004}
 *
 * @llr
 * - \llrid{LLR_UACCESS_X86_64_JA_ADDR_TO_GDB_01}: If `space_id` is 0 and
 *   `x86_64_is_kernel_area()` returns true for `addr` and `size`,
 *   `ja_addr_to_gdb()` shall return `addr` as a pointer.
 *   <br>Traces to:
 *   - \hlrref{HLR_UACCESS_X86_64_JA_ADDR_TO_GDB_01}
 * - \llrid{LLR_UACCESS_X86_64_JA_ADDR_TO_GDB_02}: If `space_id` is 0 and
 *   `x86_64_is_kernel_area()` returns false for `addr` and `size`,
 *   `ja_addr_to_gdb()` shall return NULL.
 *   <br>Traces to:
 *   - \hlrref{HLR_UACCESS_X86_64_JA_ADDR_TO_GDB_01}
 * - \llrid{LLR_UACCESS_X86_64_JA_ADDR_TO_GDB_03}: If `space_id` is not 0 and
 *   `size` is not 0, `ja_addr_to_gdb()` shall return the result of
 *   `ja_user_to_kernel_space()` for `addr`, `size` and `space_id`.
 *   <br>Traces to:
 *   - \hlrref{HLR_UACCESS_X86_64_JA_ADDR_TO_GDB_01}
 * - \llrid{LLR_UACCESS_X86_64_JA_ADDR_TO_GDB_04}: If `space_id` is not 0 and
 *   `size` is 0, `ja_addr_to_gdb()` shall return NULL without calling
 *   `ja_user_to_kernel_space()`.
 *   <br>Traces to:
 *   - \hlrref{HLR_UACCESS_X86_64_JA_ADDR_TO_GDB_01}
 *
 * @design
 * 1. Set `res` to `NULL`. \llrref{LLR_UACCESS_X86_64_JA_ADDR_TO_GDB_02},
 *    \llrref{LLR_UACCESS_X86_64_JA_ADDR_TO_GDB_04}
 * 2. If `space_id` is equal to `0U`:
 *    \llrref{LLR_UACCESS_X86_64_JA_ADDR_TO_GDB_01},
 *    \llrref{LLR_UACCESS_X86_64_JA_ADDR_TO_GDB_02}
 *    1. If `x86_64_is_kernel_area(addr, size)` is true:
 *       1. Set `res` to `(void*)addr`.
 *          \llrref{LLR_UACCESS_X86_64_JA_ADDR_TO_GDB_01}
 * 3. Otherwise, if `size` is not equal to `0U`:
 *    \llrref{LLR_UACCESS_X86_64_JA_ADDR_TO_GDB_03},
 *    \llrref{LLR_UACCESS_X86_64_JA_ADDR_TO_GDB_04}
 *    1. Call `ja_user_to_kernel_space()` with the arguments
 *       `(void* JET_USER)addr`, `size` and `space_id`, and store the result in
 *       `res`. \llrref{LLR_UACCESS_X86_64_JA_ADDR_TO_GDB_03}
 * 4. Return `res`. \llrref{LLR_UACCESS_X86_64_JA_ADDR_TO_GDB_01},
 *    \llrref{LLR_UACCESS_X86_64_JA_ADDR_TO_GDB_02},
 *    \llrref{LLR_UACCESS_X86_64_JA_ADDR_TO_GDB_03},
 *    \llrref{LLR_UACCESS_X86_64_JA_ADDR_TO_GDB_04}
 *
 * @calls
 * - `x86_64_is_kernel_area()`
 * - `ja_user_to_kernel_space()`
 */
void* ja_addr_to_gdb(uintptr_t addr, size_t size, jet_space_id space_id)
{
    void* res = NULL;

    if(space_id == 0U)
    {
        // The kernel maps its memory one-to-one.
        if(x86_64_is_kernel_area(addr, size)) {
            res = (void*)addr;
        }
    }
    else if(size != 0U)
    {
        res = ja_user_to_kernel_space((void* JET_USER)addr, size, space_id);
    }
    else
    {
        // Empty area: nothing to access.
    }

    return res;
}

/**
 * @funcname `ja_addr_to_gdb_ro`
 *
 * @description
 * Returns the kernel address of memory that GDB reads: kernel memory (space 0)
 * or a user area.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `addr` | `uintptr_t` | [in] The address. |
 * | `size` | `size_t` | [in] The size. |
 * | `space_id` | `jet_space_id` | [in] The space, or 0 for the kernel. |
 *
 * @return The kernel address, or NULL.
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `res` | `const void *` | The kernel address |
 *
 * @hlr
 * - \hlrid{HLR_UACCESS_X86_64_JA_ADDR_TO_GDB_RO_01}: `ja_addr_to_gdb_ro()`
 *   shall give the kernel address of memory that the debugger reads: of the
 *   kernel (space 0) or of a user area, or NULL.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_DBG_004}
 *
 * @llr
 * - \llrid{LLR_UACCESS_X86_64_JA_ADDR_TO_GDB_RO_01}: If `space_id` is 0 and
 *   `x86_64_is_kernel_area()` returns true for `addr` and `size`,
 *   `ja_addr_to_gdb_ro()` shall return `addr` as a pointer.
 *   <br>Traces to:
 *   - \hlrref{HLR_UACCESS_X86_64_JA_ADDR_TO_GDB_RO_01}
 * - \llrid{LLR_UACCESS_X86_64_JA_ADDR_TO_GDB_RO_02}: If `space_id` is 0 and
 *   `x86_64_is_kernel_area()` returns false for `addr` and `size`,
 *   `ja_addr_to_gdb_ro()` shall return NULL.
 *   <br>Traces to:
 *   - \hlrref{HLR_UACCESS_X86_64_JA_ADDR_TO_GDB_RO_01}
 * - \llrid{LLR_UACCESS_X86_64_JA_ADDR_TO_GDB_RO_03}: If `space_id` is not 0 and
 *   `size` is not 0, `ja_addr_to_gdb_ro()` shall return the result of
 *   `ja_user_to_kernel_ro_space()` for `addr`, `size` and `space_id`.
 *   <br>Traces to:
 *   - \hlrref{HLR_UACCESS_X86_64_JA_ADDR_TO_GDB_RO_01}
 * - \llrid{LLR_UACCESS_X86_64_JA_ADDR_TO_GDB_RO_04}: If `space_id` is not 0 and
 *   `size` is 0, `ja_addr_to_gdb_ro()` shall return NULL without calling
 *   `ja_user_to_kernel_ro_space()`.
 *   <br>Traces to:
 *   - \hlrref{HLR_UACCESS_X86_64_JA_ADDR_TO_GDB_RO_01}
 *
 * @design
 * 1. Set `res` to `NULL`. \llrref{LLR_UACCESS_X86_64_JA_ADDR_TO_GDB_RO_02},
 *    \llrref{LLR_UACCESS_X86_64_JA_ADDR_TO_GDB_RO_04}
 * 2. If `space_id` is equal to `0U`:
 *    \llrref{LLR_UACCESS_X86_64_JA_ADDR_TO_GDB_RO_01},
 *    \llrref{LLR_UACCESS_X86_64_JA_ADDR_TO_GDB_RO_02}
 *    1. If `x86_64_is_kernel_area(addr, size)` is true:
 *       1. Set `res` to `(const void*)addr`.
 *          \llrref{LLR_UACCESS_X86_64_JA_ADDR_TO_GDB_RO_01}
 * 3. Otherwise, if `size` is not equal to `0U`:
 *    \llrref{LLR_UACCESS_X86_64_JA_ADDR_TO_GDB_RO_03},
 *    \llrref{LLR_UACCESS_X86_64_JA_ADDR_TO_GDB_RO_04}
 *    1. Call `ja_user_to_kernel_ro_space()` with the arguments
 *       `(const void* JET_USER)addr`, `size` and `space_id`, and store the
 *       result in `res`. \llrref{LLR_UACCESS_X86_64_JA_ADDR_TO_GDB_RO_03}
 * 4. Return `res`. \llrref{LLR_UACCESS_X86_64_JA_ADDR_TO_GDB_RO_01},
 *    \llrref{LLR_UACCESS_X86_64_JA_ADDR_TO_GDB_RO_02},
 *    \llrref{LLR_UACCESS_X86_64_JA_ADDR_TO_GDB_RO_03},
 *    \llrref{LLR_UACCESS_X86_64_JA_ADDR_TO_GDB_RO_04}
 *
 * @calls
 * - `x86_64_is_kernel_area()`
 * - `ja_user_to_kernel_ro_space()`
 */
const void* ja_addr_to_gdb_ro(uintptr_t addr, size_t size,
    jet_space_id space_id)
{
    const void* res = NULL;

    if(space_id == 0U)
    {
        // The kernel maps its memory one-to-one.
        if(x86_64_is_kernel_area(addr, size)) {
            res = (const void*)addr;
        }
    }
    else if(size != 0U)
    {
        res = ja_user_to_kernel_ro_space((const void* JET_USER)addr, size,
            space_id);
    }
    else
    {
        // Empty area: nothing to access.
    }

    return res;
}

#endif /* POK_NEEDS_GDB */
