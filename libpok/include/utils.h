/* Modified by Kirill Brazhnikov, 2026-10-31: built-in integer types (int,
 * unsigned, short, long, ...) are replaced with the fixed-width types
 * from <stdint.h>.
 *
 * Modified by Kirill Brazhnikov, 2026-11-02: MISRA C:2025: the header
 * guard isn't a reserved identifier; U suffixes for unsigned constants;
 * deviations are documented where a guideline can't be followed.
 *
 * Modified by Kirill Brazhnikov, 2026-11-03: long lines are wrapped at 80
 * columns.
 */
#ifndef POK_UTILS_H
#define POK_UTILS_H
#include <stdint.h>

/* 
 * Useful functions for enforce alignment.
 * 
 * By analogy with Linux kernel.
 */

/* 
 * Return minimal value, which is equal-or-more than given one
 * and has required alignment.
 */
static inline uintptr_t ALIGN(uintptr_t val, uintptr_t align)
{
    uintptr_t mask = align - 1U;
    return (val + mask) & ~mask;
}

/* 
 * Return minimal pointer, which is equal-or-more than
 * given one and has required alignment.
 */
static inline void* ALIGN_PTR(void* ptr, uintptr_t align)
{
    /* MISRA C:2025 Rule 11.6 deviation: alignment is computed on the
     * address. */
    return (void*)ALIGN((uintptr_t)ptr, align);
}

/* Return number of elements in the array.
 * 
 * The array should be declared as '<type> arr[<num>];'.
 */
#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0])

// TODO: this should be removed as it transforms possible read-only string.
void strtoupper(char* s);

#endif
