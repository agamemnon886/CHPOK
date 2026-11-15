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
 * Modified by Kirill Brazhnikov, 2026-11-02: MISRA C:2025: the header
 * guard isn't a reserved identifier; U suffixes for unsigned constants.
 *
 * Modified by Kirill Brazhnikov, 2026-11-05: the second UART and the device
 * tree, for the GDB server.
 *
 * Modified by Kirill Brazhnikov, 2026-11-08: partitions' memory starts 16
 * MiB into the RAM (0x41000000, was 0x48000000), as on x86_64, so that a
 * VM of the hypervisor needs no more RAM on arm64.
 *
 * Modified by Kirill Brazhnikov, 2026-11-13: Doxygen comments for the file and
 * its macros.
 *
 * Modified by Kirill Brazhnikov, 2026-11-14: the macros of
 * boards/arm64-qemu/bsp.c and cons.c are moved here; BOARD_RAM_SIZE and
 * BOARD_PARTITION_PHYS_START are documented once, with their x86_64 definitions
 * (Doxygen merged the two); the Doxygen comment of the file is removed (header
 * files are not documented).
 *
 * Modified by Kirill Brazhnikov, 2026-11-15: moved from
 * boards/arm64-qemu/include/board/bsp.h (the board support packages are in
 * kernel/bsps); the comments name the new paths.
 */

#ifndef JET_ARM64_QEMU_BSP_H
#define JET_ARM64_QEMU_BSP_H

/**
 * @name Memory map
 * The physical address space of QEMU 'virt' with '-m 1G'.
 * @{
 */

/**
 * @macroname `BOARD_DEVICE_BASE`
 *
 * @description
 * Devices occupy the first GiB of the physical address space: its start.
 */
#define BOARD_DEVICE_BASE 0x00000000ULL

/**
 * @macroname `BOARD_DEVICE_SIZE`
 *
 * @description
 * The size of the device area (1 GiB).
 */
#define BOARD_DEVICE_SIZE 0x40000000ULL

/**
 * @macroname `BOARD_RAM_BASE`
 *
 * @description
 * RAM, as given with '-m 1G': its start. The kernel is linked at
 * 0x40100000.
 */
#define BOARD_RAM_BASE    0x40000000ULL

/*
 * The size of the RAM (1 GiB).
 *
 * Documented with its x86_64 definition:
 * kernel/bsps/x86_64-qemu/include/board/bsp.h.
 */
#define BOARD_RAM_SIZE    0x40000000ULL

/*
 * Partitions' memory, 16 MiB into the RAM (as on x86_64): its physical
 * start address. Below it: the kernel image (with partitions' ELFs
 * embedded) and the kernel heap.
 *
 * Documented with its x86_64 definition:
 * kernel/bsps/x86_64-qemu/include/board/bsp.h.
 */
#define BOARD_PARTITION_PHYS_START 0x41000000ULL

/** @} */

/**
 * @name Devices
 * The physical addresses of the devices of QEMU 'virt' that the kernel
 * uses, and the interrupt of its timer.
 * @{
 */

/**
 * @macroname `BOARD_GICD_BASE`
 *
 * @description
 * GICv2 distributor: its registers.
 */
#define BOARD_GICD_BASE   0x08000000ULL

/**
 * @macroname `BOARD_GICC_BASE`
 *
 * @description
 * GICv2 CPU interface: its registers.
 */
#define BOARD_GICC_BASE   0x08010000ULL

/**
 * @macroname `BOARD_UART_BASE`
 *
 * @description
 * The first PL011 UART: the main console.
 */
#define BOARD_UART_BASE   0x09000000ULL

/**
 * @macroname `BOARD_UART1_BASE`
 *
 * @description
 * The second PL011, for the GDB server. QEMU creates it only for a second
 * -serial option, and then lists it (pl011@9040000) in the device tree.
 */
#define BOARD_UART1_BASE  0x09040000ULL

/**
 * @macroname `BOARD_UART1_NODE`
 *
 * @description
 * The name of the node of the second PL011 in the device tree, by which
 * the kernel finds out whether it exists.
 */
#define BOARD_UART1_NODE  "pl011@9040000"

/**
 * @macroname `BOARD_DTB_BASE`
 *
 * @description
 * QEMU puts the device tree at the beginning of RAM: its address.
 */
#define BOARD_DTB_BASE    BOARD_RAM_BASE

/**
 * @macroname `BOARD_RTC_BASE`
 *
 * @description
 * The PL031 RTC: its registers.
 */
#define BOARD_RTC_BASE    0x09010000ULL

/**
 * @macroname `BOARD_IRQ_VTIMER`
 *
 * @description
 * PPI of the EL1 virtual timer: its interrupt number (the system tick).
 */
#define BOARD_IRQ_VTIMER  27U

/** @} */

/**
 * @name GICv2 registers
 * The offsets of the registers of the GICv2 distributor (from
 * BOARD_GICD_BASE) and of its CPU interface (from BOARD_GICC_BASE) that
 * the board uses.
 * @{
 */

/**
 * @macroname `GICD_CTLR`
 *
 * @description
 * The distributor control register (1 enables the forwarding of
 * interrupts to the CPU interfaces).
 */
#define GICD_CTLR           0x000U

/**
 * @macroname `GICD_ISENABLER`
 *
 * @description
 * The offset of the n-th set-enable register of the distributor, which
 * enables interrupts 32 * n to 32 * n + 31 (one bit each).
 *
 * @params
 * | Name | Description |
 * |------|-------------|
 * | `n` | The number of the register (an interrupt number divided by 32). |
 */
#define GICD_ISENABLER(n)   (0x100U + (4U * (n)))

/**
 * @macroname `GICD_IPRIORITYR`
 *
 * @description
 * The offset of the priority byte of an interrupt in the distributor
 * (lower values are higher priorities).
 *
 * @params
 * | Name | Description |
 * |------|-------------|
 * | `n` | The interrupt number. |
 */
#define GICD_IPRIORITYR(n)  (0x400U + (n))

/**
 * @macroname `GICC_CTLR`
 *
 * @description
 * The CPU interface control register (1 enables the signalling of
 * interrupts to the CPU).
 */
#define GICC_CTLR           0x000U

/**
 * @macroname `GICC_PMR`
 *
 * @description
 * The priority mask register of the CPU interface: only interrupts with a
 * higher priority (a lower value) are signalled.
 */
#define GICC_PMR            0x004U

/**
 * @macroname `GICC_IAR`
 *
 * @description
 * The interrupt acknowledge register of the CPU interface: reading it
 * gives the number of the pending interrupt and makes it active.
 */
#define GICC_IAR            0x00cU

/**
 * @macroname `GICC_EOIR`
 *
 * @description
 * The end of interrupt register of the CPU interface: writing the value
 * read from GICC_IAR ends that interrupt.
 */
#define GICC_EOIR           0x010U

/** @} */

/**
 * @macroname `GIC_SPURIOUS`
 *
 * @description
 * The first interrupt number that GICC_IAR gives for no interrupt (1020 to
 * 1023: spurious or special); a number below it is a real interrupt.
 */
#define GIC_SPURIOUS        1020U

/**
 * @macroname `RTC_DR`
 *
 * @description
 * The data register of the PL031 RTC (offset from BOARD_RTC_BASE): the
 * current calendar time, in seconds since the Epoch.
 */
#define RTC_DR              0x000U

/**
 * @name Virtual timer control bits
 * The bits of CNTV_CTL_EL0, the control register of the EL1 virtual timer.
 * @{
 */

/**
 * @macroname `CNTV_CTL_ENABLE`
 *
 * @description
 * Enables the timer.
 */
#define CNTV_CTL_ENABLE     (1U << 0)

/**
 * @macroname `CNTV_CTL_ISTATUS`
 *
 * @description
 * Set when the timer condition holds (the counter has reached the compare
 * value).
 */
#define CNTV_CTL_ISTATUS    (1U << 2)

/** @} */

/**
 * @macroname `PSCI_SYSTEM_RESET`
 *
 * @description
 * The PSCI function ID of SYSTEM_RESET, which resets the system (QEMU
 * restarts the machine).
 */
#define PSCI_SYSTEM_RESET   0x84000009U

/*
 * Set by entry.S. PSCI uses smc if the kernel was entered at EL2, hvc
 * otherwise.
 */
extern char ja_boot_from_el2;

/**
 * @name PL011 UART registers
 * The offsets of the registers of a PL011 UART (from its base address)
 * and the bits of its flag register that the console uses.
 * @{
 */

/**
 * @macroname `UART_DR`
 *
 * @description
 * The data register: writing it sends a byte, reading it gives the
 * received byte.
 */
#define UART_DR 0x00U

/**
 * @macroname `UART_FR`
 *
 * @description
 * The flag register (UART_FR_RXFE, UART_FR_TXFF).
 */
#define UART_FR 0x18U

/**
 * @macroname `UART_FR_RXFE`
 *
 * @description
 * UART_FR: the receive FIFO is empty (no byte to read).
 */
#define   UART_FR_RXFE (1U << 4)

/**
 * @macroname `UART_FR_TXFF`
 *
 * @description
 * UART_FR: the transmit FIFO is full (no byte can be written).
 */
#define   UART_FR_TXFF (1U << 5)

/** @} */

/**
 * @name Device tree format
 * Device tree (flattened): header fields, tokens. The offsets of the
 * header fields (big-endian 32-bit words) from the start of the device
 * tree, the size of the header, and the tokens of its structure block.
 * @{
 */

/**
 * @macroname `FDT_MAGIC`
 *
 * @description
 * The first word of a device tree (its magic number).
 */
#define FDT_MAGIC          0xD00DFEEDU

/**
 * @macroname `FDT_OFF_TOTALSIZE`
 *
 * @description
 * The offset of the header field totalsize: the size of the whole device
 * tree.
 */
#define FDT_OFF_TOTALSIZE  4U

/**
 * @macroname `FDT_OFF_STRUCT`
 *
 * @description
 * The offset of the header field off_dt_struct: the offset of the
 * structure block.
 */
#define FDT_OFF_STRUCT     8U

/**
 * @macroname `FDT_SIZE_STRUCT`
 *
 * @description
 * The offset of the header field size_dt_struct: the size of the
 * structure block.
 */
#define FDT_SIZE_STRUCT    36U

/**
 * @macroname `FDT_HEADER_SIZE`
 *
 * @description
 * The size of the header (version 17), the smallest valid device tree.
 */
#define FDT_HEADER_SIZE    40U

/**
 * @macroname `FDT_BEGIN_NODE`
 *
 * @description
 * The token that starts a node (followed by its name).
 */
#define FDT_BEGIN_NODE     1U

/**
 * @macroname `FDT_END_NODE`
 *
 * @description
 * The token that ends a node.
 */
#define FDT_END_NODE       2U

/**
 * @macroname `FDT_PROP`
 *
 * @description
 * The token of a property (followed by its length, its name offset and
 * its value).
 */
#define FDT_PROP           3U

/**
 * @macroname `FDT_NOP`
 *
 * @description
 * A token to ignore.
 */
#define FDT_NOP            4U

/** @} */

/**
 * @macroname `FDT_SIZE_MAX`
 *
 * @description
 * Sanity limit of the size of the device tree: a larger totalsize means
 * that there is no valid device tree.
 */
#define FDT_SIZE_MAX       0x100000U

#endif /* JET_ARM64_QEMU_BSP_H */
