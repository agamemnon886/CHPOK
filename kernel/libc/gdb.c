/* Modified by Kirill Brazhnikov, 2026-11-02: MISRA C:2025: essentially
 * Boolean conditions; the partition error handler takes a pok_bool_t.
 *
 * Modified by Kirill Brazhnikov, 2026-11-04: ported to x86_64: the GDB
 * partition enters the GDB server through jet_gdb_request() only when GDB
 * sends something (a packet, an acknowledgement or Ctrl-C) and drops
 * other characters; its stack has the default kernel size, as the server
 * runs on it; prototyped static functions, gdb_operations is defined in
 * the function that uses it, no message on every entry.
 */

/*
 * GDB partition: a kernel-only partition which runs in the time slots of
 * type GDB and waits for GDB on the second serial port.
 */

#include <config.h>

#ifdef POK_NEEDS_GDB

#include <libc.h>
#include <asp/arch.h>
#include <asp/stack.h>
#include <core/debug.h>
#include <core/partition.h>
#include <core/sched.h>
#include <cons.h>
#include <gdb.h>

/* Ctrl-C from GDB. */
#define GDB_CHAR_INTERRUPT 3

/*
 * Characters which start a talk with GDB: a packet, an acknowledgement
 * (GDB sends one when it connects) or Ctrl-C.
 */
static pok_bool_t gdb_is_start_char(int32_t c)
{
    return (c == (int32_t)'$') || (c == (int32_t)'+')
        || (c == GDB_CHAR_INTERRUPT);
}

static void pok_gdb_thread(void)
{
    for (;;) {
        if (data_to_read_1() == 1) {
            int32_t c = read_serial_1();

            if (gdb_is_start_char(c)) {
                ja_preempt_disable();
                jet_gdb_request(c);
                ja_preempt_enable();
            }
            // Other characters are dropped.
        }
    }
}

static void gdb_process_error(pok_system_state_t partition_state,
        pok_error_id_t error_id,
        pok_bool_t state_byte_preempt_local,
        void* failed_address)
{
    (void) partition_state;
    (void) error_id;
    (void) state_byte_preempt_local;
    (void) failed_address;
    pok_fatal("Error in gdb");
}

void pok_gdb_thread_init(void)
{
    static const struct pok_partition_operations gdb_operations =
    {
        .start = pok_gdb_thread,
        .process_partition_error = gdb_process_error
    };

    pok_partition_init(&partition_gdb);
    partition_gdb.part_sched_ops = &partition_sched_ops_kernel;
    partition_gdb.part_ops = &gdb_operations;
    // The GDB server runs on this stack when GDB interrupts the system.
    partition_gdb.initial_sp = pok_stack_alloc(KERNEL_STACK_SIZE_DEFAULT);

    (void) printf("GDB server: connect GDB to the second serial port\n");
}

#endif /* POK_NEEDS_GDB */
