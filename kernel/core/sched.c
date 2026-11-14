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
 *
 * @llr
 *
 * @design
 * 1. Call `pok_preemption_enable()`.
 * 2. Call `ja_inf_loop()`.
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
 *
 * @llr
 *
 * @design
 * 1. Set `part->sp` to `NULL`.
 * 2. Set `part->restarted_externally` to `TRUE`.
 * 3. Increment `part->partition_generation`.
 * 4. If `part->partition_generation` is equal to `0U`:
 *    1. Set `part->partition_generation` to `1`.
 * 5. Set `part->partition_event_end` to `0`.
 * 6. Set `part->partition_event_begin` to `0`.
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
 *
 * @llr
 *
 * @design
 * 1. Set `part` to `current_partition`.
 * 2. Set `part->is_event` to `FALSE`.
 * 3. Set `part->partition_event_end` to `0`.
 * 4. Set `part->partition_event_begin` to `0`.
 * 5. Set `part->preempt_local_disabled` to `TRUE`.
 * 6. Call `pok_preemption_enable()`.
 * 7. If `part->part_ops` is not NULL and `part->part_ops->start` is not NULL:
 *    1. Call the function `part->part_ops->start`.
 * 8. Call `ja_inf_loop()`.
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
 *
 * @llr
 *
 * @design
 * 1. Set `old_sp` to the address of `current_partition->sp`.
 * 2. Set `new_sp` to `old_sp`.
 * 3. If `current_partition_is_paused` is true:
 *    1. Set `old_sp` to the address of `idle_sp`.
 * 4. If `current_partition->is_paused` is true:
 *    1. Set `new_sp` to the address of `idle_sp`.
 * 5. If `old_sp` is not equal to `new_sp`:
 *    1. Set `current_partition_is_paused` to `current_partition->is_paused`.
 *    2. If the value at `old_sp` is NULL:
 *       1. Call `ja_context_jump()` with the value at `new_sp`.
 *       2. Return.
 *    3. Otherwise, if the value at `new_sp` is NULL:
 *       1. Call `ja_context_init()` with the arguments
 *          `current_partition->initial_sp` and the address of
 *          `start_partition`, and store the result in `*new_sp`.
 *    4. Otherwise:
 *       1. Do nothing.
 *    5. Call `ja_context_switch()` with the arguments `old_sp` and the value at
 *       `new_sp`.
 *    6. Return.
 * 6. If the value at `old_sp` is NULL:
 *    1. Call `ja_context_restart_and_save()` with the arguments
 *       `current_partition->initial_sp`, the address of `start_partition` and
 *       `old_sp`.
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
 *
 * @llr
 *
 * @design
 * 1. Set `old_sp` to the address of `current_partition->sp`.
 * 2. Set `new_sp` to the address of `part->sp`.
 * 3. Set `current_partition->entry_sp` to `global_thread_stack`.
 * 4. Set `current_partition` to `part`.
 * 5. If `part->space_id` is not equal to `0U`:
 *    1. Call `ja_space_switch()` with the argument `part->space_id`.
 * 6. Otherwise:
 *    1. Call `ja_space_switch()` with the argument `0`.
 * 7. If `current_partition_is_paused` is true:
 *    1. Set `old_sp` to the address of `idle_sp`.
 * 8. If `part->is_paused` is true:
 *    1. Check that `idle_sp != NULL` holds (assert: stop otherwise).
 *    2. Set `new_sp` to the address of `idle_sp`.
 * 9. If `old_sp` is equal to `new_sp`:
 *    1. Return.
 * 10. Set `current_partition_is_paused` to `part->is_paused`.
 * 11. If the value at `new_sp` is NULL:
 *     1. Call `ja_context_init()` with the arguments `part->initial_sp` and the
 *        address of `start_partition`, and store the result in `*new_sp`.
 * 12. If the value at `old_sp` is NULL:
 *     1. Call `ja_context_jump()` with the value at `new_sp`.
 * 13. Otherwise:
 *     1. Call `ja_context_switch()` with the arguments `old_sp` and the value
 *        at `new_sp`.
 *
 * @calls
 * - `ja_space_switch()`
 * - `pok_assertion_fail()`
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
 *
 * @llr
 *
 * @design
 * 1. Set `first_frame_starts` to `jet_system_time()`.
 * 2. Call `ja_context_init()` with the arguments `idle_stack` and the address
 *    of `idle_function`, and store the result in `idle_sp`.
 * 3. Call `for_each_partition()` with the address of `pok_partition_reset`.
 * 4. Set `sched_need_recheck` to `FALSE`.
 * 5. Execute the assembly code `barrier()`.
 * 6. Set `pok_sched_current_slot` to `0`.
 * 7. Set `pok_sched_next_major_frame` to
 *    `first_frame_starts + pok_config_scheduling_major_frame`.
 * 8. Set `pok_sched_next_deadline` to
 *    `pok_module_sched[0].duration + first_frame_starts`.
 * 9. Set `current_partition` to `pok_module_sched[0].partition`.
 * 10. Set `new_sp` to the address of `current_partition->sp`.
 * 11. If `current_partition->is_paused` is true:
 *     1. Set `new_sp` to the address of `idle_sp`.
 * 12. Set `current_partition_is_paused` to `current_partition->is_paused`.
 * 13. If the value at `new_sp` is NULL:
 *     1. Call `ja_context_init()` with the arguments
 *        `current_partition->initial_sp` and the address of `start_partition`,
 *        and store the result in `*new_sp`.
 * 14. If `current_partition->space_id` is not equal to `0xffU`:
 *     1. Call `ja_space_switch()` with the argument
 *        `current_partition->space_id`.
 * 15. Otherwise:
 *     1. Call `ja_space_switch()` with the argument `0xff`.
 * 16. Set `kernel_state` to `POK_SYSTEM_STATE_OS_PART`.
 * 17. Call `ja_context_jump()` with the value at `new_sp`.
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
 *
 * @llr
 *
 * @design
 * 1. Call `pok_stack_alloc()` with the argument `KERNEL_STACK_SIZE_DEFAULT`,
 *    and store the result in `idle_stack`.
 * 2. Call `pok_sched_restart()`.
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
 *
 * @llr
 *
 * @design
 * 1. Set `part` to `current_partition`.
 * 2. If `flag_test_and_reset(&sched_need_recheck)` is false:
 *    1. Return.
 * 3. Set `now` to `jet_system_time()`.
 * 4. If `pok_sched_next_deadline` is greater than `now`:
 *    1. Go to the label `same_partition`.
 * 5. Set `pok_sched_current_slot` to `(pok_sched_current_slot + 1U)`.
 * 6. If `pok_sched_current_slot` is equal to `pok_module_sched_n`:
 *    1. Add `pok_config_scheduling_major_frame` to
 *       `pok_sched_next_major_frame`.
 *    2. Set `pok_sched_current_slot` to `0`.
 * 7. Add `pok_module_sched[pok_sched_current_slot].duration` to
 *    `pok_sched_next_deadline`.
 * 8. Set `new_partition` to
 *    `pok_module_sched[pok_sched_current_slot].partition`.
 * 9. If `new_partition` is equal to `part`:
 *    1. Go to the label `same_partition`.
 * 10. Call `inter_partition_switch()` with the argument `new_partition`.
 * 11. Set `now` to `jet_system_time()`.
 * 12. If `part->timer` is not equal to `0` and `part->timer` is less than or
 *     equal to `now`:
 *     1. Call `pok_partition_add_event()` with the arguments `part`,
 *        `JET_PARTITION_EVENT_TYPE_TIMER` and `0`.
 *     2. Set `part->timer` to `0`.
 * 13. Return.
 * 14. Label `same_partition`:
 * 15. Call `intra_partition_switch()`.
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
 *
 * @llr
 *
 * @design
 * 1. Check that `ja_preempt_enabled()` holds (assert: stop otherwise).
 * 2. Call `ja_preempt_disable()`.
 *
 * @calls
 * - `ja_preempt_enabled()`
 * - `pok_assertion_fail()`
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
 *
 * @llr
 *
 * @design
 * 1. Check that `!ja_preempt_enabled()` holds (assert: stop otherwise).
 * 2. Call `pok_sched()`.
 * 3. If `current_partition->preempt_local_disabled` is true or
 *    `current_partition->is_event` is false:
 *    1. Call `ja_preempt_enable()`.
 *    2. Return.
 * 4. Set `current_partition->preempt_local_disabled` to `TRUE`.
 * 5. Repeat the following while `current_partition->preempt_local_disabled` is
 *    true and `current_partition->is_event` is true (at least once):
 *    1. Call `ja_preempt_enable()`.
 *    2. Call the function `current_partition->part_sched_ops->on_event`.
 *    3. Call `ja_preempt_disable()`.
 * 6. Set `current_partition->preempt_local_disabled` to `FALSE`.
 * 7. Call `ja_preempt_enable()`.
 *
 * @calls
 * - `ja_preempt_enabled()`
 * - `pok_assertion_fail()`
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
 *
 * @llr
 *
 * @design
 * 1. Check that `!ja_preempt_enabled()` holds (assert: stop otherwise).
 * 2. Call `ja_preempt_enable()`.
 *
 * @calls
 * - `ja_preempt_enabled()`
 * - `pok_assertion_fail()`
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
 *
 * @llr
 *
 * @design
 * 1. Set `offset` to `pok_sched_next_deadline`.
 * 2. Set `time_slot_index` to `pok_sched_current_slot`.
 * 3. Set `i` to `0U`; then while `i` is less than `pok_module_sched_n`, repeat
 *    (after each pass: increment `i`):
 *    1. Increment `time_slot_index`.
 *    2. If `time_slot_index` is equal to `pok_module_sched_n`:
 *       1. Set `time_slot_index` to `0U`.
 *    3. Set `slot` to the address of `pok_module_sched[time_slot_index]`.
 *    4. If `slot->periodic_processing_start` is true and `slot->partition` is
 *       equal to `current_partition`:
 *       1. Return `offset`.
 *    5. Add `slot->duration` to `offset`.
 * 4. Use the macro `unreachable()`.
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
 *
 * @llr
 *
 * @design
 * 1. Check that `!ja_preempt_enabled()` holds (assert: stop otherwise).
 * 2. Set `part` to `current_partition`.
 * 3. Set `sched_need_recheck` to `TRUE`.
 * 4. Set `in_user_space` to `pok_in_user_space`.
 * 5. If `in_user_space` is true:
 *    1. Set `part->entry_sp_user` to `global_thread_stack`.
 *    2. Set `pok_in_user_space` to `FALSE`.
 * 6. Call `pok_sched()`.
 * 7. If `current_partition_is_paused` is true:
 *    1. Go to the label `out`.
 * 8. Set `preempt_local_disabled_old` to
 *    `current_partition->preempt_local_disabled`.
 * 9. Set `current_time` to `jet_system_time()`.
 * 10. If `part->timer` is not equal to `0` and `part->timer` is less than or
 *     equal to `current_time`:
 *     1. Call `pok_partition_add_event()` with the arguments `part`,
 *        `JET_PARTITION_EVENT_TYPE_TIMER` and `0`.
 *     2. Set `part->timer` to `0`.
 * 11. If `preempt_local_disabled_old` is true or `part->is_event` is false:
 *     1. Go to the label `out`.
 * 12. Repeat the following while `part->preempt_local_disabled` is true and
 *     `part->is_event` is true (at least once):
 *     1. Set `part->preempt_local_disabled` to `TRUE`.
 *     2. Call `ja_preempt_enable()`.
 *     3. Call the function `part->part_sched_ops->on_event`.
 *     4. Call `ja_preempt_disable()`.
 * 13. Set `part->preempt_local_disabled` to `FALSE`.
 * 14. Label `out`:
 * 15. Set `pok_in_user_space` to `in_user_space`.
 *
 * @calls
 * - `ja_preempt_enabled()`
 * - `pok_assertion_fail()`
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
 *
 * @llr
 *
 * @design
 * 1. Set `part` to `current_partition`.
 * 2. Call `pok_preemption_disable()`.
 * 3. While `part->preempt_local_disabled` is true and `part->is_event` is true:
 *    1. Set `part->preempt_local_disabled` to `TRUE`.
 *    2. Call `ja_preempt_enable()`.
 *    3. Call the function `part->part_sched_ops->on_event`.
 *    4. Call `ja_preempt_disable()`.
 * 4. Set `part->preempt_local_disabled` to `FALSE`.
 * 5. Set `pok_in_user_space` to `TRUE`.
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
 *
 * @llr
 *
 * @design
 * 1. Call `pok_partition_return_user_common()`.
 * 2. Call `pok_partition_return_user_fp()`.
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
 *
 * @llr
 *
 * @design
 * 1. Set `part` to `current_partition`.
 * 2. Check that `part->fp_store_current != NULL` holds (assert: stop
 *    otherwise).
 * 3. If `fp_store_last` is not equal to `part->fp_store_current`:
 *    1. If `fp_store_last` is not NULL:
 *       1. Call `ja_fp_save()` with the argument `fp_store_last`.
 *    2. Set `fp_store_last` to `part->fp_store_current`.
 *    3. Call `ja_fp_restore()` with the argument `fp_store_last`.
 *
 * @calls
 * - `pok_assertion_fail()`
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
 *
 * @llr
 *
 * @design
 * 1. If `fp_store_last` is not NULL:
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
 *
 * @llr
 *
 * @design
 * 1. If `fp_store_last` is not NULL:
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
 *
 * @llr
 *
 * @design
 * 1. Set `part` to `current_partition`.
 * 2. Call `pok_partition_return_user_common()`.
 * 3. Check that `part->fp_store_current != NULL` holds (assert: stop
 *    otherwise).
 * 4. If `fp_store_last` is not NULL and `fp_store_last` is not equal to
 *    `part->fp_store_current`:
 *    1. Call `ja_fp_save()` with the argument `fp_store_last`.
 * 5. Set `fp_store_last` to `part->fp_store_current`.
 * 6. Call `ja_fp_init()`.
 * 7. Call `ja_user_space_jump()` with the arguments `stack_kernel`,
 *    `part->space_id`, `entry` and `(uintptr_t)stack_user`.
 *
 * @calls
 * - `pok_partition_return_user_common()`
 * - `pok_assertion_fail()`
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
 *
 * @llr
 *
 * @design
 * 1. Set `part` to `current_partition`.
 * 2. Check that `part != NULL` holds (assert: stop otherwise).
 * 3. Call `pok_preemption_disable()`.
 * 4. Increment `part->partition_generation`.
 * 5. If `part->partition_generation` is equal to `0U`:
 *    1. Set `part->partition_generation` to `1`.
 * 6. Set `part->sp` to `NULL`.
 * 7. Set `sched_need_recheck` to `TRUE`.
 * 8. Call `pok_preemption_enable()`.
 * 9. Use the macro `unreachable()`.
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
 *
 * @llr
 *
 * @design
 * 1. Call `pok_partition_init()` with the address of `partition_idle`.
 * 2. Call `pok_stack_alloc()` with the argument `4096`, and store the result in
 *    `partition_idle.initial_sp`.
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
