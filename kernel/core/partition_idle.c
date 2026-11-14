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
 * Modified by Kirill Brazhnikov, 2026-11-02: MISRA C:2025: essentially
 * Boolean conditions.
 *
 * Modified by Kirill Brazhnikov, 2026-11-03: MISRA C:2025: unused
 * parameters are cast to void.
 *
 * Modified by Kirill Brazhnikov, 2026-11-08: Doxygen headers for the file
 * and its functions.
 *
 * Modified by Kirill Brazhnikov, 2026-11-13: pok_part_ops_t is used
 * by its typedef name.
 */

/**
 * @file
 * The idle partition: it runs when no other partition has time, and only
 * loops.
 */

// Implementation for idle partition.

#include <core/partition.h>
#include <common.h>
#include <asp/arch.h>
#include <core/sched.h>

/**
 * @funcname `partition_idle_thread`
 *
 * @description
 * The start function of the idle partition: loops forever.
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
 * 1. Call `ja_inf_loop()`.
 *
 * @calls
 * - `ja_inf_loop()`
 */
static void partition_idle_thread(void)
{
    ja_inf_loop();
}

/**
 * @funcname `partition_idle_process_error`
 *
 * @description
 * The error processing of the idle partition: any error in it is fatal.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `partition_state` | `pok_system_state_t` | [in] The system state (not used). |
 * | `error_id` | `pok_error_id_t` | [in] The error (not used). |
 * | `state_byte_preempt_local` | `pok_bool_t` | [in] Whether local preemption was disabled (not used). |
 * | `failed_address` | `void *` | [in] The faulting address (not used). |
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
 * 1. Mark `partition_state` as not used.
 * 2. Mark `error_id` as not used.
 * 3. Mark `state_byte_preempt_local` as not used.
 * 4. Mark `failed_address` as not used.
 * 5. Call `pok_fatal()` with the argument `"Error in idle partition"`.
 *
 * @calls
 * - `pok_fatal()`
 */
static void partition_idle_process_error(pok_system_state_t partition_state,
        pok_error_id_t error_id,
        pok_bool_t state_byte_preempt_local,
        void* failed_address)
{
    (void) partition_state;
    (void) error_id;
    (void) state_byte_preempt_local;
    (void) failed_address;
    pok_fatal("Error in idle partition");
}


static const pok_part_ops_t partition_idle_operations =
{
    .start = partition_idle_thread,
    .process_partition_error = partition_idle_process_error
};

pok_partition_t partition_idle =
{
    .name = "Idle",

    .partition_event_max = 0,

    .period = 0,
    .space_id = 0,

    .part_sched_ops = &partition_sched_ops_kernel,
    .part_ops = &partition_idle_operations,

    .multi_partition_hm_selector = &pok_hm_multi_partition_selector_default,
    .multi_partition_hm_table = &pok_hm_multi_partition_table_default,
};
