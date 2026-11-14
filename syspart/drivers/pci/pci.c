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
 *
 * Modified by Kirill Brazhnikov, 2026-10-31: built-in integer types (int,
 * unsigned, short, long, ...) are replaced with the fixed-width types
 * from <stdint.h>.
 *
 * Modified by Kirill Brazhnikov, 2026-11-04: MISRA C:2025: braces around
 * all if/else/loop bodies; discarded results are cast to void; functions
 * without parameters are declared with (void); file-local definitions
 * have internal linkage; essentially Boolean conditions; U suffixes for
 * unsigned constants; pci_init() writes BAR r to its own register (all
 * BARs were written to BAR0); the configuration address is computed in
 * one helper with unsigned arithmetic.
 *
 * Modified by Kirill Brazhnikov, 2026-11-08: Doxygen headers for the file
 * and its functions.
 *
 * Modified by Kirill Brazhnikov, 2026-11-14: OK is in pci_internal.h; the
 * struct types are used by their typedef names.
 */

/**
 * @file
 * The PCI driver of the system partition: configuration mechanism \#1,
 * the resources of a device, and the setup of the configured devices.
 */

#include <pci.h>
#include <pci_config.h>
#include <ioports.h>
#include <stdio.h>

#include "pci_internal.h"

#include <string.h>

/**
 * @funcname `pci_config_address`
 *
 * @description
 * Returns the address of a configuration register of a device (configuration
 * mechanism \#1).
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `dev` | `const pci_dev_t *` | [in] The device. |
 * | `where` | `int32_t` | [in] The register's offset. |
 *
 * @return The address for PCI_CONFIG_ADDRESS.
 *
 * @globals
 * None.
 *
 * @locals
 * None.
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Return
 *    `0x80000000U | ((uint32_t) dev->bus << 16) | ((uint32_t) dev->dev << 11)`
 *    `| ((uint32_t) dev->fn << 8) | ((uint32_t) where & 0xfcU)`.
 *
 * @calls
 * None.
 */
static uint32_t pci_config_address(const pci_dev_t *dev, int32_t where)
{
    return 0x80000000U | ((uint32_t) dev->bus << 16)
        | ((uint32_t) dev->dev << 11)
        | ((uint32_t) dev->fn << 8) | ((uint32_t) where & 0xfcU);
}

#ifdef PCI_DEBUG
/**
 * @funcname `get_pci_class_name`
 *
 * @description
 * Returns the name of a PCI device class (PCI_DEBUG).
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `classcode` | `int32_t` | [in] The class code. |
 *
 * @return The name, or "Unknown".
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `pci_classnames` | `const struct (anonymous)[87]` | The names of the PCI device classes |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `i` | `int32_t` | The index of a class |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Set `i` to `0`.
 * 2. While `pci_classnames[i].name` is not NULL:
 *    1. If `pci_classnames[i].classcode` is equal to `(classcode & 0xFFFF00)`:
 *       1. Return `pci_classnames[i].name`.
 *    2. Increment `i`.
 * 3. Return `"Unknown"`.
 *
 * @calls
 * None.
 */
static char *get_pci_class_name(int32_t classcode) {
    int32_t i = 0;
    while (pci_classnames[i].name != NULL) {
        if (pci_classnames[i].classcode == (classcode & 0xFFFF00)) {
            return pci_classnames[i].name;
        }
        i++;
    }
    return "Unknown";
}
#endif


/**
 * @funcname `pci_read_config_byte`
 *
 * @description
 * Reads a byte of a device's configuration space.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `dev` | `pci_dev_t *` | [in] The device. |
 * | `where` | `int32_t` | [in] The offset. |
 * | `val` | `uint8_t *` | [out] Where the value is stored. |
 *
 * @return 0 (OK).
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `addr` | `uint32_t` | The configuration address |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Call `pci_config_address()` with the arguments `dev` and `where`, and
 *    store the result in `addr`.
 * 2. Execute the assembly code `outl(PCI_CONFIG_ADDRESS, addr)`.
 * 3. Set `*val` to `inb(PCI_CONFIG_DATA + (where & 3))`.
 * 4. Return `OK`.
 *
 * @calls
 * - `pci_config_address()`
 */
int32_t pci_read_config_byte(pci_dev_t *dev, int32_t where, uint8_t *val)
{
    uint32_t addr = pci_config_address(dev, where);

    outl(PCI_CONFIG_ADDRESS, addr);
    *val = inb(PCI_CONFIG_DATA + (where & 3));
    return OK;
}

/**
 * @funcname `pci_read_config_word`
 *
 * @description
 * Reads a 16-bit word of a device's configuration space.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `dev` | `pci_dev_t *` | [in] The device. |
 * | `where` | `int32_t` | [in] The offset. |
 * | `val` | `uint16_t *` | [out] Where the value is stored. |
 *
 * @return 0 (OK).
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `addr` | `uint32_t` | The configuration address |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Call `pci_config_address()` with the arguments `dev` and `where`, and
 *    store the result in `addr`.
 * 2. Execute the assembly code `outl(PCI_CONFIG_ADDRESS, addr)`.
 * 3. Set `*val` to `inw(PCI_CONFIG_DATA + (where & 2))`.
 * 4. Return `OK`.
 *
 * @calls
 * - `pci_config_address()`
 */
int32_t pci_read_config_word(pci_dev_t *dev, int32_t where, uint16_t *val)
{
    uint32_t addr = pci_config_address(dev, where);

    outl(PCI_CONFIG_ADDRESS, addr);
    *val = inw(PCI_CONFIG_DATA + (where & 2));
    return OK;
}

/**
 * @funcname `pci_read_config_dword`
 *
 * @description
 * Reads a 32-bit word of a device's configuration space.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `dev` | `pci_dev_t *` | [in] The device. |
 * | `where` | `int32_t` | [in] The offset. |
 * | `val` | `uint32_t *` | [out] Where the value is stored. |
 *
 * @return 0 (OK).
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `addr` | `uint32_t` | The configuration address |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Call `pci_config_address()` with the arguments `dev` and `where`, and
 *    store the result in `addr`.
 * 2. Execute the assembly code `outl(PCI_CONFIG_ADDRESS, addr)`.
 * 3. Set `*val` to `inl(PCI_CONFIG_DATA)`.
 * 4. Return `OK`.
 *
 * @calls
 * - `pci_config_address()`
 */
int32_t pci_read_config_dword(pci_dev_t *dev, int32_t where, uint32_t *val)
{
    uint32_t addr = pci_config_address(dev, where);

    outl(PCI_CONFIG_ADDRESS, addr);
    *val = inl(PCI_CONFIG_DATA);
    return OK;
}

/**
 * @funcname `pci_write_config_byte`
 *
 * @description
 * Writes a byte of a device's configuration space.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `dev` | `pci_dev_t *` | [in] The device. |
 * | `where` | `int32_t` | [in] The offset. |
 * | `val` | `uint8_t` | [in] The value. |
 *
 * @return 0 (OK).
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `addr` | `uint32_t` | The configuration address |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Call `pci_config_address()` with the arguments `dev` and `where`, and
 *    store the result in `addr`.
 * 2. Execute the assembly code `outl(PCI_CONFIG_ADDRESS, addr)`.
 * 3. Execute the assembly code `outb(PCI_CONFIG_DATA + (where & 3), val)`.
 * 4. Return `OK`.
 *
 * @calls
 * - `pci_config_address()`
 */
int32_t pci_write_config_byte(pci_dev_t *dev, int32_t where, uint8_t val)
{
    uint32_t addr = pci_config_address(dev, where);

    outl(PCI_CONFIG_ADDRESS, addr);
    outb(PCI_CONFIG_DATA + (where & 3), val);
    return OK;
}

/**
 * @funcname `pci_write_config_word`
 *
 * @description
 * Writes a 16-bit word of a device's configuration space.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `dev` | `pci_dev_t *` | [in] The device. |
 * | `where` | `int32_t` | [in] The offset. |
 * | `val` | `uint16_t` | [in] The value. |
 *
 * @return 0 (OK).
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `addr` | `uint32_t` | The configuration address |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Call `pci_config_address()` with the arguments `dev` and `where`, and
 *    store the result in `addr`.
 * 2. Execute the assembly code `outl(PCI_CONFIG_ADDRESS, addr)`.
 * 3. Execute the assembly code `outw(PCI_CONFIG_DATA + (where & 2), val)`.
 * 4. Return `OK`.
 *
 * @calls
 * - `pci_config_address()`
 */
int32_t pci_write_config_word(pci_dev_t *dev, int32_t where, uint16_t val)
{
    uint32_t addr = pci_config_address(dev, where);

    outl(PCI_CONFIG_ADDRESS, addr);
    outw(PCI_CONFIG_DATA + (where & 2), val);
    return OK;
}

/**
 * @funcname `pci_write_config_dword`
 *
 * @description
 * Writes a 32-bit word of a device's configuration space.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `dev` | `pci_dev_t *` | [in] The device. |
 * | `where` | `int32_t` | [in] The offset. |
 * | `val` | `uint32_t` | [in] The value. |
 *
 * @return 0 (OK).
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `addr` | `uint32_t` | The configuration address |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Call `pci_config_address()` with the arguments `dev` and `where`, and
 *    store the result in `addr`.
 * 2. Execute the assembly code `outl(PCI_CONFIG_ADDRESS, addr)`.
 * 3. Execute the assembly code `outl(PCI_CONFIG_DATA, val)`.
 * 4. Return `OK`.
 *
 * @calls
 * - `pci_config_address()`
 */
int32_t pci_write_config_dword(pci_dev_t *dev, int32_t where, uint32_t val)
{
    uint32_t addr = pci_config_address(dev, where);

    outl(PCI_CONFIG_ADDRESS, addr);
    outl(PCI_CONFIG_DATA, val);
    return OK;
}


/**
 * @funcname `get_resource_addr_from_config`
 *
 * @description
 * Returns the address of a resource of a device in the PCI configuration of the
 * partition (a linear search).
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `bus` | `uint8_t` | [in] The bus. |
 * | `dev` | `uint8_t` | [in] The device. |
 * | `fn` | `uint8_t` | [in] The function. |
 * | `idx` | `pci_resource_index_e` | [in] The resource. |
 *
 * @return The address, or 0 if the device isn't in the configuration.
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `pci_configs` | `pci_dev_config_t[]` | The PCI devices of the configuration and their resources |
 * | `pci_configs_nb` | `uint32_t` | Their number |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `d` | `pci_dev_config_t *` | A configured device |
 * | `i` | `uint32_t` | Its index |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Set `i` to `0U`; then while `i` is less than `pci_configs_nb`, repeat
 *    (after each pass: increment `i`):
 *    1. Set `d` to the address of `pci_configs[i]`.
 *    2. If `d->bus` is equal to `bus` and `d->dev` is equal to `dev` and
 *       `d->fn` is equal to `fn`:
 *       1. Return `d->resources[idx].addr`.
 * 2. Return `0`.
 *
 * @calls
 * None.
 */
static uintptr_t get_resource_addr_from_config(uint8_t bus, uint8_t dev,
    uint8_t fn, pci_resource_index_e idx)
{
    //TODO optimize this linear search
    pci_dev_config_t *d;
    for (uint32_t i = 0U; i < pci_configs_nb; i++) {
        d = &pci_configs[i];
        if ((d->bus == bus) && (d->dev == dev) && (d->fn == fn)) {
            return d->resources[idx].addr;
        }
    }

    return 0;
}



/**
 * @funcname `pci_fill_resource`
 *
 * @description
 * Fills in a resource of a device: its type, PCI address and size (sized by
 * writing all ones to the BAR), and its address from the configuration.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `dev` | `pci_dev_t *` | [in] The device. |
 * | `res_idx` | `pci_resource_index_e` | [in] The resource. |
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `base` | `uint32_t` | The PCI address |
 * | `mask` | `uint32_t` | The mask written |
 * | `reg` | `int32_t` | The register of the resource |
 * | `resource` | `pci_resource_t *` | The resource |
 * | `size` | `uint32_t` | The size read back |
 * | `val` | `uint32_t` | Its value |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Set `resource` to the address of `dev->resources[res_idx]`.
 * 2. Call `memset()` with the arguments `resource`, `0` and
 *    `sizeof(*resource)`.
 * 3. If `res_idx` is not equal to `PCI_RESOURCE_ROM`:
 *    1. Set `reg` to `PCI_BASE_ADDRESS_0 + ((int32_t) res_idx * 4)`.
 *    2. Set `mask` to `0xffffffffU`.
 * 4. Otherwise:
 *    1. Set `reg` to `PCI_ROM_ADDRESS`.
 *    2. Set `mask` to `PCI_ROM_ADDRESS_MASK`.
 * 5. Call `pci_read_config_dword()` with the arguments `dev`, `reg` and the
 *    address of `val`.
 * 6. Call `pci_write_config_dword()` with the arguments `dev`, `reg` and
 *    `mask`.
 * 7. Call `pci_read_config_dword()` with the arguments `dev`, `reg` and the
 *    address of `size`.
 * 8. Call `pci_write_config_dword()` with the arguments `dev`, `reg` and
 *    `val`.
 * 9. If `res_idx` is equal to `PCI_RESOURCE_ROM`:
 *    1. Set `resource->type` to `PCI_RESOURCE_TYPE_ROM`.
 *    2. Set `base` to `val & PCI_ROM_ADDRESS_MASK`.
 *    3. Set `resource->pci_addr` to `base`.
 *    4. Keep only the bits `PCI_ROM_ADDRESS_MASK` of `size`.
 * 10. Otherwise, if `(val&PCI_BASE_ADDRESS_SPACE)` is equal to
 *     `PCI_BASE_ADDRESS_SPACE_IO`:
 *     1. Set `resource->type` to `PCI_RESOURCE_TYPE_BAR_IO`.
 *     2. Set `base` to `val & PCI_BASE_ADDRESS_IO_MASK`.
 *     3. Set `resource->pci_addr` to `base`.
 *     4. Keep only the bits `PCI_BASE_ADDRESS_IO_MASK` of `size`.
 * 11. Otherwise:
 *     1. If `(val & PCI_BASE_ADDRESS_MEM_TYPE_MASK)` is equal to
 *        `PCI_BASE_ADDRESS_MEM_TYPE_32`:
 *        1. Set the bits `PCI_RESOURCE_MEM_MASK_32` in `resource->mem_flags`.
 *     2. If `(val & PCI_BASE_ADDRESS_MEM_PREFETCH)` is not equal to `0U`:
 *        1. Set the bits `PCI_RESOURCE_MEM_MASK_PREFETCH` in
 *           `resource->mem_flags`.
 *     3. Set `resource->type` to `PCI_RESOURCE_TYPE_BAR_MEM`.
 *     4. Set `base` to `val & PCI_BASE_ADDRESS_MEM_MASK`.
 *     5. Set `resource->pci_addr` to `base`.
 *     6. Keep only the bits `PCI_BASE_ADDRESS_MEM_MASK` of `size`.
 * 12. Set `size` to `~size + 1U`.
 * 13. Set `resource->size` to `size`.
 * 14. Call `get_resource_addr_from_config()` with the arguments `dev->bus`,
 *     `dev->dev`, `dev->fn` and `res_idx`, and store the result in
 *     `resource->addr`.
 *
 * @calls
 * - `memset()`
 * - `pci_read_config_dword()`
 * - `pci_write_config_dword()`
 * - `get_resource_addr_from_config()`
 */
static void pci_fill_resource(pci_dev_t *dev, pci_resource_index_e res_idx)
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
    int32_t reg;
    uint32_t val, size, mask;

    pci_resource_t *resource = &dev->resources[res_idx];
    (void) memset(resource, 0, sizeof(*resource));


    if (res_idx != PCI_RESOURCE_ROM) {
        reg = PCI_BASE_ADDRESS_0 + ((int32_t) res_idx * 4);
        mask =  0xffffffffU;
    } else {
        reg = PCI_ROM_ADDRESS;
        mask = PCI_ROM_ADDRESS_MASK;
    }

    (void) pci_read_config_dword(dev, reg, &val);
    (void) pci_write_config_dword(dev, reg, mask);
    (void) pci_read_config_dword(dev, reg, &size);
    (void) pci_write_config_dword(dev, reg, val);

    if (res_idx == PCI_RESOURCE_ROM)  {
        resource->type = PCI_RESOURCE_TYPE_ROM;
        uint32_t base = val & PCI_ROM_ADDRESS_MASK;
        resource->pci_addr = base;
        size &= PCI_ROM_ADDRESS_MASK;

    } else if ((val&PCI_BASE_ADDRESS_SPACE) == PCI_BASE_ADDRESS_SPACE_IO) {
        resource->type = PCI_RESOURCE_TYPE_BAR_IO;
        uint32_t base = val & PCI_BASE_ADDRESS_IO_MASK;
        resource->pci_addr = base;
        size &= PCI_BASE_ADDRESS_IO_MASK;

    } else {
        if ((val & PCI_BASE_ADDRESS_MEM_TYPE_MASK) == PCI_BASE_ADDRESS_MEM_TYPE_32) {
            resource->mem_flags |= PCI_RESOURCE_MEM_MASK_32;
        }
        if ((val & PCI_BASE_ADDRESS_MEM_PREFETCH) != 0U) {
            resource->mem_flags |= PCI_RESOURCE_MEM_MASK_PREFETCH;
        }
        resource->type = PCI_RESOURCE_TYPE_BAR_MEM;
        uint32_t base = val & PCI_BASE_ADDRESS_MEM_MASK;
        resource->pci_addr = base;
        size &= PCI_BASE_ADDRESS_MEM_MASK;
    }

    size = ~size + 1U;
    resource->size = size;
    resource->addr = get_resource_addr_from_config(dev->bus, dev->dev, dev->fn, res_idx);
}

/**
 * @funcname `pci_get_dev_by_bdf`
 *
 * @description
 * Finds a PCI device by its (bus, dev, fn) triple and fills in its information;
 * resources aren't filled for PCI bridges. A vendor_id of 0xFFFF means that the
 * device doesn't exist.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `bus` | `uint8_t` | [in] The bus. |
 * | `dev` | `uint8_t` | [in] The device. |
 * | `fn` | `uint8_t` | [in] The function. |
 * | `pci_dev` | `pci_dev_t *` | [out] Where the information is stored. |
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `i` | `int32_t` | The index of a resource |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Call `memset()` with the arguments `pci_dev`, `0` and `sizeof(*pci_dev)`.
 * 2. Set `pci_dev->bus` to `bus`.
 * 3. Set `pci_dev->dev` to `dev`.
 * 4. Set `pci_dev->fn` to `fn`.
 * 5. Call `pci_read_config_word()` with the arguments `pci_dev`,
 *    `PCI_VENDOR_ID` and the address of `pci_dev->vendor_id`.
 * 6. Call `pci_read_config_word()` with the arguments `pci_dev`,
 *    `PCI_DEVICE_ID` and the address of `pci_dev->device_id`.
 * 7. Call `pci_read_config_word()` with the arguments `pci_dev`,
 *    `PCI_CLASS_DEVICE` and the address of `pci_dev->class_code`.
 * 8. Call `pci_read_config_byte()` with the arguments `pci_dev`,
 *    `PCI_HEADER_TYPE` and the address of `pci_dev->hdr_type`.
 * 9. If `pci_dev->hdr_type` is not equal to `0U`:
 *    1. Return.
 * 10. Set `i` to `0`; then while `i` is less than `PCI_NUM_RESOURCES`, repeat
 *     (after each pass: increment `i`):
 *     1. Call `pci_fill_resource()` with the arguments `pci_dev` and `i`.
 *
 * @calls
 * - `memset()`
 * - `pci_read_config_word()`
 * - `pci_read_config_byte()`
 * - `pci_fill_resource()`
 */
void pci_get_dev_by_bdf(uint8_t bus, uint8_t dev, uint8_t fn,
    pci_dev_t *pci_dev)
{
    (void) memset(pci_dev, 0, sizeof(*pci_dev));

    pci_dev->bus = bus;
    pci_dev->dev = dev;
    pci_dev->fn = fn;

    (void) pci_read_config_word(pci_dev, PCI_VENDOR_ID, &pci_dev->vendor_id);
    //if (vendor_id == 0xFFFF) {
    //}
    (void) pci_read_config_word(pci_dev, PCI_DEVICE_ID,    &pci_dev->device_id);
    (void) pci_read_config_word(
        pci_dev, PCI_CLASS_DEVICE, &pci_dev->class_code);
    (void) pci_read_config_byte(pci_dev, PCI_HEADER_TYPE,  &pci_dev->hdr_type);

    if (pci_dev->hdr_type != 0U) {
        return;
    }

    for (int32_t i = 0; i < PCI_NUM_RESOURCES; i++) {
        pci_fill_resource(pci_dev, i);
    }

}

/**
 * @funcname `pci_list`
 *
 * @description
 * Lists the PCI devices with their resources (PCI_DEBUG; nothing otherwise).
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `bus` | `uint32_t` | The bus |
 * | `dev` | `uint32_t` | The device number |
 * | `fn` | `uint32_t` | The function |
 * | `i` | `int32_t` | The index of a resource |
 * | `pci_dev` | `pci_dev_t` | A device |
 * | `res` | `pci_resource_t *` | The resource |
 * | `tmp` | `uint16_t` | A 16-bit register of a bridge |
 * | `tmp32` | `uint32_t` | A 32-bit register of a bridge |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Set `bus` to `0`; then while `bus` is less than `PCI_BUS_MAX`, repeat
 *    (after each pass: increment `bus`):
 *    1. Set `dev` to `0`; then while `dev` is less than `PCI_DEV_MAX`, repeat
 *       (after each pass: increment `dev`):
 *       1. Set `fn` to `0`; then while `fn` is less than `PCI_FN_MAX`, repeat
 *          (after each pass: increment `fn`):
 *          1. Call `pci_get_dev_by_bdf()` with the arguments `bus`, `dev`, `fn`
 *             and the address of `pci_dev`.
 *          2. If `pci_dev.vendor_id` is equal to `0xFFFF`:
 *             1. Go on with the next pass of the loop.
 *          3. Call `printf()` with the arguments `"%02x:%02x:%02x %%s\n"`,
 *             `bus`, `dev`, `fn` and `get_pci_class_name(pci_dev.class_code)`.
 *          4. Call `printf()` with the arguments
 *             `"\t PCI device %04x:%04x (header 0x%02x)\n"`,
 *             `pci_dev.vendor_id`, `pci_dev.device_id` and `pci_dev.hdr_type`.
 *          5. If `pci_dev.hdr_type` is not equal to `0`:
 *             1. Call `pci_read_config_word()` with the arguments the address
 *                of `pci_dev`, `0x10` and the address of `tmp`.
 *             2. Call `printf()` with the arguments `"\t bar0: 0x%%x\n"` and
 *                `tmp`.
 *             3. Call `pci_read_config_word()` with the arguments the address
 *                of `pci_dev`, `0x20` and the address of `tmp`.
 *             4. Call `printf()` with the arguments `"\t mem base: 0x%%x\n"`
 *                and `tmp`.
 *             5. Call `pci_read_config_word()` with the arguments the address
 *                of `pci_dev`, `0x22` and the address of `tmp`.
 *             6. Call `printf()` with the arguments `"\t mem limit: 0x%%x\n"`
 *                and `tmp`.
 *             7. Call `pci_read_config_word()` with the arguments the address
 *                of `pci_dev`, `0x24` and the address of `tmp`.
 *             8. Call `printf()` with the arguments
 *                `"\t pr mem base: 0x%%x\n"` and `tmp`.
 *             9. Call `pci_read_config_word()` with the arguments the address
 *                of `pci_dev`, `0x26` and the address of `tmp`.
 *             10. Call `printf()` with the arguments
 *                 `"\t pr mem limit: 0x%%x\n"` and `tmp`.
 *             11. Call `pci_read_config_dword()` with the arguments the address
 *                 of `pci_dev`, `0x28` and the address of `tmp32`.
 *             12. Call `printf()` with the arguments
 *                 `"\t pr mem base upp: 0x%%x\n"` and `tmp`.
 *             13. Call `pci_read_config_dword()` with the arguments the address
 *                 of `pci_dev`, `0x2C` and the address of `tmp32`.
 *             14. Call `printf()` with the arguments
 *                 `"\t pr limit upp: 0x%%x\n"` and `tmp`.
 *             15. Call `pci_read_config_dword()` with the arguments the address
 *                 of `pci_dev`, `0x38` and the address of `tmp32`.
 *             16. Call `printf()` with the arguments `"\t exp rom : 0x%%lx\n"`
 *                 and `tmp32`.
 *             17. Go on with the next pass of the loop.
 *          6. Set `i` to `0`; then while `i` is less than `PCI_NUM_RESOURCES`,
 *             repeat (after each pass: increment `i`):
 *             1. Set `res` to the address of `pci_dev.resources[i]`.
 *             2. If `res->size` is equal to `0`:
 *                1. Go on with the next pass of the loop.
 *             3. If `res->type` is not equal to `PCI_RESOURCE_TYPE_ROM`:
 *                1. Call `printf()` with the arguments `"\t BAR%%d: "` and `i`.
 *                2. If `res->type` is equal to `PCI_RESOURCE_TYPE_BAR_IO`:
 *                   1. Call `printf()` with the argument `"I/O "`.
 *                3. Otherwise:
 *                   1. Call `printf()` with the arguments `"%%s bit %%smem "`,
 *                      `"32"` if `res->mem_flags & PCI_RESOURCE_MEM_MASK_32` is
 *                      true, otherwise `"<UNSUPPORTED> 64"` and `"prefetch "`
 *                      if `res->mem_flags & PCI_RESOURCE_MEM_MASK_PREFETCH` is
 *                      true, otherwise `""`.
 *             4. Otherwise:
 *                1. Call `printf()` with the argument `"\t ROM: "`.
 *             5. If `res->pci_addr` is true:
 *                1. Call `printf()` with the arguments `"0x%%llx "` and
 *                   `res->pci_addr`.
 *             6. Otherwise:
 *                1. Call `printf()` with the argument `"_ "`.
 *             7. Call `printf()` with the arguments `"[size=0x%%zx]\n"` and
 *                `res->size`.
 *
 * @calls
 * - `pci_get_dev_by_bdf()`
 * - `printf()`
 * - `get_pci_class_name()`
 * - `pci_read_config_word()`
 * - `pci_read_config_dword()`
 */
void pci_list(void)
{
#ifdef PCI_DEBUG
    pci_dev_t pci_dev;
    for (uint32_t bus = 0; bus < PCI_BUS_MAX; bus++) {
      for (uint32_t dev = 0; dev < PCI_DEV_MAX; dev++) {
        for (uint32_t fn = 0; fn < PCI_FN_MAX; fn++)
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

            for (int32_t i = 0; i < PCI_NUM_RESOURCES; i++) {
                pci_resource_t *res = &pci_dev.resources[i];
                if (res->size == 0) {
                    continue;
                }

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
      }
    }
#endif
}

/**
 * @funcname `pci_convert_legacy_port`
 *
 * @description
 * Converts a legacy port to the address used by ioread/iowrite (the same for
 * now; dev will be useful when the bridge isn't hardcoded).
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `dev` | `pci_dev_t *` | [in] The device (not used). |
 * | `port` | `uint16_t` | [in] The port. |
 *
 * @return The address.
 *
 * @globals
 * None.
 *
 * @locals
 * None.
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Mark `dev` as not used.
 * 2. Return `port`.
 *
 * @calls
 * None.
 */
uintptr_t pci_convert_legacy_port(pci_dev_t *dev, uint16_t port)
{
    (void) dev;
    return port;
}

/**
 * @funcname `pci_init`
 *
 * @description
 * Initializes the configured PCI devices: writes the addresses of their BARs
 * and ROM and enables their I/O and memory decoding.
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `pci_configs` | `pci_dev_config_t[]` | The PCI devices of the configuration and their resources |
 * | `pci_configs_nb` | `uint32_t` | Their number |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `command` | `uint16_t` | Its command register |
 * | `dev_config` | `pci_dev_config_t *` | Its configuration |
 * | `i` | `uint32_t` | The index of a configured device |
 * | `pci_dev` | `pci_dev_t` | The device |
 * | `r` | `int32_t` | The index of a BAR |
 * | `vendor_id` | `uint16_t` | Its vendor ID |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Call `printf()` with the argument
 *    `"PCI initialization using configuration:\n"`.
 * 2. Set `i` to `0U`; then while `i` is less than `pci_configs_nb`, repeat
 *    (after each pass: increment `i`):
 *    1. Set `dev_config` to the address of `pci_configs[i]`.
 *    2. Call `printf()` with the arguments `"%02x:%02x:%02x "`,
 *       `dev_config->bus`, `dev_config->dev` and `dev_config->fn`.
 *    3. Set `pci_dev.bus` to `dev_config->bus`.
 *    4. Set `pci_dev.dev` to `dev_config->dev`.
 *    5. Set `pci_dev.fn` to `dev_config->fn`.
 *    6. Call `pci_read_config_word()` with the arguments the address of
 *       `pci_dev`, `PCI_VENDOR_ID` and the address of `vendor_id`.
 *    7. If `vendor_id` is equal to `0xFFFFU`:
 *       1. Call `printf()` with the argument `"Not found\n"`.
 *       2. Go on with the next pass of the loop.
 *    8. Call `printf()` with the argument `"OK\n"`.
 *    9. Set `command` to `0U`.
 *    10. Set `r` to `0`; then while `r` is less than
 *        `(int32_t) PCI_RESOURCE_ROM`, repeat (after each pass: increment `r`):
 *        1. If `dev_config->resources[r].pci_addr` is equal to `0U`:
 *           1. Go on with the next pass of the loop.
 *        2. Call `pci_write_config_dword()` with the arguments the address of
 *           `pci_dev`, `PCI_BASE_ADDRESS_0 + (r * 4)` and
 *           `(uint32_t) dev_config->resources[r].pci_addr`.
 *        3. If `dev_config->resources[r].type` is equal to
 *           `PCI_RESOURCE_TYPE_BAR_MEM`:
 *           1. Set the bits `PCI_COMMAND_MEMORY` in `command`.
 *        4. Otherwise, if `dev_config->resources[r].type` is equal to
 *           `PCI_RESOURCE_TYPE_BAR_IO`:
 *           1. Set the bits `PCI_COMMAND_IO` in `command`.
 *        5. Otherwise:
 *           1. Do nothing.
 *    11. If `dev_config->resources[PCI_RESOURCE_ROM].pci_addr` is not equal to
 *        `0U`:
 *        1. Call `pci_write_config_dword()` with the arguments the address of
 *           `pci_dev`, `PCI_ROM_ADDRESS` and
 *           `(uint32_t) dev_config->resources[PCI_RESOURCE_ROM].pci_addr |`
 *           `PCI_ROM_ADDRESS_ENABLE`.
 *        2. Set the bits `PCI_COMMAND_MEMORY` in `command`.
 *    12. Call `pci_write_config_word()` with the arguments the address of
 *        `pci_dev`, `PCI_COMMAND` and `command`.
 * 3. Call `pci_list()`.
 * 4. Call `printf()` with the argument `"\n"`.
 *
 * @calls
 * - `printf()`
 * - `pci_read_config_word()`
 * - `pci_write_config_dword()`
 * - `pci_write_config_word()`
 * - `pci_list()`
 */
void pci_init(void)
{
    //printf("PCI enumeration:\n");
    //pci_enumerate();

    (void) printf("PCI initialization using configuration:\n");

    for (uint32_t i = 0U; i < pci_configs_nb; i++) {
        pci_dev_config_t *dev_config = &pci_configs[i];
        pci_dev_t pci_dev;

        (void) printf("%02x:%02x:%02x  ",
                dev_config->bus,
                dev_config->dev,
                dev_config->fn);

        pci_dev.bus = dev_config->bus;
        pci_dev.dev = dev_config->dev;
        pci_dev.fn =  dev_config->fn;

        {
            uint16_t vendor_id;
            (void) pci_read_config_word(&pci_dev, PCI_VENDOR_ID, &vendor_id);

            if (vendor_id == 0xFFFFU) {
                (void) printf("Not found\n");
                continue;
            }
            (void) printf("OK\n");
        }

        uint16_t command = 0U;
        for (int32_t r = 0; r < (int32_t) PCI_RESOURCE_ROM; r++) {
            if (dev_config->resources[r].pci_addr == 0U) {
                continue;
            }

            /* BAR r is at PCI_BASE_ADDRESS_0 + 4 * r (all BARs were
             * written to BAR0). */
            (void) pci_write_config_dword(&pci_dev,
                    PCI_BASE_ADDRESS_0 + (r * 4),
                    (uint32_t) dev_config->resources[r].pci_addr);

            if (dev_config->resources[r].type == PCI_RESOURCE_TYPE_BAR_MEM) {
                command |= PCI_COMMAND_MEMORY;
            } else if (dev_config->resources[r].type
                       == PCI_RESOURCE_TYPE_BAR_IO) {
                command |= PCI_COMMAND_IO;
            } else {
                /* ROM: enabled below. */
            }
        }

        if (dev_config->resources[PCI_RESOURCE_ROM].pci_addr != 0U) {
            (void) pci_write_config_dword(&pci_dev, PCI_ROM_ADDRESS,
                    (uint32_t) dev_config->resources[PCI_RESOURCE_ROM].pci_addr
                    | PCI_ROM_ADDRESS_ENABLE);
            command |= PCI_COMMAND_MEMORY;
        }

        (void) pci_write_config_word(&pci_dev, PCI_COMMAND, command);
    }
    //printf("PCI init result:\n");
    pci_list();
    (void) printf("\n");
}
