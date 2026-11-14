/* Modified by Kirill Brazhnikov, 2026-11-04: MISRA C:2025: include guard;
 * the macro parameters and results are parenthesized and the constants are
 * unsigned.
 *
 * Modified by Kirill Brazhnikov, 2026-11-14: the macros and types are
 * documented.
 */
#ifndef UDP_IP_IP_ADDR_H
#define UDP_IP_IP_ADDR_H

/**
 * @macroname `IP_ADDR`
 *
 * @description
 * The IPv4 address a.b.c.d as a 32-bit value (a the most significant byte).
 *
 * @params
 * | Name | Description |
 * |------|-------------|
 * | `a` | The first byte. |
 * | `b` | The second byte. |
 * | `c` | The third byte. |
 * | `d` | The fourth byte. |
 */
#define IP_ADDR(a,b,c,d) (((uint32_t)((a) & 0xffU) << 24) | \
                          ((uint32_t)((b) & 0xffU) << 16) | \
                          ((uint32_t)((c) & 0xffU) << 8)  | \
                           (uint32_t)((d) & 0xffU))

/**
 * @macroname `IP_PRINT`
 *
 * @description
 * The four bytes of an IPv4 address, as printf() arguments.
 *
 * @params
 * | Name | Description |
 * |------|-------------|
 * | `X` | The address (32 bits). |
 */
#define IP_PRINT(X) (((X) >> 24) & 0xffU), (((X) >> 16) & 0xffU), \
                    (((X) >> 8) & 0xffU), ((X) & 0xffU)

#endif /* UDP_IP_IP_ADDR_H */
