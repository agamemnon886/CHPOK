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
 * Modified by Kirill Brazhnikov, 2026-11-02: MISRA C:2025: the header
 * guard isn't a reserved identifier.
 */

#ifndef LIBJET_ARINC_SEMAPHORE_H
#define LIBJET_ARINC_SEMAPHORE_H

#include <config.h>

#ifdef POK_NEEDS_ARINC653_SEMAPHORE
#include <arinc653/types.h>
#include <arinc653/semaphore.h>
#include <msection.h>
#include <types.h>

struct arinc_semaphore
{
    SEMAPHORE_NAME_TYPE semaphore_name;
    
    SEMAPHORE_VALUE_TYPE current_value;
    SEMAPHORE_VALUE_TYPE maximum_value;

    QUEUING_DISCIPLINE_TYPE discipline;
    
    struct msection section;
    struct msection_wq process_queue;
};

/* Preallocated array of semaphores. */
extern struct arinc_semaphore* arinc_semaphores;

#endif /* POK_NEEDS_ARINC653_SEMAPHORE */

#endif /* LIBJET_ARINC_SEMAPHORE_H */
