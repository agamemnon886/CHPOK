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
static void pok_network_thread_init(void)
{
    pok_threads[NETWORK_THREAD].entry = pok_network_thread;
    pok_threads[NETWORK_THREAD].sp = pok_context_create(NETWORK_THREAD, 4096, (uintptr_t) pok_network_thread);
}
#endif


#define NCOMMANDS (sizeof(commands) / sizeof(commands[0]))



/*
 *  Don't use '[' in your command!
 */



/*
 * Convert string in int
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
static uint8_t partition_pause_get(pok_partition_id_t id)
{
    assert(id < 32U);
    
    return (uint8_t) ((partition_on_pause_bits >> id) & 1U);
}

static pok_bool_t want_to_exit=FALSE; 

int32_t mon_help(int32_t argc, char **argv);

int32_t help_about(int32_t argc,char **argv);

int32_t print_partition(int32_t argc, char **argv); // List of partition

int32_t pause_N(int32_t argc, char **argv);// pause partition N. 

int32_t resume_N(int32_t argc, char **argv); // continue partition N

int32_t restart_N(int32_t argc, char **argv); // restart pertition N

int32_t exit_from_monitor(int32_t argc, char **argv); //exit from monitor

int32_t cpu_reset(int32_t argc, char **argv); //cpu reset

int32_t info_partition(int32_t argc,char ** argv);

struct Command {
    const char *name;
    const char *argc;
    const char *desc;
    int32_t (*func)(int32_t argc, char** argv);
};

static struct Command commands[] = {
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
    struct jet_space_layout space_layout;
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

int32_t 
exit_from_monitor(int32_t argc, char **argv){

    (void) argc;
    (void) argv;
    want_to_exit=TRUE;
    return 0;

}

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

#define WHITESPACE "\t\r\n "
#define MAXARGS 16

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


void pok_monitor_thread_init(void)
{
    static const struct pok_partition_operations monitor_operations =
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

