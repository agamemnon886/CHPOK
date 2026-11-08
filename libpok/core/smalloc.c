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
 * NULL, which its callers don't check.
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

void* smalloc(size_t size)
{
    return smalloc_aligned(size, libja_mem_get_alignment(size));
}

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
