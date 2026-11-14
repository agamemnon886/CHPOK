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
 *
 * @llr
 *
 * @design
 * 1. Call `ja_inf_loop()`.
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
 *
 * @llr
 *
 * @design
 * 1. Set `part` to `current_partition_arinc`.
 * 2. Set `part->base_part.preempt_local_disabled` to `TRUE`.
 * 3. Call `ja_context_restart()` with the arguments
 *    `part->base_part.initial_sp` and the address of `idle_func`.
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
 *
 * @llr
 *
 * @design
 * 1. Set `t->name[0]` to `'\0'`.
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
 *
 * @llr
 *
 * @design
 * 1. Call `pok_stack_alloc()` with the argument `KERNEL_STACK_SIZE_DEFAULT`,
 *    and store the result in `t->initial_sp`.
 * 2. Call `ja_alloc_fp_store()`, and store the result in `t->fp_store`.
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
 *
 * @llr
 *
 * @design
 * 1. Set `part` to `current_partition_arinc`.
 * 2. If `part->base_part.restarted_externally` is true:
 *    1. Set `part->base_part.restarted_externally` to `FALSE`.
 *    2. Set `part->mode` to `POK_PARTITION_MODE_INIT_COLD`.
 * 3. Call `jet_loader_elf_load()` with the arguments
 *    `part->base_part.space_id - 1U`, `part->base_part.space_id` and the
 *    address of `part->main_entry`.
 * 4. Call `jet_gdb_space_loaded()` with the argument
 *    `part->base_part.space_id`.
 * 5. Call `ja_space_shared_data()` with the argument
 *    `part->base_part.space_id`, and store the result in `part->kshd`.
 * 6. If `part->heap_size` is greater than `0U`:
 *    1. Call `ja_space_get_heap()` with the argument
 *       `part->base_part.space_id`, and store the result in `heap_start`.
 *    2. Set `heap_end` to `heap_start + part->heap_size`.
 *    3. Set `part->kshd->heap_start` to `heap_start`.
 *    4. Set `part->kshd->heap_end` to `heap_end`.
 * 7. Otherwise:
 *    1. Set `part->kshd->heap_start` to `NULL`.
 *    2. Set `part->kshd->heap_end` to `NULL`.
 * 8. Set `i` to `0U`; then while `i` is less than `part->nports_queuing`,
 *    repeat (after each pass: increment `i`):
 *    1. Call `pok_port_queuing_init()` with the address of
 *       `part->ports_queuing[i]`.
 * 9. Set `i` to `0U`; then while `i` is less than `part->nports_sampling`,
 *    repeat (after each pass: increment `i`):
 *    1. Call `pok_port_sampling_init()` with the address of
 *       `part->ports_sampling[i]`.
 * 10. Call `ja_ustack_init()` with the argument `part->base_part.space_id`.
 * 11. Call `INIT_LIST_HEAD()` with the address of `part->eligible_threads`.
 * 12. Call `delayed_event_queue_init()` with the address of
 *     `part->partition_delayed_events`.
 * 13. Set `i` to `0U`; then while `i` is less than `part->nthreads`, repeat
 *     (after each pass: increment `i`):
 *     1. Call `thread_reset()` with the address of `part->threads[i]`.
 * 14. Set `part->nthreads_used` to `0`.
 * 15. Set `part->thread_current` to `NULL`.
 * 16. Set `part->thread_error` to `NULL`.
 * 17. Set `part->error_handler_concurrency` to
 *     `POK_ERROR_HANDLER_PROCESSES_PAUSE`.
 * 18. Set `part->error_handler_core` to `POK_CORE_AFFINITY_NO_PREFERENCE`.
 * 19. Call `INIT_LIST_HEAD()` with the address of `part->error_list`.
 * 20. Set `thread_main` to the address of
 *     `part->threads[POK_PARTITION_ARINC_MAIN_THREAD_ID]`.
 * 21. Set `thread_main->entry` to `(void* JET_USER)part->main_entry`.
 * 22. Set `thread_main->base_priority` to `0`.
 * 23. Set `thread_main->period` to `POK_TIME_INFINITY`.
 * 24. Set `thread_main->time_capacity` to `POK_TIME_INFINITY`.
 * 25. Set `thread_main->deadline` to `DEADLINE_SOFT`.
 * 26. Call `strncpy()` with the arguments `thread_main->name`,
 *     `main_thread_name` and `MAX_NAME_LENGTH`.
 * 27. Set `thread_main->user_stack_size` to `part->main_user_stack_size`.
 * 28. If `thread_create(thread_main)` is false:
 *     1. Use the macro `unreachable()`.
 * 29. Set `part->nthreads_used` to `POK_PARTITION_ARINC_MAIN_THREAD_ID + 1U`.
 * 30. Set `part->kshd->current_thread_id` to `JET_THREAD_ID_NONE`.
 * 31. Set `part->kshd->max_n_threads` to `part->nthreads`.
 * 32. Set `part->kshd->partition_mode` to `part->mode`.
 * 33. Call `sched_arinc_start()`.
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
 *
 * @llr
 *
 * @design
 * 1. Set `part` to `current_partition_arinc`.
 * 2. Set `part->base_part.preempt_local_disabled` to `TRUE`.
 * 3. Check that `(mode == POK_PARTITION_MODE_INIT_WARM) || (mode ==`
 *    `POK_PARTITION_MODE_INIT_COLD)` holds (assert: stop otherwise).
 * 4. Check that `(mode == POK_PARTITION_MODE_INIT_COLD) || (part->mode !=`
 *    `POK_PARTITION_MODE_INIT_COLD)` holds (assert: stop otherwise).
 * 5. Set `part->mode` to `mode`.
 * 6. Call `pok_partition_restart()`.
 *
 * @calls
 * - `pok_assertion_fail()`
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
 *
 * @llr
 *
 * @design
 * 1. Set `part_arinc` to `part->part_arinc`.
 * 2. Return `(int32_t)part_arinc->nthreads + 1`.
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
 *
 * @llr
 *
 * @design
 * 1. Set `part_arinc` to `part->part_arinc`.
 * 2. Set `index` to `part_arinc->nthreads`.
 * 3. Set `i` to `0U`; then while `i` is less than `part_arinc->nthreads`,
 *    repeat (after each pass: increment `i`):
 *    1. If the address of `part_arinc->threads[i]` is equal to
 *       `part_arinc->thread_current`:
 *       1. Set `index` to `i`.
 * 4. Return `(int32_t)index`.
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
 *
 * @llr
 *
 * @design
 * 1. Return `(index >= 0) && ((uint32_t) index < part->nthreads_used)`.
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
 *
 * @llr
 *
 * @design
 * 1. Set `part_arinc` to `part->part_arinc`.
 * 2. Set `ret` to `0`.
 * 3. If `index` is greater than or equal to `0` and `(uint32_t) index` is equal
 *    to `part_arinc->nthreads`:
 *    1. Set `*private` to `NULL`.
 * 4. Otherwise, if `arinc_thread_is_created(part_arinc, index)` is true:
 *    1. Set `*private` to the address of `part_arinc->threads[index]`.
 * 5. Otherwise:
 *    1. Set `ret` to `1`.
 * 6. Return `ret`.
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
 *
 * @llr
 *
 * @design
 * 1. Call the function `print_cb` with the arguments `s`, `strlen(s)` and
 *    `cb_data`.
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
 *
 * @llr
 *
 * @design
 * 1. Set `t` to `private`.
 * 2. Set `part_arinc` to `part->part_arinc`.
 * 3. If `t` is NULL:
 *    1. Call `arinc_thread_write_str()` with the arguments `print_cb`,
 *       `cb_data` and `"IDLE"`.
 * 4. Otherwise, if `index` is equal to
 *    `(int32_t)POK_PARTITION_ARINC_MAIN_THREAD_ID`:
 *    1. Call `arinc_thread_write_str()` with the arguments `print_cb`,
 *       `cb_data` and `t->name`.
 * 5. Otherwise, if `arinc_thread_is_created(part_arinc, index)` is true:
 *    1. Set `state` to `""`.
 *    2. Depending on `t->state`:
 *       1. Case `POK_STATE_STOPPED`:
 *          1. Set `state` to `"Stopped"`.
 *          2. Leave the switch.
 *       2. Case `POK_STATE_WAITING`:
 *          1. Set `state` to `"Waiting"`.
 *          2. Leave the switch.
 *       3. Case `POK_STATE_RUNNABLE`:
 *          1. If `t->suspended` is true:
 *             1. Set `state` to `"Suspended"`.
 *          2. Otherwise, if `part_arinc->thread_current` is equal to `t`:
 *             1. Set `state` to `"Running"`.
 *          3. Otherwise:
 *             1. Set `state` to `"Ready"`.
 *          4. Leave the switch.
 *       4. Otherwise (default):
 *          1. Use the macro `unreachable()`.
 *          2. Leave the switch.
 *    3. Call `arinc_thread_write_str()` with the arguments `print_cb`,
 *       `cb_data` and `t->name`.
 *    4. Call `arinc_thread_write_str()` with the arguments `print_cb`,
 *       `cb_data` and `" "`.
 *    5. Call `arinc_thread_write_str()` with the arguments `print_cb`,
 *       `cb_data` and `state`.
 * 6. Otherwise:
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
 *
 * @llr
 *
 * @design
 * 1. Set `t` to `private`.
 * 2. Set `regs` to `NULL`.
 * 3. If `t` is not NULL and `arinc_thread_is_created(part->part_arinc, index)`
 *    is true:
 *    1. Set `regs` to `t->entry_sp_user`.
 * 4. Return `regs`.
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
 *
 * @llr
 *
 * @design
 * 1. Set `t` to `private`.
 * 2. Set `fp_store` to `NULL`.
 * 3. If `t` is not NULL and `arinc_thread_is_created(part->part_arinc, index)`
 *    is true:
 *    1. Set `fp_store` to `t->fp_store`.
 * 4. Return `fp_store`.
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
 *
 * @llr
 *
 * @design
 * 1. Call `pok_partition_init()` with the address of `part->base_part`.
 * 2. Set `part->base_part.part_arinc` to `part`.
 * 3. Call `pok_stack_alloc()` with the argument `DEFAULT_STACK_SIZE`, and store
 *    the result in `part->base_part.initial_sp`.
 * 4. Set `i` to `0U`; then while `i` is less than `part->nthreads`, repeat
 *    (after each pass: increment `i`):
 *    1. Call `thread_init()` with the address of `part->threads[i]`.
 * 5. Set `part->base_part.part_ops` to the address of `arinc_ops`.
 * 6. Set `part->base_part.part_sched_ops` to the address of `arinc_sched_ops`.
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
 *
 * @llr
 *
 * @design
 * 1. Set `part` to `current_partition_arinc`.
 * 2. Set `current_time` to `jet_system_time()`.
 * 3. Set `periodic_release_point` to `POK_TIME_INFINITY`.
 * 4. Set `part->mode` to `POK_PARTITION_MODE_NORMAL`.
 * 5. Set `part->lock_level` to `0`.
 * 6. Call `pok_sched_local_invalidate()`.
 * 7. Set `i` to `POK_PARTITION_ARINC_MAIN_THREAD_ID + 1U`; then while `i` is
 *    less than `part->nthreads_used`, repeat (after each pass: increment `i`):
 *    1. Set `t` to the address of `part->threads[i]`.
 *    2. If `t->state` is equal to `POK_STATE_STOPPED`:
 *       1. Go on with the next pass of the loop.
 *    3. If `pok_time_is_infinity(t->period)` is true:
 *       1. Call `pok_time_add()` with the arguments `current_time` and
 *          `t->delayed_time`, and store the result in `thread_start_time`.
 *    4. Otherwise:
 *       1. If `pok_time_is_infinity(periodic_release_point)` is true:
 *          1. Call `get_next_periodic_processing_start()`, and store the result
 *             in `periodic_release_point`.
 *       2. Call `pok_time_add()` with the arguments `periodic_release_point`
 *          and `t->delayed_time`, and store the result in `thread_start_time`.
 *       3. Set `t->next_activation` to `thread_start_time`.
 *    5. If `thread_start_time` is less than or equal to `current_time`:
 *       1. Call `thread_wake_up()` with the argument `t`.
 *    6. Otherwise:
 *       1. Call `thread_delay_event()` with the arguments `t`,
 *          `thread_start_time` and the address of `thread_wake_up`.
 *    7. If `pok_time_is_infinity(t->time_capacity)` is false:
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
 *
 * @llr
 *
 * @design
 * 1. Set `part` to `current_partition_arinc`.
 * 2. Depending on `mode`:
 *    1. Case `POK_PARTITION_MODE_INIT_WARM` or `POK_PARTITION_MODE_INIT_COLD`:
 *       1. If `mode` is equal to `POK_PARTITION_MODE_INIT_WARM` and
 *          `part->mode` is equal to `POK_PARTITION_MODE_INIT_COLD`:
 *          1. Return `POK_ERRNO_PARTITION_MODE`.
 *       2. Set `part->base_part.start_condition` to
 *          `POK_START_CONDITION_PARTITION_RESTART`.
 *       3. Call `pok_partition_arinc_reset()` with the argument `mode`.
 *       4. Leave the switch.
 *    2. Case `POK_PARTITION_MODE_IDLE`:
 *       1. Call `pok_partition_arinc_idle()`.
 *       2. Leave the switch.
 *    3. Case `POK_PARTITION_MODE_NORMAL`:
 *       1. If `part->mode` is equal to `POK_PARTITION_MODE_NORMAL`:
 *          1. Return `POK_ERRNO_UNAVAILABLE`.
 *       2. Call `partition_set_mode_normal()`.
 *       3. Leave the switch.
 *    4. Otherwise (default):
 *       1. Return `POK_ERRNO_EINVAL`.
 * 3. Set `part->kshd->partition_mode` to `part->mode`.
 * 4. Return `POK_ERRNO_OK`.
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
 *
 * @llr
 *
 * @design
 * 1. Call `pok_preemption_local_disable()`.
 * 2. Call `partition_set_mode_internal()` with the argument `mode`, and store
 *    the result in `res`.
 * 3. Call `pok_preemption_local_enable()`.
 * 4. Return `res`.
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
 *
 * @llr
 *
 * @design
 * 1. Set `k_status` to `jet_user_to_kernel_typed(status)`.
 * 2. If `k_status` is NULL:
 *    1. Return `POK_ERRNO_EFAULT`.
 * 3. Set `k_status->id` to `current_partition->partition_id`.
 * 4. Set `k_status->period` to `current_partition->period`.
 * 5. Set `k_status->duration` to `current_partition->duration`.
 * 6. Set `k_status->mode` to `current_partition_arinc->mode`.
 * 7. Set `k_status->lock_level` to `current_partition_arinc->lock_level`.
 * 8. Set `k_status->start_condition` to `current_partition->start_condition`.
 * 9. Return `POK_ERRNO_OK`.
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
 *
 * @llr
 *
 * @design
 * 1. Set `part` to `current_partition_arinc`.
 * 2. Return
 *    `(part->mode != POK_PARTITION_MODE_NORMAL) || (part->thread_current ==`
 *    `part->thread_error)`.
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
 *
 * @llr
 *
 * @design
 * 1. Set `part` to `current_partition_arinc`.
 * 2. If `is_lock_level_blocked()` is true:
 *    1. Return `POK_ERRNO_PARTITION_MODE`.
 * 3. If `part->lock_level` is equal to `MAX_LOCK_LEVEL`:
 *    1. Return `POK_ERRNO_EINVAL`.
 * 4. Set `k_lock_level` to `jet_user_to_kernel_typed(lock_level)`.
 * 5. If `k_lock_level` is NULL:
 *    1. Return `POK_ERRNO_EFAULT`.
 * 6. Call `pok_preemption_local_disable()`.
 * 7. Increment `part->lock_level`.
 * 8. Set `part->thread_locked` to `part->thread_current`.
 * 9. Call `pok_preemption_local_enable()`.
 * 10. Set `*k_lock_level` to `current_partition_arinc->lock_level`.
 * 11. Return `POK_ERRNO_OK`.
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
 *
 * @llr
 *
 * @design
 * 1. If `is_lock_level_blocked()` is true:
 *    1. Return `POK_ERRNO_PARTITION_MODE`.
 * 2. If `current_partition_arinc->lock_level` is equal to `0U`:
 *    1. Return `POK_ERRNO_EINVAL`.
 * 3. Set `k_lock_level` to `jet_user_to_kernel_typed(lock_level)`.
 * 4. If `k_lock_level` is NULL:
 *    1. Return `POK_ERRNO_EFAULT`.
 * 5. Call `pok_preemption_local_disable()`.
 * 6. If `--current_partition_arinc->lock_level` is equal to `0U`:
 *    1. If `current_thread->eligible_elem.prev` is not equal to the address of
 *       `current_partition_arinc->eligible_threads`:
 *       1. Call `pok_sched_local_invalidate()`.
 * 7. Call `pok_preemption_local_enable()`.
 * 8. Set `*k_lock_level` to `current_partition_arinc->lock_level`.
 * 9. Return `POK_ERRNO_OK`.
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
 *
 * @llr
 *
 * @design
 * 1. Set `i` to `0U`; then while `i` is less than `pok_partitions_arinc_n`,
 *    repeat (after each pass: increment `i`):
 *    1. Call `pok_partition_arinc_init()` with the address of
 *       `pok_partitions_arinc[i]`.
 *    2. Call `pok_instrumentation_partition_archi()` with the argument `i`.
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
