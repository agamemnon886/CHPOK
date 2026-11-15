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
 *
 * Modified by Kirill Brazhnikov, 2026-11-08: Doxygen headers for the file
 * and its functions.
 *
 * Modified by Kirill Brazhnikov, 2026-11-13: the macros are moved to
 * boards/arm64-qemu/include/board/bsp.h; jet_iostream_t is used by its typedef
 * name.
 *
 * Modified by Kirill Brazhnikov, 2026-11-14: The function headers no longer
 * have a step for the declaration of a local variable; the @locals table
 * describes it, and an initialized one is set in a step.
 *
 * Modified by Kirill Brazhnikov, 2026-11-15: moved from
 * boards/arm64-qemu/cons.c (the board support packages are in kernel/bsps).
 */

/**
 * @file
 * The consoles of QEMU 'virt' (arm64): the main stream on the PL011 UART at
 * 0x09000000, and the debug stream of the GDB server on the second PL011
 * (0x09040000). QEMU creates the second UART only when it is run with two
 * -serial options; accessing a UART which is not there would be a fatal bus
 * error, so it is looked for in the device tree that QEMU gives to the
 * kernel.
 */

#include <config.h>
#include <libc.h>
#include <asp/cons.h>
#include <arch/ioports.h>
#include <bsp/bsp.h>

#ifdef POK_NEEDS_CONSOLE

/**
 * @funcname `uart_putc`
 *
 * @description
 * Writes one byte to a PL011 UART, waiting while its transmit FIFO is full.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `base` | `uintptr_t` | [in] The address of the UART. |
 * | `c` | `char` | [in] The byte to write. |
 *
 * @globals
 * None.
 *
 * @locals
 * None.
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. While `(ja_mmio_read32(base + UART_FR) & UART_FR_TXFF)` is not equal to
 *    `0U`:
 *    1. Do nothing.
 * 2. Call `ja_mmio_write32()` with the arguments `base + UART_DR` and
 *    `(uint8_t)c`.
 *
 * @calls
 * - `ja_mmio_read32()`
 * - `ja_mmio_write32()`
 */
static void uart_putc(uintptr_t base, char c)
{
    while ((ja_mmio_read32(base + UART_FR) & UART_FR_TXFF) != 0U) {
        ;
    }
    ja_mmio_write32(base + UART_DR, (uint8_t)c);
}

/**
 * @funcname `uart_read`
 *
 * @description
 * Reads the bytes that a PL011 UART has received, up to length, without
 * waiting for more.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `base` | `uintptr_t` | [in] The address of the UART. |
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
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Set `i` to `0`; then while `i` is not equal to `length`, repeat (after
 *    each pass: increment `i`):
 *    1. If `(ja_mmio_read32(base + UART_FR) & UART_FR_RXFE)` is not equal to
 *       `0U`:
 *       1. Leave the loop.
 *    2. Set `s[i]` to `(char)ja_mmio_read32(base + UART_DR)`.
 * 2. Return `i`.
 *
 * @calls
 * - `ja_mmio_read32()`
 */
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

/**
 * @funcname `iostream_write`
 *
 * @description
 * The write operation of the main stream: writes bytes to the first UART,
 * with a carriage return before each line feed (as on x86, serial terminals
 * need "\r\n").
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
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
 * | `i` | `size_t` | The index of the byte being written |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Set `i` to `0`; then while `i` is less than `length`, repeat (after each
 *    pass: increment `i`):
 *    1. If `s[i]` is equal to `'\n'`:
 *       1. Call `uart_putc()` with the arguments `BOARD_UART_BASE` and `'\r'`.
 *    2. Call `uart_putc()` with the arguments `BOARD_UART_BASE` and `s[i]`.
 * 2. Return `length`.
 *
 * @calls
 * - `uart_putc()`
 */
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

/**
 * @funcname `iostream_read`
 *
 * @description
 * The read operation of the main stream: reads from the first UART.
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
 *
 * @llr
 *
 * @design
 * 1. Call `uart_read()` with the arguments `BOARD_UART_BASE`, `s` and `length`,
 *    and return its result.
 *
 * @calls
 * - `uart_read()`
 */
static size_t iostream_read(char* s, size_t length)
{
    return uart_read(BOARD_UART_BASE, s, length);
}

static jet_iostream_t arm64_stream_main =
{
    .write = &iostream_write,
    .read  = &iostream_read,
    .init  = NULL // QEMU's PL011 needs no setup.
};

/**
 * @funcname `fdt_be32`
 *
 * @description
 * Reads a big-endian 32-bit value of the device tree, byte by byte (so that
 * it needn't be aligned).
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `p` | `const uint8_t *` | [in] The address of the value. |
 *
 * @return The value.
 *
 * @globals
 * None.
 *
 * @locals
 * None.
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Return
 *    `((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8)`
 *    `| (uint32_t)p[3]`.
 *
 * @calls
 * None.
 */
static uint32_t fdt_be32(const uint8_t* p)
{
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16)
        | ((uint32_t)p[2] << 8) | (uint32_t)p[3];
}

/**
 * @funcname `fdt_align`
 *
 * @description
 * Rounds a size up to a multiple of 4: the tokens and names of the device
 * tree are aligned to 4 bytes.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `n` | `uint32_t` | [in] The size. |
 *
 * @return n rounded up to a multiple of 4.
 *
 * @globals
 * None.
 *
 * @locals
 * None.
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Return `(n + 3U) & ~3U`.
 *
 * @calls
 * None.
 */
static uint32_t fdt_align(uint32_t n)
{
    return (n + 3U) & ~3U;
}

/**
 * @funcname `fdt_block_has_node`
 *
 * @description
 * Tells whether the node called name is in the structure block
 * [begin, begin + size) of a device tree. Nothing outside the block is
 * read, so a damaged tree doesn't make it read anywhere else.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `fdt` | `const uint8_t *` | [in] The device tree. |
 * | `begin` | `uint32_t` | [in] The offset of the structure block. |
 * | `size` | `uint32_t` | [in] The size of the structure block. |
 * | `name` | `const char *` | [in] The node name to look for, with its unit address. |
 *
 * @return TRUE if the node is there, FALSE otherwise.
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `found` | `pok_bool_t` | Whether the node was found |
 * | `pos` | `uint32_t` | The offset of the next token |
 * | `end` | `uint32_t` | The offset of the end of the block |
 * | `token` | `uint32_t` | The token being read |
 * | `n` | `uint32_t` | The length of the node name being read |
 * | `same` | `pok_bool_t` | Whether the node name read so far matches |
 * | `left` | `uint32_t` | The bytes left in the block after a FDT_PROP |
 * | `length` | `uint32_t` | The length of a property value |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Set `found` to `FALSE`.
 * 2. Set `pos` to `begin`.
 * 3. Set `end` to `begin + size`.
 * 4. While `found` is false and `(end - pos)` is greater than or equal to `4U`:
 *    1. Call `fdt_be32()` with the address of `fdt[pos]`, and store the result
 *       in `token`.
 *    2. Add `4U` to `pos`.
 *    3. If `token` is equal to `FDT_BEGIN_NODE`:
 *       1. Set `n` to `0U`.
 *       2. Set `same` to `TRUE`.
 *       3. While `(pos + n)` is less than `end` and `fdt[pos + n]` is not equal
 *          to `0U`:
 *          1. If `same` is true and (`name[n]` is equal to `'\0'` or
 *             `(char)fdt[pos + n]` is not equal to `name[n]`):
 *             1. Set `same` to `FALSE`.
 *          2. Increment `n`.
 *       4. Set `found` to `same && ((pos + n) < end) && (name[n] == '\0')`.
 *       5. Add `fdt_align(n + 1U)` to `pos`.
 *    4. Otherwise, if `token` is equal to `FDT_PROP`:
 *       1. Set `left` to `end - pos`.
 *       2. If `left` is less than `8U`:
 *          1. Set `pos` to `end`.
 *       3. Otherwise:
 *          1. Call `fdt_be32()` with the address of `fdt[pos]`, and store the
 *             result in `length`.
 *          2. If `length` is greater than `(left - 8U)`:
 *             1. Set `pos` to `end`.
 *          3. Otherwise:
 *             1. Add `8U + fdt_align(length)` to `pos`.
 *    5. Otherwise, if `token` is equal to `FDT_END_NODE` or `token` is equal to
 *       `FDT_NOP`:
 *       1. Do nothing.
 *    6. Otherwise:
 *       1. Set `pos` to `end`.
 *    7. If `pos` is greater than `end`:
 *       1. Set `pos` to `end`.
 * 5. Return `found`.
 *
 * @calls
 * - `fdt_be32()`
 * - `fdt_align()`
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

/**
 * @funcname `fdt_has_node`
 *
 * @description
 * Tells whether the device tree that QEMU gives to the kernel (at the start
 * of RAM) has the node called name. The tree is read byte by byte, so that
 * it can be read with the MMU off too.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `name` | `const char *` | [in] The node name to look for, with its unit address. |
 *
 * @return TRUE if the node is there, FALSE otherwise (also if there is no
 *         valid device tree).
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `fdt` | `const uint8_t *` | The device tree |
 * | `found` | `pok_bool_t` | Whether the node was found |
 * | `total` | `uint32_t` | The size of the device tree |
 * | `begin` | `uint32_t` | The offset of the structure block |
 * | `size` | `uint32_t` | The size of the structure block |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Set `fdt` to `(const uint8_t*)BOARD_DTB_BASE`.
 * 2. Set `found` to `FALSE`.
 * 3. If `fdt_be32(fdt)` is equal to `FDT_MAGIC`:
 *    1. Call `fdt_be32()` with the address of `fdt[FDT_OFF_TOTALSIZE]`, and
 *       store the result in `total`.
 *    2. Call `fdt_be32()` with the address of `fdt[FDT_OFF_STRUCT]`, and store
 *       the result in `begin`.
 *    3. Call `fdt_be32()` with the address of `fdt[FDT_SIZE_STRUCT]`, and store
 *       the result in `size`.
 *    4. If `total` is less than or equal to `FDT_SIZE_MAX` and `total` is
 *       greater than or equal to `FDT_HEADER_SIZE` and `begin` is less than or
 *       equal to `total` and `size` is less than or equal to `(total - begin)`:
 *       1. Call `fdt_block_has_node()` with the arguments `fdt`, `begin`,
 *          `size` and `name`, and store the result in `found`.
 * 4. Return `found`.
 *
 * @calls
 * - `fdt_be32()`
 * - `fdt_block_has_node()`
 */
/*
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

/**
 * @funcname `uart1_probe`
 *
 * @description
 * The init operation of the debug stream: finds out once whether the second
 * UART exists. It is called for the read and the write stream, at each
 * console init.
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `uart1_present` | `pok_bool_t` | Whether the second UART exists |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `uart1_probed` | `static pok_bool_t` | Whether the device tree was read |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. If `uart1_probed` is false:
 *    1. Call `fdt_has_node()` with the argument `BOARD_UART1_NODE`, and store
 *       the result in `uart1_present`.
 *    2. Set `uart1_probed` to `TRUE`.
 *
 * @calls
 * - `fdt_has_node()`
 */
static void uart1_probe(void)
{
    static pok_bool_t uart1_probed = FALSE;

    if (!uart1_probed)
    {
        uart1_present = fdt_has_node(BOARD_UART1_NODE);
        uart1_probed = TRUE;
    }
}

/**
 * @funcname `debug_write`
 *
 * @description
 * The write operation of the debug stream: writes the bytes of the GDB
 * protocol to the second UART unchanged, if it exists.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `s` | `const char *` | [in] The bytes to write. |
 * | `length` | `size_t` | [in] The number of bytes. |
 *
 * @return length (the bytes are dropped if there is no second UART).
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `uart1_present` | `pok_bool_t` | Whether the second UART exists |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `i` | `size_t` | The index of the byte being written |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. If `uart1_present` is true:
 *    1. Set `i` to `0`; then while `i` is less than `length`, repeat (after
 *       each pass: increment `i`):
 *       1. Call `uart_putc()` with the arguments `BOARD_UART1_BASE` and `s[i]`.
 * 2. Return `length`.
 *
 * @calls
 * - `uart_putc()`
 */
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

/**
 * @funcname `debug_read`
 *
 * @description
 * The read operation of the debug stream: reads from the second UART, if it
 * exists.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `s` | `char *` | [out] Where the bytes are stored. |
 * | `length` | `size_t` | [in] The maximum number of bytes to read. |
 *
 * @return The number of bytes read (0 if there is no second UART).
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `uart1_present` | `pok_bool_t` | Whether the second UART exists |
 *
 * @locals
 * None.
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Return `uart_read(BOARD_UART1_BASE, s, length)` if `uart1_present` is
 *    true, otherwise `0U`.
 *
 * @calls
 * - `uart_read()`
 */
static size_t debug_read(char* s, size_t length)
{
    return uart1_present ? uart_read(BOARD_UART1_BASE, s, length) : 0U;
}

static jet_iostream_t arm64_stream_debug =
{
    .write = &debug_write,
    .read  = &debug_read,
    .init  = &uart1_probe
};

jet_iostream_t* ja_stream_default_read = &arm64_stream_main;
jet_iostream_t* ja_stream_default_write = &arm64_stream_main;
jet_iostream_t* ja_stream_default_read_debug = &arm64_stream_debug;
jet_iostream_t* ja_stream_default_write_debug = &arm64_stream_debug;

#else

jet_iostream_t* ja_stream_default_read = NULL;
jet_iostream_t* ja_stream_default_write = NULL;
jet_iostream_t* ja_stream_default_read_debug = NULL;
jet_iostream_t* ja_stream_default_write_debug = NULL;

#endif /* POK_NEEDS_CONSOLE */
