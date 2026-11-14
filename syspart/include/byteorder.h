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
 * Modified by Kirill Brazhnikov, 2026-11-04: MISRA C:2025: the header
 * guard isn't a reserved identifier.
 *
 * Modified by Kirill Brazhnikov, 2026-11-14: the macros and types are
 * documented.
 */

#ifndef POK_BYTEORDER_H
#define POK_BYTEORDER_H


#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__

/**
 * @macroname `cpu_to_le64`
 *
 * @description
 * The 64-bit value `x` converted from the CPU's byte order to little endian.
 *
 * @params
 * | Name | Description |
 * |------|-------------|
 * | `x` | The value. |
 */
#define cpu_to_le64(x) ((uint64_t)(x))

/**
 * @macroname `le64_to_cpu`
 *
 * @description
 * The 64-bit little endian value `x` converted to the CPU's byte order.
 *
 * @params
 * | Name | Description |
 * |------|-------------|
 * | `x` | The value. |
 */
#define le64_to_cpu(x) ((uint64_t)(x))

/**
 * @macroname `cpu_to_le32`
 *
 * @description
 * The 32-bit value `x` converted from the CPU's byte order to little endian.
 *
 * @params
 * | Name | Description |
 * |------|-------------|
 * | `x` | The value. |
 */
#define cpu_to_le32(x) ((uint32_t)(x))

/**
 * @macroname `le32_to_cpu`
 *
 * @description
 * The 32-bit little endian value `x` converted to the CPU's byte order.
 *
 * @params
 * | Name | Description |
 * |------|-------------|
 * | `x` | The value. |
 */
#define le32_to_cpu(x) ((uint32_t)(x))

/**
 * @macroname `cpu_to_le16`
 *
 * @description
 * The 16-bit value `x` converted from the CPU's byte order to little endian.
 *
 * @params
 * | Name | Description |
 * |------|-------------|
 * | `x` | The value. |
 */
#define cpu_to_le16(x) ((uint16_t)(x))

/**
 * @macroname `le16_to_cpu`
 *
 * @description
 * The 16-bit little endian value `x` converted to the CPU's byte order.
 *
 * @params
 * | Name | Description |
 * |------|-------------|
 * | `x` | The value. |
 */
#define le16_to_cpu(x) ((uint16_t)(x))

/**
 * @macroname `cpu_to_be64`
 *
 * @description
 * The 64-bit value `x` converted from the CPU's byte order to big endian.
 *
 * @params
 * | Name | Description |
 * |------|-------------|
 * | `x` | The value. |
 */
#define cpu_to_be64(x) (__builtin_bswap64((x)))

/**
 * @macroname `be64_to_cpu`
 *
 * @description
 * The 64-bit big endian value `x` converted to the CPU's byte order.
 *
 * @params
 * | Name | Description |
 * |------|-------------|
 * | `x` | The value. |
 */
#define be64_to_cpu(x) (__builtin_bswap64((x)))

/**
 * @macroname `cpu_to_be32`
 *
 * @description
 * The 32-bit value `x` converted from the CPU's byte order to big endian.
 *
 * @params
 * | Name | Description |
 * |------|-------------|
 * | `x` | The value. |
 */
#define cpu_to_be32(x) (__builtin_bswap32((x)))

/**
 * @macroname `be32_to_cpu`
 *
 * @description
 * The 32-bit big endian value `x` converted to the CPU's byte order.
 *
 * @params
 * | Name | Description |
 * |------|-------------|
 * | `x` | The value. |
 */
#define be32_to_cpu(x) (__builtin_bswap32((x)))

/**
 * @macroname `cpu_to_be16`
 *
 * @description
 * The 16-bit value `x` converted from the CPU's byte order to big endian.
 *
 * @params
 * | Name | Description |
 * |------|-------------|
 * | `x` | The value. |
 */
#define cpu_to_be16(x) (__builtin_bswap16((x)))

/**
 * @macroname `be16_to_cpu`
 *
 * @description
 * The 16-bit big endian value `x` converted to the CPU's byte order.
 *
 * @params
 * | Name | Description |
 * |------|-------------|
 * | `x` | The value. |
 */
#define be16_to_cpu(x) (__builtin_bswap16((x)))


#elif __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__

/**
 * @macroname `cpu_to_le64`
 *
 * @description
 * The 64-bit value `x` converted from the CPU's byte order to little endian.
 *
 * @params
 * | Name | Description |
 * |------|-------------|
 * | `x` | The value. |
 */
#define cpu_to_le64(x) (__builtin_bswap64((x)))

/**
 * @macroname `le64_to_cpu`
 *
 * @description
 * The 64-bit little endian value `x` converted to the CPU's byte order.
 *
 * @params
 * | Name | Description |
 * |------|-------------|
 * | `x` | The value. |
 */
#define le64_to_cpu(x) (__builtin_bswap64((x))

/**
 * @macroname `cpu_to_le32`
 *
 * @description
 * The 32-bit value `x` converted from the CPU's byte order to little endian.
 *
 * @params
 * | Name | Description |
 * |------|-------------|
 * | `x` | The value. |
 */
#define cpu_to_le32(x) (__builtin_bswap32((x)))

/**
 * @macroname `le32_to_cpu`
 *
 * @description
 * The 32-bit little endian value `x` converted to the CPU's byte order.
 *
 * @params
 * | Name | Description |
 * |------|-------------|
 * | `x` | The value. |
 */
#define le32_to_cpu(x) (__builtin_bswap32((x)))

/**
 * @macroname `cpu_to_le16`
 *
 * @description
 * The 16-bit value `x` converted from the CPU's byte order to little endian.
 *
 * @params
 * | Name | Description |
 * |------|-------------|
 * | `x` | The value. |
 */
#define cpu_to_le16(x) (__builtin_bswap16((x)))

/**
 * @macroname `le16_to_cpu`
 *
 * @description
 * The 16-bit little endian value `x` converted to the CPU's byte order.
 *
 * @params
 * | Name | Description |
 * |------|-------------|
 * | `x` | The value. |
 */
#define le16_to_cpu(x) (__builtin_bswap16((x)))

/**
 * @macroname `cpu_to_be64`
 *
 * @description
 * The 64-bit value `x` converted from the CPU's byte order to big endian.
 *
 * @params
 * | Name | Description |
 * |------|-------------|
 * | `x` | The value. |
 */
#define cpu_to_be64(x) ((uint64_t)(x))

/**
 * @macroname `be64_to_cpu`
 *
 * @description
 * The 64-bit big endian value `x` converted to the CPU's byte order.
 *
 * @params
 * | Name | Description |
 * |------|-------------|
 * | `x` | The value. |
 */
#define be64_to_cpu(x) ((uint64_t)(x))

/**
 * @macroname `cpu_to_be32`
 *
 * @description
 * The 32-bit value `x` converted from the CPU's byte order to big endian.
 *
 * @params
 * | Name | Description |
 * |------|-------------|
 * | `x` | The value. |
 */
#define cpu_to_be32(x) ((uint32_t)(x))

/**
 * @macroname `be32_to_cpu`
 *
 * @description
 * The 32-bit big endian value `x` converted to the CPU's byte order.
 *
 * @params
 * | Name | Description |
 * |------|-------------|
 * | `x` | The value. |
 */
#define be32_to_cpu(x) ((uint32_t)(x))

/**
 * @macroname `cpu_to_be16`
 *
 * @description
 * The 16-bit value `x` converted from the CPU's byte order to big endian.
 *
 * @params
 * | Name | Description |
 * |------|-------------|
 * | `x` | The value. |
 */
#define cpu_to_be16(x) ((uint16_t)(x))

/**
 * @macroname `be16_to_cpu`
 *
 * @description
 * The 16-bit big endian value `x` converted to the CPU's byte order.
 *
 * @params
 * | Name | Description |
 * |------|-------------|
 * | `x` | The value. |
 */
#define be16_to_cpu(x) ((uint16_t)(x))


#else
    #error "unrecognized endianness"
#endif

#endif
