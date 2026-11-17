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
 * Modified by Kirill Brazhnikov, 2026-10-31: ported from
 * boards/x86-qemu/bsp.c to x86_64 (64-bit).
 *
 * Modified by Kirill Brazhnikov, 2026-11-02: MISRA C:2025: includes
 * <bsp/bsp.h>, which declares the functions defined here.
 *
 * Modified by Kirill Brazhnikov, 2026-11-08: Doxygen headers for the file
 * and its functions.
 *
 * Modified by Kirill Brazhnikov, 2026-11-15: moved from
 * boards/x86_64-qemu/bsp.c (the board support packages are in kernel/bsps).
 *
 * Modified by Kirill Brazhnikov, 2026-11-16: high and low level requirements of
 * the functions, traced to the system requirements (docs/SRQs), and the steps
 * of their design linked to the LLRs.
 */

/**
 * @file
 * Board initialization of the x86_64 QEMU board: the consoles, the
 * interrupt controller and the timer.
 */

#include "cons.h"
#include "pit.h"
#include "pic.h"
#include <asp/entries.h>

#include <assert.h>
#include <bsp/bsp.h>

/**
 * @funcname `ja_bsp_init`
 *
 * @description
 * Initializes the board when the kernel starts: the consoles, the 8259
 * interrupt controllers and the PIT timer, in this order.
 *
 * @globals
 * None.
 *
 * @locals
 * None.
 *
 * @hlr
 * - \hlrid{HLR_BSP_X86_64_JA_BSP_INIT_01}: `ja_bsp_init()` shall initialize the
 *   board when the kernel starts: first the consoles, then the interrupt
 *   controllers, then the timer.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_BOOT_002}
 *   - \srqref{SRQ_JET_BOOT_003}
 *   - \srqref{SRQ_JET_BSP_002}
 *   - \srqref{SRQ_JET_BSP_005}
 *
 * @llr
 * - \llrid{LLR_BSP_X86_64_JA_BSP_INIT_01}: `ja_bsp_init()` shall call, in this
 *   order, `jet_console_init_all()`, `pok_pic_init()` and
 *   `pok_x86_qemu_timer_init()`.
 *   <br>Traces to:
 *   - \hlrref{HLR_BSP_X86_64_JA_BSP_INIT_01}
 *
 * @design
 * 1. Call `jet_console_init_all()`. \llrref{LLR_BSP_X86_64_JA_BSP_INIT_01}
 * 2. Call `pok_pic_init()`. \llrref{LLR_BSP_X86_64_JA_BSP_INIT_01}
 * 3. Call `pok_x86_qemu_timer_init()`. \llrref{LLR_BSP_X86_64_JA_BSP_INIT_01}
 *
 * @calls
 * - `jet_console_init_all()`
 * - `pok_pic_init()`
 * - `pok_x86_qemu_timer_init()`
 */
void ja_bsp_init (void)
{
   jet_console_init_all();
   pok_pic_init ();
   pok_x86_qemu_timer_init ();
}
