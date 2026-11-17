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
 * Modified by Kirill Brazhnikov, 2026-11-08: Doxygen headers for the file and
 * its functions.
 *
 * Modified by Kirill Brazhnikov, 2026-11-14: the enum types are named with _e:
 * node_e (was node_t); the macros, types and declarations are in
 * arch/internal.h, documented there.
 *
 * Modified by Kirill Brazhnikov, 2026-11-15: moved from
 * hypervisor/arch/arm64/fdt.c (the code of the board, its devices and the
 * virtual devices of the VMs is in hypervisor/bsps); memset() of the library
 * shared with the kernel replaces hv_memset().
 *
 * Modified by Kirill Brazhnikov, 2026-11-16: high and low level requirements of
 * the functions, traced to the system requirements (docs/SRQs), and the steps
 * of their design linked to the LLRs.
 *
 * Modified by Kirill Brazhnikov, 2026-11-17: requirements with more than one
 * "shall" are split into requirements of one "shall" each.
 */

/**
 * @file
 * The host's flattened device tree, as QEMU 'virt' gives it (at the start
 * of the RAM): one pass over its structure block collects what the
 * hypervisor needs (hv_fdt_info_t): the RAM (/memory@...), the CPUs' MPIDRs
 * (/cpus/cpu@...), the GIC (/intc@...: version and register ranges), the PCI
 * configuration space (/pcie@...) and whether the second PL011 exists
 * (/pl011@9040000). Everything is checked against the size of the blob.
 */

#include <arch/internal.h>

/**
 * @funcname `be32`
 *
 * @description
 * Reads a big-endian 32-bit word of the device tree, byte by byte; the caller
 * checks that it is inside the blob.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `p` | `const uint8_t *` | [in] The blob. |
 * | `offset` | `uint32_t` | [in] The offset of the word. |
 *
 * @return The word.
 *
 * @globals
 * None.
 *
 * @locals
 * None.
 *
 * @hlr
 * - \hlrid{HLR_FDT_BE32_01}: `be32()` shall read a big-endian 32-bit word of
 *   the device tree of the host at any alignment.
 *   <br>Traces to:
 *   - \srqref{SRQ_HV_BOOT_004}
 *
 * @llr
 * - \llrid{LLR_FDT_BE32_01}: `be32()` shall return the 32-bit value whose
 *   bytes, from the most significant, are `p[offset]`, `p[offset + 1]`,
 *   `p[offset + 2]` and `p[offset + 3]`.
 *   <br>Traces to:
 *   - \hlrref{HLR_FDT_BE32_01}
 *
 * @design
 * 1. Return
 *    `((uint32_t) p[offset] << 24) | ((uint32_t) p[offset + 1U] << 16) |`
 *    `((uint32_t) p[offset + 2U] << 8) | (uint32_t) p[offset + 3U]`.
 *    \llrref{LLR_FDT_BE32_01}
 *
 * @calls
 * None.
 */
static uint32_t be32(const uint8_t *p, uint32_t offset)
{
    return ((uint32_t) p[offset] << 24) | ((uint32_t) p[offset + 1U] << 16)
           | ((uint32_t) p[offset + 2U] << 8) | (uint32_t) p[offset + 3U];
}

/**
 * @funcname `align4`
 *
 * @description
 * Rounds a size up to the 4-byte alignment of the tokens.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `n` | `uint32_t` | [in] The size. |
 *
 * @return n rounded up to a multiple of 4.
 *
 * @globals
 * None.
 *
 * @locals
 * None.
 *
 * @hlr
 * - \hlrid{HLR_FDT_ALIGN4_01}: `align4()` shall round a size of the device tree
 *   up to a multiple of 4.
 *   <br>Traces to:
 *   - \srqref{SRQ_HV_BOOT_004}
 *
 * @llr
 * - \llrid{LLR_FDT_ALIGN4_01}: `align4()` shall return `(n + 3U) & ~3U`.
 *   <br>Traces to:
 *   - \hlrref{HLR_FDT_ALIGN4_01}
 *
 * @design
 * 1. Return `(n + 3U) & ~3U`. \llrref{LLR_FDT_ALIGN4_01}
 *
 * @calls
 * None.
 */
static uint32_t align4(uint32_t n)
{
    return (n + 3U) & ~3U;
}

/**
 * @funcname `name_is`
 *
 * @description
 * Tells whether the NUL-terminated name at an offset of the blob starts with a
 * prefix, or is equal to it.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `fdt` | `const fdt_t *` | [in] The device tree. |
 * | `offset` | `uint32_t` | [in] The offset of the name. |
 * | `limit` | `uint32_t` | [in] The offset where the name must end (not read beyond). |
 * | `prefix` | `const char *` | [in] The prefix. |
 * | `whole` | `bool` | [in] true if the name must be equal to the prefix. |
 *
 * @return true if the name matches.
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `i` | `uint32_t` | The index in the prefix |
 * | `match` | `bool` | Whether the name matches so far |
 *
 * @hlr
 * - \hlrid{HLR_FDT_NAME_IS_01}: `name_is()` shall tell whether a name in the
 *   device tree starts with a prefix, or is equal to it, reading nothing beyond
 *   a limit.
 *   <br>Traces to:
 *   - \srqref{SRQ_HV_BOOT_004}
 *
 * @llr
 * - \llrid{LLR_FDT_NAME_IS_01}: If each character of `prefix` is equal to the
 *   byte of `fdt->blob` at `offset` plus its position, each of these bytes
 *   below `limit`, and `whole` is false, `name_is()` shall return true.
 *   <br>Traces to:
 *   - \hlrref{HLR_FDT_NAME_IS_01}
 * - \llrid{LLR_FDT_NAME_IS_02}: If each character of `prefix` is equal to the
 *   byte of `fdt->blob` at `offset` plus its position, each of these bytes
 *   below `limit`, `whole` is true, and the byte after them is below `limit`
 *   and is 0, `name_is()` shall return true.
 *   <br>Traces to:
 *   - \hlrref{HLR_FDT_NAME_IS_01}
 * - \llrid{LLR_FDT_NAME_IS_03}: If a character of `prefix` has its position at
 *   `limit` or beyond, or is not equal to the byte of `fdt->blob` at its
 *   position, `name_is()` shall return false, without reading the bytes after
 *   that position.
 *   <br>Traces to:
 *   - \hlrref{HLR_FDT_NAME_IS_01}
 * - \llrid{LLR_FDT_NAME_IS_04}: If each character of `prefix` is equal to the
 *   byte of `fdt->blob` at `offset` plus its position, `whole` is true, and the
 *   byte after them is at `limit` or is not 0, `name_is()` shall return false.
 *   <br>Traces to:
 *   - \hlrref{HLR_FDT_NAME_IS_01}
 *
 * @design
 * 1. Set `i` to `0U`. \llrref{LLR_FDT_NAME_IS_01}, \llrref{LLR_FDT_NAME_IS_02},
 *    \llrref{LLR_FDT_NAME_IS_03}, \llrref{LLR_FDT_NAME_IS_04}
 * 2. Set `match` to `true`. \llrref{LLR_FDT_NAME_IS_01},
 *    \llrref{LLR_FDT_NAME_IS_02}, \llrref{LLR_FDT_NAME_IS_03},
 *    \llrref{LLR_FDT_NAME_IS_04}
 * 3. While `match` is true and `prefix[i]` is not equal to `'\0'`:
 *    \llrref{LLR_FDT_NAME_IS_01}, \llrref{LLR_FDT_NAME_IS_02},
 *    \llrref{LLR_FDT_NAME_IS_03}, \llrref{LLR_FDT_NAME_IS_04}
 *    1. If `(offset + i)` is greater than or equal to `limit` or
 *       `(char) fdt->blob[offset + i]` is not equal to `prefix[i]`:
 *       \llrref{LLR_FDT_NAME_IS_03}
 *       1. Set `match` to `false`.
 *    2. Increment `i`.
 * 4. If `match` is true and `whole` is true: \llrref{LLR_FDT_NAME_IS_01},
 *    \llrref{LLR_FDT_NAME_IS_02}, \llrref{LLR_FDT_NAME_IS_04}
 *    1. Set `match` to
 *       `((offset + i) < limit) && (fdt->blob[offset + i] == 0U)`.
 *       \llrref{LLR_FDT_NAME_IS_02}, \llrref{LLR_FDT_NAME_IS_04}
 * 5. Return `match`. \llrref{LLR_FDT_NAME_IS_01}, \llrref{LLR_FDT_NAME_IS_02},
 *    \llrref{LLR_FDT_NAME_IS_03}, \llrref{LLR_FDT_NAME_IS_04}
 *
 * @calls
 * None.
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

/**
 * @funcname `prop_is`
 *
 * @description
 * Tells whether a property's name, at an offset of the strings block, is a
 * given name.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `fdt` | `const fdt_t *` | [in] The device tree. |
 * | `nameoff` | `uint32_t` | [in] The offset of the name in the strings block. |
 * | `name` | `const char *` | [in] The name. |
 *
 * @return true if the names are equal.
 *
 * @globals
 * None.
 *
 * @locals
 * None.
 *
 * @hlr
 * - \hlrid{HLR_FDT_PROP_IS_01}: `prop_is()` shall tell whether the name of a
 *   property, in the strings block, is a given name.
 *   <br>Traces to:
 *   - \srqref{SRQ_HV_BOOT_004}
 *
 * @llr
 * - \llrid{LLR_FDT_PROP_IS_01}: If `nameoff` is less than `strings_size` of
 *   `fdt` and `name_is()` returns true for `strings` plus `nameoff`, the limit
 *   `strings` plus `strings_size`, `name` and true, `prop_is()` shall return
 *   true.
 *   <br>Traces to:
 *   - \hlrref{HLR_FDT_PROP_IS_01}
 * - \llrid{LLR_FDT_PROP_IS_02}: If `nameoff` is not less than `strings_size` of
 *   `fdt`, or `name_is()` returns false for `strings` plus `nameoff`, the limit
 *   `strings` plus `strings_size`, `name` and true, `prop_is()` shall return
 *   false.
 *   <br>Traces to:
 *   - \hlrref{HLR_FDT_PROP_IS_01}
 *
 * @design
 * 1. Return
 *    `(nameoff < fdt->strings_size) && name_is(fdt, fdt->strings + nameoff,`
 *    `fdt->strings + fdt->strings_size, name, true)`.
 *    \llrref{LLR_FDT_PROP_IS_01}, \llrref{LLR_FDT_PROP_IS_02}
 *
 * @calls
 * - `name_is()`
 */
static bool prop_is(const fdt_t *fdt, uint32_t nameoff, const char *name)
{
    return (nameoff < fdt->strings_size)
           && name_is(fdt, fdt->strings + nameoff,
                      fdt->strings + fdt->strings_size, name, true);
}

/**
 * @funcname `value_has_string`
 *
 * @description
 * Tells whether a string list property value (such as compatible) contains a
 * string.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `fdt` | `const fdt_t *` | [in] The device tree. |
 * | `offset` | `uint32_t` | [in] The offset of the value. |
 * | `length` | `uint32_t` | [in] The length of the value. |
 * | `s` | `const char *` | [in] The string. |
 *
 * @return true if one of the strings is s.
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `pos` | `uint32_t` | The offset of the string in the value |
 * | `found` | `bool` | Whether s was found |
 *
 * @hlr
 * - \hlrid{HLR_FDT_VALUE_HAS_STRING_01}: `value_has_string()` shall tell
 *   whether a string list value of the device tree, such as compatible,
 *   contains a string.
 *   <br>Traces to:
 *   - \srqref{SRQ_HV_BOOT_003}
 *
 * @llr
 * - \llrid{LLR_FDT_VALUE_HAS_STRING_01}: If `name_is()` returns true for the
 *   start of one of the strings of the `length` bytes at `offset` (each ends
 *   with a 0 byte or at `length`), the limit `offset` plus `length`, `s` and
 *   true, `value_has_string()` shall return true, without comparing the strings
 *   after it.
 *   <br>Traces to:
 *   - \hlrref{HLR_FDT_VALUE_HAS_STRING_01}
 * - \llrid{LLR_FDT_VALUE_HAS_STRING_02}: If `length` is 0, or `name_is()`
 *   returns false for the start of each string of the `length` bytes at
 *   `offset`, `value_has_string()` shall return false.
 *   <br>Traces to:
 *   - \hlrref{HLR_FDT_VALUE_HAS_STRING_01}
 *
 * @design
 * 1. Set `pos` to `0U`. \llrref{LLR_FDT_VALUE_HAS_STRING_01},
 *    \llrref{LLR_FDT_VALUE_HAS_STRING_02}
 * 2. Set `found` to `false`. \llrref{LLR_FDT_VALUE_HAS_STRING_02}
 * 3. While `found` is false and `pos` is less than `length`:
 *    \llrref{LLR_FDT_VALUE_HAS_STRING_01}, \llrref{LLR_FDT_VALUE_HAS_STRING_02}
 *    1. Call `name_is()` with the arguments `fdt`, `offset + pos`,
 *       `offset + length`, `s` and `true`, and store the result in `found`.
 *    2. While `pos` is less than `length` and `fdt->blob[offset + pos]` is not
 *       equal to `0U`:
 *       1. Increment `pos`.
 *    3. Increment `pos`.
 * 4. Return `found`. \llrref{LLR_FDT_VALUE_HAS_STRING_01},
 *    \llrref{LLR_FDT_VALUE_HAS_STRING_02}
 *
 * @calls
 * - `name_is()`
 */
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

/**
 * @funcname `cells_value`
 *
 * @description
 * Reads a value of one or two 32-bit cells (an address or size) of the device
 * tree.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `fdt` | `const fdt_t *` | [in] The device tree. |
 * | `offset` | `uint32_t` | [in] The offset of the value. |
 * | `cells` | `uint32_t` | [in] The number of cells, 1 or 2 (checked by the caller). |
 *
 * @return The value.
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `value` | `uint64_t` | The value |
 *
 * @hlr
 * - \hlrid{HLR_FDT_CELLS_VALUE_01}: `cells_value()` shall read an address or a
 *   size of one or two 32-bit cells of the device tree.
 *   <br>Traces to:
 *   - \srqref{SRQ_HV_BOOT_004}
 *
 * @llr
 * - \llrid{LLR_FDT_CELLS_VALUE_01}: If `cells` is 2, `cells_value()` shall
 *   return the 64-bit value whose high 32 bits are the result of `be32()` for
 *   `fdt->blob` and `offset`, and whose low 32 bits are the result of `be32()`
 *   for `fdt->blob` and `offset` plus 4.
 *   <br>Traces to:
 *   - \hlrref{HLR_FDT_CELLS_VALUE_01}
 * - \llrid{LLR_FDT_CELLS_VALUE_02}: If `cells` is not 2, `cells_value()` shall
 *   return the result of `be32()` for `fdt->blob` and `offset`.
 *   <br>Traces to:
 *   - \hlrref{HLR_FDT_CELLS_VALUE_01}
 *
 * @design
 * 1. Call `be32()` with the arguments `fdt->blob` and `offset`, and store the
 *    result in `value`. \llrref{LLR_FDT_CELLS_VALUE_01},
 *    \llrref{LLR_FDT_CELLS_VALUE_02}
 * 2. If `cells` is equal to `2U`: \llrref{LLR_FDT_CELLS_VALUE_01}
 *    1. Set `value` to `(value << 32) | be32(fdt->blob, offset + 4U)`.
 * 3. Return `value`. \llrref{LLR_FDT_CELLS_VALUE_01},
 *    \llrref{LLR_FDT_CELLS_VALUE_02}
 *
 * @calls
 * - `be32()`
 */
static uint64_t cells_value(const fdt_t *fdt, uint32_t offset,
                            uint32_t cells)
{
    uint64_t value = be32(fdt->blob, offset);

    if (cells == 2U) {
        value = (value << 32) | be32(fdt->blob, offset + 4U);
    }
    return value;
}

/**
 * @funcname `node_kind`
 *
 * @description
 * Returns the kind of a node from its name and depth: the root, /memory@...,
 * /cpus, /cpus/cpu@..., the GIC (/intc@...) or /pcie@...; notes whether the
 * second PL011 (/pl011@9040000) exists.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `fdt` | `const fdt_t *` | [in] The device tree. |
 * | `offset` | `uint32_t` | [in] The offset of the node's name. |
 * | `limit` | `uint32_t` | [in] The end of the structure block. |
 * | `depth` | `uint32_t` | [in] The node's depth (1 for the root). |
 * | `parent` | `node_e` | [in] The kind of the parent node. |
 * | `info` | `hv_fdt_info_t *` | [out] The information collected (uart1). |
 *
 * @return The node's kind (NODE_OTHER for the nodes not looked at).
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `kind` | `node_e` | The node's kind |
 *
 * @hlr
 * - \hlrid{HLR_FDT_NODE_KIND_01}: `node_kind()` shall recognize the nodes of
 *   the device tree that the hypervisor needs (the root, the memory, the CPUs,
 *   the GIC and the PCI bus) and note whether the second PL011 exists.
 *   <br>Traces to:
 *   - \srqref{SRQ_HV_BOOT_004}
 *   - \srqref{SRQ_HV_CON_006}
 *
 * @llr
 * - \llrid{LLR_FDT_NODE_KIND_01}: If `depth` is 1, `node_kind()` shall return
 *   `NODE_ROOT`.
 *   <br>Traces to:
 *   - \hlrref{HLR_FDT_NODE_KIND_01}
 * - \llrid{LLR_FDT_NODE_KIND_02}: If `depth` is 2, `parent` is `NODE_ROOT` and
 *   `name_is()` returns true for the name and the prefix `"memory@"`,
 *   `node_kind()` shall return `NODE_MEMORY`.
 *   <br>Traces to:
 *   - \hlrref{HLR_FDT_NODE_KIND_01}
 * - \llrid{LLR_FDT_NODE_KIND_03}: If `depth` is 2, `parent` is `NODE_ROOT`, and
 *   `name_is()` returns false for the prefix `"memory@"` and true for the whole
 *   name `"cpus"`, `node_kind()` shall return `NODE_CPUS`.
 *   <br>Traces to:
 *   - \hlrref{HLR_FDT_NODE_KIND_01}
 * - \llrid{LLR_FDT_NODE_KIND_04}: If `depth` is 2, `parent` is `NODE_ROOT`, and
 *   `name_is()` returns false for the prefix `"memory@"` and the whole name
 *   `"cpus"`, and true for the prefix `"intc@"`, `node_kind()` shall return
 *   `NODE_GIC`.
 *   <br>Traces to:
 *   - \hlrref{HLR_FDT_NODE_KIND_01}
 * - \llrid{LLR_FDT_NODE_KIND_05}: If `depth` is 2, `parent` is `NODE_ROOT`, and
 *   `name_is()` returns false for the prefix `"memory@"`, the whole name
 *   `"cpus"` and the prefix `"intc@"`, and true for the prefix `"pcie@"`,
 *   `node_kind()` shall return `NODE_PCIE`.
 *   <br>Traces to:
 *   - \hlrref{HLR_FDT_NODE_KIND_01}
 * - \llrid{LLR_FDT_NODE_KIND_06}: If `depth` is 2, `parent` is `NODE_ROOT`, and
 *   `name_is()` returns false for the prefixes `"memory@"`, `"intc@"` and
 *   `"pcie@"` and the whole name `"cpus"`, and true for the whole name
 *   `"pl011@9040000"`, `node_kind()` shall set `uart1` of `info` to true and
 *   return `NODE_OTHER`.
 *   <br>Traces to:
 *   - \hlrref{HLR_FDT_NODE_KIND_01}
 * - \llrid{LLR_FDT_NODE_KIND_07}: If `depth` is 3, `parent` is `NODE_CPUS` and
 *   `name_is()` returns true for the name and the prefix `"cpu@"`,
 *   `node_kind()` shall return `NODE_CPU`.
 *   <br>Traces to:
 *   - \hlrref{HLR_FDT_NODE_KIND_01}
 * - \llrid{LLR_FDT_NODE_KIND_08}: If `depth` is 2 and `parent` is not
 *   `NODE_ROOT`, or `depth` is 2, `parent` is `NODE_ROOT` and `name_is()`
 *   returns false for `"memory@"`, `"cpus"`, `"intc@"`, `"pcie@"` and
 *   `"pl011@9040000"`, or `depth` is 3 and `parent` is not `NODE_CPUS`, or
 *   `depth` is 3, `parent` is `NODE_CPUS` and `name_is()` returns false for
 *   `"cpu@"`, or `depth` is none of 1, 2 and 3, `node_kind()` shall return
 *   `NODE_OTHER` and leave `info` as it is.
 *   <br>Traces to:
 *   - \hlrref{HLR_FDT_NODE_KIND_01}
 *
 * @design
 * 1. Set `kind` to `NODE_OTHER`. \llrref{LLR_FDT_NODE_KIND_06},
 *    \llrref{LLR_FDT_NODE_KIND_08}
 * 2. If `depth` is equal to `1U`: \llrref{LLR_FDT_NODE_KIND_01}
 *    1. Set `kind` to `NODE_ROOT`.
 * 3. Otherwise, if `depth` is equal to `2U` and `parent` is equal to
 *    `NODE_ROOT`: \llrref{LLR_FDT_NODE_KIND_02}, \llrref{LLR_FDT_NODE_KIND_03},
 *    \llrref{LLR_FDT_NODE_KIND_04}, \llrref{LLR_FDT_NODE_KIND_05},
 *    \llrref{LLR_FDT_NODE_KIND_06}, \llrref{LLR_FDT_NODE_KIND_08}
 *    1. If `name_is(fdt, offset, limit, "memory@", false)` is true:
 *       \llrref{LLR_FDT_NODE_KIND_02}
 *       1. Set `kind` to `NODE_MEMORY`.
 *    2. Otherwise, if `name_is(fdt, offset, limit, "cpus", true)` is true:
 *       \llrref{LLR_FDT_NODE_KIND_03}
 *       1. Set `kind` to `NODE_CPUS`.
 *    3. Otherwise, if `name_is(fdt, offset, limit, "intc@", false)` is true:
 *       \llrref{LLR_FDT_NODE_KIND_04}
 *       1. Set `kind` to `NODE_GIC`.
 *    4. Otherwise, if `name_is(fdt, offset, limit, "pcie@", false)` is true:
 *       \llrref{LLR_FDT_NODE_KIND_05}
 *       1. Set `kind` to `NODE_PCIE`.
 *    5. Otherwise, if `name_is(fdt, offset, limit, "pl011@9040000", true)` is
 *       true: \llrref{LLR_FDT_NODE_KIND_06}, \llrref{LLR_FDT_NODE_KIND_08}
 *       1. Set `info->uart1` to `true`.
 * 4. Otherwise, if `depth` is equal to `3U` and `parent` is equal to
 *    `NODE_CPUS` and `name_is(fdt, offset, limit, "cpu@", false)` is true:
 *    \llrref{LLR_FDT_NODE_KIND_07}, \llrref{LLR_FDT_NODE_KIND_08}
 *    1. Set `kind` to `NODE_CPU`.
 * 5. Return `kind`. \llrref{LLR_FDT_NODE_KIND_01},
 *    \llrref{LLR_FDT_NODE_KIND_02}, \llrref{LLR_FDT_NODE_KIND_03},
 *    \llrref{LLR_FDT_NODE_KIND_04}, \llrref{LLR_FDT_NODE_KIND_05},
 *    \llrref{LLR_FDT_NODE_KIND_06}, \llrref{LLR_FDT_NODE_KIND_07},
 *    \llrref{LLR_FDT_NODE_KIND_08}
 *
 * @calls
 * - `name_is()`
 */
static node_e node_kind(const fdt_t *fdt, uint32_t offset, uint32_t limit,
                        uint32_t depth, node_e parent, hv_fdt_info_t *info)
{
    node_e kind = NODE_OTHER;

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

/**
 * @funcname `use_property`
 *
 * @description
 * Uses a property of a node: the root's cell sizes, the CPUs' MPIDRs, the RAM,
 * the PCI configuration space and the GIC's version and register ranges.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `fdt` | `const fdt_t *` | [in] The device tree. |
 * | `node` | `node_e` | [in] The kind of the node. |
 * | `nameoff` | `uint32_t` | [in] The offset of the property's name in the strings block. |
 * | `value` | `uint32_t` | [in] The offset of the property's value. |
 * | `length` | `uint32_t` | [in] The length of the value. |
 * | `cells` | `cells_t *` | [in,out] The cell sizes, updated. |
 * | `info` | `hv_fdt_info_t *` | [in,out] The information collected. |
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `cells_ok` | `bool` | Whether the root's cell sizes are 1 or 2 |
 * | `entry` | `uint32_t` | The bytes of one (address, size) entry of a reg property, or 0 |
 * | `bases` | `uint64_t *[4]` | Where the GIC's register ranges go |
 * | `i` | `uint32_t` | The index of a GIC register range |
 *
 * @hlr
 * - \hlrid{HLR_FDT_USE_PROPERTY_01}: `use_property()` shall take from the
 *   properties of the device tree the cell sizes, the affinities of the CPUs,
 *   the RAM, the PCI configuration space, and the version and register ranges
 *   of the GIC.
 *   <br>Traces to:
 *   - \srqref{SRQ_HV_BOOT_004}
 *   - \srqref{SRQ_HV_CPU_002}
 *   - \srqref{SRQ_HV_BOOT_003}
 *
 * @llr
 * - \llrid{LLR_FDT_USE_PROPERTY_01}: If `node` is `NODE_ROOT`, `length` is 4
 *   and `prop_is()` returns true for `nameoff` and `"#address-cells"`,
 *   `use_property()` shall set `address_cells` of `cells` to the result of
 *   `be32()` for the value.
 *   <br>Traces to:
 *   - \hlrref{HLR_FDT_USE_PROPERTY_01}
 * - \llrid{LLR_FDT_USE_PROPERTY_02}: If `node` is `NODE_ROOT`, `length` is 4,
 *   and `prop_is()` returns false for `"#address-cells"` and true for
 *   `"#size-cells"`, `use_property()` shall set `size_cells` of `cells` to the
 *   result of `be32()` for the value.
 *   <br>Traces to:
 *   - \hlrref{HLR_FDT_USE_PROPERTY_01}
 * - \llrid{LLR_FDT_USE_PROPERTY_03}: If `node` is `NODE_CPUS`, `length` is 4
 *   and `prop_is()` returns true for `nameoff` and `"#address-cells"`,
 *   `use_property()` shall set `cpu_cells` of `cells` to the result of `be32()`
 *   for the value.
 *   <br>Traces to:
 *   - \hlrref{HLR_FDT_USE_PROPERTY_01}
 * - \llrid{LLR_FDT_USE_PROPERTY_04}: If `node` is `NODE_CPU`, `prop_is()`
 *   returns true for `"reg"`, `cpu_cells` is 1 or 2, `length` is 4 times
 *   `cpu_cells` and `nb_cpus` of `info` is less than `HV_MAX_CPUS`,
 *   `use_property()` shall set `mpidr[nb_cpus]` of `info` to the low 24 bits of
 *   the result of `be32()` for the last 4 bytes of the value, and increment
 *   `nb_cpus`.
 *   <br>Traces to:
 *   - \hlrref{HLR_FDT_USE_PROPERTY_01}
 * - \llrid{LLR_FDT_USE_PROPERTY_05}: If `node` is `NODE_CPU`, `prop_is()`
 *   returns true for `"reg"`, `cpu_cells` is 1 or 2, `length` is 4 times
 *   `cpu_cells` and `nb_cpus` of `info` is `HV_MAX_CPUS` or more,
 *   `use_property()` shall increment `nb_cpus` and leave `mpidr` as it is.
 *   <br>Traces to:
 *   - \hlrref{HLR_FDT_USE_PROPERTY_01}
 * - \llrid{LLR_FDT_USE_PROPERTY_06}: If `node` is `NODE_MEMORY`, `prop_is()`
 *   returns true for `"reg"`, `address_cells` and `size_cells` are each 1 or 2,
 *   `length` is not less than 4 times their sum and `ram_size` of `info` is 0,
 *   `use_property()` shall set `ram_base` to the result of `cells_value()` for
 *   the value and `address_cells`, and `ram_size` to the result of
 *   `cells_value()` for the value plus 4 times `address_cells` and
 *   `size_cells`.
 *   <br>Traces to:
 *   - \hlrref{HLR_FDT_USE_PROPERTY_01}
 * - \llrid{LLR_FDT_USE_PROPERTY_07}: If `node` is `NODE_PCIE`, `prop_is()`
 *   returns true for `"reg"`, `address_cells` and `size_cells` are each 1 or 2
 *   and `length` is not less than 4 times their sum, `use_property()` shall set
 *   `ecam_base` to the result of `cells_value()` for the value and
 *   `address_cells`, and `ecam_size` to the result of `cells_value()` for the
 *   value plus 4 times `address_cells` and `size_cells`.
 *   <br>Traces to:
 *   - \hlrref{HLR_FDT_USE_PROPERTY_01}
 * - \llrid{LLR_FDT_USE_PROPERTY_08}: If `node` is `NODE_GIC` and `prop_is()`
 *   returns true for `"compatible"`, `use_property()` shall set `gic_v2` of
 *   `info` to the result of `value_has_string()` for the value, `length` and
 *   `"arm,cortex-a15-gic"`.
 *   <br>Traces to:
 *   - \hlrref{HLR_FDT_USE_PROPERTY_01}
 * - \llrid{LLR_FDT_USE_PROPERTY_09}: If `node` is `NODE_GIC`, `prop_is()`
 *   returns false for `"compatible"` and true for `"reg"`, and `address_cells`
 *   and `size_cells` are each 1 or 2, `use_property()` shall set `gic_regs` of
 *   `info` to `length` divided by 4 times their sum, and `gicd`, `gicc`, `gich`
 *   and `gicv`, in this order, as many as `gic_regs` and at most 4, to the
 *   results of `cells_value()` for the addresses of the entries of the value.
 *   <br>Traces to:
 *   - \hlrref{HLR_FDT_USE_PROPERTY_01}
 * - \llrid{LLR_FDT_USE_PROPERTY_10}: If `node` is `NODE_GIC`, `prop_is()`
 *   returns false for `"compatible"` and true for `"reg"`, `address_cells` and
 *   `size_cells` are each 1 or 2 and `length` divided by 4 times their sum is 4
 *   or more, `use_property()` shall set `gicv_size` of `info` to the result of
 *   `cells_value()` for the size of the fourth entry of the value.
 *   <br>Traces to:
 *   - \hlrref{HLR_FDT_USE_PROPERTY_01}
 * - \llrid{LLR_FDT_USE_PROPERTY_11}: If `node` is `NODE_GIC`, `prop_is()`
 *   returns false for `"compatible"` and true for `"reg"`, `address_cells` and
 *   `size_cells` are each 1 or 2 and `length` divided by 4 times their sum is
 *   less than 4, `use_property()` shall leave `gicv_size` of `info` as it is.
 *   <br>Traces to:
 *   - \hlrref{HLR_FDT_USE_PROPERTY_01}
 * - \llrid{LLR_FDT_USE_PROPERTY_12}: If `node` is `NODE_ROOT` and `length` is
 *   not 4 or `prop_is()` returns false for `"#address-cells"` and
 *   `"#size-cells"`, or `node` is `NODE_CPUS` and `length` is not 4 or
 *   `prop_is()` returns false for `"#address-cells"`, `use_property()` shall
 *   leave `cells` and `info` as they are.
 *   <br>Traces to:
 *   - \hlrref{HLR_FDT_USE_PROPERTY_01}
 * - \llrid{LLR_FDT_USE_PROPERTY_13}: If `node` is `NODE_CPU`, and `prop_is()`
 *   returns false for `"reg"`, or `cpu_cells` is neither 1 nor 2, or `length`
 *   is not 4 times `cpu_cells`, `use_property()` shall leave `cells` and `info`
 *   as they are.
 *   <br>Traces to:
 *   - \hlrref{HLR_FDT_USE_PROPERTY_01}
 * - \llrid{LLR_FDT_USE_PROPERTY_14}: If `node` is `NODE_MEMORY` or `NODE_PCIE`,
 *   and `prop_is()` returns false for `"reg"`, or `address_cells` or
 *   `size_cells` is neither 1 nor 2, or `length` is less than 4 times their
 *   sum, or `node` is `NODE_MEMORY` and `ram_size` of `info` is not 0,
 *   `use_property()` shall leave `cells` and `info` as they are.
 *   <br>Traces to:
 *   - \hlrref{HLR_FDT_USE_PROPERTY_01}
 * - \llrid{LLR_FDT_USE_PROPERTY_15}: If `node` is `NODE_GIC`, and `prop_is()`
 *   returns false for `"compatible"` and `"reg"`, or returns true for `"reg"`
 *   while `address_cells` or `size_cells` is neither 1 nor 2, `use_property()`
 *   shall leave `cells` and `info` as they are.
 *   <br>Traces to:
 *   - \hlrref{HLR_FDT_USE_PROPERTY_01}
 * - \llrid{LLR_FDT_USE_PROPERTY_16}: If `node` is none of `NODE_ROOT`,
 *   `NODE_CPUS`, `NODE_CPU`, `NODE_MEMORY`, `NODE_PCIE` and `NODE_GIC`,
 *   `use_property()` shall leave `cells` and `info` as they are.
 *   <br>Traces to:
 *   - \hlrref{HLR_FDT_USE_PROPERTY_01}
 *
 * @design
 * 1. Set `cells_ok` to
 *    `(cells->address_cells >= 1U) && (cells->address_cells <= 2U) &&`
 *    `(cells->size_cells >= 1U) && (cells->size_cells <= 2U)`.
 *    \llrref{LLR_FDT_USE_PROPERTY_06}, \llrref{LLR_FDT_USE_PROPERTY_07},
 *    \llrref{LLR_FDT_USE_PROPERTY_09}, \llrref{LLR_FDT_USE_PROPERTY_10},
 *    \llrref{LLR_FDT_USE_PROPERTY_11}, \llrref{LLR_FDT_USE_PROPERTY_14},
 *    \llrref{LLR_FDT_USE_PROPERTY_15}
 * 2. Set `entry` to `((cells->address_cells + cells->size_cells) * 4U)` if
 *    `cells_ok` is true, otherwise `0U`. \llrref{LLR_FDT_USE_PROPERTY_06},
 *    \llrref{LLR_FDT_USE_PROPERTY_07}, \llrref{LLR_FDT_USE_PROPERTY_09},
 *    \llrref{LLR_FDT_USE_PROPERTY_10}, \llrref{LLR_FDT_USE_PROPERTY_11},
 *    \llrref{LLR_FDT_USE_PROPERTY_14}, \llrref{LLR_FDT_USE_PROPERTY_15}
 * 3. If `node` is equal to `NODE_ROOT` and `length` is equal to `4U`:
 *    \llrref{LLR_FDT_USE_PROPERTY_01}, \llrref{LLR_FDT_USE_PROPERTY_02},
 *    \llrref{LLR_FDT_USE_PROPERTY_12}
 *    1. If `prop_is(fdt, nameoff, "#address-cells")` is true:
 *       \llrref{LLR_FDT_USE_PROPERTY_01}
 *       1. Call `be32()` with the arguments `fdt->blob` and `value`, and store
 *          the result in `cells->address_cells`.
 *    2. Otherwise, if `prop_is(fdt, nameoff, "#size-cells")` is true:
 *       \llrref{LLR_FDT_USE_PROPERTY_02}, \llrref{LLR_FDT_USE_PROPERTY_12}
 *       1. Call `be32()` with the arguments `fdt->blob` and `value`, and store
 *          the result in `cells->size_cells`.
 * 4. Otherwise, if `node` is equal to `NODE_CPUS` and `length` is equal to `4U`
 *    and `prop_is(fdt, nameoff, "#address-cells")` is true:
 *    \llrref{LLR_FDT_USE_PROPERTY_03}, \llrref{LLR_FDT_USE_PROPERTY_12}
 *    1. Call `be32()` with the arguments `fdt->blob` and `value`, and store the
 *       result in `cells->cpu_cells`.
 * 5. Otherwise, if `node` is equal to `NODE_CPU` and
 *    `prop_is(fdt, nameoff, "reg")` is true and `cells->cpu_cells` is greater
 *    than or equal to `1U` and `cells->cpu_cells` is less than or equal to `2U`
 *    and `length` is equal to `(cells->cpu_cells * 4U)`:
 *    \llrref{LLR_FDT_USE_PROPERTY_04}, \llrref{LLR_FDT_USE_PROPERTY_05},
 *    \llrref{LLR_FDT_USE_PROPERTY_13}
 *    1. If `info->nb_cpus` is less than `HV_MAX_CPUS`:
 *       \llrref{LLR_FDT_USE_PROPERTY_04}, \llrref{LLR_FDT_USE_PROPERTY_05}
 *       1. Set `info->mpidr[info->nb_cpus]` to
 *          `be32(fdt->blob, (value + length) - 4U) & 0xFFFFFFU`.
 *    2. Increment `info->nb_cpus`. \llrref{LLR_FDT_USE_PROPERTY_04},
 *       \llrref{LLR_FDT_USE_PROPERTY_05}
 * 6. Otherwise, if `node` is equal to `NODE_MEMORY` and
 *    `prop_is(fdt, nameoff, "reg")` is true and `entry` is not equal to `0U`
 *    and `length` is greater than or equal to `entry` and `info->ram_size` is
 *    equal to `0U`: \llrref{LLR_FDT_USE_PROPERTY_06},
 *    \llrref{LLR_FDT_USE_PROPERTY_14}
 *    1. Call `cells_value()` with the arguments `fdt`, `value` and
 *       `cells->address_cells`, and store the result in `info->ram_base`.
 *    2. Call `cells_value()` with the arguments `fdt`,
 *       `value + (cells->address_cells * 4U)` and `cells->size_cells`, and
 *       store the result in `info->ram_size`.
 * 7. Otherwise, if `node` is equal to `NODE_PCIE` and
 *    `prop_is(fdt, nameoff, "reg")` is true and `entry` is not equal to `0U`
 *    and `length` is greater than or equal to `entry`:
 *    \llrref{LLR_FDT_USE_PROPERTY_07}, \llrref{LLR_FDT_USE_PROPERTY_14}
 *    1. Call `cells_value()` with the arguments `fdt`, `value` and
 *       `cells->address_cells`, and store the result in `info->ecam_base`.
 *    2. Call `cells_value()` with the arguments `fdt`,
 *       `value + (cells->address_cells * 4U)` and `cells->size_cells`, and
 *       store the result in `info->ecam_size`.
 * 8. Otherwise, if `node` is equal to `NODE_GIC` and
 *    `prop_is(fdt, nameoff, "compatible")` is true:
 *    \llrref{LLR_FDT_USE_PROPERTY_08}, \llrref{LLR_FDT_USE_PROPERTY_15}
 *    1. Call `value_has_string()` with the arguments `fdt`, `value`, `length`
 *       and `"arm,cortex-a15-gic"`, and store the result in `info->gic_v2`.
 * 9. Otherwise, if `node` is equal to `NODE_GIC` and
 *    `prop_is(fdt, nameoff, "reg")` is true and `entry` is not equal to `0U`:
 *    \llrref{LLR_FDT_USE_PROPERTY_09}, \llrref{LLR_FDT_USE_PROPERTY_10},
 *    \llrref{LLR_FDT_USE_PROPERTY_11}, \llrref{LLR_FDT_USE_PROPERTY_15},
 *    \llrref{LLR_FDT_USE_PROPERTY_16}
 *    1. Set `bases` to
 *       `{ &info->gicd, &info->gicc, &info->gich, &info->gicv }`.
 *       \llrref{LLR_FDT_USE_PROPERTY_09}
 *    2. Set `info->gic_regs` to `length / entry`.
 *       \llrref{LLR_FDT_USE_PROPERTY_09}
 *    3. Set `i` to `0U`; then while `i` is less than `info->gic_regs` and `i`
 *       is less than `4U`, repeat (after each pass: increment `i`):
 *       \llrref{LLR_FDT_USE_PROPERTY_09}
 *       1. Call `cells_value()` with the arguments `fdt`, `value + (i * entry)`
 *          and `cells->address_cells`, and store the result in `*bases[i]`.
 *    4. If `info->gic_regs` is greater than or equal to `4U`:
 *       \llrref{LLR_FDT_USE_PROPERTY_10}, \llrref{LLR_FDT_USE_PROPERTY_11}
 *       1. Call `cells_value()` with the arguments `fdt`,
 *          `value + (3U * entry) + (cells->address_cells * 4U)` and
 *          `cells->size_cells`, and store the result in `info->gicv_size`.
 *
 * @calls
 * - `prop_is()`
 * - `be32()`
 * - `cells_value()`
 * - `value_has_string()`
 */
static void use_property(const fdt_t *fdt, node_e node, uint32_t nameoff,
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

/**
 * @funcname `fdt_read`
 *
 * @description
 * Reads the host's device tree in one pass over its structure block, collecting
 * the RAM, CPUs, GIC, PCI configuration space and second UART.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `address` | `uint64_t` | [in] The physical address of the device tree. |
 * | `info` | `hv_fdt_info_t *` | [out] Where the information is stored. |
 *
 * @return false if there is no valid device tree there.
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `fdt` | `fdt_t` | The blob and its strings block |
 * | `cells` | `cells_t` | The cell sizes (defaults 2, 1, 1) |
 * | `stack` | `node_e[DEPTH_MAX]` | The kinds of the nodes being read |
 * | `depth` | `uint32_t` | The depth of the node |
 * | `pos` | `uint32_t` | The offset of the next token |
 * | `end` | `uint32_t` | The end of the structure block |
 * | `ok` | `bool` | Whether the tree is valid so far |
 * | `done` | `bool` | Whether FDT_END was read |
 * | `token` | `uint32_t` | The token |
 * | `name` | `uint32_t` | The offset of a node's name |
 * | `parent` | `node_e` | The kind of the parent node |
 * | `length` | `uint32_t` | The length of a property's value |
 * | `nameoff` | `uint32_t` | The offset of a property's name |
 *
 * @hlr
 * - \hlrid{HLR_FDT_FDT_READ_01}: `fdt_read()` shall read the device tree of the
 *   host in one pass and collect the RAM, the CPUs, the GIC, the PCI
 *   configuration space and the second UART, and tell whether the tree is
 *   valid, reading nothing outside it.
 *   <br>Traces to:
 *   - \srqref{SRQ_HV_BOOT_004}
 *
 * @llr
 * - \llrid{LLR_FDT_FDT_READ_01}: `fdt_read()` shall set every byte of `info` to
 *   0 with `memset()` before it reads the tree.
 *   <br>Traces to:
 *   - \hlrref{HLR_FDT_FDT_READ_01}
 * - \llrid{LLR_FDT_FDT_READ_02}: If the 32-bit big-endian word at `address`
 *   (`be32()`) is not `FDT_MAGIC`, `fdt_read()` shall return false.
 *   <br>Traces to:
 *   - \hlrref{HLR_FDT_FDT_READ_01}
 * - \llrid{LLR_FDT_FDT_READ_03}: If the total size (the word at
 *   `FDT_OFF_TOTALSIZE`) is more than `FDT_SIZE_MAX` or less than
 *   `FDT_HEADER_SIZE`, or the version (at `FDT_OFF_VERSION`) is less than
 *   `FDT_VERSION_MIN`, or the offset of the structure block (at
 *   `FDT_OFF_STRUCT`) or its end (the offset plus the word at
 *   `FDT_OFF_SIZE_STRUCT`) is more than the total size, or the end is less than
 *   the offset, or the offset of the strings block (at `FDT_OFF_STRINGS`) is
 *   more than the total size, or its size (at `FDT_OFF_SIZE_STRINGS`) is more
 *   than the total size minus its offset, `fdt_read()` shall return false.
 *   <br>Traces to:
 *   - \hlrref{HLR_FDT_FDT_READ_01}
 * - \llrid{LLR_FDT_FDT_READ_04}: `fdt_read()` shall read the tokens of the
 *   structure block in order, each a 32-bit big-endian word, until the token
 *   `FDT_END` or a failure, starting at depth 0 with 2 address cells, 1 size
 *   cell and 1 CPU cell in `cells`.
 *   <br>Traces to:
 *   - \hlrref{HLR_FDT_FDT_READ_01}
 * - \llrid{LLR_FDT_FDT_READ_05}: If fewer than 4 bytes are left in the
 *   structure block for a token, `fdt_read()` shall return false.
 *   <br>Traces to:
 *   - \hlrref{HLR_FDT_FDT_READ_01}
 * - \llrid{LLR_FDT_FDT_READ_06}: If the token is `FDT_BEGIN_NODE`, the name
 *   after it has a 0 byte before the end of the block and the depth is less
 *   than `DEPTH_MAX`, `fdt_read()` shall increment the depth, store as the kind
 *   of the node the result of `node_kind()` for the name, the end of the block,
 *   the depth, the kind of the node around it (`NODE_OTHER` at depth 0) and
 *   `info`, and go on after the name and its 0 byte, rounded up to a multiple
 *   of 4 with `align4()`.
 *   <br>Traces to:
 *   - \hlrref{HLR_FDT_FDT_READ_01}
 * - \llrid{LLR_FDT_FDT_READ_07}: If the token is `FDT_BEGIN_NODE` and the name
 *   after it has no 0 byte before the end of the block, or the depth is
 *   `DEPTH_MAX`, `fdt_read()` shall return false.
 *   <br>Traces to:
 *   - \hlrref{HLR_FDT_FDT_READ_01}
 * - \llrid{LLR_FDT_FDT_READ_08}: If the token is `FDT_END_NODE` and the depth
 *   is not 0, `fdt_read()` shall decrement the depth.
 *   <br>Traces to:
 *   - \hlrref{HLR_FDT_FDT_READ_01}
 * - \llrid{LLR_FDT_FDT_READ_09}: If the token is `FDT_END_NODE` and the depth
 *   is 0, `fdt_read()` shall return false.
 *   <br>Traces to:
 *   - \hlrref{HLR_FDT_FDT_READ_01}
 * - \llrid{LLR_FDT_FDT_READ_10}: If the token is `FDT_PROP`, at least 8 bytes
 *   follow it in the block, the length (the first word after it) is not more
 *   than the bytes left after the two words, and the depth is not 0,
 *   `fdt_read()` shall call `use_property()` once with the kind of the node
 *   around it, the offset of the name (the second word), the offset of the
 *   value, the length, `cells` and `info`, and go on after the value, rounded
 *   up to a multiple of 4 with `align4()`.
 *   <br>Traces to:
 *   - \hlrref{HLR_FDT_FDT_READ_01}
 * - \llrid{LLR_FDT_FDT_READ_11}: If the token is `FDT_PROP` and fewer than 8
 *   bytes follow it in the block, or the length is more than the bytes left
 *   after the two words, or the depth is 0, `fdt_read()` shall return false.
 *   <br>Traces to:
 *   - \hlrref{HLR_FDT_FDT_READ_01}
 * - \llrid{LLR_FDT_FDT_READ_12}: If the token is `FDT_NOP`, `fdt_read()` shall
 *   go on with the next token.
 *   <br>Traces to:
 *   - \hlrref{HLR_FDT_FDT_READ_01}
 * - \llrid{LLR_FDT_FDT_READ_13}: If the token is `FDT_END`, `fdt_read()` shall
 *   return true.
 *   <br>Traces to:
 *   - \hlrref{HLR_FDT_FDT_READ_01}
 * - \llrid{LLR_FDT_FDT_READ_14}: If the token is none of `FDT_BEGIN_NODE`,
 *   `FDT_END_NODE`, `FDT_PROP`, `FDT_NOP` and `FDT_END`, `fdt_read()` shall
 *   return false.
 *   <br>Traces to:
 *   - \hlrref{HLR_FDT_FDT_READ_01}
 *
 * @design
 * 1. Set `cells` to `{ 2U, 1U, 1U }`. \llrref{LLR_FDT_FDT_READ_04}
 * 2. Set `depth` to `0U`. \llrref{LLR_FDT_FDT_READ_04}
 * 3. Set `ok` to `true`. \llrref{LLR_FDT_FDT_READ_04}
 * 4. Set `done` to `false`. \llrref{LLR_FDT_FDT_READ_04}
 * 5. Call `memset()` with the arguments `info`, `0` and `sizeof(*info)`.
 *    \llrref{LLR_FDT_FDT_READ_01}
 * 6. Set `fdt.blob` to `(const uint8_t *) (uintptr_t) address`.
 *    \llrref{LLR_FDT_FDT_READ_02}, \llrref{LLR_FDT_FDT_READ_03},
 *    \llrref{LLR_FDT_FDT_READ_04}
 * 7. If `be32(fdt.blob, 0U)` is not equal to `FDT_MAGIC`:
 *    \llrref{LLR_FDT_FDT_READ_02}
 *    1. Return `false`.
 * 8. Call `be32()` with the arguments `fdt.blob` and `FDT_OFF_TOTALSIZE`, and
 *    store the result in `fdt.size`. \llrref{LLR_FDT_FDT_READ_03}
 * 9. Call `be32()` with the arguments `fdt.blob` and `FDT_OFF_STRUCT`, and
 *    store the result in `pos`. \llrref{LLR_FDT_FDT_READ_03}
 * 10. Set `end` to `pos + be32(fdt.blob, FDT_OFF_SIZE_STRUCT)`.
 *     \llrref{LLR_FDT_FDT_READ_03}
 * 11. Call `be32()` with the arguments `fdt.blob` and `FDT_OFF_STRINGS`, and
 *     store the result in `fdt.strings`. \llrref{LLR_FDT_FDT_READ_03}
 * 12. Call `be32()` with the arguments `fdt.blob` and `FDT_OFF_SIZE_STRINGS`,
 *     and store the result in `fdt.strings_size`. \llrref{LLR_FDT_FDT_READ_03}
 * 13. If `fdt.size` is greater than `FDT_SIZE_MAX` or `fdt.size` is less than
 *     `FDT_HEADER_SIZE` or `be32(fdt.blob, FDT_OFF_VERSION)` is less than
 *     `FDT_VERSION_MIN` or `pos` is greater than `fdt.size` or `end` is greater
 *     than `fdt.size` or `end` is less than `pos` or `fdt.strings` is greater
 *     than `fdt.size` or `fdt.strings_size` is greater than
 *     `(fdt.size - fdt.strings)`: \llrref{LLR_FDT_FDT_READ_03}
 *     1. Return `false`.
 * 14. While `ok` is true and `done` is false: \llrref{LLR_FDT_FDT_READ_04}
 *     1. Set `token` to `FDT_END + 1U`. \llrref{LLR_FDT_FDT_READ_05}
 *     2. If `(pos + 4U)` is less than or equal to `end`:
 *        \llrref{LLR_FDT_FDT_READ_04}, \llrref{LLR_FDT_FDT_READ_05}
 *        1. Call `be32()` with the arguments `fdt.blob` and `pos`, and store
 *           the result in `token`.
 *        2. Add `4U` to `pos`.
 *     3. If `token` is equal to `FDT_BEGIN_NODE`: \llrref{LLR_FDT_FDT_READ_06},
 *        \llrref{LLR_FDT_FDT_READ_07}
 *        1. Set `name` to `pos`.
 *        2. Set `parent` to `stack[depth - 1U]` if `depth` is greater than
 *           `0U`, otherwise `NODE_OTHER`.
 *        3. While `pos` is less than `end` and `fdt.blob[pos]` is not equal to
 *           `0U`:
 *           1. Increment `pos`.
 *        4. If `pos` is greater than or equal to `end` or `depth` is greater
 *           than or equal to `DEPTH_MAX`: \llrref{LLR_FDT_FDT_READ_07}
 *           1. Set `ok` to `false`.
 *        5. Otherwise: \llrref{LLR_FDT_FDT_READ_06}
 *           1. Increment `depth`.
 *           2. Call `node_kind()` with the arguments the address of `fdt`,
 *              `name`, `end`, `depth`, `parent` and `info`, and store the
 *              result in `stack[depth - 1U]`.
 *           3. Set `pos` to `name + align4((pos - name) + 1U)`.
 *     4. Otherwise, if `token` is equal to `FDT_END_NODE`:
 *        \llrref{LLR_FDT_FDT_READ_08}, \llrref{LLR_FDT_FDT_READ_09}
 *        1. If `depth` is equal to `0U`: \llrref{LLR_FDT_FDT_READ_09}
 *           1. Set `ok` to `false`.
 *        2. Otherwise: \llrref{LLR_FDT_FDT_READ_08}
 *           1. Decrement `depth`.
 *     5. Otherwise, if `token` is equal to `FDT_PROP`:
 *        \llrref{LLR_FDT_FDT_READ_10}, \llrref{LLR_FDT_FDT_READ_11}
 *        1. If `(pos + 8U)` is greater than `end`: \llrref{LLR_FDT_FDT_READ_11}
 *           1. Set `ok` to `false`.
 *        2. Otherwise: \llrref{LLR_FDT_FDT_READ_10},
 *           \llrref{LLR_FDT_FDT_READ_11}
 *           1. Call `be32()` with the arguments `fdt.blob` and `pos`, and store
 *              the result in `length`.
 *           2. Call `be32()` with the arguments `fdt.blob` and `pos + 4U`, and
 *              store the result in `nameoff`.
 *           3. Add `8U` to `pos`.
 *           4. If `length` is greater than `(end - pos)` or `depth` is equal to
 *              `0U`: \llrref{LLR_FDT_FDT_READ_11}
 *              1. Set `ok` to `false`.
 *           5. Otherwise: \llrref{LLR_FDT_FDT_READ_10}
 *              1. Call `use_property()` with the arguments the address of
 *                 `fdt`, `stack[depth - 1U]`, `nameoff`, `pos`, `length`, the
 *                 address of `cells` and `info`.
 *              2. Add `align4(length)` to `pos`.
 *     6. Otherwise, if `token` is equal to `FDT_NOP`:
 *        \llrref{LLR_FDT_FDT_READ_12}
 *     7. Otherwise, if `token` is equal to `FDT_END`:
 *        \llrref{LLR_FDT_FDT_READ_13}
 *        1. Set `done` to `true`.
 *     8. Otherwise: \llrref{LLR_FDT_FDT_READ_05}, \llrref{LLR_FDT_FDT_READ_14}
 *        1. Set `ok` to `false`.
 * 15. Return `ok && done`. \llrref{LLR_FDT_FDT_READ_05},
 *     \llrref{LLR_FDT_FDT_READ_07}, \llrref{LLR_FDT_FDT_READ_09},
 *     \llrref{LLR_FDT_FDT_READ_11}, \llrref{LLR_FDT_FDT_READ_13},
 *     \llrref{LLR_FDT_FDT_READ_14}
 *
 * @calls
 * - `memset()`
 * - `be32()`
 * - `node_kind()`
 * - `align4()`
 * - `use_property()`
 */
/*
 * MISRA C:2025 Rule 11.4 deviation (fdt_read()): QEMU puts the device
 * tree at a physical address (identity-mapped), converted to a pointer.
 */
bool fdt_read(uint64_t address, hv_fdt_info_t *info)
{
    fdt_t fdt;
    cells_t cells = { 2U, 1U, 1U };
    node_e stack[DEPTH_MAX];
    uint32_t depth = 0U;
    uint32_t pos;
    uint32_t end;
    bool ok = true;
    bool done = false;

    (void) memset(info, 0, sizeof(*info));
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
            node_e parent = (depth > 0U) ? stack[depth - 1U] : NODE_OTHER;

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
