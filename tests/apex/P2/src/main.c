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
 * APEX test, partition P2.
 *
 * Checks the start conditions (NORMAL_START, then HM_MODULE_RESTART after
 * P1 triggers a module RESET) and the validity of sampling messages with a
 * huge (INT64_MAX ns) and a tiny (1 ms) refresh period. The last line is
 * "P2 DONE: <n> failure(s)" (see tests/README).
 */
#include <stdio.h>
#include <string.h>
#include <arinc653/partition.h>
#include <arinc653/process.h>
#include <arinc653/time.h>
#include <arinc653/sampling.h>

#define SECOND 1000000000LL

static SAMPLING_PORT_ID_TYPE SP2, SP4;
static int32_t fails;

static void check(const char *name, int64_t got, int64_t want)
{
    if (got == want) {
        printf("TEST P2 %s: PASS\n", name);
    } else {
        printf("TEST P2 %s: FAIL (got %lld, want %lld)\n", name,
               (long long) got, (long long) want);
        fails++;
    }
}

static void reader(void)
{
    RETURN_CODE_TYPE ret;
    char msg[64];
    MESSAGE_SIZE_TYPE len;
    VALIDITY_TYPE validity;
    int32_t got_sp2 = 0, got_sp4 = 0, i;

    for (i = 0; i < 20 && !(got_sp2 && got_sp4); i++) {
        TIMED_WAIT(SECOND / 2, &ret);
        if (!got_sp2) {
            READ_SAMPLING_MESSAGE(SP2, (MESSAGE_ADDR_TYPE) msg, &len, &validity, &ret);
            if (ret == NO_ERROR) {
                got_sp2 = 1;
                // Before the fix ts + refresh overflowed: always INVALID.
                check("SP2 (refresh INT64_MAX) message VALID", validity, VALID);
                check("SP2 message length", len, 8);
            }
        }
        if (!got_sp4) {
            READ_SAMPLING_MESSAGE(SP4, (MESSAGE_ADDR_TYPE) msg, &len, &validity, &ret);
            if (ret == NO_ERROR) {
                got_sp4 = 1;
                // Written once; read at least 1 ms later: must be stale.
                TIMED_WAIT(SECOND / 10, &ret);
                READ_SAMPLING_MESSAGE(SP4, (MESSAGE_ADDR_TYPE) msg, &len, &validity, &ret);
                check("SP4 (refresh 1 ms) old message INVALID", validity, INVALID);
            }
        }
    }
    check("received SP2", got_sp2, 1);
    check("received SP4", got_sp4, 1);
    printf("P2 DONE: %d failure(s)\n", (int) fails);
    STOP_SELF();
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
    printf("P2 start: START_CONDITION=%d\n", (int) st.START_CONDITION);
    check("NUM_ASSIGNED_CORES", st.NUM_ASSIGNED_CORES, 1);

    if (st.START_CONDITION == NORMAL_START) {
        // Wait for the module RESET triggered by P1.
        SET_PARTITION_MODE(NORMAL, &ret);
        STOP_SELF();
    }

    check("after module RESET START_CONDITION", st.START_CONDITION, HM_MODULE_RESTART);

    // Largest finite refresh period: INFINITE_TIME_VALUE is -1.
    CREATE_SAMPLING_PORT("SP2", 64, DESTINATION, 0x7fffffffffffffffLL, &SP2, &ret);
    check("create SP2 refresh INT64_MAX", ret, NO_ERROR);
    CREATE_SAMPLING_PORT("SP4", 64, DESTINATION, 1000000LL, &SP4, &ret);
    check("create SP4 refresh 1 ms", ret, NO_ERROR);

    attrs.ENTRY_POINT = reader;
    strncpy(attrs.NAME, "reader", sizeof(PROCESS_NAME_TYPE));
    CREATE_PROCESS(&attrs, &pid, &ret);
    START(pid, &ret);
    SET_PARTITION_MODE(NORMAL, &ret);
    STOP_SELF();
}
