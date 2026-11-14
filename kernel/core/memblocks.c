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
 *
 * Modified by Kirill Brazhnikov, 2026-11-08: Doxygen headers for the file
 * and its functions.
 *
 * Modified by Kirill Brazhnikov, 2026-11-14: the enum types are named with _e:
 * pok_ret_e (was pok_ret_t).
 */

/**
 * @file
 * Memory blocks: the status of a memory block that a partition asks for by
 * name.
 */
#include <core/memblocks.h>
#include <libc.h>
#include <types.h>
#include <core/partition.h>
#include <core/memblocks_config.h>
#include <core/uaccess.h>

/**
 * @funcname `pok_memory_block_get_status`
 *
 * @description
 * Gives a partition the status of a memory block by its name: its address, size
 * and the access the partition has to it (the memory block status syscall).
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `name` | `const char * JET_USER` | [in] Partition address of the block's name. |
 * | `status` | `jet_memory_block_status_t * JET_USER` | [out] Partition address of the result. |
 *
 * @return POK_ERRNO_OK; POK_ERRNO_EFAULT if name or status isn't a partition
 *         address; POK_ERRNO_EINVAL if there is no such block; POK_ERRNO_EPERM
 *         if the partition has no access to it.
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `current_partition` | `pok_partition_t *` | The partition that makes the call |
 * | `jet_memory_blocks` | `memory_block_t[]` | The memory blocks (generated configuration) |
 * | `jet_memory_blocks_n` | `size_t` | The number of memory blocks |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `current_pid` | `int32_t` | The space ID of the calling partition |
 * | `i` | `size_t` | The index of the block |
 * | `k_name` | `const char *` | Kernel address of name |
 * | `k_status` | `jet_memory_block_status_t *` | Kernel address of status |
 * | `kernel_name` | `char[30]` | A copy of the name |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Set `current_pid` to `current_partition->space_id`.
 * 2. Call `jet_user_to_kernel_ro()` with the arguments `name` and
 *    `MAX_NAME_LENGTH`, and store the result in `k_name`.
 * 3. If `k_name` is NULL:
 *    1. Return `POK_ERRNO_EFAULT`.
 * 4. Set `k_status` to `jet_user_to_kernel_typed(status)`.
 * 5. If `k_status` is NULL:
 *    1. Return `POK_ERRNO_EFAULT`.
 * 6. Call `memcpy()` with the arguments `kernel_name`, `k_name` and
 *    `MAX_NAME_LENGTH`.
 * 7. Set `i` to `0U`; then while `i` is less than `jet_memory_blocks_n`, repeat
 *    (after each pass: increment `i`):
 *    1. If `pok_compare_names(jet_memory_blocks[i].name, kernel_name)` is equal
 *       to `0`:
 *       1. Leave the loop.
 * 8. If `i` is equal to `jet_memory_blocks_n`:
 *    1. Return `POK_ERRNO_EINVAL`.
 * 9. If `jet_memory_blocks[i].pid_to_rights[current_pid]` is equal to
 *    `MB_CONFIG_NO_ACCESS`:
 *    1. Return `POK_ERRNO_EPERM`.
 * 10. Set `k_status->addr` to `jet_memory_blocks[i].virt_addr`.
 * 11. Set `k_status->size` to `jet_memory_blocks[i].size`.
 * 12. If `jet_memory_blocks[i].pid_to_rights[current_pid]` is equal to
 *     `MB_CONFIG_READ_WRITE`:
 *     1. Set `k_status->mode` to `JET_MEMORY_BLOCK_READ_WRITE`.
 * 13. Otherwise:
 *     1. Set `k_status->mode` to `JET_MEMORY_BLOCK_READ`.
 * 14. Return `POK_ERRNO_OK`.
 *
 * @calls
 * - `jet_user_to_kernel_ro()`
 * - `jet_user_to_kernel()`
 * - `memcpy()`
 * - `pok_compare_names()`
 */
/*
 * MISRA C:2025 Rule 11.5 deviation (this file): the jet_user_to_kernel*()
 * helpers return the kernel address of user memory as a pointer to void,
 * which is converted to the type of the object the user passed.
 */
pok_ret_e pok_memory_block_get_status(
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

