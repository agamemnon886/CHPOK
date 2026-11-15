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
 * are in arch/internal.h, documented there.
 *
 * Modified by Kirill Brazhnikov, 2026-11-15: moved from
 * hypervisor/arch/arm64/vgicd.c (the code of the board, its devices and the
 * virtual devices of the VMs is in hypervisor/bsps); memset() of the library
 * shared with the kernel replaces hv_memset().
 */

/**
 * @file
 * The GICv2 distributor of a VM (guest-physical 0x08000000), emulated: each
 * access exits (mmio.c). The VM has one CPU and interrupts 0-31 (TYPER:
 * ITLinesNumber 0, CPUNumber 0); of these only the virtual timer (27) ever
 * comes (gic.c).
 *
 * - CTLR: read/write (EnableGrp0).
 * - TYPER, IIDR: read only (IIDR: the host's).
 * - ISENABLER0, ICENABLER0: the PPIs' enables; SGIs are always enabled.
 * - IPRIORITYR0-7: byte or word accesses (bits 7:3 kept).
 * - ITARGETSR0-7: read only, CPU 0.
 * - ICFGR0: read only, SGIs edge-triggered. ICFGR1: stored.
 * - IGROUPR0, ISPENDR0, ICPENDR0, ISACTIVER0, ICACTIVER0, SGIR, CPENDSGIRn,
 *   SPENDSGIRn and the ID registers: read as 0, writes ignored.
 *
 * Any other register reads as 0 and ignores writes, and is reported.
 */

#include <arch/internal.h>

/**
 * @funcname `vgicd_reset`
 *
 * @description
 * Resets the distributor of a VM as at power-on: off, interrupts disabled,
 * priority 0.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `gicd` | `hv_vgicd_t *` | [out] The emulated distributor. |
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
 * 1. Call `memset()` with the arguments `gicd`, `0` and `sizeof(*gicd)`.
 *
 * @calls
 * - `memset()`
 */
void vgicd_reset(hv_vgicd_t *gicd)
{
    (void) memset(gicd, 0, sizeof(*gicd));
}

/**
 * @funcname `vgicd_timer_enabled`
 *
 * @description
 * Tells whether the guest lets the timer interrupt through: its distributor is
 * on and interrupt 27 enabled.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `gicd` | `const hv_vgicd_t *` | [in] The emulated distributor. |
 *
 * @return true if the timer interrupt may come.
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
 *    `gicd->enabled && ((gicd->enable & ((uint32_t) 1U << HV_IRQ_VTIMER)) !=`
 *    `0U)`.
 *
 * @calls
 * None.
 */
bool vgicd_timer_enabled(const hv_vgicd_t *gicd)
{
    return gicd->enabled
           && ((gicd->enable & ((uint32_t) 1U << HV_IRQ_VTIMER)) != 0U);
}

/**
 * @funcname `vgicd_timer_priority`
 *
 * @description
 * Returns the guest's priority of the timer interrupt.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `gicd` | `const hv_vgicd_t *` | [in] The emulated distributor. |
 *
 * @return The priority of interrupt 27.
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
 * 1. Return `gicd->priority[HV_IRQ_VTIMER]`.
 *
 * @calls
 * None.
 */
uint8_t vgicd_timer_priority(const hv_vgicd_t *gicd)
{
    return gicd->priority[HV_IRQ_VTIMER];
}

/**
 * @funcname `in`
 *
 * @description
 * Tells whether an offset is in a range.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `offset` | `uint32_t` | [in] The offset. |
 * | `first` | `uint32_t` | [in] The start of the range. |
 * | `end` | `uint32_t` | [in] The end of the range (excluded). |
 *
 * @return true if first <= offset < end.
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
 * 1. Return `(offset >= first) && (offset < end)`.
 *
 * @calls
 * None.
 */
static bool in(uint32_t offset, uint32_t first, uint32_t end)
{
    return (offset >= first) && (offset < end);
}

/**
 * @funcname `priority_word`
 *
 * @description
 * Returns a word of the priority registers: four priorities, little-endian.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `gicd` | `const hv_vgicd_t *` | [in] The emulated distributor. |
 * | `first` | `uint32_t` | [in] The first interrupt of the word. |
 *
 * @return The word.
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `value` | `uint32_t` | The word |
 * | `i` | `uint32_t` | The byte of the word |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Set `value` to `0U`.
 * 2. Set `i` to `0U`; then while `i` is less than `4U`, repeat (after each
 *    pass: increment `i`):
 *    1. Set the bits `(uint32_t) gicd->priority[first + i] << (8U * i)` in
 *       `value`.
 * 3. Return `value`.
 *
 * @calls
 * None.
 */
static uint32_t priority_word(const hv_vgicd_t *gicd, uint32_t first)
{
    uint32_t value = 0U;

    for (uint32_t i = 0U; i < 4U; i++) {
        value |= (uint32_t) gicd->priority[first + i] << (8U * i);
    }
    return value;
}

/**
 * @funcname `read_word`
 *
 * @description
 * Reads a 32-bit register of the emulated distributor.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `gicd` | `const hv_vgicd_t *` | [in] The emulated distributor. |
 * | `offset` | `uint32_t` | [in] The register offset. |
 * | `value` | `uint32_t *` | [out] Where the value is stored. |
 *
 * @return false if the register isn't emulated (it reads as 0).
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `known` | `bool` | Whether the register is emulated |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Set `known` to `true`.
 * 2. If `offset` is equal to `GICD_CTLR`:
 *    1. Set `*value` to `1U` if `gicd->enabled` is true, otherwise `0U`.
 * 3. Otherwise, if `offset` is equal to `GICD_TYPER`:
 *    1. Set `*value` to `0U`.
 * 4. Otherwise, if `offset` is equal to `GICD_IIDR`:
 *    1. Call `gic_host_iidr()`, and store the result in `*value`.
 * 5. Otherwise, if `offset` is equal to `GICD_ISENABLER0` or `offset` is equal
 *    to `GICD_ICENABLER0`:
 *    1. Set `*value` to `gicd->enable | SGIS`.
 * 6. Otherwise, if `in(offset, GICD_IPRIORITYR0, GICD_IPRIORITYR_END)` is true:
 *    1. Call `priority_word()` with the arguments `gicd` and
 *       `offset - GICD_IPRIORITYR0`, and store the result in `*value`.
 * 7. Otherwise, if `in(offset, GICD_ITARGETSR0, GICD_ITARGETSR_END)` is true:
 *    1. Set `*value` to `TARGETS_CPU0`.
 * 8. Otherwise, if `offset` is equal to `GICD_ICFGR0`:
 *    1. Set `*value` to `ICFGR0_SGIS`.
 * 9. Otherwise, if `offset` is equal to `GICD_ICFGR1`:
 *    1. Set `*value` to `gicd->config_ppi`.
 * 10. Otherwise:
 *     1. Set `*value` to `0U`.
 *     2. Set `known` to
 *        `(offset == GICD_IGROUPR0) || (offset == GICD_ISPENDR0) || (offset ==`
 *        `GICD_ICPENDR0) || (offset == GICD_ISACTIVER0) || (offset ==`
 *        `GICD_ICACTIVER0) || in(offset, GICD_SGI_PEND_FIRST,`
 *        `GICD_SGI_PEND_END) || in(offset, GICD_ID_FIRST, GICD_ID_END)`.
 * 11. Return `known`.
 *
 * @calls
 * - `gic_host_iidr()`
 * - `in()`
 * - `priority_word()`
 */
static bool read_word(const hv_vgicd_t *gicd, uint32_t offset,
                      uint32_t *value)
{
    bool known = true;

    if (offset == GICD_CTLR) {
        *value = gicd->enabled ? 1U : 0U;
    } else if (offset == GICD_TYPER) {
        *value = 0U;
    } else if (offset == GICD_IIDR) {
        *value = gic_host_iidr();
    } else if ((offset == GICD_ISENABLER0) || (offset == GICD_ICENABLER0)) {
        *value = gicd->enable | SGIS;
    } else if (in(offset, GICD_IPRIORITYR0, GICD_IPRIORITYR_END)) {
        *value = priority_word(gicd, offset - GICD_IPRIORITYR0);
    } else if (in(offset, GICD_ITARGETSR0, GICD_ITARGETSR_END)) {
        *value = TARGETS_CPU0;
    } else if (offset == GICD_ICFGR0) {
        *value = ICFGR0_SGIS;
    } else if (offset == GICD_ICFGR1) {
        *value = gicd->config_ppi;
    } else {
        *value = 0U;
        known = (offset == GICD_IGROUPR0) || (offset == GICD_ISPENDR0)
                || (offset == GICD_ICPENDR0) || (offset == GICD_ISACTIVER0)
                || (offset == GICD_ICACTIVER0)
                || in(offset, GICD_SGI_PEND_FIRST, GICD_SGI_PEND_END)
                || in(offset, GICD_ID_FIRST, GICD_ID_END);
    }
    return known;
}

/**
 * @funcname `write_word`
 *
 * @description
 * Writes a 32-bit register of the emulated distributor.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `gicd` | `hv_vgicd_t *` | [in,out] The emulated distributor. |
 * | `offset` | `uint32_t` | [in] The register offset. |
 * | `value` | `uint32_t` | [in] The value. |
 *
 * @return false if the register isn't emulated (the write is ignored).
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `known` | `bool` | Whether the register is emulated |
 * | `i` | `uint32_t` | The byte of a priority word |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Set `known` to `true`.
 * 2. If `offset` is equal to `GICD_CTLR`:
 *    1. Set `gicd->enabled` to `(value & 1U) != 0U`.
 * 3. Otherwise, if `offset` is equal to `GICD_ISENABLER0`:
 *    1. Set the bits `value & ~SGIS` in `gicd->enable`.
 * 4. Otherwise, if `offset` is equal to `GICD_ICENABLER0`:
 *    1. Keep only the bits `~(value & ~SGIS)` of `gicd->enable`.
 * 5. Otherwise, if `in(offset, GICD_IPRIORITYR0, GICD_IPRIORITYR_END)` is true:
 *    1. Set `i` to `0U`; then while `i` is less than `4U`, repeat (after each
 *       pass: increment `i`):
 *       1. Set `gicd->priority[(offset - GICD_IPRIORITYR0) + i]` to
 *          `(uint8_t) ((value >> (8U * i)) & PRIORITY_BITS)`.
 * 6. Otherwise, if `offset` is equal to `GICD_ICFGR1`:
 *    1. Set `gicd->config_ppi` to `value`.
 * 7. Otherwise:
 *    1. Set `known` to
 *       `(offset == GICD_TYPER) || (offset == GICD_IIDR) || in(offset,`
 *       `GICD_ITARGETSR0, GICD_ITARGETSR_END) || (offset == GICD_ICFGR0) ||`
 *       `(offset == GICD_IGROUPR0) || (offset == GICD_ISPENDR0) || (offset ==`
 *       `GICD_ICPENDR0) || (offset == GICD_ISACTIVER0) || (offset ==`
 *       `GICD_ICACTIVER0) || (offset == GICD_SGIR) || in(offset,`
 *       `GICD_SGI_PEND_FIRST, GICD_SGI_PEND_END)`.
 * 8. Return `known`.
 *
 * @calls
 * - `in()`
 */
static bool write_word(hv_vgicd_t *gicd, uint32_t offset, uint32_t value)
{
    bool known = true;

    if (offset == GICD_CTLR) {
        gicd->enabled = (value & 1U) != 0U;
    } else if (offset == GICD_ISENABLER0) {
        gicd->enable |= value & ~SGIS;
    } else if (offset == GICD_ICENABLER0) {
        gicd->enable &= ~(value & ~SGIS);
    } else if (in(offset, GICD_IPRIORITYR0, GICD_IPRIORITYR_END)) {
        for (uint32_t i = 0U; i < 4U; i++) {
            gicd->priority[(offset - GICD_IPRIORITYR0) + i] =
                (uint8_t) ((value >> (8U * i)) & PRIORITY_BITS);
        }
    } else if (offset == GICD_ICFGR1) {
        gicd->config_ppi = value;
    } else {
        // Read-only, or read as 0 and writes ignored.
        known = (offset == GICD_TYPER) || (offset == GICD_IIDR)
                || in(offset, GICD_ITARGETSR0, GICD_ITARGETSR_END)
                || (offset == GICD_ICFGR0) || (offset == GICD_IGROUPR0)
                || (offset == GICD_ISPENDR0) || (offset == GICD_ICPENDR0)
                || (offset == GICD_ISACTIVER0)
                || (offset == GICD_ICACTIVER0) || (offset == GICD_SGIR)
                || in(offset, GICD_SGI_PEND_FIRST, GICD_SGI_PEND_END);
    }
    return known;
}

/**
 * @funcname `vgicd_access`
 *
 * @description
 * Performs an access of the guest to its distributor (an MMIO exit), and gives
 * or takes back the timer interrupt if the access changed whether it may come.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `vm` | `hv_vm_t *` | [in,out] The VM. |
 * | `offset` | `uint32_t` | [in] The offset in the distributor. |
 * | `size` | `uint32_t` | [in] The access size: 4, or 1 in the byte-accessible registers (IPRIORITYR, ITARGETSR, CPENDSGIR, SPENDSGIR). |
 * | `write` | `bool` | [in] true for a write. |
 * | `value` | `uint64_t *` | [out] The value written, or where the value read is stored. |
 *
 * @return false if the access isn't emulated (a read gives 0).
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `gicd` | `hv_vgicd_t *` | The emulated distributor |
 * | `word_offset` | `uint32_t` | The offset of the register's word |
 * | `shift` | `uint32_t` | The position of a byte in the word |
 * | `word` | `uint32_t` | The register's word |
 * | `bytes` | `bool` | Whether the register can be accessed by byte |
 * | `known` | `bool` | Whether the access is emulated |
 * | `read` | `uint32_t` | The value read |
 * | `mask` | `uint32_t` | The bits of the byte written |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Set `gicd` to the address of `vm->arch.gicd`.
 * 2. Set `word_offset` to `offset & ~3U`.
 * 3. Set `shift` to `8U * (offset & 3U)`.
 * 4. Set `word` to `0U`.
 * 5. Set `bytes` to `in(word_offset, GICD_IPRIORITYR0, GICD_IPRIORITYR_END) ||`
 *    `in(word_offset, GICD_ITARGETSR0, GICD_ITARGETSR_END) || in(word_offset,`
 *    `GICD_SGI_PEND_FIRST, GICD_SGI_PEND_END)`.
 * 6. If not ((`size` is equal to `4U` and `shift` is equal to `0U`)) and not
 *    ((`size` is equal to `1U` and `bytes` is true)):
 *    1. Set `*value` to `0U`.
 *    2. Return `false`.
 * 7. Call `read_word()` with the arguments `gicd`, `word_offset` and the
 *    address of `word`, and store the result in `known`.
 * 8. If `write` is false:
 *    1. Set `read` to `word` if `size` is equal to `4U`, otherwise
 *       `((word >> shift) & 0xFFU)`.
 *    2. Set `*value` to `read`.
 * 9. Otherwise, if `size` is equal to `4U`:
 *    1. Call `write_word()` with the arguments `gicd`, `word_offset` and
 *       `(uint32_t) *value`, and store the result in `known`.
 * 10. Otherwise:
 *     1. Set `mask` to `0xFFU << shift`.
 *     2. Set `word` to
 *        `(word & ~mask) | (((uint32_t) *value << shift) & mask)`.
 *     3. Call `write_word()` with the arguments `gicd`, `word_offset` and
 *        `word`, and store the result in `known`.
 * 11. If `write` is true:
 *     1. Call `gic_update()` with the argument `vm`.
 * 12. Return `known`.
 *
 * @calls
 * - `in()`
 * - `read_word()`
 * - `write_word()`
 * - `gic_update()`
 */
bool vgicd_access(hv_vm_t *vm, uint32_t offset, uint32_t size, bool write,
                  uint64_t *value)
{
    hv_vgicd_t *gicd = &vm->arch.gicd;
    uint32_t word_offset = offset & ~3U;
    uint32_t shift = 8U * (offset & 3U);
    uint32_t word = 0U;
    bool bytes = in(word_offset, GICD_IPRIORITYR0, GICD_IPRIORITYR_END)
                 || in(word_offset, GICD_ITARGETSR0, GICD_ITARGETSR_END)
                 || in(word_offset, GICD_SGI_PEND_FIRST, GICD_SGI_PEND_END);
    bool known;

    if (!((size == 4U) && (shift == 0U)) && !((size == 1U) && bytes)) {
        *value = 0U;
        return false;
    }
    known = read_word(gicd, word_offset, &word);
    if (!write) {
        uint32_t read = (size == 4U) ? word : ((word >> shift) & 0xFFU);

        *value = read;
    } else if (size == 4U) {
        known = write_word(gicd, word_offset, (uint32_t) *value);
    } else {
        // A byte: the other bytes of the word are written as they are.
        uint32_t mask = 0xFFU << shift;

        word = (word & ~mask) | (((uint32_t) *value << shift) & mask);
        known = write_word(gicd, word_offset, word);
    }
    if (write) {
        gic_update(vm);
    }
    return known;
}
