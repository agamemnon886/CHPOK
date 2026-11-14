/*
 * Modified by Kirill Brazhnikov, 2026-10-31: built-in integer types (int,
 * unsigned, short, long, ...) are replaced with the fixed-width types
 * from <stdint.h>.
 *
 * Modified by Kirill Brazhnikov, 2026-11-03: MISRA C:2025: chr_toupper() is
 * internal to this file and works on characters; braces around the if
 * body; the loop condition is a comparison with '\0'.
 *
 * Modified by Kirill Brazhnikov, 2026-11-08: Doxygen headers for the file
 * and its functions.
 */

/**
 * @file
 * strtoupper(): converts a string to upper case in place.
 */
#include <utils.h>

/**
 * @funcname `chr_toupper`
 *
 * @description
 * Converts an ASCII letter to upper case.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `c` | `char` | [in] The character. |
 *
 * @return The upper case letter, or c if it isn't a lower case letter.
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `upper` | `char` | The result |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Set `upper` to `c`.
 * 2. If `c` is greater than or equal to `'a'` and `c` is less than or equal to
 *    `'z'`:
 *    1. Set `upper` to `(c - 'a') + 'A'`.
 * 3. Return `upper`.
 *
 * @calls
 * None.
 */
static char chr_toupper(char c)
{
  char upper = c;

  if ((c >= 'a') && (c <= 'z')) {
    upper = (c - 'a') + 'A';
  }

  return upper;
}

/**
 * @funcname `strtoupper`
 *
 * @description
 * Converts a string to upper case in place.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `s` | `char *` | [in,out] The string. |
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `i` | `int32_t` | The index of the character |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Set `i` to `0`.
 * 2. While `s[i]` is not equal to `'\0'`:
 *    1. Call `chr_toupper()` with the argument `s[i]`, and store the result in
 *       `s[i]`.
 *    2. Increment `i`.
 *
 * @calls
 * - `chr_toupper()`
 */
void strtoupper(char* s)
{
  int32_t i = 0;
  while (s[i] != '\0') {
     s[i] = chr_toupper(s[i]);
     i++;
  }
}
