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

/* QEMU 'virt' machine (qemu-system-aarch64 -M virt -m 1G). */

#ifndef __JET_ARM64_QEMU_BSP_H__
#define __JET_ARM64_QEMU_BSP_H__

/* Devices occupy the first GiB of the physical address space. */
#define BOARD_DEVICE_BASE 0x00000000ULL
#define BOARD_DEVICE_SIZE 0x40000000ULL

/* RAM, as given with '-m 1G'. The kernel is linked at 0x40100000. */
#define BOARD_RAM_BASE    0x40000000ULL
#define BOARD_RAM_SIZE    0x40000000ULL

/*
 * Partitions' memory. Below it: the kernel image (with partitions'
 * ELFs embedded) and the kernel heap.
 */
#define BOARD_PARTITION_PHYS_START 0x48000000ULL

#define BOARD_GICD_BASE   0x08000000ULL /* GICv2 distributor */
#define BOARD_GICC_BASE   0x08010000ULL /* GICv2 CPU interface */
#define BOARD_UART_BASE   0x09000000ULL /* PL011 */
#define BOARD_RTC_BASE    0x09010000ULL /* PL031 */

/* PPI of the EL1 virtual timer. */
#define BOARD_IRQ_VTIMER  27

#endif /* __JET_ARM64_QEMU_BSP_H__ */
