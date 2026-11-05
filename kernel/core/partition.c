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

/* 
 * Add event to partition's queue and notify it if needed.
 * 
 * Should be called with global preemption disabled.
 */
void pok_partition_add_event(pok_partition_t* part,
    enum jet_partition_event_type event_type,
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

   struct jet_partition_event* partition_event = part->partition_events
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


/* 
 * Consume event from current partition's queue.
 * 
 * Return TRUE on success, FALSE if the queue is empty.
 * 
 * Should be called with local preemption disabled.
 */
pok_bool_t pok_partition_get_event(struct jet_partition_event* event)
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

void pok_partition_set_timer(pok_partition_t* part,
    pok_time_t timer_new)
{
     part->timer = timer_new;
}


static void kernel_thread_on_event(void)
{
    //Shouldn't be call
    unreachable();
}

#ifdef POK_NEEDS_GDB

static int32_t kernel_thread_get_number_of_threads(pok_partition_t* part)
{
   (void)part;
   return 1;
}

static int32_t kernel_thread_get_current_thread_index(pok_partition_t* part)
{
   (void)part;
   return 0;
}

static int32_t kernel_thread_get_thread_at_index(pok_partition_t* part,
     int32_t index, void** private)
{
   (void)part;
   (void)private;
   return (index == 0) ? 0 : 1;
}

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

const struct pok_partition_sched_operations partition_sched_ops_kernel =
{
   .on_event = &kernel_thread_on_event,
#ifdef POK_NEEDS_GDB
   .get_number_of_threads = &kernel_thread_get_number_of_threads,
   .get_current_thread_index = &kernel_thread_get_current_thread_index,
   .get_thread_at_index = &kernel_thread_get_thread_at_index,
   .get_thread_info = &kernel_thread_get_thread_info
#endif /* POK_NEEDS_GDB */
};
