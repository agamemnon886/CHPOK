/* Modified by Kirill Brazhnikov, 2026-10-31: built-in integer types (int,
 * unsigned, short, long, ...) are replaced with the fixed-width types
 * from <stdint.h>.
 */
#ifndef __POK_UTILS_H__
#define __POK_UTILS_H__
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
    uintptr_t mask = align - 1;
    return (val + mask) & ~mask;
}

/* 
 * Return minimal pointer, which is equal-or-more than
 * given one and has required alignment.
 */
static inline void* ALIGN_PTR(void* ptr, uintptr_t align)
{
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
