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
 * Stage-2 translation of a VM: guest-physical addresses (32 bits, 4 KiB
 * granule, starting at level 1) to host-physical ones.
 *
 *   0x08010000-0x08011FFF  the GIC CPU interface: the host's GICV
 *   0x09000000, 0x09040000 a PL011 passed through: the host's PL011
 *   0x40000000-...         the VM's RAM, in 2 MiB blocks
 *
 * Nothing else is mapped: the distributor, the emulated PL011s and the
 * RTC exit to the hypervisor (mmio.c), any other access stops the VM.
 */

#include <arch/internal.h>

#define ENTRIES             512U
#define L1_SHIFT            30U
#define L2_SHIFT            21U
#define L3_SHIFT            12U

#define DESC_TABLE          0x3UL
#define DESC_BLOCK          0x1UL
#define DESC_PAGE           0x3UL
#define S2_MEMATTR_NORMAL   (0xFUL << 2)    // Write-back, inner and outer
#define S2_MEMATTR_DEVICE   (0x1UL << 2)    // Device-nGnRE
#define S2_AP_RW            (3UL << 6)
#define S2_SH_INNER         (3UL << 8)
#define S2_AF               (1UL << 10)
#define S2_XN               (1UL << 54)
#define S2_RAM              (DESC_BLOCK | S2_MEMATTR_NORMAL | S2_AP_RW \
                             | S2_SH_INNER | S2_AF)
#define S2_DEVICE           (DESC_PAGE | S2_MEMATTR_DEVICE | S2_AP_RW \
                             | S2_AF | S2_XN)

#define VTTBR_VMID_SHIFT    48U

// The tables of a VM.
typedef struct {
    uint64_t l1[ENTRIES];       // 4 entries used (4 GiB)
    uint64_t l2_devices[ENTRIES];
    uint64_t l3_gic[ENTRIES];
    uint64_t l3_uart[ENTRIES];
    uint64_t l2_ram[ENTRIES];
} stage2_tables_t;

/*
 * MISRA C:2025 Rule 11.4 deviation (table_pa()): the tables are given to
 * the MMU by their physical addresses (identity-mapped), converted from
 * pointers.
 */
static uint64_t table_pa(const uint64_t *table)
{
    return (uint64_t) (uintptr_t) table;
}

// Index of address in a table of the level with this shift.
static uint32_t index_of(uint64_t address, uint32_t shift)
{
    return (uint32_t) ((address >> shift) & (ENTRIES - 1U));
}

// Map the 4 KiB page at guest-physical ipa to host-physical device page pa.
static void map_device(stage2_tables_t *t, uint64_t ipa, uint64_t pa)
{
    uint64_t *l3 = (index_of(ipa, L2_SHIFT) == index_of(HV_GUEST_GICD,
                                                        L2_SHIFT))
                   ? t->l3_gic : t->l3_uart;

    l3[index_of(ipa, L3_SHIFT)] = pa | S2_DEVICE;
}

/*
 * Build the tables of a VM (its RAM placed, its UARTs set up) with the
 * host's GICV at gicv; returns its VTTBR_EL2 (VMID: VM index + 1).
 */
uint64_t stage2_build(const hv_vm_t *vm, uint64_t gicv)
{
    static stage2_tables_t tables[HV_MAX_VMS] HV_ALIGNED(4096);
    stage2_tables_t *t = &tables[vm->index];
    uint64_t blocks = vm->config->memory_size >> L2_SHIFT;
    const uint64_t uarts[2] = { HV_GUEST_UART0, HV_GUEST_UART1 };

    hv_memset(t, 0U, sizeof(*t));
    t->l1[index_of(HV_GUEST_GICD, L1_SHIFT)] = table_pa(t->l2_devices)
                                               | DESC_TABLE;
    t->l1[index_of(HV_GUEST_RAM, L1_SHIFT)] = table_pa(t->l2_ram)
                                              | DESC_TABLE;
    t->l2_devices[index_of(HV_GUEST_GICD, L2_SHIFT)] = table_pa(t->l3_gic)
                                                       | DESC_TABLE;
    t->l2_devices[index_of(HV_GUEST_UART0, L2_SHIFT)] =
        table_pa(t->l3_uart) | DESC_TABLE;

    // The guest's GIC CPU interface is the host's virtual CPU interface.
    for (uint64_t offset = 0U; offset < HV_GUEST_GICC_SIZE;
         offset += HV_PAGE_SIZE) {
        map_device(t, HV_GUEST_GICC + offset, gicv + offset);
    }
    for (uint32_t i = 0U; i < 2U; i++) {
        const hv_vuart_t *uart = &vm->arch.uart[i];

        if (uart->passthrough) {
            map_device(t, uarts[i], hv_uart_base(uart->host_port));
        }
    }
    for (uint64_t i = 0U; (i < blocks) && (i < ENTRIES); i++) {
        t->l2_ram[i] = (vm->ram_base + (i << L2_SHIFT)) | S2_RAM;
    }
    arm64_dsb_ish();
    return ((uint64_t) (vm->index + 1U) << VTTBR_VMID_SHIFT)
           | table_pa(t->l1);
}
