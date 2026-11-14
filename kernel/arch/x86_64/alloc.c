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
 * large size can't wrap around past the limit check. Doxygen headers for the
 * file and its functions.
 *
 * Modified by Kirill Brazhnikov, 2026-11-13: the declaration of __pok_end is
 * moved to kernel/include/asp/alloc.h.
 *
 * Modified by Kirill Brazhnikov, 2026-11-14: The function headers no longer
 * have a step for the declaration of a local variable; the @locals table
 * describes it, and an initialized one is set in a step.
 */

/**
 * @file
 * Memory allocation of the kernel on x86_64: the kernel heap after the
 * kernel image, and kernel stacks.
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


/**
 * @funcname `ja_mem_alloc_aligned`
 *
 * @description
 * Allocates memory from the kernel heap, which starts after the kernel image
 * and must stay below the partitions' memory; it is never freed.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `size` | `size_t` | [in] The size. |
 * | `alignment` | `uint32_t` | [in] The alignment, a power of 2. |
 *
 * @return The memory (the kernel stops if the heap overflows).
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `__pok_end` | `char[]` | The end of the kernel image (linker script) |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `heap_end` | `static char *` | The end of the heap used so far |
 * | `res` | `char *` | The memory allocated |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Set `res` to `(char *)(ALIGN_VAL((uintptr_t)heap_end, alignment))`.
 * 2. If `(uintptr_t)res` is greater than `BOARD_PARTITION_PHYS_START` or `size`
 *    is greater than `(BOARD_PARTITION_PHYS_START - (uintptr_t)res)`:
 *    1. Call `pok_fatal()` with the argument `"Kernel heap overflow"`.
 * 3. Set `heap_end` to `res + size`.
 * 4. Return `res`.
 *
 * @calls
 * - `ALIGN_VAL()`
 * - `pok_fatal()`
 */
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

/**
 * @funcname `ja_mem_get_alignment`
 *
 * @description
 * Returns the alignment for an object of a size: the largest power of 2 not
 * greater than the size, up to 16.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `size` | `size_t` | [in] The size. |
 *
 * @return The alignment.
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `alignment` | `uint32_t` | The alignment |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Set `alignment` to `1`.
 * 2. While `alignment` is less than `16U` and `(alignment * 2U)` is less than
 *    or equal to `size`:
 *    1. Multiply `alignment` by `2U`.
 * 3. Return `alignment`.
 *
 * @calls
 * None.
 */
uint32_t ja_mem_get_alignment (size_t size)
{
  uint32_t alignment = 1;

  // The largest power of 2 not greater than the size, up to 16 (long double).
  while ((alignment < 16U) && ((alignment * 2U) <= size)) {
    alignment *= 2U;
  }

  return alignment;
}

/**
 * @funcname `pok_stack_alloc`
 *
 * @description
 * Allocates a kernel stack of a size (rounded up to 16 bytes), with a zeroed
 * terminating frame at its head.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `stack_size` | `uint32_t` | [in] The size. |
 *
 * @return The head of the stack.
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `alignment` | `const uint32_t` | The stack alignment (System V ABI) |
 * | `stack_head` | `char *` | The head of the stack |
 * | `stack_size_real` | `uint32_t` | The size rounded up |
 * | `stack_tail` | `char *` | The bottom of the stack |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Set `alignment` to `16`.
 * 2. Call `ALIGN_VAL()` with the arguments `stack_size` and `alignment`, and
 *    store the result in `stack_size_real`.
 * 3. Call `ja_mem_alloc_aligned()` with the arguments `stack_size_real` and
 *    `alignment`, and store the result in `stack_tail`.
 * 4. Set `stack_head` to
 *    `stack_tail + stack_size_real - (2U * sizeof(uint64_t))`.
 * 5. Call `memset()` with the arguments `stack_head`, `0` and
 *    `2U * sizeof(uint64_t)`.
 * 6. Return `(jet_stack_t)stack_head`.
 *
 * @calls
 * - `ALIGN_VAL()`
 * - `ja_mem_alloc_aligned()`
 * - `memset()`
 */
jet_stack_t pok_stack_alloc(uint32_t stack_size)
{
    /*
     * Align stack size.
     *
     * This is needed because we need aligned stack head, not only stack
     * tail. The System V ABI requires 16-byte stack alignment.
     */
    const uint32_t alignment = 16;
    uint32_t stack_size_real = ALIGN_VAL(stack_size, alignment);

    char* stack_tail = ja_mem_alloc_aligned(stack_size_real, alignment);
    /*
     * Reserve area *above* stack and fill it with zeroes.
     * These bytes compose terminated stack frame.
     *
     * See also comments for type 'jet_stack_t' on x86_64.
     */
    char* stack_head = stack_tail + stack_size_real - (2U * sizeof(uint64_t));
    (void) memset(stack_head, 0, 2U * sizeof(uint64_t));

    return (jet_stack_t)stack_head;
}
