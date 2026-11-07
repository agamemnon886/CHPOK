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
 *
 * Modified by Kirill Brazhnikov, 2026-11-07: the port name is passed as a
 * SAMPLING_PORT_NAME_TYPE (the APEX services read all MAX_NAME_LENGTH
 * characters).
 */

/*
 * Health monitoring example, partition P1 (writer).
 *
 * Demonstrates GET_PARTITION_STATUS, REPORT_APPLICATION_MESSAGE and
 * CREATE_SAMPLING_PORT for a source port. P1 is started three times, and
 * GET_PARTITION_STATUS tells it why it was started:
 *
 *   1. NORMAL_START: the module has just started. P1 restarts itself with
 *      SET_PARTITION_MODE(COLD_START).
 *   2. PARTITION_RESTART: restarted by itself. P1 raises an application
 *      error; it has no error handler process, so the health monitor
 *      takes the action from P1's HM table (config.xml): COLD_START.
 *   3. HM_PARTITION_RESTART: restarted by the health monitor. P1 creates
 *      sampling port SP_OUT and a writer process, which writes a few
 *      readings and then stops, so that the reader in P2 sees the last
 *      reading go stale.
 */

#include <stdio.h>
#include <string.h>
#include <arinc653/types.h>
#include <arinc653/partition.h>
#include <arinc653/process.h>
#include <arinc653/time.h>
#include <arinc653/error.h>
#include <arinc653/sampling.h>

#define MILLISECOND 1000000LL           /* SYSTEM_TIME_TYPE is in ns */
#define NB_READINGS 6                   /* readings the writer sends */
#define READING_SIZE 32                 /* MaxMessageSize of SP_OUT */

static SAMPLING_PORT_ID_TYPE sp_out;

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

static const char *start_condition_name(START_CONDITION_TYPE condition)
{
    switch (condition) {
    case NORMAL_START:         return "NORMAL_START";
    case PARTITION_RESTART:    return "PARTITION_RESTART";
    case HM_MODULE_RESTART:    return "HM_MODULE_RESTART";
    case HM_PARTITION_RESTART: return "HM_PARTITION_RESTART";
    }
    return "?";
}

static const char *operating_mode_name(OPERATING_MODE_TYPE mode)
{
    switch (mode) {
    case IDLE:       return "IDLE";
    case COLD_START: return "COLD_START";
    case WARM_START: return "WARM_START";
    case NORMAL:     return "NORMAL";
    }
    return "?";
}

/*
 * Sends a message to the health monitor. The kernel logs it on the
 * console together with the partition name; the process continues.
 */
static void report(const char *text)
{
    RETURN_CODE_TYPE ret;

    REPORT_APPLICATION_MESSAGE((MESSAGE_ADDR_TYPE) text,
                               (MESSAGE_SIZE_TYPE) strlen(text), &ret);
    if (ret != NO_ERROR)
        printf("P1: REPORT_APPLICATION_MESSAGE failed: %s\n", return_code_name(ret));
}

static void writer(void)
{
    RETURN_CODE_TYPE ret;
    char reading[READING_SIZE];

    for (int32_t i = 1; i <= NB_READINGS; i++) {
        snprintf(reading, sizeof(reading), "reading #%d", (int) i);
        WRITE_SAMPLING_MESSAGE(sp_out, (MESSAGE_ADDR_TYPE) reading,
                               (MESSAGE_SIZE_TYPE) strlen(reading) + 1, &ret);
        if (ret != NO_ERROR)
            printf("P1: WRITE_SAMPLING_MESSAGE failed: %s\n", return_code_name(ret));
        else
            printf("P1: wrote \"%s\"\n", reading);

        TIMED_WAIT(500 * MILLISECOND, &ret);
    }

    report("writer stopped, no new readings on SP_OUT");
    STOP_SELF();
}

static void create_port_and_writer(void)
{
    static const SAMPLING_PORT_NAME_TYPE sp_out_name = "SP_OUT";
    RETURN_CODE_TYPE ret;
    PROCESS_ID_TYPE pid;
    PROCESS_ATTRIBUTE_TYPE attributes = {
        .ENTRY_POINT = writer,
        .STACK_SIZE = 8096,
        .BASE_PRIORITY = MIN_PRIORITY_VALUE,
        .PERIOD = INFINITE_TIME_VALUE,
        .TIME_CAPACITY = INFINITE_TIME_VALUE,
        .DEADLINE = SOFT,
    };

    /*
     * The arguments must match the configuration (P1/config.xml). Here
     * MAX_MESSAGE_SIZE is wrong on purpose, so the port is not created.
     */
    CREATE_SAMPLING_PORT(sp_out_name, 64, SOURCE, 1000 * MILLISECOND, &sp_out,
                         &ret);
    printf("P1: CREATE_SAMPLING_PORT(SP_OUT, size 64): %s (configured size is %d)\n",
           return_code_name(ret), READING_SIZE);

    CREATE_SAMPLING_PORT(sp_out_name, READING_SIZE, SOURCE,
                         1000 * MILLISECOND, &sp_out, &ret);
    printf("P1: CREATE_SAMPLING_PORT(SP_OUT, size %d): %s\n",
           READING_SIZE, return_code_name(ret));
    if (ret != NO_ERROR)
        return;

    strncpy(attributes.NAME, "writer", sizeof(PROCESS_NAME_TYPE));
    CREATE_PROCESS(&attributes, &pid, &ret);
    if (ret != NO_ERROR) {
        printf("P1: CREATE_PROCESS failed: %s\n", return_code_name(ret));
        return;
    }
    START(pid, &ret);
    if (ret != NO_ERROR)
        printf("P1: START failed: %s\n", return_code_name(ret));
}

void main(void)
{
    RETURN_CODE_TYPE ret;
    PARTITION_STATUS_TYPE status;

    GET_PARTITION_STATUS(&status, &ret);
    if (ret != NO_ERROR) {
        printf("P1: GET_PARTITION_STATUS failed: %s\n", return_code_name(ret));
        STOP_SELF();
    }
    printf("P1: partition %d started: start condition %s, mode %s,\n"
           "    period %lld ms, duration %lld ms, lock level %d, %u core(s)\n",
           (int) status.IDENTIFIER,
           start_condition_name(status.START_CONDITION),
           operating_mode_name(status.OPERATING_MODE),
           (long long) (status.PERIOD / MILLISECOND),
           (long long) (status.DURATION / MILLISECOND),
           (int) status.LOCK_LEVEL,
           (unsigned) status.NUM_ASSIGNED_CORES);

    switch (status.START_CONDITION) {
    case NORMAL_START: {
        /*
         * A message need not be a NUL-terminated string: exactly LENGTH
         * bytes are sent. This one is a 4-byte status code.
         */
        static const char code[4] = { 'S', '0', '0', '1' };
        REPORT_APPLICATION_MESSAGE((MESSAGE_ADDR_TYPE) code, sizeof(code), &ret);

        report("first start, restarting the partition");
        SET_PARTITION_MODE(COLD_START, &ret);   /* does not return */
        printf("P1: SET_PARTITION_MODE(COLD_START) failed: %s\n", return_code_name(ret));
        break;
    }

    case PARTITION_RESTART:
        report("restarted by SET_PARTITION_MODE, raising an application error");
        RAISE_APPLICATION_ERROR(APPLICATION_ERROR,
                                (MESSAGE_ADDR_TYPE) "simulated failure", 17, &ret);
        printf("P1: RAISE_APPLICATION_ERROR returned: %s\n", return_code_name(ret));
        break;

    case HM_PARTITION_RESTART:
        report("restarted by the health monitor, starting the writer");
        create_port_and_writer();
        SET_PARTITION_MODE(NORMAL, &ret);       /* does not return */
        printf("P1: SET_PARTITION_MODE(NORMAL) failed: %s\n", return_code_name(ret));
        break;

    default:
        break;
    }

    STOP_SELF();
}
