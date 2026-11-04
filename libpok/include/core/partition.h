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
 * Created by julien on Mon Jan 19 14:18:51 2009 
 *
 * Copyright (C) 2026 Kirill Brazhnikov <agamemnon886@gmail.com>
 * Modified by Kirill Brazhnikov, 2026-11-02: MISRA C:2025: the header
 * guard isn't a reserved identifier.
 * The modifications are distributed under the POK licence
 * (BSD-2-Clause, see POK-LICENSE).
 */

#ifndef POK_PARTITION_H
#define POK_PARTITION_H

#include <config.h>

#include <core/dependencies.h>

#ifdef POK_NEEDS_PARTITIONS

#include <types.h>
#include <errno.h>

#include <uapi/partition_types.h>
#include <uapi/partition_arinc_types.h>

#include <core/syscall.h>

// Rename some syscalls
#define pok_partition_set_mode pok_partition_set_mode_current
#define pok_partition_inc_lock_level pok_current_partition_inc_lock_level
#define pok_partition_dec_lock_level pok_current_partition_dec_lock_level

// Wrapper around corresponded syscall. Returns whether preemption is disabled.
#define pok_current_partition_preemption_disabled() \
    ({int32_t lock_level; pok_current_partition_get_lock_level(&lock_level); lock_level > 0;})


#endif

#endif /* POK_PARTITION_H */
