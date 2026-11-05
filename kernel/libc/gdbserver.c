/* Modified by Kirill Brazhnikov, 2026-10-31: built-in integer types (int,
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

/* Maximum size of the data of a packet (PacketSize). */
#define GDB_PACKET_SIZE 4096U

/* Software breakpoints GDB may insert at the same time. */
#define GDB_SW_BREAKPOINTS 64U

/* Process of the kernel-only partitions. */
#define GDB_PID_KERNEL 1U
/* Largest process: 255 user partitions (pok_partitions_arinc_n) + 1. */
#define GDB_PID_MAX 256U

/* Signals reported to GDB. */
#define GDB_SIGNAL_NONE 0U
#define GDB_SIGNAL_INT  2U
#define GDB_SIGNAL_TRAP 5U

/* Ctrl-C from GDB. */
#define GDB_CHAR_INTERRUPT 3

/* No index (of a partition). */
#define GDB_NO_INDEX 0xFFFFFFFFU

/* Thread as GDB sees it. */
struct gdb_thread
{
    uint32_t pid;       /* Process, from 1. */
    uint32_t tid;       /* Thread within the process, from 1. */
    pok_partition_t* part;
    void* part_private; /* Thread data of the partition. */
};

/* Thread id as GDB writes it: -1 means all, 0 means any. */
struct gdb_thread_ref
{
    int64_t pid;
    int64_t tid;
};

/* Software breakpoint inserted by GDB. */
struct gdb_sw_breakpoint
{
    pok_bool_t used;
    jet_space_id space_id;
    uintptr_t addr;
    uint8_t saved[JA_GDB_BREAKPOINT_SIZE]; /* Code under the breakpoint. */
};

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
static struct jet_interrupt_context* gdb_stop_ctx;
static struct gdb_thread gdb_current;
static uint32_t gdb_stop_signal;
static enum jet_gdb_stop_reason gdb_stop_reason;
static uintptr_t gdb_stop_addr;
/* Thread for registers, memory and breakpoints (Hg). */
static struct gdb_thread gdb_selected;
/* Whether GDB has written registers (FP ones must be reloaded). */
static pok_bool_t gdb_registers_changed;

/*
 * Single step: the thread which steps. If the system stops for another
 * reason first, the step is cancelled: its trap is ignored when it comes.
 */
static pok_bool_t gdb_step_pending = FALSE;
static pok_bool_t gdb_step_cancelled = FALSE;
static struct gdb_thread gdb_step_thread;

/* Next thread for qsThreadInfo (pid 0: no more). */
static uint32_t gdb_list_pid;
static uint32_t gdb_list_tid;

static struct gdb_sw_breakpoint gdb_sw_breakpoints[GDB_SW_BREAKPOINTS];

/***********************************************************************
 * Partitions and threads.
 */

/* State of a search of partitions with for_each_partition(). */
static struct
{
    pok_bool_t is_kernel;          /* Kernel-only or user partitions. */
    uint32_t index;                /* Index of the partition wanted, */
    const pok_partition_t* wanted; /* or the partition wanted. */
    uint32_t count;                /* Partitions of that kind visited. */
    pok_partition_t* found;        /* Partition at 'index'. */
    uint32_t found_index;          /* Index of 'wanted'. */
} gdb_search;

static pok_bool_t gdb_partition_is_kernel(const pok_partition_t* part)
{
    return part->space_id == 0U;
}

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

/* Visit the partitions of one kind, looking for 'index' or 'wanted'. */
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

/* Partition with 'index' among kernel-only or user ones, or NULL. */
static pok_partition_t* gdb_partition_at(pok_bool_t is_kernel,
    uint32_t index)
{
    gdb_search_run(is_kernel, index, NULL);
    return gdb_search.found;
}

/* Number of kernel-only or user partitions. */
static uint32_t gdb_partition_count(pok_bool_t is_kernel)
{
    gdb_search_run(is_kernel, GDB_NO_INDEX, NULL);
    return gdb_search.count;
}

/* Index of the partition among the partitions of its kind. */
static uint32_t gdb_partition_index(const pok_partition_t* part)
{
    gdb_search_run(gdb_partition_is_kernel(part), GDB_NO_INDEX, part);
    return gdb_search.found_index;
}

/* Process of the partition. */
static uint32_t gdb_partition_pid(const pok_partition_t* part)
{
    uint32_t pid = GDB_PID_KERNEL;

    if (!gdb_partition_is_kernel(part)) {
        pid = gdb_partition_index(part) + 2U;
    }

    return pid;
}

/* User partition of process 'pid', or NULL. */
static pok_partition_t* gdb_process_partition(uint32_t pid)
{
    pok_partition_t* part = NULL;

    if ((pid >= 2U) && (pid <= GDB_PID_MAX)) {
        part = gdb_partition_at(FALSE, pid - 2U);
    }

    return part;
}

/* Largest process id. */
static uint32_t gdb_process_last(void)
{
    return gdb_partition_count(FALSE) + 1U;
}

/* Number of threads of process 'pid' (0 if there is no such process). */
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

/* Index of the thread within its partition. */
static int32_t gdb_thread_index(const struct gdb_thread* t)
{
    // A kernel-only partition has a single thread.
    uint32_t index = (t->pid == GDB_PID_KERNEL) ? 0U : (t->tid - 1U);

    return (int32_t)index;
}

/* Fill 't' for thread 'tid' of process 'pid'. FALSE if there is none. */
static pok_bool_t gdb_thread_find(struct gdb_thread* t, uint32_t pid,
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

static pok_bool_t gdb_thread_equal(const struct gdb_thread* t1,
    const struct gdb_thread* t2)
{
    return (t1->pid == t2->pid) && (t1->tid == t2->tid);
}

/* Current thread of process 'pid'. */
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

/*
 * The thread which runs now, when the system has stopped with registers
 * 'ctx'.
 */
static void gdb_thread_get_current(struct gdb_thread* t,
    const struct jet_interrupt_context* ctx)
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

/* Thread GDB refers to; any thread of a process is its current one. */
static pok_bool_t gdb_thread_resolve(struct gdb_thread* t,
    const struct gdb_thread_ref* ref)
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

/* Registers of a thread (NULL if it has none saved). */
static struct jet_interrupt_context* gdb_thread_regs(
    const struct gdb_thread* t)
{
    struct jet_interrupt_context* ctx;
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

/* Floating point registers of a thread (NULL if it has none). */
static struct jet_fp_store* gdb_thread_fp(const struct gdb_thread* t)
{
    struct jet_fp_store* fp = NULL;
    const struct pok_partition_sched_operations* ops =
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

static pok_bool_t gdb_is_attached(uint32_t pid)
{
    return (pid <= GDB_PID_MAX) && gdb_attached[pid];
}

static void gdb_attach(uint32_t pid)
{
    if (pid <= GDB_PID_MAX) {
        gdb_attached[pid] = TRUE;
    }
}

static void gdb_detach_all(void)
{
    for (uint32_t pid = 0U; pid <= GDB_PID_MAX; pid++)
    {
        gdb_attached[pid] = FALSE;
    }
}

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

/*
 * Process listed after 'pid' (0: the first): user processes, then the
 * kernel's (GDB used to forget all inferiors except the last listed one,
 * so it couldn't find the address space of the current kernel thread).
 * 0 after the last.
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

/* Go to the next thread of an attached process. FALSE at the end. */
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

static struct gdb_sw_breakpoint* gdb_sw_find(jet_space_id space_id,
    uintptr_t addr)
{
    struct gdb_sw_breakpoint* bp = NULL;

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

/* Save the code under the breakpoint and write the breakpoint. */
static pok_bool_t gdb_sw_write(struct gdb_sw_breakpoint* bp)
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

/* Put the code under the breakpoint back. */
static void gdb_sw_restore(const struct gdb_sw_breakpoint* bp)
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
            struct gdb_sw_breakpoint* bp = &gdb_sw_breakpoints[i];

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

static pok_bool_t gdb_sw_remove(jet_space_id space_id, uintptr_t addr)
{
    struct gdb_sw_breakpoint* bp = gdb_sw_find(space_id, addr);
    pok_bool_t removed = FALSE;

    if (bp != NULL)
    {
        gdb_sw_restore(bp);
        bp->used = FALSE;
        removed = TRUE;
    }

    return removed;
}

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

pok_bool_t jet_gdb_is_breakpoint(jet_space_id space_id, uintptr_t addr)
{
    return gdb_sw_find(space_id, addr) != NULL;
}

void jet_gdb_space_loaded(jet_space_id space_id)
{
    for (uint32_t i = 0U; i < GDB_SW_BREAKPOINTS; i++)
    {
        struct gdb_sw_breakpoint* bp = &gdb_sw_breakpoints[i];

        if (bp->used && (bp->space_id == space_id))
        {
            // The code is new: save it again.
            bp->used = gdb_sw_write(bp);
        }
    }
}

/*
 * Memory as GDB should see it: the code under the breakpoints inserted
 * into [addr, addr + len) of space 'space_id' instead of the breakpoint
 * instructions ('mem' is a copy of that memory).
 */
static void gdb_sw_hide(jet_space_id space_id, uintptr_t addr, uint8_t* mem,
    size_t len)
{
    for (uint32_t i = 0U; i < GDB_SW_BREAKPOINTS; i++)
    {
        const struct gdb_sw_breakpoint* bp = &gdb_sw_breakpoints[i];

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

/*
 * GDB has written [addr, addr + len) of space 'space_id' (kernel address
 * 'mem'): the new code under the breakpoints there is saved, and the
 * breakpoint instructions are written again.
 */
static void gdb_sw_rewrite(jet_space_id space_id, uintptr_t addr,
    uint8_t* mem, size_t len)
{
    for (uint32_t i = 0U; i < GDB_SW_BREAKPOINTS; i++)
    {
        struct gdb_sw_breakpoint* bp = &gdb_sw_breakpoints[i];

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

static void gdb_put_chars(const char* s, size_t len)
{
    (void) pok_cons_write_1(s, len);
}

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

/* Value of hexadecimal digit 'c', or -1. */
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

/*
 * Read the rest of a packet after '$' into gdb_in and acknowledge it.
 * FALSE if it is lost (bad checksum or too long): GDB sends it again.
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

/* Receive a packet into gdb_in. */
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

/* Wait for GDB's acknowledgement: '+' or '-'. */
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

/* Send the reply in gdb_out; send it again until GDB receives it. */
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

static void out_reset(void)
{
    gdb_out_len = 0U;
}

static void out_char(char c)
{
    if (gdb_out_len < GDB_PACKET_SIZE)
    {
        gdb_out[gdb_out_len] = c;
        gdb_out_len++;
    }
}

static void out_str(const char* s)
{
    for (size_t i = 0U; s[i] != '\0'; i++)
    {
        out_char(s[i]);
    }
}

static void out_byte(uint8_t b)
{
    out_char(gdb_hex_chars[b >> 4]);
    out_char(gdb_hex_chars[b & 0xFU]);
}

static void out_bytes(const uint8_t* data, size_t len)
{
    for (size_t i = 0U; i < len; i++)
    {
        out_byte(data[i]);
    }
}

/* 'len' bytes which are not available. */
static void out_unavailable(size_t len)
{
    for (size_t i = 0U; i < len; i++)
    {
        out_str("xx");
    }
}

/* Number in hexadecimal, without leading zeros. */
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

static void out_thread_id(const struct gdb_thread* t)
{
    out_char('p');
    out_hex(t->pid);
    out_char('.');
    out_hex(t->tid);
}

/* Callback of type print_cb_t: the data, hex-encoded. */
static void out_print_cb(const void* data, size_t len, void* cb_data)
{
    const uint8_t* bytes = data;

    (void) cb_data;
    out_bytes(bytes, len);
}

/***********************************************************************
 * Parsing the packet.
 */

static char in_peek(void)
{
    return (gdb_in_pos < gdb_in_len) ? gdb_in[gdb_in_pos] : '\0';
}

/* Skip the next character (gdb_in_pos never passes the end). */
static void in_skip(void)
{
    if (gdb_in_pos < gdb_in_len) {
        gdb_in_pos++;
    }
}

/* Skip 'c' if it is next. */
static pok_bool_t in_expect(char c)
{
    pok_bool_t found = (gdb_in_pos < gdb_in_len) && (gdb_in[gdb_in_pos] == c);

    if (found) {
        gdb_in_pos++;
    }

    return found;
}

/* Skip 'prefix' if the rest of the packet starts with it. */
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

/* Read a hexadecimal number. FALSE if there is none or it's too big. */
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

/* Read 'len' hex-encoded bytes. */
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

/* Process or thread id: -1 (all) or a hexadecimal number. */
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

/* Thread id: [p<pid>[.<tid>]] or <tid>. */
static pok_bool_t in_thread_ref(struct gdb_thread_ref* ref)
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

/* Whether the qSupported packet lists 'feature' (e.g. "swbreak+"). */
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

/* GDB has connected. */
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

/* GDB has gone: remove what it has inserted. */
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

/* Stop reply: signal, thread, some registers, reason. */
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

/* qfThreadInfo, qsThreadInfo: as many threads as fit. */
static void gdb_cmd_thread_info(void)
{
    struct gdb_thread t;
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

/* qThreadExtraInfo: partition and process name, state. */
static void gdb_cmd_thread_extra_info(void)
{
    struct gdb_thread_ref ref;
    struct gdb_thread t;

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

/* H: thread for the following commands. */
static void gdb_cmd_set_thread(void)
{
    struct gdb_thread_ref ref;
    struct gdb_thread t;
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

/* T: whether the thread exists. */
static void gdb_cmd_thread_alive(void)
{
    struct gdb_thread_ref ref;
    struct gdb_thread t;

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

/* g: registers of the selected thread. */
static void gdb_cmd_read_registers(void)
{
    const struct jet_interrupt_context* ctx = gdb_thread_regs(&gdb_selected);
    const struct jet_fp_store* fp = gdb_thread_fp(&gdb_selected);
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

/*
 * G: write registers of the selected thread ('x': leave it).
 *
 * The registers of a thread which the scheduler has switched off in the
 * kernel are in a kernel interrupt frame: changing them would change
 * where the kernel resumes, so they are left (only its user space
 * registers, saved on entry to the kernel, and those of the thread which
 * has stopped may change).
 */
static void gdb_cmd_write_registers(void)
{
    struct jet_interrupt_context* ctx = gdb_thread_regs(&gdb_selected);
    struct jet_fp_store* fp = gdb_thread_fp(&gdb_selected);

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

/* Space of the selected thread: memory and breakpoints are there. */
static jet_space_id gdb_selected_space(void)
{
    return gdb_selected.part->space_id;
}

/* m: read memory. A shorter reply is fine if the end isn't readable. */
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

/* M: write memory. */
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

/* Z, z: insert or remove a breakpoint or watchpoint. */
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
        enum ja_gdb_hw_type hw_type = JA_GDB_HW_EXEC;

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

/* c, C, s, S: resume (from 'addr', if given), maybe for one step. */
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

/* Detach from process 'pid'. TRUE if none is left: GDB has gone. */
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

/*
 * vAttach;pid: reply with a stop of the process. Its thread becomes the
 * selected one, which qC reports.
 */
static void gdb_cmd_attach(void)
{
    uintptr_t pid;
    struct gdb_thread t;

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

/* v packets. TRUE: resume the system. */
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

/* D[;pid]: detach. TRUE: resume the system. */
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

/* Process the packet in gdb_in and reply. TRUE: resume the system. */
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

/* Talk to GDB until it resumes the system. */
static void gdb_stop(struct jet_interrupt_context* ctx,
    const struct gdb_thread* current, uint32_t signal,
    enum jet_gdb_stop_reason reason, uintptr_t addr)
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

/* Signal of a stop by the GDB partition or at boot. */
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

pok_bool_t jet_gdb_trap(struct jet_interrupt_context* ctx,
    enum jet_gdb_stop_reason reason, uintptr_t addr)
{
    pok_bool_t handled = TRUE;
    struct gdb_thread current;

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

void jet_gdb_request(int32_t c)
{
    gdb_request_char = c;
    pok_trap();
}

#endif /* POK_NEEDS_GDB */
