/*
 *                               POK header
 *
 * The following file is a part of the POK project. Any modification should
 * made according to the POK licence. You CANNOT use this file or a part of
 * this file is this part of a file for your own project
 *
 * For more information on the POK licence, please see our LICENCE FILE
 *
 * Please follow the coding guidelines described in doc/CODING_GUIDELINES
 *
 *                                      Copyright (c) 2007-2009 POK team
 *
 * Created by julien on Thu Jan 15 23:34:13 2009
 *
 * Copyright (C) 2026 Kirill Brazhnikov <agamemnon886@gmail.com>
 * Modified by Kirill Brazhnikov, 2026-10-31: ported from
 * boards/x86-qemu/cons.c to x86_64 (64-bit).
 * Modified by Kirill Brazhnikov, 2026-10-31: built-in integer types (int,
 * unsigned, short, long, ...) are replaced with the fixed-width types
 * from <stdint.h>.
 * Modified by Kirill Brazhnikov, 2026-11-01: scrolling the VGA screen
 * uses memmove(): the source and destination rows overlap, which is
 * undefined behaviour for memcpy() (MISRA C:2025 Rule 19.1).
 * Modified by Kirill Brazhnikov, 2026-11-02: MISRA C:2025: braces around
 * all if/else/loop bodies; essentially Boolean conditions; U suffixes for
 * unsigned constants; discarded results are cast to void; internal
 * linkage for file-local definitions; the screen scroll size is computed
 * as size_t; deviations are documented where a guideline can't be
 * followed.
 * Modified by Kirill Brazhnikov, 2026-11-03: MISRA C:2025: the cursor
 * position is an int32_t (it was a char used in arithmetic);
 * is_transmit_empty() and write_serial() have internal linkage, and
 * is_transmit_empty() returns a pok_bool_t; increments and assignments
 * are separate statements; tab_size is defined in the function that uses
 * it; iostream_write_common() doesn't modify its parameter; operands of
 * mixed precedence are parenthesized; deviations are documented where a
 * guideline can't be followed.
 * Modified by Kirill Brazhnikov, 2026-11-04: the debug stream (second
 * serial port, used by the GDB server) writes bytes unchanged: no CR
 * before LF and no copy on the VGA screen.
 * Modified by Kirill Brazhnikov, 2026-11-08: Doxygen headers for the
 * file and its functions.
 * Modified by Kirill Brazhnikov, 2026-11-13: the macros and the type are moved
 * to boards/x86_64-qemu/include/board/bsp.h; struct s_cons is the typedef
 * s_cons_t; jet_iostream_t is used by its typedef name.
 * Modified by Kirill Brazhnikov, 2026-11-14: The function headers no longer
 * have a step for the declaration of a local variable; the @locals table
 * describes it, and an initialized one is set in a step.
 * Modified by Kirill Brazhnikov, 2026-11-15: moved from
 * boards/x86_64-qemu/cons.c (the board support packages are in kernel/bsps);
 * the function headers follow outb() and inb(), which call x86_outb() and
 * x86_inb(); the port numbers are converted to uint16_t at these calls.
 * Modified by Kirill Brazhnikov, 2026-11-16: high and low level requirements of
 * the functions, traced to the system requirements (docs/SRQs), and the steps
 * of their design linked to the LLRs.
 * Modified by Kirill Brazhnikov, 2026-11-17: requirements with more than one
 * "shall" are split into requirements of one "shall" each.
 * The modifications are distributed under the POK licence
 * (BSD-2-Clause, see POK-LICENSE). Long lines are wrapped at 80 columns.
 */

/**
 * @file
 * The consoles of the x86_64 QEMU board: the main stream on the first serial
 * port (COM1, 0x3f8) with a copy on the VGA text screen, and the debug stream
 * of the GDB server on the second serial port (COM2, 0x2f8).
 */

#include <config.h>

#include <errno.h>

#include <ioports.h>
#include <libc.h>
#include <core/debug.h>
#include <asp/cons.h>

#include "cons.h"
#include <bsp/bsp.h>

/*
 * MISRA C:2025 Rule 18.4 deviation (this file): characters are written into
 * the VGA text buffer at its address plus their offset.
 * MISRA C:2025 Dir 1.2 deviation (this file): inb() is a GNU statement
 * expression, see <arch/ioports.h>.
 */

#ifdef POK_NEEDS_CONSOLE

static const int32_t screen_w = 80;
static const int32_t screen_h = 25;
/* MISRA C:2025 Rule 11.4 deviation: the VGA text buffer is at this physical
 * address. */
static char *const   vga_base = (char *)0xb8000;
static s_cons_t g_cons;


/**
 * @funcname `is_transmit_empty`
 *
 * @description
 * Tells whether a 16550 serial port can take the next byte.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `port` | `int32_t` | [in] The I/O base address of the serial port. |
 *
 * @return TRUE if the transmit holding register is empty, FALSE otherwise.
 *
 * @globals
 * None.
 *
 * @locals
 * None.
 *
 * @hlr
 * - \hlrid{HLR_CONS_IS_TRANSMIT_EMPTY_01}: `is_transmit_empty()` shall tell
 *   whether a 16550 serial port can take the next byte.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_CONS_005}
 *
 * @llr
 * - \llrid{LLR_CONS_IS_TRANSMIT_EMPTY_01}: If the bit `0x20` is set in the byte
 *   read with `inb()` from `port + 5`, `is_transmit_empty()` shall return true.
 *   <br>Traces to:
 *   - \hlrref{HLR_CONS_IS_TRANSMIT_EMPTY_01}
 * - \llrid{LLR_CONS_IS_TRANSMIT_EMPTY_02}: If the bit `0x20` is not set in the
 *   byte read with `inb()` from `port + 5`, `is_transmit_empty()` shall return
 *   false.
 *   <br>Traces to:
 *   - \hlrref{HLR_CONS_IS_TRANSMIT_EMPTY_01}
 *
 * @design
 * 1. Return `(inb((uint16_t)port + 5U) & 0x20U) != 0U`.
 *    \llrref{LLR_CONS_IS_TRANSMIT_EMPTY_01},
 *    \llrref{LLR_CONS_IS_TRANSMIT_EMPTY_02}
 *
 * @calls
 * - `x86_inb()`
 */
static pok_bool_t is_transmit_empty(int32_t port) {
   return (inb((uint16_t)port + 5U) & 0x20U) != 0U;
}

/**
 * @funcname `write_serial`
 *
 * @description
 * Writes one byte to a serial port, waiting until the port can take it.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `port` | `int32_t` | [in] The I/O base address of the serial port. |
 * | `a` | `char` | [in] The byte to write. |
 *
 * @globals
 * None.
 *
 * @locals
 * None.
 *
 * @hlr
 * - \hlrid{HLR_CONS_WRITE_SERIAL_01}: `write_serial()` shall write a byte to a
 *   serial port, waiting until the port can take it.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_CONS_005}
 *
 * @llr
 * - \llrid{LLR_CONS_WRITE_SERIAL_01}: `write_serial()` shall call
 *   `is_transmit_empty()` with `port` until it returns true, and then write `a`
 *   to `port` once with `outb()`.
 *   <br>Traces to:
 *   - \hlrref{HLR_CONS_WRITE_SERIAL_01}
 *
 * @design
 * 1. While `is_transmit_empty(port)` is false:
 *    \llrref{LLR_CONS_WRITE_SERIAL_01}
 * 2. Use the macro `outb(port, a)`. \llrref{LLR_CONS_WRITE_SERIAL_01}
 *
 * @calls
 * - `is_transmit_empty()`
 * - `x86_outb()`
 */
static void write_serial(int32_t port, char a) {
   while (!is_transmit_empty(port)) {
      ;
   }

   outb(port, a);
}

/**
 * @funcname `data_to_read`
 *
 * @description
 * Tells whether a serial port has a received byte that can be read.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `port` | `int32_t` | [in] The I/O base address of the serial port. |
 *
 * @return 1 if a byte was received without a FIFO error, 0 otherwise.
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `flags` | `int32_t` | The line status register |
 *
 * @hlr
 * - \hlrid{HLR_CONS_DATA_TO_READ_01}: `data_to_read()` shall tell whether a
 *   serial port has a byte that was received without an error.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_CONS_004}
 *
 * @llr
 * - \llrid{LLR_CONS_DATA_TO_READ_01}: If `COM_LSR_DATA` is not set in the byte
 *   read with `inb()` from `port + COM_LSR`, or `COM_LSR_RFE` is set in it,
 *   `data_to_read()` shall return 0.
 *   <br>Traces to:
 *   - \hlrref{HLR_CONS_DATA_TO_READ_01}
 * - \llrid{LLR_CONS_DATA_TO_READ_02}: If `COM_LSR_DATA` is set in the byte read
 *   with `inb()` from `port + COM_LSR` and `COM_LSR_RFE` is not set in it,
 *   `data_to_read()` shall return 1.
 *   <br>Traces to:
 *   - \hlrref{HLR_CONS_DATA_TO_READ_01}
 *
 * @design
 * 1. Set `flags` to `inb((uint16_t)port + COM_LSR)`.
 *    \llrref{LLR_CONS_DATA_TO_READ_01}, \llrref{LLR_CONS_DATA_TO_READ_02}
 * 2. If `(flags & COM_LSR_DATA)` is equal to `0` or `((flags & COM_LSR_RFE))`
 *    is not equal to `0`: \llrref{LLR_CONS_DATA_TO_READ_01}
 *    1. Return `0`.
 * 3. Return `1`. \llrref{LLR_CONS_DATA_TO_READ_02}
 *
 * @calls
 * - `x86_inb()`
 */
static int32_t data_to_read(int32_t port) //return 0 if no data to read
{
    int32_t flags = inb((uint16_t)port + COM_LSR);
	if (((flags & COM_LSR_DATA) == 0) || (((flags & COM_LSR_RFE)) != 0)) {
		return 0;
	}
	return 1;
}

/**
 * @funcname `read_serial`
 *
 * @description
 * Reads one received byte from a serial port, without waiting.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `port` | `int32_t` | [in] The I/O base address of the serial port. |
 *
 * @return The byte (0-255), or -1 if there is none or it was received with
 *         an error.
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `data` | `int32_t` | The byte read from the receive buffer |
 *
 * @hlr
 * - \hlrid{HLR_CONS_READ_SERIAL_01}: `read_serial()` shall read a byte that a
 *   serial port received, without waiting, and give none for a byte received
 *   with an error.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_CONS_004}
 *
 * @llr
 * - \llrid{LLR_CONS_READ_SERIAL_01}: If `data_to_read()` returns 0 for `port`,
 *   `read_serial()` shall return -1 without reading `port`.
 *   <br>Traces to:
 *   - \hlrref{HLR_CONS_READ_SERIAL_01}
 * - \llrid{LLR_CONS_READ_SERIAL_02}: If `data_to_read()` returns a value other
 *   than 0 for `port`, and `COM_LSR_RFE` is not set in the byte read with
 *   `inb()` from `port + COM_LSR` after the byte read from `port`,
 *   `read_serial()` shall return the byte read from `port`.
 *   <br>Traces to:
 *   - \hlrref{HLR_CONS_READ_SERIAL_01}
 * - \llrid{LLR_CONS_READ_SERIAL_03}: If `data_to_read()` returns a value other
 *   than 0 for `port`, and `COM_LSR_RFE` is set in the byte read with `inb()`
 *   from `port + COM_LSR` after the byte read from `port`, `read_serial()`
 *   shall return -1.
 *   <br>Traces to:
 *   - \hlrref{HLR_CONS_READ_SERIAL_01}
 *
 * @design
 * 1. If `data_to_read(port)` is equal to `0`: \llrref{LLR_CONS_READ_SERIAL_01}
 *    1. Return `-1`.
 * 2. Set `data` to `inb((uint16_t)port)`. \llrref{LLR_CONS_READ_SERIAL_02},
 *    \llrref{LLR_CONS_READ_SERIAL_03}
 * 3. If `(inb((uint16_t)port + COM_LSR) & (uint8_t)COM_LSR_RFE)` is equal to
 *    `0U`: \llrref{LLR_CONS_READ_SERIAL_02}
 *    1. Return `data`.
 * 4. Return `-1`. \llrref{LLR_CONS_READ_SERIAL_03}
 *
 * @calls
 * - `data_to_read()`
 * - `x86_inb()`
 */
static int32_t read_serial(int32_t port)
{
	if(data_to_read(port) == 0) {
	   return -1;
	}

   int32_t data;
	data=inb((uint16_t)port); /* COM_RX: register 0 */
	if ( (inb((uint16_t)port + COM_LSR) & (uint8_t)COM_LSR_RFE) == 0U ) {
		return data;
	}
	return -1;
}

/**
 * @funcname `iostream_read_common`
 *
 * @description
 * Reads the bytes that a serial port has received, up to length, without
 * waiting for more.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `port` | `int32_t` | [in] The I/O base address of the serial port. |
 * | `s` | `char *` | [out] Where the bytes are stored. |
 * | `length` | `size_t` | [in] The maximum number of bytes to read. |
 *
 * @return The number of bytes read.
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `i` | `size_t` | The number of bytes read so far |
 * | `data` | `int32_t` | The byte read, or -1 if there is none |
 *
 * @hlr
 * - \hlrid{HLR_CONS_IOSTREAM_READ_COMMON_01}: `iostream_read_common()` shall
 *   read the bytes that a serial port has received, up to a length, without
 *   waiting for more.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_CONS_004}
 *
 * @llr
 * - \llrid{LLR_CONS_IOSTREAM_READ_COMMON_01}: `iostream_read_common()` shall
 *   call `read_serial()` with `port` and store each result other than -1,
 *   converted to `char`, at the next index of `s` from 0 on, until `length`
 *   results are stored, and return the number of results stored.
 *   <br>Traces to:
 *   - \hlrref{HLR_CONS_IOSTREAM_READ_COMMON_01}
 * - \llrid{LLR_CONS_IOSTREAM_READ_COMMON_02}: If `read_serial()` returns -1,
 *   `iostream_read_common()` shall return the number of results stored before,
 *   without calling `read_serial()` again.
 *   <br>Traces to:
 *   - \hlrref{HLR_CONS_IOSTREAM_READ_COMMON_01}
 *
 * @design
 * 1. Set `i` to `0`; then while `i` is not equal to `length`, repeat (after
 *    each pass: increment `i`): \llrref{LLR_CONS_IOSTREAM_READ_COMMON_01}
 *    1. Call `read_serial()` with the argument `port`, and store the result in
 *       `data`.
 *    2. If `data` is equal to `-1`: \llrref{LLR_CONS_IOSTREAM_READ_COMMON_02}
 *       1. Leave the loop.
 *    3. Set `s[i]` to `(char)data`.
 * 2. Return `i`. \llrref{LLR_CONS_IOSTREAM_READ_COMMON_01},
 *    \llrref{LLR_CONS_IOSTREAM_READ_COMMON_02}
 *
 * @calls
 * - `read_serial()`
 */
static size_t iostream_read_common(int32_t port, char* s, size_t length)
{
   size_t i;

   for(i = 0; i != length; i++)
   {
      int32_t data = read_serial(port);
      if(data == -1) {
         break;
      }

      s[i] = (char)data;
   }

   return i;
}

/**
 * @funcname `pok_cons_clear`
 *
 * @description
 * Clears the VGA text screen with the current attribute and moves the cursor
 * to the top left corner.
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `g_cons` | `s_cons_t` | The cursor position and character attribute |
 * | `screen_h` | `const int32_t` | The number of rows of the screen |
 * | `screen_w` | `const int32_t` | The number of columns of the screen |
 * | `vga_base` | `char *const` | The address of the VGA text buffer |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `i` | `int32_t` | The row being cleared |
 * | `j` | `int32_t` | The column being cleared |
 * | `ptr` | `char *` | The next byte of the text buffer |
 * | `local_curs` | `static s_cons_t` | Copy of the cursor being updated |
 *
 * @hlr
 * - \hlrid{HLR_CONS_POK_CONS_CLEAR_01}: `pok_cons_clear()` shall clear the VGA
 *   text screen with the current attribute and put the cursor at the top left
 *   corner.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_BSP_002}
 *
 * @llr
 * - \llrid{LLR_CONS_POK_CONS_CLEAR_01}: `pok_cons_clear()` shall store 0 and
 *   then `g_cons.cur_attr` in the two bytes of each of the `screen_h` times
 *   `screen_w` cells from `vga_base` on, and set `row` and `col` of `g_cons` to
 *   0, leaving its `cur_attr` as it is.
 *   <br>Traces to:
 *   - \hlrref{HLR_CONS_POK_CONS_CLEAR_01}
 *
 * @design
 * 1. Set `ptr` to `vga_base`. \llrref{LLR_CONS_POK_CONS_CLEAR_01}
 * 2. Set `local_curs` to `g_cons`. \llrref{LLR_CONS_POK_CONS_CLEAR_01}
 * 3. Set `i` to `0`; then while `i` is less than `screen_h`, repeat (after each
 *    pass: increment `i`): \llrref{LLR_CONS_POK_CONS_CLEAR_01}
 *    1. Set `j` to `0`; then while `j` is less than `screen_w`, repeat (after
 *       each pass: increment `j`):
 *       1. Set `*ptr` to `0`.
 *       2. Increment `ptr`.
 *       3. Set `*ptr` to `g_cons.cur_attr`.
 *       4. Increment `ptr`.
 * 4. Set `local_curs.row` to `0`. \llrref{LLR_CONS_POK_CONS_CLEAR_01}
 * 5. Set `local_curs.col` to `0`. \llrref{LLR_CONS_POK_CONS_CLEAR_01}
 * 6. Set `g_cons` to `local_curs`. \llrref{LLR_CONS_POK_CONS_CLEAR_01}
 *
 * @calls
 * None.
 */
static void pok_cons_clear (void)
{
   int32_t i;
   int32_t j;
   char  *ptr;
   /* local copy of the current cursor position */
   static s_cons_t		local_curs;

   ptr = vga_base;
   local_curs = g_cons;

   for (i = 0 ; i < screen_h ; ++i)
   {
      for (j = 0 ; j < screen_w ; ++j)
      {
         *ptr = 0;
         ++ptr;
         *ptr = g_cons.cur_attr;
         ++ptr;
      }
   }

   local_curs.row = 0;
   local_curs.col = 0;

   g_cons = local_curs; /* reset the global cursor to the new position */
}

/**
 * @funcname `pok_write_vga`
 *
 * @description
 * Writes one character at the cursor of the VGA text screen and moves the
 * cursor; carriage return, line feed and tab move it only. When the cursor
 * goes below the last row, the screen scrolls up by one row.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `c` | `const char` | [in] The character to write. |
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `g_cons` | `s_cons_t` | The cursor position and character attribute |
 * | `screen_h` | `const int32_t` | The number of rows of the screen |
 * | `screen_w` | `const int32_t` | The number of columns of the screen |
 * | `vga_base` | `char *const` | The address of the VGA text buffer |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `tab_size` | `static const int32_t` | The distance between tab stops |
 * | `ptr` | `char *` | The cell of the text buffer at the cursor |
 * | `local_curs` | `static s_cons_t` | Copy of the cursor being updated |
 * | `i` | `int32_t` | The column of the last row being cleared |
 * | `row_bytes` | `const size_t` | The size of one row of the text buffer |
 *
 * @hlr
 * - \hlrid{HLR_CONS_POK_WRITE_VGA_01}: `pok_write_vga()` shall write a
 *   character at the cursor of the VGA text screen and move the cursor,
 *   scrolling the screen up by a line below the last row.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_BSP_002}
 *   - \srqref{SRQ_JET_CONS_003}
 * - \hlrid{HLR_CONS_POK_WRITE_VGA_02}: `pok_write_vga()` shall move the cursor
 *   for a carriage return, a line feed and a tab, without writing them to the
 *   screen.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_BSP_002}
 *   - \srqref{SRQ_JET_CONS_003}
 *
 * @llr
 * - \llrid{LLR_CONS_POK_WRITE_VGA_01}: If `c` is `'\r'`, `pok_write_vga()`
 *   shall set the column of the cursor (`col` of `g_cons`) to 0.
 *   <br>Traces to:
 *   - \hlrref{HLR_CONS_POK_WRITE_VGA_02}
 * - \llrid{LLR_CONS_POK_WRITE_VGA_02}: If `c` is `'\n'`, `pok_write_vga()`
 *   shall set the column of the cursor to 0 and increment its row (`row` of
 *   `g_cons`).
 *   <br>Traces to:
 *   - \hlrref{HLR_CONS_POK_WRITE_VGA_02}
 * - \llrid{LLR_CONS_POK_WRITE_VGA_03}: If `c` is `'\t'`, `pok_write_vga()`
 *   shall add to the column of the cursor 8 minus the column modulo 8.
 *   <br>Traces to:
 *   - \hlrref{HLR_CONS_POK_WRITE_VGA_02}
 * - \llrid{LLR_CONS_POK_WRITE_VGA_04}: If `c` is none of `'\r'`, `'\n'` and
 *   `'\t'`, `pok_write_vga()` shall store `c` and then `cur_attr` of `g_cons`
 *   in the two bytes of the cell of the cursor (at `vga_base` plus 2 times
 *   `screen_w` times the row, plus 2 times the column), and increment the
 *   column.
 *   <br>Traces to:
 *   - \hlrref{HLR_CONS_POK_WRITE_VGA_01}
 * - \llrid{LLR_CONS_POK_WRITE_VGA_05}: If and only if the column is then
 *   `screen_w` or more, `pok_write_vga()` shall set the column to 0 and
 *   increment the row.
 *   <br>Traces to:
 *   - \hlrref{HLR_CONS_POK_WRITE_VGA_01}
 *   - \hlrref{HLR_CONS_POK_WRITE_VGA_02}
 * - \llrid{LLR_CONS_POK_WRITE_VGA_06}: If and only if the row is then
 *   `screen_h` or more, `pok_write_vga()` shall call `memmove()` once to move
 *   the rows 1 to `screen_h` minus 1 of the screen to its start, store 0 and
 *   `cur_attr` in the two bytes of each of the `screen_w` cells of the last
 *   row, and set the row to `screen_h` minus 1.
 *   <br>Traces to:
 *   - \hlrref{HLR_CONS_POK_WRITE_VGA_01}
 *   - \hlrref{HLR_CONS_POK_WRITE_VGA_02}
 *
 * @design
 * 1. Set `local_curs` to `g_cons`. \llrref{LLR_CONS_POK_WRITE_VGA_01},
 *    \llrref{LLR_CONS_POK_WRITE_VGA_02}, \llrref{LLR_CONS_POK_WRITE_VGA_03},
 *    \llrref{LLR_CONS_POK_WRITE_VGA_04}, \llrref{LLR_CONS_POK_WRITE_VGA_05},
 *    \llrref{LLR_CONS_POK_WRITE_VGA_06}
 * 2. If `c` is equal to `'\r'`: \llrref{LLR_CONS_POK_WRITE_VGA_01}
 *    1. Set `local_curs.col` to `0`.
 * 3. If `c` is equal to `'\n'`: \llrref{LLR_CONS_POK_WRITE_VGA_02}
 *    1. Set `local_curs.col` to `0`.
 *    2. Increment `local_curs.row`.
 * 4. If `c` is equal to `'\t'`: \llrref{LLR_CONS_POK_WRITE_VGA_03}
 *    1. Add `tab_size - (local_curs.col % tab_size)` to `local_curs.col`.
 * 5. If `c` is not equal to `'\r'` and `c` is not equal to `'\n'` and `c` is
 *    not equal to `'\t'`: \llrref{LLR_CONS_POK_WRITE_VGA_04}
 *    1. Set `ptr` to
 *       `vga_base + (2 * screen_w * local_curs.row) + (2 * local_curs.col)`.
 *    2. Increment `local_curs.col`.
 *    3. Set `*ptr` to `c`.
 *    4. Increment `ptr`.
 *    5. Set `*ptr` to `local_curs.cur_attr`.
 * 6. If `local_curs.col` is greater than or equal to `screen_w`:
 *    \llrref{LLR_CONS_POK_WRITE_VGA_05}
 *    1. Set `local_curs.col` to `0`.
 *    2. Increment `local_curs.row`.
 * 7. If `local_curs.row` is greater than or equal to `screen_h`:
 *    \llrref{LLR_CONS_POK_WRITE_VGA_06}
 *    1. Set `row_bytes` to `(size_t) screen_w * 2U`.
 *    2. Call `memmove()` with the arguments `vga_base`, `vga_base + row_bytes`
 *       and `row_bytes * ((size_t) screen_h - 1U)`.
 *    3. Set `i` to `0`; then while `i` is less than `screen_w`, repeat (after
 *       each pass: increment `i`):
 *       1. Set `*(vga_base + (2 * screen_w * (screen_h - 1)) + (2 * i))` to
 *          `0`.
 *       2. Set `*(vga_base + (2 * screen_w * (screen_h - 1)) + (2 * i) + 1)`
 *          to `local_curs.cur_attr`.
 *    4. Set `local_curs.row` to `screen_h - 1`.
 * 8. Set `g_cons` to `local_curs`. \llrref{LLR_CONS_POK_WRITE_VGA_01},
 *    \llrref{LLR_CONS_POK_WRITE_VGA_02}, \llrref{LLR_CONS_POK_WRITE_VGA_03},
 *    \llrref{LLR_CONS_POK_WRITE_VGA_04}, \llrref{LLR_CONS_POK_WRITE_VGA_05},
 *    \llrref{LLR_CONS_POK_WRITE_VGA_06}
 *
 * @calls
 * - `memmove()`
 */
static void pok_write_vga (const char c)
{
   static const int32_t tab_size = 8;

   char*                ptr;
   static s_cons_t local_curs; /* Local copy of the curent cursor position */
   int32_t              i;

   local_curs = g_cons;

   if (c == '\r')
   {
      local_curs.col = 0;
   }

   if (c == '\n')
   {
      local_curs.col = 0;
      ++local_curs.row;
   }

   if (c == '\t')
   {
      local_curs.col += tab_size - (local_curs.col % tab_size);
   }

   if ((c != '\r') && (c != '\n') && (c != '\t'))
   {
      ptr = vga_base + (2 * screen_w * local_curs.row) + (2 * local_curs.col);
      local_curs.col++;
      *ptr = c;
      ++ptr;
      *ptr = local_curs.cur_attr;
   }

   if (local_curs.col >= screen_w)
   {
      local_curs.col = 0;
      ++local_curs.row;
   }

   if (local_curs.row >= screen_h)
   {
      // Rows 1..h-1 move up to rows 0..h-2: the areas overlap.
      const size_t row_bytes = (size_t) screen_w * 2U;

      (void) memmove (vga_base, vga_base + row_bytes,
                      row_bytes * ((size_t) screen_h - 1U));
      for (i = 0; i < screen_w; ++i)
      {
         *(vga_base + (2 * screen_w * (screen_h - 1)) + (2 * i)) = 0;
         *(vga_base + (2 * screen_w * (screen_h - 1)) + (2 * i) + 1) =
             local_curs.cur_attr;
      }
      local_curs.row = screen_h - 1;
   }

   // Reset the global cursor to the new position
   g_cons = local_curs;

}

/**
 * @funcname `iostream_write_common`
 *
 * @description
 * Writes bytes to a serial port and to the VGA screen, sending each line
 * feed as a carriage return and a line feed.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `port` | `int32_t` | [in] The I/O base address of the serial port. |
 * | `s` | `const char *` | [in] The bytes to write. |
 * | `length` | `size_t` | [in] The number of bytes. |
 *
 * @return length: every byte is written.
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `c` | `char` | The byte being written |
 * | `rest` | `size_t` | The number of bytes left to write |
 *
 * @hlr
 * - \hlrid{HLR_CONS_IOSTREAM_WRITE_COMMON_01}: `iostream_write_common()` shall
 *   write all the bytes to a serial port, each line feed as a carriage return
 *   and a line feed, and to the VGA screen.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_CONS_003}
 *   - \srqref{SRQ_JET_CONS_005}
 *
 * @llr
 * - \llrid{LLR_CONS_IOSTREAM_WRITE_COMMON_01}: For each of the `length` bytes
 *   of `s`, in their order, that is not `'\n'`, `iostream_write_common()` shall
 *   call `write_serial()` once with `port` and the byte.
 *   <br>Traces to:
 *   - \hlrref{HLR_CONS_IOSTREAM_WRITE_COMMON_01}
 * - \llrid{LLR_CONS_IOSTREAM_WRITE_COMMON_02}: For each of the `length` bytes
 *   of `s`, in their order, that is `'\n'`, `iostream_write_common()` shall
 *   call `write_serial()` with `port` and `'\r'`, and then with `port` and
 *   `'\n'`.
 *   <br>Traces to:
 *   - \hlrref{HLR_CONS_IOSTREAM_WRITE_COMMON_01}
 * - \llrid{LLR_CONS_IOSTREAM_WRITE_COMMON_03}: `iostream_write_common()` shall
 *   call `pok_write_vga()` once with each of the `length` bytes of `s`, after
 *   the calls of `write_serial()` for it, and return `length`.
 *   <br>Traces to:
 *   - \hlrref{HLR_CONS_IOSTREAM_WRITE_COMMON_01}
 *
 * @design
 * 1. Set `rest` to `length`. \llrref{LLR_CONS_IOSTREAM_WRITE_COMMON_01},
 *    \llrref{LLR_CONS_IOSTREAM_WRITE_COMMON_02},
 *    \llrref{LLR_CONS_IOSTREAM_WRITE_COMMON_03}
 * 2. While `rest` is greater than `0U`, repeat (after each pass: decrement
 *    `rest`): \llrref{LLR_CONS_IOSTREAM_WRITE_COMMON_01},
 *    \llrref{LLR_CONS_IOSTREAM_WRITE_COMMON_02},
 *    \llrref{LLR_CONS_IOSTREAM_WRITE_COMMON_03}
 *    1. Set `c` to `s[length - rest]`.
 *    2. If `c` is not equal to `'\n'`:
 *       \llrref{LLR_CONS_IOSTREAM_WRITE_COMMON_01}
 *       1. Call `write_serial()` with the arguments `port` and `c`.
 *    3. Otherwise: \llrref{LLR_CONS_IOSTREAM_WRITE_COMMON_02}
 *       1. Call `write_serial()` with the arguments `port` and `'\r'`.
 *       2. Call `write_serial()` with the arguments `port` and `'\n'`.
 *    4. Call `pok_write_vga()` with the argument `c`.
 *       \llrref{LLR_CONS_IOSTREAM_WRITE_COMMON_03}
 * 3. Return `length`. \llrref{LLR_CONS_IOSTREAM_WRITE_COMMON_03}
 *
 * @calls
 * - `write_serial()`
 * - `pok_write_vga()`
 */
static size_t iostream_write_common (int32_t port, const char *s, size_t length)
{
    char c;
    size_t rest = length;
    for (; rest > 0U; rest--) {
        c = s[length - rest];
        if (c != '\n') {
            write_serial(port, c);
        } else {
            write_serial(port, '\r');
            write_serial(port, '\n');
        }
	pok_write_vga(c);
    }




   return length;
}

/**
 * @funcname `iostream_init_common`
 *
 * @description
 * Initializes the VGA screen and a serial port: 38400 baud, 8 data bits, no
 * parity, one stop bit, FIFOs enabled.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `port` | `int32_t` | [in] The I/O base address of the serial port. |
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `g_cons` | `s_cons_t` | The cursor position and character attribute |
 *
 * @locals
 * None.
 *
 * @hlr
 * - \hlrid{HLR_CONS_IOSTREAM_INIT_COMMON_01}: `iostream_init_common()` shall
 *   clear the VGA screen and set up a serial port at 38400 baud, 8 data bits,
 *   no parity, one stop bit, with its FIFOs and no interrupts.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_BSP_002}
 *   - \srqref{SRQ_JET_BOOT_003}
 *
 * @llr
 * - \llrid{LLR_CONS_IOSTREAM_INIT_COMMON_01}: `iostream_init_common()` shall
 *   call `pok_cons_clear()` once, set `g_cons.cur_attr` to `0x07`, and then
 *   write with `outb()`, in this order, `0x00` to `port + 1`, `0x80` to `port +
 *   3`, `0x03` to `port`, `0x00` to `port + 1`, `0x03` to `port + 3`, `0xC7` to
 *   `port + 2` and `0x0B` to `port + 4`.
 *   <br>Traces to:
 *   - \hlrref{HLR_CONS_IOSTREAM_INIT_COMMON_01}
 *
 * @design
 * 1. Call `pok_cons_clear()`. \llrref{LLR_CONS_IOSTREAM_INIT_COMMON_01}
 * 2. Set `g_cons.cur_attr` to `(char) 0x07U`.
 *    \llrref{LLR_CONS_IOSTREAM_INIT_COMMON_01}
 * 3. Use the macro `outb((uint16_t)port + 1U, 0x00)`.
 *    \llrref{LLR_CONS_IOSTREAM_INIT_COMMON_01}
 * 4. Use the macro `outb((uint16_t)port + 3U, 0x80)`.
 *    \llrref{LLR_CONS_IOSTREAM_INIT_COMMON_01}
 * 5. Use the macro `outb((uint16_t)port, 0x03)`.
 *    \llrref{LLR_CONS_IOSTREAM_INIT_COMMON_01}
 * 6. Use the macro `outb((uint16_t)port + 1U, 0x00)`.
 *    \llrref{LLR_CONS_IOSTREAM_INIT_COMMON_01}
 * 7. Use the macro `outb((uint16_t)port + 3U, 0x03)`.
 *    \llrref{LLR_CONS_IOSTREAM_INIT_COMMON_01}
 * 8. Use the macro `outb((uint16_t)port + 2U, 0xC7)`.
 *    \llrref{LLR_CONS_IOSTREAM_INIT_COMMON_01}
 * 9. Use the macro `outb((uint16_t)port + 4U, 0x0B)`.
 *    \llrref{LLR_CONS_IOSTREAM_INIT_COMMON_01}
 *
 * @calls
 * - `pok_cons_clear()`
 * - `x86_outb()`
 */
static void iostream_init_common (int32_t port)
{
   /* Init VGA screen */
   pok_cons_clear ();
   g_cons.cur_attr = (char) 0x07U; // Light grey on black.
   /* To be fixed : init serial */
   outb((uint16_t)port + 1U, 0x00);    // Disable all interrupts
   outb((uint16_t)port + 3U, 0x80);    // Enable DLAB (set baud rate divisor)
   outb((uint16_t)port, 0x03);    // Set divisor to 3 (lo byte) 38400 baud
   outb((uint16_t)port + 1U, 0x00);    //                  (hi byte)
   outb((uint16_t)port + 3U, 0x03);    // 8 bits, no parity, one stop bit
   // Enable FIFO, clear them, with 14-byte threshold
   outb((uint16_t)port + 2U, 0xC7);
   outb((uint16_t)port + 4U, 0x0B);    // IRQs enabled, RTS/DSR set
}

/**
 * @funcname `iostream_read_main`
 *
 * @description
 * The read operation of the main stream: reads from the first serial port.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `s` | `char *` | [out] Where the bytes are stored. |
 * | `length` | `size_t` | [in] The maximum number of bytes to read. |
 *
 * @return The number of bytes read.
 *
 * @globals
 * None.
 *
 * @locals
 * None.
 *
 * @hlr
 * - \hlrid{HLR_CONS_IOSTREAM_READ_MAIN_01}: `iostream_read_main()` shall read
 *   the main console from the first serial port.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_CONS_004}
 *
 * @llr
 * - \llrid{LLR_CONS_IOSTREAM_READ_MAIN_01}: `iostream_read_main()` shall return
 *   the result of `iostream_read_common()` for `COM0`, `s` and `length`.
 *   <br>Traces to:
 *   - \hlrref{HLR_CONS_IOSTREAM_READ_MAIN_01}
 *
 * @design
 * 1. Call `iostream_read_common()` with the arguments `COM0`, `s` and `length`,
 *    and return its result. \llrref{LLR_CONS_IOSTREAM_READ_MAIN_01}
 *
 * @calls
 * - `iostream_read_common()`
 */
static size_t iostream_read_main(char* s, size_t length)
{
    return iostream_read_common(COM0, s, length);
}
/**
 * @funcname `iostream_write_main`
 *
 * @description
 * The write operation of the main stream: writes to the first serial port
 * and the VGA screen.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `s` | `const char *` | [in] The bytes to write. |
 * | `length` | `size_t` | [in] The number of bytes. |
 *
 * @return length.
 *
 * @globals
 * None.
 *
 * @locals
 * None.
 *
 * @hlr
 * - \hlrid{HLR_CONS_IOSTREAM_WRITE_MAIN_01}: `iostream_write_main()` shall
 *   write the main console to the first serial port and the VGA screen.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_CONS_005}
 *   - \srqref{SRQ_JET_CONS_003}
 *
 * @llr
 * - \llrid{LLR_CONS_IOSTREAM_WRITE_MAIN_01}: `iostream_write_main()` shall
 *   return the result of `iostream_write_common()` for `COM0`, `s` and
 *   `length`.
 *   <br>Traces to:
 *   - \hlrref{HLR_CONS_IOSTREAM_WRITE_MAIN_01}
 *
 * @design
 * 1. Call `iostream_write_common()` with the arguments `COM0`, `s` and
 *    `length`, and return its result. \llrref{LLR_CONS_IOSTREAM_WRITE_MAIN_01}
 *
 * @calls
 * - `iostream_write_common()`
 */
static size_t iostream_write_main(const char* s, size_t length)
{
   return iostream_write_common(COM0, s, length);
}
/**
 * @funcname `iostream_init_main`
 *
 * @description
 * The init operation of the main stream: initializes the screen and the
 * first serial port.
 *
 * @globals
 * None.
 *
 * @locals
 * None.
 *
 * @hlr
 * - \hlrid{HLR_CONS_IOSTREAM_INIT_MAIN_01}: `iostream_init_main()` shall
 *   initialize the screen and the first serial port for the main console.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_BOOT_003}
 *
 * @llr
 * - \llrid{LLR_CONS_IOSTREAM_INIT_MAIN_01}: `iostream_init_main()` shall call
 *   `iostream_init_common()` once with `COM0`.
 *   <br>Traces to:
 *   - \hlrref{HLR_CONS_IOSTREAM_INIT_MAIN_01}
 *
 * @design
 * 1. Call `iostream_init_common()` with the argument `COM0`.
 *    \llrref{LLR_CONS_IOSTREAM_INIT_MAIN_01}
 *
 * @calls
 * - `iostream_init_common()`
 */
static void iostream_init_main(void)
{
   iostream_init_common(COM0);
}

static jet_iostream_t x86_stream_main =
{
    .write = &iostream_write_main,
    .read  = &iostream_read_main,
    .init = &iostream_init_main
};


/**
 * @funcname `iostream_read_debug`
 *
 * @description
 * The read operation of the debug stream (the GDB server): reads from the
 * second serial port.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `s` | `char *` | [out] Where the bytes are stored. |
 * | `length` | `size_t` | [in] The maximum number of bytes to read. |
 *
 * @return The number of bytes read.
 *
 * @globals
 * None.
 *
 * @locals
 * None.
 *
 * @hlr
 * - \hlrid{HLR_CONS_IOSTREAM_READ_DEBUG_01}: `iostream_read_debug()` shall read
 *   the debug console from the second serial port.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_CONS_004}
 *
 * @llr
 * - \llrid{LLR_CONS_IOSTREAM_READ_DEBUG_01}: `iostream_read_debug()` shall
 *   return the result of `iostream_read_common()` for `COM1`, `s` and `length`.
 *   <br>Traces to:
 *   - \hlrref{HLR_CONS_IOSTREAM_READ_DEBUG_01}
 *
 * @design
 * 1. Call `iostream_read_common()` with the arguments `COM1`, `s` and `length`,
 *    and return its result. \llrref{LLR_CONS_IOSTREAM_READ_DEBUG_01}
 *
 * @calls
 * - `iostream_read_common()`
 */
static size_t iostream_read_debug(char* s, size_t length)
{
    return iostream_read_common(COM1, s, length);
}
/**
 * @funcname `iostream_write_debug`
 *
 * @description
 * The write operation of the debug stream (the GDB server): writes the bytes
 * to the second serial port unchanged, because the GDB protocol needs them
 * so, and not on the screen.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `s` | `const char *` | [in] The bytes to write. |
 * | `length` | `size_t` | [in] The number of bytes. |
 *
 * @return length.
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `i` | `size_t` | The index of the byte being written |
 *
 * @hlr
 * - \hlrid{HLR_CONS_IOSTREAM_WRITE_DEBUG_01}: `iostream_write_debug()` shall
 *   write the debug console to the second serial port unchanged, and not to the
 *   screen.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_CONS_007}
 *
 * @llr
 * - \llrid{LLR_CONS_IOSTREAM_WRITE_DEBUG_01}: `iostream_write_debug()` shall
 *   call `write_serial()` with `COM1` and each of the `length` bytes of `s`, in
 *   their order, and return `length`.
 *   <br>Traces to:
 *   - \hlrref{HLR_CONS_IOSTREAM_WRITE_DEBUG_01}
 *
 * @design
 * 1. Set `i` to `0U`; then while `i` is less than `length`, repeat (after each
 *    pass: increment `i`): \llrref{LLR_CONS_IOSTREAM_WRITE_DEBUG_01}
 *    1. Call `write_serial()` with the arguments `COM1` and `s[i]`.
 * 2. Return `length`. \llrref{LLR_CONS_IOSTREAM_WRITE_DEBUG_01}
 *
 * @calls
 * - `write_serial()`
 */
static size_t iostream_write_debug(const char* s, size_t length)
{
   for (size_t i = 0U; i < length; i++) {
      write_serial(COM1, s[i]);
   }

   return length;
}
/**
 * @funcname `iostream_init_debug`
 *
 * @description
 * The init operation of the debug stream: initializes the screen and the
 * second serial port.
 *
 * @globals
 * None.
 *
 * @locals
 * None.
 *
 * @hlr
 * - \hlrid{HLR_CONS_IOSTREAM_INIT_DEBUG_01}: `iostream_init_debug()` shall
 *   initialize the screen and the second serial port for the debug console.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_BOOT_003}
 *
 * @llr
 * - \llrid{LLR_CONS_IOSTREAM_INIT_DEBUG_01}: `iostream_init_debug()` shall call
 *   `iostream_init_common()` once with `COM1`.
 *   <br>Traces to:
 *   - \hlrref{HLR_CONS_IOSTREAM_INIT_DEBUG_01}
 *
 * @design
 * 1. Call `iostream_init_common()` with the argument `COM1`.
 *    \llrref{LLR_CONS_IOSTREAM_INIT_DEBUG_01}
 *
 * @calls
 * - `iostream_init_common()`
 */
static void iostream_init_debug(void)
{
   iostream_init_common(COM1);
}

static jet_iostream_t x86_stream_debug =
{
    .write = &iostream_write_debug,
    .read  = &iostream_read_debug,
    .init = &iostream_init_debug
};

jet_iostream_t* ja_stream_default_read = &x86_stream_main;
jet_iostream_t* ja_stream_default_write = &x86_stream_main;
jet_iostream_t* ja_stream_default_read_debug = &x86_stream_debug;
jet_iostream_t* ja_stream_default_write_debug = &x86_stream_debug;

#else

jet_iostream_t* ja_stream_default_read = NULL;
jet_iostream_t* ja_stream_default_write = NULL;
jet_iostream_t* ja_stream_default_read_debug = NULL;
jet_iostream_t* ja_stream_default_write_debug = NULL;

#endif
