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

/*
 * Check of the hypervisor image at boot: the CRC-32 of the loaded image,
 * from hv_image_begin up to the word at hv_image_crc (hv.lds), must be the
 * one hvcrc.py wrote into that word after linking. The CRC is the one of
 * zlib and IEEE 802.3 (reflected polynomial 0x04C11DB7, initial value and
 * final XOR 0xFFFFFFFF; the CRC of "123456789" is 0xCBF43926). It detects
 * accidental corruption, not deliberate changes: whoever changes the image
 * can also change the stored CRC.
 */

#include <hv/hv.h>

#define CRC32_POLYNOMIAL    0xEDB88320U     // 0x04C11DB7, bits reversed
#define CRC32_INITIAL       0xFFFFFFFFU
#define CRC32_FINAL_XOR     0xFFFFFFFFU
#define CRC32_TABLE_SIZE    256U
#define BITS_PER_BYTE       8U
#define BYTE_MASK           0xFFU

/*
 * Linker addresses (hv.lds): the start of the loaded image, and the word
 * after .rodata that holds the CRC-32 of the bytes before it.
 */
extern const uint8_t hv_image_begin[];
extern const uint32_t hv_image_crc;

// CRC of each byte value, filled by the first hv_crc32().
static uint32_t crc_table[CRC32_TABLE_SIZE];
static bool crc_table_ready = false;

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

// CRC-32 of length bytes at data.
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

/*
 * Check the CRC-32 of the hypervisor image. If it isn't the stored one,
 * print both and halt (hv_fatal()).
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
