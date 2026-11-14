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
 * guard isn't a reserved identifier; __user/__kuser are renamed
 * JET_USER/JET_KUSER.
 *
 * Modified by Kirill Brazhnikov, 2026-11-03: long lines are wrapped at 80
 * columns. ja_space_switch()'s parameter has the name of its definitions (Rule
 * 8.3).
 *
 * Modified by Kirill Brazhnikov, 2026-11-11: the Doxygen comments that don't
 * document a function definition are plain comments, so that only the function
 * headers are in the function documentation.
 *
 * Modified by Kirill Brazhnikov, 2026-11-13: the function declarations are
 * hidden from Doxygen (\cond): only the definitions of the functions are
 * documented; struct jet_space_layout is the typedef jet_space_layout_t, struct
 * jet_fp_store is the typedef jet_fp_store_t; jet_kernel_shared_t is used
 * by its typedef name; Doxygen comments for the file and its types (with their
 * members).
 *
 * Modified by Kirill Brazhnikov, 2026-11-14: the Doxygen comment of the file is
 * removed (header files are not documented).
 */

#ifndef JET_ASP_SPACE_H
#define JET_ASP_SPACE_H

#include <types.h>
#include <asp/cswitch.h>
#include <common.h>
#include <errno.h>
#include <uapi/kernel_shared_data.h>

/* 
 * Arch header should define:
 * 
 * type 'jet_ustack_t' - pointer to allocated user stack.
 *   Value 0 should be assignable and comparable to given type, and
 *   never corresponds to allocated stack.
 */
#include <arch/space.h>

/**
 * @typename `jet_space_id`
 *
 * @description
 * Identificator for the (memory) space.
 *
 * Value 0 corresponds to kernel space. Non-zero values correspond to user
 * spaces.
 */
typedef uint8_t jet_space_id;

/**
 * @typename `jet_space_layout_t`
 *
 * @description
 * Layout of the user space for load partition's code and data.
 */
typedef struct {
    /* 
     * Currently whole space is treated as single chunk.
     * 
     * TODO: There should be division for execute-only, read-only and
     * other sections. But this requires arch support.
     */
    /**
     * Currently whole space is treated as single chunk: its address for the
     * kernel.
     */
    char* kernel_addr;
    char* user_addr; /**< Its address in the partition. */
    size_t size; /**< Its size, in bytes. */
} jet_space_layout_t;

/* Fill layout for given user space. */
/** \cond */
void ja_space_layout_get(jet_space_id space_id,
    jet_space_layout_t* space_layout);

/* Return pointer to the heap for given space. */
void* ja_space_get_heap(jet_space_id space_id);

/* Return pointer to the kernel shared data for given space. */
jet_kernel_shared_t* JET_KUSER ja_space_shared_data(
    jet_space_id space_id);

/*
 * Jump to the user space.
 * 
 * Kernel stack passed as 'sp' will be used in interrupts/syscalls.
 */
void ja_user_space_jump(
    jet_stack_t stack_kernel,
    jet_space_id space_id, /* Actually, unused (should already be set with ja_space_switch). */
    void (JET_USER * entry_user)(void),
    jet_ustack_t stack_user);

/*
 * Switch to given space.
 */
void   ja_space_switch (jet_space_id space_id);

/*
 * Return id of current space.
 */
jet_space_id ja_space_get_current (void);

/* 
 * (Re)initialize user stacks allocator for given space.
 * 
 * 'space_id' shouldn't be 0.
 */
void ja_ustack_init (jet_space_id space_id);

/*
 * Allocates user stack for given space.
 * 
 * In case of insufficient memory returns 0.
 * 
 * 'space_id' shouldn't be 0.
 */
jet_ustack_t ja_ustack_alloc (jet_space_id space_id, size_t stack_size);

/** \endcond */

/*
 * Place for store floating point registers.
 * 
 * Kernel code doesn't use floating point operations,
 * so floating point registers needs to be stored only when switching
 * to other user space thread.
 */
/*
 * The floating point registers of a thread, which each architecture
 * defines (kernel/arch/<arch>/space.h); used by pointer.
 */
typedef struct jet_fp_store jet_fp_store_t;

/* 
 * Allocate place for store floating point registers.
 * 
 * May be called only during OS init.
 */
/** \cond */
jet_fp_store_t* ja_alloc_fp_store(void);

/* Save floating point registers into given place. */
void ja_fp_save(jet_fp_store_t* fp_store);

/* Restore floating point registers into given place. */
void ja_fp_restore(jet_fp_store_t* fp_store);

/* Initialize floating point registers with zero. */
void ja_fp_init(void);

/** \endcond */


#endif /* JET_ASP_SPACE_H */
