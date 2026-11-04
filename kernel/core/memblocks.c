/*
 * Modified by Kirill Brazhnikov, 2026-10-31: memory block names are
 * compared case-insensitively (ARINC 653 NAME_TYPE); the name and status
 * pointers passed by the partition are checked before they are used.
 *
 * Modified by Kirill Brazhnikov, 2026-10-31: built-in integer types (int,
 * unsigned, short, long, ...) are replaced with the fixed-width types
 * from <stdint.h>.
 *
 * Modified by Kirill Brazhnikov, 2026-11-02: MISRA C:2025: __user/__kuser
 * are renamed JET_USER/JET_KUSER; braces around all if/else/loop bodies;
 * essentially Boolean conditions; U suffixes for unsigned constants;
 * discarded results are cast to void; operands of the same essential
 * type.
 *
 * Modified by Kirill Brazhnikov, 2026-11-03: MISRA C:2025: deviations are
 * documented where a guideline can't be followed.
 * Long lines are wrapped at 80 columns.
 */
#include <core/memblocks.h>
#include <libc.h>
#include <types.h>
#include <core/partition.h>
#include <core/memblocks_config.h>
#include <core/uaccess.h>

/*
 * MISRA C:2025 Rule 11.5 deviation (this file): the jet_user_to_kernel*()
 * helpers return the kernel address of user memory as a pointer to void,
 * which is converted to the type of the object the user passed.
 */
pok_ret_t pok_memory_block_get_status(
        const char* JET_USER name,
        jet_memory_block_status_t* JET_USER status)
{
    size_t i;
    int32_t current_pid = current_partition->space_id;
    char kernel_name[MAX_NAME_LENGTH];

    const char* JET_KUSER k_name = jet_user_to_kernel_ro(name, MAX_NAME_LENGTH);
    if (k_name == NULL) {
        return POK_ERRNO_EFAULT;
    }

    jet_memory_block_status_t* JET_KUSER k_status =
        jet_user_to_kernel_typed(status);
    if (k_status == NULL) {
        return POK_ERRNO_EFAULT;
    }

    // Compare a private copy: the partition may change its buffer meanwhile.
    (void) memcpy(kernel_name, k_name, MAX_NAME_LENGTH);

    for (i = 0U; i < jet_memory_blocks_n; i++) {
        if (pok_compare_names(jet_memory_blocks[i].name, kernel_name) == 0) {
            break;
        }
    }
    if (i == jet_memory_blocks_n) {
        return POK_ERRNO_EINVAL;
    }

    if (jet_memory_blocks[i].pid_to_rights[current_pid]
        == MB_CONFIG_NO_ACCESS) {
        return POK_ERRNO_EPERM;
    }

    k_status->addr = jet_memory_blocks[i].virt_addr;
    k_status->size = jet_memory_blocks[i].size;
    if (jet_memory_blocks[i].pid_to_rights[current_pid]
        == MB_CONFIG_READ_WRITE) {
        k_status->mode = JET_MEMORY_BLOCK_READ_WRITE;
    } else {
        k_status->mode = JET_MEMORY_BLOCK_READ;
    }
    return POK_ERRNO_OK;
}

