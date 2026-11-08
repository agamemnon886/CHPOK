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
 * arm64 limits and serial ports (used by <hv/config.h> and the generated
 * VM table). The COM ports are numbered: COM1 and COM2 are the PL011s of
 * QEMU 'virt' (0x09000000 and 0x09040000), COM3-COM16 are 16550s on the
 * PCI bus (QEMU pci-serial, in slot 0x10 + n - 3 for COMn).
 */

#ifndef HV_ARCH_CONFIG_H
#define HV_ARCH_CONFIG_H

// Limit of the CPU table (GICv2 has at most 8 CPUs); hvconfig.py checks it.
#define HV_MAX_CPUS     8U

#define HV_PORT_COM1    1U
#define HV_PORT_COM2    2U
#define HV_PORT_COM3    3U
#define HV_PORT_COM4    4U
#define HV_PORT_COM5    5U
#define HV_PORT_COM6    6U
#define HV_PORT_COM7    7U
#define HV_PORT_COM8    8U
#define HV_PORT_COM9    9U
#define HV_PORT_COM10   10U
#define HV_PORT_COM11   11U
#define HV_PORT_COM12   12U
#define HV_PORT_COM13   13U
#define HV_PORT_COM14   14U
#define HV_PORT_COM15   15U
#define HV_PORT_COM16   16U

// Number of COM ports.
#define HV_NB_PORTS     16U

#endif // HV_ARCH_CONFIG_H
