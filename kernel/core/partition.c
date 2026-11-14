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
 * unsigned constants; operands of the same essential type.
 *
 * Modified by Kirill Brazhnikov, 2026-11-03: MISRA C:2025: operands of
 * mixed precedence are parenthesized; deviations are documented where a
 * guideline can't be followed.
 *
 * Modified by Kirill Brazhnikov, 2026-11-04: partition_gdb is declared only
 * in <core/sched.h> (MISRA C:2025 Rule 8.5); the GDB callbacks, built on
 * x86_64 now, make operator precedence explicit (Rule 12.1).
 *
 * Modified by Kirill Brazhnikov, 2026-11-08: Doxygen headers for the file
 * and its functions.
 *
 * Modified by Kirill Brazhnikov, 2026-11-13: the structures and enums are used
 * by their typedef names: jet_partition_event_t, jet_partition_event_type_t,
 * pok_part_sched_ops_t.
 *
 * Modified by Kirill Brazhnikov, 2026-11-14: the enum types are named with _e:
 * jet_partition_event_type_e (was jet_partition_event_type_t).
 */

/**
 * @file
 * Common partition functions: the events queue of a partition, iteration
 * over all partitions, and the scheduler operations of kernel-only
 * partitions (one kernel thread).
 */

#include <config.h>
#include <core/partition_arinc.h>
#include <alloc.h>

#include <core/sched.h>    // partition_monitor, partition_gdb

/*
 * MISRA C:2025 Rule 11.5 deviation (this file): the allocator returns its
 * memory as a pointer to void, which is converted to the type of the object
 * allocated.
 * MISRA C:2025 Rule 18.4 deviation (this file): partition events are stored
 * in a ring, addressed by the start of the ring plus an index.
 */

/**
 * @funcname `pok_partition_init`
 *
 * @description
 * Initializes the common part of a partition: allocates its ring of events, if
 * it can receive events.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `part` | `pok_partition_t *` | [in,out] The partition. |
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
 * 1. If `part->partition_event_max` is not equal to `0U`:
 *    1. Call `ja_mem_alloc_aligned()` with the arguments
 *       `sizeof(*part->partition_events) * (part->partition_event_max + 1U)`
 *       and `__alignof__(*part->partition_events)`, and store the result in
 *       `part->partition_events`.
 * 2. Otherwise:
 *    1. Set `part->partition_events` to `NULL`.
 *
 * @calls
 * - `ja_mem_alloc_aligned()`
 */
void pok_partition_init(pok_partition_t* part)
{
   if(part->partition_event_max != 0U)
   {
      /*
       * DEV: Allocate one element more than required.
       * 
       * This simplifies implementation of ring buffer, when
       * extracting messages may be concurrent with adding them.
       */ 
      part->partition_events = ja_mem_alloc_aligned(
         sizeof(*part->partition_events) * (part->partition_event_max + 1U),
         __alignof__(*part->partition_events));
   }
   else
   {
      part->partition_events = NULL;
   }
}

/**
 * @funcname `for_each_partition`
 *
 * @description
 * Calls a function for every partition: the monitor and GDB partitions (if
 * configured) and the ARINC 653 partitions.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `f` | `void (*)(pok_partition_t* part)` | [in] The function. |
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `partition_gdb` | `pok_partition_t` | The GDB server partition |
 * | `partition_monitor` | `pok_partition_t` | The monitor partition |
 * | `pok_partitions_arinc` | `pok_partition_arinc_t[]` | The ARINC 653 partitions (generated) |
 * | `pok_partitions_arinc_n` | `const uint8_t` | Their number |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `i` | `uint32_t` | The index of an ARINC 653 partition |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Call the function `f` with the address of `partition_monitor`.
 * 2. Call the function `f` with the address of `partition_gdb`.
 * 3. Set `i` to `0U`; then while `i` is less than `pok_partitions_arinc_n`,
 *    repeat (after each pass: increment `i`):
 *    1. Call the function `f` with the address of
 *       `pok_partitions_arinc[i].base_part`.
 *
 * @calls
 * - `f()` (indirect)
 */
void for_each_partition(void (*f)(pok_partition_t* part))
{
#ifdef POK_NEEDS_MONITOR
   f(&partition_monitor);
#endif /* POK_NEEDS_MONITOR */

#ifdef POK_NEEDS_GDB
   f(&partition_gdb);
#endif /* POK_NEEDS_GDB */


   for(uint32_t i = 0U; i < pok_partitions_arinc_n; i++)
   {
      f(&pok_partitions_arinc[i].base_part);
   }
}

/**
 * @funcname `pok_partition_add_event`
 *
 * @description
 * Adds an event to a partition's ring and marks the partition as having events
 * if the ring was empty. Called with global preemption disabled.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `part` | `pok_partition_t *` | [in,out] The partition. |
 * | `event_type` | `jet_partition_event_type_e` | [in] The type of the event. |
 * | `handler_id` | `uint16_t` | [in] The object the event is for (e.g. a port). |
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `partition_event` | `jet_partition_event_t *` | The ring element written |
 * | `set_event` | `pok_bool_t` | Whether the ring was empty |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Set `set_event` to
 *    `(part->partition_event_end == part->partition_event_begin)`.
 * 2. Check that `part->partition_event_max != 0U` holds (assert: stop
 *    otherwise).
 * 3. Set `partition_event` to
 *    `part->partition_events + part->partition_event_end`.
 * 4. Set `partition_event->handler_id` to `handler_id`.
 * 5. Set `partition_event->event_type` to `event_type`.
 * 6. Increment `part->partition_event_end`.
 * 7. If `part->partition_event_end` is greater than
 *    `part->partition_event_max`:
 *    1. Set `part->partition_event_end` to `0`.
 * 8. Check that `part->partition_event_end != part->partition_event_begin`
 *    holds (assert: stop otherwise).
 * 9. If `set_event` is true:
 *    1. Set `part->is_event` to `TRUE`.
 *
 * @calls
 * - `pok_assertion_fail()`
 */
void pok_partition_add_event(pok_partition_t* part,
    jet_partition_event_type_e event_type,
    uint16_t handler_id)
{
   // Whether it is needed to set is_event flag for partition.
   pok_bool_t set_event =
      (part->partition_event_end == part->partition_event_begin);
   /* 
    * TODO: This is a result of configuration error, when partition
    * doesn't expect events at all.
    */
   assert(part->partition_event_max != 0U);

   jet_partition_event_t* partition_event = part->partition_events
      + part->partition_event_end;

   partition_event->handler_id = handler_id;
   partition_event->event_type = event_type;

   part->partition_event_end++;
   if(part->partition_event_end > part->partition_event_max) {
      part->partition_event_end = 0;
   }

   /*
    * TODO: This is a result of configuration error, when partition
    * doesn't expect so much events pending.
    */
   assert(part->partition_event_end != part->partition_event_begin);

   if(set_event) {
      part->is_event = TRUE;
   }
}


/**
 * @funcname `pok_partition_get_event`
 *
 * @description
 * Takes the next event from the current partition's ring. Called with local
 * preemption disabled.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `event` | `jet_partition_event_t *` | [out] Where the event is stored. |
 *
 * @return TRUE if an event was taken, FALSE if the ring is empty.
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `current_partition` | `pok_partition_t *` | The running partition |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `part` | `pok_partition_t *` | The current partition |
 * | `partition_event_begin` | `uint16_t` | The ring's start |
 * | `partition_event_end` | `uint16_t` | The ring's end (read once) |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Set `part` to `current_partition`.
 * 2. Set `partition_event_begin` to `part->partition_event_begin`.
 * 3. Set `partition_event_end` to `ACCESS_ONCE(part->partition_event_end)`.
 * 4. If `partition_event_end` is equal to `partition_event_begin`:
 *    1. Return `FALSE`.
 * 5. Set `*event` to
 *    `ACCESS_ONCE(part->partition_events[partition_event_begin])`.
 * 6. Use the macro `ACCESS_ONCE()` with the argument
 *    `part->partition_event_begin) = (partition_event_begin ==`
 *    `part->partition_event_max) ? 0U: (partition_event_begin + 1U`.
 * 7. Return `TRUE`.
 *
 * @calls
 * None.
 */
pok_bool_t pok_partition_get_event(jet_partition_event_t* event)
{
   pok_partition_t* part = current_partition;

   uint16_t partition_event_begin = part->partition_event_begin;
   uint16_t partition_event_end = ACCESS_ONCE(part->partition_event_end);

   if(partition_event_end == partition_event_begin) {
      return FALSE;
   }

   *event = ACCESS_ONCE(part->partition_events[partition_event_begin]);

   ACCESS_ONCE(part->partition_event_begin) =
      (partition_event_begin == part->partition_event_max) ?
         0U: (partition_event_begin + 1U);

   return TRUE;
}

/**
 * @funcname `pok_partition_set_timer`
 *
 * @description
 * Sets the time at which a partition wants its timer event.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `part` | `pok_partition_t *` | [out] The partition. |
 * | `timer_new` | `pok_time_t` | [in] The time, or 0 for none. |
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
 * 1. Set `part->timer` to `timer_new`.
 *
 * @calls
 * None.
 */
void pok_partition_set_timer(pok_partition_t* part,
    pok_time_t timer_new)
{
     part->timer = timer_new;
}


/**
 * @funcname `kernel_thread_on_event`
 *
 * @description
 * The on_event operation of kernel-only partitions: never called.
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
 * 1. Use the macro `unreachable()`.
 *
 * @calls
 * - `pok_assertion_fail()`
 */
static void kernel_thread_on_event(void)
{
    //Shouldn't be call
    unreachable();
}

#ifdef POK_NEEDS_GDB

/**
 * @funcname `kernel_thread_get_number_of_threads`
 *
 * @description
 * The GDB operation of kernel-only partitions: the number of threads.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `part` | `pok_partition_t *` | [in] The partition (not used). |
 *
 * @return 1: the kernel thread.
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
 * 1. Mark `part` as not used.
 * 2. Return `1`.
 *
 * @calls
 * None.
 */
static int32_t kernel_thread_get_number_of_threads(pok_partition_t* part)
{
   (void)part;
   return 1;
}

/**
 * @funcname `kernel_thread_get_current_thread_index`
 *
 * @description
 * The GDB operation of kernel-only partitions: the index of the current thread.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `part` | `pok_partition_t *` | [in] The partition (not used). |
 *
 * @return 0: the kernel thread.
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
 * 1. Mark `part` as not used.
 * 2. Return `0`.
 *
 * @calls
 * None.
 */
static int32_t kernel_thread_get_current_thread_index(pok_partition_t* part)
{
   (void)part;
   return 0;
}

/**
 * @funcname `kernel_thread_get_thread_at_index`
 *
 * @description
 * The GDB operation of kernel-only partitions: whether there is a thread at an
 * index.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `part` | `pok_partition_t *` | [in] The partition (not used). |
 * | `index` | `int32_t` | [in] The index. |
 * | `private` | `void **` | [in] The thread's private data (not used). |
 *
 * @return 0 for index 0, 1 (no thread) otherwise.
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
 * 1. Mark `part` as not used.
 * 2. Mark `private` as not used.
 * 3. Return `0` if `index` is equal to `0`, otherwise `1`.
 *
 * @calls
 * None.
 */
static int32_t kernel_thread_get_thread_at_index(pok_partition_t* part,
     int32_t index, void** private)
{
   (void)part;
   (void)private;
   return (index == 0) ? 0 : 1;
}

/**
 * @funcname `kernel_thread_get_thread_info`
 *
 * @description
 * The GDB operation of kernel-only partitions: the description of the thread.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `part` | `pok_partition_t *` | [in] The partition (not used). |
 * | `index` | `int32_t` | [in] The index (not used). |
 * | `private` | `void *` | [in] The thread's private data (not used). |
 * | `print_cb` | `print_cb_t` | [in] The function that prints the description. |
 * | `cb_data` | `void *` | [in] The data passed to print_cb. |
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `kernel_thread_name` | `static const char *` | The description: "kernel_thread" |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Mark `part` as not used.
 * 2. Mark `private` as not used.
 * 3. Mark `index` as not used.
 * 4. Call the function `print_cb` with the arguments `kernel_thread_name`,
 *    `strlen(kernel_thread_name)` and `cb_data`.
 *
 * @calls
 * - `print_cb()` (indirect)
 * - `strlen()`
 */
static void kernel_thread_get_thread_info(pok_partition_t* part,
   int32_t index, void* private,
   print_cb_t print_cb, void* cb_data)
{
   (void)part;
   (void)private;
   (void)index;

   static const char* kernel_thread_name = "kernel_thread";

   print_cb(kernel_thread_name, strlen(kernel_thread_name), cb_data);
}

#endif /* POK_NEEDS_GDB */

const pok_part_sched_ops_t partition_sched_ops_kernel =
{
   .on_event = &kernel_thread_on_event,
#ifdef POK_NEEDS_GDB
   .get_number_of_threads = &kernel_thread_get_number_of_threads,
   .get_current_thread_index = &kernel_thread_get_current_thread_index,
   .get_thread_at_index = &kernel_thread_get_thread_at_index,
   .get_thread_info = &kernel_thread_get_thread_info
#endif /* POK_NEEDS_GDB */
};
