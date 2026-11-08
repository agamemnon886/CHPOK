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
 * Modified by Kirill Brazhnikov, 2026-11-08: moved from hypervisor/src/ to
 * arch/x86_64/.
 */

/*
 * The local APIC IDs of the physical CPUs, from the ACPI MADT that the
 * firmware (SeaBIOS under QEMU) leaves in low memory.
 */

#include <arch/internal.h>

#define EBDA_SEGMENT_PTR    0x40EUL
#define BIOS_AREA_START     0xE0000UL
#define BIOS_AREA_END       0x100000UL

#define SDT_HEADER_SIZE     36U
#define MADT_ENTRIES        44U
#define MADT_LOCAL_APIC     0U
#define MADT_LOCAL_X2APIC   9U
#define MADT_CPU_ENABLED    1U
#define MADT_CPU_ONLINE_CAPABLE 2U

// Read a byte of the ACPI tables at a physical address.
/*
 * MISRA C:2025 Rule 11.4 deviation (read8()): the ACPI tables are at
 * physical addresses, identity-mapped by entry.S, which have to be
 * converted to pointers.
 */
static uint8_t read8(uint64_t pa)
{
    const uint8_t *p = (const uint8_t *) (uintptr_t) pa;

    return *p;
}

// Read a little-endian 16-bit value at a physical address.
static uint32_t read16(uint64_t pa)
{
    return (uint32_t) read8(pa) | ((uint32_t) read8(pa + 1U) << 8);
}

// Read a little-endian 32-bit value at a physical address.
static uint32_t read32(uint64_t pa)
{
    return read16(pa) | (read16(pa + 2U) << 16);
}

// Read a little-endian 64-bit value at a physical address.
static uint64_t read64(uint64_t pa)
{
    return (uint64_t) read32(pa) | ((uint64_t) read32(pa + 4U) << 32);
}

// Check that the bytes of an ACPI structure add up to 0 (mod 256).
static bool checksum_ok(uint64_t pa, uint32_t length)
{
    uint8_t sum = 0U;

    for (uint32_t i = 0U; i < length; i++) {
        sum = (uint8_t) (sum + read8(pa + i));
    }
    return sum == 0U;
}

// Check whether the bytes at a physical address are the given signature.
static bool signature_is(uint64_t pa, const char *signature, uint32_t length)
{
    bool same = true;

    for (uint32_t i = 0U; (i < length) && same; i++) {
        same = (read8(pa + i) == (uint8_t) signature[i]);
    }
    return same;
}

// RSDP in [start, end), on a 16-byte boundary; 0 if there is none.
static uint64_t find_rsdp_in(uint64_t start, uint64_t end)
{
    uint64_t found = 0U;

    for (uint64_t pa = start; (pa < end) && (found == 0U); pa += 16U) {
        if (signature_is(pa, "RSD PTR ", 8U) && checksum_ok(pa, 20U)) {
            found = pa;
        }
    }
    return found;
}

/*
 * Find the ACPI RSDP: in the first KiB of the EBDA, then in the BIOS area
 * 0xE0000-0xFFFFF; returns 0 if there is none.
 */
static uint64_t find_rsdp(void)
{
    uint64_t ebda = (uint64_t) read16(EBDA_SEGMENT_PTR) << 4;
    uint64_t rsdp = 0U;

    if (ebda != 0U) {
        rsdp = find_rsdp_in(ebda, ebda + 1024U);
    }
    if (rsdp == 0U) {
        rsdp = find_rsdp_in(BIOS_AREA_START, BIOS_AREA_END);
    }
    return rsdp;
}

// Address of the MADT listed in the RSDT or XSDT; 0 if there is none.
static uint64_t find_madt(uint64_t rsdp)
{
    // ACPI 2.0 and later (revision >= 2) have the 64-bit XSDT.
    bool xsdt = (read8(rsdp + 15U) >= 2U) && (read64(rsdp + 24U) != 0U);
    uint64_t sdt = xsdt ? read64(rsdp + 24U) : (uint64_t) read32(rsdp + 16U);
    uint32_t entry_size = xsdt ? 8U : 4U;
    uint32_t length = read32(sdt + 4U);
    uint64_t madt = 0U;

    if (length < SDT_HEADER_SIZE) {
        length = SDT_HEADER_SIZE;
    }
    // The table's header is followed by the addresses of the other tables.
    for (uint32_t off = SDT_HEADER_SIZE;
         ((off + entry_size) <= length) && (madt == 0U);
         off += entry_size) {
        uint64_t table = xsdt ? read64(sdt + off)
                              : (uint64_t) read32(sdt + off);

        if (signature_is(table, "APIC", 4U)) {
            madt = table;
        }
    }
    return madt;
}

/*
 * Fill apic_ids with the local APIC IDs of the enabled CPUs in the MADT (at
 * most max) and return their number. Stops the hypervisor if there is no MADT.
 */
uint32_t acpi_find_cpus(uint32_t apic_ids[], uint32_t max)
{
    uint64_t rsdp = find_rsdp();
    uint64_t madt = (rsdp != 0U) ? find_madt(rsdp) : 0U;
    uint32_t count = 0U;

    if (madt == 0U) {
        hv_fatal("no ACPI MADT found");
    }

    uint32_t length = read32(madt + 4U);
    uint32_t off = MADT_ENTRIES;

    // Entries: type, length, then the type's fields.
    while (((off + 2U) <= length) && (count < max)) {
        uint32_t type = read8(madt + off);
        uint32_t entry_length = read8(madt + off + 1U);
        uint32_t flags = 0U;
        uint32_t apic_id = 0U;
        bool cpu = false;

        if ((type == MADT_LOCAL_APIC) && (entry_length >= 8U)) {
            apic_id = read8(madt + off + 3U);
            flags = read32(madt + off + 4U);
            cpu = true;
        } else if ((type == MADT_LOCAL_X2APIC) && (entry_length >= 16U)) {
            apic_id = read32(madt + off + 4U);
            flags = read32(madt + off + 8U);
            cpu = true;
        } else {
            // Other entries (I/O APIC, overrides, NMI) are not used.
        }
        // Disabled CPUs (only "online capable") can't be started.
        if (cpu && ((flags & MADT_CPU_ENABLED) != 0U)) {
            apic_ids[count] = apic_id;
            count++;
        }
        if (entry_length < 2U) {
            off = length;   // Malformed entry: stop
        } else {
            off += entry_length;
        }
    }
    return count;
}
