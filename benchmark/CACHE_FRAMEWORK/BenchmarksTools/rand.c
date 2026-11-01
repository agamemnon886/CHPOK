/* Modified by Kirill Brazhnikov, 2026-10-31: the generator state is
 * uint32_t (it was unsigned long, 64 bits on 64-bit targets). */
#include "standalone_libs.h"

/* 32-bit state: the same sequence on 32- and 64-bit targets. */
static uint32_t next = 1;

unsigned int rand(void)
{
    next = next * 1103515245 + 12345;
    return (unsigned int)(next) % RAND_MAX;
}

void srand(unsigned int seed)
{
    next = seed;
}