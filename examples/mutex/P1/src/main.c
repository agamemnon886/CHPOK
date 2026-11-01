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
 * Mutex example, partition P1.
 *
 * Four processes share a navigation data record, protected by the mutex
 * NAV_DATA (priority ceiling 20, PRIORITY queuing). Each process waits
 * until a fixed time (milliseconds after NORMAL mode starts), so the run
 * is the same every time:
 *
 *     t (ms)  process (priority)  what it does
 *       0     writer (12)         ACQUIRE_MUTEX, runs at priority 20,
 *                                 writes update #1, holds the mutex 100 ms
 *      20     display (14)        ACQUIRE_MUTEX (no time-out): waits
 *      40     monitor (10)        GET_MUTEX_STATUS: owned by writer, one
 *                                 waiting process; ACQUIRE_MUTEX with
 *                                 TIME_OUT 0 (NOT_AVAILABLE) and 20 ms
 *                                 (TIMED_OUT)
 *     100     writer              RELEASE_MUTEX: display gets the mutex
 *     150     monitor             ACQUIRE_MUTEX with TIME_OUT 0: free now
 *     300     faulty (13)         ACQUIRE_MUTEX, starts an update and
 *                                 holds the mutex
 *     320     display             ACQUIRE_MUTEX: waits
 *     350     faulty              fails (RAISE_APPLICATION_ERROR) while it
 *                                 owns the mutex. The error handler finds
 *                                 the mutex (GET_PROCESS_MUTEX_STATE),
 *                                 stops faulty, marks the data invalid and
 *                                 calls RESET_MUTEX: display gets it.
 *     500     writer              writes update #2.
 */

#include <stdio.h>
#include <string.h>
#include <arinc653/types.h>
#include <arinc653/partition.h>
#include <arinc653/process.h>
#include <arinc653/time.h>
#include <arinc653/error.h>
#include <arinc653/mutex.h>

#define MILLISECOND  1000000LL          /* SYSTEM_TIME_TYPE is in ns */
#define STACK_BYTES  8192
#define NAV_PRIORITY 20                 /* ceiling of NAV_DATA */

/* The shared data. Read and written only while owning NAV_DATA. */
static struct {
    int32_t  update;                    /* number of the last update */
    int32_t  altitude_ft;
    int32_t  valid;                     /* 0 while an update is in progress */
} nav;

static SYSTEM_TIME_TYPE t0 = -1;        /* start of NORMAL mode */

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

/* Milliseconds since NORMAL mode started, rounded to 10 ms. */
static int32_t now_ms(void)
{
    SYSTEM_TIME_TYPE now;
    RETURN_CODE_TYPE ret;

    GET_TIME(&now, &ret);
    if (t0 < 0)
        t0 = now;               /* the first process to ask sets it */
    return (int32_t) (((now - t0) / MILLISECOND + 5) / 10 * 10);
}

static void wait_until(int32_t ms)
{
    RETURN_CODE_TYPE ret;
    int32_t delay = ms - now_ms();

    if (delay > 0)
        TIMED_WAIT(delay * MILLISECOND, &ret);
}

static PRIORITY_TYPE current_priority(void)
{
    PROCESS_ID_TYPE me;
    PROCESS_STATUS_TYPE status;
    RETURN_CODE_TYPE ret;

    GET_MY_ID(&me, &ret);
    GET_PROCESS_STATUS(me, &status, &ret);
    return status.CURRENT_PRIORITY;
}

static const char *process_name(PROCESS_ID_TYPE pid)
{
    static PROCESS_STATUS_TYPE status;
    RETURN_CODE_TYPE ret;

    if (pid == NULL_PROCESS_ID)
        return "nobody";
    GET_PROCESS_STATUS(pid, &status, &ret);
    return ret == NO_ERROR ? status.ATTRIBUTES.NAME : "?";
}

/* Every process looks the mutex up by name; names are case insensitive. */
static MUTEX_ID_TYPE nav_mutex(void)
{
    MUTEX_ID_TYPE id = 0;
    RETURN_CODE_TYPE ret;

    GET_MUTEX_ID("nav_data", &id, &ret);
    return id;
}

static void print_status(const char *who, MUTEX_ID_TYPE id)
{
    MUTEX_STATUS_TYPE status;
    RETURN_CODE_TYPE ret;

    GET_MUTEX_STATUS(id, &status, &ret);
    printf("[%4d] %s: NAV_DATA is %s, owner %s, lock count %d, %d waiting\n",
           (int) now_ms(), who,
           status.MUTEX_STATE == OWNED ? "OWNED" : "AVAILABLE",
           process_name(status.MUTEX_OWNER), (int) status.LOCK_COUNT,
           (int) status.WAITING_PROCESSES);
}

static void writer(void)
{
    MUTEX_ID_TYPE id = nav_mutex();
    RETURN_CODE_TYPE ret;

    for (int32_t update = 1; update <= 2; update++) {
        wait_until(update == 1 ? 0 : 500);
        ACQUIRE_MUTEX(id, INFINITE_TIME_VALUE, &ret);
        printf("[%4d] writer: ACQUIRE_MUTEX: %s, priority %d (base 12)\n",
               (int) now_ms(), return_code_name(ret), (int) current_priority());

        nav.valid = 0;
        nav.update = update;
        nav.altitude_ft = 10000 + 500 * update;
        if (update == 1)
            wait_until(100);    /* a long update: others have to wait */
        nav.valid = 1;

        printf("[%4d] writer: wrote update #%d, RELEASE_MUTEX\n",
               (int) now_ms(), (int) update);
        RELEASE_MUTEX(id, &ret);
        printf("[%4d] writer: priority %d again\n",
               (int) now_ms(), (int) current_priority());
    }

    print_status("writer", id);
    printf("[%4d] writer: done\n", (int) now_ms());
    STOP_SELF();
}

static void display(void)
{
    MUTEX_ID_TYPE id = nav_mutex();
    RETURN_CODE_TYPE ret;

    for (int32_t round = 1; round <= 2; round++) {
        wait_until(round == 1 ? 20 : 320);
        printf("[%4d] display: ACQUIRE_MUTEX, waiting...\n", (int) now_ms());
        ACQUIRE_MUTEX(id, INFINITE_TIME_VALUE, &ret);
        printf("[%4d] display: ACQUIRE_MUTEX: %s, priority %d (base 14)\n",
               (int) now_ms(), return_code_name(ret), (int) current_priority());
        if (nav.valid)
            printf("[%4d] display: update #%d, altitude %d ft\n",
                   (int) now_ms(), (int) nav.update, (int) nav.altitude_ft);
        else
            printf("[%4d] display: data invalid (update #%d didn't finish)\n",
                   (int) now_ms(), (int) nav.update);
        RELEASE_MUTEX(id, &ret);
    }
    STOP_SELF();
}

static void monitor(void)
{
    MUTEX_ID_TYPE id = nav_mutex();
    RETURN_CODE_TYPE ret;

    wait_until(40);
    print_status("monitor", id);

    ACQUIRE_MUTEX(id, 0, &ret);
    printf("[%4d] monitor: ACQUIRE_MUTEX, TIME_OUT 0: %s\n",
           (int) now_ms(), return_code_name(ret));
    ACQUIRE_MUTEX(id, 20 * MILLISECOND, &ret);
    printf("[%4d] monitor: ACQUIRE_MUTEX, TIME_OUT 20 ms: %s\n",
           (int) now_ms(), return_code_name(ret));

    wait_until(150);
    ACQUIRE_MUTEX(id, 0, &ret);
    printf("[%4d] monitor: ACQUIRE_MUTEX, TIME_OUT 0: %s, update #%d\n",
           (int) now_ms(), return_code_name(ret), (int) nav.update);
    if (ret == NO_ERROR)
        RELEASE_MUTEX(id, &ret);
    STOP_SELF();
}

/* Fails in the middle of an update, while it owns the mutex. */
static void faulty(void)
{
    static const char message[] = "update aborted";
    MUTEX_ID_TYPE id = nav_mutex();
    RETURN_CODE_TYPE ret;

    wait_until(300);
    ACQUIRE_MUTEX(id, INFINITE_TIME_VALUE, &ret);
    printf("[%4d] faulty: ACQUIRE_MUTEX: %s, starting update #%d\n",
           (int) now_ms(), return_code_name(ret), (int) nav.update + 1);
    nav.valid = 0;
    nav.update++;
    wait_until(350);
    printf("[%4d] faulty: RAISE_APPLICATION_ERROR(\"%s\")\n",
           (int) now_ms(), message);
    RAISE_APPLICATION_ERROR(APPLICATION_ERROR, (MESSAGE_ADDR_TYPE) message,
                            sizeof(message) - 1, &ret);
    printf("faulty: not reached\n");
    STOP_SELF();
}

/*
 * The error handler recovers a process that fails while it owns a mutex:
 * it stops the process and releases the mutex with RESET_MUTEX, so the
 * processes waiting for it can go on.
 */
static void error_handler(void)
{
    ERROR_STATUS_TYPE status;
    MUTEX_ID_TYPE id;
    RETURN_CODE_TYPE ret;

    for (;;) {
        GET_ERROR_STATUS(&status, &ret);
        if (ret != NO_ERROR)
            break;

        status.MESSAGE[status.LENGTH < MAX_ERROR_MESSAGE_SIZE ? status.LENGTH : 0] = 0;
        printf("[%4d] error handler: error in %s: \"%s\"\n", (int) now_ms(),
               process_name(status.FAILED_PROCESS_ID), (char *) status.MESSAGE);

        GET_PROCESS_MUTEX_STATE(status.FAILED_PROCESS_ID, &id, &ret);
        STOP(status.FAILED_PROCESS_ID, &ret);
        printf("[%4d] error handler: STOP: %s\n", (int) now_ms(),
               return_code_name(ret));

        if (id > 0) {
            printf("[%4d] error handler: it owns NAV_DATA "
                   "(GET_PROCESS_MUTEX_STATE)\n", (int) now_ms());
            print_status("error handler", id);
            /* The update was interrupted: the data stays marked invalid. */
            RESET_MUTEX(id, status.FAILED_PROCESS_ID, &ret);
            printf("[%4d] error handler: RESET_MUTEX: %s\n", (int) now_ms(),
                   return_code_name(ret));
            print_status("error handler", id);
        }
    }
    STOP_SELF();
}

static PROCESS_ID_TYPE create_process(const char *name, void (*entry)(void),
                                      PRIORITY_TYPE priority)
{
    RETURN_CODE_TYPE ret;
    PROCESS_ID_TYPE pid = 0;
    PROCESS_ATTRIBUTE_TYPE attrs = {
        .PERIOD = INFINITE_TIME_VALUE,          /* aperiodic */
        .TIME_CAPACITY = INFINITE_TIME_VALUE,
        .STACK_SIZE = STACK_BYTES,
        .BASE_PRIORITY = priority,
        .DEADLINE = SOFT,
        .ENTRY_POINT = entry,
    };

    strncpy(attrs.NAME, name, sizeof(PROCESS_NAME_TYPE));
    CREATE_PROCESS(&attrs, &pid, &ret);
    if (ret != NO_ERROR)
        printf("P1: CREATE_PROCESS(%s): %s\n", name, return_code_name(ret));
    START(pid, &ret);
    return pid;
}

void main(void)
{
    MUTEX_ID_TYPE id;
    RETURN_CODE_TYPE ret;

    /* Mutexes are created during initialization; the number of them is
       ARINC653_Mutexes Count in P1/config.xml. */
    CREATE_MUTEX("NAV_DATA", NAV_PRIORITY, PRIORITY, &id, &ret);
    printf("P1: CREATE_MUTEX(NAV_DATA, priority %d, PRIORITY): %s\n",
           NAV_PRIORITY, return_code_name(ret));
    CREATE_MUTEX("SECOND", NAV_PRIORITY, FIFO, &id, &ret);
    printf("P1: CREATE_MUTEX(SECOND): %s (Count is 1)\n", return_code_name(ret));

    CREATE_ERROR_HANDLER(error_handler, STACK_BYTES, &ret);

    create_process("writer", writer, 12);
    create_process("display", display, 14);
    create_process("monitor", monitor, 10);
    create_process("faulty", faulty, 13);

    printf("P1: SET_PARTITION_MODE(NORMAL)\n");
    SET_PARTITION_MODE(NORMAL, &ret);
    STOP_SELF();
}
