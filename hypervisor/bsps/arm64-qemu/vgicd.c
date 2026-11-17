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
 *
 * Modified by Kirill Brazhnikov, 2026-11-16: high and low level requirements of
 * the functions, traced to the system requirements (docs/SRQs), and the steps
 * of their design linked to the LLRs.
 *
 * Modified by Kirill Brazhnikov, 2026-11-17: the low level requirements
 * name the register and range helpers that the functions call; the high
 * level requirements trace to the system requirements split into
 * requirements of one "shall" each.
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
 * - \hlrid{HLR_VGICD_VGICD_RESET_01}: `vgicd_reset()` shall reset the emulated
 *   distributor of a VM as at power-on: off, interrupts disabled, priorities 0.
 *   <br>Traces to:
 *   - \srqref{SRQ_HV_INT_003}
 *   - \srqref{SRQ_HV_VM_004}
 *
 * @llr
 * - \llrid{LLR_VGICD_VGICD_RESET_01}: `vgicd_reset()` shall set every byte of
 *   the distributor `gicd` to 0 with `memset()`.
 *   <br>Traces to:
 *   - \hlrref{HLR_VGICD_VGICD_RESET_01}
 *
 * @design
 * 1. Call `memset()` with the arguments `gicd`, `0` and `sizeof(*gicd)`.
 *    \llrref{LLR_VGICD_VGICD_RESET_01}
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
 * - \hlrid{HLR_VGICD_VGICD_TIMER_ENABLED_01}: `vgicd_timer_enabled()` shall
 *   tell whether the guest lets the interrupt of its timer through: its
 *   distributor on and interrupt 27 enabled.
 *   <br>Traces to:
 *   - \srqref{SRQ_HV_INT_004}
 *   - \srqref{SRQ_HV_INT_009}
 *
 * @llr
 * - \llrid{LLR_VGICD_VGICD_TIMER_ENABLED_01}: If `enabled` of the distributor
 *   is true and bit `HV_IRQ_VTIMER` of `enable` is set, `vgicd_timer_enabled()`
 *   shall return true.
 *   <br>Traces to:
 *   - \hlrref{HLR_VGICD_VGICD_TIMER_ENABLED_01}
 * - \llrid{LLR_VGICD_VGICD_TIMER_ENABLED_02}: If `enabled` of the distributor
 *   is false, or bit `HV_IRQ_VTIMER` of `enable` is not set,
 *   `vgicd_timer_enabled()` shall return false.
 *   <br>Traces to:
 *   - \hlrref{HLR_VGICD_VGICD_TIMER_ENABLED_01}
 *
 * @design
 * 1. Return
 *    `gicd->enabled && ((gicd->enable & ((uint32_t) 1U << HV_IRQ_VTIMER)) !=`
 *    `0U)`. \llrref{LLR_VGICD_VGICD_TIMER_ENABLED_01},
 *    \llrref{LLR_VGICD_VGICD_TIMER_ENABLED_02}
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
 * - \hlrid{HLR_VGICD_VGICD_TIMER_PRIORITY_01}: `vgicd_timer_priority()` shall
 *   give the priority that the guest gave the interrupt of its timer.
 *   <br>Traces to:
 *   - \srqref{SRQ_HV_INT_004}
 *
 * @llr
 * - \llrid{LLR_VGICD_VGICD_TIMER_PRIORITY_01}: `vgicd_timer_priority()` shall
 *   return the priority of `HV_IRQ_VTIMER`.
 *   <br>Traces to:
 *   - \hlrref{HLR_VGICD_VGICD_TIMER_PRIORITY_01}
 *
 * @design
 * 1. Return `gicd->priority[HV_IRQ_VTIMER]`.
 *    \llrref{LLR_VGICD_VGICD_TIMER_PRIORITY_01}
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
 * - \hlrid{HLR_VGICD_IN_01}: `in()` shall tell whether an offset is in a range.
 *   <br>Traces to:
 *   - \srqref{SRQ_HV_INT_003}
 *
 * @llr
 * - \llrid{LLR_VGICD_IN_01}: If `offset` is `first` or more and is less than
 *   `end`, `in()` shall return true.
 *   <br>Traces to:
 *   - \hlrref{HLR_VGICD_IN_01}
 * - \llrid{LLR_VGICD_IN_02}: If `offset` is less than `first`, or is `end` or
 *   more, `in()` shall return false.
 *   <br>Traces to:
 *   - \hlrref{HLR_VGICD_IN_01}
 *
 * @design
 * 1. Return `(offset >= first) && (offset < end)`. \llrref{LLR_VGICD_IN_01},
 *    \llrref{LLR_VGICD_IN_02}
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
 * - \hlrid{HLR_VGICD_PRIORITY_WORD_01}: `priority_word()` shall give a word of
 *   the priority registers: four priorities, the first in the low byte.
 *   <br>Traces to:
 *   - \srqref{SRQ_HV_INT_003}
 *
 * @llr
 * - \llrid{LLR_VGICD_PRIORITY_WORD_01}: `priority_word()` shall return the
 *   32-bit value whose byte `i`, for `i` from 0 to 3, is `priority[first + i]`
 *   of the distributor, byte 0 being the low byte.
 *   <br>Traces to:
 *   - \hlrref{HLR_VGICD_PRIORITY_WORD_01}
 *
 * @design
 * 1. Set `value` to `0U`. \llrref{LLR_VGICD_PRIORITY_WORD_01}
 * 2. Set `i` to `0U`; then while `i` is less than `4U`, repeat (after each
 *    pass: increment `i`): \llrref{LLR_VGICD_PRIORITY_WORD_01}
 *    1. Set the bits `(uint32_t) gicd->priority[first + i] << (8U * i)` in
 *       `value`.
 * 3. Return `value`. \llrref{LLR_VGICD_PRIORITY_WORD_01}
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
 * - \hlrid{HLR_VGICD_READ_WORD_01}: `read_word()` shall give a register of the
 *   emulated distributor for the interrupts 0 to 31 and one CPU, and tell
 *   whether it is emulated (others read as zero).
 *   <br>Traces to:
 *   - \srqref{SRQ_HV_INT_003}
 *
 * @llr
 * - \llrid{LLR_VGICD_READ_WORD_01}: If `offset` is `GICD_CTLR`, `read_word()`
 *   shall store 1 at `value` if `enabled` of the distributor is true, and 0 if
 *   it is false.
 *   <br>Traces to:
 *   - \hlrref{HLR_VGICD_READ_WORD_01}
 * - \llrid{LLR_VGICD_READ_WORD_02}: If `offset` is `GICD_TYPER`, `read_word()`
 *   shall store 0 at `value`.
 *   <br>Traces to:
 *   - \hlrref{HLR_VGICD_READ_WORD_01}
 * - \llrid{LLR_VGICD_READ_WORD_03}: If `offset` is `GICD_IIDR`, `read_word()`
 *   shall store the result of `gic_host_iidr()` at `value`.
 *   <br>Traces to:
 *   - \hlrref{HLR_VGICD_READ_WORD_01}
 * - \llrid{LLR_VGICD_READ_WORD_04}: If `offset` is `GICD_ISENABLER0` or
 *   `GICD_ICENABLER0`, `read_word()` shall store `enable` of the distributor
 *   with the bits `SGIS` set at `value`.
 *   <br>Traces to:
 *   - \hlrref{HLR_VGICD_READ_WORD_01}
 * - \llrid{LLR_VGICD_READ_WORD_05}: If `offset` is from `GICD_IPRIORITYR0` to
 *   below `GICD_IPRIORITYR_END` (`in()`), `read_word()` shall store the result
 *   of `priority_word()` for the distributor and `offset` minus
 *   `GICD_IPRIORITYR0` at `value`.
 *   <br>Traces to:
 *   - \hlrref{HLR_VGICD_READ_WORD_01}
 * - \llrid{LLR_VGICD_READ_WORD_06}: If `offset` is from `GICD_ITARGETSR0` to
 *   below `GICD_ITARGETSR_END`, `read_word()` shall store `TARGETS_CPU0` at
 *   `value`.
 *   <br>Traces to:
 *   - \hlrref{HLR_VGICD_READ_WORD_01}
 * - \llrid{LLR_VGICD_READ_WORD_07}: If `offset` is `GICD_ICFGR0`, `read_word()`
 *   shall store `ICFGR0_SGIS` at `value`.
 *   <br>Traces to:
 *   - \hlrref{HLR_VGICD_READ_WORD_01}
 * - \llrid{LLR_VGICD_READ_WORD_08}: If `offset` is `GICD_ICFGR1`, `read_word()`
 *   shall store `config_ppi` of the distributor at `value`.
 *   <br>Traces to:
 *   - \hlrref{HLR_VGICD_READ_WORD_01}
 * - \llrid{LLR_VGICD_READ_WORD_09}: If `offset` is none of `GICD_CTLR`,
 *   `GICD_TYPER`, `GICD_IIDR`, `GICD_ISENABLER0`, `GICD_ICENABLER0`,
 *   `GICD_ICFGR0` and `GICD_ICFGR1`, and is neither from `GICD_IPRIORITYR0` to
 *   below `GICD_IPRIORITYR_END` nor from `GICD_ITARGETSR0` to below
 *   `GICD_ITARGETSR_END`, `read_word()` shall store 0 at `value`.
 *   <br>Traces to:
 *   - \hlrref{HLR_VGICD_READ_WORD_01}
 * - \llrid{LLR_VGICD_READ_WORD_10}: If `offset` is `GICD_CTLR`, `GICD_TYPER`,
 *   `GICD_IIDR`, `GICD_ISENABLER0`, `GICD_ICENABLER0`, `GICD_ICFGR0`,
 *   `GICD_ICFGR1`, `GICD_IGROUPR0`, `GICD_ISPENDR0`, `GICD_ICPENDR0`,
 *   `GICD_ISACTIVER0` or `GICD_ICACTIVER0`, or is from `GICD_IPRIORITYR0` to
 *   below `GICD_IPRIORITYR_END`, from `GICD_ITARGETSR0` to below
 *   `GICD_ITARGETSR_END`, from `GICD_SGI_PEND_FIRST` to below
 *   `GICD_SGI_PEND_END` or from `GICD_ID_FIRST` to below `GICD_ID_END`,
 *   `read_word()` shall return true.
 *   <br>Traces to:
 *   - \hlrref{HLR_VGICD_READ_WORD_01}
 * - \llrid{LLR_VGICD_READ_WORD_11}: If `offset` is none of `GICD_CTLR`,
 *   `GICD_TYPER`, `GICD_IIDR`, `GICD_ISENABLER0`, `GICD_ICENABLER0`,
 *   `GICD_ICFGR0`, `GICD_ICFGR1`, `GICD_IGROUPR0`, `GICD_ISPENDR0`,
 *   `GICD_ICPENDR0`, `GICD_ISACTIVER0` and `GICD_ICACTIVER0`, and is in none of
 *   the ranges from `GICD_IPRIORITYR0` to below `GICD_IPRIORITYR_END`, from
 *   `GICD_ITARGETSR0` to below `GICD_ITARGETSR_END`, from `GICD_SGI_PEND_FIRST`
 *   to below `GICD_SGI_PEND_END` and from `GICD_ID_FIRST` to below
 *   `GICD_ID_END`, `read_word()` shall return false.
 *   <br>Traces to:
 *   - \hlrref{HLR_VGICD_READ_WORD_01}
 *
 * @design
 * 1. Set `known` to `true`. \llrref{LLR_VGICD_READ_WORD_10}
 * 2. If `offset` is equal to `GICD_CTLR`: \llrref{LLR_VGICD_READ_WORD_01}
 *    1. Set `*value` to `1U` if `gicd->enabled` is true, otherwise `0U`.
 * 3. Otherwise, if `offset` is equal to `GICD_TYPER`:
 *    \llrref{LLR_VGICD_READ_WORD_02}
 *    1. Set `*value` to `0U`.
 * 4. Otherwise, if `offset` is equal to `GICD_IIDR`:
 *    \llrref{LLR_VGICD_READ_WORD_03}
 *    1. Call `gic_host_iidr()`, and store the result in `*value`.
 * 5. Otherwise, if `offset` is equal to `GICD_ISENABLER0` or `offset` is equal
 *    to `GICD_ICENABLER0`: \llrref{LLR_VGICD_READ_WORD_04}
 *    1. Set `*value` to `gicd->enable | SGIS`.
 * 6. Otherwise, if `in(offset, GICD_IPRIORITYR0, GICD_IPRIORITYR_END)` is true:
 *    \llrref{LLR_VGICD_READ_WORD_05}
 *    1. Call `priority_word()` with the arguments `gicd` and
 *       `offset - GICD_IPRIORITYR0`, and store the result in `*value`.
 * 7. Otherwise, if `in(offset, GICD_ITARGETSR0, GICD_ITARGETSR_END)` is true:
 *    \llrref{LLR_VGICD_READ_WORD_06}
 *    1. Set `*value` to `TARGETS_CPU0`.
 * 8. Otherwise, if `offset` is equal to `GICD_ICFGR0`:
 *    \llrref{LLR_VGICD_READ_WORD_07}
 *    1. Set `*value` to `ICFGR0_SGIS`.
 * 9. Otherwise, if `offset` is equal to `GICD_ICFGR1`:
 *    \llrref{LLR_VGICD_READ_WORD_08}
 *    1. Set `*value` to `gicd->config_ppi`.
 * 10. Otherwise: \llrref{LLR_VGICD_READ_WORD_09},
 *     \llrref{LLR_VGICD_READ_WORD_10}, \llrref{LLR_VGICD_READ_WORD_11}
 *     1. Set `*value` to `0U`. \llrref{LLR_VGICD_READ_WORD_09}
 *     2. Set `known` to
 *        `(offset == GICD_IGROUPR0) || (offset == GICD_ISPENDR0) || (offset ==`
 *        `GICD_ICPENDR0) || (offset == GICD_ISACTIVER0) || (offset ==`
 *        `GICD_ICACTIVER0) || in(offset, GICD_SGI_PEND_FIRST,`
 *        `GICD_SGI_PEND_END) || in(offset, GICD_ID_FIRST, GICD_ID_END)`.
 *        \llrref{LLR_VGICD_READ_WORD_10}, \llrref{LLR_VGICD_READ_WORD_11}
 * 11. Return `known`. \llrref{LLR_VGICD_READ_WORD_10},
 *     \llrref{LLR_VGICD_READ_WORD_11}
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
 * - \hlrid{HLR_VGICD_WRITE_WORD_01}: `write_word()` shall write a register of
 *   the emulated distributor, and tell whether it is emulated (others ignore
 *   writes).
 *   <br>Traces to:
 *   - \srqref{SRQ_HV_INT_003}
 *
 * @llr
 * - \llrid{LLR_VGICD_WRITE_WORD_01}: If `offset` is `GICD_CTLR`, `write_word()`
 *   shall set `enabled` of the distributor to true if bit 0 of `value` is set,
 *   and to false if it is not.
 *   <br>Traces to:
 *   - \hlrref{HLR_VGICD_WRITE_WORD_01}
 * - \llrid{LLR_VGICD_WRITE_WORD_02}: If `offset` is `GICD_ISENABLER0`,
 *   `write_word()` shall set in `enable` of the distributor the bits of `value`
 *   that are not in `SGIS`.
 *   <br>Traces to:
 *   - \hlrref{HLR_VGICD_WRITE_WORD_01}
 * - \llrid{LLR_VGICD_WRITE_WORD_03}: If `offset` is `GICD_ICENABLER0`,
 *   `write_word()` shall clear in `enable` of the distributor the bits of
 *   `value` that are not in `SGIS`.
 *   <br>Traces to:
 *   - \hlrref{HLR_VGICD_WRITE_WORD_01}
 * - \llrid{LLR_VGICD_WRITE_WORD_04}: If `offset` is from `GICD_IPRIORITYR0` to
 *   below `GICD_IPRIORITYR_END` (`in()`), `write_word()` shall set
 *   `priority[(offset - GICD_IPRIORITYR0) + i]` of the distributor, for `i`
 *   from 0 to 3, to the bits `PRIORITY_BITS` of byte `i` of `value`.
 *   <br>Traces to:
 *   - \hlrref{HLR_VGICD_WRITE_WORD_01}
 * - \llrid{LLR_VGICD_WRITE_WORD_05}: If `offset` is `GICD_ICFGR1`,
 *   `write_word()` shall set `config_ppi` of the distributor to `value`.
 *   <br>Traces to:
 *   - \hlrref{HLR_VGICD_WRITE_WORD_01}
 * - \llrid{LLR_VGICD_WRITE_WORD_06}: If `offset` is none of `GICD_CTLR`,
 *   `GICD_ISENABLER0`, `GICD_ICENABLER0` and `GICD_ICFGR1`, and is not from
 *   `GICD_IPRIORITYR0` to below `GICD_IPRIORITYR_END`, `write_word()` shall
 *   leave the distributor as it is.
 *   <br>Traces to:
 *   - \hlrref{HLR_VGICD_WRITE_WORD_01}
 * - \llrid{LLR_VGICD_WRITE_WORD_07}: If `offset` is `GICD_CTLR`,
 *   `GICD_ISENABLER0`, `GICD_ICENABLER0`, `GICD_ICFGR1`, `GICD_TYPER`,
 *   `GICD_IIDR`, `GICD_ICFGR0`, `GICD_IGROUPR0`, `GICD_ISPENDR0`,
 *   `GICD_ICPENDR0`, `GICD_ISACTIVER0`, `GICD_ICACTIVER0` or `GICD_SGIR`, or is
 *   from `GICD_IPRIORITYR0` to below `GICD_IPRIORITYR_END`, from
 *   `GICD_ITARGETSR0` to below `GICD_ITARGETSR_END` or from
 *   `GICD_SGI_PEND_FIRST` to below `GICD_SGI_PEND_END`, `write_word()` shall
 *   return true.
 *   <br>Traces to:
 *   - \hlrref{HLR_VGICD_WRITE_WORD_01}
 * - \llrid{LLR_VGICD_WRITE_WORD_08}: If `offset` is none of `GICD_CTLR`,
 *   `GICD_ISENABLER0`, `GICD_ICENABLER0`, `GICD_ICFGR1`, `GICD_TYPER`,
 *   `GICD_IIDR`, `GICD_ICFGR0`, `GICD_IGROUPR0`, `GICD_ISPENDR0`,
 *   `GICD_ICPENDR0`, `GICD_ISACTIVER0`, `GICD_ICACTIVER0` and `GICD_SGIR`, and
 *   is in none of the ranges from `GICD_IPRIORITYR0` to below
 *   `GICD_IPRIORITYR_END`, from `GICD_ITARGETSR0` to below `GICD_ITARGETSR_END`
 *   and from `GICD_SGI_PEND_FIRST` to below `GICD_SGI_PEND_END`, `write_word()`
 *   shall return false.
 *   <br>Traces to:
 *   - \hlrref{HLR_VGICD_WRITE_WORD_01}
 *
 * @design
 * 1. Set `known` to `true`. \llrref{LLR_VGICD_WRITE_WORD_07}
 * 2. If `offset` is equal to `GICD_CTLR`: \llrref{LLR_VGICD_WRITE_WORD_01}
 *    1. Set `gicd->enabled` to `(value & 1U) != 0U`.
 * 3. Otherwise, if `offset` is equal to `GICD_ISENABLER0`:
 *    \llrref{LLR_VGICD_WRITE_WORD_02}
 *    1. Set the bits `value & ~SGIS` in `gicd->enable`.
 * 4. Otherwise, if `offset` is equal to `GICD_ICENABLER0`:
 *    \llrref{LLR_VGICD_WRITE_WORD_03}
 *    1. Keep only the bits `~(value & ~SGIS)` of `gicd->enable`.
 * 5. Otherwise, if `in(offset, GICD_IPRIORITYR0, GICD_IPRIORITYR_END)` is true:
 *    \llrref{LLR_VGICD_WRITE_WORD_04}
 *    1. Set `i` to `0U`; then while `i` is less than `4U`, repeat (after each
 *       pass: increment `i`):
 *       1. Set `gicd->priority[(offset - GICD_IPRIORITYR0) + i]` to
 *          `(uint8_t) ((value >> (8U * i)) & PRIORITY_BITS)`.
 * 6. Otherwise, if `offset` is equal to `GICD_ICFGR1`:
 *    \llrref{LLR_VGICD_WRITE_WORD_05}
 *    1. Set `gicd->config_ppi` to `value`.
 * 7. Otherwise: \llrref{LLR_VGICD_WRITE_WORD_06},
 *    \llrref{LLR_VGICD_WRITE_WORD_07}, \llrref{LLR_VGICD_WRITE_WORD_08}
 *    1. Set `known` to
 *       `(offset == GICD_TYPER) || (offset == GICD_IIDR) || in(offset,`
 *       `GICD_ITARGETSR0, GICD_ITARGETSR_END) || (offset == GICD_ICFGR0) ||`
 *       `(offset == GICD_IGROUPR0) || (offset == GICD_ISPENDR0) || (offset ==`
 *       `GICD_ICPENDR0) || (offset == GICD_ISACTIVER0) || (offset ==`
 *       `GICD_ICACTIVER0) || (offset == GICD_SGIR) || in(offset,`
 *       `GICD_SGI_PEND_FIRST, GICD_SGI_PEND_END)`.
 *       \llrref{LLR_VGICD_WRITE_WORD_07}, \llrref{LLR_VGICD_WRITE_WORD_08}
 * 8. Return `known`. \llrref{LLR_VGICD_WRITE_WORD_07},
 *    \llrref{LLR_VGICD_WRITE_WORD_08}
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
 * - \hlrid{HLR_VGICD_VGICD_ACCESS_01}: `vgicd_access()` shall perform an access
 *   of a guest to its distributor, of 4 bytes or of 1 byte to the byte
 *   registers, and give or take back the interrupt of the timer if it changed
 *   whether it may come.
 *   <br>Traces to:
 *   - \srqref{SRQ_HV_INT_003}
 *   - \srqref{SRQ_HV_DEV_004}
 *
 * @llr
 * - \llrid{LLR_VGICD_VGICD_ACCESS_01}: If `size` is 4 and `offset` is not a
 *   multiple of 4, or `size` is 1 and `offset` with its low 2 bits cleared is
 *   in none of the ranges from `GICD_IPRIORITYR0` to below
 *   `GICD_IPRIORITYR_END` (`in()`), from `GICD_ITARGETSR0` to below
 *   `GICD_ITARGETSR_END` and from `GICD_SGI_PEND_FIRST` to below
 *   `GICD_SGI_PEND_END`, or `size` is neither 4 nor 1, `vgicd_access()` shall
 *   store 0 at `value` and return false, without calling `read_word()`,
 *   `write_word()` or `gic_update()`.
 *   <br>Traces to:
 *   - \hlrref{HLR_VGICD_VGICD_ACCESS_01}
 * - \llrid{LLR_VGICD_VGICD_ACCESS_02}: If `size` is 4, `offset` is a multiple
 *   of 4 and `write` is false, `vgicd_access()` shall store at `value` the word
 *   that `read_word()` gives for the distributor of the VM and `offset`, and
 *   return the result of `read_word()`.
 *   <br>Traces to:
 *   - \hlrref{HLR_VGICD_VGICD_ACCESS_01}
 * - \llrid{LLR_VGICD_VGICD_ACCESS_03}: If `size` is 1, `offset` with its low 2
 *   bits cleared is from `GICD_IPRIORITYR0` to below `GICD_IPRIORITYR_END`,
 *   from `GICD_ITARGETSR0` to below `GICD_ITARGETSR_END` or from
 *   `GICD_SGI_PEND_FIRST` to below `GICD_SGI_PEND_END`, and `write` is false,
 *   `vgicd_access()` shall store at `value` the byte `offset & 3` of the word
 *   that `read_word()` gives for the distributor of the VM and `offset` with
 *   its low 2 bits cleared, and return the result of `read_word()`.
 *   <br>Traces to:
 *   - \hlrref{HLR_VGICD_VGICD_ACCESS_01}
 * - \llrid{LLR_VGICD_VGICD_ACCESS_04}: If `size` is 4, `offset` is a multiple
 *   of 4 and `write` is true, `vgicd_access()` shall call `write_word()` once
 *   with the distributor of the VM, `offset` and the low 32 bits of the value
 *   at `value`, and return its result.
 *   <br>Traces to:
 *   - \hlrref{HLR_VGICD_VGICD_ACCESS_01}
 * - \llrid{LLR_VGICD_VGICD_ACCESS_05}: If `size` is 1, `offset` with its low 2
 *   bits cleared is from `GICD_IPRIORITYR0` to below `GICD_IPRIORITYR_END`,
 *   from `GICD_ITARGETSR0` to below `GICD_ITARGETSR_END` or from
 *   `GICD_SGI_PEND_FIRST` to below `GICD_SGI_PEND_END`, and `write` is true,
 *   `vgicd_access()` shall call `write_word()` once with the distributor of the
 *   VM, `offset` with its low 2 bits cleared, and the word that `read_word()`
 *   gives for it with its byte `offset & 3` replaced by the low 8 bits of the
 *   value at `value`, and return its result.
 *   <br>Traces to:
 *   - \hlrref{HLR_VGICD_VGICD_ACCESS_01}
 * - \llrid{LLR_VGICD_VGICD_ACCESS_06}: If and only if `write` is true, and
 *   `size` is 4 and `offset` is a multiple of 4, or `size` is 1 and `offset`
 *   with its low 2 bits cleared is from `GICD_IPRIORITYR0` to below
 *   `GICD_IPRIORITYR_END`, from `GICD_ITARGETSR0` to below `GICD_ITARGETSR_END`
 *   or from `GICD_SGI_PEND_FIRST` to below `GICD_SGI_PEND_END`,
 *   `vgicd_access()` shall call `gic_update()` once with `vm`, after
 *   `write_word()`.
 *   <br>Traces to:
 *   - \hlrref{HLR_VGICD_VGICD_ACCESS_01}
 *
 * @design
 * 1. Set `gicd` to the address of `vm->arch.gicd`.
 *    \llrref{LLR_VGICD_VGICD_ACCESS_02}, \llrref{LLR_VGICD_VGICD_ACCESS_03},
 *    \llrref{LLR_VGICD_VGICD_ACCESS_04}, \llrref{LLR_VGICD_VGICD_ACCESS_05}
 * 2. Set `word_offset` to `offset & ~3U`. \llrref{LLR_VGICD_VGICD_ACCESS_01},
 *    \llrref{LLR_VGICD_VGICD_ACCESS_02}, \llrref{LLR_VGICD_VGICD_ACCESS_03},
 *    \llrref{LLR_VGICD_VGICD_ACCESS_04}, \llrref{LLR_VGICD_VGICD_ACCESS_05}
 * 3. Set `shift` to `8U * (offset & 3U)`. \llrref{LLR_VGICD_VGICD_ACCESS_01},
 *    \llrref{LLR_VGICD_VGICD_ACCESS_03}, \llrref{LLR_VGICD_VGICD_ACCESS_05}
 * 4. Set `word` to `0U`. \llrref{LLR_VGICD_VGICD_ACCESS_02},
 *    \llrref{LLR_VGICD_VGICD_ACCESS_03}, \llrref{LLR_VGICD_VGICD_ACCESS_05}
 * 5. Set `bytes` to `in(word_offset, GICD_IPRIORITYR0, GICD_IPRIORITYR_END) ||`
 *    `in(word_offset, GICD_ITARGETSR0, GICD_ITARGETSR_END) || in(word_offset,`
 *    `GICD_SGI_PEND_FIRST, GICD_SGI_PEND_END)`.
 *    \llrref{LLR_VGICD_VGICD_ACCESS_01}, \llrref{LLR_VGICD_VGICD_ACCESS_03},
 *    \llrref{LLR_VGICD_VGICD_ACCESS_05}, \llrref{LLR_VGICD_VGICD_ACCESS_06}
 * 6. If not ((`size` is equal to `4U` and `shift` is equal to `0U`)) and not
 *    ((`size` is equal to `1U` and `bytes` is true)):
 *    \llrref{LLR_VGICD_VGICD_ACCESS_01}
 *    1. Set `*value` to `0U`.
 *    2. Return `false`.
 * 7. Call `read_word()` with the arguments `gicd`, `word_offset` and the
 *    address of `word`, and store the result in `known`.
 *    \llrref{LLR_VGICD_VGICD_ACCESS_02}, \llrref{LLR_VGICD_VGICD_ACCESS_03},
 *    \llrref{LLR_VGICD_VGICD_ACCESS_04}, \llrref{LLR_VGICD_VGICD_ACCESS_05}
 * 8. If `write` is false: \llrref{LLR_VGICD_VGICD_ACCESS_02},
 *    \llrref{LLR_VGICD_VGICD_ACCESS_03}
 *    1. Set `read` to `word` if `size` is equal to `4U`, otherwise
 *       `((word >> shift) & 0xFFU)`.
 *    2. Set `*value` to `read`.
 * 9. Otherwise, if `size` is equal to `4U`: \llrref{LLR_VGICD_VGICD_ACCESS_04}
 *    1. Call `write_word()` with the arguments `gicd`, `word_offset` and
 *       `(uint32_t) *value`, and store the result in `known`.
 * 10. Otherwise: \llrref{LLR_VGICD_VGICD_ACCESS_05}
 *     1. Set `mask` to `0xFFU << shift`.
 *     2. Set `word` to
 *        `(word & ~mask) | (((uint32_t) *value << shift) & mask)`.
 *     3. Call `write_word()` with the arguments `gicd`, `word_offset` and
 *        `word`, and store the result in `known`.
 * 11. If `write` is true: \llrref{LLR_VGICD_VGICD_ACCESS_06}
 *     1. Call `gic_update()` with the argument `vm`.
 * 12. Return `known`. \llrref{LLR_VGICD_VGICD_ACCESS_02},
 *     \llrref{LLR_VGICD_VGICD_ACCESS_03}, \llrref{LLR_VGICD_VGICD_ACCESS_04},
 *     \llrref{LLR_VGICD_VGICD_ACCESS_05}
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
