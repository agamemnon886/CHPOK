/* Modified by Kirill Brazhnikov, 2026-10-31: built-in integer types (int,
 * unsigned, short, long, ...) are replaced with the fixed-width types
 * from <stdint.h>.
 *
 * Modified by Kirill Brazhnikov, 2026-11-02: MISRA C:2025: essentially
 * Boolean conditions; the unused serial input code (cons_intr2(),
 * cons_getc2() and their buffer) is removed.
 */
#include <libc.h>
#include <cons.h>

/*
*   Function described in cons.c:
*   data_to_read_0() - return 1 if there is data to read, or 0 otherwise
*   read_serial_0()  - read and return 1 word from receiver buffer
*/


int32_t
getchar2(void)
{
    while (data_to_read_1() == 0){
    }
    return read_serial_1();
}

