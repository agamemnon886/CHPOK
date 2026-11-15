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
 * System control port B of the PC, with the gate and the output of
 * channel 2 of the 8254 timer, for the kernel (kernel/bsps/x86_64-qemu),
 * which calibrates its clock with it, and the hypervisor
 * (hypervisor/bsps/x86_64-qemu), which also emulates it for its VMs.
 *
 * The definitions were in kernel/bsps/x86_64-qemu/pit.h and in
 * hypervisor/csps/x86_64/include/arch/internal.h, each with its own copy.
 */

#ifndef JET_DEV_I8254_H
#define JET_DEV_I8254_H

/**
 * @macroname `PORT_B`
 *
 * @description
 * The I/O port of the system control port B.
 */
#define PORT_B              0x61U

/**
 * @macroname `PORT_B_GATE2`
 *
 * @description
 * The bit of port B that gates channel 2 of the PIT.
 */
#define PORT_B_GATE2        0x01U

/**
 * @macroname `PORT_B_SPEAKER`
 *
 * @description
 * The bit of port B that connects channel 2 to the speaker.
 */
#define PORT_B_SPEAKER      0x02U

/**
 * @macroname `PORT_B_OUT2`
 *
 * @description
 * The bit of port B that reads the output of channel 2.
 */
#define PORT_B_OUT2         0x20U

#endif /* JET_DEV_I8254_H */
