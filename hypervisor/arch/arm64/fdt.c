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
 * The host's flattened device tree, as QEMU 'virt' gives it (at the start
 * of the RAM): one pass over its structure block collects what the
 * hypervisor needs (hv_fdt_info_t): the RAM (/memory@...), the CPUs'
 * MPIDRs (/cpus/cpu@...), the GIC (/intc@...: version and register
 * ranges), the PCI configuration space (/pcie@...) and whether the second
 * PL011 exists (/pl011@9040000). Everything is checked against the size
 * of the blob.
 */

#include <arch/internal.h>

#define FDT_MAGIC           0xD00DFEEDU
#define FDT_HEADER_SIZE     40U
#define FDT_SIZE_MAX        0x100000U       // QEMU's tree is far smaller
#define FDT_OFF_TOTALSIZE   4U
#define FDT_OFF_STRUCT      8U
#define FDT_OFF_STRINGS     12U
#define FDT_OFF_VERSION     20U
#define FDT_OFF_SIZE_STRINGS 32U
#define FDT_OFF_SIZE_STRUCT 36U
#define FDT_VERSION_MIN     16U

#define FDT_BEGIN_NODE      1U
#define FDT_END_NODE        2U
#define FDT_PROP            3U
#define FDT_NOP             4U
#define FDT_END             9U

#define DEPTH_MAX           16U

// The nodes looked at; the others are skipped.
typedef enum {
    NODE_OTHER,
    NODE_ROOT,
    NODE_MEMORY,
    NODE_CPUS,
    NODE_CPU,
    NODE_GIC,
    NODE_PCIE
} node_t;

typedef struct {
    const uint8_t *blob;
    uint32_t size;              // totalsize
    uint32_t strings;           // Offset of the strings block
    uint32_t strings_size;
} fdt_t;

// Big-endian 32-bit word at offset (checked by the caller).
static uint32_t be32(const uint8_t *p, uint32_t offset)
{
    return ((uint32_t) p[offset] << 24) | ((uint32_t) p[offset + 1U] << 16)
           | ((uint32_t) p[offset + 2U] << 8) | (uint32_t) p[offset + 3U];
}

// Round up to the 4-byte alignment of the tokens.
static uint32_t align4(uint32_t n)
{
    return (n + 3U) & ~3U;
}

/*
 * Whether the NUL-terminated name at blob[offset] (within limit) starts
 * with prefix (or is equal to it, if whole).
 */
static bool name_is(const fdt_t *fdt, uint32_t offset, uint32_t limit,
                    const char *prefix, bool whole)
{
    uint32_t i = 0U;
    bool match = true;

    while (match && (prefix[i] != '\0')) {
        if (((offset + i) >= limit)
            || ((char) fdt->blob[offset + i] != prefix[i])) {
            match = false;
        }
        i++;
    }
    if (match && whole) {
        match = ((offset + i) < limit) && (fdt->blob[offset + i] == 0U);
    }
    return match;
}

// Whether property name number nameoff (in the strings block) is name.
static bool prop_is(const fdt_t *fdt, uint32_t nameoff, const char *name)
{
    return (nameoff < fdt->strings_size)
           && name_is(fdt, fdt->strings + nameoff,
                      fdt->strings + fdt->strings_size, name, true);
}

// Whether the string list value (length bytes at offset) contains s.
static bool value_has_string(const fdt_t *fdt, uint32_t offset,
                             uint32_t length, const char *s)
{
    uint32_t pos = 0U;
    bool found = false;

    while (!found && (pos < length)) {
        found = name_is(fdt, offset + pos, offset + length, s, true);
        // On to the next string.
        while ((pos < length) && (fdt->blob[offset + pos] != 0U)) {
            pos++;
        }
        pos++;
    }
    return found;
}

// A value of cells 32-bit cells at blob[offset] (1 or 2 cells, checked).
static uint64_t cells_value(const fdt_t *fdt, uint32_t offset,
                            uint32_t cells)
{
    uint64_t value = be32(fdt->blob, offset);

    if (cells == 2U) {
        value = (value << 32) | be32(fdt->blob, offset + 4U);
    }
    return value;
}

// Kind of a node from its name at blob[offset] and its depth.
static node_t node_kind(const fdt_t *fdt, uint32_t offset, uint32_t limit,
                        uint32_t depth, node_t parent, hv_fdt_info_t *info)
{
    node_t kind = NODE_OTHER;

    if (depth == 1U) {
        kind = NODE_ROOT;
    } else if ((depth == 2U) && (parent == NODE_ROOT)) {
        if (name_is(fdt, offset, limit, "memory@", false)) {
            kind = NODE_MEMORY;
        } else if (name_is(fdt, offset, limit, "cpus", true)) {
            kind = NODE_CPUS;
        } else if (name_is(fdt, offset, limit, "intc@", false)) {
            kind = NODE_GIC;
        } else if (name_is(fdt, offset, limit, "pcie@", false)) {
            kind = NODE_PCIE;
        } else if (name_is(fdt, offset, limit, "pl011@9040000", true)) {
            info->uart1 = true;
        } else {
            // Not needed.
        }
    } else if ((depth == 3U) && (parent == NODE_CPUS)
               && name_is(fdt, offset, limit, "cpu@", false)) {
        kind = NODE_CPU;
    } else {
        // Not needed.
    }
    return kind;
}

/*
 * Cells of the root's addresses and sizes, and of the CPUs' reg
 * (#address-cells of /cpus).
 */
typedef struct {
    uint32_t address_cells;
    uint32_t size_cells;
    uint32_t cpu_cells;
} cells_t;

// Use a property of a node of kind node.
static void use_property(const fdt_t *fdt, node_t node, uint32_t nameoff,
                         uint32_t value, uint32_t length, cells_t *cells,
                         hv_fdt_info_t *info)
{
    bool cells_ok = (cells->address_cells >= 1U)
                    && (cells->address_cells <= 2U)
                    && (cells->size_cells >= 1U) && (cells->size_cells <= 2U);
    // Bytes of one (address, size) entry of a reg property; 0: not known.
    uint32_t entry = cells_ok ? ((cells->address_cells + cells->size_cells)
                                 * 4U) : 0U;

    if ((node == NODE_ROOT) && (length == 4U)) {
        if (prop_is(fdt, nameoff, "#address-cells")) {
            cells->address_cells = be32(fdt->blob, value);
        } else if (prop_is(fdt, nameoff, "#size-cells")) {
            cells->size_cells = be32(fdt->blob, value);
        } else {
            // Not needed.
        }
    } else if ((node == NODE_CPUS) && (length == 4U)
               && prop_is(fdt, nameoff, "#address-cells")) {
        cells->cpu_cells = be32(fdt->blob, value);
    } else if ((node == NODE_CPU) && prop_is(fdt, nameoff, "reg")
               && (cells->cpu_cells >= 1U) && (cells->cpu_cells <= 2U)
               && (length == (cells->cpu_cells * 4U))) {
        // Affinity levels 0-2 of the MPIDR (the last cell).
        if (info->nb_cpus < HV_MAX_CPUS) {
            info->mpidr[info->nb_cpus] =
                be32(fdt->blob, (value + length) - 4U) & 0xFFFFFFU;
        }
        info->nb_cpus++;
    } else if ((node == NODE_MEMORY) && prop_is(fdt, nameoff, "reg")
               && (entry != 0U) && (length >= entry)
               && (info->ram_size == 0U)) {
        info->ram_base = cells_value(fdt, value, cells->address_cells);
        info->ram_size = cells_value(fdt, value
                                     + (cells->address_cells * 4U),
                                     cells->size_cells);
    } else if ((node == NODE_PCIE) && prop_is(fdt, nameoff, "reg")
               && (entry != 0U) && (length >= entry)) {
        info->ecam_base = cells_value(fdt, value, cells->address_cells);
        info->ecam_size = cells_value(fdt, value
                                      + (cells->address_cells * 4U),
                                      cells->size_cells);
    } else if ((node == NODE_GIC) && prop_is(fdt, nameoff, "compatible")) {
        info->gic_v2 = value_has_string(fdt, value, length,
                                        "arm,cortex-a15-gic");
    } else if ((node == NODE_GIC) && prop_is(fdt, nameoff, "reg")
               && (entry != 0U)) {
        // Distributor, CPU interface, and with the virtualization
        // extensions the virtual interface control and virtual CPU
        // interface.
        uint64_t *bases[4] = { &info->gicd, &info->gicc, &info->gich,
                               &info->gicv };

        info->gic_regs = length / entry;
        for (uint32_t i = 0U; (i < info->gic_regs) && (i < 4U); i++) {
            *bases[i] = cells_value(fdt, value + (i * entry),
                                    cells->address_cells);
        }
        if (info->gic_regs >= 4U) {
            info->gicv_size = cells_value(fdt, value + (3U * entry)
                                          + (cells->address_cells * 4U),
                                          cells->size_cells);
        }
    } else {
        // Not needed.
    }
}

/*
 * Read the device tree at address into info; returns false if there is
 * no valid tree there.
 */
/*
 * MISRA C:2025 Rule 11.4 deviation (fdt_read()): QEMU puts the device
 * tree at a physical address (identity-mapped), converted to a pointer.
 */
bool fdt_read(uint64_t address, hv_fdt_info_t *info)
{
    fdt_t fdt;
    cells_t cells = { 2U, 1U, 1U };
    node_t stack[DEPTH_MAX];
    uint32_t depth = 0U;
    uint32_t pos;
    uint32_t end;
    bool ok = true;
    bool done = false;

    hv_memset(info, 0U, sizeof(*info));
    fdt.blob = (const uint8_t *) (uintptr_t) address;
    if (be32(fdt.blob, 0U) != FDT_MAGIC) {
        return false;
    }
    fdt.size = be32(fdt.blob, FDT_OFF_TOTALSIZE);
    pos = be32(fdt.blob, FDT_OFF_STRUCT);
    end = pos + be32(fdt.blob, FDT_OFF_SIZE_STRUCT);
    fdt.strings = be32(fdt.blob, FDT_OFF_STRINGS);
    fdt.strings_size = be32(fdt.blob, FDT_OFF_SIZE_STRINGS);
    if ((fdt.size > FDT_SIZE_MAX) || (fdt.size < FDT_HEADER_SIZE)
        || (be32(fdt.blob, FDT_OFF_VERSION) < FDT_VERSION_MIN)
        || (pos > fdt.size) || (end > fdt.size) || (end < pos)
        || (fdt.strings > fdt.size)
        || (fdt.strings_size > (fdt.size - fdt.strings))) {
        return false;
    }

    while (ok && !done) {
        uint32_t token = FDT_END + 1U;      // Not a token

        if ((pos + 4U) <= end) {
            token = be32(fdt.blob, pos);
            pos += 4U;
        }
        if (token == FDT_BEGIN_NODE) {
            uint32_t name = pos;
            node_t parent = (depth > 0U) ? stack[depth - 1U] : NODE_OTHER;

            while ((pos < end) && (fdt.blob[pos] != 0U)) {
                pos++;
            }
            if ((pos >= end) || (depth >= DEPTH_MAX)) {
                ok = false;
            } else {
                depth++;
                stack[depth - 1U] = node_kind(&fdt, name, end, depth,
                                              parent, info);
                pos = name + align4((pos - name) + 1U);
            }
        } else if (token == FDT_END_NODE) {
            if (depth == 0U) {
                ok = false;
            } else {
                depth--;
            }
        } else if (token == FDT_PROP) {
            uint32_t length;
            uint32_t nameoff;

            if ((pos + 8U) > end) {
                ok = false;
            } else {
                length = be32(fdt.blob, pos);
                nameoff = be32(fdt.blob, pos + 4U);
                pos += 8U;
                if ((length > (end - pos)) || (depth == 0U)) {
                    ok = false;
                } else {
                    use_property(&fdt, stack[depth - 1U], nameoff, pos,
                                 length, &cells, info);
                    pos += align4(length);
                }
            }
        } else if (token == FDT_NOP) {
            // Nothing.
        } else if (token == FDT_END) {
            done = true;
        } else {
            ok = false;
        }
    }
    return ok && done;
}
