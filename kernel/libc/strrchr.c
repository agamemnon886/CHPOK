/*
 * Modified by Kirill Brazhnikov, 2026-11-02: MISRA C:2025: braces around
 * all if/else/loop bodies; deviations are documented where a guideline
 * can't be followed.
 *
 * Modified by Kirill Brazhnikov, 2026-11-03: MISRA C:2025: an index loop
 * that doesn't modify the parameters. Long lines are wrapped at 80 columns.
 *
 * Modified by Kirill Brazhnikov, 2026-11-08: Doxygen headers for the file
 * and its functions.
 */

/**
 * @file
 * strrchr() of the kernel's C library.
 */

#include <libc.h>
/**
 * @funcname `strrchr`
 *
 * @description
 * Finds the last occurrence of a character in a string (C standard).
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `s` | `const char *` | [in] The string. |
 * | `c` | `int` | [in] The character (converted to char). |
 *
 * @return The address of the character, or NULL if it isn't there.
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `done` | `bool` | Whether the end of the string was reached |
 * | `i` | `size_t` | The index of the character |
 * | `last` | `const char *` | The last occurrence so far |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Set `last` to `NULL`.
 * 2. Set `i` to `0U`.
 * 3. Set `done` to `false`.
 * 4. While `done` is false:
 *    1. If `s[i]` is equal to `(char)c`:
 *       1. Set `last` to the address of `s[i]`.
 *    2. Set `done` to `(s[i] == '\0')`.
 *    3. Increment `i`.
 * 5. Return `(char *)last`.
 *
 * @calls
 * None.
 */
char *strrchr(const char *s, int c)
{
        const char *last = NULL;
        size_t i = 0U;
        bool done = false;

        while (!done) {
                if (s[i] == (char)c) {
                        last = &s[i];
                }
                done = (s[i] == '\0');
                i++;
        }
        /*
         * MISRA C:2025 Rule 11.8 deviation: the standard strrchr() returns a
         * pointer to char into the string it gets as a pointer to const.
         */
        return (char *)last;
}
