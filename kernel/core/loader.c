/*
 *                               POK header
 * 
 * The following file is a part of the POK project. Any modification should
 * made according to the POK licence. You CANNOT use this file or a part of
 * this file is this part of a file for your own project
 *
 * For more information on the POK licence, please see our LICENCE FILE
 *
 * Please follow the coding guidelines described in doc/CODING_GUIDELINES
 *
 *                                      Copyright (c) 2007-2009 POK team 
 *
 * Created by julien on Thu Jan 15 23:34:13 2009 
 *
 * Copyright (C) 2026 Kirill Brazhnikov <agamemnon886@gmail.com>
 * Modified by Kirill Brazhnikov, 2026-10-31: loads 64-bit ELF images;
 * skips segments which are not PT_LOAD.
 * Modified by Kirill Brazhnikov, 2026-10-31: built-in integer types (int,
 * unsigned, short, long, ...) are replaced with the fixed-width types
 * from <stdint.h>.
 * Modified by Kirill Brazhnikov, 2026-11-02: MISRA C:2025: braces around
 * all if/else/loop bodies; U suffixes for unsigned constants; discarded
 * results are cast to void; operands of the same essential type;
 * deviations are documented where a guideline can't be followed.
 * Modified by Kirill Brazhnikov, 2026-11-03: MISRA C:2025: the ELF
 * headers are read through pointers to const; the printf() formats match
 * the size_t arguments; one declarator per declaration; operands of mixed
 * precedence are parenthesized; deviations are documented where a
 * guideline can't be followed.
 * Modified by Kirill Brazhnikov, 2026-11-08: Doxygen headers for the file
 * and its functions.
 * Modified by Kirill Brazhnikov, 2026-11-13: jet_space_layout_t is used by its
 * typedef name; the declarations of the partition images are moved to
 * kernel/include/core/loader.h.
 * Modified by Kirill Brazhnikov, 2026-11-14: the string literals of the Design
 * steps are wrapped at their spaces, each in one code span (they were cut
 * inside names).
 * The modifications are distributed under the POK licence
 * (BSD-2-Clause, see POK-LICENSE). Long lines are wrapped at 80 columns.
 */

/**
 * @file
 * Contains all needed stuff to load partitions (elf files).
 * This needs the partitioning service (POK_NEEDS_PARTITIONS must
 * be defined) to work. (Written by Julian Pidancet and Julien Delange
 * for POK, 2008-2009.)
 */

#include <config.h>

#ifdef POK_NEEDS_PARTITIONS

#include <errno.h>
#include <types.h>
#include <libc.h>
#include <core/cpio.h>
#include <core/error.h>
#include <core/partition.h>
#include <core/debug.h>
#include <elf.h>

#include <core/space.h>

#include <core/loader.h>

/*
 * MISRA C:2025 Rule 18.4 deviation (this file): the partition ELF images are
 * byte arrays: their headers, segments and their copies in partition memory
 * are found by adding offsets and sizes to the start of the image and of the
 * memory, and the segments are checked against the bounds of that memory.
 */


/**
 * @funcname `jet_loader_elf_load`
 *
 * @description
 * Loads a partition's ELF image from the kernel's archive into the partition's
 * memory: checks the image and its PT_LOAD segments against the partition's
 * space, copies them and clears their BSS, and gives the entry point. A bad
 * image raises a configuration or partition load error.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `elf_id` | `uint8_t` | [in] The index of the image in the archive. |
 * | `space_id` | `jet_space_id` | [in] The partition's memory space. |
 * | `entry` | `void (**)(void)` | [out] Where the entry point is stored. |
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `__archive2_begin` | `char` | The start of the archive of images (linker) |
 * | `pok_elf_sizes` | `size_t[]` | The size of each image (generated) |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `elf_end_user` | `char *` | The highest user address the segments reach |
 * | `elf_header` | `const Elf_Ehdr *` | The ELF header |
 * | `elf_offset` | `size_t` | The offset of the image in the archive |
 * | `elf_phdr` | `const Elf_Phdr *` | The program headers |
 * | `elf_section_end_user` | `char *` | The user address of the end of a segment |
 * | `elf_size` | `size_t` | The size of the image |
 * | `elf_span` | `ptrdiff_t` | How far the segments reach above the space's start |
 * | `elf_start` | `const char *` | The start of the image |
 * | `filesz` | `size_t` | The size of a segment in the file |
 * | `i` | `uint32_t` | The index of an image, then of a program header |
 * | `kernel_dest` | `char *` | The kernel address of a segment |
 * | `memsz` | `size_t` | The size of a segment in memory |
 * | `space_layout` | `jet_space_layout_t` | The space's user and kernel addresses and its size |
 * | `user_dest` | `char *` | The user address of a segment |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Call `ja_space_layout_get()` with the arguments `space_id` and the address
 *    of `space_layout`.
 * 2. Set `elf_offset` to `0`.
 * 3. Set `i` to `0`; then while `i` is less than `elf_id`, repeat (after each
 *    pass: increment `i`):
 *    1. Add `pok_elf_sizes[i]` to `elf_offset`.
 * 4. Set `elf_size` to `pok_elf_sizes[elf_id]`.
 * 5. If `elf_size` is greater than `space_layout.size`:
 *    1. Call `printf()` with the arguments
 *       `"Attempt to load elf %%u of size %%zx into space of size %%zx.\n"`,
 *       `elf_id`, `elf_size` and `space_layout.size`.
 *    2. Call `pok_raise_error()` with the arguments
 *       `POK_ERROR_ID_CONFIG_ERROR`, `FALSE` and `NULL`.
 * 6. Set `elf_start` to `&__archive2_begin + elf_offset`.
 * 7. Set `elf_header` to `(const Elf_Ehdr*)elf_start`.
 * 8. If `elf_header->e_ident[0]` is not equal to `0x7fU` or
 *    `elf_header->e_ident[1]` is not equal to `0x45U` or
 *    `elf_header->e_ident[2]` is not equal to `0x4cU` or
 *    `elf_header->e_ident[3]` is not equal to `0x46U`:
 *    1. Call `printf()` with the argument
 *       `"Partition's ELF has incorrect format"`.
 *    2. Call `pok_raise_error()` with the arguments
 *       `POK_ERROR_ID_PARTLOAD_ERROR`, `FALSE` and `NULL`.
 * 9. Set `*entry` to `(void (*)(void)) elf_header->e_entry`.
 * 10. Set `elf_phdr` to `(const Elf_Phdr*)(elf_start + elf_header->e_phoff)`.
 * 11. Set `elf_end_user` to `space_layout.user_addr`.
 * 12. Set `i` to `0U`; then while `i` is less than `elf_header->e_phnum`,
 *     repeat (after each pass: increment `i`):
 *     1. If `elf_phdr[i].p_type` is not equal to `PT_LOAD`:
 *        1. Go on with the next pass of the loop.
 *     2. Set `user_dest` to `(char *)elf_phdr[i].p_vaddr`.
 *     3. Set `filesz` to `elf_phdr[i].p_filesz`.
 *     4. Set `memsz` to `elf_phdr[i].p_memsz`.
 *     5. If `filesz` is greater than `memsz`:
 *        1. Call `printf()` with the arguments
 *           `"Partition's ELF has section with 'filesz=%%zu' which is more "`
 *           `"than 'memsz=%%zu'.\n"`, `filesz` and `memsz`.
 *        2. Call `pok_raise_error()` with the arguments
 *           `POK_ERROR_ID_PARTLOAD_ERROR`, `FALSE` and `NULL`.
 *     6. If `user_dest` is less than `space_layout.user_addr`:
 *        1. Call `printf()` with the arguments
 *           `"Partition's ELF maps to virtual address %%p, but space starts "`
 *           `"with %%p.\n"`, `user_dest` and `space_layout.user_addr`.
 *        2. Call `pok_raise_error()` with the arguments
 *           `POK_ERROR_ID_PARTLOAD_ERROR`, `FALSE` and `NULL`.
 *     7. Set `elf_section_end_user` to `user_dest + memsz`.
 *     8. If `elf_end_user` is less than `elf_section_end_user`:
 *        1. Set `elf_end_user` to `elf_section_end_user`.
 * 13. If `elf_end_user` is greater than
 *     `(space_layout.user_addr + space_layout.size)`:
 *     1. Set `elf_span` to `elf_end_user - space_layout.user_addr`.
 *     2. Call `printf()` with the arguments
 *        `"ERROR: Elf maps up to virtual address %%p, which is above
 *        section's " "start by 0x%%zx. But size of the section is only
 *        0x%%zx.\n"`, `elf_end_user`, `(size_t) elf_span` and
 *        `space_layout.size`.
 *     3. Call `printf()` with the argument
 *        `"HINT: Probably, you need to configure more space for the "`
 *        `"partition.\n"`.
 *     4. Call `pok_raise_error()` with the arguments
 *        `POK_ERROR_ID_PARTLOAD_ERROR`, `FALSE` and `NULL`.
 * 14. Set `i` to `0U`; then while `i` is less than `elf_header->e_phnum`,
 *     repeat (after each pass: increment `i`):
 *     1. If `elf_phdr[i].p_type` is not equal to `PT_LOAD`:
 *        1. Go on with the next pass of the loop.
 *     2. Set `user_dest` to `(char *)elf_phdr[i].p_vaddr`.
 *     3. Set `filesz` to `elf_phdr[i].p_filesz`.
 *     4. Set `memsz` to `elf_phdr[i].p_memsz`.
 *     5. Check that
 *        `(user_dest >= space_layout.user_addr) && ((user_dest + filesz) <=`
 *        `(space_layout.user_addr + space_layout.size)) && ((user_dest +`
 *        `memsz) <= (space_layout.user_addr + space_layout.size))` holds
 *        (assert: stop otherwise).
 *     6. Set `kernel_dest` to
 *        `space_layout.kernel_addr + (user_dest - space_layout.user_addr)`.
 *     7. Call `memcpy()` with the arguments `kernel_dest`,
 *        `elf_phdr[i].p_offset + elf_start` and `filesz`.
 *     8. Call `memset()` with the arguments `kernel_dest + filesz`, `0` and
 *        `memsz - filesz`.
 *
 * @calls
 * - `ja_space_layout_get()`
 * - `printf()`
 * - `pok_raise_error()`
 * - `pok_assertion_fail()`
 * - `memcpy()`
 * - `memset()`
 */
void jet_loader_elf_load   (uint8_t elf_id,
                                 jet_space_id space_id,
                                 void (** entry)(void))
{
    size_t elf_offset;
    size_t elf_size;

    jet_space_layout_t space_layout;
    ja_space_layout_get(space_id, &space_layout);

    elf_offset = 0;
    for (uint8_t i = 0; i < elf_id; i++) {
        elf_offset += pok_elf_sizes[i];
    }

    elf_size = pok_elf_sizes[elf_id];

    if (elf_size > space_layout.size)
    {
         (void) printf(
             "Attempt to load elf %u of size %zx into space of size %zx.\n",
            elf_id, elf_size, space_layout.size);
         pok_raise_error(POK_ERROR_ID_CONFIG_ERROR, FALSE, NULL);
    }

    const char* elf_start = &__archive2_begin + elf_offset;

    const Elf_Ehdr*  elf_header;
    const Elf_Phdr*  elf_phdr;

    /*
     * MISRA C:2025 Rule 11.3 deviation: the partition's ELF image is an array
     * of bytes in the kernel image; its headers are read through the ELF
     * structure types. The archive aligns each image for that.
     */
    elf_header = (const Elf_Ehdr*)elf_start;

    if ((elf_header->e_ident[0] != 0x7fU) ||
         (elf_header->e_ident[1] != 0x45U) ||     /* 'E' */
         (elf_header->e_ident[2] != 0x4cU) ||     /* 'L' */
         (elf_header->e_ident[3] != 0x46U))       /* 'F' */
    {
        (void) printf("Partition's ELF has incorrect format");
        pok_raise_error(POK_ERROR_ID_PARTLOAD_ERROR, FALSE, NULL);
    }

    /* MISRA C:2025 Rule 11.1 deviation: the ELF entry point is an address. */
    *entry = (void (*)(void)) elf_header->e_entry;

    elf_phdr = (const Elf_Phdr*)(elf_start + elf_header->e_phoff);

    // First cycle: determine the last virtual address in the elf
    char* elf_end_user = space_layout.user_addr;

    for (uint32_t i = 0U; i < elf_header->e_phnum; ++i)
    {
        // Only PT_LOAD segments occupy memory (e.g. PT_GNU_STACK has p_vaddr 0).
        if (elf_phdr[i].p_type != PT_LOAD) {
            continue;
        }

        /* MISRA C:2025 Rule 11.4 deviation: p_vaddr is a user-space address. */
        char* user_dest = (char *)elf_phdr[i].p_vaddr;
        size_t filesz = elf_phdr[i].p_filesz;
        size_t memsz = elf_phdr[i].p_memsz;

        if(filesz > memsz)
        {
           (void) printf(
               "Partition's ELF has section with 'filesz=%zu' which is more "
               "than 'memsz=%zu'.\n",
               filesz, memsz);
           pok_raise_error(POK_ERROR_ID_PARTLOAD_ERROR, FALSE, NULL);
        }


        if(user_dest < space_layout.user_addr)
        {
           (void) printf(
               "Partition's ELF maps to virtual address %p, but space starts "
               "with %p.\n",
               user_dest, space_layout.user_addr);
           pok_raise_error(POK_ERROR_ID_PARTLOAD_ERROR, FALSE, NULL);
        }

          // Where given section ends(in user space).
        char* elf_section_end_user = user_dest + memsz;

        if(elf_end_user < elf_section_end_user) {
           elf_end_user = elf_section_end_user;
        }
    }

    if(elf_end_user > (space_layout.user_addr + space_layout.size))
    {
       ptrdiff_t elf_span = elf_end_user - space_layout.user_addr;

       (void) printf(
           "ERROR: Elf maps up to virtual address %p, which is above section's "
           "start by 0x%zx. But size of the section is only 0x%zx.\n",
         elf_end_user, (size_t) elf_span, space_layout.size);
       (void) printf(
           "HINT: Probably, you need to configure more space for the "
           "partition.\n");
       pok_raise_error(POK_ERROR_ID_PARTLOAD_ERROR, FALSE, NULL);
    }

    for (uint32_t i = 0U; i < elf_header->e_phnum; ++i)
    {
        // Only PT_LOAD segments occupy memory (e.g. PT_GNU_STACK has p_vaddr 0).
        if (elf_phdr[i].p_type != PT_LOAD) {
            continue;
        }

        /* MISRA C:2025 Rule 11.4 deviation: p_vaddr is a user-space address. */
        char* user_dest = (char *)elf_phdr[i].p_vaddr;
        size_t filesz = elf_phdr[i].p_filesz;
        size_t memsz = elf_phdr[i].p_memsz;

        assert((user_dest >= space_layout.user_addr)
               && ((user_dest + filesz)
                   <= (space_layout.user_addr + space_layout.size))
               && ((user_dest + memsz)
                   <= (space_layout.user_addr + space_layout.size)));

        char* kernel_dest = space_layout.kernel_addr + (user_dest - space_layout.user_addr);

        (void) memcpy (kernel_dest, elf_phdr[i].p_offset + elf_start, filesz);
        (void) memset (kernel_dest + filesz, 0, memsz - filesz);
   }
}

#endif /* POK_NEEDS_PARTITIONS */
