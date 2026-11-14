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
 * Some defines was taken from linux source "include/uapi/linux/pci_regs.h"
 * PCI standard defines
 * Copyright 1994, Drew Eckhardt
 * Copyright 1997--1999 Martin Mares <mj@ucw.cz>
 *
 *
 * Modified by Kirill Brazhnikov, 2026-10-31: built-in integer types (int,
 * unsigned, short, long, ...) are replaced with the fixed-width types
 * from <stdint.h>.
 *
 * Modified by Kirill Brazhnikov, 2026-11-04: MISRA C:2025: U suffixes for
 * unsigned constants; the register masks and flags are 32-bit unsigned
 * constants.
 *
 * Modified by Kirill Brazhnikov, 2026-11-13: the function declarations are
 * hidden from Doxygen (\cond): only the definitions of the functions are
 * documented.
 *
 * Modified by Kirill Brazhnikov, 2026-11-14: the macros of radeon/main.c and
 * the declaration of p3041_test() are here; the types are typedefs without a
 * tag, and the macros and types are documented.
 */


# ifndef __POK__PCI_H__
# define __POK__PCI_H__

#include <types.h>
#include <ioports.h>

/**
 * @name PCI configuration ports
 * PCI configuration registers
 * @{
 */

/**
 * @macroname `PCI_CONFIG_ADDRESS`
 *
 * @description
 * The I/O port of the address of the PCI configuration space.
 */
#  define PCI_CONFIG_ADDRESS	0xCF8

/**
 * @macroname `PCI_CONFIG_DATA`
 *
 * @description
 * The I/O port of the data of the PCI configuration space.
 */
#  define PCI_CONFIG_DATA	0xCFC

/** @} */


/**
 * @name PCI configuration space registers
 * Configuration space registers
 * @{
 */

/**
 * @macroname `PCI_VENDOR_ID`
 *
 * @description
 * The vendor ID register (16 bits).
 */
#define PCI_VENDOR_ID           0x00

/**
 * @macroname `PCI_DEVICE_ID`
 *
 * @description
 * The device ID register (16 bits).
 */
#define PCI_DEVICE_ID           0x02

/**
 * @macroname `PCI_COMMAND`
 *
 * @description
 * The command register (16 bits).
 */
#define PCI_COMMAND             0x04

/**
 * @macroname `PCI_CLASS_REVISION`
 *
 * @description
 * The class and revision register: the class in the high 24 bits, the revision
 * in the low 8.
 */
#define PCI_CLASS_REVISION      0x08

/**
 * @macroname `PCI_REVISION_ID`
 *
 * @description
 * The revision ID register.
 */
#define PCI_REVISION_ID         0x08

/**
 * @macroname `PCI_CLASS_PROG`
 *
 * @description
 * The register-level programming interface register.
 */
#define PCI_CLASS_PROG          0x09

/**
 * @macroname `PCI_CLASS_DEVICE`
 *
 * @description
 * The device class register.
 */
#define PCI_CLASS_DEVICE        0x0a

/**
 * @macroname `PCI_HEADER_TYPE`
 *
 * @description
 * The header type register.
 */
#define PCI_HEADER_TYPE         0x0E

/**
 * @macroname `PCI_HEADER_TYPE_NORMAL`
 *
 * @description
 * A normal device.
 */
#define  PCI_HEADER_TYPE_NORMAL         0

/**
 * @macroname `PCI_HEADER_TYPE_BRIDGE`
 *
 * @description
 * A PCI-to-PCI bridge.
 */
#define  PCI_HEADER_TYPE_BRIDGE         1

/**
 * @macroname `PCI_HEADER_TYPE_CARDBUS`
 *
 * @description
 * A CardBus bridge.
 */
#define  PCI_HEADER_TYPE_CARDBUS        2

/** @} */


/**
 * @name PCI base address registers
 * Base addresses specify locations in memory or I/O space.
 * @{
 */

/**
 * @macroname `PCI_BASE_ADDRESS_0`
 *
 * @description
 * Base address register 0 (32 bits).
 */
#define PCI_BASE_ADDRESS_0      0x10

/**
 * @macroname `PCI_BASE_ADDRESS_1`
 *
 * @description
 * Base address register 1 (32 bits; header types 0 and 1 only).
 */
#define PCI_BASE_ADDRESS_1      0x14

/**
 * @macroname `PCI_BASE_ADDRESS_2`
 *
 * @description
 * Base address register 2 (32 bits; header type 0 only).
 */
#define PCI_BASE_ADDRESS_2      0x18

/**
 * @macroname `PCI_BASE_ADDRESS_3`
 *
 * @description
 * Base address register 3 (32 bits).
 */
#define PCI_BASE_ADDRESS_3      0x1c

/**
 * @macroname `PCI_BASE_ADDRESS_4`
 *
 * @description
 * Base address register 4 (32 bits).
 */
#define PCI_BASE_ADDRESS_4      0x20

/**
 * @macroname `PCI_BASE_ADDRESS_5`
 *
 * @description
 * Base address register 5 (32 bits).
 */
#define PCI_BASE_ADDRESS_5      0x24

/**
 * @macroname `PCI_BASE_ADDRESS_SPACE`
 *
 * @description
 * The space bit of a BAR: 0 for memory, 1 for I/O.
 */
#define  PCI_BASE_ADDRESS_SPACE         0x01U

/**
 * @macroname `PCI_BASE_ADDRESS_SPACE_IO`
 *
 * @description
 * An I/O BAR.
 */
#define  PCI_BASE_ADDRESS_SPACE_IO      0x01U

/**
 * @macroname `PCI_BASE_ADDRESS_SPACE_MEMORY`
 *
 * @description
 * A memory BAR.
 */
#define  PCI_BASE_ADDRESS_SPACE_MEMORY  0x00U

/**
 * @macroname `PCI_BASE_ADDRESS_MEM_TYPE_MASK`
 *
 * @description
 * The type bits of a memory BAR.
 */
#define  PCI_BASE_ADDRESS_MEM_TYPE_MASK 0x06U

/**
 * @macroname `PCI_BASE_ADDRESS_MEM_TYPE_32`
 *
 * @description
 * A memory BAR with a 32-bit address.
 */
#define  PCI_BASE_ADDRESS_MEM_TYPE_32   0x00U

/**
 * @macroname `PCI_BASE_ADDRESS_MEM_TYPE_1M`
 *
 * @description
 * A memory BAR below 1 MiB (obsolete).
 */
#define  PCI_BASE_ADDRESS_MEM_TYPE_1M   0x02U

/**
 * @macroname `PCI_BASE_ADDRESS_MEM_TYPE_64`
 *
 * @description
 * A memory BAR with a 64-bit address.
 */
#define  PCI_BASE_ADDRESS_MEM_TYPE_64   0x04U

/**
 * @macroname `PCI_BASE_ADDRESS_MEM_PREFETCH`
 *
 * @description
 * A prefetchable memory BAR.
 */
#define  PCI_BASE_ADDRESS_MEM_PREFETCH  0x08U

/**
 * @macroname `PCI_BASE_ADDRESS_MEM_MASK`
 *
 * @description
 * The address bits of a memory BAR.
 */
#define  PCI_BASE_ADDRESS_MEM_MASK      0xfffffff0U

/**
 * @macroname `PCI_BASE_ADDRESS_IO_MASK`
 *
 * @description
 * The address bits of an I/O BAR.
 */
#define  PCI_BASE_ADDRESS_IO_MASK       0xfffffffcU

/**
 * @macroname `PCI_ROM_ADDRESS`
 *
 * @description
 * The expansion ROM base register: bits 31..11 are the address, 10..1 reserved.
 */
#define PCI_ROM_ADDRESS         0x30

/**
 * @macroname `PCI_ROM_ADDRESS_ENABLE`
 *
 * @description
 * Enables the expansion ROM.
 */
#define  PCI_ROM_ADDRESS_ENABLE 0x01U

/**
 * @macroname `PCI_ROM_ADDRESS_MASK`
 *
 * @description
 * The address bits of the expansion ROM base.
 */
#define PCI_ROM_ADDRESS_MASK    0xfffff800U

/** @} */


/**
 * @macroname `PCI_BUS_MAX`
 *
 * @description
 * The number of PCI buses scanned.
 */
#  define PCI_BUS_MAX		8

/**
 * @macroname `PCI_DEV_MAX`
 *
 * @description
 * The number of devices on a bus.
 */
#  define PCI_DEV_MAX		32

/**
 * @macroname `PCI_FN_MAX`
 *
 * @description
 * The number of functions of a device.
 */
#  define PCI_FN_MAX		8

/**
 * @name PCI command register bits
 * Command regs
 * @{
 */

/**
 * @macroname `PCI_COMMAND_IO`
 *
 * @description
 * Enable response in I/O space.
 */
#define  PCI_COMMAND_IO           0x1U

/**
 * @macroname `PCI_COMMAND_MEMORY`
 *
 * @description
 * Enable response in Memory space.
 */
#define  PCI_COMMAND_MEMORY       0x2U

/**
 * @macroname `PCI_COMMAND_MASTER`
 *
 * @description
 * Enable bus mastering.
 */
#define  PCI_COMMAND_MASTER       0x4

/**
 * @macroname `PCI_COMMAND_SPECIAL`
 *
 * @description
 * Enable response to special cycles.
 */
#define  PCI_COMMAND_SPECIAL      0x8

/**
 * @macroname `PCI_COMMAND_INVALIDATE`
 *
 * @description
 * Use memory write and invalidate.
 */
#define  PCI_COMMAND_INVALIDATE   0x10

/**
 * @macroname `PCI_COMMAND_VGA_PALETTE`
 *
 * @description
 * Enable palette snooping.
 */
#define  PCI_COMMAND_VGA_PALETTE  0x20

/**
 * @macroname `PCI_COMMAND_PARITY`
 *
 * @description
 * Enable parity checking.
 */
#define  PCI_COMMAND_PARITY       0x40

/**
 * @macroname `PCI_COMMAND_WAIT`
 *
 * @description
 * Enable address/data stepping.
 */
#define  PCI_COMMAND_WAIT         0x80

/**
 * @macroname `PCI_COMMAND_SERR`
 *
 * @description
 * Enable SERR.
 */
#define  PCI_COMMAND_SERR         0x100

/**
 * @macroname `PCI_COMMAND_FAST_BACK`
 *
 * @description
 * Enable back-to-back writes.
 */
#define  PCI_COMMAND_FAST_BACK    0x200

/**
 * @macroname `PCI_COMMAND_INTX_DISABLE`
 *
 * @description
 * INTx Emulation Disable.
 */
#define  PCI_COMMAND_INTX_DISABLE 0x400

/** @} */


/**
 * @macroname `PCI_NUM_RESOURCES`
 *
 * @description
 * The number of resources of a PCI device: BARs 0 to 5, then the expansion ROM
 * (6).
 */
#define PCI_NUM_RESOURCES 7

/**
 * @typename `pci_resource_index_e`
 *
 * @description
 * The index of a resource of a PCI device (pci_dev_t.resources).
 */
typedef enum {
    PCI_RESOURCE_BAR0, /**< BAR 0. */
    PCI_RESOURCE_BAR1, /**< BAR 1. */
    PCI_RESOURCE_BAR2, /**< BAR 2. */
    PCI_RESOURCE_BAR3, /**< BAR 3. */
    PCI_RESOURCE_BAR4, /**< BAR 4. */
    PCI_RESOURCE_BAR5, /**< BAR 5. */
    PCI_RESOURCE_ROM /**< The expansion ROM. */
} pci_resource_index_e;

/**
 * @typename `pci_resource_type_e`
 *
 * @description
 * The kind of a resource of a PCI device.
 */
typedef enum {
  PCI_RESOURCE_TYPE_BAR_IO, /**< An I/O BAR. */
  PCI_RESOURCE_TYPE_BAR_MEM, /**< A memory BAR. */
  PCI_RESOURCE_TYPE_ROM /**< The expansion ROM. */
} pci_resource_type_e;

/**
 * @typename `pci_resource_mem_flag_masks_e`
 *
 * @description
 * The flags of a memory resource of a PCI device.
 */
typedef enum {
    /**
     * If this bit is
     *      0 then resource is 64bit region //UNSUPPORTED yet!!
     *      1 then resource is 32bit region.
     */
    PCI_RESOURCE_MEM_MASK_32 = 1,

    /**
     * If this bit is
     *      0 then resource has prefetchable memory region
     *      1 then resource has non-prefetchable memory region.
     */
    PCI_RESOURCE_MEM_MASK_PREFETCH = 2,
} pci_resource_mem_flag_masks_e;

/**
 * @typename `pci_resource_t`
 *
 * @description
 * A resource of a PCI device (a BAR or the expansion ROM). A size of 0 means
 * that the resource is not enabled or not supported by the device; a nonzero
 * size with an address of 0 means that it is not initialized.
 */
typedef struct {
    uintptr_t addr; /**< Virtual address. */
    uint64_t  pci_addr; /**< "physical" addr in pci address space. */
    size_t size; /**< Its size, in bytes. */
    pci_resource_type_e type; /**< Its kind. */

    /**
     * Sets only when type == PCI_RESOURCE_BAR_MEM. For values meaning see
     * pci_resource_mem_flag_masks_e.
     */
    uint8_t mem_flags;
} pci_resource_t;


/**
 * @typename `pci_dev_t`
 *
 * @description
 * A PCI device: its address, its IDs and its resources.
 */
typedef struct {
    uint16_t    bus; /**< Its bus. */
    uint16_t    dev; /**< Its device number. */
    uint16_t    fn; /**< Its function. */
    uint16_t    vendor_id; /**< Its vendor ID. */
    uint16_t    device_id; /**< Its device ID. */
    uint16_t    class_code; /**< Class code and sub class. */
    uint8_t     hdr_type; /**< Its header type. */
    /**
     * Its resources (pci_resource_index_e).
     */
    pci_resource_t resources[PCI_NUM_RESOURCES];
} pci_dev_t;

/** \cond */
void pci_init(void);
void pci_list(void);

//result == 0 means no error
int32_t pci_read_config_byte(pci_dev_t *dev, int32_t where, uint8_t *val);
int32_t pci_read_config_word(pci_dev_t *dev, int32_t where, uint16_t *val);
int32_t pci_read_config_dword(pci_dev_t *dev, int32_t where, uint32_t *val);
int32_t pci_write_config_byte(pci_dev_t *dev, int32_t where, uint8_t val);
int32_t pci_write_config_word(pci_dev_t *dev, int32_t where, uint16_t val);
int32_t pci_write_config_dword(pci_dev_t *dev, int32_t where, uint32_t val);


/*
 * Finds pci device by triple (bus, dev, fn) and fill info
 * about the device to struct pointed by pci_dev.
 * 'resources' field is not filled for pci bridges
 *
 *  pci_dev->vendor_id == 0xFFFF means that device does not exist
 */
void pci_get_dev_by_bdf(uint8_t bus, uint8_t dev, uint8_t fn,
    pci_dev_t *pci_dev);

//convert legacy port to mem address that can be used in ioread/iowrite functions
uintptr_t pci_convert_legacy_port(pci_dev_t *dev, uint16_t port);
/** \endcond */

/* The Radeon driver (syspart/drivers/radeon/main.c), which has no header of its
 * own. */

/**
 * @macroname `RADEON_BUS`
 *
 * @description
 * The PCI bus of the Radeon card.
 */
#define RADEON_BUS 1

/**
 * @macroname `RADEON_DEV`
 *
 * @description
 * The PCI device of the Radeon card.
 */
#define RADEON_DEV 0

/**
 * @macroname `RADEON_FUN`
 *
 * @description
 * The PCI function of the Radeon card.
 */
#define RADEON_FUN 0

/**
 * @macroname `RBIOS8`
 *
 * @description
 * The byte `i` of the BIOS of the Radeon card (radeon_bios, a variable of the
 * driver).
 *
 * @params
 * | Name | Description |
 * |------|-------------|
 * | `i` | The offset in the BIOS. |
 */
#define RBIOS8(i) (radeon_bios[i])

/**
 * @macroname `RBIOS16`
 *
 * @description
 * The 16-bit little endian word at the offset `i` of the BIOS of the Radeon
 * card.
 *
 * @params
 * | Name | Description |
 * |------|-------------|
 * | `i` | The offset in the BIOS. |
 */
#define RBIOS16(i) ((uint16_t) ((uint32_t) RBIOS8(i) \
                        | ((uint32_t) RBIOS8((i) + 1U) << 8)))

/** \cond */
/* Test for the P3041 board (nothing in this tree calls it). */
void p3041_test(void);
/** \endcond */

# endif /* __POK_PCI_H__ */
