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
 *
 * Modified by Kirill Brazhnikov, 2026-11-14: the types are typedefs without a
 * tag, used by their typedef names, and are documented with their macros.
 */
# ifndef __POK__PCI_CONFIG_H__
# define __POK__PCI_CONFIG_H__
#include <stdint.h>

#include <pci.h>

/**
 * @typename `pci_resource_config_t`
 *
 * @description
 * The configuration of a resource of a PCI device: its virtual and PCI
 * addresses and kind.
 */
typedef struct {
    uintptr_t addr; /**< Virtual addr. */
    uint64_t  pci_addr; /**< "physical" addr in pci address space. */
    pci_resource_type_e type; /**< Its kind. */
} pci_resource_config_t;

/**
 * @typename `pci_dev_config_t`
 *
 * @description
 * The configuration of a PCI device (the deployment of the partition): its
 * address and resources.
 */
typedef struct {
    uint16_t    bus; /**< Its bus. */
    uint16_t    dev; /**< Its device number. */
    uint16_t    fn; /**< Its function. */
    /**
     * Its resources (pci_resource_index_e).
     */
    pci_resource_config_t resources[PCI_NUM_RESOURCES];
} pci_dev_config_t;

extern pci_dev_config_t pci_configs[];
extern uint32_t pci_configs_nb;
# endif
