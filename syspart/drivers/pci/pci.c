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
 * This file also incorporates work covered by POK License.
 * Copyright (c) 2007-2009 POK team
 *
 * Modified by Kirill Brazhnikov, 2026-10-31: removed PowerPC support.
 */

#include <pci.h>
#include <pci_config.h>
#include <ioports.h>
#include <stdio.h>

#include "pci_internal.h"

#include <string.h>

#define OK 0

#ifdef PCI_DEBUG
static char *get_pci_class_name(int classcode) {
    int i = 0;
    while (pci_classnames[i].name != NULL) {
        if (pci_classnames[i].classcode == (classcode & 0xFFFF00))
            return pci_classnames[i].name;
        i++;
    }
    return "Unknown";
}
#endif


int pci_read_config_byte(struct pci_dev *dev, int where, uint8_t *val)
{
    uint32_t addr = (uint32_t) (1 << 31) | (dev->bus << 16) | (dev->dev << 11) |
        (dev->fn << 8) | (where & 0xfc);

    outl(PCI_CONFIG_ADDRESS, addr);
    *val = inb(PCI_CONFIG_DATA + (where & 3));
    return OK;
}

int pci_read_config_word(struct pci_dev *dev, int where, uint16_t *val)
{
    uint32_t addr = (uint32_t) (1 << 31) | (dev->bus << 16) | (dev->dev << 11) |
        (dev->fn << 8) | (where & 0xfc);

    outl(PCI_CONFIG_ADDRESS, addr);
    *val = inw(PCI_CONFIG_DATA + (where & 2));
    return OK;
}

int pci_read_config_dword(struct pci_dev *dev, int where, uint32_t *val)
{
    uint32_t addr = (uint32_t) (1 << 31) | (dev->bus << 16) | (dev->dev << 11) |
        (dev->fn << 8) | (where & 0xfc);

    outl(PCI_CONFIG_ADDRESS, addr);
    *val = inl(PCI_CONFIG_DATA);
    return OK;
}

int pci_write_config_byte(struct pci_dev *dev, int where, uint8_t val)
{
    uint32_t addr = (uint32_t) (1 << 31) | (dev->bus << 16) | (dev->dev << 11) |
        (dev->fn << 8) | (where & 0xfc);

    outl(PCI_CONFIG_ADDRESS, addr);
    outb(PCI_CONFIG_DATA + (where & 3), val);
    return OK;
}

int pci_write_config_word(struct pci_dev *dev, int where, uint16_t val)
{
    uint32_t addr = (uint32_t) (1 << 31) | (dev->bus << 16) | (dev->dev << 11) |
        (dev->fn << 8) | (where & 0xfc);

    outl(PCI_CONFIG_ADDRESS, addr);
    outw(PCI_CONFIG_DATA + (where & 2), val);
    return OK;
}

int pci_write_config_dword(struct pci_dev *dev, int where, uint32_t val)
{
    uint32_t addr = (uint32_t) (1 << 31) | (dev->bus << 16) | (dev->dev << 11) |
        (dev->fn << 8) | (where & 0xfc);

    outl(PCI_CONFIG_ADDRESS, addr);
    outl(PCI_CONFIG_DATA, val);
    return OK;
}


// zero means that device config is not found
static uintptr_t get_resource_addr_from_config(uint8_t bus, uint8_t dev, uint8_t fn, enum PCI_RESOURCE_INDEX idx)
{
    //TODO optimize this linear search
    struct pci_dev_config *d;
    for (int i = 0; i < pci_configs_nb; i++) {
        d = &pci_configs[i];
        if (d->bus == bus && d->dev == dev && d->fn == fn)
            return d->resources[idx].addr;
    }

    return 0;
}



static void pci_fill_resource(struct pci_dev *dev, enum PCI_RESOURCE_INDEX res_idx)
{
    //TODO :
    // 1. Decode (I/O or memory) of a register is disabled via the command
    //       register before sizing a Base Address register.
    // 2. The original value in the Base Address register is restored before re-enabling
    //       decode in the command register of the device.
    // 3. "Note that the upper 16 bits of the result is ignored if the Base
    //       Address register is for I/O and bits 16-31 returned zero upon read."
    // 4. 64 bits bars
    //
    int reg;
    uint32_t val, size, mask;

    struct pci_resource *resource = &dev->resources[res_idx];
    memset(resource, 0, sizeof(*resource));


    if (res_idx != PCI_RESOURCE_ROM) {
        reg = PCI_BASE_ADDRESS_0 + res_idx*4;
        mask =  0xffffffff;
    } else {
        reg = PCI_ROM_ADDRESS;
        mask = PCI_ROM_ADDRESS_MASK;
    }

    pci_read_config_dword(dev, reg, &val);
    pci_write_config_dword(dev, reg, mask);
    pci_read_config_dword(dev, reg, &size);
    pci_write_config_dword(dev, reg, val);

    if (res_idx == PCI_RESOURCE_ROM)  {
        resource->type = PCI_RESOURCE_TYPE_ROM;
        resource->pci_addr = val & PCI_ROM_ADDRESS_MASK;
        size &= PCI_ROM_ADDRESS_MASK;

    } else if ((val&PCI_BASE_ADDRESS_SPACE) == PCI_BASE_ADDRESS_SPACE_IO) {
        resource->type = PCI_RESOURCE_TYPE_BAR_IO;
        resource->pci_addr = val & PCI_BASE_ADDRESS_IO_MASK;
        size &= PCI_BASE_ADDRESS_IO_MASK;

    } else {
        if ((val & PCI_BASE_ADDRESS_MEM_TYPE_MASK) == PCI_BASE_ADDRESS_MEM_TYPE_32) {
            resource->mem_flags |= PCI_RESOURCE_MEM_MASK_32;
        }
        if (val & PCI_BASE_ADDRESS_MEM_PREFETCH) {
            resource->mem_flags |= PCI_RESOURCE_MEM_MASK_PREFETCH;
        }
        resource->type = PCI_RESOURCE_TYPE_BAR_MEM;
        resource->pci_addr = val & PCI_BASE_ADDRESS_MEM_MASK;
        size &= PCI_BASE_ADDRESS_MEM_MASK;
    }

    resource->size = ~size + 1;
    resource->addr = get_resource_addr_from_config(dev->bus, dev->dev, dev->fn, res_idx);
}

/*
 * see comment in pci.h
 */
void pci_get_dev_by_bdf(uint8_t bus, uint8_t dev, uint8_t fn, struct pci_dev *pci_dev)
{
    memset(pci_dev, 0, sizeof(*pci_dev));

    pci_dev->bus = bus;
    pci_dev->dev = dev;
    pci_dev->fn = fn;

    pci_read_config_word(pci_dev, PCI_VENDOR_ID, &pci_dev->vendor_id);
    //if (vendor_id == 0xFFFF) {
    //}
    pci_read_config_word(pci_dev, PCI_DEVICE_ID,    &pci_dev->device_id);
    pci_read_config_word(pci_dev, PCI_CLASS_DEVICE, &pci_dev->class_code);
    pci_read_config_byte(pci_dev, PCI_HEADER_TYPE,  &pci_dev->hdr_type);

    if (pci_dev->hdr_type != 0)
        return;

    for (int i = 0; i < PCI_NUM_RESOURCES; i++) {
        pci_fill_resource(pci_dev, i);
    }

}

void pci_list()
{
#ifdef PCI_DEBUG
    struct pci_dev pci_dev;
    for (unsigned int bus = 0; bus < PCI_BUS_MAX; bus++)
      for (unsigned int dev = 0; dev < PCI_DEV_MAX; dev++)
        for (unsigned int fn = 0; fn < PCI_FN_MAX; fn++)
        {
            pci_get_dev_by_bdf(bus, dev, fn, &pci_dev);

            if (pci_dev.vendor_id == 0xFFFF) {
                continue;
            }

            printf("%02x:%02x:%02x %s\n",
                    bus, dev, fn, get_pci_class_name(pci_dev.class_code));
            printf("\t PCI device %04x:%04x (header 0x%02x)\n",
                    pci_dev.vendor_id,
                    pci_dev.device_id,
                    pci_dev.hdr_type);

            if (pci_dev.hdr_type != 0) {
                uint16_t tmp;
                uint32_t tmp32;

                pci_read_config_word(&pci_dev, 0x10, &tmp);
                printf("\t bar0: 0x%x\n", tmp);

                pci_read_config_word(&pci_dev, 0x20, &tmp);
                printf("\t mem base: 0x%x\n", tmp);

                pci_read_config_word(&pci_dev, 0x22, &tmp);
                printf("\t mem limit: 0x%x\n", tmp);


                pci_read_config_word(&pci_dev, 0x24, &tmp);
                printf("\t pr mem base: 0x%x\n", tmp);


                pci_read_config_word(&pci_dev, 0x26, &tmp);
                printf("\t pr mem limit: 0x%x\n", tmp);

                pci_read_config_dword(&pci_dev, 0x28, &tmp32);
                printf("\t pr mem base upp: 0x%x\n", tmp);

                pci_read_config_dword(&pci_dev, 0x2C, &tmp32);
                printf("\t pr limit upp: 0x%x\n", tmp);

                pci_read_config_dword(&pci_dev, 0x38, &tmp32);
                printf("\t exp rom : 0x%lx\n", tmp32);

                continue;
            }

            for (int i = 0; i < PCI_NUM_RESOURCES; i++) {
                struct pci_resource *res = &pci_dev.resources[i];
                if (res->size == 0)
                    continue;

                if (res->type != PCI_RESOURCE_TYPE_ROM){
                    printf("\t BAR%d: ", i);
                    if (res->type == PCI_RESOURCE_TYPE_BAR_IO) {
                        printf("I/O ");
                    } else {
                        printf("%s bit %smem ",
                            res->mem_flags & PCI_RESOURCE_MEM_MASK_32 ? "32": "<UNSUPPORTED> 64",
                            res->mem_flags & PCI_RESOURCE_MEM_MASK_PREFETCH ? "prefetch ":"");
                    }
                }
                else {
                    printf("\t ROM: ");
                }


                if (res->pci_addr) {
                    printf("0x%llx ", res->pci_addr);
                } else {
                    printf("_ ");

                }

                printf("[size=0x%zx]\n", res->size);

            }
        }
#endif
}

//dev will be usefull when bridge is not hardcoded
uintptr_t pci_convert_legacy_port(struct pci_dev *dev, uint16_t port)
{
    (void) dev;
    return port;
}

void pci_init()
{
    //printf("PCI enumeration:\n");
    //pci_enumerate();

    printf("PCI initialization using configuration:\n");

    for (int i = 0; i < pci_configs_nb; i++) {
        struct pci_dev_config *dev_config = &pci_configs[i];
        struct pci_dev pci_dev;

        printf("%02x:%02x:%02x  ",
                dev_config->bus,
                dev_config->dev,
                dev_config->fn);

        pci_dev.bus = dev_config->bus;
        pci_dev.dev = dev_config->dev;
        pci_dev.fn =  dev_config->fn;

        {
            uint16_t vendor_id;
            pci_read_config_word(&pci_dev, PCI_VENDOR_ID, &vendor_id);

            if (vendor_id == 0xFFFF) {
                printf("Not found\n");
                continue;
            }
            printf("OK\n");
        }

        int command = 0;
        for (int i = 0; i < PCI_RESOURCE_ROM; i++) {
            if (dev_config->resources[i].pci_addr == 0)
                continue;

            pci_write_config_dword(&pci_dev, PCI_BASE_ADDRESS_0,
                    dev_config->resources[i].pci_addr);

            if (dev_config->resources[i].type == PCI_RESOURCE_TYPE_BAR_MEM)
                command |= PCI_COMMAND_MEMORY;
            else if (dev_config->resources[i].type == PCI_RESOURCE_TYPE_BAR_IO)
                command |= PCI_COMMAND_IO;
        }

        if (dev_config->resources[PCI_RESOURCE_ROM].pci_addr != 0) {
            pci_write_config_dword(&pci_dev, PCI_ROM_ADDRESS,
                    dev_config->resources[PCI_RESOURCE_ROM].pci_addr|PCI_ROM_ADDRESS_ENABLE);
            command |= PCI_COMMAND_MEMORY;
        }

        pci_write_config_word(&pci_dev, PCI_COMMAND, command);
    }
    //printf("PCI init result:\n");
    pci_list();
    printf("\n");
}
