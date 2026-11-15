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
 * PSCI (Arm Power State Coordination Interface): the function
 * identifiers and return values, for the kernel (kernel/bsps/arm64-qemu),
 * which calls PSCI, and the hypervisor (hypervisor/bsps/arm64-qemu),
 * which calls it and provides it to its VMs.
 *
 * The definitions were in the board header of the kernel
 * (kernel/bsps/arm64-qemu/include/board/bsp.h) and in the internal
 * header of the hypervisor
 * (hypervisor/csps/arm64/include/arch/internal.h), each with its own
 * copy.
 */

#ifndef JET_DEV_PSCI_H
#define JET_DEV_PSCI_H

/**
 * @macroname `PSCI_VERSION`
 *
 * @description
 * The function ID of PSCI_VERSION.
 */
#define PSCI_VERSION            0x84000000U

/**
 * @macroname `PSCI_CPU_OFF`
 *
 * @description
 * The function ID of CPU_OFF.
 */
#define PSCI_CPU_OFF            0x84000002U

/**
 * @macroname `PSCI_CPU_ON_32`
 *
 * @description
 * The function ID of CPU_ON (32-bit arguments).
 */
#define PSCI_CPU_ON_32          0x84000003U

/**
 * @macroname `PSCI_CPU_ON_64`
 *
 * @description
 * The function ID of CPU_ON (64-bit arguments).
 */
#define PSCI_CPU_ON_64          0xC4000003U

/**
 * @macroname `PSCI_AFFINITY_INFO_32`
 *
 * @description
 * The function ID of AFFINITY_INFO (32-bit arguments).
 */
#define PSCI_AFFINITY_INFO_32   0x84000004U

/**
 * @macroname `PSCI_AFFINITY_INFO_64`
 *
 * @description
 * The function ID of AFFINITY_INFO (64-bit arguments).
 */
#define PSCI_AFFINITY_INFO_64   0xC4000004U

/**
 * @macroname `PSCI_SYSTEM_OFF`
 *
 * @description
 * The function ID of SYSTEM_OFF.
 */
#define PSCI_SYSTEM_OFF         0x84000008U

/**
 * @macroname `PSCI_SYSTEM_RESET`
 *
 * @description
 * The function ID of SYSTEM_RESET.
 */
#define PSCI_SYSTEM_RESET       0x84000009U

/**
 * @macroname `PSCI_FEATURES`
 *
 * @description
 * The function ID of PSCI_FEATURES.
 */
#define PSCI_FEATURES           0x8400000AU

/**
 * @macroname `PSCI_VERSION_1_0`
 *
 * @description
 * The version 1.0 of PSCI, as PSCI_VERSION returns it.
 */
#define PSCI_VERSION_1_0        0x00010000

/**
 * @macroname `PSCI_SUCCESS`
 *
 * @description
 * The call succeeded.
 */
#define PSCI_SUCCESS            0

/**
 * @macroname `PSCI_NOT_SUPPORTED`
 *
 * @description
 * The function is not supported.
 */
#define PSCI_NOT_SUPPORTED      (-1)

/**
 * @macroname `PSCI_INVALID_PARAMETERS`
 *
 * @description
 * A parameter is invalid.
 */
#define PSCI_INVALID_PARAMETERS (-2)

/**
 * @macroname `PSCI_ALREADY_ON`
 *
 * @description
 * The CPU is already on.
 */
#define PSCI_ALREADY_ON         (-4)

/**
 * @macroname `PSCI_AFFINITY_ON`
 *
 * @description
 * The CPU is on (the result of AFFINITY_INFO).
 */
#define PSCI_AFFINITY_ON        0

#endif /* JET_DEV_PSCI_H */
