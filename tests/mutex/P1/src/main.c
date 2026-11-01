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
 * Mutex test, partition P1.
 *
 * The partition is started twice:
 *
 *   NORMAL_START       fills the mutex table (ARINC653_Mutexes Count=4)
 *                      and restarts with SET_PARTITION_MODE(COLD_START).
 *   PARTITION_RESTART  creates M1 (ceiling 20, PRIORITY queuing) and M2
 *                      (ceiling 25, FIFO), checks the services in
 *                      initialization mode, then process A drives the
 *                      NORMAL mode tests: priority ceiling, recursive
 *                      locking, hand-over to waiting processes in
 *                      priority order, SET_PRIORITY of an owner,
 *                      RESET_MUTEX of a stopped owner, a waiting process
 *                      that is stopped, the preemption lock and the error
 *                      handler.
 *
 * Expected return codes: for CREATE_MUTEX and ACQUIRE_MUTEX they follow
 * the error conditions of ARINC 653P1-5 3.7.2.5 as listed in the DLR
 * a653rs bindings. For the other services they are JetOS's choices, made
 * without the standard's text: INVALID_MODE for RELEASE_MUTEX by a process
 * that doesn't own the mutex and for RESET_MUTEX of a process that doesn't
 * own it, INVALID_PARAM for PREEMPTION_LOCK_MUTEX in GET_MUTEX_STATUS.
 *
 * The last line is "P1 DONE: <n> failure(s)" (see tests/README).
 */
#include <stdio.h>
#include <string.h>
#include <arinc653/types.h>
#include <arinc653/partition.h>
#include <arinc653/process.h>
#include <arinc653/time.h>
#include <arinc653/error.h>
#include <arinc653/mutex.h>

#define MS 1000000LL

static MUTEX_ID_TYPE M1, M2;
static PROCESS_ID_TYPE pidA, pidB, pidC, pidD, pidE, pidG, pidH;
static int32_t fails;
static char order[32];
static int32_t norder;

static void check(const char *name, int64_t got, int64_t want)
{
    if (got == want) {
        printf("TEST %s: PASS\n", name);
    } else {
        printf("TEST %s: FAIL (got %lld, want %lld)\n", name,
               (long long) got, (long long) want);
        fails++;
    }
}

static void mark(char c)
{
    order[norder++] = c;
    order[norder] = '\0';
}

static PRIORITY_TYPE cur_prio(PROCESS_ID_TYPE pid)
{
    PROCESS_STATUS_TYPE st;
    RETURN_CODE_TYPE ret;

    GET_PROCESS_STATUS(pid, &st, &ret);
    return st.CURRENT_PRIORITY;
}

static void get_status(MUTEX_ID_TYPE m, MUTEX_STATUS_TYPE *st)
{
    RETURN_CODE_TYPE ret;

    GET_MUTEX_STATUS(m, st, &ret);
    if (ret != NO_ERROR)
        check("GET_MUTEX_STATUS", ret, NO_ERROR);
}

static void check_available(const char *name, MUTEX_ID_TYPE m)
{
    MUTEX_STATUS_TYPE st;
    char buf[80];

    get_status(m, &st);
    snprintf(buf, sizeof(buf), "%s: state AVAILABLE", name);
    check(buf, st.MUTEX_STATE, AVAILABLE);
    snprintf(buf, sizeof(buf), "%s: owner NULL_PROCESS_ID", name);
    check(buf, st.MUTEX_OWNER, NULL_PROCESS_ID);
    snprintf(buf, sizeof(buf), "%s: lock count 0", name);
    check(buf, st.LOCK_COUNT, 0);
}

static void error_handler(void)
{
    ERROR_STATUS_TYPE es;
    RETURN_CODE_TYPE ret;

    for (;;) {
        GET_ERROR_STATUS(&es, &ret);
        if (ret != NO_ERROR)
            break;
        ACQUIRE_MUTEX(M1, 0, &ret);
        check("error handler: ACQUIRE_MUTEX", ret, INVALID_MODE);
    }
    STOP_SELF();
}

static void proc_b(void)
{
    RETURN_CODE_TYPE ret;
    MUTEX_ID_TYPE id = 0;

    ACQUIRE_MUTEX(M1, 0, &ret);
    check("B: ACQUIRE owned mutex, TIME_OUT 0", ret, NOT_AVAILABLE);
    ACQUIRE_MUTEX(M1, 10 * MS, &ret);
    check("B: ACQUIRE owned mutex, TIME_OUT 10 ms", ret, TIMED_OUT);
    ACQUIRE_MUTEX(M1, INFINITE_TIME_VALUE, &ret);
    check("B: ACQUIRE, waited for A", ret, NO_ERROR);
    mark('B');
    check("B: priority raised to ceiling 20", cur_prio(pidB), 20);
    GET_PROCESS_MUTEX_STATE(pidB, &id, &ret);
    check("B: GET_PROCESS_MUTEX_STATE(B) is M1", id, M1);
    RELEASE_MUTEX(M1, &ret);
    check("B: RELEASE", ret, NO_ERROR);
    mark('b');
    check("B: priority back to 15", cur_prio(pidB), 15);
    STOP_SELF();
}

static void proc_c(void)
{
    RETURN_CODE_TYPE ret;

    ACQUIRE_MUTEX(M1, INFINITE_TIME_VALUE, &ret);
    check("C: ACQUIRE, waited for B", ret, NO_ERROR);
    mark('C');
    check("C: priority raised to ceiling 20", cur_prio(pidC), 20);
    RELEASE_MUTEX(M1, &ret);
    check("C: RELEASE", ret, NO_ERROR);
    mark('c');
    STOP_SELF();
}

static void proc_h(void)
{
    RETURN_CODE_TYPE ret;
    MUTEX_ID_TYPE id = 0;

    ACQUIRE_MUTEX(M1, 0, &ret);
    check("H: ACQUIRE with priority 30 > ceiling 20", ret, INVALID_MODE);
    GET_PROCESS_MUTEX_STATE(pidA, &id, &ret);
    check("H: GET_PROCESS_MUTEX_STATE(A)", ret, NO_ERROR);
    check("H:   is M1", id, M1);
    STOP_SELF();
}

/* Stops while owning M2. */
static void proc_d(void)
{
    RETURN_CODE_TYPE ret;

    ACQUIRE_MUTEX(M2, INFINITE_TIME_VALUE, &ret);
    check("D: ACQUIRE M2", ret, NO_ERROR);
    STOP_SELF();
}

static void proc_e(void)
{
    RETURN_CODE_TYPE ret;
    MUTEX_STATUS_TYPE st;

    ACQUIRE_MUTEX(M2, INFINITE_TIME_VALUE, &ret);
    check("E: ACQUIRE M2, after RESET_MUTEX", ret, NO_ERROR);
    get_status(M2, &st);
    check("E: M2 owner is E", st.MUTEX_OWNER, pidE);
    check("E: M2 lock count 1", st.LOCK_COUNT, 1);
    check("E: priority raised to ceiling 25", cur_prio(pidE), 25);
    RELEASE_MUTEX(M2, &ret);
    check("E: RELEASE M2", ret, NO_ERROR);
    STOP_SELF();
}

/* Waits for M1 and is stopped while waiting. */
static void proc_g(void)
{
    RETURN_CODE_TYPE ret;

    ACQUIRE_MUTEX(M1, INFINITE_TIME_VALUE, &ret);
    printf("TEST G: ACQUIRE returned %d after STOP: FAIL\n", (int) ret);
    fails++;
    STOP_SELF();
}

static void proc_a(void)
{
    RETURN_CODE_TYPE ret;
    MUTEX_ID_TYPE id = 0;
    MUTEX_STATUS_TYPE st;
    LOCK_LEVEL_TYPE ll;
    int32_t i, ok;

    CREATE_MUTEX("M9", 20, FIFO, &id, &ret);
    check("CREATE_MUTEX in NORMAL mode", ret, INVALID_MODE);

    /* Ceiling, recursion, one mutex per process. */
    ACQUIRE_MUTEX(M1, INFINITE_TIME_VALUE, &ret);
    check("A: ACQUIRE M1", ret, NO_ERROR);
    check("A: priority raised to ceiling 20", cur_prio(pidA), 20);
    ACQUIRE_MUTEX(M1, 0, &ret);
    check("A: ACQUIRE M1 again", ret, NO_ERROR);
    get_status(M1, &st);
    check("A: M1 state OWNED", st.MUTEX_STATE, OWNED);
    check("A: M1 owner A", st.MUTEX_OWNER, pidA);
    check("A: M1 lock count 2", st.LOCK_COUNT, 2);
    check("A: M1 priority 20", st.MUTEX_PRIORITY, 20);
    ACQUIRE_MUTEX(M2, 0, &ret);
    check("A: ACQUIRE M2 while owning M1", ret, INVALID_MODE);

    /* Waiters: C queues first, but B has the higher priority. */
    START(pidC, &ret);
    START(pidB, &ret);
    TIMED_WAIT(100 * MS, &ret);
    get_status(M1, &st);
    check("A: M1 has 2 waiting processes", st.WAITING_PROCESSES, 2);

    START(pidH, &ret);          /* runs at once: 30 > 20 */

    SET_PRIORITY(pidA, 11, &ret);
    check("A: SET_PRIORITY(A, 11) while owning", ret, NO_ERROR);
    check("A:   current priority stays 20", cur_prio(pidA), 20);

    RELEASE_MUTEX(M1, &ret);
    check("A: RELEASE M1 (count 2 -> 1)", ret, NO_ERROR);
    get_status(M1, &st);
    check("A: M1 still owned by A", st.MUTEX_OWNER, pidA);
    RELEASE_MUTEX(M1, &ret);    /* B, then C, take M1 and finish */
    check("A: RELEASE M1 (count 1 -> 0)", ret, NO_ERROR);
    check("A: priority is the retained 11", cur_prio(pidA), 11);
    printf("order: %s\n", order);
    check("A: B got M1 before C (PRIORITY queuing)", strcmp(order, "BCbc"), 0);
    check_available("A: M1 after B and C", M1);
    RELEASE_MUTEX(M1, &ret);
    check("A: RELEASE M1 not owned", ret, INVALID_MODE);

    /* Lock count limit. */
    ok = 1;
    for (i = 0; i < MAX_LOCK_LEVEL; i++) {
        ACQUIRE_MUTEX(M1, 0, &ret);
        ok &= ret == NO_ERROR;
    }
    check("A: ACQUIRE M1 MAX_LOCK_LEVEL times", ok, 1);
    ACQUIRE_MUTEX(M1, 0, &ret);
    check("A: ACQUIRE M1 once more", ret, INVALID_CONFIG);
    ok = 1;
    for (i = 0; i < MAX_LOCK_LEVEL; i++) {
        RELEASE_MUTEX(M1, &ret);
        ok &= ret == NO_ERROR;
    }
    check("A: RELEASE M1 MAX_LOCK_LEVEL times", ok, 1);
    check_available("A: M1 after lock count test", M1);

    /* RESET_MUTEX of a stopped owner passes M2 to the waiter. */
    START(pidD, &ret);          /* takes M2 and stops */
    get_status(M2, &st);
    check("A: M2 owned by stopped D", st.MUTEX_OWNER, pidD);
    GET_PROCESS_MUTEX_STATE(pidD, &id, &ret);
    check("A: GET_PROCESS_MUTEX_STATE(D) is M2", id, M2);
    START(pidE, &ret);          /* waits for M2 */
    RESET_MUTEX(M2, pidA, &ret);
    check("A: RESET_MUTEX(M2, A), A isn't the owner", ret, INVALID_MODE);
    RESET_MUTEX(M2, 999, &ret);
    check("A: RESET_MUTEX(M2, 999)", ret, INVALID_PARAM);
    RESET_MUTEX(PREEMPTION_LOCK_MUTEX, pidD, &ret);
    check("A: RESET_MUTEX(PREEMPTION_LOCK_MUTEX, D)", ret, INVALID_PARAM);
    RESET_MUTEX(M2, pidD, &ret);    /* E takes M2 and finishes */
    check("A: RESET_MUTEX(M2, D)", ret, NO_ERROR);
    check_available("A: M2 after E", M2);

    /* A waiting process is stopped. */
    ACQUIRE_MUTEX(M1, INFINITE_TIME_VALUE, &ret);
    START(pidG, &ret);
    TIMED_WAIT(20 * MS, &ret);  /* G waits for M1 */
    get_status(M1, &st);
    check("A: M1 has 1 waiting process (G)", st.WAITING_PROCESSES, 1);
    STOP(pidG, &ret);
    check("A: STOP(G)", ret, NO_ERROR);
    get_status(M1, &st);
    check("A: M1 has no waiting process", st.WAITING_PROCESSES, 0);
    RELEASE_MUTEX(M1, &ret);
    check_available("A: M1 after stopped waiter", M1);

    /* Preemption lock. */
    GET_PROCESS_MUTEX_STATE(pidA, &id, &ret);
    check("A: GET_PROCESS_MUTEX_STATE(A), nothing owned", id, NO_MUTEX_OWNED);
    LOCK_PREEMPTION(&ll, &ret);
    GET_PROCESS_MUTEX_STATE(pidA, &id, &ret);
    check("A: GET_PROCESS_MUTEX_STATE(A), preemption locked", id, PREEMPTION_LOCK_MUTEX);
    UNLOCK_PREEMPTION(&ll, &ret);
    GET_PROCESS_MUTEX_STATE(pidA, &id, &ret);
    check("A: GET_PROCESS_MUTEX_STATE(A), unlocked", id, NO_MUTEX_OWNED);

    /* Parameter checks. */
    ACQUIRE_MUTEX(PREEMPTION_LOCK_MUTEX, 0, &ret);
    check("ACQUIRE_MUTEX(PREEMPTION_LOCK_MUTEX)", ret, INVALID_PARAM);
    ACQUIRE_MUTEX(99, 0, &ret);
    check("ACQUIRE_MUTEX(99)", ret, INVALID_PARAM);
    ACQUIRE_MUTEX(M1, -5, &ret);
    check("ACQUIRE_MUTEX TIME_OUT -5", ret, INVALID_PARAM);
    RELEASE_MUTEX(0, &ret);
    check("RELEASE_MUTEX(0)", ret, INVALID_PARAM);
    GET_MUTEX_STATUS(PREEMPTION_LOCK_MUTEX, &st, &ret);
    check("GET_MUTEX_STATUS(PREEMPTION_LOCK_MUTEX)", ret, INVALID_PARAM);
    GET_PROCESS_MUTEX_STATE(999, &id, &ret);
    check("GET_PROCESS_MUTEX_STATE(999)", ret, INVALID_PARAM);
    GET_PROCESS_MUTEX_STATE(pidA + 256, &id, &ret);
    check("GET_PROCESS_MUTEX_STATE(A + 256)", ret, INVALID_PARAM);
    GET_PROCESS_MUTEX_STATE(MAIN_PROCESS_ID, &id, &ret);
    check("GET_PROCESS_MUTEX_STATE(MAIN_PROCESS_ID)", ret, NO_ERROR);
    check("  NO_MUTEX_OWNED", id, NO_MUTEX_OWNED);
    SET_PRIORITY(pidA + 256, 10, &ret);
    check("SET_PRIORITY(A + 256)", ret, INVALID_PARAM);

    /* The error handler can't acquire mutexes. */
    RAISE_APPLICATION_ERROR(APPLICATION_ERROR, (MESSAGE_ADDR_TYPE) "x", 1, &ret);

    printf("P1 DONE: %d failure(s)\n", (int) fails);
    STOP_SELF();
}

static PROCESS_ID_TYPE create(const char *name, void (*entry)(void), PRIORITY_TYPE prio)
{
    RETURN_CODE_TYPE ret;
    PROCESS_ID_TYPE pid = 0;
    PROCESS_ATTRIBUTE_TYPE attrs = {
        .PERIOD = INFINITE_TIME_VALUE,
        .TIME_CAPACITY = INFINITE_TIME_VALUE,
        .STACK_SIZE = 8192,
        .BASE_PRIORITY = prio,
        .DEADLINE = SOFT,
        .ENTRY_POINT = entry,
    };

    strncpy(attrs.NAME, name, sizeof(PROCESS_NAME_TYPE));
    CREATE_PROCESS(&attrs, &pid, &ret);
    if (ret != NO_ERROR)
        check(name, ret, NO_ERROR);
    return pid;
}

void main(void)
{
    RETURN_CODE_TYPE ret;
    PARTITION_STATUS_TYPE pst;
    MUTEX_ID_TYPE id = 0;
    MUTEX_STATUS_TYPE st;

    GET_PARTITION_STATUS(&pst, &ret);

    if (pst.START_CONDITION == NORMAL_START) {
        int32_t i, ok = 1;
        char name[8] = "FILLn";

        for (i = 0; i < 4; i++) {
            name[4] = '0' + i;
            CREATE_MUTEX(name, 10, FIFO, &id, &ret);
            ok &= ret == NO_ERROR;
        }
        check("CREATE_MUTEX 4 (configured Count)", ok, 1);
        CREATE_MUTEX("FILL9", 10, FIFO, &id, &ret);
        check("CREATE_MUTEX 5th", ret, INVALID_CONFIG);
        SET_PARTITION_MODE(COLD_START, &ret);
        printf("TEST SET_PARTITION_MODE returned: FAIL\n");
    }

    CREATE_MUTEX("BAD", 0, FIFO, &id, &ret);
    check("CREATE_MUTEX priority 0", ret, INVALID_PARAM);
    CREATE_MUTEX("BAD", MAX_PRIORITY_VALUE + 1, FIFO, &id, &ret);
    check("CREATE_MUTEX priority MAX+1", ret, INVALID_PARAM);
    CREATE_MUTEX("BAD", 10, (QUEUING_DISCIPLINE_TYPE) 7, &id, &ret);
    check("CREATE_MUTEX discipline 7", ret, INVALID_PARAM);
    CREATE_MUTEX("M1", 20, PRIORITY, &M1, &ret);
    check("CREATE_MUTEX M1", ret, NO_ERROR);
    check("  M1 is ID 1 after the restart", M1, 1);
    CREATE_MUTEX("m1", 20, PRIORITY, &id, &ret);
    check("CREATE_MUTEX 'm1' again", ret, NO_ACTION);
    CREATE_MUTEX("M2", 25, FIFO, &M2, &ret);
    check("CREATE_MUTEX M2", ret, NO_ERROR);
    GET_MUTEX_ID("m2", &id, &ret);
    check("GET_MUTEX_ID('m2')", ret, NO_ERROR);
    check("  is M2", id, M2);
    GET_MUTEX_ID("nope", &id, &ret);
    check("GET_MUTEX_ID unknown", ret, INVALID_CONFIG);
    get_status(M1, &st);
    check("M1 priority 20", st.MUTEX_PRIORITY, 20);
    check("M1 waiting 0", st.WAITING_PROCESSES, 0);
    check_available("M1 initially", M1);

    /* The main process may use mutexes during initialization. */
    ACQUIRE_MUTEX(M2, 0, &ret);
    check("main: ACQUIRE M2", ret, NO_ERROR);
    GET_PROCESS_MUTEX_STATE(MAIN_PROCESS_ID, &id, &ret);
    check("main: GET_PROCESS_MUTEX_STATE(MAIN_PROCESS_ID) is M2", id, M2);
    get_status(M2, &st);
    check("main: M2 owner MAIN_PROCESS_ID", st.MUTEX_OWNER, MAIN_PROCESS_ID);
    RELEASE_MUTEX(M2, &ret);
    check("main: RELEASE M2", ret, NO_ERROR);
    RELEASE_MUTEX(M2, &ret);
    check("main: RELEASE M2 again", ret, INVALID_MODE);
    ACQUIRE_MUTEX(M2, 0, &ret);
    RESET_MUTEX(M2, MAIN_PROCESS_ID, &ret);
    check("main: RESET_MUTEX(M2, MAIN_PROCESS_ID)", ret, NO_ERROR);
    check_available("main: M2 after reset", M2);

    CREATE_ERROR_HANDLER(error_handler, 8192, &ret);
    pidA = create("A", proc_a, 10);
    pidB = create("B", proc_b, 15);
    pidC = create("C", proc_c, 12);
    pidD = create("D", proc_d, 13);
    pidE = create("E", proc_e, 14);
    pidG = create("G", proc_g, 13);
    pidH = create("H", proc_h, 30);

    START(pidA, &ret);
    SET_PARTITION_MODE(NORMAL, &ret);
    STOP_SELF();
}
