/*
 * Institute for System Programming of the Russian Academy of Sciences
 * Copyright (C) 2016 ISPRAS
 * Copyright (C) 2026 Kirill Brazhnikov <agamemnon886@gmail.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation, Version 3.
 *
 * This program is distributed in the hope # that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * See the GNU General Public License version 3 for more details.
 *
 * This file also incorporates work covered by POK License.
 * Copyright (c) 2007-2009 POK team
 *
 * Modified by Kirill Brazhnikov, 2026-10-31: built-in integer types (int,
 * unsigned, short, long, ...) are replaced with the fixed-width types
 * from <stdint.h>.
 *
 * Modified by Kirill Brazhnikov, 2026-11-02: MISRA C:2025: the header
 * guard isn't a reserved identifier; __user/__kuser are renamed
 * JET_USER/JET_KUSER; U suffixes for unsigned constants; deviations are
 * documented where a guideline can't be followed.
 *
 * Modified by Kirill Brazhnikov, 2026-11-03: MISRA C:2025: deviations are
 * documented where a guideline can't be followed.
 *
 * Modified by Kirill Brazhnikov, 2026-11-08: Doxygen headers for the file
 * and its functions.
 *
 * Modified by Kirill Brazhnikov, 2026-11-11: the Doxygen comments that don't
 * document a function definition are plain comments, so that only the function
 * headers are in the function documentation.
 *
 * Modified by Kirill Brazhnikov, 2026-11-13: Doxygen comments for its macros.
 *
 * Modified by Kirill Brazhnikov, 2026-11-14: the Doxygen comment of the file is
 * removed (header files are not documented); the Doxygen headers of the static
 * inline functions are plain comments (they are not documented).
 */

#ifndef POK_COMMON_H
#define POK_COMMON_H
#include <stdint.h>

#include <compiler.h>

/**
 * @macroname `JET_USER`
 *
 * @description
 * Mark pointer as pointer to user space.
 *
 * Pointers of such types should be dereferenced only with special function (see
 * uaccess.h).
 *
 * Some checkers can set this mark to something, which *actually* prohibits
 * incorrect usage of marked pointers.
 *
 * Usage example:
 *
 *    void a(char* JET_USER buf);.
 */
#define JET_USER

/**
 * @macroname `JET_KUSER`
 *
 * @description
 * Kernel-space address of user memory area.
 *
 * The address can be accessed only while being in corresponded space.
 */
#define JET_KUSER

/*
 * Casts pointer to member of structure to pointer to structure itself.
 * 
 * @ptr: the pointer to the member
 * @type: the type of the container struct
 * @member: name of the member within the container.
 * 
 * Implementation is taken from Linux kernel.
 *
 * MISRA C:2025 Rule 11.3, Rule 11.4, Rule 18.4 and Dir 1.2 deviation: going
 * from a member back to the structure that contains it needs a conversion
 * between object pointer types and pointer arithmetic outside the member;
 * typeof(((type *)0)->member) converts 0 to a pointer only to name the
 * member's type (it isn't evaluated). This is
 * correct only if ptr points to the member of a 'type' object, which is how
 * the intrusive lists and the partition/thread structures are built. The
 * GNU extensions (statement expression, typeof) make the compiler check
 * that ptr has the member's type, which plain C can't express.
 */

/**
 * @macroname `container_of`
 *
 * @description
 * Casts pointer to member of structure to pointer to structure itself: the
 * structure of type type whose member member is at ptr. Implementation is taken
 * from Linux kernel.
 *
 * @params
 * | Name | Description |
 * |------|-------------|
 * | `ptr` | The pointer to the member. |
 * | `type` | The type of the container struct. |
 * | `member` | The name of the member within the container. |
 */
#define container_of(ptr, type, member) ({                   \
    const typeof( ((type *)0)->member)* container_of_mptr = (ptr); \
    (type *)( (char*) container_of_mptr - offsetof(type, member) ); })

/*
 * Rounds a value up to a multiple of an alignment.
 */
static inline uintptr_t ALIGN_VAL(uintptr_t val, uintptr_t align)
{
    return (val + align - 1U) & (~(align - 1U));
}

#endif /* !POK_COMMON_H */
