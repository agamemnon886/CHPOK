/*
 * Modified by Kirill Brazhnikov, 2026-11-01: MISRA C:2025: the header guard
 * isn't a reserved identifier, flag_test_and_reset() is an inline function
 * on bool instead of a GNU statement expression, the flag macros assign
 * true/false and end with while (false), and the GNU extensions which
 * remain are documented as deviations.
 *
 * Modified by Kirill Brazhnikov, 2026-11-03: long lines are wrapped at 80
 * columns.
 *
 * Modified by Kirill Brazhnikov, 2026-11-08: Doxygen headers for the file
 * and its functions.
 *
 * Modified by Kirill Brazhnikov, 2026-11-13: Doxygen comments for its macros.
 *
 * Modified by Kirill Brazhnikov, 2026-11-14: the Doxygen comment of the file is
 * removed (header files are not documented); the Doxygen headers of the static
 * inline functions are plain comments (they are not documented).
 */

#ifndef POK_COMPILER_H
#define POK_COMPILER_H

#include <stdbool.h>
/*
 * Functions and macros, which are compiler-specific.
 * 
 * Normally, there are operations which disables some compiler
 * optimizations, which can invalidate semanic when interrupts occures.
 * 
 * Currently assume compiler to be gcc.
 */

/**
 * @macroname `barrier`
 *
 * @description
 * A compiler memory barrier: the compiler doesn't move memory accesses across
 * it (GCC inline assembly).
 */
/*
 * MISRA C:2025 Dir 4.3 and Dir 1.2 deviation: a compiler memory barrier can
 * only be written as GCC inline assembly; it is isolated in this macro.
 */
#define barrier() __asm__ __volatile__("": : :"memory")

/*
 * Access (read or write) given variable only once. Do not cache its value.
 * 
 * Applicable to simple types, which can be accessed using single asm instruction.
 */

/**
 * @macroname `ACCESS_ONCE`
 *
 * @description
 * Access (read or write) given variable only once. Do not cache its value.
 * Applicable to simple types, which can be accessed using single asm
 * instruction (a volatile access of the variable's type).
 *
 * @params
 * | Name | Description |
 * |------|-------------|
 * | `x` | The variable. |
 */
/*
 * MISRA C:2025 Dir 1.2 deviation: typeof (a GNU extension) gives the volatile
 * access the type of the variable, for variables of any type; standard C99
 * has no equivalent.
 */
#define ACCESS_ONCE(x) (*(volatile typeof(x) *)&(x))

/**
 * @macroname `flag_set`
 *
 * @description
 * Set given variable as a signal of some operation performed before.
 *
 * @params
 * | Name | Description |
 * |------|-------------|
 * | `var` | The flag (a pok_bool_t variable). |
 */
#define flag_set(var) do { barrier(); ACCESS_ONCE(var) = true; } while (false)

/*
 * Reads a flag and clears it if it is set (the flag may be set by an interrupt
 * handler): the flag indicates an operation, which consumes it. If flag_set()
 * runs concurrently, either TRUE is returned or the flag stays set, to be found
 * later.
 */
static inline bool flag_test_and_reset(volatile bool *var)
{
    bool val = *var;

    if (val) {
        *var = false;
    }
    barrier();
    return val;
}


/**
 * @macroname `flag_reset`
 *
 * @description
 * Reset given variable.
 *
 * Note: Using flag_reset() and flag_test_and_reset() for *single* variable has
 * no sence.
 *
 * @params
 * | Name | Description |
 * |------|-------------|
 * | `var` | The flag (a pok_bool_t variable). |
 */
#define flag_reset(var) \
    do { barrier(); ACCESS_ONCE(var) = false; } while (false)


#endif /* !POK_COMPILER_H */
