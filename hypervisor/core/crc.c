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
 * Modified by Kirill Brazhnikov, 2026-11-08: Doxygen headers for the file and
 * its functions.
 *
 * Modified by Kirill Brazhnikov, 2026-11-14: the macros, types and declarations
 * are in include/hv/hv.h, documented there.
 *
 * Modified by Kirill Brazhnikov, 2026-11-15: moved from hypervisor/src/crc.c
 * (the common code and its headers are in hypervisor/core).
 */

/**
 * @file
 * Check of the hypervisor image at boot: the CRC-32 of the loaded image,
 * from hv_image_begin up to the word at hv_image_crc (hv.lds), must be the
 * one hvcrc.py wrote into that word after linking. The CRC is the one of
 * zlib and IEEE 802.3 (reflected polynomial 0x04C11DB7, initial value and
 * final XOR 0xFFFFFFFF; the CRC of "123456789" is 0xCBF43926). It detects
 * accidental corruption, not deliberate changes: whoever changes the image
 * can also change the stored CRC.
 */

#include <hv/hv.h>

// CRC of each byte value, filled by the first hv_crc32().
static uint32_t crc_table[CRC32_TABLE_SIZE];
static bool crc_table_ready = false;

/**
 * @funcname `crc_table_init`
 *
 * @description
 * Fills the table of the CRC-32 of each byte value, used by hv_crc32().
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `crc_table` | `uint32_t[256]` | The CRC of each byte value |
 * | `crc_table_ready` | `bool` | Whether crc_table is filled |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `i` | `uint32_t` | The byte value |
 * | `value` | `uint32_t` | The CRC of the byte value being computed |
 * | `bit` | `uint32_t` | The bit being shifted out |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Set `i` to `0U`; then while `i` is less than `CRC32_TABLE_SIZE`, repeat
 *    (after each pass: increment `i`):
 *    1. Set `value` to `i`.
 *    2. Set `bit` to `0U`; then while `bit` is less than `BITS_PER_BYTE`,
 *       repeat (after each pass: increment `bit`):
 *       1. If `(value & 1U)` is not equal to `0U`:
 *          1. Set `value` to `(value >> 1U) ^ CRC32_POLYNOMIAL`.
 *       2. Otherwise:
 *          1. Shift `value` right by `1U` bits.
 *    3. Set `crc_table[i]` to `value`.
 * 2. Set `crc_table_ready` to `true`.
 *
 * @calls
 * None.
 */
static void crc_table_init(void)
{
    for (uint32_t i = 0U; i < CRC32_TABLE_SIZE; i++) {
        uint32_t value = i;

        for (uint32_t bit = 0U; bit < BITS_PER_BYTE; bit++) {
            if ((value & 1U) != 0U) {
                value = (value >> 1U) ^ CRC32_POLYNOMIAL;
            } else {
                value >>= 1U;
            }
        }
        crc_table[i] = value;
    }
    crc_table_ready = true;
}

/**
 * @funcname `hv_crc32`
 *
 * @description
 * Computes the CRC-32 (zlib, IEEE 802.3) of a block of bytes.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `data` | `const uint8_t *` | [in] The bytes. |
 * | `length` | `size_t` | [in] The number of bytes. |
 *
 * @return The CRC-32.
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `crc_table` | `uint32_t[256]` | The CRC of each byte value |
 * | `crc_table_ready` | `bool` | Whether crc_table is filled |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `crc` | `uint32_t` | The CRC so far |
 * | `i` | `size_t` | The index of the byte |
 * | `index` | `uint32_t` | The table entry for the byte |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Set `crc` to `CRC32_INITIAL`.
 * 2. If `crc_table_ready` is false:
 *    1. Call `crc_table_init()`.
 * 3. Set `i` to `0U`; then while `i` is less than `length`, repeat (after each
 *    pass: increment `i`):
 *    1. Set `index` to `(crc ^ (uint32_t) data[i]) & BYTE_MASK`.
 *    2. Set `crc` to `(crc >> BITS_PER_BYTE) ^ crc_table[index]`.
 * 4. Return `crc ^ CRC32_FINAL_XOR`.
 *
 * @calls
 * - `crc_table_init()`
 */
uint32_t hv_crc32(const uint8_t *data, size_t length)
{
    uint32_t crc = CRC32_INITIAL;

    if (!crc_table_ready) {
        crc_table_init();
    }
    for (size_t i = 0U; i < length; i++) {
        uint32_t index = (crc ^ (uint32_t) data[i]) & BYTE_MASK;

        crc = (crc >> BITS_PER_BYTE) ^ crc_table[index];
    }
    return crc ^ CRC32_FINAL_XOR;
}

/**
 * @funcname `hv_image_check`
 *
 * @description
 * Checks the CRC-32 of the hypervisor image at boot against the one stored
 * after linking. If it isn't the stored one, prints both and halts.
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `hv_image_begin` | `const uint8_t[]` | The start of the loaded image (hv.lds) |
 * | `hv_image_crc` | `const uint32_t` | The stored CRC-32 (hv.lds, hvcrc.py) |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `begin` | `uintptr_t` | The address of the start of the image |
 * | `end` | `uintptr_t` | The address of the stored CRC |
 * | `size` | `size_t` | The size of the checked part |
 * | `expected` | `uint32_t` | The stored CRC-32 |
 * | `actual` | `uint32_t` | The CRC-32 of the image |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Set `begin` to `(uintptr_t) hv_image_begin`.
 * 2. Set `end` to `(uintptr_t) &hv_image_crc`.
 * 3. Set `size` to `(size_t) (end - begin)`.
 * 4. Set `expected` to `hv_image_crc`.
 * 5. Call `hv_crc32()` with the arguments `hv_image_begin` and `size`, and
 *    store the result in `actual`.
 * 6. If `actual` is not equal to `expected`:
 *    1. Call `hv_printf()` with the arguments
 *       `"[hv] image CRC-32 mismatch: expected 0x%08x, " "actual 0x%08x\n"`,
 *       `expected` and `actual`.
 *    2. Call `hv_fatal()` with the argument
 *       `"the hypervisor image is corrupted"`.
 * 7. Call `hv_printf()` with the arguments
 *    `"[hv] image CRC-32 0x%08x OK (%%zu KiB)\n"`, `actual` and `size >> 10U`.
 *
 * @calls
 * - `hv_crc32()`
 * - `hv_printf()`
 * - `hv_fatal()`
 */
/*
 * MISRA C:2025 Rule 11.4 deviation (hv_image_check()): the size of the
 * checked part is the distance between two linker addresses, which
 * aren't in one C object, so they are converted to integers.
 */
void hv_image_check(void)
{
    uintptr_t begin = (uintptr_t) hv_image_begin;
    uintptr_t end = (uintptr_t) &hv_image_crc;
    size_t size = (size_t) (end - begin);
    uint32_t expected = hv_image_crc;
    uint32_t actual = hv_crc32(hv_image_begin, size);

    if (actual != expected) {
        hv_printf("[hv] image CRC-32 mismatch: expected 0x%08x, "
                  "actual 0x%08x\n", expected, actual);
        hv_fatal("the hypervisor image is corrupted");
    }
    hv_printf("[hv] image CRC-32 0x%08x OK (%zu KiB)\n", actual,
              size >> 10U);
}
