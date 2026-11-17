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
 *
 * Modified by Kirill Brazhnikov, 2026-11-15: moved from
 * kernel/arch/x86_64/alloc.c (the CPU support packages are in kernel/csps).
 *
 * Modified by Kirill Brazhnikov, 2026-11-16: high and low level requirements of
 * the functions, traced to the system requirements (docs/SRQs), and the steps
 * of their design linked to the LLRs.
 *
 * Modified by Kirill Brazhnikov, 2026-11-17: low level requirements of one
 * behaviour each; code that compiles to nothing (assertions, (void) x;,
 * empty branches) is left out of the design; requirements with more than one
 * "shall" are split into requirements of one "shall" each, and the high level
 * requirements trace to the system requirements split in the same way.
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
 * - \hlrid{HLR_ALLOC_X86_64_JA_MEM_ALLOC_ALIGNED_01}: `ja_mem_alloc_aligned()`
 *   shall allocate memory with an alignment from the kernel heap, which starts
 *   after the kernel image, never frees memory and ends below the memory of the
 *   partitions.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_MEM_010}
 *   - \srqref{SRQ_JET_MEM_019}
 * - \hlrid{HLR_ALLOC_X86_64_JA_MEM_ALLOC_ALIGNED_02}: `ja_mem_alloc_aligned()`
 *   shall stop the kernel when an allocation would not end below the memory of
 *   the partitions.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_MEM_010}
 *
 * @llr
 * - \llrid{LLR_ALLOC_X86_64_JA_MEM_ALLOC_ALIGNED_01}: If `heap_end` rounded up
 *   to `alignment` (`ALIGN_VAL()`) is greater than
 *   `BOARD_PARTITION_PHYS_START`, or `size` is greater than
 *   `BOARD_PARTITION_PHYS_START` minus that rounded address,
 *   `ja_mem_alloc_aligned()` shall call `pok_fatal()` with `"Kernel heap
 *   overflow"`.
 *   <br>Traces to:
 *   - \hlrref{HLR_ALLOC_X86_64_JA_MEM_ALLOC_ALIGNED_02}
 * - \llrid{LLR_ALLOC_X86_64_JA_MEM_ALLOC_ALIGNED_02}: If `heap_end` rounded up
 *   to `alignment` (`ALIGN_VAL()`) is not greater than
 *   `BOARD_PARTITION_PHYS_START` and `size` is not greater than
 *   `BOARD_PARTITION_PHYS_START` minus that rounded address,
 *   `ja_mem_alloc_aligned()` shall set `heap_end` to the rounded address plus
 *   `size` and return the rounded address, without calling `pok_fatal()`.
 *   <br>Traces to:
 *   - \hlrref{HLR_ALLOC_X86_64_JA_MEM_ALLOC_ALIGNED_01}
 *
 * @design
 * 1. Set `res` to `(char *)(ALIGN_VAL((uintptr_t)heap_end, alignment))`.
 *    \llrref{LLR_ALLOC_X86_64_JA_MEM_ALLOC_ALIGNED_01},
 *    \llrref{LLR_ALLOC_X86_64_JA_MEM_ALLOC_ALIGNED_02}
 * 2. If `(uintptr_t)res` is greater than `BOARD_PARTITION_PHYS_START` or `size`
 *    is greater than `(BOARD_PARTITION_PHYS_START - (uintptr_t)res)`:
 *    \llrref{LLR_ALLOC_X86_64_JA_MEM_ALLOC_ALIGNED_01}
 *    1. Call `pok_fatal()` with the argument `"Kernel heap overflow"`.
 * 3. Set `heap_end` to `res + size`.
 *    \llrref{LLR_ALLOC_X86_64_JA_MEM_ALLOC_ALIGNED_02}
 * 4. Return `res`. \llrref{LLR_ALLOC_X86_64_JA_MEM_ALLOC_ALIGNED_02}
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
 * - \hlrid{HLR_ALLOC_X86_64_JA_MEM_GET_ALIGNMENT_01}: `ja_mem_get_alignment()`
 *   shall give the alignment of an object of a size: the largest power of 2 not
 *   greater than the size, at most 16.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_MEM_010}
 *
 * @llr
 * - \llrid{LLR_ALLOC_X86_64_JA_MEM_GET_ALIGNMENT_01}: `ja_mem_get_alignment()`
 *   shall return the largest of 1, 2, 4, 8 and 16 that is not greater than
 *   `size`, and 1 if `size` is 0.
 *   <br>Traces to:
 *   - \hlrref{HLR_ALLOC_X86_64_JA_MEM_GET_ALIGNMENT_01}
 *
 * @design
 * 1. Set `alignment` to `1`. \llrref{LLR_ALLOC_X86_64_JA_MEM_GET_ALIGNMENT_01}
 * 2. While `alignment` is less than `16U` and `(alignment * 2U)` is less than
 *    or equal to `size`: \llrref{LLR_ALLOC_X86_64_JA_MEM_GET_ALIGNMENT_01}
 *    1. Multiply `alignment` by `2U`.
 * 3. Return `alignment`. \llrref{LLR_ALLOC_X86_64_JA_MEM_GET_ALIGNMENT_01}
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
 * - \hlrid{HLR_ALLOC_X86_64_POK_STACK_ALLOC_01}: `pok_stack_alloc()` shall
 *   allocate a kernel stack of a size rounded up to 16 bytes from the kernel
 *   heap, with a terminating frame of zeros at its head, and give its head.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_THR_013}
 *   - \srqref{SRQ_JET_MEM_010}
 *
 * @llr
 * - \llrid{LLR_ALLOC_X86_64_POK_STACK_ALLOC_01}: `pok_stack_alloc()` shall call
 *   `ja_mem_alloc_aligned()` once with `stack_size` rounded up to a multiple of
 *   16 (`ALIGN_VAL()`) and the alignment 16, set the last 16 bytes of the
 *   memory that it gives to 0 with `memset()`, and return the address of these
 *   16 bytes.
 *   <br>Traces to:
 *   - \hlrref{HLR_ALLOC_X86_64_POK_STACK_ALLOC_01}
 *
 * @design
 * 1. Set `alignment` to `16`. \llrref{LLR_ALLOC_X86_64_POK_STACK_ALLOC_01}
 * 2. Call `ALIGN_VAL()` with the arguments `stack_size` and `alignment`, and
 *    store the result in `stack_size_real`.
 *    \llrref{LLR_ALLOC_X86_64_POK_STACK_ALLOC_01}
 * 3. Call `ja_mem_alloc_aligned()` with the arguments `stack_size_real` and
 *    `alignment`, and store the result in `stack_tail`.
 *    \llrref{LLR_ALLOC_X86_64_POK_STACK_ALLOC_01}
 * 4. Set `stack_head` to
 *    `stack_tail + stack_size_real - (2U * sizeof(uint64_t))`.
 *    \llrref{LLR_ALLOC_X86_64_POK_STACK_ALLOC_01}
 * 5. Call `memset()` with the arguments `stack_head`, `0` and
 *    `2U * sizeof(uint64_t)`. \llrref{LLR_ALLOC_X86_64_POK_STACK_ALLOC_01}
 * 6. Return `(jet_stack_t)stack_head`.
 *    \llrref{LLR_ALLOC_X86_64_POK_STACK_ALLOC_01}
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
