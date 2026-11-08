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
 * Modified by Kirill Brazhnikov, 2026-11-08: the CPU limit and the serial
 * ports come from the architecture (<arch/config.h>); boot_data is what
 * the loader puts at the start of the guest RAM besides the image (the
 * device tree on arm64).
 */

/*
 * Configuration of the virtual machines, generated from the XML file by
 * hvconfig.py (build/hv_config.c, from templates/hv_config.c.tpl).
 */

#ifndef HV_CONFIG_H
#define HV_CONFIG_H

#include <stdint.h>
#include <stddef.h>

#include <arch/config.h>

// Limit of the VM table; hvconfig.py checks it too (and HV_MAX_CPUS).
#define HV_MAX_VMS      8U

/*
 * The serial ports, HV_PORT_COM1 and up, are defined by <arch/config.h>;
 * HV_PORT_NONE is no port.
 */
#define HV_PORT_NONE    0U

typedef struct {
    const char *name;
    uint64_t memory_size;       // Guest RAM, bytes (multiple of 2 MiB)
    uint32_t nb_cpus;
    uint16_t console_port;      // Host port for the guest's console
    uint16_t gdb_port;          // Host port for the guest's GDB port, or none
    const uint8_t *image;       // Loadable bytes of the image
    const uint64_t *image_size; // Their size (defined in hv_images.S).
    uint32_t load_addr;         // Guest-physical address of image[0]
    uint32_t bss_end;           // End of the zeroed area after the image
    uint32_t entry;             // Entry point (Multiboot: 32-bit)
    const uint8_t *boot_data;   // Copied to the start of the RAM, or NULL
    const uint64_t *boot_data_size;
} hv_vm_config_t;

extern const hv_vm_config_t hv_vm_configs[];
extern const uint32_t hv_nb_vms;
extern const uint16_t hv_console_port;

#endif // HV_CONFIG_H
