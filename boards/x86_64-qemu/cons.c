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
 * The modifications are distributed under the POK licence
 * (BSD-2-Clause, see POK-LICENSE). Long lines are wrapped at 80 columns.
 */

#include <config.h>

#include <errno.h>

#include <ioports.h>
#include <libc.h>
#include <core/debug.h>
#include <asp/cons.h>

#include "cons.h"

/*
 * MISRA C:2025 Rule 18.4 deviation (this file): characters are written into
 * the VGA text buffer at its address plus their offset.
 * MISRA C:2025 Dir 1.2 deviation (this file): inb() is a GNU statement
 * expression, see <arch/ioports.h>.
 */

#ifdef POK_NEEDS_CONSOLE

#define  COM0      0x3F8
#define  COM1      0x2f8

struct s_cons
{
  int32_t row;
  int32_t col;
  char cur_attr;
};

static const int32_t screen_w = 80;
static const int32_t screen_h = 25;
/* MISRA C:2025 Rule 11.4 deviation: the VGA text buffer is at this physical
 * address. */
static char *const   vga_base = (char *)0xb8000;
static struct s_cons g_cons;


static pok_bool_t is_transmit_empty(int32_t port) {
   return (inb(port + 5) & 0x20U) != 0U;
}

static void write_serial(int32_t port, char a) {
   while (!is_transmit_empty(port)) {
      ;
   }

   outb(port, a);
}

#define COM_LSR		5	// In:	Line Status Register
#define COM_RX		0	// In:	Receive buffer (DLAB=0)
#define   COM_LSR_DATA	0x01	//   Data available
#define   COM_LSR_RFE	0x80	//   Error in Received FIFO

static int32_t data_to_read(int32_t port) //return 0 if no data to read
{
    int32_t flags = inb(port + COM_LSR);
	if (((flags & COM_LSR_DATA) == 0) || (((flags & COM_LSR_RFE)) != 0)) {
		return 0;
	}
	return 1;
}

static int32_t read_serial(int32_t port)
{
	if(data_to_read(port) == 0) {
	   return -1;
	}

   int32_t data;
	data=inb(port+COM_RX);
	if ( (inb(port+COM_LSR) & COM_LSR_RFE) == 0 ) {
		return data;
	}
	return -1;
}

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

static void pok_cons_clear (void)
{
   int32_t i;
   int32_t j;
   char  *ptr;
   static struct s_cons		local_curs; /* local copy of the current cursor position */

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

static void pok_write_vga (const char c)
{
   static const int32_t tab_size = 8;

   char*                ptr;
   static struct s_cons local_curs; /* Local copy of the curent cursor position */
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

static void iostream_init_common (int32_t port)
{
   /* Init VGA screen */
   pok_cons_clear ();
   g_cons.cur_attr = (char) 0x07U; // Light grey on black.
   /* To be fixed : init serial */
   outb(port + 1, 0x00);    // Disable all interrupts
   outb(port + 3, 0x80);    // Enable DLAB (set baud rate divisor)
   outb(port + 0, 0x03);    // Set divisor to 3 (lo byte) 38400 baud
   outb(port + 1, 0x00);    //                  (hi byte)
   outb(port + 3, 0x03);    // 8 bits, no parity, one stop bit
   outb(port + 2, 0xC7);    // Enable FIFO, clear them, with 14-byte threshold
   outb(port + 4, 0x0B);    // IRQs enabled, RTS/DSR set
}

static size_t iostream_read_main(char* s, size_t length)
{
    return iostream_read_common(COM0, s, length);
}
static size_t iostream_write_main(const char* s, size_t length)
{
   return iostream_write_common(COM0, s, length);
}
static void iostream_init_main(void)
{
   iostream_init_common(COM0);
}

static struct jet_iostream x86_stream_main =
{
    .write = &iostream_write_main,
    .read  = &iostream_read_main,
    .init = &iostream_init_main
};


static size_t iostream_read_debug(char* s, size_t length)
{
    return iostream_read_common(COM1, s, length);
}
static size_t iostream_write_debug(const char* s, size_t length)
{
   return iostream_write_common(COM1, s, length);
}
static void iostream_init_debug(void)
{
   iostream_init_common(COM1);
}

static struct jet_iostream x86_stream_debug =
{
    .write = &iostream_write_debug,
    .read  = &iostream_read_debug,
    .init = &iostream_init_debug
};

struct jet_iostream* ja_stream_default_read = &x86_stream_main;
struct jet_iostream* ja_stream_default_write = &x86_stream_main;
struct jet_iostream* ja_stream_default_read_debug = &x86_stream_debug;
struct jet_iostream* ja_stream_default_write_debug = &x86_stream_debug;

#else

struct jet_iostream* ja_stream_default_read = NULL;
struct jet_iostream* ja_stream_default_write = NULL;
struct jet_iostream* ja_stream_default_read_debug = NULL;
struct jet_iostream* ja_stream_default_write_debug = NULL;

#endif
