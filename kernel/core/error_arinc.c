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
 * This file also incorporates work covered by POK License.
 * Copyright (c) 2007-2009 POK team
 *
 * Modified by Kirill Brazhnikov, 2026-10-31: a partition restarted by
 * the health monitor gets start condition HM_PARTITION_RESTART; added
 * pok_error_report_application_message() for REPORT_APPLICATION_MESSAGE.
 *
 * Modified by Kirill Brazhnikov, 2026-10-31: RAISE_APPLICATION_ERROR
 * accepts an empty message (LENGTH 0) instead of silently ignoring the
 * request.
 *
 * Modified by Kirill Brazhnikov, 2026-11-01: added
 * pok_error_thread_configure() for CONFIGURE_ERROR_HANDLER.
 *
 * Modified by Kirill Brazhnikov, 2026-11-02: MISRA C:2025: __user/__kuser
 * are renamed JET_USER/JET_KUSER; braces around all if/else/loop bodies;
 * essentially Boolean conditions; U suffixes for unsigned constants;
 * discarded results are cast to void; internal linkage for file-local
 * definitions; deviations are documented where a guideline can't be
 * followed; process_error_partition() no longer calls itself; switch-
 * clauses fall through no more.
 *
 * Modified by Kirill Brazhnikov, 2026-11-03: MISRA C:2025:
 * error_thread_name is defined in the function that uses it; unused
 * parameters are cast to void; operands of mixed precedence are
 * parenthesized; deviations are documented where a guideline can't be
 * followed. Long lines are wrapped at 80 columns.
 *
 * Modified by Kirill Brazhnikov, 2026-11-08: Doxygen headers for the file
 * and its functions.
 *
 * Modified by Kirill Brazhnikov, 2026-11-11: the Doxygen comments that don't
 * document a function definition are plain comments, so that only the function
 * headers are in the function documentation.
 *
 * Modified by Kirill Brazhnikov, 2026-11-13: the macro is moved to
 * kernel/include/core/error_arinc.h.
 *
 * Modified by Kirill Brazhnikov, 2026-11-14: the enum types are named with _e:
 * pok_ret_e (was pok_ret_t).
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
 * Health monitoring of ARINC 653 partitions: the error handler thread, the
 * partition-level actions of the partition HM table (ignore, idle, cold or
 * warm restart), the errors the error handler gets (GET_ERROR_STATUS), and
 * application errors and messages.
 */

#include <config.h>

#ifdef POK_NEEDS_ERROR_HANDLING

#include <types.h>
#include <core/error_arinc.h>
#include <core/partition_arinc.h>
#include <core/debug.h>
#include <core/sched_arinc.h>

#include <assert.h>
#include <libc.h>
#include <core/uaccess.h>
#include "thread_internal.h"
#include <cons.h>

/*
 * MISRA C:2025 Rule 11.5 deviation (this file): the jet_user_to_kernel*()
 * helpers return the kernel address of user memory as a pointer to void,
 * which is converted to the type of the object the user passed.
 * MISRA C:2025 Rule 18.4 deviation (this file): thread and object IDs are
 * the index of an element, computed as the difference between its address
 * and the start of its array.
 */

/*
 * MISRA C:2025 Rule 15.1 deviation (this file): a forward goto to out
 * leaves the function through its common exit path.
 */

/**
 * @funcname `get_error_kind`
 *
 * @description
 * Returns the kind of an error, from the current partition's error map.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `error_id` | `pok_error_id_t` | [in] The error. |
 *
 * @return The error kind.
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
 * - \hlrid{HLR_ERROR_ARINC_GET_ERROR_KIND_01}: `get_error_kind()` shall give
 *   the kind of an error from the error map of the current partition.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_HM_007}
 *
 * @llr
 * - \llrid{LLR_ERROR_ARINC_GET_ERROR_KIND_01}: `get_error_kind()` shall return
 *   `error_kind` of the error's entry in the map.
 *   <br>Traces to:
 *   - \hlrref{HLR_ERROR_ARINC_GET_ERROR_KIND_01}
 *
 * @design
 * 1. Return
 *    `current_partition_arinc-> thread_error_info->map[error_id].error_kind`.
 *    \llrref{LLR_ERROR_ARINC_GET_ERROR_KIND_01}
 *
 * @calls
 * None.
 */
static inline pok_error_kind_t get_error_kind(pok_error_id_t error_id)
{
    return current_partition_arinc->
        thread_error_info->map[error_id].error_kind;
}

/**
 * @funcname `get_error_description`
 *
 * @description
 * Returns the description of an error, from the current partition's error map.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `error_id` | `pok_error_id_t` | [in] The error. |
 *
 * @return The description.
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
 * - \hlrid{HLR_ERROR_ARINC_GET_ERROR_DESCRIPTION_01}: `get_error_description()`
 *   shall give the description of an error from the error map of the current
 *   partition.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_HM_007}
 *   - \srqref{SRQ_JET_HM_011}
 *
 * @llr
 * - \llrid{LLR_ERROR_ARINC_GET_ERROR_DESCRIPTION_01}: `get_error_description()`
 *   shall return `error_description` of the error's entry in the map.
 *   <br>Traces to:
 *   - \hlrref{HLR_ERROR_ARINC_GET_ERROR_DESCRIPTION_01}
 *
 * @design
 * 1. Return `current_partition_arinc->`
 *    `thread_error_info->map[error_id].error_description`.
 *    \llrref{LLR_ERROR_ARINC_GET_ERROR_DESCRIPTION_01}
 *
 * @calls
 * None.
 */
static inline const char* get_error_description(pok_error_id_t error_id)
{
    return current_partition_arinc->
        thread_error_info->map[error_id].error_description;
}



/**
 * @funcname `pok_error_thread_create`
 *
 * @description
 * Creates the error handler thread of the current partition
 * (CREATE_ERROR_HANDLER, outside NORMAL mode), at priority 254.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `stack_size` | `uint32_t` | [in] The thread's stack size. |
 * | `entry` | `void * JET_USER` | [in] The thread's entry point. |
 *
 * @return POK_ERRNO_OK; POK_ERRNO_PARTITION_MODE in NORMAL mode;
 *         POK_ERRNO_EXISTS if there is one; POK_ERRNO_TOOMANY with no thread
 *         left; POK_ERRNO_PARAM for a bad entry; POK_ERRNO_UNAVAILABLE if the
 *         stack can't be allocated.
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `current_partition` | `pok_partition_t *` | The running partition |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `error_thread_name` | `static const char[14]` | The thread's name: "Error_Handler" |
 * | `part` | `pok_partition_arinc_t *` | The current partition |
 * | `t` | `pok_thread_t *` | The new thread |
 *
 * @hlr
 * - \hlrid{HLR_ERROR_ARINC_POK_ERROR_THREAD_CREATE_01}:
 *   `pok_error_thread_create()` shall create the one error handler process of
 *   the current partition, outside `NORMAL` mode, at its entry point with a
 *   stack size, with a priority above all the priorities of the application.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_HM_006}
 * - \hlrid{HLR_ERROR_ARINC_POK_ERROR_THREAD_CREATE_02}:
 *   `pok_error_thread_create()` shall refuse an entry point outside the code of
 *   the partition, and fail when no process slot or no stack is left.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_MEM_007}
 *   - \srqref{SRQ_JET_MEM_008}
 *
 * @llr
 * - \llrid{LLR_ERROR_ARINC_POK_ERROR_THREAD_CREATE_01}: If `mode` of the
 *   partition (`current_partition_arinc`) is `POK_PARTITION_MODE_NORMAL`,
 *   `pok_error_thread_create()` shall return `POK_ERRNO_PARTITION_MODE`.
 *   <br>Traces to:
 *   - \hlrref{HLR_ERROR_ARINC_POK_ERROR_THREAD_CREATE_01}
 * - \llrid{LLR_ERROR_ARINC_POK_ERROR_THREAD_CREATE_02}: If `mode` is not
 *   `POK_PARTITION_MODE_NORMAL` and `thread_error` of the partition is not
 *   NULL, `pok_error_thread_create()` shall return `POK_ERRNO_EXISTS`.
 *   <br>Traces to:
 *   - \hlrref{HLR_ERROR_ARINC_POK_ERROR_THREAD_CREATE_01}
 * - \llrid{LLR_ERROR_ARINC_POK_ERROR_THREAD_CREATE_03}: If `mode` is not
 *   `POK_PARTITION_MODE_NORMAL`, `thread_error` is NULL and `nthreads_used` is
 *   `nthreads`, `pok_error_thread_create()` shall return `POK_ERRNO_TOOMANY`.
 *   <br>Traces to:
 *   - \hlrref{HLR_ERROR_ARINC_POK_ERROR_THREAD_CREATE_02}
 * - \llrid{LLR_ERROR_ARINC_POK_ERROR_THREAD_CREATE_04}: If these checks pass
 *   and `jet_check_access_exec()` returns false for `entry`,
 *   `pok_error_thread_create()` shall return `POK_ERRNO_PARAM`.
 *   <br>Traces to:
 *   - \hlrref{HLR_ERROR_ARINC_POK_ERROR_THREAD_CREATE_02}
 * - \llrid{LLR_ERROR_ARINC_POK_ERROR_THREAD_CREATE_05}: If
 *   `jet_check_access_exec()` returns true for `entry`,
 *   `pok_error_thread_create()` shall set, in `threads[nthreads_used]`, `name`
 *   to `"Error_Handler"` (`strncpy()` with `MAX_NAME_LENGTH`), `entry` to
 *   `entry`, `base_priority` to `ERROR_THREAD_PRIORITY`, `period` and
 *   `time_capacity` to `POK_TIME_INFINITY`, `deadline` to `DEADLINE_SOFT` and
 *   `user_stack_size` to `stack_size`, and call `thread_create()` once with
 *   that thread.
 *   <br>Traces to:
 *   - \hlrref{HLR_ERROR_ARINC_POK_ERROR_THREAD_CREATE_01}
 * - \llrid{LLR_ERROR_ARINC_POK_ERROR_THREAD_CREATE_06}: If `thread_create()`
 *   returns false, `pok_error_thread_create()` shall return
 *   `POK_ERRNO_UNAVAILABLE` and leave `nthreads_used` and `thread_error` as
 *   they are.
 *   <br>Traces to:
 *   - \hlrref{HLR_ERROR_ARINC_POK_ERROR_THREAD_CREATE_02}
 * - \llrid{LLR_ERROR_ARINC_POK_ERROR_THREAD_CREATE_07}: If `thread_create()`
 *   returns true, `pok_error_thread_create()` shall set `error_thread_id` of
 *   the shared data (`kshd`) to `nthreads_used`, increment `nthreads_used`, set
 *   `thread_error` to the thread, and return `POK_ERRNO_OK`.
 *   <br>Traces to:
 *   - \hlrref{HLR_ERROR_ARINC_POK_ERROR_THREAD_CREATE_01}
 *
 * @design
 * 1. Set `part` to `current_partition_arinc`.
 *    \llrref{LLR_ERROR_ARINC_POK_ERROR_THREAD_CREATE_01},
 *    \llrref{LLR_ERROR_ARINC_POK_ERROR_THREAD_CREATE_02},
 *    \llrref{LLR_ERROR_ARINC_POK_ERROR_THREAD_CREATE_03},
 *    \llrref{LLR_ERROR_ARINC_POK_ERROR_THREAD_CREATE_04},
 *    \llrref{LLR_ERROR_ARINC_POK_ERROR_THREAD_CREATE_05},
 *    \llrref{LLR_ERROR_ARINC_POK_ERROR_THREAD_CREATE_06},
 *    \llrref{LLR_ERROR_ARINC_POK_ERROR_THREAD_CREATE_07}
 * 2. If `part->mode` is equal to `POK_PARTITION_MODE_NORMAL`:
 *    \llrref{LLR_ERROR_ARINC_POK_ERROR_THREAD_CREATE_01}
 *    1. Return `POK_ERRNO_PARTITION_MODE`.
 * 3. If `part->thread_error` is not NULL:
 *    \llrref{LLR_ERROR_ARINC_POK_ERROR_THREAD_CREATE_02}
 *    1. Return `POK_ERRNO_EXISTS`.
 * 4. If `part->nthreads_used` is equal to `part->nthreads`:
 *    \llrref{LLR_ERROR_ARINC_POK_ERROR_THREAD_CREATE_03}
 *    1. Return `POK_ERRNO_TOOMANY`.
 * 5. If `jet_check_access_exec(entry)` is false:
 *    \llrref{LLR_ERROR_ARINC_POK_ERROR_THREAD_CREATE_04}
 *    1. Return `POK_ERRNO_PARAM`.
 * 6. Set `t` to the address of `part->threads[part->nthreads_used]`.
 *    \llrref{LLR_ERROR_ARINC_POK_ERROR_THREAD_CREATE_05}
 * 7. Call `strncpy()` with the arguments `t->name`, `error_thread_name` and
 *    `MAX_NAME_LENGTH`. \llrref{LLR_ERROR_ARINC_POK_ERROR_THREAD_CREATE_05}
 * 8. Set `t->entry` to `entry`.
 *    \llrref{LLR_ERROR_ARINC_POK_ERROR_THREAD_CREATE_05}
 * 9. Set `t->base_priority` to `ERROR_THREAD_PRIORITY`.
 *    \llrref{LLR_ERROR_ARINC_POK_ERROR_THREAD_CREATE_05}
 * 10. Set `t->period` to `POK_TIME_INFINITY`.
 *     \llrref{LLR_ERROR_ARINC_POK_ERROR_THREAD_CREATE_05}
 * 11. Set `t->time_capacity` to `POK_TIME_INFINITY`.
 *     \llrref{LLR_ERROR_ARINC_POK_ERROR_THREAD_CREATE_05}
 * 12. Set `t->deadline` to `DEADLINE_SOFT`.
 *     \llrref{LLR_ERROR_ARINC_POK_ERROR_THREAD_CREATE_05}
 * 13. Set `t->user_stack_size` to `stack_size`.
 *     \llrref{LLR_ERROR_ARINC_POK_ERROR_THREAD_CREATE_05}
 * 14. If `thread_create(t)` is false:
 *     \llrref{LLR_ERROR_ARINC_POK_ERROR_THREAD_CREATE_06},
 *     \llrref{LLR_ERROR_ARINC_POK_ERROR_THREAD_CREATE_07}
 *     1. Return `POK_ERRNO_UNAVAILABLE`.
 * 15. Set `part->kshd->error_thread_id` to `part->nthreads_used`.
 *     \llrref{LLR_ERROR_ARINC_POK_ERROR_THREAD_CREATE_07}
 * 16. Increment `part->nthreads_used`.
 *     \llrref{LLR_ERROR_ARINC_POK_ERROR_THREAD_CREATE_07}
 * 17. Set `part->thread_error` to `t`.
 *     \llrref{LLR_ERROR_ARINC_POK_ERROR_THREAD_CREATE_07}
 * 18. Return `POK_ERRNO_OK`.
 *     \llrref{LLR_ERROR_ARINC_POK_ERROR_THREAD_CREATE_07}
 *
 * @calls
 * - `jet_check_access_exec()`
 * - `strncpy()`
 * - `thread_create()`
 */
pok_ret_e pok_error_thread_create (uint32_t stack_size, void* JET_USER entry)
{
    static const char error_thread_name[] = "Error_Handler";

    pok_thread_t* t;
    pok_partition_arinc_t* part = current_partition_arinc;

    /*
     * We can create a thread only if the partition is in INIT mode
     */
    if (part->mode == POK_PARTITION_MODE_NORMAL) {
        return POK_ERRNO_PARTITION_MODE;
    }

    if(part->thread_error != NULL) {
        return POK_ERRNO_EXISTS;
    }

    if (part->nthreads_used == part->nthreads) {
        return POK_ERRNO_TOOMANY;
    }

    // do at least basic check of entry point
    if (!jet_check_access_exec(entry)) {
        return POK_ERRNO_PARAM;
    }


    t = &part->threads[part->nthreads_used];

    (void) strncpy(t->name, error_thread_name, MAX_NAME_LENGTH);
    t->entry = entry;
    t->base_priority = ERROR_THREAD_PRIORITY;
    t->period = POK_TIME_INFINITY;
    t->time_capacity = POK_TIME_INFINITY; // TODO: support deadline for error handler
    t->deadline = DEADLINE_SOFT;
    t->user_stack_size = stack_size;

    if(!thread_create(t)) {
       return POK_ERRNO_UNAVAILABLE;
    }

    // Update kernel shared data
    part->kshd->error_thread_id = part->nthreads_used;

    part->nthreads_used++;

    part->thread_error = t;

    return POK_ERRNO_OK;
}

/**
 * @funcname `pok_error_thread_configure`
 *
 * @description
 * Configures the error handler (CONFIGURE_ERROR_HANDLER, outside NORMAL mode):
 * its concurrency control and processor core.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `concurrency_control` | `uint32_t` | [in] PROCESSES_PAUSE or PROCESSES_SCHEDULED. |
 * | `processor_core_id` | `int32_t` | [in] The core: no preference or 0 (one core). |
 *
 * @return POK_ERRNO_OK; POK_ERRNO_UNAVAILABLE without an error handler;
 *         POK_ERRNO_EINVAL for a bad concurrency control; POK_ERRNO_PARAM for a
 *         bad core; POK_ERRNO_PARTITION_MODE in NORMAL mode.
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
 * - \hlrid{HLR_ERROR_ARINC_POK_ERROR_THREAD_CONFIGURE_01}:
 *   `pok_error_thread_configure()` shall set the concurrency control (processes
 *   paused or scheduled) and the processor core of the error handler of the
 *   current partition, outside `NORMAL` mode.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_HM_006}
 *
 * @llr
 * - \llrid{LLR_ERROR_ARINC_POK_ERROR_THREAD_CONFIGURE_01}: If `thread_error` of
 *   the partition is NULL, `pok_error_thread_configure()` shall return
 *   `POK_ERRNO_UNAVAILABLE`.
 *   <br>Traces to:
 *   - \hlrref{HLR_ERROR_ARINC_POK_ERROR_THREAD_CONFIGURE_01}
 * - \llrid{LLR_ERROR_ARINC_POK_ERROR_THREAD_CONFIGURE_02}: If `thread_error` is
 *   not NULL and `concurrency_control` is neither
 *   `POK_ERROR_HANDLER_PROCESSES_PAUSE` nor
 *   `POK_ERROR_HANDLER_PROCESSES_SCHEDULED`, `pok_error_thread_configure()`
 *   shall return `POK_ERRNO_EINVAL`.
 *   <br>Traces to:
 *   - \hlrref{HLR_ERROR_ARINC_POK_ERROR_THREAD_CONFIGURE_01}
 * - \llrid{LLR_ERROR_ARINC_POK_ERROR_THREAD_CONFIGURE_03}: If these checks pass
 *   and `processor_core_id` is neither `POK_CORE_AFFINITY_NO_PREFERENCE` nor 0,
 *   `pok_error_thread_configure()` shall return `POK_ERRNO_PARAM`.
 *   <br>Traces to:
 *   - \hlrref{HLR_ERROR_ARINC_POK_ERROR_THREAD_CONFIGURE_01}
 * - \llrid{LLR_ERROR_ARINC_POK_ERROR_THREAD_CONFIGURE_04}: If these checks pass
 *   and `mode` of the partition is `POK_PARTITION_MODE_NORMAL`,
 *   `pok_error_thread_configure()` shall return `POK_ERRNO_PARTITION_MODE`.
 *   <br>Traces to:
 *   - \hlrref{HLR_ERROR_ARINC_POK_ERROR_THREAD_CONFIGURE_01}
 * - \llrid{LLR_ERROR_ARINC_POK_ERROR_THREAD_CONFIGURE_05}: If these checks pass
 *   and `mode` is not `POK_PARTITION_MODE_NORMAL`,
 *   `pok_error_thread_configure()` shall set `error_handler_concurrency` of the
 *   partition to `concurrency_control` and `error_handler_core` to
 *   `processor_core_id`, and return `POK_ERRNO_OK`.
 *   <br>Traces to:
 *   - \hlrref{HLR_ERROR_ARINC_POK_ERROR_THREAD_CONFIGURE_01}
 *
 * @design
 * 1. Set `part` to `current_partition_arinc`.
 *    \llrref{LLR_ERROR_ARINC_POK_ERROR_THREAD_CONFIGURE_01},
 *    \llrref{LLR_ERROR_ARINC_POK_ERROR_THREAD_CONFIGURE_02},
 *    \llrref{LLR_ERROR_ARINC_POK_ERROR_THREAD_CONFIGURE_03},
 *    \llrref{LLR_ERROR_ARINC_POK_ERROR_THREAD_CONFIGURE_04},
 *    \llrref{LLR_ERROR_ARINC_POK_ERROR_THREAD_CONFIGURE_05}
 * 2. If `part->thread_error` is NULL:
 *    \llrref{LLR_ERROR_ARINC_POK_ERROR_THREAD_CONFIGURE_01}
 *    1. Return `POK_ERRNO_UNAVAILABLE`.
 * 3. If `concurrency_control` is not equal to
 *    `POK_ERROR_HANDLER_PROCESSES_PAUSE` and `concurrency_control` is not equal
 *    to `POK_ERROR_HANDLER_PROCESSES_SCHEDULED`:
 *    \llrref{LLR_ERROR_ARINC_POK_ERROR_THREAD_CONFIGURE_02}
 *    1. Return `POK_ERRNO_EINVAL`.
 * 4. If `processor_core_id` is not equal to `POK_CORE_AFFINITY_NO_PREFERENCE`
 *    and `processor_core_id` is not equal to `0`:
 *    \llrref{LLR_ERROR_ARINC_POK_ERROR_THREAD_CONFIGURE_03}
 *    1. Return `POK_ERRNO_PARAM`.
 * 5. If `part->mode` is equal to `POK_PARTITION_MODE_NORMAL`:
 *    \llrref{LLR_ERROR_ARINC_POK_ERROR_THREAD_CONFIGURE_04}
 *    1. Return `POK_ERRNO_PARTITION_MODE`.
 * 6. Set `part->error_handler_concurrency` to `concurrency_control`.
 *    \llrref{LLR_ERROR_ARINC_POK_ERROR_THREAD_CONFIGURE_05}
 * 7. Set `part->error_handler_core` to `processor_core_id`.
 *    \llrref{LLR_ERROR_ARINC_POK_ERROR_THREAD_CONFIGURE_05}
 * 8. Return `POK_ERRNO_OK`.
 *    \llrref{LLR_ERROR_ARINC_POK_ERROR_THREAD_CONFIGURE_05}
 *
 * @calls
 * None.
 */
pok_ret_e pok_error_thread_configure (uint32_t concurrency_control,
                                      int32_t processor_core_id)
{
    pok_partition_arinc_t* part = current_partition_arinc;

    // Checks follow the order of the error conditions in ARINC 653.
    if (part->thread_error == NULL) {
        return POK_ERRNO_UNAVAILABLE;
    }

    if ((concurrency_control != POK_ERROR_HANDLER_PROCESSES_PAUSE)
        && (concurrency_control != POK_ERROR_HANDLER_PROCESSES_SCHEDULED)) {
        return POK_ERRNO_EINVAL;
    }

    // The partition has one processor core, with partition-relative ID 0.
    if ((processor_core_id != POK_CORE_AFFINITY_NO_PREFERENCE)
        && (processor_core_id != 0)) {
        return POK_ERRNO_PARAM;
    }

    if (part->mode == POK_PARTITION_MODE_NORMAL) {
        return POK_ERRNO_PARTITION_MODE;
    }

    /*
     * On a single core both concurrency controls behave the same: the
     * error handler has the highest priority and isn't preempted, so no
     * other process of the partition runs until it finishes.
     */
    part->error_handler_concurrency = concurrency_control;
    part->error_handler_core = processor_core_id;

    return POK_ERRNO_OK;
}

/**
 * @funcname `take_fixed_action`
 *
 * @description
 * Takes a partition-level HM action: a restart is reported first (with the
 * error's description) and recorded as the start condition; IGNORE returns,
 * IDLE stops the partition, COLD_START and WARM_START restart it.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `action` | `pok_error_action_t` | [in] The action. |
 * | `error_id` | `pok_error_id_t` | [in] The error. |
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `current_partition` | `pok_partition_t *` | The running partition |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `msg` | `const char *` | The error's description |
 * | `part` | `pok_partition_arinc_t *` | The current partition |
 *
 * @hlr
 * - \hlrid{HLR_ERROR_ARINC_TAKE_FIXED_ACTION_01}: `take_fixed_action()` shall
 *   take a partition action: ignore the error, stop the partition (`IDLE`), or
 *   restart it in `COLD_START` or `WARM_START` mode, reporting a restart on the
 *   console with the description of the error and recording it as the reason of
 *   the start.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_HM_004}
 *   - \srqref{SRQ_JET_HM_011}
 *
 * @llr
 * - \llrid{LLR_ERROR_ARINC_TAKE_FIXED_ACTION_01}: If `action` is
 *   `POK_ERROR_ACTION_COLD_START` or `POK_ERROR_ACTION_WARM_START`,
 *   `take_fixed_action()` shall first call `pok_cons_write()` with `"[KERNEL]
 *   Restarting partition "` and 30, with `name` of the base partition and its
 *   length (`strlen()`), with `" due to: "` and 9, with the result of
 *   `get_error_description()` for `error_id` and its length, and with `"\n"`
 *   and 1, and then set `start_condition` of the base partition to
 *   `POK_START_CONDITION_HM_PARTITION_RESTART`.
 *   <br>Traces to:
 *   - \hlrref{HLR_ERROR_ARINC_TAKE_FIXED_ACTION_01}
 * - \llrid{LLR_ERROR_ARINC_TAKE_FIXED_ACTION_02}: If `action` is
 *   `POK_ERROR_ACTION_IGNORE`, `take_fixed_action()` shall return without
 *   writing.
 *   <br>Traces to:
 *   - \hlrref{HLR_ERROR_ARINC_TAKE_FIXED_ACTION_01}
 * - \llrid{LLR_ERROR_ARINC_TAKE_FIXED_ACTION_03}: If `action` is
 *   `POK_ERROR_ACTION_IDLE`, `take_fixed_action()` shall call
 *   `pok_partition_arinc_idle()` and then, if it returns,
 *   `pok_assertion_fail()` (`unreachable()`), without writing.
 *   <br>Traces to:
 *   - \hlrref{HLR_ERROR_ARINC_TAKE_FIXED_ACTION_01}
 * - \llrid{LLR_ERROR_ARINC_TAKE_FIXED_ACTION_04}: If `action` is
 *   `POK_ERROR_ACTION_COLD_START`, `take_fixed_action()` shall call
 *   `pok_partition_arinc_reset()` with `POK_PARTITION_MODE_INIT_COLD` and then,
 *   if it returns, `pok_assertion_fail()` (`unreachable()`).
 *   <br>Traces to:
 *   - \hlrref{HLR_ERROR_ARINC_TAKE_FIXED_ACTION_01}
 * - \llrid{LLR_ERROR_ARINC_TAKE_FIXED_ACTION_05}: If `action` is
 *   `POK_ERROR_ACTION_WARM_START`, `take_fixed_action()` shall call
 *   `pok_partition_arinc_reset()` with `POK_PARTITION_MODE_INIT_WARM` and then,
 *   if it returns, `pok_assertion_fail()` (`unreachable()`).
 *   <br>Traces to:
 *   - \hlrref{HLR_ERROR_ARINC_TAKE_FIXED_ACTION_01}
 * - \llrid{LLR_ERROR_ARINC_TAKE_FIXED_ACTION_06}: If `action` is none of
 *   `POK_ERROR_ACTION_IGNORE`, `POK_ERROR_ACTION_IDLE`,
 *   `POK_ERROR_ACTION_COLD_START` and `POK_ERROR_ACTION_WARM_START`,
 *   `take_fixed_action()` shall call `pok_assertion_fail()` (`unreachable()`)
 *   without writing.
 *   <br>Traces to:
 *   - \hlrref{HLR_ERROR_ARINC_TAKE_FIXED_ACTION_01}
 *
 * @design
 * 1. If `action` is equal to `POK_ERROR_ACTION_COLD_START` or `action` is equal
 *    to `POK_ERROR_ACTION_WARM_START`:
 *    \llrref{LLR_ERROR_ARINC_TAKE_FIXED_ACTION_01}
 *    1. Set `part` to `current_partition_arinc`.
 *    2. Call `pok_cons_write()` with the arguments
 *       `"[KERNEL] Restarting partition "` and `30`.
 *    3. Call `pok_cons_write()` with the arguments `part->base_part.name` and
 *       `strlen(part->base_part.name)`.
 *    4. Call `pok_cons_write()` with the arguments `" due to: "` and `9`.
 *    5. Call `get_error_description()` with the argument `error_id`, and store
 *       the result in `msg`.
 *    6. Call `pok_cons_write()` with the arguments `msg` and `strlen(msg)`.
 *    7. Call `pok_cons_write()` with the arguments `"\n"` and `1`.
 *    8. Set `part->base_part.start_condition` to
 *       `POK_START_CONDITION_HM_PARTITION_RESTART`.
 * 2. Depending on `action`: \llrref{LLR_ERROR_ARINC_TAKE_FIXED_ACTION_02},
 *    \llrref{LLR_ERROR_ARINC_TAKE_FIXED_ACTION_03},
 *    \llrref{LLR_ERROR_ARINC_TAKE_FIXED_ACTION_04},
 *    \llrref{LLR_ERROR_ARINC_TAKE_FIXED_ACTION_05},
 *    \llrref{LLR_ERROR_ARINC_TAKE_FIXED_ACTION_06}
 *    1. Case `POK_ERROR_ACTION_IGNORE`:
 *       \llrref{LLR_ERROR_ARINC_TAKE_FIXED_ACTION_02}
 *       1. Return.
 *    2. Case `POK_ERROR_ACTION_IDLE`:
 *       \llrref{LLR_ERROR_ARINC_TAKE_FIXED_ACTION_03}
 *       1. Call `pok_partition_arinc_idle()`.
 *       2. Use the macro `unreachable()`.
 *       3. Leave the switch.
 *    3. Case `POK_ERROR_ACTION_COLD_START`:
 *       \llrref{LLR_ERROR_ARINC_TAKE_FIXED_ACTION_04}
 *       1. Call `pok_partition_arinc_reset()` with the argument
 *          `POK_PARTITION_MODE_INIT_COLD`.
 *       2. Use the macro `unreachable()`.
 *       3. Leave the switch.
 *    4. Case `POK_ERROR_ACTION_WARM_START`:
 *       \llrref{LLR_ERROR_ARINC_TAKE_FIXED_ACTION_05}
 *       1. Call `pok_partition_arinc_reset()` with the argument
 *          `POK_PARTITION_MODE_INIT_WARM`.
 *       2. Use the macro `unreachable()`.
 *       3. Leave the switch.
 *    5. Otherwise (default): \llrref{LLR_ERROR_ARINC_TAKE_FIXED_ACTION_06}
 *       1. Use the macro `unreachable()`.
 *
 * @calls
 * - `pok_cons_write()`
 * - `strlen()`
 * - `get_error_description()`
 * - `pok_partition_arinc_idle()`
 * - `pok_assertion_fail()`
 * - `pok_partition_arinc_reset()`
 */
static void take_fixed_action(pok_error_action_t action, pok_error_id_t error_id)
{
    /*************Modified JB****************/
    //If the partition restarts, write the reason why
    if((action == POK_ERROR_ACTION_COLD_START)
       || (action == POK_ERROR_ACTION_WARM_START)){
        pok_partition_arinc_t* part = current_partition_arinc;
        (void) pok_cons_write("[KERNEL] Restarting partition ", 30);
        (void) pok_cons_write(part->base_part.name,
                              strlen(part->base_part.name));
        (void) pok_cons_write(" due to: ", 9);
        const char *msg = get_error_description(error_id);
        (void) pok_cons_write(msg, strlen(msg));
        (void) pok_cons_write("\n", 1);

        // Reported by GET_PARTITION_STATUS after the restart.
        part->base_part.start_condition = POK_START_CONDITION_HM_PARTITION_RESTART;
    }
    /****************************************/

    switch (action) {
        case POK_ERROR_ACTION_IGNORE:
            // TODO not all kinds of errors can be ignored just like that
            return;
        case POK_ERROR_ACTION_IDLE:
            pok_partition_arinc_idle();
            unreachable();
            break;
        case POK_ERROR_ACTION_COLD_START:
            pok_partition_arinc_reset(POK_PARTITION_MODE_INIT_COLD);
            unreachable(); // pok_partition_arinc_reset() doesn't return.
            break;
        case POK_ERROR_ACTION_WARM_START:
            // TODO: Instead of assert() this should be another error or other actions.
            assert(current_partition_arinc->mode != POK_PARTITION_MODE_INIT_COLD);

            pok_partition_arinc_reset(POK_PARTITION_MODE_INIT_WARM);
            unreachable();
            break;
        default:
            unreachable();
    }
}

/**
 * @funcname `take_partition_action`
 *
 * @description
 * Takes the action of the current partition's HM table for an error in a system
 * state.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `system_state` | `pok_system_state_t` | [in] The system state. |
 * | `error_id` | `pok_error_id_t` | [in] The error. |
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `current_partition` | `pok_partition_t *` | The running partition |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `action` | `pok_error_action_t` | The action |
 *
 * @hlr
 * - \hlrid{HLR_ERROR_ARINC_TAKE_PARTITION_ACTION_01}: `take_partition_action()`
 *   shall take the action of the health monitoring table of the current
 *   partition for an error in a system state.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_HM_004}
 *   - \srqref{SRQ_JET_CFG_006}
 *   - \srqref{SRQ_JET_CFG_012}
 *
 * @llr
 * - \llrid{LLR_ERROR_ARINC_TAKE_PARTITION_ACTION_01}: `take_partition_action()`
 *   shall call `take_fixed_action()` with the table's action for the state and
 *   the error.
 *   <br>Traces to:
 *   - \hlrref{HLR_ERROR_ARINC_TAKE_PARTITION_ACTION_01}
 *
 * @design
 * 1. Set `action` to `current_partition_arinc->partition_hm_table`
 *    `->actions[system_state][error_id]`.
 *    \llrref{LLR_ERROR_ARINC_TAKE_PARTITION_ACTION_01}
 * 2. Call `take_fixed_action()` with the arguments `action` and `error_id`.
 *    \llrref{LLR_ERROR_ARINC_TAKE_PARTITION_ACTION_01}
 *
 * @calls
 * - `take_fixed_action()`
 */
static void take_partition_action(pok_system_state_t system_state,
    pok_error_id_t error_id)
{
    pok_error_action_t action = current_partition_arinc->partition_hm_table
        ->actions[system_state][error_id];

    take_fixed_action(action, error_id);
}

/**
 * @funcname `process_error_partition`
 *
 * @description
 * Handles an error at the partition level: a process-level error in user code
 * (NORMAL mode, with an error handler) is left to the error handler; otherwise
 * the partition HM table's action is taken (and repeated while unrecoverable
 * threads remain).
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `system_state` | `pok_system_state_t` | [in] USER, ERROR_HANDLER or OS_PART. |
 * | `error_id` | `pok_error_id_t` | [in] The error. |
 *
 * @return FALSE if the error handler must handle it, TRUE if it was handled.
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
 * - \hlrid{HLR_ERROR_ARINC_PROCESS_ERROR_PARTITION_01}:
 *   `process_error_partition()` shall leave an error of a user process to the
 *   error handler when the partition selector selects the process level, the
 *   partition is in `NORMAL` mode and has an error handler.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_HM_005}
 * - \hlrid{HLR_ERROR_ARINC_PROCESS_ERROR_PARTITION_02}:
 *   `process_error_partition()` shall otherwise take the action of the
 *   partition's table, and, while processes with unrecoverable errors remain,
 *   take the action for an unhandled interrupt.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_HM_004}
 *   - \srqref{SRQ_JET_HM_009}
 *   - \srqref{SRQ_JET_HM_012}
 *
 * @llr
 * - \llrid{LLR_ERROR_ARINC_PROCESS_ERROR_PARTITION_01}: If `system_state` is
 *   `POK_SYSTEM_STATE_USER` and, with `POK_NEEDS_ERROR_HANDLING`,
 *   `pok_error_level_select()` returns true for `partition_hm_selector` of the
 *   partition, `system_state` and `error_id`, `mode` is
 *   `POK_PARTITION_MODE_NORMAL` and `thread_error` is not NULL,
 *   `process_error_partition()` shall return `FALSE` without calling
 *   `take_partition_action()`.
 *   <br>Traces to:
 *   - \hlrref{HLR_ERROR_ARINC_PROCESS_ERROR_PARTITION_01}
 * - \llrid{LLR_ERROR_ARINC_PROCESS_ERROR_PARTITION_02}: If `system_state` is
 *   none of `POK_SYSTEM_STATE_USER`, `POK_SYSTEM_STATE_ERROR_HANDLER` (with
 *   `POK_NEEDS_ERROR_HANDLING`) and `POK_SYSTEM_STATE_OS_PART`,
 *   `process_error_partition()` shall call `pok_fatal()` with `"Unexpected
 *   partition state while error"`.
 *   <br>Traces to:
 *   - \hlrref{HLR_ERROR_ARINC_PROCESS_ERROR_PARTITION_02}
 * - \llrid{LLR_ERROR_ARINC_PROCESS_ERROR_PARTITION_03}: If `system_state` is
 *   `POK_SYSTEM_STATE_USER` and `pok_error_level_select()` returns false, or
 *   `mode` is not `POK_PARTITION_MODE_NORMAL`, or `thread_error` is NULL, or if
 *   `system_state` is `POK_SYSTEM_STATE_ERROR_HANDLER` or
 *   `POK_SYSTEM_STATE_OS_PART`, `process_error_partition()` shall call
 *   `take_partition_action()` once with `system_state` and `error_id`.
 *   <br>Traces to:
 *   - \hlrref{HLR_ERROR_ARINC_PROCESS_ERROR_PARTITION_02}
 * - \llrid{LLR_ERROR_ARINC_PROCESS_ERROR_PARTITION_04}: If
 *   `nthreads_unrecoverable` of the partition is then not 0,
 *   `process_error_partition()` shall set it to 0 and call
 *   `take_partition_action()` with `POK_SYSTEM_STATE_OS_PART` and
 *   `POK_ERROR_ID_UNHANDLED_INT` again and again, without returning.
 *   <br>Traces to:
 *   - \hlrref{HLR_ERROR_ARINC_PROCESS_ERROR_PARTITION_02}
 * - \llrid{LLR_ERROR_ARINC_PROCESS_ERROR_PARTITION_05}: If
 *   `nthreads_unrecoverable` of the partition is then 0,
 *   `process_error_partition()` shall return `TRUE`.
 *   <br>Traces to:
 *   - \hlrref{HLR_ERROR_ARINC_PROCESS_ERROR_PARTITION_02}
 *
 * @design
 * 1. Set `part` to `current_partition_arinc`.
 *    \llrref{LLR_ERROR_ARINC_PROCESS_ERROR_PARTITION_01},
 *    \llrref{LLR_ERROR_ARINC_PROCESS_ERROR_PARTITION_03},
 *    \llrref{LLR_ERROR_ARINC_PROCESS_ERROR_PARTITION_04},
 *    \llrref{LLR_ERROR_ARINC_PROCESS_ERROR_PARTITION_05}
 * 2. Depending on `system_state`:
 *    \llrref{LLR_ERROR_ARINC_PROCESS_ERROR_PARTITION_01},
 *    \llrref{LLR_ERROR_ARINC_PROCESS_ERROR_PARTITION_02},
 *    \llrref{LLR_ERROR_ARINC_PROCESS_ERROR_PARTITION_03}
 *    1. Case `POK_SYSTEM_STATE_USER`:
 *       \llrref{LLR_ERROR_ARINC_PROCESS_ERROR_PARTITION_01},
 *       \llrref{LLR_ERROR_ARINC_PROCESS_ERROR_PARTITION_03}
 *       1. If
 *          `pok_error_level_select(part->partition_hm_selector, system_state,`
 *          `error_id) && (part->mode == POK_PARTITION_MODE_NORMAL) &&`
 *          `(part->thread_error != NULL)` holds:
 *          \llrref{LLR_ERROR_ARINC_PROCESS_ERROR_PARTITION_01}
 *          1. Return `FALSE`.
 *       2. Leave the switch.
 *    2. Case `POK_SYSTEM_STATE_ERROR_HANDLER`:
 *       \llrref{LLR_ERROR_ARINC_PROCESS_ERROR_PARTITION_03}
 *       1. Leave the switch.
 *    3. Case `POK_SYSTEM_STATE_OS_PART`:
 *       \llrref{LLR_ERROR_ARINC_PROCESS_ERROR_PARTITION_03}
 *       1. Leave the switch.
 *    4. Otherwise (default):
 *       \llrref{LLR_ERROR_ARINC_PROCESS_ERROR_PARTITION_02}
 *       1. Call `pok_fatal()` with the argument
 *          `"Unexpected partition state while error"`.
 * 3. Call `take_partition_action()` with the arguments `system_state` and
 *    `error_id`. \llrref{LLR_ERROR_ARINC_PROCESS_ERROR_PARTITION_03}
 * 4. If `part->nthreads_unrecoverable` is not equal to `0U`:
 *    \llrref{LLR_ERROR_ARINC_PROCESS_ERROR_PARTITION_04},
 *    \llrref{LLR_ERROR_ARINC_PROCESS_ERROR_PARTITION_05}
 *    1. Set `part->nthreads_unrecoverable` to `0`.
 *       \llrref{LLR_ERROR_ARINC_PROCESS_ERROR_PARTITION_04}
 *    2. While `true` is true:
 *       \llrref{LLR_ERROR_ARINC_PROCESS_ERROR_PARTITION_04}
 *       1. Call `take_partition_action()` with the arguments
 *          `POK_SYSTEM_STATE_OS_PART` and `POK_ERROR_ID_UNHANDLED_INT`.
 * 5. Return `TRUE`. \llrref{LLR_ERROR_ARINC_PROCESS_ERROR_PARTITION_05}
 *
 * @calls
 * - `pok_error_level_select()`
 * - `pok_fatal()`
 * - `take_partition_action()`
 */
static pok_bool_t process_error_partition(pok_system_state_t system_state,
    pok_error_id_t error_id)
{
    pok_partition_arinc_t* part = current_partition_arinc;

    switch(system_state)
    {
    case POK_SYSTEM_STATE_USER:
#ifdef POK_NEEDS_ERROR_HANDLING
        if(pok_error_level_select(part->partition_hm_selector,
            system_state, error_id)
            && (part->mode == POK_PARTITION_MODE_NORMAL)
            && (part->thread_error != NULL)) {
            return FALSE;
        }
#endif /* POK_NEEDS_ERROR_HANDLING */
        break;      // Processed at partition level, as below.
#ifdef POK_NEEDS_ERROR_HANDLING
    case POK_SYSTEM_STATE_ERROR_HANDLER: // This state can only be viewed with error handling enabled.
        break;
#endif /* POK_NEEDS_ERROR_HANDLING */
    case POK_SYSTEM_STATE_OS_PART:
        break;
    default:
        pok_fatal("Unexpected partition state while error");
    }

    take_partition_action(system_state, error_id);

    if(part->nthreads_unrecoverable != 0U)
    {
        part->nthreads_unrecoverable = 0;
        while(true) {
            take_partition_action(POK_SYSTEM_STATE_OS_PART,
                POK_ERROR_ID_UNHANDLED_INT);
        }
    }

    return TRUE;
}

/**
 * @funcname `thread_set_unrecoverable`
 *
 * @description
 * Marks a thread as having an unrecoverable error, and counts it.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `thread` | `pok_thread_t *` | [in,out] The thread. |
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
 * - \hlrid{HLR_ERROR_ARINC_THREAD_SET_UNRECOVERABLE_01}:
 *   `thread_set_unrecoverable()` shall mark a process as having an error that
 *   the error handler cannot recover, once.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_HM_009}
 *
 * @llr
 * - \llrid{LLR_ERROR_ARINC_THREAD_SET_UNRECOVERABLE_01}: If and only if
 *   `is_unrecoverable` of `thread` is false, `thread_set_unrecoverable()` shall
 *   set it to `TRUE` and increment `nthreads_unrecoverable` of the partition.
 *   <br>Traces to:
 *   - \hlrref{HLR_ERROR_ARINC_THREAD_SET_UNRECOVERABLE_01}
 *
 * @design
 * 1. If `thread->is_unrecoverable` is false:
 *    \llrref{LLR_ERROR_ARINC_THREAD_SET_UNRECOVERABLE_01}
 *    1. Set `thread->is_unrecoverable` to `TRUE`.
 *    2. Increment `current_partition_arinc->nthreads_unrecoverable`.
 *
 * @calls
 * None.
 */
static void thread_set_unrecoverable(pok_thread_t* thread)
{
    if(!thread->is_unrecoverable)
    {
        thread->is_unrecoverable = TRUE;
        current_partition_arinc->nthreads_unrecoverable++;
    }
}

#ifdef POK_NEEDS_ERROR_HANDLING

/**
 * @funcname `thread_add_error_bit`
 *
 * @description
 * Adds an error of a thread for the error handler: sets its bit and puts the
 * thread in the partition's error list (the error handler then runs).
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `thread` | `pok_thread_t *` | [in,out] The thread. |
 * | `bit` | `pok_thread_error_bits_t` | [in] The error bit (not set yet). |
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
 * - \hlrid{HLR_ERROR_ARINC_THREAD_ADD_ERROR_BIT_01}: `thread_add_error_bit()`
 *   shall record an error of a process in the error bits of the process.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_HM_005}
 * - \hlrid{HLR_ERROR_ARINC_THREAD_ADD_ERROR_BIT_02}: `thread_add_error_bit()`
 *   shall add a process without recorded errors to the processes that the error
 *   handler handles, and make the partition schedule again.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_HM_005}
 *
 * @llr
 * - \llrid{LLR_ERROR_ARINC_THREAD_ADD_ERROR_BIT_01}: `thread_add_error_bit()`
 *   shall set `bit` in `error_bits` of `thread`.
 *   <br>Traces to:
 *   - \hlrref{HLR_ERROR_ARINC_THREAD_ADD_ERROR_BIT_01}
 * - \llrid{LLR_ERROR_ARINC_THREAD_ADD_ERROR_BIT_02}: If and only if
 *   `list_empty()` returns true for `error_elem` of `thread`,
 *   `thread_add_error_bit()` shall add it at the end of `error_list` of the
 *   partition with `list_add_tail()` and call `pok_sched_local_invalidate()`.
 *   <br>Traces to:
 *   - \hlrref{HLR_ERROR_ARINC_THREAD_ADD_ERROR_BIT_02}
 *
 * @design
 * 1. Set the bits `bit` in `thread->error_bits`.
 *    \llrref{LLR_ERROR_ARINC_THREAD_ADD_ERROR_BIT_01}
 * 2. If `list_empty(&thread->error_elem)` is true:
 *    \llrref{LLR_ERROR_ARINC_THREAD_ADD_ERROR_BIT_02}
 *    1. Call `list_add_tail()` with the arguments the address of
 *       `thread->error_elem` and the address of
 *       `current_partition_arinc->error_list`.
 *    2. Call `pok_sched_local_invalidate()`.
 *
 * @calls
 * - `list_empty()`
 * - `list_add_tail()`
 * - `pok_sched_local_invalidate()`
 */
static void thread_add_error_bit(pok_thread_t* thread,
    pok_thread_error_bits_t bit)
{
    assert((thread->error_bits & bit) == 0U);

    thread->error_bits |= bit;

    if(list_empty(&thread->error_elem))
    {
        list_add_tail(&thread->error_elem, &current_partition_arinc->error_list);
        pok_sched_local_invalidate();
    }
}

/**
 * @funcname `thread_clear_error_bit`
 *
 * @description
 * Clears an error bit of a thread; without errors left it leaves the error
 * list.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `thread` | `pok_thread_t *` | [in,out] The thread. |
 * | `bit` | `pok_thread_error_bits_t` | [in] The error bit (set). |
 *
 * @globals
 * None.
 *
 * @locals
 * None.
 *
 * @hlr
 * - \hlrid{HLR_ERROR_ARINC_THREAD_CLEAR_ERROR_BIT_01}:
 *   `thread_clear_error_bit()` shall remove an error of a process that the
 *   error handler took, and take the process out of the list when it has no
 *   error left.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_HM_007}
 *
 * @llr
 * - \llrid{LLR_ERROR_ARINC_THREAD_CLEAR_ERROR_BIT_01}:
 *   `thread_clear_error_bit()` shall subtract `bit` from `error_bits` of
 *   `thread`.
 *   <br>Traces to:
 *   - \hlrref{HLR_ERROR_ARINC_THREAD_CLEAR_ERROR_BIT_01}
 * - \llrid{LLR_ERROR_ARINC_THREAD_CLEAR_ERROR_BIT_02}: If and only if
 *   `error_bits` is then 0, `thread_clear_error_bit()` shall call
 *   `list_del_init()` for `error_elem` of `thread`.
 *   <br>Traces to:
 *   - \hlrref{HLR_ERROR_ARINC_THREAD_CLEAR_ERROR_BIT_01}
 *
 * @design
 * 1. Subtract `bit` from `thread->error_bits`.
 *    \llrref{LLR_ERROR_ARINC_THREAD_CLEAR_ERROR_BIT_01}
 * 2. If `thread->error_bits` is equal to `0U`:
 *    \llrref{LLR_ERROR_ARINC_THREAD_CLEAR_ERROR_BIT_02}
 *    1. Call `list_del_init()` with the address of `thread->error_elem`.
 *
 * @calls
 * - `list_del_init()`
 */
static void thread_clear_error_bit(pok_thread_t* thread,
    pok_thread_error_bits_t bit)
{
    assert((thread->error_bits & bit) == bit);
    assert(!list_empty(&thread->error_elem));

    thread->error_bits -= bit;

    if(thread->error_bits == 0U)
    {
        list_del_init(&thread->error_elem);
    }
}


/**
 * @funcname `thread_emit_sync_error`
 *
 * @description
 * Reports a synchronous error of a thread to the error handler, with the failed
 * address.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `thread` | `pok_thread_t *` | [in,out] The thread. |
 * | `error_id` | `pok_error_id_t` | [in] The error. |
 * | `failed_addr` | `void * JET_USER` | [in] The faulting address, or NULL. |
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
 * - \hlrid{HLR_ERROR_ARINC_THREAD_EMIT_SYNC_ERROR_01}:
 *   `thread_emit_sync_error()` shall record a synchronous error of a process
 *   with the failed address for the error handler.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_HM_005}
 *   - \srqref{SRQ_JET_HM_007}
 *
 * @llr
 * - \llrid{LLR_ERROR_ARINC_THREAD_EMIT_SYNC_ERROR_01}:
 *   `thread_emit_sync_error()` shall store the error ID and the address in the
 *   partition and set `POK_THREAD_ERROR_BIT_SYNC` with
 *   `thread_add_error_bit()`.
 *   <br>Traces to:
 *   - \hlrref{HLR_ERROR_ARINC_THREAD_EMIT_SYNC_ERROR_01}
 *
 * @design
 * 1. Set `current_partition_arinc->sync_error` to `error_id`.
 *    \llrref{LLR_ERROR_ARINC_THREAD_EMIT_SYNC_ERROR_01}
 * 2. Set `current_partition_arinc->sync_error_failed_addr` to `failed_addr`.
 *    \llrref{LLR_ERROR_ARINC_THREAD_EMIT_SYNC_ERROR_01}
 * 3. Call `thread_add_error_bit()` with the arguments `thread` and
 *    `POK_THREAD_ERROR_BIT_SYNC`.
 *    \llrref{LLR_ERROR_ARINC_THREAD_EMIT_SYNC_ERROR_01}
 *
 * @calls
 * - `thread_add_error_bit()`
 */
static void thread_emit_sync_error(pok_thread_t* thread,
    pok_error_id_t error_id, void* JET_USER failed_addr)
{
    current_partition_arinc->sync_error = error_id;
    current_partition_arinc->sync_error_failed_addr = failed_addr;

    thread_add_error_bit(thread, POK_THREAD_ERROR_BIT_SYNC);
}

#endif /* POK_NEEDS_ERROR_HANDLING */

/**
 * @funcname `pok_partition_arinc_process_error`
 *
 * @description
 * The error processing operation of ARINC 653 partitions: handles the error at
 * the partition level, or reports it to the error handler as a synchronous
 * error of the current thread.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `partition_state` | `pok_system_state_t` | [in] The system state. |
 * | `error_id` | `pok_error_id_t` | [in] The error. |
 * | `preempt_local_disabled_old` | `pok_bool_t` | [in] Whether local preemption was disabled (not used). |
 * | `failed_address` | `void *` | [in] The faulting address, if any. |
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
 * - \hlrid{HLR_ERROR_ARINC_POK_PARTITION_ARINC_PROCESS_ERROR_01}:
 *   `pok_partition_arinc_process_error()` shall handle an error of an ARINC 653
 *   partition: at the partition level, or as a synchronous error of the current
 *   process for the error handler.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_HM_004}
 *   - \srqref{SRQ_JET_HM_005}
 *   - \srqref{SRQ_JET_HM_012}
 *
 * @llr
 * - \llrid{LLR_ERROR_ARINC_POK_PARTITION_ARINC_PROCESS_ERROR_01}: If
 *   `process_error_partition()` returns true for `partition_state` and
 *   `error_id`, `pok_partition_arinc_process_error()` shall return without
 *   calling `thread_emit_sync_error()`.
 *   <br>Traces to:
 *   - \hlrref{HLR_ERROR_ARINC_POK_PARTITION_ARINC_PROCESS_ERROR_01}
 * - \llrid{LLR_ERROR_ARINC_POK_PARTITION_ARINC_PROCESS_ERROR_02}: If
 *   `process_error_partition()` returns false,
 *   `pok_partition_arinc_process_error()` shall call, with
 *   `POK_NEEDS_ERROR_HANDLING`, `pok_preemption_local_disable()`, then
 *   `thread_emit_sync_error()` with `thread_current` of the partition,
 *   `error_id` and `failed_address`, and then `pok_preemption_local_enable()`.
 *   <br>Traces to:
 *   - \hlrref{HLR_ERROR_ARINC_POK_PARTITION_ARINC_PROCESS_ERROR_01}
 *
 * @design
 * 1. Set `part` to `current_partition_arinc`.
 *    \llrref{LLR_ERROR_ARINC_POK_PARTITION_ARINC_PROCESS_ERROR_02}
 * 2. If `process_error_partition(partition_state, error_id)` is true:
 *    \llrref{LLR_ERROR_ARINC_POK_PARTITION_ARINC_PROCESS_ERROR_01},
 *    \llrref{LLR_ERROR_ARINC_POK_PARTITION_ARINC_PROCESS_ERROR_02}
 *    1. Return.
 * 3. Call `pok_preemption_local_disable()`.
 *    \llrref{LLR_ERROR_ARINC_POK_PARTITION_ARINC_PROCESS_ERROR_02}
 * 4. Call `thread_emit_sync_error()` with the arguments `part->thread_current`,
 *    `error_id` and `(JET_USER void*)failed_address`.
 *    \llrref{LLR_ERROR_ARINC_POK_PARTITION_ARINC_PROCESS_ERROR_02}
 * 5. Call `pok_preemption_local_enable()`.
 *    \llrref{LLR_ERROR_ARINC_POK_PARTITION_ARINC_PROCESS_ERROR_02}
 *
 * @calls
 * - `process_error_partition()`
 * - `pok_preemption_local_disable()`
 * - `thread_emit_sync_error()`
 * - `pok_preemption_local_enable()`
 */
void pok_partition_arinc_process_error(
    pok_system_state_t partition_state,
    pok_error_id_t error_id,
    pok_bool_t preempt_local_disabled_old,
    void* failed_address)
{
    pok_partition_arinc_t* part = current_partition_arinc;

    // TODO: Take into account `preempt_local_disabled_old`
    (void) preempt_local_disabled_old;

    if(process_error_partition(partition_state, error_id)) {
        return;
    }

#ifdef POK_NEEDS_ERROR_HANDLING

    assert(partition_state == POK_SYSTEM_STATE_USER);
    assert(part->thread_current != part->thread_error);

    pok_preemption_local_disable();

    thread_emit_sync_error(part->thread_current, error_id,
                           (JET_USER void*)failed_address);

    pok_preemption_local_enable();
#else
    assert(FALSE); // Unreachable
#endif /* POK_NEEDS_ERROR_HANDLING */
}

/**
 * @funcname `pok_thread_emit_deadline_missed`
 *
 * @description
 * Reports a missed deadline of a thread: handled at the partition level, or by
 * the error handler.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `thread` | `pok_thread_t *` | [in,out] The thread. |
 *
 * @globals
 * None.
 *
 * @locals
 * None.
 *
 * @hlr
 * - \hlrid{HLR_ERROR_ARINC_POK_THREAD_EMIT_DEADLINE_MISSED_01}:
 *   `pok_thread_emit_deadline_missed()` shall report that a process missed its
 *   deadline, at the partition level or to the error handler.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_THR_012}
 *   - \srqref{SRQ_JET_HM_005}
 *   - \srqref{SRQ_JET_HM_012}
 *
 * @llr
 * - \llrid{LLR_ERROR_ARINC_POK_THREAD_EMIT_DEADLINE_MISSED_01}: If
 *   `process_error_partition()` returns true for `POK_SYSTEM_STATE_USER` and
 *   `POK_ERROR_ID_DEADLINE_MISSED`, `pok_thread_emit_deadline_missed()` shall
 *   return without calling `thread_add_error_bit()`.
 *   <br>Traces to:
 *   - \hlrref{HLR_ERROR_ARINC_POK_THREAD_EMIT_DEADLINE_MISSED_01}
 * - \llrid{LLR_ERROR_ARINC_POK_THREAD_EMIT_DEADLINE_MISSED_02}: If
 *   `process_error_partition()` returns false,
 *   `pok_thread_emit_deadline_missed()` shall call `thread_add_error_bit()`
 *   with `thread` and `POK_THREAD_ERROR_BIT_DEADLINE` (with
 *   `POK_NEEDS_ERROR_HANDLING`).
 *   <br>Traces to:
 *   - \hlrref{HLR_ERROR_ARINC_POK_THREAD_EMIT_DEADLINE_MISSED_01}
 *
 * @design
 * 1. If `process_error_partition(POK_SYSTEM_STATE_USER,`
 *    `POK_ERROR_ID_DEADLINE_MISSED)` is true:
 *    \llrref{LLR_ERROR_ARINC_POK_THREAD_EMIT_DEADLINE_MISSED_01},
 *    \llrref{LLR_ERROR_ARINC_POK_THREAD_EMIT_DEADLINE_MISSED_02}
 *    1. Return.
 * 2. Call `thread_add_error_bit()` with the arguments `thread` and
 *    `POK_THREAD_ERROR_BIT_DEADLINE`.
 *    \llrref{LLR_ERROR_ARINC_POK_THREAD_EMIT_DEADLINE_MISSED_02}
 *
 * @calls
 * - `process_error_partition()`
 * - `thread_add_error_bit()`
 */
void pok_thread_emit_deadline_missed(pok_thread_t* thread)
{
    if(process_error_partition(POK_SYSTEM_STATE_USER,
        POK_ERROR_ID_DEADLINE_MISSED)) {
        return;
    }

#ifdef POK_NEEDS_ERROR_HANDLING
    thread_add_error_bit(thread, POK_THREAD_ERROR_BIT_DEADLINE);
#else
    assert(FALSE); // Unreachable
#endif /* POK_NEEDS_ERROR_HANDLING */
}

/**
 * @funcname `pok_thread_emit_deadline_oor`
 *
 * @description
 * Reports a deadline out of range of a thread (an unrecoverable illegal
 * request): handled at the partition level, or by the error handler.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `thread` | `pok_thread_t *` | [in,out] The thread. |
 *
 * @globals
 * None.
 *
 * @locals
 * None.
 *
 * @hlr
 * - \hlrid{HLR_ERROR_ARINC_POK_THREAD_EMIT_DEADLINE_OOR_01}:
 *   `pok_thread_emit_deadline_oor()` shall report a deadline of a process out
 *   of range as an unrecoverable illegal request, at the partition level or to
 *   the error handler.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_HM_005}
 *   - \srqref{SRQ_JET_HM_012}
 *   - \srqref{SRQ_JET_HM_009}
 *
 * @llr
 * - \llrid{LLR_ERROR_ARINC_POK_THREAD_EMIT_DEADLINE_OOR_01}:
 *   `pok_thread_emit_deadline_oor()` shall first call
 *   `thread_set_unrecoverable()` with `thread`.
 *   <br>Traces to:
 *   - \hlrref{HLR_ERROR_ARINC_POK_THREAD_EMIT_DEADLINE_OOR_01}
 * - \llrid{LLR_ERROR_ARINC_POK_THREAD_EMIT_DEADLINE_OOR_02}: If
 *   `process_error_partition()` returns true for `POK_SYSTEM_STATE_USER` and
 *   `POK_ERROR_ID_ILLEGAL_REQUEST`, `pok_thread_emit_deadline_oor()` shall
 *   return without calling `thread_add_error_bit()`.
 *   <br>Traces to:
 *   - \hlrref{HLR_ERROR_ARINC_POK_THREAD_EMIT_DEADLINE_OOR_01}
 * - \llrid{LLR_ERROR_ARINC_POK_THREAD_EMIT_DEADLINE_OOR_03}: If
 *   `process_error_partition()` returns false, `pok_thread_emit_deadline_oor()`
 *   shall call `thread_add_error_bit()` with `thread` and
 *   `POK_THREAD_ERROR_BIT_DEADLINE_OOR` (with `POK_NEEDS_ERROR_HANDLING`).
 *   <br>Traces to:
 *   - \hlrref{HLR_ERROR_ARINC_POK_THREAD_EMIT_DEADLINE_OOR_01}
 *
 * @design
 * 1. Call `thread_set_unrecoverable()` with the argument `thread`.
 *    \llrref{LLR_ERROR_ARINC_POK_THREAD_EMIT_DEADLINE_OOR_01}
 * 2. If `process_error_partition(POK_SYSTEM_STATE_USER,`
 *    `POK_ERROR_ID_ILLEGAL_REQUEST)` is true:
 *    \llrref{LLR_ERROR_ARINC_POK_THREAD_EMIT_DEADLINE_OOR_02},
 *    \llrref{LLR_ERROR_ARINC_POK_THREAD_EMIT_DEADLINE_OOR_03}
 *    1. Return.
 * 3. Call `thread_add_error_bit()` with the arguments `thread` and
 *    `POK_THREAD_ERROR_BIT_DEADLINE_OOR`.
 *    \llrref{LLR_ERROR_ARINC_POK_THREAD_EMIT_DEADLINE_OOR_03}
 *
 * @calls
 * - `thread_set_unrecoverable()`
 * - `process_error_partition()`
 * - `thread_add_error_bit()`
 */
void pok_thread_emit_deadline_oor(pok_thread_t* thread)
{
    thread_set_unrecoverable(thread);

    if(process_error_partition(POK_SYSTEM_STATE_USER,
        POK_ERROR_ID_ILLEGAL_REQUEST)) {
        return;
    }

#ifdef POK_NEEDS_ERROR_HANDLING
    thread_add_error_bit(thread, POK_THREAD_ERROR_BIT_DEADLINE_OOR);
#else
    assert(FALSE); // Unreachable
#endif /* POK_NEEDS_ERROR_HANDLING */
}

#ifdef POK_NEEDS_ERROR_HANDLING
/**
 * @funcname `pok_error_raise_application_error`
 *
 * @description
 * Raises an application error with a message (RAISE_APPLICATION_ERROR): handled
 * at the partition level, or by the error handler, which gets the message.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `msg` | `const char * JET_USER` | [in] Partition address of the message. |
 * | `msg_size` | `size_t` | [in] The message's size. |
 *
 * @return POK_ERRNO_OK; POK_ERRNO_EINVAL for a message too long;
 *         POK_ERRNO_EFAULT for a bad address.
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `current_partition` | `pok_partition_t *` | The running partition |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `k_msg` | `const char *` | Kernel address of the message, or "" |
 * | `part` | `pok_partition_arinc_t *` | The current partition |
 * | `partition_state` | `pok_system_state_t` | ERROR_HANDLER in the error handler, USER otherwise |
 * | `thread` | `pok_thread_t *` | The current thread |
 *
 * @hlr
 * - \hlrid{HLR_ERROR_ARINC_POK_ERROR_RAISE_APPLICATION_ERROR_01}:
 *   `pok_error_raise_application_error()` shall raise an application error with
 *   a message of the current process at the partition level.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_HM_008}
 *   - \srqref{SRQ_JET_HM_012}
 * - \hlrid{HLR_ERROR_ARINC_POK_ERROR_RAISE_APPLICATION_ERROR_02}:
 *   `pok_error_raise_application_error()` shall give the message to the error
 *   handler when the error is not handled at the partition level.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_HM_008}
 *   - \srqref{SRQ_JET_HM_005}
 * - \hlrid{HLR_ERROR_ARINC_POK_ERROR_RAISE_APPLICATION_ERROR_03}:
 *   `pok_error_raise_application_error()` shall refuse a message longer than
 *   the maximum or that the partition may not read.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_SYS_005}
 *   - \srqref{SRQ_JET_MEM_006}
 *   - \srqref{SRQ_JET_MEM_016}
 *
 * @llr
 * - \llrid{LLR_ERROR_ARINC_POK_ERROR_RAISE_APPLICATION_ERROR_01}: If `msg_size`
 *   is greater than `POK_ERROR_MAX_MSG_SIZE`,
 *   `pok_error_raise_application_error()` shall return `POK_ERRNO_EINVAL`.
 *   <br>Traces to:
 *   - \hlrref{HLR_ERROR_ARINC_POK_ERROR_RAISE_APPLICATION_ERROR_03}
 * - \llrid{LLR_ERROR_ARINC_POK_ERROR_RAISE_APPLICATION_ERROR_02}: If `msg_size`
 *   is greater than 0 and not greater than `POK_ERROR_MAX_MSG_SIZE` and
 *   `jet_user_to_kernel_ro()` returns NULL for `msg` and `msg_size`,
 *   `pok_error_raise_application_error()` shall return `POK_ERRNO_EFAULT`.
 *   <br>Traces to:
 *   - \hlrref{HLR_ERROR_ARINC_POK_ERROR_RAISE_APPLICATION_ERROR_03}
 * - \llrid{LLR_ERROR_ARINC_POK_ERROR_RAISE_APPLICATION_ERROR_03}: If the
 *   message passes these checks and `thread_current` of the partition is
 *   `thread_error`, `pok_error_raise_application_error()` shall call
 *   `process_error_partition()` once with `POK_SYSTEM_STATE_ERROR_HANDLER` and
 *   `POK_ERROR_ID_APPLICATION_ERROR`.
 *   <br>Traces to:
 *   - \hlrref{HLR_ERROR_ARINC_POK_ERROR_RAISE_APPLICATION_ERROR_01}
 * - \llrid{LLR_ERROR_ARINC_POK_ERROR_RAISE_APPLICATION_ERROR_04}: If the
 *   message passes these checks and `thread_current` is not `thread_error`,
 *   `pok_error_raise_application_error()` shall call
 *   `process_error_partition()` once with `POK_SYSTEM_STATE_USER` and
 *   `POK_ERROR_ID_APPLICATION_ERROR`.
 *   <br>Traces to:
 *   - \hlrref{HLR_ERROR_ARINC_POK_ERROR_RAISE_APPLICATION_ERROR_01}
 * - \llrid{LLR_ERROR_ARINC_POK_ERROR_RAISE_APPLICATION_ERROR_05}: If
 *   `process_error_partition()` returns true,
 *   `pok_error_raise_application_error()` shall return `POK_ERRNO_OK` without
 *   calling `thread_emit_sync_error()`.
 *   <br>Traces to:
 *   - \hlrref{HLR_ERROR_ARINC_POK_ERROR_RAISE_APPLICATION_ERROR_01}
 * - \llrid{LLR_ERROR_ARINC_POK_ERROR_RAISE_APPLICATION_ERROR_06}: If
 *   `process_error_partition()` returns false,
 *   `pok_error_raise_application_error()` shall set `wait_buffer.src` of
 *   `thread_current` to the kernel address of the message (`""` if `msg_size`
 *   is 0) and its `wait_len` to `msg_size`, call
 *   `pok_preemption_local_disable()`, `thread_emit_sync_error()` with the
 *   thread, `POK_ERROR_ID_APPLICATION_ERROR` and NULL, and
 *   `pok_preemption_local_enable()`, and return `POK_ERRNO_OK`.
 *   <br>Traces to:
 *   - \hlrref{HLR_ERROR_ARINC_POK_ERROR_RAISE_APPLICATION_ERROR_02}
 *
 * @design
 * 1. Set `part` to `current_partition_arinc`.
 *    \llrref{LLR_ERROR_ARINC_POK_ERROR_RAISE_APPLICATION_ERROR_03},
 *    \llrref{LLR_ERROR_ARINC_POK_ERROR_RAISE_APPLICATION_ERROR_04},
 *    \llrref{LLR_ERROR_ARINC_POK_ERROR_RAISE_APPLICATION_ERROR_06}
 * 2. Set `thread` to `part->thread_current`.
 *    \llrref{LLR_ERROR_ARINC_POK_ERROR_RAISE_APPLICATION_ERROR_03},
 *    \llrref{LLR_ERROR_ARINC_POK_ERROR_RAISE_APPLICATION_ERROR_04},
 *    \llrref{LLR_ERROR_ARINC_POK_ERROR_RAISE_APPLICATION_ERROR_06}
 * 3. If `msg_size` is greater than `POK_ERROR_MAX_MSG_SIZE`:
 *    \llrref{LLR_ERROR_ARINC_POK_ERROR_RAISE_APPLICATION_ERROR_01}
 *    1. Return `POK_ERRNO_EINVAL`.
 * 4. If `msg_size` is greater than `0U`:
 *    \llrref{LLR_ERROR_ARINC_POK_ERROR_RAISE_APPLICATION_ERROR_02},
 *    \llrref{LLR_ERROR_ARINC_POK_ERROR_RAISE_APPLICATION_ERROR_06}
 *    1. Call `jet_user_to_kernel_ro()` with the arguments `msg` and `msg_size`,
 *       and store the result in `k_msg`.
 *    2. If `k_msg` is NULL:
 *       \llrref{LLR_ERROR_ARINC_POK_ERROR_RAISE_APPLICATION_ERROR_02}
 *       1. Return `POK_ERRNO_EFAULT`.
 * 5. Otherwise: \llrref{LLR_ERROR_ARINC_POK_ERROR_RAISE_APPLICATION_ERROR_06}
 *    1. Set `k_msg` to `""`.
 * 6. If `thread` is equal to `part->thread_error`:
 *    \llrref{LLR_ERROR_ARINC_POK_ERROR_RAISE_APPLICATION_ERROR_03}
 *    1. Set `partition_state` to `POK_SYSTEM_STATE_ERROR_HANDLER`.
 * 7. Otherwise: \llrref{LLR_ERROR_ARINC_POK_ERROR_RAISE_APPLICATION_ERROR_04}
 *    1. Set `partition_state` to `POK_SYSTEM_STATE_USER`.
 * 8. If `process_error_partition(partition_state,`
 *    `POK_ERROR_ID_APPLICATION_ERROR)` is true:
 *    \llrref{LLR_ERROR_ARINC_POK_ERROR_RAISE_APPLICATION_ERROR_05},
 *    \llrref{LLR_ERROR_ARINC_POK_ERROR_RAISE_APPLICATION_ERROR_06}
 *    1. Return `POK_ERRNO_OK`.
 *       \llrref{LLR_ERROR_ARINC_POK_ERROR_RAISE_APPLICATION_ERROR_05}
 * 9. Set `thread->wait_buffer.src` to `k_msg`.
 *    \llrref{LLR_ERROR_ARINC_POK_ERROR_RAISE_APPLICATION_ERROR_06}
 * 10. Set `thread->wait_len` to `msg_size`.
 *     \llrref{LLR_ERROR_ARINC_POK_ERROR_RAISE_APPLICATION_ERROR_06}
 * 11. Call `pok_preemption_local_disable()`.
 *     \llrref{LLR_ERROR_ARINC_POK_ERROR_RAISE_APPLICATION_ERROR_06}
 * 12. Call `thread_emit_sync_error()` with the arguments `thread`,
 *     `POK_ERROR_ID_APPLICATION_ERROR` and `NULL`.
 *     \llrref{LLR_ERROR_ARINC_POK_ERROR_RAISE_APPLICATION_ERROR_06}
 * 13. Call `pok_preemption_local_enable()`.
 *     \llrref{LLR_ERROR_ARINC_POK_ERROR_RAISE_APPLICATION_ERROR_06}
 * 14. Return `POK_ERRNO_OK`.
 *     \llrref{LLR_ERROR_ARINC_POK_ERROR_RAISE_APPLICATION_ERROR_06}
 *
 * @calls
 * - `jet_user_to_kernel_ro()`
 * - `process_error_partition()`
 * - `pok_preemption_local_disable()`
 * - `thread_emit_sync_error()`
 * - `pok_preemption_local_enable()`
 */
pok_ret_e pok_error_raise_application_error (const char* JET_USER msg,
                                             size_t msg_size)
{
    pok_partition_arinc_t* part = current_partition_arinc;
    pok_thread_t* thread = part->thread_current;

    pok_system_state_t partition_state;
    const char* JET_KUSER k_msg;

    if (msg_size > POK_ERROR_MAX_MSG_SIZE) {
        return POK_ERRNO_EINVAL;
    }

    if (msg_size > 0U) {
        k_msg = jet_user_to_kernel_ro(msg, msg_size);
        if (k_msg == NULL) {
            return POK_ERRNO_EFAULT;
        }
    } else {
        // An empty message is valid; GET_ERROR_STATUS reports LENGTH 0.
        k_msg = "";
    }

    if(thread == part->thread_error)
    {
        partition_state = POK_SYSTEM_STATE_ERROR_HANDLER;
    }
    else
    {
        partition_state = POK_SYSTEM_STATE_USER;
    }

    if(process_error_partition(partition_state,
                               POK_ERROR_ID_APPLICATION_ERROR)) {
        return POK_ERRNO_OK;
    }

    assert(part->thread_error != NULL);
    assert(part->thread_current != part->thread_error);

    thread->wait_buffer.src = k_msg;
    thread->wait_len = msg_size;

    pok_preemption_local_disable();

    thread_emit_sync_error(thread, POK_ERROR_ID_APPLICATION_ERROR, NULL);

    pok_preemption_local_enable();

    return POK_ERRNO_OK;
}

/**
 * @funcname `pok_error_report_application_message`
 *
 * @description
 * Writes an application message to the console, with the partition's name
 * (REPORT_APPLICATION_MESSAGE).
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `msg` | `const char * JET_USER` | [in] Partition address of the message. |
 * | `msg_size` | `size_t` | [in] The message's size. |
 *
 * @return POK_ERRNO_OK; POK_ERRNO_EINVAL for a message too long;
 *         POK_ERRNO_EFAULT for a bad address.
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `current_partition` | `pok_partition_t *` | The running partition |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `k_msg` | `const char *` | Kernel address of the message |
 * | `part` | `pok_partition_arinc_t *` | The current partition |
 * | `prefix` | `static const char[41]` | The text before the partition's name |
 *
 * @hlr
 * - \hlrid{HLR_ERROR_ARINC_POK_ERROR_REPORT_APPLICATION_MESSAGE_01}:
 *   `pok_error_report_application_message()` shall write a message of the
 *   application to the console, after the name of the partition, as one line.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_HM_008}
 *   - \srqref{SRQ_JET_CONS_001}
 *   - \srqref{SRQ_JET_CONS_008}
 * - \hlrid{HLR_ERROR_ARINC_POK_ERROR_REPORT_APPLICATION_MESSAGE_02}:
 *   `pok_error_report_application_message()` shall refuse a message longer than
 *   the maximum or that the partition may not read.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_SYS_005}
 *   - \srqref{SRQ_JET_MEM_006}
 *   - \srqref{SRQ_JET_MEM_016}
 *
 * @llr
 * - \llrid{LLR_ERROR_ARINC_POK_ERROR_REPORT_APPLICATION_MESSAGE_01}: If
 *   `msg_size` is greater than `POK_ERROR_MAX_MSG_SIZE`,
 *   `pok_error_report_application_message()` shall return `POK_ERRNO_EINVAL`
 *   without writing.
 *   <br>Traces to:
 *   - \hlrref{HLR_ERROR_ARINC_POK_ERROR_REPORT_APPLICATION_MESSAGE_02}
 * - \llrid{LLR_ERROR_ARINC_POK_ERROR_REPORT_APPLICATION_MESSAGE_02}: If
 *   `msg_size` is greater than 0 and not greater than `POK_ERROR_MAX_MSG_SIZE`
 *   and `jet_user_to_kernel_ro()` returns NULL for `msg` and `msg_size`,
 *   `pok_error_report_application_message()` shall return `POK_ERRNO_EFAULT`
 *   without writing.
 *   <br>Traces to:
 *   - \hlrref{HLR_ERROR_ARINC_POK_ERROR_REPORT_APPLICATION_MESSAGE_02}
 * - \llrid{LLR_ERROR_ARINC_POK_ERROR_REPORT_APPLICATION_MESSAGE_03}: If
 *   `msg_size` is 0, `pok_error_report_application_message()` shall call
 *   `pok_preemption_local_disable()`, then `pok_cons_write()` with `"[HM]
 *   Application message from partition "` and its length, with `name` of the
 *   base partition and its length, with `": "` and 2, and with `"\n"` and 1,
 *   and then `pok_preemption_local_enable()`, and return `POK_ERRNO_OK`,
 *   without calling `jet_user_to_kernel_ro()`.
 *   <br>Traces to:
 *   - \hlrref{HLR_ERROR_ARINC_POK_ERROR_REPORT_APPLICATION_MESSAGE_01}
 * - \llrid{LLR_ERROR_ARINC_POK_ERROR_REPORT_APPLICATION_MESSAGE_04}: If
 *   `msg_size` is greater than 0 and not greater than `POK_ERROR_MAX_MSG_SIZE`
 *   and `jet_user_to_kernel_ro()` returns an address,
 *   `pok_error_report_application_message()` shall write in the same way, with
 *   a call of `pok_cons_write()` with that address and `msg_size` between those
 *   with `": "` and with `"\n"`, and return `POK_ERRNO_OK`.
 *   <br>Traces to:
 *   - \hlrref{HLR_ERROR_ARINC_POK_ERROR_REPORT_APPLICATION_MESSAGE_01}
 *
 * @design
 * 1. Set `part` to `current_partition_arinc`.
 *    \llrref{LLR_ERROR_ARINC_POK_ERROR_REPORT_APPLICATION_MESSAGE_03},
 *    \llrref{LLR_ERROR_ARINC_POK_ERROR_REPORT_APPLICATION_MESSAGE_04}
 * 2. Set `k_msg` to `NULL`.
 *    \llrref{LLR_ERROR_ARINC_POK_ERROR_REPORT_APPLICATION_MESSAGE_03}
 * 3. If `msg_size` is greater than `POK_ERROR_MAX_MSG_SIZE`:
 *    \llrref{LLR_ERROR_ARINC_POK_ERROR_REPORT_APPLICATION_MESSAGE_01}
 *    1. Return `POK_ERRNO_EINVAL`.
 * 4. If `msg_size` is greater than `0U`:
 *    \llrref{LLR_ERROR_ARINC_POK_ERROR_REPORT_APPLICATION_MESSAGE_02},
 *    \llrref{LLR_ERROR_ARINC_POK_ERROR_REPORT_APPLICATION_MESSAGE_04}
 *    1. Call `jet_user_to_kernel_ro()` with the arguments `msg` and `msg_size`,
 *       and store the result in `k_msg`.
 *    2. If `k_msg` is NULL:
 *       \llrref{LLR_ERROR_ARINC_POK_ERROR_REPORT_APPLICATION_MESSAGE_02}
 *       1. Return `POK_ERRNO_EFAULT`.
 * 5. Call `pok_preemption_local_disable()`.
 *    \llrref{LLR_ERROR_ARINC_POK_ERROR_REPORT_APPLICATION_MESSAGE_03},
 *    \llrref{LLR_ERROR_ARINC_POK_ERROR_REPORT_APPLICATION_MESSAGE_04}
 * 6. Call `pok_cons_write()` with the arguments `prefix` and
 *    `sizeof(prefix) - 1U`.
 *    \llrref{LLR_ERROR_ARINC_POK_ERROR_REPORT_APPLICATION_MESSAGE_03},
 *    \llrref{LLR_ERROR_ARINC_POK_ERROR_REPORT_APPLICATION_MESSAGE_04}
 * 7. Call `pok_cons_write()` with the arguments `part->base_part.name` and
 *    `strlen(part->base_part.name)`.
 *    \llrref{LLR_ERROR_ARINC_POK_ERROR_REPORT_APPLICATION_MESSAGE_03},
 *    \llrref{LLR_ERROR_ARINC_POK_ERROR_REPORT_APPLICATION_MESSAGE_04}
 * 8. Call `pok_cons_write()` with the arguments `": "` and `2`.
 *    \llrref{LLR_ERROR_ARINC_POK_ERROR_REPORT_APPLICATION_MESSAGE_03},
 *    \llrref{LLR_ERROR_ARINC_POK_ERROR_REPORT_APPLICATION_MESSAGE_04}
 * 9. If `msg_size` is greater than `0U`:
 *    \llrref{LLR_ERROR_ARINC_POK_ERROR_REPORT_APPLICATION_MESSAGE_03},
 *    \llrref{LLR_ERROR_ARINC_POK_ERROR_REPORT_APPLICATION_MESSAGE_04}
 *    1. Call `pok_cons_write()` with the arguments `k_msg` and `msg_size`.
 *       \llrref{LLR_ERROR_ARINC_POK_ERROR_REPORT_APPLICATION_MESSAGE_04}
 * 10. Call `pok_cons_write()` with the arguments `"\n"` and `1`.
 *     \llrref{LLR_ERROR_ARINC_POK_ERROR_REPORT_APPLICATION_MESSAGE_03},
 *     \llrref{LLR_ERROR_ARINC_POK_ERROR_REPORT_APPLICATION_MESSAGE_04}
 * 11. Call `pok_preemption_local_enable()`.
 *     \llrref{LLR_ERROR_ARINC_POK_ERROR_REPORT_APPLICATION_MESSAGE_03},
 *     \llrref{LLR_ERROR_ARINC_POK_ERROR_REPORT_APPLICATION_MESSAGE_04}
 * 12. Return `POK_ERRNO_OK`.
 *     \llrref{LLR_ERROR_ARINC_POK_ERROR_REPORT_APPLICATION_MESSAGE_03},
 *     \llrref{LLR_ERROR_ARINC_POK_ERROR_REPORT_APPLICATION_MESSAGE_04}
 *
 * @calls
 * - `jet_user_to_kernel_ro()`
 * - `pok_preemption_local_disable()`
 * - `pok_cons_write()`
 * - `strlen()`
 * - `pok_preemption_local_enable()`
 */
pok_ret_e pok_error_report_application_message (const char* JET_USER msg,
                                                size_t msg_size)
{
    static const char prefix[] = "[HM] Application message from partition ";
    pok_partition_arinc_t* part = current_partition_arinc;
    const char* JET_KUSER k_msg = NULL;

    if (msg_size > POK_ERROR_MAX_MSG_SIZE) {
        return POK_ERRNO_EINVAL;
    }

    // An empty message is valid; don't touch user memory for it.
    if (msg_size > 0U) {
        k_msg = jet_user_to_kernel_ro(msg, msg_size);
        if (k_msg == NULL) {
            return POK_ERRNO_EFAULT;
        }
    }

    // Keep the record on one line even if other partitions print.
    pok_preemption_local_disable();

    (void) pok_cons_write(prefix, sizeof(prefix) - 1U);
    (void) pok_cons_write(part->base_part.name, strlen(part->base_part.name));
    (void) pok_cons_write(": ", 2);
    if (msg_size > 0U) {
        (void) pok_cons_write(k_msg, msg_size);
    }
    (void) pok_cons_write("\n", 1);

    pok_preemption_local_enable();

    return POK_ERRNO_OK;
}

/**
 * @funcname `pok_error_raise_os_error`
 *
 * @description
 * Raises an error found by the partition's OS code: writes its message to the
 * console and handles an application error at the partition level (an unhandled
 * error if that doesn't end it).
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `msg` | `const char * JET_USER` | [in] Partition address of the message. |
 * | `msg_size` | `size_t` | [in] The message's size. |
 *
 * @return POK_ERRNO_OK if the error was handled.
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `k_msg` | `const char *` | Kernel address of the message |
 *
 * @hlr
 * - \hlrid{HLR_ERROR_ARINC_POK_ERROR_RAISE_OS_ERROR_01}:
 *   `pok_error_raise_os_error()` shall write the message of an error found by
 *   the operating system code of the partition to the console.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_HM_008}
 * - \hlrid{HLR_ERROR_ARINC_POK_ERROR_RAISE_OS_ERROR_02}:
 *   `pok_error_raise_os_error()` shall handle the error as an application error
 *   at the partition level.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_HM_008}
 *   - \srqref{SRQ_JET_HM_004}
 * - \hlrid{HLR_ERROR_ARINC_POK_ERROR_RAISE_OS_ERROR_03}:
 *   `pok_error_raise_os_error()` shall raise an unhandled interrupt error at
 *   the partition level again and again, without returning, when the partition
 *   level does not handle the application error.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_HM_004}
 *
 * @llr
 * - \llrid{LLR_ERROR_ARINC_POK_ERROR_RAISE_OS_ERROR_01}: If `msg_size` is not 0
 *   and `jet_user_to_kernel_ro()` returns an address for `msg` and `msg_size`,
 *   `pok_error_raise_os_error()` shall call `jet_console_write()` once with
 *   that address and `msg_size`.
 *   <br>Traces to:
 *   - \hlrref{HLR_ERROR_ARINC_POK_ERROR_RAISE_OS_ERROR_01}
 * - \llrid{LLR_ERROR_ARINC_POK_ERROR_RAISE_OS_ERROR_02}: If `msg_size` is 0, or
 *   `jet_user_to_kernel_ro()` returns NULL, `pok_error_raise_os_error()` shall
 *   not call `jet_console_write()`.
 *   <br>Traces to:
 *   - \hlrref{HLR_ERROR_ARINC_POK_ERROR_RAISE_OS_ERROR_01}
 * - \llrid{LLR_ERROR_ARINC_POK_ERROR_RAISE_OS_ERROR_03}: If
 *   `process_error_partition()` then returns true for
 *   `POK_SYSTEM_STATE_OS_PART` and `POK_ERROR_ID_APPLICATION_ERROR`,
 *   `pok_error_raise_os_error()` shall return `POK_ERRNO_OK`.
 *   <br>Traces to:
 *   - \hlrref{HLR_ERROR_ARINC_POK_ERROR_RAISE_OS_ERROR_02}
 * - \llrid{LLR_ERROR_ARINC_POK_ERROR_RAISE_OS_ERROR_04}: If
 *   `process_error_partition()` returns false, `pok_error_raise_os_error()`
 *   shall call `process_error_partition()` with `POK_SYSTEM_STATE_OS_PART` and
 *   `POK_ERROR_ID_UNHANDLED_INT` again and again, without returning.
 *   <br>Traces to:
 *   - \hlrref{HLR_ERROR_ARINC_POK_ERROR_RAISE_OS_ERROR_03}
 *
 * @design
 * 1. If `msg_size` is equal to `0U`:
 *    \llrref{LLR_ERROR_ARINC_POK_ERROR_RAISE_OS_ERROR_01},
 *    \llrref{LLR_ERROR_ARINC_POK_ERROR_RAISE_OS_ERROR_02}
 *    1. Go to the label `out`.
 *       \llrref{LLR_ERROR_ARINC_POK_ERROR_RAISE_OS_ERROR_02}
 * 2. Call `jet_user_to_kernel_ro()` with the arguments `msg` and `msg_size`,
 *    and store the result in `k_msg`.
 *    \llrref{LLR_ERROR_ARINC_POK_ERROR_RAISE_OS_ERROR_01},
 *    \llrref{LLR_ERROR_ARINC_POK_ERROR_RAISE_OS_ERROR_02}
 * 3. If `k_msg` is NULL: \llrref{LLR_ERROR_ARINC_POK_ERROR_RAISE_OS_ERROR_01},
 *    \llrref{LLR_ERROR_ARINC_POK_ERROR_RAISE_OS_ERROR_02}
 *    1. Go to the label `out`.
 *       \llrref{LLR_ERROR_ARINC_POK_ERROR_RAISE_OS_ERROR_02}
 * 4. Call `jet_console_write()` with the arguments `k_msg` and `msg_size`.
 *    \llrref{LLR_ERROR_ARINC_POK_ERROR_RAISE_OS_ERROR_01}
 * 5. Label `out`: \llrref{LLR_ERROR_ARINC_POK_ERROR_RAISE_OS_ERROR_03},
 *    \llrref{LLR_ERROR_ARINC_POK_ERROR_RAISE_OS_ERROR_04}
 * 6. If `process_error_partition(POK_SYSTEM_STATE_OS_PART,`
 *    `POK_ERROR_ID_APPLICATION_ERROR)` is true:
 *    \llrref{LLR_ERROR_ARINC_POK_ERROR_RAISE_OS_ERROR_03},
 *    \llrref{LLR_ERROR_ARINC_POK_ERROR_RAISE_OS_ERROR_04}
 *    1. Return `POK_ERRNO_OK`.
 *       \llrref{LLR_ERROR_ARINC_POK_ERROR_RAISE_OS_ERROR_03}
 * 7. While `true` is true: \llrref{LLR_ERROR_ARINC_POK_ERROR_RAISE_OS_ERROR_04}
 *    1. Call `process_error_partition()` with the arguments
 *       `POK_SYSTEM_STATE_OS_PART` and `POK_ERROR_ID_UNHANDLED_INT`.
 *
 * @calls
 * - `jet_user_to_kernel_ro()`
 * - `jet_console_write()`
 * - `process_error_partition()`
 */
pok_ret_e pok_error_raise_os_error (const char* JET_USER msg, size_t msg_size)
{
    const char* JET_KUSER k_msg;

    if (msg_size == 0U) {
        goto out; // Cannot return. Just ignore message.
    }

    k_msg = jet_user_to_kernel_ro(msg, msg_size);

    if (k_msg == NULL) {
        goto out; // Cannot return. Just ignore message.
    }

    (void) jet_console_write(k_msg, msg_size);

out:
    if(process_error_partition(POK_SYSTEM_STATE_OS_PART,
                               POK_ERROR_ID_APPLICATION_ERROR)) {
        return POK_ERRNO_OK;
    }

    // Should never return.
    while(true) {
        (void) process_error_partition(POK_SYSTEM_STATE_OS_PART,
            POK_ERROR_ID_UNHANDLED_INT);
    }
}


/**
 * @funcname `error_check_after_handler`
 *
 * @description
 * Called when the error handler stops: if unrecoverable errors remain, the
 * partition handles them (UNHANDLED_INT).
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
 * - \hlrid{HLR_ERROR_ARINC_ERROR_CHECK_AFTER_HANDLER_01}:
 *   `error_check_after_handler()` shall handle, when the error handler stops,
 *   the unrecoverable errors that remain as errors of the partition.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_HM_009}
 *
 * @llr
 * - \llrid{LLR_ERROR_ARINC_ERROR_CHECK_AFTER_HANDLER_01}: If and only if
 *   `nthreads_unrecoverable` of the partition is not 0,
 *   `error_check_after_handler()` shall call `process_error_partition()` once
 *   with `POK_SYSTEM_STATE_ERROR_HANDLER` and `POK_ERROR_ID_UNHANDLED_INT`.
 *   <br>Traces to:
 *   - \hlrref{HLR_ERROR_ARINC_ERROR_CHECK_AFTER_HANDLER_01}
 *
 * @design
 * 1. Set `part` to `current_partition_arinc`.
 *    \llrref{LLR_ERROR_ARINC_ERROR_CHECK_AFTER_HANDLER_01}
 * 2. If `part->nthreads_unrecoverable` is not equal to `0U`:
 *    \llrref{LLR_ERROR_ARINC_ERROR_CHECK_AFTER_HANDLER_01}
 *    1. Call `process_error_partition()` with the arguments
 *       `POK_SYSTEM_STATE_ERROR_HANDLER` and `POK_ERROR_ID_UNHANDLED_INT`.
 *
 * @calls
 * - `process_error_partition()`
 */
void error_check_after_handler(void)
{
    pok_partition_arinc_t* part = current_partition_arinc;
    if(part->nthreads_unrecoverable != 0U)
    {
        /*
         * Note, that we do not reset unrecoverable counter.
         *
         * Such a way partition's error processing mechanism treats
         * error as unrecoverable too.
         */
        (void) process_error_partition(POK_SYSTEM_STATE_ERROR_HANDLER,
            POK_ERROR_ID_UNHANDLED_INT);
    }
}


/**
 * @funcname `copy_error_to_user`
 *
 * @description
 * Copies an error's description and kind into the error status of the error
 * handler.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `error_id` | `pok_error_id_t` | [in] The error. |
 * | `k_status` | `pok_error_status_t * JET_KUSER` | [out] Kernel address of the error status. |
 * | `k_msg` | `void * JET_KUSER` | [out] Kernel address of the message buffer. |
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `msg` | `const char *` | The description |
 * | `msg_size` | `size_t` | Its size, with the NUL if it fits |
 *
 * @hlr
 * - \hlrid{HLR_ERROR_ARINC_COPY_ERROR_TO_USER_01}: `copy_error_to_user()` shall
 *   give the error handler the description and the kind of an error.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_HM_007}
 *
 * @llr
 * - \llrid{LLR_ERROR_ARINC_COPY_ERROR_TO_USER_01}: If the result of `strnlen()`
 *   for the description of the error (`get_error_description()`) and
 *   `POK_ERROR_MAX_MSG_SIZE` is less than `POK_ERROR_MAX_MSG_SIZE`,
 *   `copy_error_to_user()` shall copy that many bytes plus 1 (with the `'\0'`)
 *   to `k_msg` with `memcpy()` and set `msg_size` of `k_status` to that number
 *   plus 1.
 *   <br>Traces to:
 *   - \hlrref{HLR_ERROR_ARINC_COPY_ERROR_TO_USER_01}
 * - \llrid{LLR_ERROR_ARINC_COPY_ERROR_TO_USER_02}: If that result is
 *   `POK_ERROR_MAX_MSG_SIZE`, `copy_error_to_user()` shall copy
 *   `POK_ERROR_MAX_MSG_SIZE` bytes to `k_msg` with `memcpy()` and set
 *   `msg_size` of `k_status` to `POK_ERROR_MAX_MSG_SIZE`.
 *   <br>Traces to:
 *   - \hlrref{HLR_ERROR_ARINC_COPY_ERROR_TO_USER_01}
 * - \llrid{LLR_ERROR_ARINC_COPY_ERROR_TO_USER_03}: `copy_error_to_user()` shall
 *   set `error_kind` of `k_status` to the result of `get_error_kind()` for
 *   `error_id`.
 *   <br>Traces to:
 *   - \hlrref{HLR_ERROR_ARINC_COPY_ERROR_TO_USER_01}
 *
 * @design
 * 1. Call `get_error_description()` with the argument `error_id`, and store the
 *    result in `msg`. \llrref{LLR_ERROR_ARINC_COPY_ERROR_TO_USER_01},
 *    \llrref{LLR_ERROR_ARINC_COPY_ERROR_TO_USER_02}
 * 2. Call `strnlen()` with the arguments `msg` and `POK_ERROR_MAX_MSG_SIZE`,
 *    and store the result in `msg_size`.
 *    \llrref{LLR_ERROR_ARINC_COPY_ERROR_TO_USER_01},
 *    \llrref{LLR_ERROR_ARINC_COPY_ERROR_TO_USER_02}
 * 3. If `msg_size` is not equal to `POK_ERROR_MAX_MSG_SIZE`:
 *    \llrref{LLR_ERROR_ARINC_COPY_ERROR_TO_USER_01},
 *    \llrref{LLR_ERROR_ARINC_COPY_ERROR_TO_USER_02}
 *    1. Increment `msg_size`. \llrref{LLR_ERROR_ARINC_COPY_ERROR_TO_USER_01}
 * 4. Call `memcpy()` with the arguments `k_msg`, `msg` and `msg_size`.
 *    \llrref{LLR_ERROR_ARINC_COPY_ERROR_TO_USER_01},
 *    \llrref{LLR_ERROR_ARINC_COPY_ERROR_TO_USER_02}
 * 5. Set `k_status->msg_size` to `msg_size`.
 *    \llrref{LLR_ERROR_ARINC_COPY_ERROR_TO_USER_01},
 *    \llrref{LLR_ERROR_ARINC_COPY_ERROR_TO_USER_02}
 * 6. Call `get_error_kind()` with the argument `error_id`, and store the result
 *    in `k_status->error_kind`. \llrref{LLR_ERROR_ARINC_COPY_ERROR_TO_USER_03}
 *
 * @calls
 * - `get_error_description()`
 * - `strnlen()`
 * - `memcpy()`
 * - `get_error_kind()`
 */
static void copy_error_to_user(pok_error_id_t error_id,
    pok_error_status_t* JET_KUSER k_status, void* JET_KUSER k_msg)
{
    const char* msg = get_error_description(error_id);
    size_t msg_size;

    msg_size = strnlen(msg, POK_ERROR_MAX_MSG_SIZE);
    if(msg_size != POK_ERROR_MAX_MSG_SIZE) {
        msg_size++;
    }

    (void) memcpy(k_msg, msg, msg_size);
    k_status->msg_size = msg_size;
    k_status->error_kind = get_error_kind(error_id);
}

/**
 * @funcname `pok_error_get`
 *
 * @description
 * Gives the error handler the next error (GET_ERROR_STATUS): of the first
 * thread in the error list, a synchronous error first, then a missed deadline,
 * then a deadline out of range; with its message, kind, thread and failed
 * address.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `status` | `pok_error_status_t * JET_USER` | [out] Partition address of the error status. |
 * | `msg` | `void * JET_USER` | [out] Partition address of the message buffer. |
 *
 * @return POK_ERRNO_OK; POK_ERRNO_THREAD outside the error handler;
 *         POK_ERRNO_UNAVAILABLE with no error; POK_ERRNO_EFAULT for a bad
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
 * | `error_bits` | `pok_thread_error_bits_t` | Its error bits |
 * | `failed_addr` | `void *` | The failed address |
 * | `k_msg` | `void *` | Kernel address of the message buffer |
 * | `k_status` | `pok_error_status_t *` | Kernel address of the error status |
 * | `part` | `pok_partition_arinc_t *` | The current partition |
 * | `thread` | `pok_thread_t *` | The thread with the error |
 *
 * @hlr
 * - \hlrid{HLR_ERROR_ARINC_POK_ERROR_GET_01}: `pok_error_get()` shall give the
 *   error handler of the current partition the next error to handle: of the
 *   first process in the error list, a synchronous error first, then a missed
 *   deadline, then a deadline out of range; with its message, kind, process and
 *   failed address.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_HM_007}
 * - \hlrid{HLR_ERROR_ARINC_POK_ERROR_GET_02}: `pok_error_get()` shall refuse a
 *   caller other than the error handler, report that there is no error, and
 *   refuse addresses that the partition may not write.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_SYS_005}
 *   - \srqref{SRQ_JET_MEM_006}
 *   - \srqref{SRQ_JET_MEM_016}
 *
 * @llr
 * - \llrid{LLR_ERROR_ARINC_POK_ERROR_GET_01}: If `thread_current` of the
 *   partition is not `thread_error`, `pok_error_get()` shall return
 *   `POK_ERRNO_THREAD`.
 *   <br>Traces to:
 *   - \hlrref{HLR_ERROR_ARINC_POK_ERROR_GET_02}
 * - \llrid{LLR_ERROR_ARINC_POK_ERROR_GET_02}: If `thread_current` is
 *   `thread_error` and `list_empty()` returns true for `error_list`,
 *   `pok_error_get()` shall return `POK_ERRNO_UNAVAILABLE`.
 *   <br>Traces to:
 *   - \hlrref{HLR_ERROR_ARINC_POK_ERROR_GET_02}
 * - \llrid{LLR_ERROR_ARINC_POK_ERROR_GET_03}: If these checks pass and
 *   `jet_user_to_kernel_typed()` returns NULL for `status`, `pok_error_get()`
 *   shall return `POK_ERRNO_EFAULT`.
 *   <br>Traces to:
 *   - \hlrref{HLR_ERROR_ARINC_POK_ERROR_GET_02}
 * - \llrid{LLR_ERROR_ARINC_POK_ERROR_GET_04}: If these checks pass and
 *   `jet_user_to_kernel()` returns NULL for `msg` and `POK_ERROR_MAX_MSG_SIZE`,
 *   `pok_error_get()` shall return `POK_ERRNO_EFAULT`.
 *   <br>Traces to:
 *   - \hlrref{HLR_ERROR_ARINC_POK_ERROR_GET_02}
 * - \llrid{LLR_ERROR_ARINC_POK_ERROR_GET_05}: If `POK_THREAD_ERROR_BIT_SYNC` is
 *   set in `error_bits` of the first thread of `error_list` and `sync_error` of
 *   the partition is `POK_ERROR_ID_APPLICATION_ERROR`, `pok_error_get()` shall
 *   call `thread_clear_error_bit()` with the thread and
 *   `POK_THREAD_ERROR_BIT_SYNC`, set `msg_size` of the status to `wait_len` of
 *   the thread and `error_kind` to the result of `get_error_kind()` for
 *   `POK_ERROR_ID_APPLICATION_ERROR`, copy `wait_len` bytes of
 *   `wait_buffer.src` to the message with `memcpy()`, and take
 *   `sync_error_failed_addr` as the failed address.
 *   <br>Traces to:
 *   - \hlrref{HLR_ERROR_ARINC_POK_ERROR_GET_01}
 * - \llrid{LLR_ERROR_ARINC_POK_ERROR_GET_06}: If `POK_THREAD_ERROR_BIT_SYNC` is
 *   set and `sync_error` is not `POK_ERROR_ID_APPLICATION_ERROR`,
 *   `pok_error_get()` shall call `thread_clear_error_bit()` with the thread and
 *   `POK_THREAD_ERROR_BIT_SYNC` and `copy_error_to_user()` with `sync_error`,
 *   the status and the message, and take `sync_error_failed_addr` as the failed
 *   address.
 *   <br>Traces to:
 *   - \hlrref{HLR_ERROR_ARINC_POK_ERROR_GET_01}
 * - \llrid{LLR_ERROR_ARINC_POK_ERROR_GET_07}: If `POK_THREAD_ERROR_BIT_SYNC` is
 *   not set and `POK_THREAD_ERROR_BIT_DEADLINE` is set, `pok_error_get()` shall
 *   call `thread_clear_error_bit()` with the thread and
 *   `POK_THREAD_ERROR_BIT_DEADLINE` and `copy_error_to_user()` with
 *   `POK_ERROR_ID_DEADLINE_MISSED`, the status and the message, and take NULL
 *   as the failed address.
 *   <br>Traces to:
 *   - \hlrref{HLR_ERROR_ARINC_POK_ERROR_GET_01}
 * - \llrid{LLR_ERROR_ARINC_POK_ERROR_GET_08}: If neither
 *   `POK_THREAD_ERROR_BIT_SYNC` nor `POK_THREAD_ERROR_BIT_DEADLINE` is set and
 *   `POK_THREAD_ERROR_BIT_DEADLINE_OOR` is set, `pok_error_get()` shall call
 *   `thread_clear_error_bit()` with the thread and
 *   `POK_THREAD_ERROR_BIT_DEADLINE_OOR` and `copy_error_to_user()` with
 *   `POK_ERROR_ID_ILLEGAL_REQUEST`, the status and the message, and take NULL
 *   as the failed address.
 *   <br>Traces to:
 *   - \hlrref{HLR_ERROR_ARINC_POK_ERROR_GET_01}
 * - \llrid{LLR_ERROR_ARINC_POK_ERROR_GET_09}: If none of the three bits is set,
 *   `pok_error_get()` shall call `pok_assertion_fail()` (`unreachable()`).
 *   <br>Traces to:
 *   - \hlrref{HLR_ERROR_ARINC_POK_ERROR_GET_01}
 * - \llrid{LLR_ERROR_ARINC_POK_ERROR_GET_10}: `pok_error_get()` shall then set
 *   `failed_thread` of the status to the index of the thread and `failed_addr`
 *   to the failed address, and return `POK_ERRNO_OK`.
 *   <br>Traces to:
 *   - \hlrref{HLR_ERROR_ARINC_POK_ERROR_GET_01}
 *
 * @design
 * 1. Set `part` to `current_partition_arinc`.
 *    \llrref{LLR_ERROR_ARINC_POK_ERROR_GET_01},
 *    \llrref{LLR_ERROR_ARINC_POK_ERROR_GET_02},
 *    \llrref{LLR_ERROR_ARINC_POK_ERROR_GET_03},
 *    \llrref{LLR_ERROR_ARINC_POK_ERROR_GET_04},
 *    \llrref{LLR_ERROR_ARINC_POK_ERROR_GET_05},
 *    \llrref{LLR_ERROR_ARINC_POK_ERROR_GET_06},
 *    \llrref{LLR_ERROR_ARINC_POK_ERROR_GET_07},
 *    \llrref{LLR_ERROR_ARINC_POK_ERROR_GET_08},
 *    \llrref{LLR_ERROR_ARINC_POK_ERROR_GET_09},
 *    \llrref{LLR_ERROR_ARINC_POK_ERROR_GET_10}
 * 2. Set `failed_addr` to `NULL`. \llrref{LLR_ERROR_ARINC_POK_ERROR_GET_10}
 * 3. If `part->thread_current` is not equal to `part->thread_error`:
 *    \llrref{LLR_ERROR_ARINC_POK_ERROR_GET_01}
 *    1. Return `POK_ERRNO_THREAD`.
 * 4. If `list_empty(&part->error_list)` is true:
 *    \llrref{LLR_ERROR_ARINC_POK_ERROR_GET_02}
 *    1. Return `POK_ERRNO_UNAVAILABLE`.
 * 5. Set `k_status` to `jet_user_to_kernel_typed(status)`.
 *    \llrref{LLR_ERROR_ARINC_POK_ERROR_GET_03}
 * 6. If `k_status` is NULL: \llrref{LLR_ERROR_ARINC_POK_ERROR_GET_03}
 *    1. Return `POK_ERRNO_EFAULT`.
 * 7. Call `jet_user_to_kernel()` with the arguments `msg` and
 *    `POK_ERROR_MAX_MSG_SIZE`, and store the result in `k_msg`.
 *    \llrref{LLR_ERROR_ARINC_POK_ERROR_GET_04}
 * 8. If `k_msg` is NULL: \llrref{LLR_ERROR_ARINC_POK_ERROR_GET_04}
 *    1. Return `POK_ERRNO_EFAULT`.
 * 9. Set `thread` to
 *    `list_first_entry(&part->error_list, pok_thread_t, error_elem)`.
 *    \llrref{LLR_ERROR_ARINC_POK_ERROR_GET_05},
 *    \llrref{LLR_ERROR_ARINC_POK_ERROR_GET_06},
 *    \llrref{LLR_ERROR_ARINC_POK_ERROR_GET_07},
 *    \llrref{LLR_ERROR_ARINC_POK_ERROR_GET_08},
 *    \llrref{LLR_ERROR_ARINC_POK_ERROR_GET_09}
 * 10. Set `error_bits` to `thread->error_bits`.
 *     \llrref{LLR_ERROR_ARINC_POK_ERROR_GET_05},
 *     \llrref{LLR_ERROR_ARINC_POK_ERROR_GET_06},
 *     \llrref{LLR_ERROR_ARINC_POK_ERROR_GET_07},
 *     \llrref{LLR_ERROR_ARINC_POK_ERROR_GET_08},
 *     \llrref{LLR_ERROR_ARINC_POK_ERROR_GET_09}
 * 11. If `(error_bits & POK_THREAD_ERROR_BIT_SYNC)` is not equal to `0U`:
 *     \llrref{LLR_ERROR_ARINC_POK_ERROR_GET_05},
 *     \llrref{LLR_ERROR_ARINC_POK_ERROR_GET_06}
 *     1. Call `thread_clear_error_bit()` with the arguments `thread` and
 *        `POK_THREAD_ERROR_BIT_SYNC`.
 *     2. If `part->sync_error` is equal to `POK_ERROR_ID_APPLICATION_ERROR`:
 *        \llrref{LLR_ERROR_ARINC_POK_ERROR_GET_05},
 *        \llrref{LLR_ERROR_ARINC_POK_ERROR_GET_06}
 *        1. Set `k_status->msg_size` to `thread->wait_len`.
 *           \llrref{LLR_ERROR_ARINC_POK_ERROR_GET_05}
 *        2. Call `get_error_kind()` with the argument
 *           `POK_ERROR_ID_APPLICATION_ERROR`, and store the result in
 *           `k_status->error_kind`. \llrref{LLR_ERROR_ARINC_POK_ERROR_GET_05}
 *        3. Call `memcpy()` with the arguments `k_msg`,
 *           `thread->wait_buffer.src` and `thread->wait_len`.
 *           \llrref{LLR_ERROR_ARINC_POK_ERROR_GET_05}
 *     3. Otherwise: \llrref{LLR_ERROR_ARINC_POK_ERROR_GET_06}
 *        1. Call `copy_error_to_user()` with the arguments `part->sync_error`,
 *           `k_status` and `k_msg`.
 *     4. Set `failed_addr` to `part->sync_error_failed_addr`.
 *        \llrref{LLR_ERROR_ARINC_POK_ERROR_GET_05},
 *        \llrref{LLR_ERROR_ARINC_POK_ERROR_GET_06}
 * 12. Otherwise, if `(error_bits & POK_THREAD_ERROR_BIT_DEADLINE)` is not equal
 *     to `0U`: \llrref{LLR_ERROR_ARINC_POK_ERROR_GET_07}
 *     1. Call `thread_clear_error_bit()` with the arguments `thread` and
 *        `POK_THREAD_ERROR_BIT_DEADLINE`.
 *     2. Call `copy_error_to_user()` with the arguments
 *        `POK_ERROR_ID_DEADLINE_MISSED`, `k_status` and `k_msg`.
 * 13. Otherwise, if `(error_bits & POK_THREAD_ERROR_BIT_DEADLINE_OOR)` is not
 *     equal to `0U`: \llrref{LLR_ERROR_ARINC_POK_ERROR_GET_08}
 *     1. Call `thread_clear_error_bit()` with the arguments `thread` and
 *        `POK_THREAD_ERROR_BIT_DEADLINE_OOR`.
 *     2. Call `copy_error_to_user()` with the arguments
 *        `POK_ERROR_ID_ILLEGAL_REQUEST`, `k_status` and `k_msg`.
 * 14. Otherwise: \llrref{LLR_ERROR_ARINC_POK_ERROR_GET_09}
 *     1. Use the macro `unreachable()`.
 * 15. Set `k_status->failed_thread` to `thread - part->threads`.
 *     \llrref{LLR_ERROR_ARINC_POK_ERROR_GET_10}
 * 16. Set `k_status->failed_addr` to `(uintptr_t)failed_addr`.
 *     \llrref{LLR_ERROR_ARINC_POK_ERROR_GET_10}
 * 17. Return `POK_ERRNO_OK`. \llrref{LLR_ERROR_ARINC_POK_ERROR_GET_10}
 *
 * @calls
 * - `list_empty()`
 * - `jet_user_to_kernel()`
 * - `thread_clear_error_bit()`
 * - `get_error_kind()`
 * - `memcpy()`
 * - `copy_error_to_user()`
 * - `pok_assertion_fail()`
 */
pok_ret_e pok_error_get (pok_error_status_t* JET_USER status,
    void* JET_USER msg)
{
    pok_partition_arinc_t* part = current_partition_arinc;
    pok_thread_t* thread;
    pok_thread_error_bits_t error_bits;

    void* JET_USER failed_addr = NULL;

    if (part->thread_current != part->thread_error) {
        return POK_ERRNO_THREAD;
    }

    if(list_empty(&part->error_list)) {
        return POK_ERRNO_UNAVAILABLE;
    }

    pok_error_status_t* JET_KUSER k_status = jet_user_to_kernel_typed(status);
    if(k_status == NULL) {
        return POK_ERRNO_EFAULT;
    }

    void* JET_KUSER k_msg = jet_user_to_kernel(msg, POK_ERROR_MAX_MSG_SIZE);
    if(k_msg == NULL) {
        return POK_ERRNO_EFAULT;
    }

    thread = list_first_entry(&part->error_list, pok_thread_t, error_elem);

    error_bits = thread->error_bits;

    if((error_bits & POK_THREAD_ERROR_BIT_SYNC) != 0U)
    {
         thread_clear_error_bit(thread, POK_THREAD_ERROR_BIT_SYNC);

         if(part->sync_error == POK_ERROR_ID_APPLICATION_ERROR)
         {
             k_status->msg_size = thread->wait_len;
             k_status->error_kind = get_error_kind(POK_ERROR_ID_APPLICATION_ERROR);
             (void) memcpy(k_msg, thread->wait_buffer.src, thread->wait_len);
         }
         else
         {
             copy_error_to_user(part->sync_error, k_status, k_msg);
         }

         failed_addr = part->sync_error_failed_addr;
    }
    else if((error_bits & POK_THREAD_ERROR_BIT_DEADLINE) != 0U)
    {
        thread_clear_error_bit(thread, POK_THREAD_ERROR_BIT_DEADLINE);

        copy_error_to_user(POK_ERROR_ID_DEADLINE_MISSED, k_status, k_msg);
    }

    else if((error_bits & POK_THREAD_ERROR_BIT_DEADLINE_OOR) != 0U)
    {
        thread_clear_error_bit(thread, POK_THREAD_ERROR_BIT_DEADLINE_OOR);

        copy_error_to_user(POK_ERROR_ID_ILLEGAL_REQUEST, k_status, k_msg);
    }
    else
    {
        unreachable();
    }

    k_status->failed_thread = thread - part->threads;
    /* MISRA C:2025 Rule 11.6 deviation: the failed address is reported as a
     * number. */
    k_status->failed_addr =  (uintptr_t)failed_addr;

    return POK_ERRNO_OK;
}

/**
 * @funcname `error_ignore_sync`
 *
 * @description
 * Called when the error handler stops: the synchronous error of the first
 * thread in the error list is dropped.
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
 * | `thread` | `pok_thread_t *` | The first thread in the error list |
 *
 * @hlr
 * - \hlrid{HLR_ERROR_ARINC_ERROR_IGNORE_SYNC_01}: `error_ignore_sync()` shall
 *   drop, when the error handler stops, the synchronous error of the first
 *   process in the error list.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_HM_007}
 *   - \srqref{SRQ_JET_HM_009}
 *
 * @llr
 * - \llrid{LLR_ERROR_ARINC_ERROR_IGNORE_SYNC_01}: If `list_empty()` returns
 *   true for `error_list` of the partition, `error_ignore_sync()` shall return
 *   without clearing a bit.
 *   <br>Traces to:
 *   - \hlrref{HLR_ERROR_ARINC_ERROR_IGNORE_SYNC_01}
 * - \llrid{LLR_ERROR_ARINC_ERROR_IGNORE_SYNC_02}: If `error_list` is not empty
 *   and `POK_THREAD_ERROR_BIT_SYNC` is set in `error_bits` of its first thread,
 *   `error_ignore_sync()` shall call `thread_clear_error_bit()` once with that
 *   thread and `POK_THREAD_ERROR_BIT_SYNC`.
 *   <br>Traces to:
 *   - \hlrref{HLR_ERROR_ARINC_ERROR_IGNORE_SYNC_01}
 * - \llrid{LLR_ERROR_ARINC_ERROR_IGNORE_SYNC_03}: If `error_list` is not empty
 *   and `POK_THREAD_ERROR_BIT_SYNC` is not set in `error_bits` of its first
 *   thread, `error_ignore_sync()` shall clear no bit.
 *   <br>Traces to:
 *   - \hlrref{HLR_ERROR_ARINC_ERROR_IGNORE_SYNC_01}
 *
 * @design
 * 1. Set `part` to `current_partition_arinc`.
 *    \llrref{LLR_ERROR_ARINC_ERROR_IGNORE_SYNC_01},
 *    \llrref{LLR_ERROR_ARINC_ERROR_IGNORE_SYNC_02},
 *    \llrref{LLR_ERROR_ARINC_ERROR_IGNORE_SYNC_03}
 * 2. If `list_empty(&part->error_list)` is true:
 *    \llrref{LLR_ERROR_ARINC_ERROR_IGNORE_SYNC_01}
 *    1. Return.
 * 3. Set `thread` to
 *    `list_first_entry(&part->error_list, pok_thread_t, error_elem)`.
 *    \llrref{LLR_ERROR_ARINC_ERROR_IGNORE_SYNC_02},
 *    \llrref{LLR_ERROR_ARINC_ERROR_IGNORE_SYNC_03}
 * 4. If `(thread->error_bits & POK_THREAD_ERROR_BIT_SYNC)` is not equal to
 *    `0U`: \llrref{LLR_ERROR_ARINC_ERROR_IGNORE_SYNC_02},
 *    \llrref{LLR_ERROR_ARINC_ERROR_IGNORE_SYNC_03}
 *    1. Call `thread_clear_error_bit()` with the arguments `thread` and
 *       `POK_THREAD_ERROR_BIT_SYNC`.
 *       \llrref{LLR_ERROR_ARINC_ERROR_IGNORE_SYNC_02}
 *
 * @calls
 * - `list_empty()`
 * - `thread_clear_error_bit()`
 */
void error_ignore_sync(void)
{
    pok_partition_arinc_t* part = current_partition_arinc;

    if(list_empty(&part->error_list)) {
        return;
    }

    // Synchronous error could be only the first in the list.
    pok_thread_t* thread = list_first_entry(&part->error_list, pok_thread_t, error_elem);

    if((thread->error_bits & POK_THREAD_ERROR_BIT_SYNC) != 0U) {
        thread_clear_error_bit(thread, POK_THREAD_ERROR_BIT_SYNC);
    }
}
#endif /* POK_NEEDS_ERROR_HANDLING */

#endif

