/*
 * Modified by Kirill Brazhnikov, 2026-11-02: MISRA C:2025: essentially
 * Boolean conditions; the partition error handler takes a pok_bool_t.
 *
 * Modified by Kirill Brazhnikov, 2026-11-04: ported to x86_64: the GDB
 * partition enters the GDB server through jet_gdb_request() only when GDB
 * sends something (a packet, an acknowledgement or Ctrl-C) and drops
 * other characters; its stack has the default kernel size, as the server
 * runs on it; prototyped static functions, gdb_operations is defined in
 * the function that uses it, no message on every entry.
 *
 * Modified by Kirill Brazhnikov, 2026-11-08: Doxygen headers for the file
 * and its functions.
 *
 * Modified by Kirill Brazhnikov, 2026-11-13: pok_part_ops_t is used
 * by its typedef name; the macro is moved to gdb.h.
 *
 * Modified by Kirill Brazhnikov, 2026-11-14: The function headers no longer
 * have a step for the declaration of a local variable; the @locals table
 * describes it, and an initialized one is set in a step.
 */

/**
 * @file
 * The GDB partition: a kernel-only partition which runs in the time slots
 * of type GDB, waits for GDB's characters on the second serial port (the
 * debug console) and hands requests to the GDB server.
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


/**
 * @funcname `gdb_is_start_char`
 *
 * @description
 * Tells whether a character starts a GDB request: a packet ($), an
 * acknowledgment (+) or an interrupt (Ctrl-C).
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `c` | `int32_t` | [in] The character. |
 *
 * @return TRUE if it does.
 *
 * @globals
 * None.
 *
 * @locals
 * None.
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Return
 *    `(c == (int32_t)'$') || (c == (int32_t)'+') || (c == GDB_CHAR_INTERRUPT)`.
 *
 * @calls
 * None.
 */
static pok_bool_t gdb_is_start_char(int32_t c)
{
    return (c == (int32_t)'$') || (c == (int32_t)'+')
        || (c == GDB_CHAR_INTERRUPT);
}

/**
 * @funcname `pok_gdb_thread`
 *
 * @description
 * The GDB server's thread: forever, reads the debug console and handles each
 * request with preemption disabled.
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `c` | `int32_t` | The character read |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Repeat forever:
 *    1. If `data_to_read_1()` is equal to `1`:
 *       1. Call `read_serial_1()`, and store the result in `c`.
 *       2. If `gdb_is_start_char(c)` is true:
 *          1. Call `ja_preempt_disable()`.
 *          2. Call `jet_gdb_request()` with the argument `c`.
 *          3. Call `ja_preempt_enable()`.
 *
 * @calls
 * - `data_to_read_1()`
 * - `read_serial_1()`
 * - `gdb_is_start_char()`
 * - `ja_preempt_disable()`
 * - `jet_gdb_request()`
 * - `ja_preempt_enable()`
 */
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

/**
 * @funcname `gdb_process_error`
 *
 * @description
 * The error processing of the GDB partition: any error in it is fatal.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `partition_state` | `pok_system_state_t` | [in] The system state (not used). |
 * | `error_id` | `pok_error_id_t` | [in] The error (not used). |
 * | `state_byte_preempt_local` | `pok_bool_t` | [in] Whether local preemption was disabled (not used). |
 * | `failed_address` | `void *` | [in] The faulting address (not used). |
 *
 * @globals
 * None.
 *
 * @locals
 * None.
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Mark `partition_state` as not used.
 * 2. Mark `error_id` as not used.
 * 3. Mark `state_byte_preempt_local` as not used.
 * 4. Mark `failed_address` as not used.
 * 5. Call `pok_fatal()` with the argument `"Error in gdb"`.
 *
 * @calls
 * - `pok_fatal()`
 */
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

/**
 * @funcname `pok_gdb_thread_init`
 *
 * @description
 * Initializes the GDB server's partition at boot: a kernel-only partition
 * running pok_gdb_thread().
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `partition_gdb` | `pok_partition_t` | The GDB server partition |
 * | `partition_sched_ops_kernel` | `const pok_part_sched_ops_t` | The scheduler operations of kernel-only partitions |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `gdb_operations` | `static const pok_part_ops_t` | The operations of the GDB partition |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Call `pok_partition_init()` with the address of `partition_gdb`.
 * 2. Set `partition_gdb.part_sched_ops` to the address of
 *    `partition_sched_ops_kernel`.
 * 3. Set `partition_gdb.part_ops` to the address of `gdb_operations`.
 * 4. Call `pok_stack_alloc()` with the argument `KERNEL_STACK_SIZE_DEFAULT`,
 *    and store the result in `partition_gdb.initial_sp`.
 * 5. Call `printf()` with the argument
 *    `"GDB server: connect GDB to the second serial port\n"`.
 *
 * @calls
 * - `pok_partition_init()`
 * - `pok_stack_alloc()`
 * - `printf()`
 */
void pok_gdb_thread_init(void)
{
    static const pok_part_ops_t gdb_operations =
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
