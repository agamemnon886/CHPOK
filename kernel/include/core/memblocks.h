/*
 * Modified by Kirill Brazhnikov, 2026-10-31: the name argument is marked
 * as a user-space pointer.
 *
 * Modified by Kirill Brazhnikov, 2026-11-02: MISRA C:2025: the header
 * guard isn't a reserved identifier; __user/__kuser are renamed
 * JET_USER/JET_KUSER.
 *
 * Modified by Kirill Brazhnikov, 2026-11-13: the function declarations are
 * hidden from Doxygen (\cond): only the definitions of the functions are
 * documented.
 *
 * Modified by Kirill Brazhnikov, 2026-11-14: the enum types are named with _e:
 * pok_ret_e (was pok_ret_t).
 */
#ifndef JET_KERNEL_CORE_MEMBLOCKS_H
#define JET_KERNEL_CORE_MEMBLOCKS_H

#include <uapi/memblock_types.h>
#include <common.h>
#include <errno.h>

/** \cond */
pok_ret_e pok_memory_block_get_status(
        const char* JET_USER name,
        jet_memory_block_status_t* JET_USER status);
/** \endcond */
#endif
