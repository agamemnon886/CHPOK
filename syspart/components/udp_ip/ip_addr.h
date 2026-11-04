/* Modified by Kirill Brazhnikov, 2026-11-04: MISRA C:2025: include guard;
 * the macro parameters and results are parenthesized and the constants are
 * unsigned.
 */
#ifndef UDP_IP_IP_ADDR_H
#define UDP_IP_IP_ADDR_H

#define IP_ADDR(a,b,c,d) (((uint32_t)((a) & 0xffU) << 24) | \
                          ((uint32_t)((b) & 0xffU) << 16) | \
                          ((uint32_t)((c) & 0xffU) << 8)  | \
                           (uint32_t)((d) & 0xffU))

/* The four bytes of an IPv4 address, as printf() arguments. */
#define IP_PRINT(X) (((X) >> 24) & 0xffU), (((X) >> 16) & 0xffU), \
                    (((X) >> 8) & 0xffU), ((X) & 0xffU)

#endif /* UDP_IP_IP_ADDR_H */
