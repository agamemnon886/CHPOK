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
 *
 * @llr
 *
 * @design
 * 1. Return `table->actions[system_state][error_id]`.
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
 *
 * @llr
 *
 * @design
 * 1. Set `part->start_condition` to `POK_START_CONDITION_HM_MODULE_RESTART`.
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
 *
 * @llr
 *
 * @design
 * 1. If `action` is equal to `POK_ERROR_MODULE_ACTION_IGNORE`:
 *    1. Return.
 * 2. Call `ja_preempt_disable()`.
 * 3. Depending on `action`:
 *    1. Case `POK_ERROR_MODULE_ACTION_RESET`:
 *       1. Call `pok_cons_write()` with the arguments `msg_reset` and
 *          `sizeof(msg_reset) - 1U`.
 *       2. Call `for_each_partition()` with the address of
 *          `set_hm_module_restart`.
 *       3. Call `pok_sched_restart()`.
 *       4. Leave the switch.
 *    2. Case `POK_ERROR_MODULE_ACTION_SHUTDOWN`:
 *       1. Call `pok_cons_write()` with the arguments `msg_shutdown` and
 *          `sizeof(msg_shutdown) - 1U`.
 *       2. Call `ja_inf_loop()`.
 *       3. Leave the switch.
 *    3. Otherwise (default):
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
 *
 * @llr
 *
 * @design
 * 1. If `kernel_state` is not equal to `POK_SYSTEM_STATE_OS_PART`:
 *    1. Return `kernel_state`.
 * 2. Set `part` to `current_partition`.
 * 3. If `user_space` is false:
 *    1. Return `POK_SYSTEM_STATE_OS_PART`.
 * 4. Return `POK_SYSTEM_STATE_ERROR_HANDLER` if `part->is_error_handler` is
 *    true, otherwise `POK_SYSTEM_STATE_USER`.
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
 *
 * @llr
 *
 * @design
 * 1. If
 *    `pok_error_level_select(&pok_hm_module_selector, system_state, error_id)`
 *    is true:
 *    1. Set `part` to `current_partition`.
 *    2. If `pok_error_level_select(part->multi_partition_hm_selector,`
 *       `system_state, error_id)` is true:
 *       1. Return `FALSE`.
 *    3. Call `get_module_action()` with the arguments
 *       `part->multi_partition_hm_table`, `system_state` and `error_id`, and
 *       store the result in `action`.
 * 2. Otherwise:
 *    1. Call `get_module_action()` with the arguments the address of
 *       `pok_hm_module_table`, `system_state` and `error_id`, and store the
 *       result in `action`.
 * 3. Call `perform_module_action()` with the argument `action`.
 * 4. Return `TRUE`.
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
 *
 * @llr
 *
 * @design
 * 1. Set `part` to `NULL`.
 * 2. Set `need_call_process_partition_error` to `FALSE`.
 * 3. Set `preempt_local_disabled_old` to `!ja_preempt_enabled()`.
 * 4. If `preempt_local_disabled_old` is false:
 *    1. Call `pok_preemption_disable()`.
 * 5. Call `get_current_state()` with the argument `user_space`, and store the
 *    result in `system_state`.
 * 6. If `process_error_module(system_state, error_id)` is false:
 *    1. Set `part` to `current_partition`.
 *    2. Check that `part != NULL` holds (assert: stop otherwise).
 *    3. If `part->part_ops` is not NULL and
 *       `part->part_ops->process_partition_error` is not NULL:
 *       1. Set `need_call_process_partition_error` to `TRUE`.
 *       2. Set `preempt_local_disabled_old` to `part->preempt_local_disabled`.
 *       3. Set `part->preempt_local_disabled` to `FALSE`.
 * 7. If `preempt_local_disabled_old` is false:
 *    1. Call `pok_preemption_enable()`.
 * 8. If `need_call_process_partition_error` is true:
 *    1. Call the function `part->part_ops->process_partition_error` with the
 *       arguments `system_state`, `error_id`, `preempt_local_disabled_old` and
 *       `failed_address`.
 *
 * @calls
 * - `ja_preempt_enabled()`
 * - `pok_preemption_disable()`
 * - `get_current_state()`
 * - `process_error_module()`
 * - `pok_assertion_fail()`
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
 *
 * @llr
 *
 * @design
 * 1. Check that `system_state >= POK_SYSTEM_STATE_MIN_PARTITION` holds (assert:
 *    stop otherwise).
 * 2. Call `process_error_module()` with the arguments `system_state` and
 *    `error_id`, and return its result.
 *
 * @calls
 * - `pok_assertion_fail()`
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
