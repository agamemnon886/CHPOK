/*
 * Modified by Kirill Brazhnikov, 2026-10-31: built-in integer types (int,
 * unsigned, short, long, ...) are replaced with the fixed-width types
 * from <stdint.h>.
 *
 * Modified by Kirill Brazhnikov, 2026-11-04: ported to x86_64 and
 * rewritten to follow the GDB remote protocol: 64-bit clean parsing of
 * numbers and thread ids; registers, breakpoints, single-stepping and
 * hardware breakpoints and watchpoints through the architecture interface
 * (<asp/gdb.h>); memory is accessed only through ja_addr_to_gdb*();
 * correct replies to qSupported (with PacketSize), vAttach, Hg, D and
 * vKill; a stop reply is sent only when GDB waits for one; breakpoints
 * survive reloading of partitions; the system time stops while GDB holds
 * the system; Ctrl-C which comes while the system is stopped (e.g. during a
 * step) stops it again when GDB resumes it; memory reads and writes don't
 * see the inserted breakpoints; the registers of threads switched off in
 * the kernel aren't written. The PowerPC and i386 code is removed.
 * MISRA C:2025.
 *
 * Modified by Kirill Brazhnikov, 2026-11-05: arm64 too; a stop in the
 * kernel's code while a user partition runs is reported for the GDB
 * partition's thread, so that GDB finds the kernel's breakpoints and
 * watchpoints, and a step of the kernel's code ends in the thread which
 * has stepped; Z0 refuses an unaligned address.
 *
 * Modified by Kirill Brazhnikov, 2026-11-08: Doxygen headers for the file
 * and its functions.
 *
 * Modified by Kirill Brazhnikov, 2026-11-13: the structures and enums are used
 * by their typedef names: interrupt_frame, ja_gdb_hw_type_t, jet_fp_store_t,
 * jet_gdb_stop_reason_t, pok_part_sched_ops_t; the macros and the types are
 * moved to kernel/include/gdb.h; the structures are the typedefs gdb_thread_t,
 * gdb_thread_ref_t, gdb_sw_breakpoint_t and gdb_search_t; GDB_CHAR_INTERRUPT is
 * in gdb.h.
 *
 * Modified by Kirill Brazhnikov, 2026-11-14: the enum types are named with _e:
 * ja_gdb_hw_type_e (was ja_gdb_hw_type_t), jet_gdb_stop_reason_e (was
 * jet_gdb_stop_reason_t).
 *
 * Modified by Kirill Brazhnikov, 2026-11-15: moved from
 * kernel/libc/gdbserver.c (kernel/libc is split up: the GDB server, the
 * monitor and readline are in kernel/debug).
 *
 * Modified by Kirill Brazhnikov, 2026-11-16: high and low level requirements of
 * the functions, traced to the system requirements (docs/SRQs), and the steps
 * of their design linked to the LLRs.
 *
 * Modified by Kirill Brazhnikov, 2026-11-17: low level requirements of one
 * behaviour each; code that compiles to nothing (assertions, (void) x;,
 * empty branches) is left out of the design; requirements with more than one
 * "shall" are split into requirements of one "shall" each, and the high level
 * requirements trace to the system requirements split in the same way.
 */

/**
 * @file
 * The kernel's GDB server: the GDB remote serial protocol on the debug
 * console (packets, threads of the partitions, registers, memory,
 * software and hardware breakpoints, watchpoints and single steps).
 */
#include <config.h>

#ifdef POK_NEEDS_GDB

/****************************************************************************

        THIS SOFTWARE IS NOT COPYRIGHTED

   HP offers the following for use in the public domain.  HP makes no
   warranty with regard to the software or it's performance and the
   user accepts the software "AS IS" with all faults.

   HP DISCLAIMS ANY WARRANTIES, EXPRESS OR IMPLIED, WITH REGARD
   TO THIS SOFTWARE INCLUDING BUT NOT LIMITED TO THE WARRANTIES
   OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.

****************************************************************************/

/****************************************************************************
 *  Header: remcom.c,v 1.34 91/03/09 12:29:49 glenne Exp $
 *
 *  Module name: remcom.c $
 *  Revision: 1.34 $
 *  Date: 91/03/09 12:29:49 $
 *  Contributor:     Lake Stevens Instrument Division$
 *
 *  Description:     low level support for gdb debugger. $
 *
 *  Considerations:  only works on target hardware $
 *
 *  Written by:      Glenn Engel $
 *  ModuleState:     Experimental $
 *
 *  NOTES:           See Below $
 *
 *  Modified for 386 by Jim Kingdon, Cygnus Support.
 *
 ****************************************************************************
 *
 * GDB server of JetOS.
 *
 * GDB talks to it over the second serial port, in extended-remote mode.
 * The server is entered from the architecture code (jet_gdb_trap()) with
 * interrupts disabled, so the whole system stops while GDB holds it
 * (all-stop mode). The ways in:
 *
 *  - the GDB partition (a time slot of type GDB, see gdb.c) sees data
 *    from GDB: a connection, or Ctrl-C while the system runs;
 *  - POK_NEEDS_WAIT_FOR_GDB: the kernel waits for GDB at boot;
 *  - a breakpoint, a single step, a hardware breakpoint or watchpoint.
 *
 * Supported packets: ?, !, qSupported, qC, qfThreadInfo, qsThreadInfo,
 * qThreadExtraInfo, qAttached, qSymbol, H, T, g, G, m, M, Z0-Z4, z0-z4,
 * c, C, s, S, vAttach, vKill, D, k. Other packets get the empty reply
 * ("not supported"); GDB falls back to the packets above.
 *
 * All commands and responses are sent with a packet which includes a
 * checksum.  A packet consists of
 *
 * $<packet info>#<checksum>.
 *
 * where
 * <packet info> :: <characters representing the command or response>
 * <checksum>    :: < two hex digits computed as modulo 256 sum of <packetinfo>>
 *
 * When a packet is received, it is first acknowledged with either '+' or '-'.
 * '+' indicates a successful transfer.  '-' indicates a failed transfer.
 *
 * Example:
 *
 * Host:                  Reply:
 * $m0,10#2a               +$00010203040506070809101112131415#42
 *
 ****************************************************************************/

#include <libc.h>
#include <cons.h>
#include <gdb.h>
#include <asp/time.h>
#include <asp/uaccess.h>
#include <core/debug.h>
#include <core/partition.h>
#include <core/sched.h>

/*
 * MISRA C:2025 Rule 11.5 deviation (this file): the kernel addresses of
 * the memory GDB reads and writes (ja_addr_to_gdb*()), the data given to
 * the print callback and the thread data of a partition are pointers to
 * void, converted to the type of the data.
 */

/***********************************************************************
 *
 * Threading model of the POK.
 *
 * For each kernel-only partition with (relative) index K there is thread
 * 1.(K+1)
 *
 * For each partition with access to user space (space_id is not 0)
 * with (relative) index U there is inferior
 * (U+2). For each thread with index T within such partition there is
 * GDB thread (U+2).(T+1)
 *
 * In the GDB protocol, these are process (pid) 1 for the kernel-only
 * partitions and process U+2 for user partition U, so thread ids are
 * p1.(K+1) and p(U+2).(T+1). GDB lists the threads of the processes it
 * has attached to (vAttach); process 1 is attached on connection.
 */

static const char gdb_hex_chars[] = "0123456789abcdef";

/* Received packet; gdb_in_pos is the next character to parse. */
static char gdb_in[GDB_PACKET_SIZE + 1U];
static size_t gdb_in_len;
static size_t gdb_in_pos;

/* Reply being built. */
static char gdb_out[GDB_PACKET_SIZE + 1U];
static size_t gdb_out_len;

/* Character read ahead, which gdb_get_char() returns first (or -1). */
static int32_t gdb_pushback = -1;
/* Character which has made the GDB partition enter (or -1). */
static int32_t gdb_request_char = -1;

/* Whether the server is talking to GDB (inside gdb_stop()). */
static pok_bool_t gdb_in_server = FALSE;
/* Whether GDB is connected. */
static pok_bool_t gdb_session = FALSE;
/* Whether GDB waits for a stop reply (it has resumed the system). */
static pok_bool_t gdb_stop_reply_pending = FALSE;
/*
 * Whether Ctrl-C has come while the system was stopped (GDB sends it
 * when it waits for a stop, e.g. during a step): the system stops again
 * as soon as GDB resumes it, as gdbserver does.
 */
static pok_bool_t gdb_interrupt_pending = FALSE;
/* Processes GDB is attached to. */
static pok_bool_t gdb_attached[GDB_PID_MAX + 1U];
/* Stop reasons GDB understands (qSupported). */
static pok_bool_t gdb_swbreak_feature = FALSE;
static pok_bool_t gdb_hwbreak_feature = FALSE;

/* The stop: registers and thread which has stopped, and why. */
static interrupt_frame* gdb_stop_ctx;
static gdb_thread_t gdb_current;
static uint32_t gdb_stop_signal;
static jet_gdb_stop_reason_e gdb_stop_reason;
static uintptr_t gdb_stop_addr;
/* Thread for registers, memory and breakpoints (Hg). */
static gdb_thread_t gdb_selected;
/* Whether GDB has written registers (FP ones must be reloaded). */
static pok_bool_t gdb_registers_changed;

/*
 * Single step: the thread which steps. If the system stops for another
 * reason first, the step is cancelled: its trap is ignored when it comes.
 */
static pok_bool_t gdb_step_pending = FALSE;
static pok_bool_t gdb_step_cancelled = FALSE;
static gdb_thread_t gdb_step_thread;

/* Next thread for qsThreadInfo (pid 0: no more). */
static uint32_t gdb_list_pid;
static uint32_t gdb_list_tid;

static gdb_sw_breakpoint_t gdb_sw_breakpoints[GDB_SW_BREAKPOINTS];

/***********************************************************************
 * Partitions and threads.
 */

/* State of a search of partitions with for_each_partition(). */
static gdb_search_t gdb_search;

/**
 * @funcname `gdb_partition_is_kernel`
 *
 * @description
 * Tells whether a partition is kernel-only (no memory space).
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `part` | `const pok_partition_t *` | [in] The partition. |
 *
 * @return TRUE for a kernel-only partition.
 *
 * @globals
 * None.
 *
 * @locals
 * None.
 *
 * @hlr
 * - \hlrid{HLR_GDBSERVER_GDB_PARTITION_IS_KERNEL_01}:
 *   `gdb_partition_is_kernel()` shall tell whether a partition is a kernel-only
 *   partition, one without a memory space.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_DBG_002}
 *
 * @llr
 * - \llrid{LLR_GDBSERVER_GDB_PARTITION_IS_KERNEL_01}: If `space_id` of `part`
 *   is 0, `gdb_partition_is_kernel()` shall return true.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDBSERVER_GDB_PARTITION_IS_KERNEL_01}
 * - \llrid{LLR_GDBSERVER_GDB_PARTITION_IS_KERNEL_02}: If `space_id` of `part`
 *   is not 0, `gdb_partition_is_kernel()` shall return false.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDBSERVER_GDB_PARTITION_IS_KERNEL_01}
 *
 * @design
 * 1. Return `part->space_id == 0U`.
 *    \llrref{LLR_GDBSERVER_GDB_PARTITION_IS_KERNEL_01},
 *    \llrref{LLR_GDBSERVER_GDB_PARTITION_IS_KERNEL_02}
 *
 * @calls
 * None.
 */
static pok_bool_t gdb_partition_is_kernel(const pok_partition_t* part)
{
    return part->space_id == 0U;
}

/**
 * @funcname `gdb_search_cb`
 *
 * @description
 * The callback of the partition search: counts the partitions of the kind
 * looked for, and notes the one at the index and the index of the one wanted.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `part` | `pok_partition_t *` | [in] A partition. |
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `gdb_search` | `gdb_search_t` | The partition search of the GDB server (what is looked for, what was found) |
 *
 * @locals
 * None.
 *
 * @hlr
 * - \hlrid{HLR_GDBSERVER_GDB_SEARCH_CB_01}: `gdb_search_cb()` shall count the
 *   partitions of the kind searched for, and note the one at the index wanted
 *   and the index of the partition wanted.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_DBG_002}
 *
 * @llr
 * - \llrid{LLR_GDBSERVER_GDB_SEARCH_CB_01}: If the result of
 *   `gdb_partition_is_kernel()` for `part` is equal to `gdb_search.is_kernel`,
 *   `gdb_search_cb()` shall increment `gdb_search.count`.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDBSERVER_GDB_SEARCH_CB_01}
 * - \llrid{LLR_GDBSERVER_GDB_SEARCH_CB_02}: For such a partition, if and only
 *   if `gdb_search.count` before the increment is `gdb_search.index`,
 *   `gdb_search_cb()` shall set `gdb_search.found` to `part`.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDBSERVER_GDB_SEARCH_CB_01}
 * - \llrid{LLR_GDBSERVER_GDB_SEARCH_CB_03}: For such a partition, if and only
 *   if `part` is `gdb_search.wanted`, `gdb_search_cb()` shall set
 *   `gdb_search.found_index` to `gdb_search.count` before the increment.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDBSERVER_GDB_SEARCH_CB_01}
 * - \llrid{LLR_GDBSERVER_GDB_SEARCH_CB_04}: If the result of
 *   `gdb_partition_is_kernel()` for `part` is not equal to
 *   `gdb_search.is_kernel`, `gdb_search_cb()` shall leave `gdb_search` as it
 *   is.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDBSERVER_GDB_SEARCH_CB_01}
 *
 * @design
 * 1. If `gdb_partition_is_kernel(part)` is equal to `gdb_search.is_kernel`:
 *    \llrref{LLR_GDBSERVER_GDB_SEARCH_CB_01},
 *    \llrref{LLR_GDBSERVER_GDB_SEARCH_CB_02},
 *    \llrref{LLR_GDBSERVER_GDB_SEARCH_CB_03},
 *    \llrref{LLR_GDBSERVER_GDB_SEARCH_CB_04}
 *    1. If `gdb_search.count` is equal to `gdb_search.index`:
 *       \llrref{LLR_GDBSERVER_GDB_SEARCH_CB_02}
 *       1. Set `gdb_search.found` to `part`.
 *    2. If `part` is equal to `gdb_search.wanted`:
 *       \llrref{LLR_GDBSERVER_GDB_SEARCH_CB_03}
 *       1. Set `gdb_search.found_index` to `gdb_search.count`.
 *    3. Increment `gdb_search.count`. \llrref{LLR_GDBSERVER_GDB_SEARCH_CB_01}
 *
 * @calls
 * - `gdb_partition_is_kernel()`
 */
static void gdb_search_cb(pok_partition_t* part)
{
    if (gdb_partition_is_kernel(part) == gdb_search.is_kernel)
    {
        if (gdb_search.count == gdb_search.index) {
            gdb_search.found = part;
        }
        if (part == gdb_search.wanted) {
            gdb_search.found_index = gdb_search.count;
        }
        gdb_search.count++;
    }
}

/**
 * @funcname `gdb_search_run`
 *
 * @description
 * Searches the partitions of one kind (kernel-only or user) for an index or for
 * a partition.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `is_kernel` | `pok_bool_t` | [in] The kind. |
 * | `index` | `uint32_t` | [in] The index looked for (GDB_NO_INDEX: none). |
 * | `wanted` | `const pok_partition_t *` | [in] The partition looked for, or NULL. |
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `gdb_search` | `gdb_search_t` | The partition search of the GDB server (what is looked for, what was found) |
 *
 * @locals
 * None.
 *
 * @hlr
 * - \hlrid{HLR_GDBSERVER_GDB_SEARCH_RUN_01}: `gdb_search_run()` shall search
 *   the partitions of one kind, kernel-only or user, in their order, for an
 *   index or for a partition.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_DBG_002}
 *
 * @llr
 * - \llrid{LLR_GDBSERVER_GDB_SEARCH_RUN_01}: `gdb_search_run()` shall set, in
 *   `gdb_search`, `is_kernel`, `index` and `wanted` to the arguments of the
 *   same names, `count` to 0, `found` to NULL and `found_index` to
 *   `GDB_NO_INDEX`, and then call `for_each_partition()` once with
 *   `gdb_search_cb`.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDBSERVER_GDB_SEARCH_RUN_01}
 *
 * @design
 * 1. Set `gdb_search.is_kernel` to `is_kernel`.
 *    \llrref{LLR_GDBSERVER_GDB_SEARCH_RUN_01}
 * 2. Set `gdb_search.index` to `index`.
 *    \llrref{LLR_GDBSERVER_GDB_SEARCH_RUN_01}
 * 3. Set `gdb_search.wanted` to `wanted`.
 *    \llrref{LLR_GDBSERVER_GDB_SEARCH_RUN_01}
 * 4. Set `gdb_search.count` to `0U`. \llrref{LLR_GDBSERVER_GDB_SEARCH_RUN_01}
 * 5. Set `gdb_search.found` to `NULL`. \llrref{LLR_GDBSERVER_GDB_SEARCH_RUN_01}
 * 6. Set `gdb_search.found_index` to `GDB_NO_INDEX`.
 *    \llrref{LLR_GDBSERVER_GDB_SEARCH_RUN_01}
 * 7. Call `for_each_partition()` with the address of `gdb_search_cb`.
 *    \llrref{LLR_GDBSERVER_GDB_SEARCH_RUN_01}
 *
 * @calls
 * - `for_each_partition()`
 */
static void gdb_search_run(pok_bool_t is_kernel, uint32_t index,
    const pok_partition_t* wanted)
{
    gdb_search.is_kernel = is_kernel;
    gdb_search.index = index;
    gdb_search.wanted = wanted;
    gdb_search.count = 0U;
    gdb_search.found = NULL;
    gdb_search.found_index = GDB_NO_INDEX;

    for_each_partition(&gdb_search_cb);
}

/**
 * @funcname `gdb_partition_at`
 *
 * @description
 * Returns the partition of a kind at an index.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `is_kernel` | `pok_bool_t` | [in] The kind. |
 * | `index` | `uint32_t` | [in] The index. |
 *
 * @return The partition, or NULL.
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `gdb_search` | `gdb_search_t` | The partition search of the GDB server (what is looked for, what was found) |
 *
 * @locals
 * None.
 *
 * @hlr
 * - \hlrid{HLR_GDBSERVER_GDB_PARTITION_AT_01}: `gdb_partition_at()` shall give
 *   the partition of a kind at an index among the partitions of that kind, or
 *   none.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_DBG_002}
 *
 * @llr
 * - \llrid{LLR_GDBSERVER_GDB_PARTITION_AT_01}: `gdb_partition_at()` shall call
 *   `gdb_search_run()` once with `is_kernel`, `index` and NULL, and return
 *   `gdb_search.found`.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDBSERVER_GDB_PARTITION_AT_01}
 *
 * @design
 * 1. Call `gdb_search_run()` with the arguments `is_kernel`, `index` and
 *    `NULL`. \llrref{LLR_GDBSERVER_GDB_PARTITION_AT_01}
 * 2. Return `gdb_search.found`. \llrref{LLR_GDBSERVER_GDB_PARTITION_AT_01}
 *
 * @calls
 * - `gdb_search_run()`
 */
static pok_partition_t* gdb_partition_at(pok_bool_t is_kernel,
    uint32_t index)
{
    gdb_search_run(is_kernel, index, NULL);
    return gdb_search.found;
}

/**
 * @funcname `gdb_partition_count`
 *
 * @description
 * Returns the number of partitions of a kind.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `is_kernel` | `pok_bool_t` | [in] The kind. |
 *
 * @return The number.
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `gdb_search` | `gdb_search_t` | The partition search of the GDB server (what is looked for, what was found) |
 *
 * @locals
 * None.
 *
 * @hlr
 * - \hlrid{HLR_GDBSERVER_GDB_PARTITION_COUNT_01}: `gdb_partition_count()` shall
 *   give the number of partitions of a kind.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_DBG_002}
 *
 * @llr
 * - \llrid{LLR_GDBSERVER_GDB_PARTITION_COUNT_01}: `gdb_partition_count()` shall
 *   call `gdb_search_run()` once with `is_kernel`, `GDB_NO_INDEX` and NULL, and
 *   return `gdb_search.count`.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDBSERVER_GDB_PARTITION_COUNT_01}
 *
 * @design
 * 1. Call `gdb_search_run()` with the arguments `is_kernel`, `GDB_NO_INDEX` and
 *    `NULL`. \llrref{LLR_GDBSERVER_GDB_PARTITION_COUNT_01}
 * 2. Return `gdb_search.count`. \llrref{LLR_GDBSERVER_GDB_PARTITION_COUNT_01}
 *
 * @calls
 * - `gdb_search_run()`
 */
static uint32_t gdb_partition_count(pok_bool_t is_kernel)
{
    gdb_search_run(is_kernel, GDB_NO_INDEX, NULL);
    return gdb_search.count;
}

/**
 * @funcname `gdb_partition_index`
 *
 * @description
 * Returns the index of a partition among the partitions of its kind.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `part` | `const pok_partition_t *` | [in] The partition. |
 *
 * @return The index, or GDB_NO_INDEX.
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `gdb_search` | `gdb_search_t` | The partition search of the GDB server (what is looked for, what was found) |
 *
 * @locals
 * None.
 *
 * @hlr
 * - \hlrid{HLR_GDBSERVER_GDB_PARTITION_INDEX_01}: `gdb_partition_index()` shall
 *   give the index of a partition among the partitions of its kind.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_DBG_002}
 *
 * @llr
 * - \llrid{LLR_GDBSERVER_GDB_PARTITION_INDEX_01}: `gdb_partition_index()` shall
 *   call `gdb_search_run()` once with the result of `gdb_partition_is_kernel()`
 *   for `part`, `GDB_NO_INDEX` and `part`, and return `gdb_search.found_index`.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDBSERVER_GDB_PARTITION_INDEX_01}
 *
 * @design
 * 1. Call `gdb_search_run()` with the arguments
 *    `gdb_partition_is_kernel(part)`, `GDB_NO_INDEX` and `part`.
 *    \llrref{LLR_GDBSERVER_GDB_PARTITION_INDEX_01}
 * 2. Return `gdb_search.found_index`.
 *    \llrref{LLR_GDBSERVER_GDB_PARTITION_INDEX_01}
 *
 * @calls
 * - `gdb_search_run()`
 * - `gdb_partition_is_kernel()`
 */
static uint32_t gdb_partition_index(const pok_partition_t* part)
{
    gdb_search_run(gdb_partition_is_kernel(part), GDB_NO_INDEX, part);
    return gdb_search.found_index;
}

/**
 * @funcname `gdb_partition_pid`
 *
 * @description
 * Returns the GDB process of a partition: 1 for the kernel-only partitions
 * (threads of the kernel), 2 and up for the user partitions.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `part` | `const pok_partition_t *` | [in] The partition. |
 *
 * @return The process ID.
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `pid` | `uint32_t` | The process ID |
 *
 * @hlr
 * - \hlrid{HLR_GDBSERVER_GDB_PARTITION_PID_01}: `gdb_partition_pid()` shall
 *   give the debugger process of a partition: 1 for a kernel-only partition,
 *   and 2 plus its index for a user partition.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_DBG_002}
 *
 * @llr
 * - \llrid{LLR_GDBSERVER_GDB_PARTITION_PID_01}: If `gdb_partition_is_kernel()`
 *   returns true for `part`, `gdb_partition_pid()` shall return
 *   `GDB_PID_KERNEL`.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDBSERVER_GDB_PARTITION_PID_01}
 * - \llrid{LLR_GDBSERVER_GDB_PARTITION_PID_02}: If `gdb_partition_is_kernel()`
 *   returns false for `part`, `gdb_partition_pid()` shall return the result of
 *   `gdb_partition_index()` for `part` plus 2.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDBSERVER_GDB_PARTITION_PID_01}
 *
 * @design
 * 1. Set `pid` to `GDB_PID_KERNEL`. \llrref{LLR_GDBSERVER_GDB_PARTITION_PID_01}
 * 2. If `gdb_partition_is_kernel(part)` is false:
 *    \llrref{LLR_GDBSERVER_GDB_PARTITION_PID_01},
 *    \llrref{LLR_GDBSERVER_GDB_PARTITION_PID_02}
 *    1. Set `pid` to `gdb_partition_index(part) + 2U`.
 *       \llrref{LLR_GDBSERVER_GDB_PARTITION_PID_02}
 * 3. Return `pid`. \llrref{LLR_GDBSERVER_GDB_PARTITION_PID_01},
 *    \llrref{LLR_GDBSERVER_GDB_PARTITION_PID_02}
 *
 * @calls
 * - `gdb_partition_is_kernel()`
 * - `gdb_partition_index()`
 */
static uint32_t gdb_partition_pid(const pok_partition_t* part)
{
    uint32_t pid = GDB_PID_KERNEL;

    if (!gdb_partition_is_kernel(part)) {
        pid = gdb_partition_index(part) + 2U;
    }

    return pid;
}

/**
 * @funcname `gdb_process_partition`
 *
 * @description
 * Returns the user partition of a GDB process (2 and up).
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `pid` | `uint32_t` | [in] The process ID. |
 *
 * @return The partition, or NULL.
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `part` | `pok_partition_t *` | The partition |
 *
 * @hlr
 * - \hlrid{HLR_GDBSERVER_GDB_PROCESS_PARTITION_01}: `gdb_process_partition()`
 *   shall give the user partition of a debugger process from 2 on, or none.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_DBG_002}
 *
 * @llr
 * - \llrid{LLR_GDBSERVER_GDB_PROCESS_PARTITION_01}: If `pid` is 2 or more and
 *   is not greater than `GDB_PID_MAX`, `gdb_process_partition()` shall return
 *   the result of `gdb_partition_at()` for `FALSE` and `pid` minus 2.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDBSERVER_GDB_PROCESS_PARTITION_01}
 * - \llrid{LLR_GDBSERVER_GDB_PROCESS_PARTITION_02}: If `pid` is less than 2 or
 *   is greater than `GDB_PID_MAX`, `gdb_process_partition()` shall return NULL
 *   without calling `gdb_partition_at()`.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDBSERVER_GDB_PROCESS_PARTITION_01}
 *
 * @design
 * 1. Set `part` to `NULL`. \llrref{LLR_GDBSERVER_GDB_PROCESS_PARTITION_02}
 * 2. If `pid` is greater than or equal to `2U` and `pid` is less than or equal
 *    to `GDB_PID_MAX`: \llrref{LLR_GDBSERVER_GDB_PROCESS_PARTITION_01},
 *    \llrref{LLR_GDBSERVER_GDB_PROCESS_PARTITION_02}
 *    1. Call `gdb_partition_at()` with the arguments `FALSE` and `pid - 2U`,
 *       and store the result in `part`.
 *       \llrref{LLR_GDBSERVER_GDB_PROCESS_PARTITION_01}
 * 3. Return `part`. \llrref{LLR_GDBSERVER_GDB_PROCESS_PARTITION_01},
 *    \llrref{LLR_GDBSERVER_GDB_PROCESS_PARTITION_02}
 *
 * @calls
 * - `gdb_partition_at()`
 */
static pok_partition_t* gdb_process_partition(uint32_t pid)
{
    pok_partition_t* part = NULL;

    if ((pid >= 2U) && (pid <= GDB_PID_MAX)) {
        part = gdb_partition_at(FALSE, pid - 2U);
    }

    return part;
}

/**
 * @funcname `gdb_process_last`
 *
 * @description
 * Returns the last GDB process ID (1 plus the number of user partitions).
 *
 * @return The last process ID.
 *
 * @globals
 * None.
 *
 * @locals
 * None.
 *
 * @hlr
 * - \hlrid{HLR_GDBSERVER_GDB_PROCESS_LAST_01}: `gdb_process_last()` shall give
 *   the last debugger process identifier: 1 plus the number of user partitions.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_DBG_002}
 *
 * @llr
 * - \llrid{LLR_GDBSERVER_GDB_PROCESS_LAST_01}: `gdb_process_last()` shall
 *   return the result of `gdb_partition_count()` for `FALSE` plus 1.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDBSERVER_GDB_PROCESS_LAST_01}
 *
 * @design
 * 1. Return `gdb_partition_count(FALSE) + 1U`.
 *    \llrref{LLR_GDBSERVER_GDB_PROCESS_LAST_01}
 *
 * @calls
 * - `gdb_partition_count()`
 */
static uint32_t gdb_process_last(void)
{
    return gdb_partition_count(FALSE) + 1U;
}

/**
 * @funcname `gdb_process_threads`
 *
 * @description
 * Returns the number of threads of a GDB process: the kernel-only partitions
 * for process 1, the partition's threads otherwise.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `pid` | `uint32_t` | [in] The process ID. |
 *
 * @return The number of threads (0 for no such process).
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `n` | `uint32_t` | The number |
 * | `part` | `pok_partition_t *` | The partition |
 * | `threads` | `int32_t` | The partition's number of threads |
 *
 * @hlr
 * - \hlrid{HLR_GDBSERVER_GDB_PROCESS_THREADS_01}: `gdb_process_threads()` shall
 *   give the number of threads of a debugger process: the kernel-only
 *   partitions for process 1, and the threads of the partition otherwise, 0 for
 *   no such process.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_DBG_002}
 *
 * @llr
 * - \llrid{LLR_GDBSERVER_GDB_PROCESS_THREADS_01}: If `pid` is `GDB_PID_KERNEL`,
 *   `gdb_process_threads()` shall return the result of `gdb_partition_count()`
 *   for `TRUE`.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDBSERVER_GDB_PROCESS_THREADS_01}
 * - \llrid{LLR_GDBSERVER_GDB_PROCESS_THREADS_02}: If `pid` is not
 *   `GDB_PID_KERNEL`, `gdb_process_partition()` returns a partition for `pid`,
 *   and the function `get_number_of_threads` of its `part_sched_ops` returns a
 *   value greater than 0 for it, `gdb_process_threads()` shall return that
 *   value.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDBSERVER_GDB_PROCESS_THREADS_01}
 * - \llrid{LLR_GDBSERVER_GDB_PROCESS_THREADS_03}: If `pid` is not
 *   `GDB_PID_KERNEL`, and `gdb_process_partition()` returns NULL for `pid` or
 *   the function `get_number_of_threads` returns a value that is not greater
 *   than 0, `gdb_process_threads()` shall return 0.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDBSERVER_GDB_PROCESS_THREADS_01}
 *
 * @design
 * 1. Set `n` to `0U`. \llrref{LLR_GDBSERVER_GDB_PROCESS_THREADS_03}
 * 2. If `pid` is equal to `GDB_PID_KERNEL`:
 *    \llrref{LLR_GDBSERVER_GDB_PROCESS_THREADS_01}
 *    1. Call `gdb_partition_count()` with the argument `TRUE`, and store the
 *       result in `n`.
 * 3. Otherwise: \llrref{LLR_GDBSERVER_GDB_PROCESS_THREADS_02},
 *    \llrref{LLR_GDBSERVER_GDB_PROCESS_THREADS_03}
 *    1. Call `gdb_process_partition()` with the argument `pid`, and store the
 *       result in `part`.
 *    2. If `part` is not NULL:
 *       1. Call the function `part->part_sched_ops->get_number_of_threads` with
 *          the argument `part`, and store the result in `threads`.
 *       2. If `threads` is greater than `0`:
 *          1. Set `n` to `(uint32_t)threads`.
 *             \llrref{LLR_GDBSERVER_GDB_PROCESS_THREADS_02}
 * 4. Return `n`. \llrref{LLR_GDBSERVER_GDB_PROCESS_THREADS_01},
 *    \llrref{LLR_GDBSERVER_GDB_PROCESS_THREADS_02},
 *    \llrref{LLR_GDBSERVER_GDB_PROCESS_THREADS_03}
 *
 * @calls
 * - `gdb_partition_count()`
 * - `gdb_process_partition()`
 * - `part->part_sched_ops->get_number_of_threads()` (indirect)
 */
static uint32_t gdb_process_threads(uint32_t pid)
{
    uint32_t n = 0U;

    if (pid == GDB_PID_KERNEL)
    {
        n = gdb_partition_count(TRUE);
    }
    else
    {
        pok_partition_t* part = gdb_process_partition(pid);

        if (part != NULL)
        {
            int32_t threads =
                part->part_sched_ops->get_number_of_threads(part);

            if (threads > 0) {
                n = (uint32_t)threads;
            }
        }
    }

    return n;
}

/**
 * @funcname `gdb_thread_index`
 *
 * @description
 * Returns the index of a GDB thread in its partition (0 for a kernel thread).
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `t` | `const gdb_thread_t *` | [in] The thread. |
 *
 * @return The index.
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `index` | `uint32_t` | The index |
 *
 * @hlr
 * - \hlrid{HLR_GDBSERVER_GDB_THREAD_INDEX_01}: `gdb_thread_index()` shall give
 *   the index of a debugger thread in its partition, 0 for a thread of the
 *   kernel.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_DBG_002}
 *
 * @llr
 * - \llrid{LLR_GDBSERVER_GDB_THREAD_INDEX_01}: If `pid` of `t` is
 *   `GDB_PID_KERNEL`, `gdb_thread_index()` shall return 0.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDBSERVER_GDB_THREAD_INDEX_01}
 * - \llrid{LLR_GDBSERVER_GDB_THREAD_INDEX_02}: If `pid` of `t` is not
 *   `GDB_PID_KERNEL`, `gdb_thread_index()` shall return `tid` of `t` minus 1.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDBSERVER_GDB_THREAD_INDEX_01}
 *
 * @design
 * 1. Set `index` to `0U` if `t->pid` is equal to `GDB_PID_KERNEL`, otherwise
 *    `(t->tid - 1U)`. \llrref{LLR_GDBSERVER_GDB_THREAD_INDEX_01},
 *    \llrref{LLR_GDBSERVER_GDB_THREAD_INDEX_02}
 * 2. Return `(int32_t)index`. \llrref{LLR_GDBSERVER_GDB_THREAD_INDEX_01},
 *    \llrref{LLR_GDBSERVER_GDB_THREAD_INDEX_02}
 *
 * @calls
 * None.
 */
static int32_t gdb_thread_index(const gdb_thread_t* t)
{
    // A kernel-only partition has a single thread.
    uint32_t index = (t->pid == GDB_PID_KERNEL) ? 0U : (t->tid - 1U);

    return (int32_t)index;
}

/**
 * @funcname `gdb_thread_find`
 *
 * @description
 * Finds the GDB thread of a process and thread ID.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `t` | `gdb_thread_t *` | [out] Where the thread is stored. |
 * | `pid` | `uint32_t` | [in] The process ID. |
 * | `tid` | `uint32_t` | [in] The thread ID, from 1. |
 *
 * @return TRUE if it exists.
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `found` | `pok_bool_t` | Whether it exists |
 * | `index` | `int32_t` | Its index in the partition |
 * | `part` | `pok_partition_t *` | Its partition |
 * | `private` | `void *` | The partition's data of the thread |
 * | `thread_index` | `uint32_t` | The thread ID minus 1 |
 *
 * @hlr
 * - \hlrid{HLR_GDBSERVER_GDB_THREAD_FIND_01}: `gdb_thread_find()` shall find
 *   the thread that a debugger process and thread identifier name, and tell
 *   whether it exists.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_DBG_002}
 *
 * @llr
 * - \llrid{LLR_GDBSERVER_GDB_THREAD_FIND_01}: If `tid` is 0 or is greater than
 *   the result of `gdb_process_threads()` for `pid`, `gdb_thread_find()` shall
 *   return `FALSE` and leave the thread at `t` as it is.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDBSERVER_GDB_THREAD_FIND_01}
 * - \llrid{LLR_GDBSERVER_GDB_THREAD_FIND_02}: If `tid` is not 0 and is not
 *   greater than the result of `gdb_process_threads()` for `pid`, and `pid` is
 *   `GDB_PID_KERNEL`, `gdb_thread_find()` shall take as the partition the
 *   result of `gdb_partition_at()` for `TRUE` and `tid` minus 1, and 0 as the
 *   index.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDBSERVER_GDB_THREAD_FIND_01}
 * - \llrid{LLR_GDBSERVER_GDB_THREAD_FIND_03}: If `tid` is not 0 and is not
 *   greater than the result of `gdb_process_threads()` for `pid`, and `pid` is
 *   not `GDB_PID_KERNEL`, `gdb_thread_find()` shall take as the partition the
 *   result of `gdb_process_partition()` for `pid`, and `tid` minus 1 as the
 *   index.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDBSERVER_GDB_THREAD_FIND_01}
 * - \llrid{LLR_GDBSERVER_GDB_THREAD_FIND_04}: If the partition taken is not
 *   NULL and the function `get_thread_at_index` of its `part_sched_ops` returns
 *   0 for the partition, the index and the address of a private pointer,
 *   `gdb_thread_find()` shall set, at `t`, `pid` and `tid` to the arguments of
 *   the same names, `part` to the partition and `part_private` to the private
 *   pointer, and return `TRUE`.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDBSERVER_GDB_THREAD_FIND_01}
 * - \llrid{LLR_GDBSERVER_GDB_THREAD_FIND_05}: If the partition taken is NULL,
 *   or the function `get_thread_at_index` returns a value other than 0,
 *   `gdb_thread_find()` shall return `FALSE` and leave the thread at `t` as it
 *   is.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDBSERVER_GDB_THREAD_FIND_01}
 *
 * @design
 * 1. Set `found` to `FALSE`. \llrref{LLR_GDBSERVER_GDB_THREAD_FIND_01},
 *    \llrref{LLR_GDBSERVER_GDB_THREAD_FIND_05}
 * 2. Set `part` to `NULL`. \llrref{LLR_GDBSERVER_GDB_THREAD_FIND_02},
 *    \llrref{LLR_GDBSERVER_GDB_THREAD_FIND_03}
 * 3. Set `index` to `0`. \llrref{LLR_GDBSERVER_GDB_THREAD_FIND_02},
 *    \llrref{LLR_GDBSERVER_GDB_THREAD_FIND_03}
 * 4. If `tid` is equal to `0U` or `tid` is greater than
 *    `gdb_process_threads(pid)`: \llrref{LLR_GDBSERVER_GDB_THREAD_FIND_01}
 * 5. Otherwise, if `pid` is equal to `GDB_PID_KERNEL`:
 *    \llrref{LLR_GDBSERVER_GDB_THREAD_FIND_02}
 *    1. Call `gdb_partition_at()` with the arguments `TRUE` and `tid - 1U`, and
 *       store the result in `part`.
 * 6. Otherwise: \llrref{LLR_GDBSERVER_GDB_THREAD_FIND_03}
 *    1. Set `thread_index` to `tid - 1U`.
 *    2. Call `gdb_process_partition()` with the argument `pid`, and store the
 *       result in `part`.
 *    3. Set `index` to `(int32_t)thread_index`.
 * 7. If `part` is not NULL: \llrref{LLR_GDBSERVER_GDB_THREAD_FIND_04},
 *    \llrref{LLR_GDBSERVER_GDB_THREAD_FIND_05}
 *    1. Set `private` to `NULL`.
 *    2. If `part->part_sched_ops->get_thread_at_index(part, index, &private)`
 *       is equal to `0`:
 *       1. Set `t->pid` to `pid`. \llrref{LLR_GDBSERVER_GDB_THREAD_FIND_04}
 *       2. Set `t->tid` to `tid`. \llrref{LLR_GDBSERVER_GDB_THREAD_FIND_04}
 *       3. Set `t->part` to `part`. \llrref{LLR_GDBSERVER_GDB_THREAD_FIND_04}
 *       4. Set `t->part_private` to `private`.
 *          \llrref{LLR_GDBSERVER_GDB_THREAD_FIND_04}
 *       5. Set `found` to `TRUE`. \llrref{LLR_GDBSERVER_GDB_THREAD_FIND_04}
 * 8. Return `found`. \llrref{LLR_GDBSERVER_GDB_THREAD_FIND_01},
 *    \llrref{LLR_GDBSERVER_GDB_THREAD_FIND_04},
 *    \llrref{LLR_GDBSERVER_GDB_THREAD_FIND_05}
 *
 * @calls
 * - `gdb_process_threads()`
 * - `gdb_partition_at()`
 * - `gdb_process_partition()`
 * - `part->part_sched_ops->get_thread_at_index()` (indirect)
 */
static pok_bool_t gdb_thread_find(gdb_thread_t* t, uint32_t pid,
    uint32_t tid)
{
    pok_bool_t found = FALSE;
    pok_partition_t* part = NULL;
    int32_t index = 0;

    if ((tid == 0U) || (tid > gdb_process_threads(pid)))
    {
        // No such thread.
    }
    else if (pid == GDB_PID_KERNEL)
    {
        part = gdb_partition_at(TRUE, tid - 1U);
    }
    else
    {
        uint32_t thread_index = tid - 1U;

        part = gdb_process_partition(pid);
        index = (int32_t)thread_index;
    }

    if (part != NULL)
    {
        void* private = NULL;

        if (part->part_sched_ops->get_thread_at_index(part, index,
            &private) == 0)
        {
            t->pid = pid;
            t->tid = tid;
            t->part = part;
            t->part_private = private;
            found = TRUE;
        }
    }

    return found;
}

/**
 * @funcname `gdb_thread_equal`
 *
 * @description
 * Tells whether two GDB threads are the same.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `t1` | `const gdb_thread_t *` | [in] A thread. |
 * | `t2` | `const gdb_thread_t *` | [in] Another thread. |
 *
 * @return TRUE if their process and thread IDs are equal.
 *
 * @globals
 * None.
 *
 * @locals
 * None.
 *
 * @hlr
 * - \hlrid{HLR_GDBSERVER_GDB_THREAD_EQUAL_01}: `gdb_thread_equal()` shall tell
 *   whether two debugger threads are the same.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_DBG_002}
 *
 * @llr
 * - \llrid{LLR_GDBSERVER_GDB_THREAD_EQUAL_01}: If `pid` and `tid` of `t1` are
 *   equal to `pid` and `tid` of `t2`, `gdb_thread_equal()` shall return true.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDBSERVER_GDB_THREAD_EQUAL_01}
 * - \llrid{LLR_GDBSERVER_GDB_THREAD_EQUAL_02}: If `pid` of `t1` differs from
 *   `pid` of `t2`, or `tid` of `t1` differs from `tid` of `t2`,
 *   `gdb_thread_equal()` shall return false.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDBSERVER_GDB_THREAD_EQUAL_01}
 *
 * @design
 * 1. Return `(t1->pid == t2->pid) && (t1->tid == t2->tid)`.
 *    \llrref{LLR_GDBSERVER_GDB_THREAD_EQUAL_01},
 *    \llrref{LLR_GDBSERVER_GDB_THREAD_EQUAL_02}
 *
 * @calls
 * None.
 */
static pok_bool_t gdb_thread_equal(const gdb_thread_t* t1,
    const gdb_thread_t* t2)
{
    return (t1->pid == t2->pid) && (t1->tid == t2->tid);
}

/**
 * @funcname `gdb_process_current_tid`
 *
 * @description
 * Returns the current thread of a GDB process: the stopped thread for its
 * process, the partition's current thread otherwise (1 by default).
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `pid` | `uint32_t` | [in] The process ID. |
 *
 * @return The thread ID.
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `gdb_current` | `gdb_thread_t` | The thread that stopped the system |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `index` | `int32_t` | The partition's current thread index |
 * | `part` | `pok_partition_t *` | The partition |
 * | `tid` | `uint32_t` | The thread ID |
 *
 * @hlr
 * - \hlrid{HLR_GDBSERVER_GDB_PROCESS_CURRENT_TID_01}:
 *   `gdb_process_current_tid()` shall give the current thread of a debugger
 *   process: the stopped thread for its own process, the current thread of the
 *   partition otherwise, and 1 by default.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_DBG_002}
 *
 * @llr
 * - \llrid{LLR_GDBSERVER_GDB_PROCESS_CURRENT_TID_01}: If `pid` is
 *   `gdb_current.pid`, `gdb_process_current_tid()` shall return
 *   `gdb_current.tid`.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDBSERVER_GDB_PROCESS_CURRENT_TID_01}
 * - \llrid{LLR_GDBSERVER_GDB_PROCESS_CURRENT_TID_02}: If `pid` is not
 *   `gdb_current.pid` and is `GDB_PID_KERNEL`, `gdb_process_current_tid()`
 *   shall return 1.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDBSERVER_GDB_PROCESS_CURRENT_TID_01}
 * - \llrid{LLR_GDBSERVER_GDB_PROCESS_CURRENT_TID_03}: If `pid` is neither
 *   `gdb_current.pid` nor `GDB_PID_KERNEL`, `gdb_process_partition()` returns a
 *   partition for `pid`, and the function `get_current_thread_index` of its
 *   `part_sched_ops` returns a value that is not less than 0,
 *   `gdb_process_current_tid()` shall return that value plus 1.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDBSERVER_GDB_PROCESS_CURRENT_TID_01}
 * - \llrid{LLR_GDBSERVER_GDB_PROCESS_CURRENT_TID_04}: If `pid` is neither
 *   `gdb_current.pid` nor `GDB_PID_KERNEL`, and `gdb_process_partition()`
 *   returns NULL for `pid` or the function `get_current_thread_index` returns a
 *   value less than 0, `gdb_process_current_tid()` shall return 1.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDBSERVER_GDB_PROCESS_CURRENT_TID_01}
 *
 * @design
 * 1. Set `tid` to `1U`. \llrref{LLR_GDBSERVER_GDB_PROCESS_CURRENT_TID_02},
 *    \llrref{LLR_GDBSERVER_GDB_PROCESS_CURRENT_TID_04}
 * 2. If `pid` is equal to `gdb_current.pid`:
 *    \llrref{LLR_GDBSERVER_GDB_PROCESS_CURRENT_TID_01}
 *    1. Set `tid` to `gdb_current.tid`.
 * 3. Otherwise, if `pid` is not equal to `GDB_PID_KERNEL`:
 *    \llrref{LLR_GDBSERVER_GDB_PROCESS_CURRENT_TID_02},
 *    \llrref{LLR_GDBSERVER_GDB_PROCESS_CURRENT_TID_03},
 *    \llrref{LLR_GDBSERVER_GDB_PROCESS_CURRENT_TID_04}
 *    1. Call `gdb_process_partition()` with the argument `pid`, and store the
 *       result in `part`.
 *    2. If `part` is not NULL:
 *       1. Call the function `part->part_sched_ops->get_current_thread_index`
 *          with the argument `part`, and store the result in `index`.
 *       2. If `index` is greater than or equal to `0`:
 *          1. Set `tid` to `(uint32_t)index + 1U`.
 *             \llrref{LLR_GDBSERVER_GDB_PROCESS_CURRENT_TID_03}
 * 4. Return `tid`. \llrref{LLR_GDBSERVER_GDB_PROCESS_CURRENT_TID_01},
 *    \llrref{LLR_GDBSERVER_GDB_PROCESS_CURRENT_TID_02},
 *    \llrref{LLR_GDBSERVER_GDB_PROCESS_CURRENT_TID_03},
 *    \llrref{LLR_GDBSERVER_GDB_PROCESS_CURRENT_TID_04}
 *
 * @calls
 * - `gdb_process_partition()`
 * - `part->part_sched_ops->get_current_thread_index()` (indirect)
 */
static uint32_t gdb_process_current_tid(uint32_t pid)
{
    uint32_t tid = 1U;

    if (pid == gdb_current.pid)
    {
        tid = gdb_current.tid;
    }
    else if (pid != GDB_PID_KERNEL)
    {
        pok_partition_t* part = gdb_process_partition(pid);

        if (part != NULL)
        {
            int32_t index =
                part->part_sched_ops->get_current_thread_index(part);

            if (index >= 0) {
                tid = (uint32_t)index + 1U;
            }
        }
    }
    else
    {
        // Any thread of the kernel.
    }

    return tid;
}

/**
 * @funcname `gdb_thread_get_current`
 *
 * @description
 * Returns the GDB thread that runs in a context: the GDB partition stands for
 * the boot, the idle partition and the kernel's code run by a user partition;
 * otherwise the partition's current thread.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `t` | `gdb_thread_t *` | [out] Where the thread is stored. |
 * | `ctx` | `const interrupt_frame *` | [in] The context. |
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `current_partition` | `pok_partition_t *` | The running partition |
 * | `partition_gdb` | `pok_partition_t` | The GDB server partition |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `index` | `int32_t` | The partition's current thread index |
 * | `part` | `pok_partition_t *` | The partition that stands for the context |
 * | `pid` | `uint32_t` | The process ID |
 * | `tid` | `uint32_t` | The thread ID |
 *
 * @hlr
 * - \hlrid{HLR_GDBSERVER_GDB_THREAD_GET_CURRENT_01}: `gdb_thread_get_current()`
 *   shall give the debugger thread that runs in a stopped context: the GDB
 *   partition for the boot, the idle partition and the code of the kernel run
 *   by a user partition, and the partition's current thread otherwise.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_DBG_002}
 *
 * @llr
 * - \llrid{LLR_GDBSERVER_GDB_THREAD_GET_CURRENT_01}: If `current_partition` is
 *   NULL, or `gdb_partition_is_kernel()` returns true for it and
 *   `gdb_partition_index()` returns `GDB_NO_INDEX` for it,
 *   `gdb_thread_get_current()` shall take `partition_gdb` as the partition.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDBSERVER_GDB_THREAD_GET_CURRENT_01}
 * - \llrid{LLR_GDBSERVER_GDB_THREAD_GET_CURRENT_02}: If `current_partition` is
 *   not NULL, `gdb_partition_is_kernel()` returns false for it and
 *   `ja_gdb_context_is_user()` returns false for `ctx`,
 *   `gdb_thread_get_current()` shall take `partition_gdb` as the partition.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDBSERVER_GDB_THREAD_GET_CURRENT_01}
 * - \llrid{LLR_GDBSERVER_GDB_THREAD_GET_CURRENT_03}: If `current_partition` is
 *   not NULL, and `gdb_partition_is_kernel()` returns true for it while
 *   `gdb_partition_index()` gives an index other than `GDB_NO_INDEX`, or
 *   `gdb_partition_is_kernel()` returns false for it while
 *   `ja_gdb_context_is_user()` returns true for `ctx`,
 *   `gdb_thread_get_current()` shall take `current_partition` as the partition.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDBSERVER_GDB_THREAD_GET_CURRENT_01}
 * - \llrid{LLR_GDBSERVER_GDB_THREAD_GET_CURRENT_04}: If the result of
 *   `gdb_partition_pid()` for the partition is `GDB_PID_KERNEL`,
 *   `gdb_thread_get_current()` shall take as the thread ID the result of
 *   `gdb_partition_index()` for the partition plus 1.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDBSERVER_GDB_THREAD_GET_CURRENT_01}
 * - \llrid{LLR_GDBSERVER_GDB_THREAD_GET_CURRENT_05}: If the result of
 *   `gdb_partition_pid()` for the partition is not `GDB_PID_KERNEL`,
 *   `gdb_thread_get_current()` shall take as the thread ID the result of the
 *   function `get_current_thread_index` of `part_sched_ops` of the partition
 *   plus 1 if that result is not less than 0, and 1 if it is less than 0.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDBSERVER_GDB_THREAD_GET_CURRENT_01}
 * - \llrid{LLR_GDBSERVER_GDB_THREAD_GET_CURRENT_06}: If `gdb_thread_find()`
 *   returns false for `t`, that process ID and that thread ID,
 *   `gdb_thread_get_current()` shall set, at `t`, `pid` and `tid` to them,
 *   `part` to the partition and `part_private` to NULL.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDBSERVER_GDB_THREAD_GET_CURRENT_01}
 * - \llrid{LLR_GDBSERVER_GDB_THREAD_GET_CURRENT_07}: If `gdb_thread_find()`
 *   returns true for `t`, that process ID and that thread ID,
 *   `gdb_thread_get_current()` shall leave the thread that it stored at `t` as
 *   it is.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDBSERVER_GDB_THREAD_GET_CURRENT_01}
 *
 * @design
 * 1. Set `part` to `current_partition`.
 *    \llrref{LLR_GDBSERVER_GDB_THREAD_GET_CURRENT_01},
 *    \llrref{LLR_GDBSERVER_GDB_THREAD_GET_CURRENT_02},
 *    \llrref{LLR_GDBSERVER_GDB_THREAD_GET_CURRENT_03}
 * 2. If `part` is NULL or (`gdb_partition_is_kernel(part) &&
 *    (gdb_partition_index(part) == GDB_NO_INDEX)` holds):
 *    \llrref{LLR_GDBSERVER_GDB_THREAD_GET_CURRENT_01}
 *    1. Set `part` to the address of `partition_gdb`.
 * 3. Otherwise, if `gdb_partition_is_kernel(part)` is false and
 *    `ja_gdb_context_is_user(ctx)` is false:
 *    \llrref{LLR_GDBSERVER_GDB_THREAD_GET_CURRENT_02},
 *    \llrref{LLR_GDBSERVER_GDB_THREAD_GET_CURRENT_03}
 *    1. Set `part` to the address of `partition_gdb`.
 *       \llrref{LLR_GDBSERVER_GDB_THREAD_GET_CURRENT_02}
 * 4. Call `gdb_partition_pid()` with the argument `part`, and store the result
 *    in `pid`. \llrref{LLR_GDBSERVER_GDB_THREAD_GET_CURRENT_04},
 *    \llrref{LLR_GDBSERVER_GDB_THREAD_GET_CURRENT_05}
 * 5. If `pid` is equal to `GDB_PID_KERNEL`:
 *    \llrref{LLR_GDBSERVER_GDB_THREAD_GET_CURRENT_04}
 *    1. Set `tid` to `gdb_partition_index(part) + 1U`.
 * 6. Otherwise: \llrref{LLR_GDBSERVER_GDB_THREAD_GET_CURRENT_05}
 *    1. Call the function `part->part_sched_ops->get_current_thread_index` with
 *       the argument `part`, and store the result in `index`.
 *    2. Set `tid` to `((uint32_t)index + 1U)` if `index` is greater than or
 *       equal to `0`, otherwise `1U`.
 * 7. If `gdb_thread_find(t, pid, tid)` is false:
 *    \llrref{LLR_GDBSERVER_GDB_THREAD_GET_CURRENT_06},
 *    \llrref{LLR_GDBSERVER_GDB_THREAD_GET_CURRENT_07}
 *    1. Set `t->pid` to `pid`. \llrref{LLR_GDBSERVER_GDB_THREAD_GET_CURRENT_06}
 *    2. Set `t->tid` to `tid`. \llrref{LLR_GDBSERVER_GDB_THREAD_GET_CURRENT_06}
 *    3. Set `t->part` to `part`.
 *       \llrref{LLR_GDBSERVER_GDB_THREAD_GET_CURRENT_06}
 *    4. Set `t->part_private` to `NULL`.
 *       \llrref{LLR_GDBSERVER_GDB_THREAD_GET_CURRENT_06}
 *
 * @calls
 * - `gdb_partition_is_kernel()`
 * - `gdb_partition_index()`
 * - `ja_gdb_context_is_user()`
 * - `gdb_partition_pid()`
 * - `part->part_sched_ops->get_current_thread_index()` (indirect)
 * - `gdb_thread_find()`
 */
static void gdb_thread_get_current(gdb_thread_t* t,
    const interrupt_frame* ctx)
{
    pok_partition_t* part = current_partition;
    uint32_t pid;
    uint32_t tid;

    if ((part == NULL) || (gdb_partition_is_kernel(part)
        && (gdb_partition_index(part) == GDB_NO_INDEX)))
    {
        /*
         * Boot (POK_NEEDS_WAIT_FOR_GDB), or the idle partition, which GDB
         * doesn't see: the GDB partition stands for them.
         */
        part = &partition_gdb;
    }
    else if (!gdb_partition_is_kernel(part) && !ja_gdb_context_is_user(ctx))
    {
        /*
         * A user partition runs the kernel's code (a system call, an
         * interrupt). The program of the kernel, with its breakpoints and
         * watchpoints, is the one of process 1: GDB looks for them there
         * (by the program of the thread which has stopped), so the GDB
         * partition stands for the kernel here too. The partition's own
         * thread is shown where it has entered the kernel.
         */
        part = &partition_gdb;
    }
    else
    {
        // The partition that runs is the thread.
    }

    pid = gdb_partition_pid(part);
    if (pid == GDB_PID_KERNEL)
    {
        tid = gdb_partition_index(part) + 1U;
    }
    else
    {
        int32_t index = part->part_sched_ops->get_current_thread_index(part);

        tid = (index >= 0) ? ((uint32_t)index + 1U) : 1U;
    }

    if (!gdb_thread_find(t, pid, tid))
    {
        // Can't happen: the thread which runs exists.
        t->pid = pid;
        t->tid = tid;
        t->part = part;
        t->part_private = NULL;
    }
}

/**
 * @funcname `gdb_thread_resolve`
 *
 * @description
 * Finds the GDB thread that a thread reference of a packet names (-1 or 0 for
 * "any": the current process or thread).
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `t` | `gdb_thread_t *` | [out] Where the thread is stored. |
 * | `ref` | `const gdb_thread_ref_t *` | [in] The reference. |
 *
 * @return TRUE if it exists.
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `gdb_current` | `gdb_thread_t` | The thread that stopped the system |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `found` | `pok_bool_t` | Whether it exists |
 * | `pid` | `uint32_t` | The process ID |
 * | `tid` | `uint32_t` | The thread ID |
 *
 * @hlr
 * - \hlrid{HLR_GDBSERVER_GDB_THREAD_RESOLVE_01}: `gdb_thread_resolve()` shall
 *   find the thread that a thread reference of a packet names, -1 or 0 meaning
 *   the current process or thread, and tell whether it exists.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_DBG_002}
 *
 * @llr
 * - \llrid{LLR_GDBSERVER_GDB_THREAD_RESOLVE_01}: If `pid` of `ref` is greater
 *   than `GDB_PID_MAX`, `gdb_thread_resolve()` shall return `FALSE` without
 *   calling `gdb_thread_find()`.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDBSERVER_GDB_THREAD_RESOLVE_01}
 * - \llrid{LLR_GDBSERVER_GDB_THREAD_RESOLVE_02}: If `pid` of `ref` is greater
 *   than 0 and is not greater than `GDB_PID_MAX`, `gdb_thread_resolve()` shall
 *   take it as the process.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDBSERVER_GDB_THREAD_RESOLVE_01}
 * - \llrid{LLR_GDBSERVER_GDB_THREAD_RESOLVE_03}: If `pid` of `ref` is not
 *   greater than 0, `gdb_thread_resolve()` shall take `gdb_current.pid` as the
 *   process.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDBSERVER_GDB_THREAD_RESOLVE_01}
 * - \llrid{LLR_GDBSERVER_GDB_THREAD_RESOLVE_04}: If `tid` of `ref` is not
 *   greater than 0, `gdb_thread_resolve()` shall take as the thread the result
 *   of `gdb_process_current_tid()` for the process.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDBSERVER_GDB_THREAD_RESOLVE_01}
 * - \llrid{LLR_GDBSERVER_GDB_THREAD_RESOLVE_05}: If `tid` of `ref` is greater
 *   than 0 and is not greater than `GDB_NO_INDEX`, `gdb_thread_resolve()` shall
 *   take it as the thread.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDBSERVER_GDB_THREAD_RESOLVE_01}
 * - \llrid{LLR_GDBSERVER_GDB_THREAD_RESOLVE_06}: If `tid` of `ref` is greater
 *   than `GDB_NO_INDEX`, `gdb_thread_resolve()` shall take 0 as the thread.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDBSERVER_GDB_THREAD_RESOLVE_01}
 * - \llrid{LLR_GDBSERVER_GDB_THREAD_RESOLVE_07}: If `pid` of `ref` is not
 *   greater than `GDB_PID_MAX`, `gdb_thread_resolve()` shall return the result
 *   of `gdb_thread_find()` for `t`, the process and the thread.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDBSERVER_GDB_THREAD_RESOLVE_01}
 *
 * @design
 * 1. Set `found` to `FALSE`. \llrref{LLR_GDBSERVER_GDB_THREAD_RESOLVE_01}
 * 2. Set `pid` to `gdb_current.pid`.
 *    \llrref{LLR_GDBSERVER_GDB_THREAD_RESOLVE_03}
 * 3. If `ref->pid` is greater than `(int64_t)GDB_PID_MAX`:
 *    \llrref{LLR_GDBSERVER_GDB_THREAD_RESOLVE_01}
 * 4. Otherwise: \llrref{LLR_GDBSERVER_GDB_THREAD_RESOLVE_02},
 *    \llrref{LLR_GDBSERVER_GDB_THREAD_RESOLVE_03},
 *    \llrref{LLR_GDBSERVER_GDB_THREAD_RESOLVE_04},
 *    \llrref{LLR_GDBSERVER_GDB_THREAD_RESOLVE_05},
 *    \llrref{LLR_GDBSERVER_GDB_THREAD_RESOLVE_06},
 *    \llrref{LLR_GDBSERVER_GDB_THREAD_RESOLVE_07}
 *    1. If `ref->pid` is greater than `0`:
 *       \llrref{LLR_GDBSERVER_GDB_THREAD_RESOLVE_02},
 *       \llrref{LLR_GDBSERVER_GDB_THREAD_RESOLVE_03}
 *       1. Set `pid` to `(uint32_t)ref->pid`.
 *          \llrref{LLR_GDBSERVER_GDB_THREAD_RESOLVE_02}
 *    2. If `ref->tid` is less than or equal to `0`:
 *       \llrref{LLR_GDBSERVER_GDB_THREAD_RESOLVE_04}
 *       1. Call `gdb_process_current_tid()` with the argument `pid`, and store
 *          the result in `tid`.
 *    3. Otherwise, if `ref->tid` is less than or equal to
 *       `(int64_t)GDB_NO_INDEX`: \llrref{LLR_GDBSERVER_GDB_THREAD_RESOLVE_05}
 *       1. Set `tid` to `(uint32_t)ref->tid`.
 *    4. Otherwise: \llrref{LLR_GDBSERVER_GDB_THREAD_RESOLVE_06}
 *       1. Set `tid` to `0U`.
 *    5. Call `gdb_thread_find()` with the arguments `t`, `pid` and `tid`, and
 *       store the result in `found`.
 *       \llrref{LLR_GDBSERVER_GDB_THREAD_RESOLVE_07}
 * 5. Return `found`. \llrref{LLR_GDBSERVER_GDB_THREAD_RESOLVE_01},
 *    \llrref{LLR_GDBSERVER_GDB_THREAD_RESOLVE_07}
 *
 * @calls
 * - `gdb_process_current_tid()`
 * - `gdb_thread_find()`
 */
static pok_bool_t gdb_thread_resolve(gdb_thread_t* t,
    const gdb_thread_ref_t* ref)
{
    pok_bool_t found = FALSE;
    uint32_t pid = gdb_current.pid;

    if (ref->pid > (int64_t)GDB_PID_MAX)
    {
        // No such process.
    }
    else
    {
        uint32_t tid;

        if (ref->pid > 0) {
            pid = (uint32_t)ref->pid;
        }

        if (ref->tid <= 0) {
            tid = gdb_process_current_tid(pid);
        } else if (ref->tid <= (int64_t)GDB_NO_INDEX) {
            tid = (uint32_t)ref->tid;
        } else {
            tid = 0U; // No such thread.
        }

        found = gdb_thread_find(t, pid, tid);
    }

    return found;
}

/**
 * @funcname `gdb_thread_regs`
 *
 * @description
 * Returns the registers of a GDB thread: the stopped context for the stopped
 * thread, the saved contexts of the partitions and threads otherwise.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `t` | `const gdb_thread_t *` | [in] The thread. |
 *
 * @return The registers, or NULL.
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `current_partition` | `pok_partition_t *` | The running partition |
 * | `gdb_current` | `gdb_thread_t` | The thread that stopped the system |
 * | `gdb_stop_ctx` | `interrupt_frame *` | The registers of the stopped context |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `ctx` | `interrupt_frame *` | The registers |
 * | `part` | `pok_partition_t *` | The thread's partition |
 *
 * @hlr
 * - \hlrid{HLR_GDBSERVER_GDB_THREAD_REGS_01}: `gdb_thread_regs()` shall give
 *   the registers of a debugger thread: the stopped context for the stopped
 *   thread, and the contexts saved for the partitions and their threads
 *   otherwise.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_DBG_004}
 *
 * @llr
 * - \llrid{LLR_GDBSERVER_GDB_THREAD_REGS_01}: If `gdb_thread_equal()` returns
 *   true for `t` and `gdb_current`, `gdb_thread_regs()` shall return
 *   `gdb_stop_ctx`.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDBSERVER_GDB_THREAD_REGS_01}
 * - \llrid{LLR_GDBSERVER_GDB_THREAD_REGS_02}: If `gdb_thread_equal()` returns
 *   false and `pid` of `t` is `GDB_PID_KERNEL`, `gdb_thread_regs()` shall
 *   return `entry_sp` of the partition of `t`.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDBSERVER_GDB_THREAD_REGS_01}
 * - \llrid{LLR_GDBSERVER_GDB_THREAD_REGS_03}: If neither holds, the partition
 *   of `t` is `current_partition`, the result of `gdb_process_current_tid()`
 *   for `pid` of `t` is `tid` of `t`, and `entry_sp_user` of the partition is
 *   not NULL, `gdb_thread_regs()` shall return `entry_sp_user` of the
 *   partition.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDBSERVER_GDB_THREAD_REGS_01}
 * - \llrid{LLR_GDBSERVER_GDB_THREAD_REGS_04}: If neither holds, the partition
 *   of `t` is `current_partition`, the result of `gdb_process_current_tid()`
 *   for `pid` of `t` is `tid` of `t`, and `entry_sp_user` of the partition is
 *   NULL, `gdb_thread_regs()` shall return the result of the function
 *   `get_thread_registers` of `part_sched_ops` of the partition for the
 *   partition, the result of `gdb_thread_index()` for `t` and `part_private` of
 *   `t`.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDBSERVER_GDB_THREAD_REGS_01}
 * - \llrid{LLR_GDBSERVER_GDB_THREAD_REGS_05}: If neither holds, the partition
 *   of `t` is not `current_partition`, the result of
 *   `gdb_process_current_tid()` for `pid` of `t` is `tid` of `t`, `entry_sp` of
 *   the partition is not NULL, `ja_gdb_context_is_user()` returns false for it
 *   and `entry_sp_user` of the partition is not NULL, `gdb_thread_regs()` shall
 *   return `entry_sp_user` of the partition.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDBSERVER_GDB_THREAD_REGS_01}
 * - \llrid{LLR_GDBSERVER_GDB_THREAD_REGS_06}: If neither holds, the partition
 *   of `t` is not `current_partition`, the result of
 *   `gdb_process_current_tid()` for `pid` of `t` is `tid` of `t`, and
 *   `entry_sp` of the partition is NULL, or `ja_gdb_context_is_user()` returns
 *   true for it, or `entry_sp_user` of the partition is NULL,
 *   `gdb_thread_regs()` shall return `entry_sp` of the partition.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDBSERVER_GDB_THREAD_REGS_01}
 * - \llrid{LLR_GDBSERVER_GDB_THREAD_REGS_07}: If neither holds and the result
 *   of `gdb_process_current_tid()` for `pid` of `t` is not `tid` of `t`,
 *   `gdb_thread_regs()` shall return the result of the function
 *   `get_thread_registers` of `part_sched_ops` of the partition for the
 *   partition, the result of `gdb_thread_index()` for `t` and `part_private` of
 *   `t`.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDBSERVER_GDB_THREAD_REGS_01}
 *
 * @design
 * 1. Set `part` to `t->part`. \llrref{LLR_GDBSERVER_GDB_THREAD_REGS_02},
 *    \llrref{LLR_GDBSERVER_GDB_THREAD_REGS_03},
 *    \llrref{LLR_GDBSERVER_GDB_THREAD_REGS_04},
 *    \llrref{LLR_GDBSERVER_GDB_THREAD_REGS_05},
 *    \llrref{LLR_GDBSERVER_GDB_THREAD_REGS_06},
 *    \llrref{LLR_GDBSERVER_GDB_THREAD_REGS_07}
 * 2. If `gdb_thread_equal(t, &gdb_current)` is true:
 *    \llrref{LLR_GDBSERVER_GDB_THREAD_REGS_01}
 *    1. Set `ctx` to `gdb_stop_ctx`.
 * 3. Otherwise, if `t->pid` is equal to `GDB_PID_KERNEL`:
 *    \llrref{LLR_GDBSERVER_GDB_THREAD_REGS_02}
 *    1. Set `ctx` to `part->entry_sp`.
 * 4. Otherwise, if `part` is equal to `current_partition` and
 *    `gdb_process_current_tid(t->pid)` is equal to `t->tid`:
 *    \llrref{LLR_GDBSERVER_GDB_THREAD_REGS_03},
 *    \llrref{LLR_GDBSERVER_GDB_THREAD_REGS_04}
 *    1. Set `ctx` to `part->entry_sp_user`.
 *    2. If `ctx` is NULL:
 *       1. Call the function `part->part_sched_ops->get_thread_registers` with
 *          the arguments `part`, `gdb_thread_index(t)` and `t->part_private`,
 *          and store the result in `ctx`.
 *          \llrref{LLR_GDBSERVER_GDB_THREAD_REGS_04}
 * 5. Otherwise, if `part` is not equal to `current_partition` and
 *    `gdb_process_current_tid(t->pid)` is equal to `t->tid`:
 *    \llrref{LLR_GDBSERVER_GDB_THREAD_REGS_05},
 *    \llrref{LLR_GDBSERVER_GDB_THREAD_REGS_06}
 *    1. Set `ctx` to `part->entry_sp`.
 *    2. If `ctx` is not NULL and `ja_gdb_context_is_user(ctx)` is false and
 *       `part->entry_sp_user` is not NULL:
 *       1. Set `ctx` to `part->entry_sp_user`.
 *          \llrref{LLR_GDBSERVER_GDB_THREAD_REGS_05}
 * 6. Otherwise: \llrref{LLR_GDBSERVER_GDB_THREAD_REGS_07}
 *    1. Call the function `part->part_sched_ops->get_thread_registers` with the
 *       arguments `part`, `gdb_thread_index(t)` and `t->part_private`, and
 *       store the result in `ctx`.
 * 7. Return `ctx`. \llrref{LLR_GDBSERVER_GDB_THREAD_REGS_01},
 *    \llrref{LLR_GDBSERVER_GDB_THREAD_REGS_02},
 *    \llrref{LLR_GDBSERVER_GDB_THREAD_REGS_03},
 *    \llrref{LLR_GDBSERVER_GDB_THREAD_REGS_04},
 *    \llrref{LLR_GDBSERVER_GDB_THREAD_REGS_05},
 *    \llrref{LLR_GDBSERVER_GDB_THREAD_REGS_06},
 *    \llrref{LLR_GDBSERVER_GDB_THREAD_REGS_07}
 *
 * @calls
 * - `gdb_thread_equal()`
 * - `gdb_process_current_tid()`
 * - `part->part_sched_ops->get_thread_registers()` (indirect)
 * - `gdb_thread_index()`
 * - `ja_gdb_context_is_user()`
 */
static interrupt_frame* gdb_thread_regs(
    const gdb_thread_t* t)
{
    interrupt_frame* ctx;
    pok_partition_t* part = t->part;

    if (gdb_thread_equal(t, &gdb_current))
    {
        ctx = gdb_stop_ctx;
    }
    else if (t->pid == GDB_PID_KERNEL)
    {
        // Registers saved when the partition was switched off.
        ctx = part->entry_sp;
    }
    else if ((part == current_partition)
        && (gdb_process_current_tid(t->pid) == t->tid))
    {
        /*
         * Current thread of the partition which runs, in the kernel (see
         * gdb_thread_get_current()): where it has entered the kernel, or
         * where it last did if that isn't recorded yet.
         */
        ctx = part->entry_sp_user;
        if (ctx == NULL)
        {
            ctx = part->part_sched_ops->get_thread_registers(part,
                gdb_thread_index(t), t->part_private);
        }
    }
    else if ((part != current_partition)
        && (gdb_process_current_tid(t->pid) == t->tid))
    {
        /*
         * Current thread of a partition which isn't running: registers
         * saved when the partition was switched off. If it was in the
         * kernel then, show where it has left user space.
         */
        ctx = part->entry_sp;
        if ((ctx != NULL) && !ja_gdb_context_is_user(ctx)
            && (part->entry_sp_user != NULL))
        {
            ctx = part->entry_sp_user;
        }
    }
    else
    {
        ctx = part->part_sched_ops->get_thread_registers(part,
            gdb_thread_index(t), t->part_private);
    }

    return ctx;
}

/**
 * @funcname `gdb_thread_fp`
 *
 * @description
 * Returns the FP store of a user thread, if the partition has one.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `t` | `const gdb_thread_t *` | [in] The thread. |
 *
 * @return The FP store, or NULL.
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `fp` | `jet_fp_store_t *` | The FP store |
 * | `ops` | `const pok_part_sched_ops_t *` | The partition's scheduler operations |
 *
 * @hlr
 * - \hlrid{HLR_GDBSERVER_GDB_THREAD_FP_01}: `gdb_thread_fp()` shall give the
 *   floating point store of a user thread, if its partition keeps one.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_DBG_004}
 *
 * @llr
 * - \llrid{LLR_GDBSERVER_GDB_THREAD_FP_01}: If `pid` of `t` is not
 *   `GDB_PID_KERNEL` and `get_thread_fp_store` of `part_sched_ops` of the
 *   partition of `t` is not NULL, `gdb_thread_fp()` shall return the result of
 *   that function for the partition, the result of `gdb_thread_index()` for `t`
 *   and `part_private` of `t`.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDBSERVER_GDB_THREAD_FP_01}
 * - \llrid{LLR_GDBSERVER_GDB_THREAD_FP_02}: If `pid` of `t` is `GDB_PID_KERNEL`
 *   or `get_thread_fp_store` of `part_sched_ops` of the partition of `t` is
 *   NULL, `gdb_thread_fp()` shall return NULL.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDBSERVER_GDB_THREAD_FP_01}
 *
 * @design
 * 1. Set `fp` to `NULL`. \llrref{LLR_GDBSERVER_GDB_THREAD_FP_02}
 * 2. Set `ops` to `t->part->part_sched_ops`.
 *    \llrref{LLR_GDBSERVER_GDB_THREAD_FP_01},
 *    \llrref{LLR_GDBSERVER_GDB_THREAD_FP_02}
 * 3. If `t->pid` is not equal to `GDB_PID_KERNEL` and
 *    `ops->get_thread_fp_store` is not NULL:
 *    \llrref{LLR_GDBSERVER_GDB_THREAD_FP_01},
 *    \llrref{LLR_GDBSERVER_GDB_THREAD_FP_02}
 *    1. Call the function `ops->get_thread_fp_store` with the arguments
 *       `t->part`, `gdb_thread_index(t)` and `t->part_private`, and store the
 *       result in `fp`. \llrref{LLR_GDBSERVER_GDB_THREAD_FP_01}
 * 4. Return `fp`. \llrref{LLR_GDBSERVER_GDB_THREAD_FP_01},
 *    \llrref{LLR_GDBSERVER_GDB_THREAD_FP_02}
 *
 * @calls
 * - `ops->get_thread_fp_store()` (indirect)
 * - `gdb_thread_index()`
 */
static jet_fp_store_t* gdb_thread_fp(const gdb_thread_t* t)
{
    jet_fp_store_t* fp = NULL;
    const pok_part_sched_ops_t* ops =
        t->part->part_sched_ops;

    if ((t->pid != GDB_PID_KERNEL) && (ops->get_thread_fp_store != NULL))
    {
        fp = ops->get_thread_fp_store(t->part, gdb_thread_index(t),
            t->part_private);
    }

    return fp;
}

/***********************************************************************
 * Processes GDB is attached to.
 */

/**
 * @funcname `gdb_is_attached`
 *
 * @description
 * Tells whether GDB is attached to a process.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `pid` | `uint32_t` | [in] The process ID. |
 *
 * @return TRUE if attached.
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `gdb_attached` | `pok_bool_t[257]` | Which GDB processes are attached |
 *
 * @locals
 * None.
 *
 * @hlr
 * - \hlrid{HLR_GDBSERVER_GDB_IS_ATTACHED_01}: `gdb_is_attached()` shall tell
 *   whether the debugger is attached to a process.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_DBG_007}
 *
 * @llr
 * - \llrid{LLR_GDBSERVER_GDB_IS_ATTACHED_01}: If `pid` is not greater than
 *   `GDB_PID_MAX` and `gdb_attached[pid]` is true, `gdb_is_attached()` shall
 *   return true.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDBSERVER_GDB_IS_ATTACHED_01}
 * - \llrid{LLR_GDBSERVER_GDB_IS_ATTACHED_02}: If `pid` is greater than
 *   `GDB_PID_MAX`, or `gdb_attached[pid]` is false, `gdb_is_attached()` shall
 *   return false.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDBSERVER_GDB_IS_ATTACHED_01}
 *
 * @design
 * 1. Return `(pid <= GDB_PID_MAX) && gdb_attached[pid]`.
 *    \llrref{LLR_GDBSERVER_GDB_IS_ATTACHED_01},
 *    \llrref{LLR_GDBSERVER_GDB_IS_ATTACHED_02}
 *
 * @calls
 * None.
 */
static pok_bool_t gdb_is_attached(uint32_t pid)
{
    return (pid <= GDB_PID_MAX) && gdb_attached[pid];
}

/**
 * @funcname `gdb_attach`
 *
 * @description
 * Attaches GDB to a process.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `pid` | `uint32_t` | [in] The process ID. |
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `gdb_attached` | `pok_bool_t[257]` | Which GDB processes are attached |
 *
 * @locals
 * None.
 *
 * @hlr
 * - \hlrid{HLR_GDBSERVER_GDB_ATTACH_01}: `gdb_attach()` shall attach the
 *   debugger to a process.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_DBG_007}
 *
 * @llr
 * - \llrid{LLR_GDBSERVER_GDB_ATTACH_01}: If and only if `pid` is not greater
 *   than `GDB_PID_MAX`, `gdb_attach()` shall set `gdb_attached[pid]` to `TRUE`.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDBSERVER_GDB_ATTACH_01}
 *
 * @design
 * 1. If `pid` is less than or equal to `GDB_PID_MAX`:
 *    \llrref{LLR_GDBSERVER_GDB_ATTACH_01}
 *    1. Set `gdb_attached[pid]` to `TRUE`.
 *
 * @calls
 * None.
 */
static void gdb_attach(uint32_t pid)
{
    if (pid <= GDB_PID_MAX) {
        gdb_attached[pid] = TRUE;
    }
}

/**
 * @funcname `gdb_detach_all`
 *
 * @description
 * Detaches GDB from every process.
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `gdb_attached` | `pok_bool_t[257]` | Which GDB processes are attached |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `pid` | `uint32_t` | The process ID |
 *
 * @hlr
 * - \hlrid{HLR_GDBSERVER_GDB_DETACH_ALL_01}: `gdb_detach_all()` shall detach
 *   the debugger from every process.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_DBG_007}
 *   - \srqref{SRQ_JET_DBG_015}
 *
 * @llr
 * - \llrid{LLR_GDBSERVER_GDB_DETACH_ALL_01}: `gdb_detach_all()` shall set each
 *   of the elements 0 to `GDB_PID_MAX` of `gdb_attached` to `FALSE`.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDBSERVER_GDB_DETACH_ALL_01}
 *
 * @design
 * 1. Set `pid` to `0U`; then while `pid` is less than or equal to
 *    `GDB_PID_MAX`, repeat (after each pass: increment `pid`):
 *    \llrref{LLR_GDBSERVER_GDB_DETACH_ALL_01}
 *    1. Set `gdb_attached[pid]` to `FALSE`.
 *
 * @calls
 * None.
 */
static void gdb_detach_all(void)
{
    for (uint32_t pid = 0U; pid <= GDB_PID_MAX; pid++)
    {
        gdb_attached[pid] = FALSE;
    }
}

/**
 * @funcname `gdb_attached_none`
 *
 * @description
 * Tells whether GDB is attached to no process.
 *
 * @return TRUE if none is attached.
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `gdb_attached` | `pok_bool_t[257]` | Which GDB processes are attached |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `none` | `pok_bool_t` | Whether none is attached |
 * | `pid` | `uint32_t` | The process ID |
 *
 * @hlr
 * - \hlrid{HLR_GDBSERVER_GDB_ATTACHED_NONE_01}: `gdb_attached_none()` shall
 *   tell whether the debugger is attached to no process.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_DBG_007}
 *   - \srqref{SRQ_JET_DBG_015}
 *
 * @llr
 * - \llrid{LLR_GDBSERVER_GDB_ATTACHED_NONE_01}: If one of the elements 0 to
 *   `GDB_PID_MAX` of `gdb_attached` is true, `gdb_attached_none()` shall return
 *   `FALSE`.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDBSERVER_GDB_ATTACHED_NONE_01}
 * - \llrid{LLR_GDBSERVER_GDB_ATTACHED_NONE_02}: If all the elements 0 to
 *   `GDB_PID_MAX` of `gdb_attached` are false, `gdb_attached_none()` shall
 *   return `TRUE`.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDBSERVER_GDB_ATTACHED_NONE_01}
 *
 * @design
 * 1. Set `none` to `TRUE`. \llrref{LLR_GDBSERVER_GDB_ATTACHED_NONE_01},
 *    \llrref{LLR_GDBSERVER_GDB_ATTACHED_NONE_02}
 * 2. Set `pid` to `0U`; then while `pid` is less than or equal to
 *    `GDB_PID_MAX`, repeat (after each pass: increment `pid`):
 *    \llrref{LLR_GDBSERVER_GDB_ATTACHED_NONE_01},
 *    \llrref{LLR_GDBSERVER_GDB_ATTACHED_NONE_02}
 *    1. If `gdb_attached[pid]` is true:
 *       1. Set `none` to `FALSE`.
 * 3. Return `none`. \llrref{LLR_GDBSERVER_GDB_ATTACHED_NONE_01},
 *    \llrref{LLR_GDBSERVER_GDB_ATTACHED_NONE_02}
 *
 * @calls
 * None.
 */
static pok_bool_t gdb_attached_none(void)
{
    pok_bool_t none = TRUE;

    for (uint32_t pid = 0U; pid <= GDB_PID_MAX; pid++)
    {
        if (gdb_attached[pid]) {
            none = FALSE;
        }
    }

    return none;
}

/**
 * @funcname `gdb_process_next`
 *
 * @description
 * Returns the next GDB process in the order of the thread list (the user
 * partitions, then the kernel's process 1, then 0 for the end).
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `pid` | `uint32_t` | [in] The process ID (0: the start). |
 *
 * @return The next process ID, or 0.
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `last` | `uint32_t` | The last process ID |
 * | `next` | `uint32_t` | The next process ID |
 *
 * @hlr
 * - \hlrid{HLR_GDBSERVER_GDB_PROCESS_NEXT_01}: `gdb_process_next()` shall give
 *   the next debugger process in the order of the thread list: the user
 *   partitions, then process 1, then the end.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_DBG_002}
 *
 * @llr
 * - \llrid{LLR_GDBSERVER_GDB_PROCESS_NEXT_01}: If `pid` is 0 and the result of
 *   `gdb_process_last()` is 2 or more, `gdb_process_next()` shall return 2.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDBSERVER_GDB_PROCESS_NEXT_01}
 * - \llrid{LLR_GDBSERVER_GDB_PROCESS_NEXT_02}: If `pid` is 0 and the result of
 *   `gdb_process_last()` is less than 2, `gdb_process_next()` shall return
 *   `GDB_PID_KERNEL`.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDBSERVER_GDB_PROCESS_NEXT_01}
 * - \llrid{LLR_GDBSERVER_GDB_PROCESS_NEXT_03}: If `pid` is `GDB_PID_KERNEL`,
 *   `gdb_process_next()` shall return 0.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDBSERVER_GDB_PROCESS_NEXT_01}
 * - \llrid{LLR_GDBSERVER_GDB_PROCESS_NEXT_04}: If `pid` is neither 0 nor
 *   `GDB_PID_KERNEL` and is less than the result of `gdb_process_last()`,
 *   `gdb_process_next()` shall return `pid` plus 1.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDBSERVER_GDB_PROCESS_NEXT_01}
 * - \llrid{LLR_GDBSERVER_GDB_PROCESS_NEXT_05}: If `pid` is neither 0 nor
 *   `GDB_PID_KERNEL` and is not less than the result of `gdb_process_last()`,
 *   `gdb_process_next()` shall return `GDB_PID_KERNEL`.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDBSERVER_GDB_PROCESS_NEXT_01}
 *
 * @design
 * 1. Call `gdb_process_last()`, and store the result in `last`.
 *    \llrref{LLR_GDBSERVER_GDB_PROCESS_NEXT_01},
 *    \llrref{LLR_GDBSERVER_GDB_PROCESS_NEXT_02},
 *    \llrref{LLR_GDBSERVER_GDB_PROCESS_NEXT_04},
 *    \llrref{LLR_GDBSERVER_GDB_PROCESS_NEXT_05}
 * 2. If `pid` is equal to `0U`: \llrref{LLR_GDBSERVER_GDB_PROCESS_NEXT_01},
 *    \llrref{LLR_GDBSERVER_GDB_PROCESS_NEXT_02}
 *    1. Set `next` to `2U` if `last` is greater than or equal to `2U`,
 *       otherwise `GDB_PID_KERNEL`.
 * 3. Otherwise, if `pid` is equal to `GDB_PID_KERNEL`:
 *    \llrref{LLR_GDBSERVER_GDB_PROCESS_NEXT_03}
 *    1. Set `next` to `0U`.
 * 4. Otherwise, if `pid` is less than `last`:
 *    \llrref{LLR_GDBSERVER_GDB_PROCESS_NEXT_04}
 *    1. Set `next` to `pid + 1U`.
 * 5. Otherwise: \llrref{LLR_GDBSERVER_GDB_PROCESS_NEXT_05}
 *    1. Set `next` to `GDB_PID_KERNEL`.
 * 6. Return `next`. \llrref{LLR_GDBSERVER_GDB_PROCESS_NEXT_01},
 *    \llrref{LLR_GDBSERVER_GDB_PROCESS_NEXT_02},
 *    \llrref{LLR_GDBSERVER_GDB_PROCESS_NEXT_03},
 *    \llrref{LLR_GDBSERVER_GDB_PROCESS_NEXT_04},
 *    \llrref{LLR_GDBSERVER_GDB_PROCESS_NEXT_05}
 *
 * @calls
 * - `gdb_process_last()`
 */
static uint32_t gdb_process_next(uint32_t pid)
{
    uint32_t last = gdb_process_last();
    uint32_t next;

    if (pid == 0U) {
        next = (last >= 2U) ? 2U : GDB_PID_KERNEL;
    } else if (pid == GDB_PID_KERNEL) {
        next = 0U;
    } else if (pid < last) {
        next = pid + 1U;
    } else {
        next = GDB_PID_KERNEL;
    }

    return next;
}

/**
 * @funcname `gdb_list_next`
 *
 * @description
 * Moves the thread list to the next thread of an attached process.
 *
 * @return TRUE if there is one.
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `gdb_list_pid` | `uint32_t` | The process of the thread list being sent |
 * | `gdb_list_tid` | `uint32_t` | The thread of the thread list being sent |
 *
 * @locals
 * None.
 *
 * @hlr
 * - \hlrid{HLR_GDBSERVER_GDB_LIST_NEXT_01}: `gdb_list_next()` shall move the
 *   list of threads to the next thread of an attached process, and tell whether
 *   there is one.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_DBG_002}
 *   - \srqref{SRQ_JET_DBG_007}
 *
 * @llr
 * - \llrid{LLR_GDBSERVER_GDB_LIST_NEXT_01}: `gdb_list_next()` shall increment
 *   `gdb_list_tid` and then, while `gdb_list_pid` is not 0 and either
 *   `gdb_is_attached()` returns false for it or `gdb_list_tid` is greater than
 *   the result of `gdb_process_threads()` for it, set `gdb_list_pid` to the
 *   result of `gdb_process_next()` for it and `gdb_list_tid` to 1.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDBSERVER_GDB_LIST_NEXT_01}
 * - \llrid{LLR_GDBSERVER_GDB_LIST_NEXT_02}: If `gdb_list_pid` is then not 0,
 *   `gdb_list_next()` shall return true.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDBSERVER_GDB_LIST_NEXT_01}
 * - \llrid{LLR_GDBSERVER_GDB_LIST_NEXT_03}: If `gdb_list_pid` is then 0,
 *   `gdb_list_next()` shall return false.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDBSERVER_GDB_LIST_NEXT_01}
 *
 * @design
 * 1. Increment `gdb_list_tid`. \llrref{LLR_GDBSERVER_GDB_LIST_NEXT_01}
 * 2. While `gdb_list_pid` is not equal to `0U` and
 *    (`gdb_is_attached(gdb_list_pid)` is false or `gdb_list_tid` is greater
 *    than `gdb_process_threads(gdb_list_pid)`):
 *    \llrref{LLR_GDBSERVER_GDB_LIST_NEXT_01}
 *    1. Call `gdb_process_next()` with the argument `gdb_list_pid`, and store
 *       the result in `gdb_list_pid`.
 *    2. Set `gdb_list_tid` to `1U`.
 * 3. Return `gdb_list_pid != 0U`. \llrref{LLR_GDBSERVER_GDB_LIST_NEXT_02},
 *    \llrref{LLR_GDBSERVER_GDB_LIST_NEXT_03}
 *
 * @calls
 * - `gdb_is_attached()`
 * - `gdb_process_threads()`
 * - `gdb_process_next()`
 */
static pok_bool_t gdb_list_next(void)
{
    gdb_list_tid++;

    while ((gdb_list_pid != 0U) && (!gdb_is_attached(gdb_list_pid)
        || (gdb_list_tid > gdb_process_threads(gdb_list_pid))))
    {
        gdb_list_pid = gdb_process_next(gdb_list_pid);
        gdb_list_tid = 1U;
    }

    return gdb_list_pid != 0U;
}

/***********************************************************************
 * Software breakpoints.
 */

/**
 * @funcname `gdb_sw_find`
 *
 * @description
 * Finds a software breakpoint at an address of a memory space.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `space_id` | `jet_space_id` | [in] The memory space. |
 * | `addr` | `uintptr_t` | [in] The address. |
 *
 * @return The breakpoint, or NULL.
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `gdb_sw_breakpoints` | `gdb_sw_breakpoint_t[64]` | The software breakpoints |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `bp` | `gdb_sw_breakpoint_t *` | The breakpoint found |
 * | `i` | `uint32_t` | The index of a breakpoint |
 *
 * @hlr
 * - \hlrid{HLR_GDBSERVER_GDB_SW_FIND_01}: `gdb_sw_find()` shall find the
 *   software breakpoint at an address of a memory space.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_DBG_005}
 *   - \srqref{SRQ_JET_DBG_014}
 *
 * @llr
 * - \llrid{LLR_GDBSERVER_GDB_SW_FIND_01}: If one of the `GDB_SW_BREAKPOINTS`
 *   elements of `gdb_sw_breakpoints` has `used` true, `space_id` equal to
 *   `space_id` and `addr` equal to `addr`, `gdb_sw_find()` shall return the
 *   address of the last such element.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDBSERVER_GDB_SW_FIND_01}
 * - \llrid{LLR_GDBSERVER_GDB_SW_FIND_02}: If every element of
 *   `gdb_sw_breakpoints` has `used` false, or a `space_id` other than
 *   `space_id`, or an `addr` other than `addr`, `gdb_sw_find()` shall return
 *   NULL.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDBSERVER_GDB_SW_FIND_01}
 *
 * @design
 * 1. Set `bp` to `NULL`. \llrref{LLR_GDBSERVER_GDB_SW_FIND_01},
 *    \llrref{LLR_GDBSERVER_GDB_SW_FIND_02}
 * 2. Set `i` to `0U`; then while `i` is less than `GDB_SW_BREAKPOINTS`, repeat
 *    (after each pass: increment `i`): \llrref{LLR_GDBSERVER_GDB_SW_FIND_01},
 *    \llrref{LLR_GDBSERVER_GDB_SW_FIND_02}
 *    1. If `gdb_sw_breakpoints[i].used` is true and
 *       `gdb_sw_breakpoints[i].space_id` is equal to `space_id` and
 *       `gdb_sw_breakpoints[i].addr` is equal to `addr`:
 *       \llrref{LLR_GDBSERVER_GDB_SW_FIND_01}
 *       1. Set `bp` to the address of `gdb_sw_breakpoints[i]`.
 * 3. Return `bp`. \llrref{LLR_GDBSERVER_GDB_SW_FIND_01},
 *    \llrref{LLR_GDBSERVER_GDB_SW_FIND_02}
 *
 * @calls
 * None.
 */
static gdb_sw_breakpoint_t* gdb_sw_find(jet_space_id space_id,
    uintptr_t addr)
{
    gdb_sw_breakpoint_t* bp = NULL;

    for (uint32_t i = 0U; i < GDB_SW_BREAKPOINTS; i++)
    {
        if (gdb_sw_breakpoints[i].used
            && (gdb_sw_breakpoints[i].space_id == space_id)
            && (gdb_sw_breakpoints[i].addr == addr))
        {
            bp = &gdb_sw_breakpoints[i];
        }
    }

    return bp;
}

/**
 * @funcname `gdb_sw_write`
 *
 * @description
 * Writes a software breakpoint into the code, saving the code under it.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `bp` | `gdb_sw_breakpoint_t *` | [in,out] The breakpoint. |
 *
 * @return TRUE if the code could be written.
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `ja_gdb_breakpoint_insn` | `const uint8_t[1]` | The breakpoint instruction of the architecture |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `code` | `uint8_t *` | The code, at its kernel address |
 * | `i` | `uint32_t` | The index of a byte |
 * | `written` | `pok_bool_t` | Whether it was written |
 *
 * @hlr
 * - \hlrid{HLR_GDBSERVER_GDB_SW_WRITE_01}: `gdb_sw_write()` shall write a
 *   software breakpoint into the code of a memory space, keeping the code under
 *   it, and tell whether it could.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_DBG_005}
 *
 * @llr
 * - \llrid{LLR_GDBSERVER_GDB_SW_WRITE_01}: If `ja_addr_to_gdb()` returns an
 *   address for `addr` of `bp`, `JA_GDB_BREAKPOINT_SIZE` and `space_id` of
 *   `bp`, `gdb_sw_write()` shall copy the `JA_GDB_BREAKPOINT_SIZE` bytes there
 *   to `saved` of `bp`, replace them with the bytes of
 *   `ja_gdb_breakpoint_insn`, call `ja_gdb_sync_icache()` with the address and
 *   `JA_GDB_BREAKPOINT_SIZE`, and return `TRUE`.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDBSERVER_GDB_SW_WRITE_01}
 * - \llrid{LLR_GDBSERVER_GDB_SW_WRITE_02}: If `ja_addr_to_gdb()` returns NULL
 *   for `addr` of `bp`, `JA_GDB_BREAKPOINT_SIZE` and `space_id` of `bp`,
 *   `gdb_sw_write()` shall return `FALSE` and leave `bp` as it is.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDBSERVER_GDB_SW_WRITE_01}
 *
 * @design
 * 1. Call `ja_addr_to_gdb()` with the arguments `bp->addr`,
 *    `JA_GDB_BREAKPOINT_SIZE` and `bp->space_id`, and store the result in
 *    `code`. \llrref{LLR_GDBSERVER_GDB_SW_WRITE_01},
 *    \llrref{LLR_GDBSERVER_GDB_SW_WRITE_02}
 * 2. Set `written` to `FALSE`. \llrref{LLR_GDBSERVER_GDB_SW_WRITE_02}
 * 3. If `code` is not NULL: \llrref{LLR_GDBSERVER_GDB_SW_WRITE_01},
 *    \llrref{LLR_GDBSERVER_GDB_SW_WRITE_02}
 *    1. Set `i` to `0U`; then while `i` is less than `JA_GDB_BREAKPOINT_SIZE`,
 *       repeat (after each pass: increment `i`):
 *       \llrref{LLR_GDBSERVER_GDB_SW_WRITE_01}
 *       1. Set `bp->saved[i]` to `code[i]`.
 *       2. Set `code[i]` to `ja_gdb_breakpoint_insn[i]`.
 *    2. Call `ja_gdb_sync_icache()` with the arguments `code` and
 *       `JA_GDB_BREAKPOINT_SIZE`. \llrref{LLR_GDBSERVER_GDB_SW_WRITE_01}
 *    3. Set `written` to `TRUE`. \llrref{LLR_GDBSERVER_GDB_SW_WRITE_01}
 * 4. Return `written`. \llrref{LLR_GDBSERVER_GDB_SW_WRITE_01},
 *    \llrref{LLR_GDBSERVER_GDB_SW_WRITE_02}
 *
 * @calls
 * - `ja_addr_to_gdb()`
 * - `ja_gdb_sync_icache()`
 */
static pok_bool_t gdb_sw_write(gdb_sw_breakpoint_t* bp)
{
    uint8_t* code = ja_addr_to_gdb(bp->addr, JA_GDB_BREAKPOINT_SIZE,
        bp->space_id);
    pok_bool_t written = FALSE;

    if (code != NULL)
    {
        for (uint32_t i = 0U; i < JA_GDB_BREAKPOINT_SIZE; i++)
        {
            bp->saved[i] = code[i];
            code[i] = ja_gdb_breakpoint_insn[i];
        }
        ja_gdb_sync_icache(code, JA_GDB_BREAKPOINT_SIZE);
        written = TRUE;
    }

    return written;
}

/**
 * @funcname `gdb_sw_restore`
 *
 * @description
 * Puts back the code under a software breakpoint.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `bp` | `const gdb_sw_breakpoint_t *` | [in] The breakpoint. |
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `code` | `uint8_t *` | The code, at its kernel address |
 * | `i` | `uint32_t` | The index of a byte |
 *
 * @hlr
 * - \hlrid{HLR_GDBSERVER_GDB_SW_RESTORE_01}: `gdb_sw_restore()` shall put back
 *   the code under a software breakpoint.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_DBG_005}
 *
 * @llr
 * - \llrid{LLR_GDBSERVER_GDB_SW_RESTORE_01}: If and only if `ja_addr_to_gdb()`
 *   returns an address for `addr` of `bp`, `JA_GDB_BREAKPOINT_SIZE` and
 *   `space_id` of `bp`, `gdb_sw_restore()` shall copy the
 *   `JA_GDB_BREAKPOINT_SIZE` bytes of `saved` of `bp` there and call
 *   `ja_gdb_sync_icache()` with the address and `JA_GDB_BREAKPOINT_SIZE`.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDBSERVER_GDB_SW_RESTORE_01}
 *
 * @design
 * 1. Call `ja_addr_to_gdb()` with the arguments `bp->addr`,
 *    `JA_GDB_BREAKPOINT_SIZE` and `bp->space_id`, and store the result in
 *    `code`. \llrref{LLR_GDBSERVER_GDB_SW_RESTORE_01}
 * 2. If `code` is not NULL: \llrref{LLR_GDBSERVER_GDB_SW_RESTORE_01}
 *    1. Set `i` to `0U`; then while `i` is less than `JA_GDB_BREAKPOINT_SIZE`,
 *       repeat (after each pass: increment `i`):
 *       1. Set `code[i]` to `bp->saved[i]`.
 *    2. Call `ja_gdb_sync_icache()` with the arguments `code` and
 *       `JA_GDB_BREAKPOINT_SIZE`.
 *
 * @calls
 * - `ja_addr_to_gdb()`
 * - `ja_gdb_sync_icache()`
 */
static void gdb_sw_restore(const gdb_sw_breakpoint_t* bp)
{
    uint8_t* code = ja_addr_to_gdb(bp->addr, JA_GDB_BREAKPOINT_SIZE,
        bp->space_id);

    if (code != NULL)
    {
        for (uint32_t i = 0U; i < JA_GDB_BREAKPOINT_SIZE; i++)
        {
            code[i] = bp->saved[i];
        }
        ja_gdb_sync_icache(code, JA_GDB_BREAKPOINT_SIZE);
    }
}

/**
 * @funcname `gdb_sw_insert`
 *
 * @description
 * Inserts a software breakpoint (Z0): of the architecture's size, aligned; an
 * existing one is kept.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `space_id` | `jet_space_id` | [in] The memory space. |
 * | `addr` | `uintptr_t` | [in] The address. |
 * | `kind` | `uintptr_t` | [in] The size GDB asks for. |
 *
 * @return TRUE if it is in place.
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `gdb_sw_breakpoints` | `gdb_sw_breakpoint_t[64]` | The software breakpoints |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `bp` | `gdb_sw_breakpoint_t *` | A free breakpoint |
 * | `i` | `uint32_t` | The index of a breakpoint |
 * | `inserted` | `pok_bool_t` | Whether it is in place |
 *
 * @hlr
 * - \hlrid{HLR_GDBSERVER_GDB_SW_INSERT_01}: `gdb_sw_insert()` shall insert a
 *   software breakpoint of the size of the architecture at an aligned address
 *   of a memory space, keep one that is already there, and tell whether it is
 *   in place.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_DBG_005}
 *
 * @llr
 * - \llrid{LLR_GDBSERVER_GDB_SW_INSERT_01}: If `kind` is not
 *   `JA_GDB_BREAKPOINT_SIZE`, `gdb_sw_insert()` shall return `FALSE` and leave
 *   `gdb_sw_breakpoints` as it is.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDBSERVER_GDB_SW_INSERT_01}
 * - \llrid{LLR_GDBSERVER_GDB_SW_INSERT_02}: If `kind` is
 *   `JA_GDB_BREAKPOINT_SIZE` and `addr` is not a multiple of
 *   `JA_GDB_BREAKPOINT_SIZE`, `gdb_sw_insert()` shall return `FALSE` and leave
 *   `gdb_sw_breakpoints` as it is.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDBSERVER_GDB_SW_INSERT_01}
 * - \llrid{LLR_GDBSERVER_GDB_SW_INSERT_03}: If `kind` is
 *   `JA_GDB_BREAKPOINT_SIZE`, `addr` is a multiple of it and `gdb_sw_find()`
 *   returns an element for `space_id` and `addr`, `gdb_sw_insert()` shall
 *   return `TRUE` and leave `gdb_sw_breakpoints` as it is.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDBSERVER_GDB_SW_INSERT_01}
 * - \llrid{LLR_GDBSERVER_GDB_SW_INSERT_04}: If `kind` is
 *   `JA_GDB_BREAKPOINT_SIZE`, `addr` is a multiple of it and `gdb_sw_find()`
 *   returns NULL for `space_id` and `addr`, `gdb_sw_insert()` shall, for the
 *   elements of `gdb_sw_breakpoints` whose `used` is false, in their order, set
 *   `space_id` and `addr` of the element to the arguments, call
 *   `gdb_sw_write()` with it and set its `used` to the result, until a result
 *   is true, and return the last result.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDBSERVER_GDB_SW_INSERT_01}
 * - \llrid{LLR_GDBSERVER_GDB_SW_INSERT_05}: If `kind` is
 *   `JA_GDB_BREAKPOINT_SIZE`, `addr` is a multiple of it, `gdb_sw_find()`
 *   returns NULL and `used` of every element of `gdb_sw_breakpoints` is true,
 *   `gdb_sw_insert()` shall return `FALSE`.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDBSERVER_GDB_SW_INSERT_01}
 *
 * @design
 * 1. Set `inserted` to `FALSE`. \llrref{LLR_GDBSERVER_GDB_SW_INSERT_01},
 *    \llrref{LLR_GDBSERVER_GDB_SW_INSERT_02},
 *    \llrref{LLR_GDBSERVER_GDB_SW_INSERT_05}
 * 2. If `kind` is not equal to `JA_GDB_BREAKPOINT_SIZE`:
 *    \llrref{LLR_GDBSERVER_GDB_SW_INSERT_01}
 * 3. Otherwise, if `(addr % JA_GDB_BREAKPOINT_SIZE)` is not equal to `0U`:
 *    \llrref{LLR_GDBSERVER_GDB_SW_INSERT_02}
 * 4. Otherwise, if `gdb_sw_find(space_id, addr)` is not NULL:
 *    \llrref{LLR_GDBSERVER_GDB_SW_INSERT_03}
 *    1. Set `inserted` to `TRUE`.
 * 5. Otherwise: \llrref{LLR_GDBSERVER_GDB_SW_INSERT_04},
 *    \llrref{LLR_GDBSERVER_GDB_SW_INSERT_05}
 *    1. Set `i` to `0U`; then while `i` is less than `GDB_SW_BREAKPOINTS` and
 *       `inserted` is false, repeat (after each pass: increment `i`):
 *       1. Set `bp` to the address of `gdb_sw_breakpoints[i]`.
 *       2. If `bp->used` is false:
 *          1. Set `bp->space_id` to `space_id`.
 *             \llrref{LLR_GDBSERVER_GDB_SW_INSERT_04}
 *          2. Set `bp->addr` to `addr`. \llrref{LLR_GDBSERVER_GDB_SW_INSERT_04}
 *          3. Call `gdb_sw_write()` with the argument `bp`, and store the
 *             result in `inserted`. \llrref{LLR_GDBSERVER_GDB_SW_INSERT_04}
 *          4. Set `bp->used` to `inserted`.
 *             \llrref{LLR_GDBSERVER_GDB_SW_INSERT_04}
 * 6. Return `inserted`. \llrref{LLR_GDBSERVER_GDB_SW_INSERT_01},
 *    \llrref{LLR_GDBSERVER_GDB_SW_INSERT_02},
 *    \llrref{LLR_GDBSERVER_GDB_SW_INSERT_03},
 *    \llrref{LLR_GDBSERVER_GDB_SW_INSERT_04},
 *    \llrref{LLR_GDBSERVER_GDB_SW_INSERT_05}
 *
 * @calls
 * - `gdb_sw_find()`
 * - `gdb_sw_write()`
 */
static pok_bool_t gdb_sw_insert(jet_space_id space_id, uintptr_t addr,
    uintptr_t kind)
{
    pok_bool_t inserted = FALSE;

    if (kind != JA_GDB_BREAKPOINT_SIZE)
    {
        // Another breakpoint instruction: not supported.
    }
    else if ((addr % JA_GDB_BREAKPOINT_SIZE) != 0U)
    {
        // Instructions are aligned (arm64): it isn't a place for code.
    }
    else if (gdb_sw_find(space_id, addr) != NULL)
    {
        inserted = TRUE; // Already there: the code is saved.
    }
    else
    {
        for (uint32_t i = 0U; (i < GDB_SW_BREAKPOINTS) && !inserted; i++)
        {
            gdb_sw_breakpoint_t* bp = &gdb_sw_breakpoints[i];

            if (!bp->used)
            {
                bp->space_id = space_id;
                bp->addr = addr;
                inserted = gdb_sw_write(bp);
                bp->used = inserted;
            }
        }
    }

    return inserted;
}

/**
 * @funcname `gdb_sw_remove`
 *
 * @description
 * Removes a software breakpoint (z0).
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `space_id` | `jet_space_id` | [in] The memory space. |
 * | `addr` | `uintptr_t` | [in] The address. |
 *
 * @return TRUE if there was one.
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `bp` | `gdb_sw_breakpoint_t *` | The breakpoint |
 * | `removed` | `pok_bool_t` | Whether it was removed |
 *
 * @hlr
 * - \hlrid{HLR_GDBSERVER_GDB_SW_REMOVE_01}: `gdb_sw_remove()` shall remove the
 *   software breakpoint at an address of a memory space, putting back the code,
 *   and tell whether there was one.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_DBG_005}
 *
 * @llr
 * - \llrid{LLR_GDBSERVER_GDB_SW_REMOVE_01}: If `gdb_sw_find()` returns an
 *   element for `space_id` and `addr`, `gdb_sw_remove()` shall call
 *   `gdb_sw_restore()` once with it, set its `used` to `FALSE`, and return
 *   `TRUE`.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDBSERVER_GDB_SW_REMOVE_01}
 * - \llrid{LLR_GDBSERVER_GDB_SW_REMOVE_02}: If `gdb_sw_find()` returns NULL for
 *   `space_id` and `addr`, `gdb_sw_remove()` shall return `FALSE` without
 *   calling `gdb_sw_restore()`.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDBSERVER_GDB_SW_REMOVE_01}
 *
 * @design
 * 1. Call `gdb_sw_find()` with the arguments `space_id` and `addr`, and store
 *    the result in `bp`. \llrref{LLR_GDBSERVER_GDB_SW_REMOVE_01},
 *    \llrref{LLR_GDBSERVER_GDB_SW_REMOVE_02}
 * 2. Set `removed` to `FALSE`. \llrref{LLR_GDBSERVER_GDB_SW_REMOVE_02}
 * 3. If `bp` is not NULL: \llrref{LLR_GDBSERVER_GDB_SW_REMOVE_01},
 *    \llrref{LLR_GDBSERVER_GDB_SW_REMOVE_02}
 *    1. Call `gdb_sw_restore()` with the argument `bp`.
 *       \llrref{LLR_GDBSERVER_GDB_SW_REMOVE_01}
 *    2. Set `bp->used` to `FALSE`. \llrref{LLR_GDBSERVER_GDB_SW_REMOVE_01}
 *    3. Set `removed` to `TRUE`. \llrref{LLR_GDBSERVER_GDB_SW_REMOVE_01}
 * 4. Return `removed`. \llrref{LLR_GDBSERVER_GDB_SW_REMOVE_01},
 *    \llrref{LLR_GDBSERVER_GDB_SW_REMOVE_02}
 *
 * @calls
 * - `gdb_sw_find()`
 * - `gdb_sw_restore()`
 */
static pok_bool_t gdb_sw_remove(jet_space_id space_id, uintptr_t addr)
{
    gdb_sw_breakpoint_t* bp = gdb_sw_find(space_id, addr);
    pok_bool_t removed = FALSE;

    if (bp != NULL)
    {
        gdb_sw_restore(bp);
        bp->used = FALSE;
        removed = TRUE;
    }

    return removed;
}

/**
 * @funcname `gdb_sw_remove_all`
 *
 * @description
 * Removes every software breakpoint (end of the session).
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `gdb_sw_breakpoints` | `gdb_sw_breakpoint_t[64]` | The software breakpoints |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `i` | `uint32_t` | The index of a breakpoint |
 *
 * @hlr
 * - \hlrid{HLR_GDBSERVER_GDB_SW_REMOVE_ALL_01}: `gdb_sw_remove_all()` shall
 *   remove every software breakpoint at the end of the session.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_DBG_015}
 *
 * @llr
 * - \llrid{LLR_GDBSERVER_GDB_SW_REMOVE_ALL_01}: For each of the
 *   `GDB_SW_BREAKPOINTS` elements of `gdb_sw_breakpoints`, if and only if its
 *   `used` is true, `gdb_sw_remove_all()` shall call `gdb_sw_restore()` with it
 *   and set its `used` to `FALSE`.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDBSERVER_GDB_SW_REMOVE_ALL_01}
 *
 * @design
 * 1. Set `i` to `0U`; then while `i` is less than `GDB_SW_BREAKPOINTS`, repeat
 *    (after each pass: increment `i`):
 *    \llrref{LLR_GDBSERVER_GDB_SW_REMOVE_ALL_01}
 *    1. If `gdb_sw_breakpoints[i].used` is true:
 *       1. Call `gdb_sw_restore()` with the address of `gdb_sw_breakpoints[i]`.
 *       2. Set `gdb_sw_breakpoints[i].used` to `FALSE`.
 *
 * @calls
 * - `gdb_sw_restore()`
 */
static void gdb_sw_remove_all(void)
{
    for (uint32_t i = 0U; i < GDB_SW_BREAKPOINTS; i++)
    {
        if (gdb_sw_breakpoints[i].used)
        {
            gdb_sw_restore(&gdb_sw_breakpoints[i]);
            gdb_sw_breakpoints[i].used = FALSE;
        }
    }
}

/**
 * @funcname `jet_gdb_is_breakpoint`
 *
 * @description
 * Tells whether a software breakpoint of GDB is at an address (the
 * architecture's trap handler asks).
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `space_id` | `jet_space_id` | [in] The memory space. |
 * | `addr` | `uintptr_t` | [in] The address. |
 *
 * @return TRUE if there is one.
 *
 * @globals
 * None.
 *
 * @locals
 * None.
 *
 * @hlr
 * - \hlrid{HLR_GDBSERVER_JET_GDB_IS_BREAKPOINT_01}: `jet_gdb_is_breakpoint()`
 *   shall tell the trap handler of the architecture whether a software
 *   breakpoint of the debugger is at an address of a memory space.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_DBG_014}
 *
 * @llr
 * - \llrid{LLR_GDBSERVER_JET_GDB_IS_BREAKPOINT_01}: If `gdb_sw_find()` returns
 *   an element for `space_id` and `addr`, `jet_gdb_is_breakpoint()` shall
 *   return true.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDBSERVER_JET_GDB_IS_BREAKPOINT_01}
 * - \llrid{LLR_GDBSERVER_JET_GDB_IS_BREAKPOINT_02}: If `gdb_sw_find()` returns
 *   NULL for `space_id` and `addr`, `jet_gdb_is_breakpoint()` shall return
 *   false.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDBSERVER_JET_GDB_IS_BREAKPOINT_01}
 *
 * @design
 * 1. Return `gdb_sw_find(space_id, addr) != NULL`.
 *    \llrref{LLR_GDBSERVER_JET_GDB_IS_BREAKPOINT_01},
 *    \llrref{LLR_GDBSERVER_JET_GDB_IS_BREAKPOINT_02}
 *
 * @calls
 * - `gdb_sw_find()`
 */
pok_bool_t jet_gdb_is_breakpoint(jet_space_id space_id, uintptr_t addr)
{
    return gdb_sw_find(space_id, addr) != NULL;
}

/**
 * @funcname `jet_gdb_space_loaded`
 *
 * @description
 * Writes the software breakpoints of a memory space again after its image was
 * loaded (a partition restart).
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `space_id` | `jet_space_id` | [in] The memory space. |
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `gdb_sw_breakpoints` | `gdb_sw_breakpoint_t[64]` | The software breakpoints |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `bp` | `gdb_sw_breakpoint_t *` | A breakpoint |
 * | `i` | `uint32_t` | The index of a breakpoint |
 *
 * @hlr
 * - \hlrid{HLR_GDBSERVER_JET_GDB_SPACE_LOADED_01}: `jet_gdb_space_loaded()`
 *   shall write the software breakpoints of a memory space again after its
 *   image was loaded, so that they stay across a restart of the partition.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_DBG_005}
 *
 * @llr
 * - \llrid{LLR_GDBSERVER_JET_GDB_SPACE_LOADED_01}: For each of the
 *   `GDB_SW_BREAKPOINTS` elements of `gdb_sw_breakpoints`, if and only if its
 *   `used` is true and its `space_id` is `space_id`, `jet_gdb_space_loaded()`
 *   shall call `gdb_sw_write()` with it and set its `used` to the result.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDBSERVER_JET_GDB_SPACE_LOADED_01}
 *
 * @design
 * 1. Set `i` to `0U`; then while `i` is less than `GDB_SW_BREAKPOINTS`, repeat
 *    (after each pass: increment `i`):
 *    \llrref{LLR_GDBSERVER_JET_GDB_SPACE_LOADED_01}
 *    1. Set `bp` to the address of `gdb_sw_breakpoints[i]`.
 *    2. If `bp->used` is true and `bp->space_id` is equal to `space_id`:
 *       1. Call `gdb_sw_write()` with the argument `bp`, and store the result
 *          in `bp->used`.
 *
 * @calls
 * - `gdb_sw_write()`
 */
void jet_gdb_space_loaded(jet_space_id space_id)
{
    for (uint32_t i = 0U; i < GDB_SW_BREAKPOINTS; i++)
    {
        gdb_sw_breakpoint_t* bp = &gdb_sw_breakpoints[i];

        if (bp->used && (bp->space_id == space_id))
        {
            // The code is new: save it again.
            bp->used = gdb_sw_write(bp);
        }
    }
}

/**
 * @funcname `gdb_sw_hide`
 *
 * @description
 * Shows GDB the code under the software breakpoints in a copy of memory read.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `space_id` | `jet_space_id` | [in] The memory space. |
 * | `addr` | `uintptr_t` | [in] The address of the memory. |
 * | `mem` | `uint8_t *` | [out] The copy. |
 * | `len` | `size_t` | [in] Its length. |
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `gdb_sw_breakpoints` | `gdb_sw_breakpoint_t[64]` | The software breakpoints |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `bp` | `const gdb_sw_breakpoint_t *` | A breakpoint |
 * | `byte_addr` | `uintptr_t` | The address of that byte |
 * | `i` | `uint32_t` | The index of a breakpoint |
 * | `j` | `uint32_t` | The index of a byte of the breakpoint |
 *
 * @hlr
 * - \hlrid{HLR_GDBSERVER_GDB_SW_HIDE_01}: `gdb_sw_hide()` shall show the
 *   debugger, in the memory that it reads, the code under the software
 *   breakpoints instead of the breakpoints.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_DBG_004}
 *
 * @llr
 * - \llrid{LLR_GDBSERVER_GDB_SW_HIDE_01}: For each byte `j`, less than
 *   `JA_GDB_BREAKPOINT_SIZE`, of each element of `gdb_sw_breakpoints` whose
 *   `used` is true and whose `space_id` is `space_id`, if and only if the
 *   address of the byte (`addr` of the element plus `j`) is not less than
 *   `addr` and is less than `addr` plus `len`, `gdb_sw_hide()` shall store
 *   `saved[j]` of the element in `mem` at the index of that address minus
 *   `addr`.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDBSERVER_GDB_SW_HIDE_01}
 *
 * @design
 * 1. Set `i` to `0U`; then while `i` is less than `GDB_SW_BREAKPOINTS`, repeat
 *    (after each pass: increment `i`): \llrref{LLR_GDBSERVER_GDB_SW_HIDE_01}
 *    1. Set `bp` to the address of `gdb_sw_breakpoints[i]`.
 *    2. Set `j` to `0U`; then while `bp->used` is true and `bp->space_id` is
 *       equal to `space_id` and `j` is less than `JA_GDB_BREAKPOINT_SIZE`,
 *       repeat (after each pass: increment `j`):
 *       1. Set `byte_addr` to `bp->addr + j`.
 *       2. If `byte_addr` is greater than or equal to `addr` and
 *          `(byte_addr - addr)` is less than `len`:
 *          1. Set `mem[byte_addr - addr]` to `bp->saved[j]`.
 *
 * @calls
 * None.
 */
static void gdb_sw_hide(jet_space_id space_id, uintptr_t addr, uint8_t* mem,
    size_t len)
{
    for (uint32_t i = 0U; i < GDB_SW_BREAKPOINTS; i++)
    {
        const gdb_sw_breakpoint_t* bp = &gdb_sw_breakpoints[i];

        for (uint32_t j = 0U; bp->used && (bp->space_id == space_id)
            && (j < JA_GDB_BREAKPOINT_SIZE); j++)
        {
            uintptr_t byte_addr = bp->addr + j;

            if ((byte_addr >= addr) && ((byte_addr - addr) < len)) {
                mem[byte_addr - addr] = bp->saved[j];
            }
        }
    }
}

/**
 * @funcname `gdb_sw_rewrite`
 *
 * @description
 * Keeps the software breakpoints in memory that GDB wrote: the new bytes are
 * saved as the code under them, and the breakpoint goes back.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `space_id` | `jet_space_id` | [in] The memory space. |
 * | `addr` | `uintptr_t` | [in] The address of the memory. |
 * | `mem` | `uint8_t *` | [in,out] The memory written. |
 * | `len` | `size_t` | [in] Its length. |
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `gdb_sw_breakpoints` | `gdb_sw_breakpoint_t[64]` | The software breakpoints |
 * | `ja_gdb_breakpoint_insn` | `const uint8_t[1]` | The breakpoint instruction of the architecture |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `bp` | `gdb_sw_breakpoint_t *` | A breakpoint |
 * | `byte_addr` | `uintptr_t` | The address of that byte |
 * | `i` | `uint32_t` | The index of a breakpoint |
 * | `j` | `uint32_t` | The index of a byte of the breakpoint |
 *
 * @hlr
 * - \hlrid{HLR_GDBSERVER_GDB_SW_REWRITE_01}: `gdb_sw_rewrite()` shall keep the
 *   software breakpoints in memory that the debugger writes: the new bytes
 *   become the code under them.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_DBG_004}
 *
 * @llr
 * - \llrid{LLR_GDBSERVER_GDB_SW_REWRITE_01}: For each byte `j`, less than
 *   `JA_GDB_BREAKPOINT_SIZE`, of each element of `gdb_sw_breakpoints` whose
 *   `used` is true and whose `space_id` is `space_id`, if and only if the
 *   address of the byte (`addr` of the element plus `j`) is not less than
 *   `addr` and is less than `addr` plus `len`, `gdb_sw_rewrite()` shall set
 *   `saved[j]` of the element to the byte of `mem` at the index of that address
 *   minus `addr`, and then replace that byte of `mem` with
 *   `ja_gdb_breakpoint_insn[j]`.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDBSERVER_GDB_SW_REWRITE_01}
 *
 * @design
 * 1. Set `i` to `0U`; then while `i` is less than `GDB_SW_BREAKPOINTS`, repeat
 *    (after each pass: increment `i`): \llrref{LLR_GDBSERVER_GDB_SW_REWRITE_01}
 *    1. Set `bp` to the address of `gdb_sw_breakpoints[i]`.
 *    2. Set `j` to `0U`; then while `bp->used` is true and `bp->space_id` is
 *       equal to `space_id` and `j` is less than `JA_GDB_BREAKPOINT_SIZE`,
 *       repeat (after each pass: increment `j`):
 *       1. Set `byte_addr` to `bp->addr + j`.
 *       2. If `byte_addr` is greater than or equal to `addr` and
 *          `(byte_addr - addr)` is less than `len`:
 *          1. Set `bp->saved[j]` to `mem[byte_addr - addr]`.
 *          2. Set `mem[byte_addr - addr]` to `ja_gdb_breakpoint_insn[j]`.
 *
 * @calls
 * None.
 */
static void gdb_sw_rewrite(jet_space_id space_id, uintptr_t addr,
    uint8_t* mem, size_t len)
{
    for (uint32_t i = 0U; i < GDB_SW_BREAKPOINTS; i++)
    {
        gdb_sw_breakpoint_t* bp = &gdb_sw_breakpoints[i];

        for (uint32_t j = 0U; bp->used && (bp->space_id == space_id)
            && (j < JA_GDB_BREAKPOINT_SIZE); j++)
        {
            uintptr_t byte_addr = bp->addr + j;

            if ((byte_addr >= addr) && ((byte_addr - addr) < len))
            {
                bp->saved[j] = mem[byte_addr - addr];
                mem[byte_addr - addr] = ja_gdb_breakpoint_insn[j];
            }
        }
    }
}

/***********************************************************************
 * Serial line.
 */

/**
 * @funcname `gdb_put_chars`
 *
 * @description
 * Sends characters to GDB (the debug console).
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `s` | `const char *` | [in] The characters. |
 * | `len` | `size_t` | [in] Their number. |
 *
 * @globals
 * None.
 *
 * @locals
 * None.
 *
 * @hlr
 * - \hlrid{HLR_GDBSERVER_GDB_PUT_CHARS_01}: `gdb_put_chars()` shall send
 *   characters to the debugger over the debug console.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_DBG_001}
 *
 * @llr
 * - \llrid{LLR_GDBSERVER_GDB_PUT_CHARS_01}: `gdb_put_chars()` shall call
 *   `pok_cons_write_1()` once with `s` and `len`.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDBSERVER_GDB_PUT_CHARS_01}
 *
 * @design
 * 1. Call `pok_cons_write_1()` with the arguments `s` and `len`.
 *    \llrref{LLR_GDBSERVER_GDB_PUT_CHARS_01}
 *
 * @calls
 * - `pok_cons_write_1()`
 */
static void gdb_put_chars(const char* s, size_t len)
{
    (void) pok_cons_write_1(s, len);
}

/**
 * @funcname `gdb_get_char`
 *
 * @description
 * Reads the next character from GDB (or the one read ahead).
 *
 * @return The character.
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `gdb_pushback` | `int32_t` | A character read ahead, or -1 |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `c` | `int32_t` | The character |
 *
 * @hlr
 * - \hlrid{HLR_GDBSERVER_GDB_GET_CHAR_01}: `gdb_get_char()` shall give the next
 *   character from the debugger, starting with one that was read ahead.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_DBG_001}
 *
 * @llr
 * - \llrid{LLR_GDBSERVER_GDB_GET_CHAR_01}: If `gdb_pushback` is not less than
 *   0, `gdb_get_char()` shall return it and set `gdb_pushback` to -1, without
 *   calling `getchar2()`.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDBSERVER_GDB_GET_CHAR_01}
 * - \llrid{LLR_GDBSERVER_GDB_GET_CHAR_02}: If `gdb_pushback` is less than 0,
 *   `gdb_get_char()` shall return the result of `getchar2()`.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDBSERVER_GDB_GET_CHAR_01}
 *
 * @design
 * 1. Set `c` to `gdb_pushback`. \llrref{LLR_GDBSERVER_GDB_GET_CHAR_01},
 *    \llrref{LLR_GDBSERVER_GDB_GET_CHAR_02}
 * 2. If `c` is greater than or equal to `0`:
 *    \llrref{LLR_GDBSERVER_GDB_GET_CHAR_01}
 *    1. Set `gdb_pushback` to `-1`.
 * 3. Otherwise: \llrref{LLR_GDBSERVER_GDB_GET_CHAR_02}
 *    1. Call `getchar2()`, and store the result in `c`.
 * 4. Return `c`. \llrref{LLR_GDBSERVER_GDB_GET_CHAR_01},
 *    \llrref{LLR_GDBSERVER_GDB_GET_CHAR_02}
 *
 * @calls
 * - `getchar2()`
 */
static int32_t gdb_get_char(void)
{
    int32_t c = gdb_pushback;

    if (c >= 0) {
        gdb_pushback = -1;
    } else {
        c = getchar2();
    }

    return c;
}

/**
 * @funcname `gdb_hex_value`
 *
 * @description
 * Returns the value of a hex digit.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `c` | `int32_t` | [in] The character. |
 *
 * @return 0-15, or -1 if it isn't a hex digit.
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `value` | `int32_t` | The value |
 *
 * @hlr
 * - \hlrid{HLR_GDBSERVER_GDB_HEX_VALUE_01}: `gdb_hex_value()` shall give the
 *   value of a hexadecimal digit of the protocol, or -1 for another character.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_DBG_001}
 *
 * @llr
 * - \llrid{LLR_GDBSERVER_GDB_HEX_VALUE_01}: If `c` is from `'0'` to `'9'`,
 *   `gdb_hex_value()` shall return `c` minus `'0'`.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDBSERVER_GDB_HEX_VALUE_01}
 * - \llrid{LLR_GDBSERVER_GDB_HEX_VALUE_02}: If `c` is from `'a'` to `'f'`,
 *   `gdb_hex_value()` shall return `c` minus `'a'` plus 10.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDBSERVER_GDB_HEX_VALUE_01}
 * - \llrid{LLR_GDBSERVER_GDB_HEX_VALUE_03}: If `c` is from `'A'` to `'F'`,
 *   `gdb_hex_value()` shall return `c` minus `'A'` plus 10.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDBSERVER_GDB_HEX_VALUE_01}
 * - \llrid{LLR_GDBSERVER_GDB_HEX_VALUE_04}: If `c` is not from `'0'` to `'9'`,
 *   not from `'a'` to `'f'` and not from `'A'` to `'F'`, `gdb_hex_value()`
 *   shall return -1.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDBSERVER_GDB_HEX_VALUE_01}
 *
 * @design
 * 1. Set `value` to `-1`. \llrref{LLR_GDBSERVER_GDB_HEX_VALUE_04}
 * 2. If `c` is greater than or equal to `(int32_t)'0'` and `c` is less than or
 *    equal to `(int32_t)'9'`: \llrref{LLR_GDBSERVER_GDB_HEX_VALUE_01}
 *    1. Set `value` to `c - (int32_t)'0'`.
 * 3. Otherwise, if `c` is greater than or equal to `(int32_t)'a'` and `c` is
 *    less than or equal to `(int32_t)'f'`:
 *    \llrref{LLR_GDBSERVER_GDB_HEX_VALUE_02}
 *    1. Set `value` to `(c - (int32_t)'a') + 10`.
 * 4. Otherwise, if `c` is greater than or equal to `(int32_t)'A'` and `c` is
 *    less than or equal to `(int32_t)'F'`:
 *    \llrref{LLR_GDBSERVER_GDB_HEX_VALUE_03},
 *    \llrref{LLR_GDBSERVER_GDB_HEX_VALUE_04}
 *    1. Set `value` to `(c - (int32_t)'A') + 10`.
 * 5. Return `value`. \llrref{LLR_GDBSERVER_GDB_HEX_VALUE_01},
 *    \llrref{LLR_GDBSERVER_GDB_HEX_VALUE_02},
 *    \llrref{LLR_GDBSERVER_GDB_HEX_VALUE_03},
 *    \llrref{LLR_GDBSERVER_GDB_HEX_VALUE_04}
 *
 * @calls
 * None.
 */
static int32_t gdb_hex_value(int32_t c)
{
    int32_t value = -1;

    if ((c >= (int32_t)'0') && (c <= (int32_t)'9')) {
        value = c - (int32_t)'0';
    } else if ((c >= (int32_t)'a') && (c <= (int32_t)'f')) {
        value = (c - (int32_t)'a') + 10;
    } else if ((c >= (int32_t)'A') && (c <= (int32_t)'F')) {
        value = (c - (int32_t)'A') + 10;
    } else {
        // Not a digit.
    }

    return value;
}

/**
 * @funcname `gdb_get_packet_data`
 *
 * @description
 * Reads a packet's data after its $, up to # and the checksum, and acknowledges
 * it (+, or - if it is bad or too long); a $ restarts the packet.
 *
 * @return TRUE if a good packet was received.
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `gdb_in` | `char[4097]` | The packet received |
 * | `gdb_in_len` | `size_t` | The length of the packet received |
 * | `gdb_in_pos` | `size_t` | The position read in the packet received |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `c` | `int32_t` | The character read |
 * | `checksum` | `int32_t` | The checksum received |
 * | `end` | `pok_bool_t` | Whether # was read |
 * | `high` | `int32_t` | The first checksum digit |
 * | `len` | `size_t` | The length of the data |
 * | `low` | `int32_t` | The second checksum digit |
 * | `overflow` | `pok_bool_t` | Whether the data is too long |
 * | `received` | `pok_bool_t` | Whether the packet is good |
 * | `sum` | `uint8_t` | The checksum of the data |
 *
 * @hlr
 * - \hlrid{HLR_GDBSERVER_GDB_GET_PACKET_DATA_01}: `gdb_get_packet_data()` shall
 *   read the data of a packet up to its `'#'` and its checksum, acknowledge a
 *   good packet with `'+'` and a bad or too long one with `'-'`, and tell
 *   whether it was good.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_DBG_001}
 * - \hlrid{HLR_GDBSERVER_GDB_GET_PACKET_DATA_02}: `gdb_get_packet_data()` shall
 *   start the packet again at a `'$'` in its data.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_DBG_001}
 *
 * @llr
 * - \llrid{LLR_GDBSERVER_GDB_GET_PACKET_DATA_01}: `gdb_get_packet_data()` shall
 *   read characters with `gdb_get_char()` until one of them is `'#'`.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDBSERVER_GDB_GET_PACKET_DATA_01}
 * - \llrid{LLR_GDBSERVER_GDB_GET_PACKET_DATA_02}: If a character read is `'$'`,
 *   `gdb_get_packet_data()` shall start again: with no data, the sum 0 and no
 *   overflow.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDBSERVER_GDB_GET_PACKET_DATA_02}
 * - \llrid{LLR_GDBSERVER_GDB_GET_PACKET_DATA_03}: If a character read is
 *   neither `'#'` nor `'$'` and fewer than `GDB_PACKET_SIZE` characters of data
 *   are stored, `gdb_get_packet_data()` shall add it to the sum, modulo 256,
 *   and store it at the next index of `gdb_in`.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDBSERVER_GDB_GET_PACKET_DATA_01}
 * - \llrid{LLR_GDBSERVER_GDB_GET_PACKET_DATA_04}: If a character read is
 *   neither `'#'` nor `'$'` and `GDB_PACKET_SIZE` characters of data are
 *   stored, `gdb_get_packet_data()` shall add it to the sum, modulo 256, and
 *   note an overflow, without storing it.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDBSERVER_GDB_GET_PACKET_DATA_01}
 * - \llrid{LLR_GDBSERVER_GDB_GET_PACKET_DATA_05}: After the `'#'`,
 *   `gdb_get_packet_data()` shall read two characters with `gdb_get_char()` and
 *   take their values (`gdb_hex_value()`) as the high and the low digit of the
 *   checksum.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDBSERVER_GDB_GET_PACKET_DATA_01}
 * - \llrid{LLR_GDBSERVER_GDB_GET_PACKET_DATA_06}: If there is no overflow, both
 *   values are not less than 0 and the high value times 16 plus the low value
 *   is the sum, `gdb_get_packet_data()` shall store `'\0'` after the data in
 *   `gdb_in`, set `gdb_in_len` to the length of the data and `gdb_in_pos` to 0,
 *   call `gdb_put_chars()` with `"+"` and 1, and return `TRUE`.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDBSERVER_GDB_GET_PACKET_DATA_01}
 * - \llrid{LLR_GDBSERVER_GDB_GET_PACKET_DATA_07}: If there is an overflow, or
 *   one of the two values is less than 0, or the high value times 16 plus the
 *   low value is not the sum, `gdb_get_packet_data()` shall call
 *   `gdb_put_chars()` with `"-"` and 1, and return `FALSE`.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDBSERVER_GDB_GET_PACKET_DATA_01}
 *
 * @design
 * 1. Set `len` to `0U`. \llrref{LLR_GDBSERVER_GDB_GET_PACKET_DATA_03}
 * 2. Set `sum` to `0U`. \llrref{LLR_GDBSERVER_GDB_GET_PACKET_DATA_03}
 * 3. Set `overflow` to `FALSE`. \llrref{LLR_GDBSERVER_GDB_GET_PACKET_DATA_04},
 *    \llrref{LLR_GDBSERVER_GDB_GET_PACKET_DATA_06}
 * 4. Set `end` to `FALSE`. \llrref{LLR_GDBSERVER_GDB_GET_PACKET_DATA_01}
 * 5. Set `received` to `FALSE`. \llrref{LLR_GDBSERVER_GDB_GET_PACKET_DATA_07}
 * 6. While `end` is false: \llrref{LLR_GDBSERVER_GDB_GET_PACKET_DATA_01},
 *    \llrref{LLR_GDBSERVER_GDB_GET_PACKET_DATA_02},
 *    \llrref{LLR_GDBSERVER_GDB_GET_PACKET_DATA_03},
 *    \llrref{LLR_GDBSERVER_GDB_GET_PACKET_DATA_04}
 *    1. Call `gdb_get_char()`, and store the result in `c`.
 *    2. If `c` is equal to `(int32_t)'#'`:
 *       \llrref{LLR_GDBSERVER_GDB_GET_PACKET_DATA_01}
 *       1. Set `end` to `TRUE`.
 *    3. Otherwise, if `c` is equal to `(int32_t)'$'`:
 *       \llrref{LLR_GDBSERVER_GDB_GET_PACKET_DATA_02}
 *       1. Set `len` to `0U`.
 *       2. Set `sum` to `0U`.
 *       3. Set `overflow` to `FALSE`.
 *    4. Otherwise: \llrref{LLR_GDBSERVER_GDB_GET_PACKET_DATA_03},
 *       \llrref{LLR_GDBSERVER_GDB_GET_PACKET_DATA_04}
 *       1. Set `sum` to `(uint8_t)(sum + (uint8_t)c)`.
 *       2. If `len` is less than `GDB_PACKET_SIZE`:
 *          \llrref{LLR_GDBSERVER_GDB_GET_PACKET_DATA_03}
 *          1. Set `gdb_in[len]` to `(char)c`.
 *          2. Increment `len`.
 *       3. Otherwise: \llrref{LLR_GDBSERVER_GDB_GET_PACKET_DATA_04}
 *          1. Set `overflow` to `TRUE`.
 * 7. Call `gdb_hex_value()` with the argument `gdb_get_char()`, and store the
 *    result in `high`. \llrref{LLR_GDBSERVER_GDB_GET_PACKET_DATA_05}
 * 8. Call `gdb_hex_value()` with the argument `gdb_get_char()`, and store the
 *    result in `low`. \llrref{LLR_GDBSERVER_GDB_GET_PACKET_DATA_05}
 * 9. Set `checksum` to `(high * 16) + low`.
 *    \llrref{LLR_GDBSERVER_GDB_GET_PACKET_DATA_05}
 * 10. If `overflow` is false and `high` is greater than or equal to `0` and
 *     `low` is greater than or equal to `0` and `checksum` is equal to
 *     `(int32_t)sum`: \llrref{LLR_GDBSERVER_GDB_GET_PACKET_DATA_06}
 *     1. Set `gdb_in[len]` to `'\0'`.
 *     2. Set `gdb_in_len` to `len`.
 *     3. Set `gdb_in_pos` to `0U`.
 *     4. Call `gdb_put_chars()` with the arguments `"+"` and `1U`.
 *     5. Set `received` to `TRUE`.
 * 11. Otherwise: \llrref{LLR_GDBSERVER_GDB_GET_PACKET_DATA_07}
 *     1. Call `gdb_put_chars()` with the arguments `"-"` and `1U`.
 * 12. Return `received`. \llrref{LLR_GDBSERVER_GDB_GET_PACKET_DATA_06},
 *     \llrref{LLR_GDBSERVER_GDB_GET_PACKET_DATA_07}
 *
 * @calls
 * - `gdb_get_char()`
 * - `gdb_hex_value()`
 * - `gdb_put_chars()`
 */
static pok_bool_t gdb_get_packet_data(void)
{
    size_t len = 0U;
    uint8_t sum = 0U;
    pok_bool_t overflow = FALSE;
    pok_bool_t end = FALSE;
    pok_bool_t received = FALSE;

    while (!end)
    {
        int32_t c = gdb_get_char();

        if (c == (int32_t)'#')
        {
            end = TRUE;
        }
        else if (c == (int32_t)'$')
        {
            // A new packet starts: the previous one is lost.
            len = 0U;
            sum = 0U;
            overflow = FALSE;
        }
        else
        {
            sum = (uint8_t)(sum + (uint8_t)c);
            if (len < GDB_PACKET_SIZE)
            {
                gdb_in[len] = (char)c;
                len++;
            }
            else
            {
                overflow = TRUE;
            }
        }
    }

    int32_t high = gdb_hex_value(gdb_get_char());
    int32_t low = gdb_hex_value(gdb_get_char());
    int32_t checksum = (high * 16) + low;

    if (!overflow && (high >= 0) && (low >= 0)
        && (checksum == (int32_t)sum))
    {
        gdb_in[len] = '\0';
        gdb_in_len = len;
        gdb_in_pos = 0U;
        gdb_put_chars("+", 1U);
        received = TRUE;
    }
    else
    {
        gdb_put_chars("-", 1U);
    }

    return received;
}

/**
 * @funcname `gdb_get_packet`
 *
 * @description
 * Waits for a good packet from GDB, noting Ctrl-C on the way.
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `gdb_interrupt_pending` | `pok_bool_t` | Whether GDB sent Ctrl-C |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `c` | `int32_t` | The character read |
 * | `received` | `pok_bool_t` | Whether a packet was received |
 *
 * @hlr
 * - \hlrid{HLR_GDBSERVER_GDB_GET_PACKET_01}: `gdb_get_packet()` shall wait for
 *   a good packet from the debugger, noting an interrupt request that comes
 *   before it.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_DBG_001}
 *   - \srqref{SRQ_JET_DBG_006}
 *
 * @llr
 * - \llrid{LLR_GDBSERVER_GDB_GET_PACKET_01}: `gdb_get_packet()` shall read
 *   characters with `gdb_get_char()` until `gdb_get_packet_data()` returns
 *   true.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDBSERVER_GDB_GET_PACKET_01}
 * - \llrid{LLR_GDBSERVER_GDB_GET_PACKET_02}: If a character read is `'$'`,
 *   `gdb_get_packet()` shall call `gdb_get_packet_data()` once.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDBSERVER_GDB_GET_PACKET_01}
 * - \llrid{LLR_GDBSERVER_GDB_GET_PACKET_03}: If a character read is
 *   `GDB_CHAR_INTERRUPT`, `gdb_get_packet()` shall set `gdb_interrupt_pending`
 *   to `TRUE`.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDBSERVER_GDB_GET_PACKET_01}
 * - \llrid{LLR_GDBSERVER_GDB_GET_PACKET_04}: If a character read is neither
 *   `'$'` nor `GDB_CHAR_INTERRUPT`, `gdb_get_packet()` shall read the next
 *   character and leave `gdb_interrupt_pending` as it is.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDBSERVER_GDB_GET_PACKET_01}
 *
 * @design
 * 1. Set `received` to `FALSE`. \llrref{LLR_GDBSERVER_GDB_GET_PACKET_01}
 * 2. While `received` is false: \llrref{LLR_GDBSERVER_GDB_GET_PACKET_01}
 *    1. Call `gdb_get_char()`, and store the result in `c`.
 *    2. If `c` is equal to `(int32_t)'$'`:
 *       \llrref{LLR_GDBSERVER_GDB_GET_PACKET_02}
 *       1. Call `gdb_get_packet_data()`, and store the result in `received`.
 *    3. Otherwise, if `c` is equal to `GDB_CHAR_INTERRUPT`:
 *       \llrref{LLR_GDBSERVER_GDB_GET_PACKET_03},
 *       \llrref{LLR_GDBSERVER_GDB_GET_PACKET_04}
 *       1. Set `gdb_interrupt_pending` to `TRUE`.
 *          \llrref{LLR_GDBSERVER_GDB_GET_PACKET_03}
 *
 * @calls
 * - `gdb_get_char()`
 * - `gdb_get_packet_data()`
 */
static void gdb_get_packet(void)
{
    pok_bool_t received = FALSE;

    while (!received)
    {
        int32_t c = gdb_get_char();

        if (c == (int32_t)'$') {
            received = gdb_get_packet_data();
        } else if (c == GDB_CHAR_INTERRUPT) {
            gdb_interrupt_pending = TRUE;
        } else {
            // Acknowledgements are ignored here.
        }
    }
}

/**
 * @funcname `gdb_wait_ack`
 *
 * @description
 * Waits for GDB's acknowledgment of a packet sent; a new packet counts as an
 * acknowledgment (it is read ahead), Ctrl-C is noted.
 *
 * @return The acknowledgment: + or -.
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `gdb_interrupt_pending` | `pok_bool_t` | Whether GDB sent Ctrl-C |
 * | `gdb_pushback` | `int32_t` | A character read ahead, or -1 |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `c` | `int32_t` | The character read |
 *
 * @hlr
 * - \hlrid{HLR_GDBSERVER_GDB_WAIT_ACK_01}: `gdb_wait_ack()` shall wait for the
 *   acknowledgment of a packet sent, a new packet counting as a positive one,
 *   and note an interrupt request.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_DBG_001}
 *   - \srqref{SRQ_JET_DBG_006}
 *
 * @llr
 * - \llrid{LLR_GDBSERVER_GDB_WAIT_ACK_01}: If a character read with
 *   `gdb_get_char()` is `'+'` or `'-'`, `gdb_wait_ack()` shall return it.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDBSERVER_GDB_WAIT_ACK_01}
 * - \llrid{LLR_GDBSERVER_GDB_WAIT_ACK_02}: If a character read is `'$'`,
 *   `gdb_wait_ack()` shall set `gdb_pushback` to it and return `'+'`.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDBSERVER_GDB_WAIT_ACK_01}
 * - \llrid{LLR_GDBSERVER_GDB_WAIT_ACK_03}: If a character read is none of
 *   `'+'`, `'-'` and `'$'`, `gdb_wait_ack()` shall read the next character with
 *   `gdb_get_char()`.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDBSERVER_GDB_WAIT_ACK_01}
 * - \llrid{LLR_GDBSERVER_GDB_WAIT_ACK_04}: If and only if a character read that
 *   is none of `'+'`, `'-'` and `'$'` is `GDB_CHAR_INTERRUPT`, `gdb_wait_ack()`
 *   shall set `gdb_interrupt_pending` to `TRUE`.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDBSERVER_GDB_WAIT_ACK_01}
 *
 * @design
 * 1. Call `gdb_get_char()`, and store the result in `c`.
 *    \llrref{LLR_GDBSERVER_GDB_WAIT_ACK_01}
 * 2. While `c` is not equal to `(int32_t)'+'` and `c` is not equal to
 *    `(int32_t)'-'`: \llrref{LLR_GDBSERVER_GDB_WAIT_ACK_01},
 *    \llrref{LLR_GDBSERVER_GDB_WAIT_ACK_02},
 *    \llrref{LLR_GDBSERVER_GDB_WAIT_ACK_03},
 *    \llrref{LLR_GDBSERVER_GDB_WAIT_ACK_04}
 *    1. If `c` is equal to `(int32_t)'$'`:
 *       \llrref{LLR_GDBSERVER_GDB_WAIT_ACK_02}
 *       1. Set `gdb_pushback` to `c`.
 *       2. Set `c` to `(int32_t)'+'`.
 *    2. Otherwise: \llrref{LLR_GDBSERVER_GDB_WAIT_ACK_03},
 *       \llrref{LLR_GDBSERVER_GDB_WAIT_ACK_04}
 *       1. If `c` is equal to `GDB_CHAR_INTERRUPT`:
 *          \llrref{LLR_GDBSERVER_GDB_WAIT_ACK_04}
 *          1. Set `gdb_interrupt_pending` to `TRUE`.
 *       2. Call `gdb_get_char()`, and store the result in `c`.
 *          \llrref{LLR_GDBSERVER_GDB_WAIT_ACK_03}
 * 3. Return `c`. \llrref{LLR_GDBSERVER_GDB_WAIT_ACK_01},
 *    \llrref{LLR_GDBSERVER_GDB_WAIT_ACK_02}
 *
 * @calls
 * - `gdb_get_char()`
 */
static int32_t gdb_wait_ack(void)
{
    int32_t c = gdb_get_char();

    while ((c != (int32_t)'+') && (c != (int32_t)'-'))
    {
        if (c == (int32_t)'$')
        {
            // GDB has sent a packet instead: take that as '+'.
            gdb_pushback = c;
            c = (int32_t)'+';
        }
        else
        {
            if (c == GDB_CHAR_INTERRUPT) {
                gdb_interrupt_pending = TRUE;
            }
            c = gdb_get_char();
        }
    }

    return c;
}

/**
 * @funcname `gdb_put_packet`
 *
 * @description
 * Sends the reply packet ($data#checksum) until GDB acknowledges it.
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `gdb_hex_chars` | `const char[17]` | The hex digit characters |
 * | `gdb_out` | `char[4097]` | The packet to send |
 * | `gdb_out_len` | `size_t` | The length of the packet to send |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `acked` | `pok_bool_t` | Whether GDB acknowledged it |
 * | `frame` | `static char[4100]` | The packet |
 * | `i` | `size_t` | The index of a character of the data |
 * | `len` | `size_t` | The length of the packet |
 * | `sum` | `uint8_t` | The checksum of the data |
 *
 * @hlr
 * - \hlrid{HLR_GDBSERVER_GDB_PUT_PACKET_01}: `gdb_put_packet()` shall send the
 *   reply as a packet, with its checksum, until the debugger acknowledges it.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_DBG_001}
 *
 * @llr
 * - \llrid{LLR_GDBSERVER_GDB_PUT_PACKET_01}: `gdb_put_packet()` shall build a
 *   frame of `'$'`, the `gdb_out_len` characters of `gdb_out`, `'#'`, and the
 *   two characters of `gdb_hex_chars` for the high and the low 4 bits of the
 *   sum of these characters modulo 256.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDBSERVER_GDB_PUT_PACKET_01}
 * - \llrid{LLR_GDBSERVER_GDB_PUT_PACKET_02}: `gdb_put_packet()` shall call
 *   `gdb_put_chars()` with the frame and its length and then `gdb_wait_ack()`,
 *   and do so again until `gdb_wait_ack()` returns `'+'`.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDBSERVER_GDB_PUT_PACKET_01}
 *
 * @design
 * 1. Set `len` to `0U`. \llrref{LLR_GDBSERVER_GDB_PUT_PACKET_01}
 * 2. Set `sum` to `0U`. \llrref{LLR_GDBSERVER_GDB_PUT_PACKET_01}
 * 3. Set `acked` to `FALSE`. \llrref{LLR_GDBSERVER_GDB_PUT_PACKET_02}
 * 4. Set `frame[len]` to `'$'`. \llrref{LLR_GDBSERVER_GDB_PUT_PACKET_01}
 * 5. Increment `len`. \llrref{LLR_GDBSERVER_GDB_PUT_PACKET_01}
 * 6. Set `i` to `0U`; then while `i` is less than `gdb_out_len`, repeat (after
 *    each pass: increment `i`): \llrref{LLR_GDBSERVER_GDB_PUT_PACKET_01}
 *    1. Set `frame[len]` to `gdb_out[i]`.
 *    2. Increment `len`.
 *    3. Set `sum` to `(uint8_t)(sum + (uint8_t)gdb_out[i])`.
 * 7. Set `frame[len]` to `'#'`. \llrref{LLR_GDBSERVER_GDB_PUT_PACKET_01}
 * 8. Increment `len`. \llrref{LLR_GDBSERVER_GDB_PUT_PACKET_01}
 * 9. Set `frame[len]` to `gdb_hex_chars[sum >> 4]`.
 *    \llrref{LLR_GDBSERVER_GDB_PUT_PACKET_01}
 * 10. Increment `len`. \llrref{LLR_GDBSERVER_GDB_PUT_PACKET_01}
 * 11. Set `frame[len]` to `gdb_hex_chars[sum & 0xFU]`.
 *     \llrref{LLR_GDBSERVER_GDB_PUT_PACKET_01}
 * 12. Increment `len`. \llrref{LLR_GDBSERVER_GDB_PUT_PACKET_01}
 * 13. While `acked` is false: \llrref{LLR_GDBSERVER_GDB_PUT_PACKET_02}
 *     1. Call `gdb_put_chars()` with the arguments `frame` and `len`.
 *     2. Set `acked` to `(gdb_wait_ack() == (int32_t)'+')`.
 *
 * @calls
 * - `gdb_put_chars()`
 * - `gdb_wait_ack()`
 */
static void gdb_put_packet(void)
{
    static char frame[GDB_PACKET_SIZE + 4U];
    size_t len = 0U;
    uint8_t sum = 0U;
    pok_bool_t acked = FALSE;

    frame[len] = '$';
    len++;
    for (size_t i = 0U; i < gdb_out_len; i++)
    {
        frame[len] = gdb_out[i];
        len++;
        sum = (uint8_t)(sum + (uint8_t)gdb_out[i]);
    }
    frame[len] = '#';
    len++;
    frame[len] = gdb_hex_chars[sum >> 4];
    len++;
    frame[len] = gdb_hex_chars[sum & 0xFU];
    len++;

    while (!acked)
    {
        gdb_put_chars(frame, len);
        acked = (gdb_wait_ack() == (int32_t)'+');
    }
}

/***********************************************************************
 * Building the reply.
 */

/**
 * @funcname `out_reset`
 *
 * @description
 * Empties the reply packet.
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `gdb_out_len` | `size_t` | The length of the packet to send |
 *
 * @locals
 * None.
 *
 * @hlr
 * - \hlrid{HLR_GDBSERVER_OUT_RESET_01}: `out_reset()` shall make the reply
 *   packet empty.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_DBG_001}
 *
 * @llr
 * - \llrid{LLR_GDBSERVER_OUT_RESET_01}: `out_reset()` shall set `gdb_out_len`
 *   to 0.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDBSERVER_OUT_RESET_01}
 *
 * @design
 * 1. Set `gdb_out_len` to `0U`. \llrref{LLR_GDBSERVER_OUT_RESET_01}
 *
 * @calls
 * None.
 */
static void out_reset(void)
{
    gdb_out_len = 0U;
}

/**
 * @funcname `out_char`
 *
 * @description
 * Adds a character to the reply packet, if it has room.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `c` | `char` | [in] The character. |
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `gdb_out` | `char[4097]` | The packet to send |
 * | `gdb_out_len` | `size_t` | The length of the packet to send |
 *
 * @locals
 * None.
 *
 * @hlr
 * - \hlrid{HLR_GDBSERVER_OUT_CHAR_01}: `out_char()` shall add a character to
 *   the reply packet while it has room.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_DBG_001}
 *
 * @llr
 * - \llrid{LLR_GDBSERVER_OUT_CHAR_01}: If and only if `gdb_out_len` is less
 *   than `GDB_PACKET_SIZE`, `out_char()` shall store `c` at
 *   `gdb_out[gdb_out_len]` and increment `gdb_out_len`.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDBSERVER_OUT_CHAR_01}
 *
 * @design
 * 1. If `gdb_out_len` is less than `GDB_PACKET_SIZE`:
 *    \llrref{LLR_GDBSERVER_OUT_CHAR_01}
 *    1. Set `gdb_out[gdb_out_len]` to `c`.
 *    2. Increment `gdb_out_len`.
 *
 * @calls
 * None.
 */
static void out_char(char c)
{
    if (gdb_out_len < GDB_PACKET_SIZE)
    {
        gdb_out[gdb_out_len] = c;
        gdb_out_len++;
    }
}

/**
 * @funcname `out_str`
 *
 * @description
 * Adds a string to the reply packet.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `s` | `const char *` | [in] The string. |
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `i` | `size_t` | The index of the character |
 *
 * @hlr
 * - \hlrid{HLR_GDBSERVER_OUT_STR_01}: `out_str()` shall add a string to the
 *   reply packet.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_DBG_001}
 *
 * @llr
 * - \llrid{LLR_GDBSERVER_OUT_STR_01}: `out_str()` shall call `out_char()` with
 *   each character of `s` before its `'\0'`, in their order.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDBSERVER_OUT_STR_01}
 *
 * @design
 * 1. Set `i` to `0U`; then while `s[i]` is not equal to `'\0'`, repeat (after
 *    each pass: increment `i`): \llrref{LLR_GDBSERVER_OUT_STR_01}
 *    1. Call `out_char()` with the argument `s[i]`.
 *
 * @calls
 * - `out_char()`
 */
static void out_str(const char* s)
{
    for (size_t i = 0U; s[i] != '\0'; i++)
    {
        out_char(s[i]);
    }
}

/**
 * @funcname `out_byte`
 *
 * @description
 * Adds a byte to the reply packet, as two hex digits.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `b` | `uint8_t` | [in] The byte. |
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `gdb_hex_chars` | `const char[17]` | The hex digit characters |
 *
 * @locals
 * None.
 *
 * @hlr
 * - \hlrid{HLR_GDBSERVER_OUT_BYTE_01}: `out_byte()` shall add a byte to the
 *   reply packet as two hexadecimal digits.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_DBG_001}
 *
 * @llr
 * - \llrid{LLR_GDBSERVER_OUT_BYTE_01}: `out_byte()` shall call `out_char()`
 *   with the character of `gdb_hex_chars` for the high 4 bits of `b`, and then
 *   with the one for its low 4 bits.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDBSERVER_OUT_BYTE_01}
 *
 * @design
 * 1. Call `out_char()` with the argument `gdb_hex_chars[b >> 4]`.
 *    \llrref{LLR_GDBSERVER_OUT_BYTE_01}
 * 2. Call `out_char()` with the argument `gdb_hex_chars[b & 0xFU]`.
 *    \llrref{LLR_GDBSERVER_OUT_BYTE_01}
 *
 * @calls
 * - `out_char()`
 */
static void out_byte(uint8_t b)
{
    out_char(gdb_hex_chars[b >> 4]);
    out_char(gdb_hex_chars[b & 0xFU]);
}

/**
 * @funcname `out_bytes`
 *
 * @description
 * Adds bytes to the reply packet, in hex.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `data` | `const uint8_t *` | [in] The bytes. |
 * | `len` | `size_t` | [in] Their number. |
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `i` | `size_t` | The index of the byte |
 *
 * @hlr
 * - \hlrid{HLR_GDBSERVER_OUT_BYTES_01}: `out_bytes()` shall add bytes to the
 *   reply packet in hexadecimal.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_DBG_001}
 *   - \srqref{SRQ_JET_DBG_004}
 *
 * @llr
 * - \llrid{LLR_GDBSERVER_OUT_BYTES_01}: `out_bytes()` shall call `out_byte()`
 *   with each of the `len` bytes of `data`, in their order.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDBSERVER_OUT_BYTES_01}
 *
 * @design
 * 1. Set `i` to `0U`; then while `i` is less than `len`, repeat (after each
 *    pass: increment `i`): \llrref{LLR_GDBSERVER_OUT_BYTES_01}
 *    1. Call `out_byte()` with the argument `data[i]`.
 *
 * @calls
 * - `out_byte()`
 */
static void out_bytes(const uint8_t* data, size_t len)
{
    for (size_t i = 0U; i < len; i++)
    {
        out_byte(data[i]);
    }
}

/**
 * @funcname `out_unavailable`
 *
 * @description
 * Adds bytes that can't be read to the reply packet (xx each).
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `len` | `size_t` | [in] Their number. |
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `i` | `size_t` | The index of the byte |
 *
 * @hlr
 * - \hlrid{HLR_GDBSERVER_OUT_UNAVAILABLE_01}: `out_unavailable()` shall add to
 *   the reply packet the marks of bytes that cannot be read.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_DBG_004}
 *
 * @llr
 * - \llrid{LLR_GDBSERVER_OUT_UNAVAILABLE_01}: `out_unavailable()` shall call
 *   `out_str()` with `"xx"` `len` times.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDBSERVER_OUT_UNAVAILABLE_01}
 *
 * @design
 * 1. Set `i` to `0U`; then while `i` is less than `len`, repeat (after each
 *    pass: increment `i`): \llrref{LLR_GDBSERVER_OUT_UNAVAILABLE_01}
 *    1. Call `out_str()` with the argument `"xx"`.
 *
 * @calls
 * - `out_str()`
 */
static void out_unavailable(size_t len)
{
    for (size_t i = 0U; i < len; i++)
    {
        out_str("xx");
    }
}

/**
 * @funcname `out_hex`
 *
 * @description
 * Adds a number to the reply packet, in hex without leading zeros.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `value` | `uint64_t` | [in] The number. |
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `gdb_hex_chars` | `const char[17]` | The hex digit characters |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `digit` | `uint64_t` | The digit |
 * | `digit_shift` | `uint32_t` | The position of the digit |
 * | `shift` | `uint32_t` | The position after the digit |
 * | `started` | `pok_bool_t` | Whether a digit was written |
 *
 * @hlr
 * - \hlrid{HLR_GDBSERVER_OUT_HEX_01}: `out_hex()` shall add a number to the
 *   reply packet in hexadecimal, without leading zeros.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_DBG_001}
 *
 * @llr
 * - \llrid{LLR_GDBSERVER_OUT_HEX_01}: `out_hex()` shall call `out_char()` with
 *   the character of `gdb_hex_chars` for each of the 16 hexadecimal digits of
 *   `value`, from the highest one on, that is not 0, or follows a digit given
 *   before, or is the last digit.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDBSERVER_OUT_HEX_01}
 * - \llrid{LLR_GDBSERVER_OUT_HEX_02}: `out_hex()` shall not call `out_char()`
 *   for a digit of `value` that is 0, has no digit given before it and is not
 *   the last digit.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDBSERVER_OUT_HEX_01}
 *
 * @design
 * 1. Set `started` to `FALSE`. \llrref{LLR_GDBSERVER_OUT_HEX_01},
 *    \llrref{LLR_GDBSERVER_OUT_HEX_02}
 * 2. Set `shift` to `64U`; then while `shift` is greater than `0U`, repeat
 *    (after each pass: subtract `4U` from `shift`):
 *    \llrref{LLR_GDBSERVER_OUT_HEX_01}, \llrref{LLR_GDBSERVER_OUT_HEX_02}
 *    1. Set `digit_shift` to `shift - 4U`.
 *    2. Set `digit` to `value >> digit_shift`.
 *    3. Keep only the bits `0xFU` of `digit`.
 *    4. If `digit` is not equal to `0U` or `started` is true or `shift` is
 *       equal to `4U`:
 *       1. Call `out_char()` with the argument `gdb_hex_chars[digit]`.
 *          \llrref{LLR_GDBSERVER_OUT_HEX_01}
 *       2. Set `started` to `TRUE`. \llrref{LLR_GDBSERVER_OUT_HEX_01}
 *
 * @calls
 * - `out_char()`
 */
static void out_hex(uint64_t value)
{
    pok_bool_t started = FALSE;

    for (uint32_t shift = 64U; shift > 0U; shift -= 4U)
    {
        uint32_t digit_shift = shift - 4U;
        uint64_t digit = value >> digit_shift;

        digit &= 0xFU;

        if ((digit != 0U) || started || (shift == 4U))
        {
            out_char(gdb_hex_chars[digit]);
            started = TRUE;
        }
    }
}

/**
 * @funcname `out_thread_id`
 *
 * @description
 * Adds a thread ID to the reply packet: `p<pid>.<tid>`.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `t` | `const gdb_thread_t *` | [in] The thread. |
 *
 * @globals
 * None.
 *
 * @locals
 * None.
 *
 * @hlr
 * - \hlrid{HLR_GDBSERVER_OUT_THREAD_ID_01}: `out_thread_id()` shall add a
 *   thread identifier to the reply packet as `p<pid>.<tid>`.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_DBG_002}
 *
 * @llr
 * - \llrid{LLR_GDBSERVER_OUT_THREAD_ID_01}: `out_thread_id()` shall call, in
 *   this order, `out_char()` with `'p'`, `out_hex()` with `pid` of `t`,
 *   `out_char()` with `'.'` and `out_hex()` with `tid` of `t`.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDBSERVER_OUT_THREAD_ID_01}
 *
 * @design
 * 1. Call `out_char()` with the argument `'p'`.
 *    \llrref{LLR_GDBSERVER_OUT_THREAD_ID_01}
 * 2. Call `out_hex()` with the argument `t->pid`.
 *    \llrref{LLR_GDBSERVER_OUT_THREAD_ID_01}
 * 3. Call `out_char()` with the argument `'.'`.
 *    \llrref{LLR_GDBSERVER_OUT_THREAD_ID_01}
 * 4. Call `out_hex()` with the argument `t->tid`.
 *    \llrref{LLR_GDBSERVER_OUT_THREAD_ID_01}
 *
 * @calls
 * - `out_char()`
 * - `out_hex()`
 */
static void out_thread_id(const gdb_thread_t* t)
{
    out_char('p');
    out_hex(t->pid);
    out_char('.');
    out_hex(t->tid);
}

/**
 * @funcname `out_print_cb`
 *
 * @description
 * The print callback of the partitions' thread descriptions: adds the bytes in
 * hex.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `data` | `const void *` | [in] The bytes. |
 * | `len` | `size_t` | [in] Their number. |
 * | `cb_data` | `void *` | [in] Not used. |
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `bytes` | `const uint8_t *` | The bytes |
 *
 * @hlr
 * - \hlrid{HLR_GDBSERVER_OUT_PRINT_CB_01}: `out_print_cb()` shall add the
 *   description of a thread, which a partition gives, to the reply packet in
 *   hexadecimal.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_DBG_002}
 *
 * @llr
 * - \llrid{LLR_GDBSERVER_OUT_PRINT_CB_01}: `out_print_cb()` shall call
 *   `out_bytes()` once with `data` and `len`.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDBSERVER_OUT_PRINT_CB_01}
 *
 * @design
 * 1. Set `bytes` to `data`. \llrref{LLR_GDBSERVER_OUT_PRINT_CB_01}
 * 2. Call `out_bytes()` with the arguments `bytes` and `len`.
 *    \llrref{LLR_GDBSERVER_OUT_PRINT_CB_01}
 *
 * @calls
 * - `out_bytes()`
 */
static void out_print_cb(const void* data, size_t len, void* cb_data)
{
    const uint8_t* bytes = data;

    (void) cb_data;
    out_bytes(bytes, len);
}

/***********************************************************************
 * Parsing the packet.
 */

/**
 * @funcname `in_peek`
 *
 * @description
 * Returns the next character of the packet received, without reading it.
 *
 * @return The character, or NUL at the end.
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `gdb_in` | `char[4097]` | The packet received |
 * | `gdb_in_len` | `size_t` | The length of the packet received |
 * | `gdb_in_pos` | `size_t` | The position read in the packet received |
 *
 * @locals
 * None.
 *
 * @hlr
 * - \hlrid{HLR_GDBSERVER_IN_PEEK_01}: `in_peek()` shall give the next character
 *   of the packet received without reading it, or NUL at its end.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_DBG_001}
 *
 * @llr
 * - \llrid{LLR_GDBSERVER_IN_PEEK_01}: If `gdb_in_pos` is less than
 *   `gdb_in_len`, `in_peek()` shall return `gdb_in[gdb_in_pos]`.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDBSERVER_IN_PEEK_01}
 * - \llrid{LLR_GDBSERVER_IN_PEEK_02}: If `gdb_in_pos` is not less than
 *   `gdb_in_len`, `in_peek()` shall return `'\0'`.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDBSERVER_IN_PEEK_01}
 *
 * @design
 * 1. Return `gdb_in[gdb_in_pos]` if `gdb_in_pos` is less than `gdb_in_len`,
 *    otherwise `'\0'`. \llrref{LLR_GDBSERVER_IN_PEEK_01},
 *    \llrref{LLR_GDBSERVER_IN_PEEK_02}
 *
 * @calls
 * None.
 */
static char in_peek(void)
{
    return (gdb_in_pos < gdb_in_len) ? gdb_in[gdb_in_pos] : '\0';
}

/**
 * @funcname `in_skip`
 *
 * @description
 * Skips the next character of the packet received.
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `gdb_in_len` | `size_t` | The length of the packet received |
 * | `gdb_in_pos` | `size_t` | The position read in the packet received |
 *
 * @locals
 * None.
 *
 * @hlr
 * - \hlrid{HLR_GDBSERVER_IN_SKIP_01}: `in_skip()` shall skip the next character
 *   of the packet received.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_DBG_001}
 *
 * @llr
 * - \llrid{LLR_GDBSERVER_IN_SKIP_01}: If and only if `gdb_in_pos` is less than
 *   `gdb_in_len`, `in_skip()` shall increment `gdb_in_pos`.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDBSERVER_IN_SKIP_01}
 *
 * @design
 * 1. If `gdb_in_pos` is less than `gdb_in_len`:
 *    \llrref{LLR_GDBSERVER_IN_SKIP_01}
 *    1. Increment `gdb_in_pos`.
 *
 * @calls
 * None.
 */
static void in_skip(void)
{
    if (gdb_in_pos < gdb_in_len) {
        gdb_in_pos++;
    }
}

/**
 * @funcname `in_expect`
 *
 * @description
 * Reads a character of the packet received if it is the one expected.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `c` | `char` | [in] The character expected. |
 *
 * @return TRUE if it was there.
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `gdb_in` | `char[4097]` | The packet received |
 * | `gdb_in_len` | `size_t` | The length of the packet received |
 * | `gdb_in_pos` | `size_t` | The position read in the packet received |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `found` | `pok_bool_t` | Whether it was there |
 *
 * @hlr
 * - \hlrid{HLR_GDBSERVER_IN_EXPECT_01}: `in_expect()` shall read the next
 *   character of the packet received if it is the one expected, and tell
 *   whether it was.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_DBG_001}
 *
 * @llr
 * - \llrid{LLR_GDBSERVER_IN_EXPECT_01}: If `gdb_in_pos` is less than
 *   `gdb_in_len` and `gdb_in[gdb_in_pos]` is `c`, `in_expect()` shall increment
 *   `gdb_in_pos` and return true.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDBSERVER_IN_EXPECT_01}
 * - \llrid{LLR_GDBSERVER_IN_EXPECT_02}: If `gdb_in_pos` is not less than
 *   `gdb_in_len`, or `gdb_in[gdb_in_pos]` is not `c`, `in_expect()` shall
 *   return false and leave `gdb_in_pos` as it is.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDBSERVER_IN_EXPECT_01}
 *
 * @design
 * 1. Set `found` to `(gdb_in_pos < gdb_in_len) && (gdb_in[gdb_in_pos] == c)`.
 *    \llrref{LLR_GDBSERVER_IN_EXPECT_01}, \llrref{LLR_GDBSERVER_IN_EXPECT_02}
 * 2. If `found` is true: \llrref{LLR_GDBSERVER_IN_EXPECT_01},
 *    \llrref{LLR_GDBSERVER_IN_EXPECT_02}
 *    1. Increment `gdb_in_pos`. \llrref{LLR_GDBSERVER_IN_EXPECT_01}
 * 3. Return `found`. \llrref{LLR_GDBSERVER_IN_EXPECT_01},
 *    \llrref{LLR_GDBSERVER_IN_EXPECT_02}
 *
 * @calls
 * None.
 */
static pok_bool_t in_expect(char c)
{
    pok_bool_t found = (gdb_in_pos < gdb_in_len) && (gdb_in[gdb_in_pos] == c);

    if (found) {
        gdb_in_pos++;
    }

    return found;
}

/**
 * @funcname `in_prefix`
 *
 * @description
 * Reads a string of the packet received if the packet goes on with it.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `prefix` | `const char *` | [in] The string. |
 *
 * @return TRUE if it was there.
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `gdb_in` | `char[4097]` | The packet received |
 * | `gdb_in_len` | `size_t` | The length of the packet received |
 * | `gdb_in_pos` | `size_t` | The position read in the packet received |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `found` | `pok_bool_t` | Whether it was there |
 * | `len` | `size_t` | Its length |
 *
 * @hlr
 * - \hlrid{HLR_GDBSERVER_IN_PREFIX_01}: `in_prefix()` shall read a string from
 *   the packet received if the packet continues with it, and tell whether it
 *   did.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_DBG_001}
 *
 * @llr
 * - \llrid{LLR_GDBSERVER_IN_PREFIX_01}: If at least as many characters as the
 *   length of `prefix` (`strlen()`) remain from `gdb_in_pos` to `gdb_in_len`
 *   and `strncmp()` returns 0 for them and `prefix`, `in_prefix()` shall add
 *   that length to `gdb_in_pos` and return true.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDBSERVER_IN_PREFIX_01}
 * - \llrid{LLR_GDBSERVER_IN_PREFIX_02}: If fewer characters than the length of
 *   `prefix` remain from `gdb_in_pos` to `gdb_in_len`, or `strncmp()` returns a
 *   value other than 0 for them and `prefix`, `in_prefix()` shall return false
 *   and leave `gdb_in_pos` as it is.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDBSERVER_IN_PREFIX_01}
 *
 * @design
 * 1. Call `strlen()` with the argument `prefix`, and store the result in `len`.
 *    \llrref{LLR_GDBSERVER_IN_PREFIX_01}, \llrref{LLR_GDBSERVER_IN_PREFIX_02}
 * 2. Set `found` to
 *    `((gdb_in_len - gdb_in_pos) >= len) && (strncmp(&gdb_in[gdb_in_pos],`
 *    `prefix, len) == 0)`. \llrref{LLR_GDBSERVER_IN_PREFIX_01},
 *    \llrref{LLR_GDBSERVER_IN_PREFIX_02}
 * 3. If `found` is true: \llrref{LLR_GDBSERVER_IN_PREFIX_01},
 *    \llrref{LLR_GDBSERVER_IN_PREFIX_02}
 *    1. Add `len` to `gdb_in_pos`. \llrref{LLR_GDBSERVER_IN_PREFIX_01}
 * 4. Return `found`. \llrref{LLR_GDBSERVER_IN_PREFIX_01},
 *    \llrref{LLR_GDBSERVER_IN_PREFIX_02}
 *
 * @calls
 * - `strlen()`
 * - `strncmp()`
 */
static pok_bool_t in_prefix(const char* prefix)
{
    size_t len = strlen(prefix);
    pok_bool_t found = ((gdb_in_len - gdb_in_pos) >= len)
        && (strncmp(&gdb_in[gdb_in_pos], prefix, len) == 0);

    if (found) {
        gdb_in_pos += len;
    }

    return found;
}

/**
 * @funcname `in_hex`
 *
 * @description
 * Reads a hex number of the packet received.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `value` | `uintptr_t *` | [out] Where the number is stored. |
 *
 * @return TRUE if there were digits and no overflow.
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `gdb_in_pos` | `size_t` | The position read in the packet received |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `digit` | `int32_t` | The value of the digit |
 * | `digits` | `uint32_t` | The number of digits |
 * | `overflow` | `pok_bool_t` | Whether it overflowed |
 * | `v` | `uintptr_t` | The number |
 *
 * @hlr
 * - \hlrid{HLR_GDBSERVER_IN_HEX_01}: `in_hex()` shall read a hexadecimal number
 *   from the packet received, and tell whether it had digits and fit in an
 *   address.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_DBG_001}
 *   - \srqref{SRQ_JET_SYS_005}
 *
 * @llr
 * - \llrid{LLR_GDBSERVER_IN_HEX_01}: `in_hex()` shall read the hexadecimal
 *   digits from `gdb_in_pos` on (the characters for which `gdb_hex_value()` of
 *   `in_peek()` is not less than 0), advance `gdb_in_pos` past them, and store
 *   at `value` the number that they make, with only its low bits if it does not
 *   fit.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDBSERVER_IN_HEX_01}
 * - \llrid{LLR_GDBSERVER_IN_HEX_02}: If there is at least one digit and no
 *   digit is added to a value greater than the largest `uintptr_t` shifted
 *   right by 4 bits, `in_hex()` shall return true.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDBSERVER_IN_HEX_01}
 * - \llrid{LLR_GDBSERVER_IN_HEX_03}: If there is no digit, or a digit is added
 *   to a value greater than the largest `uintptr_t` shifted right by 4 bits,
 *   `in_hex()` shall return false.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDBSERVER_IN_HEX_01}
 *
 * @design
 * 1. Set `v` to `0U`. \llrref{LLR_GDBSERVER_IN_HEX_01}
 * 2. Set `digits` to `0U`. \llrref{LLR_GDBSERVER_IN_HEX_01}
 * 3. Set `overflow` to `FALSE`. \llrref{LLR_GDBSERVER_IN_HEX_01}
 * 4. Call `gdb_hex_value()` with the argument `(int32_t)in_peek()`, and store
 *    the result in `digit`. \llrref{LLR_GDBSERVER_IN_HEX_01}
 * 5. While `digit` is greater than or equal to `0`:
 *    \llrref{LLR_GDBSERVER_IN_HEX_01}
 *    1. If `v` is greater than `(~(uintptr_t)0U >> 4)`:
 *       \llrref{LLR_GDBSERVER_IN_HEX_02}, \llrref{LLR_GDBSERVER_IN_HEX_03}
 *       1. Set `overflow` to `TRUE`.
 *    2. Set `v` to `(v << 4) | (uintptr_t)digit`.
 *    3. Increment `digits`.
 *    4. Increment `gdb_in_pos`.
 *    5. Call `gdb_hex_value()` with the argument `(int32_t)in_peek()`, and
 *       store the result in `digit`.
 * 6. Set `*value` to `v`. \llrref{LLR_GDBSERVER_IN_HEX_01}
 * 7. Return `(digits != 0U) && !overflow`. \llrref{LLR_GDBSERVER_IN_HEX_02},
 *    \llrref{LLR_GDBSERVER_IN_HEX_03}
 *
 * @calls
 * - `gdb_hex_value()`
 * - `in_peek()`
 */
static pok_bool_t in_hex(uintptr_t* value)
{
    uintptr_t v = 0U;
    uint32_t digits = 0U;
    pok_bool_t overflow = FALSE;
    int32_t digit = gdb_hex_value((int32_t)in_peek());

    while (digit >= 0)
    {
        if (v > (~(uintptr_t)0U >> 4)) {
            overflow = TRUE;
        }
        v = (v << 4) | (uintptr_t)digit;
        digits++;
        gdb_in_pos++;
        digit = gdb_hex_value((int32_t)in_peek());
    }

    *value = v;
    return (digits != 0U) && !overflow;
}

/**
 * @funcname `in_bytes`
 *
 * @description
 * Reads bytes in hex from the packet received.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `data` | `uint8_t *` | [out] Where the bytes are stored. |
 * | `len` | `size_t` | [in] Their number. |
 *
 * @return TRUE if they were all there.
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `gdb_in` | `char[4097]` | The packet received |
 * | `gdb_in_len` | `size_t` | The length of the packet received |
 * | `gdb_in_pos` | `size_t` | The position read in the packet received |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `high` | `int32_t` | The first digit |
 * | `i` | `size_t` | The index of the byte |
 * | `low` | `int32_t` | The second digit |
 * | `ok` | `pok_bool_t` | Whether they are good so far |
 * | `value` | `int32_t` | The byte |
 *
 * @hlr
 * - \hlrid{HLR_GDBSERVER_IN_BYTES_01}: `in_bytes()` shall read bytes in
 *   hexadecimal from the packet received, and tell whether they were all there.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_DBG_001}
 *   - \srqref{SRQ_JET_DBG_004}
 *
 * @llr
 * - \llrid{LLR_GDBSERVER_IN_BYTES_01}: If fewer than 2 times `len` characters
 *   remain from `gdb_in_pos` to `gdb_in_len`, `in_bytes()` shall return false
 *   without reading them.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDBSERVER_IN_BYTES_01}
 * - \llrid{LLR_GDBSERVER_IN_BYTES_02}: If at least 2 times `len` characters
 *   remain, `in_bytes()` shall, for each `i` less than `len` for which
 *   `gdb_hex_value()` is not less than 0 for the next two characters of
 *   `gdb_in` and for those of the bytes before, store at `data[i]` the byte of
 *   these two characters (the value of the first times 16 plus the value of the
 *   second) and add 2 to `gdb_in_pos`.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDBSERVER_IN_BYTES_01}
 * - \llrid{LLR_GDBSERVER_IN_BYTES_03}: If at least 2 times `len` characters
 *   remain and `gdb_hex_value()` is not less than 0 for each of the next 2
 *   times `len` characters of `gdb_in`, `in_bytes()` shall return true.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDBSERVER_IN_BYTES_01}
 * - \llrid{LLR_GDBSERVER_IN_BYTES_04}: If `gdb_hex_value()` is less than 0 for
 *   one of the two characters of a byte, `in_bytes()` shall return false
 *   without storing that byte or the bytes after it.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDBSERVER_IN_BYTES_01}
 *
 * @design
 * 1. Set `ok` to `(gdb_in_len - gdb_in_pos) >= (2U * len)`.
 *    \llrref{LLR_GDBSERVER_IN_BYTES_01}, \llrref{LLR_GDBSERVER_IN_BYTES_02},
 *    \llrref{LLR_GDBSERVER_IN_BYTES_03}
 * 2. Set `i` to `0U`; then while `ok` is true and `i` is less than `len`,
 *    repeat (after each pass: increment `i`):
 *    \llrref{LLR_GDBSERVER_IN_BYTES_02}, \llrref{LLR_GDBSERVER_IN_BYTES_03},
 *    \llrref{LLR_GDBSERVER_IN_BYTES_04}
 *    1. Call `gdb_hex_value()` with the argument `(int32_t)gdb_in[gdb_in_pos]`,
 *       and store the result in `high`.
 *    2. Call `gdb_hex_value()` with the argument
 *       `(int32_t)gdb_in[gdb_in_pos + 1U]`, and store the result in `low`.
 *    3. If `high` is greater than or equal to `0` and `low` is greater than or
 *       equal to `0`: \llrref{LLR_GDBSERVER_IN_BYTES_02}
 *       1. Set `value` to `(high * 16) + low`.
 *       2. Set `data[i]` to `(uint8_t)value`.
 *       3. Add `2U` to `gdb_in_pos`.
 *    4. Otherwise: \llrref{LLR_GDBSERVER_IN_BYTES_04}
 *       1. Set `ok` to `FALSE`.
 * 3. Return `ok`. \llrref{LLR_GDBSERVER_IN_BYTES_01},
 *    \llrref{LLR_GDBSERVER_IN_BYTES_03}, \llrref{LLR_GDBSERVER_IN_BYTES_04}
 *
 * @calls
 * - `gdb_hex_value()`
 */
static pok_bool_t in_bytes(uint8_t* data, size_t len)
{
    pok_bool_t ok = (gdb_in_len - gdb_in_pos) >= (2U * len);

    for (size_t i = 0U; ok && (i < len); i++)
    {
        int32_t high = gdb_hex_value((int32_t)gdb_in[gdb_in_pos]);
        int32_t low = gdb_hex_value((int32_t)gdb_in[gdb_in_pos + 1U]);

        if ((high >= 0) && (low >= 0))
        {
            int32_t value = (high * 16) + low;

            data[i] = (uint8_t)value;
            gdb_in_pos += 2U;
        }
        else
        {
            ok = FALSE;
        }
    }

    return ok;
}

/**
 * @funcname `in_id`
 *
 * @description
 * Reads a process or thread ID of the packet received (-1 means all).
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `id` | `int64_t *` | [out] Where the ID is stored. |
 *
 * @return TRUE if it is good.
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `ok` | `pok_bool_t` | Whether it is good |
 * | `value` | `uintptr_t` | The number read |
 *
 * @hlr
 * - \hlrid{HLR_GDBSERVER_IN_ID_01}: `in_id()` shall read a process or thread
 *   identifier from the packet received, -1 meaning all, and tell whether it is
 *   good.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_DBG_002}
 *
 * @llr
 * - \llrid{LLR_GDBSERVER_IN_ID_01}: If `in_expect()` returns true for `'-'`,
 *   and `in_hex()` returns true and gives 1, `in_id()` shall store -1 at `id`
 *   and return true.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDBSERVER_IN_ID_01}
 * - \llrid{LLR_GDBSERVER_IN_ID_02}: If `in_expect()` returns true for `'-'`,
 *   and `in_hex()` returns false or gives a value other than 1, `in_id()` shall
 *   store -1 at `id` and return false.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDBSERVER_IN_ID_01}
 * - \llrid{LLR_GDBSERVER_IN_ID_03}: If `in_expect()` returns false for `'-'`,
 *   and `in_hex()` returns true and gives a value that is not greater than
 *   `GDB_NO_INDEX`, `in_id()` shall store that value at `id` and return true.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDBSERVER_IN_ID_01}
 * - \llrid{LLR_GDBSERVER_IN_ID_04}: If `in_expect()` returns false for `'-'`,
 *   and `in_hex()` returns false or gives a value greater than `GDB_NO_INDEX`,
 *   `in_id()` shall store the value that `in_hex()` gives at `id` and return
 *   false.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDBSERVER_IN_ID_01}
 *
 * @design
 * 1. If `in_expect('-')` is true: \llrref{LLR_GDBSERVER_IN_ID_01},
 *    \llrref{LLR_GDBSERVER_IN_ID_02}
 *    1. Set `ok` to `in_hex(&value) && (value == 1U)`.
 *    2. Set `*id` to `-1`.
 * 2. Otherwise: \llrref{LLR_GDBSERVER_IN_ID_03},
 *    \llrref{LLR_GDBSERVER_IN_ID_04}
 *    1. Set `ok` to `in_hex(&value) && (value <= GDB_NO_INDEX)`.
 *    2. Set `*id` to `(int64_t)value`.
 * 3. Return `ok`. \llrref{LLR_GDBSERVER_IN_ID_01},
 *    \llrref{LLR_GDBSERVER_IN_ID_02}, \llrref{LLR_GDBSERVER_IN_ID_03},
 *    \llrref{LLR_GDBSERVER_IN_ID_04}
 *
 * @calls
 * - `in_expect()`
 * - `in_hex()`
 */
static pok_bool_t in_id(int64_t* id)
{
    uintptr_t value;
    pok_bool_t ok;

    if (in_expect('-'))
    {
        ok = in_hex(&value) && (value == 1U);
        *id = -1;
    }
    else
    {
        ok = in_hex(&value) && (value <= GDB_NO_INDEX);
        *id = (int64_t)value;
    }

    return ok;
}

/**
 * @funcname `in_thread_ref`
 *
 * @description
 * Reads a thread reference of the packet received: `p<pid>.<tid>`,
 * `p<pid>` (all its threads) or `<tid>`.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `ref` | `gdb_thread_ref_t *` | [out] Where the reference is stored. |
 *
 * @return TRUE if it is good.
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `ok` | `pok_bool_t` | Whether it is good |
 *
 * @hlr
 * - \hlrid{HLR_GDBSERVER_IN_THREAD_REF_01}: `in_thread_ref()` shall read a
 *   thread reference from the packet received: `p<pid>.<tid>`, `p<pid>` for all
 *   its threads, or `<tid>`.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_DBG_002}
 *
 * @llr
 * - \llrid{LLR_GDBSERVER_IN_THREAD_REF_01}: If `in_expect()` returns false for
 *   `'p'`, `in_thread_ref()` shall set `pid` of `ref` to 0 and return the
 *   result of `in_id()` for `tid` of `ref`.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDBSERVER_IN_THREAD_REF_01}
 * - \llrid{LLR_GDBSERVER_IN_THREAD_REF_02}: If `in_expect()` returns true for
 *   `'p'` and `in_id()` returns false for `pid` of `ref`, `in_thread_ref()`
 *   shall set `tid` of `ref` to 0 and return false.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDBSERVER_IN_THREAD_REF_01}
 * - \llrid{LLR_GDBSERVER_IN_THREAD_REF_03}: If `in_expect()` returns true for
 *   `'p'`, `in_id()` returns true for `pid` of `ref` and `in_expect()` returns
 *   true for `'.'`, `in_thread_ref()` shall return the result of `in_id()` for
 *   `tid` of `ref`.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDBSERVER_IN_THREAD_REF_01}
 * - \llrid{LLR_GDBSERVER_IN_THREAD_REF_04}: If `in_expect()` returns true for
 *   `'p'`, `in_id()` returns true for `pid` of `ref` and `in_expect()` returns
 *   false for `'.'`, `in_thread_ref()` shall set `tid` of `ref` to -1 and
 *   return true.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDBSERVER_IN_THREAD_REF_01}
 *
 * @design
 * 1. Set `ref->pid` to `0`. \llrref{LLR_GDBSERVER_IN_THREAD_REF_01},
 *    \llrref{LLR_GDBSERVER_IN_THREAD_REF_02},
 *    \llrref{LLR_GDBSERVER_IN_THREAD_REF_03},
 *    \llrref{LLR_GDBSERVER_IN_THREAD_REF_04}
 * 2. Set `ref->tid` to `0`. \llrref{LLR_GDBSERVER_IN_THREAD_REF_01},
 *    \llrref{LLR_GDBSERVER_IN_THREAD_REF_02},
 *    \llrref{LLR_GDBSERVER_IN_THREAD_REF_03},
 *    \llrref{LLR_GDBSERVER_IN_THREAD_REF_04}
 * 3. If `in_expect('p')` is true: \llrref{LLR_GDBSERVER_IN_THREAD_REF_02},
 *    \llrref{LLR_GDBSERVER_IN_THREAD_REF_03},
 *    \llrref{LLR_GDBSERVER_IN_THREAD_REF_04}
 *    1. Call `in_id()` with the address of `ref->pid`, and store the result in
 *       `ok`.
 *    2. If `ok` is true:
 *       1. If `in_expect('.')` is true: \llrref{LLR_GDBSERVER_IN_THREAD_REF_03}
 *          1. Call `in_id()` with the address of `ref->tid`, and store the
 *             result in `ok`.
 *       2. Otherwise: \llrref{LLR_GDBSERVER_IN_THREAD_REF_04}
 *          1. Set `ref->tid` to `-1`.
 * 4. Otherwise: \llrref{LLR_GDBSERVER_IN_THREAD_REF_01}
 *    1. Call `in_id()` with the address of `ref->tid`, and store the result in
 *       `ok`.
 * 5. Return `ok`. \llrref{LLR_GDBSERVER_IN_THREAD_REF_01},
 *    \llrref{LLR_GDBSERVER_IN_THREAD_REF_02},
 *    \llrref{LLR_GDBSERVER_IN_THREAD_REF_03},
 *    \llrref{LLR_GDBSERVER_IN_THREAD_REF_04}
 *
 * @calls
 * - `in_expect()`
 * - `in_id()`
 */
static pok_bool_t in_thread_ref(gdb_thread_ref_t* ref)
{
    pok_bool_t ok;

    ref->pid = 0;
    ref->tid = 0;

    if (in_expect('p'))
    {
        ok = in_id(&ref->pid);
        if (ok)
        {
            if (in_expect('.')) {
                ok = in_id(&ref->tid);
            } else {
                ref->tid = -1; // All threads of the process.
            }
        }
    }
    else
    {
        ok = in_id(&ref->tid);
    }

    return ok;
}

/**
 * @funcname `in_has_feature`
 *
 * @description
 * Tells whether the rest of the packet received names a feature (in a
 * qSupported list).
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `feature` | `const char *` | [in] The feature. |
 *
 * @return TRUE if it is there.
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `gdb_in` | `char[4097]` | The packet received |
 * | `gdb_in_len` | `size_t` | The length of the packet received |
 * | `gdb_in_pos` | `size_t` | The position read in the packet received |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `end` | `size_t` | Its end |
 * | `found` | `pok_bool_t` | Whether it is there |
 * | `len` | `size_t` | The feature's length |
 * | `pos` | `size_t` | The position looked at |
 * | `start` | `size_t` | The start of a feature in the list |
 *
 * @hlr
 * - \hlrid{HLR_GDBSERVER_IN_HAS_FEATURE_01}: `in_has_feature()` shall tell
 *   whether the rest of the packet received names a feature in its list of
 *   features.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_DBG_001}
 *
 * @llr
 * - \llrid{LLR_GDBSERVER_IN_HAS_FEATURE_01}: If, after a `':'` or a `';'` of
 *   `gdb_in` from `gdb_in_pos` on, the characters of the length of `feature`
 *   (`strlen()`) end at or before `gdb_in_len`, `strncmp()` returns 0 for them
 *   and `feature`, and they end at `gdb_in_len` or a `';'` follows them,
 *   `in_has_feature()` shall return `TRUE`.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDBSERVER_IN_HAS_FEATURE_01}
 * - \llrid{LLR_GDBSERVER_IN_HAS_FEATURE_02}: If, after every `':'` and `';'` of
 *   `gdb_in` from `gdb_in_pos` on, the characters of the length of `feature`
 *   end after `gdb_in_len`, or `strncmp()` returns a value other than 0 for
 *   them and `feature`, or a character other than `';'` follows them,
 *   `in_has_feature()` shall return `FALSE`.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDBSERVER_IN_HAS_FEATURE_01}
 *
 * @design
 * 1. Call `strlen()` with the argument `feature`, and store the result in
 *    `len`. \llrref{LLR_GDBSERVER_IN_HAS_FEATURE_01},
 *    \llrref{LLR_GDBSERVER_IN_HAS_FEATURE_02}
 * 2. Set `found` to `FALSE`. \llrref{LLR_GDBSERVER_IN_HAS_FEATURE_01},
 *    \llrref{LLR_GDBSERVER_IN_HAS_FEATURE_02}
 * 3. Set `pos` to `gdb_in_pos`. \llrref{LLR_GDBSERVER_IN_HAS_FEATURE_01},
 *    \llrref{LLR_GDBSERVER_IN_HAS_FEATURE_02}
 * 4. While `pos` is less than `gdb_in_len`:
 *    \llrref{LLR_GDBSERVER_IN_HAS_FEATURE_01},
 *    \llrref{LLR_GDBSERVER_IN_HAS_FEATURE_02}
 *    1. If `gdb_in[pos]` is equal to `':'` or `gdb_in[pos]` is equal to `';'`:
 *       1. Set `start` to `pos + 1U`.
 *       2. Set `end` to `start + len`.
 *       3. If `end` is less than or equal to `gdb_in_len` and
 *          `strncmp(&gdb_in[start], feature, len)` is equal to `0` and (`end`
 *          is equal to `gdb_in_len` or `gdb_in[end]` is equal to `';'`):
 *          1. Set `found` to `TRUE`. \llrref{LLR_GDBSERVER_IN_HAS_FEATURE_01}
 *    2. Increment `pos`.
 * 5. Return `found`. \llrref{LLR_GDBSERVER_IN_HAS_FEATURE_01},
 *    \llrref{LLR_GDBSERVER_IN_HAS_FEATURE_02}
 *
 * @calls
 * - `strlen()`
 * - `strncmp()`
 */
static pok_bool_t in_has_feature(const char* feature)
{
    size_t len = strlen(feature);
    pok_bool_t found = FALSE;
    size_t pos = gdb_in_pos;

    // Features are separated by ';' and follow ':'.
    while (pos < gdb_in_len)
    {
        if ((gdb_in[pos] == ':') || (gdb_in[pos] == ';'))
        {
            size_t start = pos + 1U;
            size_t end = start + len;

            if ((end <= gdb_in_len)
                && (strncmp(&gdb_in[start], feature, len) == 0)
                && ((end == gdb_in_len) || (gdb_in[end] == ';')))
            {
                found = TRUE;
            }
        }
        pos++;
    }

    return found;
}

/***********************************************************************
 * Session.
 */

/**
 * @funcname `gdb_session_start`
 *
 * @description
 * Starts a GDB session at its first packet: no pending stop reply or Ctrl-C, no
 * features, attached to the current process only.
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `gdb_current` | `gdb_thread_t` | The thread that stopped the system |
 * | `gdb_hwbreak_feature` | `pok_bool_t` | Whether GDB supports hwbreak stop replies |
 * | `gdb_interrupt_pending` | `pok_bool_t` | Whether GDB sent Ctrl-C |
 * | `gdb_session` | `pok_bool_t` | Whether a GDB session is open |
 * | `gdb_stop_reply_pending` | `pok_bool_t` | Whether a stop reply is owed to GDB |
 * | `gdb_swbreak_feature` | `pok_bool_t` | Whether GDB supports swbreak stop replies |
 *
 * @locals
 * None.
 *
 * @hlr
 * - \hlrid{HLR_GDBSERVER_GDB_SESSION_START_01}: `gdb_session_start()` shall
 *   start a session of the debugger at its first packet: no stop reply owed, no
 *   interrupt pending, no stop reason features, and the debugger attached to
 *   the current process only.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_DBG_007}
 *
 * @llr
 * - \llrid{LLR_GDBSERVER_GDB_SESSION_START_01}: `gdb_session_start()` shall set
 *   `gdb_session` to `TRUE` and `gdb_stop_reply_pending`,
 *   `gdb_interrupt_pending`, `gdb_swbreak_feature` and `gdb_hwbreak_feature` to
 *   `FALSE`, and then call `gdb_detach_all()` and `gdb_attach()` with
 *   `gdb_current.pid`.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDBSERVER_GDB_SESSION_START_01}
 *
 * @design
 * 1. Set `gdb_session` to `TRUE`. \llrref{LLR_GDBSERVER_GDB_SESSION_START_01}
 * 2. Set `gdb_stop_reply_pending` to `FALSE`.
 *    \llrref{LLR_GDBSERVER_GDB_SESSION_START_01}
 * 3. Set `gdb_interrupt_pending` to `FALSE`.
 *    \llrref{LLR_GDBSERVER_GDB_SESSION_START_01}
 * 4. Set `gdb_swbreak_feature` to `FALSE`.
 *    \llrref{LLR_GDBSERVER_GDB_SESSION_START_01}
 * 5. Set `gdb_hwbreak_feature` to `FALSE`.
 *    \llrref{LLR_GDBSERVER_GDB_SESSION_START_01}
 * 6. Call `gdb_detach_all()`. \llrref{LLR_GDBSERVER_GDB_SESSION_START_01}
 * 7. Call `gdb_attach()` with the argument `gdb_current.pid`.
 *    \llrref{LLR_GDBSERVER_GDB_SESSION_START_01}
 *
 * @calls
 * - `gdb_detach_all()`
 * - `gdb_attach()`
 */
static void gdb_session_start(void)
{
    gdb_session = TRUE;
    gdb_stop_reply_pending = FALSE;
    gdb_interrupt_pending = FALSE;
    gdb_swbreak_feature = FALSE;
    gdb_hwbreak_feature = FALSE;
    gdb_detach_all();
    gdb_attach(gdb_current.pid);
}

/**
 * @funcname `gdb_session_end`
 *
 * @description
 * Ends the GDB session: removes the breakpoints and watchpoints, detaches from
 * every process and cancels a pending step.
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `gdb_interrupt_pending` | `pok_bool_t` | Whether GDB sent Ctrl-C |
 * | `gdb_session` | `pok_bool_t` | Whether a GDB session is open |
 * | `gdb_step_cancelled` | `pok_bool_t` | Whether the pending step was cancelled |
 * | `gdb_stop_reply_pending` | `pok_bool_t` | Whether a stop reply is owed to GDB |
 *
 * @locals
 * None.
 *
 * @hlr
 * - \hlrid{HLR_GDBSERVER_GDB_SESSION_END_01}: `gdb_session_end()` shall end the
 *   session of the debugger: remove all its breakpoints and watchpoints, detach
 *   it from every process and cancel a step in progress.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_DBG_015}
 *
 * @llr
 * - \llrid{LLR_GDBSERVER_GDB_SESSION_END_01}: `gdb_session_end()` shall call,
 *   in this order, `gdb_sw_remove_all()`, `ja_gdb_hw_remove_all()` and
 *   `gdb_detach_all()`, and then set `gdb_session`, `gdb_stop_reply_pending`
 *   and `gdb_interrupt_pending` to `FALSE` and `gdb_step_cancelled` to `TRUE`.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDBSERVER_GDB_SESSION_END_01}
 *
 * @design
 * 1. Call `gdb_sw_remove_all()`. \llrref{LLR_GDBSERVER_GDB_SESSION_END_01}
 * 2. Call `ja_gdb_hw_remove_all()`. \llrref{LLR_GDBSERVER_GDB_SESSION_END_01}
 * 3. Call `gdb_detach_all()`. \llrref{LLR_GDBSERVER_GDB_SESSION_END_01}
 * 4. Set `gdb_session` to `FALSE`. \llrref{LLR_GDBSERVER_GDB_SESSION_END_01}
 * 5. Set `gdb_stop_reply_pending` to `FALSE`.
 *    \llrref{LLR_GDBSERVER_GDB_SESSION_END_01}
 * 6. Set `gdb_interrupt_pending` to `FALSE`.
 *    \llrref{LLR_GDBSERVER_GDB_SESSION_END_01}
 * 7. Set `gdb_step_cancelled` to `TRUE`.
 *    \llrref{LLR_GDBSERVER_GDB_SESSION_END_01}
 *
 * @calls
 * - `gdb_sw_remove_all()`
 * - `ja_gdb_hw_remove_all()`
 * - `gdb_detach_all()`
 */
static void gdb_session_end(void)
{
    gdb_sw_remove_all();
    ja_gdb_hw_remove_all();
    gdb_detach_all();
    gdb_session = FALSE;
    gdb_stop_reply_pending = FALSE;
    gdb_interrupt_pending = FALSE;
    // A step in progress: ignore its trap.
    gdb_step_cancelled = TRUE;
}

/***********************************************************************
 * Packets.
 */

/**
 * @funcname `gdb_out_stop_reply`
 *
 * @description
 * Writes the stop reply: the signal, the thread, its frame pointer, stack
 * pointer and PC, and the reason (breakpoint or watchpoint); GDB is then
 * attached to the thread's process.
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `gdb_current` | `gdb_thread_t` | The thread that stopped the system |
 * | `gdb_hwbreak_feature` | `pok_bool_t` | Whether GDB supports hwbreak stop replies |
 * | `gdb_stop_addr` | `uintptr_t` | The address of the watchpoint that stopped the system |
 * | `gdb_stop_ctx` | `interrupt_frame *` | The registers of the stopped context |
 * | `gdb_stop_reason` | `jet_gdb_stop_reason_e` | Why the system stopped |
 * | `gdb_stop_signal` | `uint32_t` | The signal of the stop |
 * | `gdb_swbreak_feature` | `pok_bool_t` | Whether GDB supports swbreak stop replies |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `expedited` | `static const uint32_t[3]` | The registers sent with the reply |
 * | `i` | `uint32_t` | The index of a register in expedited |
 * | `regno` | `uint32_t` | The register |
 * | `value` | `uint8_t[16]` | The value of a register |
 *
 * @hlr
 * - \hlrid{HLR_GDBSERVER_GDB_OUT_STOP_REPLY_01}: `gdb_out_stop_reply()` shall
 *   tell the debugger why and where the module stopped: the signal, the thread,
 *   its frame pointer, stack pointer and program counter, and a breakpoint or
 *   watchpoint (with its address) as the reason; and attach the debugger to the
 *   thread's process.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_DBG_014}
 *   - \srqref{SRQ_JET_DBG_006}
 *
 * @llr
 * - \llrid{LLR_GDBSERVER_GDB_OUT_STOP_REPLY_01}: `gdb_out_stop_reply()` shall
 *   call, in this order, `out_char()` with `'T'`, `out_byte()` with
 *   `gdb_stop_signal`, `out_str()` with `"thread:"`, `out_thread_id()` with the
 *   address of `gdb_current`, and `out_char()` with `';'`.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDBSERVER_GDB_OUT_STOP_REPLY_01}
 * - \llrid{LLR_GDBSERVER_GDB_OUT_STOP_REPLY_02}: For each of `JA_GDB_REG_FP`,
 *   `JA_GDB_REG_SP` and `JA_GDB_REG_PC`, in this order, if and only if
 *   `ja_gdb_register_get()` returns true for `gdb_stop_ctx`, NULL, the register
 *   and a buffer, `gdb_out_stop_reply()` shall call `out_byte()` with the
 *   number of the register, `out_char()` with `':'`, `out_bytes()` with the
 *   buffer and the result of `ja_gdb_register_size()` for the register, and
 *   `out_char()` with `';'`.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDBSERVER_GDB_OUT_STOP_REPLY_01}
 * - \llrid{LLR_GDBSERVER_GDB_OUT_STOP_REPLY_03}: If `gdb_stop_reason` is
 *   `JET_GDB_STOP_SW_BREAKPOINT`, `gdb_out_stop_reply()` shall call `out_str()`
 *   with `"swbreak:;"` if and only if `gdb_swbreak_feature` is true.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDBSERVER_GDB_OUT_STOP_REPLY_01}
 * - \llrid{LLR_GDBSERVER_GDB_OUT_STOP_REPLY_04}: If `gdb_stop_reason` is
 *   `JET_GDB_STOP_HW_BREAKPOINT`, `gdb_out_stop_reply()` shall call `out_str()`
 *   with `"hwbreak:;"` if and only if `gdb_hwbreak_feature` is true.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDBSERVER_GDB_OUT_STOP_REPLY_01}
 * - \llrid{LLR_GDBSERVER_GDB_OUT_STOP_REPLY_05}: If `gdb_stop_reason` is
 *   `JET_GDB_STOP_WATCH`, `JET_GDB_STOP_RWATCH` or `JET_GDB_STOP_AWATCH`,
 *   `gdb_out_stop_reply()` shall call `out_str()` with `"watch:"`, `"rwatch:"`
 *   or `"awatch:"`, in this order, and then `out_hex()` with `gdb_stop_addr`
 *   and `out_char()` with `';'`.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDBSERVER_GDB_OUT_STOP_REPLY_01}
 * - \llrid{LLR_GDBSERVER_GDB_OUT_STOP_REPLY_06}: If `gdb_stop_reason` is none
 *   of `JET_GDB_STOP_SW_BREAKPOINT`, `JET_GDB_STOP_HW_BREAKPOINT`,
 *   `JET_GDB_STOP_WATCH`, `JET_GDB_STOP_RWATCH` and `JET_GDB_STOP_AWATCH`,
 *   `gdb_out_stop_reply()` shall add nothing for the reason.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDBSERVER_GDB_OUT_STOP_REPLY_01}
 * - \llrid{LLR_GDBSERVER_GDB_OUT_STOP_REPLY_07}: `gdb_out_stop_reply()` shall
 *   end with a call of `gdb_attach()` with `gdb_current.pid`.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDBSERVER_GDB_OUT_STOP_REPLY_01}
 *
 * @design
 * 1. Call `out_char()` with the argument `'T'`.
 *    \llrref{LLR_GDBSERVER_GDB_OUT_STOP_REPLY_01}
 * 2. Call `out_byte()` with the argument `(uint8_t)gdb_stop_signal`.
 *    \llrref{LLR_GDBSERVER_GDB_OUT_STOP_REPLY_01}
 * 3. Call `out_str()` with the argument `"thread:"`.
 *    \llrref{LLR_GDBSERVER_GDB_OUT_STOP_REPLY_01}
 * 4. Call `out_thread_id()` with the address of `gdb_current`.
 *    \llrref{LLR_GDBSERVER_GDB_OUT_STOP_REPLY_01}
 * 5. Call `out_char()` with the argument `';'`.
 *    \llrref{LLR_GDBSERVER_GDB_OUT_STOP_REPLY_01}
 * 6. Set `i` to `0U`; then while `i` is less than
 *    `(sizeof(expedited) / sizeof(expedited[0]))`, repeat (after each pass:
 *    increment `i`): \llrref{LLR_GDBSERVER_GDB_OUT_STOP_REPLY_02}
 *    1. Set `regno` to `expedited[i]`.
 *    2. If `ja_gdb_register_get(gdb_stop_ctx, NULL, regno, value)` is true:
 *       1. Call `out_byte()` with the argument `(uint8_t)regno`.
 *       2. Call `out_char()` with the argument `':'`.
 *       3. Call `out_bytes()` with the arguments `value` and
 *          `ja_gdb_register_size(regno)`.
 *       4. Call `out_char()` with the argument `';'`.
 * 7. Depending on `gdb_stop_reason`:
 *    \llrref{LLR_GDBSERVER_GDB_OUT_STOP_REPLY_03},
 *    \llrref{LLR_GDBSERVER_GDB_OUT_STOP_REPLY_04},
 *    \llrref{LLR_GDBSERVER_GDB_OUT_STOP_REPLY_05},
 *    \llrref{LLR_GDBSERVER_GDB_OUT_STOP_REPLY_06}
 *    1. Case `JET_GDB_STOP_SW_BREAKPOINT`:
 *       \llrref{LLR_GDBSERVER_GDB_OUT_STOP_REPLY_03}
 *       1. If `gdb_swbreak_feature` is true:
 *          1. Call `out_str()` with the argument `"swbreak:;"`.
 *       2. Leave the switch.
 *    2. Case `JET_GDB_STOP_HW_BREAKPOINT`:
 *       \llrref{LLR_GDBSERVER_GDB_OUT_STOP_REPLY_04}
 *       1. If `gdb_hwbreak_feature` is true:
 *          1. Call `out_str()` with the argument `"hwbreak:;"`.
 *       2. Leave the switch.
 *    3. Case `JET_GDB_STOP_WATCH`: \llrref{LLR_GDBSERVER_GDB_OUT_STOP_REPLY_05}
 *       1. Call `out_str()` with the argument `"watch:"`.
 *       2. Call `out_hex()` with the argument `gdb_stop_addr`.
 *       3. Call `out_char()` with the argument `';'`.
 *       4. Leave the switch.
 *    4. Case `JET_GDB_STOP_RWATCH`:
 *       \llrref{LLR_GDBSERVER_GDB_OUT_STOP_REPLY_05}
 *       1. Call `out_str()` with the argument `"rwatch:"`.
 *       2. Call `out_hex()` with the argument `gdb_stop_addr`.
 *       3. Call `out_char()` with the argument `';'`.
 *       4. Leave the switch.
 *    5. Case `JET_GDB_STOP_AWATCH`:
 *       \llrref{LLR_GDBSERVER_GDB_OUT_STOP_REPLY_05}
 *       1. Call `out_str()` with the argument `"awatch:"`.
 *       2. Call `out_hex()` with the argument `gdb_stop_addr`.
 *       3. Call `out_char()` with the argument `';'`.
 *       4. Leave the switch.
 *    6. Otherwise (default): \llrref{LLR_GDBSERVER_GDB_OUT_STOP_REPLY_06}
 *       1. Leave the switch.
 * 8. Call `gdb_attach()` with the argument `gdb_current.pid`.
 *    \llrref{LLR_GDBSERVER_GDB_OUT_STOP_REPLY_07}
 *
 * @calls
 * - `out_char()`
 * - `out_byte()`
 * - `out_str()`
 * - `out_thread_id()`
 * - `ja_gdb_register_get()`
 * - `out_bytes()`
 * - `ja_gdb_register_size()`
 * - `out_hex()`
 * - `gdb_attach()`
 */
static void gdb_out_stop_reply(void)
{
    uint8_t value[JA_GDB_REGISTER_SIZE_MAX];
    static const uint32_t expedited[] = {
        JA_GDB_REG_FP, JA_GDB_REG_SP, JA_GDB_REG_PC
    };

    out_char('T');
    out_byte((uint8_t)gdb_stop_signal);
    out_str("thread:");
    out_thread_id(&gdb_current);
    out_char(';');

    for (uint32_t i = 0U; i < (sizeof(expedited) / sizeof(expedited[0]));
        i++)
    {
        uint32_t regno = expedited[i];

        if (ja_gdb_register_get(gdb_stop_ctx, NULL, regno, value))
        {
            out_byte((uint8_t)regno);
            out_char(':');
            out_bytes(value, ja_gdb_register_size(regno));
            out_char(';');
        }
    }

    switch (gdb_stop_reason)
    {
    case JET_GDB_STOP_SW_BREAKPOINT:
        if (gdb_swbreak_feature) {
            out_str("swbreak:;");
        }
        break;
    case JET_GDB_STOP_HW_BREAKPOINT:
        if (gdb_hwbreak_feature) {
            out_str("hwbreak:;");
        }
        break;
    case JET_GDB_STOP_WATCH:
        out_str("watch:");
        out_hex(gdb_stop_addr);
        out_char(';');
        break;
    case JET_GDB_STOP_RWATCH:
        out_str("rwatch:");
        out_hex(gdb_stop_addr);
        out_char(';');
        break;
    case JET_GDB_STOP_AWATCH:
        out_str("awatch:");
        out_hex(gdb_stop_addr);
        out_char(';');
        break;
    default:
        // Nothing more to say.
        break;
    }

    // GDB will know this process now.
    gdb_attach(gdb_current.pid);
}

/**
 * @funcname `gdb_cmd_supported`
 *
 * @description
 * Answers qSupported: the packet size, multiprocess, and the stop reasons GDB
 * supports.
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `gdb_hwbreak_feature` | `pok_bool_t` | Whether GDB supports hwbreak stop replies |
 * | `gdb_swbreak_feature` | `pok_bool_t` | Whether GDB supports swbreak stop replies |
 *
 * @locals
 * None.
 *
 * @hlr
 * - \hlrid{HLR_GDBSERVER_GDB_CMD_SUPPORTED_01}: `gdb_cmd_supported()` shall
 *   answer the debugger's list of features with the packet size, multiprocess,
 *   and the stop reasons that both support.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_DBG_001}
 *
 * @llr
 * - \llrid{LLR_GDBSERVER_GDB_CMD_SUPPORTED_01}: `gdb_cmd_supported()` shall set
 *   `gdb_swbreak_feature` to the result of `in_has_feature()` for `"swbreak+"`
 *   and `gdb_hwbreak_feature` to its result for `"hwbreak+"`, and then call
 *   `out_str()` with `"PacketSize="`, `out_hex()` with `GDB_PACKET_SIZE` and
 *   `out_str()` with `";multiprocess+"`.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDBSERVER_GDB_CMD_SUPPORTED_01}
 * - \llrid{LLR_GDBSERVER_GDB_CMD_SUPPORTED_02}: If and only if
 *   `gdb_swbreak_feature` is then true, `gdb_cmd_supported()` shall call
 *   `out_str()` with `";swbreak+"`.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDBSERVER_GDB_CMD_SUPPORTED_01}
 * - \llrid{LLR_GDBSERVER_GDB_CMD_SUPPORTED_03}: If and only if
 *   `gdb_hwbreak_feature` is then true, `gdb_cmd_supported()` shall call
 *   `out_str()` with `";hwbreak+"`.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDBSERVER_GDB_CMD_SUPPORTED_01}
 *
 * @design
 * 1. Call `in_has_feature()` with the argument `"swbreak+"`, and store the
 *    result in `gdb_swbreak_feature`.
 *    \llrref{LLR_GDBSERVER_GDB_CMD_SUPPORTED_01}
 * 2. Call `in_has_feature()` with the argument `"hwbreak+"`, and store the
 *    result in `gdb_hwbreak_feature`.
 *    \llrref{LLR_GDBSERVER_GDB_CMD_SUPPORTED_01}
 * 3. Call `out_str()` with the argument `"PacketSize="`.
 *    \llrref{LLR_GDBSERVER_GDB_CMD_SUPPORTED_01}
 * 4. Call `out_hex()` with the argument `GDB_PACKET_SIZE`.
 *    \llrref{LLR_GDBSERVER_GDB_CMD_SUPPORTED_01}
 * 5. Call `out_str()` with the argument `";multiprocess+"`.
 *    \llrref{LLR_GDBSERVER_GDB_CMD_SUPPORTED_01}
 * 6. If `gdb_swbreak_feature` is true:
 *    \llrref{LLR_GDBSERVER_GDB_CMD_SUPPORTED_02}
 *    1. Call `out_str()` with the argument `";swbreak+"`.
 * 7. If `gdb_hwbreak_feature` is true:
 *    \llrref{LLR_GDBSERVER_GDB_CMD_SUPPORTED_03}
 *    1. Call `out_str()` with the argument `";hwbreak+"`.
 *
 * @calls
 * - `in_has_feature()`
 * - `out_str()`
 * - `out_hex()`
 */
static void gdb_cmd_supported(void)
{
    gdb_swbreak_feature = in_has_feature("swbreak+");
    gdb_hwbreak_feature = in_has_feature("hwbreak+");

    out_str("PacketSize=");
    out_hex(GDB_PACKET_SIZE);
    out_str(";multiprocess+");
    if (gdb_swbreak_feature) {
        out_str(";swbreak+");
    }
    if (gdb_hwbreak_feature) {
        out_str(";hwbreak+");
    }
}

/**
 * @funcname `gdb_cmd_thread_info`
 *
 * @description
 * Answers qfThreadInfo and qsThreadInfo: the next threads of the attached
 * processes (m...), or l at the end.
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `gdb_list_pid` | `uint32_t` | The process of the thread list being sent |
 * | `gdb_list_tid` | `uint32_t` | The thread of the thread list being sent |
 * | `gdb_out_len` | `size_t` | The length of the packet to send |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `more` | `pok_bool_t` | Whether there are more threads |
 * | `t` | `gdb_thread_t` | A thread |
 *
 * @hlr
 * - \hlrid{HLR_GDBSERVER_GDB_CMD_THREAD_INFO_01}: `gdb_cmd_thread_info()` shall
 *   give the debugger the next threads of the processes that it is attached to,
 *   as many as fit in a packet, or the end of the list.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_DBG_002}
 *
 * @llr
 * - \llrid{LLR_GDBSERVER_GDB_CMD_THREAD_INFO_01}: If `gdb_list_next()` returns
 *   false, `gdb_cmd_thread_info()` shall call `out_char()` once with `'l'`.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDBSERVER_GDB_CMD_THREAD_INFO_01}
 * - \llrid{LLR_GDBSERVER_GDB_CMD_THREAD_INFO_02}: If `gdb_list_next()` returns
 *   true, `gdb_cmd_thread_info()` shall call `out_char()` with `'m'` and then,
 *   while there is a next thread and `gdb_out_len` plus 32 is less than
 *   `GDB_PACKET_SIZE`, call `out_thread_id()` with the thread `gdb_list_pid`,
 *   `gdb_list_tid` if `gdb_thread_find()` returns true for it, and call
 *   `gdb_list_next()` again if `gdb_out_len` plus 32 is still less than
 *   `GDB_PACKET_SIZE`.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDBSERVER_GDB_CMD_THREAD_INFO_01}
 * - \llrid{LLR_GDBSERVER_GDB_CMD_THREAD_INFO_03}: If and only if `gdb_out_len`
 *   is greater than 1 before a thread is added, `gdb_cmd_thread_info()` shall
 *   first call `out_char()` with `','`.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDBSERVER_GDB_CMD_THREAD_INFO_01}
 * - \llrid{LLR_GDBSERVER_GDB_CMD_THREAD_INFO_04}: If `gdb_thread_find()`
 *   returns false for a thread of the list, `gdb_cmd_thread_info()` shall add
 *   nothing for it.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDBSERVER_GDB_CMD_THREAD_INFO_01}
 * - \llrid{LLR_GDBSERVER_GDB_CMD_THREAD_INFO_05}: If and only if `gdb_out_len`
 *   is 1 after the list, `gdb_cmd_thread_info()` shall call `out_reset()` and
 *   then `out_char()` with `'l'`.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDBSERVER_GDB_CMD_THREAD_INFO_01}
 *
 * @design
 * 1. Call `gdb_list_next()`, and store the result in `more`.
 *    \llrref{LLR_GDBSERVER_GDB_CMD_THREAD_INFO_01},
 *    \llrref{LLR_GDBSERVER_GDB_CMD_THREAD_INFO_02}
 * 2. If `more` is false: \llrref{LLR_GDBSERVER_GDB_CMD_THREAD_INFO_01}
 *    1. Call `out_char()` with the argument `'l'`.
 * 3. Otherwise: \llrref{LLR_GDBSERVER_GDB_CMD_THREAD_INFO_02},
 *    \llrref{LLR_GDBSERVER_GDB_CMD_THREAD_INFO_03},
 *    \llrref{LLR_GDBSERVER_GDB_CMD_THREAD_INFO_04},
 *    \llrref{LLR_GDBSERVER_GDB_CMD_THREAD_INFO_05}
 *    1. Call `out_char()` with the argument `'m'`.
 *    2. While `more` is true and `(gdb_out_len + 32U)` is less than
 *       `GDB_PACKET_SIZE`: \llrref{LLR_GDBSERVER_GDB_CMD_THREAD_INFO_02},
 *       \llrref{LLR_GDBSERVER_GDB_CMD_THREAD_INFO_03},
 *       \llrref{LLR_GDBSERVER_GDB_CMD_THREAD_INFO_04}
 *       1. If `gdb_thread_find(&t, gdb_list_pid, gdb_list_tid)` is true:
 *          \llrref{LLR_GDBSERVER_GDB_CMD_THREAD_INFO_02},
 *          \llrref{LLR_GDBSERVER_GDB_CMD_THREAD_INFO_04}
 *          1. If `gdb_out_len` is greater than `1U`:
 *             \llrref{LLR_GDBSERVER_GDB_CMD_THREAD_INFO_03}
 *             1. Call `out_char()` with the argument `','`.
 *          2. Call `out_thread_id()` with the address of `t`.
 *             \llrref{LLR_GDBSERVER_GDB_CMD_THREAD_INFO_02}
 *       2. Set `more` to
 *          `((gdb_out_len + 32U) < GDB_PACKET_SIZE) && gdb_list_next()`.
 *          \llrref{LLR_GDBSERVER_GDB_CMD_THREAD_INFO_02}
 *    3. If `gdb_out_len` is equal to `1U`:
 *       \llrref{LLR_GDBSERVER_GDB_CMD_THREAD_INFO_05}
 *       1. Call `out_reset()`.
 *       2. Call `out_char()` with the argument `'l'`.
 *
 * @calls
 * - `gdb_list_next()`
 * - `out_char()`
 * - `gdb_thread_find()`
 * - `out_thread_id()`
 * - `out_reset()`
 */
static void gdb_cmd_thread_info(void)
{
    gdb_thread_t t;
    pok_bool_t more = gdb_list_next();

    if (!more)
    {
        out_char('l');
    }
    else
    {
        out_char('m');
        // A thread id takes at most 21 characters.
        while (more && ((gdb_out_len + 32U) < GDB_PACKET_SIZE))
        {
            if (gdb_thread_find(&t, gdb_list_pid, gdb_list_tid))
            {
                if (gdb_out_len > 1U) {
                    out_char(',');
                }
                out_thread_id(&t);
            }
            more = ((gdb_out_len + 32U) < GDB_PACKET_SIZE)
                && gdb_list_next();
        }

        if (gdb_out_len == 1U)
        {
            // None of the threads left exists: the end of the list.
            out_reset();
            out_char('l');
        }
    }
}

/**
 * @funcname `gdb_cmd_thread_extra_info`
 *
 * @description
 * Answers qThreadExtraInfo: the partition's name and the thread's description,
 * in hex.
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `i` | `size_t` | The index of a character |
 * | `name` | `const char *` | The partition's name |
 * | `ref` | `gdb_thread_ref_t` | The thread reference |
 * | `t` | `gdb_thread_t` | The thread |
 *
 * @hlr
 * - \hlrid{HLR_GDBSERVER_GDB_CMD_THREAD_EXTRA_INFO_01}:
 *   `gdb_cmd_thread_extra_info()` shall give the debugger the name of the
 *   partition and the description of a thread, in hexadecimal.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_DBG_002}
 *
 * @llr
 * - \llrid{LLR_GDBSERVER_GDB_CMD_THREAD_EXTRA_INFO_01}: If `in_thread_ref()`
 *   returns true and `gdb_thread_resolve()` returns true for the reference,
 *   `gdb_cmd_thread_extra_info()` shall call `out_byte()` with each character
 *   of `name` of the partition of the thread before its `'\0'`, at most
 *   `MAX_NAME_LENGTH` of them, then `out_byte()` with `'.'`, and then the
 *   function `get_thread_info` of `part_sched_ops` of the partition with the
 *   partition, the result of `gdb_thread_index()` for the thread, its
 *   `part_private`, `out_print_cb` and NULL.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDBSERVER_GDB_CMD_THREAD_EXTRA_INFO_01}
 * - \llrid{LLR_GDBSERVER_GDB_CMD_THREAD_EXTRA_INFO_02}: If `in_thread_ref()`
 *   returns false, or `gdb_thread_resolve()` returns false for the reference,
 *   `gdb_cmd_thread_extra_info()` shall call `out_str()` with `"E01"`.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDBSERVER_GDB_CMD_THREAD_EXTRA_INFO_01}
 *
 * @design
 * 1. If `in_thread_ref(&ref) && gdb_thread_resolve(&t, &ref)` holds:
 *    \llrref{LLR_GDBSERVER_GDB_CMD_THREAD_EXTRA_INFO_01}
 *    1. Set `name` to `t.part->name`.
 *    2. Set `i` to `0U`; then while `i` is less than `(size_t)MAX_NAME_LENGTH`
 *       and `name[i]` is not equal to `'\0'`, repeat (after each pass:
 *       increment `i`):
 *       1. Call `out_byte()` with the argument `(uint8_t)name[i]`.
 *    3. Call `out_byte()` with the argument `(uint8_t)'.'`.
 *    4. Call the function `t.part->part_sched_ops->get_thread_info` with the
 *       arguments `t.part`, `gdb_thread_index(&t)`, `t.part_private`, the
 *       address of `out_print_cb` and `NULL`.
 * 2. Otherwise: \llrref{LLR_GDBSERVER_GDB_CMD_THREAD_EXTRA_INFO_02}
 *    1. Call `out_str()` with the argument `"E01"`.
 *
 * @calls
 * - `in_thread_ref()`
 * - `gdb_thread_resolve()`
 * - `out_byte()`
 * - `t.part->part_sched_ops->get_thread_info()` (indirect)
 * - `gdb_thread_index()`
 * - `out_str()`
 */
static void gdb_cmd_thread_extra_info(void)
{
    gdb_thread_ref_t ref;
    gdb_thread_t t;

    if (in_thread_ref(&ref) && gdb_thread_resolve(&t, &ref))
    {
        const char* name = t.part->name;

        for (size_t i = 0U; (i < (size_t)MAX_NAME_LENGTH) && (name[i] != '\0');
            i++)
        {
            out_byte((uint8_t)name[i]);
        }
        out_byte((uint8_t)'.');
        t.part->part_sched_ops->get_thread_info(t.part, gdb_thread_index(&t),
            t.part_private, &out_print_cb, NULL);
    }
    else
    {
        out_str("E01");
    }
}

/**
 * @funcname `gdb_cmd_query`
 *
 * @description
 * Answers the q packets: Supported, fThreadInfo, sThreadInfo, ThreadExtraInfo,
 * Attached, Symbol and C; others get an empty reply.
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `gdb_list_pid` | `uint32_t` | The process of the thread list being sent |
 * | `gdb_list_tid` | `uint32_t` | The thread of the thread list being sent |
 * | `gdb_selected` | `gdb_thread_t` | The thread that GDB selected (Hg) |
 *
 * @locals
 * None.
 *
 * @hlr
 * - \hlrid{HLR_GDBSERVER_GDB_CMD_QUERY_01}: `gdb_cmd_query()` shall answer the
 *   query packets: the features, the thread list, the thread descriptions,
 *   whether attached, symbols and the current thread.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_DBG_001}
 *   - \srqref{SRQ_JET_DBG_002}
 * - \hlrid{HLR_GDBSERVER_GDB_CMD_QUERY_02}: `gdb_cmd_query()` shall give an
 *   empty reply to the other query packets.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_DBG_001}
 *
 * @llr
 * - \llrid{LLR_GDBSERVER_GDB_CMD_QUERY_01}: If `in_prefix()` returns true for
 *   `"Supported"`, `gdb_cmd_query()` shall call `gdb_cmd_supported()`.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDBSERVER_GDB_CMD_QUERY_01}
 * - \llrid{LLR_GDBSERVER_GDB_CMD_QUERY_02}: If `in_prefix()` returns true for
 *   `"fThreadInfo"`, `gdb_cmd_query()` shall set `gdb_list_pid` to the result
 *   of `gdb_process_next()` for 0 and `gdb_list_tid` to 0, and call
 *   `gdb_cmd_thread_info()`.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDBSERVER_GDB_CMD_QUERY_01}
 * - \llrid{LLR_GDBSERVER_GDB_CMD_QUERY_03}: If `in_prefix()` returns true for
 *   `"sThreadInfo"`, `gdb_cmd_query()` shall call `gdb_cmd_thread_info()`.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDBSERVER_GDB_CMD_QUERY_01}
 * - \llrid{LLR_GDBSERVER_GDB_CMD_QUERY_04}: If `in_prefix()` returns true for
 *   `"ThreadExtraInfo,"`, `gdb_cmd_query()` shall call
 *   `gdb_cmd_thread_extra_info()`.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDBSERVER_GDB_CMD_QUERY_01}
 * - \llrid{LLR_GDBSERVER_GDB_CMD_QUERY_05}: If `in_prefix()` returns true for
 *   `"Attached"`, `gdb_cmd_query()` shall call `out_char()` with `'1'`.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDBSERVER_GDB_CMD_QUERY_01}
 * - \llrid{LLR_GDBSERVER_GDB_CMD_QUERY_06}: If `in_prefix()` returns true for
 *   `"Symbol::"`, `gdb_cmd_query()` shall call `out_str()` with `"OK"`.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDBSERVER_GDB_CMD_QUERY_01}
 * - \llrid{LLR_GDBSERVER_GDB_CMD_QUERY_07}: If `in_prefix()` returns false for
 *   these six prefixes and true for `"C"`, and `in_peek()` then returns `'\0'`,
 *   `gdb_cmd_query()` shall call `out_str()` with `"QC"` and `out_thread_id()`
 *   with the address of `gdb_selected`.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDBSERVER_GDB_CMD_QUERY_01}
 * - \llrid{LLR_GDBSERVER_GDB_CMD_QUERY_08}: If `in_prefix()` returns false for
 *   these six prefixes, and it returns false for `"C"` or `in_peek()` does not
 *   return `'\0'` after it, `gdb_cmd_query()` shall add nothing to the reply.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDBSERVER_GDB_CMD_QUERY_02}
 *
 * @design
 * 1. If `in_prefix("Supported")` is true:
 *    \llrref{LLR_GDBSERVER_GDB_CMD_QUERY_01}
 *    1. Call `gdb_cmd_supported()`.
 * 2. Otherwise, if `in_prefix("fThreadInfo")` is true:
 *    \llrref{LLR_GDBSERVER_GDB_CMD_QUERY_02}
 *    1. Call `gdb_process_next()` with the argument `0U`, and store the result
 *       in `gdb_list_pid`.
 *    2. Set `gdb_list_tid` to `0U`.
 *    3. Call `gdb_cmd_thread_info()`.
 * 3. Otherwise, if `in_prefix("sThreadInfo")` is true:
 *    \llrref{LLR_GDBSERVER_GDB_CMD_QUERY_03}
 *    1. Call `gdb_cmd_thread_info()`.
 * 4. Otherwise, if `in_prefix("ThreadExtraInfo,")` is true:
 *    \llrref{LLR_GDBSERVER_GDB_CMD_QUERY_04}
 *    1. Call `gdb_cmd_thread_extra_info()`.
 * 5. Otherwise, if `in_prefix("Attached")` is true:
 *    \llrref{LLR_GDBSERVER_GDB_CMD_QUERY_05}
 *    1. Call `out_char()` with the argument `'1'`.
 * 6. Otherwise, if `in_prefix("Symbol::")` is true:
 *    \llrref{LLR_GDBSERVER_GDB_CMD_QUERY_06}
 *    1. Call `out_str()` with the argument `"OK"`.
 * 7. Otherwise, if `in_prefix("C") && (in_peek() == '\0')` holds:
 *    \llrref{LLR_GDBSERVER_GDB_CMD_QUERY_07},
 *    \llrref{LLR_GDBSERVER_GDB_CMD_QUERY_08}
 *    1. Call `out_str()` with the argument `"QC"`.
 *       \llrref{LLR_GDBSERVER_GDB_CMD_QUERY_07}
 *    2. Call `out_thread_id()` with the address of `gdb_selected`.
 *       \llrref{LLR_GDBSERVER_GDB_CMD_QUERY_07}
 *
 * @calls
 * - `in_prefix()`
 * - `gdb_cmd_supported()`
 * - `gdb_process_next()`
 * - `gdb_cmd_thread_info()`
 * - `gdb_cmd_thread_extra_info()`
 * - `out_char()`
 * - `out_str()`
 * - `in_peek()`
 * - `out_thread_id()`
 */
static void gdb_cmd_query(void)
{
    if (in_prefix("Supported"))
    {
        gdb_cmd_supported();
    }
    else if (in_prefix("fThreadInfo"))
    {
        gdb_list_pid = gdb_process_next(0U);
        gdb_list_tid = 0U;
        gdb_cmd_thread_info();
    }
    else if (in_prefix("sThreadInfo"))
    {
        gdb_cmd_thread_info();
    }
    else if (in_prefix("ThreadExtraInfo,"))
    {
        gdb_cmd_thread_extra_info();
    }
    else if (in_prefix("Attached"))
    {
        out_char('1'); // Processes exist before GDB attaches to them.
    }
    else if (in_prefix("Symbol::"))
    {
        out_str("OK");
    }
    else if (in_prefix("C") && (in_peek() == '\0'))
    {
        // The thread of the last stop or attach (vAttach).
        out_str("QC");
        out_thread_id(&gdb_selected);
    }
    else
    {
        // Not supported.
    }
}

/**
 * @funcname `gdb_cmd_set_thread`
 *
 * @description
 * Answers H: Hc is accepted (resuming resumes everything), Hg selects the
 * thread of the register and memory commands.
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `gdb_selected` | `gdb_thread_t` | The thread that GDB selected (Hg) |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `op` | `char` | The operation: c or g |
 * | `ref` | `gdb_thread_ref_t` | The thread reference |
 * | `t` | `gdb_thread_t` | The thread |
 *
 * @hlr
 * - \hlrid{HLR_GDBSERVER_GDB_CMD_SET_THREAD_01}: `gdb_cmd_set_thread()` shall
 *   select the thread of the register and memory requests (`Hg`), and accept
 *   the thread for resuming (`Hc`), as the whole module resumes.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_DBG_004}
 *   - \srqref{SRQ_JET_DBG_006}
 *
 * @llr
 * - \llrid{LLR_GDBSERVER_GDB_CMD_SET_THREAD_01}: If `in_thread_ref()` returns
 *   false after the operation character (`in_peek()`, `in_skip()`),
 *   `gdb_cmd_set_thread()` shall call `out_str()` with `"E01"`.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDBSERVER_GDB_CMD_SET_THREAD_01}
 * - \llrid{LLR_GDBSERVER_GDB_CMD_SET_THREAD_02}: If `in_thread_ref()` returns
 *   true and the operation character is `'c'`, `gdb_cmd_set_thread()` shall
 *   call `out_str()` with `"OK"`.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDBSERVER_GDB_CMD_SET_THREAD_01}
 * - \llrid{LLR_GDBSERVER_GDB_CMD_SET_THREAD_03}: If `in_thread_ref()` returns
 *   true, the operation character is `'g'` and `gdb_thread_resolve()` returns
 *   true for the reference, `gdb_cmd_set_thread()` shall set `gdb_selected` to
 *   the thread and call `out_str()` with `"OK"`.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDBSERVER_GDB_CMD_SET_THREAD_01}
 * - \llrid{LLR_GDBSERVER_GDB_CMD_SET_THREAD_04}: If `in_thread_ref()` returns
 *   true, the operation character is `'g'` and `gdb_thread_resolve()` returns
 *   false for the reference, `gdb_cmd_set_thread()` shall call `out_str()` with
 *   `"E01"`.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDBSERVER_GDB_CMD_SET_THREAD_01}
 * - \llrid{LLR_GDBSERVER_GDB_CMD_SET_THREAD_05}: If `in_thread_ref()` returns
 *   true and the operation character is neither `'c'` nor `'g'`,
 *   `gdb_cmd_set_thread()` shall add nothing to the reply.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDBSERVER_GDB_CMD_SET_THREAD_01}
 *
 * @design
 * 1. Call `in_peek()`, and store the result in `op`.
 *    \llrref{LLR_GDBSERVER_GDB_CMD_SET_THREAD_01},
 *    \llrref{LLR_GDBSERVER_GDB_CMD_SET_THREAD_02},
 *    \llrref{LLR_GDBSERVER_GDB_CMD_SET_THREAD_03},
 *    \llrref{LLR_GDBSERVER_GDB_CMD_SET_THREAD_04},
 *    \llrref{LLR_GDBSERVER_GDB_CMD_SET_THREAD_05}
 * 2. Call `in_skip()`. \llrref{LLR_GDBSERVER_GDB_CMD_SET_THREAD_01},
 *    \llrref{LLR_GDBSERVER_GDB_CMD_SET_THREAD_02},
 *    \llrref{LLR_GDBSERVER_GDB_CMD_SET_THREAD_03},
 *    \llrref{LLR_GDBSERVER_GDB_CMD_SET_THREAD_04},
 *    \llrref{LLR_GDBSERVER_GDB_CMD_SET_THREAD_05}
 * 3. If `in_thread_ref(&ref)` is false:
 *    \llrref{LLR_GDBSERVER_GDB_CMD_SET_THREAD_01}
 *    1. Call `out_str()` with the argument `"E01"`.
 * 4. Otherwise, if `op` is equal to `'c'`:
 *    \llrref{LLR_GDBSERVER_GDB_CMD_SET_THREAD_02}
 *    1. Call `out_str()` with the argument `"OK"`.
 * 5. Otherwise, if `op` is equal to `'g'`:
 *    \llrref{LLR_GDBSERVER_GDB_CMD_SET_THREAD_03},
 *    \llrref{LLR_GDBSERVER_GDB_CMD_SET_THREAD_04},
 *    \llrref{LLR_GDBSERVER_GDB_CMD_SET_THREAD_05}
 *    1. If `gdb_thread_resolve(&t, &ref)` is true:
 *       \llrref{LLR_GDBSERVER_GDB_CMD_SET_THREAD_03}
 *       1. Set `gdb_selected` to `t`.
 *       2. Call `out_str()` with the argument `"OK"`.
 *    2. Otherwise: \llrref{LLR_GDBSERVER_GDB_CMD_SET_THREAD_04}
 *       1. Call `out_str()` with the argument `"E01"`.
 *
 * @calls
 * - `in_peek()`
 * - `in_skip()`
 * - `in_thread_ref()`
 * - `out_str()`
 * - `gdb_thread_resolve()`
 */
static void gdb_cmd_set_thread(void)
{
    gdb_thread_ref_t ref;
    gdb_thread_t t;
    char op = in_peek();

    in_skip();
    if (!in_thread_ref(&ref))
    {
        out_str("E01");
    }
    else if (op == 'c')
    {
        out_str("OK"); // Resuming always resumes the whole system.
    }
    else if (op == 'g')
    {
        if (gdb_thread_resolve(&t, &ref))
        {
            gdb_selected = t;
            out_str("OK");
        }
        else
        {
            out_str("E01");
        }
    }
    else
    {
        // Not supported.
    }
}

/**
 * @funcname `gdb_cmd_thread_alive`
 *
 * @description
 * Answers T: OK if the thread exists in an attached process.
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `ref` | `gdb_thread_ref_t` | The thread reference |
 * | `t` | `gdb_thread_t` | The thread |
 *
 * @hlr
 * - \hlrid{HLR_GDBSERVER_GDB_CMD_THREAD_ALIVE_01}: `gdb_cmd_thread_alive()`
 *   shall tell the debugger whether a thread exists in a process that it is
 *   attached to.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_DBG_002}
 *
 * @llr
 * - \llrid{LLR_GDBSERVER_GDB_CMD_THREAD_ALIVE_01}: If `in_thread_ref()` returns
 *   true, `gdb_thread_resolve()` returns true for the reference and
 *   `gdb_is_attached()` returns true for `pid` of the thread,
 *   `gdb_cmd_thread_alive()` shall call `out_str()` with `"OK"`.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDBSERVER_GDB_CMD_THREAD_ALIVE_01}
 * - \llrid{LLR_GDBSERVER_GDB_CMD_THREAD_ALIVE_02}: If `in_thread_ref()` returns
 *   false, or `gdb_thread_resolve()` returns false for the reference, or
 *   `gdb_is_attached()` returns false for `pid` of the thread,
 *   `gdb_cmd_thread_alive()` shall call `out_str()` with `"E01"`.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDBSERVER_GDB_CMD_THREAD_ALIVE_01}
 *
 * @design
 * 1. If `in_thread_ref(&ref) && gdb_thread_resolve(&t, &ref) &&`
 *    `gdb_is_attached(t.pid)` holds:
 *    \llrref{LLR_GDBSERVER_GDB_CMD_THREAD_ALIVE_01}
 *    1. Call `out_str()` with the argument `"OK"`.
 * 2. Otherwise: \llrref{LLR_GDBSERVER_GDB_CMD_THREAD_ALIVE_02}
 *    1. Call `out_str()` with the argument `"E01"`.
 *
 * @calls
 * - `in_thread_ref()`
 * - `gdb_thread_resolve()`
 * - `gdb_is_attached()`
 * - `out_str()`
 */
static void gdb_cmd_thread_alive(void)
{
    gdb_thread_ref_t ref;
    gdb_thread_t t;

    if (in_thread_ref(&ref) && gdb_thread_resolve(&t, &ref)
        && gdb_is_attached(t.pid))
    {
        out_str("OK");
    }
    else
    {
        out_str("E01");
    }
}

/**
 * @funcname `gdb_cmd_read_registers`
 *
 * @description
 * Answers g: the registers of the selected thread (xx for those that can't be
 * read).
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `gdb_selected` | `gdb_thread_t` | The thread that GDB selected (Hg) |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `ctx` | `const interrupt_frame *` | The registers |
 * | `fp` | `const jet_fp_store_t *` | The FP store |
 * | `regno` | `uint32_t` | The register |
 * | `size` | `size_t` | Its size |
 * | `value` | `uint8_t[16]` | The value of a register |
 *
 * @hlr
 * - \hlrid{HLR_GDBSERVER_GDB_CMD_READ_REGISTERS_01}: `gdb_cmd_read_registers()`
 *   shall give the debugger the registers of the selected thread, marking those
 *   that cannot be read.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_DBG_004}
 *
 * @llr
 * - \llrid{LLR_GDBSERVER_GDB_CMD_READ_REGISTERS_01}: For each register `regno`
 *   less than `JA_GDB_NUM_REGS`, in their order, for which
 *   `ja_gdb_register_get()` returns true for the context of `gdb_selected`
 *   (`gdb_thread_regs()`), its FP store (`gdb_thread_fp()`), `regno` and a
 *   buffer, `gdb_cmd_read_registers()` shall call `out_bytes()` with the buffer
 *   and the result of `ja_gdb_register_size()` for `regno`.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDBSERVER_GDB_CMD_READ_REGISTERS_01}
 * - \llrid{LLR_GDBSERVER_GDB_CMD_READ_REGISTERS_02}: For each register `regno`
 *   less than `JA_GDB_NUM_REGS` for which `ja_gdb_register_get()` returns
 *   false, `gdb_cmd_read_registers()` shall call `out_unavailable()` with the
 *   result of `ja_gdb_register_size()` for `regno`.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDBSERVER_GDB_CMD_READ_REGISTERS_01}
 *
 * @design
 * 1. Call `gdb_thread_regs()` with the address of `gdb_selected`, and store the
 *    result in `ctx`. \llrref{LLR_GDBSERVER_GDB_CMD_READ_REGISTERS_01},
 *    \llrref{LLR_GDBSERVER_GDB_CMD_READ_REGISTERS_02}
 * 2. Call `gdb_thread_fp()` with the address of `gdb_selected`, and store the
 *    result in `fp`. \llrref{LLR_GDBSERVER_GDB_CMD_READ_REGISTERS_01},
 *    \llrref{LLR_GDBSERVER_GDB_CMD_READ_REGISTERS_02}
 * 3. Set `regno` to `0U`; then while `regno` is less than `JA_GDB_NUM_REGS`,
 *    repeat (after each pass: increment `regno`):
 *    \llrref{LLR_GDBSERVER_GDB_CMD_READ_REGISTERS_01},
 *    \llrref{LLR_GDBSERVER_GDB_CMD_READ_REGISTERS_02}
 *    1. Call `ja_gdb_register_size()` with the argument `regno`, and store the
 *       result in `size`.
 *    2. If `ja_gdb_register_get(ctx, fp, regno, value)` is true:
 *       \llrref{LLR_GDBSERVER_GDB_CMD_READ_REGISTERS_01}
 *       1. Call `out_bytes()` with the arguments `value` and `size`.
 *    3. Otherwise: \llrref{LLR_GDBSERVER_GDB_CMD_READ_REGISTERS_02}
 *       1. Call `out_unavailable()` with the argument `size`.
 *
 * @calls
 * - `gdb_thread_regs()`
 * - `gdb_thread_fp()`
 * - `ja_gdb_register_size()`
 * - `ja_gdb_register_get()`
 * - `out_bytes()`
 * - `out_unavailable()`
 */
static void gdb_cmd_read_registers(void)
{
    const interrupt_frame* ctx = gdb_thread_regs(&gdb_selected);
    const jet_fp_store_t* fp = gdb_thread_fp(&gdb_selected);
    uint8_t value[JA_GDB_REGISTER_SIZE_MAX];

    for (uint32_t regno = 0U; regno < JA_GDB_NUM_REGS; regno++)
    {
        size_t size = ja_gdb_register_size(regno);

        if (ja_gdb_register_get(ctx, fp, regno, value)) {
            out_bytes(value, size);
        } else {
            out_unavailable(size);
        }
    }
}

/**
 * @funcname `gdb_cmd_write_registers`
 *
 * @description
 * Answers G: writes the registers of the selected thread (xx keeps a register);
 * only user contexts or the stopped context are written.
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `gdb_in_len` | `size_t` | The length of the packet received |
 * | `gdb_in_pos` | `size_t` | The position read in the packet received |
 * | `gdb_registers_changed` | `pok_bool_t` | Whether GDB wrote registers |
 * | `gdb_selected` | `gdb_thread_t` | The thread that GDB selected (Hg) |
 * | `gdb_stop_ctx` | `interrupt_frame *` | The registers of the stopped context |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `ctx` | `interrupt_frame *` | The registers |
 * | `fp` | `jet_fp_store_t *` | The FP store |
 * | `ok` | `pok_bool_t` | Whether the packet is good so far |
 * | `regno` | `uint32_t` | The register |
 * | `size` | `size_t` | Its size |
 * | `value` | `uint8_t[16]` | The value of a register |
 *
 * @hlr
 * - \hlrid{HLR_GDBSERVER_GDB_CMD_WRITE_REGISTERS_01}:
 *   `gdb_cmd_write_registers()` shall write the registers that the debugger
 *   gives into the selected thread, keeping those marked `"xx"`, and only into
 *   a user context or the stopped context.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_DBG_004}
 *
 * @llr
 * - \llrid{LLR_GDBSERVER_GDB_CMD_WRITE_REGISTERS_01}: If the context of
 *   `gdb_selected` (`gdb_thread_regs()`) is not NULL, is not `gdb_stop_ctx` and
 *   `ja_gdb_context_is_user()` returns false for it,
 *   `gdb_cmd_write_registers()` shall give NULL to `ja_gdb_register_set()` as
 *   the context.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDBSERVER_GDB_CMD_WRITE_REGISTERS_01}
 * - \llrid{LLR_GDBSERVER_GDB_CMD_WRITE_REGISTERS_02}: If the context of
 *   `gdb_selected` is NULL, or is `gdb_stop_ctx`, or `ja_gdb_context_is_user()`
 *   returns true for it, `gdb_cmd_write_registers()` shall give it to
 *   `ja_gdb_register_set()` as it is.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDBSERVER_GDB_CMD_WRITE_REGISTERS_01}
 * - \llrid{LLR_GDBSERVER_GDB_CMD_WRITE_REGISTERS_03}: For each register `regno`
 *   less than `JA_GDB_NUM_REGS`, in their order, while the packet is good and
 *   `gdb_in_pos` is less than `gdb_in_len`: if `in_peek()` returns `'x'` and at
 *   least 2 times the size of the register (`ja_gdb_register_size()`)
 *   characters remain, `gdb_cmd_write_registers()` shall add that number to
 *   `gdb_in_pos` without writing the register.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDBSERVER_GDB_CMD_WRITE_REGISTERS_01}
 * - \llrid{LLR_GDBSERVER_GDB_CMD_WRITE_REGISTERS_04}: If `in_peek()` returns
 *   `'x'` for a register and fewer than 2 times its size characters remain,
 *   `gdb_cmd_write_registers()` shall take the packet as bad.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDBSERVER_GDB_CMD_WRITE_REGISTERS_01}
 * - \llrid{LLR_GDBSERVER_GDB_CMD_WRITE_REGISTERS_05}: If `in_peek()` does not
 *   return `'x'` for a register and `in_bytes()` returns true for a buffer and
 *   its size, `gdb_cmd_write_registers()` shall call `ja_gdb_register_set()`
 *   with the context, the FP store of `gdb_selected` (`gdb_thread_fp()`),
 *   `regno` and the buffer.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDBSERVER_GDB_CMD_WRITE_REGISTERS_01}
 * - \llrid{LLR_GDBSERVER_GDB_CMD_WRITE_REGISTERS_06}: If `in_peek()` does not
 *   return `'x'` for a register and `in_bytes()` returns false for a buffer and
 *   its size, `gdb_cmd_write_registers()` shall take the packet as bad.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDBSERVER_GDB_CMD_WRITE_REGISTERS_01}
 * - \llrid{LLR_GDBSERVER_GDB_CMD_WRITE_REGISTERS_07}:
 *   `gdb_cmd_write_registers()` shall then set `gdb_registers_changed` to
 *   `TRUE`, and call `out_str()` with `"OK"` if the packet is good, and with
 *   `"E01"` if it is bad.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDBSERVER_GDB_CMD_WRITE_REGISTERS_01}
 *
 * @design
 * 1. Call `gdb_thread_regs()` with the address of `gdb_selected`, and store the
 *    result in `ctx`. \llrref{LLR_GDBSERVER_GDB_CMD_WRITE_REGISTERS_01},
 *    \llrref{LLR_GDBSERVER_GDB_CMD_WRITE_REGISTERS_02}
 * 2. Call `gdb_thread_fp()` with the address of `gdb_selected`, and store the
 *    result in `fp`. \llrref{LLR_GDBSERVER_GDB_CMD_WRITE_REGISTERS_05}
 * 3. If `ctx` is not NULL and `ctx` is not equal to `gdb_stop_ctx` and
 *    `ja_gdb_context_is_user(ctx)` is false:
 *    \llrref{LLR_GDBSERVER_GDB_CMD_WRITE_REGISTERS_01},
 *    \llrref{LLR_GDBSERVER_GDB_CMD_WRITE_REGISTERS_02}
 *    1. Set `ctx` to `NULL`. \llrref{LLR_GDBSERVER_GDB_CMD_WRITE_REGISTERS_01}
 * 4. Set `ok` to `TRUE`. \llrref{LLR_GDBSERVER_GDB_CMD_WRITE_REGISTERS_07}
 * 5. Set `regno` to `0U`; then while `ok` is true and `regno` is less than
 *    `JA_GDB_NUM_REGS` and `gdb_in_pos` is less than `gdb_in_len`, repeat
 *    (after each pass: increment `regno`):
 *    \llrref{LLR_GDBSERVER_GDB_CMD_WRITE_REGISTERS_03},
 *    \llrref{LLR_GDBSERVER_GDB_CMD_WRITE_REGISTERS_04},
 *    \llrref{LLR_GDBSERVER_GDB_CMD_WRITE_REGISTERS_05},
 *    \llrref{LLR_GDBSERVER_GDB_CMD_WRITE_REGISTERS_06}
 *    1. Call `ja_gdb_register_size()` with the argument `regno`, and store the
 *       result in `size`.
 *    2. If `in_peek()` is equal to `'x'`:
 *       \llrref{LLR_GDBSERVER_GDB_CMD_WRITE_REGISTERS_03},
 *       \llrref{LLR_GDBSERVER_GDB_CMD_WRITE_REGISTERS_04}
 *       1. Set `ok` to `(gdb_in_len - gdb_in_pos) >= (2U * size)`.
 *       2. If `ok` is true:
 *          1. Add `2U * size` to `gdb_in_pos`.
 *             \llrref{LLR_GDBSERVER_GDB_CMD_WRITE_REGISTERS_03}
 *    3. Otherwise, if `in_bytes(value, size)` is true:
 *       \llrref{LLR_GDBSERVER_GDB_CMD_WRITE_REGISTERS_05}
 *       1. Call `ja_gdb_register_set()` with the arguments `ctx`, `fp`, `regno`
 *          and `value`.
 *    4. Otherwise: \llrref{LLR_GDBSERVER_GDB_CMD_WRITE_REGISTERS_06}
 *       1. Set `ok` to `FALSE`.
 * 6. Set `gdb_registers_changed` to `TRUE`.
 *    \llrref{LLR_GDBSERVER_GDB_CMD_WRITE_REGISTERS_07}
 * 7. Call `out_str()` with the argument `"OK"` if `ok` is true, otherwise
 *    `"E01"`. \llrref{LLR_GDBSERVER_GDB_CMD_WRITE_REGISTERS_07}
 *
 * @calls
 * - `gdb_thread_regs()`
 * - `gdb_thread_fp()`
 * - `ja_gdb_context_is_user()`
 * - `ja_gdb_register_size()`
 * - `in_peek()`
 * - `in_bytes()`
 * - `ja_gdb_register_set()`
 * - `out_str()`
 */
static void gdb_cmd_write_registers(void)
{
    interrupt_frame* ctx = gdb_thread_regs(&gdb_selected);
    jet_fp_store_t* fp = gdb_thread_fp(&gdb_selected);

    if ((ctx != NULL) && (ctx != gdb_stop_ctx)
        && !ja_gdb_context_is_user(ctx))
    {
        ctx = NULL;
    }
    uint8_t value[JA_GDB_REGISTER_SIZE_MAX];
    pok_bool_t ok = TRUE;

    for (uint32_t regno = 0U;
        ok && (regno < JA_GDB_NUM_REGS) && (gdb_in_pos < gdb_in_len);
        regno++)
    {
        size_t size = ja_gdb_register_size(regno);

        if (in_peek() == 'x')
        {
            ok = (gdb_in_len - gdb_in_pos) >= (2U * size);
            if (ok) {
                gdb_in_pos += 2U * size;
            }
        }
        else if (in_bytes(value, size))
        {
            (void) ja_gdb_register_set(ctx, fp, regno, value);
        }
        else
        {
            ok = FALSE;
        }
    }

    gdb_registers_changed = TRUE;
    out_str(ok ? "OK" : "E01");
}

/**
 * @funcname `gdb_selected_space`
 *
 * @description
 * Returns the memory space of the selected thread.
 *
 * @return The memory space.
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `gdb_selected` | `gdb_thread_t` | The thread that GDB selected (Hg) |
 *
 * @locals
 * None.
 *
 * @hlr
 * - \hlrid{HLR_GDBSERVER_GDB_SELECTED_SPACE_01}: `gdb_selected_space()` shall
 *   give the memory space of the selected thread.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_DBG_004}
 *
 * @llr
 * - \llrid{LLR_GDBSERVER_GDB_SELECTED_SPACE_01}: `gdb_selected_space()` shall
 *   return `space_id` of the partition of `gdb_selected`.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDBSERVER_GDB_SELECTED_SPACE_01}
 *
 * @design
 * 1. Return `gdb_selected.part->space_id`.
 *    \llrref{LLR_GDBSERVER_GDB_SELECTED_SPACE_01}
 *
 * @calls
 * None.
 */
static jet_space_id gdb_selected_space(void)
{
    return gdb_selected.part->space_id;
}

/**
 * @funcname `gdb_cmd_read_memory`
 *
 * @description
 * Answers m: reads memory of the selected thread's space (as much as can be
 * read), with the code under the breakpoints.
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `addr` | `uintptr_t` | The address |
 * | `copy` | `static uint8_t[2048]` | A copy of the memory |
 * | `len` | `uintptr_t` | The length asked for |
 * | `mem` | `const uint8_t *` | The memory, at its kernel address |
 * | `readable` | `size_t` | The length that can be read |
 * | `size` | `size_t` | The length read |
 * | `space_id` | `jet_space_id` | The memory space |
 *
 * @hlr
 * - \hlrid{HLR_GDBSERVER_GDB_CMD_READ_MEMORY_01}: `gdb_cmd_read_memory()` shall
 *   give the debugger the memory of the space of the selected thread, as much
 *   of it as can be read, showing the code under the software breakpoints.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_DBG_004}
 *
 * @llr
 * - \llrid{LLR_GDBSERVER_GDB_CMD_READ_MEMORY_01}: If `in_hex()` returns false
 *   for the address, or `in_expect()` returns false for `','`, or `in_hex()`
 *   returns false for the length, `gdb_cmd_read_memory()` shall call
 *   `out_str()` with `"E01"`.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDBSERVER_GDB_CMD_READ_MEMORY_01}
 * - \llrid{LLR_GDBSERVER_GDB_CMD_READ_MEMORY_02}: If the packet is good,
 *   `gdb_cmd_read_memory()` shall take as the size the length if it is less
 *   than `GDB_PACKET_SIZE / 2`, and `GDB_PACKET_SIZE / 2` if it is not.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDBSERVER_GDB_CMD_READ_MEMORY_01}
 * - \llrid{LLR_GDBSERVER_GDB_CMD_READ_MEMORY_03}: If `ja_addr_to_gdb_ro()`
 *   returns an address for the address, the size and the result of
 *   `gdb_selected_space()`, `gdb_cmd_read_memory()` shall copy the bytes there
 *   to a buffer with `memcpy()`, call `gdb_sw_hide()` with the space, the
 *   address, the buffer and the size, and then `out_bytes()` with the buffer
 *   and the size.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDBSERVER_GDB_CMD_READ_MEMORY_01}
 * - \llrid{LLR_GDBSERVER_GDB_CMD_READ_MEMORY_04}: If and only if
 *   `ja_addr_to_gdb_ro()` returns NULL and the size is greater than 1,
 *   `gdb_cmd_read_memory()` shall reduce the size to the number of bytes, from
 *   the address on, for each of which `ja_addr_to_gdb_ro()` returns an address
 *   for 1 byte, and call `ja_addr_to_gdb_ro()` again with the address and that
 *   size.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDBSERVER_GDB_CMD_READ_MEMORY_01}
 * - \llrid{LLR_GDBSERVER_GDB_CMD_READ_MEMORY_05}: If the last result of
 *   `ja_addr_to_gdb_ro()` for the address and the size is NULL,
 *   `gdb_cmd_read_memory()` shall call `out_str()` with `"E03"`.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDBSERVER_GDB_CMD_READ_MEMORY_01}
 *
 * @design
 * 1. If `in_hex(&addr) && in_expect(',') && in_hex(&len)` holds:
 *    \llrref{LLR_GDBSERVER_GDB_CMD_READ_MEMORY_02},
 *    \llrref{LLR_GDBSERVER_GDB_CMD_READ_MEMORY_03},
 *    \llrref{LLR_GDBSERVER_GDB_CMD_READ_MEMORY_04},
 *    \llrref{LLR_GDBSERVER_GDB_CMD_READ_MEMORY_05}
 *    1. Call `gdb_selected_space()`, and store the result in `space_id`.
 *       \llrref{LLR_GDBSERVER_GDB_CMD_READ_MEMORY_03}
 *    2. Set `size` to `len` if `len` is less than `(GDB_PACKET_SIZE / 2U)`,
 *       otherwise `(GDB_PACKET_SIZE / 2U)`.
 *       \llrref{LLR_GDBSERVER_GDB_CMD_READ_MEMORY_02}
 *    3. Call `ja_addr_to_gdb_ro()` with the arguments `addr`, `size` and
 *       `space_id`, and store the result in `mem`.
 *       \llrref{LLR_GDBSERVER_GDB_CMD_READ_MEMORY_03},
 *       \llrref{LLR_GDBSERVER_GDB_CMD_READ_MEMORY_04},
 *       \llrref{LLR_GDBSERVER_GDB_CMD_READ_MEMORY_05}
 *    4. If `mem` is NULL and `size` is greater than `1U`:
 *       \llrref{LLR_GDBSERVER_GDB_CMD_READ_MEMORY_04}
 *       1. Set `readable` to `0U`.
 *       2. While `readable` is less than `size` and
 *          `ja_addr_to_gdb_ro(addr + readable, 1U, space_id)` is not NULL:
 *          1. Increment `readable`.
 *       3. Set `size` to `readable`.
 *       4. Call `ja_addr_to_gdb_ro()` with the arguments `addr`, `size` and
 *          `space_id`, and store the result in `mem`.
 *    5. If `mem` is not NULL: \llrref{LLR_GDBSERVER_GDB_CMD_READ_MEMORY_03}
 *       1. Call `memcpy()` with the arguments `copy`, `mem` and `size`.
 *       2. Call `gdb_sw_hide()` with the arguments `space_id`, `addr`, `copy`
 *          and `size`.
 *       3. Call `out_bytes()` with the arguments `copy` and `size`.
 *    6. Otherwise: \llrref{LLR_GDBSERVER_GDB_CMD_READ_MEMORY_05}
 *       1. Call `out_str()` with the argument `"E03"`.
 * 2. Otherwise: \llrref{LLR_GDBSERVER_GDB_CMD_READ_MEMORY_01}
 *    1. Call `out_str()` with the argument `"E01"`.
 *
 * @calls
 * - `in_hex()`
 * - `in_expect()`
 * - `gdb_selected_space()`
 * - `ja_addr_to_gdb_ro()`
 * - `memcpy()`
 * - `gdb_sw_hide()`
 * - `out_bytes()`
 * - `out_str()`
 */
static void gdb_cmd_read_memory(void)
{
    uintptr_t addr;
    uintptr_t len;

    if (in_hex(&addr) && in_expect(',') && in_hex(&len))
    {
        jet_space_id space_id = gdb_selected_space();
        size_t size = (len < (GDB_PACKET_SIZE / 2U)) ? len
            : (GDB_PACKET_SIZE / 2U);
        const uint8_t* mem = ja_addr_to_gdb_ro(addr, size, space_id);

        if ((mem == NULL) && (size > 1U))
        {
            size_t readable = 0U;

            while ((readable < size)
                && (ja_addr_to_gdb_ro(addr + readable, 1U, space_id) != NULL))
            {
                readable++;
            }
            size = readable;
            mem = ja_addr_to_gdb_ro(addr, size, space_id);
        }

        if (mem != NULL)
        {
            // GDB doesn't see the breakpoints it has inserted.
            static uint8_t copy[GDB_PACKET_SIZE / 2U];

            (void) memcpy(copy, mem, size);
            gdb_sw_hide(space_id, addr, copy, size);
            out_bytes(copy, size);
        }
        else
        {
            out_str("E03");
        }
    }
    else
    {
        out_str("E01");
    }
}

/**
 * @funcname `gdb_cmd_write_memory`
 *
 * @description
 * Answers M: writes memory of the selected thread's space, keeping the
 * breakpoints, and syncs the instruction cache.
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `gdb_in_len` | `size_t` | The length of the packet received |
 * | `gdb_in_pos` | `size_t` | The position read in the packet received |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `addr` | `uintptr_t` | The address |
 * | `len` | `uintptr_t` | The length |
 * | `mem` | `uint8_t *` | The memory, at its kernel address |
 * | `space_id` | `jet_space_id` | The memory space |
 *
 * @hlr
 * - \hlrid{HLR_GDBSERVER_GDB_CMD_WRITE_MEMORY_01}: `gdb_cmd_write_memory()`
 *   shall write the bytes that the debugger gives into the memory of the space
 *   of the selected thread, keeping the breakpoints, and make the instruction
 *   cache see them.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_DBG_004}
 *   - \srqref{SRQ_JET_DBG_005}
 *
 * @llr
 * - \llrid{LLR_GDBSERVER_GDB_CMD_WRITE_MEMORY_01}: If `in_hex()` returns false
 *   for the address, or `in_expect()` returns false for `','`, or `in_hex()`
 *   returns false for the length, or `in_expect()` returns false for `':'`, or
 *   the length is greater than `GDB_PACKET_SIZE / 2`, or the number of
 *   characters that remain is not 2 times the length, `gdb_cmd_write_memory()`
 *   shall call `out_str()` with `"E01"`.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDBSERVER_GDB_CMD_WRITE_MEMORY_01}
 * - \llrid{LLR_GDBSERVER_GDB_CMD_WRITE_MEMORY_02}: If the packet is good and
 *   the length is 0, `gdb_cmd_write_memory()` shall call `out_str()` with
 *   `"OK"` and write no memory.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDBSERVER_GDB_CMD_WRITE_MEMORY_01}
 * - \llrid{LLR_GDBSERVER_GDB_CMD_WRITE_MEMORY_03}: If the packet is good, the
 *   length is not 0, `ja_addr_to_gdb()` returns an address for the address, the
 *   length and the result of `gdb_selected_space()`, and `in_bytes()` returns
 *   true for that memory and the length, `gdb_cmd_write_memory()` shall call
 *   `gdb_sw_rewrite()` with the space, the address, the memory and the length,
 *   then `ja_gdb_sync_icache()` with the memory and the length, and `out_str()`
 *   with `"OK"`.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDBSERVER_GDB_CMD_WRITE_MEMORY_01}
 * - \llrid{LLR_GDBSERVER_GDB_CMD_WRITE_MEMORY_04}: If the packet is good, the
 *   length is not 0, and `ja_addr_to_gdb()` returns NULL or `in_bytes()`
 *   returns false, `gdb_cmd_write_memory()` shall call `out_str()` with
 *   `"E03"`.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDBSERVER_GDB_CMD_WRITE_MEMORY_01}
 *
 * @design
 * 1. If `in_hex(&addr) && in_expect(',') && in_hex(&len) && in_expect(':') &&`
 *    `(len <= (GDB_PACKET_SIZE / 2U)) && ((gdb_in_len - gdb_in_pos) == (2U *`
 *    `len))` holds: \llrref{LLR_GDBSERVER_GDB_CMD_WRITE_MEMORY_02},
 *    \llrref{LLR_GDBSERVER_GDB_CMD_WRITE_MEMORY_03},
 *    \llrref{LLR_GDBSERVER_GDB_CMD_WRITE_MEMORY_04}
 *    1. Call `gdb_selected_space()`, and store the result in `space_id`.
 *    2. Call `ja_addr_to_gdb()` with the arguments `addr`, `len` and
 *       `space_id`, and store the result in `mem`.
 *    3. If `len` is equal to `0U`:
 *       \llrref{LLR_GDBSERVER_GDB_CMD_WRITE_MEMORY_02}
 *       1. Call `out_str()` with the argument `"OK"`.
 *    4. Otherwise, if `mem` is not NULL and `in_bytes(mem, len)` is true:
 *       \llrref{LLR_GDBSERVER_GDB_CMD_WRITE_MEMORY_03}
 *       1. Call `gdb_sw_rewrite()` with the arguments `space_id`, `addr`, `mem`
 *          and `len`.
 *       2. Call `ja_gdb_sync_icache()` with the arguments `mem` and `len`.
 *       3. Call `out_str()` with the argument `"OK"`.
 *    5. Otherwise: \llrref{LLR_GDBSERVER_GDB_CMD_WRITE_MEMORY_04}
 *       1. Call `out_str()` with the argument `"E03"`.
 * 2. Otherwise: \llrref{LLR_GDBSERVER_GDB_CMD_WRITE_MEMORY_01}
 *    1. Call `out_str()` with the argument `"E01"`.
 *
 * @calls
 * - `in_hex()`
 * - `in_expect()`
 * - `gdb_selected_space()`
 * - `ja_addr_to_gdb()`
 * - `out_str()`
 * - `in_bytes()`
 * - `gdb_sw_rewrite()`
 * - `ja_gdb_sync_icache()`
 */
static void gdb_cmd_write_memory(void)
{
    uintptr_t addr;
    uintptr_t len;

    if (in_hex(&addr) && in_expect(',') && in_hex(&len) && in_expect(':')
        && (len <= (GDB_PACKET_SIZE / 2U))
        && ((gdb_in_len - gdb_in_pos) == (2U * len)))
    {
        jet_space_id space_id = gdb_selected_space();
        uint8_t* mem = ja_addr_to_gdb(addr, len, space_id);

        if (len == 0U)
        {
            out_str("OK");
        }
        else if ((mem != NULL) && in_bytes(mem, len))
        {
            // Breakpoints stay where GDB has inserted them.
            gdb_sw_rewrite(space_id, addr, mem, len);
            ja_gdb_sync_icache(mem, len);
            out_str("OK");
        }
        else
        {
            out_str("E03");
        }
    }
    else
    {
        out_str("E01");
    }
}

/**
 * @funcname `gdb_cmd_breakpoint`
 *
 * @description
 * Answers Z and z: inserts or removes a software breakpoint (type 0) or a
 * hardware breakpoint or watchpoint (types 1-4).
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `insert` | `pok_bool_t` | [in] TRUE for Z, FALSE for z. |
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `addr` | `uintptr_t` | The address |
 * | `hw_type` | `ja_gdb_hw_type_e` | The hardware type |
 * | `kind` | `uintptr_t` | The size |
 * | `ok` | `pok_bool_t` | Whether it worked |
 * | `space_id` | `jet_space_id` | The memory space |
 * | `supported` | `pok_bool_t` | Whether the type is supported |
 * | `type` | `uintptr_t` | The type |
 *
 * @hlr
 * - \hlrid{HLR_GDBSERVER_GDB_CMD_BREAKPOINT_01}: `gdb_cmd_breakpoint()` shall
 *   insert or remove a software breakpoint (type 0) or, as far as the processor
 *   supports it, a hardware breakpoint (1) or a write, read or access
 *   watchpoint (2 to 4), in the space of the selected thread.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_DBG_005}
 *
 * @llr
 * - \llrid{LLR_GDBSERVER_GDB_CMD_BREAKPOINT_01}: If `in_hex()` returns false
 *   for the type, or `in_expect()` returns false for `','`, or `in_hex()`
 *   returns false for the address, or `in_expect()` returns false for the
 *   second `','`, or `in_hex()` returns false for the kind,
 *   `gdb_cmd_breakpoint()` shall call `out_str()` with `"E01"`.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDBSERVER_GDB_CMD_BREAKPOINT_01}
 * - \llrid{LLR_GDBSERVER_GDB_CMD_BREAKPOINT_02}: If the packet is good and the
 *   type is 0, `gdb_cmd_breakpoint()` shall take as the result that of
 *   `gdb_sw_insert()` for the result of `gdb_selected_space()`, the address and
 *   the kind if `insert` is true, and that of `gdb_sw_remove()` for the space
 *   and the address if `insert` is false.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDBSERVER_GDB_CMD_BREAKPOINT_01}
 * - \llrid{LLR_GDBSERVER_GDB_CMD_BREAKPOINT_03}: If the packet is good and the
 *   type is 1, 2, 3 or 4, `gdb_cmd_breakpoint()` shall take as the result that
 *   of `ja_gdb_hw_insert()` if `insert` is true, and that of
 *   `ja_gdb_hw_remove()` if `insert` is false, for `JA_GDB_HW_EXEC`,
 *   `JA_GDB_HW_WRITE`, `JA_GDB_HW_READ` or `JA_GDB_HW_ACCESS`, in this order,
 *   the address, the kind and the result of `gdb_selected_space()`.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDBSERVER_GDB_CMD_BREAKPOINT_01}
 * - \llrid{LLR_GDBSERVER_GDB_CMD_BREAKPOINT_04}: If the packet is good, the
 *   type is from 0 to 4 and the result is true, `gdb_cmd_breakpoint()` shall
 *   call `out_str()` with `"OK"`.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDBSERVER_GDB_CMD_BREAKPOINT_01}
 * - \llrid{LLR_GDBSERVER_GDB_CMD_BREAKPOINT_05}: If the packet is good, the
 *   type is from 0 to 4 and the result is false, `gdb_cmd_breakpoint()` shall
 *   call `out_str()` with `"E22"`.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDBSERVER_GDB_CMD_BREAKPOINT_01}
 * - \llrid{LLR_GDBSERVER_GDB_CMD_BREAKPOINT_06}: If the packet is good and the
 *   type is greater than 4, `gdb_cmd_breakpoint()` shall add nothing to the
 *   reply and call none of `gdb_sw_insert()`, `gdb_sw_remove()`,
 *   `ja_gdb_hw_insert()` and `ja_gdb_hw_remove()`.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDBSERVER_GDB_CMD_BREAKPOINT_01}
 *
 * @design
 * 1. If `in_hex(&type) && in_expect(',') && in_hex(&addr) && in_expect(',') &&`
 *    `in_hex(&kind)` holds: \llrref{LLR_GDBSERVER_GDB_CMD_BREAKPOINT_02},
 *    \llrref{LLR_GDBSERVER_GDB_CMD_BREAKPOINT_03},
 *    \llrref{LLR_GDBSERVER_GDB_CMD_BREAKPOINT_04},
 *    \llrref{LLR_GDBSERVER_GDB_CMD_BREAKPOINT_05},
 *    \llrref{LLR_GDBSERVER_GDB_CMD_BREAKPOINT_06}
 *    1. Call `gdb_selected_space()`, and store the result in `space_id`.
 *    2. Set `ok` to `FALSE`.
 *    3. Set `supported` to `TRUE`.
 *    4. Set `hw_type` to `JA_GDB_HW_EXEC`.
 *    5. Depending on `type`: \llrref{LLR_GDBSERVER_GDB_CMD_BREAKPOINT_02},
 *       \llrref{LLR_GDBSERVER_GDB_CMD_BREAKPOINT_03},
 *       \llrref{LLR_GDBSERVER_GDB_CMD_BREAKPOINT_06}
 *       1. Case `0U`: \llrref{LLR_GDBSERVER_GDB_CMD_BREAKPOINT_02}
 *          1. Set `ok` to `gdb_sw_insert(space_id, addr, kind)` if `insert` is
 *             true, otherwise `gdb_sw_remove(space_id, addr)`.
 *          2. Leave the switch.
 *       2. Case `1U`: \llrref{LLR_GDBSERVER_GDB_CMD_BREAKPOINT_03}
 *          1. Set `hw_type` to `JA_GDB_HW_EXEC`.
 *          2. Leave the switch.
 *       3. Case `2U`: \llrref{LLR_GDBSERVER_GDB_CMD_BREAKPOINT_03}
 *          1. Set `hw_type` to `JA_GDB_HW_WRITE`.
 *          2. Leave the switch.
 *       4. Case `3U`: \llrref{LLR_GDBSERVER_GDB_CMD_BREAKPOINT_03}
 *          1. Set `hw_type` to `JA_GDB_HW_READ`.
 *          2. Leave the switch.
 *       5. Case `4U`: \llrref{LLR_GDBSERVER_GDB_CMD_BREAKPOINT_03}
 *          1. Set `hw_type` to `JA_GDB_HW_ACCESS`.
 *          2. Leave the switch.
 *       6. Otherwise (default): \llrref{LLR_GDBSERVER_GDB_CMD_BREAKPOINT_06}
 *          1. Set `supported` to `FALSE`.
 *          2. Leave the switch.
 *    6. If `supported` is true and `type` is not equal to `0U`:
 *       \llrref{LLR_GDBSERVER_GDB_CMD_BREAKPOINT_03}
 *       1. Set `ok` to `ja_gdb_hw_insert(hw_type, addr, kind, space_id)` if
 *          `insert` is true, otherwise
 *          `ja_gdb_hw_remove(hw_type, addr, kind, space_id)`.
 *    7. If `supported` is true: \llrref{LLR_GDBSERVER_GDB_CMD_BREAKPOINT_04},
 *       \llrref{LLR_GDBSERVER_GDB_CMD_BREAKPOINT_05}
 *       1. Call `out_str()` with the argument `"OK"` if `ok` is true, otherwise
 *          `"E22"`.
 * 2. Otherwise: \llrref{LLR_GDBSERVER_GDB_CMD_BREAKPOINT_01}
 *    1. Call `out_str()` with the argument `"E01"`.
 *
 * @calls
 * - `in_hex()`
 * - `in_expect()`
 * - `gdb_selected_space()`
 * - `gdb_sw_insert()`
 * - `gdb_sw_remove()`
 * - `ja_gdb_hw_insert()`
 * - `ja_gdb_hw_remove()`
 * - `out_str()`
 */
static void gdb_cmd_breakpoint(pok_bool_t insert)
{
    uintptr_t type;
    uintptr_t addr;
    uintptr_t kind;

    if (in_hex(&type) && in_expect(',') && in_hex(&addr) && in_expect(',')
        && in_hex(&kind))
    {
        jet_space_id space_id = gdb_selected_space();
        pok_bool_t ok = FALSE;
        pok_bool_t supported = TRUE;
        ja_gdb_hw_type_e hw_type = JA_GDB_HW_EXEC;

        switch (type)
        {
        case 0U:
            ok = insert ? gdb_sw_insert(space_id, addr, kind)
                : gdb_sw_remove(space_id, addr);
            break;
        case 1U: hw_type = JA_GDB_HW_EXEC; break;
        case 2U: hw_type = JA_GDB_HW_WRITE; break;
        case 3U: hw_type = JA_GDB_HW_READ; break;
        case 4U: hw_type = JA_GDB_HW_ACCESS; break;
        default: supported = FALSE; break;
        }

        if (supported && (type != 0U))
        {
            ok = insert ? ja_gdb_hw_insert(hw_type, addr, kind, space_id)
                : ja_gdb_hw_remove(hw_type, addr, kind, space_id);
        }

        if (supported) {
            out_str(ok ? "OK" : "E22");
        }
    }
    else
    {
        out_str("E01");
    }
}

/**
 * @funcname `gdb_cmd_resume`
 *
 * @description
 * Prepares a resume (c, C, s, S): an address sets the PC; a step sets the
 * single step for the stopped thread; a stop reply is owed.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `with_signal` | `pok_bool_t` | [in] Whether the packet has a signal (C, S). |
 * | `step` | `pok_bool_t` | [in] Whether it is a step (s, S). |
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `gdb_current` | `gdb_thread_t` | The thread that stopped the system |
 * | `gdb_step_cancelled` | `pok_bool_t` | Whether the pending step was cancelled |
 * | `gdb_step_pending` | `pok_bool_t` | Whether a single step is pending |
 * | `gdb_step_thread` | `gdb_thread_t` | The thread that steps |
 * | `gdb_stop_ctx` | `interrupt_frame *` | The registers of the stopped context |
 * | `gdb_stop_reply_pending` | `pok_bool_t` | Whether a stop reply is owed to GDB |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `has_addr` | `pok_bool_t` | Whether an address was given |
 * | `value` | `uintptr_t` | The number read |
 *
 * @hlr
 * - \hlrid{HLR_GDBSERVER_GDB_CMD_RESUME_01}: `gdb_cmd_resume()` shall prepare
 *   the module to resume, at a new address if the debugger gives one, one
 *   instruction at a time for a step, with a stop reply owed.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_DBG_006}
 *
 * @llr
 * - \llrid{LLR_GDBSERVER_GDB_CMD_RESUME_01}: If `with_signal` is true,
 *   `gdb_cmd_resume()` shall call `in_hex()` once for the signal, and take an
 *   address as given if `in_expect()` then returns true for `';'` and
 *   `in_hex()` returns true after it.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDBSERVER_GDB_CMD_RESUME_01}
 * - \llrid{LLR_GDBSERVER_GDB_CMD_RESUME_02}: If `with_signal` is false,
 *   `gdb_cmd_resume()` shall take an address as given if `in_hex()` returns
 *   true.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDBSERVER_GDB_CMD_RESUME_01}
 * - \llrid{LLR_GDBSERVER_GDB_CMD_RESUME_03}: If and only if an address is
 *   given, `gdb_cmd_resume()` shall call `ja_gdb_pc_set()` with `gdb_stop_ctx`
 *   and the address.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDBSERVER_GDB_CMD_RESUME_01}
 * - \llrid{LLR_GDBSERVER_GDB_CMD_RESUME_04}: If and only if `step` is true,
 *   `gdb_cmd_resume()` shall call `ja_gdb_single_step()` with `gdb_stop_ctx`
 *   and `TRUE`, and set `gdb_step_pending` to `TRUE`, `gdb_step_cancelled` to
 *   `FALSE` and `gdb_step_thread` to `gdb_current`.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDBSERVER_GDB_CMD_RESUME_01}
 * - \llrid{LLR_GDBSERVER_GDB_CMD_RESUME_05}: `gdb_cmd_resume()` shall set
 *   `gdb_stop_reply_pending` to `TRUE`.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDBSERVER_GDB_CMD_RESUME_01}
 *
 * @design
 * 1. If `with_signal` is true: \llrref{LLR_GDBSERVER_GDB_CMD_RESUME_01}
 *    1. Call `in_hex()` with the address of `value`.
 *    2. Set `has_addr` to `in_expect(';') && in_hex(&value)`.
 * 2. Otherwise: \llrref{LLR_GDBSERVER_GDB_CMD_RESUME_02}
 *    1. Call `in_hex()` with the address of `value`, and store the result in
 *       `has_addr`.
 * 3. If `has_addr` is true: \llrref{LLR_GDBSERVER_GDB_CMD_RESUME_03}
 *    1. Call `ja_gdb_pc_set()` with the arguments `gdb_stop_ctx` and `value`.
 * 4. If `step` is true: \llrref{LLR_GDBSERVER_GDB_CMD_RESUME_04}
 *    1. Call `ja_gdb_single_step()` with the arguments `gdb_stop_ctx` and
 *       `TRUE`.
 *    2. Set `gdb_step_pending` to `TRUE`.
 *    3. Set `gdb_step_cancelled` to `FALSE`.
 *    4. Set `gdb_step_thread` to `gdb_current`.
 * 5. Set `gdb_stop_reply_pending` to `TRUE`.
 *    \llrref{LLR_GDBSERVER_GDB_CMD_RESUME_05}
 *
 * @calls
 * - `in_hex()`
 * - `in_expect()`
 * - `ja_gdb_pc_set()`
 * - `ja_gdb_single_step()`
 */
static void gdb_cmd_resume(pok_bool_t with_signal, pok_bool_t step)
{
    uintptr_t value;
    pok_bool_t has_addr;

    if (with_signal)
    {
        // The signal is ignored: partitions have no signals.
        (void) in_hex(&value);
        has_addr = in_expect(';') && in_hex(&value);
    }
    else
    {
        has_addr = in_hex(&value);
    }

    if (has_addr) {
        ja_gdb_pc_set(gdb_stop_ctx, value);
    }

    if (step)
    {
        ja_gdb_single_step(gdb_stop_ctx, TRUE);
        gdb_step_pending = TRUE;
        gdb_step_cancelled = FALSE;
        gdb_step_thread = gdb_current;
    }

    gdb_stop_reply_pending = TRUE;
}

/**
 * @funcname `gdb_detach`
 *
 * @description
 * Detaches GDB from a process; with no process left, the session ends.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `pid` | `uint32_t` | [in] The process ID. |
 *
 * @return TRUE if the session ended (the system resumes).
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `gdb_attached` | `pok_bool_t[257]` | Which GDB processes are attached |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `gone` | `pok_bool_t` | Whether no process is attached |
 *
 * @hlr
 * - \hlrid{HLR_GDBSERVER_GDB_DETACH_01}: `gdb_detach()` shall detach the
 *   debugger from a process and end the session when it is attached to none,
 *   and tell whether it ended.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_DBG_007}
 *   - \srqref{SRQ_JET_DBG_015}
 *
 * @llr
 * - \llrid{LLR_GDBSERVER_GDB_DETACH_01}: If and only if `pid` is not greater
 *   than `GDB_PID_MAX`, `gdb_detach()` shall set `gdb_attached[pid]` to
 *   `FALSE`.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDBSERVER_GDB_DETACH_01}
 * - \llrid{LLR_GDBSERVER_GDB_DETACH_02}: If `gdb_attached_none()` then returns
 *   true, `gdb_detach()` shall call `gdb_session_end()` and return true.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDBSERVER_GDB_DETACH_01}
 * - \llrid{LLR_GDBSERVER_GDB_DETACH_03}: If `gdb_attached_none()` then returns
 *   false, `gdb_detach()` shall return false without calling
 *   `gdb_session_end()`.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDBSERVER_GDB_DETACH_01}
 *
 * @design
 * 1. If `pid` is less than or equal to `GDB_PID_MAX`:
 *    \llrref{LLR_GDBSERVER_GDB_DETACH_01}
 *    1. Set `gdb_attached[pid]` to `FALSE`.
 * 2. Call `gdb_attached_none()`, and store the result in `gone`.
 *    \llrref{LLR_GDBSERVER_GDB_DETACH_02}, \llrref{LLR_GDBSERVER_GDB_DETACH_03}
 * 3. If `gone` is true: \llrref{LLR_GDBSERVER_GDB_DETACH_02},
 *    \llrref{LLR_GDBSERVER_GDB_DETACH_03}
 *    1. Call `gdb_session_end()`. \llrref{LLR_GDBSERVER_GDB_DETACH_02}
 * 4. Return `gone`. \llrref{LLR_GDBSERVER_GDB_DETACH_02},
 *    \llrref{LLR_GDBSERVER_GDB_DETACH_03}
 *
 * @calls
 * - `gdb_attached_none()`
 * - `gdb_session_end()`
 */
static pok_bool_t gdb_detach(uint32_t pid)
{
    pok_bool_t gone;

    if (pid <= GDB_PID_MAX) {
        gdb_attached[pid] = FALSE;
    }

    gone = gdb_attached_none();
    if (gone) {
        gdb_session_end();
    }

    return gone;
}

/**
 * @funcname `gdb_cmd_attach`
 *
 * @description
 * Answers vAttach: attaches to a process and selects its current thread.
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `gdb_selected` | `gdb_thread_t` | The thread that GDB selected (Hg) |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `pid` | `uintptr_t` | The process ID |
 * | `t` | `gdb_thread_t` | The thread |
 *
 * @hlr
 * - \hlrid{HLR_GDBSERVER_GDB_CMD_ATTACH_01}: `gdb_cmd_attach()` shall attach
 *   the debugger to a process and select its current thread.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_DBG_007}
 *
 * @llr
 * - \llrid{LLR_GDBSERVER_GDB_CMD_ATTACH_01}: If `in_hex()` returns true for the
 *   process ID, the ID is not greater than `GDB_PID_MAX` and
 *   `gdb_thread_find()` returns true for the ID and the result of
 *   `gdb_process_current_tid()` for it, `gdb_cmd_attach()` shall call
 *   `gdb_attach()` with `pid` of the thread found, set `gdb_selected` to the
 *   thread, and call `out_char()` with `'T'`, `out_byte()` with
 *   `GDB_SIGNAL_NONE`, `out_str()` with `"thread:"`, `out_thread_id()` with the
 *   thread and `out_char()` with `';'`.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDBSERVER_GDB_CMD_ATTACH_01}
 * - \llrid{LLR_GDBSERVER_GDB_CMD_ATTACH_02}: If `in_hex()` returns false for
 *   the process ID, or the ID is greater than `GDB_PID_MAX`, or
 *   `gdb_thread_find()` returns false, `gdb_cmd_attach()` shall call
 *   `out_str()` with `"E01"`.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDBSERVER_GDB_CMD_ATTACH_01}
 *
 * @design
 * 1. If `in_hex(&pid) && (pid <= GDB_PID_MAX) && gdb_thread_find(&t,`
 *    `(uint32_t)pid, gdb_process_current_tid((uint32_t)pid))` holds:
 *    \llrref{LLR_GDBSERVER_GDB_CMD_ATTACH_01}
 *    1. Call `gdb_attach()` with the argument `t.pid`.
 *    2. Set `gdb_selected` to `t`.
 *    3. Call `out_char()` with the argument `'T'`.
 *    4. Call `out_byte()` with the argument `(uint8_t)GDB_SIGNAL_NONE`.
 *    5. Call `out_str()` with the argument `"thread:"`.
 *    6. Call `out_thread_id()` with the address of `t`.
 *    7. Call `out_char()` with the argument `';'`.
 * 2. Otherwise: \llrref{LLR_GDBSERVER_GDB_CMD_ATTACH_02}
 *    1. Call `out_str()` with the argument `"E01"`.
 *
 * @calls
 * - `in_hex()`
 * - `gdb_thread_find()`
 * - `gdb_process_current_tid()`
 * - `gdb_attach()`
 * - `out_char()`
 * - `out_byte()`
 * - `out_str()`
 * - `out_thread_id()`
 */
static void gdb_cmd_attach(void)
{
    uintptr_t pid;
    gdb_thread_t t;

    if (in_hex(&pid) && (pid <= GDB_PID_MAX)
        && gdb_thread_find(&t, (uint32_t)pid,
            gdb_process_current_tid((uint32_t)pid)))
    {
        gdb_attach(t.pid);
        gdb_selected = t;
        out_char('T');
        out_byte((uint8_t)GDB_SIGNAL_NONE);
        out_str("thread:");
        out_thread_id(&t);
        out_char(';');
    }
    else
    {
        out_str("E01");
    }
}

/**
 * @funcname `gdb_cmd_v`
 *
 * @description
 * Answers the v packets: vAttach and vKill (a detach).
 *
 * @return TRUE if the system resumes.
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `pid` | `uintptr_t` | The process ID |
 * | `resume` | `pok_bool_t` | Whether the system resumes |
 *
 * @hlr
 * - \hlrid{HLR_GDBSERVER_GDB_CMD_V_01}: `gdb_cmd_v()` shall answer the attach
 *   and kill requests (a kill detaching from the process), and tell whether the
 *   module resumes.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_DBG_007}
 *
 * @llr
 * - \llrid{LLR_GDBSERVER_GDB_CMD_V_01}: If `in_prefix()` returns true for
 *   `"Attach;"`, `gdb_cmd_v()` shall call `gdb_cmd_attach()` and return
 *   `FALSE`.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDBSERVER_GDB_CMD_V_01}
 * - \llrid{LLR_GDBSERVER_GDB_CMD_V_02}: If `in_prefix()` returns true for
 *   `"Kill;"` and `in_hex()` returns true for a process ID, `gdb_cmd_v()` shall
 *   call `gdb_detach()` with the ID if it is not greater than `GDB_PID_MAX`,
 *   and with 0 if it is, call `out_str()` with `"OK"`, and return the result of
 *   `gdb_detach()`.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDBSERVER_GDB_CMD_V_01}
 * - \llrid{LLR_GDBSERVER_GDB_CMD_V_03}: If `in_prefix()` returns true for
 *   `"Kill;"` and `in_hex()` returns false, `gdb_cmd_v()` shall call
 *   `out_str()` with `"E01"` and return `FALSE`.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDBSERVER_GDB_CMD_V_01}
 * - \llrid{LLR_GDBSERVER_GDB_CMD_V_04}: If `in_prefix()` returns false for
 *   `"Attach;"` and for `"Kill;"`, `gdb_cmd_v()` shall return `FALSE` and add
 *   nothing to the reply.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDBSERVER_GDB_CMD_V_01}
 *
 * @design
 * 1. Set `resume` to `FALSE`. \llrref{LLR_GDBSERVER_GDB_CMD_V_01},
 *    \llrref{LLR_GDBSERVER_GDB_CMD_V_03}, \llrref{LLR_GDBSERVER_GDB_CMD_V_04}
 * 2. If `in_prefix("Attach;")` is true: \llrref{LLR_GDBSERVER_GDB_CMD_V_01}
 *    1. Call `gdb_cmd_attach()`.
 * 3. Otherwise, if `in_prefix("Kill;")` is true:
 *    \llrref{LLR_GDBSERVER_GDB_CMD_V_02}, \llrref{LLR_GDBSERVER_GDB_CMD_V_03},
 *    \llrref{LLR_GDBSERVER_GDB_CMD_V_04}
 *    1. If `in_hex(&pid)` is true: \llrref{LLR_GDBSERVER_GDB_CMD_V_02}
 *       1. Call `gdb_detach()` with the argument `(uint32_t)pid` if `pid` is
 *          less than or equal to `GDB_PID_MAX`, otherwise `0U`, and store the
 *          result in `resume`.
 *       2. Call `out_str()` with the argument `"OK"`.
 *    2. Otherwise: \llrref{LLR_GDBSERVER_GDB_CMD_V_03}
 *       1. Call `out_str()` with the argument `"E01"`.
 * 4. Return `resume`. \llrref{LLR_GDBSERVER_GDB_CMD_V_01},
 *    \llrref{LLR_GDBSERVER_GDB_CMD_V_02}, \llrref{LLR_GDBSERVER_GDB_CMD_V_03},
 *    \llrref{LLR_GDBSERVER_GDB_CMD_V_04}
 *
 * @calls
 * - `in_prefix()`
 * - `gdb_cmd_attach()`
 * - `in_hex()`
 * - `gdb_detach()`
 * - `out_str()`
 */
static pok_bool_t gdb_cmd_v(void)
{
    pok_bool_t resume = FALSE;
    uintptr_t pid;

    if (in_prefix("Attach;"))
    {
        gdb_cmd_attach();
    }
    else if (in_prefix("Kill;"))
    {
        if (in_hex(&pid)) {
            resume = gdb_detach((pid <= GDB_PID_MAX) ? (uint32_t)pid : 0U);
            out_str("OK");
        } else {
            out_str("E01");
        }
    }
    else
    {
        // Not supported (vCont, vMustReplyEmpty, vRun, ...).
    }

    return resume;
}

/**
 * @funcname `gdb_cmd_detach`
 *
 * @description
 * Answers D: ends the session, or detaches from one process.
 *
 * @return TRUE if the system resumes.
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `pid` | `uintptr_t` | The process ID |
 * | `resume` | `pok_bool_t` | Whether the system resumes |
 *
 * @hlr
 * - \hlrid{HLR_GDBSERVER_GDB_CMD_DETACH_01}: `gdb_cmd_detach()` shall end the
 *   session or detach the debugger from one process, and tell whether the
 *   module resumes.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_DBG_007}
 *   - \srqref{SRQ_JET_DBG_015}
 *
 * @llr
 * - \llrid{LLR_GDBSERVER_GDB_CMD_DETACH_01}: If `in_expect()` returns false for
 *   `';'`, `gdb_cmd_detach()` shall call `gdb_session_end()` and `out_str()`
 *   with `"OK"`, and return `TRUE`.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDBSERVER_GDB_CMD_DETACH_01}
 * - \llrid{LLR_GDBSERVER_GDB_CMD_DETACH_02}: If `in_expect()` returns true for
 *   `';'` and `in_hex()` returns true for a process ID, `gdb_cmd_detach()`
 *   shall call `gdb_detach()` with the ID if it is not greater than
 *   `GDB_PID_MAX`, and with 0 if it is, call `out_str()` with `"OK"`, and
 *   return the result of `gdb_detach()`.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDBSERVER_GDB_CMD_DETACH_01}
 * - \llrid{LLR_GDBSERVER_GDB_CMD_DETACH_03}: If `in_expect()` returns true for
 *   `';'` and `in_hex()` returns false, `gdb_cmd_detach()` shall call
 *   `out_str()` with `"E01"` and return `FALSE`.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDBSERVER_GDB_CMD_DETACH_01}
 *
 * @design
 * 1. Set `resume` to `FALSE`. \llrref{LLR_GDBSERVER_GDB_CMD_DETACH_03}
 * 2. If `in_expect(';')` is false: \llrref{LLR_GDBSERVER_GDB_CMD_DETACH_01}
 *    1. Call `gdb_session_end()`.
 *    2. Set `resume` to `TRUE`.
 *    3. Call `out_str()` with the argument `"OK"`.
 * 3. Otherwise, if `in_hex(&pid)` is true:
 *    \llrref{LLR_GDBSERVER_GDB_CMD_DETACH_02}
 *    1. Call `gdb_detach()` with the argument `(uint32_t)pid` if `pid` is less
 *       than or equal to `GDB_PID_MAX`, otherwise `0U`, and store the result in
 *       `resume`.
 *    2. Call `out_str()` with the argument `"OK"`.
 * 4. Otherwise: \llrref{LLR_GDBSERVER_GDB_CMD_DETACH_03}
 *    1. Call `out_str()` with the argument `"E01"`.
 * 5. Return `resume`. \llrref{LLR_GDBSERVER_GDB_CMD_DETACH_01},
 *    \llrref{LLR_GDBSERVER_GDB_CMD_DETACH_02},
 *    \llrref{LLR_GDBSERVER_GDB_CMD_DETACH_03}
 *
 * @calls
 * - `in_expect()`
 * - `gdb_session_end()`
 * - `out_str()`
 * - `in_hex()`
 * - `gdb_detach()`
 */
static pok_bool_t gdb_cmd_detach(void)
{
    pok_bool_t resume = FALSE;
    uintptr_t pid;

    if (!in_expect(';'))
    {
        gdb_session_end();
        resume = TRUE;
        out_str("OK");
    }
    else if (in_hex(&pid))
    {
        resume = gdb_detach((pid <= GDB_PID_MAX) ? (uint32_t)pid : 0U);
        out_str("OK");
    }
    else
    {
        out_str("E01");
    }

    return resume;
}

/**
 * @funcname `gdb_handle_packet`
 *
 * @description
 * Handles a packet received and sends its reply (resumes reply only when the
 * system stops; a resume after Ctrl-C stops at once).
 *
 * @return TRUE if the system resumes.
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `gdb_interrupt_pending` | `pok_bool_t` | Whether GDB sent Ctrl-C |
 * | `gdb_stop_reason` | `jet_gdb_stop_reason_e` | Why the system stopped |
 * | `gdb_stop_reply_pending` | `pok_bool_t` | Whether a stop reply is owed to GDB |
 * | `gdb_stop_signal` | `uint32_t` | The signal of the stop |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `cmd` | `char` | The packet's command |
 * | `reply` | `pok_bool_t` | Whether a reply is sent now |
 * | `resume` | `pok_bool_t` | Whether the system resumes |
 *
 * @hlr
 * - \hlrid{HLR_GDBSERVER_GDB_HANDLE_PACKET_01}: `gdb_handle_packet()` shall
 *   handle a request of the debugger, send its reply, and tell whether the
 *   module resumes.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_DBG_001}
 *   - \srqref{SRQ_JET_DBG_006}
 * - \hlrid{HLR_GDBSERVER_GDB_HANDLE_PACKET_02}: `gdb_handle_packet()` shall
 *   answer a resume request only when the module stops again.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_DBG_006}
 * - \hlrid{HLR_GDBSERVER_GDB_HANDLE_PACKET_03}: `gdb_handle_packet()` shall
 *   stop the module at once, with a stop reply, on a resume request that comes
 *   after an interrupt request.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_DBG_006}
 *
 * @llr
 * - \llrid{LLR_GDBSERVER_GDB_HANDLE_PACKET_01}: `gdb_handle_packet()` shall
 *   take the first character of the packet as the command (`in_peek()`,
 *   `in_skip()`) and call `out_reset()`.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDBSERVER_GDB_HANDLE_PACKET_01}
 * - \llrid{LLR_GDBSERVER_GDB_HANDLE_PACKET_02}: If the command is `'?'`, `'q'`,
 *   `'H'`, `'T'`, `'g'`, `'G'`, `'m'` or `'M'`, `gdb_handle_packet()` shall
 *   call `gdb_out_stop_reply()`, `gdb_cmd_query()`, `gdb_cmd_set_thread()`,
 *   `gdb_cmd_thread_alive()`, `gdb_cmd_read_registers()`,
 *   `gdb_cmd_write_registers()`, `gdb_cmd_read_memory()` or
 *   `gdb_cmd_write_memory()`, in this order, then `gdb_put_packet()`, and
 *   return `FALSE`.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDBSERVER_GDB_HANDLE_PACKET_01}
 * - \llrid{LLR_GDBSERVER_GDB_HANDLE_PACKET_03}: If the command is `'!'`,
 *   `gdb_handle_packet()` shall call `out_str()` with `"OK"`, then
 *   `gdb_put_packet()`, and return `FALSE`.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDBSERVER_GDB_HANDLE_PACKET_01}
 * - \llrid{LLR_GDBSERVER_GDB_HANDLE_PACKET_04}: If the command is `'Z'` or
 *   `'z'`, `gdb_handle_packet()` shall call `gdb_cmd_breakpoint()` with `TRUE`
 *   or `FALSE`, in this order, then `gdb_put_packet()`, and return `FALSE`.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDBSERVER_GDB_HANDLE_PACKET_01}
 * - \llrid{LLR_GDBSERVER_GDB_HANDLE_PACKET_05}: If the command is `'v'` or
 *   `'D'`, `gdb_handle_packet()` shall call `gdb_cmd_v()` or
 *   `gdb_cmd_detach()`, in this order, then `gdb_put_packet()`, and return the
 *   result of that function.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDBSERVER_GDB_HANDLE_PACKET_01}
 * - \llrid{LLR_GDBSERVER_GDB_HANDLE_PACKET_06}: If the command is `'c'`, `'C'`,
 *   `'s'` or `'S'` and `gdb_interrupt_pending` is true, `gdb_handle_packet()`
 *   shall set `gdb_interrupt_pending` to `FALSE`, call `gdb_cmd_resume()` with
 *   whether the command is `'C'` or `'S'` and `FALSE`, set
 *   `gdb_stop_reply_pending` to `FALSE`, `gdb_stop_signal` to `GDB_SIGNAL_INT`
 *   and `gdb_stop_reason` to `JET_GDB_STOP_REQUEST`, call
 *   `gdb_out_stop_reply()` and `gdb_put_packet()`, and return `FALSE`.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDBSERVER_GDB_HANDLE_PACKET_03}
 * - \llrid{LLR_GDBSERVER_GDB_HANDLE_PACKET_07}: If the command is `'c'`, `'C'`,
 *   `'s'` or `'S'` and `gdb_interrupt_pending` is false, `gdb_handle_packet()`
 *   shall call `gdb_cmd_resume()` with whether the command is `'C'` or `'S'`
 *   and whether it is `'s'` or `'S'`, and return `TRUE` without calling
 *   `gdb_put_packet()`.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDBSERVER_GDB_HANDLE_PACKET_02}
 * - \llrid{LLR_GDBSERVER_GDB_HANDLE_PACKET_08}: If the command is `'k'`,
 *   `gdb_handle_packet()` shall call `gdb_session_end()` and return `TRUE`
 *   without calling `gdb_put_packet()`.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDBSERVER_GDB_HANDLE_PACKET_01}
 * - \llrid{LLR_GDBSERVER_GDB_HANDLE_PACKET_09}: If the command is none of
 *   `'?'`, `'!'`, `'q'`, `'H'`, `'T'`, `'g'`, `'G'`, `'m'`, `'M'`, `'Z'`,
 *   `'z'`, `'v'`, `'D'`, `'c'`, `'C'`, `'s'`, `'S'` and `'k'`,
 *   `gdb_handle_packet()` shall call `gdb_put_packet()` with an empty reply and
 *   return `FALSE`.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDBSERVER_GDB_HANDLE_PACKET_01}
 *
 * @design
 * 1. Set `resume` to `FALSE`. \llrref{LLR_GDBSERVER_GDB_HANDLE_PACKET_01}
 * 2. Set `reply` to `TRUE`. \llrref{LLR_GDBSERVER_GDB_HANDLE_PACKET_01}
 * 3. Call `in_peek()`, and store the result in `cmd`.
 *    \llrref{LLR_GDBSERVER_GDB_HANDLE_PACKET_01}
 * 4. Call `in_skip()`. \llrref{LLR_GDBSERVER_GDB_HANDLE_PACKET_01}
 * 5. Call `out_reset()`. \llrref{LLR_GDBSERVER_GDB_HANDLE_PACKET_01}
 * 6. Depending on `cmd`: \llrref{LLR_GDBSERVER_GDB_HANDLE_PACKET_02},
 *    \llrref{LLR_GDBSERVER_GDB_HANDLE_PACKET_03},
 *    \llrref{LLR_GDBSERVER_GDB_HANDLE_PACKET_04},
 *    \llrref{LLR_GDBSERVER_GDB_HANDLE_PACKET_05},
 *    \llrref{LLR_GDBSERVER_GDB_HANDLE_PACKET_06},
 *    \llrref{LLR_GDBSERVER_GDB_HANDLE_PACKET_07},
 *    \llrref{LLR_GDBSERVER_GDB_HANDLE_PACKET_08},
 *    \llrref{LLR_GDBSERVER_GDB_HANDLE_PACKET_09}
 *    1. Case `'?'`: \llrref{LLR_GDBSERVER_GDB_HANDLE_PACKET_02}
 *       1. Call `gdb_out_stop_reply()`.
 *       2. Leave the switch.
 *    2. Case `'!'`: \llrref{LLR_GDBSERVER_GDB_HANDLE_PACKET_03}
 *       1. Call `out_str()` with the argument `"OK"`.
 *       2. Leave the switch.
 *    3. Case `'q'`: \llrref{LLR_GDBSERVER_GDB_HANDLE_PACKET_02}
 *       1. Call `gdb_cmd_query()`.
 *       2. Leave the switch.
 *    4. Case `'H'`: \llrref{LLR_GDBSERVER_GDB_HANDLE_PACKET_02}
 *       1. Call `gdb_cmd_set_thread()`.
 *       2. Leave the switch.
 *    5. Case `'T'`: \llrref{LLR_GDBSERVER_GDB_HANDLE_PACKET_02}
 *       1. Call `gdb_cmd_thread_alive()`.
 *       2. Leave the switch.
 *    6. Case `'g'`: \llrref{LLR_GDBSERVER_GDB_HANDLE_PACKET_02}
 *       1. Call `gdb_cmd_read_registers()`.
 *       2. Leave the switch.
 *    7. Case `'G'`: \llrref{LLR_GDBSERVER_GDB_HANDLE_PACKET_02}
 *       1. Call `gdb_cmd_write_registers()`.
 *       2. Leave the switch.
 *    8. Case `'m'`: \llrref{LLR_GDBSERVER_GDB_HANDLE_PACKET_02}
 *       1. Call `gdb_cmd_read_memory()`.
 *       2. Leave the switch.
 *    9. Case `'M'`: \llrref{LLR_GDBSERVER_GDB_HANDLE_PACKET_02}
 *       1. Call `gdb_cmd_write_memory()`.
 *       2. Leave the switch.
 *    10. Case `'Z'`: \llrref{LLR_GDBSERVER_GDB_HANDLE_PACKET_04}
 *        1. Call `gdb_cmd_breakpoint()` with the argument `TRUE`.
 *        2. Leave the switch.
 *    11. Case `'z'`: \llrref{LLR_GDBSERVER_GDB_HANDLE_PACKET_04}
 *        1. Call `gdb_cmd_breakpoint()` with the argument `FALSE`.
 *        2. Leave the switch.
 *    12. Case `'v'`: \llrref{LLR_GDBSERVER_GDB_HANDLE_PACKET_05}
 *        1. Call `gdb_cmd_v()`, and store the result in `resume`.
 *        2. Leave the switch.
 *    13. Case `'D'`: \llrref{LLR_GDBSERVER_GDB_HANDLE_PACKET_05}
 *        1. Call `gdb_cmd_detach()`, and store the result in `resume`.
 *        2. Leave the switch.
 *    14. Case `'c'` or `'C'` or `'s'` or `'S'`:
 *        \llrref{LLR_GDBSERVER_GDB_HANDLE_PACKET_06},
 *        \llrref{LLR_GDBSERVER_GDB_HANDLE_PACKET_07}
 *        1. If `gdb_interrupt_pending` is true:
 *           \llrref{LLR_GDBSERVER_GDB_HANDLE_PACKET_06}
 *           1. Set `gdb_interrupt_pending` to `FALSE`.
 *           2. Call `gdb_cmd_resume()` with the arguments
 *              `(cmd == 'C') || (cmd == 'S')` and `FALSE`.
 *           3. Set `gdb_stop_reply_pending` to `FALSE`.
 *           4. Set `gdb_stop_signal` to `GDB_SIGNAL_INT`.
 *           5. Set `gdb_stop_reason` to `JET_GDB_STOP_REQUEST`.
 *           6. Call `gdb_out_stop_reply()`.
 *        2. Otherwise: \llrref{LLR_GDBSERVER_GDB_HANDLE_PACKET_07}
 *           1. Call `gdb_cmd_resume()` with the arguments
 *              `(cmd == 'C') || (cmd == 'S')` and
 *              `(cmd == 's') || (cmd == 'S')`.
 *           2. Set `resume` to `TRUE`.
 *           3. Set `reply` to `FALSE`.
 *        3. Leave the switch.
 *    15. Case `'k'`: \llrref{LLR_GDBSERVER_GDB_HANDLE_PACKET_08}
 *        1. Call `gdb_session_end()`.
 *        2. Set `resume` to `TRUE`.
 *        3. Set `reply` to `FALSE`.
 *        4. Leave the switch.
 *    16. Otherwise (default): \llrref{LLR_GDBSERVER_GDB_HANDLE_PACKET_09}
 *        1. Leave the switch.
 * 7. If `reply` is true: \llrref{LLR_GDBSERVER_GDB_HANDLE_PACKET_02},
 *    \llrref{LLR_GDBSERVER_GDB_HANDLE_PACKET_03},
 *    \llrref{LLR_GDBSERVER_GDB_HANDLE_PACKET_04},
 *    \llrref{LLR_GDBSERVER_GDB_HANDLE_PACKET_05},
 *    \llrref{LLR_GDBSERVER_GDB_HANDLE_PACKET_06},
 *    \llrref{LLR_GDBSERVER_GDB_HANDLE_PACKET_07},
 *    \llrref{LLR_GDBSERVER_GDB_HANDLE_PACKET_08},
 *    \llrref{LLR_GDBSERVER_GDB_HANDLE_PACKET_09}
 *    1. Call `gdb_put_packet()`.
 * 8. Return `resume`. \llrref{LLR_GDBSERVER_GDB_HANDLE_PACKET_02},
 *    \llrref{LLR_GDBSERVER_GDB_HANDLE_PACKET_03},
 *    \llrref{LLR_GDBSERVER_GDB_HANDLE_PACKET_04},
 *    \llrref{LLR_GDBSERVER_GDB_HANDLE_PACKET_05},
 *    \llrref{LLR_GDBSERVER_GDB_HANDLE_PACKET_06},
 *    \llrref{LLR_GDBSERVER_GDB_HANDLE_PACKET_07},
 *    \llrref{LLR_GDBSERVER_GDB_HANDLE_PACKET_08},
 *    \llrref{LLR_GDBSERVER_GDB_HANDLE_PACKET_09}
 *
 * @calls
 * - `in_peek()`
 * - `in_skip()`
 * - `out_reset()`
 * - `gdb_out_stop_reply()`
 * - `out_str()`
 * - `gdb_cmd_query()`
 * - `gdb_cmd_set_thread()`
 * - `gdb_cmd_thread_alive()`
 * - `gdb_cmd_read_registers()`
 * - `gdb_cmd_write_registers()`
 * - `gdb_cmd_read_memory()`
 * - `gdb_cmd_write_memory()`
 * - `gdb_cmd_breakpoint()`
 * - `gdb_cmd_v()`
 * - `gdb_cmd_detach()`
 * - `gdb_cmd_resume()`
 * - `gdb_session_end()`
 * - `gdb_put_packet()`
 */
static pok_bool_t gdb_handle_packet(void)
{
    pok_bool_t resume = FALSE;
    pok_bool_t reply = TRUE;
    char cmd = in_peek();

    in_skip();
    out_reset();

    switch (cmd)
    {
    case '?': gdb_out_stop_reply(); break;
    case '!': out_str("OK"); break; // Extended mode.
    case 'q': gdb_cmd_query(); break;
    case 'H': gdb_cmd_set_thread(); break;
    case 'T': gdb_cmd_thread_alive(); break;
    case 'g': gdb_cmd_read_registers(); break;
    case 'G': gdb_cmd_write_registers(); break;
    case 'm': gdb_cmd_read_memory(); break;
    case 'M': gdb_cmd_write_memory(); break;
    case 'Z': gdb_cmd_breakpoint(TRUE); break;
    case 'z': gdb_cmd_breakpoint(FALSE); break;
    case 'v': resume = gdb_cmd_v(); break;
    case 'D': resume = gdb_cmd_detach(); break;
    case 'c':
    case 'C':
    case 's':
    case 'S':
        if (gdb_interrupt_pending)
        {
            // Ctrl-C has come: stop again at once, without a step.
            gdb_interrupt_pending = FALSE;
            gdb_cmd_resume((cmd == 'C') || (cmd == 'S'), FALSE);
            gdb_stop_reply_pending = FALSE;
            gdb_stop_signal = GDB_SIGNAL_INT;
            gdb_stop_reason = JET_GDB_STOP_REQUEST;
            gdb_out_stop_reply();
        }
        else
        {
            gdb_cmd_resume((cmd == 'C') || (cmd == 'S'),
                (cmd == 's') || (cmd == 'S'));
            resume = TRUE;
            reply = FALSE; // The reply comes when the system stops.
        }
        break;
    case 'k':
        gdb_session_end();
        resume = TRUE;
        reply = FALSE;
        break;
    default:
        // Not supported: the empty reply.
        break;
    }

    if (reply) {
        gdb_put_packet();
    }

    return resume;
}

/***********************************************************************
 * Stops.
 */

/**
 * @funcname `gdb_stop`
 *
 * @description
 * Holds the stopped system for GDB: stops the system time, sends the stop reply
 * owed, and handles packets until GDB resumes the system.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `ctx` | `interrupt_frame *` | [in,out] The stopped context. |
 * | `current` | `const gdb_thread_t *` | [in] The stopped thread. |
 * | `signal` | `uint32_t` | [in] The signal. |
 * | `reason` | `jet_gdb_stop_reason_e` | [in] Why the system stopped. |
 * | `addr` | `uintptr_t` | [in] The address of a watchpoint, if any. |
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `gdb_current` | `gdb_thread_t` | The thread that stopped the system |
 * | `gdb_in_server` | `pok_bool_t` | Whether the CPU runs the GDB server |
 * | `gdb_registers_changed` | `pok_bool_t` | Whether GDB wrote registers |
 * | `gdb_selected` | `gdb_thread_t` | The thread that GDB selected (Hg) |
 * | `gdb_session` | `pok_bool_t` | Whether a GDB session is open |
 * | `gdb_step_cancelled` | `pok_bool_t` | Whether the pending step was cancelled |
 * | `gdb_step_pending` | `pok_bool_t` | Whether a single step is pending |
 * | `gdb_step_thread` | `gdb_thread_t` | The thread that steps |
 * | `gdb_stop_addr` | `uintptr_t` | The address of the watchpoint that stopped the system |
 * | `gdb_stop_ctx` | `interrupt_frame *` | The registers of the stopped context |
 * | `gdb_stop_reason` | `jet_gdb_stop_reason_e` | Why the system stopped |
 * | `gdb_stop_reply_pending` | `pok_bool_t` | Whether a stop reply is owed to GDB |
 * | `gdb_stop_signal` | `uint32_t` | The signal of the stop |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `resume` | `pok_bool_t` | Whether the system resumes |
 *
 * @hlr
 * - \hlrid{HLR_GDBSERVER_GDB_STOP_01}: `gdb_stop()` shall hold the stopped
 *   module for the debugger, with the system time stopped, until the debugger
 *   resumes it, the time stopped not being counted.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_DBG_003}
 *   - \srqref{SRQ_JET_DBG_013}
 * - \hlrid{HLR_GDBSERVER_GDB_STOP_02}: `gdb_stop()` shall send the stop reply
 *   that is owed and handle requests of the debugger until it resumes the
 *   module, starting a session at the first request.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_DBG_001}
 *   - \srqref{SRQ_JET_DBG_006}
 * - \hlrid{HLR_GDBSERVER_GDB_STOP_03}: `gdb_stop()` shall show the debugger the
 *   floating point state of the processor and load it again if the debugger
 *   changed registers.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_DBG_004}
 *
 * @llr
 * - \llrid{LLR_GDBSERVER_GDB_STOP_01}: `gdb_stop()` shall first set
 *   `gdb_in_server` to `TRUE` and call `ja_system_time_stop()` and
 *   `ja_gdb_hw_enter()`, and at the end call `ja_gdb_hw_leave()` and
 *   `ja_system_time_resume()` and set `gdb_in_server` to `FALSE`.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDBSERVER_GDB_STOP_01}
 * - \llrid{LLR_GDBSERVER_GDB_STOP_02}: `gdb_stop()` shall call `jet_fp_sync()`
 *   once after `ja_gdb_hw_enter()`.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDBSERVER_GDB_STOP_03}
 * - \llrid{LLR_GDBSERVER_GDB_STOP_03}: If and only if `gdb_registers_changed`
 *   is true when a packet resumes the module, `gdb_stop()` shall call
 *   `jet_fp_reload()` before `ja_gdb_hw_leave()`.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDBSERVER_GDB_STOP_03}
 * - \llrid{LLR_GDBSERVER_GDB_STOP_04}: `gdb_stop()` shall set `gdb_stop_ctx` to
 *   `ctx`, `gdb_current` and `gdb_selected` to the thread at `current`,
 *   `gdb_stop_signal` to `signal`, `gdb_stop_reason` to `reason`,
 *   `gdb_stop_addr` to `addr` and `gdb_registers_changed` to `FALSE`.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDBSERVER_GDB_STOP_02}
 * - \llrid{LLR_GDBSERVER_GDB_STOP_05}: If `gdb_step_pending` is true, `reason`
 *   is `JET_GDB_STOP_STEP` and `gdb_thread_equal()` returns true for `current`
 *   and `gdb_step_thread`, `gdb_stop()` shall set `gdb_step_pending` to
 *   `FALSE`.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDBSERVER_GDB_STOP_02}
 * - \llrid{LLR_GDBSERVER_GDB_STOP_06}: If `gdb_step_pending` is true, and
 *   `reason` is not `JET_GDB_STOP_STEP` or `gdb_thread_equal()` returns false
 *   for `current` and `gdb_step_thread`, `gdb_stop()` shall set
 *   `gdb_step_cancelled` to `TRUE`.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDBSERVER_GDB_STOP_02}
 * - \llrid{LLR_GDBSERVER_GDB_STOP_07}: If `gdb_step_pending` is false,
 *   `gdb_stop()` shall leave `gdb_step_pending` and `gdb_step_cancelled` as
 *   they are.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDBSERVER_GDB_STOP_02}
 * - \llrid{LLR_GDBSERVER_GDB_STOP_08}: If and only if `gdb_stop_reply_pending`
 *   is true, `gdb_stop()` shall call `out_reset()`, `gdb_out_stop_reply()` and
 *   `gdb_put_packet()`, and set `gdb_stop_reply_pending` to `FALSE`.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDBSERVER_GDB_STOP_02}
 * - \llrid{LLR_GDBSERVER_GDB_STOP_09}: `gdb_stop()` shall then call
 *   `gdb_get_packet()` and `gdb_handle_packet()`, and do so again until
 *   `gdb_handle_packet()` returns true.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDBSERVER_GDB_STOP_02}
 * - \llrid{LLR_GDBSERVER_GDB_STOP_10}: If and only if `gdb_session` is false
 *   after `gdb_get_packet()`, `gdb_stop()` shall call `gdb_session_start()`
 *   before `gdb_handle_packet()`.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDBSERVER_GDB_STOP_02}
 *
 * @design
 * 1. Set `resume` to `FALSE`. \llrref{LLR_GDBSERVER_GDB_STOP_09}
 * 2. Set `gdb_in_server` to `TRUE`. \llrref{LLR_GDBSERVER_GDB_STOP_01}
 * 3. Call `ja_system_time_stop()`. \llrref{LLR_GDBSERVER_GDB_STOP_01}
 * 4. Call `ja_gdb_hw_enter()`. \llrref{LLR_GDBSERVER_GDB_STOP_01}
 * 5. Call `jet_fp_sync()`. \llrref{LLR_GDBSERVER_GDB_STOP_02}
 * 6. Set `gdb_stop_ctx` to `ctx`. \llrref{LLR_GDBSERVER_GDB_STOP_04}
 * 7. Set `gdb_current` to the value at `current`.
 *    \llrref{LLR_GDBSERVER_GDB_STOP_04}
 * 8. Set `gdb_selected` to the value at `current`.
 *    \llrref{LLR_GDBSERVER_GDB_STOP_04}
 * 9. Set `gdb_stop_signal` to `signal`. \llrref{LLR_GDBSERVER_GDB_STOP_04}
 * 10. Set `gdb_stop_reason` to `reason`. \llrref{LLR_GDBSERVER_GDB_STOP_04}
 * 11. Set `gdb_stop_addr` to `addr`. \llrref{LLR_GDBSERVER_GDB_STOP_04}
 * 12. Set `gdb_registers_changed` to `FALSE`.
 *     \llrref{LLR_GDBSERVER_GDB_STOP_04}
 * 13. If `gdb_step_pending` is true: \llrref{LLR_GDBSERVER_GDB_STOP_05},
 *     \llrref{LLR_GDBSERVER_GDB_STOP_06}, \llrref{LLR_GDBSERVER_GDB_STOP_07}
 *     1. If `reason` is equal to `JET_GDB_STOP_STEP` and
 *        `gdb_thread_equal(current, &gdb_step_thread)` is true:
 *        \llrref{LLR_GDBSERVER_GDB_STOP_05}
 *        1. Set `gdb_step_pending` to `FALSE`.
 *     2. Otherwise: \llrref{LLR_GDBSERVER_GDB_STOP_06}
 *        1. Set `gdb_step_cancelled` to `TRUE`.
 * 14. If `gdb_stop_reply_pending` is true: \llrref{LLR_GDBSERVER_GDB_STOP_08}
 *     1. Call `out_reset()`.
 *     2. Call `gdb_out_stop_reply()`.
 *     3. Call `gdb_put_packet()`.
 *     4. Set `gdb_stop_reply_pending` to `FALSE`.
 * 15. While `resume` is false: \llrref{LLR_GDBSERVER_GDB_STOP_09},
 *     \llrref{LLR_GDBSERVER_GDB_STOP_10}
 *     1. Call `gdb_get_packet()`. \llrref{LLR_GDBSERVER_GDB_STOP_09}
 *     2. If `gdb_session` is false: \llrref{LLR_GDBSERVER_GDB_STOP_10}
 *        1. Call `gdb_session_start()`.
 *     3. Call `gdb_handle_packet()`, and store the result in `resume`.
 *        \llrref{LLR_GDBSERVER_GDB_STOP_09}
 * 16. If `gdb_registers_changed` is true: \llrref{LLR_GDBSERVER_GDB_STOP_03}
 *     1. Call `jet_fp_reload()`.
 * 17. Call `ja_gdb_hw_leave()`. \llrref{LLR_GDBSERVER_GDB_STOP_01}
 * 18. Call `ja_system_time_resume()`. \llrref{LLR_GDBSERVER_GDB_STOP_01}
 * 19. Set `gdb_in_server` to `FALSE`. \llrref{LLR_GDBSERVER_GDB_STOP_01}
 *
 * @calls
 * - `ja_system_time_stop()`
 * - `ja_gdb_hw_enter()`
 * - `jet_fp_sync()`
 * - `gdb_thread_equal()`
 * - `out_reset()`
 * - `gdb_out_stop_reply()`
 * - `gdb_put_packet()`
 * - `gdb_get_packet()`
 * - `gdb_session_start()`
 * - `gdb_handle_packet()`
 * - `jet_fp_reload()`
 * - `ja_gdb_hw_leave()`
 * - `ja_system_time_resume()`
 */
static void gdb_stop(interrupt_frame* ctx,
    const gdb_thread_t* current, uint32_t signal,
    jet_gdb_stop_reason_e reason, uintptr_t addr)
{
    pok_bool_t resume = FALSE;

    gdb_in_server = TRUE;
    ja_system_time_stop();
    ja_gdb_hw_enter();
    jet_fp_sync();

    gdb_stop_ctx = ctx;
    gdb_current = *current;
    gdb_selected = *current;
    gdb_stop_signal = signal;
    gdb_stop_reason = reason;
    gdb_stop_addr = addr;
    gdb_registers_changed = FALSE;

    if (gdb_step_pending)
    {
        if ((reason == JET_GDB_STOP_STEP)
            && gdb_thread_equal(current, &gdb_step_thread))
        {
            gdb_step_pending = FALSE;
        }
        else
        {
            gdb_step_cancelled = TRUE;
        }
    }

    if (gdb_stop_reply_pending)
    {
        out_reset();
        gdb_out_stop_reply();
        gdb_put_packet();
        gdb_stop_reply_pending = FALSE;
    }

    while (!resume)
    {
        gdb_get_packet();
        if (!gdb_session) {
            gdb_session_start();
        }
        resume = gdb_handle_packet();
    }

    if (gdb_registers_changed) {
        jet_fp_reload();
    }
    ja_gdb_hw_leave();
    ja_system_time_resume();
    gdb_in_server = FALSE;
}

/**
 * @funcname `gdb_request_signal`
 *
 * @description
 * Returns the signal of a stop asked for by a character of GDB: Ctrl-C gives
 * SIGINT in a session; a packet's start is read ahead (and ends an old
 * session).
 *
 * @return The signal.
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `gdb_interrupt_pending` | `pok_bool_t` | Whether GDB sent Ctrl-C |
 * | `gdb_pushback` | `int32_t` | A character read ahead, or -1 |
 * | `gdb_request_char` | `int32_t` | The character that started the request, or -1 |
 * | `gdb_session` | `pok_bool_t` | Whether a GDB session is open |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `c` | `int32_t` | The character |
 * | `signal` | `uint32_t` | The signal |
 *
 * @hlr
 * - \hlrid{HLR_GDBSERVER_GDB_REQUEST_SIGNAL_01}: `gdb_request_signal()` shall
 *   give the signal of a stop that a character of the debugger asked for: an
 *   interrupt during a session stops with SIGINT, and the start of a packet is
 *   read ahead and ends an old session.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_DBG_006}
 *
 * @llr
 * - \llrid{LLR_GDBSERVER_GDB_REQUEST_SIGNAL_01}: `gdb_request_signal()` shall
 *   take the character in `gdb_request_char` and set `gdb_request_char` to -1.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDBSERVER_GDB_REQUEST_SIGNAL_01}
 * - \llrid{LLR_GDBSERVER_GDB_REQUEST_SIGNAL_02}: If the character is
 *   `GDB_CHAR_INTERRUPT` and `gdb_session` is true, `gdb_request_signal()`
 *   shall set `gdb_interrupt_pending` to `FALSE` and return `GDB_SIGNAL_INT`.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDBSERVER_GDB_REQUEST_SIGNAL_01}
 * - \llrid{LLR_GDBSERVER_GDB_REQUEST_SIGNAL_03}: If the character is
 *   `GDB_CHAR_INTERRUPT` and `gdb_session` is false, `gdb_request_signal()`
 *   shall set `gdb_interrupt_pending` to `FALSE` and return `GDB_SIGNAL_TRAP`.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDBSERVER_GDB_REQUEST_SIGNAL_01}
 * - \llrid{LLR_GDBSERVER_GDB_REQUEST_SIGNAL_04}: If the character is not
 *   `GDB_CHAR_INTERRUPT`, `gdb_request_signal()` shall return
 *   `GDB_SIGNAL_TRAP`.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDBSERVER_GDB_REQUEST_SIGNAL_01}
 * - \llrid{LLR_GDBSERVER_GDB_REQUEST_SIGNAL_05}: If the character is not
 *   `GDB_CHAR_INTERRUPT`, `gdb_request_signal()` shall set `gdb_pushback` to it
 *   if and only if it is not less than 0.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDBSERVER_GDB_REQUEST_SIGNAL_01}
 * - \llrid{LLR_GDBSERVER_GDB_REQUEST_SIGNAL_06}: If the character is not
 *   `GDB_CHAR_INTERRUPT`, `gdb_request_signal()` shall call `gdb_session_end()`
 *   if and only if `gdb_session` is true.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDBSERVER_GDB_REQUEST_SIGNAL_01}
 *
 * @design
 * 1. Set `c` to `gdb_request_char`.
 *    \llrref{LLR_GDBSERVER_GDB_REQUEST_SIGNAL_01}
 * 2. Set `signal` to `GDB_SIGNAL_TRAP`.
 *    \llrref{LLR_GDBSERVER_GDB_REQUEST_SIGNAL_03},
 *    \llrref{LLR_GDBSERVER_GDB_REQUEST_SIGNAL_04}
 * 3. Set `gdb_request_char` to `-1`.
 *    \llrref{LLR_GDBSERVER_GDB_REQUEST_SIGNAL_01}
 * 4. If `c` is equal to `GDB_CHAR_INTERRUPT`:
 *    \llrref{LLR_GDBSERVER_GDB_REQUEST_SIGNAL_02},
 *    \llrref{LLR_GDBSERVER_GDB_REQUEST_SIGNAL_03}
 *    1. If `gdb_session` is true:
 *       1. Set `signal` to `GDB_SIGNAL_INT`.
 *          \llrref{LLR_GDBSERVER_GDB_REQUEST_SIGNAL_02}
 *    2. Set `gdb_interrupt_pending` to `FALSE`.
 * 5. Otherwise: \llrref{LLR_GDBSERVER_GDB_REQUEST_SIGNAL_04},
 *    \llrref{LLR_GDBSERVER_GDB_REQUEST_SIGNAL_05},
 *    \llrref{LLR_GDBSERVER_GDB_REQUEST_SIGNAL_06}
 *    1. If `c` is greater than or equal to `0`:
 *       \llrref{LLR_GDBSERVER_GDB_REQUEST_SIGNAL_05}
 *       1. Set `gdb_pushback` to `c`.
 *    2. If `gdb_session` is true: \llrref{LLR_GDBSERVER_GDB_REQUEST_SIGNAL_06}
 *       1. Call `gdb_session_end()`.
 * 6. Return `signal`. \llrref{LLR_GDBSERVER_GDB_REQUEST_SIGNAL_02},
 *    \llrref{LLR_GDBSERVER_GDB_REQUEST_SIGNAL_03},
 *    \llrref{LLR_GDBSERVER_GDB_REQUEST_SIGNAL_04}
 *
 * @calls
 * - `gdb_session_end()`
 */
static uint32_t gdb_request_signal(void)
{
    int32_t c = gdb_request_char;
    uint32_t signal = GDB_SIGNAL_TRAP;

    gdb_request_char = -1;

    if (c == GDB_CHAR_INTERRUPT)
    {
        if (gdb_session) {
            signal = GDB_SIGNAL_INT; // Ctrl-C.
        }
        gdb_interrupt_pending = FALSE;
    }
    else
    {
        if (c >= 0) {
            gdb_pushback = c; // Start of a packet.
        }
        if (gdb_session) {
            // GDB has connected again: forget the previous one.
            gdb_session_end();
        }
    }

    return signal;
}

/**
 * @funcname `jet_gdb_trap`
 *
 * @description
 * The entry of the GDB server from the architecture's trap handler: a request,
 * a breakpoint, a watchpoint or a single step stops the system for GDB; a
 * cancelled step goes on.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `ctx` | `interrupt_frame *` | [in,out] The context of the trap. |
 * | `reason` | `jet_gdb_stop_reason_e` | [in] Why it trapped. |
 * | `addr` | `uintptr_t` | [in] The address of a watchpoint, if any. |
 *
 * @return FALSE if the trap isn't GDB's (no session).
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `gdb_in_server` | `pok_bool_t` | Whether the CPU runs the GDB server |
 * | `gdb_session` | `pok_bool_t` | Whether a GDB session is open |
 * | `gdb_step_cancelled` | `pok_bool_t` | Whether the pending step was cancelled |
 * | `gdb_step_pending` | `pok_bool_t` | Whether a single step is pending |
 * | `gdb_step_thread` | `gdb_thread_t` | The thread that steps |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `current` | `gdb_thread_t` | The thread that stopped |
 * | `handled` | `pok_bool_t` | Whether the trap is GDB's |
 * | `signal` | `uint32_t` | The signal of a request |
 *
 * @hlr
 * - \hlrid{HLR_GDBSERVER_JET_GDB_TRAP_01}: `jet_gdb_trap()` shall stop the
 *   module for the debugger at a request, a breakpoint, a watchpoint or the end
 *   of a step during a session, and tell the trap handler whether the trap was
 *   the debugger's.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_DBG_014}
 *   - \srqref{SRQ_JET_DBG_006}
 * - \hlrid{HLR_GDBSERVER_JET_GDB_TRAP_02}: `jet_gdb_trap()` shall treat a trap
 *   inside the GDB server as fatal.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_DBG_011}
 *
 * @llr
 * - \llrid{LLR_GDBSERVER_JET_GDB_TRAP_01}: If `gdb_in_server` is true,
 *   `jet_gdb_trap()` shall call `pok_fatal()` with `"GDB: trap inside the GDB
 *   server"`.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDBSERVER_JET_GDB_TRAP_02}
 * - \llrid{LLR_GDBSERVER_JET_GDB_TRAP_02}: `jet_gdb_trap()` shall take the
 *   current thread with `gdb_thread_get_current()` for `ctx`, and take
 *   `gdb_step_thread` in its place if and only if `reason` is
 *   `JET_GDB_STOP_STEP`, `gdb_step_pending` is true, `pid` of the current
 *   thread is `GDB_PID_KERNEL` and `pid` of `gdb_step_thread` is
 *   `GDB_PID_KERNEL`.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDBSERVER_JET_GDB_TRAP_01}
 * - \llrid{LLR_GDBSERVER_JET_GDB_TRAP_03}: If `reason` is
 *   `JET_GDB_STOP_REQUEST`, `jet_gdb_trap()` shall call `gdb_stop()` once with
 *   `ctx`, the thread, the result of `gdb_request_signal()`, `reason` and
 *   `addr`, and return `TRUE`.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDBSERVER_JET_GDB_TRAP_01}
 * - \llrid{LLR_GDBSERVER_JET_GDB_TRAP_04}: If `reason` is `JET_GDB_STOP_STEP`,
 *   `gdb_step_pending` is true, `gdb_step_cancelled` is true or `gdb_session`
 *   is false, and `gdb_thread_equal()` returns true for the thread and
 *   `gdb_step_thread`, `jet_gdb_trap()` shall set `gdb_step_pending` to `FALSE`
 *   and return `TRUE` without calling `gdb_stop()`.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDBSERVER_JET_GDB_TRAP_01}
 * - \llrid{LLR_GDBSERVER_JET_GDB_TRAP_05}: If `reason` is not
 *   `JET_GDB_STOP_REQUEST`, the step of the requirement before is not the case
 *   and `gdb_session` is false, `jet_gdb_trap()` shall return `FALSE` without
 *   calling `gdb_stop()`.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDBSERVER_JET_GDB_TRAP_01}
 * - \llrid{LLR_GDBSERVER_JET_GDB_TRAP_06}: If `reason` is not
 *   `JET_GDB_STOP_REQUEST`, the step of the requirement before the last is not
 *   the case and `gdb_session` is true, `jet_gdb_trap()` shall call
 *   `gdb_stop()` once with `ctx`, the thread, `GDB_SIGNAL_TRAP`, `reason` and
 *   `addr`, and return `TRUE`.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDBSERVER_JET_GDB_TRAP_01}
 *
 * @design
 * 1. Set `handled` to `TRUE`. \llrref{LLR_GDBSERVER_JET_GDB_TRAP_03},
 *    \llrref{LLR_GDBSERVER_JET_GDB_TRAP_04},
 *    \llrref{LLR_GDBSERVER_JET_GDB_TRAP_06}
 * 2. If `gdb_in_server` is true: \llrref{LLR_GDBSERVER_JET_GDB_TRAP_01}
 *    1. Call `pok_fatal()` with the argument
 *       `"GDB: trap inside the GDB server"`.
 * 3. Call `gdb_thread_get_current()` with the arguments the address of
 *    `current` and `ctx`. \llrref{LLR_GDBSERVER_JET_GDB_TRAP_02}
 * 4. If `reason` is equal to `JET_GDB_STOP_STEP` and `gdb_step_pending` is true
 *    and `current.pid` is equal to `GDB_PID_KERNEL` and `gdb_step_thread.pid`
 *    is equal to `GDB_PID_KERNEL`: \llrref{LLR_GDBSERVER_JET_GDB_TRAP_02}
 *    1. Set `current` to `gdb_step_thread`.
 * 5. If `reason` is equal to `JET_GDB_STOP_REQUEST`:
 *    \llrref{LLR_GDBSERVER_JET_GDB_TRAP_03}
 *    1. Call `gdb_request_signal()`, and store the result in `signal`.
 *    2. Call `gdb_stop()` with the arguments `ctx`, the address of `current`,
 *       `signal`, `reason` and `addr`.
 * 6. Otherwise, if `reason` is equal to `JET_GDB_STOP_STEP` and
 *    `gdb_step_pending` is true and (`gdb_step_cancelled` is true or
 *    `gdb_session` is false) and `gdb_thread_equal(&current, &gdb_step_thread)`
 *    is true: \llrref{LLR_GDBSERVER_JET_GDB_TRAP_04}
 *    1. Set `gdb_step_pending` to `FALSE`.
 * 7. Otherwise, if `gdb_session` is false:
 *    \llrref{LLR_GDBSERVER_JET_GDB_TRAP_05}
 *    1. Set `handled` to `FALSE`.
 * 8. Otherwise: \llrref{LLR_GDBSERVER_JET_GDB_TRAP_06}
 *    1. Call `gdb_stop()` with the arguments `ctx`, the address of `current`,
 *       `GDB_SIGNAL_TRAP`, `reason` and `addr`.
 * 9. Return `handled`. \llrref{LLR_GDBSERVER_JET_GDB_TRAP_03},
 *    \llrref{LLR_GDBSERVER_JET_GDB_TRAP_04},
 *    \llrref{LLR_GDBSERVER_JET_GDB_TRAP_05},
 *    \llrref{LLR_GDBSERVER_JET_GDB_TRAP_06}
 *
 * @calls
 * - `pok_fatal()`
 * - `gdb_thread_get_current()`
 * - `gdb_request_signal()`
 * - `gdb_stop()`
 * - `gdb_thread_equal()`
 */
pok_bool_t jet_gdb_trap(interrupt_frame* ctx,
    jet_gdb_stop_reason_e reason, uintptr_t addr)
{
    pok_bool_t handled = TRUE;
    gdb_thread_t current;

    if (gdb_in_server) {
        pok_fatal("GDB: trap inside the GDB server");
    }

    gdb_thread_get_current(&current, ctx);

    if ((reason == JET_GDB_STOP_STEP) && gdb_step_pending
        && (current.pid == GDB_PID_KERNEL)
        && (gdb_step_thread.pid == GDB_PID_KERNEL))
    {
        /*
         * A step of the kernel's code ends in the context which has
         * stepped, but the thread which stands for it may have changed
         * (the step has switched partitions): it's still the one which
         * has stepped.
         */
        current = gdb_step_thread;
    }

    if (reason == JET_GDB_STOP_REQUEST)
    {
        uint32_t signal = gdb_request_signal();

        gdb_stop(ctx, &current, signal, reason, addr);
    }
    else if ((reason == JET_GDB_STOP_STEP) && gdb_step_pending
        && (gdb_step_cancelled || !gdb_session)
        && gdb_thread_equal(&current, &gdb_step_thread))
    {
        // The step was cancelled: just go on.
        gdb_step_pending = FALSE;
    }
    else if (!gdb_session)
    {
        handled = FALSE;
    }
    else
    {
        gdb_stop(ctx, &current, GDB_SIGNAL_TRAP, reason, addr);
    }

    return handled;
}

/**
 * @funcname `jet_gdb_request`
 *
 * @description
 * Asks the GDB server to stop the system for a character that GDB sent (a
 * trap).
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `c` | `int32_t` | [in] The character. |
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `gdb_request_char` | `int32_t` | The character that started the request, or -1 |
 *
 * @locals
 * None.
 *
 * @hlr
 * - \hlrid{HLR_GDBSERVER_JET_GDB_REQUEST_01}: `jet_gdb_request()` shall make
 *   the GDB server stop the module for a character that the debugger sent.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_DBG_001}
 *   - \srqref{SRQ_JET_DBG_006}
 *
 * @llr
 * - \llrid{LLR_GDBSERVER_JET_GDB_REQUEST_01}: `jet_gdb_request()` shall set
 *   `gdb_request_char` to `c` and then call `pok_trap()` once.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDBSERVER_JET_GDB_REQUEST_01}
 *
 * @design
 * 1. Set `gdb_request_char` to `c`. \llrref{LLR_GDBSERVER_JET_GDB_REQUEST_01}
 * 2. Call `pok_trap()`. \llrref{LLR_GDBSERVER_JET_GDB_REQUEST_01}
 *
 * @calls
 * - `pok_trap()`
 */
void jet_gdb_request(int32_t c)
{
    gdb_request_char = c;
    pok_trap();
}

#endif /* POK_NEEDS_GDB */
