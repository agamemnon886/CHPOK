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
 *
 * Modified by Kirill Brazhnikov, 2026-11-02: MISRA C:2025: braces around
 * all if/else/loop bodies; essentially Boolean conditions; U suffixes for
 * unsigned constants.
 *
 * Modified by Kirill Brazhnikov, 2026-11-05: the debug stream (the second
 * UART, used by the GDB server) writes bytes unchanged and is used only if
 * QEMU has created the UART.
 */

/*
 * Console on the PL011 UART, and the debug stream on the second one.
 *
 * QEMU creates the second UART only when it is run with two -serial
 * options; accessing a UART which is not there would be a fatal bus error,
 * so it is looked for in the device tree that QEMU gives to the kernel.
 */

#include <config.h>
#include <libc.h>
#include <asp/cons.h>
#include <arch/ioports.h>
#include <bsp/bsp.h>

#ifdef POK_NEEDS_CONSOLE

#define UART_DR 0x00U
#define UART_FR 0x18U
#define   UART_FR_RXFE (1U << 4)
#define   UART_FR_TXFF (1U << 5)

static void uart_putc(uintptr_t base, char c)
{
    while ((ja_mmio_read32(base + UART_FR) & UART_FR_TXFF) != 0U) {
        ;
    }
    ja_mmio_write32(base + UART_DR, (uint8_t)c);
}

static size_t uart_read(uintptr_t base, char* s, size_t length)
{
    size_t i;
    for (i = 0; i != length; i++)
    {
        if ((ja_mmio_read32(base + UART_FR) & UART_FR_RXFE) != 0U) {
            break;
        }
        s[i] = (char)ja_mmio_read32(base + UART_DR);
    }
    return i;
}

static size_t iostream_write(const char* s, size_t length)
{
    for (size_t i = 0; i < length; i++)
    {
        // Same as on x86: serial terminals need "\r\n".
        if (s[i] == '\n') {
            uart_putc(BOARD_UART_BASE, '\r');
        }
        uart_putc(BOARD_UART_BASE, s[i]);
    }
    return length;
}

static size_t iostream_read(char* s, size_t length)
{
    return uart_read(BOARD_UART_BASE, s, length);
}

static struct jet_iostream arm64_stream_main =
{
    .write = &iostream_write,
    .read  = &iostream_read,
    .init  = NULL // QEMU's PL011 needs no setup.
};

/* Device tree (flattened): header fields, tokens. */
#define FDT_MAGIC          0xD00DFEEDU
#define FDT_OFF_TOTALSIZE  4U
#define FDT_OFF_STRUCT     8U
#define FDT_SIZE_STRUCT    36U
#define FDT_HEADER_SIZE    40U
#define FDT_BEGIN_NODE     1U
#define FDT_END_NODE       2U
#define FDT_PROP           3U
#define FDT_NOP            4U
/* Sanity limit of the size of the device tree. */
#define FDT_SIZE_MAX       0x100000U

static uint32_t fdt_be32(const uint8_t* p)
{
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16)
        | ((uint32_t)p[2] << 8) | (uint32_t)p[3];
}

/* Round up to the multiple of 4 (the tokens and the names are aligned). */
static uint32_t fdt_align(uint32_t n)
{
    return (n + 3U) & ~3U;
}

/*
 * Whether the node called 'name' is in the structure block
 * [begin, begin + size) of the device tree at 'fdt'. Nothing outside the
 * block is read.
 */
static pok_bool_t fdt_block_has_node(const uint8_t* fdt, uint32_t begin,
    uint32_t size, const char* name)
{
    pok_bool_t found = FALSE;
    uint32_t pos = begin;
    uint32_t end = begin + size;

    while (!found && ((end - pos) >= 4U))
    {
        uint32_t token = fdt_be32(&fdt[pos]);

        pos += 4U;

        if (token == FDT_BEGIN_NODE)
        {
            uint32_t n = 0U;
            pok_bool_t same = TRUE;

            // The name is a string, up to its NUL.
            while (((pos + n) < end) && (fdt[pos + n] != 0U))
            {
                if (same && ((name[n] == '\0')
                    || ((char)fdt[pos + n] != name[n])))
                {
                    same = FALSE;
                }
                n++;
            }
            found = same && ((pos + n) < end) && (name[n] == '\0');
            pos += fdt_align(n + 1U);
        }
        else if (token == FDT_PROP)
        {
            // Length of the value and the name of the property, the value.
            uint32_t left = end - pos;

            if (left < 8U)
            {
                pos = end;
            }
            else
            {
                uint32_t length = fdt_be32(&fdt[pos]);

                if (length > (left - 8U)) {
                    pos = end;
                } else {
                    pos += 8U + fdt_align(length);
                }
            }
        }
        else if ((token == FDT_END_NODE) || (token == FDT_NOP))
        {
            // No data.
        }
        else
        {
            pos = end; // The end of the tree, or garbage.
        }

        if (pos > end) {
            pos = end;
        }
    }

    return found;
}

/*
 * Whether the device tree at 'fdt' has the node called 'name'.
 *
 * MISRA C:2025 Rule 11.4 deviation: the device tree is at a physical
 * address, which is converted to a pointer to bytes. It is read byte by
 * byte, so that it can be read with the MMU off too.
 */
static pok_bool_t fdt_has_node(const char* name)
{
    const uint8_t* fdt = (const uint8_t*)BOARD_DTB_BASE;
    pok_bool_t found = FALSE;

    if (fdt_be32(fdt) == FDT_MAGIC)
    {
        uint32_t total = fdt_be32(&fdt[FDT_OFF_TOTALSIZE]);
        uint32_t begin = fdt_be32(&fdt[FDT_OFF_STRUCT]);
        uint32_t size = fdt_be32(&fdt[FDT_SIZE_STRUCT]);

        if ((total <= FDT_SIZE_MAX) && (total >= FDT_HEADER_SIZE)
            && (begin <= total) && (size <= (total - begin)))
        {
            found = fdt_block_has_node(fdt, begin, size, name);
        }
    }

    return found;
}

/* Whether the second UART exists (known after uart1_probe()). */
static pok_bool_t uart1_present = FALSE;

/* Called for the read and the write stream, at each console init. */
static void uart1_probe(void)
{
    static pok_bool_t uart1_probed = FALSE;

    if (!uart1_probed)
    {
        uart1_present = fdt_has_node(BOARD_UART1_NODE);
        uart1_probed = TRUE;
    }
}

/* The debug stream carries bytes of the GDB protocol: unchanged. */
static size_t debug_write(const char* s, size_t length)
{
    if (uart1_present)
    {
        for (size_t i = 0; i < length; i++)
        {
            uart_putc(BOARD_UART1_BASE, s[i]);
        }
    }
    return length;
}

static size_t debug_read(char* s, size_t length)
{
    return uart1_present ? uart_read(BOARD_UART1_BASE, s, length) : 0U;
}

static struct jet_iostream arm64_stream_debug =
{
    .write = &debug_write,
    .read  = &debug_read,
    .init  = &uart1_probe
};

struct jet_iostream* ja_stream_default_read = &arm64_stream_main;
struct jet_iostream* ja_stream_default_write = &arm64_stream_main;
struct jet_iostream* ja_stream_default_read_debug = &arm64_stream_debug;
struct jet_iostream* ja_stream_default_write_debug = &arm64_stream_debug;

#else

struct jet_iostream* ja_stream_default_read = NULL;
struct jet_iostream* ja_stream_default_write = NULL;
struct jet_iostream* ja_stream_default_read_debug = NULL;
struct jet_iostream* ja_stream_default_write_debug = NULL;

#endif /* POK_NEEDS_CONSOLE */
