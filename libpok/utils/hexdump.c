/*
 * Institute for System Programming of the Russian Academy of Sciences
 * Copyright (C) 2016 ISPRAS
 * Copyright (C) 2026 Kirill Brazhnikov <agamemnon886@gmail.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation, Version 3.
 *
 * This program is distributed in the hope # that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * See the GNU General Public License version 3 for more details.
 *
 * Modified by Kirill Brazhnikov, 2026-10-31: built-in integer types (int,
 * unsigned, short, long, ...) are replaced with the fixed-width types
 * from <stdint.h>.
 *
 * Modified by Kirill Brazhnikov, 2026-11-03: MISRA C:2025: braces around
 * all if/else bodies; discarded printf() results are cast to void; the
 * printed values have the types of their formats; a negative length is
 * rejected like 0 (the last line printed the uninitialized buffer).
 *
 * Modified by Kirill Brazhnikov, 2026-11-08: Doxygen headers for the file
 * and its functions.
 *
 * Modified by Kirill Brazhnikov, 2026-11-14: The function headers no longer
 * have a step for the declaration of a local variable; the @locals table
 * describes it, and an initialized one is set in a step.
 */

/**
 * @file
 * hexdump(): prints memory in hex and ASCII, 16 bytes per line.
 */

#include <debug.h>
#include <stdio.h>

/**
 * @funcname `hexdump`
 *
 * @description
 * Prints memory in hex and ASCII, 16 bytes per line with their offset
 * (non-printable characters as dots).
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `addr` | `const void *` | [in] The memory. |
 * | `len` | `int32_t` | [in] Its length. |
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `buff` | `char[17]` | The ASCII of the line |
 * | `i` | `int32_t` | The index of the byte |
 * | `pc` | `const uint8_t *` | The memory, as bytes |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Set `pc` to `(const uint8_t*)addr`.
 * 2. If `len` is less than or equal to `0`:
 *    1. Call `printf()` with the argument `"Len is zero\n"`.
 *    2. Return.
 * 3. Set `i` to `0`; then while `i` is less than `len`, repeat (after each
 *    pass: increment `i`):
 *    1. If `(i % 16)` is equal to `0`:
 *       1. If `i` is not equal to `0`:
 *          1. Call `printf()` with the arguments `" %%s\n"` and `buff`.
 *       2. Call `printf()` with the arguments `" %04x "` and
 *          `(unsigned int) i`.
 *    2. Call `printf()` with the arguments `" %02x"` and
 *       `(unsigned int) pc[i]`.
 *    3. If `pc[i]` is less than `0x20U` or `pc[i]` is greater than `0x7eU`:
 *       1. Set `buff[i % 16]` to `'.'`.
 *    4. Otherwise:
 *       1. Set `buff[i % 16]` to `(char) pc[i]`.
 *    5. Set `buff[(i % 16) + 1]` to `'\0'`.
 * 4. While `(i % 16)` is not equal to `0`:
 *    1. Call `printf()` with the argument `" "`.
 *    2. Increment `i`.
 * 5. Call `printf()` with the arguments `" %%s\n"` and `buff`.
 *
 * @calls
 * - `printf()`
 */
void hexdump (const void *addr, int32_t len)
{
    int32_t i;
    char buff[17];
    /* MISRA C:2025 Rule 11.5 deviation: the bytes of any object are dumped,
     * so it is passed as a pointer to void. */
    const uint8_t *pc = (const uint8_t*)addr;

    if (len <= 0) {
        (void) printf("Len is zero\n");
        return;
    }

    // Process every byte in the data.
    for (i = 0; i < len; i++) {
        // Multiple of 16 means new line (with line offset).

        if ((i % 16) == 0) {
            // Just don't print ASCII for the zeroth line.
            if (i != 0) {
                (void) printf("  %s\n", buff);
            }

            // Output the offset.
            (void) printf("  %04x ", (unsigned int) i);
        }

        // Now the hex code for the specific character.
        (void) printf(" %02x", (unsigned int) pc[i]);

        // And store a printable ASCII character for later.
        if ((pc[i] < 0x20U) || (pc[i] > 0x7eU)) {
            buff[i % 16] = '.';
        } else {
            buff[i % 16] = (char) pc[i];
        }
        buff[(i % 16) + 1] = '\0';
    }

    // Pad out last line if not exactly 16 characters.
    while ((i % 16) != 0) {
        (void) printf("   ");
        i++;
    }

    // And print the final ASCII bit.
    (void) printf("  %s\n", buff);
}
