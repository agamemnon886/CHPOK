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
 * Health monitoring example, partition P2 (reader).
 *
 * Demonstrates CREATE_SAMPLING_PORT for a destination port and message
 * validity: a message older than the refresh period given to
 * CREATE_SAMPLING_PORT (1 s) is read as INVALID. The reader reports stale
 * data to the health monitor with REPORT_APPLICATION_MESSAGE, once per
 * stale period, and keeps running.
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
#define REFRESH_PERIOD (1000 * MILLISECOND)
#define NB_READS 16                     /* one read every 500 ms */
#define READING_SIZE 32                 /* MaxMessageSize of SP_IN */

static SAMPLING_PORT_ID_TYPE sp_in;

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

static void reader(void)
{
    RETURN_CODE_TYPE ret;
    char reading[READING_SIZE];
    MESSAGE_SIZE_TYPE length;
    VALIDITY_TYPE validity;
    int32_t stale_reported = 0;

    for (int32_t i = 0; i < NB_READS; i++) {
        READ_SAMPLING_MESSAGE(sp_in, (MESSAGE_ADDR_TYPE) reading, &length,
                              &validity, &ret);
        if (ret == NO_ACTION) {
            printf("P2: no reading yet\n");
        } else if (ret != NO_ERROR) {
            printf("P2: READ_SAMPLING_MESSAGE failed: %s\n", return_code_name(ret));
        } else {
            reading[READING_SIZE - 1] = '\0';
            printf("P2: read \"%s\" (%ld bytes): %s\n", reading, (long) length,
                   validity == VALID ? "VALID" : "INVALID, older than the refresh period");

            if (validity == INVALID && !stale_reported) {
                static const char text[] = "stale reading on SP_IN";
                REPORT_APPLICATION_MESSAGE((MESSAGE_ADDR_TYPE) text,
                                           sizeof(text) - 1, &ret);
                stale_reported = 1;
            } else if (validity == VALID) {
                stale_reported = 0;
            }
        }

        TIMED_WAIT(500 * MILLISECOND, &ret);
    }

    printf("P2: done\n");
    STOP_SELF();
}

void main(void)
{
    RETURN_CODE_TYPE ret;
    PARTITION_STATUS_TYPE status;
    PROCESS_ID_TYPE pid;
    PROCESS_ATTRIBUTE_TYPE attributes = {
        .ENTRY_POINT = reader,
        .STACK_SIZE = 8096,
        .BASE_PRIORITY = MIN_PRIORITY_VALUE,
        .PERIOD = INFINITE_TIME_VALUE,
        .TIME_CAPACITY = INFINITE_TIME_VALUE,
        .DEADLINE = SOFT,
    };

    GET_PARTITION_STATUS(&status, &ret);
    if (ret == NO_ERROR)
        printf("P2: partition %d started, start condition %s\n",
               (int) status.IDENTIFIER,
               status.START_CONDITION == NORMAL_START ? "NORMAL_START" : "restart");

    /*
     * Destination port: messages older than REFRESH_PERIOD are INVALID.
     * The name, size and direction must match P2/config.xml.
     */
    CREATE_SAMPLING_PORT("SP_IN", READING_SIZE, DESTINATION, REFRESH_PERIOD, &sp_in, &ret);
    printf("P2: CREATE_SAMPLING_PORT(SP_IN, refresh %lld ms): %s\n",
           (long long) (REFRESH_PERIOD / MILLISECOND), return_code_name(ret));
    if (ret != NO_ERROR)
        STOP_SELF();

    strncpy(attributes.NAME, "reader", sizeof(PROCESS_NAME_TYPE));
    CREATE_PROCESS(&attributes, &pid, &ret);
    if (ret != NO_ERROR) {
        printf("P2: CREATE_PROCESS failed: %s\n", return_code_name(ret));
        STOP_SELF();
    }
    START(pid, &ret);

    SET_PARTITION_MODE(NORMAL, &ret);           /* does not return */
    printf("P2: SET_PARTITION_MODE(NORMAL) failed: %s\n", return_code_name(ret));
    STOP_SELF();
}
