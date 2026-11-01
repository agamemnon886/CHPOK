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
 * Error handler example, partition P1.
 *
 * Demonstrates CREATE_ERROR_HANDLER and CONFIGURE_ERROR_HANDLER, and what
 * the error handler process does with GET_ERROR_STATUS.
 *
 * During initialization the main process creates and configures the
 * error handler, then creates three processes and switches the partition
 * to NORMAL mode. The "demo" process then causes three process-level
 * errors, one after the other:
 *
 *   APPLICATION_ERROR  demo calls RAISE_APPLICATION_ERROR. The error
 *                      handler logs it and lets demo continue.
 *   MEMORY_VIOLATION   process "bad_pointer" writes to address 0x10. The
 *                      error handler stops it: resuming would execute the
 *                      faulting instruction again.
 *   DEADLINE_MISSED    process "overrun" runs longer than its time
 *                      capacity. The error handler stops it.
 *
 * The error handler has the highest priority: it preempts the failed
 * process (even with preemption locked) as soon as an error is reported,
 * and runs until it calls STOP_SELF.
 */

#include <stdio.h>
#include <string.h>
#include <arinc653/types.h>
#include <arinc653/partition.h>
#include <arinc653/process.h>
#include <arinc653/time.h>
#include <arinc653/error.h>

#define MILLISECOND  1000000LL          /* SYSTEM_TIME_TYPE is in ns */
#define STACK_BYTES  8192

static PROCESS_ID_TYPE pid_bad_pointer, pid_overrun;

static const char *return_code_name(RETURN_CODE_TYPE ret)
{
    switch (ret) {
    case NO_ERROR:       return "NO_ERROR";
    case NO_ACTION:      return "NO_ACTION";
    case NOT_AVAILABLE:  return "NOT_AVAILABLE";
    case INVALID_PARAM:  return "INVALID_PARAM";
    case INVALID_CONFIG: return "INVALID_CONFIG";
    case INVALID_MODE:   return "INVALID_MODE";
    case TIMED_OUT:      return "TIMED_OUT";
    }
    return "?";
}

static const char *error_code_name(ERROR_CODE_TYPE code)
{
    switch (code) {
    case DEADLINE_MISSED:   return "DEADLINE_MISSED";
    case APPLICATION_ERROR: return "APPLICATION_ERROR";
    case NUMERIC_ERROR:     return "NUMERIC_ERROR";
    case ILLEGAL_REQUEST:   return "ILLEGAL_REQUEST";
    case STACK_OVERFLOW:    return "STACK_OVERFLOW";
    case MEMORY_VIOLATION:  return "MEMORY_VIOLATION";
    case HARDWARE_FAULT:    return "HARDWARE_FAULT";
    case POWER_FAIL:        return "POWER_FAIL";
    }
    return "?";
}

static const char *concurrency_name(ERROR_HANDLER_CONCURRENCY_CONTROL_TYPE c)
{
    switch (c) {
    case PROCESSES_PAUSE:     return "PROCESSES_PAUSE";
    case PROCESSES_SCHEDULED: return "PROCESSES_SCHEDULED";
    }
    return "?";
}

static void configure(ERROR_HANDLER_CONCURRENCY_CONTROL_TYPE concurrency,
                      PROCESSOR_CORE_ID_TYPE core, const char *comment)
{
    RETURN_CODE_TYPE ret;

    CONFIGURE_ERROR_HANDLER(concurrency, core, &ret);
    printf("P1: CONFIGURE_ERROR_HANDLER(%s, core %d): %s%s\n",
           concurrency_name(concurrency), (int) core,
           return_code_name(ret), comment);
}

/*
 * The error handler process. The kernel starts it whenever a
 * process-level error is pending; it handles all pending errors and
 * stops. If another error comes in while it runs, it's started again.
 */
static void error_handler(void)
{
    ERROR_STATUS_TYPE status;
    PROCESS_STATUS_TYPE process;
    RETURN_CODE_TYPE ret;
    char message[MAX_ERROR_MESSAGE_SIZE + 1];

    for (;;) {
        GET_ERROR_STATUS(&status, &ret);
        if (ret != NO_ERROR)
            break;              /* NO_ACTION: no more pending errors */

        GET_PROCESS_STATUS(status.FAILED_PROCESS_ID, &process, &ret);
        if (ret != NO_ERROR)
            strcpy(process.ATTRIBUTES.NAME, "?");

        /* The message isn't NUL-terminated: it's LENGTH bytes long. */
        memcpy(message, status.MESSAGE, status.LENGTH);
        message[status.LENGTH] = '\0';

        printf("error handler: %s in process %d (%s): \"%s\"\n",
               error_code_name(status.ERROR_CODE),
               (int) status.FAILED_PROCESS_ID, process.ATTRIBUTES.NAME,
               message);

        switch (status.ERROR_CODE) {
        case APPLICATION_ERROR:
            /* The application reported it and can go on. */
            printf("error handler: recovery: none, the process continues\n");
            break;
        case MEMORY_VIOLATION:
            /* FAILED_ADDRESS is the address of the faulting instruction. */
            printf("error handler: failed instruction at 0x%lx\n",
                   (unsigned long) (uintptr_t) status.FAILED_ADDRESS);
            /* Fall through. */
        default:
            STOP(status.FAILED_PROCESS_ID, &ret);
            printf("error handler: recovery: STOP(%d): %s\n",
                   (int) status.FAILED_PROCESS_ID, return_code_name(ret));
            break;
        }
    }

    STOP_SELF();
}

static void demo(void)
{
    RETURN_CODE_TYPE ret;
    static const char app_message[] = "sensor value out of range";

    /* Both services are allowed only during initialization. */
    configure(PROCESSES_PAUSE, 0, " (partition is in NORMAL mode)");
    CREATE_ERROR_HANDLER(error_handler, STACK_BYTES, &ret);
    printf("demo: CREATE_ERROR_HANDLER: %s (partition is in NORMAL mode)\n",
           return_code_name(ret));

    printf("demo: RAISE_APPLICATION_ERROR(\"%s\")\n", app_message);
    RAISE_APPLICATION_ERROR(APPLICATION_ERROR, (MESSAGE_ADDR_TYPE) app_message,
                            sizeof(app_message) - 1, &ret);
    printf("demo: continues after the error handler, RAISE_APPLICATION_ERROR: %s\n",
           return_code_name(ret));

    /* Higher priority than demo: each one runs as soon as it's started. */
    printf("demo: START(bad_pointer)\n");
    START(pid_bad_pointer, &ret);

    printf("demo: START(overrun)\n");
    START(pid_overrun, &ret);

    printf("demo: done\n");
    STOP_SELF();
}

static void bad_pointer(void)
{
    printf("bad_pointer: writing to address 0x10\n");
    *(volatile uint32_t *) 0x10 = 1;
    printf("bad_pointer: not reached\n");
    STOP_SELF();
}

/* Busy for 200 ms, with a time capacity of 20 ms. */
static void overrun(void)
{
    RETURN_CODE_TYPE ret;
    SYSTEM_TIME_TYPE start, now;

    printf("overrun: running for 200 ms, time capacity is 20 ms\n");
    GET_TIME(&start, &ret);
    do {
        GET_TIME(&now, &ret);
    } while (now - start < 200 * MILLISECOND);
    printf("overrun: not reached\n");
    STOP_SELF();
}

static PROCESS_ID_TYPE create_process(const char *name, void (*entry)(void),
                                      PRIORITY_TYPE priority,
                                      SYSTEM_TIME_TYPE time_capacity)
{
    RETURN_CODE_TYPE ret;
    PROCESS_ID_TYPE pid = 0;
    PROCESS_ATTRIBUTE_TYPE attrs = {
        .PERIOD = INFINITE_TIME_VALUE,          /* aperiodic */
        .TIME_CAPACITY = time_capacity,
        .STACK_SIZE = STACK_BYTES,
        .BASE_PRIORITY = priority,
        .DEADLINE = SOFT,
        .ENTRY_POINT = entry,
    };

    strncpy(attrs.NAME, name, sizeof(PROCESS_NAME_TYPE));
    CREATE_PROCESS(&attrs, &pid, &ret);
    printf("P1: CREATE_PROCESS(%s): %s\n", name, return_code_name(ret));
    return pid;
}

void main(void)
{
    RETURN_CODE_TYPE ret;
    PARTITION_STATUS_TYPE status;
    PROCESS_ID_TYPE pid_demo;

    GET_PARTITION_STATUS(&status, &ret);
    printf("P1: started, %lu processor core(s)\n",
           (unsigned long) status.NUM_ASSIGNED_CORES);

    configure(PROCESSES_PAUSE, 0, " (no error handler yet)");

    CREATE_ERROR_HANDLER(error_handler, STACK_BYTES, &ret);
    printf("P1: CREATE_ERROR_HANDLER: %s\n", return_code_name(ret));
    CREATE_ERROR_HANDLER(error_handler, STACK_BYTES, &ret);
    printf("P1: CREATE_ERROR_HANDLER again: %s\n", return_code_name(ret));

    /*
     * The partition has one core, ID 0. On one core both concurrency
     * controls work the same: while the error handler runs, no other
     * process of the partition runs.
     */
    configure(PROCESSES_PAUSE, 1, " (the partition has only core 0)");
    configure(PROCESSES_PAUSE, 0, "");

    pid_demo = create_process("demo", demo, 10, INFINITE_TIME_VALUE);
    pid_bad_pointer = create_process("bad_pointer", bad_pointer, 20,
                                     INFINITE_TIME_VALUE);
    pid_overrun = create_process("overrun", overrun, 20, 20 * MILLISECOND);

    START(pid_demo, &ret);

    printf("P1: SET_PARTITION_MODE(NORMAL)\n");
    SET_PARTITION_MODE(NORMAL, &ret);
    printf("P1: SET_PARTITION_MODE returned: %s\n", return_code_name(ret));
    STOP_SELF();
}
