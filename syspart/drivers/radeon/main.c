/* Modified by Kirill Brazhnikov, 2026-10-31: built-in integer types (int,
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
 */
#include <stdio.h>
#include <string.h>
#include <pci.h>


#define RADEON_BUS 1
#define RADEON_DEV 0
#define RADEON_FUN 0

static struct pci_dev radeon_dev;
static uint8_t *radeon_bios;

#define RBIOS8(i) (radeon_bios[i])
#define RBIOS16(i) ((uint16_t) ((uint32_t) RBIOS8(i) \
                        | ((uint32_t) RBIOS8((i) + 1U) << 8)))

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

static int32_t radeon_get_temp(int32_t b, int32_t d, int32_t f)
{
    struct pci_dev radeon_pci_dev;
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

/* Test for the P3041 board (nothing in this tree calls it). */
void p3041_test(void);
void p3041_test(void)
{
    (void) radeon_tst_atom_magic_simple(RADEON_BUS, RADEON_DEV, RADEON_FUN);
    (void) radeon_get_temp(RADEON_BUS, RADEON_DEV, RADEON_FUN);
}
