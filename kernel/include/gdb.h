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
 * guard isn't a reserved identifier.
 *
 * Modified by Kirill Brazhnikov, 2026-11-04: NUMREGBYTES is removed (the
 * architecture describes the registers); the entry points of the GDB
 * server for the architecture code and the scheduler are declared here.
 *
 * Modified by Kirill Brazhnikov, 2026-11-13: the function declarations are
 * hidden from Doxygen (\cond): only the definitions of the functions are
 * documented; enum jet_gdb_stop_reason is the typedef jet_gdb_stop_reason_t;
 * interrupt_frame is used by its typedef name; GDB_CHAR_INTERRUPT (of gdb.c and
 * gdbserver.c) is defined here; Doxygen comments for the file, its types (with
 * their members) and its macros.
 *
 * Modified by Kirill Brazhnikov, 2026-11-14: the macros and the types of
 * kernel/libc/gdbserver.c are moved here; the enum types are named with _e:
 * jet_gdb_stop_reason_e (was jet_gdb_stop_reason_t); the Doxygen comment of the
 * file is removed (header files are not documented); the descriptions that
 * start with a function name write it in lower case, as in the code.
 */

#ifndef POK_GDB_H
#define POK_GDB_H

#include <config.h>
#include <libc.h>
#include <asp/gdb.h>
#include <stdint.h>
#include <stddef.h>
#include <types.h>
#include <asp/space.h>

/*
 * Whether we are in user space.
 *
 * This variable is set when we jump/return to user space and cleared
 * on return from there.
 *
 * This variable is checked in interrupts handlers for decide, whether
 * need to store 'global_thread_stack' localy.
 */
extern volatile pok_bool_t pok_in_user_space;

/**
 * @macroname `GDB_CHAR_INTERRUPT`
 *
 * @description
 * Ctrl-C from GDB: the character that GDB sends to stop the system.
 */
#define GDB_CHAR_INTERRUPT 3

/**
 * @typename `print_cb_t`
 *
 * @description
 * Callback function for append string(or other data) into some stream.
 *
 * 'data' - data for append 'len' - length of the data for append 'cb_data' -
 * opaque value, specific for concrete callback.
 */
typedef void (*print_cb_t)(const void* data, size_t len, void* cb_data);

#ifdef POK_NEEDS_GDB

/* Why the system has stopped in the GDB server. */
/**
 * @typename `jet_gdb_stop_reason_e`
 *
 * @description
 * Why the system stopped for GDB (the stop reply that GDB gets).
 */
typedef enum {
    /**
     * pok_trap(): data from GDB (connection or Ctrl-C), or boot.
     */
    JET_GDB_STOP_REQUEST,
    /**
     * Breakpoint instruction which wasn't inserted by GDB.
     */
    JET_GDB_STOP_TRAP,
    /**
     * Breakpoint inserted by GDB; the PC is at the breakpoint.
     */
    JET_GDB_STOP_SW_BREAKPOINT,
    /**
     * Single step is done.
     */
    JET_GDB_STOP_STEP,
    /**
     * Hardware breakpoint.
     */
    JET_GDB_STOP_HW_BREAKPOINT,
    /* Hardware watchpoints: write, read, access. */
    JET_GDB_STOP_WATCH, /**< Hardware watchpoint (write). */
    JET_GDB_STOP_RWATCH, /**< Hardware watchpoint (read). */
    JET_GDB_STOP_AWATCH /**< Hardware watchpoint (access). */
} jet_gdb_stop_reason_e;

/*
 * Called by the architecture on a breakpoint, single-step or debug
 * trap, with interrupts disabled. 'addr' is the address of the
 * breakpoint or watchpoint.
 *
 * Stops the system and talks to GDB until GDB resumes it.
 *
 * Return FALSE if the trap isn't for the GDB server (no GDB is
 * connected): it should be handled as an ordinary exception.
 */
/** \cond */
pok_bool_t jet_gdb_trap(interrupt_frame* ctx,
    jet_gdb_stop_reason_e reason, uintptr_t addr);

/*
 * Whether GDB has inserted a breakpoint at virtual address 'addr' of
 * space 'space_id'.
 */
pok_bool_t jet_gdb_is_breakpoint(jet_space_id space_id, uintptr_t addr);

/*
 * Called after the code of space 'space_id' has been (re)loaded:
 * inserts the breakpoints of that space again.
 */
void jet_gdb_space_loaded(jet_space_id space_id);

/*
 * Called by the GDB partition when character 'c' came from GDB: enters
 * the GDB server through pok_trap().
 */
void jet_gdb_request(int32_t c);

/* Initialize the GDB partition. */
void pok_gdb_thread_init(void);

/*
 * Save the floating point registers which are in the processor (they
 * are switched lazily) into their store, so all stores are up to date
 * (jet_fp_sync()); load them back after GDB has changed the store
 * (jet_fp_reload()). Defined in kernel/core/sched.c.
 */
void jet_fp_sync(void);
void jet_fp_reload(void);

/** \endcond */

/**
 * @macroname `GDB_PACKET_SIZE`
 *
 * @description
 * Maximum size of the data of a packet (PacketSize, as the server tells GDB).
 */
#define GDB_PACKET_SIZE 4096U

/**
 * @macroname `GDB_SW_BREAKPOINTS`
 *
 * @description
 * Software breakpoints GDB may insert at the same time.
 */
#define GDB_SW_BREAKPOINTS 64U

/**
 * @name GDB processes
 * The process IDs of the GDB protocol: process 1 for the kernel-only
 * partitions, process U+2 for user partition U.
 * @{
 */

/**
 * @macroname `GDB_PID_KERNEL`
 *
 * @description
 * Process of the kernel-only partitions.
 */
#define GDB_PID_KERNEL 1U

/**
 * @macroname `GDB_PID_MAX`
 *
 * @description
 * Largest process: 255 user partitions (pok_partitions_arinc_n) + 1.
 */
#define GDB_PID_MAX 256U

/** @} */

/**
 * @name GDB signals
 * Signals reported to GDB (in stop replies).
 * @{
 */

/**
 * @macroname `GDB_SIGNAL_NONE`
 *
 * @description
 * No signal (the system stopped for no reason GDB knows).
 */
#define GDB_SIGNAL_NONE 0U

/**
 * @macroname `GDB_SIGNAL_INT`
 *
 * @description
 * SIGINT: the system was interrupted (Ctrl-C).
 */
#define GDB_SIGNAL_INT  2U

/**
 * @macroname `GDB_SIGNAL_TRAP`
 *
 * @description
 * SIGTRAP: a breakpoint, a watchpoint or a step.
 */
#define GDB_SIGNAL_TRAP 5U

/** @} */

/**
 * @macroname `GDB_NO_INDEX`
 *
 * @description
 * No index (of a partition).
 */
#define GDB_NO_INDEX 0xFFFFFFFFU

/**
 * @typename `gdb_thread_t`
 *
 * @description
 * Thread as GDB sees it: its process and thread numbers (see the threading
 * model at the beginning of gdbserver.c) and the partition and thread of the
 * kernel.
 */
typedef struct
{
    uint32_t pid;       /**< Process, from 1. */
    uint32_t tid;       /**< Thread within the process, from 1. */
    pok_partition_t* part; /**< The partition of the thread. */
    void* part_private; /**< Thread data of the partition. */
} gdb_thread_t;

/**
 * @typename `gdb_thread_ref_t`
 *
 * @description
 * Thread id as GDB writes it: -1 means all, 0 means any.
 */
typedef struct
{
    int64_t pid; /**< The process (-1: all, 0: any). */
    int64_t tid; /**< The thread (-1: all, 0: any). */
} gdb_thread_ref_t;

/**
 * @typename `gdb_sw_breakpoint_t`
 *
 * @description
 * Software breakpoint inserted by GDB: the breakpoint instruction written at an
 * address of a space, and the code it replaced.
 */
typedef struct
{
    pok_bool_t used; /**< The slot is in use. */
    jet_space_id space_id; /**< The space of the address. */
    uintptr_t addr; /**< The address of the breakpoint. */
    uint8_t saved[JA_GDB_BREAKPOINT_SIZE]; /**< Code under the breakpoint. */
} gdb_sw_breakpoint_t;

/**
 * @typename `gdb_search_t`
 *
 * @description
 * State of a search of partitions with for_each_partition(): what is looked for
 * (a partition by its index among the partitions of a kind, or the index of a
 * partition) and what was found.
 */
typedef struct
{
    pok_bool_t is_kernel;          /**< Kernel-only or user partitions. */
    uint32_t index;                /**< Index of the partition wanted, */
    const pok_partition_t* wanted; /**< or the partition wanted. */
    uint32_t count;                /**< Partitions of that kind visited. */
    pok_partition_t* found;        /**< Partition at 'index'. */
    uint32_t found_index;          /**< Index of 'wanted'. */
} gdb_search_t;

#endif /* POK_NEEDS_GDB */

#endif /* POK_GDB_H */
