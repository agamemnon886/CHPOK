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
 * Modified by Kirill Brazhnikov, 2026-10-31: built-in integer types (int,
 * unsigned, short, long, ...) are replaced with the fixed-width types
 * from <stdint.h>.
 *
 * Modified by Kirill Brazhnikov, 2026-11-02: MISRA C:2025: __user/__kuser
 * are renamed JET_USER/JET_KUSER; braces around all if/else/loop bodies;
 * essentially Boolean conditions.
 *
 * Modified by Kirill Brazhnikov, 2026-11-03: long lines are wrapped at 80
 * columns.
 *
 * Modified by Kirill Brazhnikov, 2026-11-08: Doxygen headers for the file
 * and its functions.
 *
 * Modified by Kirill Brazhnikov, 2026-11-16: high and low level requirements of
 * the functions, traced to the system requirements (docs/SRQs), and the steps
 * of their design linked to the LLRs.
 *
 * Modified by Kirill Brazhnikov, 2026-11-17: the high level requirements
 * trace to the system requirements split into requirements of one "shall"
 * each.
 */

/**
 * @file
 * Access to user space from kernel space: the kernel addresses of the
 * current partition's memory, checked against its space.
 */

#include <core/uaccess.h>
#include <core/partition.h>

/**
 * @funcname `jet_user_to_kernel`
 *
 * @description
 * Returns the kernel address of user memory that the kernel may read and write
 * for the current partition.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `addr` | `void * JET_USER` | [in] The user address. |
 * | `size` | `size_t` | [in] The size of the memory. |
 *
 * @return The kernel address, or NULL if the partition can't write all of it.
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `current_partition` | `pok_partition_t *` | The running partition |
 *
 * @locals
 * None.
 *
 * @hlr
 * - \hlrid{HLR_UACCESS_JET_USER_TO_KERNEL_01}: `jet_user_to_kernel()` shall
 *   give the kernel address of an area of user memory that the current
 *   partition may read and write, and NULL for any other area.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_MEM_006}
 *   - \srqref{SRQ_JET_MEM_015}
 *
 * @llr
 * - \llrid{LLR_UACCESS_JET_USER_TO_KERNEL_01}: `jet_user_to_kernel()` shall
 *   return the result of `ja_user_to_kernel_space()` for the area and the space
 *   of the current partition.
 *   <br>Traces to:
 *   - \hlrref{HLR_UACCESS_JET_USER_TO_KERNEL_01}
 *
 * @design
 * 1. Call `ja_user_to_kernel_space()` with the arguments `addr`, `size` and
 *    `current_partition->space_id`, and return its result.
 *    \llrref{LLR_UACCESS_JET_USER_TO_KERNEL_01}
 *
 * @calls
 * - `ja_user_to_kernel_space()`
 */
void* JET_KUSER jet_user_to_kernel(void* JET_USER addr, size_t size)
{
    return ja_user_to_kernel_space(addr, size, current_partition->space_id);
}

/**
 * @funcname `jet_user_to_kernel_ro`
 *
 * @description
 * Returns the kernel address of user memory that the kernel may read for the
 * current partition.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `addr` | `const void * JET_USER` | [in] The user address. |
 * | `size` | `size_t` | [in] The size of the memory. |
 *
 * @return The kernel address, or NULL if the partition can't read all of it.
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `current_partition` | `pok_partition_t *` | The running partition |
 *
 * @locals
 * None.
 *
 * @hlr
 * - \hlrid{HLR_UACCESS_JET_USER_TO_KERNEL_RO_01}: `jet_user_to_kernel_ro()`
 *   shall give the kernel address of an area of user memory that the current
 *   partition may read, and NULL for any other area.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_MEM_006}
 *   - \srqref{SRQ_JET_MEM_015}
 *
 * @llr
 * - \llrid{LLR_UACCESS_JET_USER_TO_KERNEL_RO_01}: `jet_user_to_kernel_ro()`
 *   shall return the result of `ja_user_to_kernel_ro_space()` for the area and
 *   the space of the current partition.
 *   <br>Traces to:
 *   - \hlrref{HLR_UACCESS_JET_USER_TO_KERNEL_RO_01}
 *
 * @design
 * 1. Call `ja_user_to_kernel_ro_space()` with the arguments `addr`, `size` and
 *    `current_partition->space_id`, and return its result.
 *    \llrref{LLR_UACCESS_JET_USER_TO_KERNEL_RO_01}
 *
 * @calls
 * - `ja_user_to_kernel_ro_space()`
 */
const void* JET_KUSER jet_user_to_kernel_ro(const void* JET_USER addr,
                                            size_t size)
{
    return ja_user_to_kernel_ro_space(addr, size, current_partition->space_id);
}

/**
 * @funcname `jet_check_access_exec`
 *
 * @description
 * Tells whether a user address may hold code that the current partition can
 * execute (an additional check of entry points; the kernel doesn't access the
 * address).
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `addr` | `void * JET_USER` | [in] The user address. |
 *
 * @return TRUE if it may be executable code of the partition.
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `current_partition` | `pok_partition_t *` | The running partition |
 *
 * @locals
 * None.
 *
 * @hlr
 * - \hlrid{HLR_UACCESS_JET_CHECK_ACCESS_EXEC_01}: `jet_check_access_exec()`
 *   shall tell whether a user address may hold code that the current partition
 *   can execute.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_MEM_007}
 *
 * @llr
 * - \llrid{LLR_UACCESS_JET_CHECK_ACCESS_EXEC_01}: `jet_check_access_exec()`
 *   shall return the result of `ja_check_access_exec()` for the address and the
 *   space of the current partition.
 *   <br>Traces to:
 *   - \hlrref{HLR_UACCESS_JET_CHECK_ACCESS_EXEC_01}
 *
 * @design
 * 1. Call `ja_check_access_exec()` with the arguments `addr` and
 *    `current_partition->space_id`, and return its result.
 *    \llrref{LLR_UACCESS_JET_CHECK_ACCESS_EXEC_01}
 *
 * @calls
 * - `ja_check_access_exec()`
 */
pok_bool_t jet_check_access_exec(void* JET_USER addr)
{
    return ja_check_access_exec(addr, current_partition->space_id);
}

/**
 * @funcname `pok_copy_name_to_user`
 *
 * @description
 * Copies a name (at most MAX_NAME_LENGTH characters, up to its NUL) to user
 * memory that the caller has checked.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `to` | `char *` | [out] The destination. |
 * | `name` | `const char *` | [in] The name. |
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `i` | `int32_t` | The index of the character |
 * | `null_found` | `pok_bool_t` | Whether the NUL was copied |
 *
 * @hlr
 * - \hlrid{HLR_UACCESS_POK_COPY_NAME_TO_USER_01}: `pok_copy_name_to_user()`
 *   shall copy a name to user memory that the caller has checked, up to and
 *   including its NUL, and never more than `MAX_NAME_LENGTH` characters.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_THR_003}
 *   - \srqref{SRQ_JET_SYS_005}
 *
 * @llr
 * - \llrid{LLR_UACCESS_POK_COPY_NAME_TO_USER_01}: `pok_copy_name_to_user()`
 *   shall copy the characters one by one and stop after the NUL character or
 *   after `MAX_NAME_LENGTH` characters, whichever comes first.
 *   <br>Traces to:
 *   - \hlrref{HLR_UACCESS_POK_COPY_NAME_TO_USER_01}
 *
 * @design
 * 1. Set `null_found` to `FALSE`. \llrref{LLR_UACCESS_POK_COPY_NAME_TO_USER_01}
 * 2. Set `i` to `0`; then while `null_found` is false and `i` is less than
 *    `MAX_NAME_LENGTH`, repeat (after each pass: increment `i`):
 *    \llrref{LLR_UACCESS_POK_COPY_NAME_TO_USER_01}
 *    1. Set `to[i]` to `name[i]`.
 *    2. If `name[i]` is equal to `'\0'`:
 *       1. Set `null_found` to `TRUE`.
 *
 * @calls
 * None.
 */
void pok_copy_name_to_user(char* to, const char* name)
{
    pok_bool_t null_found = FALSE;

    for(int32_t i = 0; !null_found && (i < MAX_NAME_LENGTH); i++)
    {
        to[i] = name[i];
        if(name[i] == '\0') {
            null_found = TRUE;
        }
    }
}
