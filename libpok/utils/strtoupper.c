/* Modified by Kirill Brazhnikov, 2026-10-31: built-in integer types (int,
 * unsigned, short, long, ...) are replaced with the fixed-width types
 * from <stdint.h>.
 */
#include <utils.h>

int32_t chr_toupper(int32_t c)
{
  if (c >= 'a' && c <= 'z') return 'A' + c - 'a';

  return c;
}

void strtoupper(char* s)
{
  int32_t i=0;
  while(s[i]){
     s[i]=chr_toupper(s[i]);
     i++;
     }
}
