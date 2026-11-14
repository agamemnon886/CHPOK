
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
 * Modified by Kirill Brazhnikov, 2026-10-31: built-in integer types (int,
 * unsigned, short, long, ...) are replaced with the fixed-width types
 * from <stdint.h>.
 *
 * Modified by Kirill Brazhnikov, 2026-11-02: MISRA C:2025: braces around
 * all if/else/loop bodies; essentially Boolean conditions; U suffixes for
 * unsigned constants; discarded results are cast to void; pok_fatal() is
 * _Noreturn.
 *
 * Modified by Kirill Brazhnikov, 2026-11-03: MISRA C:2025: debug_strlen()
 * doesn't modify its parameter; deviations are documented where a
 * guideline can't be followed.
 *
 * Modified by Kirill Brazhnikov, 2026-11-08: Doxygen headers for the file
 * and its functions.
 *
 * Modified by Kirill Brazhnikov, 2026-11-14: The function headers no longer
 * have a step for the declaration of a local variable; the @locals table
 * describes it, and an initialized one is set in a step.
 */

/**
 * @file
 * Kernel debugging helpers: the length of a message, the fatal error stop
 * and a hex dump.
 */

#include <config.h>

#ifdef POK_NEEDS_DEBUG

#include <asp/arch.h>
#include <errno.h>
#include <core/debug.h>
#include <cons.h>
#include <core/sched.h>
#include <core/thread.h>
#include <core/partition.h>

/*
 * MISRA C:2025 Rule 11.5 deviation (this file): the code at an address is
 * dumped as bytes.
 */

/**
 * @funcname `debug_strlen`
 *
 * @description
 * Returns the length of a string, for pok_fatal() (which can't rely on the rest
 * of the C library).
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `str` | `const char *` | [in] The string. |
 *
 * @return The number of characters before the NUL.
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `i` | `int32_t` | The index of the character |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Set `i` to `0`.
 * 2. While `str[i]` is not equal to `'\0'`:
 *    1. Increment `i`.
 * 3. Return `i`.
 *
 * @calls
 * None.
 */
int32_t debug_strlen (const char* str)
{
  int32_t		i = 0;

  while (str[i] != '\0')
  {
    i++;
  }
  return i;
}

/**
 * @funcname `pok_debug_print_current_state`
 *
 * @description
 * Prints the state of the current partition and thread (a debugging aid). The
 * printing code is commented out: it does nothing.
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
 * 1. Do nothing.
 *
 * @calls
 * None.
 */
void pok_debug_print_current_state (void)
{
   /*uint32_t i;
   printf ("\nCurrent state\n");
   printf ("-------------\n");
   printf ("Kernel thread        : %d\n", KERNEL_THREAD);
   printf ("Idle thread          : %d\n", IDLE_THREAD);
#ifdef POK_NEEDS_PARTITIONS
   printf ("Current partition    : %d\n", POK_SCHED_CURRENT_PARTITION);
   printf ("Thread index         : %lu\n",   (unsigned long) POK_CURRENT_PARTITION.thread_index);
   printf ("Thread low           : %lu\n",   (unsigned long) POK_CURRENT_PARTITION.thread_index_low);
   printf ("Thread high          : %lu\n",   (unsigned long) POK_CURRENT_PARTITION.thread_index_high);
   printf ("Thread capacity      : %lu\n",   (unsigned long) POK_CURRENT_PARTITION.nthreads);
   printf ("Base addr            : 0x%lx\n", (unsigned long) POK_CURRENT_PARTITION.base_addr);
   printf ("Base vaddr           : 0x%lx\n", (unsigned long) POK_CURRENT_PARTITION.base_vaddr);
   printf ("Size                 : %lu\n", (unsigned long) POK_CURRENT_PARTITION.size);
   printf ("Current thread       : %lu\n", (unsigned long) POK_CURRENT_PARTITION.current_thread);
   printf ("Prev current thread  : %lu\n", (unsigned long) POK_CURRENT_PARTITION.prev_thread);
   printf ("Main thread          : %u\n", POK_CURRENT_PARTITION.thread_main);
   printf ("Main thread entry    : 0x%lx\n", (unsigned long) POK_CURRENT_PARTITION.thread_main_entry);
   printf ("Partition threads sp :");
   for (i = POK_CURRENT_PARTITION.thread_index_low ; i < POK_CURRENT_PARTITION.thread_index_low + POK_CURRENT_PARTITION.thread_index ; i++)
   {
      printf (" 0x%lx", (unsigned long) pok_threads[i].sp);
   }
   printf ("\n");
   printf ("-------------\n");
#endif
   printf ("Current thread    : %d\n", POK_SCHED_CURRENT_THREAD);
   printf ("Period            : %lld\n", (long long) POK_CURRENT_THREAD.period);
   printf ("Deadline          : %d\n", POK_CURRENT_THREAD.deadline);
   printf ("Partition         : %d\n", POK_CURRENT_THREAD.partition);
   printf ("sp                : 0x%lx\n", (unsigned long) POK_CURRENT_THREAD.sp);
   printf ("init_stack_addr   : 0x%lx\n", (unsigned long) POK_CURRENT_THREAD.init_stack_addr);
   printf ("entry             : 0x%p\n", POK_CURRENT_THREAD.entry);*/
}

/**
 * @funcname `pok_fatal`
 *
 * @description
 * Stops the kernel after a fatal error: prints the message and loops forever.
 * It can be called from a kernel thread with preemption enabled.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `message` | `const char *` | [in] The error message. |
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
 * 1. Call `ja_preempt_disable()`.
 * 2. Call `jet_console_write()` with the arguments `"FATAL ERROR: \n"` and
 *    `13`.
 * 3. Call `jet_console_write()` with the arguments `message` and
 *    `debug_strlen(message)`.
 * 4. Use the macro `POK_DEBUG_PRINT_CURRENT_STATE`.
 * 5. Call `ja_inf_loop()`.
 *
 * @calls
 * - `ja_preempt_disable()`
 * - `jet_console_write()`
 * - `debug_strlen()`
 * - `pok_debug_print_current_state()`
 * - `ja_inf_loop()`
 */
_Noreturn void pok_fatal (const char* message)
{
  // it can be called from a kernel thread
  // with preemption enabled
  // also, some bugs somehow reenable preemption
  // where it shouldn't be enabled
  ja_preempt_disable();

  (void) jet_console_write ("FATAL ERROR: \n", 13);
  (void) jet_console_write (message , debug_strlen(message));

  POK_DEBUG_PRINT_CURRENT_STATE
  ja_inf_loop();
}

/**
 * @funcname `hexdump`
 *
 * @description
 * Prints a block of memory as hex bytes, 16 per line with their offset and
 * their printable characters.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `addr` | `const void *` | [in] The memory. |
 * | `len` | `int32_t` | [in] The number of bytes. |
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `buff` | `uint8_t[17]` | The printable characters of the current line |
 * | `i` | `int32_t` | The index of the byte |
 * | `pc` | `const uint8_t *` | The memory, as bytes |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Set `pc` to `(const uint8_t*)addr`.
 * 2. If `len` is equal to `0`:
 *    1. Call `printf()` with the argument `"Len is zero\n"`.
 *    2. Return.
 * 3. Set `i` to `0`; then while `i` is less than `len`, repeat (after each
 *    pass: increment `i`):
 *    1. If `(i % 16)` is equal to `0`:
 *       1. If `i` is not equal to `0`:
 *          1. Call `printf()` with the arguments `" %%s\n"` and `buff`.
 *       2. Call `printf()` with the arguments `" %04x "` and `i`.
 *    2. Call `printf()` with the arguments `" %02x"` and `pc[i]`.
 *    3. If `pc[i]` is less than `0x20U` or `pc[i]` is greater than `0x7eU`:
 *       1. Set `buff[i % 16]` to `'.'`.
 *    4. Otherwise:
 *       1. Set `buff[i % 16]` to `pc[i]`.
 *    5. Set `buff[(i % 16) + 1]` to `'\0'`.
 * 4. While `(i % 16)` is not equal to `0`:
 *    1. Call `printf()` with the argument `" "`.
 *    2. Increment `i`.
 * 5. Call `printf()` with the arguments `" %%s\n"` and `buff`.
 *
 * @calls
 * - `printf()`
 */
void hexdump (const void *addr, int32_t len)
{
    int32_t i;
    uint8_t buff[17];
    const uint8_t *pc = (const uint8_t*)addr;

    if (len == 0) {
        (void) printf("Len is zero\n");
        return;
    }

    // Process every byte in the data.
    for (i = 0; i < len; i++) {
        // Multiple of 16 means new line (with line offset).

        if ((i % 16) == 0) {
            // Just don't print ASCII for the zeroth line.
            if (i != 0) {
                (void) printf("  %s\n", buff);
            }

            // Output the offset.
            (void) printf("  %04x ", i);
        }

        // Now the hex code for the specific character.
        (void) printf(" %02x", pc[i]);

        // And store a printable ASCII character for later.
        if ((pc[i] < 0x20U) || (pc[i] > 0x7eU)) {
            buff[i % 16] = '.';
        } else {
            buff[i % 16] = pc[i];
        }
        buff[(i % 16) + 1] = '\0';
    }

    // Pad out last line if not exactly 16 characters.
    while ((i % 16) != 0) {
        (void) printf("   ");
        i++;
    }

    // And print the final ASCII bit.
    (void) printf("  %s\n", buff);
}

#endif /* POK_CONFIG_NEEDS_DEBUG */
