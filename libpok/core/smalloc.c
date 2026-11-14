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
 * Modified by Kirill Brazhnikov, 2026-11-02: MISRA C:2025: braces around
 * all if/else/loop bodies; essentially Boolean conditions; U suffixes for
 * unsigned constants; discarded results are cast to void; internal
 * linkage for file-local definitions; deviations are documented where a
 * guideline can't be followed; heap exhaustion raises an application
 * error instead of calling abort().
 *
 * Modified by Kirill Brazhnikov, 2026-11-03: MISRA C:2025: operands of
 * mixed precedence are parenthesized; deviations are documented where a
 * guideline can't be followed. Long lines are wrapped at 80 columns.
 *
 * Modified by Kirill Brazhnikov, 2026-11-08: if the health monitor ignores
 * the heap exhaustion error, the partition goes IDLE instead of getting
 * NULL, which its callers don't check. Doxygen headers for the file and its
 * functions.
 *
 * Modified by Kirill Brazhnikov, 2026-11-14: The function headers no longer
 * have a step for the declaration of a local variable; the @locals table
 * describes it, and an initialized one is set in a step.
 */

/**
 * @file
 * The partition's heap: allocation at initialization (not in NORMAL mode),
 * never freed.
 */

#include <smalloc.h>
#include <kernel_shared_data.h>
#include <core/assert_os.h>
/*
 * MISRA C:2025 Rule 21.6 deviation: printf() is libpok's own console output
 * (a syscall to the JetOS console); no streams or files are used.
 */
#include <stdio.h>
#include <core/syscall.h>
#include <core/partition.h>
#include <utils.h>

/*
 * MISRA C:2025 Rule 11.5 deviation (this file): aligned addresses are
 * computed in the heap and returned as pointers to void.
 * MISRA C:2025 Rule 18.4 deviation (this file): objects are carved out of
 * the heap by adding sizes to addresses.
 */

/**
 * @funcname `heap_exhausted`
 *
 * @description
 * Reports that the partition heap is exhausted: as an application error, as if
 * the health monitor ignores the error, the heap is still exhausted and the
 * callers don't expect NULL, so the partition goes IDLE.
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `msg` | `static const char[40]` | The message of the error |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Call `printf()` with the argument
 *    `"ERROR: Failed to allocate memory from the heap.\n"`.
 * 2. Call `printf()` with the argument
 *    `"NOTE: Probably, you need to configure more heap for the partition.\n"`.
 * 3. Call `pok_error_raise_application_error()` with the arguments `msg` and
 *    `sizeof(msg) - 1U`.
 * 4. Repeat forever:
 *    1. Call `pok_partition_set_mode_current()` with the argument
 *       `POK_PARTITION_MODE_IDLE`.
 *
 * @calls
 * - `printf()`
 * - `pok_error_raise_application_error()`
 * - `pok_partition_set_mode_current()`
 */
/*
 * The partition heap is exhausted. Reported as an application error, as
 * abort() used to do (MISRA C:2025 Rule 21.8: no abort()). Doesn't return:
 * if the health monitor ignores the error, the heap is still exhausted and
 * the callers don't expect NULL, so the partition goes IDLE.
 */
static _Noreturn void heap_exhausted(void)
{
    static const char msg[] = "Failed to allocate memory from the heap";

    (void) printf("ERROR: Failed to allocate memory from the heap.\n");
    (void) printf(
        "NOTE: Probably, you need to configure more heap for the partition.\n");
    (void) pok_error_raise_application_error(msg, sizeof(msg) - 1U);
    for (;;) {
        // Setting the IDLE mode doesn't return.
        (void) pok_partition_set_mode(POK_PARTITION_MODE_IDLE);
    }
}

/**
 * @funcname `smalloc_aligned`
 *
 * @description
 * Allocates memory of a size and alignment from the partition heap (not in
 * NORMAL mode); an exhausted heap doesn't return.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `size` | `size_t` | [in] The size. |
 * | `alignment` | `size_t` | [in] The alignment. |
 *
 * @return The memory, or NULL for size 0.
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `heap_current` | `char *` | The end of the partition's heap used so far |
 * | `kshd` | `jet_kernel_shared_t` | The data that the kernel shares with the partition (current thread, threads, partition mode) |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `obj_end` | `char *` | Its end |
 * | `obj_start` | `char *` | The start of the memory |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Check that `kshd.partition_mode != POK_PARTITION_MODE_NORMAL` holds
 *    (assert_os: stop otherwise).
 * 2. If `size` is equal to `0U`:
 *    1. Return `NULL`.
 * 3. Call `ALIGN_PTR()` with the arguments `heap_current` and `alignment`, and
 *    store the result in `obj_start`.
 * 4. Set `obj_end` to `obj_start + size`.
 * 5. If `obj_end` is greater than `kshd.heap_end`:
 *    1. Call `heap_exhausted()`.
 * 6. Set `heap_current` to `obj_end`.
 * 7. Return `obj_start`.
 *
 * @calls
 * - `assertion_os_fail()`
 * - `ALIGN_PTR()`
 * - `heap_exhausted()`
 */
void* smalloc_aligned(size_t size, size_t alignment)
{
    assert_os(kshd.partition_mode != POK_PARTITION_MODE_NORMAL);

    if(size == 0U) {
        return NULL;
    }

    char* obj_start = ALIGN_PTR(heap_current, alignment);
    char* obj_end = obj_start + size;

    if(obj_end > kshd.heap_end)
    {
        heap_exhausted(); // Doesn't return.
    }

    heap_current = obj_end;

    return obj_start;
}

/**
 * @funcname `smalloc`
 *
 * @description
 * Allocates memory of a size from the partition heap, with the alignment for
 * its size.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `size` | `size_t` | [in] The size. |
 *
 * @return The memory, or NULL for size 0.
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
 * 1. Call `smalloc_aligned()` with the arguments `size` and
 *    `libja_mem_get_alignment(size)`, and return its result.
 *
 * @calls
 * - `smalloc_aligned()`
 * - `libja_mem_get_alignment()`
 */
void* smalloc(size_t size)
{
    return smalloc_aligned(size, libja_mem_get_alignment(size));
}

/**
 * @funcname `scalloc`
 *
 * @description
 * Allocates an array from the partition heap, with the alignment of its
 * elements.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `nmemb` | `size_t` | [in] The number of elements. |
 * | `size` | `size_t` | [in] The size of an element. |
 *
 * @return The memory, or NULL for 0 elements or size 0.
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `heap_current` | `char *` | The end of the partition's heap used so far |
 * | `kshd` | `jet_kernel_shared_t` | The data that the kernel shares with the partition (current thread, threads, partition mode) |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `alignment` | `size_t` | The alignment of an element |
 * | `mem_end` | `char *` | Its end |
 * | `mem_start` | `char *` | The start of the memory |
 * | `stride` | `size_t` | The distance between elements |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. If `size` is equal to `0U` or `nmemb` is equal to `0U`:
 *    1. Return `NULL`.
 * 2. If `nmemb` is equal to `1U`:
 *    1. Call `smalloc()` with the argument `size`, and return its result.
 * 3. Call `libja_mem_get_alignment()` with the argument `size`, and store the
 *    result in `alignment`.
 * 4. Call `ALIGN()` with the arguments `size` and `alignment`, and store the
 *    result in `stride`.
 * 5. Call `ALIGN_PTR()` with the arguments `heap_current` and `alignment`, and
 *    store the result in `mem_start`.
 * 6. Set `mem_end` to `mem_start + (stride * (nmemb - 1U)) + size`.
 * 7. If `mem_end` is greater than `kshd.heap_end`:
 *    1. Call `heap_exhausted()`.
 * 8. Set `heap_current` to `mem_end`.
 * 9. Return `mem_start`.
 *
 * @calls
 * - `smalloc()`
 * - `libja_mem_get_alignment()`
 * - `ALIGN()`
 * - `ALIGN_PTR()`
 * - `heap_exhausted()`
 */
void* scalloc(size_t nmemb, size_t size)
{
    if((size == 0U) || (nmemb == 0U)) {
        return NULL;
    }

    if(nmemb == 1U) {
        return smalloc(size);
    }

    size_t alignment = libja_mem_get_alignment(size);
    size_t stride = ALIGN(size, alignment);

    char* mem_start = ALIGN_PTR(heap_current, alignment);
    char* mem_end = mem_start + (stride * (nmemb - 1U)) + size;

    if(mem_end > kshd.heap_end)
    {
        heap_exhausted(); // Doesn't return.
    }

    heap_current = mem_end;

    return mem_start;
}
