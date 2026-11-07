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
 * Configuration of the virtual machines, generated from the XML file by
 * hvconfig.py (build/hv_config.c, from templates/hv_config.c.tpl).
 */

#ifndef HV_CONFIG_H
#define HV_CONFIG_H

#include <stdint.h>

// Limits of the static tables; hvconfig.py checks them too.
#define HV_MAX_VMS      8U
#define HV_MAX_CPUS     16U

// Base I/O ports of the ISA serial ports.
#define HV_PORT_COM1    0x3F8U
#define HV_PORT_COM2    0x2F8U
#define HV_PORT_COM3    0x3E8U
#define HV_PORT_COM4    0x2E8U

// No serial port assigned.
#define HV_PORT_NONE    0U

typedef struct {
    const char *name;
    uint64_t memory_size;       // Guest RAM, bytes (multiple of 2 MiB)
    uint32_t nb_cpus;
    uint16_t console_port;      // Host port for guest COM1
    uint16_t gdb_port;          // Host port for guest COM2, or none
    const uint8_t *image;       // Loadable bytes of the image
    const uint64_t *image_size; // Their size (defined in hv_images.S).
    uint32_t load_addr;         // Guest-physical address of image[0]
    uint32_t bss_end;           // End of the zeroed area after the image
    uint32_t entry;             // Multiboot entry point (32-bit)
} hv_vm_config_t;

extern const hv_vm_config_t hv_vm_configs[];
extern const uint32_t hv_nb_vms;
extern const uint16_t hv_console_port;

#endif // HV_CONFIG_H
