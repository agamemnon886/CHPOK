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
 * Modified by Kirill Brazhnikov, 2026-11-02: MISRA C:2025: the header
 * guard isn't a reserved identifier; essentially Boolean conditions; U
 * suffixes for unsigned constants.
 *
 * Modified by Kirill Brazhnikov, 2026-11-03: long lines are wrapped at 80
 * columns.
 *
 * Modified by Kirill Brazhnikov, 2026-11-08: Doxygen headers for the file
 * and its functions.
 *
 * Modified by Kirill Brazhnikov, 2026-11-12: the function declarations are
 * hidden from Doxygen (\cond), which would otherwise document the functions a
 * second time, in the section of this header.
 *
 * Modified by Kirill Brazhnikov, 2026-11-13: Doxygen comments for its types and
 * its macros; pok_error_module_action_table_t is renamed pok_error_hm_module_t,
 * pok_error_level_selector_t is renamed pok_error_levels_t (names too long for
 * the tables of the documentation); the unused macro POK_ERROR_ID_NONE is
 * removed.
 *
 * Modified by Kirill Brazhnikov, 2026-11-14: the macro SHUTDOWN_FOR_ALL_ERRORS
 * of kernel/core/error.c (with its check of POK_ERROR_ID_MAX) is moved here;
 * the Doxygen comment of the file is removed (header files are not documented);
 * the Doxygen headers of the static inline functions are plain comments (they
 * are not documented).
 */

#ifndef POK_CORE_ERROR_H
#define POK_CORE_ERROR_H

#include <assert.h>

/* Global error processing. */

/**
 * @typename `pok_error_id_t`
 *
 * @description
 * The ID of an error (POK_ERROR_ID_*): the errors of the health monitoring
 * tables.
 */
typedef uint8_t pok_error_id_t;

/**
 * @name Error identifiers
 * Possible error identificators
 * @{
 */

/**
 * @macroname `POK_ERROR_ID_MODPOSTPROCEVENT_ELIST`
 *
 * @description
 * A module post-processing event list error.
 */
#define POK_ERROR_ID_MODPOSTPROCEVENT_ELIST 0U

/**
 * @macroname `POK_ERROR_ID_ILLEGAL_REQUEST`
 *
 * @description
 * An illegal request (e.g. an invalid instruction or syscall).
 */
#define POK_ERROR_ID_ILLEGAL_REQUEST        1U

/**
 * @macroname `POK_ERROR_ID_APPLICATION_ERROR`
 *
 * @description
 * An error raised by the application.
 */
#define POK_ERROR_ID_APPLICATION_ERROR      2U

/**
 * @macroname `POK_ERROR_ID_PARTLOAD_ERROR`
 *
 * @description
 * A partition load error.
 */
#define POK_ERROR_ID_PARTLOAD_ERROR         3U

/**
 * @macroname `POK_ERROR_ID_NUMERIC_ERROR`
 *
 * @description
 * A numeric error (e.g. a division by zero).
 */
#define POK_ERROR_ID_NUMERIC_ERROR          4U

/**
 * @macroname `POK_ERROR_ID_MEMORY_VIOLATION`
 *
 * @description
 * A memory violation.
 */
#define POK_ERROR_ID_MEMORY_VIOLATION       5U

/**
 * @macroname `POK_ERROR_ID_DEADLINE_MISSED`
 *
 * @description
 * A deadline missed.
 */
#define POK_ERROR_ID_DEADLINE_MISSED        6U

/**
 * @macroname `POK_ERROR_ID_HARDWARE_FAULT`
 *
 * @description
 * A hardware fault.
 */
#define POK_ERROR_ID_HARDWARE_FAULT         7U

/**
 * @macroname `POK_ERROR_ID_POWER_FAIL`
 *
 * @description
 * A power failure.
 */
#define POK_ERROR_ID_POWER_FAIL             8U

/**
 * @macroname `POK_ERROR_ID_STACK_OVERFLOW`
 *
 * @description
 * A stack overflow.
 */
#define POK_ERROR_ID_STACK_OVERFLOW         9U

/**
 * @macroname `POK_ERROR_ID_PROCINIT_ERROR`
 *
 * @description
 * A process initialization error.
 */
#define POK_ERROR_ID_PROCINIT_ERROR         10U

/**
 * @macroname `POK_ERROR_ID_NOMEMORY_PROCDATA`
 *
 * @description
 * No memory for the data of a process.
 */
#define POK_ERROR_ID_NOMEMORY_PROCDATA      11U

/**
 * @macroname `POK_ERROR_ID_ASSERT`
 *
 * @description
 * A failed assertion.
 */
#define POK_ERROR_ID_ASSERT                 12U

/**
 * @macroname `POK_ERROR_ID_CONFIG_ERROR`
 *
 * @description
 * A configuration error.
 */
#define POK_ERROR_ID_CONFIG_ERROR           13U

/**
 * @macroname `POK_ERROR_ID_CHECK_POOL`
 *
 * @description
 * A check of a memory pool failed.
 */
#define POK_ERROR_ID_CHECK_POOL             14U

/**
 * @macroname `POK_ERROR_ID_UNHANDLED_INT`
 *
 * @description
 * An unhandled interrupt.
 */
#define POK_ERROR_ID_UNHANDLED_INT          15U

/** @} */


/**
 * @macroname `POK_ERROR_ID_MAX`
 *
 * @description
 * Maximum error identificator.
 */
#define POK_ERROR_ID_MAX 15U

/**
 * @typename `pok_system_state_t`
 *
 * @description
 * The state of the system when an error occurs (POK_SYSTEM_STATE_*): it selects
 * the level of the error and the action.
 */
typedef uint8_t pok_system_state_t;

/**
 * @name System states
 * Possible system states
 * @{
 */

/**
 * @macroname `POK_SYSTEM_STATE_INIT_PARTOS`
 *
 * @description
 * The kernel initializes a partition (its OS part).
 */
#define POK_SYSTEM_STATE_INIT_PARTOS        0U

/**
 * @macroname `POK_SYSTEM_STATE_INIT_PARTUSER`
 *
 * @description
 * A partition initializes itself (user space, before NORMAL mode).
 */
#define POK_SYSTEM_STATE_INIT_PARTUSER      1U

/**
 * @macroname `POK_SYSTEM_STATE_INTERRUPT_HANDLER`
 *
 * @description
 * The kernel handles an interrupt.
 */
#define POK_SYSTEM_STATE_INTERRUPT_HANDLER  2U

/**
 * @macroname `POK_SYSTEM_STATE_OS_MOD`
 *
 * @description
 * The kernel runs for the module (not for a partition).
 */
#define POK_SYSTEM_STATE_OS_MOD             3U

/**
 * @macroname `POK_SYSTEM_STATE_OS_PART`
 *
 * @description
 * This is a partition state while it is in kernel space.
 */
#define POK_SYSTEM_STATE_OS_PART            4U

/**
 * @macroname `POK_SYSTEM_STATE_ERROR_HANDLER`
 *
 * @description
 * This is a partition state while it is in user space and .is_error_handler
 * field is set.
 */
#define POK_SYSTEM_STATE_ERROR_HANDLER      5U

/**
 * @macroname `POK_SYSTEM_STATE_USER`
 *
 * @description
 * This is a partition state while it is in user space and .is_error_handler
 * field is NOT set.
 */
#define POK_SYSTEM_STATE_USER               6U

/** @} */



/**
 * @macroname `POK_SYSTEM_STATE_MAX`
 *
 * @description
 * Maximum system state. Should be less than 7.
 */
#define POK_SYSTEM_STATE_MAX                6U

/**
 * @macroname `POK_SYSTEM_STATE_MIN_PARTITION`
 *
 * @description
 * Minimum system state identificator, which can be used for raise error by
 * partition.
 */
#define POK_SYSTEM_STATE_MIN_PARTITION  POK_SYSTEM_STATE_OS_PART

/**
 * @typename `pok_error_levels_t`
 *
 * @description
 * Selector for error level.
 */
typedef struct {
    // Bit levels[ID]:STATE selects one of two error levels.
    /**
     * Bit levels[ID]:STATE selects one of two error levels.
     */
    uint8_t levels[POK_ERROR_ID_MAX + 1U];
} pok_error_levels_t;

/*
 * Tells whether an error in a system state is a partition-level error, from a
 * selector (one bit per state for each error).
 */
static inline pok_bool_t pok_error_level_select(
    const pok_error_levels_t* selector,
    pok_system_state_t system_state,
    pok_error_id_t error_id)
{
    assert(system_state < 8U);

    uint8_t level_val = selector->levels[error_id];
    
    return (level_val & (1U << system_state)) != 0U;
}


/*
 * Module-level error selector.
 * 
 * In this selector 0 value means module level, 1 - partition level.
 * 
 * Should be defined in deployment.c.
 */
extern pok_error_levels_t pok_hm_module_selector;


/**
 * @typename `pok_error_module_action_t`
 *
 * @description
 * The action taken for a module-level error (POK_ERROR_MODULE_ACTION_*).
 */
typedef uint8_t pok_error_module_action_t;

/**
 * @name Module-level actions
 * The values of pok_error_module_action_t. Platform-specific actions could be
 * added there.
 * @{
 */

/**
 * @macroname `POK_ERROR_MODULE_ACTION_IGNORE`
 *
 * @description
 * Ignore the error.
 */
#define POK_ERROR_MODULE_ACTION_IGNORE      0U

/**
 * @macroname `POK_ERROR_MODULE_ACTION_SHUTDOWN`
 *
 * @description
 * Stop the module.
 */
#define POK_ERROR_MODULE_ACTION_SHUTDOWN    1U 

/**
 * @macroname `POK_ERROR_MODULE_ACTION_RESET`
 *
 * @description
 * Restart the module.
 */
#define POK_ERROR_MODULE_ACTION_RESET       2U

/** @} */

// Platform-specific actions could be added there.



/**
 * @typename `pok_error_hm_module_t`
 *
 * @description
 * Table of actions for module-level errors.
 */
typedef struct {
    /**
     * actions[STATE][ID] give action, which should be taken on module level.
     *
     * Only those [STATE][ID] cells are used, which corresponds to
     * module-level errors.
     */
    pok_error_module_action_t
        actions[POK_SYSTEM_STATE_MAX + 1U][POK_ERROR_ID_MAX + 1U];
} pok_error_hm_module_t ;

/*
 * Module HM table. Should be defined in deployment.c.
 */
extern pok_error_hm_module_t pok_hm_module_table;

/* 
 * Raise error with given identificator.
 * 
 * Error level will be determined using generic mechanism,
 * and action will be taken.
 * 
 * @arg user_space: True if error is originated in user space, false otherwise.
 * 
 * @arg failed_address: May contain address of failed instruction,
 * if it is applicable for given error.
 */
/** \cond */
void pok_raise_error(pok_error_id_t error_id, pok_bool_t user_space, void* failed_address);

/*
 * Raise error from the partition.
 * 
 * @arg system_state: One of the *partition state*
 * (at least, POK_SYSTEM_STATE_MIN_PARTITION).
 * 
 * Returns TRUE if error is module-level, and it is ignored.
 * 
 * Do not return, if error is module-level and appropriate action
 * is taken for module except ignoring.
 * 
 * Returns FALSE if error is partition-level. In this case partition
 * may process error in partition-specific manner.
 */
pok_bool_t pok_raise_error_by_partition(pok_system_state_t system_state,
    pok_error_id_t error_id);

/** \endcond */


// Current state of the module or POK_SYSTEM_STATE_OS_PART.
extern pok_system_state_t kernel_state;

/********************* Default HM Multi-partition tables **********************/
// Default HM multi-partition selector - all errors are partition-level.
extern pok_error_levels_t pok_hm_multi_partition_selector_default;

// Default HM multi-partition table - shutdown for all errors.
extern pok_error_hm_module_t pok_hm_multi_partition_table_default;

_Static_assert(POK_ERROR_ID_MAX == 15U, "update SHUTDOWN_FOR_ALL_ERRORS");

/**
 * @macroname `SHUTDOWN_FOR_ALL_ERRORS`
 *
 * @description
 * The initializer of one row of pok_hm_multi_partition_table_default (one
 * system state): the action POK_ERROR_MODULE_ACTION_SHUTDOWN for each of the
 * POK_ERROR_ID_MAX + 1 error IDs.
 *
 * Every element is given (MISRA C:2025 Rule 9.3): the rows used to list only
 * the first error ID, so the others were 0, i.e. IGNORE.
 */
#define SHUTDOWN_FOR_ALL_ERRORS { \
    POK_ERROR_MODULE_ACTION_SHUTDOWN, POK_ERROR_MODULE_ACTION_SHUTDOWN, \
    POK_ERROR_MODULE_ACTION_SHUTDOWN, POK_ERROR_MODULE_ACTION_SHUTDOWN, \
    POK_ERROR_MODULE_ACTION_SHUTDOWN, POK_ERROR_MODULE_ACTION_SHUTDOWN, \
    POK_ERROR_MODULE_ACTION_SHUTDOWN, POK_ERROR_MODULE_ACTION_SHUTDOWN, \
    POK_ERROR_MODULE_ACTION_SHUTDOWN, POK_ERROR_MODULE_ACTION_SHUTDOWN, \
    POK_ERROR_MODULE_ACTION_SHUTDOWN, POK_ERROR_MODULE_ACTION_SHUTDOWN, \
    POK_ERROR_MODULE_ACTION_SHUTDOWN, POK_ERROR_MODULE_ACTION_SHUTDOWN, \
    POK_ERROR_MODULE_ACTION_SHUTDOWN, POK_ERROR_MODULE_ACTION_SHUTDOWN }

#endif /* POK_CORE_ERROR_H */
