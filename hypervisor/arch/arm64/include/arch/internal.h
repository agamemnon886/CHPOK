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
 * Interfaces between the arm64 parts of the hypervisor (arch/arm64), and
 * the addresses of QEMU 'virt' (-machine virt,virtualization=on,
 * gic-version=2) that they use: the host's devices, and what a VM sees.
 */

#ifndef HV_ARCH_INTERNAL_H
#define HV_ARCH_INTERNAL_H

#include <hv/hv.h>

// Host: QEMU 'virt' devices (the GIC is found in the device tree).
#define HV_HOST_UART0       0x09000000UL    // PL011, COM1
#define HV_HOST_RTC         0x09010000UL    // PL031
#define HV_HOST_UART1       0x09040000UL    // PL011, COM2
#define HV_HOST_PCI_IO      0x3EFF0000UL    // PCI I/O space window
#define HV_HOST_RAM_BASE    0x40000000UL    // The device tree is put here

// Guest-physical memory map of a VM (as QEMU 'virt' has it).
#define HV_GUEST_GICD       0x08000000UL
#define HV_GUEST_GICC       0x08010000UL    // 8 KiB, the host's GICV
#define HV_GUEST_GICC_SIZE  0x2000UL
#define HV_GUEST_UART0      0x09000000UL
#define HV_GUEST_RTC        0x09010000UL
#define HV_GUEST_UART1      0x09040000UL
#define HV_GUEST_RAM        0x40000000UL
#define HV_GUEST_DTB_SPACE  0x10000UL       // Room of the device tree

// The guest's virtual timer interrupt (PPI 11).
#define HV_IRQ_VTIMER       27U

// Why hv_guest_enter() returned (vectors.S).
#define HV_EXIT_SYNC        0U
#define HV_EXIT_IRQ         1U
#define HV_EXIT_FIQ         2U
#define HV_EXIT_SERROR      3U
#define HV_EXIT_AARCH32     4U

// What the host's device tree says (fdt.c).
typedef struct {
    uint64_t ram_base;
    uint64_t ram_size;
    uint32_t nb_cpus;           // CPUs found (at most HV_MAX_CPUS kept)
    uint32_t mpidr[HV_MAX_CPUS];
    uint64_t ecam_base;         // PCI configuration space (0: none)
    uint64_t ecam_size;
    bool gic_v2;                // A GICv2 ("arm,cortex-a15-gic")
    uint32_t gic_regs;          // Number of GIC register ranges (4: with
                                // the virtualization extensions)
    uint64_t gicd;              // Distributor
    uint64_t gicc;              // CPU interface
    uint64_t gich;              // Virtual interface control
    uint64_t gicv;              // Virtual CPU interface
    uint64_t gicv_size;
    bool uart1;                 // The second PL011 (COM2) exists
} hv_fdt_info_t;

// Values of the EL2 translation registers (mmu.c; entry.S loads them).
typedef struct {
    uint64_t mair;
    uint64_t tcr;
    uint64_t ttbr0;
    uint64_t sctlr;
} hv_mmu_regs_t;

extern hv_mmu_regs_t hv_el2_mmu;

// entry.S, vectors.S
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
hv_action_t psci_guest_call(hv_vcpu_t *vcpu, uint32_t immediate);

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
hv_action_t mmio_abort(hv_vcpu_t *vcpu, uint64_t esr);

// stage2.c
uint64_t stage2_build(const hv_vm_t *vm, uint64_t gicv);

// vcpu.c
void hv_report_state(const hv_vcpu_t *vcpu);

#endif // HV_ARCH_INTERNAL_H
