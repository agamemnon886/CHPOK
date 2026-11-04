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
 * Modified by Kirill Brazhnikov, 2026-11-02: MISRA C:2025: the header
 * guard isn't a reserved identifier; ja_inf_loop() is _Noreturn (a
 * documented Dir 1.2 deviation); pok_arch_init() is declared; deviations
 * are documented where a guideline can't be followed.
 */

/* Unsorted arch-specific functions. */

#ifndef JET_ASP_ARCH_H
#define JET_ASP_ARCH_H

#include <types.h>

// TODO: Where should be that definition?
#define KERNEL_STACK_SIZE_DEFAULT 8192

/**
 * Disable interrupts
 */
void ja_preempt_disable (void);

/**
 * Enable interrupts
 */
void ja_preempt_enable (void);

/**
 * Returns true if interrupts are enabled
 */
pok_bool_t ja_preempt_enabled(void);

/**
 * Function that do nothing. Useful for the idle task for example.
 */
/*
 * MISRA C:2025 Dir 1.2 deviation: _Noreturn is a C11 keyword, accepted by GCC
 * in gnu99 mode; it tells the compiler and the reader that the function
 * doesn't return.
 */
_Noreturn void ja_inf_loop(void);

/* Architecture initialization, called from the boot code (entry.S). */
void pok_arch_init(void);

/* Architecture initialization, called from the boot code (entry.S). */
void pok_arch_init(void);

/*
 * reset cpu
 */
void ja_cpu_reset(void);

#endif /* !__POK_ARCH_H__ */
