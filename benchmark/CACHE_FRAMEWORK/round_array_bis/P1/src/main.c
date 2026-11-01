/*
 *                               POK header
 *
 * The following file is a part of the POK project. Any modification should
 * made according to the POK licence. You CANNOT use this file or a part of
 * this file is this part of a file for your own project
 *
 * For more information on the POK licence, please see our LICENCE FILE
 *
 * Please follow the coding guidelines described in doc/CODING_GUIDELINES
 *
 *                                      Copyright (c) 2007-2009 POK team
 *
 * Created by Jean-Baptiste on 22/03/2018
 *
 * Copyright (C) 2026 Kirill Brazhnikov <agamemnon886@gmail.com>
 * Modified by Kirill Brazhnikov, 2026-10-31: ported to x86_64 and AArch64:
 * the tracer markers are emitted only with CACHE_TRACE_MARKERS on x86_64,
 * process IDs use PROCESS_ID_TYPE and printf formats match their types.
 * The modifications are distributed under the POK licence
 * (BSD-2-Clause, see POK-LICENSE).
 */

/*******************************************************************************
 * ARINC-653 Multicore RTOS Benchmark
 *
 * Developped by:
 *    Alexy Torres Aurora Dugo
 *    Jean-Baptiste Lefoul
 *
 * Test: CPU stress tests
 * Activity:
 *    Generating matrices
 *    Multiplying matrices
 *
 * This tests aim to use the CPU and Memory at a balanced rate
 *
 ******************************************************************************/

#include <stdio.h>
#include <string.h>
#include <types.h>
#include <stdlib.h>

#include <arinc653/partition.h>
#include <arinc653/types.h>
#include <arinc653/time.h>

#include "access_array.h"

/*******************************************************************************
 * TESTS SETTINGS
 ******************************************************************************/

#define NB_SETS 128
#define NB_WAYS 5
#define LINE_SIZE 32

#define NB_LOOP 1

#define PROC_PERIOD     100000000ll
#define PROC_DEADLINE   PROC_PERIOD

/*
 * Start/stop markers around the measured accesses, for the external
 * memory tracer the benchmark was written for: the byte sequences
 * 0F A7 00 00 and 0F A6 00 00. They are not valid instructions on x86_64
 * processors or in QEMU (they raise an invalid-opcode fault) and have no
 * AArch64 equivalent, so they are emitted only on x86_64 and only when
 * CACHE_TRACE_MARKERS is defined (e.g. in the partition's cflags).
 */
#if defined(CACHE_TRACE_MARKERS) && defined(__x86_64__)
#define CACHE_TRACE_START() __asm__ __volatile__(".byte 0x0F, 0xA7, 0x00, 0x00\n")
#define CACHE_TRACE_STOP()  __asm__ __volatile__(".byte 0x0F, 0xA6, 0x00, 0x00\n")
#else
#define CACHE_TRACE_START() do { } while (0)
#define CACHE_TRACE_STOP()  do { } while (0)
#endif



/*******************************************************************************
 * PARTITION SPECIFIC VARIABLES
 ******************************************************************************/
static volatile uint32_t __attribute__ ((aligned (LINE_SIZE))) tabs[NB_SETS*NB_WAYS*LINE_SIZE/4] = {0};

static uint32_t partition_id;

/*******************************************************************************
 * TESTS FUNCTIONS
 ******************************************************************************/

void* process_func(void)
{
    RETURN_CODE_TYPE ret;
    while(1)
    {
        PROCESS_ID_TYPE my_id;
        GET_MY_ID(&my_id, &ret);
        if(ret != NO_ERROR)
        {
            printf("[Part %d] Cannot get ROUND_ARRAY_A653_PROC_ id[%d]\n", 
                    partition_id, ret);
            STOP_SELF();
        }
        CACHE_TRACE_START();
        for(register int32_t j = 0; j < NB_LOOP; ++j)
        {
            access_array(tabs);
        }
        CACHE_TRACE_STOP();
        printf("Finished loop\n");
        PERIODIC_WAIT(&ret);
    }
}

/*******************************************************************************
 * TESTS MAIN
 ******************************************************************************/
int main ()
{
    PARTITION_STATUS_TYPE part_stat;
    RETURN_CODE_TYPE       ret_type;   
    GET_PARTITION_STATUS (&part_stat, &ret_type);
    if(ret_type != NO_ERROR)
    {
        printf("Cannot get partition status\n");
        return 1;
    }
    partition_id = part_stat.IDENTIFIER;

    printf("[Part %d] Init ROUND_ARRAY_A653 partition\n", partition_id);
    

    PROCESS_ID_TYPE        thread;


    PROCESS_ATTRIBUTE_TYPE tattr = {
        .ENTRY_POINT   = process_func,
        .DEADLINE      = HARD,
        .PERIOD        = PROC_PERIOD,
        .STACK_SIZE    = 8096,
        .TIME_CAPACITY = PROC_DEADLINE,
        .BASE_PRIORITY = 1,
        .NAME = "ROUND_ARRAY_A653_PROC_0"
    };

    CREATE_PROCESS(&tattr, &thread, &ret_type);
    if(ret_type != NO_ERROR)
    {
        printf("[Part %d] Cannot create ROUND_ARRAY_A653_PROC_0 process[%d]\n", 
                partition_id, ret_type);
    }
    printf("[Part %d] %s thread number: %d\n", (int) partition_id,
           tattr.NAME, (int) thread);


    START(thread, &ret_type);
    if(ret_type != NO_ERROR)
    {
        printf("[Part %d] Cannot start ROUND_ARRAY_A653_PROC_0 process[%d]\n", 
                partition_id, ret_type);
    }

    /* Parition has been initialized, now switch to normal mode */
    printf("[Part %d] ROUND_ARRAY_A653 partition switching to normal mode\n",
            partition_id);
    SET_PARTITION_MODE (NORMAL, &ret_type);
    if(ret_type != NO_ERROR)
    {
        printf("[Part %d] Cannot switch ROUND_ARRAY_A653 partition to NORMAL state[%d]\n", 
                partition_id, ret_type);
        return -1;
    }

    STOP_SELF();

    return (0);
}
