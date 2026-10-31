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
 * Modified by Kirill Brazhnikov, 2026-10-31: ported from
 * kernel/arch/x86/alloc.c to x86_64 (64-bit).
 */

#include <asp/alloc.h>
#include <asp/stack.h>
#include <common.h>
#include <libc.h>
#include <core/debug.h>
#include <bsp/bsp.h>

extern char __pok_end[];
static char *heap_end = __pok_end;

void *ja_mem_alloc_aligned (size_t size, unsigned int alignment)
{
  char *res;

  res = (char *)(ALIGN_VAL((uintptr_t)heap_end, alignment));
  heap_end = res + size;

  // Kernel heap must not overlap with partitions' memory.
  if ((uintptr_t)heap_end > BOARD_PARTITION_PHYS_START)
    pok_fatal("Kernel heap overflow");

  return res;
}

unsigned int ja_mem_get_alignment (size_t size)
{
  unsigned int alignment = 1;

  // The largest power of 2 not greater than the size, up to 16 (long double).
  while (alignment < 16 && alignment * 2 <= size)
    alignment *= 2;

  return alignment;
}

jet_stack_t pok_stack_alloc(uint32_t stack_size)
{
    /*
     * Align stack size.
     *
     * This is needed because we need aligned stack head, not only stack
     * tail. The System V ABI requires 16-byte stack alignment.
     */
    const unsigned int alignment = 16;
    uint32_t stack_size_real = ALIGN_VAL(stack_size, alignment);

    char* stack_tail = ja_mem_alloc_aligned(stack_size_real, alignment);
    /*
     * Reserve area *above* stack and fill it with zeroes.
     * These bytes compose terminated stack frame.
     *
     * See also comments for type 'jet_stack_t' on x86_64.
     */
    char* stack_head = stack_tail + stack_size_real - 2 * sizeof(uint64_t);
    memset(stack_head, 0, 2 * sizeof(uint64_t));

    return (jet_stack_t)stack_head;
}
