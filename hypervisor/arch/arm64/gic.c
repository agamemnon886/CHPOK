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
 * distributor (vgicd.c), the tick stays pending (and the physical
 * interrupt active) until it enables it.
 *
 * The HW bit of the list registers would let the guest's end of interrupt
 * deactivate interrupt 27 itself, without the maintenance interrupt, but
 * QEMU (11.1) doesn't update the physical CPU interface then: a tick that
 * is pending when it is deactivated (the line still high, e.g. after GDB
 * has held the guest) never comes, and the guest's time stands still.
 */

#include <arch/internal.h>

// Distributor.
#define GICD_CTLR           0x000U
#define GICD_TYPER          0x004U
#define GICD_IIDR           0x008U
#define GICD_ISENABLER0     0x100U
#define GICD_ICENABLER(n)   (0x180U + (4U * (n)))
#define GICD_ICPENDR0       0x280U
#define GICD_ICACTIVER0     0x380U
#define GICD_IPRIORITYR     0x400U
#define GICD_CTLR_ENABLE    1U
#define GICD_TYPER_LINES    0x1FU

// CPU interface.
#define GICC_CTLR           0x000U
#define GICC_PMR            0x004U
#define GICC_BPR            0x008U
#define GICC_IAR            0x00CU
#define GICC_EOIR           0x010U
#define GICC_DIR            0x1000U
#define GICC_CTLR_ENABLE    1U
#define GICC_CTLR_EOIMODE   ((uint32_t) 1U << 9)
#define GICC_IAR_ID         0x3FFU
#define GIC_SPURIOUS        1020U

// Virtual interface control.
#define GICH_HCR            0x000U
#define GICH_VTR            0x004U
#define GICH_VMCR           0x008U
#define GICH_EISR0          0x020U
#define GICH_APR            0x0F0U
#define GICH_LR(n)          (0x100U + (4U * (n)))
#define GICH_HCR_EN         1U
#define GICH_VTR_LISTREGS   0x3FU
#define GICH_LR_PENDING     ((uint32_t) 1U << 28)
#define GICH_LR_STATE       ((uint32_t) 3U << 28)
#define GICH_LR_PRIORITY    23U         // Shift of priority bits 7:3
#define GICH_LR_EOI         ((uint32_t) 1U << 19)   // Maintenance on EOI

#define IRQ_MAINTENANCE     25U         // PPI 9
#define HOST_PRIORITY       0xA0U
#define PPIS                32U         // SGIs and PPIs
#define HOST_IRQS           (((uint32_t) 1U << HV_IRQ_VTIMER) \
                             | ((uint32_t) 1U << IRQ_MAINTENANCE))

static uint64_t gicd;
static uint64_t gicc;
static uint64_t gich;
static uint64_t gicv;
static uint32_t nb_lrs = 0U;
static uint32_t host_iidr;

/*
 * MISRA C:2025 Rule 11.4 deviation (the register accessors below): the
 * GIC's registers are used by physical address (identity-mapped),
 * converted to pointers.
 */
static uint32_t gic_read32(uint64_t base, uint32_t offset)
{
    return *(const volatile uint32_t *) (uintptr_t) (base + offset);
}

static void gic_write32(uint64_t base, uint32_t offset, uint32_t value)
{
    *(volatile uint32_t *) (uintptr_t) (base + offset) = value;
}

static void gic_write8(uint64_t base, uint32_t offset, uint8_t value)
{
    *(volatile uint8_t *) (uintptr_t) (base + offset) = value;
}

/*
 * Set up the distributor (on the boot CPU): every shared interrupt
 * disabled; the GIC's addresses come from the device tree.
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

// Number of list registers of the virtual interface.
uint32_t gic_list_registers(void)
{
    return nb_lrs;
}

// Physical address of the virtual CPU interface (the guests' GICC).
uint64_t gic_gicv_base(void)
{
    return gicv;
}

// The distributor's identification (what the guests' distributor reads).
uint32_t gic_host_iidr(void)
{
    return host_iidr;
}

/*
 * Set up this CPU's interface and banked distributor registers: only the
 * virtual timer's and the maintenance interrupts are enabled; the virtual
 * interface is on.
 */
void gic_init_cpu(void)
{
    gic_write32(gicd, GICD_ICENABLER(0U), 0xFFFFFFFFU);
    gic_write32(gicd, GICD_ICPENDR0, 0xFFFFFFFFU);
    gic_write32(gicd, GICD_ICACTIVER0, 0xFFFFFFFFU);
    gic_write8(gicd, GICD_IPRIORITYR + HV_IRQ_VTIMER, HOST_PRIORITY);
    gic_write8(gicd, GICD_IPRIORITYR + IRQ_MAINTENANCE, HOST_PRIORITY);
    gic_write32(gicd, GICD_ISENABLER0, HOST_IRQS);

    gic_write32(gicc, GICC_PMR, 0xFFU);
    gic_write32(gicc, GICC_BPR, 0U);
    gic_write32(gicc, GICC_CTLR, GICC_CTLR_ENABLE | GICC_CTLR_EOIMODE);
    gic_vcpu_reset();
    gic_write32(gich, GICH_HCR, GICH_HCR_EN);
}

// Stop this CPU's interrupts (a stopped VM, a parked vCPU, a fatal error).
void gic_cpu_off(void)
{
    if (nb_lrs != 0U) {
        gic_write32(gich, GICH_HCR, 0U);
        gic_write32(gicc, GICC_CTLR, 0U);
        gic_write32(gicd, GICD_ICENABLER(0U), HOST_IRQS);
    }
}

/*
 * The guest's interrupt state as at power-on: empty list registers, no
 * active priorities, the virtual CPU interface off, and the physical timer
 * and maintenance interrupts neither pending nor active.
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

/*
 * Give the guest its pending timer tick if its distributor lets it have
 * one, or take back a tick it hasn't taken yet if it has disabled the
 * interrupt (called after each change of either).
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

/*
 * The maintenance interrupt: if the guest has ended its timer interrupt
 * (list register 0 is empty again, with its end of interrupt request),
 * the list register is cleared and the physical timer interrupt
 * deactivated. The maintenance interrupt itself is deactivated after its
 * cause is gone.
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

/*
 * A physical interrupt, taken while the guest of this CPU ran: the timer
 * tick goes to the guest, the maintenance interrupt ends it, anything else
 * is unexpected.
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
