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
 */

#include <debug.h>
#include <stdio.h>

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
