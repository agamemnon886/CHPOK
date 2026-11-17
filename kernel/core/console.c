/*
 * Modified by Kirill Brazhnikov, 2026-10-31: built-in integer types (int,
 * unsigned, short, long, ...) are replaced with the fixed-width types
 * from <stdint.h>.
 *
 * Modified by Kirill Brazhnikov, 2026-11-02: MISRA C:2025: braces around
 * all if/else/loop bodies; essentially Boolean conditions; U suffixes for
 * unsigned constants; discarded results are cast to void; getchar() no
 * longer has i386 code.
 *
 * Modified by Kirill Brazhnikov, 2026-11-03: MISRA C:2025: increments and
 * assignments are separate statements.
 *
 * Modified by Kirill Brazhnikov, 2026-11-08: Doxygen headers for the file
 * and its functions.
 *
 * Modified by Kirill Brazhnikov, 2026-11-13: the macro and the type are moved
 * to kernel/include/cons.h; struct cons_buffer is the typedef cons_buffer_t.
 *
 * Modified by Kirill Brazhnikov, 2026-11-14: The function headers no longer
 * have a step for the declaration of a local variable; the @locals table
 * describes it, and an initialized one is set in a step.
 *
 * Modified by Kirill Brazhnikov, 2026-11-15: moved from kernel/libc/console.c
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
 * getchar() of the kernel: characters of the main console, through a ring
 * buffer.
 */
#include <libc.h>
#include <cons.h>


/*
*   Function described in cons.c:
*   data_to_read_0() - return 1 if there is data to read, or 0 otherwise
*   read_serial_0()  - read and return 1 word from receiver buffer
*/


/**
 * @funcname `serial_proc_data`
 *
 * @description
 * Reads one character of the main console, if there is one.
 *
 * @return The character, or -1 if there is none.
 *
 * @globals
 * None.
 *
 * @locals
 * None.
 *
 * @hlr
 * - \hlrid{HLR_CONSOLE_SERIAL_PROC_DATA_01}: `serial_proc_data()` shall give
 *   one character of the main console, or `-1` if none has arrived.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_CONS_004}
 *
 * @llr
 * - \llrid{LLR_CONSOLE_SERIAL_PROC_DATA_01}: If `data_to_read_0()` returns 0,
 *   `serial_proc_data()` shall return -1 without calling `read_serial_0()`.
 *   <br>Traces to:
 *   - \hlrref{HLR_CONSOLE_SERIAL_PROC_DATA_01}
 * - \llrid{LLR_CONSOLE_SERIAL_PROC_DATA_02}: If `data_to_read_0()` returns a
 *   value other than 0, `serial_proc_data()` shall return the result of
 *   `read_serial_0()`.
 *   <br>Traces to:
 *   - \hlrref{HLR_CONSOLE_SERIAL_PROC_DATA_01}
 *
 * @design
 * 1. If `data_to_read_0()` is equal to `0`:
 *    \llrref{LLR_CONSOLE_SERIAL_PROC_DATA_01}
 *    1. Return `-1`.
 * 2. Call `read_serial_0()`, and return its result.
 *    \llrref{LLR_CONSOLE_SERIAL_PROC_DATA_02}
 *
 * @calls
 * - `data_to_read_0()`
 * - `read_serial_0()`
 */
static int32_t
serial_proc_data(void) //Read data from DUART (1 word)
{
    if (data_to_read_0() == 0) {
        return -1;
    }
    return read_serial_0();
}


static cons_buffer_t cons;

/**
 * @funcname `cons_intr`
 *
 * @description
 * Moves the characters waiting on the main console into the ring buffer (NUL
 * characters are dropped).
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `cons` | `cons_buffer_t` | The ring buffer of input characters |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `c` | `int32_t` | The character read |
 *
 * @hlr
 * - \hlrid{HLR_CONSOLE_CONS_INTR_01}: `cons_intr()` shall move the characters
 *   that have arrived on the main console, except NUL characters, into the
 *   input ring buffer.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_CONS_004}
 *
 * @llr
 * - \llrid{LLR_CONSOLE_CONS_INTR_01}: `cons_intr()` shall call
 *   `serial_proc_data()` until it returns -1.
 *   <br>Traces to:
 *   - \hlrref{HLR_CONSOLE_CONS_INTR_01}
 * - \llrid{LLR_CONSOLE_CONS_INTR_02}: For each result that is neither -1 nor 0,
 *   `cons_intr()` shall store it at `cons.buf[cons.wpos]` and increment
 *   `cons.wpos`.
 *   <br>Traces to:
 *   - \hlrref{HLR_CONSOLE_CONS_INTR_01}
 * - \llrid{LLR_CONSOLE_CONS_INTR_03}: If and only if `cons.wpos` is then
 *   `CONSBUFSIZE`, `cons_intr()` shall set it to 0.
 *   <br>Traces to:
 *   - \hlrref{HLR_CONSOLE_CONS_INTR_01}
 * - \llrid{LLR_CONSOLE_CONS_INTR_04}: For each result 0, `cons_intr()` shall
 *   leave `cons.buf` and `cons.wpos` as they are.
 *   <br>Traces to:
 *   - \hlrref{HLR_CONSOLE_CONS_INTR_01}
 *
 * @design
 * 1. Call `serial_proc_data()`, and store the result in `c`; then while `c` is
 *    not equal to `-1`, repeat (after each pass: call `serial_proc_data()`, and
 *    store the result in `c`): \llrref{LLR_CONSOLE_CONS_INTR_01},
 *    \llrref{LLR_CONSOLE_CONS_INTR_02}, \llrref{LLR_CONSOLE_CONS_INTR_04}
 *    1. If `c` is equal to `0`: \llrref{LLR_CONSOLE_CONS_INTR_04}
 *       1. Go on with the next pass of the loop.
 *    2. Set `cons.buf[cons.wpos]` to `c`. \llrref{LLR_CONSOLE_CONS_INTR_02}
 *    3. Increment `cons.wpos`. \llrref{LLR_CONSOLE_CONS_INTR_02}
 *    4. If `cons.wpos` is equal to `CONSBUFSIZE`:
 *       \llrref{LLR_CONSOLE_CONS_INTR_03}
 *       1. Set `cons.wpos` to `0`.
 *
 * @calls
 * - `serial_proc_data()`
 */
static void
cons_intr(void) //read all elements from DUART
{
    int32_t c;

    for (c = serial_proc_data(); c != -1; c = serial_proc_data()) {
        if (c == 0) {
            continue;
        }
        cons.buf[cons.wpos] = c;
        cons.wpos++;
        if (cons.wpos == CONSBUFSIZE) {
            cons.wpos = 0;
        }
    }
}

/**
 * @funcname `cons_getc`
 *
 * @description
 * Returns the next input character of the main console, from the ring buffer
 * after filling it.
 *
 * @return The character, or 0 if none is waiting.
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `cons` | `cons_buffer_t` | The ring buffer of input characters |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `c` | `int32_t` | The character |
 *
 * @hlr
 * - \hlrid{HLR_CONSOLE_CONS_GETC_01}: `cons_getc()` shall give the next input
 *   character of the main console, or 0 if none is waiting.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_CONS_004}
 *
 * @llr
 * - \llrid{LLR_CONSOLE_CONS_GETC_01}: `cons_getc()` shall first call
 *   `cons_intr()` once.
 *   <br>Traces to:
 *   - \hlrref{HLR_CONSOLE_CONS_GETC_01}
 * - \llrid{LLR_CONSOLE_CONS_GETC_02}: If `cons.rpos` is not `cons.wpos`,
 *   `cons_getc()` shall return `cons.buf[cons.rpos]` and increment `cons.rpos`.
 *   <br>Traces to:
 *   - \hlrref{HLR_CONSOLE_CONS_GETC_01}
 * - \llrid{LLR_CONSOLE_CONS_GETC_03}: If and only if `cons.rpos` is then
 *   `CONSBUFSIZE`, `cons_getc()` shall set it to 0.
 *   <br>Traces to:
 *   - \hlrref{HLR_CONSOLE_CONS_GETC_01}
 * - \llrid{LLR_CONSOLE_CONS_GETC_04}: If `cons.rpos` is `cons.wpos`,
 *   `cons_getc()` shall return 0 and leave `cons.rpos` as it is.
 *   <br>Traces to:
 *   - \hlrref{HLR_CONSOLE_CONS_GETC_01}
 *
 * @design
 * 1. Call `cons_intr()`. \llrref{LLR_CONSOLE_CONS_GETC_01}
 * 2. If `cons.rpos` is not equal to `cons.wpos`:
 *    \llrref{LLR_CONSOLE_CONS_GETC_02}, \llrref{LLR_CONSOLE_CONS_GETC_03}
 *    1. Set `c` to `cons.buf[cons.rpos]`.
 *    2. Increment `cons.rpos`.
 *    3. If `cons.rpos` is equal to `CONSBUFSIZE`:
 *       \llrref{LLR_CONSOLE_CONS_GETC_03}
 *       1. Set `cons.rpos` to `0`.
 *    4. Return `c`.
 * 3. Return `0`. \llrref{LLR_CONSOLE_CONS_GETC_04}
 *
 * @calls
 * - `cons_intr()`
 */
static int32_t
cons_getc(void) //Get 1 element from cons
{
    int32_t c;

    cons_intr();

    // grab the next character from the input buffer.
    if (cons.rpos != cons.wpos) {
        c = cons.buf[cons.rpos];
        cons.rpos++;
        if (cons.rpos == CONSBUFSIZE) {
            cons.rpos = 0;
        }
        return c;
    }
    return 0;
}


/**
 * @funcname `getchar`
 *
 * @description
 * Waits for the next character of the main console and returns it.
 *
 * @return The character.
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `c` | `int32_t` | The character |
 *
 * @hlr
 * - \hlrid{HLR_CONSOLE_GETCHAR_01}: `getchar()` shall wait for the next
 *   character of the main console and give it.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_CONS_009}
 *   - \srqref{SRQ_JET_DBG_009}
 *
 * @llr
 * - \llrid{LLR_CONSOLE_GETCHAR_01}: `getchar()` shall call `cons_getc()` until
 *   it returns a value other than 0, and return that value.
 *   <br>Traces to:
 *   - \hlrref{HLR_CONSOLE_GETCHAR_01}
 *
 * @design
 * 1. Repeat the following while `c` is equal to `0` (at least once):
 *    \llrref{LLR_CONSOLE_GETCHAR_01}
 *    1. Call `cons_getc()`, and store the result in `c`.
 * 2. Return `c`. \llrref{LLR_CONSOLE_GETCHAR_01}
 *
 * @calls
 * - `cons_getc()`
 */
int
getchar(void)
{
    int32_t c;

    do {
        c = cons_getc();
    } while (c == 0);
    return c;
}

