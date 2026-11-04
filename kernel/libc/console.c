/* Modified by Kirill Brazhnikov, 2026-10-31: built-in integer types (int,
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
 */
#include <libc.h>
#include <cons.h>


/*
*   Function described in cons.c:
*   data_to_read_0() - return 1 if there is data to read, or 0 otherwise
*   read_serial_0()  - read and return 1 word from receiver buffer
*/


static int32_t
serial_proc_data(void) //Read data from DUART (1 word)
{
    if (data_to_read_0() == 0) {
        return -1;
    }
    return read_serial_0();
}


#define CONSBUFSIZE 512U

static struct cons_buffer {
    uint8_t buf[CONSBUFSIZE];
    uint32_t rpos;
    uint32_t wpos;
} cons;

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

// return the next input character from the console, or 0 if none waiting
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


int
getchar(void)
{
    int32_t c;

    do {
        c = cons_getc();
    } while (c == 0);
    return c;
}

