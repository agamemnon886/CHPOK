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
 * Modified by Kirill Brazhnikov, 2026-11-16: high and low level requirements of
 * the functions, traced to the system requirements (docs/SRQs), and the steps
 * of their design linked to the LLRs.
 * Modified by Kirill Brazhnikov, 2026-11-17: low level requirements of one
 * behaviour each; code that compiles to nothing (assertions, (void) x;,
 * empty branches) is left out of the design; requirements with more than one
 * "shall" are split into requirements of one "shall" each.
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
 * - \hlrid{HLR_INSTRUMENTATION_POK_INSTRUMENTATION_TASK_ARCHI_01}:
 *   `pok_instrumentation_task_archi()` shall print the Cheddar description of a
 *   process, as a periodic task, except for the first process of its partition.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_DBG_010}
 * - \hlrid{HLR_INSTRUMENTATION_POK_INSTRUMENTATION_TASK_ARCHI_02}:
 *   `pok_instrumentation_task_archi()` shall close the list of the address
 *   spaces and open the list of the tasks before the first description that it
 *   prints.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_DBG_010}
 *
 * @llr
 * - \llrid{LLR_INSTRUMENTATION_POK_INSTRUMENTATION_TASK_ARCHI_01}: If `id` is
 *   `thread_index_low` of the partition of the thread
 *   (`pok_partitions[pok_threads[id].partition]`),
 *   `pok_instrumentation_task_archi()` shall return without calling `printf()`.
 *   <br>Traces to:
 *   - \hlrref{HLR_INSTRUMENTATION_POK_INSTRUMENTATION_TASK_ARCHI_01}
 * - \llrid{LLR_INSTRUMENTATION_POK_INSTRUMENTATION_TASK_ARCHI_02}: If `id` is
 *   not `thread_index_low` of the partition of the thread and
 *   `pok_instrumentation_task_is_init` is 0, `pok_instrumentation_task_archi()`
 *   shall first call `printf()` with `"[INSTRUMENTATION][CHEDDAR][ARCHI]
 *   </address_spaces>\n"` and then with `"[INSTRUMENTATION][CHEDDAR][ARCHI]
 *   <tasks>\n"`.
 *   <br>Traces to:
 *   - \hlrref{HLR_INSTRUMENTATION_POK_INSTRUMENTATION_TASK_ARCHI_02}
 * - \llrid{LLR_INSTRUMENTATION_POK_INSTRUMENTATION_TASK_ARCHI_03}: If `id` is
 *   not `thread_index_low` of the partition of the thread,
 *   `pok_instrumentation_task_archi()` shall set
 *   `pok_instrumentation_task_is_init` to 1 and call `printf()` 18 times, with
 *   lines made of `"[INSTRUMENTATION][CHEDDAR][ARCHI]"`, four spaces (one space
 *   for the first and the last line), and, in this order, `<task
 *   task_type="PERIODIC_TYPE">`, `<cpu_name>pokkernel</cpu_name>`,
 *   `<address_space_name>partition%%d</address_space_name>` twice with
 *   `partition` of the thread, `<name>task%%lu</name>` with `id`,
 *   `<capacity>%%llu</capacity>` with `time_capacity` of the thread,
 *   `<start_time>0</start_time>`, `<policy>SCHED_FIFO</policy>`,
 *   `<deadline>%%d</deadline>` with its `deadline`,
 *   `<criticality>0</criticality>`,
 *   `<context_switch_overhead>0</context_switch_overhead>`,
 *   `<blocking_time>0</blocking_time>`, `<priority>1</priority>`,
 *   `<text_memory_size>0</text_memory_size>`,
 *   `<stack_memory_size>0</stack_memory_size>`, `<period>%%llu</period>` with
 *   its `period`, `<jitter>0</jitter>` and `</task>`, each followed by a line
 *   feed.
 *   <br>Traces to:
 *   - \hlrref{HLR_INSTRUMENTATION_POK_INSTRUMENTATION_TASK_ARCHI_01}
 *
 * @design
 * 1. If `id == pok_partitions[pok_threads[id].partition].thread_index_low` is
 *    true: \llrref{LLR_INSTRUMENTATION_POK_INSTRUMENTATION_TASK_ARCHI_01}
 *    1. Return.
 * 2. If `pok_instrumentation_task_is_init` is equal to `0`:
 *    \llrref{LLR_INSTRUMENTATION_POK_INSTRUMENTATION_TASK_ARCHI_02}
 *    1. Call `printf()` with the argument
 *       `"[INSTRUMENTATION][CHEDDAR][ARCHI] </address_spaces>\n"`.
 *    2. Call `printf()` with the argument
 *       `"[INSTRUMENTATION][CHEDDAR][ARCHI] <tasks>\n"`.
 * 3. Set `pok_instrumentation_task_is_init` to `1`.
 *    \llrref{LLR_INSTRUMENTATION_POK_INSTRUMENTATION_TASK_ARCHI_03}
 * 4. Call `printf()` with the argument
 *    <tt>&quot;[INSTRUMENTATION][CHEDDAR][ARCHI] &lt;task
 *    task_type=\\&quot;PERIODIC_TYPE\\&quot;&gt;\\n&quot;</tt>.
 *    \llrref{LLR_INSTRUMENTATION_POK_INSTRUMENTATION_TASK_ARCHI_03}
 * 5. Call `printf()` with the argument
 *    `"[INSTRUMENTATION][CHEDDAR][ARCHI] <cpu_name>pokkernel</cpu_name>\n"`.
 *    \llrref{LLR_INSTRUMENTATION_POK_INSTRUMENTATION_TASK_ARCHI_03}
 * 6. Call `printf()` with the arguments
 *    `"[INSTRUMENTATION][CHEDDAR][ARCHI] <name>task%%lu</name>\n"` and `id`.
 *    \llrref{LLR_INSTRUMENTATION_POK_INSTRUMENTATION_TASK_ARCHI_03}
 * 7. Call `printf()` with the argument
 *    `"[INSTRUMENTATION][CHEDDAR][ARCHI] <start_time>0</start_time>\n"`.
 *    \llrref{LLR_INSTRUMENTATION_POK_INSTRUMENTATION_TASK_ARCHI_03}
 * 8. Call `printf()` with the argument
 *    `"[INSTRUMENTATION][CHEDDAR][ARCHI] <policy>SCHED_FIFO</policy>\n"`.
 *    \llrref{LLR_INSTRUMENTATION_POK_INSTRUMENTATION_TASK_ARCHI_03}
 * 9. Call `printf()` with the argument
 *    `"[INSTRUMENTATION][CHEDDAR][ARCHI] <criticality>0</criticality>\n"`.
 *    \llrref{LLR_INSTRUMENTATION_POK_INSTRUMENTATION_TASK_ARCHI_03}
 * 10. Call `printf()` with the argument
 *     `"[INSTRUMENTATION][CHEDDAR][ARCHI]
 *     <context_switch_overhead>0</context_switch_overhead>\n"`.
 *     \llrref{LLR_INSTRUMENTATION_POK_INSTRUMENTATION_TASK_ARCHI_03}
 * 11. Call `printf()` with the argument
 *     `"[INSTRUMENTATION][CHEDDAR][ARCHI] <blocking_time>0</blocking_time>\n"`.
 *     \llrref{LLR_INSTRUMENTATION_POK_INSTRUMENTATION_TASK_ARCHI_03}
 * 12. Call `printf()` with the argument
 *     `"[INSTRUMENTATION][CHEDDAR][ARCHI] <priority>1</priority>\n"`.
 *     \llrref{LLR_INSTRUMENTATION_POK_INSTRUMENTATION_TASK_ARCHI_03}
 * 13. Call `printf()` with the argument
 *     `"[INSTRUMENTATION][CHEDDAR][ARCHI]
 *     <text_memory_size>0</text_memory_size>\n"`.
 *     \llrref{LLR_INSTRUMENTATION_POK_INSTRUMENTATION_TASK_ARCHI_03}
 * 14. Call `printf()` with the argument
 *     `"[INSTRUMENTATION][CHEDDAR][ARCHI]
 *     <stack_memory_size>0</stack_memory_size>\n"`.
 *     \llrref{LLR_INSTRUMENTATION_POK_INSTRUMENTATION_TASK_ARCHI_03}
 * 15. Call `printf()` with the argument
 *     `"[INSTRUMENTATION][CHEDDAR][ARCHI] <jitter>0</jitter>\n"`.
 *     \llrref{LLR_INSTRUMENTATION_POK_INSTRUMENTATION_TASK_ARCHI_03}
 * 16. Call `printf()` with the argument
 *     `"[INSTRUMENTATION][CHEDDAR][ARCHI] </task>\n"`.
 *     \llrref{LLR_INSTRUMENTATION_POK_INSTRUMENTATION_TASK_ARCHI_03}
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
 * - \hlrid{HLR_INSTRUMENTATION_POK_INSTRUMENTATION_PARTITION_ARCHI_01}:
 *   `pok_instrumentation_partition_archi()` shall print the Cheddar description
 *   of a partition, as an address space.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_DBG_010}
 * - \hlrid{HLR_INSTRUMENTATION_POK_INSTRUMENTATION_PARTITION_ARCHI_02}:
 *   `pok_instrumentation_partition_archi()` shall open the list of the address
 *   spaces before the first description that it prints.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_DBG_010}
 *
 * @llr
 * - \llrid{LLR_INSTRUMENTATION_POK_INSTRUMENTATION_PARTITION_ARCHI_01}: If and
 *   only if `pok_instrumentation_partition_is_init` is 0,
 *   `pok_instrumentation_partition_archi()` shall first call `printf()` with
 *   `"[INSTRUMENTATION][CHEDDAR][ARCHI] <address_spaces>\n"`.
 *   <br>Traces to:
 *   - \hlrref{HLR_INSTRUMENTATION_POK_INSTRUMENTATION_PARTITION_ARCHI_02}
 * - \llrid{LLR_INSTRUMENTATION_POK_INSTRUMENTATION_PARTITION_ARCHI_02}:
 *   `pok_instrumentation_partition_archi()` shall set
 *   `pok_instrumentation_partition_is_init` to 1 and call `printf()` with lines
 *   made of `"[INSTRUMENTATION][CHEDDAR][ARCHI]"`, spaces and, in this order,
 *   `<address_space>`, `<name>partition%%d</name>` with `id`,
 *   `<cpu_name>pokkernel</cpu_name>`, `<text_memory_size>
 *   0</text_memory_size>`, `<stack_memory_size> 0</stack_memory_size>`,
 *   `<data_memory_size> 0</data_memory_size>`, `<heap_memory_size>
 *   0</heap_memory_size>`, `<scheduler quantum= "%%lu">` with `period` of
 *   `pok_partitions[id]` (without a line feed),
 *   `POSIX_1003_HIGHEST_PRIORITY_FIRST_PROTOCOL </scheduler>` and
 *   `</address_space>`, the others followed by a line feed.
 *   <br>Traces to:
 *   - \hlrref{HLR_INSTRUMENTATION_POK_INSTRUMENTATION_PARTITION_ARCHI_01}
 *
 * @design
 * 1. If `pok_instrumentation_partition_is_init` is equal to `0`:
 *    \llrref{LLR_INSTRUMENTATION_POK_INSTRUMENTATION_PARTITION_ARCHI_01}
 *    1. Call `printf()` with the argument
 *       `"[INSTRUMENTATION][CHEDDAR][ARCHI] <address_spaces>\n"`.
 * 2. Set `pok_instrumentation_partition_is_init` to `1`.
 *    \llrref{LLR_INSTRUMENTATION_POK_INSTRUMENTATION_PARTITION_ARCHI_02}
 * 3. Call `printf()` with the argument
 *    `"[INSTRUMENTATION][CHEDDAR][ARCHI] <address_space>\n"`.
 *    \llrref{LLR_INSTRUMENTATION_POK_INSTRUMENTATION_PARTITION_ARCHI_02}
 * 4. Call `printf()` with the arguments
 *    `"[INSTRUMENTATION][CHEDDAR][ARCHI] <name>partition%%d</name>\n"` and
 *    `id`. \llrref{LLR_INSTRUMENTATION_POK_INSTRUMENTATION_PARTITION_ARCHI_02}
 * 5. Call `printf()` with the argument
 *    `"[INSTRUMENTATION][CHEDDAR][ARCHI] <cpu_name>pokkernel</cpu_name>\n"`.
 *    \llrref{LLR_INSTRUMENTATION_POK_INSTRUMENTATION_PARTITION_ARCHI_02}
 * 6. Call `printf()` with the argument
 *    `"[INSTRUMENTATION][CHEDDAR][ARCHI]
 *    <text_memory_size> 0</text_memory_size>\n"`.
 *    \llrref{LLR_INSTRUMENTATION_POK_INSTRUMENTATION_PARTITION_ARCHI_02}
 * 7. Call `printf()` with the argument
 *    `"[INSTRUMENTATION][CHEDDAR][ARCHI]
 *    <stack_memory_size> 0</stack_memory_size>\n"`.
 *    \llrref{LLR_INSTRUMENTATION_POK_INSTRUMENTATION_PARTITION_ARCHI_02}
 * 8. Call `printf()` with the argument
 *    `"[INSTRUMENTATION][CHEDDAR][ARCHI]
 *    <data_memory_size> 0</data_memory_size>\n"`.
 *    \llrref{LLR_INSTRUMENTATION_POK_INSTRUMENTATION_PARTITION_ARCHI_02}
 * 9. Call `printf()` with the argument
 *    `"[INSTRUMENTATION][CHEDDAR][ARCHI]
 *    <heap_memory_size> 0</heap_memory_size>\n"`.
 *    \llrref{LLR_INSTRUMENTATION_POK_INSTRUMENTATION_PARTITION_ARCHI_02}
 * 10. Call `printf()` with the argument
 *     `"[INSTRUMENTATION][CHEDDAR][ARCHI]
 *     POSIX_1003_HIGHEST_PRIORITY_FIRST_PROTOCOL </scheduler>\n"`.
 *     \llrref{LLR_INSTRUMENTATION_POK_INSTRUMENTATION_PARTITION_ARCHI_02}
 * 11. Call `printf()` with the argument
 *     `"[INSTRUMENTATION][CHEDDAR][ARCHI] </address_space>\n"`.
 *     \llrref{LLR_INSTRUMENTATION_POK_INSTRUMENTATION_PARTITION_ARCHI_02}
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
 * - \hlrid{HLR_INSTRUMENTATION_POK_INSTRUMENTATION_RUNNING_TASK_01}:
 *   `pok_instrumentation_running_task()` shall print a Cheddar event telling
 *   that a process runs from the present time on.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_DBG_010}
 *
 * @llr
 * - \llrid{LLR_INSTRUMENTATION_POK_INSTRUMENTATION_RUNNING_TASK_01}:
 *   `pok_instrumentation_running_task()` shall call `printf()` with
 *   `"[INSTRUMENTATION][CHEDDAR][EVENTS] <running_task>"` and three spaces,
 *   then with `"%%llu"` and the result of `jet_system_time()`, then with two
 *   spaces and `"task%%lu"` and `id`, and then with `"</running_task>\n"`.
 *   <br>Traces to:
 *   - \hlrref{HLR_INSTRUMENTATION_POK_INSTRUMENTATION_RUNNING_TASK_01}
 *
 * @design
 * 1. Call `printf()` with the argument
 *    `"[INSTRUMENTATION][CHEDDAR][EVENTS] <running_task> "`.
 *    \llrref{LLR_INSTRUMENTATION_POK_INSTRUMENTATION_RUNNING_TASK_01}
 * 2. Call `printf()` with the arguments `"%%llu"` and `jet_system_time()`.
 *    \llrref{LLR_INSTRUMENTATION_POK_INSTRUMENTATION_RUNNING_TASK_01}
 * 3. Call `printf()` with the arguments `" task%%lu"` and `id`.
 *    \llrref{LLR_INSTRUMENTATION_POK_INSTRUMENTATION_RUNNING_TASK_01}
 * 4. Call `printf()` with the argument `"</running_task>\n"`.
 *    \llrref{LLR_INSTRUMENTATION_POK_INSTRUMENTATION_RUNNING_TASK_01}
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
