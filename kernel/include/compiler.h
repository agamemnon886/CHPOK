/*
 * Modified by Kirill Brazhnikov, 2026-11-01: MISRA C:2025: the header guard
 * isn't a reserved identifier, flag_test_and_reset() is an inline function
 * on bool instead of a GNU statement expression, the flag macros assign
 * true/false and end with while (false), and the GNU extensions which
 * remain are documented as deviations.
 *
 * Modified by Kirill Brazhnikov, 2026-11-03: long lines are wrapped at 80
 * columns.
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
/*
 * MISRA C:2025 Dir 1.2 deviation: typeof (a GNU extension) gives the volatile
 * access the type of the variable, for variables of any type; standard C99
 * has no equivalent.
 */
#define ACCESS_ONCE(x) (*(volatile typeof(x) *)&(x))

/* 
 * Set given variable as a signal of some operation performed before.
 */
#define flag_set(var) do { barrier(); ACCESS_ONCE(var) = true; } while (false)

/*
 * Return current value of variable and reset it.
 * 
 * Variable is assumed to contain some operation's indicator, which is
 * "consumed" by given operation.
 * 
 * If the flag is set, clear it and return true; otherwise return false.
 * 
 * Such way, if flag_set() has been called concurrently, then either
 * non-zero will be returned, or variable will become non-zero, so
 * it can be found to be set after.
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


/* 
 * Reset given variable.
 * 
 * Note: Using flag_reset() and flag_test_and_reset() for *single*
 * variable has no sence.
 */
#define flag_reset(var) \
    do { barrier(); ACCESS_ONCE(var) = false; } while (false)


#endif /* !POK_COMPILER_H */
