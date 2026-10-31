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
 * Modified by Kirill Brazhnikov, 2026-10-31: adapted from
 * misc/templates/arch/x86/deployment_kernel.tpl for arm64.
 */

#include <arch/deployment.h>

struct ja_arm64_space ja_spaces[{{conf.spaces | length}}] =
{
{%for space in conf.spaces%}
    {
        //.phys_base is filled upon initialization
        .size_normal = {{space.size}},
        .size_heap = {{space.part.get_heap_size()}},
        // Currently stack size is hardcoded to 8K.
        .size_stack = {{space.part.get_needed_threads()}} * 8 * 1024
    },
{%endfor%}
};

int ja_spaces_n = {{conf.spaces | length}};
