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
 * Registers of the Arm GICv2 interrupt controller of QEMU 'virt', for
 * the kernel (kernel/bsps/arm64-qemu) and the hypervisor
 * (hypervisor/bsps/arm64-qemu), which has the registers that only it
 * uses in its own header.
 *
 * The definitions were in the board header of the kernel
 * (kernel/bsps/arm64-qemu/include/board/bsp.h) and in the internal
 * header of the hypervisor
 * (hypervisor/csps/arm64/include/arch/internal.h), each with its own
 * copy.
 */

#ifndef JET_DEV_GICV2_H
#define JET_DEV_GICV2_H

/**
 * @name GICv2 registers
 * The offsets of the registers of the GICv2 distributor (from the
 * address of the distributor) and of its CPU interface (from the
 * address of the CPU interface) that both the kernel and the
 * hypervisor use.
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

#endif /* JET_DEV_GICV2_H */
