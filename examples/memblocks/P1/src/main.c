/* Modified by Kirill Brazhnikov, 2026-10-31: MEMORY_BLOCK_SIZE is printed
 * with a format matching its 32-bit type.
 *
 * Modified by Kirill Brazhnikov, 2026-10-31: built-in integer types (int,
 * unsigned, short, long, ...) are replaced with the fixed-width types
 * from <stdint.h>.
 *
 * Modified by Kirill Brazhnikov, 2026-11-07: the port and memory block
 * names are passed as QUEUING_PORT_NAME_TYPE and MEMORY_BLOCK_NAME_TYPE
 * (the APEX services read all MAX_NAME_LENGTH characters).
 */
#include <stdio.h>
#include <string.h>
#include <arinc653/queueing.h>
#include <arinc653/partition.h>
#include <arinc653/time.h>
#include <arinc653/memblocks.h>

QUEUING_PORT_ID_TYPE QP1;
void *mb0addr;

static void first_process(void)
{
    RETURN_CODE_TYPE ret;

    int32_t i = 0;
    int32_t status = 1; //non-zero means 'ready'
    while (1) {
        snprintf(mb0addr, 1000, "hello %d", i);

        SEND_QUEUING_MESSAGE(QP1, (MESSAGE_ADDR_TYPE) &status, sizeof(status),
                0, &ret);
        if (ret != NO_ERROR) {
                printf("P1 error: %d\n", (int) ret);
        }

        i++;
        TIMED_WAIT(1000000000LL, &ret);
    }
}


static int32_t real_main(void)
{
    static const QUEUING_PORT_NAME_TYPE qp1_name = "QP1";
    static const MEMORY_BLOCK_NAME_TYPE mb_name = "shared_mb0";
    RETURN_CODE_TYPE ret;
    PROCESS_ID_TYPE pid;
    PROCESS_ATTRIBUTE_TYPE process_attrs = {
        .PERIOD = INFINITE_TIME_VALUE,
        .TIME_CAPACITY = INFINITE_TIME_VALUE,
        .STACK_SIZE = 8096, // the only accepted stack size!
        .BASE_PRIORITY = MIN_PRIORITY_VALUE,
        .DEADLINE = SOFT,
    };

    // create ports
    CREATE_QUEUING_PORT(qp1_name, 64, 10, SOURCE, FIFO, &QP1, &ret);

    if (ret != NO_ERROR) {
        printf("error creating a port: %d\n", (int) ret);
        return 1;
    }

    // create process 1
    process_attrs.ENTRY_POINT = first_process;
    strncpy(process_attrs.NAME, "process 1", sizeof(PROCESS_NAME_TYPE));

    CREATE_PROCESS(&process_attrs, &pid, &ret);
    if (ret != NO_ERROR) {
        printf("couldn't create process 1: %d\n", (int) ret);
        return 1;
    } else {
        printf("process 1 created\n");
    }

    START(pid, &ret);
    if (ret != NO_ERROR) {
        printf("couldn't start process 1: %d\n", (int) ret);
        return 1;
    } else {
        printf("process 1 \"started\" (it won't actually run until operating mode becomes NORMAL)\n");
    }

    MEMORY_BLOCK_STATUS_TYPE status;
    GET_MEMORY_BLOCK_STATUS(mb_name, &status, &ret);
    if (ret != NO_ERROR) {
        printf("couldn't get Memory Block: %d\n", (int) ret);
        return 1;
    }
    printf("P1 memblock: %s %p [0x%lx]\n",
            status.MEMORY_BLOCK_MODE == MB_READ? "MB_READ": "MB_READ_WRITE",
            status.MEMORY_BLOCK_ADDR,
            (unsigned long) status.MEMORY_BLOCK_SIZE);
    mb0addr = status.MEMORY_BLOCK_ADDR;


    // transition to NORMAL operating mode
    // N.B. if everything is OK, this never returns
    SET_PARTITION_MODE(NORMAL, &ret);

    if (ret != NO_ERROR) {
        printf("couldn't transit to normal operating mode: %d\n", (int) ret);
    }

    STOP_SELF();
    return 0;
}

void main(void) {
    real_main();
    STOP_SELF();
}
