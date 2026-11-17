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
 * hypervisor/arch/arm64/gic.c (the code of the board, its devices and the
 * virtual devices of the VMs is in hypervisor/bsps). GICD_IPRIORITYR() takes
 * the interrupt number (the definition shared with the kernel).
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
 * The host's GICv2 and its virtualization extensions.
 *
 * Each CPU takes its physical interrupts at EL2 (HCR_EL2.IMO). Two are
 * enabled: PPI 11 (interrupt 27), the virtual timer, which belongs to the
 * guest of the CPU, and PPI 9 (interrupt 25), the GIC's maintenance
 * interrupt. The CPU interface splits the end of interrupt (EOImode): the
 * hypervisor drops the priority of interrupt 27 but leaves it active, and
 * gives the tick to the guest in list register 0 with an end of interrupt
 * request. The guest's end of interrupt (through its GICC, mapped to the
 * GICV) raises the maintenance interrupt, and the hypervisor then
 * deactivates interrupt 27, which can come again (at once if the timer's
 * line is still high). While the guest has interrupt 27 disabled in its
 * distributor (vgicd.c), the tick stays pending (and the physical interrupt
 * active) until it enables it.
 *
 * The HW bit of the list registers would let the guest's end of interrupt
 * deactivate interrupt 27 itself, without the maintenance interrupt, but
 * QEMU (11.1) doesn't update the physical CPU interface then: a tick that is
 * pending when it is deactivated (the line still high, e.g. after GDB has
 * held the guest) never comes, and the guest's time stands still.
 */

#include <arch/internal.h>

static uint64_t gicd;
static uint64_t gicc;
static uint64_t gich;
static uint64_t gicv;
static uint32_t nb_lrs = 0U;
static uint32_t host_iidr;

/**
 * @funcname `gic_read32`
 *
 * @description
 * Reads a 32-bit GIC register.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `base` | `uint64_t` | [in] The physical address of the register block. |
 * | `offset` | `uint32_t` | [in] The register offset. |
 *
 * @return The register value.
 *
 * @globals
 * None.
 *
 * @locals
 * None.
 *
 * @hlr
 * - \hlrid{HLR_GIC_GIC_READ32_01}: `gic_read32()` shall read a 32-bit register
 *   of the GIC.
 *   <br>Traces to:
 *   - \srqref{SRQ_HV_INT_008}
 *
 * @llr
 * - \llrid{LLR_GIC_GIC_READ32_01}: `gic_read32()` shall return the 32-bit value
 *   at the address `base` plus `offset`, read once.
 *   <br>Traces to:
 *   - \hlrref{HLR_GIC_GIC_READ32_01}
 *
 * @design
 * 1. Return the value at
 *    `(const volatile uint32_t *) (uintptr_t) (base + offset)`.
 *    \llrref{LLR_GIC_GIC_READ32_01}
 *
 * @calls
 * None.
 */
/*
 * MISRA C:2025 Rule 11.4 deviation (the register accessors below): the
 * GIC's registers are used by physical address (identity-mapped),
 * converted to pointers.
 */
static uint32_t gic_read32(uint64_t base, uint32_t offset)
{
    return *(const volatile uint32_t *) (uintptr_t) (base + offset);
}

/**
 * @funcname `gic_write32`
 *
 * @description
 * Writes a 32-bit GIC register.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `base` | `uint64_t` | [in] The physical address of the register block. |
 * | `offset` | `uint32_t` | [in] The register offset. |
 * | `value` | `uint32_t` | [in] The value. |
 *
 * @globals
 * None.
 *
 * @locals
 * None.
 *
 * @hlr
 * - \hlrid{HLR_GIC_GIC_WRITE32_01}: `gic_write32()` shall write a 32-bit
 *   register of the GIC.
 *   <br>Traces to:
 *   - \srqref{SRQ_HV_INT_008}
 *
 * @llr
 * - \llrid{LLR_GIC_GIC_WRITE32_01}: `gic_write32()` shall write the 32-bit
 *   value `value` once at the address `base` plus `offset`.
 *   <br>Traces to:
 *   - \hlrref{HLR_GIC_GIC_WRITE32_01}
 *
 * @design
 * 1. Set `*(volatile uint32_t *) (uintptr_t) (base + offset)` to `value`.
 *    \llrref{LLR_GIC_GIC_WRITE32_01}
 *
 * @calls
 * None.
 */
static void gic_write32(uint64_t base, uint32_t offset, uint32_t value)
{
    *(volatile uint32_t *) (uintptr_t) (base + offset) = value;
}

/**
 * @funcname `gic_write8`
 *
 * @description
 * Writes a byte of a byte-accessible GIC register (a priority).
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `base` | `uint64_t` | [in] The physical address of the register block. |
 * | `offset` | `uint32_t` | [in] The byte offset. |
 * | `value` | `uint8_t` | [in] The value. |
 *
 * @globals
 * None.
 *
 * @locals
 * None.
 *
 * @hlr
 * - \hlrid{HLR_GIC_GIC_WRITE8_01}: `gic_write8()` shall write a byte of a
 *   byte-accessible register of the GIC.
 *   <br>Traces to:
 *   - \srqref{SRQ_HV_INT_008}
 *
 * @llr
 * - \llrid{LLR_GIC_GIC_WRITE8_01}: `gic_write8()` shall write the byte `value`
 *   once at the address `base` plus `offset`.
 *   <br>Traces to:
 *   - \hlrref{HLR_GIC_GIC_WRITE8_01}
 *
 * @design
 * 1. Set `*(volatile uint8_t *) (uintptr_t) (base + offset)` to `value`.
 *    \llrref{LLR_GIC_GIC_WRITE8_01}
 *
 * @calls
 * None.
 */
static void gic_write8(uint64_t base, uint32_t offset, uint8_t value)
{
    *(volatile uint8_t *) (uintptr_t) (base + offset) = value;
}

/**
 * @funcname `gic_init`
 *
 * @description
 * Sets up the distributor on the boot CPU, with the GIC's addresses from the
 * device tree: every shared interrupt disabled.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `info` | `const hv_fdt_info_t *` | [in] The device tree information (GIC register ranges). |
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `gicd` | `uint64_t` | The distributor's address |
 * | `gicc` | `uint64_t` | The CPU interface's address |
 * | `gich` | `uint64_t` | The virtual interface control's address |
 * | `gicv` | `uint64_t` | The virtual CPU interface's address |
 * | `host_iidr` | `uint32_t` | The distributor's identification |
 * | `nb_lrs` | `uint32_t` | The number of list registers (0 before gic_init()) |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `words` | `uint32_t` | The number of 32-interrupt enable registers |
 * | `i` | `uint32_t` | The index of an enable register |
 *
 * @hlr
 * - \hlrid{HLR_GIC_GIC_INIT_01}: `gic_init()` shall set up the distributor of
 *   the GIC at boot, with the addresses of the device tree, every shared
 *   interrupt disabled.
 *   <br>Traces to:
 *   - \srqref{SRQ_HV_INT_005}
 *
 * @llr
 * - \llrid{LLR_GIC_GIC_INIT_01}: `gic_init()` shall set `gicd`, `gicc`, `gich`
 *   and `gicv` to the addresses of the same names of `info`.
 *   <br>Traces to:
 *   - \hlrref{HLR_GIC_GIC_INIT_01}
 * - \llrid{LLR_GIC_GIC_INIT_02}: `gic_init()` shall write to the distributor
 *   with `gic_write32()`, in this order: 0 to `GICD_CTLR`, `0xFFFFFFFF` to
 *   `GICD_ICENABLER(i)` for each `i` from 1 to the value of the bits
 *   `GICD_TYPER_LINES` of the register `GICD_TYPER` (`gic_read32()`), and
 *   `GICD_CTLR_ENABLE` to `GICD_CTLR`.
 *   <br>Traces to:
 *   - \hlrref{HLR_GIC_GIC_INIT_01}
 * - \llrid{LLR_GIC_GIC_INIT_03}: `gic_init()` shall set `host_iidr` to the
 *   register `GICD_IIDR` of the distributor (`gic_read32()`), and `nb_lrs` to
 *   the value of the bits `GICH_VTR_LISTREGS` of the register `GICH_VTR` of the
 *   virtual interface plus 1.
 *   <br>Traces to:
 *   - \hlrref{HLR_GIC_GIC_INIT_01}
 *
 * @design
 * 1. Set `gicd` to `info->gicd`. \llrref{LLR_GIC_GIC_INIT_01}
 * 2. Set `gicc` to `info->gicc`. \llrref{LLR_GIC_GIC_INIT_01}
 * 3. Set `gich` to `info->gich`. \llrref{LLR_GIC_GIC_INIT_01}
 * 4. Set `gicv` to `info->gicv`. \llrref{LLR_GIC_GIC_INIT_01}
 * 5. Call `gic_write32()` with the arguments `gicd`, `GICD_CTLR` and `0U`.
 *    \llrref{LLR_GIC_GIC_INIT_02}
 * 6. Set `words` to `(gic_read32(gicd, GICD_TYPER) & GICD_TYPER_LINES) + 1U`.
 *    \llrref{LLR_GIC_GIC_INIT_02}
 * 7. Set `i` to `1U`; then while `i` is less than `words`, repeat (after each
 *    pass: increment `i`): \llrref{LLR_GIC_GIC_INIT_02}
 *    1. Call `gic_write32()` with the arguments `gicd`, `GICD_ICENABLER(i)` and
 *       `0xFFFFFFFFU`.
 * 8. Call `gic_write32()` with the arguments `gicd`, `GICD_CTLR` and
 *    `GICD_CTLR_ENABLE`. \llrref{LLR_GIC_GIC_INIT_02}
 * 9. Call `gic_read32()` with the arguments `gicd` and `GICD_IIDR`, and store
 *    the result in `host_iidr`. \llrref{LLR_GIC_GIC_INIT_03}
 * 10. Set `nb_lrs` to `(gic_read32(gich, GICH_VTR) & GICH_VTR_LISTREGS) + 1U`.
 *     \llrref{LLR_GIC_GIC_INIT_03}
 *
 * @calls
 * - `gic_write32()`
 * - `gic_read32()`
 */
void gic_init(const hv_fdt_info_t *info)
{
    uint32_t words;

    gicd = info->gicd;
    gicc = info->gicc;
    gich = info->gich;
    gicv = info->gicv;

    gic_write32(gicd, GICD_CTLR, 0U);
    words = (gic_read32(gicd, GICD_TYPER) & GICD_TYPER_LINES) + 1U;
    for (uint32_t i = 1U; i < words; i++) {
        gic_write32(gicd, GICD_ICENABLER(i), 0xFFFFFFFFU);
    }
    gic_write32(gicd, GICD_CTLR, GICD_CTLR_ENABLE);
    host_iidr = gic_read32(gicd, GICD_IIDR);
    nb_lrs = (gic_read32(gich, GICH_VTR) & GICH_VTR_LISTREGS) + 1U;
}

/**
 * @funcname `gic_list_registers`
 *
 * @description
 * Returns the number of list registers of the virtual interface.
 *
 * @return The number of list registers.
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `nb_lrs` | `uint32_t` | The number of list registers (0 before gic_init()) |
 *
 * @locals
 * None.
 *
 * @hlr
 * - \hlrid{HLR_GIC_GIC_LIST_REGISTERS_01}: `gic_list_registers()` shall give
 *   the number of list registers of the virtual interface.
 *   <br>Traces to:
 *   - \srqref{SRQ_HV_BOOT_006}
 *
 * @llr
 * - \llrid{LLR_GIC_GIC_LIST_REGISTERS_01}: `gic_list_registers()` shall return
 *   `nb_lrs`.
 *   <br>Traces to:
 *   - \hlrref{HLR_GIC_GIC_LIST_REGISTERS_01}
 *
 * @design
 * 1. Return `nb_lrs`. \llrref{LLR_GIC_GIC_LIST_REGISTERS_01}
 *
 * @calls
 * None.
 */
uint32_t gic_list_registers(void)
{
    return nb_lrs;
}

/**
 * @funcname `gic_gicv_base`
 *
 * @description
 * Returns the physical address of the virtual CPU interface, which stage 2 maps
 * as the guests' GICC.
 *
 * @return The address of the GICV.
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `gicv` | `uint64_t` | The virtual CPU interface's address |
 *
 * @locals
 * None.
 *
 * @hlr
 * - \hlrid{HLR_GIC_GIC_GICV_BASE_01}: `gic_gicv_base()` shall give the address
 *   of the virtual CPU interface, which the guests use as their CPU interface.
 *   <br>Traces to:
 *   - \srqref{SRQ_HV_INT_008}
 *   - \srqref{SRQ_HV_MEM_003}
 *
 * @llr
 * - \llrid{LLR_GIC_GIC_GICV_BASE_01}: `gic_gicv_base()` shall return `gicv`.
 *   <br>Traces to:
 *   - \hlrref{HLR_GIC_GIC_GICV_BASE_01}
 *
 * @design
 * 1. Return `gicv`. \llrref{LLR_GIC_GIC_GICV_BASE_01}
 *
 * @calls
 * None.
 */
uint64_t gic_gicv_base(void)
{
    return gicv;
}

/**
 * @funcname `gic_host_iidr`
 *
 * @description
 * Returns the distributor's identification, which the guests' distributor reads
 * too.
 *
 * @return GICD_IIDR.
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `host_iidr` | `uint32_t` | The distributor's identification |
 *
 * @locals
 * None.
 *
 * @hlr
 * - \hlrid{HLR_GIC_GIC_HOST_IIDR_01}: `gic_host_iidr()` shall give the
 *   identification of the distributor of the host, which the emulated
 *   distributors show.
 *   <br>Traces to:
 *   - \srqref{SRQ_HV_INT_003}
 *
 * @llr
 * - \llrid{LLR_GIC_GIC_HOST_IIDR_01}: `gic_host_iidr()` shall return
 *   `host_iidr`.
 *   <br>Traces to:
 *   - \hlrref{HLR_GIC_GIC_HOST_IIDR_01}
 *
 * @design
 * 1. Return `host_iidr`. \llrref{LLR_GIC_GIC_HOST_IIDR_01}
 *
 * @calls
 * None.
 */
uint32_t gic_host_iidr(void)
{
    return host_iidr;
}

/**
 * @funcname `gic_init_cpu`
 *
 * @description
 * Sets up this CPU's interface and banked distributor registers: only the
 * virtual timer's and the maintenance interrupts are enabled, end of interrupt
 * and deactivation are split, and the virtual interface is on.
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `gicd` | `uint64_t` | The distributor's address |
 * | `gicc` | `uint64_t` | The CPU interface's address |
 * | `gich` | `uint64_t` | The virtual interface control's address |
 *
 * @locals
 * None.
 *
 * @hlr
 * - \hlrid{HLR_GIC_GIC_INIT_CPU_01}: `gic_init_cpu()` shall set up the CPU
 *   interface of the calling CPU so that it takes only the virtual timer and
 *   the maintenance interrupts, with end of interrupt and deactivation split.
 *   <br>Traces to:
 *   - \srqref{SRQ_HV_INT_005}
 * - \hlrid{HLR_GIC_GIC_INIT_CPU_02}: `gic_init_cpu()` shall enable the virtual
 *   interface for the guest, with its interrupt state reset.
 *   <br>Traces to:
 *   - \srqref{SRQ_HV_INT_008}
 *
 * @llr
 * - \llrid{LLR_GIC_GIC_INIT_CPU_01}: `gic_init_cpu()` shall write to the
 *   distributor, in this order: `0xFFFFFFFF` with `gic_write32()` to
 *   `GICD_ICENABLER(0)`, to `GICD_ICPENDR0` and to `GICD_ICACTIVER0`,
 *   `HOST_PRIORITY` with `gic_write8()` to `GICD_IPRIORITYR(HV_IRQ_VTIMER)` and
 *   to `GICD_IPRIORITYR(IRQ_MAINTENANCE)`, and `HOST_IRQS` with `gic_write32()`
 *   to `GICD_ISENABLER0`.
 *   <br>Traces to:
 *   - \hlrref{HLR_GIC_GIC_INIT_CPU_01}
 * - \llrid{LLR_GIC_GIC_INIT_CPU_02}: `gic_init_cpu()` shall then write to the
 *   CPU interface with `gic_write32()`, in this order: `0xFF` to `GICC_PMR`, 0
 *   to `GICC_BPR` and `GICC_CTLR_ENABLE | GICC_CTLR_EOIMODE` to `GICC_CTLR`.
 *   <br>Traces to:
 *   - \hlrref{HLR_GIC_GIC_INIT_CPU_01}
 * - \llrid{LLR_GIC_GIC_INIT_CPU_03}: `gic_init_cpu()` shall then call
 *   `gic_vcpu_reset()` once and write `GICH_HCR_EN` with `gic_write32()` to
 *   `GICH_HCR` of the virtual interface.
 *   <br>Traces to:
 *   - \hlrref{HLR_GIC_GIC_INIT_CPU_02}
 *
 * @design
 * 1. Call `gic_write32()` with the arguments `gicd`, `GICD_ICENABLER(0U)` and
 *    `0xFFFFFFFFU`. \llrref{LLR_GIC_GIC_INIT_CPU_01}
 * 2. Call `gic_write32()` with the arguments `gicd`, `GICD_ICPENDR0` and
 *    `0xFFFFFFFFU`. \llrref{LLR_GIC_GIC_INIT_CPU_01}
 * 3. Call `gic_write32()` with the arguments `gicd`, `GICD_ICACTIVER0` and
 *    `0xFFFFFFFFU`. \llrref{LLR_GIC_GIC_INIT_CPU_01}
 * 4. Call `gic_write8()` with the arguments `gicd`,
 *    `GICD_IPRIORITYR(HV_IRQ_VTIMER)` and `HOST_PRIORITY`.
 *    \llrref{LLR_GIC_GIC_INIT_CPU_01}
 * 5. Call `gic_write8()` with the arguments `gicd`,
 *    `GICD_IPRIORITYR(IRQ_MAINTENANCE)` and `HOST_PRIORITY`.
 *    \llrref{LLR_GIC_GIC_INIT_CPU_01}
 * 6. Call `gic_write32()` with the arguments `gicd`, `GICD_ISENABLER0` and
 *    `HOST_IRQS`. \llrref{LLR_GIC_GIC_INIT_CPU_01}
 * 7. Call `gic_write32()` with the arguments `gicc`, `GICC_PMR` and `0xFFU`.
 *    \llrref{LLR_GIC_GIC_INIT_CPU_02}
 * 8. Call `gic_write32()` with the arguments `gicc`, `GICC_BPR` and `0U`.
 *    \llrref{LLR_GIC_GIC_INIT_CPU_02}
 * 9. Call `gic_write32()` with the arguments `gicc`, `GICC_CTLR` and
 *    `GICC_CTLR_ENABLE | GICC_CTLR_EOIMODE`. \llrref{LLR_GIC_GIC_INIT_CPU_02}
 * 10. Call `gic_vcpu_reset()`. \llrref{LLR_GIC_GIC_INIT_CPU_03}
 * 11. Call `gic_write32()` with the arguments `gich`, `GICH_HCR` and
 *     `GICH_HCR_EN`. \llrref{LLR_GIC_GIC_INIT_CPU_03}
 *
 * @calls
 * - `gic_write32()`
 * - `gic_write8()`
 * - `gic_vcpu_reset()`
 */
void gic_init_cpu(void)
{
    gic_write32(gicd, GICD_ICENABLER(0U), 0xFFFFFFFFU);
    gic_write32(gicd, GICD_ICPENDR0, 0xFFFFFFFFU);
    gic_write32(gicd, GICD_ICACTIVER0, 0xFFFFFFFFU);
    gic_write8(gicd, GICD_IPRIORITYR(HV_IRQ_VTIMER), HOST_PRIORITY);
    gic_write8(gicd, GICD_IPRIORITYR(IRQ_MAINTENANCE), HOST_PRIORITY);
    gic_write32(gicd, GICD_ISENABLER0, HOST_IRQS);

    gic_write32(gicc, GICC_PMR, 0xFFU);
    gic_write32(gicc, GICC_BPR, 0U);
    gic_write32(gicc, GICC_CTLR, GICC_CTLR_ENABLE | GICC_CTLR_EOIMODE);
    gic_vcpu_reset();
    gic_write32(gich, GICH_HCR, GICH_HCR_EN);
}

/**
 * @funcname `gic_cpu_off`
 *
 * @description
 * Stops this CPU's interrupts (a stopped VM, a parked vCPU, a fatal error).
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `gicc` | `uint64_t` | The CPU interface's address |
 * | `gicd` | `uint64_t` | The distributor's address |
 * | `gich` | `uint64_t` | The virtual interface control's address |
 * | `nb_lrs` | `uint32_t` | The number of list registers (0 before gic_init()) |
 *
 * @locals
 * None.
 *
 * @hlr
 * - \hlrid{HLR_GIC_GIC_CPU_OFF_01}: `gic_cpu_off()` shall stop the interrupts
 *   of the calling CPU.
 *   <br>Traces to:
 *   - \srqref{SRQ_HV_CPU_007}
 *
 * @llr
 * - \llrid{LLR_GIC_GIC_CPU_OFF_01}: If and only if `nb_lrs` is not 0,
 *   `gic_cpu_off()` shall write with `gic_write32()`, in this order: 0 to
 *   `GICH_HCR` of the virtual interface, 0 to `GICC_CTLR` of the CPU interface
 *   and `HOST_IRQS` to `GICD_ICENABLER(0)` of the distributor.
 *   <br>Traces to:
 *   - \hlrref{HLR_GIC_GIC_CPU_OFF_01}
 *
 * @design
 * 1. If `nb_lrs` is not equal to `0U`: \llrref{LLR_GIC_GIC_CPU_OFF_01}
 *    1. Call `gic_write32()` with the arguments `gich`, `GICH_HCR` and `0U`.
 *    2. Call `gic_write32()` with the arguments `gicc`, `GICC_CTLR` and `0U`.
 *    3. Call `gic_write32()` with the arguments `gicd`, `GICD_ICENABLER(0U)`
 *       and `HOST_IRQS`.
 *
 * @calls
 * - `gic_write32()`
 */
void gic_cpu_off(void)
{
    if (nb_lrs != 0U) {
        gic_write32(gich, GICH_HCR, 0U);
        gic_write32(gicc, GICC_CTLR, 0U);
        gic_write32(gicd, GICD_ICENABLER(0U), HOST_IRQS);
    }
}

/**
 * @funcname `gic_vcpu_reset`
 *
 * @description
 * Resets the guest's interrupt state as at power-on: empty list registers, no
 * active priorities, the virtual CPU interface off, and the physical timer and
 * maintenance interrupts neither pending nor active.
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `gicd` | `uint64_t` | The distributor's address |
 * | `gich` | `uint64_t` | The virtual interface control's address |
 * | `nb_lrs` | `uint32_t` | The number of list registers (0 before gic_init()) |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `i` | `uint32_t` | The index of a list register |
 *
 * @hlr
 * - \hlrid{HLR_GIC_GIC_VCPU_RESET_01}: `gic_vcpu_reset()` shall reset the
 *   interrupt state of the guest as at power-on: no interrupt listed or active,
 *   the virtual CPU interface off, and the timer and maintenance interrupts
 *   neither pending nor active.
 *   <br>Traces to:
 *   - \srqref{SRQ_HV_INT_008}
 *   - \srqref{SRQ_HV_VM_004}
 *
 * @llr
 * - \llrid{LLR_GIC_GIC_VCPU_RESET_01}: `gic_vcpu_reset()` shall write 0 with
 *   `gic_write32()` to `GICH_LR(i)` of the virtual interface for each `i` below
 *   `nb_lrs`.
 *   <br>Traces to:
 *   - \hlrref{HLR_GIC_GIC_VCPU_RESET_01}
 * - \llrid{LLR_GIC_GIC_VCPU_RESET_02}: `gic_vcpu_reset()` shall then write with
 *   `gic_write32()` 0 to `GICH_APR` and to `GICH_VMCR` of the virtual
 *   interface, and `HOST_IRQS` to `GICD_ICPENDR0` and to `GICD_ICACTIVER0` of
 *   the distributor.
 *   <br>Traces to:
 *   - \hlrref{HLR_GIC_GIC_VCPU_RESET_01}
 *
 * @design
 * 1. Set `i` to `0U`; then while `i` is less than `nb_lrs`, repeat (after each
 *    pass: increment `i`): \llrref{LLR_GIC_GIC_VCPU_RESET_01}
 *    1. Call `gic_write32()` with the arguments `gich`, `GICH_LR(i)` and `0U`.
 * 2. Call `gic_write32()` with the arguments `gich`, `GICH_APR` and `0U`.
 *    \llrref{LLR_GIC_GIC_VCPU_RESET_02}
 * 3. Call `gic_write32()` with the arguments `gich`, `GICH_VMCR` and `0U`.
 *    \llrref{LLR_GIC_GIC_VCPU_RESET_02}
 * 4. Call `gic_write32()` with the arguments `gicd`, `GICD_ICPENDR0` and
 *    `HOST_IRQS`. \llrref{LLR_GIC_GIC_VCPU_RESET_02}
 * 5. Call `gic_write32()` with the arguments `gicd`, `GICD_ICACTIVER0` and
 *    `HOST_IRQS`. \llrref{LLR_GIC_GIC_VCPU_RESET_02}
 *
 * @calls
 * - `gic_write32()`
 */
void gic_vcpu_reset(void)
{
    for (uint32_t i = 0U; i < nb_lrs; i++) {
        gic_write32(gich, GICH_LR(i), 0U);
    }
    gic_write32(gich, GICH_APR, 0U);
    gic_write32(gich, GICH_VMCR, 0U);
    gic_write32(gicd, GICD_ICPENDR0, HOST_IRQS);
    gic_write32(gicd, GICD_ICACTIVER0, HOST_IRQS);
}

/**
 * @funcname `gic_update`
 *
 * @description
 * Gives the guest its pending timer tick if its distributor lets it have one,
 * or takes back a tick it hasn't taken yet if it has disabled the interrupt.
 * Called after each change of either.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `vm` | `hv_vm_t *` | [in,out] The VM of this CPU. |
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `gich` | `uint64_t` | The virtual interface control's address |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `lr` | `uint32_t` | List register 0 |
 * | `enabled` | `bool` | Whether the guest lets the timer interrupt through |
 * | `priority` | `uint32_t` | The guest's priority of the timer interrupt |
 *
 * @hlr
 * - \hlrid{HLR_GIC_GIC_UPDATE_01}: `gic_update()` shall give the guest its
 *   pending timer interrupt when its distributor lets it have one, with its
 *   priority, and take back an interrupt that it has not taken when it disables
 *   it.
 *   <br>Traces to:
 *   - \srqref{SRQ_HV_INT_004}
 *   - \srqref{SRQ_HV_INT_009}
 *
 * @llr
 * - \llrid{LLR_GIC_GIC_UPDATE_01}: If `timer_pending` of the VM is true,
 *   `vgicd_timer_enabled()` returns true for the distributor of the VM and the
 *   bits `GICH_LR_STATE` of the register `GICH_LR(0)` (`gic_read32()`) are 0,
 *   `gic_update()` shall write (`gic_write32()`) to `GICH_LR(0)` the value
 *   `GICH_LR_PENDING | GICH_LR_EOI | HV_IRQ_VTIMER` with the result of
 *   `vgicd_timer_priority()` shifted right by 3 in the field at
 *   `GICH_LR_PRIORITY`, set `timer_pending` to false and increment `timer_irqs`
 *   of the VM.
 *   <br>Traces to:
 *   - \hlrref{HLR_GIC_GIC_UPDATE_01}
 * - \llrid{LLR_GIC_GIC_UPDATE_02}: If `vgicd_timer_enabled()` returns false for
 *   the distributor of the VM and the bits `GICH_LR_STATE` of the register
 *   `GICH_LR(0)` are `GICH_LR_PENDING`, `gic_update()` shall write 0 to
 *   `GICH_LR(0)` with `gic_write32()` and set `timer_pending` of the VM to
 *   true.
 *   <br>Traces to:
 *   - \hlrref{HLR_GIC_GIC_UPDATE_01}
 * - \llrid{LLR_GIC_GIC_UPDATE_03}: If `vgicd_timer_enabled()` returns true and
 *   `timer_pending` of the VM is false or the bits `GICH_LR_STATE` of
 *   `GICH_LR(0)` are not 0, or `vgicd_timer_enabled()` returns false and the
 *   bits `GICH_LR_STATE` of `GICH_LR(0)` are not `GICH_LR_PENDING`,
 *   `gic_update()` shall leave `GICH_LR(0)`, `timer_pending` and `timer_irqs`
 *   as they are.
 *   <br>Traces to:
 *   - \hlrref{HLR_GIC_GIC_UPDATE_01}
 *
 * @design
 * 1. Call `gic_read32()` with the arguments `gich` and `GICH_LR(0U)`, and store
 *    the result in `lr`. \llrref{LLR_GIC_GIC_UPDATE_01},
 *    \llrref{LLR_GIC_GIC_UPDATE_02}, \llrref{LLR_GIC_GIC_UPDATE_03}
 * 2. Call `vgicd_timer_enabled()` with the address of `vm->arch.gicd`, and
 *    store the result in `enabled`. \llrref{LLR_GIC_GIC_UPDATE_01},
 *    \llrref{LLR_GIC_GIC_UPDATE_02}, \llrref{LLR_GIC_GIC_UPDATE_03}
 * 3. If `vm->arch.timer_pending` is true and `enabled` is true and
 *    `(lr & GICH_LR_STATE)` is equal to `0U`: \llrref{LLR_GIC_GIC_UPDATE_01},
 *    \llrref{LLR_GIC_GIC_UPDATE_03}
 *    1. Call `vgicd_timer_priority()` with the address of `vm->arch.gicd`, and
 *       store the result in `priority`.
 *    2. Call `gic_write32()` with the arguments `gich`, `GICH_LR(0U)` and
 *       `GICH_LR_PENDING | GICH_LR_EOI | ((priority >> 3) << GICH_LR_PRIORITY)`
 *       `| HV_IRQ_VTIMER`.
 *    3. Set `vm->arch.timer_pending` to `false`.
 *    4. Increment `vm->arch.timer_irqs`.
 * 4. Otherwise, if `enabled` is false and `(lr & GICH_LR_STATE)` is equal to
 *    `GICH_LR_PENDING`: \llrref{LLR_GIC_GIC_UPDATE_02},
 *    \llrref{LLR_GIC_GIC_UPDATE_03}
 *    1. Call `gic_write32()` with the arguments `gich`, `GICH_LR(0U)` and `0U`.
 *    2. Set `vm->arch.timer_pending` to `true`.
 *
 * @calls
 * - `gic_read32()`
 * - `vgicd_timer_enabled()`
 * - `vgicd_timer_priority()`
 * - `gic_write32()`
 */
void gic_update(hv_vm_t *vm)
{
    uint32_t lr = gic_read32(gich, GICH_LR(0U));
    bool enabled = vgicd_timer_enabled(&vm->arch.gicd);

    if (vm->arch.timer_pending && enabled && ((lr & GICH_LR_STATE) == 0U)) {
        uint32_t priority = vgicd_timer_priority(&vm->arch.gicd);

        gic_write32(gich, GICH_LR(0U), GICH_LR_PENDING | GICH_LR_EOI
                | ((priority >> 3) << GICH_LR_PRIORITY) | HV_IRQ_VTIMER);
        vm->arch.timer_pending = false;
        vm->arch.timer_irqs++;
    } else if (!enabled && ((lr & GICH_LR_STATE) == GICH_LR_PENDING)) {
        // Not taken: pending again until the guest enables it.
        gic_write32(gich, GICH_LR(0U), 0U);
        vm->arch.timer_pending = true;
    } else {
        // Nothing changes.
    }
}

/**
 * @funcname `maintenance`
 *
 * @description
 * Handles the maintenance interrupt: if the guest has ended its timer interrupt
 * (list register 0 empty, with its end of interrupt request), clears the list
 * register and deactivates the physical timer interrupt, which can then come
 * again.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `vm` | `hv_vm_t *` | [in,out] The VM of this CPU. |
 * | `iar` | `uint32_t` | [in] The maintenance interrupt's acknowledge value. |
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `gich` | `uint64_t` | The virtual interface control's address |
 * | `gicc` | `uint64_t` | The CPU interface's address |
 *
 * @locals
 * None.
 *
 * @hlr
 * - \hlrid{HLR_GIC_MAINTENANCE_01}: `maintenance()` shall end the timer
 *   interrupt of the host when the guest has ended it, so that each tick is
 *   delivered once.
 *   <br>Traces to:
 *   - \srqref{SRQ_HV_INT_010}
 *
 * @llr
 * - \llrid{LLR_GIC_MAINTENANCE_01}: If and only if bit 0 of the register
 *   `GICH_EISR0` (`gic_read32()`) is set, `maintenance()` shall write with
 *   `gic_write32()` 0 to `GICH_LR(0)` and `HV_IRQ_VTIMER` to `GICC_DIR`.
 *   <br>Traces to:
 *   - \hlrref{HLR_GIC_MAINTENANCE_01}
 * - \llrid{LLR_GIC_MAINTENANCE_02}: `maintenance()` shall then write `iar` to
 *   `GICC_DIR` and call `gic_update()` once with `vm`.
 *   <br>Traces to:
 *   - \hlrref{HLR_GIC_MAINTENANCE_01}
 *
 * @design
 * 1. If `(gic_read32(gich, GICH_EISR0) & 1U)` is not equal to `0U`:
 *    \llrref{LLR_GIC_MAINTENANCE_01}
 *    1. Call `gic_write32()` with the arguments `gich`, `GICH_LR(0U)` and `0U`.
 *    2. Call `gic_write32()` with the arguments `gicc`, `GICC_DIR` and
 *       `HV_IRQ_VTIMER`.
 * 2. Call `gic_write32()` with the arguments `gicc`, `GICC_DIR` and `iar`.
 *    \llrref{LLR_GIC_MAINTENANCE_02}
 * 3. Call `gic_update()` with the argument `vm`.
 *    \llrref{LLR_GIC_MAINTENANCE_02}
 *
 * @calls
 * - `gic_read32()`
 * - `gic_write32()`
 * - `gic_update()`
 */
static void maintenance(hv_vm_t *vm, uint32_t iar)
{
    if ((gic_read32(gich, GICH_EISR0) & 1U) != 0U) {
        gic_write32(gich, GICH_LR(0U), 0U);
        gic_write32(gicc, GICC_DIR, HV_IRQ_VTIMER);
    }
    gic_write32(gicc, GICC_DIR, iar);
    gic_update(vm);
}

/**
 * @funcname `gic_handle_irq`
 *
 * @description
 * Handles a physical interrupt taken while the guest of this CPU ran: the timer
 * tick goes to the guest, the maintenance interrupt ends it, anything else is
 * unexpected.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `cpu` | `hv_cpu_t *` | [in,out] This CPU. |
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `gicc` | `uint64_t` | The CPU interface's address |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `iar` | `uint32_t` | The interrupt acknowledge value |
 * | `irq` | `uint32_t` | The interrupt ID |
 * | `bit` | `uint32_t` | The PPI's bit in irqs_logged, or 0 |
 *
 * @hlr
 * - \hlrid{HLR_GIC_GIC_HANDLE_IRQ_01}: `gic_handle_irq()` shall give the tick
 *   of the virtual timer to the guest and handle the maintenance interrupt.
 *   <br>Traces to:
 *   - \srqref{SRQ_HV_INT_004}
 *   - \srqref{SRQ_HV_INT_010}
 *   - \srqref{SRQ_HV_TMR_007}
 * - \hlrid{HLR_GIC_GIC_HANDLE_IRQ_02}: `gic_handle_irq()` shall ignore a
 *   spurious interrupt and report any other one, a banked one once, and go on.
 *   <br>Traces to:
 *   - \srqref{SRQ_HV_INT_011}
 *
 * @llr
 * - \llrid{LLR_GIC_GIC_HANDLE_IRQ_01}: If the ID (the bits `GICC_IAR_ID` of the
 *   register `GICC_IAR`, read once with `gic_read32()`) is `GIC_SPURIOUS` or
 *   more, `gic_handle_irq()` shall return without writing a register.
 *   <br>Traces to:
 *   - \hlrref{HLR_GIC_GIC_HANDLE_IRQ_02}
 * - \llrid{LLR_GIC_GIC_HANDLE_IRQ_02}: If the ID is less than `GIC_SPURIOUS`,
 *   `gic_handle_irq()` shall write the value read from `GICC_IAR` to
 *   `GICC_EOIR` with `gic_write32()`.
 *   <br>Traces to:
 *   - \hlrref{HLR_GIC_GIC_HANDLE_IRQ_01}
 *   - \hlrref{HLR_GIC_GIC_HANDLE_IRQ_02}
 * - \llrid{LLR_GIC_GIC_HANDLE_IRQ_03}: If the ID is `HV_IRQ_VTIMER`,
 *   `gic_handle_irq()` shall set `timer_pending` of the VM of the CPU to true
 *   and call `gic_update()` once with that VM.
 *   <br>Traces to:
 *   - \hlrref{HLR_GIC_GIC_HANDLE_IRQ_01}
 * - \llrid{LLR_GIC_GIC_HANDLE_IRQ_04}: If the ID is `IRQ_MAINTENANCE`,
 *   `gic_handle_irq()` shall call `maintenance()` once with the VM of the CPU
 *   and the value read from `GICC_IAR`.
 *   <br>Traces to:
 *   - \hlrref{HLR_GIC_GIC_HANDLE_IRQ_01}
 * - \llrid{LLR_GIC_GIC_HANDLE_IRQ_05}: If the ID is less than `GIC_SPURIOUS`
 *   and is neither `HV_IRQ_VTIMER` nor `IRQ_MAINTENANCE`, `gic_handle_irq()`
 *   shall write the value read from `GICC_IAR` to `GICC_DIR`.
 *   <br>Traces to:
 *   - \hlrref{HLR_GIC_GIC_HANDLE_IRQ_02}
 * - \llrid{LLR_GIC_GIC_HANDLE_IRQ_06}: If the ID is neither `HV_IRQ_VTIMER` nor
 *   `IRQ_MAINTENANCE`, is less than `PPIS` and its bit is not set in
 *   `irqs_logged` of the CPU, `gic_handle_irq()` shall set that bit and call
 *   `hv_printf()` once with `"[hv] CPU %%u: unexpected interrupt %%u\n"`, the
 *   index of the CPU and the ID.
 *   <br>Traces to:
 *   - \hlrref{HLR_GIC_GIC_HANDLE_IRQ_02}
 * - \llrid{LLR_GIC_GIC_HANDLE_IRQ_07}: If the ID is neither `HV_IRQ_VTIMER` nor
 *   `IRQ_MAINTENANCE`, is `PPIS` or more and is less than `GIC_SPURIOUS`,
 *   `gic_handle_irq()` shall call `hv_printf()` once with `"[hv] CPU %%u:
 *   unexpected interrupt %%u\n"`, the index of the CPU and the ID.
 *   <br>Traces to:
 *   - \hlrref{HLR_GIC_GIC_HANDLE_IRQ_02}
 * - \llrid{LLR_GIC_GIC_HANDLE_IRQ_08}: If the ID is neither `HV_IRQ_VTIMER` nor
 *   `IRQ_MAINTENANCE`, is less than `PPIS` and its bit is set in `irqs_logged`
 *   of the CPU, `gic_handle_irq()` shall leave `irqs_logged` as it is and not
 *   call `hv_printf()`.
 *   <br>Traces to:
 *   - \hlrref{HLR_GIC_GIC_HANDLE_IRQ_02}
 *
 * @design
 * 1. Call `gic_read32()` with the arguments `gicc` and `GICC_IAR`, and store
 *    the result in `iar`. \llrref{LLR_GIC_GIC_HANDLE_IRQ_01},
 *    \llrref{LLR_GIC_GIC_HANDLE_IRQ_02}, \llrref{LLR_GIC_GIC_HANDLE_IRQ_03},
 *    \llrref{LLR_GIC_GIC_HANDLE_IRQ_04}, \llrref{LLR_GIC_GIC_HANDLE_IRQ_05},
 *    \llrref{LLR_GIC_GIC_HANDLE_IRQ_06}, \llrref{LLR_GIC_GIC_HANDLE_IRQ_07},
 *    \llrref{LLR_GIC_GIC_HANDLE_IRQ_08}
 * 2. Set `irq` to `iar & GICC_IAR_ID`. \llrref{LLR_GIC_GIC_HANDLE_IRQ_01},
 *    \llrref{LLR_GIC_GIC_HANDLE_IRQ_02}, \llrref{LLR_GIC_GIC_HANDLE_IRQ_03},
 *    \llrref{LLR_GIC_GIC_HANDLE_IRQ_04}, \llrref{LLR_GIC_GIC_HANDLE_IRQ_05},
 *    \llrref{LLR_GIC_GIC_HANDLE_IRQ_06}, \llrref{LLR_GIC_GIC_HANDLE_IRQ_07},
 *    \llrref{LLR_GIC_GIC_HANDLE_IRQ_08}
 * 3. If `irq` is greater than or equal to `GIC_SPURIOUS`:
 *    \llrref{LLR_GIC_GIC_HANDLE_IRQ_01}
 *    1. Return.
 * 4. Call `gic_write32()` with the arguments `gicc`, `GICC_EOIR` and `iar`.
 *    \llrref{LLR_GIC_GIC_HANDLE_IRQ_02}
 * 5. If `irq` is equal to `HV_IRQ_VTIMER`: \llrref{LLR_GIC_GIC_HANDLE_IRQ_03}
 *    1. Set `cpu->vcpu.vm->arch.timer_pending` to `true`.
 *    2. Call `gic_update()` with the argument `cpu->vcpu.vm`.
 * 6. Otherwise, if `irq` is equal to `IRQ_MAINTENANCE`:
 *    \llrref{LLR_GIC_GIC_HANDLE_IRQ_04}
 *    1. Call `maintenance()` with the arguments `cpu->vcpu.vm` and `iar`.
 * 7. Otherwise: \llrref{LLR_GIC_GIC_HANDLE_IRQ_05},
 *    \llrref{LLR_GIC_GIC_HANDLE_IRQ_06}, \llrref{LLR_GIC_GIC_HANDLE_IRQ_07},
 *    \llrref{LLR_GIC_GIC_HANDLE_IRQ_08}
 *    1. Call `gic_write32()` with the arguments `gicc`, `GICC_DIR` and `iar`.
 *       \llrref{LLR_GIC_GIC_HANDLE_IRQ_05}
 *    2. Set `bit` to `((uint32_t) 1U << irq)` if `irq` is less than `PPIS`,
 *       otherwise `0U`. \llrref{LLR_GIC_GIC_HANDLE_IRQ_06},
 *       \llrref{LLR_GIC_GIC_HANDLE_IRQ_07}, \llrref{LLR_GIC_GIC_HANDLE_IRQ_08}
 *    3. If `bit` is not equal to `0U` and `(cpu->arch.irqs_logged & bit)` is
 *       equal to `0U`: \llrref{LLR_GIC_GIC_HANDLE_IRQ_06},
 *       \llrref{LLR_GIC_GIC_HANDLE_IRQ_08}
 *       1. Set the bits `bit` in `cpu->arch.irqs_logged`.
 *       2. Call `hv_printf()` with the arguments
 *          `"[hv] CPU %%u: unexpected interrupt %%u\n"`, `cpu->index` and
 *          `irq`.
 *    4. Otherwise, if `irq` is greater than or equal to `PPIS`:
 *       \llrref{LLR_GIC_GIC_HANDLE_IRQ_07}, \llrref{LLR_GIC_GIC_HANDLE_IRQ_08}
 *       1. Call `hv_printf()` with the arguments
 *          `"[hv] CPU %%u: unexpected interrupt %%u\n"`, `cpu->index` and
 *          `irq`.
 *
 * @calls
 * - `gic_read32()`
 * - `gic_write32()`
 * - `gic_update()`
 * - `maintenance()`
 * - `hv_printf()`
 */
void gic_handle_irq(hv_cpu_t *cpu)
{
    uint32_t iar = gic_read32(gicc, GICC_IAR);
    uint32_t irq = iar & GICC_IAR_ID;

    if (irq >= GIC_SPURIOUS) {
        return;
    }
    // Priority drop only: deactivation is separate (EOImode).
    gic_write32(gicc, GICC_EOIR, iar);
    if (irq == HV_IRQ_VTIMER) {
        // Stays active until the guest's end of interrupt (maintenance()).
        cpu->vcpu.vm->arch.timer_pending = true;
        gic_update(cpu->vcpu.vm);
    } else if (irq == IRQ_MAINTENANCE) {
        maintenance(cpu->vcpu.vm, iar);
    } else {
        gic_write32(gicc, GICC_DIR, iar);
        uint32_t bit = (irq < PPIS) ? ((uint32_t) 1U << irq) : 0U;

        if ((bit != 0U) && ((cpu->arch.irqs_logged & bit) == 0U)) {
            cpu->arch.irqs_logged |= bit;
            hv_printf("[hv] CPU %u: unexpected interrupt %u\n", cpu->index,
                      irq);
        } else if (irq >= PPIS) {
            hv_printf("[hv] CPU %u: unexpected interrupt %u\n", cpu->index,
                      irq);
        } else {
            // Reported before.
        }
    }
}
