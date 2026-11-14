/*
 * Institute for System Programming of the Russian Academy of Sciences
 * Copyright (C) 2013-2014, 2016 ISPRAS
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
 * This file also incorporates work covered by POK License.
 * Copyright (c) 2007-2009 POK team
 *
 * Modified by Kirill Brazhnikov, 2026-10-31: built-in integer types (int,
 * unsigned, short, long, ...) are replaced with the fixed-width types
 * from <stdint.h>.
 *
 * Modified by Kirill Brazhnikov, 2026-11-02: MISRA C:2025: the header
 * guard isn't a reserved identifier; essentially Boolean conditions;
 * preempt_local_disabled is a pok_bool_t.
 *
 * Modified by Kirill Brazhnikov, 2026-11-03: MISRA C:2025: the structure
 * tag is pok_partition instead of the reserved _pok_partition, and
 * pok_partition_t points to its ARINC partition (part_arinc).
 *
 * Modified by Kirill Brazhnikov, 2026-11-04: get_thread_fp_store() gives
 * the GDB server the floating point registers of a thread.
 *
 * Modified by Kirill Brazhnikov, 2026-11-11: the Doxygen comments that don't
 * document a function definition are plain comments, so that only the function
 * headers are in the function documentation; long member comments are wrapped.
 *
 * Modified by Kirill Brazhnikov, 2026-11-13: the function declarations are
 * hidden from Doxygen (\cond): only the definitions of the functions are
 * documented; struct jet_context is used as the typedef jet_context_t (declared
 * in asp/cswitch.h); struct jet_partition_event is the typedef
 * jet_partition_event_t, struct pok_partition_operations is the typedef
 * pok_part_ops_t, struct pok_partition_sched_operations is the typedef
 * pok_part_sched_ops_t, struct pok_partition is the typedef pok_partition_t,
 * struct pok_partition_arinc is the typedef pok_partition_arinc_t, enum
 * jet_partition_event_type is the typedef jet_partition_event_type_t; the
 * structures and enums are used by their typedef names: interrupt_frame,
 * jet_fp_store_t; Doxygen comments for the file and its types (with their
 * members); pok_partition_generation_t is renamed pok_part_generation_t,
 * pok_error_module_action_table_t is renamed pok_error_hm_module_t,
 * pok_error_level_selector_t is renamed pok_error_levels_t (names too long for
 * the tables of the documentation).
 *
 * Modified by Kirill Brazhnikov, 2026-11-14: the forward typedefs of
 * pok_partition_t and pok_partition_arinc_t are moved to types.h; the function
 * pointers of pok_part_sched_ops_t and pok_part_ops_t have typedefs
 * (pok_part_event_fn_t, pok_part_nthreads_fn_t, pok_part_cur_thr_fn_t,
 * pok_part_thr_at_fn_t, pok_part_thr_info_fn_t, pok_part_thr_regs_fn_t,
 * pok_part_thr_fp_fn_t, pok_part_start_fn_t, pok_part_error_fn_t), so that
 * every field fits in the table of the fields; the enum types are named with
 * _e: jet_partition_event_type_e (was jet_partition_event_type_t),
 * pok_start_condition_e (was pok_start_condition_t); the Doxygen comment of the
 * file is removed (header files are not documented).
 */


#ifndef POK_PARTITION_H
#define POK_PARTITION_H

#include <config.h>

#include <types.h>
#include <errno.h>
#include <core/error.h>

#include <gdb.h>

#include <uapi/partition_types.h>

#include <asp/cswitch.h>

#include <asp/space.h>

/**
 * @typename `pok_part_event_fn_t`
 *
 * @description
 * The handler of the asynchronous events of a partition (on_event of
 * pok_part_sched_ops_t), without parameters or result. Called when some
 * async event about partition occures. Event is encoded into partition's
 * `.state` bits(bytes).
 *
 * During call to this handler, local preemption is disabled.
 *
 * The handler should either clear `.state` bytes, or enable local
 * preemption. Otherwise the handler will be called again.
 *
 * If local preemption is already disabled, handler is not called. But
 * corresponded `.state` bytes are set nevetheless.
 */
typedef void (*pok_part_event_fn_t)(void);

#ifdef POK_NEEDS_GDB
/**
 * @typename `pok_part_nthreads_fn_t`
 *
 * @description
 * Returns the number of threads of the partition `part` (its parameter)
 * (get_number_of_threads of pok_part_sched_ops_t).
 *
 * NOTE: Called (possibly) outside of partition's timeslot, so
 * 'current_partition' doesn't correspond to given partition.
 */
typedef int32_t (*pok_part_nthreads_fn_t)(pok_partition_t* part);

/**
 * @typename `pok_part_cur_thr_fn_t`
 *
 * @description
 * Returns the index of the current thread within the partition `part` (its
 * parameter) (get_current_thread_index of pok_part_sched_ops_t).
 *
 * Returning negative value means that no current thread(is it possible?).
 *
 * If partition doesn't work with user space, never called.
 *
 * NOTE: Called (possibly) outside of partition's timeslot, so
 * 'current_partition' doesn't correspond to given partition.
 */
typedef int32_t (*pok_part_cur_thr_fn_t)(pok_partition_t* part);

/**
 * @typename `pok_part_thr_at_fn_t`
 *
 * @description
 * Looks for the thread of index `index` in the partition `part`
 * (get_thread_at_index of pok_part_sched_ops_t): returns 0 if thread with
 * given index exists, and fills `*private` with value, which will be passed
 * for callbacks below (pok_part_thr_info_fn_t, pok_part_thr_regs_fn_t,
 * pok_part_thr_fp_fn_t).
 */
typedef int32_t (*pok_part_thr_at_fn_t)(pok_partition_t* part,
    int32_t index, void** private);

/**
 * @typename `pok_part_thr_info_fn_t`
 *
 * @description
 * Gets information about the thread of index `index` of the partition `part`
 * (get_thread_info of pok_part_sched_ops_t), with `private` from
 * pok_part_thr_at_fn_t; returns nothing.
 *
 * Information is written using `print_cb` callback, with `cb_data`.
 *
 * NOTE: Called (possibly) outside of partition's timeslot, so
 * 'current_partition' doesn't correspond to given partition.
 */
typedef void (*pok_part_thr_info_fn_t)(pok_partition_t* part, int32_t index,
    void* private, print_cb_t print_cb, void* cb_data);

/**
 * @typename `pok_part_thr_regs_fn_t`
 *
 * @description
 * Returns the interrupt context of the non-current thread of index `index` of
 * the partition `part` (get_thread_registers of pok_part_sched_ops_t), with
 * `private` from pok_part_thr_at_fn_t. It is used for extract registers.
 *
 * Returning NULL means that thread has no registers assosiated with it.
 *
 * TODO: naming is bad.
 */
typedef interrupt_frame* (*pok_part_thr_regs_fn_t)(
    pok_partition_t* part, int32_t index, void* private);

/**
 * @typename `pok_part_thr_fp_fn_t`
 *
 * @description
 * Returns the store of the floating point registers of the thread of index
 * `index` of the partition `part` (get_thread_fp_store of
 * pok_part_sched_ops_t), with `private` from pok_part_thr_at_fn_t.
 *
 * Returning NULL (or having no such callback) means that the thread has no
 * floating point registers.
 */
typedef jet_fp_store_t* (*pok_part_thr_fp_fn_t)(
    pok_partition_t* part, int32_t index, void* private);
#endif /* POK_NEEDS_GDB */

/**
 * @typename `pok_part_sched_ops_t`
 *
 * @description
 * Scheduling operations specific for given partition.
 */
typedef struct {
    /** Called when some async event about partition occures. */
    pok_part_event_fn_t on_event;

#ifdef POK_NEEDS_GDB
    /** Return number of threads for given partition. */
    pok_part_nthreads_fn_t get_number_of_threads;
    /** Return index of current thread within partition. */
    pok_part_cur_thr_fn_t get_current_thread_index;
    /** Return 0 if thread with given index exists. */
    pok_part_thr_at_fn_t get_thread_at_index;
    /** Get information about thread with given index. */
    pok_part_thr_info_fn_t get_thread_info;
    /** Get interrupt context for non-current thread. */
    pok_part_thr_regs_fn_t get_thread_registers;
    /** Get the store of the floating point registers of the thread. */
    pok_part_thr_fp_fn_t get_thread_fp_store;
#endif /* POK_NEEDS_GDB */
} pok_part_sched_ops_t;

/**
 * @typename `pok_part_start_fn_t`
 *
 * @description
 * The entry-point for the partition's thread (start of pok_part_ops_t),
 * without parameters or result. Called when partition is (re)started.
 *
 * Local preemption is disabled.
 */
typedef void (*pok_part_start_fn_t)(void);

/**
 * @typename `pok_part_error_fn_t`
 *
 * @description
 * Processes a sync error related to given partition (process_partition_error
 * of pok_part_ops_t); returns nothing. Its parameters: the state of the
 * system when the error occurred (`partition_state`), the error
 * (`error_id`), the previous local preemption state
 * (`state_byte_preempt_local`) and the address of the error
 * (`failed_address`).
 *
 * Local preemption is disabled.
 */
typedef void (*pok_part_error_fn_t)(pok_system_state_t partition_state,
    pok_error_id_t error_id,
    pok_bool_t state_byte_preempt_local,
    void* failed_address);

/**
 * @typename `pok_part_ops_t`
 *
 * @description
 * The operations of a kind of partition: its start and its handling of errors.
 */
typedef struct {
    /** Called when partition is (re)started. Cannot be NULL. */
    pok_part_start_fn_t start;
    /** Process sync error related to given partition. Cannot be NULL. */
    pok_part_error_fn_t process_partition_error;
} pok_part_ops_t;

/**
 * @typename `pok_part_generation_t`
 *
 * @description
 * Non-zero number, which is incremented every time partition is started.
 */
typedef uint32_t pok_part_generation_t;

/**
 * @typename `jet_partition_event_type_e`
 *
 * @description
 * Type of outer event for partition.
 */
typedef enum {
    /**
     * Partition's timer is expired.
     */
    JET_PARTITION_EVENT_TYPE_TIMER,
    /**
     * Space is available for send message into the queuing port.
     */
    JET_PARTITION_EVENT_TYPE_PORT_SEND_AVAILABLE,
    /**
     * Message is available for receive it from the queuing port.
     */
    JET_PARTITION_EVENT_TYPE_PORT_RECEIVE_AVAILABLE,
} jet_partition_event_type_e;

/**
 * @typename `jet_partition_event_t`
 *
 * @description
 * Outer event for partition.
 */
typedef struct {
    /**
     * The identifier given with the event (e.g. the port's).
     */
    uint16_t handler_id;
    jet_partition_event_type_e event_type; /**< The type of the event. */
} jet_partition_event_t;

/**
 * @typename `pok_partition_t`
 *
 * @description
 * A partition: what the kernel needs to schedule and manage any partition (the
 * ARINC 653 partitions, pok_partition_arinc_t, start with it).
 */
struct pok_partition
{
#ifdef POK_NEEDS_MONITOR
    pok_bool_t               is_paused; /**< Partition paused or not. */
#endif

    /**
     * The scheduling operations of the partition.
     */
    const pok_part_sched_ops_t* part_sched_ops;
    /**
     * The operations of the partition.
     */
    const pok_part_ops_t* part_ops;

    /**
     * The ARINC partition whose base_part this is, or NULL for the other
     * partitions (idle, monitor, GDB). Set by pok_partition_arinc_init().
     */
    pok_partition_arinc_t* part_arinc;

    /**
     * Circular buffer with partition events.
     *
     * Allocated on initialization.
     */
    jet_partition_event_t* partition_events;

    /**
     * Maximum number of incoming events. Set in deployment.c.
     */
    uint16_t partition_event_max;
    /**
     * Index of the first event for receive.
     */
    uint16_t partition_event_begin;
    /**
     * Index after the last event for receive.
     */
    uint16_t partition_event_end;

    /**
     * If this field is positive, partition will receive event (with handler_id
     * = 0) when current time will be equal-or-more than this value.
     *
     * When timer event is fired, the field is reset to 0.
     *
     * DEV: There are 2 slots for timer events. So it is allowable to set this
     * field without preliminary reseting it and checking for events.
     */
    volatile pok_time_t timer;

    /**
     * Whether event has been fired.
     *
     * This field is set when event is added to empty queue.
     *
     * The field should be reset to 0 by partition before last event is
     * consumed.
     */
    pok_bool_t is_event;

    /**
     * Whether local preemption is disabled.
     *
     * Flag can be read, modified or cleared by the partition itself.
     *
     * Flag is set and checked by the global scheduler, when it need to call
     * partition's callbacks.
     */
    pok_bool_t preempt_local_disabled;

    /**
     * Its generation: incremented every time the partition is started.
     */
    pok_part_generation_t partition_generation;

    /**
     * Whether currently executed *user space* process is error handler.
     *
     * This field is used for determine system level in case when error is
     * catched via interrupt.
     *
     * Reseted to false when partition starts.
     */
    pok_bool_t               is_error_handler;

    const char               *name; /**< Name of the partition. */

    // Should be set in deployment.c.
    uint64_t                 period;         /* Period of the partition, unused
                                              * at this time */
    uint64_t                 duration;       /* Duration of the partition,
                                              * unused at this time */
    pok_partition_id_t       partition_id; /**< Its identifier. */

    /**
     * Kernel stack address which is used for enter into the partition.
     *
     * 0 value in this field means that partition needs to be (re)started.
     */
    jet_context_t*     sp;

    /**
     * Initial value of kernel stack (when it was allocated).
     *
     * Used for restarting partition.
     *
     * Set by particular partition's implementation.
     */
    jet_stack_t            initial_sp;

    /**
     * Identificator of (user) space, corresponded to given partition. Special
     * value 0xff means that no user space is used by this partition.
     *
     * Set in deployment.c
     *
     * Used by scheduler when it switch into partition.
     */
    uint8_t                  space_id;

    /**
     * Pointer to area for save floating point registers for current (user)
     * thread.
     *
     * Local scheduler should set this field before jumping or returning into
     * user space.
     */
    jet_fp_store_t*    fp_store_current;

#ifdef POK_NEEDS_GDB
    /**
     * Pointer to the user space registers array, stored for given partition.
     *
     * Set by global scheduler, used (may be cleared) by partition.
     */
    interrupt_frame* entry_sp_user;

    /**
     * Pointer to the registers array, stored for given partition when switched
     * off.
     *
     * Used only by global scheduler.
     */
    interrupt_frame* entry_sp;
#endif /* POK_NEEDS_GDB */

#ifdef POK_NEEDS_IO
  uint16_t		    io_min;             /* If the partition is allowed
                                                 * to perform I/O, the lower
                                                 * bound of the I/O */
  uint16_t		    io_max;             /* If the partition is allowed
                                                 * to perform I/O, the uppder
                                                 * bound of the I/O */
#endif

  pok_start_condition_e	    start_condition; /**< Why it was started. */

  /**
   * Set if partition is restarted by outside.
   *
   * Partition may clear this flag in its `start()` callback.
   */
  pok_bool_t restarted_externally;

  /**
   * Pointer to Multi partition HM selector.
   *
   * Bit's value 0 means module level error, 1 - partition level error.
   *
   * Set in deployment.c.
   */
  const pok_error_levels_t* multi_partition_hm_selector;
  /**
   * Pointer to Multi partition HM table.
   *
   * Set in deployment.c.
   */
  const pok_error_hm_module_t* multi_partition_hm_table;
};

/*
 * Pointer to the current partition.
 *
 * DEV: Readonly for all except scheduler-related stuff.
 */
extern pok_partition_t* current_partition;

/*
 * Add event to partition's queue and notify it if needed.
 *
 * Should be called with global preemption disabled.
 *
 * Note: May affect on scheduling, so preemption shouldn't be enabled using
 * __pok_preemption_enable().
 */
/** \cond */
void pok_partition_add_event(pok_partition_t* part,
    jet_partition_event_type_e event_type,
    uint16_t handler_id);

/*
 * Consume event from current partition's queue.
 *
 * Return TRUE on success, FALSE if the queue is empty.
 *
 * Should be called with local preemption disabled.
 */
pok_bool_t pok_partition_get_event(jet_partition_event_t* event);

/*
 * Set timer for given partition.
 * Setting to 0 means reseting.
 *
 * Event queue should be drained before setting timer once more.
 */
void pok_partition_set_timer(pok_partition_t* part,
    pok_time_t timer_new);

/* Initialize partition. */
void pok_partition_init(pok_partition_t* part);

/*
 * Execute given function for each partition.
 */
void for_each_partition(void (*f)(pok_partition_t* part));

/** \endcond */

/*
 * Ready-made scheduler operations for partition with single kernel thread.
 *
 * '.start' function for such partition shouldn't enable local preemption.
 */
extern const pok_part_sched_ops_t partition_sched_ops_kernel;

#endif /* POK_PARTITION_H */
