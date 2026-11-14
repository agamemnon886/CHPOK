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
 * Modified by Kirill Brazhnikov, 2026-11-02: MISRA C:2025: the header
 * guard isn't a reserved identifier.
 * Modified by Kirill Brazhnikov, 2026-11-11: the Doxygen comments that don't
 * document a function definition are plain comments, so that only the function
 * headers are in the function documentation.
 * Modified by Kirill Brazhnikov, 2026-11-13: the function declarations are
 * hidden from Doxygen (\cond): only the definitions of the functions are
 * documented.
 * Modified by Kirill Brazhnikov, 2026-11-14: the declarations of the partition
 * images of kernel/core/loader.c (pok_elf_sizes, __archive2_begin) are moved
 * here.
 * The modifications are distributed under the POK licence
 * (BSD-2-Clause, see POK-LICENSE).
 */


#ifndef JET_LOADER_H
#define JET_LOADER_H

#include <types.h>
#include <errno.h>
#include <core/space.h>
#include <stddef.h>

/*
 * Load elf into given space.
 * 
 * Entry point is returned via 'entry' parameter.
 * 
 * If 'heap_size' is not 0, allocates heap of given size and setup heap
 * in kernel_shared_data.
 */
/** \cond */
void jet_loader_elf_load   (uint8_t elf_id,
                                 jet_space_id space_id,
                                 void (** entry)(void));
/** \endcond */

/*
 * The size of each partition's ELF image, in the order of the images
 * (generated: sizes.c, written by misc/SConscript_base).
 */
extern size_t pok_elf_sizes[];
/*
 * The start of the partition ELF images embedded in the kernel image, one
 * after the other (a symbol of the linker script).
 */
extern char __archive2_begin;

#endif /* JET_LOADER_H */

