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


/*
 * Function which is executed in kernel-only partition's context when
 * partition's mode is IDLE.
 *
 * Note, that we do not enable local preemption here.
 * This has nice effect in case when partition has moved into this mode
 * because of errors: even if some partition's data are corrupted,
 * idle have high chance to work.
 */
static void idle_func(void)
{
    ja_inf_loop();
}

void pok_partition_arinc_idle(void)
{
	pok_partition_arinc_t* part = current_partition_arinc;

	// Unconditionally off preemption.
	part->base_part.preempt_local_disabled = TRUE;

	jet_context_restart(part->base_part.initial_sp, &idle_func);
}

/* Helpers */

/*
 * Reset thread object as it is not used.
 */
static void thread_reset(pok_thread_t* t)
{
    t->name[0] = '\0';
    // Everything else will be set at thread creation time.
}


/*
 * Initialize thread object.
 *
 * Executed once during partition initialization.
 */
static void thread_init(pok_thread_t* t)
{
    t->initial_sp = pok_stack_alloc(KERNEL_STACK_SIZE_DEFAULT);
    t->fp_store = ja_alloc_fp_store();
}

// This name is not accessible for user space

/* Start function for partition. */
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

void pok_partition_arinc_reset(pok_partition_mode_t mode)
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
/*
 * Thread callbacks of the GDB server. Threads are numbered as in the
 * partition; the last index (nthreads) is the IDLE pseudo-thread, which
 * runs when no process is ready.
 */
static int32_t pok_sched_arinc_get_number_of_threads(pok_partition_t* part)
{
	const pok_partition_arinc_t* part_arinc = part->part_arinc;

	return (int32_t)part_arinc->nthreads + 1; //IDLE thread
}

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

/* Whether 'index' is a process which has been created. */
static pok_bool_t arinc_thread_is_created(const pok_partition_arinc_t* part,
	int32_t index)
{
	return (index >= 0) && ((uint32_t) index < part->nthreads_used);
}

/* Processes which haven't been created don't exist for GDB. */
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

/* Write a string (null-terminated) with the callback. */
static void arinc_thread_write_str(print_cb_t print_cb, void* cb_data,
	const char* s)
{
	print_cb(s, strlen(s), cb_data);
}

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

static struct jet_interrupt_context* pok_sched_arinc_get_thread_registers(
	pok_partition_t* part, int32_t index, void* private)
{
	const pok_thread_t* t = private;
	struct jet_interrupt_context* regs = NULL;

	// The idle thread has no registers; neither has a process which isn't
	// created or hasn't entered user space yet.
	if((t != NULL) && arinc_thread_is_created(part->part_arinc, index))
	{
		regs = t->entry_sp_user;
	}

	return regs;
}

static struct jet_fp_store* pok_sched_arinc_get_thread_fp_store(
	pok_partition_t* part, int32_t index, void* private)
{
	const pok_thread_t* t = private;
	struct jet_fp_store* fp_store = NULL;

	if((t != NULL) && arinc_thread_is_created(part->part_arinc, index))
	{
		fp_store = t->fp_store;
	}

	return fp_store;
}
#endif /* POK_NEEDS_GDB */


void pok_partition_arinc_init(pok_partition_arinc_t* part)
{
	static const struct pok_partition_sched_operations arinc_sched_ops = {
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

	static const struct pok_partition_operations arinc_ops = {
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

/*
 * Transition from INIT_* mode to NORMAL.
 *
 * Executed with local preemption disabled.
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


// Executed with local preemption disabled.
static pok_ret_t partition_set_mode_internal (const pok_partition_mode_t mode)
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

pok_ret_t pok_partition_set_mode_current (const pok_partition_mode_t mode)
{
	pok_ret_t res;

	pok_preemption_local_disable();
	res = partition_set_mode_internal(mode);
	pok_preemption_local_enable();

	return res;
}

/**
 * Get partition information. Used for ARINC GET_PARTITION_STATUS function.
 */
pok_ret_t pok_current_partition_get_status(
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

/*
 * Whether lock level cannot be changed now.
 *
 * NOTE: Doesn't require disabled local preemption.
 */
static pok_bool_t is_lock_level_blocked(void)
{
	pok_partition_arinc_t* part = current_partition_arinc;
	return (part->mode != POK_PARTITION_MODE_NORMAL) ||
      (part->thread_current == part->thread_error);
}

pok_ret_t pok_current_partition_inc_lock_level(int32_t * JET_USER lock_level)
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

pok_ret_t pok_current_partition_dec_lock_level(int32_t * JET_USER lock_level)
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
