/*
 *                               POK header
 * 
 * The following file is a part of the POK project. Any modification should
 * made according to the POK licence. You CANNOT use this file or a part of
 * this file is this part of a file for your own project
 *
 * For more information on the POK licence, please see our LICENCE FILE
 *
 * Please follow the coding guidelines described in doc/CODING_GUIDELINES
 *
 *                                      Copyright (c) 2007-2009 POK team 
 *
 * Created by julien on Wed Sep 30 17:30:02 2009 
 *
 * Copyright (C) 2026 Kirill Brazhnikov <agamemnon886@gmail.com>
 * Modified by Kirill Brazhnikov, 2026-11-08: Doxygen headers for the file
 * and its functions.
 * Modified by Kirill Brazhnikov, 2026-11-14: the string literals of the Design
 * steps are wrapped at their spaces, each in one code span (they were cut
 * inside names).
 * The modifications are distributed under the POK licence
 * (BSD-2-Clause, see POK-LICENSE).
 */

/**
 * @file
 * Instrumentation: the architecture of the partitions and threads, and the
 * running threads, printed in the format of the Cheddar scheduling analyzer.
 */

#include <config.h>

#ifdef POK_NEEDS_INSTRUMENTATION

#include <core/thread.h>
#include <core/partition.h>
#include <core/time.h>
#include <libc.h>

uint8_t pok_instrumentation_task_is_init = 0;
uint8_t pok_instrumentation_partition_is_init = 0;

/**
 * @funcname `pok_instrumentation_task_archi`
 *
 * @description
 * Prints the Cheddar description of a thread (a periodic task), after the end
 * of the address spaces list before the first one. The first thread of a
 * partition (its init thread) isn't printed.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `id` | `const uint32_t` | [in] The thread. |
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `pok_instrumentation_task_is_init` | `uint8_t` | Whether the task list was started |
 *
 * @locals
 * None.
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. If `id == pok_partitions[pok_threads[id].partition].thread_index_low` is
 *    true:
 *    1. Return.
 * 2. If `pok_instrumentation_task_is_init` is equal to `0`:
 *    1. Call `printf()` with the argument
 *       `"[INSTRUMENTATION][CHEDDAR][ARCHI] </address_spaces>\n"`.
 *    2. Call `printf()` with the argument
 *       `"[INSTRUMENTATION][CHEDDAR][ARCHI] <tasks>\n"`.
 * 3. Set `pok_instrumentation_task_is_init` to `1`.
 * 4. Call `printf()` with the argument
 *    <tt>&quot;[INSTRUMENTATION][CHEDDAR][ARCHI] &lt;task
 *    task_type=\\&quot;PERIODIC_TYPE\\&quot;&gt;\\n&quot;</tt>.
 * 5. Call `printf()` with the argument
 *    `"[INSTRUMENTATION][CHEDDAR][ARCHI] <cpu_name>pokkernel</cpu_name>\n"`.
 * 6. Call `printf()` with the arguments
 *    `"[INSTRUMENTATION][CHEDDAR][ARCHI] <name>task%%lu</name>\n"` and `id`.
 * 7. Call `printf()` with the argument
 *    `"[INSTRUMENTATION][CHEDDAR][ARCHI] <start_time>0</start_time>\n"`.
 * 8. Call `printf()` with the argument
 *    `"[INSTRUMENTATION][CHEDDAR][ARCHI] <policy>SCHED_FIFO</policy>\n"`.
 * 9. Call `printf()` with the argument
 *    `"[INSTRUMENTATION][CHEDDAR][ARCHI] <criticality>0</criticality>\n"`.
 * 10. Call `printf()` with the argument
 *     `"[INSTRUMENTATION][CHEDDAR][ARCHI]
 *     <context_switch_overhead>0</context_switch_overhead>\n"`.
 * 11. Call `printf()` with the argument
 *     `"[INSTRUMENTATION][CHEDDAR][ARCHI] <blocking_time>0</blocking_time>\n"`.
 * 12. Call `printf()` with the argument
 *     `"[INSTRUMENTATION][CHEDDAR][ARCHI] <priority>1</priority>\n"`.
 * 13. Call `printf()` with the argument
 *     `"[INSTRUMENTATION][CHEDDAR][ARCHI]
 *     <text_memory_size>0</text_memory_size>\n"`.
 * 14. Call `printf()` with the argument
 *     `"[INSTRUMENTATION][CHEDDAR][ARCHI]
 *     <stack_memory_size>0</stack_memory_size>\n"`.
 * 15. Call `printf()` with the argument
 *     `"[INSTRUMENTATION][CHEDDAR][ARCHI] <jitter>0</jitter>\n"`.
 * 16. Call `printf()` with the argument
 *     `"[INSTRUMENTATION][CHEDDAR][ARCHI] </task>\n"`.
 *
 * @calls
 * - `printf()`
 */
void pok_instrumentation_task_archi (const uint32_t id)
{
   /*
    * We don't print the first thread, this is the init thread
    */
   if (id == pok_partitions[pok_threads[id].partition].thread_index_low)
   {
      return;
   }

   if (pok_instrumentation_task_is_init == 0)
   {
      printf ("[INSTRUMENTATION][CHEDDAR][ARCHI] </address_spaces>\n");
      printf ("[INSTRUMENTATION][CHEDDAR][ARCHI] <tasks>\n");
   }

   pok_instrumentation_task_is_init = 1;

   printf ("[INSTRUMENTATION][CHEDDAR][ARCHI] <task task_type=\"PERIODIC_TYPE\">\n");
   printf ("[INSTRUMENTATION][CHEDDAR][ARCHI]     <cpu_name>pokkernel</cpu_name>\n");
   printf ("[INSTRUMENTATION][CHEDDAR][ARCHI]     <address_space_name>partition%d</address_space_name>\n", pok_threads[id].partition);
   printf ("[INSTRUMENTATION][CHEDDAR][ARCHI]     <address_space_name>partition%d</address_space_name>\n", pok_threads[id].partition);
   printf ("[INSTRUMENTATION][CHEDDAR][ARCHI]     <name>task%lu</name>\n", id);
   printf ("[INSTRUMENTATION][CHEDDAR][ARCHI]     <capacity>%llu</capacity>\n", pok_threads[id].time_capacity);
   printf ("[INSTRUMENTATION][CHEDDAR][ARCHI]     <start_time>0</start_time>\n");
   printf ("[INSTRUMENTATION][CHEDDAR][ARCHI]     <policy>SCHED_FIFO</policy>\n");
   printf ("[INSTRUMENTATION][CHEDDAR][ARCHI]     <deadline>%d</deadline>\n", pok_threads[id].deadline);
   printf ("[INSTRUMENTATION][CHEDDAR][ARCHI]     <criticality>0</criticality>\n");
   printf ("[INSTRUMENTATION][CHEDDAR][ARCHI]     <context_switch_overhead>0</context_switch_overhead>\n");
   printf ("[INSTRUMENTATION][CHEDDAR][ARCHI]     <blocking_time>0</blocking_time>\n");
   printf ("[INSTRUMENTATION][CHEDDAR][ARCHI]     <priority>1</priority>\n");
   printf ("[INSTRUMENTATION][CHEDDAR][ARCHI]     <text_memory_size>0</text_memory_size>\n");
   printf ("[INSTRUMENTATION][CHEDDAR][ARCHI]     <stack_memory_size>0</stack_memory_size>\n");
   printf ("[INSTRUMENTATION][CHEDDAR][ARCHI]     <period>%llu</period>\n", pok_threads[id].period);
   printf ("[INSTRUMENTATION][CHEDDAR][ARCHI]     <jitter>0</jitter>\n");
   printf ("[INSTRUMENTATION][CHEDDAR][ARCHI] </task>\n");
}

/**
 * @funcname `pok_instrumentation_partition_archi`
 *
 * @description
 * Prints the Cheddar description of a partition (an address space), after
 * starting the address space list before the first one.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `id` | `const uint8_t` | [in] The partition. |
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `pok_instrumentation_partition_is_init` | `uint8_t` | Whether the address space list was started |
 *
 * @locals
 * None.
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. If `pok_instrumentation_partition_is_init` is equal to `0`:
 *    1. Call `printf()` with the argument
 *       `"[INSTRUMENTATION][CHEDDAR][ARCHI] <address_spaces>\n"`.
 * 2. Set `pok_instrumentation_partition_is_init` to `1`.
 * 3. Call `printf()` with the argument
 *    `"[INSTRUMENTATION][CHEDDAR][ARCHI] <address_space>\n"`.
 * 4. Call `printf()` with the arguments
 *    `"[INSTRUMENTATION][CHEDDAR][ARCHI] <name>partition%%d</name>\n"` and
 *    `id`.
 * 5. Call `printf()` with the argument
 *    `"[INSTRUMENTATION][CHEDDAR][ARCHI] <cpu_name>pokkernel</cpu_name>\n"`.
 * 6. Call `printf()` with the argument
 *    `"[INSTRUMENTATION][CHEDDAR][ARCHI]
 *    <text_memory_size> 0</text_memory_size>\n"`.
 * 7. Call `printf()` with the argument
 *    `"[INSTRUMENTATION][CHEDDAR][ARCHI]
 *    <stack_memory_size> 0</stack_memory_size>\n"`.
 * 8. Call `printf()` with the argument
 *    `"[INSTRUMENTATION][CHEDDAR][ARCHI]
 *    <data_memory_size> 0</data_memory_size>\n"`.
 * 9. Call `printf()` with the argument
 *    `"[INSTRUMENTATION][CHEDDAR][ARCHI]
 *    <heap_memory_size> 0</heap_memory_size>\n"`.
 * 10. Call `printf()` with the argument
 *     `"[INSTRUMENTATION][CHEDDAR][ARCHI]
 *     POSIX_1003_HIGHEST_PRIORITY_FIRST_PROTOCOL </scheduler>\n"`.
 * 11. Call `printf()` with the argument
 *     `"[INSTRUMENTATION][CHEDDAR][ARCHI] </address_space>\n"`.
 *
 * @calls
 * - `printf()`
 */
void pok_instrumentation_partition_archi (const uint8_t id)
{
   if (pok_instrumentation_partition_is_init == 0)
   {
      printf ("[INSTRUMENTATION][CHEDDAR][ARCHI] <address_spaces>\n");
   }

   pok_instrumentation_partition_is_init = 1;

   printf ("[INSTRUMENTATION][CHEDDAR][ARCHI] <address_space>\n");
   printf ("[INSTRUMENTATION][CHEDDAR][ARCHI]    <name>partition%d</name>\n", id);
   printf ("[INSTRUMENTATION][CHEDDAR][ARCHI]    <cpu_name>pokkernel</cpu_name>\n");
   printf ("[INSTRUMENTATION][CHEDDAR][ARCHI]    <text_memory_size> 0</text_memory_size>\n");
   printf ("[INSTRUMENTATION][CHEDDAR][ARCHI]    <stack_memory_size> 0</stack_memory_size>\n");
   printf ("[INSTRUMENTATION][CHEDDAR][ARCHI]    <data_memory_size> 0</data_memory_size>\n");
   printf ("[INSTRUMENTATION][CHEDDAR][ARCHI]    <heap_memory_size> 0</heap_memory_size>\n");
   printf ("[INSTRUMENTATION][CHEDDAR][ARCHI]    <scheduler quantum= \"%lu\">", pok_partitions[id].period);
   printf ("[INSTRUMENTATION][CHEDDAR][ARCHI]    POSIX_1003_HIGHEST_PRIORITY_FIRST_PROTOCOL </scheduler>\n");
   printf ("[INSTRUMENTATION][CHEDDAR][ARCHI] </address_space>\n");
}

/**
 * @funcname `pok_instrumentation_running_task`
 *
 * @description
 * Prints a Cheddar event: a thread runs from now on.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `id` | `const uint32_t` | [in] The thread. |
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
 * 1. Call `printf()` with the argument
 *    `"[INSTRUMENTATION][CHEDDAR][EVENTS] <running_task> "`.
 * 2. Call `printf()` with the arguments `"%%llu"` and `jet_system_time()`.
 * 3. Call `printf()` with the arguments `" task%%lu"` and `id`.
 * 4. Call `printf()` with the argument `"</running_task>\n"`.
 *
 * @calls
 * - `printf()`
 * - `ja_system_time()`
 */
void pok_instrumentation_running_task (const uint32_t id)
{
   printf ("[INSTRUMENTATION][CHEDDAR][EVENTS] <running_task>   ");
   printf ("%llu", jet_system_time() );
   printf ("  task%lu", id);
   printf ("</running_task>\n");
}

#endif
