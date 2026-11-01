/*******************************************************************************
 * ARINC-653 Multicore RTOS Benchmark
 *
 * Developped by:
 *    Felipe Gohring de Magalhaes
 *    Alexy Torres Aurora Dugo
 *    Jean-Baptiste Lefoul
 *
 * Global statistics tools and instrumentation.
 *
 * Modified by Kirill Brazhnikov, 2026-10-31: size_t comes from <stddef.h>
 * (it was redefined as a 32-bit type); usqrt() and struct int_sqrt use
 * uint32_t, as the algorithm assumes 32-bit values.
 ******************************************************************************/

#ifndef __STANDALONE_LIBS_H_
#define __STANDALONE_LIBS_H_

#include <stddef.h>
#include <stdint.h>


#ifndef PI
#define PI 3.14159265359f
#endif
#ifndef PHI
#define PHI      ((1.0+sqrt(5.0))/2.0)         /* the golden number    */
#define INV_PHI  (1.0/PHI)                     /* the golden ratio     */
#endif

#ifndef RAND_MAX
#define RAND_MAX  32767
#endif

/* Width of the usqrt() argument */
#define BITSPERLONG 32

#define TOP2BITS(x) ((x & ((uint32_t)3 << (BITSPERLONG-2))) >> (BITSPERLONG-2))

#define deg2rad(d) ((d)*PI/180)
#define rad2deg(r) ((r)*180/PI)

#define MIN(v1, v2) (v1 < v2 ? v1 : v2)

struct int_sqrt {
      uint32_t sqrt,
               frac;
};

extern void* memcpy (void* to, const void*  from, size_t n);

extern void usqrt(uint32_t x, struct int_sqrt *q);
extern double sqrt(double val);
extern double sin(double rad);
extern double fabs(double n);
extern double cos(double x);
extern double pow(double n, double powval);
extern double atan(double x);
extern double acos(double x);

extern void srand(unsigned int seed);
extern unsigned int rand(void);

#endif /* __STANDALONE_LIBS_H_ */