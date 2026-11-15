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
 * Modified by Kirill Brazhnikov, 2026-11-13: the function declarations are
 * hidden from Doxygen (\cond): only the definitions of the functions are
 * documented.
 *
 * Modified by Kirill Brazhnikov, 2026-11-14: the enum types are named with _e:
 * hv_action_e (was hv_action_t); the macros, types and external declarations of
 * the C files are here, documented; hv_main(), hv_ap_main() and
 * hv_el2_exception() are declared here.
 *
 * Modified by Kirill Brazhnikov, 2026-11-15: moved from
 * hypervisor/arch/arm64/include/arch/internal.h (the code of the processor
 * architecture is in hypervisor/csps); the comments name the new paths; the
 * device registers, the device tree format and the translation bits that the
 * kernel uses too are in the shared headers (include/).
 */

/*
 * Interfaces between the arm64 parts of the hypervisor (csps/arm64,
 * bsps/arm64-qemu), and the addresses of QEMU 'virt' (-machine
 * virt,virtualization=on, gic-version=2) that they use: the host's
 * devices, and what a VM sees.
 */

#ifndef HV_ARCH_INTERNAL_H
#define HV_ARCH_INTERNAL_H

#include <hv/hv.h>

/*
 * The registers of the devices of the board and the device tree format,
 * which the kernel uses too.
 */
#include <common/fdt.h>
#include <dev/gicv2.h>
#include <dev/pl011.h>
#include <dev/pl031.h>
#include <dev/psci.h>
/* The translation bits that the kernel uses too. */
#include <cpu/mmu.h>

/**
 * @macroname `HV_HOST_UART0`
 *
 * @description
 * The address of the first PL011 of the host (COM1), a device of QEMU 'virt'.
 */
#define HV_HOST_UART0       0x09000000UL

/**
 * @macroname `HV_HOST_RTC`
 *
 * @description
 * The address of the PL031 of the host.
 */
#define HV_HOST_RTC         0x09010000UL

/**
 * @macroname `HV_HOST_UART1`
 *
 * @description
 * The address of the second PL011 of the host (COM2).
 */
#define HV_HOST_UART1       0x09040000UL

/**
 * @macroname `HV_HOST_PCI_IO`
 *
 * @description
 * The address of the window of the PCI I/O space of the host.
 */
#define HV_HOST_PCI_IO      0x3EFF0000UL

/**
 * @macroname `HV_HOST_RAM_BASE`
 *
 * @description
 * The address of the start of the RAM of the host, where the device tree is
 * put.
 */
#define HV_HOST_RAM_BASE    0x40000000UL

/**
 * @macroname `HV_GUEST_GICD`
 *
 * @description
 * The guest-physical address of the GIC distributor of the guest (the memory
 * map of a VM is that of QEMU 'virt').
 */
#define HV_GUEST_GICD       0x08000000UL

/**
 * @macroname `HV_GUEST_GICC`
 *
 * @description
 * The guest-physical address of the GIC CPU interface of the guest (8 KiB, the
 * GICV of the host).
 */
#define HV_GUEST_GICC       0x08010000UL

/**
 * @macroname `HV_GUEST_GICC_SIZE`
 *
 * @description
 * The size of the guest's GIC CPU interface, in bytes.
 */
#define HV_GUEST_GICC_SIZE  0x2000UL

/**
 * @macroname `HV_GUEST_UART0`
 *
 * @description
 * The guest-physical address of the first PL011 of the guest.
 */
#define HV_GUEST_UART0      0x09000000UL

/**
 * @macroname `HV_GUEST_RTC`
 *
 * @description
 * The guest-physical address of the PL031 of the guest.
 */
#define HV_GUEST_RTC        0x09010000UL

/**
 * @macroname `HV_GUEST_UART1`
 *
 * @description
 * The guest-physical address of the second PL011 of the guest.
 */
#define HV_GUEST_UART1      0x09040000UL

/**
 * @macroname `HV_GUEST_RAM`
 *
 * @description
 * The guest-physical address of the start of the guest RAM.
 */
#define HV_GUEST_RAM        0x40000000UL

/**
 * @macroname `HV_GUEST_DTB_SPACE`
 *
 * @description
 * The room of the device tree of the guest, in bytes.
 */
#define HV_GUEST_DTB_SPACE  0x10000UL

/**
 * @macroname `HV_IRQ_VTIMER`
 *
 * @description
 * The guest's virtual timer interrupt (PPI 11).
 */
#define HV_IRQ_VTIMER       27U

/**
 * @macroname `HV_EXIT_SYNC`
 *
 * @description
 * The guest was interrupted by a synchronous exception (the reason
 * hv_guest_enter() returned).
 */
#define HV_EXIT_SYNC        0U

/**
 * @macroname `HV_EXIT_IRQ`
 *
 * @description
 * The guest was interrupted by a physical IRQ.
 */
#define HV_EXIT_IRQ         1U

/**
 * @macroname `HV_EXIT_FIQ`
 *
 * @description
 * The guest was interrupted by a physical FIQ.
 */
#define HV_EXIT_FIQ         2U

/**
 * @macroname `HV_EXIT_SERROR`
 *
 * @description
 * The guest was interrupted by an SError.
 */
#define HV_EXIT_SERROR      3U

/**
 * @macroname `HV_EXIT_AARCH32`
 *
 * @description
 * The guest took an exception from AArch32.
 */
#define HV_EXIT_AARCH32     4U

/**
 * @typename `hv_fdt_info_t`
 *
 * @description
 * What the host's device tree says (fdt.c).
 */
typedef struct {
    uint64_t ram_base; /**< The base address of the RAM. */
    uint64_t ram_size; /**< The size of the RAM. */
    uint32_t nb_cpus; /**< CPUs found (at most HV_MAX_CPUS kept). */
    uint32_t mpidr[HV_MAX_CPUS]; /**< The MPIDR of each CPU. */
    uint64_t ecam_base; /**< PCI configuration space (0: none). */
    uint64_t ecam_size; /**< The size of the PCI configuration space. */
    bool gic_v2; /**< A GICv2 ("arm,cortex-a15-gic"). */
    uint32_t gic_regs; /**< Number of GIC register ranges (4: with. */
    /**
     * The virtualization extensions)
     *
     * Distributor.
     */
    uint64_t gicd;
    uint64_t gicc; /**< CPU interface. */
    uint64_t gich; /**< Virtual interface control. */
    uint64_t gicv; /**< Virtual CPU interface. */
    uint64_t gicv_size; /**< The size of the virtual CPU interface. */
    bool uart1; /**< The second PL011 (COM2) exists. */
} hv_fdt_info_t;

/**
 * @typename `hv_mmu_regs_t`
 *
 * @description
 * Values of the EL2 translation registers (mmu.c; entry.S loads them).
 */
typedef struct {
    uint64_t mair; /**< The value of MAIR_EL2. */
    uint64_t tcr; /**< The value of TCR_EL2. */
    uint64_t ttbr0; /**< The value of TTBR0_EL2. */
    uint64_t sctlr; /**< The value of SCTLR_EL2. */
} hv_mmu_regs_t;

extern hv_mmu_regs_t hv_el2_mmu;

/**
 * @macroname `DEPTH_MAX`
 *
 * @description
 * The deepest nesting of nodes that is tracked.
 */
#define DEPTH_MAX           16U

/**
 * @typename `node_e`
 *
 * @description
 * The nodes looked at; the others are skipped.
 */
typedef enum {
    NODE_OTHER, /**< A node that is not looked at. */
    NODE_ROOT, /**< The root node. */
    NODE_MEMORY, /**< The memory node. */
    NODE_CPUS, /**< The cpus node. */
    NODE_CPU, /**< A cpu node. */
    NODE_GIC, /**< The interrupt controller node. */
    NODE_PCIE /**< The PCI Express host bridge node. */
} node_e;

/**
 * @typename `fdt_t`
 *
 * @description
 * A device tree blob that was checked, with the places of its blocks.
 */
typedef struct {
    const uint8_t *blob; /**< The device tree blob. */
    uint32_t size; /**< Totalsize. */
    uint32_t strings; /**< Offset of the strings block. */
    uint32_t strings_size; /**< The size of the strings block. */
} fdt_t;

/**
 * @typename `cells_t`
 *
 * @description
 * Cells of the root's addresses and sizes, and of the CPUs' reg
 * (\#address-cells of /cpus).
 */
typedef struct {
    /**
     * The number of cells of an address of the root.
     */
    uint32_t address_cells;
    uint32_t size_cells; /**< The number of cells of a size of the root. */
    uint32_t cpu_cells; /**< The number of cells of the reg of a CPU. */
} cells_t;

/**
 * @macroname `GICD_TYPER`
 *
 * @description
 * The offset of the interrupt controller type register of the distributor.
 */
#define GICD_TYPER          0x004U

/**
 * @macroname `GICD_IIDR`
 *
 * @description
 * The offset of the implementer identification register of the distributor.
 */
#define GICD_IIDR           0x008U

/**
 * @macroname `GICD_ISENABLER0`
 *
 * @description
 * The offset of the set-enable register of the interrupts 0-31.
 */
#define GICD_ISENABLER0     0x100U

/**
 * @macroname `GICD_ICENABLER`
 *
 * @description
 * The offset of the clear-enable register of the interrupts 32n to 32n + 31.
 *
 * @params
 * | Name | Description |
 * |------|-------------|
 * | `n` | The number of the register. |
 */
#define GICD_ICENABLER(n)   (0x180U + (4U * (n)))

/**
 * @macroname `GICD_ICPENDR0`
 *
 * @description
 * The offset of the clear-pending register of the interrupts 0-31.
 */
#define GICD_ICPENDR0       0x280U

/**
 * @macroname `GICD_ICACTIVER0`
 *
 * @description
 * The offset of the clear-active register of the interrupts 0-31.
 */
#define GICD_ICACTIVER0     0x380U

/**
 * @macroname `GICD_CTLR_ENABLE`
 *
 * @description
 * The bit of GICD_CTLR that enables the distributor.
 */
#define GICD_CTLR_ENABLE    1U

/**
 * @macroname `GICD_TYPER_LINES`
 *
 * @description
 * The mask of the field of GICD_TYPER that gives the number of interrupt lines.
 */
#define GICD_TYPER_LINES    0x1FU

/**
 * @macroname `GICC_BPR`
 *
 * @description
 * The offset of the binary point register of the CPU interface.
 */
#define GICC_BPR            0x008U

/**
 * @macroname `GICC_DIR`
 *
 * @description
 * The offset of the deactivate interrupt register.
 */
#define GICC_DIR            0x1000U

/**
 * @macroname `GICC_CTLR_ENABLE`
 *
 * @description
 * The bit of GICC_CTLR that enables the CPU interface.
 */
#define GICC_CTLR_ENABLE    1U

/**
 * @macroname `GICC_CTLR_EOIMODE`
 *
 * @description
 * The bit of GICC_CTLR that separates the priority drop from the deactivation.
 */
#define GICC_CTLR_EOIMODE   ((uint32_t) 1U << 9)

/**
 * @macroname `GICC_IAR_ID`
 *
 * @description
 * The mask of the interrupt ID in GICC_IAR.
 */
#define GICC_IAR_ID         0x3FFU

/**
 * @macroname `GICH_HCR`
 *
 * @description
 * The offset of the hypervisor control register of the virtual interface.
 */
#define GICH_HCR            0x000U

/**
 * @macroname `GICH_VTR`
 *
 * @description
 * The offset of the VGIC type register.
 */
#define GICH_VTR            0x004U

/**
 * @macroname `GICH_VMCR`
 *
 * @description
 * The offset of the virtual machine control register.
 */
#define GICH_VMCR           0x008U

/**
 * @macroname `GICH_EISR0`
 *
 * @description
 * The offset of the end of interrupt status register.
 */
#define GICH_EISR0          0x020U

/**
 * @macroname `GICH_APR`
 *
 * @description
 * The offset of the active priorities register.
 */
#define GICH_APR            0x0F0U

/**
 * @macroname `GICH_LR`
 *
 * @description
 * The offset of the list register n.
 *
 * @params
 * | Name | Description |
 * |------|-------------|
 * | `n` | The number of the list register. |
 */
#define GICH_LR(n)          (0x100U + (4U * (n)))

/**
 * @macroname `GICH_HCR_EN`
 *
 * @description
 * The bit of GICH_HCR that enables the virtual CPU interface.
 */
#define GICH_HCR_EN         1U

/**
 * @macroname `GICH_VTR_LISTREGS`
 *
 * @description
 * The mask of the field of GICH_VTR that gives the number of list registers
 * minus 1.
 */
#define GICH_VTR_LISTREGS   0x3FU

/**
 * @macroname `GICH_LR_PENDING`
 *
 * @description
 * The bit of a list register that makes the interrupt pending.
 */
#define GICH_LR_PENDING     ((uint32_t) 1U << 28)

/**
 * @macroname `GICH_LR_STATE`
 *
 * @description
 * The mask of the state bits of a list register.
 */
#define GICH_LR_STATE       ((uint32_t) 3U << 28)

/**
 * @macroname `GICH_LR_PRIORITY`
 *
 * @description
 * The position of the priority bits 7:3 in a list register.
 */
#define GICH_LR_PRIORITY    23U

/**
 * @macroname `GICH_LR_EOI`
 *
 * @description
 * The bit of a list register that raises a maintenance interrupt when the guest
 * ends the interrupt.
 */
#define GICH_LR_EOI         ((uint32_t) 1U << 19)

/**
 * @macroname `IRQ_MAINTENANCE`
 *
 * @description
 * The interrupt of the maintenance events of the virtual interface (PPI 9).
 */
#define IRQ_MAINTENANCE     25U

/**
 * @macroname `HOST_PRIORITY`
 *
 * @description
 * The priority of the interrupts of the host.
 */
#define HOST_PRIORITY       0xA0U

/**
 * @macroname `PPIS`
 *
 * @description
 * The number of the SGIs and PPIs (the interrupts 0-31).
 */
#define PPIS                32U

/**
 * @macroname `HOST_IRQS`
 *
 * @description
 * The interrupts of the host that are enabled in the distributor.
 */
#define HOST_IRQS           (((uint32_t) 1U << HV_IRQ_VTIMER) \
                             | ((uint32_t) 1U << IRQ_MAINTENANCE))

/**
 * @macroname `CTR_DMINLINE_SHIFT`
 *
 * @description
 * The position of the smallest data cache line size in CTR_EL0.
 */
#define CTR_DMINLINE_SHIFT  16U

/**
 * @macroname `CTR_DMINLINE_MASK`
 *
 * @description
 * The mask of the smallest data cache line size in CTR_EL0.
 */
#define CTR_DMINLINE_MASK   0xFUL

/**
 * @macroname `MPIDR_AFFINITY`
 *
 * @description
 * The mask of the affinity levels 0-2 of MPIDR_EL1.
 */
#define MPIDR_AFFINITY      0xFFFFFFUL

/**
 * @macroname `GIB_SHIFT`
 *
 * @description
 * The shift of the address to the number of its gibibyte.
 */
#define GIB_SHIFT           30U

/**
 * @macroname `GIB_DEVICES`
 *
 * @description
 * The number of the gibibyte of the host-physical space that holds the devices
 * (mapped by mmu.c).
 */
#define GIB_DEVICES         0U

/**
 * @macroname `GIB_HIGH_ECAM`
 *
 * @description
 * The number of the gibibyte of the high PCI configuration space.
 */
#define GIB_HIGH_ECAM       256U

/**
 * @macroname `ESR_ISV`
 *
 * @description
 * The bit of ESR_EL2 of a data abort that tells the syndrome is valid.
 */
#define ESR_ISV             (1UL << 24)

/**
 * @macroname `ESR_SAS_SHIFT`
 *
 * @description
 * The position of the access size in ESR_EL2.
 */
#define ESR_SAS_SHIFT       22U

/**
 * @macroname `ESR_SAS_MASK`
 *
 * @description
 * The mask of the access size in ESR_EL2.
 */
#define ESR_SAS_MASK        3UL

/**
 * @macroname `ESR_SSE`
 *
 * @description
 * The bit of ESR_EL2 that tells a loaded byte or halfword is sign-extended.
 */
#define ESR_SSE             (1UL << 21)

/**
 * @macroname `ESR_SRT_SHIFT`
 *
 * @description
 * The position of the transfer register in ESR_EL2.
 */
#define ESR_SRT_SHIFT       16U

/**
 * @macroname `ESR_SRT_MASK`
 *
 * @description
 * The mask of the transfer register in ESR_EL2.
 */
#define ESR_SRT_MASK        0x1FUL

/**
 * @macroname `ESR_SF`
 *
 * @description
 * The bit of ESR_EL2 that tells the register is 64 bits wide.
 */
#define ESR_SF              (1UL << 15)

/**
 * @macroname `ESR_S1PTW`
 *
 * @description
 * The bit of ESR_EL2 that tells the abort is on a stage 1 table walk.
 */
#define ESR_S1PTW           (1UL << 7)

/**
 * @macroname `ESR_WNR`
 *
 * @description
 * The bit of ESR_EL2 that tells the access is a write.
 */
#define ESR_WNR             (1UL << 6)

/**
 * @macroname `HPFAR_FIPA_SHIFT`
 *
 * @description
 * The shift of the faulting IPA in HPFAR_EL2 (bits 4 and up hold IPA bits 12
 * and up).
 */
#define HPFAR_FIPA_SHIFT    4U

/**
 * @macroname `PAGE_OFFSET_MASK`
 *
 * @description
 * The mask of the offset in a 4 KiB page.
 */
#define PAGE_OFFSET_MASK    0xFFFUL

/**
 * @macroname `REG_XZR`
 *
 * @description
 * The number of the register that reads as zero (XZR).
 */
#define REG_XZR             31U

/**
 * @macroname `L1_ENTRIES`
 *
 * @description
 * The number of the entries of the level 1 table of the host.
 */
#define L1_ENTRIES          512U

/**
 * @macroname `DESC_AP_RW`
 *
 * @description
 * The access permission bits of a descriptor: read and write (AP[1] is reserved
 * as 1 at EL2).
 */
#define DESC_AP_RW          (1UL << 6)

/**
 * @macroname `DESC_XN`
 *
 * @description
 * The execute-never bit of a descriptor.
 */
#define DESC_XN             (1UL << 54)

/**
 * @macroname `MAIR_DEVICE`
 *
 * @description
 * The MAIR index of device-nGnRE memory.
 */
#define MAIR_DEVICE         0U

/**
 * @macroname `MAIR_NORMAL`
 *
 * @description
 * The MAIR index of normal write-back memory.
 */
#define MAIR_NORMAL         1U

/**
 * @macroname `MAIR_VALUE`
 *
 * @description
 * The value of MAIR_EL2.
 */
#define MAIR_VALUE          ((0x04UL << (8U * MAIR_DEVICE)) \
                             | (0xFFUL << (8U * MAIR_NORMAL)))

/**
 * @macroname `DESC_DEVICE`
 *
 * @description
 * The descriptor bits of a block of device memory.
 */
#define DESC_DEVICE         (DESC_BLOCK | DESC_ATTR(MAIR_DEVICE) \
                             | DESC_AP_RW | DESC_AF | DESC_XN)

/**
 * @macroname `DESC_NORMAL`
 *
 * @description
 * The descriptor bits of a block of normal memory.
 */
#define DESC_NORMAL         (DESC_BLOCK | DESC_ATTR(MAIR_NORMAL) \
                             | DESC_AP_RW | DESC_SH_INNER | DESC_AF)

/**
 * @macroname `TCR_PS_40BIT`
 *
 * @description
 * The bits of TCR_EL2: 40-bit physical addresses.
 */
#define TCR_PS_40BIT        (2UL << 16)

/**
 * @macroname `TCR_RES1`
 *
 * @description
 * The bits of TCR_EL2 that are reserved as 1.
 */
#define TCR_RES1            ((1UL << 31) | (1UL << 23))

/**
 * @macroname `TCR_VALUE`
 *
 * @description
 * The value of TCR_EL2.
 */
#define TCR_VALUE           (TCR_T0SZ | TCR_IRGN0_WBWA | TCR_ORGN0_WBWA \
                             | TCR_SH0_INNER | TCR_PS_40BIT | TCR_RES1)

/**
 * @macroname `SCTLR_RES1`
 *
 * @description
 * The bits of SCTLR_EL2 that are reserved as 1.
 */
#define SCTLR_RES1          0x30C50830UL

/**
 * @macroname `SCTLR_SA`
 *
 * @description
 * The bit of SCTLR_EL2 that enables the stack alignment check.
 */
#define SCTLR_SA            (1UL << 3)

/**
 * @macroname `SCTLR_VALUE`
 *
 * @description
 * The value of SCTLR_EL2.
 */
#define SCTLR_VALUE         (SCTLR_RES1 | SCTLR_M | SCTLR_C | SCTLR_SA \
                             | SCTLR_I)

/**
 * @macroname `GIB_RAM_FIRST`
 *
 * @description
 * The number of the first gibibyte of the guest-physical space that holds RAM.
 */
#define GIB_RAM_FIRST       1U

/**
 * @macroname `GIB_RAM_LAST`
 *
 * @description
 * The number of the last gibibyte of the guest-physical space that holds RAM.
 */
#define GIB_RAM_LAST        3U

/**
 * @macroname `PCI_VENDOR_REDHAT`
 *
 * @description
 * The PCI vendor ID of Red Hat.
 */
#define PCI_VENDOR_REDHAT       0x1B36U

/**
 * @macroname `PCI_DEVICE_SERIAL`
 *
 * @description
 * The PCI device ID of the QEMU PCI serial card.
 */
#define PCI_DEVICE_SERIAL       0x0002U

/**
 * @macroname `PCI_ID`
 *
 * @description
 * The offset of the vendor and device IDs in the configuration space.
 */
#define PCI_ID                  0x00U

/**
 * @macroname `PCI_COMMAND`
 *
 * @description
 * The offset of the command register in the configuration space.
 */
#define PCI_COMMAND             0x04U

/**
 * @macroname `PCI_COMMAND_IO`
 *
 * @description
 * The bit of the command register that enables the I/O space.
 */
#define PCI_COMMAND_IO          0x0001U

/**
 * @macroname `PCI_BAR0`
 *
 * @description
 * The offset of the first base address register.
 */
#define PCI_BAR0                0x10U

/**
 * @macroname `SLOT_SHIFT`
 *
 * @description
 * The shift of the slot number in an ECAM address (bus 0, function 0).
 */
#define SLOT_SHIFT              15U

/**
 * @macroname `FIRST_SLOT`
 *
 * @description
 * The PCI slot of the first PCI serial card (COM3).
 */
#define FIRST_SLOT              0x10U

/**
 * @macroname `FIRST_PORT`
 *
 * @description
 * The number of the COM port of the first PCI serial card.
 */
#define FIRST_PORT              3U

/**
 * @macroname `IO_BASE`
 *
 * @description
 * The I/O address given to the first PCI serial card.
 */
#define IO_BASE                 0x1000U

/**
 * @macroname `IO_STEP`
 *
 * @description
 * The size of the I/O space of a card, in bytes.
 */
#define IO_STEP                 0x10U

/**
 * @macroname `SLOTS`
 *
 * @description
 * The number of the PCI slots that are searched.
 */
#define SLOTS                   32U

/**
 * @macroname `START_TIMEOUT_US`
 *
 * @description
 * The time to wait for another CPU to start, in microseconds.
 */
#define START_TIMEOUT_US    1000000U

/**
 * @macroname `ENTRIES`
 *
 * @description
 * The number of the entries of a stage 2 table.
 */
#define ENTRIES             512U

/**
 * @macroname `S2_MEMATTR_NORMAL`
 *
 * @description
 * The stage 2 memory attributes: normal memory, write-back, inner and outer.
 */
#define S2_MEMATTR_NORMAL   (0xFUL << 2)

/**
 * @macroname `S2_MEMATTR_DEVICE`
 *
 * @description
 * The stage 2 memory attributes: device-nGnRE.
 */
#define S2_MEMATTR_DEVICE   (0x1UL << 2)

/**
 * @macroname `S2_AP_RW`
 *
 * @description
 * The stage 2 access permissions: read and write.
 */
#define S2_AP_RW            (3UL << 6)

/**
 * @macroname `S2_SH_INNER`
 *
 * @description
 * The stage 2 shareability: inner shareable.
 */
#define S2_SH_INNER         (3UL << 8)

/**
 * @macroname `S2_AF`
 *
 * @description
 * The access flag of a stage 2 descriptor.
 */
#define S2_AF               (1UL << 10)

/**
 * @macroname `S2_XN`
 *
 * @description
 * The execute-never bits of a stage 2 descriptor.
 */
#define S2_XN               (1UL << 54)

/**
 * @macroname `S2_RAM`
 *
 * @description
 * The bits of a stage 2 descriptor of a block of RAM.
 */
#define S2_RAM              (DESC_BLOCK | S2_MEMATTR_NORMAL | S2_AP_RW \
                             | S2_SH_INNER | S2_AF)

/**
 * @macroname `S2_DEVICE`
 *
 * @description
 * The bits of a stage 2 descriptor of a page of device memory.
 */
#define S2_DEVICE           (DESC_PAGE | S2_MEMATTR_DEVICE | S2_AP_RW \
                             | S2_AF | S2_XN)

/**
 * @macroname `VTTBR_VMID_SHIFT`
 *
 * @description
 * The position of the VMID in VTTBR_EL2.
 */
#define VTTBR_VMID_SHIFT    48U

/**
 * @typename `stage2_tables_t`
 *
 * @description
 * The stage 2 tables of a VM.
 */
typedef struct {
    uint64_t l1[ENTRIES]; /**< 4 entries used (4 GiB). */
    /**
     * The level 2 table of the gibibyte of the devices.
     */
    uint64_t l2_devices[ENTRIES];
    uint64_t l3_gic[ENTRIES]; /**< The level 3 table of the GIC page. */
    /**
     * The level 3 table of the UART and RTC pages.
     */
    uint64_t l3_uart[ENTRIES];
    /**
     * The level 2 table of the gibibyte of the RAM.
     */
    uint64_t l2_ram[ENTRIES];
} stage2_tables_t;

/**
 * @macroname `UART_RBR`
 *
 * @description
 * The offset of the receive buffer register of a 16550 (DLAB = 0).
 */
#define UART_RBR            0U
/* Documented with its x86_64 definition. */
#define UART_THR            0U      // Transmit holding register (DLAB = 0)
/* Documented with its x86_64 definition. */
#define UART_IER            1U      // Interrupt enable (DLAB = 0)
/* Documented with its x86_64 definition. */
#define UART_DLL            0U      // Divisor latch, low byte (DLAB = 1)
/* Documented with its x86_64 definition. */
#define UART_DLM            1U      // Divisor latch, high byte (DLAB = 1)
/* Documented with its x86_64 definition. */
#define UART_FCR            2U
/* Documented with its x86_64 definition. */
#define UART_LCR            3U
/* Documented with its x86_64 definition. */
#define UART_MCR            4U
/* Documented with its x86_64 definition. */
#define UART_LSR            5U

/**
 * @macroname `UART_LSR_DR`
 *
 * @description
 * The bit of the line status register: data is ready.
 */
#define UART_LSR_DR         0x01U
/* Documented with its x86_64 definition. */
#define UART_LSR_THRE       0x20U   // Transmit holding register empty

/**
 * @macroname `UART_LSR_TEMT`
 *
 * @description
 * The bit of the line status register: the transmitter is empty.
 */
#define UART_LSR_TEMT       0x40U

/**
 * @typename `host_port_t`
 *
 * @description
 * A host serial port: where its registers are and which kind of UART it is.
 */
typedef struct {
    uint64_t base; /**< Physical address of the registers (0: none). */
    bool pl011; /**< Whether the port is a PL011 (otherwise a 16550). */
} host_port_t;

/**
 * @macroname `HCR_VM`
 *
 * @description
 * The bit of HCR_EL2 that enables the stage 2 translation.
 */
#define HCR_VM              (1UL << 0)

/**
 * @macroname `HCR_SWIO`
 *
 * @description
 * The bit of HCR_EL2 that makes the invalidation of the data cache by set/way
 * clean and invalidate it.
 */
#define HCR_SWIO            (1UL << 1)

/**
 * @macroname `HCR_FMO`
 *
 * @description
 * The bit of HCR_EL2 that routes the physical FIQs to EL2.
 */
#define HCR_FMO             (1UL << 3)

/**
 * @macroname `HCR_IMO`
 *
 * @description
 * The bit of HCR_EL2 that routes the physical IRQs to EL2.
 */
#define HCR_IMO             (1UL << 4)

/**
 * @macroname `HCR_AMO`
 *
 * @description
 * The bit of HCR_EL2 that routes the SErrors to EL2.
 */
#define HCR_AMO             (1UL << 5)

/**
 * @macroname `HCR_TSC`
 *
 * @description
 * The bit of HCR_EL2 that traps SMC to EL2.
 */
#define HCR_TSC             (1UL << 19)

/**
 * @macroname `HCR_RW`
 *
 * @description
 * The bit of HCR_EL2 that makes EL1 run AArch64.
 */
#define HCR_RW              (1UL << 31)

/**
 * @macroname `HCR_VALUE`
 *
 * @description
 * The value of HCR_EL2 while a guest runs.
 */
#define HCR_VALUE           (HCR_VM | HCR_SWIO | HCR_FMO | HCR_IMO \
                             | HCR_AMO | HCR_TSC | HCR_RW)

/**
 * @macroname `VTCR_T0SZ`
 *
 * @description
 * The bits of VTCR_EL2 for 32-bit guest-physical addresses (T0SZ).
 */
#define VTCR_T0SZ           32UL

/**
 * @macroname `VTCR_SL0_LEVEL1`
 *
 * @description
 * The bits of VTCR_EL2: the walk starts at level 1.
 */
#define VTCR_SL0_LEVEL1     (1UL << 6)

/**
 * @macroname `VTCR_IRGN0_WBWA`
 *
 * @description
 * The bits of VTCR_EL2: inner write-back write-allocate walks.
 */
#define VTCR_IRGN0_WBWA     (1UL << 8)

/**
 * @macroname `VTCR_ORGN0_WBWA`
 *
 * @description
 * The bits of VTCR_EL2: outer write-back write-allocate walks.
 */
#define VTCR_ORGN0_WBWA     (1UL << 10)

/**
 * @macroname `VTCR_SH0_INNER`
 *
 * @description
 * The bits of VTCR_EL2: inner shareable walks.
 */
#define VTCR_SH0_INNER      (3UL << 12)

/**
 * @macroname `VTCR_PS_40BIT`
 *
 * @description
 * The bits of VTCR_EL2: 40-bit physical addresses.
 */
#define VTCR_PS_40BIT       (2UL << 16)

/**
 * @macroname `VTCR_RES1`
 *
 * @description
 * The bit of VTCR_EL2 that is reserved as 1.
 */
#define VTCR_RES1           (1UL << 31)

/**
 * @macroname `VTCR_VALUE`
 *
 * @description
 * The value of VTCR_EL2.
 */
#define VTCR_VALUE          (VTCR_T0SZ | VTCR_SL0_LEVEL1 | VTCR_IRGN0_WBWA \
                             | VTCR_ORGN0_WBWA | VTCR_SH0_INNER \
                             | VTCR_PS_40BIT | VTCR_RES1)

/**
 * @macroname `CPTR_EL2_RES1`
 *
 * @description
 * The value of CPTR_EL2: the bits reserved as 1, with FP/SIMD not trapped.
 */
#define CPTR_EL2_RES1       0x33FFUL

/**
 * @macroname `CNTHCTL_EL1_ACCESS`
 *
 * @description
 * The value of CNTHCTL_EL2 that gives EL1 the physical counter and timer.
 */
#define CNTHCTL_EL1_ACCESS  3UL

/**
 * @macroname `PMCR_N_SHIFT`
 *
 * @description
 * The position of the number of counters in PMCR_EL0.
 */
#define PMCR_N_SHIFT        11U

/**
 * @macroname `PMCR_N_MASK`
 *
 * @description
 * The mask of the number of counters in PMCR_EL0.
 */
#define PMCR_N_MASK         0x1FUL

/**
 * @macroname `VMPIDR_RES1`
 *
 * @description
 * The bit of VMPIDR_EL2 that is reserved as 1.
 */
#define VMPIDR_RES1         (1UL << 31)

/**
 * @macroname `SCTLR_EL1_RESET`
 *
 * @description
 * The reset value of SCTLR_EL1 of the guest: the bits reserved as 1, with the
 * MMU and the caches off.
 */
#define SCTLR_EL1_RESET     0x30D00800UL

/**
 * @macroname `SPSR_EL1H_MASKED`
 *
 * @description
 * The PSTATE of the guest at its start: EL1h with D, A, I and F masked.
 */
#define SPSR_EL1H_MASKED    0x3C5UL

/**
 * @macroname `ESR_EC_SHIFT`
 *
 * @description
 * The position of the exception class in ESR_EL2.
 */
#define ESR_EC_SHIFT        26U

/**
 * @macroname `ESR_EC_MASK`
 *
 * @description
 * The mask of the exception class in ESR_EL2.
 */
#define ESR_EC_MASK         0x3FUL

/**
 * @macroname `ESR_ISS_IMM16`
 *
 * @description
 * The mask of the 16-bit immediate in the ISS of ESR_EL2.
 */
#define ESR_ISS_IMM16       0xFFFFUL

/**
 * @macroname `EC_WFX`
 *
 * @description
 * The exception class of a trapped WFI or WFE.
 */
#define EC_WFX              0x01U

/**
 * @macroname `EC_HVC64`
 *
 * @description
 * The exception class of an HVC from AArch64.
 */
#define EC_HVC64            0x16U

/**
 * @macroname `EC_SMC64`
 *
 * @description
 * The exception class of an SMC from AArch64.
 */
#define EC_SMC64            0x17U

/**
 * @macroname `EC_IABT_LOW`
 *
 * @description
 * The exception class of an instruction abort from a lower level.
 */
#define EC_IABT_LOW         0x20U

/**
 * @macroname `EC_DABT_LOW`
 *
 * @description
 * The exception class of a data abort from a lower level.
 */
#define EC_DABT_LOW         0x24U

/* MISRA C:2025 Dir 1.2 deviation: _Static_assert is a C11 keyword,
 * accepted by GCC in gnu99 mode; it checks the offsets vectors.S uses. */
_Static_assert(offsetof(hv_guest_regs_t, elr) == 248U, "regs elr");
_Static_assert(offsetof(hv_guest_regs_t, spsr) == 256U, "regs spsr");
_Static_assert(offsetof(hv_guest_regs_t, host_sp) == 264U,
               "regs host_sp");
_Static_assert(offsetof(hv_arch_vcpu_t, regs) == 0U, "vcpu regs");

/**
 * @macroname `GICD_IGROUPR0`
 *
 * @description
 * The offset of the group register of the interrupts 0-31.
 */
#define GICD_IGROUPR0       0x080U

/**
 * @macroname `GICD_ICENABLER0`
 *
 * @description
 * The offset of the clear-enable register of the interrupts 0-31.
 */
#define GICD_ICENABLER0     0x180U

/**
 * @macroname `GICD_ISPENDR0`
 *
 * @description
 * The offset of the set-pending register of the interrupts 0-31.
 */
#define GICD_ISPENDR0       0x200U

/**
 * @macroname `GICD_ISACTIVER0`
 *
 * @description
 * The offset of the set-active register of the interrupts 0-31.
 */
#define GICD_ISACTIVER0     0x300U

/**
 * @macroname `GICD_IPRIORITYR0`
 *
 * @description
 * The offset of the priority registers of the interrupts 0-31.
 */
#define GICD_IPRIORITYR0    0x400U

/**
 * @macroname `GICD_IPRIORITYR_END`
 *
 * @description
 * The offset after the priority registers of the interrupts 0-31.
 */
#define GICD_IPRIORITYR_END 0x420U

/**
 * @macroname `GICD_ITARGETSR0`
 *
 * @description
 * The offset of the target registers of the interrupts 0-31.
 */
#define GICD_ITARGETSR0     0x800U

/**
 * @macroname `GICD_ITARGETSR_END`
 *
 * @description
 * The offset after the target registers of the interrupts 0-31.
 */
#define GICD_ITARGETSR_END  0x820U

/**
 * @macroname `GICD_ICFGR0`
 *
 * @description
 * The offset of the configuration register of the SGIs.
 */
#define GICD_ICFGR0         0xC00U

/**
 * @macroname `GICD_ICFGR1`
 *
 * @description
 * The offset of the configuration register of the PPIs.
 */
#define GICD_ICFGR1         0xC04U

/**
 * @macroname `GICD_SGIR`
 *
 * @description
 * The offset of the software generated interrupt register.
 */
#define GICD_SGIR           0xF00U

/**
 * @macroname `GICD_SGI_PEND_FIRST`
 *
 * @description
 * The offset of the first SGI pending register (CPENDSGIRn and SPENDSGIRn).
 */
#define GICD_SGI_PEND_FIRST 0xF10U

/**
 * @macroname `GICD_SGI_PEND_END`
 *
 * @description
 * The offset after the SGI pending registers.
 */
#define GICD_SGI_PEND_END   0xF30U

/**
 * @macroname `GICD_ID_FIRST`
 *
 * @description
 * The offset of the first identification register.
 */
#define GICD_ID_FIRST       0xFD0U

/**
 * @macroname `GICD_ID_END`
 *
 * @description
 * The offset after the identification registers.
 */
#define GICD_ID_END         0x1000U

/**
 * @macroname `SGIS`
 *
 * @description
 * The mask of the SGIs (the interrupts 0-15, which are always enabled).
 */
#define SGIS                0x0000FFFFU

/**
 * @macroname `PRIORITY_BITS`
 *
 * @description
 * The bits of a priority that the virtual distributor implements.
 */
#define PRIORITY_BITS       0xF8U

/**
 * @macroname `ICFGR0_SGIS`
 *
 * @description
 * The value of the configuration register of the SGIs (edge-triggered).
 */
#define ICFGR0_SGIS         0xAAAAAAAAU

/**
 * @macroname `TARGETS_CPU0`
 *
 * @description
 * The value read from the target registers: every interrupt targets CPU 0.
 */
#define TARGETS_CPU0        0x01010101U

/**
 * @macroname `UART_RSR`
 *
 * @description
 * The offset of the receive status register of a PL011.
 */
#define UART_RSR            0x004U

/**
 * @macroname `UART_ILPR`
 *
 * @description
 * The offset of the IrDA low-power counter register of a PL011.
 */
#define UART_ILPR           0x020U

/**
 * @macroname `UART_IBRD`
 *
 * @description
 * The offset of the integer baud rate register of a PL011.
 */
#define UART_IBRD           0x024U

/**
 * @macroname `UART_FBRD`
 *
 * @description
 * The offset of the fractional baud rate register of a PL011.
 */
#define UART_FBRD           0x028U

/**
 * @macroname `UART_LCR_H`
 *
 * @description
 * The offset of the line control register of a PL011.
 */
#define UART_LCR_H          0x02CU

/**
 * @macroname `UART_CR`
 *
 * @description
 * The offset of the control register of a PL011.
 */
#define UART_CR             0x030U

/**
 * @macroname `UART_IFLS`
 *
 * @description
 * The offset of the interrupt FIFO level select register of a PL011.
 */
#define UART_IFLS           0x034U

/**
 * @macroname `UART_IMSC`
 *
 * @description
 * The offset of the interrupt mask set/clear register of a PL011.
 */
#define UART_IMSC           0x038U

/**
 * @macroname `UART_RIS`
 *
 * @description
 * The offset of the raw interrupt status register of a PL011.
 */
#define UART_RIS            0x03CU

/**
 * @macroname `UART_MIS`
 *
 * @description
 * The offset of the masked interrupt status register of a PL011.
 */
#define UART_MIS            0x040U

/**
 * @macroname `UART_ICR`
 *
 * @description
 * The offset of the interrupt clear register of a PL011.
 */
#define UART_ICR            0x044U

/**
 * @macroname `UART_DMACR`
 *
 * @description
 * The offset of the DMA control register of a PL011.
 */
#define UART_DMACR          0x048U

/**
 * @macroname `UART_CR_RESET`
 *
 * @description
 * The reset value of the control register of a PL011 (TXE and RXE).
 */
#define UART_CR_RESET       0x300U

/**
 * @macroname `ID_FIRST`
 *
 * @description
 * The offset of the first PrimeCell identification register (0xFE0-0xFFC).
 */
#define ID_FIRST            0xFE0U

/**
 * @macroname `ID_COUNT`
 *
 * @description
 * The number of the identification registers.
 */
#define ID_COUNT            8U

/** \cond */
// entry.S, vectors.S: the entry points call these.
void hv_main(void);
void hv_ap_main(uint64_t index);
void hv_el2_exception(uint64_t kind);

void hv_mmu_enable(const hv_mmu_regs_t *regs);
void hv_secondary_entry(void);
uint64_t hv_guest_enter(hv_guest_regs_t *regs);

// main.c
uint64_t hv_counter_hz(void);
void hv_udelay(uint64_t microseconds);

// mmu.c
void hv_mmu_init(void);

// fdt.c
bool fdt_read(uint64_t address, hv_fdt_info_t *info);

// pci.c
void pci_find_serial_ports(uint64_t ecam_base, uint64_t ecam_size);

// uart.c: host serial ports
void hv_uart_add(uint16_t port, uint64_t base, bool pl011);
bool hv_uart_present(uint16_t port);
bool hv_uart_is_pl011(uint16_t port);
uint64_t hv_uart_base(uint16_t port);
void hv_uart_write(uint16_t port, uint8_t byte);
bool hv_uart_rx_ready(uint16_t port);
uint8_t hv_uart_read(uint16_t port);
bool hv_uart_tx_empty(uint16_t port);

// smp.c, psci.c
void hv_smp_start(void);
int64_t psci_host_cpu_on(uint64_t mpidr, uint64_t entry, uint64_t context);
hv_action_e psci_guest_call(hv_vcpu_t *vcpu, uint32_t immediate);

// gic.c
void gic_init(const hv_fdt_info_t *info);
void gic_init_cpu(void);
void gic_cpu_off(void);
void gic_vcpu_reset(void);
void gic_handle_irq(hv_cpu_t *cpu);
void gic_update(hv_vm_t *vm);
uint32_t gic_host_iidr(void);
uint64_t gic_gicv_base(void);
uint32_t gic_list_registers(void);

// vgicd.c
void vgicd_reset(hv_vgicd_t *gicd);
bool vgicd_timer_enabled(const hv_vgicd_t *gicd);
uint8_t vgicd_timer_priority(const hv_vgicd_t *gicd);
bool vgicd_access(hv_vm_t *vm, uint32_t offset, uint32_t size, bool write,
                  uint64_t *value);

// vuart.c: the guest's PL011s and PL031
void vuart_reset(hv_vuart_t *uart);
bool vuart_access(hv_vuart_t *uart, uint32_t offset, bool write,
                  uint64_t *value);
bool vrtc_access(uint32_t offset, bool write, uint64_t *value);

// mmio.c
hv_action_e mmio_abort(hv_vcpu_t *vcpu, uint64_t esr);

// stage2.c
uint64_t stage2_build(const hv_vm_t *vm, uint64_t gicv);

// vcpu.c
void hv_report_state(const hv_vcpu_t *vcpu);
/** \endcond */

#endif // HV_ARCH_INTERNAL_H
