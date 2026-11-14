/*
 * Modified by Kirill Brazhnikov, 2026-10-31: built-in integer types (int,
 * unsigned, short, long, ...) are replaced with the fixed-width types
 * from <stdint.h>.
 *
 * Modified by Kirill Brazhnikov, 2026-11-02: MISRA C:2025: the header
 * guard isn't a reserved identifier; U suffixes for unsigned constants;
 * deviations are documented where a guideline can't be followed.
 *
 * Modified by Kirill Brazhnikov, 2026-11-03: long lines are wrapped at 80
 * columns.
 *
 * Modified by Kirill Brazhnikov, 2026-11-08: Doxygen headers for the file
 * and its functions.
 *
 * Modified by Kirill Brazhnikov, 2026-11-12: the function declarations are
 * hidden from Doxygen (\cond), which would otherwise document the functions a
 * second time, in the section of this header.
 *
 * Modified by Kirill Brazhnikov, 2026-11-13: Doxygen header for ARRAY_SIZE(),
 * whose missing closing parenthesis is added.
 *
 * Modified by Kirill Brazhnikov, 2026-11-14: the Doxygen comment of the file is
 * removed (header files are not documented); the Doxygen headers of the static
 * inline functions are plain comments (they are not documented).
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
 * Returns the minimal value which is equal or more than a value and has an
 * alignment.
 */
static inline uintptr_t ALIGN(uintptr_t val, uintptr_t align)
{
    uintptr_t mask = align - 1U;
    return (val + mask) & ~mask;
}

/*
 * Returns the minimal pointer which is equal or more than a pointer and has an
 * alignment.
 */
static inline void* ALIGN_PTR(void* ptr, uintptr_t align)
{
    /* MISRA C:2025 Rule 11.6 deviation: alignment is computed on the
     * address. */
    return (void*)ALIGN((uintptr_t)ptr, align);
}

/**
 * @macroname `ARRAY_SIZE`
 *
 * @description
 * Return number of elements in the array: a size_t constant expression.
 *
 * The array should be declared as `<type> arr[<num>];` (not a pointer,
 * whose size would be divided instead).
 *
 * @params
 * | Name | Description |
 * |------|-------------|
 * | `arr` | The array. |
 */
#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))

// TODO: this should be removed as it transforms possible read-only string.
/** \cond */
void strtoupper(char* s);
/** \endcond */

#endif
