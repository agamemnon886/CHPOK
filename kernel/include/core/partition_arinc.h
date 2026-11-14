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
 * Modified by Kirill Brazhnikov, 2026-11-01: added the error handler
 * configuration set by CONFIGURE_ERROR_HANDLER.
 *
 * Modified by Kirill Brazhnikov, 2026-11-02: MISRA C:2025: the header
 * guard isn't a reserved identifier; __user/__kuser are renamed
 * JET_USER/JET_KUSER; U suffixes for unsigned constants; deviations are
 * documented where a guideline can't be followed.
 *
 * Modified by Kirill Brazhnikov, 2026-11-03: MISRA C:2025: deviations are
 * documented where a guideline can't be followed; the structure tag is
 * pok_partition_arinc instead of the reserved _pok_patition_arinc, and
 * current_partition_arinc is base_part.part_arinc instead of a
 * container_of() conversion.
 *
 * Modified by Kirill Brazhnikov, 2026-11-11: the Doxygen comments that don't
 * document a function definition are plain comments, so that only the function
 * headers are in the function documentation; long member comments are wrapped.
 *
 * Modified by Kirill Brazhnikov, 2026-11-13: the function declarations are
 * hidden from Doxygen (\cond): only the definitions of the functions are
 * documented; struct delayed_event_queue is the typedef delayed_event_queue_t;
 * struct jet_context is used as the typedef jet_context_t (declared in
 * asp/cswitch.h); the structures and enums are used by their typedef names:
 * jet_kernel_shared_t, list_head_t, msection_t; Doxygen comments for the file,
 * its types (with their members) and its macros; pok_error_hm_partition_t is
 * renamed pok_error_hm_part_t, pok_error_level_selector_t is renamed
 * pok_error_levels_t, pok_error_handler_concurrency_t is renamed
 * pok_error_concurrency_t (names too long for the tables of the documentation).
 *
 * Modified by Kirill Brazhnikov, 2026-11-14: the enum types are named with _e:
 * pok_partition_mode_e (was pok_partition_mode_t), pok_ret_e (was pok_ret_t);
 * the Doxygen comment of the file is removed (header files are not documented).
 */

#ifndef POK_PARTITION_ARINC_H
#define POK_PARTITION_ARINC_H

#include <core/partition.h>
#include <core/error_arinc.h>
#include <core/port.h>

#include <uapi/partition_arinc_types.h>

/*
 * \struct pok_partition_t
 * \brief This structure contains all needed information for partition management
 */
/**
 * @typename `pok_partition_arinc_t`
 *
 * @description
 * An ARINC 653 partition: the partition (base_part) and what ARINC 653 adds:
 * its processes, ports, error handler, memory and lock level.
 */
struct pok_partition_arinc
{
    pok_partition_t        base_part; /**< The partition (its common part). */
    
    /**
     * Information which affects on scheduling has been changed.
     */
    pok_bool_t sched_local_recheck_needed;

    pok_partition_mode_e   mode; /**< Current mode of the partition. */

    /**
     * Index in the elf array where partition's image is contained.
     *
     * Set in deployment.c.
     */
    uint8_t                 elf_id;

    pok_time_t             activation; /**< Not used now. */

    /**
     * Number of (allocated) threads inside the partition.
     *
     * Set in deployment.c.
     */
    uint32_t               nthreads;       
    /**
     * Array of (allocated) threads inside the partition.
     *
     * Set in deployment.c. (thread needn't to be initialized there).
     */
    pok_thread_t*          threads;
    uint32_t               nthreads_used;   /* Number of threads which are
                                             * currently in use (created). */


    /**
     * Normal thread or special thread. NULL if doing nothing.
     */
    pok_thread_t*          thread_current;

    /**
     * Thread, selected according to scheduling algorithm.
     *
     * Selected thread may differ from current one, if selected thread wants to
     * enter into critical section, which already has an owner, or selected
     * thread kills a thread, which is currently in critical section.
     */
    pok_thread_t*          thread_selected;

    /**
     * When current thread releases given msection, it should be preempted.
     *
     * If thread_selected != thread_current but waiting_section is NULL, then
     * preemption should be occured when current thread leaves all sections and
     * terminates.
     */
    msection_t*        waiting_section;

    /**
     * Size of the heap to be allocated. Set in the deployment.c.
     */
    size_t heap_size;

    pok_port_queuing_t*    ports_queuing; /* List of queuing ports. Set in
                                           * deployment.c. */
    size_t                 nports_queuing; /**< The number of queuing ports. */

    pok_port_sampling_t*   ports_sampling; /* List of sampling ports. Set in
                                            * deployment.c. */
    /**
     * The number of sampling ports.
     */
    size_t                 nports_sampling;

/* Error and main threads are special in sence that they cannot be reffered by ID.*/

#ifdef POK_NEEDS_ERROR_HANDLING
    pok_thread_t*          thread_error;     /* Error thread. One of the
                                              * @threads. */
    list_head_t       error_list;       /* List of threads in errorneus
                                              * state. */

    /**
     * Error handler configuration (CONFIGURE_ERROR_HANDLER).
     *
     * Every partition runs on a single processor core, where the error handler
     * always preempts the other processes, so the scheduler doesn't need these
     * values yet. They are kept for multicore scheduling.
     */
    pok_error_concurrency_t error_handler_concurrency;
    /**
     * Core ID or POK_CORE_AFFINITY_NO_PREFERENCE.
     */
    int32_t                error_handler_core;
    
    /**
     * The synchronous error being handled (see POK_THREAD_ERROR_BIT_SYNC).
     */
    pok_error_id_t         sync_error;
    /**
     * The address of that error.
     */
    void* JET_USER           sync_error_failed_addr;
    
    /**
     * Select level for the partition error.
     *
     * Value 0 means partition level, 1 - thread(process) level.
     *
     * Should be set in deployment.c.
     */
    const pok_error_levels_t* partition_hm_selector;
    
    /**
     * Map contains information about thread(process)-level errors.
     *
     * Should be set in deployment.c.
     */
    const pok_thread_error_map_t* thread_error_info;
#endif
    /**
     * The data shared with the partition (in its memory).
     */
    jet_kernel_shared_t* kshd;

    /**
     * Pointer to partition HM table.
     */
    const pok_error_hm_part_t* partition_hm_table;

    /**
     * Number of threads in unrecoverable state.
     *
     * This field is incremented every time a thread finds its error state as
     * unrecoverable. Error, emitted in this case, should be processed at
     * partition(or above) level or by error handler. Otherwise, new error will
     * be emitted for partition.
     *
     * The field is decremented every time unrecoverable thread is stopped.
     */
    uint32_t                nthreads_unrecoverable;


    /**
     * The entry point of the main thread.
     */
    void                    (*main_entry)(void);
    /**
     * The size of the user stack of the main thread.
     */
    uint32_t                main_user_stack_size;


    /**
     * The lock level (LOCK_PREEMPTION).
     */
    uint32_t		        lock_level;
    /**
     * Thread which locks preemption.
     */
    pok_thread_t*           thread_locked;

    /**
     * Priority/FIFO ordered queue of eligible threads.
     *
     * Used only in NORMAL mode.
     */
    list_head_t       eligible_threads; 

    /**
     * Queue of all timed events.
     */
    delayed_event_queue_t partition_delayed_events;
    
    /**
     * After main partition's thread creates start thread, it is used as idle
     * thread.
     *
     * This is context pointer for switch to it.
     */
    jet_context_t*               idle_sp;
};

/**
 * @macroname `current_partition_arinc`
 *
 * @description
 * The current partition, which must be an ARINC partition.
 */
#define current_partition_arinc (current_partition->part_arinc)

/**
 * @macroname `current_thread`
 *
 * @description
 * The current thread of the current partition (which must be an ARINC
 * partition).
 */
#define current_thread (current_partition_arinc->thread_current)

/* 
 * Array of ARINC partitions.
 * 
 * Set in deployment.c.
 */
extern pok_partition_arinc_t pok_partitions_arinc[];

/*
 * Number of ARINC partitions.
 * 
 * Set in deployment.c
 */
extern const uint8_t pok_partitions_arinc_n;

/* 
 * Get chunk of `intra_memory` with given size and alignment.
 * 
 * Note, that this memory is reseted at partition restart.
 */
/** \cond */
void* partition_arinc_im_get(size_t size, size_t alignment);

/* 
 * Return current state of partition's intra memory.
 * 
 * Value return may be used in partition_arinc_im_rollback().
 */
void* partition_arinc_im_current(void);

/*
 * Revert all intra memory usage requests since given state.
 * 
 * `state` should be obtains with partition_arinc_im_current().
 */
void partition_arinc_im_rollback(void* prev_state);

/** \endcond */


// Main thread is always first allocated (Do not change this!).

/**
 * @macroname `POK_PARTITION_ARINC_MAIN_THREAD_ID`
 *
 * @description
 * The ID of the main thread of a partition. Main thread is always first
 * allocated (Do not change this!).
 */
#define POK_PARTITION_ARINC_MAIN_THREAD_ID 0U


/* Initialize ARINC partitions. */
/** \cond */
void pok_partition_arinc_init_all(void);

/* Initialize arinc partition. */
void pok_partition_arinc_init(pok_partition_arinc_t* part);

/*
 * Reset current arinc partition.
 * 
 * May be called as reaction for partition-level error
 * or by the use space via SET_PARTITION_MODE.
 * 
 * Switch between COLD_START and WARM_START generates error.
 * 
 * Appropriate START_CONDITION should be set before.
 */
void pok_partition_arinc_reset(pok_partition_mode_e mode);

/* 
 * Move current partition into IDLE state.
 * 
 * May be called at any time of partition's execution.
 */
void pok_partition_arinc_idle(void);

pok_ret_e pok_partition_set_mode_current (const pok_partition_mode_e mode);

pok_ret_e pok_current_partition_get_status (pok_partition_status_t* status);

pok_ret_e pok_current_partition_inc_lock_level(int32_t *lock_level);

pok_ret_e pok_current_partition_dec_lock_level(int32_t *lock_level);

/** \endcond */


/*
 * Raise error about inconsistent state of OS.
 * 
 * User space may modify part of OS state directly writting some memory
 * cells. This is faster than syscalls, but may lead to inconsistent state
 * of OS. When kernel code detects inconsistency, it should call this function.
 */
// TODO: this should raise partition-level error.

/**
 * @macroname `assert_os`
 *
 * @description
 * Checks a condition on data that the partition may have changed (shared with
 * user space): stops the kernel if it is false (assert()). TODO: this should
 * raise partition-level error.
 *
 * @params
 * | Name | Description |
 * |------|-------------|
 * | `expr` | The condition. |
 */
#define assert_os(expr) assert(expr)

#endif /* !POK_PARTITION_ARINC_H */
