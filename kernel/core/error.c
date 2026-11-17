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
 * Modified by Kirill Brazhnikov, 2026-10-31: the module-level RESET
 * action gives every partition start condition HM_MODULE_RESTART; module
 * RESET and SHUTDOWN are reported on the console.
 *
 * Modified by Kirill Brazhnikov, 2026-11-02: MISRA C:2025: braces around
 * all if/else/loop bodies; essentially Boolean conditions; U suffixes for
 * unsigned constants; discarded results are cast to void; the default
 * multi-partition HM table gives SHUTDOWN for every error ID (only the
 * first one of each state was set, the others were IGNORE).
 *
 * Modified by Kirill Brazhnikov, 2026-11-03: long lines are wrapped at 80
 * columns.
 *
 * Modified by Kirill Brazhnikov, 2026-11-08: Doxygen headers for the file
 * and its functions.
 *
 * Modified by Kirill Brazhnikov, 2026-11-13: the macro of the default HM
 * multi-partition table is moved to kernel/include/core/error.h (it is no
 * longer undefined after the table); pok_error_module_action_table_t is renamed
 * pok_error_hm_module_t, pok_error_level_selector_t is renamed
 * pok_error_levels_t (names too long for the tables of the documentation).
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
 * Health monitoring at the module level: an error is a module-level error
 * (handled by the module HM table: ignore, reset or shut down the module) or
 * a partition-level error (handed to the partition).
 */


#include <core/error.h>
#include <core/sched.h>
#include <core/partition.h>
#include <asp/arch.h>
#include <cons.h>

// TODO: this should be modified somewhere
pok_system_state_t kernel_state = POK_SYSTEM_STATE_INIT_PARTOS;

/**
 * @funcname `get_module_action`
 *
 * @description
 * Returns the action of a module HM table for an error in a system state.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `table` | `const pok_error_hm_module_t *` | [in] The module HM table. |
 * | `system_state` | `pok_system_state_t` | [in] The system state. |
 * | `error_id` | `pok_error_id_t` | [in] The error. |
 *
 * @return The action.
 *
 * @globals
 * None.
 *
 * @locals
 * None.
 *
 * @hlr
 * - \hlrid{HLR_ERROR_GET_MODULE_ACTION_01}: `get_module_action()` shall give
 *   the action of a module health monitoring table for an error in a system
 *   state.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_HM_003}
 *   - \srqref{SRQ_JET_CFG_006}
 *   - \srqref{SRQ_JET_CFG_012}
 *
 * @llr
 * - \llrid{LLR_ERROR_GET_MODULE_ACTION_01}: `get_module_action()` shall return
 *   the table's entry for the system state and the error ID.
 *   <br>Traces to:
 *   - \hlrref{HLR_ERROR_GET_MODULE_ACTION_01}
 *
 * @design
 * 1. Return `table->actions[system_state][error_id]`.
 *    \llrref{LLR_ERROR_GET_MODULE_ACTION_01}
 *
 * @calls
 * None.
 */
static pok_error_module_action_t get_module_action(
    const pok_error_hm_module_t* table,
    pok_system_state_t system_state,
    pok_error_id_t error_id)
{
    return table->actions[system_state][error_id];
}

/**
 * @funcname `set_hm_module_restart`
 *
 * @description
 * Marks a partition as restarted by the module health monitor (a callback of
 * for_each_partition()).
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `part` | `pok_partition_t *` | [out] The partition. |
 *
 * @globals
 * None.
 *
 * @locals
 * None.
 *
 * @hlr
 * - \hlrid{HLR_ERROR_SET_HM_MODULE_RESTART_01}: `set_hm_module_restart()` shall
 *   record that a partition starts because the health monitor restarted the
 *   module.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_PART_008}
 *
 * @llr
 * - \llrid{LLR_ERROR_SET_HM_MODULE_RESTART_01}: `set_hm_module_restart()` shall
 *   set the start condition of the partition to
 *   `POK_START_CONDITION_HM_MODULE_RESTART`.
 *   <br>Traces to:
 *   - \hlrref{HLR_ERROR_SET_HM_MODULE_RESTART_01}
 *
 * @design
 * 1. Set `part->start_condition` to `POK_START_CONDITION_HM_MODULE_RESTART`.
 *    \llrref{LLR_ERROR_SET_HM_MODULE_RESTART_01}
 *
 * @calls
 * None.
 */
static void set_hm_module_restart(pok_partition_t* part)
{
    part->start_condition = POK_START_CONDITION_HM_MODULE_RESTART;
}

/**
 * @funcname `perform_module_action`
 *
 * @description
 * Performs a module-level error action: ignore, restart the module (every
 * partition is reloaded and starts in COLD_START mode) or shut it down.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `action` | `pok_error_module_action_t` | [in] The action. |
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `msg_reset` | `static const char[39]` | The message of a module restart |
 * | `msg_shutdown` | `static const char[42]` | The message of a module shutdown |
 *
 * @hlr
 * - \hlrid{HLR_ERROR_PERFORM_MODULE_ACTION_01}: `perform_module_action()` shall
 *   perform a module action: ignore the error, restart the module, every
 *   partition starting again in `COLD_START` mode with the module restart as
 *   the reason, or shut the module down; each with a message on the console.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_HM_003}
 *
 * @llr
 * - \llrid{LLR_ERROR_PERFORM_MODULE_ACTION_01}: If `action` is
 *   `POK_ERROR_MODULE_ACTION_IGNORE`, `perform_module_action()` shall return
 *   without calling `ja_preempt_disable()`.
 *   <br>Traces to:
 *   - \hlrref{HLR_ERROR_PERFORM_MODULE_ACTION_01}
 * - \llrid{LLR_ERROR_PERFORM_MODULE_ACTION_02}: If `action` is
 *   `POK_ERROR_MODULE_ACTION_RESET`, `perform_module_action()` shall call, in
 *   this order, `ja_preempt_disable()`, `pok_cons_write()` with `"[KERNEL]
 *   Module HM: restarting module\n"` and its length, `for_each_partition()`
 *   with `set_hm_module_restart` and `pok_sched_restart()`.
 *   <br>Traces to:
 *   - \hlrref{HLR_ERROR_PERFORM_MODULE_ACTION_01}
 * - \llrid{LLR_ERROR_PERFORM_MODULE_ACTION_03}: If `action` is
 *   `POK_ERROR_MODULE_ACTION_SHUTDOWN`, `perform_module_action()` shall call,
 *   in this order, `ja_preempt_disable()`, `pok_cons_write()` with `"[KERNEL]
 *   Module HM: shutting down module\n"` and its length, and `ja_inf_loop()`.
 *   <br>Traces to:
 *   - \hlrref{HLR_ERROR_PERFORM_MODULE_ACTION_01}
 * - \llrid{LLR_ERROR_PERFORM_MODULE_ACTION_04}: If `action` is none of
 *   `POK_ERROR_MODULE_ACTION_IGNORE`, `POK_ERROR_MODULE_ACTION_RESET` and
 *   `POK_ERROR_MODULE_ACTION_SHUTDOWN`, `perform_module_action()` shall call
 *   `ja_preempt_disable()` and then `ja_inf_loop()`, without calling
 *   `pok_cons_write()`.
 *   <br>Traces to:
 *   - \hlrref{HLR_ERROR_PERFORM_MODULE_ACTION_01}
 *
 * @design
 * 1. If `action` is equal to `POK_ERROR_MODULE_ACTION_IGNORE`:
 *    \llrref{LLR_ERROR_PERFORM_MODULE_ACTION_01}
 *    1. Return.
 * 2. Call `ja_preempt_disable()`. \llrref{LLR_ERROR_PERFORM_MODULE_ACTION_02},
 *    \llrref{LLR_ERROR_PERFORM_MODULE_ACTION_03},
 *    \llrref{LLR_ERROR_PERFORM_MODULE_ACTION_04}
 * 3. Depending on `action`: \llrref{LLR_ERROR_PERFORM_MODULE_ACTION_02},
 *    \llrref{LLR_ERROR_PERFORM_MODULE_ACTION_03},
 *    \llrref{LLR_ERROR_PERFORM_MODULE_ACTION_04}
 *    1. Case `POK_ERROR_MODULE_ACTION_RESET`:
 *       \llrref{LLR_ERROR_PERFORM_MODULE_ACTION_02}
 *       1. Call `pok_cons_write()` with the arguments `msg_reset` and
 *          `sizeof(msg_reset) - 1U`.
 *       2. Call `for_each_partition()` with the address of
 *          `set_hm_module_restart`.
 *       3. Call `pok_sched_restart()`.
 *       4. Leave the switch.
 *    2. Case `POK_ERROR_MODULE_ACTION_SHUTDOWN`:
 *       \llrref{LLR_ERROR_PERFORM_MODULE_ACTION_03}
 *       1. Call `pok_cons_write()` with the arguments `msg_shutdown` and
 *          `sizeof(msg_shutdown) - 1U`.
 *       2. Call `ja_inf_loop()`.
 *       3. Leave the switch.
 *    3. Otherwise (default): \llrref{LLR_ERROR_PERFORM_MODULE_ACTION_04}
 *       1. Call `ja_inf_loop()`.
 *
 * @calls
 * - `ja_preempt_disable()`
 * - `pok_cons_write()`
 * - `for_each_partition()`
 * - `pok_sched_restart()`
 * - `ja_inf_loop()`
 */
static void perform_module_action(pok_error_module_action_t action)
{
    static const char msg_reset[] = "[KERNEL] Module HM: restarting module\n";
    static const char msg_shutdown[] = "[KERNEL] Module HM: shutting down module\n";

    if(action == POK_ERROR_MODULE_ACTION_IGNORE) {
        return;
    }

    ja_preempt_disable();

    switch(action) {

    case POK_ERROR_MODULE_ACTION_RESET:
        (void) pok_cons_write(msg_reset, sizeof(msg_reset) - 1U);
        /*
         * Software restart: every partition is reloaded and starts in
         * COLD_START mode (see pok_partition_reset()). Its start condition
         * is reported by GET_PARTITION_STATUS.
         */
        for_each_partition(&set_hm_module_restart);
        pok_sched_restart();
        break;

    case POK_ERROR_MODULE_ACTION_SHUTDOWN:
        (void) pok_cons_write(msg_shutdown, sizeof(msg_shutdown) - 1U);
        ja_inf_loop();
        break;

    default:
        /*
         * Incorrect configuration table.
         *
         * We do not trust module HM table anymore.
         *
         * Forse shutdown.
         */
        // TODO: Force shutdown
        ja_inf_loop();
    }
}

/**
 * @funcname `get_current_state`
 *
 * @description
 * Returns the system state in which an error happens.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `user_space` | `pok_bool_t` | [in] Whether the error happened in a partition's user code. |
 *
 * @return The kernel's state during initialization; then OS_PART for kernel
 *         code, and ERROR_HANDLER or USER for user code (by the current
 *         partition).
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `current_partition` | `pok_partition_t *` | The running partition |
 * | `kernel_state` | `pok_system_state_t` | The kernel's system state |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `part` | `pok_partition_t *` | The current partition |
 *
 * @hlr
 * - \hlrid{HLR_ERROR_GET_CURRENT_STATE_01}: `get_current_state()` shall give
 *   the system state in which an error happens: the state of the kernel during
 *   its initialization, and afterwards the partition operating system for
 *   kernel code, and the error handler or a user process for user code.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_HM_002}
 *
 * @llr
 * - \llrid{LLR_ERROR_GET_CURRENT_STATE_01}: If `kernel_state` is not
 *   `POK_SYSTEM_STATE_OS_PART`, `get_current_state()` shall return
 *   `kernel_state`.
 *   <br>Traces to:
 *   - \hlrref{HLR_ERROR_GET_CURRENT_STATE_01}
 * - \llrid{LLR_ERROR_GET_CURRENT_STATE_02}: If `kernel_state` is
 *   `POK_SYSTEM_STATE_OS_PART` and `user_space` is false, `get_current_state()`
 *   shall return `POK_SYSTEM_STATE_OS_PART`.
 *   <br>Traces to:
 *   - \hlrref{HLR_ERROR_GET_CURRENT_STATE_01}
 * - \llrid{LLR_ERROR_GET_CURRENT_STATE_03}: If `kernel_state` is
 *   `POK_SYSTEM_STATE_OS_PART`, `user_space` is true and `is_error_handler` of
 *   `current_partition` is true, `get_current_state()` shall return
 *   `POK_SYSTEM_STATE_ERROR_HANDLER`.
 *   <br>Traces to:
 *   - \hlrref{HLR_ERROR_GET_CURRENT_STATE_01}
 * - \llrid{LLR_ERROR_GET_CURRENT_STATE_04}: If `kernel_state` is
 *   `POK_SYSTEM_STATE_OS_PART`, `user_space` is true and `is_error_handler` of
 *   `current_partition` is false, `get_current_state()` shall return
 *   `POK_SYSTEM_STATE_USER`.
 *   <br>Traces to:
 *   - \hlrref{HLR_ERROR_GET_CURRENT_STATE_01}
 *
 * @design
 * 1. If `kernel_state` is not equal to `POK_SYSTEM_STATE_OS_PART`:
 *    \llrref{LLR_ERROR_GET_CURRENT_STATE_01}
 *    1. Return `kernel_state`.
 * 2. Set `part` to `current_partition`.
 *    \llrref{LLR_ERROR_GET_CURRENT_STATE_03},
 *    \llrref{LLR_ERROR_GET_CURRENT_STATE_04}
 * 3. If `user_space` is false: \llrref{LLR_ERROR_GET_CURRENT_STATE_02}
 *    1. Return `POK_SYSTEM_STATE_OS_PART`.
 * 4. Return `POK_SYSTEM_STATE_ERROR_HANDLER` if `part->is_error_handler` is
 *    true, otherwise `POK_SYSTEM_STATE_USER`.
 *    \llrref{LLR_ERROR_GET_CURRENT_STATE_03},
 *    \llrref{LLR_ERROR_GET_CURRENT_STATE_04}
 *
 * @calls
 * None.
 */
static pok_system_state_t get_current_state(pok_bool_t user_space)
{
    pok_partition_t* part;

    if(kernel_state != POK_SYSTEM_STATE_OS_PART) {
        return kernel_state;
    }

    part = current_partition;

    if(!user_space) {
        return POK_SYSTEM_STATE_OS_PART;
    }

    return part->is_error_handler
        ? POK_SYSTEM_STATE_ERROR_HANDLER
        : POK_SYSTEM_STATE_USER;
}


/**
 * @funcname `process_error_module`
 *
 * @description
 * Handles an error if it is a module-level error.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `system_state` | `pok_system_state_t` | [in] The system state. |
 * | `error_id` | `pok_error_id_t` | [in] The error. |
 *
 * @return FALSE if the error is a partition-level error (to be handled by the
 *         partition), TRUE if it was handled.
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `current_partition` | `pok_partition_t *` | The running partition |
 * | `pok_hm_module_selector` | `pok_error_levels_t` | The module HM selector (generated) |
 * | `pok_hm_module_table` | `pok_error_hm_module_t` | The module HM table (generated) |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `action` | `pok_error_module_action_t` | The module action |
 * | `part` | `pok_partition_t *` | The current partition |
 *
 * @hlr
 * - \hlrid{HLR_ERROR_PROCESS_ERROR_MODULE_01}: `process_error_module()` shall
 *   decide, from the module and the partition selectors of the configuration,
 *   whether an error in a system state is an error of the module or of the
 *   partition.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_HM_002}
 * - \hlrid{HLR_ERROR_PROCESS_ERROR_MODULE_02}: `process_error_module()` shall
 *   handle an error of the module at once by the action of the module table (or
 *   of the partition's multi-partition table).
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_HM_003}
 *
 * @llr
 * - \llrid{LLR_ERROR_PROCESS_ERROR_MODULE_01}: If `pok_error_level_select()`
 *   returns true for `pok_hm_module_selector`, `system_state` and `error_id`,
 *   and also for `multi_partition_hm_selector` of `current_partition`,
 *   `process_error_module()` shall return `FALSE` without calling
 *   `perform_module_action()`.
 *   <br>Traces to:
 *   - \hlrref{HLR_ERROR_PROCESS_ERROR_MODULE_01}
 * - \llrid{LLR_ERROR_PROCESS_ERROR_MODULE_02}: If `pok_error_level_select()`
 *   returns true for `pok_hm_module_selector` and false for
 *   `multi_partition_hm_selector` of `current_partition`,
 *   `process_error_module()` shall call `perform_module_action()` once with the
 *   result of `get_module_action()` for `multi_partition_hm_table` of the
 *   partition, `system_state` and `error_id`, and return `TRUE`.
 *   <br>Traces to:
 *   - \hlrref{HLR_ERROR_PROCESS_ERROR_MODULE_02}
 * - \llrid{LLR_ERROR_PROCESS_ERROR_MODULE_03}: If `pok_error_level_select()`
 *   returns false for `pok_hm_module_selector`, `process_error_module()` shall
 *   call `perform_module_action()` once with the result of
 *   `get_module_action()` for `pok_hm_module_table`, `system_state` and
 *   `error_id`, and return `TRUE`.
 *   <br>Traces to:
 *   - \hlrref{HLR_ERROR_PROCESS_ERROR_MODULE_02}
 *
 * @design
 * 1. If
 *    `pok_error_level_select(&pok_hm_module_selector, system_state, error_id)`
 *    is true: \llrref{LLR_ERROR_PROCESS_ERROR_MODULE_01},
 *    \llrref{LLR_ERROR_PROCESS_ERROR_MODULE_02}
 *    1. Set `part` to `current_partition`.
 *       \llrref{LLR_ERROR_PROCESS_ERROR_MODULE_01},
 *       \llrref{LLR_ERROR_PROCESS_ERROR_MODULE_02}
 *    2. If `pok_error_level_select(part->multi_partition_hm_selector,`
 *       `system_state, error_id)` is true:
 *       \llrref{LLR_ERROR_PROCESS_ERROR_MODULE_01}
 *       1. Return `FALSE`.
 *    3. Call `get_module_action()` with the arguments
 *       `part->multi_partition_hm_table`, `system_state` and `error_id`, and
 *       store the result in `action`.
 *       \llrref{LLR_ERROR_PROCESS_ERROR_MODULE_02}
 * 2. Otherwise: \llrref{LLR_ERROR_PROCESS_ERROR_MODULE_03}
 *    1. Call `get_module_action()` with the arguments the address of
 *       `pok_hm_module_table`, `system_state` and `error_id`, and store the
 *       result in `action`.
 * 3. Call `perform_module_action()` with the argument `action`.
 *    \llrref{LLR_ERROR_PROCESS_ERROR_MODULE_02},
 *    \llrref{LLR_ERROR_PROCESS_ERROR_MODULE_03}
 * 4. Return `TRUE`. \llrref{LLR_ERROR_PROCESS_ERROR_MODULE_02},
 *    \llrref{LLR_ERROR_PROCESS_ERROR_MODULE_03}
 *
 * @calls
 * - `pok_error_level_select()`
 * - `get_module_action()`
 * - `perform_module_action()`
 */
static pok_bool_t process_error_module(pok_system_state_t system_state,
    pok_error_id_t error_id)
{
    pok_error_module_action_t action;

    if(pok_error_level_select(&pok_hm_module_selector, system_state, error_id))
    {
        // Partition level error
        pok_partition_t* part = current_partition;

        if(pok_error_level_select(part->multi_partition_hm_selector,
            system_state, error_id)) {

            return FALSE; // Error should be processed in partition-specific way.
        }

        action = get_module_action(part->multi_partition_hm_table,
            system_state, error_id);
    }
    else
    {
        action = get_module_action(&pok_hm_module_table,
            system_state, error_id);
    }

    perform_module_action(action);

    return TRUE;
}

/**
 * @funcname `pok_raise_error`
 *
 * @description
 * Raises an error: a module-level one is handled at once, a partition-level one
 * is handed to the current partition's error processing.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `error_id` | `pok_error_id_t` | [in] The error. |
 * | `user_space` | `pok_bool_t` | [in] Whether it happened in user code. |
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
 * | `need_call_process_partition_error` | `pok_bool_t` | Whether the partition processes it |
 * | `part` | `pok_partition_t *` | The current partition |
 * | `preempt_local_disabled_old` | `pok_bool_t` | Whether local preemption was disabled |
 * | `system_state` | `pok_system_state_t` | The system state |
 *
 * @hlr
 * - \hlrid{HLR_ERROR_POK_RAISE_ERROR_01}: `pok_raise_error()` shall handle at
 *   once an error that the configuration makes an error of the module in the
 *   system state in which it happens.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_HM_001}
 *   - \srqref{SRQ_JET_HM_002}
 * - \hlrid{HLR_ERROR_POK_RAISE_ERROR_02}: `pok_raise_error()` shall give an
 *   error that is not an error of the module to the error processing of the
 *   current partition.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_HM_001}
 *   - \srqref{SRQ_JET_HM_002}
 *
 * @llr
 * - \llrid{LLR_ERROR_POK_RAISE_ERROR_01}: If `ja_preempt_enabled()` returns
 *   true, `pok_raise_error()` shall call `pok_preemption_disable()` once,
 *   before it calls `get_current_state()`.
 *   <br>Traces to:
 *   - \hlrref{HLR_ERROR_POK_RAISE_ERROR_01}
 *   - \hlrref{HLR_ERROR_POK_RAISE_ERROR_02}
 * - \llrid{LLR_ERROR_POK_RAISE_ERROR_02}: `pok_raise_error()` shall call
 *   `process_error_module()` once with the result of `get_current_state()` for
 *   `user_space`, and `error_id`.
 *   <br>Traces to:
 *   - \hlrref{HLR_ERROR_POK_RAISE_ERROR_01}
 * - \llrid{LLR_ERROR_POK_RAISE_ERROR_03}: If `process_error_module()` returns
 *   true, `pok_raise_error()` shall call `pok_preemption_enable()` if and only
 *   if `ja_preempt_enabled()` returned true, and call no operation of the
 *   partition.
 *   <br>Traces to:
 *   - \hlrref{HLR_ERROR_POK_RAISE_ERROR_01}
 * - \llrid{LLR_ERROR_POK_RAISE_ERROR_04}: If `process_error_module()` returns
 *   false and `part_ops` of `current_partition` and its
 *   `process_partition_error` are not NULL, `pok_raise_error()` shall set
 *   `preempt_local_disabled` of the partition to `FALSE`, call
 *   `pok_preemption_enable()` if and only if `preempt_local_disabled` was false
 *   before, and then call `process_partition_error` once with the state,
 *   `error_id`, the value that `preempt_local_disabled` had before and
 *   `failed_address`.
 *   <br>Traces to:
 *   - \hlrref{HLR_ERROR_POK_RAISE_ERROR_02}
 * - \llrid{LLR_ERROR_POK_RAISE_ERROR_05}: If `process_error_module()` returns
 *   false, and `part_ops` of `current_partition` is NULL or its
 *   `process_partition_error` is NULL, `pok_raise_error()` shall call
 *   `pok_preemption_enable()` if and only if `ja_preempt_enabled()` returned
 *   true, and call no operation of the partition.
 *   <br>Traces to:
 *   - \hlrref{HLR_ERROR_POK_RAISE_ERROR_02}
 *
 * @design
 * 1. Set `part` to `NULL`. \llrref{LLR_ERROR_POK_RAISE_ERROR_04}
 * 2. Set `need_call_process_partition_error` to `FALSE`.
 *    \llrref{LLR_ERROR_POK_RAISE_ERROR_04}
 * 3. Set `preempt_local_disabled_old` to `!ja_preempt_enabled()`.
 *    \llrref{LLR_ERROR_POK_RAISE_ERROR_01},
 *    \llrref{LLR_ERROR_POK_RAISE_ERROR_03},
 *    \llrref{LLR_ERROR_POK_RAISE_ERROR_05}
 * 4. If `preempt_local_disabled_old` is false:
 *    \llrref{LLR_ERROR_POK_RAISE_ERROR_01}
 *    1. Call `pok_preemption_disable()`.
 * 5. Call `get_current_state()` with the argument `user_space`, and store the
 *    result in `system_state`. \llrref{LLR_ERROR_POK_RAISE_ERROR_02}
 * 6. If `process_error_module(system_state, error_id)` is false:
 *    \llrref{LLR_ERROR_POK_RAISE_ERROR_02},
 *    \llrref{LLR_ERROR_POK_RAISE_ERROR_03},
 *    \llrref{LLR_ERROR_POK_RAISE_ERROR_04},
 *    \llrref{LLR_ERROR_POK_RAISE_ERROR_05}
 *    1. Set `part` to `current_partition`.
 *       \llrref{LLR_ERROR_POK_RAISE_ERROR_04},
 *       \llrref{LLR_ERROR_POK_RAISE_ERROR_05}
 *    2. If `part->part_ops` is not NULL and
 *       `part->part_ops->process_partition_error` is not NULL:
 *       \llrref{LLR_ERROR_POK_RAISE_ERROR_04}
 *       1. Set `need_call_process_partition_error` to `TRUE`.
 *       2. Set `preempt_local_disabled_old` to `part->preempt_local_disabled`.
 *       3. Set `part->preempt_local_disabled` to `FALSE`.
 * 7. If `preempt_local_disabled_old` is false:
 *    \llrref{LLR_ERROR_POK_RAISE_ERROR_03},
 *    \llrref{LLR_ERROR_POK_RAISE_ERROR_04},
 *    \llrref{LLR_ERROR_POK_RAISE_ERROR_05}
 *    1. Call `pok_preemption_enable()`.
 * 8. If `need_call_process_partition_error` is true:
 *    \llrref{LLR_ERROR_POK_RAISE_ERROR_04}
 *    1. Call the function `part->part_ops->process_partition_error` with the
 *       arguments `system_state`, `error_id`, `preempt_local_disabled_old` and
 *       `failed_address`.
 *
 * @calls
 * - `ja_preempt_enabled()`
 * - `pok_preemption_disable()`
 * - `get_current_state()`
 * - `process_error_module()`
 * - `pok_preemption_enable()`
 * - `part->part_ops->process_partition_error()` (indirect)
 */
void pok_raise_error(pok_error_id_t error_id, pok_bool_t user_space, void* failed_address)
{
    pok_system_state_t system_state;

    pok_partition_t* part = NULL;
    pok_bool_t need_call_process_partition_error = FALSE;
    pok_bool_t preempt_local_disabled_old = !ja_preempt_enabled();

    if(!preempt_local_disabled_old) {
        pok_preemption_disable();
    }

    system_state = get_current_state(user_space);

    if(!process_error_module(system_state, error_id))
    {
        part = current_partition;
        assert(part != NULL);
        if((part->part_ops != NULL)
           && (part->part_ops->process_partition_error != NULL))
        {
            need_call_process_partition_error = TRUE;
            preempt_local_disabled_old = part->preempt_local_disabled;
            part->preempt_local_disabled = FALSE;
        }
    }

    if(!preempt_local_disabled_old) {
        pok_preemption_enable();
    }

    if(need_call_process_partition_error) {
        part->part_ops->process_partition_error(system_state, error_id,
            preempt_local_disabled_old, failed_address);
    }
}

/**
 * @funcname `pok_raise_error_by_partition`
 *
 * @description
 * Raises an error found by a partition's own error processing: handles it if it
 * is a module-level error.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `system_state` | `pok_system_state_t` | [in] The system state (a partition state). |
 * | `error_id` | `pok_error_id_t` | [in] The error. |
 *
 * @return FALSE if it is a partition-level error, TRUE if it was handled.
 *
 * @globals
 * None.
 *
 * @locals
 * None.
 *
 * @hlr
 * - \hlrid{HLR_ERROR_POK_RAISE_ERROR_BY_PARTITION_01}:
 *   `pok_raise_error_by_partition()` shall handle an error that the error
 *   processing of a partition found, if it is an error of the module, and tell
 *   whether it did.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_HM_002}
 *   - \srqref{SRQ_JET_HM_003}
 *
 * @llr
 * - \llrid{LLR_ERROR_POK_RAISE_ERROR_BY_PARTITION_01}:
 *   `pok_raise_error_by_partition()` shall return the result of
 *   `process_error_module()` for `system_state` and `error_id`.
 *   <br>Traces to:
 *   - \hlrref{HLR_ERROR_POK_RAISE_ERROR_BY_PARTITION_01}
 *
 * @design
 * 1. Call `process_error_module()` with the arguments `system_state` and
 *    `error_id`, and return its result.
 *    \llrref{LLR_ERROR_POK_RAISE_ERROR_BY_PARTITION_01}
 *
 * @calls
 * - `process_error_module()`
 */
pok_bool_t pok_raise_error_by_partition(pok_system_state_t system_state,
    pok_error_id_t error_id)
{
    assert(system_state >= POK_SYSTEM_STATE_MIN_PARTITION);

    return process_error_module(system_state, error_id);
}

// Default HM multi-partition selector - all errors are partition-level.
pok_error_levels_t pok_hm_multi_partition_selector_default = {
    .levels = {
        0x7f,   /* POK_ERROR_ID_MODPOSTPROCEVENT_ELIST */
        0x7f,   /* POK_ERROR_ID_ILLEGAL_REQUEST */
        0x7f,   /* POK_ERROR_ID_APPLICATION_ERROR */
        0x7f,   /* POK_ERROR_ID_PARTLOAD_ERROR */
        0x7f,   /* POK_ERROR_ID_NUMERIC_ERROR */
        0x7f,   /* POK_ERROR_ID_MEMORY_VIOLATION */
        0x7f,   /* POK_ERROR_ID_DEADLINE_MISSED */
        0x7f,   /* POK_ERROR_ID_HARDWARE_FAULT */
        0x7f,   /* POK_ERROR_ID_POWER_FAIL */
        0x7f,   /* POK_ERROR_ID_STACK_OVERFLOW */
        0x7f,   /* POK_ERROR_ID_PROCINIT_ERROR */
        0x7f,   /* POK_ERROR_ID_NOMEMORY_PROCDATA */
        0x7f,   /* POK_ERROR_ID_ASSERT */
        0x7f,   /* POK_ERROR_ID_CONFIG_ERROR */
        0x7f,   /* POK_ERROR_ID_CHECK_POOL */
        0x7f,   /* POK_ERROR_ID_UNHANDLED_INT */
    }
};

/*
 * Default HM multi-partition table - shutdown for all errors.
 *
 * Every element is given (MISRA C:2025 Rule 9.3): the rows used to list
 * only the first error ID, so the others were 0, i.e. IGNORE.
 */
pok_error_hm_module_t pok_hm_multi_partition_table_default = {
    .actions = {
        SHUTDOWN_FOR_ALL_ERRORS, /* POK_SYSTEM_STATE_INIT_PARTOS */
        SHUTDOWN_FOR_ALL_ERRORS, /* POK_SYSTEM_STATE_INIT_PARTUSER */
        SHUTDOWN_FOR_ALL_ERRORS, /* POK_SYSTEM_STATE_INTERRUPT_HANDLER */
        SHUTDOWN_FOR_ALL_ERRORS, /* POK_SYSTEM_STATE_OS_MOD */
        SHUTDOWN_FOR_ALL_ERRORS, /* POK_SYSTEM_STATE_OS_PART */
        SHUTDOWN_FOR_ALL_ERRORS, /* POK_SYSTEM_STATE_ERROR_HANDLER */
        SHUTDOWN_FOR_ALL_ERRORS, /* POK_SYSTEM_STATE_USER */
    }
};
