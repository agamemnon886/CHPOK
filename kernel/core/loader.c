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
 * The modifications are distributed under the POK licence
 * (BSD-2-Clause, see POK-LICENSE). Long lines are wrapped at 80 columns.
 */

/**
 * \file loader.c
 * \author Julian Pidancet
 * \author Julien Delange
 * \date 2008-2009
 *
 * Contains all needed stuff to load partitions (elf files).
 * This needs the partitioning service (POK_NEEDS_PARTITIONS must
 * be defined) to work.
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

extern size_t pok_elf_sizes[];
extern char __archive2_begin;

/**
 * Load an ELF file.
 *
 *  @param file
 */
void jet_loader_elf_load   (uint8_t elf_id,
                                 jet_space_id space_id,
                                 void (** entry)(void))
{
    size_t elf_offset;
    size_t elf_size;

    struct jet_space_layout space_layout;
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
