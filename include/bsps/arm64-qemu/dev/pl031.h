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
 * Registers of the Arm PL031 real-time clock of QEMU 'virt', for the
 * kernel (kernel/bsps/arm64-qemu) and the hypervisor
 * (hypervisor/bsps/arm64-qemu).
 *
 * The definitions were in the board header of the kernel
 * (kernel/bsps/arm64-qemu/include/board/bsp.h) and in the internal
 * header of the hypervisor
 * (hypervisor/csps/arm64/include/arch/internal.h), each with its own
 * copy.
 */

#ifndef JET_DEV_PL031_H
#define JET_DEV_PL031_H

/**
 * @macroname `RTC_DR`
 *
 * @description
 * The data register of the PL031 RTC (offset from BOARD_RTC_BASE): the
 * current calendar time, in seconds since the Epoch.
 */
#define RTC_DR              0x000U

#endif /* JET_DEV_PL031_H */
