/*
 * Modified by Kirill Brazhnikov, 2026-10-31: built-in integer types (int,
 * unsigned, short, long, ...) are replaced with the fixed-width types
 * from <stdint.h>.
 *
 * Modified by Kirill Brazhnikov, 2026-11-04: MISRA C:2025: braces around
 * all if/else/loop bodies; discarded results are cast to void; functions
 * without parameters are declared with (void); file-local definitions
 * have internal linkage; essentially Boolean conditions; U suffixes for
 * unsigned constants; operands of mixed precedence are parenthesized;
 * deviations are documented where a guideline can't be followed;
 * radeon_get_temp() checks the device it has read (it checked the other
 * function's device) and keeps the 64-bit register address; the printf()
 * formats match their arguments; the BIOS signatures are compared as
 * bytes; rreg() is a function.
 *
 * Modified by Kirill Brazhnikov, 2026-11-08: Doxygen headers for the file
 * and its functions.
 *
 * Modified by Kirill Brazhnikov, 2026-11-14: the macros are in
 * syspart/include/pci.h; the struct types are used by their typedef names.
 */

/**
 * @file
 * Radeon tests for the P3041 board: the ATOM BIOS signature and the
 * temperature sensor.
 */
#include <stdio.h>
#include <string.h>
#include <pci.h>

static pci_dev_t radeon_dev;
static uint8_t *radeon_bios;

/**
 * @funcname `radeon_tst_atom_magic_simple`
 *
 * @description
 * Tests whether the ROM of a Radeon device has an ATOM BIOS ("ATOM" or "MOTA"
 * at the BIOS header).
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `b` | `int32_t` | [in] The bus. |
 * | `d` | `int32_t` | [in] The device. |
 * | `f` | `int32_t` | [in] The function. |
 *
 * @return 0 if it does, -1 otherwise.
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `radeon_bios` | `uint8_t *` | The Radeon's ROM |
 * | `radeon_dev` | `pci_dev_t` | The PCI device of the Radeon |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `atom_magic` | `static const uint8_t[4]` | "ATOM" as bytes |
 * | `bios_header_start` | `uint16_t` | The offset of the BIOS header |
 * | `mota_magic` | `static const uint8_t[4]` | "MOTA" as bytes |
 * | `res` | `int32_t` | The result |
 * | `size` | `size_t` | The size of the ROM |
 * | `tmp` | `uint16_t` | An offset in the ROM |
 * | `val1` | `int32_t` | The first byte of the ROM |
 * | `val2` | `int32_t` | The second byte |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Set `val1` to `0`.
 * 2. Set `val2` to `0`.
 * 3. Set `res` to `-1`.
 * 4. Set `size` to `0`.
 * 5. Call `printf()` with the arguments
 *    `"radeon ATOM test. pci(%%d:%%d:%%d)\n"`, `b`, `d` and `f`.
 * 6. Call `memset()` with the arguments the address of `radeon_dev`, `0x0` and
 *    `sizeof(radeon_dev)`.
 * 7. Call `pci_get_dev_by_bdf()` with the arguments `b`, `d`, `f` and the
 *    address of `radeon_dev`.
 * 8. If `radeon_dev.vendor_id` is equal to `0xffffU`:
 *    1. Call `printf()` with the argument
 *       `"radeon test: Have not found device\n"`.
 *    2. Return `res`.
 * 9. Call `printf()` with the arguments `"radeon test: vendor: 0x%04x\n"` and
 *    `radeon_dev.vendor_id`.
 * 10. Call `printf()` with the arguments `"radeon test: device: 0x%04x\n"` and
 *     `radeon_dev.device_id`.
 * 11. Set `radeon_bios` to
 *     `(uint8_t*)radeon_dev.resources[PCI_RESOURCE_ROM].addr`.
 * 12. Set `size` to `radeon_dev.resources[PCI_RESOURCE_ROM].size`.
 * 13. If `radeon_bios` is NULL:
 *     1. Call `printf()` with the argument
 *        `"radeon test: Wrong ROM address\n"`.
 *     2. Return `res`.
 * 14. If `size` is greater than or equal to `2U`:
 *     1. Call `ioread8()` with the argument `radeon_bios`, and store the result
 *        in `val1`.
 *     2. Call `ioread8()` with the argument `radeon_bios + 1`, and store the
 *        result in `val2`.
 *     3. Call `printf()` with the arguments
 *        `"radeon test: rom(%%p, %%zu): 0x%%x 0x%%x\n"`,
 *        `(void *) radeon_bios`, `size`, `(unsigned int) val1` and
 *        `(unsigned int) val2`.
 * 15. Otherwise:
 *     1. Call `printf()` with the argument
 *        `"radeon test: error: invalid rom size\n"`.
 *     2. Return `res`.
 * 16. Set `tmp` to `RBIOS16(0x18U)`.
 * 17. If `RBIOS8(tmp + 0x14U)` is not equal to `0x0U`:
 *     1. Call `printf()` with the argument `"Not an x86 BIOS ROM\n"`.
 * 18. Set `bios_header_start` to `RBIOS16(0x48U)`.
 * 19. Set `tmp` to `bios_header_start + 4U`.
 * 20. If `memcmp(&radeon_bios[tmp], atom_magic, sizeof(atom_magic))` is equal
 *     to `0` or `memcmp(&radeon_bios[tmp], mota_magic, sizeof(mota_magic))` is
 *     equal to `0`:
 *     1. Call `printf()` with the argument `"ATOM BIOS detected\n"`.
 *     2. Set `res` to `0`.
 * 21. Otherwise:
 *     1. Call `printf()` with the argument `"Fail. BIOS not detected\n"`.
 * 22. Return `res`.
 *
 * @calls
 * - `printf()`
 * - `memset()`
 * - `pci_get_dev_by_bdf()`
 * - `ioread8()`
 * - `memcmp()`
 */
static int32_t radeon_tst_atom_magic_simple(int32_t b, int32_t d, int32_t f)
{
    int32_t val1 = 0, val2 = 0, res = -1;
    size_t size = 0;
    uint16_t tmp;
    uint16_t bios_header_start;
    /* "ATOM" and "MOTA" as bytes. */
    static const uint8_t atom_magic[4] = { 0x41U, 0x54U, 0x4fU, 0x4dU };
    static const uint8_t mota_magic[4] = { 0x4dU, 0x4fU, 0x54U, 0x41U };


    (void) printf("radeon ATOM test. pci(%d:%d:%d)\n", b, d, f);

    (void) memset(&radeon_dev, 0x0, sizeof(radeon_dev));

    pci_get_dev_by_bdf(b, d, f, &radeon_dev);
    if (radeon_dev.vendor_id == 0xffffU) {
        (void) printf("radeon test: Have not found device\n");
        return res;
    }

    (void) printf("radeon test: vendor: 0x%04x\n", radeon_dev.vendor_id);
    (void) printf("radeon test: device: 0x%04x\n", radeon_dev.device_id);

    /* MISRA C:2025 Rule 11.4 deviation: the ROM is mapped at the address
     * that the PCI configuration gives as an integer. */
    radeon_bios = (uint8_t*)radeon_dev.resources[PCI_RESOURCE_ROM].addr;
    size = radeon_dev.resources[PCI_RESOURCE_ROM].size;
    if (radeon_bios == NULL) {
        (void) printf("radeon test: Wrong ROM address\n");
        return res;
    }

    if (size >= 2U) {
        val1 = ioread8(radeon_bios);
        val2 = ioread8(radeon_bios + 1);

        (void) printf("radeon test: rom(%p, %zu): 0x%x 0x%x\n",
                      (void *) radeon_bios, size, (unsigned int) val1,
                      (unsigned int) val2);
    } else {
        (void) printf("radeon test: error: invalid rom size\n");

        return res;
    }

    tmp = RBIOS16(0x18U);

    if (RBIOS8(tmp + 0x14U) != 0x0U) {
        (void) printf("Not an x86 BIOS ROM\n");
    }

    bios_header_start = RBIOS16(0x48U);
    tmp = bios_header_start + 4U;

    if ((memcmp(&radeon_bios[tmp], atom_magic, sizeof(atom_magic)) == 0) ||
            (memcmp(&radeon_bios[tmp], mota_magic, sizeof(mota_magic)) == 0)) {
        (void) printf("ATOM BIOS detected\n");

        res = 0;
    } else {
        (void) printf("Fail. BIOS not detected\n");
    }

    return res;
}

/**
 * @funcname `rreg`
 *
 * @description
 * Reads a register of the Radeon's MMIO area (directly, or through the index
 * register for one beyond the area).
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `base` | `uintptr_t` | [in] The MMIO area. |
 * | `size` | `size_t` | [in] Its size. |
 * | `reg` | `uint32_t` | [in] The register. |
 *
 * @return The value.
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `val` | `uint32_t` | The value |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. If `reg` is less than `size`:
 *    1. Call `ioread32()` with the argument `(uint32_t *) base + reg`, and
 *       store the result in `val`.
 * 2. Otherwise:
 *    1. Call `iowrite32()` with the arguments `reg` and `(uint32_t *) base`.
 *    2. Call `ioread32()` with the argument `(uint32_t *) base + 0x4`, and
 *       store the result in `val`.
 * 3. Return `val`.
 *
 * @calls
 * - `ioread32()`
 * - `iowrite32()`
 */
/*
 * MISRA C:2025 Rule 11.4 deviation: the registers are mapped at the address
 * that the PCI configuration gives as an integer.
 */
static uint32_t rreg(uintptr_t base, size_t size, uint32_t reg)
{
    uint32_t val;

    if (reg < size) {
        val = ioread32((uint32_t *) base + reg);
    } else {
        iowrite32(reg, (uint32_t *) base);
        val = ioread32((uint32_t *) base + 0x4);
    }
    return val;
}

/**
 * @funcname `radeon_get_temp`
 *
 * @description
 * Reads the temperature of a Radeon device from its sensor register.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `b` | `int32_t` | [in] The bus. |
 * | `d` | `int32_t` | [in] The device. |
 * | `f` | `int32_t` | [in] The function. |
 *
 * @return The temperature, in half degrees Celsius, or -1 if there is no
 *         device.
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `actual_temp` | `int32_t` | The temperature |
 * | `radeon_pci_dev` | `pci_dev_t` | The device |
 * | `radeon_rmmio_base` | `uintptr_t` | Its MMIO area |
 * | `radeon_rmmio_size` | `size_t` | Its size |
 * | `temp` | `uint32_t` | The sensor's value |
 * | `value` | `uint32_t` | The bits of the temperature |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Call `printf()` with the argument `"radeon temp test\n"`.
 * 2. Call `memset()` with the arguments the address of `radeon_pci_dev`, `0x0`
 *    and `sizeof(radeon_pci_dev)`.
 * 3. Call `pci_get_dev_by_bdf()` with the arguments `b`, `d`, `f` and the
 *    address of `radeon_pci_dev`.
 * 4. If `radeon_pci_dev.vendor_id` is equal to `0xffffU`:
 *    1. Call `printf()` with the argument
 *       `"radeon test: Have not found device\n"`.
 *    2. Return `-1`.
 * 5. Set `radeon_rmmio_base` to
 *    `radeon_pci_dev.resources[PCI_RESOURCE_BAR2].addr`.
 * 6. Set `radeon_rmmio_size` to
 *    `radeon_pci_dev.resources[PCI_RESOURCE_BAR2].size`.
 * 7. Call `printf()` with the arguments
 *    `"radeon: rmmio: 0x%%lx, size: %%zu\n"`,
 *    `(unsigned long) radeon_rmmio_base` and `radeon_rmmio_size`.
 * 8. Set `temp` to
 *    `(rreg(radeon_rmmio_base, radeon_rmmio_size, 0x740U) & 0x3FF0000U) >>`
 *    `16`.
 * 9. If `(temp & 0x400U)` is not equal to `0U`:
 *    1. Set `actual_temp` to `-256`.
 * 10. Otherwise, if `(temp & 0x200U)` is not equal to `0U`:
 *     1. Set `actual_temp` to `255`.
 * 11. Otherwise, if `(temp & 0x100U)` is not equal to `0U`:
 *     1. Set `value` to `temp & 0x1ffU`.
 *     2. Set `actual_temp` to `(int32_t) value`.
 *     3. Set the bits `~0x1ff` in `actual_temp`.
 * 12. Otherwise:
 *     1. Set `value` to `temp & 0xffU`.
 *     2. Set `actual_temp` to `(int32_t) value`.
 * 13. Call `printf()` with the arguments `"radeon temp: %%d\n"` and
 *     `(actual_temp * 1000) / 2`.
 * 14. Return `actual_temp`.
 *
 * @calls
 * - `printf()`
 * - `memset()`
 * - `pci_get_dev_by_bdf()`
 * - `rreg()`
 */
static int32_t radeon_get_temp(int32_t b, int32_t d, int32_t f)
{
    pci_dev_t radeon_pci_dev;
    uintptr_t radeon_rmmio_base;
    size_t radeon_rmmio_size;
    uint32_t temp;
    int32_t actual_temp;

    (void) printf("radeon temp test\n");

    (void) memset(&radeon_pci_dev, 0x0, sizeof(radeon_pci_dev));

    pci_get_dev_by_bdf(b, d, f, &radeon_pci_dev);
    if (radeon_pci_dev.vendor_id == 0xffffU) {
        (void) printf("radeon test: Have not found device\n");
        return -1;
    }

    radeon_rmmio_base = radeon_pci_dev.resources[PCI_RESOURCE_BAR2].addr;
    radeon_rmmio_size = radeon_pci_dev.resources[PCI_RESOURCE_BAR2].size;

    (void) printf("radeon: rmmio: 0x%lx, size: %zu\n",
                  (unsigned long) radeon_rmmio_base, radeon_rmmio_size);

    temp =
        (rreg(radeon_rmmio_base, radeon_rmmio_size, 0x740U) & 0x3FF0000U) >> 16;

    if ((temp & 0x400U) != 0U) {
        actual_temp = -256;
    } else if ((temp & 0x200U) != 0U) {
        actual_temp = 255;
    } else if ((temp & 0x100U) != 0U) {
        uint32_t value = temp & 0x1ffU;
        actual_temp = (int32_t) value;
        actual_temp |= ~0x1ff;
    } else {
        uint32_t value = temp & 0xffU;
        actual_temp = (int32_t) value;
    }

    (void) printf("radeon temp: %d\n", (actual_temp * 1000) / 2);

    return actual_temp;
}

/**
 * @funcname `p3041_test`
 *
 * @description
 * The Radeon tests for the P3041 board (nothing in this tree calls it).
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
 * 1. Call `radeon_tst_atom_magic_simple()` with the arguments `RADEON_BUS`,
 *    `RADEON_DEV` and `RADEON_FUN`.
 * 2. Call `radeon_get_temp()` with the arguments `RADEON_BUS`, `RADEON_DEV` and
 *    `RADEON_FUN`.
 *
 * @calls
 * - `radeon_tst_atom_magic_simple()`
 * - `radeon_get_temp()`
 */
void p3041_test(void)
{
    (void) radeon_tst_atom_magic_simple(RADEON_BUS, RADEON_DEV, RADEON_FUN);
    (void) radeon_get_temp(RADEON_BUS, RADEON_DEV, RADEON_FUN);
}
