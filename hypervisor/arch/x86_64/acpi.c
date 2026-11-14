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
 * arch/x86_64/. Doxygen headers for the file and its functions.
 *
 * Modified by Kirill Brazhnikov, 2026-11-14: the macros, types and external
 * declarations are in arch/internal.h, documented there.
 */

/**
 * @file
 * The local APIC IDs of the physical CPUs, from the ACPI MADT that the
 * firmware (SeaBIOS under QEMU) leaves in low memory.
 */

#include <arch/internal.h>

/**
 * @funcname `read8`
 *
 * @description
 * Reads a byte of the ACPI tables at a physical address (identity-mapped by
 * entry.S).
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `pa` | `uint64_t` | [in] The physical address. |
 *
 * @return The byte.
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `p` | `const uint8_t *` | The address as a pointer |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Set `p` to `(const uint8_t *) (uintptr_t) pa`.
 * 2. Return the value at `p`.
 *
 * @calls
 * None.
 */
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

/**
 * @funcname `read16`
 *
 * @description
 * Reads a little-endian 16-bit value of the ACPI tables, byte by byte (it
 * needn't be aligned).
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `pa` | `uint64_t` | [in] The physical address. |
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
 * 1. Return `(uint32_t) read8(pa) | ((uint32_t) read8(pa + 1U) << 8)`.
 *
 * @calls
 * - `read8()`
 */
static uint32_t read16(uint64_t pa)
{
    return (uint32_t) read8(pa) | ((uint32_t) read8(pa + 1U) << 8);
}

/**
 * @funcname `read32`
 *
 * @description
 * Reads a little-endian 32-bit value of the ACPI tables.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `pa` | `uint64_t` | [in] The physical address. |
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
 * 1. Return `read16(pa) | (read16(pa + 2U) << 16)`.
 *
 * @calls
 * - `read16()`
 */
static uint32_t read32(uint64_t pa)
{
    return read16(pa) | (read16(pa + 2U) << 16);
}

/**
 * @funcname `read64`
 *
 * @description
 * Reads a little-endian 64-bit value of the ACPI tables.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `pa` | `uint64_t` | [in] The physical address. |
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
 * 1. Return `(uint64_t) read32(pa) | ((uint64_t) read32(pa + 4U) << 32)`.
 *
 * @calls
 * - `read32()`
 */
static uint64_t read64(uint64_t pa)
{
    return (uint64_t) read32(pa) | ((uint64_t) read32(pa + 4U) << 32);
}

/**
 * @funcname `checksum_ok`
 *
 * @description
 * Checks the checksum of an ACPI structure: its bytes add up to 0, modulo
 * 256.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `pa` | `uint64_t` | [in] The physical address of the structure. |
 * | `length` | `uint32_t` | [in] Its length in bytes. |
 *
 * @return true if the checksum is right.
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `sum` | `uint8_t` | The sum of the bytes so far, modulo 256 |
 * | `i` | `uint32_t` | The index of the byte |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Set `sum` to `0U`.
 * 2. Set `i` to `0U`; then while `i` is less than `length`, repeat (after each
 *    pass: increment `i`):
 *    1. Set `sum` to `(uint8_t) (sum + read8(pa + i))`.
 * 3. Return `sum == 0U`.
 *
 * @calls
 * - `read8()`
 */
static bool checksum_ok(uint64_t pa, uint32_t length)
{
    uint8_t sum = 0U;

    for (uint32_t i = 0U; i < length; i++) {
        sum = (uint8_t) (sum + read8(pa + i));
    }
    return sum == 0U;
}

/**
 * @funcname `signature_is`
 *
 * @description
 * Checks whether the bytes at a physical address are a given signature.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `pa` | `uint64_t` | [in] The physical address. |
 * | `signature` | `const char *` | [in] The signature's characters. |
 * | `length` | `uint32_t` | [in] The signature's length. |
 *
 * @return true if all the bytes match.
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `same` | `bool` | Whether the bytes so far match |
 * | `i` | `uint32_t` | The index of the byte |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Set `same` to `true`.
 * 2. Set `i` to `0U`; then while `i` is less than `length` and `same` is true,
 *    repeat (after each pass: increment `i`):
 *    1. Set `same` to `(read8(pa + i) == (uint8_t) signature[i])`.
 * 3. Return `same`.
 *
 * @calls
 * - `read8()`
 */
static bool signature_is(uint64_t pa, const char *signature, uint32_t length)
{
    bool same = true;

    for (uint32_t i = 0U; (i < length) && same; i++) {
        same = (read8(pa + i) == (uint8_t) signature[i]);
    }
    return same;
}

/**
 * @funcname `find_rsdp_in`
 *
 * @description
 * Looks for the ACPI RSDP in a memory range, on 16-byte boundaries.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `start` | `uint64_t` | [in] The start of the range. |
 * | `end` | `uint64_t` | [in] The end of the range (excluded). |
 *
 * @return The address of the RSDP, or 0 if there is none.
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `found` | `uint64_t` | The address of the RSDP found, or 0 |
 * | `pa` | `uint64_t` | The address being checked |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Set `found` to `0U`.
 * 2. Set `pa` to `start`; then while `pa` is less than `end` and `found` is
 *    equal to `0U`, repeat (after each pass: add `16U` to `pa`):
 *    1. If `signature_is(pa, "RSD PTR ", 8U) && checksum_ok(pa, 20U)` holds:
 *       1. Set `found` to `pa`.
 * 3. Return `found`.
 *
 * @calls
 * - `signature_is()`
 * - `checksum_ok()`
 */
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

/**
 * @funcname `find_rsdp`
 *
 * @description
 * Finds the ACPI RSDP: in the first KiB of the EBDA, then in the BIOS area
 * 0xE0000-0xFFFFF.
 *
 * @return The address of the RSDP, or 0 if there is none.
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `ebda` | `uint64_t` | The address of the EBDA (from its segment at 0x40E) |
 * | `rsdp` | `uint64_t` | The address of the RSDP, or 0 |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Set `ebda` to `(uint64_t) read16(EBDA_SEGMENT_PTR) << 4`.
 * 2. Set `rsdp` to `0U`.
 * 3. If `ebda` is not equal to `0U`:
 *    1. Call `find_rsdp_in()` with the arguments `ebda` and `ebda + 1024U`, and
 *       store the result in `rsdp`.
 * 4. If `rsdp` is equal to `0U`:
 *    1. Call `find_rsdp_in()` with the arguments `BIOS_AREA_START` and
 *       `BIOS_AREA_END`, and store the result in `rsdp`.
 * 5. Return `rsdp`.
 *
 * @calls
 * - `read16()`
 * - `find_rsdp_in()`
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

/**
 * @funcname `find_madt`
 *
 * @description
 * Finds the MADT ("APIC" table) listed in the RSDT or, from ACPI 2.0 on, in
 * the XSDT.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `rsdp` | `uint64_t` | [in] The address of the RSDP. |
 *
 * @return The address of the MADT, or 0 if there is none.
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `xsdt` | `bool` | Whether the 64-bit XSDT is used |
 * | `sdt` | `uint64_t` | The address of the RSDT or XSDT |
 * | `entry_size` | `uint32_t` | The size of a table address: 4 or 8 |
 * | `length` | `uint32_t` | The length of the RSDT or XSDT |
 * | `madt` | `uint64_t` | The address of the MADT, or 0 |
 * | `off` | `uint32_t` | The offset of the table address being read |
 * | `table` | `uint64_t` | A table address |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Set `xsdt` to `(read8(rsdp + 15U) >= 2U) && (read64(rsdp + 24U) != 0U)`.
 * 2. Set `sdt` to `read64(rsdp + 24U)` if `xsdt` is true, otherwise
 *    `(uint64_t) read32(rsdp + 16U)`.
 * 3. Set `entry_size` to `8U` if `xsdt` is true, otherwise `4U`.
 * 4. Call `read32()` with the argument `sdt + 4U`, and store the result in
 *    `length`.
 * 5. Set `madt` to `0U`.
 * 6. If `length` is less than `SDT_HEADER_SIZE`:
 *    1. Set `length` to `SDT_HEADER_SIZE`.
 * 7. Set `off` to `SDT_HEADER_SIZE`; then while `(off + entry_size)` is less
 *    than or equal to `length` and `madt` is equal to `0U`, repeat (after each
 *    pass: add `entry_size` to `off`):
 *    1. Set `table` to `read64(sdt + off)` if `xsdt` is true, otherwise
 *       `(uint64_t) read32(sdt + off)`.
 *    2. If `signature_is(table, "APIC", 4U)` is true:
 *       1. Set `madt` to `table`.
 * 8. Return `madt`.
 *
 * @calls
 * - `read8()`
 * - `read64()`
 * - `read32()`
 * - `signature_is()`
 */
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

/**
 * @funcname `acpi_find_cpus`
 *
 * @description
 * Fills a list with the local APIC IDs of the enabled CPUs in the MADT, at
 * most max of them. Stops the hypervisor if there is no MADT.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `apic_ids` | `uint32_t[]` | [out] Where the APIC IDs are stored. |
 * | `max` | `uint32_t` | [in] The maximum number of CPUs. |
 *
 * @return The number of CPUs found.
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `rsdp` | `uint64_t` | The address of the RSDP, or 0 |
 * | `madt` | `uint64_t` | The address of the MADT, or 0 |
 * | `count` | `uint32_t` | The number of CPUs found |
 * | `length` | `uint32_t` | The length of the MADT |
 * | `off` | `uint32_t` | The offset of the entry being read |
 * | `type` | `uint32_t` | The type of the entry |
 * | `entry_length` | `uint32_t` | The length of the entry |
 * | `flags` | `uint32_t` | The flags of a CPU entry |
 * | `apic_id` | `uint32_t` | The APIC ID of a CPU entry |
 * | `cpu` | `bool` | Whether the entry is a CPU |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Call `find_rsdp()`, and store the result in `rsdp`.
 * 2. Set `madt` to `find_madt(rsdp)` if `rsdp` is not equal to `0U`, otherwise
 *    `0U`.
 * 3. Set `count` to `0U`.
 * 4. If `madt` is equal to `0U`:
 *    1. Call `hv_fatal()` with the argument `"no ACPI MADT found"`.
 * 5. Call `read32()` with the argument `madt + 4U`, and store the result in
 *    `length`.
 * 6. Set `off` to `MADT_ENTRIES`.
 * 7. While `(off + 2U)` is less than or equal to `length` and `count` is less
 *    than `max`:
 *    1. Call `read8()` with the argument `madt + off`, and store the result in
 *       `type`.
 *    2. Call `read8()` with the argument `madt + off + 1U`, and store the
 *       result in `entry_length`.
 *    3. Set `flags` to `0U`.
 *    4. Set `apic_id` to `0U`.
 *    5. Set `cpu` to `false`.
 *    6. If `type` is equal to `MADT_LOCAL_APIC` and `entry_length` is greater
 *       than or equal to `8U`:
 *       1. Call `read8()` with the argument `madt + off + 3U`, and store the
 *          result in `apic_id`.
 *       2. Call `read32()` with the argument `madt + off + 4U`, and store the
 *          result in `flags`.
 *       3. Set `cpu` to `true`.
 *    7. Otherwise, if `type` is equal to `MADT_LOCAL_X2APIC` and `entry_length`
 *       is greater than or equal to `16U`:
 *       1. Call `read32()` with the argument `madt + off + 4U`, and store the
 *          result in `apic_id`.
 *       2. Call `read32()` with the argument `madt + off + 8U`, and store the
 *          result in `flags`.
 *       3. Set `cpu` to `true`.
 *    8. Otherwise:
 *       1. Do nothing.
 *    9. If `cpu` is true and `(flags & MADT_CPU_ENABLED)` is not equal to `0U`:
 *       1. Set `apic_ids[count]` to `apic_id`.
 *       2. Increment `count`.
 *    10. If `entry_length` is less than `2U`:
 *        1. Set `off` to `length`.
 *    11. Otherwise:
 *        1. Add `entry_length` to `off`.
 * 8. Return `count`.
 *
 * @calls
 * - `find_rsdp()`
 * - `find_madt()`
 * - `hv_fatal()`
 * - `read32()`
 * - `read8()`
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
