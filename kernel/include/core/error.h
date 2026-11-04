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
 */

#ifndef POK_CORE_ERROR_H
#define POK_CORE_ERROR_H

#include <assert.h>

/* Global error processing. */

typedef uint8_t pok_error_id_t;

/* Possible error identificators */
#define POK_ERROR_ID_MODPOSTPROCEVENT_ELIST 0U
#define POK_ERROR_ID_ILLEGAL_REQUEST        1U
#define POK_ERROR_ID_APPLICATION_ERROR      2U
#define POK_ERROR_ID_PARTLOAD_ERROR         3U
#define POK_ERROR_ID_NUMERIC_ERROR          4U
#define POK_ERROR_ID_MEMORY_VIOLATION       5U
#define POK_ERROR_ID_DEADLINE_MISSED        6U
#define POK_ERROR_ID_HARDWARE_FAULT         7U
#define POK_ERROR_ID_POWER_FAIL             8U
#define POK_ERROR_ID_STACK_OVERFLOW         9U
#define POK_ERROR_ID_PROCINIT_ERROR         10U
#define POK_ERROR_ID_NOMEMORY_PROCDATA      11U
#define POK_ERROR_ID_ASSERT                 12U
#define POK_ERROR_ID_CONFIG_ERROR           13U
#define POK_ERROR_ID_CHECK_POOL             14U
#define POK_ERROR_ID_UNHANDLED_INT          15U

// Not an error. This special value may be used somewhere.
#define POK_ERROR_ID_NONE                   255U
/* Maximum error identificator */
#define POK_ERROR_ID_MAX 15U

typedef uint8_t pok_system_state_t;

/* Possible system states */
#define POK_SYSTEM_STATE_INIT_PARTOS        0U
#define POK_SYSTEM_STATE_INIT_PARTUSER      1U
#define POK_SYSTEM_STATE_INTERRUPT_HANDLER  2U
#define POK_SYSTEM_STATE_OS_MOD             3U
// This is a partition state while it is in kernel space.
#define POK_SYSTEM_STATE_OS_PART            4U
/* 
 * This is a partition state while it is in user space
 * and .is_error_handler field is set.
 */
#define POK_SYSTEM_STATE_ERROR_HANDLER      5U
/* 
 * This is a partition state while it is in user space
 * and .is_error_handler field is NOT set.
 */
#define POK_SYSTEM_STATE_USER               6U


/* Maximum system state. Should be less than 7. */
#define POK_SYSTEM_STATE_MAX                6U
/* 
 * Minimum system state identificator, which can be used for raise error
 * by partition.
 */
#define POK_SYSTEM_STATE_MIN_PARTITION  POK_SYSTEM_STATE_OS_PART

/* 
 * Selector for error level.
 */
typedef struct {
    // Bit levels[ID]:STATE selects one of two error levels.
    uint8_t levels[POK_ERROR_ID_MAX + 1U];
} pok_error_level_selector_t;

/* 
 * Return classification of the error according to the selector
 * (FALSE - 0, TRUE - 1).
 */
static inline pok_bool_t pok_error_level_select(
    const pok_error_level_selector_t* selector,
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
extern pok_error_level_selector_t pok_hm_module_selector;


typedef uint8_t pok_error_module_action_t;

#define POK_ERROR_MODULE_ACTION_IGNORE      0U
#define POK_ERROR_MODULE_ACTION_SHUTDOWN    1U 
#define POK_ERROR_MODULE_ACTION_RESET       2U
// Platform-specific actions could be added there.



/*
 * Table of actions for module-level errors.
 * 
 */
typedef struct {
    /* 
     * actions[STATE][ID] give action, which should be taken on module level.
     * 
     * Only those [STATE][ID] cells are used, which corresponds to
     * module-level errors.
     */
    pok_error_module_action_t
        actions[POK_SYSTEM_STATE_MAX + 1U][POK_ERROR_ID_MAX + 1U];
} pok_error_module_action_table_t ;

/*
 * Module HM table. Should be defined in deployment.c.
 */
extern pok_error_module_action_table_t pok_hm_module_table;

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


// Current state of the module or POK_SYSTEM_STATE_OS_PART.
extern pok_system_state_t kernel_state;

/********************* Default HM Multi-partition tables **********************/
// Default HM multi-partition selector - all errors are partition-level.
extern pok_error_level_selector_t pok_hm_multi_partition_selector_default;

// Default HM multi-partition table - shutdown for all errors.
extern pok_error_module_action_table_t pok_hm_multi_partition_table_default;

#endif /* POK_CORE_ERROR_H */
