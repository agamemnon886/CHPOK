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
 * APEX test, partition P1.
 *
 * The partition is started four times; the start condition tells which
 * phase to run:
 *
 *   NORMAL_START          APEX type sizes, REPORT_APPLICATION_MESSAGE,
 *                         RAISE_APPLICATION_ERROR errors, CREATE_PROCESS
 *                         checks; then SET_PARTITION_MODE(COLD_START).
 *   PARTITION_RESTART     RAISE_APPLICATION_ERROR with LENGTH 0, which the
 *                         HM table turns into a partition restart.
 *   HM_PARTITION_RESTART  CREATE_SAMPLING_PORT checks; then a memory
 *                         violation, configured as a module level error
 *                         with action RESET.
 *   HM_MODULE_RESTART     ports again, case-insensitive names,
 *                         CREATE/CONFIGURE_ERROR_HANDLER, the multicore
 *                         process services; then a writer process for P2
 *                         and NORMAL mode, where the writer checks the
 *                         services that are refused in NORMAL mode.
 *
 * The return codes of CONFIGURE_ERROR_HANDLER and of the multicore
 * services are JetOS's choices, made from the ARINC653.h header without
 * the standard's text (see the comments in their implementations).
 *
 * Each check prints "TEST P1 <name>: PASS" or "... FAIL"; the last line is
 * "P1 DONE: <n> failure(s)" (see tests/README).
 */
#include <stdio.h>
#include <string.h>
#include <arinc653/types.h>
#include <arinc653/partition.h>
#include <arinc653/process.h>
#include <arinc653/time.h>
#include <arinc653/error.h>
#include <arinc653/sampling.h>
#include <arinc653/memblocks.h>

#define MS     1000000LL
#define SECOND 1000000000LL
#define DAY    (86400LL * SECOND)

static SAMPLING_PORT_ID_TYPE SP1, SP3;
static int32_t fails;

static void check(const char *name, int64_t got, int64_t want)
{
    if (got == want) {
        printf("TEST P1 %s: PASS\n", name);
    } else {
        printf("TEST P1 %s: FAIL (got %lld, want %lld)\n", name,
               (long long) got, (long long) want);
        fails++;
    }
}

static void writer(void)
{
    RETURN_CODE_TYPE ret;
    char msg[64] = "hello";

    CONFIGURE_ERROR_HANDLER(PROCESSES_PAUSE, 0, &ret);
    check("CONFIGURE_ERROR_HANDLER in NORMAL mode", ret, INVALID_MODE);
    {
        PROCESS_ID_TYPE me = -1;
        PROCESS_INDEX_TYPE index = -1;
        PROCESSOR_CORE_ID_TYPE core = -1;

        GET_MY_ID(&me, &ret);
        INITIALIZE_PROCESS_CORE_AFFINITY(me, 0, &ret);
        check("INITIALIZE_PROCESS_CORE_AFFINITY in NORMAL mode", ret, INVALID_MODE);
        GET_MY_INDEX(&index, &ret);
        check("GET_MY_INDEX in writer", ret, NO_ERROR);
        // First process created; the error handler was created before it.
        check("  index 1", index, 1);
        GET_MY_PROCESSOR_CORE_ID(&core, &ret);
        check("GET_MY_PROCESSOR_CORE_ID in writer", ret, NO_ERROR);
        check("  core 0", core, 0);
    }
    // SP3: written once, so P2 must see it become stale (1 ms refresh).
    WRITE_SAMPLING_MESSAGE(SP3, (MESSAGE_ADDR_TYPE) msg, 8, &ret);
    check("write SP3", ret, NO_ERROR);
    while (1) {
        WRITE_SAMPLING_MESSAGE(SP1, (MESSAGE_ADDR_TYPE) msg, 8, &ret);
        TIMED_WAIT(SECOND / 2, &ret);
    }
}

static void dummy(void)
{
    STOP_SELF();
}

static void error_handler(void)
{
    STOP_SELF();
}

static void test_configure_error_handler(void)
{
    RETURN_CODE_TYPE ret;

    CONFIGURE_ERROR_HANDLER(PROCESSES_PAUSE, 0, &ret);
    check("CONFIGURE_ERROR_HANDLER before CREATE_ERROR_HANDLER", ret, INVALID_CONFIG);
    CREATE_ERROR_HANDLER(error_handler, 8192, &ret);
    check("CREATE_ERROR_HANDLER", ret, NO_ERROR);
    CONFIGURE_ERROR_HANDLER(PROCESSES_PAUSE, 0, &ret);
    check("CONFIGURE_ERROR_HANDLER(PAUSE, core 0)", ret, NO_ERROR);
    CONFIGURE_ERROR_HANDLER(PROCESSES_SCHEDULED, CORE_AFFINITY_NO_PREFERENCE, &ret);
    check("CONFIGURE_ERROR_HANDLER(SCHEDULED, NO_PREFERENCE)", ret, NO_ERROR);
    CONFIGURE_ERROR_HANDLER((ERROR_HANDLER_CONCURRENCY_CONTROL_TYPE) 2, 0, &ret);
    check("CONFIGURE_ERROR_HANDLER concurrency 2", ret, INVALID_PARAM);
    CONFIGURE_ERROR_HANDLER((ERROR_HANDLER_CONCURRENCY_CONTROL_TYPE) -1, 0, &ret);
    check("CONFIGURE_ERROR_HANDLER concurrency -1", ret, INVALID_PARAM);
    CONFIGURE_ERROR_HANDLER(PROCESSES_PAUSE, 1, &ret);
    check("CONFIGURE_ERROR_HANDLER core 1 (partition has 1 core)", ret, INVALID_CONFIG);
    CONFIGURE_ERROR_HANDLER(PROCESSES_PAUSE, -2, &ret);
    check("CONFIGURE_ERROR_HANDLER core -2", ret, INVALID_CONFIG);
}

static void test_multicore(PROCESS_ID_TYPE pid)
{
    RETURN_CODE_TYPE ret;
    PROCESSOR_CORE_ID_TYPE core = -1;
    PROCESS_INDEX_TYPE index = -1;

    GET_MY_PROCESSOR_CORE_ID(&core, &ret);
    check("GET_MY_PROCESSOR_CORE_ID in main", ret, NO_ERROR);
    check("  core 0", core, 0);
    GET_MY_INDEX(&index, &ret);
    check("GET_MY_INDEX in main", ret, INVALID_MODE);

    INITIALIZE_PROCESS_CORE_AFFINITY(pid, 0, &ret);
    check("INITIALIZE_PROCESS_CORE_AFFINITY(writer, 0)", ret, NO_ERROR);
    INITIALIZE_PROCESS_CORE_AFFINITY(pid, CORE_AFFINITY_NO_PREFERENCE, &ret);
    check("INITIALIZE_PROCESS_CORE_AFFINITY(writer, NO_PREFERENCE)", ret, NO_ERROR);
    INITIALIZE_PROCESS_CORE_AFFINITY(pid, 1, &ret);
    check("INITIALIZE_PROCESS_CORE_AFFINITY core 1", ret, INVALID_CONFIG);
    INITIALIZE_PROCESS_CORE_AFFINITY(pid, -2, &ret);
    check("INITIALIZE_PROCESS_CORE_AFFINITY core -2", ret, INVALID_CONFIG);
    INITIALIZE_PROCESS_CORE_AFFINITY(0, 0, &ret);
    check("INITIALIZE_PROCESS_CORE_AFFINITY process 0", ret, INVALID_PARAM);
    INITIALIZE_PROCESS_CORE_AFFINITY(pid + 1, 0, &ret);
    check("INITIALIZE_PROCESS_CORE_AFFINITY not created", ret, INVALID_PARAM);
    // Would wrap to the writer's kernel thread ID without the range check.
    INITIALIZE_PROCESS_CORE_AFFINITY(pid + 256, 0, &ret);
    check("INITIALIZE_PROCESS_CORE_AFFINITY process ID + 256", ret, INVALID_PARAM);
    INITIALIZE_PROCESS_CORE_AFFINITY(1, 0, &ret);
    check("INITIALIZE_PROCESS_CORE_AFFINITY main process (ID 1)", ret, INVALID_PARAM);
    INITIALIZE_PROCESS_CORE_AFFINITY(2, 0, &ret);
    check("INITIALIZE_PROCESS_CORE_AFFINITY error handler (ID 2)", ret, INVALID_PARAM);
}

static void test_types(void)
{
    check("sizeof(APEX_INTEGER) == 4", sizeof(APEX_INTEGER), 4);
    check("sizeof(APEX_UNSIGNED) == 4", sizeof(APEX_UNSIGNED), 4);
    check("sizeof(MESSAGE_SIZE_TYPE) == 4", sizeof(MESSAGE_SIZE_TYPE), 4);
    check("sizeof(PROCESS_ID_TYPE) == 4", sizeof(PROCESS_ID_TYPE), 4);
    check("sizeof(SYSTEM_TIME_TYPE) == 8", sizeof(SYSTEM_TIME_TYPE), 8);
}

static void test_report(void)
{
    RETURN_CODE_TYPE ret;
    char text[] = "ABCDEFGHIJ";
    char nonul[4] = { 'W', 'X', 'Y', 'Z' };   // no terminating NUL

    REPORT_APPLICATION_MESSAGE((MESSAGE_ADDR_TYPE) text, 5, &ret);
    check("report prefix of string (expect 'ABCDE')", ret, NO_ERROR);
    REPORT_APPLICATION_MESSAGE((MESSAGE_ADDR_TYPE) nonul, 4, &ret);
    check("report non-NUL-terminated (expect 'WXYZ')", ret, NO_ERROR);
    REPORT_APPLICATION_MESSAGE((MESSAGE_ADDR_TYPE) text, 0, &ret);
    check("report LENGTH=0", ret, NO_ERROR);
    REPORT_APPLICATION_MESSAGE((MESSAGE_ADDR_TYPE) text, MAX_ERROR_MESSAGE_SIZE + 1, &ret);
    check("report LENGTH=MAX+1", ret, INVALID_PARAM);
    REPORT_APPLICATION_MESSAGE((MESSAGE_ADDR_TYPE) text, -1, &ret);
    check("report LENGTH=-1", ret, INVALID_PARAM);
    REPORT_APPLICATION_MESSAGE((MESSAGE_ADDR_TYPE) 0x10, 4, &ret);
    check("report bad address", ret, INVALID_PARAM);
}

// RAISE_APPLICATION_ERROR calls that must fail without raising an error.
static void test_raise_errors(void)
{
    RETURN_CODE_TYPE ret;

    RAISE_APPLICATION_ERROR(NUMERIC_ERROR, (MESSAGE_ADDR_TYPE) "x", 1, &ret);
    check("raise ERROR_CODE != APPLICATION_ERROR", ret, INVALID_PARAM);
    RAISE_APPLICATION_ERROR(APPLICATION_ERROR, (MESSAGE_ADDR_TYPE) "x", -1, &ret);
    check("raise LENGTH=-1", ret, INVALID_PARAM);
    RAISE_APPLICATION_ERROR(APPLICATION_ERROR, (MESSAGE_ADDR_TYPE) "x", MAX_ERROR_MESSAGE_SIZE + 1, &ret);
    check("raise LENGTH=MAX+1", ret, INVALID_PARAM);
    // Was NO_ERROR although the kernel refused it (EFAULT).
    RAISE_APPLICATION_ERROR(APPLICATION_ERROR, (MESSAGE_ADDR_TYPE) 0x10, 4, &ret);
    check("raise bad address", ret, INVALID_PARAM);
}

static RETURN_CODE_TYPE try_create(const char *name, PRIORITY_TYPE prio,
    SYSTEM_TIME_TYPE period, SYSTEM_TIME_TYPE capacity)
{
    RETURN_CODE_TYPE ret;
    PROCESS_ID_TYPE pid;
    PROCESS_ATTRIBUTE_TYPE attrs = {
        .PERIOD = period,
        .TIME_CAPACITY = capacity,
        .STACK_SIZE = 8096,
        .BASE_PRIORITY = prio,
        .DEADLINE = SOFT,
        .ENTRY_POINT = dummy,
    };
    strncpy(attrs.NAME, name, sizeof(PROCESS_NAME_TYPE));
    CREATE_PROCESS(&attrs, &pid, &ret);
    return ret;
}

static void test_create_process(void)
{
    PARTITION_STATUS_TYPE st;
    RETURN_CODE_TYPE ret;

    GET_PARTITION_STATUS(&st, &ret);
    printf("P1 partition period: %lld ms\n", (long long) (st.PERIOD / MS));
    check("partition period is 60 ms", st.PERIOD, 60 * MS);

    // Was NO_ERROR: 300 wrapped to 44 in the 8-bit kernel priority.
    check("create priority 300", try_create("p300", 300, INFINITE_TIME_VALUE, INFINITE_TIME_VALUE), INVALID_PARAM);
    check("create priority 0", try_create("p0", 0, INFINITE_TIME_VALUE, INFINITE_TIME_VALUE), INVALID_PARAM);
    // Was INVALID_PARAM.
    check("create PERIOD 0", try_create("per0", 10, 0, 10 * MS), INVALID_CONFIG);
    check("create PERIOD -5", try_create("perneg", 10, -5, 10 * MS), INVALID_CONFIG);
    // Any positive period is accepted (no partition-period multiple rule).
    check("create PERIOD 90 ms", try_create("per90", 10, 90 * MS, 10 * MS), NO_ERROR);
    check("create TIME_CAPACITY -5", try_create("tcneg", 10, INFINITE_TIME_VALUE, -5), INVALID_PARAM);
    check("create TIME_CAPACITY > PERIOD", try_create("tcbig", 10, 60 * MS, 61 * MS), INVALID_PARAM);
    check("create PERIOD 120 ms", try_create("per120", 10, 120 * MS, 10 * MS), NO_ERROR);
    check("create aperiodic", try_create("aper", 10, INFINITE_TIME_VALUE, INFINITE_TIME_VALUE), NO_ERROR);
}

// ARINC 653 names are case insensitive.
static void test_names(void)
{
    RETURN_CODE_TYPE ret;
    MEMORY_BLOCK_STATUS_TYPE mb;
    SAMPLING_PORT_ID_TYPE id = -1;

    GET_SAMPLING_PORT_ID("sP1", &id, &ret);
    check("GET_SAMPLING_PORT_ID('sP1') for SP1", ret, NO_ERROR);
    check("  same id", id, SP1);
    GET_MEMORY_BLOCK_STATUS("SHARED_mb0", &mb, &ret);
    check("GET_MEMORY_BLOCK_STATUS('SHARED_mb0') for 'Shared_MB0'", ret, NO_ERROR);
    check("  size", mb.MEMORY_BLOCK_SIZE, 0x10000);
    check("  mode READ_WRITE", mb.MEMORY_BLOCK_MODE, MB_READ_WRITE);
    GET_MEMORY_BLOCK_STATUS("no_such_block", &mb, &ret);
    check("GET_MEMORY_BLOCK_STATUS unknown name", ret, INVALID_CONFIG);
    // The kernel used to dereference this pointer without checking it.
    GET_MEMORY_BLOCK_STATUS((char *) 0x10, &mb, &ret);
    check("GET_MEMORY_BLOCK_STATUS bad name pointer", ret, INVALID_CONFIG);
}

static void test_sampling_create(void)
{
    RETURN_CODE_TYPE ret;
    SAMPLING_PORT_ID_TYPE id = 12345;

    CREATE_SAMPLING_PORT("SP1", SYSTEM_LIMIT_MESSAGE_SIZE + 1, SOURCE, SECOND, &id, &ret);
    check("create size > SYSTEM_LIMIT_MESSAGE_SIZE", ret, INVALID_CONFIG);
    check("  id untouched on error", id, 12345);
    CREATE_SAMPLING_PORT("NOPE", 64, SOURCE, SECOND, &id, &ret);
    check("create unknown name", ret, INVALID_CONFIG);
    check("  id untouched on error", id, 12345);
    CREATE_SAMPLING_PORT("SP1", 64, SOURCE, INFINITE_TIME_VALUE, &id, &ret);
    check("create refresh INFINITE", ret, INVALID_CONFIG);
    CREATE_SAMPLING_PORT("SP1", 64, SOURCE, 0, &id, &ret);
    check("create refresh 0", ret, INVALID_CONFIG);
    CREATE_SAMPLING_PORT("SP1", 64, DESTINATION, SECOND, &id, &ret);
    check("create wrong direction", ret, INVALID_CONFIG);
    CREATE_SAMPLING_PORT("SP1", 64, SOURCE, 100 * DAY, &SP1, &ret);
    check("create refresh 100 days (> 2^32 ms)", ret, NO_ERROR);
    CREATE_SAMPLING_PORT("SP1", 64, SOURCE, 100 * DAY, &id, &ret);
    check("create again", ret, NO_ACTION);
}

void main(void)
{
    RETURN_CODE_TYPE ret;
    PARTITION_STATUS_TYPE st;
    PROCESS_ID_TYPE pid;
    PROCESS_ATTRIBUTE_TYPE attrs = {
        .PERIOD = INFINITE_TIME_VALUE,
        .TIME_CAPACITY = INFINITE_TIME_VALUE,
        .STACK_SIZE = 8096,
        .BASE_PRIORITY = MIN_PRIORITY_VALUE,
        .DEADLINE = SOFT,
    };

    GET_PARTITION_STATUS(&st, &ret);
    printf("P1 start: START_CONDITION=%d OPERATING_MODE=%d NUM_ASSIGNED_CORES=%lu\n",
           (int) st.START_CONDITION, (int) st.OPERATING_MODE,
           (unsigned long) st.NUM_ASSIGNED_CORES);
    check("status return code", ret, NO_ERROR);
    check("NUM_ASSIGNED_CORES", st.NUM_ASSIGNED_CORES, 1);
    check("OPERATING_MODE is COLD_START", st.OPERATING_MODE, COLD_START);

    switch (st.START_CONDITION) {
    case NORMAL_START:
        test_types();
        test_report();
        test_raise_errors();
        test_create_process();
        printf("P1: SET_PARTITION_MODE(COLD_START)\n");
        SET_PARTITION_MODE(COLD_START, &ret);
        printf("TEST P1 SET_PARTITION_MODE returned: FAIL\n");
        break;
    case PARTITION_RESTART:
        printf("P1: RAISE_APPLICATION_ERROR with LENGTH 0 (HM action: COLD_START)\n");
        // Was NO_ERROR without raising anything: the kernel refused LENGTH 0.
        RAISE_APPLICATION_ERROR(APPLICATION_ERROR, (MESSAGE_ADDR_TYPE) "", 0, &ret);
        printf("TEST P1 RAISE_APPLICATION_ERROR LENGTH 0 returned (%d): FAIL\n", (int) ret);
        break;
    case HM_PARTITION_RESTART:
        test_sampling_create();
        printf("P1: memory violation (module HM action: RESET)\n");
        *(volatile uint32_t *) 0x10 = 1;
        printf("TEST P1 memory violation ignored: FAIL\n");
        break;
    case HM_MODULE_RESTART:
        check("after module RESET START_CONDITION", st.START_CONDITION, HM_MODULE_RESTART);
        CREATE_SAMPLING_PORT("SP1", 64, SOURCE, 100 * DAY, &SP1, &ret);
        check("create SP1 after module restart", ret, NO_ERROR);
        CREATE_SAMPLING_PORT("sp3", 64, SOURCE, SECOND, &SP3, &ret);
        check("create 'sp3' (configured as SP3)", ret, NO_ERROR);
        test_names();
        test_configure_error_handler();
        attrs.ENTRY_POINT = writer;
        strncpy(attrs.NAME, "writer", sizeof(PROCESS_NAME_TYPE));
        CREATE_PROCESS(&attrs, &pid, &ret);
        {
            PROCESS_ID_TYPE pid2 = -1;
            GET_PROCESS_ID("WRITER", &pid2, &ret);
            check("GET_PROCESS_ID('WRITER') for 'writer'", ret, NO_ERROR);
            check("  same id", pid2, pid);
            attrs.ENTRY_POINT = dummy;
            strncpy(attrs.NAME, "Writer", sizeof(PROCESS_NAME_TYPE));
            CREATE_PROCESS(&attrs, &pid2, &ret);
            check("CREATE_PROCESS('Writer') when 'writer' exists", ret, NO_ACTION);
        }
        test_multicore(pid);
        printf("P1 DONE: %d failure(s)\n", (int) fails);
        START(pid, &ret);
        SET_PARTITION_MODE(NORMAL, &ret);
        break;
    default:
        printf("TEST P1 unexpected START_CONDITION %d: FAIL\n", (int) st.START_CONDITION);
    }
    STOP_SELF();
}
