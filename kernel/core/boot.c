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
 * Created by julien on Thu Jan 15 23:34:13 2009
 *
 * Copyright (C) 2026 Kirill Brazhnikov <agamemnon886@gmail.com>
 * Modified by Kirill Brazhnikov, 2026-11-02: MISRA C:2025: discarded
 * results are cast to void.
 * Modified by Kirill Brazhnikov, 2026-11-04: MISRA C:2025: discarded
 * results are cast to void in the POK_NEEDS_WAIT_FOR_GDB code.
 * Modified by Kirill Brazhnikov, 2026-11-08: Doxygen headers for the file
 * and its functions.
 * Modified by Kirill Brazhnikov, 2026-11-11: the Doxygen comments that don't
 * document a function definition are plain comments, so that only the function
 * headers are in the function documentation.
 * Modified by Kirill Brazhnikov, 2026-11-14: The function headers no longer
 * have a step for the declaration of a local variable; the @locals table
 * describes it, and an initialized one is set in a step.
 * The modifications are distributed under the POK licence
 * (BSD-2-Clause, see POK-LICENSE).
 */

/**
 * @file
 * Boot function to start the kernel (written by Julien Delange for POK,
 * 2008-2009).
 */

#include <config.h>

#include <cons.h>

#include <core/time.h>
#include <core/thread.h>
#include <core/sched.h>
#include <core/partition.h>
#include <core/partition_arinc.h>
#include <core/channel.h>
#include <asp/entries.h>
#include <libc.h>

#include <core/instrumentation.h>

#ifdef POK_NEEDS_GDB
#include <gdb.h>
#endif

/**
 * @funcname `jet_boot`
 *
 * @description
 * Starts the kernel after the architecture and board are set up: initializes
 * the partitions, the monitor and GDB threads, the scheduler and the channels,
 * then starts scheduling (or runs main() without partitions). It doesn't
 * return.
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `kernel_state` | `pok_system_state_t` | The kernel's system state |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `tmp` | `uint32_t` | The thread of an instrumentation event |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Set `kernel_state` to `POK_SYSTEM_STATE_OS_MOD`.
 * 2. Call `pok_network_init()`.
 * 3. Call `pok_partition_arinc_init_all()`.
 * 4. Call `pok_monitor_thread_init()`.
 * 5. Call `pok_gdb_thread_init()`.
 * 6. Call `pok_sched_init()`.
 * 7. Call `pok_channels_init_all()`.
 * 8. Call `pok_cons_write()` with the arguments `"JET OS kernel initialized\n"`
 *    and `26`.
 * 9. Call `printf()` with the argument
 *    `"[INSTRUMENTATION][CHEDDAR] <event_table>\n"`.
 * 10. Call `printf()` with the argument
 *     `"[INSTRUMENTATION][CHEDDAR] <processor>\n"`.
 * 11. Call `printf()` with the argument
 *     `"[INSTRUMENTATION][CHEDDAR] <name>pok_kernel</name>\n"`.
 * 12. Call `printf()` with the argument `"Waiting for GDB connection ...\n"`.
 * 13. Call `printf()` with the argument `"\n"`.
 * 14. Call `pok_trap()`.
 * 15. Call `pok_sched_start()`.
 *
 * @calls
 * - `pok_network_init()`
 * - `pok_partition_arinc_init_all()`
 * - `pok_monitor_thread_init()`
 * - `pok_gdb_thread_init()`
 * - `pok_sched_init()`
 * - `pok_channels_init_all()`
 * - `pok_cons_write()`
 * - `printf()`
 * - `pok_trap()`
 * - `pok_sched_start()`
 */
void jet_boot (void)
{
   kernel_state = POK_SYSTEM_STATE_OS_MOD; // TODO: is this choice for state right?

#ifdef POK_NEEDS_NETWORKING
   pok_network_init();
#endif

#ifdef POK_NEEDS_PARTITIONS
   pok_partition_arinc_init_all();
#endif
#ifdef POK_NEEDS_MONITOR
   pok_monitor_thread_init();
#endif
#ifdef POK_NEEDS_GDB
   pok_gdb_thread_init();
#endif

#if defined (POK_NEEDS_SCHED) || defined (POK_NEEDS_THREADS)
   pok_sched_init ();
#endif
#if defined (POK_NEEDS_PORTS_QUEUEING) || defined (POK_NEEDS_PORTS_SAMPLING)
   pok_channels_init_all ();
#endif

#if defined (POK_NEEDS_DEBUG) || defined (POK_NEEDS_CONSOLE)
  (void) pok_cons_write ("JET OS kernel initialized\n", 26);
#endif

#ifdef POK_NEEDS_INSTRUMENTATION
  uint32_t tmp;
   printf ("[INSTRUMENTATION][CHEDDAR] <event_table>\n");
   printf ("[INSTRUMENTATION][CHEDDAR] <processor>\n");
   printf ("[INSTRUMENTATION][CHEDDAR] <name>pok_kernel</name>\n");

   for (tmp = 0 ; tmp < POK_CONFIG_NB_THREADS ; tmp++)
   {
      printf ("[INSTRUMENTATION][CHEDDAR] <task_activation>   0   task %lu</task_activation>\n", tmp);
   }
#endif

#ifdef POK_NEEDS_PARTITIONS
#if defined(POK_NEEDS_GDB) && defined(POK_NEEDS_WAIT_FOR_GDB)
  (void) printf("Waiting for GDB connection ...\n");
  (void) printf("\n");
  pok_trap();
#endif
  pok_sched_start();
#else
  ja_preempt_enable();

  /*
   * If we don't use partitioning service, we execute a main
   * function. In that case, POK is acting like an executive,
   * not a real kernel
   */
  main ();
#endif
}
