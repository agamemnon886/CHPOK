/*******************************************************************************
 * ARINC-653 Multicore RTOS Benchmark
 *
 * Developped by:
 *    Alexy Torres Aurora Dugo
 *    Jean-Baptiste Lefoul
 *
 * Test: Cache Tests Bank: Private Intruction cache stress
 * Activity:
 *    Reading Instruction Caches
 *
 * This test aims to study the cache management by the OS. The purpose of the
 * test is to flood the instruction cache to study the potential interferences
 * and delay this behaviour induces.
 *
 * You can generate different size of code to fill the cache thanks to the
 * program contained in BENCH_TOOLS, gen_icache_flooder.
 *
 * Modified by Kirill Brazhnikov, 2026-10-31: runs on x86_64 and AArch64:
 * the architecture comes from BENCH_TOOLS/arch_config.h, and the flood code
 * is generated for each (icache_flood_x86_64.h, icache_flood_aarch64.h; the
 * 32-bit x86 one is removed).
 *
 * Modified by Kirill Brazhnikov, 2026-10-31: ported to 64-bit targets: the
 * process stack is 8192 bytes (1000 to 2500 bytes overflowed on 64-bit
 * targets, whose call frames are larger).
 *
 ******************************************************************************/

#include <stdio.h>
#include <string.h>
#include <types.h>
#include <stdlib.h>

#include <arinc653/partition.h>
#include <arinc653/types.h>
#include <arinc653/time.h>

#include "../../../../../BENCH_TOOLS/config.h"

/*******************************************************************************
 * TESTS SETTINGS
 ******************************************************************************/

/* Code of CACHE_SIZE_L1_INST bytes, see BENCH_TOOLS/gen_icache_flooder.c */
#if defined(__x86_64__)
    #include "icache_flood_x86_64.h"
#elif defined(__aarch64__)
    #include "icache_flood_aarch64.h"
#endif

/*******************************************************************************
 * TESTS FUNCTIONS
 ******************************************************************************/

void* flood_th(void)
{
    RETURN_CODE_TYPE      ret_type;
    PARTITION_STATUS_TYPE pr_stat;

    uint32_t k;
    k = 0;

    GET_PARTITION_STATUS(&pr_stat, &ret_type);
    if(ret_type != NO_ERROR)
    {
        printf("[TH0] Cannot get partition status [%d]\n", ret_type);
        return (void*)1;
    }

    while(1)
    {
#if ARCH_X86 || ARCH_PPC || ARCH_ARM
        asm_cache_flood();
#else
        printf("Error: architecture not defined or not supported\n");
#endif

        if(++k == 1000)
        {
            printf("L1 Cache filled 1000 times\n");
            k = 0;
        }
        PERIODIC_WAIT(&ret_type);
        if(ret_type != NO_ERROR)
        {
            printf("[TH0] Cannot achieve periodic wait [%d]\n", ret_type);
            return (void*)1;
        }
    }

    return (void*)0;
}

/*******************************************************************************
 * TESTS MAIN
 ******************************************************************************/
int main()
{
    PROCESS_ID_TYPE        thread_flood;
    PROCESS_ATTRIBUTE_TYPE th_attr_flood;

    RETURN_CODE_TYPE       ret_type;

    /* Set flood manipulation process */
    th_attr_flood.ENTRY_POINT   = flood_th;
    th_attr_flood.DEADLINE      = HARD;
    th_attr_flood.PERIOD        = 50000000;
    th_attr_flood.STACK_SIZE    = 8192;
    th_attr_flood.TIME_CAPACITY = 50000000;
    th_attr_flood.BASE_PRIORITY = 1;
    memcpy(th_attr_flood.NAME, "FLOOD_A653\0", 11 * sizeof(char));

    printf("Init P0 partition\n");

    /* Create processes */
    CREATE_PROCESS(&th_attr_flood, &thread_flood, &ret_type);
    if(ret_type != NO_ERROR)
    {
        printf("Cannot create FLOOD process [%d]\n", ret_type);
        return -1;
    }

    /* Start all processes */
    START(thread_flood, &ret_type);
    if(ret_type != NO_ERROR)
    {
        printf("Cannot start FLOOD process[%d]\n", ret_type);
        return -1;
    }

    /* Parition has been initialized, now switch to normal mode */
    printf("P0 partition switching to normal mode\n");
    SET_PARTITION_MODE(NORMAL, &ret_type);
    if(ret_type != NO_ERROR)
    {
        printf("Cannot switch P0 partition to NORMAL state[%d]\n", ret_type);
        return -1;
    }

    STOP_SELF();

    return 0;
}
