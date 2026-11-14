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
 * Modified by Kirill Brazhnikov, 2026-11-02: MISRA C:2025: discarded
 * results are cast to void; the partition entry point is
 * pok_partition_start (no reserved name).
 *
 * Modified by Kirill Brazhnikov, 2026-11-08: Doxygen headers for the file
 * and its functions.
 *
 * Modified by Kirill Brazhnikov, 2026-11-13: the declaration of main() is moved
 * to libpok/include/init_arinc.h.
 */

/**
 * @file
 * The start of a partition: libpok's entry, which calls main().
 */

#include <kernel_shared_data.h>
#include <init_arinc.h>
#include <smalloc.h>

char* heap_current = NULL;

/**
 * @funcname `pok_partition_start`
 *
 * @description
 * The entry of the partition's main thread: sets up the user-only fields of the
 * data shared with the kernel and the heap, initializes the ARINC 653 objects,
 * then runs main().
 *
 * @return 0.
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `heap_current` | `char *` | The end of the partition's heap used so far |
 * | `kshd` | `jet_kernel_shared_t` | The data that the kernel shares with the partition (current thread, threads, partition mode) |
 *
 * @locals
 * None.
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Set `kshd.main_thread_id` to `kshd.current_thread_id`.
 * 2. Set `kshd.error_thread_id` to `JET_THREAD_ID_NONE`.
 * 3. Set `heap_current` to `kshd.heap_start`.
 * 4. Call `libjet_arinc_init()`.
 * 5. Call `main()`.
 * 6. Return `(0)`.
 *
 * @calls
 * - `libjet_arinc_init()`
 * - `main()`
 */
int32_t pok_partition_start (void)
{
   // Setup user-only fields of kernel shared data.
   kshd.main_thread_id = kshd.current_thread_id;
   kshd.error_thread_id = JET_THREAD_ID_NONE;

   heap_current = kshd.heap_start;

   libjet_arinc_init();

   (void) main(); /* main loop from user */
   return (0);
}
