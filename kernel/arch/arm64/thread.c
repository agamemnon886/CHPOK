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
 * Modified by Kirill Brazhnikov, 2026-10-31: adapted from
 * kernel/arch/x86/thread.c for arm64.
 */

#include <asp/arch.h>

#include <libc.h>
#include <asp/cswitch.h>

#include "thread.h"
#include "sysreg.h"

/* This function starts kernel thread and is defined in cswitch.S.*/
void ja_kernel_thread(void);

struct jet_context* ja_context_init (jet_stack_t sp, void (*entry)(void))
{
    struct jet_context* ctx = (struct jet_context*)(sp - sizeof(*ctx));

    memset (ctx, 0, sizeof (*ctx));
    /*
     * Same guess as on x86: interrupt mask for the switched context is
     * the same as the current one.
     */
    ctx->daif = read_sysreg(daif);
    // Registers below are filled in accordance to ja_kernel_thread().
    ctx->fp = 0;
    ctx->x19_x28[0] = (uint64_t)entry;
    ctx->lr = (uint64_t)ja_kernel_thread;

    return ctx;
}
