/*
 * Modified by Kirill Brazhnikov, 2026-10-31: the name argument is marked
 * as a user-space pointer.
 *
 * Modified by Kirill Brazhnikov, 2026-11-02: MISRA C:2025: the header
 * guard isn't a reserved identifier; __user/__kuser are renamed
 * JET_USER/JET_KUSER.
 */
#ifndef JET_KERNEL_CORE_MEMBLOCKS_H
#define JET_KERNEL_CORE_MEMBLOCKS_H

#include <uapi/memblock_types.h>
#include <common.h>
#include <errno.h>

pok_ret_t pok_memory_block_get_status(
        const char* JET_USER name,
        jet_memory_block_status_t* JET_USER status);
#endif
