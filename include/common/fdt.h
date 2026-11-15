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
 * The format of a flattened device tree (devicetree blob): the offsets
 * of the fields of its header and the tokens of its structure block,
 * for the kernel (kernel/bsps/arm64-qemu) and the hypervisor
 * (hypervisor/bsps/arm64-qemu), which both read the tree that QEMU
 * gives them.
 *
 * The definitions were in the board header of the kernel
 * (kernel/bsps/arm64-qemu/include/board/bsp.h) and in the internal
 * header of the hypervisor
 * (hypervisor/csps/arm64/include/arch/internal.h), each with its own
 * copy.
 */

#ifndef JET_COMMON_FDT_H
#define JET_COMMON_FDT_H

/**
 * @macroname `FDT_MAGIC`
 *
 * @description
 * The magic number at the start of a device tree blob.
 */
#define FDT_MAGIC           0xD00DFEEDU

/**
 * @macroname `FDT_HEADER_SIZE`
 *
 * @description
 * The size of the header of the blob, in bytes.
 */
#define FDT_HEADER_SIZE     40U

/**
 * @macroname `FDT_SIZE_MAX`
 *
 * @description
 * The largest device tree blob that is accepted (the tree of QEMU is far
 * smaller).
 */
#define FDT_SIZE_MAX        0x100000U

/**
 * @macroname `FDT_OFF_TOTALSIZE`
 *
 * @description
 * The offset of the total size in the header.
 */
#define FDT_OFF_TOTALSIZE   4U

/**
 * @macroname `FDT_OFF_STRUCT`
 *
 * @description
 * The offset of the offset of the structure block in the header.
 */
#define FDT_OFF_STRUCT      8U

/**
 * @macroname `FDT_OFF_STRINGS`
 *
 * @description
 * The offset of the offset of the strings block in the header.
 */
#define FDT_OFF_STRINGS     12U

/**
 * @macroname `FDT_OFF_VERSION`
 *
 * @description
 * The offset of the version in the header.
 */
#define FDT_OFF_VERSION     20U

/**
 * @macroname `FDT_OFF_SIZE_STRINGS`
 *
 * @description
 * The offset of the size of the strings block in the header.
 */
#define FDT_OFF_SIZE_STRINGS 32U

/**
 * @macroname `FDT_OFF_SIZE_STRUCT`
 *
 * @description
 * The offset of the size of the structure block in the header.
 */
#define FDT_OFF_SIZE_STRUCT 36U

/**
 * @macroname `FDT_VERSION_MIN`
 *
 * @description
 * The oldest version of the blob that is read.
 */
#define FDT_VERSION_MIN     16U

/**
 * @macroname `FDT_BEGIN_NODE`
 *
 * @description
 * The token that begins a node.
 */
#define FDT_BEGIN_NODE      1U

/**
 * @macroname `FDT_END_NODE`
 *
 * @description
 * The token that ends a node.
 */
#define FDT_END_NODE        2U

/**
 * @macroname `FDT_PROP`
 *
 * @description
 * The token of a property.
 */
#define FDT_PROP            3U

/**
 * @macroname `FDT_NOP`
 *
 * @description
 * The token that is skipped.
 */
#define FDT_NOP             4U

/**
 * @macroname `FDT_END`
 *
 * @description
 * The token that ends the structure block.
 */
#define FDT_END             9U

#endif /* JET_COMMON_FDT_H */
