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
 * Modified by Kirill Brazhnikov, 2026-10-31: FP context restore is moved
 * into pok_partition_return_user_fp(), which is also called on return
 * from interrupts.
 *
 * Modified by Kirill Brazhnikov, 2026-10-31: built-in integer types (int,
 * unsigned, short, long, ...) are replaced with the fixed-width types
 * from <stdint.h>.
 *
 * Modified by Kirill Brazhnikov, 2026-11-02: MISRA C:2025: __user/__kuser
 * are renamed JET_USER/JET_KUSER; braces around all if/else/loop bodies;
 * essentially Boolean conditions; U suffixes for unsigned constants;
 * operands of the same essential type; internal linkage for file-local
 * definitions; if-else-if chains end with else; file-local objects are
 * static; slot loops use unsigned counters.
 *
 * Modified by Kirill Brazhnikov, 2026-11-03: MISRA C:2025: increments and
 * assignments are separate statements; first_frame_starts is defined in
 * the function that uses it; operands of mixed precedence are
 * parenthesized; deviations are documented where a guideline can't be
 * followed. Long lines are wrapped at 80 columns.
 *
 * Modified by Kirill Brazhnikov, 2026-11-04: jet_fp_sync() and
 * jet_fp_reload() let the GDB server read and write the floating point
 * registers which are still in the processor; pok_in_user_space starts as
 * FALSE.
 *
 * Modified by Kirill Brazhnikov, 2026-11-08: Doxygen headers for the file
 * and its functions.
 *
 * Modified by Kirill Brazhnikov, 2026-11-13: struct jet_context is used as the
 * typedef jet_context_t (declared in asp/cswitch.h); the structures and enums
 * are used by their typedef names: interrupt_frame, jet_fp_store_t; the macro
 * is moved to kernel/include/core/sched.h.
 *
 * Modified by Kirill Brazhnikov, 2026-11-14: The function headers no longer
 * have a step for the declaration of a local variable; the @locals table
 * describes it, and an initialized one is set in a step.
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
 * The module scheduler: partitions run in the slots of the major time frame
 * (ARINC 653 partition scheduling); switches between and inside partitions,
 * the delivery of partition events, preemption, the return to user mode with
 * the FP state of the thread, and partition restarts.
 */

#include <config.h>

#include <types.h>
#include <asp/arch.h>

#include <core/time.h>
#include <core/sched.h>
#include <core/thread.h>

#include <core/partition.h>

#include <dependencies.h>

#include <core/debug.h>
#include <core/instrumentation.h>
#include <core/error.h>

#include <assert.h>

#include <cswitch.h>
#include <core/space.h>

/*
 * MISRA C:2025 Rule 15.1 deviation (this file): forward gotos to
 * same_partition and out share the code at those labels between the paths.
 */


static pok_time_t            pok_sched_next_deadline;
static pok_time_t            pok_sched_next_major_frame;
static uint8_t               pok_sched_current_slot = 0; /* Which slot are we executing at this time ?*/

pok_partition_t* current_partition = NULL;

#ifdef POK_NEEDS_GDB
interrupt_frame* global_thread_stack = NULL;
volatile pok_bool_t pok_in_user_space = FALSE;
#endif

#ifdef POK_NEEDS_MONITOR
/*
 * Whether current partition has `.is_paused` flag set.
 *
 * This flag affects on the place, where currently used context should
 * be stored on context switch.
 * From the other side, the flag can be changed *outside* of scheduler.
 */
static pok_bool_t current_partition_is_paused;

static jet_context_t* idle_sp;
static uint32_t idle_stack;

/**
 * @funcname `idle_function`
 *
 * @description
 * The context that runs while a paused partition (monitor) has its slot:
 * enables preemption and loops.
 *
 * @globals
 * None.
 *
 * @locals
 * None.
 *
 * @hlr
 * - \hlrid{HLR_SCHED_IDLE_FUNCTION_01}: `idle_function()` shall run nothing in
 *   the slots of a partition that the monitor keeps paused, with preemption
 *   enabled.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_DBG_008}
 *   - \srqref{SRQ_JET_DBG_016}
 *
 * @llr
 * - \llrid{LLR_SCHED_IDLE_FUNCTION_01}: `idle_function()` shall call
 *   `pok_preemption_enable()` and then `ja_inf_loop()`, which does not return.
 *   <br>Traces to:
 *   - \hlrref{HLR_SCHED_IDLE_FUNCTION_01}
 *
 * @design
 * 1. Call `pok_preemption_enable()`. \llrref{LLR_SCHED_IDLE_FUNCTION_01}
 * 2. Call `ja_inf_loop()`. \llrref{LLR_SCHED_IDLE_FUNCTION_01}
 *
 * @calls
 * - `pok_preemption_enable()`
 * - `ja_inf_loop()`
 */
static void idle_function(void)
{
    pok_preemption_enable();

    ja_inf_loop();
}
#endif

/*
 * Pointer to the store area for last executed (user) thread.
 *
 * If no thread has been executed yet, or the last one dies, this is NULL.
 */
static jet_fp_store_t* fp_store_last = NULL;

/**
 * @funcname `pok_partition_reset`
 *
 * @description
 * Resets a partition's scheduling state so that it restarts: no context (it
 * starts again at its entry), a new generation, an empty event ring.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `part` | `pok_partition_t *` | [in,out] The partition. |
 *
 * @globals
 * None.
 *
 * @locals
 * None.
 *
 * @hlr
 * - \hlrid{HLR_SCHED_POK_PARTITION_RESET_01}: `pok_partition_reset()` shall
 *   make a partition start afresh, at its entry, the next time that it runs,
 *   with no pending events.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_SCHED_008}
 *   - \srqref{SRQ_JET_HM_003}
 *
 * @llr
 * - \llrid{LLR_SCHED_POK_PARTITION_RESET_01}: `pok_partition_reset()` shall set
 *   `sp` of `part` to NULL and `restarted_externally` to `TRUE`, increment
 *   `partition_generation`, and set `partition_event_end` and
 *   `partition_event_begin` to 0.
 *   <br>Traces to:
 *   - \hlrref{HLR_SCHED_POK_PARTITION_RESET_01}
 * - \llrid{LLR_SCHED_POK_PARTITION_RESET_02}: If and only if
 *   `partition_generation` is 0 after the increment, `pok_partition_reset()`
 *   shall set it to 1.
 *   <br>Traces to:
 *   - \hlrref{HLR_SCHED_POK_PARTITION_RESET_01}
 *
 * @design
 * 1. Set `part->sp` to `NULL`. \llrref{LLR_SCHED_POK_PARTITION_RESET_01}
 * 2. Set `part->restarted_externally` to `TRUE`.
 *    \llrref{LLR_SCHED_POK_PARTITION_RESET_01}
 * 3. Increment `part->partition_generation`.
 *    \llrref{LLR_SCHED_POK_PARTITION_RESET_01}
 * 4. If `part->partition_generation` is equal to `0U`:
 *    \llrref{LLR_SCHED_POK_PARTITION_RESET_02}
 *    1. Set `part->partition_generation` to `1`.
 * 5. Set `part->partition_event_end` to `0`.
 *    \llrref{LLR_SCHED_POK_PARTITION_RESET_01}
 * 6. Set `part->partition_event_begin` to `0`.
 *    \llrref{LLR_SCHED_POK_PARTITION_RESET_01}
 *
 * @calls
 * None.
 */
static void pok_partition_reset(pok_partition_t* part)
{
    part->sp = NULL;
    part->restarted_externally = TRUE;
    part->partition_generation++;
    if(part->partition_generation == 0U)
    {
        part->partition_generation = 1;
    }

    part->partition_event_end = 0;
    part->partition_event_begin = 0;
}


static pok_bool_t sched_need_recheck;

/**
 * @funcname `start_partition`
 *
 * @description
 * The first code of a partition's context: initializes its state and calls its
 * start operation; loops if that returns.
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `current_partition` | `pok_partition_t *` | The running partition |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `part` | `pok_partition_t *` | The current partition |
 *
 * @hlr
 * - \hlrid{HLR_SCHED_START_PARTITION_01}: `start_partition()` shall clear the
 *   pending events of a partition that starts or restarts and disable its local
 *   preemption, as the first code that the partition runs.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_PART_004}
 *   - \srqref{SRQ_JET_PART_013}
 *   - \srqref{SRQ_JET_SCHED_004}
 * - \hlrid{HLR_SCHED_START_PARTITION_02}: `start_partition()` shall then start
 *   the partition by its start operation.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_PART_013}
 *   - \srqref{SRQ_JET_SCHED_004}
 *
 * @llr
 * - \llrid{LLR_SCHED_START_PARTITION_01}: `start_partition()` shall set, in
 *   `current_partition`, `is_event` to `FALSE`, `partition_event_end` and
 *   `partition_event_begin` to 0 and `preempt_local_disabled` to `TRUE`, and
 *   then call `pok_preemption_enable()`.
 *   <br>Traces to:
 *   - \hlrref{HLR_SCHED_START_PARTITION_01}
 * - \llrid{LLR_SCHED_START_PARTITION_02}: If `part_ops` of the partition is not
 *   NULL and its `start` is not NULL, `start_partition()` shall call `start`
 *   and then `ja_inf_loop()`.
 *   <br>Traces to:
 *   - \hlrref{HLR_SCHED_START_PARTITION_02}
 * - \llrid{LLR_SCHED_START_PARTITION_03}: If `part_ops` of the partition is
 *   NULL or its `start` is NULL, `start_partition()` shall call `ja_inf_loop()`
 *   without calling a start operation.
 *   <br>Traces to:
 *   - \hlrref{HLR_SCHED_START_PARTITION_02}
 *
 * @design
 * 1. Set `part` to `current_partition`. \llrref{LLR_SCHED_START_PARTITION_01}
 * 2. Set `part->is_event` to `FALSE`. \llrref{LLR_SCHED_START_PARTITION_01}
 * 3. Set `part->partition_event_end` to `0`.
 *    \llrref{LLR_SCHED_START_PARTITION_01}
 * 4. Set `part->partition_event_begin` to `0`.
 *    \llrref{LLR_SCHED_START_PARTITION_01}
 * 5. Set `part->preempt_local_disabled` to `TRUE`.
 *    \llrref{LLR_SCHED_START_PARTITION_01}
 * 6. Call `pok_preemption_enable()`. \llrref{LLR_SCHED_START_PARTITION_01}
 * 7. If `part->part_ops` is not NULL and `part->part_ops->start` is not NULL:
 *    \llrref{LLR_SCHED_START_PARTITION_02},
 *    \llrref{LLR_SCHED_START_PARTITION_03}
 *    1. Call the function `part->part_ops->start`.
 *       \llrref{LLR_SCHED_START_PARTITION_02}
 * 8. Call `ja_inf_loop()`. \llrref{LLR_SCHED_START_PARTITION_02},
 *    \llrref{LLR_SCHED_START_PARTITION_03}
 *
 * @calls
 * - `pok_preemption_enable()`
 * - `part->part_ops->start()` (indirect)
 * - `ja_inf_loop()`
 */
static void start_partition(void)
{
    pok_partition_t* part = current_partition;
    // Initialize state for started partition.
    part->is_event = FALSE;
    part->partition_event_end = 0;
    part->partition_event_begin = 0;

    part->preempt_local_disabled = TRUE;

    /* It is safe to enable preemption on new stack. */
    pok_preemption_enable();

    if((part->part_ops != NULL) && (part->part_ops->start != NULL)) {
        part->part_ops->start();
    }

    ja_inf_loop();
}

/**
 * @funcname `intra_partition_switch`
 *
 * @description
 * Switches contexts inside the current partition when needed: between the
 * partition and the idle context when it is paused or resumed (monitor), or
 * restarts the partition's context when a restart was asked for.
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `current_partition` | `pok_partition_t *` | The running partition |
 * | `current_partition_is_paused` | `pok_bool_t` | Whether the context that runs is the idle one of a paused partition |
 * | `idle_sp` | `jet_context_t *` | The idle context |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `new_sp` | `jet_context_t **` | The context to run |
 * | `old_sp` | `jet_context_t **` | Where the context that runs is saved |
 *
 * @hlr
 * - \hlrid{HLR_SCHED_INTRA_PARTITION_SWITCH_01}: `intra_partition_switch()`
 *   shall switch, within the slot of the current partition, between the
 *   partition and the idle context when the monitor pauses or resumes it,
 *   keeping the partition's context.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_DBG_008}
 *   - \srqref{SRQ_JET_DBG_016}
 *   - \srqref{SRQ_JET_SCHED_004}
 * - \hlrid{HLR_SCHED_INTRA_PARTITION_SWITCH_02}: `intra_partition_switch()`
 *   shall start the context of the current partition again at its entry when
 *   the partition was restarted.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_PART_005}
 *   - \srqref{SRQ_JET_HM_004}
 *
 * @llr
 * - \llrid{LLR_SCHED_INTRA_PARTITION_SWITCH_01}: With `POK_NEEDS_MONITOR`, if
 *   `current_partition_is_paused` differs from `is_paused` of
 *   `current_partition` and the old context (the one at `idle_sp` if
 *   `current_partition_is_paused` is true, `sp` of the partition if it is
 *   false) is NULL, `intra_partition_switch()` shall set
 *   `current_partition_is_paused` to `is_paused` and call `ja_context_jump()`
 *   with the new context (the one at `idle_sp` if `is_paused` is true, `sp` of
 *   the partition if it is false).
 *   <br>Traces to:
 *   - \hlrref{HLR_SCHED_INTRA_PARTITION_SWITCH_01}
 * - \llrid{LLR_SCHED_INTRA_PARTITION_SWITCH_02}: With `POK_NEEDS_MONITOR`, if
 *   they differ, the old context is not NULL and the new context is NULL,
 *   `intra_partition_switch()` shall set `current_partition_is_paused` to
 *   `is_paused`, set the new context to the result of `ja_context_init()` for
 *   `initial_sp` of the partition and `start_partition`, and call
 *   `ja_context_switch()` with the address of the old context and the new
 *   context.
 *   <br>Traces to:
 *   - \hlrref{HLR_SCHED_INTRA_PARTITION_SWITCH_01}
 * - \llrid{LLR_SCHED_INTRA_PARTITION_SWITCH_03}: With `POK_NEEDS_MONITOR`, if
 *   they differ and neither context is NULL, `intra_partition_switch()` shall
 *   set `current_partition_is_paused` to `is_paused` and call
 *   `ja_context_switch()` with the address of the old context and the new
 *   context.
 *   <br>Traces to:
 *   - \hlrref{HLR_SCHED_INTRA_PARTITION_SWITCH_01}
 * - \llrid{LLR_SCHED_INTRA_PARTITION_SWITCH_04}: If they are equal (or without
 *   `POK_NEEDS_MONITOR`) and `sp` of `current_partition` is NULL,
 *   `intra_partition_switch()` shall call `ja_context_restart_and_save()` with
 *   `initial_sp` of the partition, `start_partition` and the address of its
 *   `sp`.
 *   <br>Traces to:
 *   - \hlrref{HLR_SCHED_INTRA_PARTITION_SWITCH_02}
 * - \llrid{LLR_SCHED_INTRA_PARTITION_SWITCH_05}: If they are equal (or without
 *   `POK_NEEDS_MONITOR`) and `sp` of `current_partition` is not NULL,
 *   `intra_partition_switch()` shall return without a context switch.
 *   <br>Traces to:
 *   - \hlrref{HLR_SCHED_INTRA_PARTITION_SWITCH_02}
 *
 * @design
 * 1. Set `old_sp` to the address of `current_partition->sp`.
 *    \llrref{LLR_SCHED_INTRA_PARTITION_SWITCH_01},
 *    \llrref{LLR_SCHED_INTRA_PARTITION_SWITCH_02},
 *    \llrref{LLR_SCHED_INTRA_PARTITION_SWITCH_03},
 *    \llrref{LLR_SCHED_INTRA_PARTITION_SWITCH_04},
 *    \llrref{LLR_SCHED_INTRA_PARTITION_SWITCH_05}
 * 2. Set `new_sp` to `old_sp`. \llrref{LLR_SCHED_INTRA_PARTITION_SWITCH_01},
 *    \llrref{LLR_SCHED_INTRA_PARTITION_SWITCH_02},
 *    \llrref{LLR_SCHED_INTRA_PARTITION_SWITCH_03},
 *    \llrref{LLR_SCHED_INTRA_PARTITION_SWITCH_04},
 *    \llrref{LLR_SCHED_INTRA_PARTITION_SWITCH_05}
 * 3. If `current_partition_is_paused` is true:
 *    \llrref{LLR_SCHED_INTRA_PARTITION_SWITCH_01},
 *    \llrref{LLR_SCHED_INTRA_PARTITION_SWITCH_02},
 *    \llrref{LLR_SCHED_INTRA_PARTITION_SWITCH_03}
 *    1. Set `old_sp` to the address of `idle_sp`.
 * 4. If `current_partition->is_paused` is true:
 *    \llrref{LLR_SCHED_INTRA_PARTITION_SWITCH_01},
 *    \llrref{LLR_SCHED_INTRA_PARTITION_SWITCH_02},
 *    \llrref{LLR_SCHED_INTRA_PARTITION_SWITCH_03}
 *    1. Set `new_sp` to the address of `idle_sp`.
 * 5. If `old_sp` is not equal to `new_sp`:
 *    \llrref{LLR_SCHED_INTRA_PARTITION_SWITCH_01},
 *    \llrref{LLR_SCHED_INTRA_PARTITION_SWITCH_02},
 *    \llrref{LLR_SCHED_INTRA_PARTITION_SWITCH_03},
 *    \llrref{LLR_SCHED_INTRA_PARTITION_SWITCH_04},
 *    \llrref{LLR_SCHED_INTRA_PARTITION_SWITCH_05}
 *    1. Set `current_partition_is_paused` to `current_partition->is_paused`.
 *       \llrref{LLR_SCHED_INTRA_PARTITION_SWITCH_01},
 *       \llrref{LLR_SCHED_INTRA_PARTITION_SWITCH_02},
 *       \llrref{LLR_SCHED_INTRA_PARTITION_SWITCH_03}
 *    2. If the value at `old_sp` is NULL:
 *       \llrref{LLR_SCHED_INTRA_PARTITION_SWITCH_01}
 *       1. Call `ja_context_jump()` with the value at `new_sp`.
 *       2. Return.
 *    3. Otherwise, if the value at `new_sp` is NULL:
 *       \llrref{LLR_SCHED_INTRA_PARTITION_SWITCH_02}
 *       1. Call `ja_context_init()` with the arguments
 *          `current_partition->initial_sp` and the address of
 *          `start_partition`, and store the result in `*new_sp`.
 *    4. Call `ja_context_switch()` with the arguments `old_sp` and the value at
 *       `new_sp`. \llrref{LLR_SCHED_INTRA_PARTITION_SWITCH_02},
 *       \llrref{LLR_SCHED_INTRA_PARTITION_SWITCH_03}
 *    5. Return. \llrref{LLR_SCHED_INTRA_PARTITION_SWITCH_02},
 *       \llrref{LLR_SCHED_INTRA_PARTITION_SWITCH_03}
 * 6. If the value at `old_sp` is NULL:
 *    \llrref{LLR_SCHED_INTRA_PARTITION_SWITCH_04},
 *    \llrref{LLR_SCHED_INTRA_PARTITION_SWITCH_05}
 *    1. Call `ja_context_restart_and_save()` with the arguments
 *       `current_partition->initial_sp`, the address of `start_partition` and
 *       `old_sp`. \llrref{LLR_SCHED_INTRA_PARTITION_SWITCH_04}
 *
 * @calls
 * - `ja_context_jump()`
 * - `ja_context_init()`
 * - `ja_context_switch()`
 * - `ja_context_restart_and_save()`
 */
static void intra_partition_switch(void)
{
    jet_context_t** old_sp = &current_partition->sp;

#ifdef POK_NEEDS_MONITOR
    jet_context_t** new_sp = old_sp;
    if(current_partition_is_paused) {
        old_sp = &idle_sp;
    }
    if(current_partition->is_paused) {
        new_sp = &idle_sp;
    }

    if(old_sp != new_sp)
    {
        /* Need to switch context */
        current_partition_is_paused = current_partition->is_paused;

        if(*old_sp == NULL)
        {
            /*
             * Restart is requested by currently executed context.
             * (new context is idle, so it doesn't need restart.)
             *
             * Perform jump instead of switch.
             */
             jet_context_jump(*new_sp);
             return;
        }
        else if(*new_sp == NULL)
        {
            /*
             * Restart is requested by new context.
             * (current context is idle, so it doesn't need restart.)
             *
             * Need to initialize new context before switch.
             */
            *new_sp = jet_context_init(
                current_partition->initial_sp,
                &start_partition);
        }
        else
        {
            // Both contexts exist: a plain switch.
        }
        jet_context_switch(old_sp, *new_sp);
        return;
    }

#endif /* POK_NEEDS_MONITOR */
    // old_sp == new_sp
    if(*old_sp == NULL) /* Same context, restart requested. */
    {
        jet_context_restart_and_save(current_partition->initial_sp,
            &start_partition,
            old_sp);
    }
}


/**
 * @funcname `inter_partition_switch`
 *
 * @description
 * Switches to another partition: its memory space and its context (created if
 * it restarts; the idle context if it is paused).
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `part` | `pok_partition_t *` | [in,out] The partition to run. |
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `current_partition` | `pok_partition_t *` | The running partition |
 * | `current_partition_is_paused` | `pok_bool_t` | Whether the context that runs is the idle one of a paused partition |
 * | `global_thread_stack` | `interrupt_frame *` | The interrupted context at the last trap (GDB) |
 * | `idle_sp` | `jet_context_t *` | The idle context |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `new_sp` | `jet_context_t **` | The context of the new partition |
 * | `old_sp` | `jet_context_t **` | Where the context of the old partition is saved |
 *
 * @hlr
 * - \hlrid{HLR_SCHED_INTER_PARTITION_SWITCH_01}: `inter_partition_switch()`
 *   shall switch the processor to the memory space of another partition.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_SCHED_003}
 * - \hlrid{HLR_SCHED_INTER_PARTITION_SWITCH_02}: `inter_partition_switch()`
 *   shall switch the processor to the context of the other partition, resuming
 *   the partition where it was interrupted.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_SCHED_003}
 *   - \srqref{SRQ_JET_SCHED_004}
 * - \hlrid{HLR_SCHED_INTER_PARTITION_SWITCH_03}: `inter_partition_switch()`
 *   shall start the other partition at its entry after a restart.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_SCHED_004}
 * - \hlrid{HLR_SCHED_INTER_PARTITION_SWITCH_04}: `inter_partition_switch()`
 *   shall run the idle context instead of a partition that the monitor keeps
 *   paused.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_DBG_008}
 *   - \srqref{SRQ_JET_DBG_016}
 *
 * @llr
 * - \llrid{LLR_SCHED_INTER_PARTITION_SWITCH_01}: `inter_partition_switch()`
 *   shall set `entry_sp` of `current_partition` to `global_thread_stack` (with
 *   `POK_NEEDS_GDB`) and then `current_partition` to `part`.
 *   <br>Traces to:
 *   - \hlrref{HLR_SCHED_INTER_PARTITION_SWITCH_01}
 *   - \hlrref{HLR_SCHED_INTER_PARTITION_SWITCH_02}
 * - \llrid{LLR_SCHED_INTER_PARTITION_SWITCH_02}: If `space_id` of `part` is not
 *   0, `inter_partition_switch()` shall call `ja_space_switch()` with
 *   `space_id` of `part`.
 *   <br>Traces to:
 *   - \hlrref{HLR_SCHED_INTER_PARTITION_SWITCH_01}
 * - \llrid{LLR_SCHED_INTER_PARTITION_SWITCH_03}: If `space_id` of `part` is 0,
 *   `inter_partition_switch()` shall call `ja_space_switch()` with 0.
 *   <br>Traces to:
 *   - \hlrref{HLR_SCHED_INTER_PARTITION_SWITCH_01}
 * - \llrid{LLR_SCHED_INTER_PARTITION_SWITCH_04}: With `POK_NEEDS_MONITOR`,
 *   `inter_partition_switch()` shall take as the old context the one at
 *   `idle_sp` if `current_partition_is_paused` is true and `sp` of the old
 *   partition if it is false, and as the new context the one at `idle_sp` if
 *   `is_paused` of `part` is true and `sp` of `part` if it is false.
 *   <br>Traces to:
 *   - \hlrref{HLR_SCHED_INTER_PARTITION_SWITCH_04}
 * - \llrid{LLR_SCHED_INTER_PARTITION_SWITCH_05}: With `POK_NEEDS_MONITOR`, if
 *   the old and the new context are the same object, `inter_partition_switch()`
 *   shall return without a context switch.
 *   <br>Traces to:
 *   - \hlrref{HLR_SCHED_INTER_PARTITION_SWITCH_04}
 * - \llrid{LLR_SCHED_INTER_PARTITION_SWITCH_06}: With `POK_NEEDS_MONITOR`, if
 *   they are different objects, `inter_partition_switch()` shall set
 *   `current_partition_is_paused` to `is_paused` of `part`.
 *   <br>Traces to:
 *   - \hlrref{HLR_SCHED_INTER_PARTITION_SWITCH_04}
 * - \llrid{LLR_SCHED_INTER_PARTITION_SWITCH_07}: If and only if the new context
 *   is NULL, `inter_partition_switch()` shall set it to the result of
 *   `ja_context_init()` for `initial_sp` of `part` and `start_partition`.
 *   <br>Traces to:
 *   - \hlrref{HLR_SCHED_INTER_PARTITION_SWITCH_03}
 * - \llrid{LLR_SCHED_INTER_PARTITION_SWITCH_08}: If the old context is NULL,
 *   `inter_partition_switch()` shall call `ja_context_jump()` with the new
 *   context.
 *   <br>Traces to:
 *   - \hlrref{HLR_SCHED_INTER_PARTITION_SWITCH_02}
 *   - \hlrref{HLR_SCHED_INTER_PARTITION_SWITCH_03}
 * - \llrid{LLR_SCHED_INTER_PARTITION_SWITCH_09}: If the old context is not
 *   NULL, `inter_partition_switch()` shall call `ja_context_switch()` with the
 *   address of the old context and the new context.
 *   <br>Traces to:
 *   - \hlrref{HLR_SCHED_INTER_PARTITION_SWITCH_02}
 *   - \hlrref{HLR_SCHED_INTER_PARTITION_SWITCH_03}
 *
 * @design
 * 1. Set `old_sp` to the address of `current_partition->sp`.
 *    \llrref{LLR_SCHED_INTER_PARTITION_SWITCH_04},
 *    \llrref{LLR_SCHED_INTER_PARTITION_SWITCH_08},
 *    \llrref{LLR_SCHED_INTER_PARTITION_SWITCH_09}
 * 2. Set `new_sp` to the address of `part->sp`.
 *    \llrref{LLR_SCHED_INTER_PARTITION_SWITCH_04},
 *    \llrref{LLR_SCHED_INTER_PARTITION_SWITCH_08},
 *    \llrref{LLR_SCHED_INTER_PARTITION_SWITCH_09}
 * 3. Set `current_partition->entry_sp` to `global_thread_stack`.
 *    \llrref{LLR_SCHED_INTER_PARTITION_SWITCH_01}
 * 4. Set `current_partition` to `part`.
 *    \llrref{LLR_SCHED_INTER_PARTITION_SWITCH_01}
 * 5. If `part->space_id` is not equal to `0U`:
 *    \llrref{LLR_SCHED_INTER_PARTITION_SWITCH_02}
 *    1. Call `ja_space_switch()` with the argument `part->space_id`.
 * 6. Otherwise: \llrref{LLR_SCHED_INTER_PARTITION_SWITCH_03}
 *    1. Call `ja_space_switch()` with the argument `0`.
 * 7. If `current_partition_is_paused` is true:
 *    \llrref{LLR_SCHED_INTER_PARTITION_SWITCH_04}
 *    1. Set `old_sp` to the address of `idle_sp`.
 * 8. If `part->is_paused` is true: \llrref{LLR_SCHED_INTER_PARTITION_SWITCH_04}
 *    1. Set `new_sp` to the address of `idle_sp`.
 * 9. If `old_sp` is equal to `new_sp`:
 *    \llrref{LLR_SCHED_INTER_PARTITION_SWITCH_05}
 *    1. Return.
 * 10. Set `current_partition_is_paused` to `part->is_paused`.
 *     \llrref{LLR_SCHED_INTER_PARTITION_SWITCH_06}
 * 11. If the value at `new_sp` is NULL:
 *     \llrref{LLR_SCHED_INTER_PARTITION_SWITCH_07}
 *     1. Call `ja_context_init()` with the arguments `part->initial_sp` and the
 *        address of `start_partition`, and store the result in `*new_sp`.
 * 12. If the value at `old_sp` is NULL:
 *     \llrref{LLR_SCHED_INTER_PARTITION_SWITCH_08}
 *     1. Call `ja_context_jump()` with the value at `new_sp`.
 * 13. Otherwise: \llrref{LLR_SCHED_INTER_PARTITION_SWITCH_09}
 *     1. Call `ja_context_switch()` with the arguments `old_sp` and the value
 *        at `new_sp`.
 *
 * @calls
 * - `ja_space_switch()`
 * - `ja_context_init()`
 * - `ja_context_jump()`
 * - `ja_context_switch()`
 */
static void inter_partition_switch(pok_partition_t* part)
{
    jet_context_t** old_sp = &current_partition->sp;
    jet_context_t** new_sp = &part->sp;
#ifdef POK_NEEDS_GDB
    current_partition->entry_sp = global_thread_stack;
#endif
    current_partition = part;

    if(part->space_id != 0U) {
        pok_space_switch(part->space_id);
    } else {
        pok_space_switch(0); // TODO: This should disable all user space tables
    }
#ifdef POK_NEEDS_MONITOR
    if(current_partition_is_paused) {
        old_sp = &idle_sp;
    }
    if(part->is_paused)
    {
        assert(idle_sp != NULL); // Idle sp shouldn't be 0.
        new_sp = &idle_sp;
    }

    if(old_sp == new_sp)
    {
        // Idle thread is continued. Nothing to do.
        return;
    }

    current_partition_is_paused = part->is_paused;
#endif /* POK_NEEDS_MONITOR */
    // old_sp != new_sp

    if(*new_sp == NULL)
    {
        /*
         * Restart is requested by new context.
         * (current context is different)
         *
         * Need to initialize new context before switch.
         */
        *new_sp = jet_context_init(part->initial_sp, &start_partition);
    }

    if(*old_sp == NULL)
    {
        jet_context_jump(*new_sp);
    }
    else
    {
        jet_context_switch(old_sp, *new_sp);
    }
}


/**
 * @funcname `pok_sched_restart`
 *
 * @description
 * (Re)starts the module schedule: every partition is reset, the first slot of a
 * new major frame starts now, and its partition's context is entered. Doesn't
 * return.
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `current_partition` | `pok_partition_t *` | The running partition |
 * | `current_partition_is_paused` | `pok_bool_t` | Whether the context that runs is the idle one of a paused partition |
 * | `idle_sp` | `jet_context_t *` | The idle context |
 * | `idle_stack` | `uint32_t` | The stack of the idle context |
 * | `kernel_state` | `pok_system_state_t` | The kernel's system state |
 * | `pok_config_scheduling_major_frame` | `const pok_time_t` | The major time frame (generated) |
 * | `pok_module_sched` | `const pok_sched_slot_t[]` | The module schedule: the slots (generated) |
 * | `pok_sched_current_slot` | `uint8_t` | The slot that runs |
 * | `pok_sched_next_deadline` | `pok_time_t` | The end of the current slot |
 * | `pok_sched_next_major_frame` | `pok_time_t` | The start of the next major frame |
 * | `sched_need_recheck` | `pok_bool_t` | Whether the scheduler must look at the time |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `first_frame_starts` | `static pok_time_t` | The start of the first major frame |
 * | `new_sp` | `jet_context_t **` | The context to enter |
 *
 * @hlr
 * - \hlrid{HLR_SCHED_POK_SCHED_RESTART_01}: `pok_sched_restart()` shall start
 *   the schedule from the first slot of a new major frame, at the present time,
 *   with every partition started afresh, and not return.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_SCHED_001}
 *   - \srqref{SRQ_JET_SCHED_008}
 *   - \srqref{SRQ_JET_BOOT_004}
 *   - \srqref{SRQ_JET_BOOT_007}
 *   - \srqref{SRQ_JET_CFG_002}
 *   - \srqref{SRQ_JET_CFG_009}
 *
 * @llr
 * - \llrid{LLR_SCHED_POK_SCHED_RESTART_01}: `pok_sched_restart()` shall set
 *   `first_frame_starts` to the result of `jet_system_time()`, set `idle_sp` to
 *   the result of `ja_context_init()` for `idle_stack` and `idle_function`
 *   (with `POK_NEEDS_MONITOR`), call `for_each_partition()` with
 *   `pok_partition_reset`, set `sched_need_recheck` to `FALSE`, call
 *   `barrier()`, set `pok_sched_current_slot` to 0,
 *   `pok_sched_next_major_frame` to `first_frame_starts` plus
 *   `pok_config_scheduling_major_frame`, `pok_sched_next_deadline` to
 *   `duration` of `pok_module_sched[0]` plus `first_frame_starts`, and
 *   `current_partition` to `partition` of `pok_module_sched[0]`.
 *   <br>Traces to:
 *   - \hlrref{HLR_SCHED_POK_SCHED_RESTART_01}
 * - \llrid{LLR_SCHED_POK_SCHED_RESTART_02}: With `POK_NEEDS_MONITOR`, if
 *   `is_paused` of that partition is true, `pok_sched_restart()` shall take the
 *   context at `idle_sp` as the new context, and set
 *   `current_partition_is_paused` to `TRUE`.
 *   <br>Traces to:
 *   - \hlrref{HLR_SCHED_POK_SCHED_RESTART_01}
 * - \llrid{LLR_SCHED_POK_SCHED_RESTART_03}: If `is_paused` of that partition is
 *   false (or without `POK_NEEDS_MONITOR`), `pok_sched_restart()` shall take
 *   `sp` of the partition as the new context, and set
 *   `current_partition_is_paused` to `FALSE` (with `POK_NEEDS_MONITOR`).
 *   <br>Traces to:
 *   - \hlrref{HLR_SCHED_POK_SCHED_RESTART_01}
 * - \llrid{LLR_SCHED_POK_SCHED_RESTART_04}: If and only if the new context is
 *   NULL, `pok_sched_restart()` shall set it to the result of
 *   `ja_context_init()` for `initial_sp` of the partition and
 *   `start_partition`.
 *   <br>Traces to:
 *   - \hlrref{HLR_SCHED_POK_SCHED_RESTART_01}
 * - \llrid{LLR_SCHED_POK_SCHED_RESTART_05}: If `space_id` of the partition is
 *   not `0xff`, `pok_sched_restart()` shall call `ja_space_switch()` with its
 *   `space_id`.
 *   <br>Traces to:
 *   - \hlrref{HLR_SCHED_POK_SCHED_RESTART_01}
 * - \llrid{LLR_SCHED_POK_SCHED_RESTART_06}: If `space_id` of the partition is
 *   `0xff`, `pok_sched_restart()` shall call `ja_space_switch()` with `0xff`.
 *   <br>Traces to:
 *   - \hlrref{HLR_SCHED_POK_SCHED_RESTART_01}
 * - \llrid{LLR_SCHED_POK_SCHED_RESTART_07}: `pok_sched_restart()` shall then
 *   set `kernel_state` to `POK_SYSTEM_STATE_OS_PART` and call
 *   `ja_context_jump()` with the new context.
 *   <br>Traces to:
 *   - \hlrref{HLR_SCHED_POK_SCHED_RESTART_01}
 *
 * @design
 * 1. Set `first_frame_starts` to `jet_system_time()`.
 *    \llrref{LLR_SCHED_POK_SCHED_RESTART_01}
 * 2. Call `ja_context_init()` with the arguments `idle_stack` and the address
 *    of `idle_function`, and store the result in `idle_sp`.
 *    \llrref{LLR_SCHED_POK_SCHED_RESTART_01}
 * 3. Call `for_each_partition()` with the address of `pok_partition_reset`.
 *    \llrref{LLR_SCHED_POK_SCHED_RESTART_01}
 * 4. Set `sched_need_recheck` to `FALSE`.
 *    \llrref{LLR_SCHED_POK_SCHED_RESTART_01}
 * 5. Execute the assembly code `barrier()`.
 *    \llrref{LLR_SCHED_POK_SCHED_RESTART_01}
 * 6. Set `pok_sched_current_slot` to `0`.
 *    \llrref{LLR_SCHED_POK_SCHED_RESTART_01}
 * 7. Set `pok_sched_next_major_frame` to
 *    `first_frame_starts + pok_config_scheduling_major_frame`.
 *    \llrref{LLR_SCHED_POK_SCHED_RESTART_01}
 * 8. Set `pok_sched_next_deadline` to
 *    `pok_module_sched[0].duration + first_frame_starts`.
 *    \llrref{LLR_SCHED_POK_SCHED_RESTART_01}
 * 9. Set `current_partition` to `pok_module_sched[0].partition`.
 *    \llrref{LLR_SCHED_POK_SCHED_RESTART_01}
 * 10. Set `new_sp` to the address of `current_partition->sp`.
 *     \llrref{LLR_SCHED_POK_SCHED_RESTART_03}
 * 11. If `current_partition->is_paused` is true:
 *     \llrref{LLR_SCHED_POK_SCHED_RESTART_02}
 *     1. Set `new_sp` to the address of `idle_sp`.
 * 12. Set `current_partition_is_paused` to `current_partition->is_paused`.
 *     \llrref{LLR_SCHED_POK_SCHED_RESTART_02},
 *     \llrref{LLR_SCHED_POK_SCHED_RESTART_03}
 * 13. If the value at `new_sp` is NULL: \llrref{LLR_SCHED_POK_SCHED_RESTART_04}
 *     1. Call `ja_context_init()` with the arguments
 *        `current_partition->initial_sp` and the address of `start_partition`,
 *        and store the result in `*new_sp`.
 * 14. If `current_partition->space_id` is not equal to `0xffU`:
 *     \llrref{LLR_SCHED_POK_SCHED_RESTART_05}
 *     1. Call `ja_space_switch()` with the argument
 *        `current_partition->space_id`.
 * 15. Otherwise: \llrref{LLR_SCHED_POK_SCHED_RESTART_06}
 *     1. Call `ja_space_switch()` with the argument `0xff`.
 * 16. Set `kernel_state` to `POK_SYSTEM_STATE_OS_PART`.
 *     \llrref{LLR_SCHED_POK_SCHED_RESTART_07}
 * 17. Call `ja_context_jump()` with the value at `new_sp`.
 *     \llrref{LLR_SCHED_POK_SCHED_RESTART_07}
 *
 * @calls
 * - `ja_system_time()`
 * - `ja_context_init()`
 * - `for_each_partition()`
 * - `ja_space_switch()`
 * - `ja_context_jump()`
 */
void pok_sched_restart (void)
{
    // Time when first major frame is started.
    static pok_time_t first_frame_starts;

    jet_context_t** new_sp;

    first_frame_starts = jet_system_time();
#ifdef POK_NEEDS_MONITOR
    idle_sp = jet_context_init(idle_stack, &idle_function);
#endif /*POK_NEEDS_MONITOR */

    for_each_partition(&pok_partition_reset);

    sched_need_recheck = FALSE; // Acquire semantic
    barrier();

    // Navigate to the first slot
    pok_sched_current_slot = 0;
    pok_sched_next_major_frame = first_frame_starts + pok_config_scheduling_major_frame;
    pok_sched_next_deadline = pok_module_sched[0].duration + first_frame_starts;

    current_partition = pok_module_sched[0].partition;

    new_sp = &current_partition->sp;
#ifdef POK_NEEDS_MONITOR
    if(current_partition->is_paused) {
        new_sp = &idle_sp;
    }
    current_partition_is_paused = current_partition->is_paused;
#endif /*POK_NEEDS_MONITOR */
    if(*new_sp == NULL)
    {
        *new_sp = jet_context_init(current_partition->initial_sp,
            &start_partition);
    }

    if(current_partition->space_id != 0xffU) {
        pok_space_switch(current_partition->space_id);
    } else {
        pok_space_switch(0xff); // TODO: This should disable all user space tables
    }

    kernel_state = POK_SYSTEM_STATE_OS_PART;

    jet_context_jump(*new_sp);
}

/**
 * @funcname `pok_sched_start`
 *
 * @description
 * Starts the module scheduler at boot: allocates the idle stack (monitor) and
 * starts the schedule. Doesn't return.
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `idle_stack` | `uint32_t` | The stack of the idle context |
 *
 * @locals
 * None.
 *
 * @hlr
 * - \hlrid{HLR_SCHED_POK_SCHED_START_01}: `pok_sched_start()` shall start the
 *   schedule at boot and not return.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_BOOT_004}
 *   - \srqref{SRQ_JET_BOOT_007}
 *
 * @llr
 * - \llrid{LLR_SCHED_POK_SCHED_START_01}: With `POK_NEEDS_MONITOR`,
 *   `pok_sched_start()` shall set `idle_stack` to the result of
 *   `pok_stack_alloc()` for `KERNEL_STACK_SIZE_DEFAULT`, before it calls
 *   `pok_sched_restart()`.
 *   <br>Traces to:
 *   - \hlrref{HLR_SCHED_POK_SCHED_START_01}
 * - \llrid{LLR_SCHED_POK_SCHED_START_02}: `pok_sched_start()` shall call
 *   `pok_sched_restart()` once.
 *   <br>Traces to:
 *   - \hlrref{HLR_SCHED_POK_SCHED_START_01}
 *
 * @design
 * 1. Call `pok_stack_alloc()` with the argument `KERNEL_STACK_SIZE_DEFAULT`,
 *    and store the result in `idle_stack`.
 *    \llrref{LLR_SCHED_POK_SCHED_START_01}
 * 2. Call `pok_sched_restart()`. \llrref{LLR_SCHED_POK_SCHED_START_02}
 *
 * @calls
 * - `pok_stack_alloc()`
 * - `pok_sched_restart()`
 */
void pok_sched_start (void)
{
#ifdef POK_NEEDS_MONITOR
    idle_stack = pok_stack_alloc(KERNEL_STACK_SIZE_DEFAULT);
#endif /*POK_NEEDS_MONITOR */
    pok_sched_restart();
}

/* Static variables used to trace with qemu */
#if QEMU_TRACING
static int32_t started = 0;
static int32_t ended = 0;
static int32_t nb_major_time_frame = 0;
#endif /* QEMU_TRACE */
/********************************************/

/**
 * @funcname `pok_sched`
 *
 * @description
 * The module scheduler: when the current slot is over, moves to the next slot
 * (and major frame) and switches to its partition, delivering the old
 * partition's timer event if it expired; otherwise switches inside the current
 * partition if needed. Called with preemption disabled.
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `current_partition` | `pok_partition_t *` | The running partition |
 * | `pok_config_scheduling_major_frame` | `const pok_time_t` | The major time frame (generated) |
 * | `pok_module_sched` | `const pok_sched_slot_t[]` | The module schedule: the slots (generated) |
 * | `pok_module_sched_n` | `const uint8_t` | The number of slots of the schedule |
 * | `pok_sched_current_slot` | `uint8_t` | The slot that runs |
 * | `pok_sched_next_deadline` | `pok_time_t` | The end of the current slot |
 * | `pok_sched_next_major_frame` | `pok_time_t` | The start of the next major frame |
 * | `sched_need_recheck` | `pok_bool_t` | Whether the scheduler must look at the time |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `new_partition` | `pok_partition_t *` | The partition of the new slot |
 * | `now` | `pok_time_t` | The current time |
 * | `part` | `pok_partition_t *` | The partition that ran |
 *
 * @hlr
 * - \hlrid{HLR_SCHED_POK_SCHED_01}: `pok_sched()` shall end the current slot
 *   when its duration has elapsed, start the next slot of the schedule (the
 *   first slot of the next major frame after the last one) and switch to its
 *   partition.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_SCHED_001}
 *   - \srqref{SRQ_JET_SCHED_002}
 *   - \srqref{SRQ_JET_SCHED_003}
 *   - \srqref{SRQ_JET_CFG_002}
 *   - \srqref{SRQ_JET_CFG_009}
 *   - \srqref{SRQ_JET_CFG_010}
 * - \hlrid{HLR_SCHED_POK_SCHED_02}: `pok_sched()` shall give the partition that
 *   leaves the processor the event of its timer if the time of the timer has
 *   come.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_SCHED_005}
 * - \hlrid{HLR_SCHED_POK_SCHED_03}: `pok_sched()` shall otherwise make the
 *   switches that the current partition needs within its slot (pause, resume,
 *   restart).
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_DBG_008}
 *   - \srqref{SRQ_JET_DBG_016}
 *   - \srqref{SRQ_JET_PART_005}
 *
 * @llr
 * - \llrid{LLR_SCHED_POK_SCHED_01}: If `flag_test_and_reset()` returns false
 *   for `sched_need_recheck`, `pok_sched()` shall return without calling
 *   `jet_system_time()`.
 *   <br>Traces to:
 *   - \hlrref{HLR_SCHED_POK_SCHED_01}
 *   - \hlrref{HLR_SCHED_POK_SCHED_02}
 *   - \hlrref{HLR_SCHED_POK_SCHED_03}
 * - \llrid{LLR_SCHED_POK_SCHED_02}: If it returns true and
 *   `pok_sched_next_deadline` is greater than the result of
 *   `jet_system_time()`, `pok_sched()` shall call `intra_partition_switch()`
 *   without changing the slot.
 *   <br>Traces to:
 *   - \hlrref{HLR_SCHED_POK_SCHED_03}
 * - \llrid{LLR_SCHED_POK_SCHED_03}: If `pok_sched_next_deadline` is not greater
 *   than the result of `jet_system_time()`, `pok_sched()` shall increment
 *   `pok_sched_current_slot` and then add `duration` of the slot
 *   `pok_sched_current_slot` of `pok_module_sched` to
 *   `pok_sched_next_deadline`.
 *   <br>Traces to:
 *   - \hlrref{HLR_SCHED_POK_SCHED_01}
 * - \llrid{LLR_SCHED_POK_SCHED_04}: If and only if the incremented
 *   `pok_sched_current_slot` is `pok_module_sched_n`, `pok_sched()` shall add
 *   `pok_config_scheduling_major_frame` to `pok_sched_next_major_frame` and set
 *   `pok_sched_current_slot` to 0, before it adds the duration.
 *   <br>Traces to:
 *   - \hlrref{HLR_SCHED_POK_SCHED_01}
 * - \llrid{LLR_SCHED_POK_SCHED_05}: If the slot ended and `partition` of the
 *   new slot is `current_partition` (at entry), `pok_sched()` shall call
 *   `intra_partition_switch()`.
 *   <br>Traces to:
 *   - \hlrref{HLR_SCHED_POK_SCHED_03}
 * - \llrid{LLR_SCHED_POK_SCHED_06}: If the slot ended and `partition` of the
 *   new slot is another partition, `pok_sched()` shall call
 *   `inter_partition_switch()` with it.
 *   <br>Traces to:
 *   - \hlrref{HLR_SCHED_POK_SCHED_01}
 * - \llrid{LLR_SCHED_POK_SCHED_07}: When `inter_partition_switch()` returns, if
 *   `timer` of the partition current at entry is not 0 and not greater than the
 *   result of `jet_system_time()`, `pok_sched()` shall call
 *   `pok_partition_add_event()` with that partition,
 *   `JET_PARTITION_EVENT_TYPE_TIMER` and 0, and set its `timer` to 0.
 *   <br>Traces to:
 *   - \hlrref{HLR_SCHED_POK_SCHED_02}
 * - \llrid{LLR_SCHED_POK_SCHED_08}: When `inter_partition_switch()` returns, if
 *   that `timer` is 0 or greater than the result of `jet_system_time()`,
 *   `pok_sched()` shall leave the timer as it is and add no event.
 *   <br>Traces to:
 *   - \hlrref{HLR_SCHED_POK_SCHED_02}
 *
 * @design
 * 1. Set `part` to `current_partition`. \llrref{LLR_SCHED_POK_SCHED_05},
 *    \llrref{LLR_SCHED_POK_SCHED_06}, \llrref{LLR_SCHED_POK_SCHED_07}
 * 2. If `flag_test_and_reset(&sched_need_recheck)` is false:
 *    \llrref{LLR_SCHED_POK_SCHED_01}
 *    1. Return.
 * 3. Set `now` to `jet_system_time()`. \llrref{LLR_SCHED_POK_SCHED_02},
 *    \llrref{LLR_SCHED_POK_SCHED_03}
 * 4. If `pok_sched_next_deadline` is greater than `now`:
 *    \llrref{LLR_SCHED_POK_SCHED_02}, \llrref{LLR_SCHED_POK_SCHED_03}
 *    1. Go to the label `same_partition`. \llrref{LLR_SCHED_POK_SCHED_02}
 * 5. Set `pok_sched_current_slot` to `(pok_sched_current_slot + 1U)`.
 *    \llrref{LLR_SCHED_POK_SCHED_03}
 * 6. If `pok_sched_current_slot` is equal to `pok_module_sched_n`:
 *    \llrref{LLR_SCHED_POK_SCHED_04}
 *    1. Add `pok_config_scheduling_major_frame` to
 *       `pok_sched_next_major_frame`.
 *    2. Set `pok_sched_current_slot` to `0`.
 * 7. Add `pok_module_sched[pok_sched_current_slot].duration` to
 *    `pok_sched_next_deadline`. \llrref{LLR_SCHED_POK_SCHED_03}
 * 8. Set `new_partition` to
 *    `pok_module_sched[pok_sched_current_slot].partition`.
 *    \llrref{LLR_SCHED_POK_SCHED_05}, \llrref{LLR_SCHED_POK_SCHED_06}
 * 9. If `new_partition` is equal to `part`: \llrref{LLR_SCHED_POK_SCHED_05},
 *    \llrref{LLR_SCHED_POK_SCHED_06}
 *    1. Go to the label `same_partition`. \llrref{LLR_SCHED_POK_SCHED_05}
 * 10. Call `inter_partition_switch()` with the argument `new_partition`.
 *     \llrref{LLR_SCHED_POK_SCHED_06}
 * 11. Set `now` to `jet_system_time()`. \llrref{LLR_SCHED_POK_SCHED_07},
 *     \llrref{LLR_SCHED_POK_SCHED_08}
 * 12. If `part->timer` is not equal to `0` and `part->timer` is less than or
 *     equal to `now`: \llrref{LLR_SCHED_POK_SCHED_07},
 *     \llrref{LLR_SCHED_POK_SCHED_08}
 *     1. Call `pok_partition_add_event()` with the arguments `part`,
 *        `JET_PARTITION_EVENT_TYPE_TIMER` and `0`.
 *        \llrref{LLR_SCHED_POK_SCHED_07}
 *     2. Set `part->timer` to `0`. \llrref{LLR_SCHED_POK_SCHED_07}
 * 13. Return. \llrref{LLR_SCHED_POK_SCHED_07}, \llrref{LLR_SCHED_POK_SCHED_08}
 * 14. Label `same_partition`: \llrref{LLR_SCHED_POK_SCHED_02},
 *     \llrref{LLR_SCHED_POK_SCHED_05}
 * 15. Call `intra_partition_switch()`. \llrref{LLR_SCHED_POK_SCHED_02},
 *     \llrref{LLR_SCHED_POK_SCHED_05}
 *
 * @calls
 * - `flag_test_and_reset()`
 * - `ja_system_time()`
 * - `inter_partition_switch()`
 * - `pok_partition_add_event()`
 * - `intra_partition_switch()`
 */
static void pok_sched(void)
{
    pok_partition_t* part = current_partition;
    pok_partition_t* new_partition;
    pok_time_t now;

    if(!flag_test_and_reset(&sched_need_recheck)) {
        return;
    }

    now = jet_system_time();

    if(pok_sched_next_deadline > now) {
        goto same_partition;
    }

    pok_sched_current_slot = (pok_sched_current_slot + 1U);
    if(pok_sched_current_slot == pok_module_sched_n)
    {
        pok_sched_next_major_frame += pok_config_scheduling_major_frame;
        pok_sched_current_slot = 0;

        /********Added code for qemu trace ************/
#if QEMU_TRACING
        ++nb_major_time_frame;
        if(started && !ended && (nb_major_time_frame >= 10))
        {
            __asm__ __volatile__(".byte 0x0F, 0xA7, 0x00, 0x00\n");
            ended = 1;
        }
        else if(!started)
        {
            __asm__ __volatile__(".byte 0x0F, 0xA6, 0x00, 0x00\n");
            started = 1;
        }
#endif /* QEMU_TRACING */
        /*********************************************/
    }
    pok_sched_next_deadline += pok_module_sched[pok_sched_current_slot].duration;

    new_partition = pok_module_sched[pok_sched_current_slot].partition;

    if(new_partition == part) {
        goto same_partition;
    }

    inter_partition_switch(new_partition);

    /*
     * If original partition's timer expires during other partition work,
     * deliver timer event now.
     */
    now = jet_system_time();
    if((part->timer != 0) && (part->timer <= now))
    {
        pok_partition_add_event(part, JET_PARTITION_EVENT_TYPE_TIMER, 0);
        part->timer = 0;
    }

    return;

same_partition:
    intra_partition_switch();
}

/**
 * @funcname `pok_preemption_disable`
 *
 * @description
 * Disables preemption (interrupts) on the CPU.
 *
 * @globals
 * None.
 *
 * @locals
 * None.
 *
 * @hlr
 * - \hlrid{HLR_SCHED_POK_PREEMPTION_DISABLE_01}: `pok_preemption_disable()`
 *   shall disable preemption (the interrupts of the processor), which must be
 *   enabled.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_INT_002}
 *
 * @llr
 * - \llrid{LLR_SCHED_POK_PREEMPTION_DISABLE_01}: `pok_preemption_disable()`
 *   shall call `ja_preempt_disable()` once.
 *   <br>Traces to:
 *   - \hlrref{HLR_SCHED_POK_PREEMPTION_DISABLE_01}
 *
 * @design
 * 1. Call `ja_preempt_disable()`. \llrref{LLR_SCHED_POK_PREEMPTION_DISABLE_01}
 *
 * @calls
 * - `ja_preempt_disable()`
 */
void pok_preemption_disable(void)
{
    assert(ja_preempt_enabled());

    ja_preempt_disable();
}


/**
 * @funcname `pok_preemption_enable`
 *
 * @description
 * Enables preemption: runs the module scheduler first, then lets the current
 * partition handle its events while it has events and doesn't disable its local
 * preemption.
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
 * - \hlrid{HLR_SCHED_POK_PREEMPTION_ENABLE_01}: `pok_preemption_enable()` shall
 *   enable preemption, which must be disabled, after it has run the scheduler
 *   of the module.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_INT_002}
 *   - \srqref{SRQ_JET_SCHED_003}
 * - \hlrid{HLR_SCHED_POK_PREEMPTION_ENABLE_02}: `pok_preemption_enable()` shall
 *   let the current partition handle its events, with preemption enabled, while
 *   it has events and does not disable its local preemption.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_SCHED_005}
 *
 * @llr
 * - \llrid{LLR_SCHED_POK_PREEMPTION_ENABLE_01}: `pok_preemption_enable()` shall
 *   first call `pok_sched()` once.
 *   <br>Traces to:
 *   - \hlrref{HLR_SCHED_POK_PREEMPTION_ENABLE_01}
 * - \llrid{LLR_SCHED_POK_PREEMPTION_ENABLE_02}: If `preempt_local_disabled` of
 *   `current_partition` is true or its `is_event` is false,
 *   `pok_preemption_enable()` shall call `ja_preempt_enable()` and return
 *   without calling `on_event`.
 *   <br>Traces to:
 *   - \hlrref{HLR_SCHED_POK_PREEMPTION_ENABLE_01}
 * - \llrid{LLR_SCHED_POK_PREEMPTION_ENABLE_03}: If `preempt_local_disabled` of
 *   `current_partition` is false and its `is_event` is true,
 *   `pok_preemption_enable()` shall set `preempt_local_disabled` to `TRUE`,
 *   call `ja_preempt_enable()`, the function `on_event` of `part_sched_ops` of
 *   the partition and `ja_preempt_disable()` again and again while
 *   `preempt_local_disabled` and `is_event` stay true, then set
 *   `preempt_local_disabled` to `FALSE` and call `ja_preempt_enable()`.
 *   <br>Traces to:
 *   - \hlrref{HLR_SCHED_POK_PREEMPTION_ENABLE_02}
 *
 * @design
 * 1. Call `pok_sched()`. \llrref{LLR_SCHED_POK_PREEMPTION_ENABLE_01}
 * 2. If `current_partition->preempt_local_disabled` is true or
 *    `current_partition->is_event` is false:
 *    \llrref{LLR_SCHED_POK_PREEMPTION_ENABLE_02},
 *    \llrref{LLR_SCHED_POK_PREEMPTION_ENABLE_03}
 *    1. Call `ja_preempt_enable()`. \llrref{LLR_SCHED_POK_PREEMPTION_ENABLE_02}
 *    2. Return. \llrref{LLR_SCHED_POK_PREEMPTION_ENABLE_02}
 * 3. Set `current_partition->preempt_local_disabled` to `TRUE`.
 *    \llrref{LLR_SCHED_POK_PREEMPTION_ENABLE_03}
 * 4. Repeat the following while `current_partition->preempt_local_disabled` is
 *    true and `current_partition->is_event` is true (at least once):
 *    \llrref{LLR_SCHED_POK_PREEMPTION_ENABLE_03}
 *    1. Call `ja_preempt_enable()`.
 *    2. Call the function `current_partition->part_sched_ops->on_event`.
 *    3. Call `ja_preempt_disable()`.
 * 5. Set `current_partition->preempt_local_disabled` to `FALSE`.
 *    \llrref{LLR_SCHED_POK_PREEMPTION_ENABLE_03}
 * 6. Call `ja_preempt_enable()`. \llrref{LLR_SCHED_POK_PREEMPTION_ENABLE_03}
 *
 * @calls
 * - `pok_sched()`
 * - `ja_preempt_enable()`
 * - `current_partition->part_sched_ops->on_event()` (indirect)
 * - `ja_preempt_disable()`
 */
void pok_preemption_enable(void)
{
    assert(!ja_preempt_enabled());

    pok_sched();
    if(current_partition->preempt_local_disabled
        || !current_partition->is_event)
    {
        // Partition doesn't require notifications. Common case.
        ja_preempt_enable();
        return;
    }

    current_partition->preempt_local_disabled = TRUE;

    // Until partition "consume" all state bits or enables preemption.
    do
    {
        ja_preempt_enable();

        current_partition->part_sched_ops->on_event();

        ja_preempt_disable();
    } while(current_partition->preempt_local_disabled
        && current_partition->is_event);

    current_partition->preempt_local_disabled = FALSE;
    ja_preempt_enable();
}

/**
 * @funcname `__pok_preemption_enable`
 *
 * @description
 * Enables preemption without running the scheduler.
 *
 * @globals
 * None.
 *
 * @locals
 * None.
 *
 * @hlr
 * - \hlrid{HLR_SCHED___POK_PREEMPTION_ENABLE_01}: `__pok_preemption_enable()`
 *   shall enable preemption, which must be disabled, without running the
 *   scheduler.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_INT_002}
 *
 * @llr
 * - \llrid{LLR_SCHED___POK_PREEMPTION_ENABLE_01}: `__pok_preemption_enable()`
 *   shall call `ja_preempt_enable()` once.
 *   <br>Traces to:
 *   - \hlrref{HLR_SCHED___POK_PREEMPTION_ENABLE_01}
 *
 * @design
 * 1. Call `ja_preempt_enable()`. \llrref{LLR_SCHED___POK_PREEMPTION_ENABLE_01}
 *
 * @calls
 * - `ja_preempt_enable()`
 */
void __pok_preemption_enable(void)
{
    assert(!ja_preempt_enabled());
    ja_preempt_enable();
}


/**
 * @funcname `get_next_periodic_processing_start`
 *
 * @description
 * Returns the start of the next slot of the current partition that starts its
 * periodic processing (ARINC 653 periodic wait), after the current slot.
 *
 * @return The start time of that slot.
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `current_partition` | `pok_partition_t *` | The running partition |
 * | `pok_module_sched` | `const pok_sched_slot_t[]` | The module schedule: the slots (generated) |
 * | `pok_module_sched_n` | `const uint8_t` | The number of slots of the schedule |
 * | `pok_sched_current_slot` | `uint8_t` | The slot that runs |
 * | `pok_sched_next_deadline` | `pok_time_t` | The end of the current slot |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `i` | `uint32_t` | The number of slots looked at |
 * | `offset` | `pok_time_t` | The start of the slot looked at |
 * | `slot` | `const pok_sched_slot_t *` | The slot looked at |
 * | `time_slot_index` | `uint32_t` | The index of the slot looked at |
 *
 * @hlr
 * - \hlrid{HLR_SCHED_GET_NEXT_PERIODIC_PROCESSING_START_01}:
 *   `get_next_periodic_processing_start()` shall give the time at which the
 *   next slot of the current partition that is marked as starting periodic
 *   processing begins, after the current slot.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_SCHED_006}
 *   - \srqref{SRQ_JET_CFG_003}
 *
 * @llr
 * - \llrid{LLR_SCHED_GET_NEXT_PERIODIC_PROCESSING_START_01}:
 *   `get_next_periodic_processing_start()` shall look at the slots of
 *   `pok_module_sched` after the slot `pok_sched_current_slot`, at most
 *   `pok_module_sched_n` of them, adding to `pok_sched_next_deadline` the
 *   `duration` of each slot that it passes.
 *   <br>Traces to:
 *   - \hlrref{HLR_SCHED_GET_NEXT_PERIODIC_PROCESSING_START_01}
 * - \llrid{LLR_SCHED_GET_NEXT_PERIODIC_PROCESSING_START_02}: For the first such
 *   slot whose `periodic_processing_start` is true and whose `partition` is
 *   `current_partition`, `get_next_periodic_processing_start()` shall return
 *   the sum reached before that slot.
 *   <br>Traces to:
 *   - \hlrref{HLR_SCHED_GET_NEXT_PERIODIC_PROCESSING_START_01}
 * - \llrid{LLR_SCHED_GET_NEXT_PERIODIC_PROCESSING_START_03}: If no such slot is
 *   found, `get_next_periodic_processing_start()` shall call
 *   `pok_assertion_fail()` (`unreachable()`).
 *   <br>Traces to:
 *   - \hlrref{HLR_SCHED_GET_NEXT_PERIODIC_PROCESSING_START_01}
 * - \llrid{LLR_SCHED_GET_NEXT_PERIODIC_PROCESSING_START_04}: If and only if the
 *   index of the next slot is `pok_module_sched_n`,
 *   `get_next_periodic_processing_start()` shall go on with slot 0.
 *   <br>Traces to:
 *   - \hlrref{HLR_SCHED_GET_NEXT_PERIODIC_PROCESSING_START_01}
 *
 * @design
 * 1. Set `offset` to `pok_sched_next_deadline`.
 *    \llrref{LLR_SCHED_GET_NEXT_PERIODIC_PROCESSING_START_01}
 * 2. Set `time_slot_index` to `pok_sched_current_slot`.
 *    \llrref{LLR_SCHED_GET_NEXT_PERIODIC_PROCESSING_START_01}
 * 3. Set `i` to `0U`; then while `i` is less than `pok_module_sched_n`, repeat
 *    (after each pass: increment `i`):
 *    \llrref{LLR_SCHED_GET_NEXT_PERIODIC_PROCESSING_START_01},
 *    \llrref{LLR_SCHED_GET_NEXT_PERIODIC_PROCESSING_START_02},
 *    \llrref{LLR_SCHED_GET_NEXT_PERIODIC_PROCESSING_START_03}
 *    1. Increment `time_slot_index`.
 *    2. If `time_slot_index` is equal to `pok_module_sched_n`:
 *       \llrref{LLR_SCHED_GET_NEXT_PERIODIC_PROCESSING_START_04}
 *       1. Set `time_slot_index` to `0U`.
 *    3. Set `slot` to the address of `pok_module_sched[time_slot_index]`.
 *    4. If `slot->periodic_processing_start` is true and `slot->partition` is
 *       equal to `current_partition`:
 *       \llrref{LLR_SCHED_GET_NEXT_PERIODIC_PROCESSING_START_02}
 *       1. Return `offset`.
 *    5. Add `slot->duration` to `offset`.
 * 4. Use the macro `unreachable()`.
 *    \llrref{LLR_SCHED_GET_NEXT_PERIODIC_PROCESSING_START_03}
 *
 * @calls
 * - `pok_assertion_fail()`
 */
pok_time_t get_next_periodic_processing_start(void)
{
    uint32_t i;

    pok_time_t offset = pok_sched_next_deadline;

    // check all time slots
    // note that we ignore current activation of _this_ slot
    // e.g. if we're currently in periodic processing window,
    // and it's the only one in schedule, we say that next one
    // will be major frame time units later
    uint32_t time_slot_index = pok_sched_current_slot;

    for (i = 0U; i < pok_module_sched_n; i++) {

        time_slot_index++;
        if(time_slot_index == pok_module_sched_n) {
            time_slot_index = 0U;
        }

        const pok_sched_slot_t *slot = &pok_module_sched[time_slot_index];

        if (slot->periodic_processing_start
            && (slot->partition == current_partition)) {
            return offset;
        }

        offset += slot->duration;
    }

    // Couldn't find the next periodic processing window (the configurator
    // shouldn't have allowed that).
    unreachable();
}

/**
 * @funcname `pok_sched_on_time_changed`
 *
 * @description
 * Called at each timer tick (preemption disabled): runs the module scheduler,
 * delivers the partition's timer event if it expired, and lets the partition
 * handle its events if its local preemption is enabled. With GDB, records the
 * user context of the interrupted partition.
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `current_partition` | `pok_partition_t *` | The running partition |
 * | `current_partition_is_paused` | `pok_bool_t` | Whether the context that runs is the idle one of a paused partition |
 * | `global_thread_stack` | `interrupt_frame *` | The interrupted context at the last trap (GDB) |
 * | `pok_in_user_space` | `volatile pok_bool_t` | Whether the CPU runs user code (GDB) |
 * | `sched_need_recheck` | `pok_bool_t` | Whether the scheduler must look at the time |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `current_time` | `pok_time_t` | The current time |
 * | `in_user_space` | `pok_bool_t` | Whether the tick interrupted user code (GDB) |
 * | `part` | `pok_partition_t *` | The partition that runs |
 * | `preempt_local_disabled_old` | `pok_bool_t` | Whether the partition's local preemption was disabled |
 *
 * @hlr
 * - \hlrid{HLR_SCHED_POK_SCHED_ON_TIME_CHANGED_01}:
 *   `pok_sched_on_time_changed()` shall make the scheduler of the module
 *   evaluate the schedule again at each tick of the timer.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_TIME_004}
 *   - \srqref{SRQ_JET_SCHED_003}
 * - \hlrid{HLR_SCHED_POK_SCHED_ON_TIME_CHANGED_02}:
 *   `pok_sched_on_time_changed()` shall give the current partition the event of
 *   its timer when its time has come, and let it handle its events if its local
 *   preemption is enabled.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_SCHED_005}
 * - \hlrid{HLR_SCHED_POK_SCHED_ON_TIME_CHANGED_03}:
 *   `pok_sched_on_time_changed()` shall let the GDB server see the user context
 *   of the interrupted partition while the tick is handled.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_DBG_004}
 *
 * @llr
 * - \llrid{LLR_SCHED_POK_SCHED_ON_TIME_CHANGED_01}:
 *   `pok_sched_on_time_changed()` shall set `sched_need_recheck` to `TRUE` and
 *   call `pok_sched()` once.
 *   <br>Traces to:
 *   - \hlrref{HLR_SCHED_POK_SCHED_ON_TIME_CHANGED_01}
 * - \llrid{LLR_SCHED_POK_SCHED_ON_TIME_CHANGED_02}: With `POK_NEEDS_GDB`, if
 *   and only if `pok_in_user_space` is true at entry,
 *   `pok_sched_on_time_changed()` shall set `entry_sp_user` of
 *   `current_partition` to `global_thread_stack` and `pok_in_user_space` to
 *   `FALSE` before it calls `pok_sched()`.
 *   <br>Traces to:
 *   - \hlrref{HLR_SCHED_POK_SCHED_ON_TIME_CHANGED_03}
 * - \llrid{LLR_SCHED_POK_SCHED_ON_TIME_CHANGED_03}: With `POK_NEEDS_GDB`,
 *   `pok_sched_on_time_changed()` shall set `pok_in_user_space` back to its
 *   value at entry before it returns.
 *   <br>Traces to:
 *   - \hlrref{HLR_SCHED_POK_SCHED_ON_TIME_CHANGED_03}
 * - \llrid{LLR_SCHED_POK_SCHED_ON_TIME_CHANGED_04}: If
 *   `current_partition_is_paused` is true after `pok_sched()`,
 *   `pok_sched_on_time_changed()` shall return without handling the timer or
 *   the events.
 *   <br>Traces to:
 *   - \hlrref{HLR_SCHED_POK_SCHED_ON_TIME_CHANGED_02}
 * - \llrid{LLR_SCHED_POK_SCHED_ON_TIME_CHANGED_05}: If it is false, and `timer`
 *   of the partition current at entry is not 0 and not greater than the result
 *   of `jet_system_time()`, `pok_sched_on_time_changed()` shall call
 *   `pok_partition_add_event()` with that partition,
 *   `JET_PARTITION_EVENT_TYPE_TIMER` and 0, and set its `timer` to 0.
 *   <br>Traces to:
 *   - \hlrref{HLR_SCHED_POK_SCHED_ON_TIME_CHANGED_02}
 * - \llrid{LLR_SCHED_POK_SCHED_ON_TIME_CHANGED_06}: If it is false, and that
 *   `timer` is 0 or greater than the result of `jet_system_time()`,
 *   `pok_sched_on_time_changed()` shall leave the timer as it is and add no
 *   event.
 *   <br>Traces to:
 *   - \hlrref{HLR_SCHED_POK_SCHED_ON_TIME_CHANGED_02}
 * - \llrid{LLR_SCHED_POK_SCHED_ON_TIME_CHANGED_07}: If it is false, and
 *   `preempt_local_disabled` of `current_partition` (after `pok_sched()`) is
 *   true or `is_event` of the partition is false, `pok_sched_on_time_changed()`
 *   shall return without calling `on_event`.
 *   <br>Traces to:
 *   - \hlrref{HLR_SCHED_POK_SCHED_ON_TIME_CHANGED_02}
 * - \llrid{LLR_SCHED_POK_SCHED_ON_TIME_CHANGED_08}: If it is false,
 *   `preempt_local_disabled` is false and `is_event` is true,
 *   `pok_sched_on_time_changed()` shall set `preempt_local_disabled` to `TRUE`,
 *   call `ja_preempt_enable()`, the function `on_event` of `part_sched_ops` of
 *   the partition and `ja_preempt_disable()` again and again while
 *   `preempt_local_disabled` and `is_event` stay true, and then set
 *   `preempt_local_disabled` to `FALSE`.
 *   <br>Traces to:
 *   - \hlrref{HLR_SCHED_POK_SCHED_ON_TIME_CHANGED_02}
 *
 * @design
 * 1. Set `part` to `current_partition`.
 *    \llrref{LLR_SCHED_POK_SCHED_ON_TIME_CHANGED_05},
 *    \llrref{LLR_SCHED_POK_SCHED_ON_TIME_CHANGED_06},
 *    \llrref{LLR_SCHED_POK_SCHED_ON_TIME_CHANGED_07},
 *    \llrref{LLR_SCHED_POK_SCHED_ON_TIME_CHANGED_08}
 * 2. Set `sched_need_recheck` to `TRUE`.
 *    \llrref{LLR_SCHED_POK_SCHED_ON_TIME_CHANGED_01}
 * 3. Set `in_user_space` to `pok_in_user_space`.
 *    \llrref{LLR_SCHED_POK_SCHED_ON_TIME_CHANGED_02},
 *    \llrref{LLR_SCHED_POK_SCHED_ON_TIME_CHANGED_03}
 * 4. If `in_user_space` is true:
 *    \llrref{LLR_SCHED_POK_SCHED_ON_TIME_CHANGED_02}
 *    1. Set `part->entry_sp_user` to `global_thread_stack`.
 *    2. Set `pok_in_user_space` to `FALSE`.
 * 5. Call `pok_sched()`. \llrref{LLR_SCHED_POK_SCHED_ON_TIME_CHANGED_01}
 * 6. If `current_partition_is_paused` is true:
 *    \llrref{LLR_SCHED_POK_SCHED_ON_TIME_CHANGED_04}
 *    1. Go to the label `out`.
 * 7. Set `preempt_local_disabled_old` to
 *    `current_partition->preempt_local_disabled`.
 *    \llrref{LLR_SCHED_POK_SCHED_ON_TIME_CHANGED_07},
 *    \llrref{LLR_SCHED_POK_SCHED_ON_TIME_CHANGED_08}
 * 8. Set `current_time` to `jet_system_time()`.
 *    \llrref{LLR_SCHED_POK_SCHED_ON_TIME_CHANGED_05},
 *    \llrref{LLR_SCHED_POK_SCHED_ON_TIME_CHANGED_06}
 * 9. If `part->timer` is not equal to `0` and `part->timer` is less than or
 *    equal to `current_time`: \llrref{LLR_SCHED_POK_SCHED_ON_TIME_CHANGED_05},
 *    \llrref{LLR_SCHED_POK_SCHED_ON_TIME_CHANGED_06}
 *    1. Call `pok_partition_add_event()` with the arguments `part`,
 *       `JET_PARTITION_EVENT_TYPE_TIMER` and `0`.
 *       \llrref{LLR_SCHED_POK_SCHED_ON_TIME_CHANGED_05}
 *    2. Set `part->timer` to `0`.
 *       \llrref{LLR_SCHED_POK_SCHED_ON_TIME_CHANGED_05}
 * 10. If `preempt_local_disabled_old` is true or `part->is_event` is false:
 *     \llrref{LLR_SCHED_POK_SCHED_ON_TIME_CHANGED_07},
 *     \llrref{LLR_SCHED_POK_SCHED_ON_TIME_CHANGED_08}
 *     1. Go to the label `out`. \llrref{LLR_SCHED_POK_SCHED_ON_TIME_CHANGED_07}
 * 11. Repeat the following while `part->preempt_local_disabled` is true and
 *     `part->is_event` is true (at least once):
 *     \llrref{LLR_SCHED_POK_SCHED_ON_TIME_CHANGED_08}
 *     1. Set `part->preempt_local_disabled` to `TRUE`.
 *     2. Call `ja_preempt_enable()`.
 *     3. Call the function `part->part_sched_ops->on_event`.
 *     4. Call `ja_preempt_disable()`.
 * 12. Set `part->preempt_local_disabled` to `FALSE`.
 *     \llrref{LLR_SCHED_POK_SCHED_ON_TIME_CHANGED_08}
 * 13. Label `out`: \llrref{LLR_SCHED_POK_SCHED_ON_TIME_CHANGED_03}
 * 14. Set `pok_in_user_space` to `in_user_space`.
 *     \llrref{LLR_SCHED_POK_SCHED_ON_TIME_CHANGED_03}
 *
 * @calls
 * - `pok_sched()`
 * - `ja_system_time()`
 * - `pok_partition_add_event()`
 * - `ja_preempt_enable()`
 * - `part->part_sched_ops->on_event()` (indirect)
 * - `ja_preempt_disable()`
 */
void pok_sched_on_time_changed(void)
{
    assert(!ja_preempt_enabled());

    pok_partition_t* part = current_partition;
    sched_need_recheck = TRUE;

#ifdef POK_NEEDS_GDB
    pok_bool_t in_user_space = pok_in_user_space;

    if(in_user_space)
    {
        part->entry_sp_user = global_thread_stack;
        pok_in_user_space = FALSE;
    }
#endif /* POK_NEEDS_GDB */
    pok_sched();

    if(current_partition_is_paused) {
        goto out;
    }

    pok_bool_t preempt_local_disabled_old = current_partition->preempt_local_disabled;

    pok_time_t current_time = jet_system_time();

    if((part->timer != 0) && (part->timer <= current_time))
    {
        pok_partition_add_event(part, JET_PARTITION_EVENT_TYPE_TIMER, 0);
        part->timer = 0;
    }

    if(preempt_local_disabled_old || !part->is_event) {
        goto out;
    }
    // Emit events for partition.
    do
    {
        part->preempt_local_disabled = TRUE;
        ja_preempt_enable();
        part->part_sched_ops->on_event();
        ja_preempt_disable();
    } while(part->preempt_local_disabled && part->is_event);

    part->preempt_local_disabled = FALSE;

    // Still with disabled preemption. It is needed for returning from interrupt.

out:
#ifdef POK_NEEDS_GDB
    // Restore user space indicator on return
    pok_in_user_space = in_user_space;
#else
    return;
#endif
}

/**
 * @funcname `pok_partition_return_user_common`
 *
 * @description
 * Prepares the return to the current partition's user code: disables preemption
 * and lets the partition handle its pending events.
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `current_partition` | `pok_partition_t *` | The running partition |
 * | `pok_in_user_space` | `volatile pok_bool_t` | Whether the CPU runs user code (GDB) |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `part` | `pok_partition_t *` | The current partition |
 *
 * @hlr
 * - \hlrid{HLR_SCHED_POK_PARTITION_RETURN_USER_COMMON_01}:
 *   `pok_partition_return_user_common()` shall prepare the return of the
 *   processor to the user code of the current partition: preemption disabled
 *   and the partition's pending events handled.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_SYS_007}
 *
 * @llr
 * - \llrid{LLR_SCHED_POK_PARTITION_RETURN_USER_COMMON_01}:
 *   `pok_partition_return_user_common()` shall call `pok_preemption_disable()`
 *   once, before it reads `preempt_local_disabled` and `is_event` of
 *   `current_partition`.
 *   <br>Traces to:
 *   - \hlrref{HLR_SCHED_POK_PARTITION_RETURN_USER_COMMON_01}
 * - \llrid{LLR_SCHED_POK_PARTITION_RETURN_USER_COMMON_02}: While
 *   `preempt_local_disabled` and `is_event` of `current_partition` are true,
 *   `pok_partition_return_user_common()` shall set `preempt_local_disabled` to
 *   `TRUE` and call `ja_preempt_enable()`, the function `on_event` of
 *   `part_sched_ops` of the partition and `ja_preempt_disable()`, in this
 *   order.
 *   <br>Traces to:
 *   - \hlrref{HLR_SCHED_POK_PARTITION_RETURN_USER_COMMON_01}
 * - \llrid{LLR_SCHED_POK_PARTITION_RETURN_USER_COMMON_03}: When
 *   `preempt_local_disabled` or `is_event` of `current_partition` is false,
 *   `pok_partition_return_user_common()` shall set `preempt_local_disabled` to
 *   `FALSE` and, with `POK_NEEDS_GDB`, `pok_in_user_space` to `TRUE`.
 *   <br>Traces to:
 *   - \hlrref{HLR_SCHED_POK_PARTITION_RETURN_USER_COMMON_01}
 *
 * @design
 * 1. Set `part` to `current_partition`.
 *    \llrref{LLR_SCHED_POK_PARTITION_RETURN_USER_COMMON_01}
 * 2. Call `pok_preemption_disable()`.
 *    \llrref{LLR_SCHED_POK_PARTITION_RETURN_USER_COMMON_01}
 * 3. While `part->preempt_local_disabled` is true and `part->is_event` is true:
 *    \llrref{LLR_SCHED_POK_PARTITION_RETURN_USER_COMMON_02}
 *    1. Set `part->preempt_local_disabled` to `TRUE`.
 *    2. Call `ja_preempt_enable()`.
 *    3. Call the function `part->part_sched_ops->on_event`.
 *    4. Call `ja_preempt_disable()`.
 * 4. Set `part->preempt_local_disabled` to `FALSE`.
 *    \llrref{LLR_SCHED_POK_PARTITION_RETURN_USER_COMMON_03}
 * 5. Set `pok_in_user_space` to `TRUE`.
 *    \llrref{LLR_SCHED_POK_PARTITION_RETURN_USER_COMMON_03}
 *
 * @calls
 * - `pok_preemption_disable()`
 * - `ja_preempt_enable()`
 * - `part->part_sched_ops->on_event()` (indirect)
 * - `ja_preempt_disable()`
 */
static void pok_partition_return_user_common(void)
{
    pok_partition_t* part = current_partition;

    pok_preemption_disable();

    // Emit events for partition.
    while(part->preempt_local_disabled
        && part->is_event)
    {
        part->preempt_local_disabled = TRUE;
        ja_preempt_enable();
        part->part_sched_ops->on_event();
        ja_preempt_disable();
    }

    part->preempt_local_disabled = FALSE;
#ifdef POK_NEEDS_GDB
    pok_in_user_space = TRUE;
#endif
}

/**
 * @funcname `pok_partition_return_user`
 *
 * @description
 * Prepares the return to user code (syscall return): handles the partition's
 * events and makes the FP state of the thread current.
 *
 * @globals
 * None.
 *
 * @locals
 * None.
 *
 * @hlr
 * - \hlrid{HLR_SCHED_POK_PARTITION_RETURN_USER_01}:
 *   `pok_partition_return_user()` shall prepare the return from a system call
 *   to the partition: its events handled and the floating point state of the
 *   returning process in the processor.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_SYS_007}
 *   - \srqref{SRQ_JET_THR_023}
 *   - \srqref{SRQ_JET_THR_024}
 *
 * @llr
 * - \llrid{LLR_SCHED_POK_PARTITION_RETURN_USER_01}:
 *   `pok_partition_return_user()` shall call
 *   `pok_partition_return_user_common()` and then
 *   `pok_partition_return_user_fp()`.
 *   <br>Traces to:
 *   - \hlrref{HLR_SCHED_POK_PARTITION_RETURN_USER_01}
 *
 * @design
 * 1. Call `pok_partition_return_user_common()`.
 *    \llrref{LLR_SCHED_POK_PARTITION_RETURN_USER_01}
 * 2. Call `pok_partition_return_user_fp()`.
 *    \llrref{LLR_SCHED_POK_PARTITION_RETURN_USER_01}
 *
 * @calls
 * - `pok_partition_return_user_common()`
 * - `pok_partition_return_user_fp()`
 */
void pok_partition_return_user(void)
{
    pok_partition_return_user_common();
    pok_partition_return_user_fp();
}

/**
 * @funcname `pok_partition_return_user_fp`
 *
 * @description
 * Makes the current partition's FP store the CPU's FP state: saves the FP state
 * of the thread that used it last and loads the new one, if they differ.
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `current_partition` | `pok_partition_t *` | The running partition |
 * | `fp_store_last` | `jet_fp_store_t *` | The FP store whose state the CPU holds, or NULL |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `part` | `pok_partition_t *` | The current partition |
 *
 * @hlr
 * - \hlrid{HLR_SCHED_POK_PARTITION_RETURN_USER_FP_01}:
 *   `pok_partition_return_user_fp()` shall put the floating point state of the
 *   current process of the partition into the processor, saving the state of
 *   the process that used it last.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_THR_023}
 *   - \srqref{SRQ_JET_THR_024}
 * - \hlrid{HLR_SCHED_POK_PARTITION_RETURN_USER_FP_02}:
 *   `pok_partition_return_user_fp()` shall change the floating point state of
 *   the processor only for the processes of the partitions, since the kernel
 *   itself does not use the floating point unit.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_GEN_002}
 *
 * @llr
 * - \llrid{LLR_SCHED_POK_PARTITION_RETURN_USER_FP_01}: If `fp_store_last` is
 *   not `fp_store_current` of `current_partition` and is not NULL,
 *   `pok_partition_return_user_fp()` shall call `ja_fp_save()` with
 *   `fp_store_last`, then set `fp_store_last` to `fp_store_current` and call
 *   `ja_fp_restore()` with it.
 *   <br>Traces to:
 *   - \hlrref{HLR_SCHED_POK_PARTITION_RETURN_USER_FP_01}
 * - \llrid{LLR_SCHED_POK_PARTITION_RETURN_USER_FP_02}: If `fp_store_last` is
 *   not `fp_store_current` and is NULL, `pok_partition_return_user_fp()` shall
 *   set `fp_store_last` to `fp_store_current` and call `ja_fp_restore()` with
 *   it, without calling `ja_fp_save()`.
 *   <br>Traces to:
 *   - \hlrref{HLR_SCHED_POK_PARTITION_RETURN_USER_FP_01}
 * - \llrid{LLR_SCHED_POK_PARTITION_RETURN_USER_FP_03}: If `fp_store_last` is
 *   `fp_store_current` of `current_partition`, `pok_partition_return_user_fp()`
 *   shall call neither `ja_fp_save()` nor `ja_fp_restore()`.
 *   <br>Traces to:
 *   - \hlrref{HLR_SCHED_POK_PARTITION_RETURN_USER_FP_02}
 *
 * @design
 * 1. Set `part` to `current_partition`.
 *    \llrref{LLR_SCHED_POK_PARTITION_RETURN_USER_FP_01},
 *    \llrref{LLR_SCHED_POK_PARTITION_RETURN_USER_FP_02},
 *    \llrref{LLR_SCHED_POK_PARTITION_RETURN_USER_FP_03}
 * 2. If `fp_store_last` is not equal to `part->fp_store_current`:
 *    \llrref{LLR_SCHED_POK_PARTITION_RETURN_USER_FP_01},
 *    \llrref{LLR_SCHED_POK_PARTITION_RETURN_USER_FP_02},
 *    \llrref{LLR_SCHED_POK_PARTITION_RETURN_USER_FP_03}
 *    1. If `fp_store_last` is not NULL:
 *       \llrref{LLR_SCHED_POK_PARTITION_RETURN_USER_FP_01}
 *       1. Call `ja_fp_save()` with the argument `fp_store_last`.
 *    2. Set `fp_store_last` to `part->fp_store_current`.
 *       \llrref{LLR_SCHED_POK_PARTITION_RETURN_USER_FP_01},
 *       \llrref{LLR_SCHED_POK_PARTITION_RETURN_USER_FP_02}
 *    3. Call `ja_fp_restore()` with the argument `fp_store_last`.
 *       \llrref{LLR_SCHED_POK_PARTITION_RETURN_USER_FP_01},
 *       \llrref{LLR_SCHED_POK_PARTITION_RETURN_USER_FP_02}
 *
 * @calls
 * - `ja_fp_save()`
 * - `ja_fp_restore()`
 */
void pok_partition_return_user_fp(void)
{
    pok_partition_t* part = current_partition;

    assert(part->fp_store_current != NULL);

    if(fp_store_last != part->fp_store_current)
    {
        if(fp_store_last != NULL)
        {
            ja_fp_save(fp_store_last);
        }

        fp_store_last = part->fp_store_current;
        ja_fp_restore(fp_store_last);
    }
}

#ifdef POK_NEEDS_GDB
/**
 * @funcname `jet_fp_sync`
 *
 * @description
 * Saves the CPU's FP state into the FP store it belongs to (so that GDB reads
 * it).
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `fp_store_last` | `jet_fp_store_t *` | The FP store whose state the CPU holds, or NULL |
 *
 * @locals
 * None.
 *
 * @hlr
 * - \hlrid{HLR_SCHED_JET_FP_SYNC_01}: `jet_fp_sync()` shall save the floating
 *   point state of the processor into the store of the process that it belongs
 *   to, so that the GDB server reads it.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_DBG_004}
 *
 * @llr
 * - \llrid{LLR_SCHED_JET_FP_SYNC_01}: If and only if `fp_store_last` is not
 *   NULL, `jet_fp_sync()` shall call `ja_fp_save()` once with it.
 *   <br>Traces to:
 *   - \hlrref{HLR_SCHED_JET_FP_SYNC_01}
 *
 * @design
 * 1. If `fp_store_last` is not NULL: \llrref{LLR_SCHED_JET_FP_SYNC_01}
 *    1. Call `ja_fp_save()` with the argument `fp_store_last`.
 *
 * @calls
 * - `ja_fp_save()`
 */
void jet_fp_sync(void)
{
    if(fp_store_last != NULL)
    {
        ja_fp_save(fp_store_last);
    }
}

/**
 * @funcname `jet_fp_reload`
 *
 * @description
 * Loads the FP state of the current FP store into the CPU again (after GDB
 * changed it).
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `fp_store_last` | `jet_fp_store_t *` | The FP store whose state the CPU holds, or NULL |
 *
 * @locals
 * None.
 *
 * @hlr
 * - \hlrid{HLR_SCHED_JET_FP_RELOAD_01}: `jet_fp_reload()` shall load the
 *   floating point state of the processor again from its store, after the GDB
 *   server changed it.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_DBG_004}
 *
 * @llr
 * - \llrid{LLR_SCHED_JET_FP_RELOAD_01}: If and only if `fp_store_last` is not
 *   NULL, `jet_fp_reload()` shall call `ja_fp_restore()` once with it.
 *   <br>Traces to:
 *   - \hlrref{HLR_SCHED_JET_FP_RELOAD_01}
 *
 * @design
 * 1. If `fp_store_last` is not NULL: \llrref{LLR_SCHED_JET_FP_RELOAD_01}
 *    1. Call `ja_fp_restore()` with the argument `fp_store_last`.
 *
 * @calls
 * - `ja_fp_restore()`
 */
void jet_fp_reload(void)
{
    if(fp_store_last != NULL)
    {
        ja_fp_restore(fp_store_last);
    }
}
#endif /* POK_NEEDS_GDB */

/**
 * @funcname `pok_partition_jump_user`
 *
 * @description
 * Starts a thread in user mode: handles the partition's events, saves the FP
 * state of the last thread, gives the new thread a clean FP state and jumps to
 * its entry.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `entry` | `void (* JET_USER)(void)` | [in] The thread's entry point (user address). |
 * | `stack_user` | `jet_ustack_t` | [in] The thread's user stack. |
 * | `stack_kernel` | `jet_stack_t` | [in] The thread's kernel stack. |
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `current_partition` | `pok_partition_t *` | The running partition |
 * | `fp_store_last` | `jet_fp_store_t *` | The FP store whose state the CPU holds, or NULL |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `part` | `pok_partition_t *` | The current partition |
 *
 * @hlr
 * - \hlrid{HLR_SCHED_POK_PARTITION_JUMP_USER_01}: `pok_partition_jump_user()`
 *   shall start a process in user mode at its entry, with its stack, with a
 *   clean floating point state, after the partition's pending events are
 *   handled.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_THR_013}
 *   - \srqref{SRQ_JET_THR_024}
 *   - \srqref{SRQ_JET_PART_013}
 *
 * @llr
 * - \llrid{LLR_SCHED_POK_PARTITION_JUMP_USER_01}: `pok_partition_jump_user()`
 *   shall first call `pok_partition_return_user_common()`.
 *   <br>Traces to:
 *   - \hlrref{HLR_SCHED_POK_PARTITION_JUMP_USER_01}
 * - \llrid{LLR_SCHED_POK_PARTITION_JUMP_USER_02}: If and only if
 *   `fp_store_last` is not NULL and is not `fp_store_current` of
 *   `current_partition`, `pok_partition_jump_user()` shall then call
 *   `ja_fp_save()` with `fp_store_last`.
 *   <br>Traces to:
 *   - \hlrref{HLR_SCHED_POK_PARTITION_JUMP_USER_01}
 * - \llrid{LLR_SCHED_POK_PARTITION_JUMP_USER_03}: `pok_partition_jump_user()`
 *   shall then set `fp_store_last` to `fp_store_current`, call `ja_fp_init()`,
 *   and call `ja_user_space_jump()` with `stack_kernel`, `space_id` of the
 *   partition, `entry` and `stack_user`.
 *   <br>Traces to:
 *   - \hlrref{HLR_SCHED_POK_PARTITION_JUMP_USER_01}
 *
 * @design
 * 1. Set `part` to `current_partition`.
 *    \llrref{LLR_SCHED_POK_PARTITION_JUMP_USER_01},
 *    \llrref{LLR_SCHED_POK_PARTITION_JUMP_USER_02},
 *    \llrref{LLR_SCHED_POK_PARTITION_JUMP_USER_03}
 * 2. Call `pok_partition_return_user_common()`.
 *    \llrref{LLR_SCHED_POK_PARTITION_JUMP_USER_01}
 * 3. If `fp_store_last` is not NULL and `fp_store_last` is not equal to
 *    `part->fp_store_current`: \llrref{LLR_SCHED_POK_PARTITION_JUMP_USER_02}
 *    1. Call `ja_fp_save()` with the argument `fp_store_last`.
 * 4. Set `fp_store_last` to `part->fp_store_current`.
 *    \llrref{LLR_SCHED_POK_PARTITION_JUMP_USER_03}
 * 5. Call `ja_fp_init()`. \llrref{LLR_SCHED_POK_PARTITION_JUMP_USER_03}
 * 6. Call `ja_user_space_jump()` with the arguments `stack_kernel`,
 *    `part->space_id`, `entry` and `(uintptr_t)stack_user`.
 *    \llrref{LLR_SCHED_POK_PARTITION_JUMP_USER_03}
 *
 * @calls
 * - `pok_partition_return_user_common()`
 * - `ja_fp_save()`
 * - `ja_fp_init()`
 * - `ja_user_space_jump()`
 */
void pok_partition_jump_user(void (* JET_USER entry)(void),
    jet_ustack_t stack_user,
    jet_stack_t stack_kernel)
{
    pok_partition_t* part = current_partition;

    pok_partition_return_user_common();

    assert(part->fp_store_current != NULL);

    if((fp_store_last != NULL) && (fp_store_last != part->fp_store_current))
    {
        ja_fp_save(fp_store_last);
    }

    fp_store_last = part->fp_store_current;
    ja_fp_init();

    jet_user_space_jump(
        stack_kernel,
        part->space_id,
        entry,
        (uintptr_t)stack_user);
}

/**
 * @funcname `pok_partition_restart`
 *
 * @description
 * Restarts the current partition: it gets a new generation and no context, so
 * the scheduler starts it again at its entry. Doesn't return.
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `current_partition` | `pok_partition_t *` | The running partition |
 * | `sched_need_recheck` | `pok_bool_t` | Whether the scheduler must look at the time |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `part` | `pok_partition_t *` | The current partition |
 *
 * @hlr
 * - \hlrid{HLR_SCHED_POK_PARTITION_RESTART_01}: `pok_partition_restart()` shall
 *   restart the current partition at its entry, with a new generation, and not
 *   return.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_PART_005}
 *   - \srqref{SRQ_JET_HM_004}
 *
 * @llr
 * - \llrid{LLR_SCHED_POK_PARTITION_RESTART_01}: `pok_partition_restart()` shall
 *   call `pok_preemption_disable()`, increment `partition_generation` of
 *   `current_partition`, set its `sp` to NULL and `sched_need_recheck` to
 *   `TRUE`, call `pok_preemption_enable()`, and then, if it returns,
 *   `pok_assertion_fail()` (`unreachable()`).
 *   <br>Traces to:
 *   - \hlrref{HLR_SCHED_POK_PARTITION_RESTART_01}
 * - \llrid{LLR_SCHED_POK_PARTITION_RESTART_02}: If and only if
 *   `partition_generation` is 0 after the increment, `pok_partition_restart()`
 *   shall set it to 1.
 *   <br>Traces to:
 *   - \hlrref{HLR_SCHED_POK_PARTITION_RESTART_01}
 *
 * @design
 * 1. Set `part` to `current_partition`.
 *    \llrref{LLR_SCHED_POK_PARTITION_RESTART_01}
 * 2. Call `pok_preemption_disable()`.
 *    \llrref{LLR_SCHED_POK_PARTITION_RESTART_01}
 * 3. Increment `part->partition_generation`.
 *    \llrref{LLR_SCHED_POK_PARTITION_RESTART_01}
 * 4. If `part->partition_generation` is equal to `0U`:
 *    \llrref{LLR_SCHED_POK_PARTITION_RESTART_02}
 *    1. Set `part->partition_generation` to `1`.
 * 5. Set `part->sp` to `NULL`. \llrref{LLR_SCHED_POK_PARTITION_RESTART_01}
 * 6. Set `sched_need_recheck` to `TRUE`.
 *    \llrref{LLR_SCHED_POK_PARTITION_RESTART_01}
 * 7. Call `pok_preemption_enable()`.
 *    \llrref{LLR_SCHED_POK_PARTITION_RESTART_01}
 * 8. Use the macro `unreachable()`. \llrref{LLR_SCHED_POK_PARTITION_RESTART_01}
 *
 * @calls
 * - `pok_assertion_fail()`
 * - `pok_preemption_disable()`
 * - `pok_preemption_enable()`
 */
void pok_partition_restart(void)
{
    pok_partition_t* part = current_partition;
    assert(part != NULL);

    pok_preemption_disable();

    // Assign new generation for partition.
    part->partition_generation++;
    // For the case of overflow.
    if(part->partition_generation == 0U) {
        part->partition_generation = 1;
    }

    part->sp = NULL;
    sched_need_recheck = TRUE;

    pok_preemption_enable();

    unreachable();
}


/**
 * @funcname `pok_sched_init`
 *
 * @description
 * Initializes the scheduler at boot: the idle partition and its stack.
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `partition_idle` | `pok_partition_t` | The idle partition |
 *
 * @locals
 * None.
 *
 * @hlr
 * - \hlrid{HLR_SCHED_POK_SCHED_INIT_01}: `pok_sched_init()` shall initialize,
 *   at boot, the idle partition that runs in the slots without a partition.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_SCHED_002}
 *   - \srqref{SRQ_JET_BOOT_002}
 *   - \srqref{SRQ_JET_CFG_010}
 *
 * @llr
 * - \llrid{LLR_SCHED_POK_SCHED_INIT_01}: `pok_sched_init()` shall call
 *   `pok_partition_init()` with the address of `partition_idle` and set its
 *   `initial_sp` to the result of `pok_stack_alloc()` for 4096.
 *   <br>Traces to:
 *   - \hlrref{HLR_SCHED_POK_SCHED_INIT_01}
 *
 * @design
 * 1. Call `pok_partition_init()` with the address of `partition_idle`.
 *    \llrref{LLR_SCHED_POK_SCHED_INIT_01}
 * 2. Call `pok_stack_alloc()` with the argument `4096`, and store the result in
 *    `partition_idle.initial_sp`. \llrref{LLR_SCHED_POK_SCHED_INIT_01}
 *
 * @calls
 * - `pok_partition_init()`
 * - `pok_stack_alloc()`
 */
void pok_sched_init(void)
{
    pok_partition_init(&partition_idle);
    partition_idle.initial_sp = pok_stack_alloc(4096);
}
