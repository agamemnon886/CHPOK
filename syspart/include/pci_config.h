/*
 * Institute for System Programming of the Russian Academy of Sciences
 * Copyright (C) 2016 ISPRAS
 * Copyright (C) 2026 Kirill Brazhnikov <agamemnon886@gmail.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation, Version 3.
 *
 * This program is distributed in the hope # that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * See the GNU General Public License version 3 for more details.
 *
 * Modified by Kirill Brazhnikov, 2026-10-31: built-in integer types (int,
 * unsigned, short, long, ...) are replaced with the fixed-width types
 * from <stdint.h>.
 */
# ifndef __POK__PCI_CONFIG_H__
# define __POK__PCI_CONFIG_H__
#include <stdint.h>

#include <pci.h>

struct pci_resource_config {
    uintptr_t addr; //virtual addr
    uint64_t  pci_addr; //"physical" addr in pci address space
    enum PCI_RESOURCE_TYPE type;
};

struct pci_dev_config
{
    uint16_t    bus;
    uint16_t    dev;
    uint16_t    fn;
    struct pci_resource_config resources[PCI_NUM_RESOURCES];
};

extern struct pci_dev_config pci_configs[];
extern uint32_t pci_configs_nb;
# endif
