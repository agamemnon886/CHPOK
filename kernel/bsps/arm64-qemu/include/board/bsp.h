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
 * kernel/bsps); the comments name the new paths; the registers of the GIC, the
 * PL011, the PL031 and PSCI, and the device tree format, which the hypervisor
 * uses too, are in the shared headers (include/bsps/arm64-qemu/dev,
 * include/common/fdt.h).
 */

#ifndef JET_ARM64_QEMU_BSP_H
#define JET_ARM64_QEMU_BSP_H

/*
 * The registers of the devices of the board and the device tree format,
 * which the hypervisor uses too.
 */
#include <common/fdt.h>
#include <dev/gicv2.h>
#include <dev/pl011.h>
#include <dev/pl031.h>
#include <dev/psci.h>

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

/*
 * Set by entry.S. PSCI uses smc if the kernel was entered at EL2, hvc
 * otherwise.
 */
extern char ja_boot_from_el2;

#endif /* JET_ARM64_QEMU_BSP_H */
