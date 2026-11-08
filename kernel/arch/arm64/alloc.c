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
 * Modified by Kirill Brazhnikov, 2026-10-31: adapted from
 * kernel/arch/x86/alloc.c for arm64.
 *
 * Modified by Kirill Brazhnikov, 2026-10-31: built-in integer types (int,
 * unsigned, short, long, ...) are replaced with the fixed-width types
 * from <stdint.h>.
 *
 * Modified by Kirill Brazhnikov, 2026-11-02: MISRA C:2025: braces around
 * all if/else/loop bodies; U suffixes for unsigned constants; discarded
 * results are cast to void; deviations are documented where a guideline
 * can't be followed.
 *
 * Modified by Kirill Brazhnikov, 2026-11-03: MISRA C:2025: heap_end is
 * defined in the function that uses it; operands of mixed precedence are
 * parenthesized; deviations are documented where a guideline can't be
 * followed.
 *
 * Modified by Kirill Brazhnikov, 2026-11-08: ja_mem_alloc_aligned() checks
 * the size against the space left before adding it to the address, so a
 * large size can't wrap around past the limit check.
 */

#include <asp/alloc.h>
#include <asp/stack.h>
#include <common.h>
#include <libc.h>
#include <core/debug.h>
#include <bsp/bsp.h>

/*
 * MISRA C:2025 Rule 11.5 deviation (this file): the allocator returns its
 * memory as a pointer to void, which is converted to the type of the object
 * allocated.
 * MISRA C:2025 Rule 18.4 deviation (this file): the heap and the stacks are
 * carved out of memory by adding sizes to addresses.
 */

extern char __pok_end[];

/*
 * MISRA C:2025 Rule 11.4 deviation (this file): the kernel heap and the
 * stacks are carved out of physical memory; their addresses are aligned and
 * compared with the board's limits as integers.
 */
void *ja_mem_alloc_aligned (size_t size, uint32_t alignment)
{
  static char *heap_end = __pok_end;

  char *res;

  res = (char *)(ALIGN_VAL((uintptr_t)heap_end, alignment));

  // Kernel heap must not overlap with partitions' memory.
  if (((uintptr_t)res > BOARD_PARTITION_PHYS_START)
      || (size > (BOARD_PARTITION_PHYS_START - (uintptr_t)res))) {
    pok_fatal("Kernel heap overflow");
  }

  heap_end = res + size;

  return res;
}

uint32_t ja_mem_get_alignment (size_t size)
{
  uint32_t alignment = 1;

  // The largest power of 2 not greater than the size, up to 16 (long double).
  while ((alignment < 16U) && ((alignment * 2U) <= size)) {
    alignment *= 2U;
  }

  return alignment;
}

jet_stack_t pok_stack_alloc(uint32_t stack_size)
{
    /*
     * Align stack size.
     *
     * This is needed because we need aligned stack head, not only stack
     * tail. AAPCS64 requires 16-byte stack alignment.
     */
    const uint32_t alignment = 16;
    uint32_t stack_size_real = ALIGN_VAL(stack_size, alignment);

    char* stack_tail = ja_mem_alloc_aligned(stack_size_real, alignment);
    /*
     * Reserve area *above* stack and fill it with zeroes.
     * These bytes compose terminated stack frame record (fp, lr).
     */
    char* stack_head = stack_tail + stack_size_real - (2U * sizeof(uint64_t));
    (void) memset(stack_head, 0, 2U * sizeof(uint64_t));

    return (jet_stack_t)stack_head;
}
