/*
 * Modified by Kirill Brazhnikov, 2026-10-31: built-in integer types (int,
 * unsigned, short, long, ...) are replaced with the fixed-width types
 * from <stdint.h>.
 *
 * Modified by Kirill Brazhnikov, 2026-11-02: MISRA C:2025: essentially
 * Boolean conditions; the unused serial input code (cons_intr2(),
 * cons_getc2() and their buffer) is removed.
 *
 * Modified by Kirill Brazhnikov, 2026-11-08: Doxygen headers for the file
 * and its functions.
 *
 * Modified by Kirill Brazhnikov, 2026-11-15: moved from kernel/libc/console2.c
 * (kernel/libc is split up: the routines only the kernel uses are in
 * kernel/core).
 *
 * Modified by Kirill Brazhnikov, 2026-11-16: high and low level requirements of
 * the functions, traced to the system requirements (docs/SRQs), and the steps
 * of their design linked to the LLRs.
 *
 * Modified by Kirill Brazhnikov, 2026-11-17: low level requirements of one
 * behaviour each; code that compiles to nothing (assertions, (void) x;,
 * empty branches) is left out of the design; the high level requirements
 * trace to the system requirements split into requirements of one "shall"
 * each.
 */

/**
 * @file
 * getchar2(): characters of the debug console (the second serial port).
 */
#include <libc.h>
#include <cons.h>

/*
*   Function described in cons.c:
*   data_to_read_0() - return 1 if there is data to read, or 0 otherwise
*   read_serial_0()  - read and return 1 word from receiver buffer
*/


/**
 * @funcname `getchar2`
 *
 * @description
 * Waits for the next character of the debug console and returns it.
 *
 * @return The character.
 *
 * @globals
 * None.
 *
 * @locals
 * None.
 *
 * @hlr
 * - \hlrid{HLR_CONSOLE2_GETCHAR2_01}: `getchar2()` shall wait for the next
 *   character of the debug console and give it.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_CONS_009}
 *   - \srqref{SRQ_JET_DBG_001}
 *
 * @llr
 * - \llrid{LLR_CONSOLE2_GETCHAR2_01}: `getchar2()` shall call
 *   `data_to_read_1()` until it returns a value other than 0, and then return
 *   the result of `read_serial_1()`.
 *   <br>Traces to:
 *   - \hlrref{HLR_CONSOLE2_GETCHAR2_01}
 *
 * @design
 * 1. While `data_to_read_1()` is equal to `0`:
 *    \llrref{LLR_CONSOLE2_GETCHAR2_01}
 * 2. Call `read_serial_1()`, and return its result.
 *    \llrref{LLR_CONSOLE2_GETCHAR2_01}
 *
 * @calls
 * - `data_to_read_1()`
 * - `read_serial_1()`
 */
int32_t
getchar2(void)
{
    while (data_to_read_1() == 0){
    }
    return read_serial_1();
}

