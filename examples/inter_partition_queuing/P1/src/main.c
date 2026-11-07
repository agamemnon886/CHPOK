/* Modified by Kirill Brazhnikov, 2026-10-31: built-in integer types (int,
 * unsigned, short, long, ...) are replaced with the fixed-width types
 * from <stdint.h>.
 *
 * Modified by Kirill Brazhnikov, 2026-11-07: the port name is passed as a
 * QUEUING_PORT_NAME_TYPE (the APEX services read all MAX_NAME_LENGTH
 * characters).
 */
#include <stdio.h>
#include <string.h>
#include <arinc653/buffer.h>
#include <arinc653/partition.h>
#include <arinc653/time.h>
#include <arinc653/queueing.h>
#include <arinc653/sampling.h>

SAMPLING_PORT_ID_TYPE QP1;
#define SECOND 1000000000LL

static void first_process(void)
{
    RETURN_CODE_TYPE ret;

    struct {
        uint32_t x;
        char message[32];
        uint32_t y;
    } __attribute__((packed)) msg;

    uint32_t last_x = 0;

    while (1) {
        MESSAGE_SIZE_TYPE len;
        RECEIVE_QUEUING_MESSAGE(QP1, INFINITE_TIME_VALUE, (MESSAGE_ADDR_TYPE) &msg, &len, &ret);

        if (ret != NO_ERROR) {
            printf("PR1: error: %u\n", ret);
        } else {
            printf("PR1: Received queueing {%u \"%s\" %u}\n", msg.x, msg.message, msg.y);

            if (msg.x < last_x) {
                printf("warning! received QP message out of order\n");
            } else if (msg.x > last_x + 1) {
                printf("warning: possible QP packet loss\n");
            } else if (msg.x == last_x && last_x != 0) {
                printf("warning: possible QP duplicate message\n");
            }
            last_x = msg.x;

        }
    }
}

static int32_t real_main(void)
{
    RETURN_CODE_TYPE ret;
    PROCESS_ID_TYPE pid;
    PROCESS_ATTRIBUTE_TYPE process_attrs = {
        .PERIOD = INFINITE_TIME_VALUE,
        .TIME_CAPACITY = INFINITE_TIME_VALUE,
        .STACK_SIZE = 8096, // the only accepted stack size!
        .BASE_PRIORITY = MIN_PRIORITY_VALUE,
        .DEADLINE = SOFT,
    };

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


    // create ports
    static const QUEUING_PORT_NAME_TYPE qp1_name = "QP1";
    CREATE_QUEUING_PORT(qp1_name, 64, 10, DESTINATION, FIFO, &QP1, &ret);


    // transition to NORMAL operating mode
    // N.B. if everything is OK, this never returns
    printf("going to NORMAL mode...\n");
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
