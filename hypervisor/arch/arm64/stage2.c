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
 */

/**
 * @file
 * Stage-2 translation of a VM: guest-physical addresses (32 bits, 4 KiB
 * granule, starting at level 1) to host-physical ones. The GIC CPU interface
 * (0x08010000-0x08011FFF) is the host's GICV, a PL011 passed through
 * (0x09000000, 0x09040000) is the host's PL011, and the VM's RAM
 * (0x40000000 on) is mapped in 2 MiB blocks. Nothing else is mapped: the
 * distributor, the emulated PL011s and the RTC exit to the hypervisor
 * (mmio.c), any other access stops the VM.
 */

#include <arch/internal.h>

/**
 * @funcname `table_pa`
 *
 * @description
 * Returns the physical address of a stage-2 table (identity-mapped).
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `table` | `const uint64_t *` | [in] The table. |
 *
 * @return Its physical address.
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
 * 1. Return `(uint64_t) (uintptr_t) table`.
 *
 * @calls
 * None.
 */
/*
 * MISRA C:2025 Rule 11.4 deviation (table_pa()): the tables are given to
 * the MMU by their physical addresses (identity-mapped), converted from
 * pointers.
 */
static uint64_t table_pa(const uint64_t *table)
{
    return (uint64_t) (uintptr_t) table;
}

/**
 * @funcname `index_of`
 *
 * @description
 * Returns the index of an address in a table of a level.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `address` | `uint64_t` | [in] The address. |
 * | `shift` | `uint32_t` | [in] The shift of the level. |
 *
 * @return The index, 0-511.
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
 * 1. Return `(uint32_t) ((address >> shift) & (ENTRIES - 1U))`.
 *
 * @calls
 * None.
 */
static uint32_t index_of(uint64_t address, uint32_t shift)
{
    return (uint32_t) ((address >> shift) & (ENTRIES - 1U));
}

/**
 * @funcname `map_device`
 *
 * @description
 * Maps a 4 KiB guest-physical page to a host-physical device page (device
 * memory, not executable).
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `t` | `stage2_tables_t *` | [in] The VM's tables. |
 * | `ipa` | `uint64_t` | [in] The guest-physical page. |
 * | `pa` | `uint64_t` | [in] The host-physical page. |
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `l3` | `uint64_t *` | The level 3 table of the page: the GIC's or the UARTs' |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Set `l3` to `t->l3_gic` if
 *    `index_of(ipa, L2_SHIFT) == index_of(HV_GUEST_GICD, L2_SHIFT)` holds,
 *    otherwise `t->l3_uart`.
 * 2. Set `l3[index_of(ipa, L3_SHIFT)]` to `pa | S2_DEVICE`.
 *
 * @calls
 * - `index_of()`
 */
static void map_device(stage2_tables_t *t, uint64_t ipa, uint64_t pa)
{
    uint64_t *l3 = (index_of(ipa, L2_SHIFT) == index_of(HV_GUEST_GICD,
                                                        L2_SHIFT))
                   ? t->l3_gic : t->l3_uart;

    l3[index_of(ipa, L3_SHIFT)] = pa | S2_DEVICE;
}

/**
 * @funcname `stage2_build`
 *
 * @description
 * Builds the stage-2 tables of a VM, once its RAM is placed and its UARTs are
 * set up.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `vm` | `const hv_vm_t *` | [in] The VM. |
 * | `gicv` | `uint64_t` | [in] The physical address of the host's virtual CPU interface. |
 *
 * @return The VM's VTTBR_EL2: the tables and the VMID (VM index + 1).
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `tables` | `static stage2_tables_t[HV_MAX_VMS]` | The tables of the VMs |
 * | `t` | `stage2_tables_t *` | The tables of this VM |
 * | `blocks` | `uint64_t` | The number of 2 MiB blocks of RAM |
 * | `uarts` | `const uint64_t[2]` | The guest-physical pages of the two PL011s |
 * | `offset` | `uint64_t` | The offset of a GICC page |
 * | `uart` | `const hv_vuart_t *` | A guest UART |
 * | `i` | `uint64_t` | The index of a UART (uint32_t) or of a RAM block (uint64_t) |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Set `t` to the address of `tables[vm->index]`.
 * 2. Set `blocks` to `vm->config->memory_size >> L2_SHIFT`.
 * 3. Set `uarts` to `{ HV_GUEST_UART0, HV_GUEST_UART1 }`.
 * 4. Call `hv_memset()` with the arguments `t`, `0U` and `sizeof(*t)`.
 * 5. Set `t->l1[index_of(HV_GUEST_GICD, L1_SHIFT)]` to
 *    `table_pa(t->l2_devices) | DESC_TABLE`.
 * 6. Set `t->l1[index_of(HV_GUEST_RAM, L1_SHIFT)]` to
 *    `table_pa(t->l2_ram) | DESC_TABLE`.
 * 7. Set `t->l2_devices[index_of(HV_GUEST_GICD, L2_SHIFT)]` to
 *    `table_pa(t->l3_gic) | DESC_TABLE`.
 * 8. Set `t->l2_devices[index_of(HV_GUEST_UART0, L2_SHIFT)]` to
 *    `table_pa(t->l3_uart) | DESC_TABLE`.
 * 9. Set `offset` to `0U`; then while `offset` is less than
 *    `HV_GUEST_GICC_SIZE`, repeat (after each pass: add `HV_PAGE_SIZE` to
 *    `offset`):
 *    1. Call `map_device()` with the arguments `t`, `HV_GUEST_GICC + offset`
 *       and `gicv + offset`.
 * 10. Set `i` to `0U`; then while `i` is less than `2U`, repeat (after each
 *     pass: increment `i`):
 *     1. Set `uart` to the address of `vm->arch.uart[i]`.
 *     2. If `uart->passthrough` is true:
 *        1. Call `map_device()` with the arguments `t`, `uarts[i]` and
 *           `hv_uart_base(uart->host_port)`.
 * 11. Set `i` to `0U`; then while `i` is less than `blocks` and `i` is less
 *     than `ENTRIES`, repeat (after each pass: increment `i`):
 *     1. Set `t->l2_ram[i]` to `(vm->ram_base + (i << L2_SHIFT)) | S2_RAM`.
 * 12. Call `arm64_dsb_ish()`.
 * 13. Return
 *     `((uint64_t) (vm->index + 1U) << VTTBR_VMID_SHIFT) | table_pa(t->l1)`.
 *
 * @calls
 * - `hv_memset()`
 * - `index_of()`
 * - `table_pa()`
 * - `map_device()`
 * - `hv_uart_base()`
 * - `arm64_dsb_ish()`
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
