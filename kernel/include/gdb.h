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
 */

#ifndef POK_GDB_H
#define POK_GDB_H

#include <config.h>
#include <libc.h>
#include <asp/gdb.h>

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

/* Callback function for append string(or other data) into
 * some stream.
 *
 * 'data' - data for append
 * 'len' - length of the data for append
 * 'cb_data' - opaque value, specific for concrete callback.
 */
typedef void (*print_cb_t)(const void* data, size_t len, void* cb_data);

#ifdef POK_NEEDS_GDB

/* Why the system has stopped in the GDB server. */
enum jet_gdb_stop_reason
{
    /* pok_trap(): data from GDB (connection or Ctrl-C), or boot. */
    JET_GDB_STOP_REQUEST,
    /* Breakpoint instruction which wasn't inserted by GDB. */
    JET_GDB_STOP_TRAP,
    /* Breakpoint inserted by GDB; the PC is at the breakpoint. */
    JET_GDB_STOP_SW_BREAKPOINT,
    /* Single step is done. */
    JET_GDB_STOP_STEP,
    /* Hardware breakpoint. */
    JET_GDB_STOP_HW_BREAKPOINT,
    /* Hardware watchpoints: write, read, access. */
    JET_GDB_STOP_WATCH,
    JET_GDB_STOP_RWATCH,
    JET_GDB_STOP_AWATCH
};

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
pok_bool_t jet_gdb_trap(struct jet_interrupt_context* ctx,
    enum jet_gdb_stop_reason reason, uintptr_t addr);

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

#endif /* POK_NEEDS_GDB */

#endif /* POK_GDB_H */
