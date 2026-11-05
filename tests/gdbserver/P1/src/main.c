/*
 * Copyright (C) 2026 Kirill Brazhnikov <agamemnon886@gmail.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * GDB server test, partitions P1 and P2: both are built from this file,
 * so the same code is at the same addresses in both.
 *
 * Process "worker" waits 100 ms between each of its ITERATIONS periods.
 * In each period it stores the system time in gdb_time (GDB checks that
 * the time doesn't run while it holds the system) and calls gdb_marker(),
 * where GDB sets breakpoints, which writes gdb_counter, which GDB
 * watches; it does some floating point work, so that GDB sees x87 and SSE
 * registers, and it prints "Pn POKE <value>" when GDB has changed
 * gdb_poke. Errors go to the error handler and fail the test.
 *
 * The worker has no deadline: under QEMU the system time follows the
 * host's, which sometimes stalls QEMU for seconds.
 *
 * Without GDB (make run) the test checks that the partitions run with the
 * GDB partition in the schedule. tests/gdbserver/run_gdb_test.sh runs it
 * with GDB.
 *
 * The last line is "Pn DONE: <n> failure(s)" (see tests/README).
 *
 * Modified by Kirill Brazhnikov, 2026-11-05: the comment doesn't say that
 * the GDB server is on x86_64 only.
 */
#include <stdio.h>
#include <string.h>
#include <arinc653/types.h>
#include <arinc653/partition.h>
#include <arinc653/process.h>
#include <arinc653/time.h>
#include <arinc653/error.h>

#define MS 1000000LL
#define ITERATIONS 60
#define WORKER_PERIOD (100 * MS)

/* Read and written by GDB. */
volatile SYSTEM_TIME_TYPE gdb_time;
volatile int32_t gdb_counter;
volatile int32_t gdb_poke;
volatile double gdb_value;

static int32_t fails;
static int32_t partition_id;

static void check(const char *name, int64_t got, int64_t want)
{
    if (got == want) {
        printf("TEST P%d %s: PASS\n", (int) partition_id, name);
    } else {
        printf("TEST P%d %s: FAIL (got %lld, want %lld)\n",
               (int) partition_id, name, (long long) got, (long long) want);
        fails++;
    }
}

/* GDB stops here. */
__attribute__((noinline)) void gdb_marker(int32_t n)
{
    gdb_counter = n;
}

static void error_handler(void)
{
    ERROR_STATUS_TYPE es;
    RETURN_CODE_TYPE ret;

    for (;;) {
        GET_ERROR_STATUS(&es, &ret);
        if (ret != NO_ERROR)
            break;
        printf("TEST P%d error %d of process %d: FAIL\n",
               (int) partition_id, (int) es.ERROR_CODE,
               (int) es.FAILED_PROCESS_ID);
        fails++;
    }
    STOP_SELF();
}

static void worker(void)
{
    RETURN_CODE_TYPE ret;
    int32_t poke = 0;
    int32_t i;
    double x = 1.0;
    SYSTEM_TIME_TYPE now;

    for (i = 1; i <= ITERATIONS; i++) {
        GET_TIME(&now, &ret);
        gdb_time = now;
        gdb_marker(i);
        x = (x * 1.5) + 0.25;
        gdb_value = x;
        if (gdb_poke != poke) {
            poke = gdb_poke;
            printf("P%d POKE %d\n", (int) partition_id, (int) poke);
        }
        TIMED_WAIT(WORKER_PERIOD, &ret);
        if (ret != NO_ERROR)
            check("TIMED_WAIT", ret, NO_ERROR);
    }

    check("periods done", gdb_counter, ITERATIONS);
    printf("P%d DONE: %d failure(s)\n", (int) partition_id, (int) fails);
    STOP_SELF();
}

void main(void)
{
    RETURN_CODE_TYPE ret;
    PARTITION_STATUS_TYPE pst;
    PROCESS_ID_TYPE pid = 0;
    PROCESS_ATTRIBUTE_TYPE attrs = {
        .PERIOD = INFINITE_TIME_VALUE,
        .TIME_CAPACITY = INFINITE_TIME_VALUE,
        .STACK_SIZE = 8192,
        .BASE_PRIORITY = 10,
        .DEADLINE = SOFT,
        .ENTRY_POINT = worker,
    };

    GET_PARTITION_STATUS(&pst, &ret);
    partition_id = (int32_t) pst.IDENTIFIER;

    CREATE_ERROR_HANDLER(error_handler, 8192, &ret);
    check("CREATE_ERROR_HANDLER", ret, NO_ERROR);

    strncpy(attrs.NAME, "worker", sizeof(PROCESS_NAME_TYPE));
    CREATE_PROCESS(&attrs, &pid, &ret);
    check("CREATE_PROCESS worker", ret, NO_ERROR);
    START(pid, &ret);
    check("START worker", ret, NO_ERROR);

    SET_PARTITION_MODE(NORMAL, &ret);
    check("SET_PARTITION_MODE returned", 1, 0);
}
