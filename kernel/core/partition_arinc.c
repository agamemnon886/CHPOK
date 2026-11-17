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
 * Modified by Kirill Brazhnikov, 2026-11-01: a partition restart also
 * resets the error handler configuration (CONFIGURE_ERROR_HANDLER).
 *
 * Modified by Kirill Brazhnikov, 2026-11-02: MISRA C:2025: __user/__kuser
 * are renamed JET_USER/JET_KUSER; braces around all if/else/loop bodies;
 * essentially Boolean conditions; U suffixes for unsigned constants;
 * discarded results are cast to void; operands of the same essential
 * type; deviations are documented where a guideline can't be followed;
 * the INIT_WARM/INIT_COLD switch-clauses are combined instead of falling
 * through.
 *
 * Modified by Kirill Brazhnikov, 2026-11-03: MISRA C:2025:
 * pok_sched_arinc_get_thread_at_index() rejects negative indexes explicitly;
 * objects used by one function are defined in it; operands of mixed precedence
 * are parenthesized; deviations are documented where a guideline can't be
 * followed; the ARINC partition is found through base_part.part_arinc instead
 * of container_of(). Long lines are wrapped at 80 columns. Delayed start times
 * and deadlines are computed with pok_time_add(), which cannot overflow.
 *
 * Modified by Kirill Brazhnikov, 2026-11-04: the GDB server callbacks
 * follow MISRA C:2025 (they are built on x86_64 now) and give the floating
 * point registers of a process (get_thread_fp_store); processes which
 * haven't been created don't exist for GDB; breakpoints are inserted again
 * after the partition's code is loaded.
 *
 * Modified by Kirill Brazhnikov, 2026-11-08: Doxygen headers for the file
 * and its functions.
 *
 * Modified by Kirill Brazhnikov, 2026-11-13: the structures and enums are used
 * by their typedef names: interrupt_frame, jet_fp_store_t,
 * pok_part_ops_t, pok_part_sched_ops_t.
 *
 * Modified by Kirill Brazhnikov, 2026-11-14: the enum types are named with _e:
 * pok_partition_mode_e (was pok_partition_mode_t), pok_ret_e (was pok_ret_t).
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
 * ARINC 653 partitions: their initialization, start (the image is loaded
 * and the main thread runs in INIT mode), the operating modes (INIT_COLD,
 * INIT_WARM, NORMAL, IDLE), the partition status and lock level, and the
 * thread information the GDB server shows.
 */

#include <config.h>

#include <core/partition_arinc.h>
#include <core/sched_arinc.h>
#include "thread_internal.h"
#include <common.h>
#include <asp/arch.h>
#include <core/uaccess.h>
#include <system_limits.h>

#include <cswitch.h>
#include <core/loader.h>
#include <alloc.h>

/*
 * MISRA C:2025 Rule 11.5 deviation (this file): the heap of a partition is
 * given by its address as a pointer to void.
 * MISRA C:2025 Rule 18.4 deviation (this file): the end of the heap of a
 * partition is its start plus its size.
 */


/**
 * @funcname `idle_func`
 *
 * @description
 * The context of a partition in IDLE mode: loops forever. Local preemption
 * isn't enabled here: if the partition went IDLE because of errors, idle
 * still has a good chance to work even with some partition data corrupted.
 *
 * @globals
 * None.
 *
 * @locals
 * None.
 *
 * @hlr
 * - \hlrid{HLR_PARTITION_ARINC_IDLE_FUNC_01}: `idle_func()` shall run none of
 *   the processes of a partition in `IDLE` mode, for ever.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_PART_007}
 *
 * @llr
 * - \llrid{LLR_PARTITION_ARINC_IDLE_FUNC_01}: `idle_func()` shall call
 *   `ja_inf_loop()`, which does not return, without enabling local preemption.
 *   <br>Traces to:
 *   - \hlrref{HLR_PARTITION_ARINC_IDLE_FUNC_01}
 *
 * @design
 * 1. Call `ja_inf_loop()`. \llrref{LLR_PARTITION_ARINC_IDLE_FUNC_01}
 *
 * @calls
 * - `ja_inf_loop()`
 */
static void idle_func(void)
{
    ja_inf_loop();
}

/**
 * @funcname `pok_partition_arinc_idle`
 *
 * @description
 * Puts the current partition into IDLE mode: its context restarts in a loop,
 * with local preemption disabled. Doesn't return.
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `current_partition` | `pok_partition_t *` | The running partition |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `part` | `pok_partition_arinc_t *` | The current partition |
 *
 * @hlr
 * - \hlrid{HLR_PARTITION_ARINC_POK_PARTITION_ARINC_IDLE_01}:
 *   `pok_partition_arinc_idle()` shall put the current partition into `IDLE`
 *   mode, in which none of its processes runs again, and not return.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_PART_014}
 *   - \srqref{SRQ_JET_PART_007}
 *   - \srqref{SRQ_JET_HM_004}
 *
 * @llr
 * - \llrid{LLR_PARTITION_ARINC_POK_PARTITION_ARINC_IDLE_01}:
 *   `pok_partition_arinc_idle()` shall disable local preemption of the
 *   partition and restart its context on its initial kernel stack in
 *   `idle_func()` with `ja_context_restart()`.
 *   <br>Traces to:
 *   - \hlrref{HLR_PARTITION_ARINC_POK_PARTITION_ARINC_IDLE_01}
 *
 * @design
 * 1. Set `part` to `current_partition_arinc`.
 *    \llrref{LLR_PARTITION_ARINC_POK_PARTITION_ARINC_IDLE_01}
 * 2. Set `part->base_part.preempt_local_disabled` to `TRUE`.
 *    \llrref{LLR_PARTITION_ARINC_POK_PARTITION_ARINC_IDLE_01}
 * 3. Call `ja_context_restart()` with the arguments
 *    `part->base_part.initial_sp` and the address of `idle_func`.
 *    \llrref{LLR_PARTITION_ARINC_POK_PARTITION_ARINC_IDLE_01}
 *
 * @calls
 * - `ja_context_restart()`
 */
void pok_partition_arinc_idle(void)
{
	pok_partition_arinc_t* part = current_partition_arinc;

	// Unconditionally off preemption.
	part->base_part.preempt_local_disabled = TRUE;

	jet_context_restart(part->base_part.initial_sp, &idle_func);
}

/* Helpers */

/**
 * @funcname `thread_reset`
 *
 * @description
 * Resets a thread at the partition's start: no name (not created).
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `t` | `pok_thread_t *` | [out] The thread. |
 *
 * @globals
 * None.
 *
 * @locals
 * None.
 *
 * @hlr
 * - \hlrid{HLR_PARTITION_ARINC_THREAD_RESET_01}: `thread_reset()` shall make a
 *   process slot of a partition one that holds no created process when the
 *   partition starts.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_PART_004}
 *
 * @llr
 * - \llrid{LLR_PARTITION_ARINC_THREAD_RESET_01}: `thread_reset()` shall set the
 *   first character of the thread's name to `'\0'`.
 *   <br>Traces to:
 *   - \hlrref{HLR_PARTITION_ARINC_THREAD_RESET_01}
 *
 * @design
 * 1. Set `t->name[0]` to `'\0'`. \llrref{LLR_PARTITION_ARINC_THREAD_RESET_01}
 *
 * @calls
 * None.
 */
static void thread_reset(pok_thread_t* t)
{
    t->name[0] = '\0';
    // Everything else will be set at thread creation time.
}


/**
 * @funcname `thread_init`
 *
 * @description
 * Initializes a thread at boot: allocates its kernel stack and its FP store.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `t` | `pok_thread_t *` | [out] The thread. |
 *
 * @globals
 * None.
 *
 * @locals
 * None.
 *
 * @hlr
 * - \hlrid{HLR_PARTITION_ARINC_THREAD_INIT_01}: `thread_init()` shall give a
 *   process slot, at boot, its kernel stack and the store of its floating point
 *   state.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_THR_013}
 *   - \srqref{SRQ_JET_MEM_010}
 *
 * @llr
 * - \llrid{LLR_PARTITION_ARINC_THREAD_INIT_01}: `thread_init()` shall allocate
 *   a kernel stack of `KERNEL_STACK_SIZE_DEFAULT` bytes with
 *   `pok_stack_alloc()` and an FP store with `ja_alloc_fp_store()`.
 *   <br>Traces to:
 *   - \hlrref{HLR_PARTITION_ARINC_THREAD_INIT_01}
 *
 * @design
 * 1. Call `pok_stack_alloc()` with the argument `KERNEL_STACK_SIZE_DEFAULT`,
 *    and store the result in `t->initial_sp`.
 *    \llrref{LLR_PARTITION_ARINC_THREAD_INIT_01}
 * 2. Call `ja_alloc_fp_store()`, and store the result in `t->fp_store`.
 *    \llrref{LLR_PARTITION_ARINC_THREAD_INIT_01}
 *
 * @calls
 * - `pok_stack_alloc()`
 * - `ja_alloc_fp_store()`
 */
static void thread_init(pok_thread_t* t)
{
    t->initial_sp = pok_stack_alloc(KERNEL_STACK_SIZE_DEFAULT);
    t->fp_store = ja_alloc_fp_store();
}

// This name is not accessible for user space

/**
 * @funcname `partition_arinc_start`
 *
 * @description
 * The start operation of an ARINC 653 partition: loads its image, sets up its
 * shared data, heap, ports, user stacks, scheduler queues and threads, creates
 * the main thread at the image's entry, and starts it.
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `current_partition` | `pok_partition_t *` | The running partition |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `heap_end` | `char *` | The end of the heap |
 * | `heap_start` | `char *` | The user address of the heap |
 * | `i` | `size_t` | The index of a port or a thread |
 * | `main_thread_name` | `static const char[30]` | The name of the main thread: "main" |
 * | `part` | `pok_partition_arinc_t *` | The current partition |
 * | `thread_main` | `pok_thread_t *` | The main thread |
 *
 * @hlr
 * - \hlrid{HLR_PARTITION_ARINC_PARTITION_ARINC_START_01}:
 *   `partition_arinc_start()` shall load the image of the current partition
 *   into its memory space each time the partition starts.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_PART_002}
 * - \hlrid{HLR_PARTITION_ARINC_PARTITION_ARINC_START_02}:
 *   `partition_arinc_start()` shall start the partition with empty state: its
 *   ports not created, its stacks free, no process created, no error handler,
 *   no pending events or errors.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_PART_004}
 *   - \srqref{SRQ_JET_IPC_004}
 *   - \srqref{SRQ_JET_MEM_018}
 * - \hlrid{HLR_PARTITION_ARINC_PARTITION_ARINC_START_03}:
 *   `partition_arinc_start()` shall set up the area that the kernel shares with
 *   the partition: the bounds of its heap, the number of its processes, its
 *   mode, and no current process.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_MEM_009}
 * - \hlrid{HLR_PARTITION_ARINC_PARTITION_ARINC_START_04}:
 *   `partition_arinc_start()` shall create the main process of the partition at
 *   the entry point of its image, with the stack size of the configuration, and
 *   run it.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_PART_013}
 * - \hlrid{HLR_PARTITION_ARINC_PARTITION_ARINC_START_05}:
 *   `partition_arinc_start()` shall start a partition that was restarted from
 *   outside (by the module or the monitor) in `COLD_START` mode.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_PART_001}
 *   - \srqref{SRQ_JET_PART_011}
 *   - \srqref{SRQ_JET_PART_008}
 * - \hlrid{HLR_PARTITION_ARINC_PARTITION_ARINC_START_06}:
 *   `partition_arinc_start()` shall let the GDB server put its breakpoints back
 *   into the image that was loaded.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_DBG_005}
 *
 * @llr
 * - \llrid{LLR_PARTITION_ARINC_PARTITION_ARINC_START_01}: If and only if
 *   `restarted_externally` of the base partition of `current_partition_arinc`
 *   is true, `partition_arinc_start()` shall set it to `FALSE` and `mode` of
 *   the partition to `POK_PARTITION_MODE_INIT_COLD`.
 *   <br>Traces to:
 *   - \hlrref{HLR_PARTITION_ARINC_PARTITION_ARINC_START_05}
 * - \llrid{LLR_PARTITION_ARINC_PARTITION_ARINC_START_02}:
 *   `partition_arinc_start()` shall call `jet_loader_elf_load()` once with
 *   `space_id` of the base partition minus 1, `space_id` and the address of
 *   `main_entry` of the partition.
 *   <br>Traces to:
 *   - \hlrref{HLR_PARTITION_ARINC_PARTITION_ARINC_START_01}
 * - \llrid{LLR_PARTITION_ARINC_PARTITION_ARINC_START_03}: With `POK_NEEDS_GDB`,
 *   `partition_arinc_start()` shall then call `jet_gdb_space_loaded()` once
 *   with `space_id`.
 *   <br>Traces to:
 *   - \hlrref{HLR_PARTITION_ARINC_PARTITION_ARINC_START_06}
 * - \llrid{LLR_PARTITION_ARINC_PARTITION_ARINC_START_04}:
 *   `partition_arinc_start()` shall set `kshd` of the partition to the result
 *   of `ja_space_shared_data()` for `space_id`.
 *   <br>Traces to:
 *   - \hlrref{HLR_PARTITION_ARINC_PARTITION_ARINC_START_03}
 * - \llrid{LLR_PARTITION_ARINC_PARTITION_ARINC_START_05}: If `heap_size` of the
 *   partition is greater than 0, `partition_arinc_start()` shall set
 *   `heap_start` of the shared data to the result of `ja_space_get_heap()` for
 *   `space_id` and `heap_end` to that address plus `heap_size`.
 *   <br>Traces to:
 *   - \hlrref{HLR_PARTITION_ARINC_PARTITION_ARINC_START_03}
 * - \llrid{LLR_PARTITION_ARINC_PARTITION_ARINC_START_06}: If `heap_size` of the
 *   partition is 0, `partition_arinc_start()` shall set `heap_start` and
 *   `heap_end` of the shared data to NULL.
 *   <br>Traces to:
 *   - \hlrref{HLR_PARTITION_ARINC_PARTITION_ARINC_START_03}
 * - \llrid{LLR_PARTITION_ARINC_PARTITION_ARINC_START_07}:
 *   `partition_arinc_start()` shall call `pok_port_queuing_init()` for each of
 *   the `nports_queuing` elements of `ports_queuing` and
 *   `pok_port_sampling_init()` for each of the `nports_sampling` elements of
 *   `ports_sampling`, `ja_ustack_init()` with `space_id`, `INIT_LIST_HEAD()`
 *   for `eligible_threads`, `delayed_event_queue_init()` for
 *   `partition_delayed_events` and `thread_reset()` for each of the `nthreads`
 *   elements of `threads`, set `nthreads_used` to 0 and `thread_current` to
 *   NULL, and, with `POK_NEEDS_ERROR_HANDLING`, set `thread_error` to NULL,
 *   `error_handler_concurrency` to `POK_ERROR_HANDLER_PROCESSES_PAUSE` and
 *   `error_handler_core` to `POK_CORE_AFFINITY_NO_PREFERENCE` and call
 *   `INIT_LIST_HEAD()` for `error_list`.
 *   <br>Traces to:
 *   - \hlrref{HLR_PARTITION_ARINC_PARTITION_ARINC_START_02}
 * - \llrid{LLR_PARTITION_ARINC_PARTITION_ARINC_START_08}:
 *   `partition_arinc_start()` shall set, in
 *   `threads[POK_PARTITION_ARINC_MAIN_THREAD_ID]`, `entry` to `main_entry`,
 *   `base_priority` to 0, `period` and `time_capacity` to `POK_TIME_INFINITY`,
 *   `deadline` to `DEADLINE_SOFT`, `name` to `"main"` (`strncpy()` with
 *   `MAX_NAME_LENGTH`) and `user_stack_size` to `main_user_stack_size` of the
 *   partition, call `thread_create()` once with that thread, and set
 *   `nthreads_used` to `POK_PARTITION_ARINC_MAIN_THREAD_ID` plus 1.
 *   <br>Traces to:
 *   - \hlrref{HLR_PARTITION_ARINC_PARTITION_ARINC_START_04}
 * - \llrid{LLR_PARTITION_ARINC_PARTITION_ARINC_START_09}: If `thread_create()`
 *   returns false for the main thread, `partition_arinc_start()` shall call
 *   `pok_assertion_fail()` (`unreachable()`).
 *   <br>Traces to:
 *   - \hlrref{HLR_PARTITION_ARINC_PARTITION_ARINC_START_04}
 * - \llrid{LLR_PARTITION_ARINC_PARTITION_ARINC_START_10}:
 *   `partition_arinc_start()` shall then set, in the shared data,
 *   `current_thread_id` to `JET_THREAD_ID_NONE`, `max_n_threads` to `nthreads`
 *   and `partition_mode` to `mode` of the partition.
 *   <br>Traces to:
 *   - \hlrref{HLR_PARTITION_ARINC_PARTITION_ARINC_START_03}
 * - \llrid{LLR_PARTITION_ARINC_PARTITION_ARINC_START_11}:
 *   `partition_arinc_start()` shall end with a call of `sched_arinc_start()`.
 *   <br>Traces to:
 *   - \hlrref{HLR_PARTITION_ARINC_PARTITION_ARINC_START_04}
 *
 * @design
 * 1. Set `part` to `current_partition_arinc`.
 *    \llrref{LLR_PARTITION_ARINC_PARTITION_ARINC_START_01},
 *    \llrref{LLR_PARTITION_ARINC_PARTITION_ARINC_START_02},
 *    \llrref{LLR_PARTITION_ARINC_PARTITION_ARINC_START_07}
 * 2. If `part->base_part.restarted_externally` is true:
 *    \llrref{LLR_PARTITION_ARINC_PARTITION_ARINC_START_01}
 *    1. Set `part->base_part.restarted_externally` to `FALSE`.
 *    2. Set `part->mode` to `POK_PARTITION_MODE_INIT_COLD`.
 * 3. Call `jet_loader_elf_load()` with the arguments
 *    `part->base_part.space_id - 1U`, `part->base_part.space_id` and the
 *    address of `part->main_entry`.
 *    \llrref{LLR_PARTITION_ARINC_PARTITION_ARINC_START_02}
 * 4. Call `jet_gdb_space_loaded()` with the argument
 *    `part->base_part.space_id`.
 *    \llrref{LLR_PARTITION_ARINC_PARTITION_ARINC_START_03}
 * 5. Call `ja_space_shared_data()` with the argument
 *    `part->base_part.space_id`, and store the result in `part->kshd`.
 *    \llrref{LLR_PARTITION_ARINC_PARTITION_ARINC_START_04}
 * 6. If `part->heap_size` is greater than `0U`:
 *    \llrref{LLR_PARTITION_ARINC_PARTITION_ARINC_START_05},
 *    \llrref{LLR_PARTITION_ARINC_PARTITION_ARINC_START_06}
 *    1. Call `ja_space_get_heap()` with the argument
 *       `part->base_part.space_id`, and store the result in `heap_start`.
 *       \llrref{LLR_PARTITION_ARINC_PARTITION_ARINC_START_05}
 *    2. Set `heap_end` to `heap_start + part->heap_size`.
 *       \llrref{LLR_PARTITION_ARINC_PARTITION_ARINC_START_05}
 *    3. Set `part->kshd->heap_start` to `heap_start`.
 *       \llrref{LLR_PARTITION_ARINC_PARTITION_ARINC_START_05}
 *    4. Set `part->kshd->heap_end` to `heap_end`.
 *       \llrref{LLR_PARTITION_ARINC_PARTITION_ARINC_START_05}
 * 7. Otherwise: \llrref{LLR_PARTITION_ARINC_PARTITION_ARINC_START_06}
 *    1. Set `part->kshd->heap_start` to `NULL`.
 *    2. Set `part->kshd->heap_end` to `NULL`.
 * 8. Set `i` to `0U`; then while `i` is less than `part->nports_queuing`,
 *    repeat (after each pass: increment `i`):
 *    \llrref{LLR_PARTITION_ARINC_PARTITION_ARINC_START_07}
 *    1. Call `pok_port_queuing_init()` with the address of
 *       `part->ports_queuing[i]`.
 * 9. Set `i` to `0U`; then while `i` is less than `part->nports_sampling`,
 *    repeat (after each pass: increment `i`):
 *    \llrref{LLR_PARTITION_ARINC_PARTITION_ARINC_START_07}
 *    1. Call `pok_port_sampling_init()` with the address of
 *       `part->ports_sampling[i]`.
 * 10. Call `ja_ustack_init()` with the argument `part->base_part.space_id`.
 *     \llrref{LLR_PARTITION_ARINC_PARTITION_ARINC_START_07}
 * 11. Call `INIT_LIST_HEAD()` with the address of `part->eligible_threads`.
 *     \llrref{LLR_PARTITION_ARINC_PARTITION_ARINC_START_07}
 * 12. Call `delayed_event_queue_init()` with the address of
 *     `part->partition_delayed_events`.
 *     \llrref{LLR_PARTITION_ARINC_PARTITION_ARINC_START_07}
 * 13. Set `i` to `0U`; then while `i` is less than `part->nthreads`, repeat
 *     (after each pass: increment `i`):
 *     \llrref{LLR_PARTITION_ARINC_PARTITION_ARINC_START_07}
 *     1. Call `thread_reset()` with the address of `part->threads[i]`.
 * 14. Set `part->nthreads_used` to `0`.
 *     \llrref{LLR_PARTITION_ARINC_PARTITION_ARINC_START_07}
 * 15. Set `part->thread_current` to `NULL`.
 *     \llrref{LLR_PARTITION_ARINC_PARTITION_ARINC_START_07}
 * 16. Set `part->thread_error` to `NULL`.
 *     \llrref{LLR_PARTITION_ARINC_PARTITION_ARINC_START_07}
 * 17. Set `part->error_handler_concurrency` to
 *     `POK_ERROR_HANDLER_PROCESSES_PAUSE`.
 *     \llrref{LLR_PARTITION_ARINC_PARTITION_ARINC_START_07}
 * 18. Set `part->error_handler_core` to `POK_CORE_AFFINITY_NO_PREFERENCE`.
 *     \llrref{LLR_PARTITION_ARINC_PARTITION_ARINC_START_07}
 * 19. Call `INIT_LIST_HEAD()` with the address of `part->error_list`.
 *     \llrref{LLR_PARTITION_ARINC_PARTITION_ARINC_START_07}
 * 20. Set `thread_main` to the address of
 *     `part->threads[POK_PARTITION_ARINC_MAIN_THREAD_ID]`.
 *     \llrref{LLR_PARTITION_ARINC_PARTITION_ARINC_START_08}
 * 21. Set `thread_main->entry` to `(void* JET_USER)part->main_entry`.
 *     \llrref{LLR_PARTITION_ARINC_PARTITION_ARINC_START_08}
 * 22. Set `thread_main->base_priority` to `0`.
 *     \llrref{LLR_PARTITION_ARINC_PARTITION_ARINC_START_08}
 * 23. Set `thread_main->period` to `POK_TIME_INFINITY`.
 *     \llrref{LLR_PARTITION_ARINC_PARTITION_ARINC_START_08}
 * 24. Set `thread_main->time_capacity` to `POK_TIME_INFINITY`.
 *     \llrref{LLR_PARTITION_ARINC_PARTITION_ARINC_START_08}
 * 25. Set `thread_main->deadline` to `DEADLINE_SOFT`.
 *     \llrref{LLR_PARTITION_ARINC_PARTITION_ARINC_START_08}
 * 26. Call `strncpy()` with the arguments `thread_main->name`,
 *     `main_thread_name` and `MAX_NAME_LENGTH`.
 *     \llrref{LLR_PARTITION_ARINC_PARTITION_ARINC_START_08}
 * 27. Set `thread_main->user_stack_size` to `part->main_user_stack_size`.
 *     \llrref{LLR_PARTITION_ARINC_PARTITION_ARINC_START_08}
 * 28. If `thread_create(thread_main)` is false:
 *     \llrref{LLR_PARTITION_ARINC_PARTITION_ARINC_START_08},
 *     \llrref{LLR_PARTITION_ARINC_PARTITION_ARINC_START_09}
 *     1. Use the macro `unreachable()`.
 *        \llrref{LLR_PARTITION_ARINC_PARTITION_ARINC_START_09}
 * 29. Set `part->nthreads_used` to `POK_PARTITION_ARINC_MAIN_THREAD_ID + 1U`.
 *     \llrref{LLR_PARTITION_ARINC_PARTITION_ARINC_START_08}
 * 30. Set `part->kshd->current_thread_id` to `JET_THREAD_ID_NONE`.
 *     \llrref{LLR_PARTITION_ARINC_PARTITION_ARINC_START_10}
 * 31. Set `part->kshd->max_n_threads` to `part->nthreads`.
 *     \llrref{LLR_PARTITION_ARINC_PARTITION_ARINC_START_10}
 * 32. Set `part->kshd->partition_mode` to `part->mode`.
 *     \llrref{LLR_PARTITION_ARINC_PARTITION_ARINC_START_10}
 * 33. Call `sched_arinc_start()`.
 *     \llrref{LLR_PARTITION_ARINC_PARTITION_ARINC_START_11}
 *
 * @calls
 * - `jet_loader_elf_load()`
 * - `jet_gdb_space_loaded()`
 * - `ja_space_shared_data()`
 * - `ja_space_get_heap()`
 * - `pok_port_queuing_init()`
 * - `pok_port_sampling_init()`
 * - `ja_ustack_init()`
 * - `INIT_LIST_HEAD()`
 * - `delayed_event_queue_init()`
 * - `thread_reset()`
 * - `strncpy()`
 * - `thread_create()`
 * - `pok_assertion_fail()`
 * - `sched_arinc_start()`
 */
static void partition_arinc_start(void)
{
    static const char main_thread_name[MAX_NAME_LENGTH] = "main";

    pok_partition_arinc_t* part = current_partition_arinc;

	if(part->base_part.restarted_externally)
	{
		part->base_part.restarted_externally = FALSE;
		part->mode = POK_PARTITION_MODE_INIT_COLD;
	}

	jet_loader_elf_load(part->base_part.space_id - 1U, /* elf_id*/
		part->base_part.space_id,
		&part->main_entry);
#ifdef POK_NEEDS_GDB
	// Loading has overwritten the breakpoints inserted into the code.
	jet_gdb_space_loaded(part->base_part.space_id);
#endif /* POK_NEEDS_GDB */

	part->kshd = ja_space_shared_data(part->base_part.space_id);

	if(part->heap_size > 0U) {
       char JET_USER *heap_start = ja_space_get_heap(part->base_part.space_id);
       char JET_USER *heap_end = heap_start + part->heap_size;

       part->kshd->heap_start = heap_start;
       part->kshd->heap_end = heap_end;
    }
    else {
       // No heap is requested.
       part->kshd->heap_start = NULL;
       part->kshd->heap_end = NULL;
    }

	for(size_t i = 0U; i < part->nports_queuing; i++)
	{
		pok_port_queuing_init(&part->ports_queuing[i]);
	}

	for(size_t i = 0U; i < part->nports_sampling; i++)
	{
		pok_port_sampling_init(&part->ports_sampling[i]);
	}

	ja_ustack_init(part->base_part.space_id);

	INIT_LIST_HEAD(&part->eligible_threads);
	delayed_event_queue_init(&part->partition_delayed_events);

	for(uint32_t i = 0U; i < part->nthreads; i++)
	{
		thread_reset(&part->threads[i]);
	}

	part->nthreads_used = 0;

	part->thread_current = NULL;
#ifdef POK_NEEDS_ERROR_HANDLING
	part->thread_error = NULL;
	part->error_handler_concurrency = POK_ERROR_HANDLER_PROCESSES_PAUSE;
	part->error_handler_core = POK_CORE_AFFINITY_NO_PREFERENCE;
	INIT_LIST_HEAD(&part->error_list);
#endif

    pok_thread_t* thread_main = &part->threads[POK_PARTITION_ARINC_MAIN_THREAD_ID];

	/* MISRA C:2025 Rule 11.1 deviation: thread entry points are user
	 * addresses. */
	thread_main->entry = (void* JET_USER)part->main_entry;
	thread_main->base_priority = 0;
	thread_main->period = POK_TIME_INFINITY;
	thread_main->time_capacity = POK_TIME_INFINITY;
	thread_main->deadline = DEADLINE_SOFT;
	(void) strncpy(thread_main->name, main_thread_name, MAX_NAME_LENGTH);
	thread_main->user_stack_size = part->main_user_stack_size;

    if(!thread_create(thread_main)) {
    	unreachable(); // Configurator should check stack size for main thread.
    }

	part->nthreads_used = POK_PARTITION_ARINC_MAIN_THREAD_ID + 1U;

	// Fill initial kernel shared data.
	part->kshd->current_thread_id = JET_THREAD_ID_NONE;
	part->kshd->max_n_threads = part->nthreads;
	part->kshd->partition_mode = part->mode;

	sched_arinc_start();

	/* Current context is lost and may be reused for "do_nothing" thread
	 * or for IDLE partition's mode.
	 */
}

/**
 * @funcname `pok_partition_arinc_reset`
 *
 * @description
 * Restarts the current partition in INIT_COLD or INIT_WARM mode (a warm start
 * can't follow a cold one). Doesn't return.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `mode` | `pok_partition_mode_e` | [in] INIT_COLD or INIT_WARM. |
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `current_partition` | `pok_partition_t *` | The running partition |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `part` | `pok_partition_arinc_t *` | The current partition |
 *
 * @hlr
 * - \hlrid{HLR_PARTITION_ARINC_POK_PARTITION_ARINC_RESET_01}:
 *   `pok_partition_arinc_reset()` shall restart the current partition in
 *   `COLD_START` or `WARM_START` mode, a warm start not following a cold one,
 *   and not return.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_PART_005}
 *   - \srqref{SRQ_JET_HM_004}
 *
 * @llr
 * - \llrid{LLR_PARTITION_ARINC_POK_PARTITION_ARINC_RESET_01}:
 *   `pok_partition_arinc_reset()` shall set `preempt_local_disabled` of the
 *   base partition to `TRUE` and `mode` of the partition to `mode`, and then
 *   call `pok_partition_restart()`.
 *   <br>Traces to:
 *   - \hlrref{HLR_PARTITION_ARINC_POK_PARTITION_ARINC_RESET_01}
 *
 * @design
 * 1. Set `part` to `current_partition_arinc`.
 *    \llrref{LLR_PARTITION_ARINC_POK_PARTITION_ARINC_RESET_01}
 * 2. Set `part->base_part.preempt_local_disabled` to `TRUE`.
 *    \llrref{LLR_PARTITION_ARINC_POK_PARTITION_ARINC_RESET_01}
 * 3. Set `part->mode` to `mode`.
 *    \llrref{LLR_PARTITION_ARINC_POK_PARTITION_ARINC_RESET_01}
 * 4. Call `pok_partition_restart()`.
 *    \llrref{LLR_PARTITION_ARINC_POK_PARTITION_ARINC_RESET_01}
 *
 * @calls
 * - `pok_partition_restart()`
 */
void pok_partition_arinc_reset(pok_partition_mode_e mode)
{
	pok_partition_arinc_t* part = current_partition_arinc;

	part->base_part.preempt_local_disabled = TRUE;

	assert((mode == POK_PARTITION_MODE_INIT_WARM)
		|| (mode == POK_PARTITION_MODE_INIT_COLD));

	assert((mode == POK_PARTITION_MODE_INIT_COLD) ||
		(part->mode != POK_PARTITION_MODE_INIT_COLD));

	part->mode = mode;

	pok_partition_restart();
}

#ifdef POK_NEEDS_GDB
/**
 * @funcname `pok_sched_arinc_get_number_of_threads`
 *
 * @description
 * The GDB operation of ARINC 653 partitions: the number of threads, the idle
 * context counting as one.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `part` | `pok_partition_t *` | [in] The partition. |
 *
 * @return The number of threads plus 1.
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `part_arinc` | `const pok_partition_arinc_t *` | The ARINC 653 partition |
 *
 * @hlr
 * - \hlrid{HLR_PARTITION_ARINC_POK_SCHED_ARINC_GET_NUMBER_OF_THREADS_01}:
 *   `pok_sched_arinc_get_number_of_threads()` shall tell the GDB server the
 *   number of threads of a partition: its process slots and the idle thread.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_DBG_002}
 *
 * @llr
 * - \llrid{LLR_PARTITION_ARINC_POK_SCHED_ARINC_GET_NUMBER_OF_THREADS_01}:
 *   `pok_sched_arinc_get_number_of_threads()` shall return `nthreads + 1`.
 *   <br>Traces to:
 *   - \hlrref{HLR_PARTITION_ARINC_POK_SCHED_ARINC_GET_NUMBER_OF_THREADS_01}
 *
 * @design
 * 1. Set `part_arinc` to `part->part_arinc`.
 *    \llrref{LLR_PARTITION_ARINC_POK_SCHED_ARINC_GET_NUMBER_OF_THREADS_01}
 * 2. Return `(int32_t)part_arinc->nthreads + 1`.
 *    \llrref{LLR_PARTITION_ARINC_POK_SCHED_ARINC_GET_NUMBER_OF_THREADS_01}
 *
 * @calls
 * None.
 */
static int32_t pok_sched_arinc_get_number_of_threads(pok_partition_t* part)
{
	const pok_partition_arinc_t* part_arinc = part->part_arinc;

	return (int32_t)part_arinc->nthreads + 1; //IDLE thread
}

/**
 * @funcname `pok_sched_arinc_get_current_thread_index`
 *
 * @description
 * The GDB operation of ARINC 653 partitions: the index of the current thread
 * (the last index for the idle context).
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `part` | `pok_partition_t *` | [in] The partition. |
 *
 * @return The index.
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `i` | `uint32_t` | The index of a thread |
 * | `index` | `uint32_t` | The index found |
 * | `part_arinc` | `const pok_partition_arinc_t *` | The ARINC 653 partition |
 *
 * @hlr
 * - \hlrid{HLR_PARTITION_ARINC_POK_SCHED_ARINC_GET_CURRENT_THREAD_INDEX_01}:
 *   `pok_sched_arinc_get_current_thread_index()` shall tell the GDB server the
 *   index of the current thread of a partition, the index after the last
 *   process standing for the idle thread.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_DBG_002}
 *
 * @llr
 * - \llrid{LLR_PARTITION_ARINC_POK_SCHED_ARINC_GET_CURRENT_THREAD_INDEX_01}: If
 *   `thread_current` of the partition (`part_arinc` of `part`) is the address
 *   of one of its `nthreads` elements of `threads`,
 *   `pok_sched_arinc_get_current_thread_index()` shall return the index of that
 *   element.
 *   <br>Traces to:
 *   - \hlrref{HLR_PARTITION_ARINC_POK_SCHED_ARINC_GET_CURRENT_THREAD_INDEX_01}
 * - \llrid{LLR_PARTITION_ARINC_POK_SCHED_ARINC_GET_CURRENT_THREAD_INDEX_02}: If
 *   `thread_current` is the address of none of them,
 *   `pok_sched_arinc_get_current_thread_index()` shall return `nthreads`.
 *   <br>Traces to:
 *   - \hlrref{HLR_PARTITION_ARINC_POK_SCHED_ARINC_GET_CURRENT_THREAD_INDEX_01}
 *
 * @design
 * 1. Set `part_arinc` to `part->part_arinc`.
 *    \llrref{LLR_PARTITION_ARINC_POK_SCHED_ARINC_GET_CURRENT_THREAD_INDEX_01},
 *    \llrref{LLR_PARTITION_ARINC_POK_SCHED_ARINC_GET_CURRENT_THREAD_INDEX_02}
 * 2. Set `index` to `part_arinc->nthreads`.
 *    \llrref{LLR_PARTITION_ARINC_POK_SCHED_ARINC_GET_CURRENT_THREAD_INDEX_01},
 *    \llrref{LLR_PARTITION_ARINC_POK_SCHED_ARINC_GET_CURRENT_THREAD_INDEX_02}
 * 3. Set `i` to `0U`; then while `i` is less than `part_arinc->nthreads`,
 *    repeat (after each pass: increment `i`):
 *    \llrref{LLR_PARTITION_ARINC_POK_SCHED_ARINC_GET_CURRENT_THREAD_INDEX_01},
 *    \llrref{LLR_PARTITION_ARINC_POK_SCHED_ARINC_GET_CURRENT_THREAD_INDEX_02}
 *    1. If the address of `part_arinc->threads[i]` is equal to
 *       `part_arinc->thread_current`:
 *       1. Set `index` to `i`.
 * 4. Return `(int32_t)index`.
 *    \llrref{LLR_PARTITION_ARINC_POK_SCHED_ARINC_GET_CURRENT_THREAD_INDEX_01},
 *    \llrref{LLR_PARTITION_ARINC_POK_SCHED_ARINC_GET_CURRENT_THREAD_INDEX_02}
 *
 * @calls
 * None.
 */
static int32_t pok_sched_arinc_get_current_thread_index(pok_partition_t* part)
{
	const pok_partition_arinc_t* part_arinc = part->part_arinc;
	uint32_t index = part_arinc->nthreads; //IDLE thread

	for(uint32_t i = 0U; i < part_arinc->nthreads; i++)
	{
		if(&part_arinc->threads[i] == part_arinc->thread_current) {
			index = i;
		}
	}

	return (int32_t)index;
}

/**
 * @funcname `arinc_thread_is_created`
 *
 * @description
 * Tells whether the thread at an index is created.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `part` | `const pok_partition_arinc_t *` | [in] The ARINC 653 partition. |
 * | `index` | `int32_t` | [in] The index. |
 *
 * @return TRUE if the index is below the number of threads used.
 *
 * @globals
 * None.
 *
 * @locals
 * None.
 *
 * @hlr
 * - \hlrid{HLR_PARTITION_ARINC_ARINC_THREAD_IS_CREATED_01}:
 *   `arinc_thread_is_created()` shall tell whether the process slot at an index
 *   of a partition holds a created process.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_DBG_002}
 *
 * @llr
 * - \llrid{LLR_PARTITION_ARINC_ARINC_THREAD_IS_CREATED_01}: If `index` is not
 *   less than 0 and is less than `nthreads_used` of `part`,
 *   `arinc_thread_is_created()` shall return true.
 *   <br>Traces to:
 *   - \hlrref{HLR_PARTITION_ARINC_ARINC_THREAD_IS_CREATED_01}
 * - \llrid{LLR_PARTITION_ARINC_ARINC_THREAD_IS_CREATED_02}: If `index` is less
 *   than 0 or is not less than `nthreads_used` of `part`,
 *   `arinc_thread_is_created()` shall return false.
 *   <br>Traces to:
 *   - \hlrref{HLR_PARTITION_ARINC_ARINC_THREAD_IS_CREATED_01}
 *
 * @design
 * 1. Return `(index >= 0) && ((uint32_t) index < part->nthreads_used)`.
 *    \llrref{LLR_PARTITION_ARINC_ARINC_THREAD_IS_CREATED_01},
 *    \llrref{LLR_PARTITION_ARINC_ARINC_THREAD_IS_CREATED_02}
 *
 * @calls
 * None.
 */
static pok_bool_t arinc_thread_is_created(const pok_partition_arinc_t* part,
	int32_t index)
{
	return (index >= 0) && ((uint32_t) index < part->nthreads_used);
}

/**
 * @funcname `pok_sched_arinc_get_thread_at_index`
 *
 * @description
 * The GDB operation of ARINC 653 partitions: the thread at an index (NULL for
 * the idle context).
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `part` | `pok_partition_t *` | [in] The partition. |
 * | `index` | `int32_t` | [in] The index. |
 * | `private` | `void **` | [out] Where the thread is stored. |
 *
 * @return 0 if there is one, 1 otherwise.
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `part_arinc` | `pok_partition_arinc_t *` | The ARINC 653 partition |
 * | `ret` | `int32_t` | The result |
 *
 * @hlr
 * - \hlrid{HLR_PARTITION_ARINC_POK_SCHED_ARINC_GET_THREAD_AT_INDEX_01}:
 *   `pok_sched_arinc_get_thread_at_index()` shall tell the GDB server whether a
 *   partition has a thread at an index (the idle thread at index `nthreads`),
 *   and give it.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_DBG_002}
 *
 * @llr
 * - \llrid{LLR_PARTITION_ARINC_POK_SCHED_ARINC_GET_THREAD_AT_INDEX_01}: If
 *   `index` is not less than 0 and is `nthreads` of the partition (`part_arinc`
 *   of `part`), `pok_sched_arinc_get_thread_at_index()` shall store NULL at
 *   `private` and return 0.
 *   <br>Traces to:
 *   - \hlrref{HLR_PARTITION_ARINC_POK_SCHED_ARINC_GET_THREAD_AT_INDEX_01}
 * - \llrid{LLR_PARTITION_ARINC_POK_SCHED_ARINC_GET_THREAD_AT_INDEX_02}: If
 *   `index` is not `nthreads` and `arinc_thread_is_created()` returns true for
 *   the partition and `index`, `pok_sched_arinc_get_thread_at_index()` shall
 *   store the address of `threads[index]` at `private` and return 0.
 *   <br>Traces to:
 *   - \hlrref{HLR_PARTITION_ARINC_POK_SCHED_ARINC_GET_THREAD_AT_INDEX_01}
 * - \llrid{LLR_PARTITION_ARINC_POK_SCHED_ARINC_GET_THREAD_AT_INDEX_03}: If
 *   `index` is not `nthreads` and `arinc_thread_is_created()` returns false,
 *   `pok_sched_arinc_get_thread_at_index()` shall return 1 and leave the
 *   pointer at `private` as it is.
 *   <br>Traces to:
 *   - \hlrref{HLR_PARTITION_ARINC_POK_SCHED_ARINC_GET_THREAD_AT_INDEX_01}
 *
 * @design
 * 1. Set `part_arinc` to `part->part_arinc`.
 *    \llrref{LLR_PARTITION_ARINC_POK_SCHED_ARINC_GET_THREAD_AT_INDEX_01},
 *    \llrref{LLR_PARTITION_ARINC_POK_SCHED_ARINC_GET_THREAD_AT_INDEX_02},
 *    \llrref{LLR_PARTITION_ARINC_POK_SCHED_ARINC_GET_THREAD_AT_INDEX_03}
 * 2. Set `ret` to `0`.
 *    \llrref{LLR_PARTITION_ARINC_POK_SCHED_ARINC_GET_THREAD_AT_INDEX_01},
 *    \llrref{LLR_PARTITION_ARINC_POK_SCHED_ARINC_GET_THREAD_AT_INDEX_02},
 *    \llrref{LLR_PARTITION_ARINC_POK_SCHED_ARINC_GET_THREAD_AT_INDEX_03}
 * 3. If `index` is greater than or equal to `0` and `(uint32_t) index` is equal
 *    to `part_arinc->nthreads`:
 *    \llrref{LLR_PARTITION_ARINC_POK_SCHED_ARINC_GET_THREAD_AT_INDEX_01}
 *    1. Set `*private` to `NULL`.
 * 4. Otherwise, if `arinc_thread_is_created(part_arinc, index)` is true:
 *    \llrref{LLR_PARTITION_ARINC_POK_SCHED_ARINC_GET_THREAD_AT_INDEX_02}
 *    1. Set `*private` to the address of `part_arinc->threads[index]`.
 * 5. Otherwise:
 *    \llrref{LLR_PARTITION_ARINC_POK_SCHED_ARINC_GET_THREAD_AT_INDEX_03}
 *    1. Set `ret` to `1`.
 * 6. Return `ret`.
 *    \llrref{LLR_PARTITION_ARINC_POK_SCHED_ARINC_GET_THREAD_AT_INDEX_01},
 *    \llrref{LLR_PARTITION_ARINC_POK_SCHED_ARINC_GET_THREAD_AT_INDEX_02},
 *    \llrref{LLR_PARTITION_ARINC_POK_SCHED_ARINC_GET_THREAD_AT_INDEX_03}
 *
 * @calls
 * - `arinc_thread_is_created()`
 */
static int32_t pok_sched_arinc_get_thread_at_index(pok_partition_t* part,
	int32_t index, void** private)
{
	pok_partition_arinc_t* part_arinc = part->part_arinc;
	int32_t ret = 0;

	if((index >= 0) && ((uint32_t) index == part_arinc->nthreads)) {
		*private = NULL;
	} else if(arinc_thread_is_created(part_arinc, index)) {
		*private = &part_arinc->threads[index];
	} else {
		ret = 1;
	}

	return ret;
}

/**
 * @funcname `arinc_thread_write_str`
 *
 * @description
 * Prints a string with the GDB server's print callback.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `print_cb` | `print_cb_t` | [in] The callback. |
 * | `cb_data` | `void *` | [in] The data passed to the callback. |
 * | `s` | `const char *` | [in] The string. |
 *
 * @globals
 * None.
 *
 * @locals
 * None.
 *
 * @hlr
 * - \hlrid{HLR_PARTITION_ARINC_ARINC_THREAD_WRITE_STR_01}:
 *   `arinc_thread_write_str()` shall give a string to the print callback of the
 *   GDB server.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_DBG_002}
 *
 * @llr
 * - \llrid{LLR_PARTITION_ARINC_ARINC_THREAD_WRITE_STR_01}:
 *   `arinc_thread_write_str()` shall call `print_cb` once with `s`, the result
 *   of `strlen()` for `s` and `cb_data`.
 *   <br>Traces to:
 *   - \hlrref{HLR_PARTITION_ARINC_ARINC_THREAD_WRITE_STR_01}
 *
 * @design
 * 1. Call the function `print_cb` with the arguments `s`, `strlen(s)` and
 *    `cb_data`. \llrref{LLR_PARTITION_ARINC_ARINC_THREAD_WRITE_STR_01}
 *
 * @calls
 * - `print_cb()` (indirect)
 * - `strlen()`
 */
static void arinc_thread_write_str(print_cb_t print_cb, void* cb_data,
	const char* s)
{
	print_cb(s, strlen(s), cb_data);
}

/**
 * @funcname `pok_sched_arinc_get_thread_info`
 *
 * @description
 * The GDB operation of ARINC 653 partitions: the description of a thread, its
 * name and state (Stopped, Waiting, Suspended, Running or Ready), "IDLE" for
 * the idle context, "[Not created]" for an unused index.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `part` | `pok_partition_t *` | [in] The partition. |
 * | `index` | `int32_t` | [in] The index. |
 * | `private` | `void *` | [in] The thread, or NULL for the idle context. |
 * | `print_cb` | `print_cb_t` | [in] The print callback. |
 * | `cb_data` | `void *` | [in] The data passed to the callback. |
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `part_arinc` | `const pok_partition_arinc_t *` | The ARINC 653 partition |
 * | `state` | `const char *` | The name of the thread's state |
 * | `t` | `const pok_thread_t *` | The thread |
 *
 * @hlr
 * - \hlrid{HLR_PARTITION_ARINC_POK_SCHED_ARINC_GET_THREAD_INFO_01}:
 *   `pok_sched_arinc_get_thread_info()` shall give the GDB server the
 *   description of a thread of a partition: `"IDLE"` for the idle thread, the
 *   name of the main process, the name and state (Stopped, Waiting, Suspended,
 *   Running or Ready) of another created process, and `"[Not created]"`
 *   otherwise.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_DBG_002}
 *
 * @llr
 * - \llrid{LLR_PARTITION_ARINC_POK_SCHED_ARINC_GET_THREAD_INFO_01}: If
 *   `private` is NULL, `pok_sched_arinc_get_thread_info()` shall call
 *   `arinc_thread_write_str()` once with `print_cb`, `cb_data` and `"IDLE"`.
 *   <br>Traces to:
 *   - \hlrref{HLR_PARTITION_ARINC_POK_SCHED_ARINC_GET_THREAD_INFO_01}
 * - \llrid{LLR_PARTITION_ARINC_POK_SCHED_ARINC_GET_THREAD_INFO_02}: If
 *   `private` is not NULL and `index` is `POK_PARTITION_ARINC_MAIN_THREAD_ID`,
 *   `pok_sched_arinc_get_thread_info()` shall call `arinc_thread_write_str()`
 *   once with `print_cb`, `cb_data` and `name` of the thread.
 *   <br>Traces to:
 *   - \hlrref{HLR_PARTITION_ARINC_POK_SCHED_ARINC_GET_THREAD_INFO_01}
 * - \llrid{LLR_PARTITION_ARINC_POK_SCHED_ARINC_GET_THREAD_INFO_03}: If
 *   `private` is not NULL, `index` is not that of the main thread and
 *   `arinc_thread_is_created()` returns true for the partition and `index`,
 *   `pok_sched_arinc_get_thread_info()` shall call `arinc_thread_write_str()`
 *   with `print_cb`, `cb_data` and, in this order, `name` of the thread, `" "`
 *   and the state below.
 *   <br>Traces to:
 *   - \hlrref{HLR_PARTITION_ARINC_POK_SCHED_ARINC_GET_THREAD_INFO_01}
 * - \llrid{LLR_PARTITION_ARINC_POK_SCHED_ARINC_GET_THREAD_INFO_04}: If `state`
 *   of the thread is `POK_STATE_STOPPED` or `POK_STATE_WAITING`,
 *   `pok_sched_arinc_get_thread_info()` shall take `"Stopped"` or `"Waiting"`,
 *   in this order, as the state.
 *   <br>Traces to:
 *   - \hlrref{HLR_PARTITION_ARINC_POK_SCHED_ARINC_GET_THREAD_INFO_01}
 * - \llrid{LLR_PARTITION_ARINC_POK_SCHED_ARINC_GET_THREAD_INFO_05}: If `state`
 *   of the thread is `POK_STATE_RUNNABLE` and its `suspended` is true,
 *   `pok_sched_arinc_get_thread_info()` shall take `"Suspended"` as the state.
 *   <br>Traces to:
 *   - \hlrref{HLR_PARTITION_ARINC_POK_SCHED_ARINC_GET_THREAD_INFO_01}
 * - \llrid{LLR_PARTITION_ARINC_POK_SCHED_ARINC_GET_THREAD_INFO_06}: If `state`
 *   of the thread is `POK_STATE_RUNNABLE`, its `suspended` is false and it is
 *   `thread_current` of the partition, `pok_sched_arinc_get_thread_info()`
 *   shall take `"Running"` as the state.
 *   <br>Traces to:
 *   - \hlrref{HLR_PARTITION_ARINC_POK_SCHED_ARINC_GET_THREAD_INFO_01}
 * - \llrid{LLR_PARTITION_ARINC_POK_SCHED_ARINC_GET_THREAD_INFO_07}: If `state`
 *   of the thread is `POK_STATE_RUNNABLE`, its `suspended` is false and it is
 *   not `thread_current` of the partition, `pok_sched_arinc_get_thread_info()`
 *   shall take `"Ready"` as the state.
 *   <br>Traces to:
 *   - \hlrref{HLR_PARTITION_ARINC_POK_SCHED_ARINC_GET_THREAD_INFO_01}
 * - \llrid{LLR_PARTITION_ARINC_POK_SCHED_ARINC_GET_THREAD_INFO_08}: If `state`
 *   of the thread is none of `POK_STATE_STOPPED`, `POK_STATE_WAITING` and
 *   `POK_STATE_RUNNABLE`, `pok_sched_arinc_get_thread_info()` shall call
 *   `pok_assertion_fail()` (`unreachable()`).
 *   <br>Traces to:
 *   - \hlrref{HLR_PARTITION_ARINC_POK_SCHED_ARINC_GET_THREAD_INFO_01}
 * - \llrid{LLR_PARTITION_ARINC_POK_SCHED_ARINC_GET_THREAD_INFO_09}: If
 *   `private` is not NULL, `index` is not that of the main thread and
 *   `arinc_thread_is_created()` returns false,
 *   `pok_sched_arinc_get_thread_info()` shall call `arinc_thread_write_str()`
 *   once with `print_cb`, `cb_data` and `"[Not created]"`.
 *   <br>Traces to:
 *   - \hlrref{HLR_PARTITION_ARINC_POK_SCHED_ARINC_GET_THREAD_INFO_01}
 *
 * @design
 * 1. Set `t` to `private`.
 *    \llrref{LLR_PARTITION_ARINC_POK_SCHED_ARINC_GET_THREAD_INFO_01},
 *    \llrref{LLR_PARTITION_ARINC_POK_SCHED_ARINC_GET_THREAD_INFO_02},
 *    \llrref{LLR_PARTITION_ARINC_POK_SCHED_ARINC_GET_THREAD_INFO_03},
 *    \llrref{LLR_PARTITION_ARINC_POK_SCHED_ARINC_GET_THREAD_INFO_09}
 * 2. Set `part_arinc` to `part->part_arinc`.
 *    \llrref{LLR_PARTITION_ARINC_POK_SCHED_ARINC_GET_THREAD_INFO_03}
 * 3. If `t` is NULL:
 *    \llrref{LLR_PARTITION_ARINC_POK_SCHED_ARINC_GET_THREAD_INFO_01}
 *    1. Call `arinc_thread_write_str()` with the arguments `print_cb`,
 *       `cb_data` and `"IDLE"`.
 * 4. Otherwise, if `index` is equal to
 *    `(int32_t)POK_PARTITION_ARINC_MAIN_THREAD_ID`:
 *    \llrref{LLR_PARTITION_ARINC_POK_SCHED_ARINC_GET_THREAD_INFO_02}
 *    1. Call `arinc_thread_write_str()` with the arguments `print_cb`,
 *       `cb_data` and `t->name`.
 * 5. Otherwise, if `arinc_thread_is_created(part_arinc, index)` is true:
 *    \llrref{LLR_PARTITION_ARINC_POK_SCHED_ARINC_GET_THREAD_INFO_03},
 *    \llrref{LLR_PARTITION_ARINC_POK_SCHED_ARINC_GET_THREAD_INFO_04},
 *    \llrref{LLR_PARTITION_ARINC_POK_SCHED_ARINC_GET_THREAD_INFO_05},
 *    \llrref{LLR_PARTITION_ARINC_POK_SCHED_ARINC_GET_THREAD_INFO_06},
 *    \llrref{LLR_PARTITION_ARINC_POK_SCHED_ARINC_GET_THREAD_INFO_07},
 *    \llrref{LLR_PARTITION_ARINC_POK_SCHED_ARINC_GET_THREAD_INFO_08}
 *    1. Set `state` to `""`.
 *       \llrref{LLR_PARTITION_ARINC_POK_SCHED_ARINC_GET_THREAD_INFO_03}
 *    2. Depending on `t->state`:
 *       \llrref{LLR_PARTITION_ARINC_POK_SCHED_ARINC_GET_THREAD_INFO_04},
 *       \llrref{LLR_PARTITION_ARINC_POK_SCHED_ARINC_GET_THREAD_INFO_05},
 *       \llrref{LLR_PARTITION_ARINC_POK_SCHED_ARINC_GET_THREAD_INFO_06},
 *       \llrref{LLR_PARTITION_ARINC_POK_SCHED_ARINC_GET_THREAD_INFO_07},
 *       \llrref{LLR_PARTITION_ARINC_POK_SCHED_ARINC_GET_THREAD_INFO_08}
 *       1. Case `POK_STATE_STOPPED`:
 *          \llrref{LLR_PARTITION_ARINC_POK_SCHED_ARINC_GET_THREAD_INFO_04}
 *          1. Set `state` to `"Stopped"`.
 *          2. Leave the switch.
 *       2. Case `POK_STATE_WAITING`:
 *          \llrref{LLR_PARTITION_ARINC_POK_SCHED_ARINC_GET_THREAD_INFO_04}
 *          1. Set `state` to `"Waiting"`.
 *          2. Leave the switch.
 *       3. Case `POK_STATE_RUNNABLE`:
 *          \llrref{LLR_PARTITION_ARINC_POK_SCHED_ARINC_GET_THREAD_INFO_05},
 *          \llrref{LLR_PARTITION_ARINC_POK_SCHED_ARINC_GET_THREAD_INFO_06},
 *          \llrref{LLR_PARTITION_ARINC_POK_SCHED_ARINC_GET_THREAD_INFO_07}
 *          1. If `t->suspended` is true:
 *             \llrref{LLR_PARTITION_ARINC_POK_SCHED_ARINC_GET_THREAD_INFO_05}
 *             1. Set `state` to `"Suspended"`.
 *          2. Otherwise, if `part_arinc->thread_current` is equal to `t`:
 *             \llrref{LLR_PARTITION_ARINC_POK_SCHED_ARINC_GET_THREAD_INFO_06}
 *             1. Set `state` to `"Running"`.
 *          3. Otherwise:
 *             \llrref{LLR_PARTITION_ARINC_POK_SCHED_ARINC_GET_THREAD_INFO_07}
 *             1. Set `state` to `"Ready"`.
 *          4. Leave the switch.
 *       4. Otherwise (default):
 *          \llrref{LLR_PARTITION_ARINC_POK_SCHED_ARINC_GET_THREAD_INFO_08}
 *          1. Use the macro `unreachable()`.
 *          2. Leave the switch.
 *    3. Call `arinc_thread_write_str()` with the arguments `print_cb`,
 *       `cb_data` and `t->name`.
 *       \llrref{LLR_PARTITION_ARINC_POK_SCHED_ARINC_GET_THREAD_INFO_03}
 *    4. Call `arinc_thread_write_str()` with the arguments `print_cb`,
 *       `cb_data` and `" "`.
 *       \llrref{LLR_PARTITION_ARINC_POK_SCHED_ARINC_GET_THREAD_INFO_03}
 *    5. Call `arinc_thread_write_str()` with the arguments `print_cb`,
 *       `cb_data` and `state`.
 *       \llrref{LLR_PARTITION_ARINC_POK_SCHED_ARINC_GET_THREAD_INFO_03}
 * 6. Otherwise: \llrref{LLR_PARTITION_ARINC_POK_SCHED_ARINC_GET_THREAD_INFO_09}
 *    1. Call `arinc_thread_write_str()` with the arguments `print_cb`,
 *       `cb_data` and `"[Not created]"`.
 *
 * @calls
 * - `arinc_thread_write_str()`
 * - `arinc_thread_is_created()`
 * - `pok_assertion_fail()`
 */
/* MISRA C:2025 Rule 11.5 deviation (the functions below): the GDB server
 * passes the thread to the callbacks as a pointer to void ('private'). */
static void pok_sched_arinc_get_thread_info(pok_partition_t* part,
	int32_t index, void* private, print_cb_t print_cb, void* cb_data)
{
	const pok_thread_t* t = private;
	const pok_partition_arinc_t* part_arinc = part->part_arinc;

	if(t == NULL)
	{
		arinc_thread_write_str(print_cb, cb_data, "IDLE");
	}
	else if(index == (int32_t)POK_PARTITION_ARINC_MAIN_THREAD_ID)
	{
		// Main thread. Currently do not bother with its state
		arinc_thread_write_str(print_cb, cb_data, t->name);
	}
	else if(arinc_thread_is_created(part_arinc, index))
	{
		const char* state = "";

		switch(t->state)
		{
		case POK_STATE_STOPPED:
			state = "Stopped";
			break;
		case POK_STATE_WAITING:
			// Waiting for anything except resume.
			state = "Waiting";
			break;
		case POK_STATE_RUNNABLE:
			if(t->suspended) {
				state = "Suspended";
			} else if(part_arinc->thread_current == t) {
				state = "Running";
			} else {
				state = "Ready";
			}
			break;
		default:
			unreachable();
			break;
		}

		arinc_thread_write_str(print_cb, cb_data, t->name);
		// Write state of the thread
		arinc_thread_write_str(print_cb, cb_data, " ");
		arinc_thread_write_str(print_cb, cb_data, state);
	}
	else
	{
		arinc_thread_write_str(print_cb, cb_data, "[Not created]");
	}
}

/**
 * @funcname `pok_sched_arinc_get_thread_registers`
 *
 * @description
 * The GDB operation of ARINC 653 partitions: the user registers saved when a
 * thread entered the kernel.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `part` | `pok_partition_t *` | [in] The partition. |
 * | `index` | `int32_t` | [in] The index. |
 * | `private` | `void *` | [in] The thread. |
 *
 * @return The registers, or NULL.
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `regs` | `interrupt_frame *` | The registers |
 * | `t` | `const pok_thread_t *` | The thread |
 *
 * @hlr
 * - \hlrid{HLR_PARTITION_ARINC_POK_SCHED_ARINC_GET_THREAD_REGISTERS_01}:
 *   `pok_sched_arinc_get_thread_registers()` shall give the GDB server the user
 *   registers of a process saved when it entered the kernel.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_DBG_004}
 *
 * @llr
 * - \llrid{LLR_PARTITION_ARINC_POK_SCHED_ARINC_GET_THREAD_REGISTERS_01}: If
 *   `private` is not NULL and `arinc_thread_is_created()` returns true for
 *   `part_arinc` of `part` and `index`,
 *   `pok_sched_arinc_get_thread_registers()` shall return `entry_sp_user` of
 *   the thread at `private`.
 *   <br>Traces to:
 *   - \hlrref{HLR_PARTITION_ARINC_POK_SCHED_ARINC_GET_THREAD_REGISTERS_01}
 * - \llrid{LLR_PARTITION_ARINC_POK_SCHED_ARINC_GET_THREAD_REGISTERS_02}: If
 *   `private` is NULL, or `arinc_thread_is_created()` returns false for
 *   `part_arinc` of `part` and `index`,
 *   `pok_sched_arinc_get_thread_registers()` shall return NULL.
 *   <br>Traces to:
 *   - \hlrref{HLR_PARTITION_ARINC_POK_SCHED_ARINC_GET_THREAD_REGISTERS_01}
 *
 * @design
 * 1. Set `t` to `private`.
 *    \llrref{LLR_PARTITION_ARINC_POK_SCHED_ARINC_GET_THREAD_REGISTERS_01},
 *    \llrref{LLR_PARTITION_ARINC_POK_SCHED_ARINC_GET_THREAD_REGISTERS_02}
 * 2. Set `regs` to `NULL`.
 *    \llrref{LLR_PARTITION_ARINC_POK_SCHED_ARINC_GET_THREAD_REGISTERS_01},
 *    \llrref{LLR_PARTITION_ARINC_POK_SCHED_ARINC_GET_THREAD_REGISTERS_02}
 * 3. If `t` is not NULL and `arinc_thread_is_created(part->part_arinc, index)`
 *    is true:
 *    \llrref{LLR_PARTITION_ARINC_POK_SCHED_ARINC_GET_THREAD_REGISTERS_01},
 *    \llrref{LLR_PARTITION_ARINC_POK_SCHED_ARINC_GET_THREAD_REGISTERS_02}
 *    1. Set `regs` to `t->entry_sp_user`.
 * 4. Return `regs`.
 *    \llrref{LLR_PARTITION_ARINC_POK_SCHED_ARINC_GET_THREAD_REGISTERS_01},
 *    \llrref{LLR_PARTITION_ARINC_POK_SCHED_ARINC_GET_THREAD_REGISTERS_02}
 *
 * @calls
 * - `arinc_thread_is_created()`
 */
static interrupt_frame* pok_sched_arinc_get_thread_registers(
	pok_partition_t* part, int32_t index, void* private)
{
	const pok_thread_t* t = private;
	interrupt_frame* regs = NULL;

	// The idle thread has no registers; neither has a process which isn't
	// created or hasn't entered user space yet.
	if((t != NULL) && arinc_thread_is_created(part->part_arinc, index))
	{
		regs = t->entry_sp_user;
	}

	return regs;
}

/**
 * @funcname `pok_sched_arinc_get_thread_fp_store`
 *
 * @description
 * The GDB operation of ARINC 653 partitions: the FP store of a thread.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `part` | `pok_partition_t *` | [in] The partition. |
 * | `index` | `int32_t` | [in] The index. |
 * | `private` | `void *` | [in] The thread. |
 *
 * @return The FP store, or NULL.
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `fp_store` | `jet_fp_store_t *` | The FP store |
 * | `t` | `const pok_thread_t *` | The thread |
 *
 * @hlr
 * - \hlrid{HLR_PARTITION_ARINC_POK_SCHED_ARINC_GET_THREAD_FP_STORE_01}:
 *   `pok_sched_arinc_get_thread_fp_store()` shall give the GDB server the
 *   floating point state of a process.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_DBG_004}
 *
 * @llr
 * - \llrid{LLR_PARTITION_ARINC_POK_SCHED_ARINC_GET_THREAD_FP_STORE_01}: If
 *   `private` is not NULL and `arinc_thread_is_created()` returns true for
 *   `part_arinc` of `part` and `index`, `pok_sched_arinc_get_thread_fp_store()`
 *   shall return `fp_store` of the thread at `private`.
 *   <br>Traces to:
 *   - \hlrref{HLR_PARTITION_ARINC_POK_SCHED_ARINC_GET_THREAD_FP_STORE_01}
 * - \llrid{LLR_PARTITION_ARINC_POK_SCHED_ARINC_GET_THREAD_FP_STORE_02}: If
 *   `private` is NULL, or `arinc_thread_is_created()` returns false for
 *   `part_arinc` of `part` and `index`, `pok_sched_arinc_get_thread_fp_store()`
 *   shall return NULL.
 *   <br>Traces to:
 *   - \hlrref{HLR_PARTITION_ARINC_POK_SCHED_ARINC_GET_THREAD_FP_STORE_01}
 *
 * @design
 * 1. Set `t` to `private`.
 *    \llrref{LLR_PARTITION_ARINC_POK_SCHED_ARINC_GET_THREAD_FP_STORE_01},
 *    \llrref{LLR_PARTITION_ARINC_POK_SCHED_ARINC_GET_THREAD_FP_STORE_02}
 * 2. Set `fp_store` to `NULL`.
 *    \llrref{LLR_PARTITION_ARINC_POK_SCHED_ARINC_GET_THREAD_FP_STORE_01},
 *    \llrref{LLR_PARTITION_ARINC_POK_SCHED_ARINC_GET_THREAD_FP_STORE_02}
 * 3. If `t` is not NULL and `arinc_thread_is_created(part->part_arinc, index)`
 *    is true:
 *    \llrref{LLR_PARTITION_ARINC_POK_SCHED_ARINC_GET_THREAD_FP_STORE_01},
 *    \llrref{LLR_PARTITION_ARINC_POK_SCHED_ARINC_GET_THREAD_FP_STORE_02}
 *    1. Set `fp_store` to `t->fp_store`.
 * 4. Return `fp_store`.
 *    \llrref{LLR_PARTITION_ARINC_POK_SCHED_ARINC_GET_THREAD_FP_STORE_01},
 *    \llrref{LLR_PARTITION_ARINC_POK_SCHED_ARINC_GET_THREAD_FP_STORE_02}
 *
 * @calls
 * - `arinc_thread_is_created()`
 */
static jet_fp_store_t* pok_sched_arinc_get_thread_fp_store(
	pok_partition_t* part, int32_t index, void* private)
{
	const pok_thread_t* t = private;
	jet_fp_store_t* fp_store = NULL;

	if((t != NULL) && arinc_thread_is_created(part->part_arinc, index))
	{
		fp_store = t->fp_store;
	}

	return fp_store;
}
#endif /* POK_NEEDS_GDB */


/**
 * @funcname `pok_partition_arinc_init`
 *
 * @description
 * Initializes an ARINC 653 partition at boot: its common part, its kernel
 * stack, its threads, and its operations.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `part` | `pok_partition_arinc_t *` | [in,out] The partition. |
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `arinc_ops` | `static const pok_part_ops_t` | The partition operations of ARINC 653 partitions |
 * | `arinc_sched_ops` | `static const pok_part_sched_ops_t` | The scheduler operations of ARINC 653 partitions |
 * | `i` | `uint32_t` | The index of a thread |
 *
 * @hlr
 * - \hlrid{HLR_PARTITION_ARINC_POK_PARTITION_ARINC_INIT_01}:
 *   `pok_partition_arinc_init()` shall initialize an ARINC 653 partition at
 *   boot: its event ring, its kernel stack, the stacks and FP stores of its
 *   process slots, and its operations.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_BOOT_002}
 *   - \srqref{SRQ_JET_MEM_010}
 *
 * @llr
 * - \llrid{LLR_PARTITION_ARINC_POK_PARTITION_ARINC_INIT_01}:
 *   `pok_partition_arinc_init()` shall call `pok_partition_init()`, link the
 *   base part to the partition, allocate a kernel stack of `DEFAULT_STACK_SIZE`
 *   bytes with `pok_stack_alloc()`, call `thread_init()` for each thread slot,
 *   and set the operations to `arinc_ops` and `arinc_sched_ops`.
 *   <br>Traces to:
 *   - \hlrref{HLR_PARTITION_ARINC_POK_PARTITION_ARINC_INIT_01}
 *
 * @design
 * 1. Call `pok_partition_init()` with the address of `part->base_part`.
 *    \llrref{LLR_PARTITION_ARINC_POK_PARTITION_ARINC_INIT_01}
 * 2. Set `part->base_part.part_arinc` to `part`.
 *    \llrref{LLR_PARTITION_ARINC_POK_PARTITION_ARINC_INIT_01}
 * 3. Call `pok_stack_alloc()` with the argument `DEFAULT_STACK_SIZE`, and store
 *    the result in `part->base_part.initial_sp`.
 *    \llrref{LLR_PARTITION_ARINC_POK_PARTITION_ARINC_INIT_01}
 * 4. Set `i` to `0U`; then while `i` is less than `part->nthreads`, repeat
 *    (after each pass: increment `i`):
 *    \llrref{LLR_PARTITION_ARINC_POK_PARTITION_ARINC_INIT_01}
 *    1. Call `thread_init()` with the address of `part->threads[i]`.
 * 5. Set `part->base_part.part_ops` to the address of `arinc_ops`.
 *    \llrref{LLR_PARTITION_ARINC_POK_PARTITION_ARINC_INIT_01}
 * 6. Set `part->base_part.part_sched_ops` to the address of `arinc_sched_ops`.
 *    \llrref{LLR_PARTITION_ARINC_POK_PARTITION_ARINC_INIT_01}
 *
 * @calls
 * - `pok_partition_init()`
 * - `pok_stack_alloc()`
 * - `thread_init()`
 */
void pok_partition_arinc_init(pok_partition_arinc_t* part)
{
	static const pok_part_sched_ops_t arinc_sched_ops = {
		.on_event = &pok_sched_arinc_on_event,
#ifdef POK_NEEDS_GDB
		.get_number_of_threads = &pok_sched_arinc_get_number_of_threads,
		.get_current_thread_index =
			&pok_sched_arinc_get_current_thread_index,
		.get_thread_at_index = &pok_sched_arinc_get_thread_at_index,
		.get_thread_info = &pok_sched_arinc_get_thread_info,
		.get_thread_registers = &pok_sched_arinc_get_thread_registers,
		.get_thread_fp_store = &pok_sched_arinc_get_thread_fp_store,
#endif /* POK_NEEDS_GDB */
	};

	static const pok_part_ops_t arinc_ops = {
		.start = &partition_arinc_start,
		.process_partition_error = &pok_partition_arinc_process_error,
	};

	pok_partition_init(&part->base_part);
	part->base_part.part_arinc = part;

	part->base_part.initial_sp = pok_stack_alloc(DEFAULT_STACK_SIZE);

	for(uint32_t i = 0U; i < part->nthreads; i++)
	{
		thread_init(&part->threads[i]);
	}

	part->base_part.part_ops = &arinc_ops;
	part->base_part.part_sched_ops = &arinc_sched_ops;
}

/**
 * @funcname `partition_set_mode_normal`
 *
 * @description
 * Puts the current partition into NORMAL mode: releases its lock and starts its
 * threads that aren't stopped: aperiodic ones after their delay, periodic ones
 * at the next periodic processing start plus their delay, with their deadlines.
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `current_partition` | `pok_partition_t *` | The running partition |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `current_time` | `pok_time_t` | The current time |
 * | `i` | `uint32_t` | The index of a thread |
 * | `part` | `pok_partition_arinc_t *` | The current partition |
 * | `periodic_release_point` | `pok_time_t` | The next periodic processing start, found once |
 * | `t` | `pok_thread_t *` | The thread |
 * | `thread_start_time` | `pok_time_t` | When the thread starts |
 *
 * @hlr
 * - \hlrid{HLR_PARTITION_ARINC_PARTITION_SET_MODE_NORMAL_01}:
 *   `partition_set_mode_normal()` shall put the current partition into `NORMAL`
 *   mode, release its lock of preemption, and start each process that was
 *   started during the initialization: an aperiodic one after its delay, a
 *   periodic one at the next start of periodic processing plus its delay, each
 *   with its deadline set.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_PART_006}
 * - \hlrid{HLR_PARTITION_ARINC_PARTITION_SET_MODE_NORMAL_02}:
 *   `partition_set_mode_normal()` shall release the periodic processes at a
 *   start of periodic processing of the partition.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_SCHED_006}
 *
 * @llr
 * - \llrid{LLR_PARTITION_ARINC_PARTITION_SET_MODE_NORMAL_01}:
 *   `partition_set_mode_normal()` shall set `mode` of the partition to
 *   `POK_PARTITION_MODE_NORMAL` and `lock_level` to 0, and call
 *   `pok_sched_local_invalidate()`.
 *   <br>Traces to:
 *   - \hlrref{HLR_PARTITION_ARINC_PARTITION_SET_MODE_NORMAL_01}
 * - \llrid{LLR_PARTITION_ARINC_PARTITION_SET_MODE_NORMAL_02}: For each thread
 *   from `POK_PARTITION_ARINC_MAIN_THREAD_ID` plus 1 below `nthreads_used`
 *   whose `state` is `POK_STATE_STOPPED`, `partition_set_mode_normal()` shall
 *   change nothing.
 *   <br>Traces to:
 *   - \hlrref{HLR_PARTITION_ARINC_PARTITION_SET_MODE_NORMAL_01}
 * - \llrid{LLR_PARTITION_ARINC_PARTITION_SET_MODE_NORMAL_03}: For each such
 *   thread that is not stopped and for which `pok_time_is_infinity()` returns
 *   true for its `period`, `partition_set_mode_normal()` shall take as its
 *   start time the result of `pok_time_add()` for the result of
 *   `jet_system_time()` (taken once at entry) and its `delayed_time`.
 *   <br>Traces to:
 *   - \hlrref{HLR_PARTITION_ARINC_PARTITION_SET_MODE_NORMAL_01}
 * - \llrid{LLR_PARTITION_ARINC_PARTITION_SET_MODE_NORMAL_04}: For the first
 *   such thread that is not stopped and has a finite `period`,
 *   `partition_set_mode_normal()` shall take the result of
 *   `get_next_periodic_processing_start()` as the release point, and call it
 *   for no other thread.
 *   <br>Traces to:
 *   - \hlrref{HLR_PARTITION_ARINC_PARTITION_SET_MODE_NORMAL_02}
 * - \llrid{LLR_PARTITION_ARINC_PARTITION_SET_MODE_NORMAL_05}: For each such
 *   thread that is not stopped and has a finite `period`,
 *   `partition_set_mode_normal()` shall take as its start time the result of
 *   `pok_time_add()` for the release point and its `delayed_time`, and set its
 *   `next_activation` to it.
 *   <br>Traces to:
 *   - \hlrref{HLR_PARTITION_ARINC_PARTITION_SET_MODE_NORMAL_02}
 * - \llrid{LLR_PARTITION_ARINC_PARTITION_SET_MODE_NORMAL_06}: If the start time
 *   of such a thread is not greater than the time at entry,
 *   `partition_set_mode_normal()` shall call `thread_wake_up()` with the
 *   thread.
 *   <br>Traces to:
 *   - \hlrref{HLR_PARTITION_ARINC_PARTITION_SET_MODE_NORMAL_01}
 * - \llrid{LLR_PARTITION_ARINC_PARTITION_SET_MODE_NORMAL_07}: If the start time
 *   of such a thread is greater than the time at entry,
 *   `partition_set_mode_normal()` shall call `thread_delay_event()` with the
 *   thread, the start time and `thread_wake_up`.
 *   <br>Traces to:
 *   - \hlrref{HLR_PARTITION_ARINC_PARTITION_SET_MODE_NORMAL_01}
 * - \llrid{LLR_PARTITION_ARINC_PARTITION_SET_MODE_NORMAL_08}: If and only if
 *   `pok_time_is_infinity()` returns false for `time_capacity` of such a
 *   thread, `partition_set_mode_normal()` shall call `thread_set_deadline()`
 *   with the thread and the result of `pok_time_add()` for the start time and
 *   `time_capacity`.
 *   <br>Traces to:
 *   - \hlrref{HLR_PARTITION_ARINC_PARTITION_SET_MODE_NORMAL_01}
 *
 * @design
 * 1. Set `part` to `current_partition_arinc`.
 *    \llrref{LLR_PARTITION_ARINC_PARTITION_SET_MODE_NORMAL_01}
 * 2. Set `current_time` to `jet_system_time()`.
 *    \llrref{LLR_PARTITION_ARINC_PARTITION_SET_MODE_NORMAL_03},
 *    \llrref{LLR_PARTITION_ARINC_PARTITION_SET_MODE_NORMAL_06},
 *    \llrref{LLR_PARTITION_ARINC_PARTITION_SET_MODE_NORMAL_07}
 * 3. Set `periodic_release_point` to `POK_TIME_INFINITY`.
 *    \llrref{LLR_PARTITION_ARINC_PARTITION_SET_MODE_NORMAL_04}
 * 4. Set `part->mode` to `POK_PARTITION_MODE_NORMAL`.
 *    \llrref{LLR_PARTITION_ARINC_PARTITION_SET_MODE_NORMAL_01}
 * 5. Set `part->lock_level` to `0`.
 *    \llrref{LLR_PARTITION_ARINC_PARTITION_SET_MODE_NORMAL_01}
 * 6. Call `pok_sched_local_invalidate()`.
 *    \llrref{LLR_PARTITION_ARINC_PARTITION_SET_MODE_NORMAL_01}
 * 7. Set `i` to `POK_PARTITION_ARINC_MAIN_THREAD_ID + 1U`; then while `i` is
 *    less than `part->nthreads_used`, repeat (after each pass: increment `i`):
 *    \llrref{LLR_PARTITION_ARINC_PARTITION_SET_MODE_NORMAL_02},
 *    \llrref{LLR_PARTITION_ARINC_PARTITION_SET_MODE_NORMAL_03},
 *    \llrref{LLR_PARTITION_ARINC_PARTITION_SET_MODE_NORMAL_04},
 *    \llrref{LLR_PARTITION_ARINC_PARTITION_SET_MODE_NORMAL_05},
 *    \llrref{LLR_PARTITION_ARINC_PARTITION_SET_MODE_NORMAL_06},
 *    \llrref{LLR_PARTITION_ARINC_PARTITION_SET_MODE_NORMAL_07},
 *    \llrref{LLR_PARTITION_ARINC_PARTITION_SET_MODE_NORMAL_08}
 *    1. Set `t` to the address of `part->threads[i]`.
 *       \llrref{LLR_PARTITION_ARINC_PARTITION_SET_MODE_NORMAL_03},
 *       \llrref{LLR_PARTITION_ARINC_PARTITION_SET_MODE_NORMAL_05}
 *    2. If `t->state` is equal to `POK_STATE_STOPPED`:
 *       \llrref{LLR_PARTITION_ARINC_PARTITION_SET_MODE_NORMAL_02}
 *       1. Go on with the next pass of the loop.
 *    3. If `pok_time_is_infinity(t->period)` is true:
 *       \llrref{LLR_PARTITION_ARINC_PARTITION_SET_MODE_NORMAL_03}
 *       1. Call `pok_time_add()` with the arguments `current_time` and
 *          `t->delayed_time`, and store the result in `thread_start_time`.
 *    4. Otherwise: \llrref{LLR_PARTITION_ARINC_PARTITION_SET_MODE_NORMAL_04},
 *       \llrref{LLR_PARTITION_ARINC_PARTITION_SET_MODE_NORMAL_05}
 *       1. If `pok_time_is_infinity(periodic_release_point)` is true:
 *          \llrref{LLR_PARTITION_ARINC_PARTITION_SET_MODE_NORMAL_04}
 *          1. Call `get_next_periodic_processing_start()`, and store the result
 *             in `periodic_release_point`.
 *       2. Call `pok_time_add()` with the arguments `periodic_release_point`
 *          and `t->delayed_time`, and store the result in `thread_start_time`.
 *          \llrref{LLR_PARTITION_ARINC_PARTITION_SET_MODE_NORMAL_05}
 *       3. Set `t->next_activation` to `thread_start_time`.
 *          \llrref{LLR_PARTITION_ARINC_PARTITION_SET_MODE_NORMAL_05}
 *    5. If `thread_start_time` is less than or equal to `current_time`:
 *       \llrref{LLR_PARTITION_ARINC_PARTITION_SET_MODE_NORMAL_06}
 *       1. Call `thread_wake_up()` with the argument `t`.
 *    6. Otherwise: \llrref{LLR_PARTITION_ARINC_PARTITION_SET_MODE_NORMAL_07}
 *       1. Call `thread_delay_event()` with the arguments `t`,
 *          `thread_start_time` and the address of `thread_wake_up`.
 *    7. If `pok_time_is_infinity(t->time_capacity)` is false:
 *       \llrref{LLR_PARTITION_ARINC_PARTITION_SET_MODE_NORMAL_08}
 *       1. Call `thread_set_deadline()` with the arguments `t` and
 *          `pok_time_add(thread_start_time, t->time_capacity)`.
 *
 * @calls
 * - `ja_system_time()`
 * - `pok_sched_local_invalidate()`
 * - `pok_time_is_infinity()`
 * - `pok_time_add()`
 * - `get_next_periodic_processing_start()`
 * - `thread_wake_up()`
 * - `thread_delay_event()`
 * - `thread_set_deadline()`
 */
static void partition_set_mode_normal(void)
{
	pok_partition_arinc_t* part = current_partition_arinc;

	// Cached value of current time.
	pok_time_t current_time = jet_system_time();
	/*
	 * Cached value of first release point.
	 *
	 * NOTE: Initially it is not cached. Such implementation allows to
	 * not calculate release point for partition which have no periodic
	 * processes.
	 */
	pok_time_t periodic_release_point = POK_TIME_INFINITY;

	part->mode = POK_PARTITION_MODE_NORMAL;
	part->lock_level = 0;
	pok_sched_local_invalidate();

	for(uint32_t i = POK_PARTITION_ARINC_MAIN_THREAD_ID + 1U;
	    i < part->nthreads_used; i++)
	{
		pok_thread_t* t = &part->threads[i];
		pok_time_t thread_start_time;

		if(t->state == POK_STATE_STOPPED) {
			continue;
		}

		/*
		 * The only thing thread can wait in `INIT_*` mode is
		 * NORMAL mode switch.
		 */

		if(pok_time_is_infinity(t->period))
		{
			// Aperiodic process.
			thread_start_time = pok_time_add(current_time,
			    t->delayed_time);
		}
		else
		{
			// Periodic process
			if(pok_time_is_infinity(periodic_release_point)) {
				periodic_release_point = get_next_periodic_processing_start();
			}
                
			thread_start_time = pok_time_add(periodic_release_point,
			    t->delayed_time);
    		t->next_activation = thread_start_time;
		}

		if(thread_start_time <= current_time) {
			thread_wake_up(t);
		} else {
			thread_delay_event(t, thread_start_time, &thread_wake_up);
		}

		if(!pok_time_is_infinity(t->time_capacity))
		{
			thread_set_deadline(t,
			    pok_time_add(thread_start_time, t->time_capacity));
		}
	}
}


/**
 * @funcname `partition_set_mode_internal`
 *
 * @description
 * Sets the operating mode of the current partition: INIT_COLD and INIT_WARM
 * restart it, IDLE stops it, NORMAL starts its threads.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `mode` | `const pok_partition_mode_e` | [in] The new mode. |
 *
 * @return POK_ERRNO_OK; POK_ERRNO_PARTITION_MODE for INIT_WARM after INIT_COLD;
 *         POK_ERRNO_UNAVAILABLE for NORMAL in NORMAL mode; POK_ERRNO_EINVAL for
 *         a bad mode.
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `current_partition` | `pok_partition_t *` | The running partition |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `part` | `pok_partition_arinc_t *` | The current partition |
 *
 * @hlr
 * - \hlrid{HLR_PARTITION_ARINC_PARTITION_SET_MODE_INTERNAL_01}:
 *   `partition_set_mode_internal()` shall change the mode of the current
 *   partition: to `COLD_START` or `WARM_START` by restarting it, recorded as a
 *   partition restart; to `IDLE` by stopping it; to `NORMAL` by starting its
 *   processes; and refuse `WARM_START` during `COLD_START`, `NORMAL` in
 *   `NORMAL` mode, and any other mode, with an error.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_PART_005}
 *   - \srqref{SRQ_JET_PART_014}
 *   - \srqref{SRQ_JET_PART_015}
 *   - \srqref{SRQ_JET_PART_016}
 * - \hlrid{HLR_PARTITION_ARINC_PARTITION_SET_MODE_INTERNAL_02}:
 *   `partition_set_mode_internal()` shall store the new mode in the area that
 *   the kernel shares with the partition.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_MEM_009}
 *
 * @llr
 * - \llrid{LLR_PARTITION_ARINC_PARTITION_SET_MODE_INTERNAL_01}: If `mode` is
 *   `POK_PARTITION_MODE_INIT_WARM` and `mode` of the partition is
 *   `POK_PARTITION_MODE_INIT_COLD`, `partition_set_mode_internal()` shall
 *   return `POK_ERRNO_PARTITION_MODE`.
 *   <br>Traces to:
 *   - \hlrref{HLR_PARTITION_ARINC_PARTITION_SET_MODE_INTERNAL_01}
 * - \llrid{LLR_PARTITION_ARINC_PARTITION_SET_MODE_INTERNAL_02}: If `mode` is
 *   `POK_PARTITION_MODE_INIT_COLD`, or is `POK_PARTITION_MODE_INIT_WARM` while
 *   `mode` of the partition is not `POK_PARTITION_MODE_INIT_COLD`,
 *   `partition_set_mode_internal()` shall set `start_condition` of the base
 *   partition to `POK_START_CONDITION_PARTITION_RESTART` and call
 *   `pok_partition_arinc_reset()` with `mode`.
 *   <br>Traces to:
 *   - \hlrref{HLR_PARTITION_ARINC_PARTITION_SET_MODE_INTERNAL_01}
 * - \llrid{LLR_PARTITION_ARINC_PARTITION_SET_MODE_INTERNAL_03}: If `mode` is
 *   `POK_PARTITION_MODE_IDLE`, `partition_set_mode_internal()` shall call
 *   `pok_partition_arinc_idle()`.
 *   <br>Traces to:
 *   - \hlrref{HLR_PARTITION_ARINC_PARTITION_SET_MODE_INTERNAL_01}
 * - \llrid{LLR_PARTITION_ARINC_PARTITION_SET_MODE_INTERNAL_04}: If `mode` is
 *   `POK_PARTITION_MODE_NORMAL` and `mode` of the partition is
 *   `POK_PARTITION_MODE_NORMAL`, `partition_set_mode_internal()` shall return
 *   `POK_ERRNO_UNAVAILABLE`.
 *   <br>Traces to:
 *   - \hlrref{HLR_PARTITION_ARINC_PARTITION_SET_MODE_INTERNAL_01}
 * - \llrid{LLR_PARTITION_ARINC_PARTITION_SET_MODE_INTERNAL_05}: If `mode` is
 *   `POK_PARTITION_MODE_NORMAL` and `mode` of the partition is not
 *   `POK_PARTITION_MODE_NORMAL`, `partition_set_mode_internal()` shall call
 *   `partition_set_mode_normal()`.
 *   <br>Traces to:
 *   - \hlrref{HLR_PARTITION_ARINC_PARTITION_SET_MODE_INTERNAL_01}
 * - \llrid{LLR_PARTITION_ARINC_PARTITION_SET_MODE_INTERNAL_06}: If `mode` is
 *   none of `POK_PARTITION_MODE_INIT_WARM`, `POK_PARTITION_MODE_INIT_COLD`,
 *   `POK_PARTITION_MODE_IDLE` and `POK_PARTITION_MODE_NORMAL`,
 *   `partition_set_mode_internal()` shall return `POK_ERRNO_EINVAL`.
 *   <br>Traces to:
 *   - \hlrref{HLR_PARTITION_ARINC_PARTITION_SET_MODE_INTERNAL_01}
 * - \llrid{LLR_PARTITION_ARINC_PARTITION_SET_MODE_INTERNAL_07}: When
 *   `pok_partition_arinc_reset()`, `pok_partition_arinc_idle()` or
 *   `partition_set_mode_normal()` returns, `partition_set_mode_internal()`
 *   shall set `partition_mode` of the shared data to `mode` of the partition
 *   and return `POK_ERRNO_OK`.
 *   <br>Traces to:
 *   - \hlrref{HLR_PARTITION_ARINC_PARTITION_SET_MODE_INTERNAL_02}
 *
 * @design
 * 1. Set `part` to `current_partition_arinc`.
 *    \llrref{LLR_PARTITION_ARINC_PARTITION_SET_MODE_INTERNAL_07}
 * 2. Depending on `mode`:
 *    \llrref{LLR_PARTITION_ARINC_PARTITION_SET_MODE_INTERNAL_01},
 *    \llrref{LLR_PARTITION_ARINC_PARTITION_SET_MODE_INTERNAL_02},
 *    \llrref{LLR_PARTITION_ARINC_PARTITION_SET_MODE_INTERNAL_03},
 *    \llrref{LLR_PARTITION_ARINC_PARTITION_SET_MODE_INTERNAL_04},
 *    \llrref{LLR_PARTITION_ARINC_PARTITION_SET_MODE_INTERNAL_05},
 *    \llrref{LLR_PARTITION_ARINC_PARTITION_SET_MODE_INTERNAL_06}
 *    1. Case `POK_PARTITION_MODE_INIT_WARM` or `POK_PARTITION_MODE_INIT_COLD`:
 *       \llrref{LLR_PARTITION_ARINC_PARTITION_SET_MODE_INTERNAL_01},
 *       \llrref{LLR_PARTITION_ARINC_PARTITION_SET_MODE_INTERNAL_02}
 *       1. If `mode` is equal to `POK_PARTITION_MODE_INIT_WARM` and
 *          `part->mode` is equal to `POK_PARTITION_MODE_INIT_COLD`:
 *          \llrref{LLR_PARTITION_ARINC_PARTITION_SET_MODE_INTERNAL_01}
 *          1. Return `POK_ERRNO_PARTITION_MODE`.
 *       2. Set `part->base_part.start_condition` to
 *          `POK_START_CONDITION_PARTITION_RESTART`.
 *          \llrref{LLR_PARTITION_ARINC_PARTITION_SET_MODE_INTERNAL_02}
 *       3. Call `pok_partition_arinc_reset()` with the argument `mode`.
 *          \llrref{LLR_PARTITION_ARINC_PARTITION_SET_MODE_INTERNAL_02}
 *       4. Leave the switch.
 *    2. Case `POK_PARTITION_MODE_IDLE`:
 *       \llrref{LLR_PARTITION_ARINC_PARTITION_SET_MODE_INTERNAL_03}
 *       1. Call `pok_partition_arinc_idle()`.
 *       2. Leave the switch.
 *    3. Case `POK_PARTITION_MODE_NORMAL`:
 *       \llrref{LLR_PARTITION_ARINC_PARTITION_SET_MODE_INTERNAL_04},
 *       \llrref{LLR_PARTITION_ARINC_PARTITION_SET_MODE_INTERNAL_05}
 *       1. If `part->mode` is equal to `POK_PARTITION_MODE_NORMAL`:
 *          \llrref{LLR_PARTITION_ARINC_PARTITION_SET_MODE_INTERNAL_04}
 *          1. Return `POK_ERRNO_UNAVAILABLE`.
 *       2. Call `partition_set_mode_normal()`.
 *          \llrref{LLR_PARTITION_ARINC_PARTITION_SET_MODE_INTERNAL_05}
 *       3. Leave the switch.
 *    4. Otherwise (default):
 *       \llrref{LLR_PARTITION_ARINC_PARTITION_SET_MODE_INTERNAL_06}
 *       1. Return `POK_ERRNO_EINVAL`.
 * 3. Set `part->kshd->partition_mode` to `part->mode`.
 *    \llrref{LLR_PARTITION_ARINC_PARTITION_SET_MODE_INTERNAL_07}
 * 4. Return `POK_ERRNO_OK`.
 *    \llrref{LLR_PARTITION_ARINC_PARTITION_SET_MODE_INTERNAL_07}
 *
 * @calls
 * - `pok_partition_arinc_reset()`
 * - `pok_partition_arinc_idle()`
 * - `partition_set_mode_normal()`
 */
static pok_ret_e partition_set_mode_internal (const pok_partition_mode_e mode)
{
	pok_partition_arinc_t* part = current_partition_arinc;

	switch(mode)
	{
	case POK_PARTITION_MODE_INIT_WARM:
	case POK_PARTITION_MODE_INIT_COLD:
		// A warm start isn't allowed during a cold start.
		if((mode == POK_PARTITION_MODE_INIT_WARM)
			&& (part->mode == POK_PARTITION_MODE_INIT_COLD)) {
			return POK_ERRNO_PARTITION_MODE;
		}
		part->base_part.start_condition = POK_START_CONDITION_PARTITION_RESTART;
		pok_partition_arinc_reset(mode); // Never return.
	break;
	case POK_PARTITION_MODE_IDLE:
		pok_partition_arinc_idle(); // Never return.
	break;
	case POK_PARTITION_MODE_NORMAL:
		if(part->mode == POK_PARTITION_MODE_NORMAL) {
			return POK_ERRNO_UNAVAILABLE; //TODO: revise error code
		}
		partition_set_mode_normal();
	break;
	default:
		return POK_ERRNO_EINVAL;
	}

	// Update kernel shared data
	part->kshd->partition_mode = part->mode;

	return POK_ERRNO_OK;
}

/**
 * @funcname `pok_partition_set_mode_current`
 *
 * @description
 * Sets the operating mode of the current partition (SET_PARTITION_MODE).
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `mode` | `const pok_partition_mode_e` | [in] The new mode. |
 *
 * @return As partition_set_mode_internal().
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `res` | `pok_ret_e` | The result |
 *
 * @hlr
 * - \hlrid{HLR_PARTITION_ARINC_POK_PARTITION_SET_MODE_CURRENT_01}:
 *   `pok_partition_set_mode_current()` shall change the mode of the current
 *   partition at its request, as `partition_set_mode_internal()` does, without
 *   being preempted by another process of the partition.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_PART_005}
 *   - \srqref{SRQ_JET_PART_014}
 *   - \srqref{SRQ_JET_PART_015}
 *   - \srqref{SRQ_JET_PART_016}
 *
 * @llr
 * - \llrid{LLR_PARTITION_ARINC_POK_PARTITION_SET_MODE_CURRENT_01}:
 *   `pok_partition_set_mode_current()` shall call
 *   `pok_preemption_local_disable()`, then `partition_set_mode_internal()` with
 *   `mode`, then `pok_preemption_local_enable()`, and return the result of
 *   `partition_set_mode_internal()`.
 *   <br>Traces to:
 *   - \hlrref{HLR_PARTITION_ARINC_POK_PARTITION_SET_MODE_CURRENT_01}
 *
 * @design
 * 1. Call `pok_preemption_local_disable()`.
 *    \llrref{LLR_PARTITION_ARINC_POK_PARTITION_SET_MODE_CURRENT_01}
 * 2. Call `partition_set_mode_internal()` with the argument `mode`, and store
 *    the result in `res`.
 *    \llrref{LLR_PARTITION_ARINC_POK_PARTITION_SET_MODE_CURRENT_01}
 * 3. Call `pok_preemption_local_enable()`.
 *    \llrref{LLR_PARTITION_ARINC_POK_PARTITION_SET_MODE_CURRENT_01}
 * 4. Return `res`.
 *    \llrref{LLR_PARTITION_ARINC_POK_PARTITION_SET_MODE_CURRENT_01}
 *
 * @calls
 * - `pok_preemption_local_disable()`
 * - `partition_set_mode_internal()`
 * - `pok_preemption_local_enable()`
 */
pok_ret_e pok_partition_set_mode_current (const pok_partition_mode_e mode)
{
	pok_ret_e res;

	pok_preemption_local_disable();
	res = partition_set_mode_internal(mode);
	pok_preemption_local_enable();

	return res;
}

/**
 * @funcname `pok_current_partition_get_status`
 *
 * @description
 * Gives the status of the current partition (GET_PARTITION_STATUS): its ID,
 * period, duration, mode, lock level and start condition.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `status` | `pok_partition_status_t * JET_USER` | [out] Partition address of the status. |
 *
 * @return POK_ERRNO_OK, or POK_ERRNO_EFAULT for a bad address.
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `current_partition` | `pok_partition_t *` | The running partition |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `k_status` | `pok_partition_status_t *` | Kernel address of the status |
 *
 * @hlr
 * - \hlrid{HLR_PARTITION_ARINC_POK_CURRENT_PARTITION_GET_STATUS_01}:
 *   `pok_current_partition_get_status()` shall give the current partition its
 *   identifier, period, duration, mode, lock level and the reason of its last
 *   start.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_PART_008}
 *   - \srqref{SRQ_JET_CFG_005}
 * - \hlrid{HLR_PARTITION_ARINC_POK_CURRENT_PARTITION_GET_STATUS_02}:
 *   `pok_current_partition_get_status()` shall refuse a status address that the
 *   partition may not write with `POK_ERRNO_EFAULT`.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_MEM_006}
 *   - \srqref{SRQ_JET_MEM_016}
 *
 * @llr
 * - \llrid{LLR_PARTITION_ARINC_POK_CURRENT_PARTITION_GET_STATUS_01}:
 *   `pok_current_partition_get_status()` shall convert `status` to a kernel
 *   address and return `POK_ERRNO_EFAULT` if it is NULL.
 *   <br>Traces to:
 *   - \hlrref{HLR_PARTITION_ARINC_POK_CURRENT_PARTITION_GET_STATUS_02}
 * - \llrid{LLR_PARTITION_ARINC_POK_CURRENT_PARTITION_GET_STATUS_02}:
 *   `pok_current_partition_get_status()` shall store the ID, period, duration,
 *   mode, lock level and start condition of the current partition in the status
 *   and return `POK_ERRNO_OK`.
 *   <br>Traces to:
 *   - \hlrref{HLR_PARTITION_ARINC_POK_CURRENT_PARTITION_GET_STATUS_01}
 *
 * @design
 * 1. Set `k_status` to `jet_user_to_kernel_typed(status)`.
 *    \llrref{LLR_PARTITION_ARINC_POK_CURRENT_PARTITION_GET_STATUS_01}
 * 2. If `k_status` is NULL:
 *    \llrref{LLR_PARTITION_ARINC_POK_CURRENT_PARTITION_GET_STATUS_01}
 *    1. Return `POK_ERRNO_EFAULT`.
 * 3. Set `k_status->id` to `current_partition->partition_id`.
 *    \llrref{LLR_PARTITION_ARINC_POK_CURRENT_PARTITION_GET_STATUS_02}
 * 4. Set `k_status->period` to `current_partition->period`.
 *    \llrref{LLR_PARTITION_ARINC_POK_CURRENT_PARTITION_GET_STATUS_02}
 * 5. Set `k_status->duration` to `current_partition->duration`.
 *    \llrref{LLR_PARTITION_ARINC_POK_CURRENT_PARTITION_GET_STATUS_02}
 * 6. Set `k_status->mode` to `current_partition_arinc->mode`.
 *    \llrref{LLR_PARTITION_ARINC_POK_CURRENT_PARTITION_GET_STATUS_02}
 * 7. Set `k_status->lock_level` to `current_partition_arinc->lock_level`.
 *    \llrref{LLR_PARTITION_ARINC_POK_CURRENT_PARTITION_GET_STATUS_02}
 * 8. Set `k_status->start_condition` to `current_partition->start_condition`.
 *    \llrref{LLR_PARTITION_ARINC_POK_CURRENT_PARTITION_GET_STATUS_02}
 * 9. Return `POK_ERRNO_OK`.
 *    \llrref{LLR_PARTITION_ARINC_POK_CURRENT_PARTITION_GET_STATUS_02}
 *
 * @calls
 * - `jet_user_to_kernel()`
 */
pok_ret_e pok_current_partition_get_status(
    pok_partition_status_t * JET_USER status)
{
    pok_partition_status_t* JET_KUSER k_status =
		jet_user_to_kernel_typed(status);
    if(k_status == NULL) {
    	return POK_ERRNO_EFAULT;
    }

    k_status->id = current_partition->partition_id;
    k_status->period = current_partition->period;
    k_status->duration = current_partition->duration;
    k_status->mode = current_partition_arinc->mode;
    k_status->lock_level = current_partition_arinc->lock_level;
    k_status->start_condition = current_partition->start_condition;

    return POK_ERRNO_OK;
}

/**
 * @funcname `is_lock_level_blocked`
 *
 * @description
 * Tells whether the lock level can't change: outside NORMAL mode, or in the
 * error handler.
 *
 * @return TRUE if it can't change.
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `current_partition` | `pok_partition_t *` | The running partition |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `part` | `pok_partition_arinc_t *` | The current partition |
 *
 * @hlr
 * - \hlrid{HLR_PARTITION_ARINC_IS_LOCK_LEVEL_BLOCKED_01}:
 *   `is_lock_level_blocked()` shall tell whether the lock level of the current
 *   partition may not change: outside `NORMAL` mode or in the error handler.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_PART_019}
 *
 * @llr
 * - \llrid{LLR_PARTITION_ARINC_IS_LOCK_LEVEL_BLOCKED_01}: If `mode` of the
 *   partition is not `POK_PARTITION_MODE_NORMAL` or `thread_current` is
 *   `thread_error`, `is_lock_level_blocked()` shall return true.
 *   <br>Traces to:
 *   - \hlrref{HLR_PARTITION_ARINC_IS_LOCK_LEVEL_BLOCKED_01}
 * - \llrid{LLR_PARTITION_ARINC_IS_LOCK_LEVEL_BLOCKED_02}: If `mode` of the
 *   partition is `POK_PARTITION_MODE_NORMAL` and `thread_current` is not
 *   `thread_error`, `is_lock_level_blocked()` shall return false.
 *   <br>Traces to:
 *   - \hlrref{HLR_PARTITION_ARINC_IS_LOCK_LEVEL_BLOCKED_01}
 *
 * @design
 * 1. Set `part` to `current_partition_arinc`.
 *    \llrref{LLR_PARTITION_ARINC_IS_LOCK_LEVEL_BLOCKED_01},
 *    \llrref{LLR_PARTITION_ARINC_IS_LOCK_LEVEL_BLOCKED_02}
 * 2. Return
 *    `(part->mode != POK_PARTITION_MODE_NORMAL) || (part->thread_current ==`
 *    `part->thread_error)`.
 *    \llrref{LLR_PARTITION_ARINC_IS_LOCK_LEVEL_BLOCKED_01},
 *    \llrref{LLR_PARTITION_ARINC_IS_LOCK_LEVEL_BLOCKED_02}
 *
 * @calls
 * None.
 */
static pok_bool_t is_lock_level_blocked(void)
{
	pok_partition_arinc_t* part = current_partition_arinc;
	return (part->mode != POK_PARTITION_MODE_NORMAL) ||
      (part->thread_current == part->thread_error);
}

/**
 * @funcname `pok_current_partition_inc_lock_level`
 *
 * @description
 * Locks preemption in the current partition (LOCK_PREEMPTION): the lock level
 * goes up and the current thread holds the lock.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `lock_level` | `int32_t * JET_USER` | [out] Partition address of the new lock level. |
 *
 * @return POK_ERRNO_OK; POK_ERRNO_PARTITION_MODE if the lock level can't
 *         change; POK_ERRNO_EINVAL at the maximum; POK_ERRNO_EFAULT for a bad
 *         address.
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `current_partition` | `pok_partition_t *` | The running partition |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `k_lock_level` | `uint32_t *` | Kernel address of the lock level |
 * | `part` | `pok_partition_arinc_t *` | The current partition |
 *
 * @hlr
 * - \hlrid{HLR_PARTITION_ARINC_POK_CURRENT_PARTITION_INC_LOCK_LEVEL_01}:
 *   `pok_current_partition_inc_lock_level()` shall raise the lock level of the
 *   preemption of the current partition by one, the current process holding the
 *   lock, and give the new level.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_PART_009}
 *   - \srqref{SRQ_JET_PART_017}
 * - \hlrid{HLR_PARTITION_ARINC_POK_CURRENT_PARTITION_INC_LOCK_LEVEL_02}:
 *   `pok_current_partition_inc_lock_level()` shall refuse to raise the lock
 *   level outside `NORMAL` mode, in the error handler, and at the maximum level
 *   of 16.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_PART_009}
 *   - \srqref{SRQ_JET_PART_019}
 * - \hlrid{HLR_PARTITION_ARINC_POK_CURRENT_PARTITION_INC_LOCK_LEVEL_03}:
 *   `pok_current_partition_inc_lock_level()` shall refuse an address of the
 *   result that the partition may not write with `POK_ERRNO_EFAULT`.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_MEM_006}
 *   - \srqref{SRQ_JET_MEM_016}
 *
 * @llr
 * - \llrid{LLR_PARTITION_ARINC_POK_CURRENT_PARTITION_INC_LOCK_LEVEL_01}: If
 *   `is_lock_level_blocked()` returns true,
 *   `pok_current_partition_inc_lock_level()` shall return
 *   `POK_ERRNO_PARTITION_MODE`.
 *   <br>Traces to:
 *   - \hlrref{HLR_PARTITION_ARINC_POK_CURRENT_PARTITION_INC_LOCK_LEVEL_02}
 * - \llrid{LLR_PARTITION_ARINC_POK_CURRENT_PARTITION_INC_LOCK_LEVEL_02}: If
 *   `is_lock_level_blocked()` returns false and `lock_level` of the partition
 *   is `MAX_LOCK_LEVEL`, `pok_current_partition_inc_lock_level()` shall return
 *   `POK_ERRNO_EINVAL`.
 *   <br>Traces to:
 *   - \hlrref{HLR_PARTITION_ARINC_POK_CURRENT_PARTITION_INC_LOCK_LEVEL_02}
 * - \llrid{LLR_PARTITION_ARINC_POK_CURRENT_PARTITION_INC_LOCK_LEVEL_03}: If
 *   these checks pass and `jet_user_to_kernel_typed()` returns NULL for
 *   `lock_level`, `pok_current_partition_inc_lock_level()` shall return
 *   `POK_ERRNO_EFAULT` without changing the level.
 *   <br>Traces to:
 *   - \hlrref{HLR_PARTITION_ARINC_POK_CURRENT_PARTITION_INC_LOCK_LEVEL_03}
 * - \llrid{LLR_PARTITION_ARINC_POK_CURRENT_PARTITION_INC_LOCK_LEVEL_04}: If
 *   these checks pass and the conversion gives an address,
 *   `pok_current_partition_inc_lock_level()` shall call
 *   `pok_preemption_local_disable()`, increment `lock_level` of the partition,
 *   set `thread_locked` to `thread_current`, call
 *   `pok_preemption_local_enable()`, store the new level at that address and
 *   return `POK_ERRNO_OK`.
 *   <br>Traces to:
 *   - \hlrref{HLR_PARTITION_ARINC_POK_CURRENT_PARTITION_INC_LOCK_LEVEL_01}
 *
 * @design
 * 1. Set `part` to `current_partition_arinc`.
 *    \llrref{LLR_PARTITION_ARINC_POK_CURRENT_PARTITION_INC_LOCK_LEVEL_04}
 * 2. If `is_lock_level_blocked()` is true:
 *    \llrref{LLR_PARTITION_ARINC_POK_CURRENT_PARTITION_INC_LOCK_LEVEL_01}
 *    1. Return `POK_ERRNO_PARTITION_MODE`.
 * 3. If `part->lock_level` is equal to `MAX_LOCK_LEVEL`:
 *    \llrref{LLR_PARTITION_ARINC_POK_CURRENT_PARTITION_INC_LOCK_LEVEL_02}
 *    1. Return `POK_ERRNO_EINVAL`.
 * 4. Set `k_lock_level` to `jet_user_to_kernel_typed(lock_level)`.
 *    \llrref{LLR_PARTITION_ARINC_POK_CURRENT_PARTITION_INC_LOCK_LEVEL_03}
 * 5. If `k_lock_level` is NULL:
 *    \llrref{LLR_PARTITION_ARINC_POK_CURRENT_PARTITION_INC_LOCK_LEVEL_03}
 *    1. Return `POK_ERRNO_EFAULT`.
 * 6. Call `pok_preemption_local_disable()`.
 *    \llrref{LLR_PARTITION_ARINC_POK_CURRENT_PARTITION_INC_LOCK_LEVEL_04}
 * 7. Increment `part->lock_level`.
 *    \llrref{LLR_PARTITION_ARINC_POK_CURRENT_PARTITION_INC_LOCK_LEVEL_04}
 * 8. Set `part->thread_locked` to `part->thread_current`.
 *    \llrref{LLR_PARTITION_ARINC_POK_CURRENT_PARTITION_INC_LOCK_LEVEL_04}
 * 9. Call `pok_preemption_local_enable()`.
 *    \llrref{LLR_PARTITION_ARINC_POK_CURRENT_PARTITION_INC_LOCK_LEVEL_04}
 * 10. Set `*k_lock_level` to `current_partition_arinc->lock_level`.
 *     \llrref{LLR_PARTITION_ARINC_POK_CURRENT_PARTITION_INC_LOCK_LEVEL_04}
 * 11. Return `POK_ERRNO_OK`.
 *     \llrref{LLR_PARTITION_ARINC_POK_CURRENT_PARTITION_INC_LOCK_LEVEL_04}
 *
 * @calls
 * - `is_lock_level_blocked()`
 * - `jet_user_to_kernel()`
 * - `pok_preemption_local_disable()`
 * - `pok_preemption_local_enable()`
 */
pok_ret_e pok_current_partition_inc_lock_level(int32_t * JET_USER lock_level)
{
    pok_partition_arinc_t* part = current_partition_arinc;

    if(is_lock_level_blocked()) {
		return POK_ERRNO_PARTITION_MODE;
    }
    if(part->lock_level == MAX_LOCK_LEVEL) {
		return POK_ERRNO_EINVAL;
    }

	uint32_t* JET_KUSER k_lock_level = jet_user_to_kernel_typed(lock_level);
	if(k_lock_level == NULL) {
		return POK_ERRNO_EFAULT;
	}

    pok_preemption_local_disable();
	part->lock_level++;
	part->thread_locked = part->thread_current;
	// Note: this doesn't invalidate any scheduling event.
	pok_preemption_local_enable();

	*k_lock_level = current_partition_arinc->lock_level;

	return POK_ERRNO_OK;
}

/**
 * @funcname `pok_current_partition_dec_lock_level`
 *
 * @description
 * Unlocks preemption in the current partition (UNLOCK_PREEMPTION): the lock
 * level goes down; at 0 the scheduler chooses again if the current thread isn't
 * the first eligible one.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `lock_level` | `int32_t * JET_USER` | [out] Partition address of the new lock level. |
 *
 * @return POK_ERRNO_OK; POK_ERRNO_PARTITION_MODE if the lock level can't
 *         change; POK_ERRNO_EINVAL at 0; POK_ERRNO_EFAULT for a bad address.
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `current_partition` | `pok_partition_t *` | The running partition |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `k_lock_level` | `uint32_t *` | Kernel address of the lock level |
 *
 * @hlr
 * - \hlrid{HLR_PARTITION_ARINC_POK_CURRENT_PARTITION_DEC_LOCK_LEVEL_01}:
 *   `pok_current_partition_dec_lock_level()` shall lower the lock level of the
 *   preemption of the current partition by one and give the new level.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_PART_009}
 * - \hlrid{HLR_PARTITION_ARINC_POK_CURRENT_PARTITION_DEC_LOCK_LEVEL_02}:
 *   `pok_current_partition_dec_lock_level()` shall make the partition choose
 *   the process to run again when the lock level becomes 0.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_PART_018}
 * - \hlrid{HLR_PARTITION_ARINC_POK_CURRENT_PARTITION_DEC_LOCK_LEVEL_03}:
 *   `pok_current_partition_dec_lock_level()` shall refuse to lower the lock
 *   level outside `NORMAL` mode, in the error handler, and at level 0.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_PART_009}
 *   - \srqref{SRQ_JET_PART_019}
 * - \hlrid{HLR_PARTITION_ARINC_POK_CURRENT_PARTITION_DEC_LOCK_LEVEL_04}:
 *   `pok_current_partition_dec_lock_level()` shall refuse an address of the
 *   result that the partition may not write with `POK_ERRNO_EFAULT`.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_MEM_006}
 *   - \srqref{SRQ_JET_MEM_016}
 *
 * @llr
 * - \llrid{LLR_PARTITION_ARINC_POK_CURRENT_PARTITION_DEC_LOCK_LEVEL_01}: If
 *   `is_lock_level_blocked()` returns true,
 *   `pok_current_partition_dec_lock_level()` shall return
 *   `POK_ERRNO_PARTITION_MODE`.
 *   <br>Traces to:
 *   - \hlrref{HLR_PARTITION_ARINC_POK_CURRENT_PARTITION_DEC_LOCK_LEVEL_03}
 * - \llrid{LLR_PARTITION_ARINC_POK_CURRENT_PARTITION_DEC_LOCK_LEVEL_02}: If
 *   `is_lock_level_blocked()` returns false and `lock_level` of the partition
 *   is 0, `pok_current_partition_dec_lock_level()` shall return
 *   `POK_ERRNO_EINVAL`.
 *   <br>Traces to:
 *   - \hlrref{HLR_PARTITION_ARINC_POK_CURRENT_PARTITION_DEC_LOCK_LEVEL_03}
 * - \llrid{LLR_PARTITION_ARINC_POK_CURRENT_PARTITION_DEC_LOCK_LEVEL_03}: If
 *   these checks pass and `jet_user_to_kernel_typed()` returns NULL for
 *   `lock_level`, `pok_current_partition_dec_lock_level()` shall return
 *   `POK_ERRNO_EFAULT` without changing the level.
 *   <br>Traces to:
 *   - \hlrref{HLR_PARTITION_ARINC_POK_CURRENT_PARTITION_DEC_LOCK_LEVEL_04}
 * - \llrid{LLR_PARTITION_ARINC_POK_CURRENT_PARTITION_DEC_LOCK_LEVEL_04}: If
 *   these checks pass and the conversion gives an address,
 *   `pok_current_partition_dec_lock_level()` shall call
 *   `pok_preemption_local_disable()`, decrement `lock_level` of the partition,
 *   call `pok_preemption_local_enable()`, store the new level at that address
 *   and return `POK_ERRNO_OK`.
 *   <br>Traces to:
 *   - \hlrref{HLR_PARTITION_ARINC_POK_CURRENT_PARTITION_DEC_LOCK_LEVEL_01}
 * - \llrid{LLR_PARTITION_ARINC_POK_CURRENT_PARTITION_DEC_LOCK_LEVEL_05}: If the
 *   decremented level is 0 and `prev` of `eligible_elem` of the current thread
 *   is not the head of `eligible_threads` (the thread is not the first eligible
 *   one), `pok_current_partition_dec_lock_level()` shall call
 *   `pok_sched_local_invalidate()` before it enables local preemption.
 *   <br>Traces to:
 *   - \hlrref{HLR_PARTITION_ARINC_POK_CURRENT_PARTITION_DEC_LOCK_LEVEL_02}
 * - \llrid{LLR_PARTITION_ARINC_POK_CURRENT_PARTITION_DEC_LOCK_LEVEL_06}: If the
 *   decremented level is not 0, or the current thread is the first eligible
 *   one, `pok_current_partition_dec_lock_level()` shall not call
 *   `pok_sched_local_invalidate()`.
 *   <br>Traces to:
 *   - \hlrref{HLR_PARTITION_ARINC_POK_CURRENT_PARTITION_DEC_LOCK_LEVEL_02}
 *
 * @design
 * 1. If `is_lock_level_blocked()` is true:
 *    \llrref{LLR_PARTITION_ARINC_POK_CURRENT_PARTITION_DEC_LOCK_LEVEL_01}
 *    1. Return `POK_ERRNO_PARTITION_MODE`.
 * 2. If `current_partition_arinc->lock_level` is equal to `0U`:
 *    \llrref{LLR_PARTITION_ARINC_POK_CURRENT_PARTITION_DEC_LOCK_LEVEL_02}
 *    1. Return `POK_ERRNO_EINVAL`.
 * 3. Set `k_lock_level` to `jet_user_to_kernel_typed(lock_level)`.
 *    \llrref{LLR_PARTITION_ARINC_POK_CURRENT_PARTITION_DEC_LOCK_LEVEL_03}
 * 4. If `k_lock_level` is NULL:
 *    \llrref{LLR_PARTITION_ARINC_POK_CURRENT_PARTITION_DEC_LOCK_LEVEL_03}
 *    1. Return `POK_ERRNO_EFAULT`.
 * 5. Call `pok_preemption_local_disable()`.
 *    \llrref{LLR_PARTITION_ARINC_POK_CURRENT_PARTITION_DEC_LOCK_LEVEL_04}
 * 6. If `--current_partition_arinc->lock_level` is equal to `0U`:
 *    \llrref{LLR_PARTITION_ARINC_POK_CURRENT_PARTITION_DEC_LOCK_LEVEL_04},
 *    \llrref{LLR_PARTITION_ARINC_POK_CURRENT_PARTITION_DEC_LOCK_LEVEL_05},
 *    \llrref{LLR_PARTITION_ARINC_POK_CURRENT_PARTITION_DEC_LOCK_LEVEL_06}
 *    1. If `current_thread->eligible_elem.prev` is not equal to the address of
 *       `current_partition_arinc->eligible_threads`:
 *       \llrref{LLR_PARTITION_ARINC_POK_CURRENT_PARTITION_DEC_LOCK_LEVEL_05},
 *       \llrref{LLR_PARTITION_ARINC_POK_CURRENT_PARTITION_DEC_LOCK_LEVEL_06}
 *       1. Call `pok_sched_local_invalidate()`.
 *          \llrref{LLR_PARTITION_ARINC_POK_CURRENT_PARTITION_DEC_LOCK_LEVEL_05}
 * 7. Call `pok_preemption_local_enable()`.
 *    \llrref{LLR_PARTITION_ARINC_POK_CURRENT_PARTITION_DEC_LOCK_LEVEL_04}
 * 8. Set `*k_lock_level` to `current_partition_arinc->lock_level`.
 *    \llrref{LLR_PARTITION_ARINC_POK_CURRENT_PARTITION_DEC_LOCK_LEVEL_04}
 * 9. Return `POK_ERRNO_OK`.
 *    \llrref{LLR_PARTITION_ARINC_POK_CURRENT_PARTITION_DEC_LOCK_LEVEL_04}
 *
 * @calls
 * - `is_lock_level_blocked()`
 * - `jet_user_to_kernel()`
 * - `pok_preemption_local_disable()`
 * - `pok_sched_local_invalidate()`
 * - `pok_preemption_local_enable()`
 */
pok_ret_e pok_current_partition_dec_lock_level(int32_t * JET_USER lock_level)
{
    if(is_lock_level_blocked()) {
		return POK_ERRNO_PARTITION_MODE;
    }
    if(current_partition_arinc->lock_level == 0U) {
		return POK_ERRNO_EINVAL;
    }

	uint32_t* JET_KUSER k_lock_level = jet_user_to_kernel_typed(lock_level);
	if(k_lock_level == NULL) {
		return POK_ERRNO_EFAULT;
	}

    pok_preemption_local_disable();
    if(--current_partition_arinc->lock_level == 0U)
    {
		if(current_thread->eligible_elem.prev != &current_partition_arinc->eligible_threads)
		{
			// We are not the first thread in eligible queue
			pok_sched_local_invalidate();
		}
	}
	pok_preemption_local_enable();

	*k_lock_level = current_partition_arinc->lock_level;

	return POK_ERRNO_OK;
}


/**
 * @funcname `pok_partition_arinc_init_all`
 *
 * @description
 * Initializes every ARINC 653 partition at boot (and prints its instrumentation
 * description if configured).
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `pok_partitions_arinc` | `pok_partition_arinc_t[]` | The ARINC 653 partitions (generated) |
 * | `pok_partitions_arinc_n` | `const uint8_t` | The number of ARINC 653 partitions |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `i` | `uint32_t` | The index of a partition |
 *
 * @hlr
 * - \hlrid{HLR_PARTITION_ARINC_POK_PARTITION_ARINC_INIT_ALL_01}:
 *   `pok_partition_arinc_init_all()` shall initialize every ARINC 653 partition
 *   of the configuration at boot.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_BOOT_002}
 *   - \srqref{SRQ_JET_CFG_001}
 *   - \srqref{SRQ_JET_CFG_007}
 * - \hlrid{HLR_PARTITION_ARINC_POK_PARTITION_ARINC_INIT_ALL_02}:
 *   `pok_partition_arinc_init_all()` shall print the Cheddar description of
 *   each partition when the kernel is built with instrumentation.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_DBG_010}
 *
 * @llr
 * - \llrid{LLR_PARTITION_ARINC_POK_PARTITION_ARINC_INIT_ALL_01}:
 *   `pok_partition_arinc_init_all()` shall call `pok_partition_arinc_init()`
 *   for each of the `pok_partitions_arinc_n` partitions in order.
 *   <br>Traces to:
 *   - \hlrref{HLR_PARTITION_ARINC_POK_PARTITION_ARINC_INIT_ALL_01}
 * - \llrid{LLR_PARTITION_ARINC_POK_PARTITION_ARINC_INIT_ALL_02}: With
 *   instrumentation, `pok_partition_arinc_init_all()` shall call
 *   `pok_instrumentation_partition_archi()` for each partition after its
 *   initialization.
 *   <br>Traces to:
 *   - \hlrref{HLR_PARTITION_ARINC_POK_PARTITION_ARINC_INIT_ALL_02}
 *
 * @design
 * 1. Set `i` to `0U`; then while `i` is less than `pok_partitions_arinc_n`,
 *    repeat (after each pass: increment `i`):
 *    \llrref{LLR_PARTITION_ARINC_POK_PARTITION_ARINC_INIT_ALL_01}
 *    1. Call `pok_partition_arinc_init()` with the address of
 *       `pok_partitions_arinc[i]`.
 *       \llrref{LLR_PARTITION_ARINC_POK_PARTITION_ARINC_INIT_ALL_01}
 *    2. Call `pok_instrumentation_partition_archi()` with the argument `i`.
 *       \llrref{LLR_PARTITION_ARINC_POK_PARTITION_ARINC_INIT_ALL_02}
 *
 * @calls
 * - `pok_partition_arinc_init()`
 * - `pok_instrumentation_partition_archi()`
 */
void pok_partition_arinc_init_all(void)
{
	for(uint32_t i = 0U; i < pok_partitions_arinc_n; i++)
	{
		pok_partition_arinc_init(&pok_partitions_arinc[i]);

#ifdef POK_NEEDS_INSTRUMENTATION
		pok_instrumentation_partition_archi (i);
#endif
	}
}
