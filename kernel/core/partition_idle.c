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
 *
 * Modified by Kirill Brazhnikov, 2026-11-16: high and low level requirements of
 * the functions, traced to the system requirements (docs/SRQs), and the steps
 * of their design linked to the LLRs.
 *
 * Modified by Kirill Brazhnikov, 2026-11-17: low level requirements of one
 * behaviour each; code that compiles to nothing (assertions, (void) x;,
 * empty branches) is left out of the design.
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
 * - \hlrid{HLR_PARTITION_IDLE_PARTITION_IDLE_THREAD_01}:
 *   `partition_idle_thread()` shall run nothing, for ever, in the time that the
 *   schedule gives to no partition.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_SCHED_002}
 *
 * @llr
 * - \llrid{LLR_PARTITION_IDLE_PARTITION_IDLE_THREAD_01}:
 *   `partition_idle_thread()` shall call `ja_inf_loop()`, which does not
 *   return.
 *   <br>Traces to:
 *   - \hlrref{HLR_PARTITION_IDLE_PARTITION_IDLE_THREAD_01}
 *
 * @design
 * 1. Call `ja_inf_loop()`. \llrref{LLR_PARTITION_IDLE_PARTITION_IDLE_THREAD_01}
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
 * - \hlrid{HLR_PARTITION_IDLE_PARTITION_IDLE_PROCESS_ERROR_01}:
 *   `partition_idle_process_error()` shall treat any error of the idle
 *   partition as fatal.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_HM_010}
 *
 * @llr
 * - \llrid{LLR_PARTITION_IDLE_PARTITION_IDLE_PROCESS_ERROR_01}:
 *   `partition_idle_process_error()` shall call `pok_fatal()` with `"Error in
 *   idle partition"`.
 *   <br>Traces to:
 *   - \hlrref{HLR_PARTITION_IDLE_PARTITION_IDLE_PROCESS_ERROR_01}
 *
 * @design
 * 1. Call `pok_fatal()` with the argument `"Error in idle partition"`.
 *    \llrref{LLR_PARTITION_IDLE_PARTITION_IDLE_PROCESS_ERROR_01}
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
