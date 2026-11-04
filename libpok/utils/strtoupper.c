/* Modified by Kirill Brazhnikov, 2026-10-31: built-in integer types (int,
 * unsigned, short, long, ...) are replaced with the fixed-width types
 * from <stdint.h>.
 *
 * Modified by Kirill Brazhnikov, 2026-11-03: MISRA C:2025: chr_toupper() is
 * internal to this file and works on characters; braces around the if
 * body; the loop condition is a comparison with '\0'.
 */
#include <utils.h>

static char chr_toupper(char c)
{
  char upper = c;

  if ((c >= 'a') && (c <= 'z')) {
    upper = (c - 'a') + 'A';
  }

  return upper;
}

void strtoupper(char* s)
{
  int32_t i = 0;
  while (s[i] != '\0') {
     s[i] = chr_toupper(s[i]);
     i++;
  }
}
