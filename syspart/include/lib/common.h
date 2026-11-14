/* Modified by Kirill Brazhnikov, 2026-11-04: MISRA C:2025: the header
 * guard isn't a reserved identifier.
 *
 * Modified by Kirill Brazhnikov, 2026-11-14: self_t is documented; port_ops_t
 * is the type of the port operations of the generated glue code.
 */
#ifndef COMMON_H
#define COMMON_H

#include <stdio.h>
#include <types.h>


/**
 * @typename `self_t`
 *
 * @description
 * A component, as the operations of an interface get it (each component
 * converts it to its own type).
 */
typedef void self_t;

/**
 * @typename `port_ops_t`
 *
 * @description
 * A port of an array of output ports of a component, in the glue code
 * (misc/templates/glue.c.tpl): the operations of the interface and the
 * component that implements them.
 */
typedef struct {
    void *ops; /**< The operations of the interface. */
    void *owner; /**< The component that implements them. */
} port_ops_t;

#endif
