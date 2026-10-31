/*
 * Copyright (C) 2026 Kirill Brazhnikov <agamemnon886@gmail.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/* Console on the PL011 UART. */

#include <config.h>
#include <libc.h>
#include <asp/cons.h>
#include <arch/ioports.h>
#include <bsp/bsp.h>

#ifdef POK_NEEDS_CONSOLE

#define UART_DR 0x00
#define UART_FR 0x18
#define   UART_FR_RXFE (1 << 4)
#define   UART_FR_TXFF (1 << 5)

static void uart_putc(char c)
{
    while (ja_mmio_read32(BOARD_UART_BASE + UART_FR) & UART_FR_TXFF);
    ja_mmio_write32(BOARD_UART_BASE + UART_DR, (unsigned char)c);
}

static size_t iostream_write(const char* s, size_t length)
{
    for (size_t i = 0; i < length; i++)
    {
        // Same as on x86: serial terminals need "\r\n".
        if (s[i] == '\n')
            uart_putc('\r');
        uart_putc(s[i]);
    }
    return length;
}

static size_t iostream_read(char* s, size_t length)
{
    size_t i;
    for (i = 0; i != length; i++)
    {
        if (ja_mmio_read32(BOARD_UART_BASE + UART_FR) & UART_FR_RXFE)
            break;
        s[i] = (char)ja_mmio_read32(BOARD_UART_BASE + UART_DR);
    }
    return i;
}

static struct jet_iostream arm64_stream_main =
{
    .write = &iostream_write,
    .read  = &iostream_read,
    .init  = NULL // QEMU's PL011 needs no setup.
};

struct jet_iostream* ja_stream_default_read = &arm64_stream_main;
struct jet_iostream* ja_stream_default_write = &arm64_stream_main;
/* The board has a single UART, which is used for the main console. */
struct jet_iostream* ja_stream_default_read_debug = NULL;
struct jet_iostream* ja_stream_default_write_debug = NULL;

#else

struct jet_iostream* ja_stream_default_read = NULL;
struct jet_iostream* ja_stream_default_write = NULL;
struct jet_iostream* ja_stream_default_read_debug = NULL;
struct jet_iostream* ja_stream_default_write_debug = NULL;

#endif /* POK_NEEDS_CONSOLE */
