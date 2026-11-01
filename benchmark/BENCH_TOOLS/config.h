/*******************************************************************************
 * ARINC-653 Multicore RTOS Benchmark
 *
 * Developped by:
 *    Alexy Torres Aurora Dugo
 *    Jean-Baptiste Lefoul
 *
 * Global tests configuration and settings. You may define architecture
 * specific values here
 *
 * Modified by Kirill Brazhnikov, 2026-10-31: the processor description
 * (cache and TLB geometry, ARCH_* selection) comes from arch_config.h and
 * depends on the target (x86_64 or aarch64) instead of a fixed x86 one.
 ******************************************************************************/

#ifndef __CONFIG_H_
#define __CONFIG_H_

/*******************************************************************************
 * PROCESSOR AND ARCHITECTURE
 ******************************************************************************/

#include "arch_config.h"

/*******************************************************************************
 * TOOLS
 ******************************************************************************/

#define MIN(v1, v2) (v1 < v2 ? v1 : v2)

/*******************************************************************************
 * INFORMATIONS
 ******************************************************************************/
/*
 *
 * Cache Index : I = (V mod (S / N)) / L
 *     I: Cache line index;
 *     V: Virtual address;
 *     S: Cache size;
 *     N: Number of cache sets;
 *     L: Line size;
 *
 * V + n*(S/N) leads to the same cache index
*/

#endif /* __CONFIG_H_ */
