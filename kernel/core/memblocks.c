/*
 * Modified by Kirill Brazhnikov, 2026-10-31: memory block names are
 * compared case-insensitively (ARINC 653 NAME_TYPE); the name and status
 * pointers passed by the partition are checked before they are used.
 *
 * Modified by Kirill Brazhnikov, 2026-10-31: built-in integer types (int,
 * unsigned, short, long, ...) are replaced with the fixed-width types
 * from <stdint.h>.
 */
#include <core/memblocks.h>
#include <libc.h>
#include <types.h>
#include <core/partition.h>
#include <core/memblocks_config.h>
#include <core/uaccess.h>
pok_ret_t pok_memory_block_get_status(
        const char* __user name,
        jet_memory_block_status_t* __user status)
{
    int32_t i;
    int32_t current_pid = current_partition->space_id;
    char kernel_name[MAX_NAME_LENGTH];

    const char* __kuser k_name = jet_user_to_kernel_ro(name, MAX_NAME_LENGTH);
    if (!k_name) return POK_ERRNO_EFAULT;

    jet_memory_block_status_t* __kuser k_status = jet_user_to_kernel_typed(status);
    if (!k_status) return POK_ERRNO_EFAULT;

    // Compare a private copy: the partition may change its buffer meanwhile.
    memcpy(kernel_name, k_name, MAX_NAME_LENGTH);

    for (i = 0; i < jet_memory_blocks_n; i++) {
        if (pok_compare_names(jet_memory_blocks[i].name, kernel_name) == 0)
            break;
    }
    if (i == jet_memory_blocks_n)
        return POK_ERRNO_EINVAL;

    if (jet_memory_blocks[i].pid_to_rights[current_pid] == 0)
        return POK_ERRNO_EPERM;

    k_status->addr = jet_memory_blocks[i].virt_addr;
    k_status->size = jet_memory_blocks[i].size;
    if (jet_memory_blocks[i].pid_to_rights[current_pid] == MB_CONFIG_READ_WRITE)
        k_status->mode = JET_MEMORY_BLOCK_READ_WRITE;
    else
        k_status->mode = JET_MEMORY_BLOCK_READ;
    return POK_ERRNO_OK;
}

