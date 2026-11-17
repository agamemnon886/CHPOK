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
 * Modified by Kirill Brazhnikov, 2026-11-02: MISRA C:2025: braces around
 * all if/else/loop bodies; essentially Boolean conditions; U suffixes for
 * unsigned constants; discarded results are cast to void; the partition
 * error handler takes a pok_bool_t; monitor() and
 * pok_monitor_thread_init() are declared with (void); atoi() is replaced
 * by parse_number(), which saturates instead of overflowing; negative
 * partition numbers are rejected (they were used as an array index);
 * command and partition loops use unsigned counters; NULL for null
 * pointers.
 *
 * Modified by Kirill Brazhnikov, 2026-11-03: MISRA C:2025:
 * monitor_process_error() has internal linkage; one declarator per
 * declaration; increments and assignments are separate statements; unused
 * parameters are cast to void; the operations structure is defined in the
 * function that uses it; runcmd() doesn't modify its parameter; operands
 * of mixed precedence are parenthesized; deviations are documented where
 * a guideline can't be followed. Long lines are wrapped at 80 columns.
 *
 * Modified by Kirill Brazhnikov, 2026-11-08: Doxygen headers for the file
 * and its functions.
 *
 * Modified by Kirill Brazhnikov, 2026-11-13: the structures and enums are used
 * by their typedef names: jet_space_layout_t, pok_part_ops_t; the macros, the
 * type and the declarations of the commands are moved to kernel/include/libc.h;
 * struct Command is the typedef command_t.
 *
 * Modified by Kirill Brazhnikov, 2026-11-14: The function headers no longer
 * have a step for the declaration of a local variable; the @locals table
 * describes it, and an initialized one is set in a step.
 *
 * Modified by Kirill Brazhnikov, 2026-11-15: moved from kernel/libc/monitor.c
 * (kernel/libc is split up: the GDB server, the monitor and readline are in
 * kernel/debug).
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
 * The kernel monitor: a kernel-only partition that, when a character comes
 * on the main console, pauses the ARINC 653 partitions and runs a command
 * line (help, ps, info_partition, pause, resume, restart, reset, exit).
 */


#include <config.h>

#include <libc.h>
#include <asp/arch.h>
#include <core/partition_arinc.h>
#include <cons.h>

/*
 * MISRA C:2025 Rule 18.4 deviation (this file): thread and object IDs are
 * the index of an element, computed as the difference between its address
 * and the start of its array.
 */


#ifdef POK_NEEDS_NETWORKING
/**
 * @funcname `pok_network_thread_init`
 *
 * @description
 * Sets up the network thread (old POK interface, POK_NEEDS_NETWORKING): its
 * entry and its context.
 *
 * @globals
 * None.
 *
 * @locals
 * None.
 *
 * @hlr
 * - \hlrid{HLR_MONITOR_POK_NETWORK_THREAD_INIT_01}: `pok_network_thread_init()`
 *   shall do nothing: the network thread of the old POK interface is not used.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_SCHED_002}
 *
 * @llr
 * - \llrid{LLR_MONITOR_POK_NETWORK_THREAD_INIT_01}: `pok_network_thread_init()`
 *   shall return without any effect.
 *   <br>Traces to:
 *   - \hlrref{HLR_MONITOR_POK_NETWORK_THREAD_INIT_01}
 *
 * @design
 * 1. Do nothing. \llrref{LLR_MONITOR_POK_NETWORK_THREAD_INIT_01}
 *
 * @calls
 * None.
 */
static void pok_network_thread_init(void)
{
    pok_threads[NETWORK_THREAD].entry = pok_network_thread;
    pok_threads[NETWORK_THREAD].sp = pok_context_create(NETWORK_THREAD, 4096, (uintptr_t) pok_network_thread);
}
#endif




/*
 *  Don't use '[' in your command!
 */



/*
 * Convert string in int
 */

/**
 * @funcname `parse_number`
 *
 * @description
 * Reads a decimal number at the start of a string, saturating at INT32_MAX.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `s` | `const char *` | [in] The string. |
 *
 * @return The number (0 if there are no digits).
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `digit` | `int32_t` | The value of the digit |
 * | `i` | `int32_t` | The index of the digit |
 * | `n` | `int32_t` | The number so far |
 *
 * @hlr
 * - \hlrid{HLR_MONITOR_PARSE_NUMBER_01}: `parse_number()` shall read the
 *   decimal number at the start of a word of a monitor command, 0 without
 *   digits, and at most `INT32_MAX`.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_DBG_008}
 *
 * @llr
 * - \llrid{LLR_MONITOR_PARSE_NUMBER_01}: `parse_number()` shall return the
 *   decimal number of the digits (`'0'` to `'9'`) at the start of `s`, up to
 *   the first character that is less than `'0'` or greater than `'9'`, and 0 if
 *   `s` starts with no digit.
 *   <br>Traces to:
 *   - \hlrref{HLR_MONITOR_PARSE_NUMBER_01}
 * - \llrid{LLR_MONITOR_PARSE_NUMBER_02}: If the number is greater than
 *   `INT32_MAX` minus a digit, divided by 10, when it reads that digit,
 *   `parse_number()` shall replace the number with `INT32_MAX`.
 *   <br>Traces to:
 *   - \hlrref{HLR_MONITOR_PARSE_NUMBER_01}
 * - \llrid{LLR_MONITOR_PARSE_NUMBER_03}: If the number is not greater than
 *   `INT32_MAX` minus a digit, divided by 10, when it reads that digit,
 *   `parse_number()` shall replace the number with 10 times the number plus the
 *   digit.
 *   <br>Traces to:
 *   - \hlrref{HLR_MONITOR_PARSE_NUMBER_01}
 *
 * @design
 * 1. Set `n` to `0`. \llrref{LLR_MONITOR_PARSE_NUMBER_01}
 * 2. Set `i` to `0`; then while `s[i]` is greater than or equal to `'0'` and
 *    `s[i]` is less than or equal to `'9'`, repeat (after each pass: increment
 *    `i`): \llrref{LLR_MONITOR_PARSE_NUMBER_01},
 *    \llrref{LLR_MONITOR_PARSE_NUMBER_02}, \llrref{LLR_MONITOR_PARSE_NUMBER_03}
 *    1. Set `digit` to `s[i] - '0'`.
 *    2. If `n` is greater than `((INT32_MAX - digit) / 10)`:
 *       \llrref{LLR_MONITOR_PARSE_NUMBER_02}
 *       1. Set `n` to `INT32_MAX`.
 *    3. Otherwise: \llrref{LLR_MONITOR_PARSE_NUMBER_03}
 *       1. Set `n` to `(10 * n) + digit`.
 * 3. Return `n`. \llrref{LLR_MONITOR_PARSE_NUMBER_01}
 *
 * @calls
 * None.
 */
/* Decimal number at the start of s (MISRA C:2025 Rule 21.7: not the library
 * function atoi). */
static int32_t parse_number(const char * s)
{
    int32_t i;
    int32_t n = 0;
    for (i = 0; (s[i] >= '0') && (s[i] <= '9'); ++i) {
        int32_t digit = s[i] - '0';

        // Saturate instead of overflowing.
        if (n > ((INT32_MAX - digit) / 10)) {
            n = INT32_MAX;
        } else {
            n = (10 * n) + digit;
        }
    }
    return n;
}

/* 
 * N-th bit means that N-th ARINC partition should paused when
 * monitor leaves active mode.
 */
static uint32_t partition_on_pause_bits = 0U;

/**
 * @funcname `partition_pause_set`
 *
 * @description
 * Records whether a partition is to stay paused after the monitor.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `id` | `pok_partition_id_t` | [in] The partition (below 32). |
 * | `is_paused` | `uint8_t` | [in] Non-zero to keep it paused. |
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `partition_on_pause_bits` | `uint32_t` | One bit per partition kept paused |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `mask` | `uint32_t` | The partition's bit |
 *
 * @hlr
 * - \hlrid{HLR_MONITOR_PARTITION_PAUSE_SET_01}: `partition_pause_set()` shall
 *   record whether a partition is to stay paused when the monitor is left.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_DBG_008}
 *   - \srqref{SRQ_JET_DBG_016}
 *
 * @llr
 * - \llrid{LLR_MONITOR_PARTITION_PAUSE_SET_01}: If `is_paused` is not 0,
 *   `partition_pause_set()` shall set the bit `id` of
 *   `partition_on_pause_bits`.
 *   <br>Traces to:
 *   - \hlrref{HLR_MONITOR_PARTITION_PAUSE_SET_01}
 * - \llrid{LLR_MONITOR_PARTITION_PAUSE_SET_02}: If `is_paused` is 0,
 *   `partition_pause_set()` shall clear the bit `id` of
 *   `partition_on_pause_bits`.
 *   <br>Traces to:
 *   - \hlrref{HLR_MONITOR_PARTITION_PAUSE_SET_01}
 *
 * @design
 * 1. Set `mask` to `(uint32_t) 1U << id`.
 *    \llrref{LLR_MONITOR_PARTITION_PAUSE_SET_01},
 *    \llrref{LLR_MONITOR_PARTITION_PAUSE_SET_02}
 * 2. If `is_paused` is not equal to `0U`:
 *    \llrref{LLR_MONITOR_PARTITION_PAUSE_SET_01}
 *    1. Set the bits `mask` in `partition_on_pause_bits`.
 * 3. Otherwise: \llrref{LLR_MONITOR_PARTITION_PAUSE_SET_02}
 *    1. Keep only the bits `~mask` of `partition_on_pause_bits`.
 *
 * @calls
 * None.
 */
static void partition_pause_set(pok_partition_id_t id, uint8_t is_paused)
{
    assert(id < 32U);
    
    uint32_t mask = (uint32_t) 1U << id;
    
    if(is_paused != 0U) {
        partition_on_pause_bits |= mask;
    } else {
        partition_on_pause_bits &= (~mask);
    }
}
/**
 * @funcname `partition_pause_get`
 *
 * @description
 * Tells whether a partition is to stay paused after the monitor.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `id` | `pok_partition_id_t` | [in] The partition (below 32). |
 *
 * @return 1 if it stays paused, 0 otherwise.
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `partition_on_pause_bits` | `uint32_t` | One bit per partition kept paused |
 *
 * @locals
 * None.
 *
 * @hlr
 * - \hlrid{HLR_MONITOR_PARTITION_PAUSE_GET_01}: `partition_pause_get()` shall
 *   tell whether a partition is to stay paused when the monitor is left.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_DBG_016}
 *
 * @llr
 * - \llrid{LLR_MONITOR_PARTITION_PAUSE_GET_01}: `partition_pause_get()` shall
 *   return the bit `id` of `partition_on_pause_bits`, as 0 or 1.
 *   <br>Traces to:
 *   - \hlrref{HLR_MONITOR_PARTITION_PAUSE_GET_01}
 *
 * @design
 * 1. Return `(uint8_t) ((partition_on_pause_bits >> id) & 1U)`.
 *    \llrref{LLR_MONITOR_PARTITION_PAUSE_GET_01}
 *
 * @calls
 * None.
 */
static uint8_t partition_pause_get(pok_partition_id_t id)
{
    assert(id < 32U);
    
    return (uint8_t) ((partition_on_pause_bits >> id) & 1U);
}

static pok_bool_t want_to_exit=FALSE; 

static command_t commands[] = {
    { "help", "" , "Display all list of commands", mon_help },
    { "help_about" , "/command/" , "Display descriptions of this command", help_about},
    {"ps", "" ,"Display list of partition",print_partition},
    {"info_partition", "/N/" ,"Display information about partition N",info_partition},
    {"pause", "/N/" ,"Pause partition N",pause_N},
    {"resume", "/N/" ,"Continue partition N",resume_N},
    {"restart", "/N/" ,"Restart partition N",restart_N},
    {"reset", "" ,"reset cpu", cpu_reset},
    {"exit", "" ,"Exit from console",exit_from_monitor},
};

/* 
 * Implementations of monitor commands
 */

/**
 * @funcname `mon_help`
 *
 * @description
 * The help command: lists the commands with their descriptions.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `argc` | `int32_t` | [in] The number of words of the command line. |
 * | `argv` | `char **` | [in] The words. |
 *
 * @return 0.
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `commands` | `command_t[9]` | The monitor's commands |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `i` | `size_t` | The index of a command |
 *
 * @hlr
 * - \hlrid{HLR_MONITOR_MON_HELP_01}: `mon_help()` shall show the commands of
 *   the monitor with their arguments and descriptions.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_DBG_008}
 *
 * @llr
 * - \llrid{LLR_MONITOR_MON_HELP_01}: If `argc` is greater than 1, `mon_help()`
 *   shall call `printf()` with `"Too many arguments for help!\n"` and return 0.
 *   <br>Traces to:
 *   - \hlrref{HLR_MONITOR_MON_HELP_01}
 * - \llrid{LLR_MONITOR_MON_HELP_02}: If `argc` is not greater than 1,
 *   `mon_help()` shall call `printf()` with `"%%s %%s- %%s\n"` and `name`,
 *   `argc` and `desc` of each of the `NCOMMANDS` elements of `commands`, in
 *   their order, and return 0.
 *   <br>Traces to:
 *   - \hlrref{HLR_MONITOR_MON_HELP_01}
 *
 * @design
 * 1. If `argc` is greater than `1`: \llrref{LLR_MONITOR_MON_HELP_01}
 *    1. Call `printf()` with the argument `"Too many arguments for help!\n"`.
 *    2. Return `0`.
 * 2. Set `i` to `0U`; then while `i` is less than `NCOMMANDS`, repeat (after
 *    each pass: increment `i`): \llrref{LLR_MONITOR_MON_HELP_02}
 *    1. Call `printf()` with the arguments `"%%s %%s- %%s\n"`,
 *       `commands[i].name`, `commands[i].argc` and `commands[i].desc`.
 * 3. Return `0`. \llrref{LLR_MONITOR_MON_HELP_02}
 *
 * @calls
 * - `printf()`
 */
int32_t
mon_help(int32_t argc, char **argv)
{
    if (argc > 1){
        (void) printf("Too many arguments for help!\n");
        return 0;
    }
    size_t i;
    (void) argc;
    (void) argv;
    for (i = 0U; i < NCOMMANDS; i++) {
        (void) printf("%s %s- %s\n", commands[i].name,commands[i].argc,
                      commands[i].desc);
    }
    return 0;
}

/**
 * @funcname `help_about`
 *
 * @description
 * The help_about command: shows the description of one command.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `argc` | `int32_t` | [in] The number of words of the command line. |
 * | `argv` | `char **` | [in] The words (argv[1]: the command). |
 *
 * @return 0.
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `commands` | `command_t[9]` | The monitor's commands |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `i` | `size_t` | The index of a command |
 *
 * @hlr
 * - \hlrid{HLR_MONITOR_HELP_ABOUT_01}: `help_about()` shall show the
 *   description of the monitor command named in its argument.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_DBG_008}
 *
 * @llr
 * - \llrid{LLR_MONITOR_HELP_ABOUT_01}: If `argc` is greater than 2,
 *   `help_about()` shall call `printf()` with `"Too many arguments for
 *   help_about!\n"` and return 0.
 *   <br>Traces to:
 *   - \hlrref{HLR_MONITOR_HELP_ABOUT_01}
 * - \llrid{LLR_MONITOR_HELP_ABOUT_02}: If `argc` is 1, `help_about()` shall
 *   call `printf()` with `"Missing parameter - Name of command!\n"` and
 *   return 0.
 *   <br>Traces to:
 *   - \hlrref{HLR_MONITOR_HELP_ABOUT_01}
 * - \llrid{LLR_MONITOR_HELP_ABOUT_03}: If `argc` is neither greater than 2 nor
 *   equal to 1, and `strcmp()` returns 0 for `argv[1]` and `name` of an element
 *   of `commands`, `help_about()` shall call `printf()` with `"%%s\n"` and
 *   `desc` of the first such element, and return 0.
 *   <br>Traces to:
 *   - \hlrref{HLR_MONITOR_HELP_ABOUT_01}
 * - \llrid{LLR_MONITOR_HELP_ABOUT_04}: If `argc` is neither greater than 2 nor
 *   equal to 1, and `strcmp()` returns a value other than 0 for `argv[1]` and
 *   `name` of each of the `NCOMMANDS` elements of `commands`, `help_about()`
 *   shall call `printf()` with `"There is no such command!\n"` and return 0.
 *   <br>Traces to:
 *   - \hlrref{HLR_MONITOR_HELP_ABOUT_01}
 *
 * @design
 * 1. If `argc` is greater than `2`: \llrref{LLR_MONITOR_HELP_ABOUT_01}
 *    1. Call `printf()` with the argument
 *       `"Too many arguments for help_about!\n"`.
 *    2. Return `0`.
 * 2. If `argc` is equal to `1`: \llrref{LLR_MONITOR_HELP_ABOUT_02}
 *    1. Call `printf()` with the argument
 *       `"Missing parameter - Name of command!\n"`.
 *    2. Return `0`.
 * 3. Set `i` to `0U`; then while `i` is less than `NCOMMANDS`, repeat (after
 *    each pass: increment `i`): \llrref{LLR_MONITOR_HELP_ABOUT_03},
 *    \llrref{LLR_MONITOR_HELP_ABOUT_04}
 *    1. If `strcmp(argv[1], commands[i].name)` is equal to `0`:
 *       \llrref{LLR_MONITOR_HELP_ABOUT_03}
 *       1. Call `printf()` with the arguments `"%%s\n"` and `commands[i].desc`.
 *       2. Return `0`.
 * 4. Call `printf()` with the argument `"There is no such command!\n"`.
 *    \llrref{LLR_MONITOR_HELP_ABOUT_04}
 * 5. Return `0`. \llrref{LLR_MONITOR_HELP_ABOUT_04}
 *
 * @calls
 * - `printf()`
 * - `strcmp()`
 */
int32_t 
help_about(int32_t argc,char **argv)
{
    if (argc > 2){
        (void) printf("Too many arguments for help_about!\n");
        return 0;
    }
    if (argc == 1){
        (void) printf("Missing parameter - Name of command!\n");
        return 0;
    }    
    for (size_t i = 0U; i < NCOMMANDS; i++) {
        if (strcmp(argv[1], commands[i].name) == 0){
            (void) printf("%s\n",commands[i].desc);
            return 0;            
        }    
    }
    (void) printf("There is no such command!\n");
    return 0;
}

/**
 * @funcname `print_partition`
 *
 * @description
 * The ps command: lists the partitions with their mode and whether they are
 * paused.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `argc` | `int32_t` | [in] The number of words of the command line. |
 * | `argv` | `char **` | [in] The words. |
 *
 * @return 0.
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
 * | `part` | `pok_partition_arinc_t *` | The partition |
 *
 * @hlr
 * - \hlrid{HLR_MONITOR_PRINT_PARTITION_01}: `print_partition()` shall list the
 *   partitions of the module, by number and name.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_DBG_008}
 *
 * @llr
 * - \llrid{LLR_MONITOR_PRINT_PARTITION_01}: If `argc` is greater than 1,
 *   `print_partition()` shall call `printf()` with `"Too many arguments for
 *   ps!\n"` and return 0.
 *   <br>Traces to:
 *   - \hlrref{HLR_MONITOR_PRINT_PARTITION_01}
 * - \llrid{LLR_MONITOR_PRINT_PARTITION_02}: If `argc` is not greater than 1 and
 *   `pok_partitions_arinc_n` is greater than 1, `print_partition()` shall call
 *   `printf()` with `"There are %%d partitions:\n"` and
 *   `pok_partitions_arinc_n`.
 *   <br>Traces to:
 *   - \hlrref{HLR_MONITOR_PRINT_PARTITION_01}
 * - \llrid{LLR_MONITOR_PRINT_PARTITION_03}: If `argc` is not greater than 1 and
 *   `pok_partitions_arinc_n` is not greater than 1, `print_partition()` shall
 *   call `printf()` with `"There is %%d partition:\n"` and
 *   `pok_partitions_arinc_n`.
 *   <br>Traces to:
 *   - \hlrref{HLR_MONITOR_PRINT_PARTITION_01}
 * - \llrid{LLR_MONITOR_PRINT_PARTITION_04}: If `argc` is not greater than 1,
 *   `print_partition()` shall then call, for each `i` less than
 *   `pok_partitions_arinc_n`, `printf()` with `"Partition %%u: %%s"`, `i` and
 *   `base_part.name` of `pok_partitions_arinc[i]`, and `printf()` with `"\n"`,
 *   and return 0.
 *   <br>Traces to:
 *   - \hlrref{HLR_MONITOR_PRINT_PARTITION_01}
 *
 * @design
 * 1. If `argc` is greater than `1`: \llrref{LLR_MONITOR_PRINT_PARTITION_01}
 *    1. Call `printf()` with the argument `"Too many arguments for ps!\n"`.
 *    2. Return `0`.
 * 2. If `pok_partitions_arinc_n` is greater than `1U`:
 *    \llrref{LLR_MONITOR_PRINT_PARTITION_02}
 *    1. Call `printf()` with the arguments `"There are %%d partitions:\n"` and
 *       `pok_partitions_arinc_n`.
 * 3. Otherwise: \llrref{LLR_MONITOR_PRINT_PARTITION_03}
 *    1. Call `printf()` with the arguments `"There is %%d partition:\n"` and
 *       `pok_partitions_arinc_n`.
 * 4. Set `i` to `0U`; then while `i` is less than `pok_partitions_arinc_n`,
 *    repeat (after each pass: increment `i`):
 *    \llrref{LLR_MONITOR_PRINT_PARTITION_04}
 *    1. Set `part` to the address of `pok_partitions_arinc[i]`.
 *    2. Call `printf()` with the arguments `"Partition %%u: %%s"`, `i` and
 *       `part->base_part.name`.
 *    3. Call `printf()` with the argument `"\n"`.
 * 5. Return `0`. \llrref{LLR_MONITOR_PRINT_PARTITION_04}
 *
 * @calls
 * - `printf()`
 */
int32_t 
print_partition(int32_t argc, char **argv){

    if (argc > 1){
        (void) printf("Too many arguments for ps!\n");
        return 0;
    }

    if (pok_partitions_arinc_n > 1U) { 
        (void) printf("There are %d partitions:\n", pok_partitions_arinc_n);
    } else { 
        (void) printf("There is %d partition:\n", pok_partitions_arinc_n);
    }
   
    for (uint32_t i = 0U ; i < pok_partitions_arinc_n ; i++){
        pok_partition_arinc_t* part = &pok_partitions_arinc[i];
        (void) printf("Partition %u: %s", i, part->base_part.name);
        (void) printf("\n");    
    }

    (void) argc;
    (void) argv;
    return 0;
}

/**
 * @funcname `info_partition`
 *
 * @description
 * The info_partition command: shows a partition's state, memory, threads, lock
 * level and ports.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `argc` | `int32_t` | [in] The number of words of the command line. |
 * | `argv` | `char **` | [in] The words (argv[1]: the partition number). |
 *
 * @return 0.
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
 * | `current` | `ptrdiff_t` | The current thread |
 * | `locked` | `ptrdiff_t` | The thread holding the lock |
 * | `number` | `int32_t` | The partition number |
 * | `part` | `pok_partition_arinc_t *` | The partition |
 * | `space_layout` | `jet_space_layout_t` | The partition's memory layout |
 *
 * @hlr
 * - \hlrid{HLR_MONITOR_INFO_PARTITION_01}: `info_partition()` shall show the
 *   state of the partition whose number is its argument: whether it stays
 *   paused, its memory, name, number of processes, period, duration, lock
 *   level, and its locking, current and main process.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_DBG_008}
 *
 * @llr
 * - \llrid{LLR_MONITOR_INFO_PARTITION_01}: If `argc` is greater than 2,
 *   `info_partition()` shall call `printf()` with `"Too many arguments for
 *   info_partition!\n"` and return 0.
 *   <br>Traces to:
 *   - \hlrref{HLR_MONITOR_INFO_PARTITION_01}
 * - \llrid{LLR_MONITOR_INFO_PARTITION_02}: If `argc` is 1, `info_partition()`
 *   shall call `printf()` with `"Missing parameter - Number of partition!\n"`
 *   and return 0.
 *   <br>Traces to:
 *   - \hlrref{HLR_MONITOR_INFO_PARTITION_01}
 * - \llrid{LLR_MONITOR_INFO_PARTITION_03}: If `argc` is neither greater than 2
 *   nor equal to 1, and the result of `parse_number()` for `argv[1]` is less
 *   than 0 or is not less than `pok_partitions_arinc_n`, `info_partition()`
 *   shall call `printf()` with `"There is no such partition!\n"` and return 0.
 *   <br>Traces to:
 *   - \hlrref{HLR_MONITOR_INFO_PARTITION_01}
 * - \llrid{LLR_MONITOR_INFO_PARTITION_04}: If `argc` is neither greater than 2
 *   nor equal to 1, and the result of `parse_number()` for `argv[1]` (the
 *   number) is not less than 0 and is less than `pok_partitions_arinc_n`,
 *   `info_partition()` shall call `ja_space_layout_get()` with
 *   `base_part.space_id` of `pok_partitions_arinc[number]`, and then
 *   `printf()`, in this order, with `"\n\n"`, with `"Info about partition
 *   #%%d\n"` and the number, with `"is_paused = %%d\n"` and the result of
 *   `partition_pause_get()` for the number, with `"base_addr = %%p\n"` and
 *   `kernel_addr` of the layout, with `"base_vaddr = %%p\n"` and its
 *   `user_addr`, with `"size = 0x%%zx\n"` and its `size`, with `"name = %%s\n"`
 *   and `base_part.name` of the partition, with `"nthreads = %%lu\n"` and its
 *   `nthreads`, with `"period = %%lu\n"` and its `base_part.period`, with
 *   `"duration = %%lu\n"` and its `base_part.duration`, with `"lock_level =
 *   %%u\n"` and its `lock_level`, with `"prev_thread = %%u\n"` and the locking
 *   thread, with `"current_thread = %%u\n"` and the index of `thread_current`
 *   in `threads`, with `"thread_main = %%u\n"` and
 *   `POK_PARTITION_ARINC_MAIN_THREAD_ID`, and with `"\n\n"`.
 *   <br>Traces to:
 *   - \hlrref{HLR_MONITOR_INFO_PARTITION_01}
 * - \llrid{LLR_MONITOR_INFO_PARTITION_05}: If `argc` is neither greater than 2
 *   nor equal to 1, and the number is not less than 0 and is less than
 *   `pok_partitions_arinc_n`, `info_partition()` shall return 0 after these
 *   calls.
 *   <br>Traces to:
 *   - \hlrref{HLR_MONITOR_INFO_PARTITION_01}
 * - \llrid{LLR_MONITOR_INFO_PARTITION_06}: If `lock_level` of the partition is
 *   not 0, `info_partition()` shall give the index of `thread_locked` in
 *   `threads` as the locking thread.
 *   <br>Traces to:
 *   - \hlrref{HLR_MONITOR_INFO_PARTITION_01}
 * - \llrid{LLR_MONITOR_INFO_PARTITION_07}: If `lock_level` of the partition is
 *   0, `info_partition()` shall give `UINT32_MAX` as the locking thread.
 *   <br>Traces to:
 *   - \hlrref{HLR_MONITOR_INFO_PARTITION_01}
 *
 * @design
 * 1. If `argc` is greater than `2`: \llrref{LLR_MONITOR_INFO_PARTITION_01}
 *    1. Call `printf()` with the argument
 *       `"Too many arguments for info_partition!\n"`.
 *    2. Return `0`.
 * 2. If `argc` is equal to `1`: \llrref{LLR_MONITOR_INFO_PARTITION_02}
 *    1. Call `printf()` with the argument
 *       `"Missing parameter - Number of partition!\n"`.
 *    2. Return `0`.
 * 3. Set `number` to `0`. \llrref{LLR_MONITOR_INFO_PARTITION_03},
 *    \llrref{LLR_MONITOR_INFO_PARTITION_04},
 *    \llrref{LLR_MONITOR_INFO_PARTITION_05}
 * 4. Call `parse_number()` with the argument `argv[1]`, and store the result in
 *    `number`. \llrref{LLR_MONITOR_INFO_PARTITION_03},
 *    \llrref{LLR_MONITOR_INFO_PARTITION_04},
 *    \llrref{LLR_MONITOR_INFO_PARTITION_05}
 * 5. If `number` is less than `0` or `number` is greater than or equal to
 *    `(int32_t) pok_partitions_arinc_n`: \llrref{LLR_MONITOR_INFO_PARTITION_03}
 *    1. Call `printf()` with the argument `"There is no such partition!\n"`.
 *    2. Return `0`.
 * 6. Set `part` to the address of `pok_partitions_arinc[number]`.
 *    \llrref{LLR_MONITOR_INFO_PARTITION_04}
 * 7. Call `ja_space_layout_get()` with the arguments `part->base_part.space_id`
 *    and the address of `space_layout`. \llrref{LLR_MONITOR_INFO_PARTITION_04}
 * 8. Call `printf()` with the argument `"\n\n"`.
 *    \llrref{LLR_MONITOR_INFO_PARTITION_04}
 * 9. Call `printf()` with the arguments `"Info about partition #%%d\n"` and
 *    `number`. \llrref{LLR_MONITOR_INFO_PARTITION_04}
 * 10. Call `printf()` with the arguments `"is_paused = %%d\n"` and
 *     `partition_pause_get(number)`. \llrref{LLR_MONITOR_INFO_PARTITION_04}
 * 11. Call `printf()` with the arguments `"base_addr = %%p\n"` and
 *     `(void *) space_layout.kernel_addr`.
 *     \llrref{LLR_MONITOR_INFO_PARTITION_04}
 * 12. Call `printf()` with the arguments `"base_vaddr = %%p\n"` and
 *     `(void *) space_layout.user_addr`. \llrref{LLR_MONITOR_INFO_PARTITION_04}
 * 13. Call `printf()` with the arguments `"size = 0x%%zx\n"` and
 *     `space_layout.size`. \llrref{LLR_MONITOR_INFO_PARTITION_04}
 * 14. Call `printf()` with the arguments `"name = %%s\n"` and
 *     `part->base_part.name`. \llrref{LLR_MONITOR_INFO_PARTITION_04}
 * 15. Call `printf()` with the arguments `"nthreads = %%lu\n"` and
 *     `(unsigned long)part->nthreads`. \llrref{LLR_MONITOR_INFO_PARTITION_04}
 * 16. Call `printf()` with the arguments `"period = %%lu\n"` and
 *     `(unsigned long)part->base_part.period`.
 *     \llrref{LLR_MONITOR_INFO_PARTITION_04}
 * 17. Call `printf()` with the arguments `"duration = %%lu\n"` and
 *     `(unsigned long)part->base_part.duration`.
 *     \llrref{LLR_MONITOR_INFO_PARTITION_04}
 * 18. Call `printf()` with the arguments `"lock_level = %%u\n"` and
 *     `(unsigned) part->lock_level`. \llrref{LLR_MONITOR_INFO_PARTITION_04}
 * 19. If `part->lock_level` is not equal to `0U`:
 *     \llrref{LLR_MONITOR_INFO_PARTITION_06}
 *     1. Set `locked` to `part->thread_locked - part->threads`.
 *     2. Call `printf()` with the arguments `"prev_thread = %%u\n"` and
 *        `(unsigned) locked`.
 * 20. Otherwise: \llrref{LLR_MONITOR_INFO_PARTITION_07}
 *     1. Call `printf()` with the arguments `"prev_thread = %%u\n"` and
 *        `(unsigned) UINT32_MAX`.
 * 21. Set `current` to `part->thread_current - part->threads`.
 *     \llrref{LLR_MONITOR_INFO_PARTITION_04}
 * 22. Call `printf()` with the arguments `"current_thread = %%u\n"` and
 *     `(unsigned) current`. \llrref{LLR_MONITOR_INFO_PARTITION_04}
 * 23. Call `printf()` with the arguments `"thread_main = %%u\n"` and
 *     `POK_PARTITION_ARINC_MAIN_THREAD_ID`.
 *     \llrref{LLR_MONITOR_INFO_PARTITION_04}
 * 24. Call `printf()` with the argument `"\n\n"`.
 *     \llrref{LLR_MONITOR_INFO_PARTITION_04}
 * 25. Return `0`. \llrref{LLR_MONITOR_INFO_PARTITION_05}
 *
 * @calls
 * - `printf()`
 * - `parse_number()`
 * - `ja_space_layout_get()`
 * - `partition_pause_get()`
 */
int32_t info_partition(int32_t argc,char **argv){
    if (argc > 2){
        (void) printf("Too many arguments for info_partition!\n");
        return 0;
    }
    if (argc == 1){
        (void) printf("Missing parameter - Number of partition!\n");
        return 0;
    }    

    int32_t number=0;
    number=parse_number(argv[1]);
    if ((number < 0) || (number >= (int32_t) pok_partitions_arinc_n)) {
        (void) printf("There is no such partition!\n");
        return 0;        
    }
    
    pok_partition_arinc_t* part = &pok_partitions_arinc[number];
    jet_space_layout_t space_layout;
    ja_space_layout_get(part->base_part.space_id, &space_layout);
    
    (void) printf("\n\n");
    (void) printf("Info about partition #%d\n",number);
    (void) printf("is_paused = %d\n", partition_pause_get(number));
    (void) printf("base_addr = %p\n", (void *) space_layout.kernel_addr);
    (void) printf("base_vaddr = %p\n", (void *) space_layout.user_addr);
    (void) printf("size = 0x%zx\n", space_layout.size);
    (void) printf("name = %s\n", part->base_part.name);
    (void) printf("nthreads = %lu\n", (unsigned long)part->nthreads);
    (void) printf("period = %lu\n", (unsigned long)part->base_part.period);
    (void) printf("duration = %lu\n", (unsigned long)part->base_part.duration);
    //printf("activation = %llu\n", part->base_part.activation);
    (void) printf("lock_level = %u\n", (unsigned) part->lock_level);
    if (part->lock_level != 0U) {
        ptrdiff_t locked = part->thread_locked - part->threads;

        (void) printf("prev_thread = %u\n", (unsigned) locked);
    } else {
        (void) printf("prev_thread = %u\n", (unsigned) UINT32_MAX);
    }
    ptrdiff_t current = part->thread_current - part->threads;

    (void) printf("current_thread = %u\n", (unsigned) current);
    (void) printf("thread_main = %u\n", POK_PARTITION_ARINC_MAIN_THREAD_ID);
#ifdef POK_NEEDS_IO
    //printf("io_min = %d\n",pok_partitions[number].io_min);        
    //printf("io_max = %d\n",pok_partitions[number].io_max);        
#endif
    (void) printf("\n\n");

    return 0;
}

/**
 * @funcname `pause_N`
 *
 * @description
 * The pause command: keeps a partition paused after the monitor.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `argc` | `int32_t` | [in] The number of words of the command line. |
 * | `argv` | `char **` | [in] The words (argv[1]: the partition number). |
 *
 * @return 0.
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `pok_partitions_arinc_n` | `const uint8_t` | The number of ARINC 653 partitions |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `number` | `int32_t` | The partition number |
 *
 * @hlr
 * - \hlrid{HLR_MONITOR_PAUSE_N_01}: `pause_N()` shall make the partition whose
 *   number is its argument stay paused when the monitor is left.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_DBG_008}
 *   - \srqref{SRQ_JET_DBG_016}
 *
 * @llr
 * - \llrid{LLR_MONITOR_PAUSE_N_01}: If `argc` is greater than 2, `pause_N()`
 *   shall call `printf()` with `"Too many arguments for pause!\n"` and
 *   return 0.
 *   <br>Traces to:
 *   - \hlrref{HLR_MONITOR_PAUSE_N_01}
 * - \llrid{LLR_MONITOR_PAUSE_N_02}: If `argc` is 1, `pause_N()` shall call
 *   `printf()` with `"Missing parameter - Number of partition!\n"` and
 *   return 0.
 *   <br>Traces to:
 *   - \hlrref{HLR_MONITOR_PAUSE_N_01}
 * - \llrid{LLR_MONITOR_PAUSE_N_03}: If `argc` is neither greater than 2 nor
 *   equal to 1, and the result of `parse_number()` for `argv[1]` is less than 0
 *   or is not less than `pok_partitions_arinc_n`, `pause_N()` shall call
 *   `printf()` with `"There is no such partition!\n"` and return 0.
 *   <br>Traces to:
 *   - \hlrref{HLR_MONITOR_PAUSE_N_01}
 * - \llrid{LLR_MONITOR_PAUSE_N_04}: If `argc` is neither greater than 2 nor
 *   equal to 1, and the result of `parse_number()` for `argv[1]` is not less
 *   than 0 and is less than `pok_partitions_arinc_n`, `pause_N()` shall call
 *   `partition_pause_set()` with that number and 1, and then `printf()` with
 *   `"Partition %%d paused\n"` and the number, and return 0.
 *   <br>Traces to:
 *   - \hlrref{HLR_MONITOR_PAUSE_N_01}
 *
 * @design
 * 1. If `argc` is greater than `2`: \llrref{LLR_MONITOR_PAUSE_N_01}
 *    1. Call `printf()` with the argument `"Too many arguments for pause!\n"`.
 *    2. Return `0`.
 * 2. If `argc` is equal to `1`: \llrref{LLR_MONITOR_PAUSE_N_02}
 *    1. Call `printf()` with the argument
 *       `"Missing parameter - Number of partition!\n"`.
 *    2. Return `0`.
 * 3. Set `number` to `0`. \llrref{LLR_MONITOR_PAUSE_N_03},
 *    \llrref{LLR_MONITOR_PAUSE_N_04}
 * 4. Call `parse_number()` with the argument `argv[1]`, and store the result in
 *    `number`. \llrref{LLR_MONITOR_PAUSE_N_03}, \llrref{LLR_MONITOR_PAUSE_N_04}
 * 5. If `number` is less than `0` or `number` is greater than or equal to
 *    `(int32_t) pok_partitions_arinc_n`: \llrref{LLR_MONITOR_PAUSE_N_03}
 *    1. Call `printf()` with the argument `"There is no such partition!\n"`.
 *    2. Return `0`.
 * 6. Call `partition_pause_set()` with the arguments `number` and `1`.
 *    \llrref{LLR_MONITOR_PAUSE_N_04}
 * 7. Call `printf()` with the arguments `"Partition %%d paused\n"` and
 *    `number`. \llrref{LLR_MONITOR_PAUSE_N_04}
 * 8. Return `0`. \llrref{LLR_MONITOR_PAUSE_N_04}
 *
 * @calls
 * - `printf()`
 * - `parse_number()`
 * - `partition_pause_set()`
 */
int32_t 
pause_N(int32_t argc, char **argv){
    if (argc > 2){
        (void) printf("Too many arguments for pause!\n");
        return 0;
    }
    if (argc == 1){
        (void) printf("Missing parameter - Number of partition!\n");
        return 0;
    }    
    int32_t number=0;
    number=parse_number(argv[1]);
    if ((number < 0) || (number >= (int32_t) pok_partitions_arinc_n)) {
        (void) printf("There is no such partition!\n");
        return 0;        
    }
    //Change mode of this partition to paused
    partition_pause_set(number, 1);
    (void) printf("Partition %d paused\n", number);

    return 0;
}

/**
 * @funcname `resume_N`
 *
 * @description
 * The resume command: lets a partition run after the monitor.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `argc` | `int32_t` | [in] The number of words of the command line. |
 * | `argv` | `char **` | [in] The words (argv[1]: the partition number). |
 *
 * @return 0.
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `pok_partitions_arinc_n` | `const uint8_t` | The number of ARINC 653 partitions |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `number` | `int32_t` | The partition number |
 *
 * @hlr
 * - \hlrid{HLR_MONITOR_RESUME_N_01}: `resume_N()` shall make the partition
 *   whose number is its argument run again when the monitor is left.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_DBG_008}
 *   - \srqref{SRQ_JET_DBG_016}
 *
 * @llr
 * - \llrid{LLR_MONITOR_RESUME_N_01}: If `argc` is greater than 2, `resume_N()`
 *   shall call `printf()` with `"Too many arguments for resume!\n"` and
 *   return 0.
 *   <br>Traces to:
 *   - \hlrref{HLR_MONITOR_RESUME_N_01}
 * - \llrid{LLR_MONITOR_RESUME_N_02}: If `argc` is 1, `resume_N()` shall call
 *   `printf()` with `"Missing parameter - Number of partition!\n"` and
 *   return 0.
 *   <br>Traces to:
 *   - \hlrref{HLR_MONITOR_RESUME_N_01}
 * - \llrid{LLR_MONITOR_RESUME_N_03}: If `argc` is neither greater than 2 nor
 *   equal to 1, and the result of `parse_number()` for `argv[1]` is less than 0
 *   or is not less than `pok_partitions_arinc_n`, `resume_N()` shall call
 *   `printf()` with `"There is no such partition!\n"` and return 0.
 *   <br>Traces to:
 *   - \hlrref{HLR_MONITOR_RESUME_N_01}
 * - \llrid{LLR_MONITOR_RESUME_N_04}: If `argc` is neither greater than 2 nor
 *   equal to 1, and the result of `parse_number()` for `argv[1]` is not less
 *   than 0 and is less than `pok_partitions_arinc_n`, `resume_N()` shall call
 *   `partition_pause_set()` with that number and 0, and then `printf()` with
 *   `"Partition %%d resumed\n"` and the number, and return 0.
 *   <br>Traces to:
 *   - \hlrref{HLR_MONITOR_RESUME_N_01}
 *
 * @design
 * 1. If `argc` is greater than `2`: \llrref{LLR_MONITOR_RESUME_N_01}
 *    1. Call `printf()` with the argument `"Too many arguments for resume!\n"`.
 *    2. Return `0`.
 * 2. If `argc` is equal to `1`: \llrref{LLR_MONITOR_RESUME_N_02}
 *    1. Call `printf()` with the argument
 *       `"Missing parameter - Number of partition!\n"`.
 *    2. Return `0`.
 * 3. Set `number` to `0`. \llrref{LLR_MONITOR_RESUME_N_03},
 *    \llrref{LLR_MONITOR_RESUME_N_04}
 * 4. Call `parse_number()` with the argument `argv[1]`, and store the result in
 *    `number`. \llrref{LLR_MONITOR_RESUME_N_03},
 *    \llrref{LLR_MONITOR_RESUME_N_04}
 * 5. If `number` is less than `0` or `number` is greater than or equal to
 *    `(int32_t) pok_partitions_arinc_n`: \llrref{LLR_MONITOR_RESUME_N_03}
 *    1. Call `printf()` with the argument `"There is no such partition!\n"`.
 *    2. Return `0`.
 * 6. Call `partition_pause_set()` with the arguments `number` and `0`.
 *    \llrref{LLR_MONITOR_RESUME_N_04}
 * 7. Call `printf()` with the arguments `"Partition %%d resumed\n"` and
 *    `number`. \llrref{LLR_MONITOR_RESUME_N_04}
 * 8. Return `0`. \llrref{LLR_MONITOR_RESUME_N_04}
 *
 * @calls
 * - `printf()`
 * - `parse_number()`
 * - `partition_pause_set()`
 */
int32_t 
resume_N(int32_t argc, char **argv){

    if (argc > 2){
        (void) printf("Too many arguments for resume!\n");
        return 0;
    }
    if (argc == 1){
        (void) printf("Missing parameter - Number of partition!\n");
        return 0;
    }    

    int32_t number=0;
    number=parse_number(argv[1]);
    if ((number < 0) || (number >= (int32_t) pok_partitions_arinc_n)) {
        (void) printf("There is no such partition!\n");
        return 0;        
    }
    //Change mode of this partition to not paused
    partition_pause_set(number, 0);
    (void) printf("Partition %d resumed\n",number);
    return 0;
}

/**
 * @funcname `restart_N`
 *
 * @description
 * The restart command: makes a partition restart (its context is dropped).
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `argc` | `int32_t` | [in] The number of words of the command line. |
 * | `argv` | `char **` | [in] The words (argv[1]: the partition number). |
 *
 * @return 0.
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
 * | `number` | `int32_t` | The partition number |
 *
 * @hlr
 * - \hlrid{HLR_MONITOR_RESTART_N_01}: `restart_N()` shall make the partition
 *   whose number is its argument start again at its entry the next time that it
 *   runs.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_DBG_008}
 *
 * @llr
 * - \llrid{LLR_MONITOR_RESTART_N_01}: If `argc` is greater than 2,
 *   `restart_N()` shall call `printf()` with `"Too many arguments for
 *   restart!\n"` and return 0.
 *   <br>Traces to:
 *   - \hlrref{HLR_MONITOR_RESTART_N_01}
 * - \llrid{LLR_MONITOR_RESTART_N_02}: If `argc` is 1, `restart_N()` shall call
 *   `printf()` with `"Missing parameter - Number of partition!\n"` and
 *   return 0.
 *   <br>Traces to:
 *   - \hlrref{HLR_MONITOR_RESTART_N_01}
 * - \llrid{LLR_MONITOR_RESTART_N_03}: If `argc` is neither greater than 2 nor
 *   equal to 1, and the result of `parse_number()` for `argv[1]` is less than 0
 *   or is not less than `pok_partitions_arinc_n`, `restart_N()` shall call
 *   `printf()` with `"There is no such partition!\n"` and return 0.
 *   <br>Traces to:
 *   - \hlrref{HLR_MONITOR_RESTART_N_01}
 * - \llrid{LLR_MONITOR_RESTART_N_04}: If `argc` is neither greater than 2 nor
 *   equal to 1, and the result of `parse_number()` for `argv[1]` is not less
 *   than 0 and is less than `pok_partitions_arinc_n`, `restart_N()` shall set
 *   `base_part.sp` of `pok_partitions_arinc[number]`, for that number, to NULL,
 *   and then call `printf()` with `"Partition %%d restarted\n"` and the number,
 *   and return 0.
 *   <br>Traces to:
 *   - \hlrref{HLR_MONITOR_RESTART_N_01}
 *
 * @design
 * 1. If `argc` is greater than `2`: \llrref{LLR_MONITOR_RESTART_N_01}
 *    1. Call `printf()` with the argument
 *       `"Too many arguments for restart!\n"`.
 *    2. Return `0`.
 * 2. If `argc` is equal to `1`: \llrref{LLR_MONITOR_RESTART_N_02}
 *    1. Call `printf()` with the argument
 *       `"Missing parameter - Number of partition!\n"`.
 *    2. Return `0`.
 * 3. Set `number` to `0`. \llrref{LLR_MONITOR_RESTART_N_03},
 *    \llrref{LLR_MONITOR_RESTART_N_04}
 * 4. Call `parse_number()` with the argument `argv[1]`, and store the result in
 *    `number`. \llrref{LLR_MONITOR_RESTART_N_03},
 *    \llrref{LLR_MONITOR_RESTART_N_04}
 * 5. If `number` is less than `0` or `number` is greater than or equal to
 *    `(int32_t) pok_partitions_arinc_n`: \llrref{LLR_MONITOR_RESTART_N_03}
 *    1. Call `printf()` with the argument `"There is no such partition!\n"`.
 *    2. Return `0`.
 * 6. Set `pok_partitions_arinc[number].base_part.sp` to `NULL`.
 *    \llrref{LLR_MONITOR_RESTART_N_04}
 * 7. Call `printf()` with the arguments `"Partition %%d restarted\n"` and
 *    `number`. \llrref{LLR_MONITOR_RESTART_N_04}
 * 8. Return `0`. \llrref{LLR_MONITOR_RESTART_N_04}
 *
 * @calls
 * - `printf()`
 * - `parse_number()`
 */
int32_t 
restart_N(int32_t argc, char **argv){

    if (argc > 2){
        (void) printf("Too many arguments for restart!\n");
        return 0;
    }
    if (argc == 1){
        (void) printf("Missing parameter - Number of partition!\n");
        return 0;
    }    

    int32_t number=0;
    number=parse_number(argv[1]);
    if ((number < 0) || (number >= (int32_t) pok_partitions_arinc_n)) {
        (void) printf("There is no such partition!\n");
        return 0;        
    }

    // Restart partition via global scheduler.
    pok_partitions_arinc[number].base_part.sp = NULL;

    (void) printf("Partition %d restarted\n",number);

    return 0;
}

/**
 * @funcname `exit_from_monitor`
 *
 * @description
 * The exit command: leaves the monitor.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `argc` | `int32_t` | [in] The number of words (not used). |
 * | `argv` | `char **` | [in] The words (not used). |
 *
 * @return 0.
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `want_to_exit` | `pok_bool_t` | Whether the monitor is to end |
 *
 * @locals
 * None.
 *
 * @hlr
 * - \hlrid{HLR_MONITOR_EXIT_FROM_MONITOR_01}: `exit_from_monitor()` shall end
 *   the command loop of the monitor.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_DBG_008}
 *
 * @llr
 * - \llrid{LLR_MONITOR_EXIT_FROM_MONITOR_01}: `exit_from_monitor()` shall set
 *   `want_to_exit` to `TRUE` and return 0.
 *   <br>Traces to:
 *   - \hlrref{HLR_MONITOR_EXIT_FROM_MONITOR_01}
 *
 * @design
 * 1. Set `want_to_exit` to `TRUE`. \llrref{LLR_MONITOR_EXIT_FROM_MONITOR_01}
 * 2. Return `0`. \llrref{LLR_MONITOR_EXIT_FROM_MONITOR_01}
 *
 * @calls
 * None.
 */
int32_t 
exit_from_monitor(int32_t argc, char **argv){

    (void) argc;
    (void) argv;
    want_to_exit=TRUE;
    return 0;

}

/**
 * @funcname `cpu_reset`
 *
 * @description
 * The reset command: resets the CPU (the board).
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `argc` | `int32_t` | [in] The number of words (not used). |
 * | `argv` | `char **` | [in] The words (not used). |
 *
 * @return 0.
 *
 * @globals
 * None.
 *
 * @locals
 * None.
 *
 * @hlr
 * - \hlrid{HLR_MONITOR_CPU_RESET_01}: `cpu_reset()` shall reset the module.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_DBG_008}
 *
 * @llr
 * - \llrid{LLR_MONITOR_CPU_RESET_01}: `cpu_reset()` shall call `printf()` with
 *   `"Rebooting\n"`, then `ja_cpu_reset()`, and return 0.
 *   <br>Traces to:
 *   - \hlrref{HLR_MONITOR_CPU_RESET_01}
 *
 * @design
 * 1. Call `printf()` with the argument `"Rebooting\n"`.
 *    \llrref{LLR_MONITOR_CPU_RESET_01}
 * 2. Call `ja_cpu_reset()`. \llrref{LLR_MONITOR_CPU_RESET_01}
 * 3. Return `0`. \llrref{LLR_MONITOR_CPU_RESET_01}
 *
 * @calls
 * - `printf()`
 * - `ja_cpu_reset()`
 */
int32_t cpu_reset(int32_t argc, char **argv)
{
    (void) argc;
    (void) argv;
    (void) printf("Rebooting\n");
    ja_cpu_reset();

    return 0;
}



/*
 * Monitor command interpreter
 */


/**
 * @funcname `runcmd`
 *
 * @description
 * Splits a command line into words at whitespace and runs the command of the
 * first word.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `line` | `char *` | [in,out] The command line (changed: the words are NUL-terminated). |
 *
 * @return The command's result, or 0 (no command, too many words, unknown
 *         command).
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `commands` | `command_t[9]` | The monitor's commands |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `argc` | `int32_t` | The number of words |
 * | `argv` | `char *[16]` | The words |
 * | `buf` | `char *` | The position in the line |
 * | `i` | `size_t` | The index of a command |
 *
 * @hlr
 * - \hlrid{HLR_MONITOR_RUNCMD_01}: `runcmd()` shall split a command line into
 *   words at white space and run the command named by the first word with the
 *   words, and report an unknown command or too many words.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_DBG_008}
 *
 * @llr
 * - \llrid{LLR_MONITOR_RUNCMD_01}: `runcmd()` shall split `line` into words,
 *   replacing each character of `WHITESPACE` (`strchr()`) before a word with
 *   `'\0'` and taking as a word the characters up to the next character of
 *   `WHITESPACE` or the end of the line.
 *   <br>Traces to:
 *   - \hlrref{HLR_MONITOR_RUNCMD_01}
 * - \llrid{LLR_MONITOR_RUNCMD_02}: If a word is found when `MAXARGS - 1` words
 *   have been taken, `runcmd()` shall call `printf()` with `"Too many arguments
 *   (max %%d)\n"` and `MAXARGS`, and return 0.
 *   <br>Traces to:
 *   - \hlrref{HLR_MONITOR_RUNCMD_01}
 * - \llrid{LLR_MONITOR_RUNCMD_03}: If the line has no word, `runcmd()` shall
 *   return 0 without calling a command.
 *   <br>Traces to:
 *   - \hlrref{HLR_MONITOR_RUNCMD_01}
 * - \llrid{LLR_MONITOR_RUNCMD_04}: If `strcmp()` returns 0 for the first word
 *   and `name` of an element of `commands`, `runcmd()` shall return the result
 *   of the function `func` of the first such element for the number of words
 *   and the array of the words, ended by NULL.
 *   <br>Traces to:
 *   - \hlrref{HLR_MONITOR_RUNCMD_01}
 * - \llrid{LLR_MONITOR_RUNCMD_05}: If the line has a word and `strcmp()`
 *   returns a value other than 0 for the first word and `name` of each of the
 *   `NCOMMANDS` elements of `commands`, `runcmd()` shall call `printf()` with
 *   `"Unknown command '%%s'\n"` and the first word, and return 0.
 *   <br>Traces to:
 *   - \hlrref{HLR_MONITOR_RUNCMD_01}
 *
 * @design
 * 1. Set `buf` to `line`. \llrref{LLR_MONITOR_RUNCMD_01}
 * 2. Set `argc` to `0`. \llrref{LLR_MONITOR_RUNCMD_01}
 * 3. Set `argv[argc]` to `NULL`. \llrref{LLR_MONITOR_RUNCMD_01}
 * 4. While `true` is true: \llrref{LLR_MONITOR_RUNCMD_01},
 *    \llrref{LLR_MONITOR_RUNCMD_02}
 *    1. While the value at `buf` is not equal to `'\0'` and
 *       `strchr(WHITESPACE, *buf)` is not NULL: \llrref{LLR_MONITOR_RUNCMD_01}
 *       1. Set `*buf` to `'\0'`.
 *       2. Increment `buf`.
 *    2. If the value at `buf` is equal to `'\0'`:
 *       \llrref{LLR_MONITOR_RUNCMD_01}
 *       1. Leave the loop.
 *    3. If `argc` is equal to `(MAXARGS-1)`: \llrref{LLR_MONITOR_RUNCMD_02}
 *       1. Call `printf()` with the arguments
 *          `"Too many arguments (max %%d)\n"` and `MAXARGS`.
 *       2. Return `0`.
 *    4. Set `argv[argc]` to `buf`. \llrref{LLR_MONITOR_RUNCMD_01}
 *    5. Increment `argc`. \llrref{LLR_MONITOR_RUNCMD_01}
 *    6. While the value at `buf` is not equal to `'\0'` and
 *       `strchr(WHITESPACE, *buf)` is NULL: \llrref{LLR_MONITOR_RUNCMD_01}
 *       1. Increment `buf`.
 * 5. Set `argv[argc]` to `NULL`. \llrref{LLR_MONITOR_RUNCMD_01},
 *    \llrref{LLR_MONITOR_RUNCMD_04}
 * 6. If `argc` is equal to `0`: \llrref{LLR_MONITOR_RUNCMD_03}
 *    1. Return `0`.
 * 7. Set `i` to `0U`; then while `i` is less than `NCOMMANDS`, repeat (after
 *    each pass: increment `i`): \llrref{LLR_MONITOR_RUNCMD_04},
 *    \llrref{LLR_MONITOR_RUNCMD_05}
 *    1. If `strcmp(argv[0], commands[i].name)` is equal to `0`:
 *       \llrref{LLR_MONITOR_RUNCMD_04}
 *       1. Call the function `commands[i].func` with the arguments `argc` and
 *          `argv`, and return its result.
 * 8. Call `printf()` with the arguments `"Unknown command '%%s'\n"` and
 *    `argv[0]`. \llrref{LLR_MONITOR_RUNCMD_05}
 * 9. Return `0`. \llrref{LLR_MONITOR_RUNCMD_05}
 *
 * @calls
 * - `strchr()`
 * - `printf()`
 * - `strcmp()`
 * - `commands[i].func()` (indirect)
 */
static int32_t
runcmd(char *line) 
{
    char *buf = line;
    int32_t argc;
    char *argv[MAXARGS];
    size_t i;

    // Parse the command buffer into whitespace-separated arguments
    argc = 0;
    argv[argc] = NULL;
    while (true) {
        // gobble whitespace
        while (((*buf) != '\0') && (strchr(WHITESPACE, *buf) != NULL)) {
            *buf = '\0';
            buf++;
        }
        if (*buf == '\0') {
            break;
        }

        // save and scan past next arg
        if (argc == (MAXARGS-1)) {
            (void) printf("Too many arguments (max %d)\n", MAXARGS);
            return 0;
        }
        argv[argc] = buf;
        argc++;
        while (((*buf) != '\0') && (strchr(WHITESPACE, *buf) == NULL)) {
            buf++;
        }
    }
    argv[argc] = NULL;

    // Lookup and invoke the command
    if (argc == 0) {
        return 0;
    }
    for (i = 0U; i < NCOMMANDS; i++) {
        if (strcmp(argv[0], commands[i].name) == 0) {
            return commands[i].func(argc, argv);
        }
    }
    (void) printf("Unknown command '%s'\n", argv[0]);
    return 0;
}

/**
 * @funcname `monitor`
 *
 * @description
 * The monitor's command loop: reads a line with the prompt "K> " and runs it,
 * until the exit command.
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `want_to_exit` | `pok_bool_t` | Whether the monitor is to end |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `buf` | `char *` | The line read |
 *
 * @hlr
 * - \hlrid{HLR_MONITOR_MONITOR_01}: `monitor()` shall greet the user and read
 *   and run monitor commands, after the prompt `"K> "`, until the exit command.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_DBG_008}
 *   - \srqref{SRQ_JET_DBG_009}
 *
 * @llr
 * - \llrid{LLR_MONITOR_MONITOR_01}: `monitor()` shall set `want_to_exit` to
 *   `FALSE`, call `printf()` with `"Welcome to the monitor!\n"` and then with
 *   `"Type 'help' for a list of commands.\n"`, and then call `readline()` with
 *   `"K> "` while `want_to_exit` is false.
 *   <br>Traces to:
 *   - \hlrref{HLR_MONITOR_MONITOR_01}
 * - \llrid{LLR_MONITOR_MONITOR_02}: If `readline()` returns a line and
 *   `runcmd()` returns a value less than 0 for it, `monitor()` shall return
 *   without calling `readline()` again.
 *   <br>Traces to:
 *   - \hlrref{HLR_MONITOR_MONITOR_01}
 * - \llrid{LLR_MONITOR_MONITOR_03}: If `readline()` returns a line and
 *   `runcmd()` returns 0 or more for it, `monitor()` shall go on with the next
 *   line while `want_to_exit` is false.
 *   <br>Traces to:
 *   - \hlrref{HLR_MONITOR_MONITOR_01}
 * - \llrid{LLR_MONITOR_MONITOR_04}: If `readline()` returns NULL, `monitor()`
 *   shall go on with the next line without calling `runcmd()`.
 *   <br>Traces to:
 *   - \hlrref{HLR_MONITOR_MONITOR_01}
 *
 * @design
 * 1. Set `want_to_exit` to `FALSE`. \llrref{LLR_MONITOR_MONITOR_01}
 * 2. Call `printf()` with the argument `"Welcome to the monitor!\n"`.
 *    \llrref{LLR_MONITOR_MONITOR_01}
 * 3. Call `printf()` with the argument
 *    `"Type 'help' for a list of commands.\n"`. \llrref{LLR_MONITOR_MONITOR_01}
 * 4. While `want_to_exit` is false: \llrref{LLR_MONITOR_MONITOR_01},
 *    \llrref{LLR_MONITOR_MONITOR_02}, \llrref{LLR_MONITOR_MONITOR_03},
 *    \llrref{LLR_MONITOR_MONITOR_04}
 *    1. Call `readline()` with the argument `"K> "`, and store the result in
 *       `buf`. \llrref{LLR_MONITOR_MONITOR_01}
 *    2. If `buf` is not NULL: \llrref{LLR_MONITOR_MONITOR_02},
 *       \llrref{LLR_MONITOR_MONITOR_03}, \llrref{LLR_MONITOR_MONITOR_04}
 *       1. If `runcmd(buf)` is less than `0`: \llrref{LLR_MONITOR_MONITOR_02},
 *          \llrref{LLR_MONITOR_MONITOR_03}
 *          1. Leave the loop.
 *
 * @calls
 * - `printf()`
 * - `readline()`
 * - `runcmd()`
 */
void
monitor(void)
{
    char *buf;
    want_to_exit=FALSE;
    (void) printf("Welcome to the monitor!\n");
    (void) printf("Type 'help' for a list of commands.\n");


    while (!want_to_exit) {
        buf = readline("K> ");
        if (buf != NULL) {
            if (runcmd(buf) < 0) {
                break;
            }
        }
            
    }
}


/**
 * @funcname `monitor_start_func`
 *
 * @description
 * The start operation of the monitor partition: forever, when a character comes
 * on the main console, pauses every partition, runs the monitor, then resumes
 * the partitions not kept paused.
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
 * | `i` | `pok_partition_id_t` | The index of a partition |
 * | `part` | `pok_partition_arinc_t *` | The partition |
 *
 * @hlr
 * - \hlrid{HLR_MONITOR_MONITOR_START_FUNC_01}: `monitor_start_func()` shall,
 *   when a character is typed on the main console, pause every partition, run
 *   the monitor, and then resume the partitions that are not kept paused.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_DBG_008}
 *   - \srqref{SRQ_JET_DBG_016}
 *
 * @llr
 * - \llrid{LLR_MONITOR_MONITOR_START_FUNC_01}: `monitor_start_func()` shall
 *   first call `partition_pause_set()` with `i` and 1 for each `i` less than
 *   `pok_partitions_arinc_n`, and then call `data_to_read_0()` again and again,
 *   without returning.
 *   <br>Traces to:
 *   - \hlrref{HLR_MONITOR_MONITOR_START_FUNC_01}
 * - \llrid{LLR_MONITOR_MONITOR_START_FUNC_02}: If `data_to_read_0()` returns 1,
 *   `monitor_start_func()` shall call `pok_preemption_disable()`, then, for
 *   each partition `pok_partitions_arinc[i]` whose `base_part.is_paused` is
 *   false, call `partition_pause_set()` with `i` and 0 and set its
 *   `base_part.is_paused` to `TRUE`, and then call `pok_preemption_enable()`
 *   and `monitor()`.
 *   <br>Traces to:
 *   - \hlrref{HLR_MONITOR_MONITOR_START_FUNC_01}
 * - \llrid{LLR_MONITOR_MONITOR_START_FUNC_03}: For a partition whose
 *   `base_part.is_paused` is true at that time, `monitor_start_func()` shall
 *   leave its `base_part.is_paused` as it is, without calling
 *   `partition_pause_set()` for it.
 *   <br>Traces to:
 *   - \hlrref{HLR_MONITOR_MONITOR_START_FUNC_01}
 * - \llrid{LLR_MONITOR_MONITOR_START_FUNC_04}: When `monitor()` returns,
 *   `monitor_start_func()` shall call `pok_preemption_disable()`, set
 *   `base_part.is_paused` to `FALSE` for each partition `i` for which
 *   `partition_pause_get()` returns 0, and then call `pok_preemption_enable()`.
 *   <br>Traces to:
 *   - \hlrref{HLR_MONITOR_MONITOR_START_FUNC_01}
 * - \llrid{LLR_MONITOR_MONITOR_START_FUNC_05}: For a partition for which
 *   `partition_pause_get()` returns a value other than 0 at that time,
 *   `monitor_start_func()` shall leave its `base_part.is_paused` as it is.
 *   <br>Traces to:
 *   - \hlrref{HLR_MONITOR_MONITOR_START_FUNC_01}
 * - \llrid{LLR_MONITOR_MONITOR_START_FUNC_06}: If `data_to_read_0()` returns a
 *   value other than 1, `monitor_start_func()` shall call neither
 *   `pok_preemption_disable()` nor `monitor()`.
 *   <br>Traces to:
 *   - \hlrref{HLR_MONITOR_MONITOR_START_FUNC_01}
 *
 * @design
 * 1. Set `i` to `0U`; then while `i` is less than `pok_partitions_arinc_n`,
 *    repeat (after each pass: increment `i`):
 *    \llrref{LLR_MONITOR_MONITOR_START_FUNC_01}
 *    1. Call `partition_pause_set()` with the arguments `i` and `1U`.
 * 2. Repeat forever: \llrref{LLR_MONITOR_MONITOR_START_FUNC_01}
 *    1. If `data_to_read_0()` is equal to `1`:
 *       \llrref{LLR_MONITOR_MONITOR_START_FUNC_02},
 *       \llrref{LLR_MONITOR_MONITOR_START_FUNC_03},
 *       \llrref{LLR_MONITOR_MONITOR_START_FUNC_04},
 *       \llrref{LLR_MONITOR_MONITOR_START_FUNC_05},
 *       \llrref{LLR_MONITOR_MONITOR_START_FUNC_06}
 *       1. Call `pok_preemption_disable()`.
 *          \llrref{LLR_MONITOR_MONITOR_START_FUNC_02}
 *       2. Set `i` to `0U`; then while `i` is less than
 *          `pok_partitions_arinc_n`, repeat (after each pass: increment `i`):
 *          \llrref{LLR_MONITOR_MONITOR_START_FUNC_02},
 *          \llrref{LLR_MONITOR_MONITOR_START_FUNC_03}
 *          1. Set `part` to the address of `pok_partitions_arinc[i]`.
 *          2. If `part->base_part.is_paused` is false:
 *             1. Call `partition_pause_set()` with the arguments `i` and `0U`.
 *                \llrref{LLR_MONITOR_MONITOR_START_FUNC_02}
 *             2. Set `part->base_part.is_paused` to `TRUE`.
 *                \llrref{LLR_MONITOR_MONITOR_START_FUNC_02}
 *       3. Call `pok_preemption_enable()`.
 *          \llrref{LLR_MONITOR_MONITOR_START_FUNC_02}
 *       4. Call `monitor()`. \llrref{LLR_MONITOR_MONITOR_START_FUNC_02}
 *       5. Call `pok_preemption_disable()`.
 *          \llrref{LLR_MONITOR_MONITOR_START_FUNC_04}
 *       6. Set `i` to `0U`; then while `i` is less than
 *          `pok_partitions_arinc_n`, repeat (after each pass: increment `i`):
 *          \llrref{LLR_MONITOR_MONITOR_START_FUNC_04},
 *          \llrref{LLR_MONITOR_MONITOR_START_FUNC_05}
 *          1. If `partition_pause_get(i)` is equal to `0U`:
 *             1. Set `pok_partitions_arinc[i].base_part.is_paused` to `FALSE`.
 *                \llrref{LLR_MONITOR_MONITOR_START_FUNC_04}
 *       7. Call `pok_preemption_enable()`.
 *          \llrref{LLR_MONITOR_MONITOR_START_FUNC_04}
 *
 * @calls
 * - `partition_pause_set()`
 * - `data_to_read_0()`
 * - `pok_preemption_disable()`
 * - `pok_preemption_enable()`
 * - `monitor()`
 * - `partition_pause_get()`
 */
static void monitor_start_func(void)
{
    for (pok_partition_id_t i = 0U; i < pok_partitions_arinc_n; i++){
        partition_pause_set(i, 1U);
    }
    for (;;) {
        if (data_to_read_0() == 1) {
            /*
             * Set all partitions on pause
             */
            pok_preemption_disable();
            for (pok_partition_id_t i = 0U; i < pok_partitions_arinc_n; i++){
                pok_partition_arinc_t* part = &pok_partitions_arinc[i];
                if (!part->base_part.is_paused){ 
                    partition_pause_set(i, 0U);
                    part->base_part.is_paused=TRUE;
                }
            }
            pok_preemption_enable();
            
            monitor();

            pok_preemption_disable();
            for (pok_partition_id_t i = 0U; i < pok_partitions_arinc_n; i++){
                if (partition_pause_get(i) == 0U){ 
                    pok_partitions_arinc[i].base_part.is_paused=FALSE;
                }
            }
            pok_preemption_enable();
        }
    }
}

/**
 * @funcname `monitor_process_error`
 *
 * @description
 * The error processing of the monitor partition: any error in it is fatal.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `partition_state` | `pok_system_state_t` | [in] The system state (not used). |
 * | `error_id` | `pok_error_id_t` | [in] The error (not used). |
 * | `state_byte_preempt_local` | `pok_bool_t` | [in] Whether local preemption was disabled (not used). |
 * | `failed_address` | `void *` | [in] The faulting address (not used). |
 *
 * @globals
 * None.
 *
 * @locals
 * None.
 *
 * @hlr
 * - \hlrid{HLR_MONITOR_MONITOR_PROCESS_ERROR_01}: `monitor_process_error()`
 *   shall treat any error of the monitor partition as fatal.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_HM_010}
 *
 * @llr
 * - \llrid{LLR_MONITOR_MONITOR_PROCESS_ERROR_01}: `monitor_process_error()`
 *   shall call `pok_fatal()` with `"Error in monitor"`.
 *   <br>Traces to:
 *   - \hlrref{HLR_MONITOR_MONITOR_PROCESS_ERROR_01}
 *
 * @design
 * 1. Call `pok_fatal()` with the argument `"Error in monitor"`.
 *    \llrref{LLR_MONITOR_MONITOR_PROCESS_ERROR_01}
 *
 * @calls
 * - `pok_fatal()`
 */
static void monitor_process_error(pok_system_state_t partition_state,
        pok_error_id_t error_id,
        pok_bool_t state_byte_preempt_local,
        void* failed_address)
{
    (void) partition_state;
    (void) error_id;
    (void) state_byte_preempt_local;
    (void) failed_address;
    pok_fatal("Error in monitor");
}


/**
 * @funcname `pok_monitor_thread_init`
 *
 * @description
 * Initializes the monitor partition at boot (POK_NEEDS_MONITOR): a kernel-only
 * partition running monitor_start_func().
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `partition_monitor` | `pok_partition_t` | The monitor partition |
 * | `partition_sched_ops_kernel` | `const pok_part_sched_ops_t` | The scheduler operations of kernel-only partitions |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `monitor_operations` | `static const pok_part_ops_t` | The operations of the monitor partition |
 *
 * @hlr
 * - \hlrid{HLR_MONITOR_POK_MONITOR_THREAD_INIT_01}: `pok_monitor_thread_init()`
 *   shall set up, at boot, the monitor as a kernel-only partition that runs the
 *   monitor's start function.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_PART_010}
 *   - \srqref{SRQ_JET_DBG_008}
 *
 * @llr
 * - \llrid{LLR_MONITOR_POK_MONITOR_THREAD_INIT_01}: `pok_monitor_thread_init()`
 *   shall call `pok_partition_init()` with the address of `partition_monitor`,
 *   set its `part_sched_ops` to the address of `partition_sched_ops_kernel`,
 *   its `part_ops` to the address of `monitor_operations` (whose `start` is
 *   `monitor_start_func` and whose `process_partition_error` is
 *   `monitor_process_error`) and its `initial_sp` to the result of
 *   `pok_stack_alloc()` for 4096.
 *   <br>Traces to:
 *   - \hlrref{HLR_MONITOR_POK_MONITOR_THREAD_INIT_01}
 *
 * @design
 * 1. Call `pok_partition_init()` with the address of `partition_monitor`.
 *    \llrref{LLR_MONITOR_POK_MONITOR_THREAD_INIT_01}
 * 2. Set `partition_monitor.part_sched_ops` to the address of
 *    `partition_sched_ops_kernel`.
 *    \llrref{LLR_MONITOR_POK_MONITOR_THREAD_INIT_01}
 * 3. Set `partition_monitor.part_ops` to the address of `monitor_operations`.
 *    \llrref{LLR_MONITOR_POK_MONITOR_THREAD_INIT_01}
 * 4. Call `pok_stack_alloc()` with the argument `4096`, and store the result in
 *    `partition_monitor.initial_sp`.
 *    \llrref{LLR_MONITOR_POK_MONITOR_THREAD_INIT_01}
 *
 * @calls
 * - `pok_partition_init()`
 * - `pok_stack_alloc()`
 */
void pok_monitor_thread_init(void)
{
    static const pok_part_ops_t monitor_operations =
    {
        .start = monitor_start_func,
        .process_partition_error = monitor_process_error
    };

#ifdef POK_NEEDS_MONITOR
    pok_partition_init(&partition_monitor);

    partition_monitor.part_sched_ops = &partition_sched_ops_kernel;
    partition_monitor.part_ops = &monitor_operations;
    partition_monitor.initial_sp = pok_stack_alloc(4096);
#endif
}

